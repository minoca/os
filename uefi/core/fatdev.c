/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatdev.c

Abstract:

    This module implements the underlying device support for the FAT library
    in the firmware environment

Author:

    Evan Green 23-Sep-2012

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>
#include <uefifw.h>
#include <minoca/uefi/protocol/diskio.h>
#include <minoca/uefi/protocol/blockio.h>
#include <minoca/uefi/protocol/sfilesys.h>
#include "fatfs.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines an I/O buffer in the firmware environment.

Members:

    Data - Stores a pointer to the I/O buffer's data buffer.

    Size - Stores the size of the memory buffer, in bytes.

    CurrentOffset - Stores the current offset into the I/O buffer. All I/O will
        begin at the current offset.

--*/

typedef struct _EFI_FAT_IO_BUFFER {
    VOID *Data;
    UINTN Size;
    UINTN CurrentOffset;
} EFI_FAT_IO_BUFFER, *PEFI_FAT_IO_BUFFER;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PFAT_IO_BUFFER
FatAllocateIoBuffer (
    PVOID DeviceToken,
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates memory for device I/O use.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Size - Supplies the size of the required allocation, in bytes.

Return Value:

    Returns a pointer to the FAT I/O buffer, or NULL on failure.

--*/

{

    PEFI_FAT_IO_BUFFER IoBuffer;
    EFI_STATUS Status;

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_FAT_IO_BUFFER) + Size,
                             (VOID **)&IoBuffer);

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    IoBuffer->Data = (VOID *)IoBuffer + sizeof(EFI_FAT_IO_BUFFER);
    IoBuffer->Size = Size;
    IoBuffer->CurrentOffset = 0;
    return IoBuffer;
}

PFAT_IO_BUFFER
FatCreateIoBuffer (
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine creates a FAT I/O buffer from the given buffer.

Arguments:

    Buffer - Supplies a pointer to the memory buffer on which to base the FAT
        I/O buffer.

    Size - Supplies the size of the memory buffer, in bytes.

Return Value:

    Returns an pointer to the FAT I/O buffer, or NULL on failure.

--*/

{

    PEFI_FAT_IO_BUFFER IoBuffer;
    EFI_STATUS Status;

    Status = EfiAllocatePool(EfiBootServicesData,
                             sizeof(EFI_FAT_IO_BUFFER),
                             (VOID **)&IoBuffer);

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    IoBuffer->Data = Buffer;
    IoBuffer->Size = Size;
    IoBuffer->CurrentOffset = 0;
    return IoBuffer;
}

VOID
FatIoBufferUpdateOffset (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN OffsetUpdate,
    BOOL Decrement
    )

/*++

Routine Description:

    This routine increments the given FAT I/O buffer's current offset by the
    given amount.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

    OffsetUpdate - Supplies the number of bytes by which the offset will be
        updated.

    Decrement - Supplies a boolean indicating whether the update will be a
        decrement (TRUE) or an increment (FALSE).

Return Value:

    None.

--*/

{

    if (Decrement == FALSE) {
        ((PEFI_FAT_IO_BUFFER)FatIoBuffer)->CurrentOffset += OffsetUpdate;

    } else {
        ((PEFI_FAT_IO_BUFFER)FatIoBuffer)->CurrentOffset -= OffsetUpdate;
    }

    ASSERT(((PEFI_FAT_IO_BUFFER)FatIoBuffer)->CurrentOffset <=
           ((PEFI_FAT_IO_BUFFER)FatIoBuffer)->Size);

    return;
}

VOID
FatIoBufferSetOffset (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN Offset
    )

/*++

Routine Description:

    This routine sets the given FAT I/O buffer's current offset.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

    Offset - Supplies the new offset to set.

Return Value:

    None.

--*/

{

    PEFI_FAT_IO_BUFFER IoBuffer;

    IoBuffer = (PEFI_FAT_IO_BUFFER)FatIoBuffer;
    IoBuffer->CurrentOffset = Offset;

    ASSERT(IoBuffer->CurrentOffset <= IoBuffer->Size);

    return;
}

KSTATUS
FatZeroIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer,
    UINTN Offset,
    UINTN ByteCount
    )

/*++

Routine Description:

    This routine zeros the contents of the FAT I/O buffer starting at the
    offset for the given number of bytes.

Arguments:

    FatIoBuffer - Supplies a pointer to the FAT I/O buffer that is to be zeroed.

    Offset - Supplies the offset within the I/O buffer where the zeroing should
        begin.

    ByteCount - Supplies the number of bytes to zero.

Return Value:

    Status code.

--*/

{

    PEFI_FAT_IO_BUFFER IoBuffer;

    IoBuffer = (PEFI_FAT_IO_BUFFER)FatIoBuffer;
    EfiSetMem(IoBuffer->Data + IoBuffer->CurrentOffset + Offset, ByteCount, 0);
    return STATUS_SUCCESS;
}

KSTATUS
FatCopyIoBuffer (
    PFAT_IO_BUFFER Destination,
    UINTN DestinationOffset,
    PFAT_IO_BUFFER Source,
    UINTN SourceOffset,
    UINTN ByteCount
    )

/*++

Routine Description:

    This routine copies the contents of the source I/O buffer starting at the
    source offset to the destination I/O buffer starting at the destination
    offset. It assumes that the arguments are correct such that the copy can
    succeed.

Arguments:

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contents will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    ByteCount - Supplies the size of the requested copy in bytes.

Return Value:

    Status code.

--*/

{

    VOID *DestinationBuffer;
    PEFI_FAT_IO_BUFFER DestinationIoBuffer;
    VOID *SourceBuffer;
    PEFI_FAT_IO_BUFFER SourceIoBuffer;

    DestinationIoBuffer = (PEFI_FAT_IO_BUFFER)Destination;
    DestinationBuffer = DestinationIoBuffer->Data +
                        DestinationIoBuffer->CurrentOffset +
                        DestinationOffset;

    SourceIoBuffer = (PEFI_FAT_IO_BUFFER)Source;
    SourceBuffer = SourceIoBuffer->Data +
                   SourceIoBuffer->CurrentOffset +
                   SourceOffset;

    EfiCopyMem(DestinationBuffer, SourceBuffer, ByteCount);
    return STATUS_SUCCESS;
}

KSTATUS
FatCopyIoBufferData (
    PFAT_IO_BUFFER FatIoBuffer,
    PVOID Buffer,
    UINTN Offset,
    UINTN Size,
    BOOL ToIoBuffer
    )

/*++

Routine Description:

    This routine copies from a buffer into the given I/O buffer or out of the
    given I/O buffer.

Arguments:

    FatIoBuffer - Supplies a pointer to the FAT I/O buffer to copy in or out of.

    Buffer - Supplies a pointer to the regular linear buffer to copy to or from.

    Offset - Supplies an offset in bytes from the beginning of the I/O buffer
        to copy to or from.

    Size - Supplies the number of bytes to copy.

    ToIoBuffer - Supplies a boolean indicating whether data is copied into the
        I/O buffer (TRUE) or out of the I/O buffer (FALSE).

Return Value:

    Status code.

--*/

{

    VOID *Destination;
    PEFI_FAT_IO_BUFFER IoBuffer;
    VOID *Source;

    IoBuffer = (PEFI_FAT_IO_BUFFER)FatIoBuffer;

    ASSERT(IoBuffer->CurrentOffset + Offset + Size <= IoBuffer->Size);

    Destination = IoBuffer->Data + IoBuffer->CurrentOffset + Offset;
    Source = Buffer;
    if (ToIoBuffer == FALSE) {
        Source = Destination;
        Destination = Buffer;
    }

    EfiCopyMem(Destination, Source, Size);
    return STATUS_SUCCESS;
}

PVOID
FatMapIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer
    )

/*++

Routine Description:

    This routine maps the given FAT I/O buffer and returns the base of the
    virtually contiguous mapping.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

Return Value:

    Returns a pointer to the virtual address of the mapping on success, or
    NULL on failure.

--*/

{

    return ((PEFI_FAT_IO_BUFFER)FatIoBuffer)->Data;
}

VOID
FatFreeIoBuffer (
    PFAT_IO_BUFFER FatIoBuffer
    )

/*++

Routine Description:

    This routine frees a FAT I/O buffer.

Arguments:

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer.

Return Value:

    None.

--*/

{

    EfiFreePool(FatIoBuffer);
    return;
}

PVOID
FatAllocatePagedMemory (
    PVOID DeviceToken,
    ULONG SizeInBytes
    )

/*++

Routine Description:

    This routine allocates paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    SizeInBytes - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the allocated memory, or NULL on failure.

--*/

{

    VOID *Allocation;
    EFI_STATUS Status;

    Status = EfiAllocatePool(EfiBootServicesData,
                             SizeInBytes,
                             &Allocation);

    if (EFI_ERROR(Status)) {
        return NULL;
    }

    return Allocation;
}

PVOID
FatAllocateNonPagedMemory (
    PVOID DeviceToken,
    ULONG SizeInBytes
    )

/*++

Routine Description:

    This routine allocates non-paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    SizeInBytes - Supplies the number of bytes to allocate.

Return Value:

    Returns a pointer to the allocated memory, or NULL on failure.

--*/

{

    return FatAllocatePagedMemory(DeviceToken, SizeInBytes);
}

VOID
FatFreePagedMemory (
    PVOID DeviceToken,
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees paged memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

{

    EfiFreePool(Allocation);
    return;
}

VOID
FatFreeNonPagedMemory (
    PVOID DeviceToken,
    PVOID Allocation
    )

/*++

Routine Description:

    This routine frees memory for the FAT library.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    Allocation - Supplies a pointer to the allocation to free.

Return Value:

    None.

--*/

{

    FatFreePagedMemory(DeviceToken, Allocation);
    return;
}

KSTATUS
FatCreateLock (
    PVOID *Lock
    )

/*++

Routine Description:

    This routine creates a lock.

Arguments:

    Lock - Supplies a pointer where an opaque pointer will be returned
        representing the lock.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the lock could not be allocated.

--*/

{

    *Lock = NULL;
    return STATUS_SUCCESS;
}

VOID
FatDestroyLock (
    PVOID Lock
    )

/*++

Routine Description:

    This routine destroys a created lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

{

    return;
}

VOID
FatAcquireLock (
    PVOID Lock
    )

/*++

Routine Description:

    This routine acquires a lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

{

    return;
}

VOID
FatReleaseLock (
    PVOID Lock
    )

/*++

Routine Description:

    This routine releases a lock.

Arguments:

    Lock - Supplies a the opaque pointer returned upon creation.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
FatReadDevice (
    PVOID DeviceToken,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    ULONG Flags,
    PVOID Irp,
    PFAT_IO_BUFFER FatIoBuffer
    )

/*++

Routine Description:

    This routine reads data from the underlying disk.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    BlockAddress - Supplies the block index to read (for physical disks, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to the IRP to pass to the read file
        function.

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer where the data from
        the disk will be returned.

Return Value:

    Status code.

--*/

{

    VOID *Buffer;
    PEFI_FAT_VOLUME Device;
    EFI_STATUS EfiStatus;
    PEFI_FAT_IO_BUFFER IoBuffer;
    KSTATUS Status;

    Device = (PEFI_FAT_VOLUME)DeviceToken;
    IoBuffer = (PEFI_FAT_IO_BUFFER)FatIoBuffer;

    ASSERT(IoBuffer != NULL);
    ASSERT(Device->Magic == EFI_FAT_VOLUME_MAGIC);
    ASSERT((ULONG)BlockCount == BlockCount);
    ASSERT((IoBuffer->Size - IoBuffer->CurrentOffset) >=
           (BlockCount * Device->BlockSize));

    Status = STATUS_SUCCESS;
    Buffer = IoBuffer->Data + IoBuffer->CurrentOffset;
    EfiStatus = Device->DiskIo->ReadDisk(Device->DiskIo,
                                         Device->MediaId,
                                         BlockAddress * Device->BlockSize,
                                         BlockCount * Device->BlockSize,
                                         Buffer);

    if (EfiStatus == EFI_MEDIA_CHANGED) {
        Status = STATUS_DEVICE_IO_ERROR;

    } else if (EfiStatus == EFI_NO_MEDIA) {
        Status = STATUS_NO_MEDIA;

    } else if (EFI_ERROR(EfiStatus)) {
        Status = STATUS_DEVICE_IO_ERROR;
    }

    if (!KSUCCESS(Status)) {
        goto ReadDeviceEnd;
    }

    Status = STATUS_SUCCESS;

ReadDeviceEnd:
    return Status;
}

KSTATUS
FatWriteDevice (
    PVOID DeviceToken,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    ULONG Flags,
    PVOID Irp,
    PFAT_IO_BUFFER FatIoBuffer
    )

/*++

Routine Description:

    This routine writes data to the underlying disk.

Arguments:

    DeviceToken - Supplies an opaque token identifying the underlying device.

    BlockAddress - Supplies the block index to write to (for physical disks,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    Flags - Supplies flags regarding the I/O operation. See IO_FLAG_*
        definitions.

    Irp - Supplies an optional pointer to an IRP to use for the disk operation.

    FatIoBuffer - Supplies a pointer to a FAT I/O buffer containing the data to
        write.

Return Value:

    Status code.

--*/

{

    VOID *Buffer;
    PEFI_FAT_VOLUME Device;
    EFI_STATUS EfiStatus;
    PEFI_FAT_IO_BUFFER IoBuffer;
    KSTATUS Status;

    ASSERT(FatIoBuffer != NULL);

    Device = (PEFI_FAT_VOLUME)DeviceToken;
    IoBuffer = (PEFI_FAT_IO_BUFFER)FatIoBuffer;
    Buffer = IoBuffer->Data + IoBuffer->CurrentOffset;
    Status = STATUS_SUCCESS;
    EfiStatus = Device->DiskIo->WriteDisk(Device->DiskIo,
                                          Device->MediaId,
                                          BlockAddress * Device->BlockSize,
                                          BlockCount * Device->BlockSize,
                                          Buffer);

    if (EfiStatus == EFI_MEDIA_CHANGED) {
        Status = STATUS_DEVICE_IO_ERROR;

    } else if (EfiStatus == EFI_NO_MEDIA) {
        Status = STATUS_NO_MEDIA;

    } else if (EFI_ERROR(EfiStatus)) {
        Status = STATUS_DEVICE_IO_ERROR;
    }

    if (!KSUCCESS(Status)) {
        goto WriteDeviceEnd;
    }

WriteDeviceEnd:
    return Status;
}

KSTATUS
FatGetDeviceBlockInformation (
    PVOID DeviceToken,
    PFILE_BLOCK_INFORMATION BlockInformation
    )

/*++

Routine Description:

    This routine converts a file's block information into disk level block
    information by modifying the offsets of each contiguous run.

Arguments:

    DeviceToken - Supplies an opaque token identify the underlying device.

    BlockInformation - Supplies a pointer to the block information to be
        updated.

Return Value:

    Status code.

--*/

{

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

ULONG
FatGetIoCacheEntryDataSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of data stored in each cache entry.

Arguments:

    None.

Return Value:

    Returns the size of the data stored in each cache entry, or 0 if there is
    no cache.

--*/

{

    return 0;
}

ULONG
FatGetPageSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the size of a physical memory page for the current FAT
    environment.

Arguments:

    None.

Return Value:

    Returns the size of a page in the current environment. Returns 0 if the
    size is not known.

--*/

{

    return EFI_PAGE_SIZE;
}

VOID
FatGetCurrentSystemTime (
    PSYSTEM_TIME SystemTime
    )

/*++

Routine Description:

    This routine returns the current system time.

Arguments:

    SystemTime - Supplies a pointer where the current system time will be
        returned.

Return Value:

    None.

--*/

{

    EfiSetMem(SystemTime, sizeof(SYSTEM_TIME), 0);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

