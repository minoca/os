/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    diskio.c

Abstract:

    This module implements the UEFI disk I/O protocol.

Author:

    Evan Green 20-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/uefi/protocol/diskio.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/drvbind.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a pointer to the disk I/O data given a pointer to the
// block I/O protocol instance.
//

#define EFI_DISK_IO_DATA_FROM_THIS(_DiskIo) \
    PARENT_STRUCTURE(_DiskIo, EFI_DISK_IO_DATA, DiskIo)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_DISK_IO_DATA_MAGIC 0x6B736944 // 'ksiD'

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the disk I/O protocol's private context.

Members:

    Magic - Stores the magic constand EFI_DISK_IO_DATA_MAGIC.

    DiskIo - Stores the disk I/O protocol.

    BlockIo - Stores a pointer to the block I/O protocol.

--*/

typedef struct _EFI_DISK_IO_DATA {
    UINT32 Magic;
    EFI_DISK_IO_PROTOCOL DiskIo;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
} EFI_DISK_IO_DATA, *PEFI_DISK_IO_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFIAPI
EFI_STATUS
EfiDiskIoSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiDiskIoStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    );

EFIAPI
EFI_STATUS
EfiDiskIoStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    );

EFIAPI
EFI_STATUS
EfiDiskIoRead (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    );

EFIAPI
EFI_STATUS
EfiDiskIoWrite (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_DRIVER_BINDING_PROTOCOL EfiDiskIoDriverBinding = {
    EfiDiskIoSupported,
    EfiDiskIoStart,
    EfiDiskIoStop,
    0xA,
    NULL,
    NULL
};

EFI_DISK_IO_DATA EfiDiskIoDataTemplate = {
    EFI_DISK_IO_DATA_MAGIC,
    {
        EFI_DISK_IO_PROTOCOL_REVISION,
        EfiDiskIoRead,
        EfiDiskIoWrite
    },

    NULL
};

EFI_GUID EfiDiskIoProtocolGuid = EFI_DISK_IO_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiDiskIoDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point into the disk I/O driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    EfiDiskIoDriverBinding.ImageHandle = ImageHandle;
    EfiDiskIoDriverBinding.DriverBindingHandle = ImageHandle;
    Status = EfiInstallMultipleProtocolInterfaces(
                             &(EfiDiskIoDriverBinding.DriverBindingHandle),
                             &EfiDriverBindingProtocolGuid,
                             &EfiDiskIoDriverBinding,
                             NULL);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFIAPI
EFI_STATUS
EfiDiskIoSupported (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine tests to see if the disk I/O driver supports this new
    controller handle. Any controller handle that contains a block protocol is
    supported.

Arguments:

    This - Supplies a pointer to the driver binding instance.

    ControllerHandle - Supplies the new controller handle to test.

    RemainingDevicePath - Supplies an optional parameter to pick a specific
        child device to start.

Return Value:

    EFI status code.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_STATUS Status;

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             (VOID **)&BlockIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiCloseProtocol(ControllerHandle,
                     &EfiBlockIoProtocolGuid,
                     This->DriverBindingHandle,
                     ControllerHandle);

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiDiskIoStart (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath
    )

/*++

Routine Description:

    This routine starts a disk I/O driver on a raw Block I/O device.

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

    PEFI_DISK_IO_DATA Instance;
    EFI_TPL OldTpl;
    BOOLEAN ProtocolOpen;
    EFI_STATUS Status;

    Instance = NULL;
    ProtocolOpen = FALSE;
    OldTpl = EfiRaiseTPL(TPL_CALLBACK);

    //
    // Connect to the block I/O interface.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             (VOID **)&(EfiDiskIoDataTemplate.BlockIo),
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_BY_DRIVER);

    if (EFI_ERROR(Status)) {
        goto DiskIoStartEnd;
    }

    ProtocolOpen = TRUE;
    Instance = EfiCoreAllocateBootPool(sizeof(EFI_DISK_IO_DATA));
    if (Instance == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto DiskIoStartEnd;
    }

    EfiCopyMem(Instance, &EfiDiskIoDataTemplate, sizeof(EFI_DISK_IO_DATA));
    Status = EfiInstallMultipleProtocolInterfaces(&ControllerHandle,
                                                  &EfiDiskIoProtocolGuid,
                                                  &(Instance->DiskIo),
                                                  NULL);

    if (EFI_ERROR(Status)) {
        goto DiskIoStartEnd;
    }

DiskIoStartEnd:
    if (EFI_ERROR(Status)) {
        if (Instance != NULL) {
            EfiFreePool(Instance);
        }

        if (ProtocolOpen != FALSE) {
            EfiCloseProtocol(ControllerHandle,
                             &EfiBlockIoProtocolGuid,
                             This->DriverBindingHandle,
                             ControllerHandle);
        }
    }

    EfiRestoreTPL(OldTpl);
    return Status;
}

EFIAPI
EFI_STATUS
EfiDiskIoStop (
    EFI_DRIVER_BINDING_PROTOCOL *This,
    EFI_HANDLE ControllerHandle,
    UINTN NumberOfChildren,
    EFI_HANDLE *ChildHandleBuffer
    )

/*++

Routine Description:

    This routine stops a disk I/O driver device, stopping any child handles
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

    EFI_DISK_IO_PROTOCOL *DiskIo;
    PEFI_DISK_IO_DATA Instance;
    EFI_STATUS Status;

    //
    // Get the context back.
    //

    Status = EfiOpenProtocol(ControllerHandle,
                             &EfiDiskIoProtocolGuid,
                             (VOID **)&DiskIo,
                             This->DriverBindingHandle,
                             ControllerHandle,
                             EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Instance = EFI_DISK_IO_DATA_FROM_THIS(DiskIo);
    Status = EfiUninstallMultipleProtocolInterfaces(ControllerHandle,
                                                    &EfiDiskIoProtocolGuid,
                                                    &(Instance->DiskIo),
                                                    NULL);

    if (!EFI_ERROR(Status)) {
        Status = EfiCloseProtocol(ControllerHandle,
                                  &EfiBlockIoProtocolGuid,
                                  This->DriverBindingHandle,
                                  ControllerHandle);

        ASSERT(!EFI_ERROR(Status));

        EfiFreePool(Instance);
    }

    return Status;
}

EFIAPI
EFI_STATUS
EfiDiskIoRead (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine reads bytes from the disk.

Arguments:

    This - Supplies the protocol instance.

    MediaId - Supplies the ID of the media, which changes every time the media
        is replaced.

    Offset - Supplies the starting byte offset to read from.

    BufferSize - Supplies the size of the given buffer.

    Buffer - Supplies a pointer where the read data will be returned.

Return Value:

    EFI_SUCCESS if all data was successfully read.

    EFI_DEVICE_ERROR if a hardware error occurred while performing the
    operation.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the current media ID doesn't match the one passed in.

    EFI_INVALID_PARAMETER if the offset is invalid.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_LBA BlockOffset;
    UINTN BlockOffsetRemainder;
    UINT32 BlockSize;
    VOID *BounceBuffer;
    VOID *BounceBufferAllocation;
    UINTN BounceBufferSize;
    PEFI_DISK_IO_DATA Instance;
    UINT32 IoAlign;
    UINTN IoSize;
    EFI_STATUS Status;

    Instance = EFI_DISK_IO_DATA_FROM_THIS(This);

    ASSERT(Instance->Magic == EFI_DISK_IO_DATA_MAGIC);

    BlockIo = Instance->BlockIo;
    if (BlockIo->Media->MediaPresent == FALSE) {
        return EFI_NO_MEDIA;
    }

    if (BufferSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    ASSERT(BufferSize != 0);

    //
    // Pass it down directly if it all lines up.
    //

    BlockSize = BlockIo->Media->BlockSize;
    IoAlign = BlockIo->Media->IoAlign;
    if (((Offset % BlockSize) == 0) &&
        ((IoAlign == 0) || (((UINTN)Buffer % IoAlign) == 0))) {

        Status = BlockIo->ReadBlocks(BlockIo,
                                     BlockIo->Media->MediaId,
                                     Offset / BlockSize,
                                     BufferSize,
                                     Buffer);

        return Status;
    }

    //
    // Allocate a bounce buffer for the read. The I/O size must be a multiple
    // of the block size. The buffer must be aligned, so allocate enough
    // space to scoot it up by the alignment.
    //

    BlockOffset = Offset / BlockSize;
    BlockOffsetRemainder = Offset - (Offset * BlockSize);
    IoSize = (BufferSize + BlockOffsetRemainder + (BlockSize - 1)) / BlockSize;
    BounceBufferSize = IoSize + IoAlign;
    BounceBufferAllocation = EfiCoreAllocateBootPool(BounceBufferSize);
    if (BounceBufferAllocation == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    BounceBuffer = ALIGN_POINTER(BounceBufferAllocation, IoAlign);

    //
    // Perform the read.
    //

    Status = BlockIo->ReadBlocks(BlockIo,
                                 BlockIo->Media->MediaId,
                                 BlockOffset,
                                 IoSize,
                                 BounceBuffer);

    //
    // If nothing went wrong, copy the result in to the final buffer.
    //

    if (!EFI_ERROR(Status)) {
        EfiCopyMem(Buffer, BounceBuffer + BlockOffsetRemainder, BufferSize);

    } else {
        EfiDebugPrint("IO Read Error block 0x%I64x Size %x: %x\n",
                      BlockOffset,
                      IoSize,
                      Status);
    }

    EfiFreePool(BounceBufferAllocation);
    return Status;
}

EFIAPI
EFI_STATUS
EfiDiskIoWrite (
    EFI_DISK_IO_PROTOCOL *This,
    UINT32 MediaId,
    UINT64 Offset,
    UINTN BufferSize,
    VOID *Buffer
    )

/*++

Routine Description:

    This routine writes bytes to the disk.

Arguments:

    This - Supplies the protocol instance.

    MediaId - Supplies the ID of the media, which changes every time the media
        is replaced.

    Offset - Supplies the starting byte offset to write to.

    BufferSize - Supplies the size of the given buffer.

    Buffer - Supplies a pointer containing the data to write.

Return Value:

    EFI_SUCCESS if all data was successfully written.

    EFI_WRITE_PROTECTED if the device cannot be written to.

    EFI_DEVICE_ERROR if a hardware error occurred while performing the
    operation.

    EFI_NO_MEDIA if there is no media in the device.

    EFI_MEDIA_CHANGED if the current media ID doesn't match the one passed in.

    EFI_INVALID_PARAMETER if the offset is invalid.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    EFI_LBA BlockOffset;
    UINTN BlockOffsetRemainder;
    UINT32 BlockSize;
    VOID *BounceBuffer;
    VOID *BounceBufferAllocation;
    UINTN BounceBufferSize;
    PEFI_DISK_IO_DATA Instance;
    UINT32 IoAlign;
    UINTN IoSize;
    EFI_STATUS Status;

    Instance = EFI_DISK_IO_DATA_FROM_THIS(This);

    ASSERT(Instance->Magic == EFI_DISK_IO_DATA_MAGIC);

    BlockIo = Instance->BlockIo;
    if (BlockIo->Media->MediaPresent == FALSE) {
        return STATUS_NO_MEDIA;
    }

    if (BufferSize == 0) {
        return EFI_INVALID_PARAMETER;
    }

    ASSERT(BufferSize != 0);

    //
    // Pass it down directly if it all lines up.
    //

    BlockSize = BlockIo->Media->BlockSize;
    IoAlign = BlockIo->Media->IoAlign;
    if (((Offset % BlockSize) == 0) &&
        ((IoAlign == 0) || (((UINTN)Buffer % IoAlign) == 0))) {

        Status = BlockIo->WriteBlocks(BlockIo,
                                      BlockIo->Media->MediaId,
                                      Offset / BlockSize,
                                      BufferSize,
                                      Buffer);

        return Status;
    }

    //
    // Allocate a bounce buffer for the write. The I/O size must be a multiple
    // of the block size. The buffer must be aligned, so allocate enough
    // space to scoot it up by the alignment.
    //

    BlockOffset = Offset / BlockSize;
    BlockOffsetRemainder = Offset - (Offset * BlockSize);
    IoSize = (BufferSize + BlockOffsetRemainder + (BlockSize - 1)) / BlockSize;
    BounceBufferSize = IoSize + IoAlign;
    BounceBufferAllocation = EfiCoreAllocateBootPool(BounceBufferSize);
    if (BounceBufferAllocation == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    BounceBuffer = ALIGN_POINTER(BounceBufferAllocation, IoAlign);

    //
    // Perform the read to get the original block data.
    //

    Status = BlockIo->ReadBlocks(BlockIo,
                                 BlockIo->Media->MediaId,
                                 BlockOffset,
                                 IoSize,
                                 BounceBuffer);

    //
    // If nothing went wrong, copy the result in to the final buffer.
    //

    if (EFI_ERROR(Status)) {
        EfiDebugPrint("IO Read Error block 0x%I64x Size %x: %x\n",
                      BlockOffset,
                      IoSize,
                      Status);

        goto DiskIoWriteEnd;
    }

    //
    // Write the data in.
    //

    EfiCopyMem(BounceBuffer + BlockOffsetRemainder, Buffer, BufferSize);

    //
    // Now write the blocks.
    //

    Status = BlockIo->WriteBlocks(BlockIo,
                                  BlockIo->Media->MediaId,
                                  BlockOffset,
                                  IoSize,
                                  BounceBuffer);

    if (EFI_ERROR(Status)) {
        EfiDebugPrint("IO Write Error block 0x%I64x Size %x: %x\n",
                      BlockOffset,
                      IoSize,
                      Status);

        goto DiskIoWriteEnd;
    }

DiskIoWriteEnd:
    EfiFreePool(BounceBufferAllocation);
    return Status;
}

