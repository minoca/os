/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    part.c

Abstract:

    This module implements support for the partition driver in UEFI.

Author:

    Evan Green 19-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "part.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiPartitionSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiPartitionStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiPartitionStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    );

EFIAPI
EFI_STATUS
EfiPartitionReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

EFIAPI
EFI_STATUS
EfiPartitionReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiPartitionWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiPartitionFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    );

EFI_STATUS
EfipPartitionProbeMediaStatus (
    EFI_DISK_IO_PROTOCOL *DiskIo,
    UINT32 MediaId,
    EFI_STATUS DefaultStatus
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_DRIVER_BINDING_PROTOCOL EfiPartitionDriverBinding = {
    EfiPartitionSupported,
    EfiPartitionStart,
    EfiPartitionStop,
    0xB,
    NULL,
    NULL
};

EFI_PARTITION_DETECT_ROUTINE EfiPartitionDetectRoutines[] = {
    EfiPartitionDetectGpt,
    EfiPartitionDetectElTorito,
    EfiPartitionDetectMbr,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiPartitionDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point into the partition driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    EfiPartitionDriverBinding.ImageHandle = ImageHandle;
    EfiPartitionDriverBinding.DriverBindingHandle = ImageHandle;
    Status = EfiInstallMultipleProtocolInterfaces(
                             &(EfiPartitionDriverBinding.DriverBindingHandle),
                             &EfiDriverBindingProtocolGuid,
                             &EfiPartitionDriverBinding,
                             NULL);

    return Status;
}

EFI_STATUS
EfiPartitionInstallChildHandle (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ParentHandle,
    EFI_DISK_IO_PROTOCOL *DiskIo,
    EFI_BLOCK_IO_PROTOCOL *BlockIo,
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath,
    EFI_DEVICE_PATH_PROTOCOL *DevicePathNode,
    EFI_LBA Start,
    EFI_LBA End,
    UINT32 BlockSize,
    BOOLEAN EfiSystemPartition
    )

/*++

Routine Description:

    This routine creates a new partition child handle for a logical block
    device that represents a partition.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ParentHandle - Supplies the parent handle for the new child.

    DiskIo - Supplies a pointer to the parent disk I/O protocol.

    BlockIo - Supplies a pointer to the block I/O protocol.

    ParentDevicePath - Supplies a pointer to the parent device path.

    DevicePathNode - Supplies the child device path node.

    Start - Supplies the starting LBA of the partition.

    End - Supplies the ending LBA of the partition, inclusive.

    BlockSize - Supplies the disk block size.

    EfiSystemPartition - Supplies a boolean indicating if this is an EFI
        system partition.

Return Value:

    EFI status code.

--*/

{

    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    Private = EfiCoreAllocateBootPool(sizeof(EFI_PARTITION_DATA));
    if (Private == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiSetMem(Private, sizeof(EFI_PARTITION_DATA), 0);
    Private->Magic = EFI_PARTITION_DATA_MAGIC;
    Private->Start = Start * BlockIo->Media->BlockSize;
    Private->End = (End + 1) * BlockIo->Media->BlockSize;
    Private->BlockSize = BlockSize;
    Private->ParentBlockIo = BlockIo;
    Private->ParentDiskIo = DiskIo;

    //
    // Initialize the Block I/O data.
    //

    Private->BlockIo.Revision = BlockIo->Revision;
    Private->BlockIo.Media = &(Private->Media);
    EfiCopyMem(Private->BlockIo.Media,
               BlockIo->Media,
               sizeof(EFI_BLOCK_IO_MEDIA));

    Private->BlockIo.Reset = EfiPartitionReset;
    Private->BlockIo.ReadBlocks = EfiPartitionReadBlocks;
    Private->BlockIo.WriteBlocks = EfiPartitionWriteBlocks;
    Private->BlockIo.FlushBlocks = EfiPartitionFlushBlocks;

    //
    // Initialize the media.
    //

    Private->Media.RemovableMedia = BlockIo->Media->RemovableMedia;
    Private->Media.ReadOnly = BlockIo->Media->ReadOnly;
    Private->Media.WriteCaching = BlockIo->Media->WriteCaching;
    Private->Media.IoAlign = 0;
    Private->Media.LogicalPartition = TRUE;
    Private->Media.LastBlock = ALIGN_VALUE(End - Start + 1, BlockSize) - 1;
    Private->Media.BlockSize = (UINT32)BlockSize;

    //
    // Per UEFI spec, set the lowest aligned LBA, logical blocks per physical
    // block, and optimal transfer length granularity to zero for logical
    // partitions.
    //

    if (Private->BlockIo.Revision >= EFI_BLOCK_IO_PROTOCOL_REVISION2) {
        Private->Media.LowestAlignedLba = 0;
        Private->Media.LogicalBlocksPerPhysicalBlock = 0;
    }

    Private->DevicePath = EfiCoreAppendDevicePathNode(ParentDevicePath,
                                                      DevicePathNode);

    if (Private->DevicePath == NULL) {
        EfiFreePool(Private);
        return EFI_OUT_OF_RESOURCES;
    }

    if (EfiSystemPartition != FALSE) {
        Private->EspGuid = &EfiPartitionTypeSystemPartitionGuid;
    }

    //
    // Create the new handle.
    //

    Status = EfiInstallMultipleProtocolInterfaces(&(Private->Handle),
                                                  &EfiDevicePathProtocolGuid,
                                                  Private->DevicePath,
                                                  &EfiBlockIoProtocolGuid,
                                                  &(Private->BlockIo),
                                                  Private->EspGuid,
                                                  NULL,
                                                  NULL);

    if (!EFI_ERROR(Status)) {
        Status = EfiOpenProtocol(ParentHandle,
                                 &EfiDiskIoProtocolGuid,
                                 (VOID **)&DiskIo,
                                 This->DriverBindingHandle,
                                 Private->Handle,
                                 EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

    } else {
        EfiFreePool(Private->DevicePath);
        EfiFreePool(Private);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiPartitionSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine tests to see if the partition driver supports this new
    controller handle. Any controller handle that contains a block I/O and
    disk I/O protocol is supported.

Arguments:

    This - Supplies a pointer to the driver binding instance.

    ControllerHandle - Supplies the new controller handle to test.

    RemainingDevicePath - Supplies an optional parameter to pick a specific
        child device to start.

Return Value:

    EFI status code.

--*/

{

    EFI_DISK_IO_PROTOCOL *DiskIo;
    EFI_DEV_PATH *Node;
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath;
    EFI_STATUS Status;

    if (RemainingDevicePath != NULL) {
        if (EfiCoreIsDevicePathEnd(RemainingDevicePath) == FALSE) {
            Node = (EFI_DEV_PATH *)RemainingDevicePath;
            if ((Node->DevPath.Type != MEDIA_DEVICE_PATH) ||
                (Node->DevPath.SubType != MEDIA_HARDDRIVE_DP) ||
                (EfiCoreGetDevicePathNodeLength(&(Node->DevPath)) !=
                 sizeof(HARDDRIVE_DEVICE_PATH))) {

                return EFI_UNSUPPORTED;
            }
        }
    }

    //
    // Try to open the abstractions needed to support partitions. Start by
    // opening the disk I/O protocol, the least common.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             (VOID **)&DiskIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (Status == EFI_ALREADY_STARTED) {
        return EFI_SUCCESS;
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCloseProtocol(ControllerHandle,
                     &EfiDiskIoProtocolGuid,
                     This->DriverBindingHandle,
                     ControllerHandle);

    //
    // Also open up the device path protocol.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDevicePathProtocolGuid,
                             (VOID **)&ParentDevicePath,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (Status == EFI_ALREADY_STARTED) {
        return EFI_SUCCESS;
    }

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCloseProtocol(ControllerHandle,
                     &EfiDevicePathProtocolGuid,
                     This->DriverBindingHandle,
                     ControllerHandle);

    //
    // Open Block I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             NULL,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_TEST_PROTOCOL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiPartitionStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine starts a partition driver on a raw Block I/O device.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the controller to start. This
        handle must support a protocol interface that supplies an I/O
        abstraction to the driver.

    RemainingDevicePath - Supplies an optional pointer to the remaining
        portion of a device path.

Return Value:

    EFI_SUCCESS if the device was started.

    EFI_DEVICE_ERROR if the device could not be started due to a device error.

    EFI_OUT_OF_RESOURCES if an allocation failed.

    Other error codes if the driver failed to start the device.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DISK_IO_PROTOCOL *DiskIo;
    BOOLEAN MediaPresent;
    EFI_TPL OldTpl;
    EFI_STATUS OpenStatus;
    EFI_DEVICE_PATH_PROTOCOL *ParentDevicePath;
    EFI_PARTITION_DETECT_ROUTINE *Routine;
    EFI_STATUS Status;

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);
    if (RemainingDevicePath != NULL) {
        if (EfiCoreIsDevicePathEnd(RemainingDevicePath) != FALSE) {
            Status = EFI_SUCCESS;
            goto PartitionStartEnd;
        }
    }

    //
    // Open up Block I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             (VOID **)&BlockIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
        goto PartitionStartEnd;
    }

    //
    // Get the device path.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDevicePathProtocolGuid,
                             (VOID **)&ParentDevicePath,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if ((EFI_ERROR(Status)) && (Status != EFI_ALREADY_STARTED)) {
        goto PartitionStartEnd;
    }

    //
    // Open Disk I/O.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             (VOID **)&DiskIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if ((EFI_ERROR(Status)) && (Status != EFI_ALREADY_STARTED)) {
        EfiCloseProtocol(ControllerHandle,
                         &EfiDevicePathProtocolGuid,
                         This->DriverBindingHandle,
                         ControllerHandle);

        goto PartitionStartEnd;
    }

    OpenStatus = Status;

    //
    // Try to read blocks when there's media or it's a removable physical
    // partition.
    //

    Status = EFI_UNSUPPORTED;
    MediaPresent = BlockIo->Media->MediaPresent;
    if ((MediaPresent != FALSE) ||
        ((BlockIo->Media->RemovableMedia != FALSE) &&
         (BlockIo->Media->LogicalPartition == FALSE))) {

        //
        // Try for GPT, El Torito, and then legacy MBR partition types.
        //

        Routine = &(EfiPartitionDetectRoutines[0]);
        while (*Routine != NULL) {
            Status = (*Routine)(This,
                                ControllerHandle,
                                DiskIo,
                                BlockIo,
                                ParentDevicePath);

            if ((!EFI_ERROR(Status)) ||
                (Status == EFI_MEDIA_CHANGED) ||
                (Status == EFI_NO_MEDIA)) {

                break;
            }

            Routine += 1;
        }
    }

    //
    // In the case that the driver is already started, the device path and disk
    // I/O are not actually opened by this driver. So, don't try and close
    // them, since they are not owned here.
    //

    if ((EFI_ERROR(Status)) && (!EFI_ERROR(OpenStatus)) &&
        (Status != EFI_MEDIA_CHANGED) &&
        ((MediaPresent == FALSE) || (Status != EFI_NO_MEDIA))) {

        EfiCloseProtocol(ControllerHandle,
                         &EfiDiskIoProtocolGuid,
                         This->DriverBindingHandle,
                         ControllerHandle);

        EfiCloseProtocol(ControllerHandle,
                         &EfiDevicePathProtocolGuid,
                         This->DriverBindingHandle,
                         ControllerHandle);
    }

PartitionStartEnd:
    EfiRestoreTPL(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiPartitionStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    )

/*++

Routine Description:

    This routine stops a partition driver device, stopping any child handles
    created by this driver.

Arguments:

    This - Supplies a pointer to the driver binding protocol instance.

    ControllerHandle - Supplies the handle of the device being stopped. The
        handle must support a bus specific I/O protocol for the driver to use
        to stop the device.

    NumberOfChildren - Supplies the number of child devices in the child handle
        buffer.

    ChildHandleBuffer - Supplies an optional array of child device handles to
        be freed. This can be NULL if the number of children specified is zero.

Return Value:

    EFI_SUCCESS if the device was stopped.

    EFI_DEVICE_ERROR if the device could not be stopped due to a device error.

--*/

{

    BOOLEAN AllChildrenStopped;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_DISK_IO_PROTOCOL *DiskIo;
    UINTN Index;
    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    BlockIo = NULL;
    Private = NULL;
    if (NumberOfChildren == 0) {
        EfiCloseProtocol(ControllerHandle,
                         &EfiDiskIoProtocolGuid,
                         This->DriverBindingHandle,
                         ControllerHandle);

        EfiCloseProtocol(ControllerHandle,
                         &EfiDevicePathProtocolGuid,
                         This->DriverBindingHandle,
                         ControllerHandle);

        return EFI_SUCCESS;
    }

    AllChildrenStopped = TRUE;
    for (Index = 0; Index < NumberOfChildren; Index += 1) {
        EfiOpenProtocol(ChildHandleBuffer[Index],
                        &EfiBlockIoProtocolGuid,
                        (VOID **)&BlockIo,
                        This->DriverBindingHandle,
                        ControllerHandle,
                        EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        Private = EFI_PARTITION_DATA_FROM_THIS(BlockIo);

        ASSERT(Private->Magic == EFI_PARTITION_DATA_MAGIC);

        EfiCloseProtocol(ControllerHandle,
                         &EfiDiskIoProtocolGuid,
                         This->DriverBindingHandle,
                         ChildHandleBuffer[Index]);

        BlockIo->FlushBlocks(BlockIo);
        Status = EfiUninstallMultipleProtocolInterfaces(
                                                    ChildHandleBuffer[Index],
                                                    &EfiDevicePathProtocolGuid,
                                                    Private->DevicePath,
                                                    &EfiBlockIoProtocolGuid,
                                                    &(Private->BlockIo),
                                                    NULL,
                                                    NULL);

        if (EFI_ERROR(Status)) {
            EfiOpenProtocol(ControllerHandle,
                            &EfiDiskIoProtocolGuid,
                            (VOID **)&DiskIo,
                            This->DriverBindingHandle,
                            ChildHandleBuffer[Index],
                            EFI_OPEN_PROTOCOL_BY_CHILD_CONTROLLER);

            AllChildrenStopped = FALSE;

        } else {
            EfiFreePool(Private->DevicePath);
            EfiFreePool(Private);
        }
    }

    if (AllChildrenStopped == FALSE) {
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiPartitionReset (
    EFI_BLOCK_IO_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    )

/*++

Routine Description:

    This routine resets the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating whether or not the
        driver should perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

--*/

{

    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    Private = EFI_PARTITION_DATA_FROM_THIS(This);
    Status = Private->ParentBlockIo->Reset(Private->ParentBlockIo,
                                           ExtendedVerification);

    return Status;
}

EFIAPI
EFI_STATUS
EfiPartitionReadBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O read from the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the read.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer where the read data will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    UINT64 Offset;
    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    Private = EFI_PARTITION_DATA_FROM_THIS(This);

    ASSERT(Private->Magic == EFI_PARTITION_DATA_MAGIC);

    if ((BufferSize % Private->BlockSize) != 0) {
        Status = EfipPartitionProbeMediaStatus(Private->ParentDiskIo,
                                               MediaId,
                                               EFI_BAD_BUFFER_SIZE);

        return Status;
    }

    Offset = Lba * Private->BlockSize + Private->Start;
    if (Offset + BufferSize > Private->End) {
        Status = EfipPartitionProbeMediaStatus(Private->ParentDiskIo,
                                               MediaId,
                                               EFI_INVALID_PARAMETER);

        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    //
    // Use the disk I/O protocol because some kinds of partitions have
    // different block sizes than their parents.
    //

    Status = Private->ParentDiskIo->ReadDisk(Private->ParentDiskIo,
                                             MediaId,
                                             Offset,
                                             BufferSize,
                                             Buffer);

    return Status;
}

EFIAPI
EFI_STATUS
EfiPartitionWriteBlocks (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine performs a block I/O write to the device.

Arguments:

    This - Supplies a pointer to the protocol instance.

    MediaId - Supplies the media identifier, which changes each time the media
        is replaced.

    Lba - Supplies the logical block address of the write.

    BufferSize - Supplies the size of the buffer in bytes.

    Buffer - Supplies the buffer containing the data to write.

Return Value:

    EFI_SUCCESS on success.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the media ID does not match the current device.

    EFI_BAD_BUFFER_SIZE if the buffer was not a multiple of the device block
    size.

    EFI_INVALID_PARAMETER if the read request contains LBAs that are not valid,
    or the buffer is not properly aligned.

--*/

{

    UINT64 Offset;
    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    Private = EFI_PARTITION_DATA_FROM_THIS(This);

    ASSERT(Private->Magic == EFI_PARTITION_DATA_MAGIC);

    if ((BufferSize % Private->BlockSize) != 0) {
        Status = EfipPartitionProbeMediaStatus(Private->ParentDiskIo,
                                               MediaId,
                                               EFI_BAD_BUFFER_SIZE);

        return Status;
    }

    Offset = Lba * Private->BlockSize + Private->Start;
    if (Offset + BufferSize > Private->End) {
        Status = EfipPartitionProbeMediaStatus(Private->ParentDiskIo,
                                               MediaId,
                                               EFI_INVALID_PARAMETER);
    }

    //
    // Use the disk I/O protocol because some kinds of partitions have
    // different block sizes than their parents.
    //

    Status = Private->ParentDiskIo->WriteDisk(Private->ParentDiskIo,
                                              MediaId,
                                              Offset,
                                              BufferSize,
                                              Buffer);

    return Status;
}

EFIAPI
EFI_STATUS
EfiPartitionFlushBlocks (
    EFI_BLOCK_IO_PROTOCOL *This
    )

/*++

Routine Description:

    This routine flushes the block device.

Arguments:

    This - Supplies a pointer to the protocol instance.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device had an error and could not complete the
    request.

    EFI_NO_MEDIA if there is no media in the device.

--*/

{

    PEFI_PARTITION_DATA Private;
    EFI_STATUS Status;

    Private = EFI_PARTITION_DATA_FROM_THIS(This);

    ASSERT(Private->Magic == EFI_PARTITION_DATA_MAGIC);

    Status = Private->ParentBlockIo->FlushBlocks(Private->ParentBlockIo);
    return Status;
}

EFI_STATUS
EfipPartitionProbeMediaStatus (
    EFI_DISK_IO_PROTOCOL *DiskIo,
    UINT32 MediaId,
    EFI_STATUS DefaultStatus
    )

/*++

Routine Description:

    This routine probes the media status and returns EFI_NO_MEDIA or
    EFI_MEDIA_CHANGED if the media changed. Otherwise a default status is
    returned.

Arguments:

    DiskIo - Supplies a pointer to the disk I/O protocol.

    MediaId - Supplies the ID of the media, which changes every time the media
        is replaced.

    DefaultStatus - Supplies the status to return if a media event did not
        occur.

Return Value:

    EFI_NO_MEDIA if there is no media.

    EFI_MEDIA_CHANGED if the media changed.

    Returns the default status parameter otherwise.

--*/

{

    EFI_STATUS Status;

    Status = DiskIo->ReadDisk(DiskIo, MediaId, 0, 1, NULL);
    if ((Status == EFI_NO_MEDIA) || (Status == EFI_MEDIA_CHANGED)) {
        return Status;
    }

    return DefaultStatus;
}

