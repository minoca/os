/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fatio.c

Abstract:

    This module implements the underlying device support for the FAT library
    when running as a kernel-mode driver.

Author:

    Evan Green 25-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/lib/fat/fat.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores information about a block device backing a FAT file
    system.

Members:

    BlockDevice - Stores the block device parameters for the device.

--*/

typedef struct _FAT_DEVICE {
    BLOCK_DEVICE_PARAMETERS BlockDevice;
} FAT_DEVICE, *PFAT_DEVICE;

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

    PIO_BUFFER IoBuffer;

    IoBuffer = MmAllocateUninitializedIoBuffer(Size, 0);
    return (PFAT_IO_BUFFER)IoBuffer;
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

    PIO_BUFFER IoBuffer;
    KSTATUS Status;

    Status = MmCreateIoBuffer(Buffer,
                              Size,
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &IoBuffer);

    if (!KSUCCESS(Status)) {
        return NULL;
    }

    return (PFAT_IO_BUFFER)IoBuffer;
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
        MmIoBufferIncrementOffset((PIO_BUFFER)FatIoBuffer, OffsetUpdate);

    } else {
        MmIoBufferDecrementOffset((PIO_BUFFER)FatIoBuffer, OffsetUpdate);
    }

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

    MmSetIoBufferCurrentOffset((PIO_BUFFER)FatIoBuffer, Offset);
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

    return MmZeroIoBuffer((PIO_BUFFER)FatIoBuffer, Offset, ByteCount);
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

    KSTATUS Status;

    Status = MmCopyIoBuffer((PIO_BUFFER)Destination,
                            DestinationOffset,
                            (PIO_BUFFER)Source,
                            SourceOffset,
                            ByteCount);

    return Status;
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

    KSTATUS Status;

    Status = MmCopyIoBufferData((PIO_BUFFER)FatIoBuffer,
                                Buffer,
                                Offset,
                                Size,
                                ToIoBuffer);

    return Status;
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

    PIO_BUFFER IoBuffer;
    KSTATUS Status;

    IoBuffer = (PFAT_IO_BUFFER)FatIoBuffer;
    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, TRUE);
    if (!KSUCCESS(Status)) {
        return NULL;
    }

    return IoBuffer->Fragment[0].VirtualAddress;
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

    MmFreeIoBuffer((PIO_BUFFER)FatIoBuffer);
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

    return MmAllocatePagedPool(SizeInBytes, FAT_ALLOCATION_TAG);
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

    return MmAllocateNonPagedPool(SizeInBytes, FAT_ALLOCATION_TAG);
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

    MmFreePagedPool(Allocation);
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

    MmFreeNonPagedPool(Allocation);
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

    PQUEUED_LOCK NewLock;

    NewLock = KeCreateQueuedLock();
    if (NewLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    *Lock = NewLock;
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

    KeDestroyQueuedLock(Lock);
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

    KeAcquireQueuedLock(Lock);
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

    KeReleaseQueuedLock(Lock);
    return;
}

KSTATUS
FatOpenDevice (
    PBLOCK_DEVICE_PARAMETERS BlockParameters
    )

/*++

Routine Description:

    This routine opens the underlying device that the FAT file system reads
    and writes blocks to.

Arguments:

    BlockParameters - Supplies the initial block device parameters for the
        device. These parameters may be modified by this call.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if there were not enough resources to open
    the device.

--*/

{

    PFAT_DEVICE FatDevice;

    FatDevice = MmAllocateNonPagedPool(sizeof(FAT_DEVICE), FAT_ALLOCATION_TAG);
    if (FatDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    FatDevice->BlockDevice = *BlockParameters;

    //
    // Overwrite the device token so this layer gets this pointer.
    //

    BlockParameters->DeviceToken = FatDevice;
    return STATUS_SUCCESS;
}

VOID
FatCloseDevice (
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine closes the device backing the FAT file system.

Arguments:

    DeviceToken - Supplies a pointer to the device token returned upon opening
        the underlying device.

Return Value:

    None.

--*/

{

    MmFreeNonPagedPool(DeviceToken);
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

    ULONG BlockSize;
    UINTN BytesCompleted;
    PFAT_DEVICE FatDevice;
    PIO_BUFFER IoBuffer;
    UINTN SizeInBytes;
    KSTATUS Status;

    IoBuffer = (PIO_BUFFER)FatIoBuffer;
    FatDevice = (PFAT_DEVICE)DeviceToken;
    BlockSize = FatDevice->BlockDevice.BlockSize;
    SizeInBytes = BlockCount * BlockSize;

    ASSERT(IoBuffer != NULL);

    Status = IoReadAtOffset(FatDevice->BlockDevice.DeviceToken,
                            IoBuffer,
                            BlockAddress * BlockSize,
                            SizeInBytes,
                            Flags,
                            WAIT_TIME_INDEFINITE,
                            &BytesCompleted,
                            Irp);

    if (!KSUCCESS(Status)) {
        goto ReadDeviceEnd;
    }

    if (BytesCompleted != SizeInBytes) {

        ASSERT(FALSE);

        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadDeviceEnd;
    }

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

    ULONG BlockSize;
    UINTN BytesCompleted;
    PFAT_DEVICE FatDevice;
    KSTATUS Status;

    FatDevice = (PFAT_DEVICE)DeviceToken;
    BlockSize = FatDevice->BlockDevice.BlockSize;
    Status = IoWriteAtOffset(FatDevice->BlockDevice.DeviceToken,
                             (PIO_BUFFER)FatIoBuffer,
                             BlockAddress * BlockSize,
                             BlockCount * BlockSize,
                             Flags,
                             WAIT_TIME_INDEFINITE,
                             &BytesCompleted,
                             Irp);

    if (!KSUCCESS(Status)) {
        goto WriteDeviceEnd;
    }

    if (BytesCompleted != (BlockCount * BlockSize)) {

        ASSERT(FALSE);

        Status = STATUS_DATA_LENGTH_MISMATCH;
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

    PFAT_DEVICE FatDevice;
    KSTATUS Status;

    ASSERT(BlockInformation != NULL);

    FatDevice = (PFAT_DEVICE)DeviceToken;
    Status = IoGetFileBlockInformation(FatDevice->BlockDevice.DeviceToken,
                                       &BlockInformation);

    return Status;
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

    return IoGetCacheEntryDataSize();
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

    return MmPageSize();
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

    KeGetSystemTime(SystemTime);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

