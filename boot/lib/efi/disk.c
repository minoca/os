/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disk.c

Abstract:

    This module implements UEFI block I/O support for the OS loader.

Author:

    Evan Green 12-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/loadimg.h>
#include <minoca/uefi/protocol/ramdisk.h>
#include "firmware.h"
#include "bootlib.h"
#include "efisup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information for an open UEFI disk (partition).

Members:

    Device - Stores the handle of the device.

    BlockIo - Stores a pointer to the block I/O protocol interface.

--*/

typedef struct _BOOT_DISK_HANDLE {
    EFI_HANDLE Device;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
} BOOT_DISK_HANDLE, *PBOOT_DISK_HANDLE;

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
BopEfiBlockIoRead (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

EFI_STATUS
BopEfiBlockIoWrite (
    EFI_BLOCK_IO_PROTOCOL *This,
    UINT32 MediaId,
    EFI_LBA Lba,
    UINTN BufferSize,
    VOID *Buffer
    );

BOOL
BopEfiMatchPartitionDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE]
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BopEfiOpenBootDisk (
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine attempts to open the boot disk, the disk from which to load
    the OS.

Arguments:

    Handle - Supplies a pointer where a handle to the disk will be returned.

Return Value:

    Status code.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIoProtocol;
    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_HANDLE DeviceHandle;
    EFI_STATUS EfiStatus;
    EFI_LOADED_IMAGE *LoadedImage;
    KSTATUS Status;

    BootDiskHandle = NULL;
    BlockIoProtocol = NULL;
    DeviceHandle = NULL;
    LoadedImage = NULL;

    //
    // Open up the loaded image protocol to get the image device path.
    //

    EfiStatus = BopEfiOpenProtocol(BoEfiImageHandle,
                                   &BoEfiLoadedImageProtocolGuid,
                                   (VOID **)&LoadedImage,
                                   BoEfiImageHandle,
                                   NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiOpenBootDiskEnd;
    }

    //
    // Attempt to open up the Block I/O protocol on the device handle.
    //

    DeviceHandle = LoadedImage->DeviceHandle;
    EfiStatus = BopEfiOpenProtocol(DeviceHandle,
                                   &BoEfiBlockIoProtocolGuid,
                                   (VOID **)&BlockIoProtocol,
                                   BoEfiImageHandle,
                                   NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiOpenBootDiskEnd;
    }

    //
    // Allocate and fill in the boot disk handle.
    //

    BootDiskHandle = BoAllocateMemory(sizeof(BOOT_DISK_HANDLE));
    if (BootDiskHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EfiOpenBootDiskEnd;
    }

    RtlZeroMemory(BootDiskHandle, sizeof(BOOT_DISK_HANDLE));
    BootDiskHandle->Device = DeviceHandle;
    BootDiskHandle->BlockIo = BlockIoProtocol;
    BlockIoProtocol = NULL;
    Status = STATUS_SUCCESS;

EfiOpenBootDiskEnd:
    if (LoadedImage != NULL) {
        BopEfiCloseProtocol(BoEfiImageHandle,
                            &BoEfiLoadedImageProtocolGuid,
                            BoEfiImageHandle,
                            NULL);
    }

    if (BlockIoProtocol != NULL) {
        BopEfiCloseProtocol(DeviceHandle,
                            &BoEfiBlockIoProtocolGuid,
                            BoEfiImageHandle,
                            NULL);
    }

    ASSERT((KSUCCESS(Status)) || (BootDiskHandle == NULL));

    *Handle = BootDiskHandle;
    return Status;
}

KSTATUS
BopEfiOpenPartition (
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE],
    PHANDLE Handle
    )

/*++

Routine Description:

    This routine opens a handle to a disk and partition with the given IDs.

Arguments:

    PartitionId - Supplies the partition identifier to match against.

    Handle - Supplies a pointer where a handle to the opened disk will be
        returned upon success.

Return Value:

    Status code.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIoProtocol;
    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH *DevicePath;
    EFI_STATUS EfiStatus;
    EFI_HANDLE *HandleArray;
    UINTN HandleArraySize;
    UINTN HandleCount;
    UINTN HandleIndex;
    KSTATUS Status;

    BootDiskHandle = NULL;
    DeviceHandle = NULL;
    HandleArray = NULL;

    //
    // Locate all the handles that support the block I/O protocol.
    //

    HandleArraySize = 0;
    BopEfiLocateHandle(ByProtocol,
                       &BoEfiBlockIoProtocolGuid,
                       NULL,
                       &HandleArraySize,
                       NULL);

    if (HandleArraySize == 0) {
        Status = STATUS_FIRMWARE_ERROR;
        goto EfiOpenPartitionEnd;
    }

    HandleArray = BoAllocateMemory(HandleArraySize);
    if (HandleArray == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EfiOpenPartitionEnd;
    }

    EfiStatus = BopEfiLocateHandle(ByProtocol,
                                   &BoEfiBlockIoProtocolGuid,
                                   NULL,
                                   &HandleArraySize,
                                   HandleArray);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiOpenPartitionEnd;
    }

    //
    // Loop over each handle that supports block I/O.
    //

    HandleCount = HandleArraySize / sizeof(EFI_HANDLE);
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {

        //
        // Get the device path for this handle.
        //

        EfiStatus = BopEfiOpenProtocol(HandleArray[HandleIndex],
                                       &BoEfiDevicePathProtocolGuid,
                                       (VOID **)&DevicePath,
                                       BoEfiImageHandle,
                                       NULL,
                                       EFI_OPEN_PROTOCOL_GET_PROTOCOL);

        if (EFI_ERROR(EfiStatus)) {
            continue;
        }

        if (BopEfiMatchPartitionDevicePath(DevicePath, PartitionId) != FALSE) {
            break;
        }
    }

    if (HandleIndex == HandleCount) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto EfiOpenPartitionEnd;
    }

    //
    // Attempt to open up the Block I/O protocol on the device handle.
    //

    DeviceHandle = HandleArray[HandleIndex];
    EfiStatus = BopEfiOpenProtocol(DeviceHandle,
                                   &BoEfiBlockIoProtocolGuid,
                                   (VOID **)&BlockIoProtocol,
                                   BoEfiImageHandle,
                                   NULL,
                                   EFI_OPEN_PROTOCOL_GET_PROTOCOL);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiOpenPartitionEnd;
    }

    //
    // Allocate and fill in the boot disk handle.
    //

    BootDiskHandle = BoAllocateMemory(sizeof(BOOT_DISK_HANDLE));
    if (BootDiskHandle == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EfiOpenPartitionEnd;
    }

    RtlZeroMemory(BootDiskHandle, sizeof(BOOT_DISK_HANDLE));
    BootDiskHandle->Device = DeviceHandle;
    BootDiskHandle->BlockIo = BlockIoProtocol;
    BlockIoProtocol = NULL;
    Status = STATUS_SUCCESS;

EfiOpenPartitionEnd:
    if (HandleArray != NULL) {
        BoFreeMemory(HandleArray);
    }

    if (BlockIoProtocol != NULL) {
        BopEfiCloseProtocol(DeviceHandle,
                            &BoEfiBlockIoProtocolGuid,
                            BoEfiImageHandle,
                            NULL);
    }

    ASSERT((KSUCCESS(Status)) || (BootDiskHandle == NULL));

    *Handle = BootDiskHandle;
    return Status;
}

VOID
BopEfiCloseDisk (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine closes an open disk.

Arguments:

    DiskHandle - Supplies a pointer to the open disk handle.

Return Value:

    None.

--*/

{

    PBOOT_DISK_HANDLE BootDiskHandle;

    BootDiskHandle = (PBOOT_DISK_HANDLE)DiskHandle;
    if (BootDiskHandle == NULL) {
        return;
    }

    if (BootDiskHandle->BlockIo != NULL) {
        BopEfiCloseProtocol(BootDiskHandle->Device,
                            &BoEfiBlockIoProtocolGuid,
                            BoEfiImageHandle,
                            NULL);
    }

    BoFreeMemory(BootDiskHandle);
    return;
}

KSTATUS
BopEfiLoaderBlockIoRead (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses firmware calls read sectors off of a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to read from.

    Sector - Supplies the zero-based sector number to read from.

    SectorCount - Supplies the number of sectors to read. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer where the data read from the disk will be
        returned upon success.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

{

    ULONG BlockSize;
    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_STATUS EfiStatus;
    EFI_BLOCK_IO_MEDIA *Media;
    ULONG SectorIndex;
    KSTATUS Status;

    BootDiskHandle = (PBOOT_DISK_HANDLE)DiskHandle;
    Media = BootDiskHandle->BlockIo->Media;
    BlockSize = Media->BlockSize;
    for (SectorIndex = 0; SectorIndex < SectorCount; SectorIndex += 1) {
        EfiStatus = BopEfiBlockIoRead(BootDiskHandle->BlockIo,
                                      Media->MediaId,
                                      Sector + SectorIndex,
                                      BlockSize,
                                      Buffer + (SectorIndex * BlockSize));

        if (EFI_ERROR(EfiStatus)) {
            Status = BopEfiStatusToKStatus(EfiStatus);
            goto EfiLoaderBlockIoReadEnd;
        }
    }

    Status = STATUS_SUCCESS;

EfiLoaderBlockIoReadEnd:
    return Status;
}

KSTATUS
BopEfiLoaderBlockIoWrite (
    HANDLE DiskHandle,
    ULONGLONG Sector,
    ULONG SectorCount,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine uses firmware calls to write sectors to a disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to write to.

    Sector - Supplies the zero-based sector number to write to.

    SectorCount - Supplies the number of sectors to write. The supplied buffer
        must be at least this large.

    Buffer - Supplies the buffer containing the data to write to the disk.

Return Value:

    STATUS_SUCCESS if the operation completed successfully.

    STATUS_FIRMWARE_ERROR if the firmware returned an error.

    Other error codes.

--*/

{

    ULONG BlockSize;
    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_STATUS EfiStatus;
    EFI_BLOCK_IO_MEDIA *Media;
    ULONG SectorIndex;
    KSTATUS Status;

    BootDiskHandle = (PBOOT_DISK_HANDLE)DiskHandle;
    Media = BootDiskHandle->BlockIo->Media;
    BlockSize = Media->BlockSize;
    for (SectorIndex = 0; SectorIndex < SectorCount; SectorIndex += 1) {
        EfiStatus = BopEfiBlockIoWrite(BootDiskHandle->BlockIo,
                                       Media->MediaId,
                                       Sector + SectorIndex,
                                       BlockSize,
                                       Buffer + (SectorIndex * BlockSize));

        if (EFI_ERROR(EfiStatus)) {
            Status = BopEfiStatusToKStatus(EfiStatus);
            goto EfiLoaderBlockIoWriteEnd;
        }
    }

    Status = STATUS_SUCCESS;

EfiLoaderBlockIoWriteEnd:
    return Status;
}

ULONG
BopEfiGetDiskBlockSize (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine determines the number of bytes in a sector on the given disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of bytes in a sector on success.

    0 on error.

--*/

{

    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_BLOCK_IO_MEDIA *Media;

    BootDiskHandle = (PBOOT_DISK_HANDLE)DiskHandle;
    Media = BootDiskHandle->BlockIo->Media;
    return Media->BlockSize;
}

ULONGLONG
BopEfiGetDiskBlockCount (
    HANDLE DiskHandle
    )

/*++

Routine Description:

    This routine determines the number of sectors on the disk.

Arguments:

    DiskHandle - Supplies a handle to the disk to query.

Return Value:

    Returns the number of sectors in the disk on success.

    0 on error.

--*/

{

    PBOOT_DISK_HANDLE BootDiskHandle;
    EFI_BLOCK_IO_MEDIA *Media;

    BootDiskHandle = (PBOOT_DISK_HANDLE)DiskHandle;
    Media = BootDiskHandle->BlockIo->Media;
    return Media->LastBlock + 1;
}

KSTATUS
BopEfiGetRamDisks (
    PBOOT_RAM_DISK *RamDisks,
    PULONG RamDiskCount
    )

/*++

Routine Description:

    This routine returns an array of the RAM disks known to the firmware.

Arguments:

    RamDisks - Supplies a pointer where an array of RAM disk structures will
        be allocated and returned. It is the caller's responsibility to free
        this memory.

    RamDiskCount - Supplies a pointer where the count of RAM disks in the
        array will be returned.

Return Value:

    Status code.

--*/

{

    PBOOT_RAM_DISK Array;
    EFI_STATUS EfiStatus;
    UINTN HandleCount;
    UINTN HandleIndex;
    EFI_HANDLE *Handles;
    UINTN NextIndex;
    EFI_RAM_DISK_PROTOCOL *RamDiskProtocol;
    KSTATUS Status;

    Array = NULL;
    HandleCount = 0;
    Handles = NULL;
    NextIndex = 0;

    //
    // Look up all handles that support the RAM Disk protocol.
    //

    EfiStatus = BopEfiLocateHandleBuffer(ByProtocol,
                                         &BoEfiRamDiskProtocolGuid,
                                         NULL,
                                         &HandleCount,
                                         &Handles);

    if (EFI_ERROR(EfiStatus)) {
        Status = BopEfiStatusToKStatus(EfiStatus);
        goto EfiGetRamDisksEnd;
    }

    //
    // Allocate the array.
    //

    Array = BoAllocateMemory(HandleCount * sizeof(BOOT_RAM_DISK));
    if (Array == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EfiGetRamDisksEnd;
    }

    RtlZeroMemory(Array, HandleCount * sizeof(BOOT_RAM_DISK));
    for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex += 1) {
        EfiStatus = BopEfiHandleProtocol(Handles[HandleIndex],
                                         &BoEfiRamDiskProtocolGuid,
                                         (VOID **)&RamDiskProtocol);

        if (EFI_ERROR(EfiStatus)) {
            continue;
        }

        Array[NextIndex].Base = RamDiskProtocol->Base;
        Array[NextIndex].Size = RamDiskProtocol->Length;
        NextIndex += 1;
    }

    Status = STATUS_SUCCESS;

EfiGetRamDisksEnd:
    if (!KSUCCESS(Status)) {
        if (Array != NULL) {
            BoFreeMemory(Array);
        }

        NextIndex = 0;
    }

    if (Handles != NULL) {
        BopEfiFreePool(Handles);
    }

    *RamDiskCount = NextIndex;
    *RamDisks = Array;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
BopEfiBlockIoRead (
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

    EFI status code.

--*/

{

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->ReadBlocks(This, MediaId, Lba, BufferSize, Buffer);
    BopEfiRestoreApplicationContext();
    return Status;
}

EFI_STATUS
BopEfiBlockIoWrite (
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

    EFI_STATUS Status;

    BopEfiRestoreFirmwareContext();
    Status = This->WriteBlocks(This, MediaId, Lba, BufferSize, Buffer);
    BopEfiRestoreApplicationContext();
    return Status;
}

BOOL
BopEfiMatchPartitionDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UCHAR PartitionId[FIRMWARE_PARTITION_ID_SIZE]
    )

/*++

Routine Description:

    This routine determines if the given device path matches the given disk and
    partition IDs. For GPT disks, only the partition ID is matched. For MBR
    disks, the disk ID and partition number embedded in the partition ID is
    matched.

Arguments:

    DevicePath - Supplies a pointer to the device path protocol to match
        against.

    PartitionId - Supplies the partition ID to match.

Return Value:

    TRUE if the given device path matches the partition ID.

    FALSE if the device path does not match the partition ID.

--*/

{

    CHAR DevicePartitionId[FIRMWARE_PARTITION_ID_SIZE];
    HARDDRIVE_DEVICE_PATH *DrivePath;
    BOOL Match;

    while (TRUE) {
        if (DevicePath->Type == END_DEVICE_PATH_TYPE) {
            break;
        }

        //
        // Look for a Hard Drive Media device path.
        //

        if ((DevicePath->Type == MEDIA_DEVICE_PATH) &&
            (DevicePath->SubType == MEDIA_HARDDRIVE_DP)) {

            DrivePath = (HARDDRIVE_DEVICE_PATH *)DevicePath;

            ASSERT(sizeof(DrivePath->Signature) >= sizeof(DevicePartitionId));

            RtlCopyMemory(DevicePartitionId,
                          DrivePath->Signature,
                          sizeof(DevicePartitionId));

            //
            // If the signature type is MBR, then stick the partition number
            // in the second four bytes of the device partition ID, as that's
            // what the partition library does.
            //

            if (DrivePath->SignatureType == SIGNATURE_TYPE_MBR) {

                ASSERT(sizeof(DrivePath->PartitionNumber) >= sizeof(ULONG));

                RtlCopyMemory(&(DevicePartitionId[sizeof(ULONG)]),
                              &(DrivePath->PartitionNumber),
                              sizeof(ULONG));
            }

            //
            // Compare the GUIDs if the partition type is known.
            //

            if ((DrivePath->SignatureType == SIGNATURE_TYPE_MBR) ||
                (DrivePath->SignatureType == SIGNATURE_TYPE_GUID)) {

                Match = RtlCompareMemory(DevicePartitionId,
                                         PartitionId,
                                         FIRMWARE_PARTITION_ID_SIZE);

                if (Match != FALSE) {
                    return TRUE;
                }
            }
        }

        //
        // Move to the next device path.
        //

        if (DevicePath->Length == 0) {

            ASSERT(FALSE);

            break;
        }

        DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)((PVOID)DevicePath +
                                                  DevicePath->Length);
    }

    return FALSE;
}

