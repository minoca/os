/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    cachedio.c

Abstract:

    This module implements I/O routines for cacheable I/O objects.

Author:

    Chris Stevens - 12-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context needed to iterate over write operations
    to the page cache. It is supplied to cache hit and miss routines.

Members:

    FileSize - Stores the size of the file being operated on.

    FileOffset - Store the current page-aligned offset into the file where the
        write is to be performed.

    BytesRemaining - Stores the number of bytes remaining to write.

    BytesCompleted - Stores the number of bytes already written.

    SourceOffset - Stores the current offset into the source buffer where the
        data should be copied from for the write.

    SourceBuffer - Stores a pointer to the source data for the write operation.

    CacheBuffer - Stores a pointer to the cache buffer to be used for the flush
        on synchronized writes.

    BytesThisRound - Stores the number of bytes to be written during the
        current round of I/O.

    PageByteOffset - Stores the offset into a page where the write is to be
        performed. The file offset plus the page byte offset gets the exact
        byte offset.

--*/

typedef struct _IO_WRITE_CONTEXT {
    ULONGLONG FileSize;
    ULONGLONG FileOffset;
    UINTN BytesRemaining;
    UINTN BytesCompleted;
    UINTN SourceOffset;
    PIO_BUFFER SourceBuffer;
    PIO_BUFFER CacheBuffer;
    ULONG BytesThisRound;
    ULONG PageByteOffset;
} IO_WRITE_CONTEXT, *PIO_WRITE_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopPerformCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopPerformCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopHandleCacheWriteMiss (
    PFILE_OBJECT FileObject,
    PIO_WRITE_CONTEXT WriteContext,
    ULONG Flags,
    ULONG TimeoutInMilliseconds
    );

KSTATUS
IopHandleCacheWriteHit (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PIO_WRITE_CONTEXT WriteContext,
    ULONG Flags
    );

KSTATUS
IopHandleCacheReadMiss (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopPerformCachedIoBufferWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    );

KSTATUS
IopPerformDefaultNonCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext
    );

KSTATUS
IopPerformDefaultNonCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext,
    BOOL UpdateFileSize
    );

KSTATUS
IopPerformDefaultPartialWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext,
    UINTN IoBufferOffset,
    BOOL UpdateFileSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopPerformCacheableIoOperation (
    PIO_HANDLE Handle,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine reads from or writes to the given handle. The I/O object type
    of the given handle must be cacheable.

Arguments:

    Handle - Supplies a pointer to the I/O handle.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in or out. Check the bytes completed value in the I/O context to find out
    how much occurred.

--*/

{

    PFILE_OBJECT FileObject;
    BOOL Modified;
    ULONGLONG OriginalOffset;
    KSTATUS Status;

    FileObject = Handle->PathPoint.PathEntry->FileObject;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT((IoContext->Flags & IO_FLAG_NO_ALLOCATE) == 0);
    ASSERT(IO_IS_OBJECT_TYPE_CACHEABLE(FileObject->Properties.Type) != FALSE);

    //
    // Get the current offset if an offset is not provided.
    //

    OriginalOffset = IoContext->Offset;
    if (OriginalOffset == IO_OFFSET_NONE) {
        IoContext->Offset = RtlAtomicOr64(&(Handle->CurrentOffset), 0);
    }

    //
    // If this is a write operation, then acquire the file object's lock
    // exclusively and perform the cached write.
    //

    if (IoContext->Write != FALSE) {
        if (FileObject->Lock != NULL) {
            KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
        }

        //
        // In append mode, set the offset to the end of the file.
        //

        if ((Handle->OpenFlags & OPEN_FLAG_APPEND) != 0) {
            READ_INT64_SYNC(&(FileObject->FileSize), &(IoContext->Offset));
        }

        if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
            Status = IopPerformCachedWrite(FileObject, IoContext);

        } else {
            Status = IopPerformNonCachedWrite(FileObject,
                                              IoContext,
                                              Handle->DeviceContext,
                                              TRUE);
        }

        if (FileObject->Lock != NULL) {
            KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
        }

        Modified = TRUE;

    //
    // Read operations acquire the file object's lock in shared mode and then
    // perform the cached read.
    //

    } else {
        if (FileObject->Lock != NULL) {
            KeAcquireSharedExclusiveLockShared(FileObject->Lock);
        }

        if (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE) {
            Status = IopPerformCachedRead(FileObject, IoContext);

        } else {
            Status = IopPerformNonCachedRead(FileObject,
                                             IoContext,
                                             Handle->DeviceContext);
        }

        if (FileObject->Lock != NULL) {
            KeReleaseSharedExclusiveLockShared(FileObject->Lock);
        }

        Modified = FALSE;
    }

    //
    // Update the access and modified times if some bytes were read or written.
    //

    if (IoContext->BytesCompleted != 0) {
        if ((Modified != FALSE) ||
            ((Handle->OpenFlags & OPEN_FLAG_NO_ACCESS_TIME) == 0)) {

            IopUpdateFileObjectTime(FileObject, Modified);
        }
    }

    //
    // If no offset was provided, update the current offset.
    //

    if (OriginalOffset == IO_OFFSET_NONE) {
        RtlAtomicAdd64(&(Handle->CurrentOffset), IoContext->BytesCompleted);
    }

    return Status;
}

KSTATUS
IopPerformNonCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext
    )

/*++

Routine Description:

    This routine performs a non-cached read from a cacheable file object. It is
    assumed that the file lock is held.

Arguments:

    FileObject - Supplies a pointer to a cacheable file object.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

Return Value:

    Status code.

--*/

{

    IO_OBJECT_TYPE IoObjectType;
    KSTATUS Status;

    IoObjectType = FileObject->Properties.Type;

    ASSERT(IoContext->Write == FALSE);
    ASSERT(IO_IS_OBJECT_TYPE_CACHEABLE(IoObjectType) != FALSE);

    switch (IoObjectType) {
    case IoObjectSharedMemoryObject:
        Status = IopPerformSharedMemoryIoOperation(FileObject,
                                                   IoContext,
                                                   FALSE);

        break;

    default:
        Status = IopPerformDefaultNonCachedRead(FileObject,
                                                IoContext,
                                                DeviceContext);

        break;
    }

    return Status;
}

KSTATUS
IopPerformNonCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext,
    BOOL UpdateFileSize
    )

/*++

Routine Description:

    This routine performs a non-cached write to a cacheable file object. It is
    assumed that the file lock is held. This routine will always modify the
    file size in the the file properties and conditionally modify the file size
    in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

    UpdateFileSize - Supplies a boolean indicating whether or not this routine
        should update the file size in the file object. Only the page cache
        code should supply FALSE.

Return Value:

    Status code.

--*/

{

    IO_OBJECT_TYPE IoObjectType;
    KSTATUS Status;

    ASSERT(IoContext->Write != FALSE);

    IoObjectType = FileObject->Properties.Type;
    if (IO_IS_OBJECT_TYPE_CACHEABLE(IoObjectType) == FALSE) {

        ASSERT(IO_IS_OBJECT_TYPE_CACHEABLE(IoObjectType) != FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    switch (IoObjectType) {
    case IoObjectSharedMemoryObject:
        Status = IopPerformSharedMemoryIoOperation(FileObject,
                                                   IoContext,
                                                   UpdateFileSize);

        break;

    default:
        Status = IopPerformDefaultNonCachedWrite(FileObject,
                                                 IoContext,
                                                 DeviceContext,
                                                 UpdateFileSize);

        break;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopPerformCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs reads from the page cache. If any of the reads miss
    the cache, then they are read into the cache. Only cacheable objects are
    supported by this routine.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    UINTN BytesRemaining;
    UINTN BytesThisRound;
    BOOL CacheMiss;
    ULONGLONG CacheMissOffset;
    ULONGLONG CurrentOffset;
    ULONG DestinationByteOffset;
    PIO_BUFFER DestinationIoBuffer;
    ULONGLONG FileSize;
    IO_CONTEXT MissContext;
    UINTN MissSize;
    PIO_BUFFER PageAlignedIoBuffer;
    ULONGLONG PageAlignedOffset;
    UINTN PageAlignedSize;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    ULONGLONG ReadEnd;
    UINTN SizeInBytes;
    KSTATUS Status;
    UINTN TotalBytesRead;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(IoContext->SizeInBytes != 0);
    ASSERT(IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE);
    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeldShared(FileObject->Lock) != FALSE));

    IoContext->BytesCompleted = 0;
    DestinationIoBuffer = IoContext->IoBuffer;
    PageAlignedIoBuffer = NULL;
    PageCacheEntry = NULL;
    PageSize = MmPageSize();
    SizeInBytes = IoContext->SizeInBytes;
    Status = STATUS_SUCCESS;
    TotalBytesRead = 0;

    //
    // Do not read past the end of the file.
    //

    READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
    if (IoContext->Offset >= FileSize) {
        Status = STATUS_END_OF_FILE;
        goto PerformCachedReadEnd;
    }

    //
    // Truncate the size if the read end is beyond the file. Don't let the size
    // wrap.
    //

    ReadEnd = IoContext->Offset + SizeInBytes;
    if (ReadEnd < IoContext->Offset) {
        Status = STATUS_INVALID_PARAMETER;
        goto PerformCachedReadEnd;
    }

    if (ReadEnd > FileSize) {
        SizeInBytes = FileSize - IoContext->Offset;
    }

    //
    // If this left no bytes to read, exit.
    //

    if (SizeInBytes == 0) {
        Status = STATUS_END_OF_FILE;
        goto PerformCachedReadEnd;
    }

    //
    // Page-align the offset and size. Note that the size does not get aligned
    // up to a page, just down.
    //

    PageAlignedOffset = ALIGN_RANGE_DOWN(IoContext->Offset, PageSize);
    DestinationByteOffset = REMAINDER(IoContext->Offset, PageSize);
    PageAlignedSize = SizeInBytes + DestinationByteOffset;

    //
    // Validate the page-aligned I/O buffer, which is currently NULL. If the
    // I/O request is page aligned in offset and size, then there is a chance
    // the I/O buffer could be used directly. Do not use the truncated size
    // here, as full page requests at the end of a file can also use the buffer
    // directly.
    //

    if ((DestinationByteOffset == 0) &&
        (IS_ALIGNED(IoContext->SizeInBytes, PageSize) != FALSE)) {

        PageAlignedIoBuffer = DestinationIoBuffer;
    }

    Status = MmValidateIoBufferForCachedIo(&PageAlignedIoBuffer,
                                           PageAlignedSize,
                                           PageSize);

    if (!KSUCCESS(Status)) {
        goto PerformCachedReadEnd;
    }

    //
    // Iterate over each page, searching for page cache entries or creating
    // new page cache entries if there is a cache miss. Batch any missed reads
    // to limit the calls to the file system.
    //

    CacheMiss = FALSE;
    CurrentOffset = PageAlignedOffset;
    CacheMissOffset = CurrentOffset;
    BytesRemaining = PageAlignedSize;
    while (BytesRemaining != 0) {
        if (BytesRemaining < PageSize) {
            BytesThisRound = BytesRemaining;

        } else {
            BytesThisRound = PageSize;
        }

        //
        // First lookup the page in the page cache. If it is found, great. Fill
        // it in the buffer.
        //

        ASSERT(IS_ALIGNED(CurrentOffset, PageSize) != FALSE);

        PageCacheEntry = IopLookupPageCacheEntry(FileObject,
                                                 CurrentOffset,
                                                 FALSE);

        if (PageCacheEntry != NULL) {

            //
            // Now read in the missed data and add it into the page-aligned
            // buffer.
            //

            if (CacheMiss != FALSE) {

                ASSERT(CurrentOffset < FileSize);
                ASSERT((CurrentOffset - CacheMissOffset) <= MAX_UINTN);

                MissContext.IoBuffer = PageAlignedIoBuffer;
                MissContext.Offset = CacheMissOffset;
                MissSize = (UINTN)(CurrentOffset - CacheMissOffset);
                MissContext.SizeInBytes = MissSize;
                MissContext.Flags = IoContext->Flags;
                MissContext.TimeoutInMilliseconds =
                                              IoContext->TimeoutInMilliseconds;

                MissContext.Write = FALSE;
                Status = IopHandleCacheReadMiss(FileObject, &MissContext);

                //
                // This should not fail due to end of file because the cache
                // was hit after this missed region.
                //

                ASSERT(Status != STATUS_END_OF_FILE);

                if (!KSUCCESS(Status)) {
                    goto PerformCachedReadEnd;
                }

                //
                // Again, assert that all the expected bytes were read.
                //

                ASSERT(MissContext.BytesCompleted ==
                       (CurrentOffset - CacheMissOffset));

                TotalBytesRead += MissContext.BytesCompleted;
                CacheMiss = FALSE;
            }

            //
            // Add the found page to the buffer. This needs to happen after
            // the missed reads are satisfied because the buffer needs to be
            // filled in sequentially.
            //

            MmIoBufferAppendPage(PageAlignedIoBuffer,
                                 PageCacheEntry,
                                 NULL,
                                 INVALID_PHYSICAL_ADDRESS);

            IoPageCacheEntryReleaseReference(PageCacheEntry);
            PageCacheEntry = NULL;
            TotalBytesRead += BytesThisRound;

        //
        // If there was no page cache entry and this is a new cache miss, then
        // mark the start of the miss.
        //

        } else if (CacheMiss == FALSE) {
            CacheMiss = TRUE;
            CacheMissOffset = CurrentOffset;
        }

        CurrentOffset += BytesThisRound;
        BytesRemaining -= BytesThisRound;
    }

    //
    // Handle any final cache read misses.
    //

    if (CacheMiss != FALSE) {

        ASSERT(CurrentOffset <= FileSize);
        ASSERT(CurrentOffset == (IoContext->Offset + SizeInBytes));

        MissContext.IoBuffer = PageAlignedIoBuffer;
        MissContext.Offset = CacheMissOffset;
        MissSize = (UINTN)(CurrentOffset - CacheMissOffset);
        MissContext.SizeInBytes = MissSize;
        MissContext.Flags = IoContext->Flags;
        MissContext.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
        MissContext.Write = FALSE;
        Status = IopHandleCacheReadMiss(FileObject, &MissContext);

        ASSERT(Status != STATUS_END_OF_FILE);

        if (!KSUCCESS(Status)) {
            goto PerformCachedReadEnd;
        }

        ASSERT(MissContext.BytesCompleted == (CurrentOffset - CacheMissOffset));

        TotalBytesRead += MissContext.BytesCompleted;
    }

    //
    // If the destination buffer was not directly filled with page cache
    // entries, copy the data read from the cache into it.
    //

    if (DestinationIoBuffer != PageAlignedIoBuffer) {
        TotalBytesRead -= DestinationByteOffset;

        ASSERT(TotalBytesRead == SizeInBytes);

        Status = MmCopyIoBuffer(DestinationIoBuffer,
                                0,
                                PageAlignedIoBuffer,
                                DestinationByteOffset,
                                TotalBytesRead);

        if (!KSUCCESS(Status)) {
            goto PerformCachedReadEnd;
        }
    }

PerformCachedReadEnd:

    //
    // If the routine was not successful and did not read directly into the
    // destination buffer, then none of the requested work was done.
    //

    if (!KSUCCESS(Status) && (DestinationIoBuffer != PageAlignedIoBuffer)) {
        TotalBytesRead = 0;
    }

    if (PageCacheEntry != NULL) {
        IoPageCacheEntryReleaseReference(PageCacheEntry);
    }

    if ((PageAlignedIoBuffer != NULL) &&
        (PageAlignedIoBuffer != DestinationIoBuffer)) {

        MmFreeIoBuffer(PageAlignedIoBuffer);
        PageAlignedIoBuffer = NULL;
    }

    ASSERT(TotalBytesRead <= SizeInBytes);

    IoContext->BytesCompleted = TotalBytesRead;
    return Status;
}

KSTATUS
IopPerformCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs writes to the page cache. If any of the writes miss
    the cache and it is a complete page of write, a page cache entry is
    created. If a cache miss is not for a complete page's worth of writes, a
    read is performed to cache the page and then this writes into the cache.
    Only cacheable file objects are supported by this routine.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    BOOL CacheBacked;
    UINTN CacheBufferSize;
    PIO_BUFFER CacheIoBuffer;
    IO_CONTEXT CacheIoContext;
    ULONGLONG EndOffset;
    ULONGLONG FileSize;
    ULONGLONG PageAlignedOffset;
    UINTN PageAlignedSize;
    ULONG PageByteOffset;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    UINTN SizeInBytes;
    KSTATUS Status;
    IO_WRITE_CONTEXT WriteContext;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE);
    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE));

    //
    // If the metadata flag is set, the data flag better be set as well.
    //

    ASSERT((IoContext->Flags &
            (IO_FLAG_DATA_SYNCHRONIZED | IO_FLAG_METADATA_SYNCHRONIZED)) !=
           IO_FLAG_METADATA_SYNCHRONIZED);

    IoContext->BytesCompleted = 0;
    CacheBufferSize = 0;
    CacheIoBuffer = NULL;
    WriteContext.BytesCompleted = 0;
    WriteContext.CacheBuffer = NULL;
    PageByteOffset = 0;
    PageSize = MmPageSize();
    SizeInBytes = IoContext->SizeInBytes;

    //
    // Do not allow the system to write beyond the end of block devices.
    //

    if (FileObject->Properties.Type == IoObjectBlockDevice) {
        EndOffset = IoContext->Offset + SizeInBytes;
        READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);
        if (EndOffset > FileSize) {
            SizeInBytes = FileSize - IoContext->Offset;
            if (SizeInBytes == 0) {
                return STATUS_OUT_OF_BOUNDS;
            }
        }
    }

    //
    // In an effort to prevent runaway writers from causing the page cache to
    // fill all of memory, ask the page cache if it's feeling uncomfortably
    // dirty. If so, make this a synchronized write to at least stop the
    // bleeding. This does not set the metadata synchronized flag because that
    // tends to cause very bad non-sequential I/O performance, which matters
    // even on media like SD.
    //

    if (IopIsPageCacheTooDirty() != FALSE) {
        IoContext->Flags |= IO_FLAG_DATA_SYNCHRONIZED;
    }

    //
    // If the the offset is page-aligned, check to see if the buffer is backed
    // by the page cache. If so, pass this on to a short-cut routine that just
    // marks page cache entries dirty. Otherwise, just set the size and page
    // byte offset as appropriate.
    //

    PageAlignedOffset = ALIGN_RANGE_DOWN(IoContext->Offset, PageSize);
    if (PageAlignedOffset == IoContext->Offset) {

        //
        // A buffer is page cache backed if the first fragment has a valid page
        // cache entry for the current file object and at the current offset.
        //

        CacheBacked = IopIsIoBufferPageCacheBacked(FileObject,
                                                   IoContext->IoBuffer,
                                                   IoContext->Offset,
                                                   SizeInBytes);

        if (CacheBacked != FALSE) {
            Status = IopPerformCachedIoBufferWrite(FileObject, IoContext);
            goto PerformCachedWriteEnd;
        }

        //
        // Since the offset is aligned, copies to cache entries will start
        // immediately. And the size remains the same. Never align a write size
        // up; this should not write more than is prescribed.
        //

        PageAlignedSize = SizeInBytes;

    //
    // Otherwise, increase the number of bytes that need to be be written
    // (aligning down) and set the source byte offset. This does not need to
    // align up to a full page.
    //

    } else {
        PageByteOffset = IoContext->Offset - PageAlignedOffset;
        PageAlignedSize = PageByteOffset + SizeInBytes;

        ASSERT(PageByteOffset < PageSize);
    }

    //
    // If this is a synchronized operation, then the "bytes completed" reported
    // back to the caller have to be accurate for the disk. As such, this
    // routine needs to create a page-aligned cache-backed buffer that will get
    // filled in with the cached data along the way. Once everything is cached,
    // it can try to flush the data and report back what made it to disk.
    //

    if ((IoContext->Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {
        CacheBufferSize = ALIGN_RANGE_UP(PageAlignedSize, PageSize);
        CacheIoBuffer = MmAllocateUninitializedIoBuffer(CacheBufferSize,
                                                        TRUE,
                                                        TRUE);

        if (CacheIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PerformCachedWriteEnd;
        }
    }

    READ_INT64_SYNC(&(FileObject->FileSize), &FileSize);

    //
    // Iterate over each page, searching for page cache entries to copy into.
    //

    WriteContext.FileSize = FileSize;
    WriteContext.FileOffset = PageAlignedOffset;
    WriteContext.BytesRemaining = SizeInBytes;
    WriteContext.SourceOffset = 0;
    WriteContext.SourceBuffer = IoContext->IoBuffer;
    WriteContext.CacheBuffer = CacheIoBuffer;
    WriteContext.BytesThisRound = 0;
    WriteContext.PageByteOffset = PageByteOffset;
    while (WriteContext.BytesRemaining != 0) {

        //
        // Move to the next page if the last page was completed.
        //

        if (WriteContext.PageByteOffset >= PageSize) {
            WriteContext.PageByteOffset = 0;
        }

        //
        // Determine how many bytes to handle this round.
        //

        if (WriteContext.PageByteOffset != 0) {
            WriteContext.BytesThisRound = PageSize -
                                          WriteContext.PageByteOffset;

            if (WriteContext.BytesThisRound > WriteContext.BytesRemaining) {
                WriteContext.BytesThisRound = WriteContext.BytesRemaining;
            }

        } else {
            if (WriteContext.BytesRemaining >= PageSize) {
                WriteContext.BytesThisRound = PageSize;

            } else {
                WriteContext.BytesThisRound = WriteContext.BytesRemaining;
            }
        }

        //
        // Look for the page in the page cache and if it is found, hand the
        // work off to the cache write hit routine.
        //

        ASSERT(IS_ALIGNED(WriteContext.FileOffset, PageSize) != FALSE);

        PageCacheEntry = IopLookupPageCacheEntry(FileObject,
                                                 WriteContext.FileOffset,
                                                 TRUE);

        if (PageCacheEntry != NULL) {
            Status = IopHandleCacheWriteHit(PageCacheEntry,
                                            &WriteContext,
                                            IoContext->Flags);

            IoPageCacheEntryReleaseReference(PageCacheEntry);
            if (!KSUCCESS(Status)) {
                goto PerformCachedWriteEnd;
            }

        //
        // If no page cache entry was found at this file offset, then handle
        // the write miss.
        //

        } else {
            Status = IopHandleCacheWriteMiss(FileObject,
                                             &WriteContext,
                                             IoContext->Flags,
                                             IoContext->TimeoutInMilliseconds);

            if (!KSUCCESS(Status)) {
                goto PerformCachedWriteEnd;
            }
        }

        WriteContext.PageByteOffset += WriteContext.BytesThisRound;
        WriteContext.SourceOffset += WriteContext.BytesThisRound;
        WriteContext.BytesRemaining -= WriteContext.BytesThisRound;
        WriteContext.FileOffset += PageSize;
    }

    //
    // There is still work left to do if this is a synchronized operation. So
    // far, everything is in the cache, but not necessarily on disk! The cache
    // buffer contains a buffer with all the page-aligned data that is in the
    // cache. Flush it out.
    //

    if ((IoContext->Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {

        ASSERT(WriteContext.CacheBuffer == CacheIoBuffer);

        CacheIoContext.IoBuffer = CacheIoBuffer;
        CacheIoContext.Offset = PageAlignedOffset;
        CacheIoContext.SizeInBytes = PageAlignedSize;
        CacheIoContext.Flags = IoContext->Flags;
        CacheIoContext.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
        CacheIoContext.Write = TRUE;
        Status = IopPerformCachedIoBufferWrite(FileObject, &CacheIoContext);
        if (CacheIoContext.BytesCompleted > WriteContext.BytesCompleted) {
            IoContext->BytesCompleted = WriteContext.BytesCompleted;

        } else {
            IoContext->BytesCompleted = CacheIoContext.BytesCompleted;
        }

        if (!KSUCCESS(Status)) {
            goto PerformCachedWriteEnd;
        }
    }

    Status = STATUS_SUCCESS;

PerformCachedWriteEnd:

    //
    // If this is not synchronized I/O and something was written, update the
    // file size and notify the page cache that it's dirty.
    //

    if ((IoContext->Flags & IO_FLAG_DATA_SYNCHRONIZED) == 0) {
        if (IoContext->BytesCompleted == 0) {
            IoContext->BytesCompleted = WriteContext.BytesCompleted;
        }

        if (IoContext->BytesCompleted != 0) {
            FileSize = IoContext->Offset + IoContext->BytesCompleted;
            IopUpdateFileObjectFileSize(FileObject, FileSize, TRUE, FALSE);
            IopNotifyPageCacheWrite();
        }
    }

    ASSERT((WriteContext.CacheBuffer == NULL) ||
           (WriteContext.CacheBuffer == CacheIoBuffer));

    if (CacheIoBuffer != NULL) {
        MmFreeIoBuffer(CacheIoBuffer);
    }

    return Status;
}

KSTATUS
IopHandleCacheWriteMiss (
    PFILE_OBJECT FileObject,
    PIO_WRITE_CONTEXT WriteContext,
    ULONG Flags,
    ULONG TimeoutInMilliseconds
    )

/*++

Routine Description:

    This routine handles cache misses when executing a write to the cache. It
    handles a few cases. The first is a partial write. In this case it must
    first read in the missed data at the page aligned file offset and then
    copy the partial page to the cache. The second is a full page miss. In this
    case it can just create a new page cache entry with the data provided.

Arguments:

    FileObject - Supplies a pointer to the file object that is the target of
        the I/O operation.

    WriteContext - Supplies a pointer to the I/O context that stores the
        current processing information for the write operation. This routine
        updates this information if it processes any bytes.

    Flags - Supplies a bitmask of flags for this I/O operation. See IO_FLAG_*
        for definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    ULONGLONG FileOffset;
    PSHARED_EXCLUSIVE_LOCK IoLock;
    IO_CONTEXT MissContext;
    ULONG PageByteOffset;
    IO_BUFFER PageCacheBuffer;
    BOOL PageCacheBufferInitialized;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PIO_BUFFER ScratchIoBuffer;
    PPAGE_CACHE_ENTRY SourceEntry;
    KSTATUS Status;
    PVOID VirtualAddress;
    ULONG ZeroSize;

    PageCacheBufferInitialized = FALSE;
    PageCacheEntry = NULL;
    PageSize = MmPageSize();
    ScratchIoBuffer = NULL;

    //
    // Handle partial page writes. Partial page cache misses need to read in
    // the page, create a cache entry and then write the data. The exceptions
    // are if this is a page aligned write that goes up to or beyond the end
    // of the file or this is a non-aligned write and the entire page is beyond
    // the end of the file. Nothing need be read in and those are handled in
    // the "else" clause.
    //

    if (((WriteContext->PageByteOffset != 0) &&
         (WriteContext->FileSize > WriteContext->FileOffset)) ||
        ((WriteContext->PageByteOffset == 0) &&
         (WriteContext->BytesRemaining < PageSize) &&
         ((WriteContext->FileOffset + WriteContext->BytesRemaining) <
          WriteContext->FileSize))) {

        //
        // Prepare a one page I/O buffer to collect the missing page cache
        // entry from the read.
        //

        Status = MmInitializeIoBuffer(&PageCacheBuffer,
                                      NULL,
                                      INVALID_PHYSICAL_ADDRESS,
                                      0,
                                      TRUE,
                                      FALSE,
                                      TRUE);

        if (!KSUCCESS(Status)) {
            goto HandleCacheWriteMissEnd;
        }

        PageCacheBufferInitialized = TRUE;

        //
        // Perform the read as if it were a cache miss on read, complete with
        // the normal read ahead behavior.
        //

        MissContext.IoBuffer = &PageCacheBuffer;
        MissContext.Offset = WriteContext->FileOffset;
        MissContext.SizeInBytes = PageSize;
        MissContext.Flags = Flags;
        MissContext.TimeoutInMilliseconds = TimeoutInMilliseconds;
        MissContext.Write = TRUE;
        Status = IopHandleCacheReadMiss(FileObject, &MissContext);
        if (!KSUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
            goto HandleCacheWriteMissEnd;
        }

        ASSERT(MissContext.BytesCompleted == PageSize);

        //
        // Copy the data to this new page cache entry.
        //

        Status = MmCopyIoBuffer(&PageCacheBuffer,
                                WriteContext->PageByteOffset,
                                WriteContext->SourceBuffer,
                                WriteContext->SourceOffset,
                                WriteContext->BytesThisRound);

        if (!KSUCCESS(Status)) {
            goto HandleCacheWriteMissEnd;
        }

        //
        // This does not take a reference on the page cache entry. The buffer
        // holds the reference.
        //

        PageCacheEntry = MmGetIoBufferPageCacheEntry(&PageCacheBuffer, 0);

        ASSERT(PageCacheEntry != NULL);

        IoMarkPageCacheEntryDirty(PageCacheEntry,
                                  WriteContext->PageByteOffset,
                                  WriteContext->BytesThisRound,
                                  FALSE);

        //
        // If this is a synchronized I/O operation, then prepare the cache
        // buffer for the flush by adding in the page cache entry.
        //

        if ((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {

            ASSERT(WriteContext->CacheBuffer != NULL);

            MmIoBufferAppendPage(WriteContext->CacheBuffer,
                                 PageCacheEntry,
                                 NULL,
                                 INVALID_PHYSICAL_ADDRESS);
        }

        WriteContext->BytesCompleted += WriteContext->BytesThisRound;

    //
    // Otherwise this should be a page-aligned cache miss that is either a
    // full page write or a write up to or beyond the end of the file. Try to
    // write out a new cache entry.
    //

    } else {

        ASSERT(((WriteContext->PageByteOffset != 0) &&
                (WriteContext->FileSize <= WriteContext->FileOffset)) ||
               ((WriteContext->PageByteOffset == 0) &&
                ((WriteContext->BytesRemaining >= PageSize) ||
                 ((WriteContext->FileOffset + WriteContext->BytesRemaining) >=
                  WriteContext->FileSize))));

        if (IS_ALIGNED(WriteContext->SourceOffset, PageSize) != FALSE) {
            SourceEntry = MmGetIoBufferPageCacheEntry(
                                                   WriteContext->SourceBuffer,
                                                   WriteContext->SourceOffset);

        } else {
            SourceEntry = NULL;
        }

        //
        // If there is no source page cache entry or the source and destination
        // cannot be linked, allocate a new page, copy the supplied data to it
        // and then insert it into the cache. Unfortunately, it's not
        // guaranteed that the physical page behind the supplied buffer can be
        // used. It could be from paged pool, or user mode.
        //

        if ((SourceEntry == NULL) ||
            (IopCanLinkPageCacheEntry(SourceEntry, FileObject) == FALSE)) {

            ScratchIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                                         MAX_ULONGLONG,
                                                         PageSize,
                                                         PageSize,
                                                         FALSE,
                                                         FALSE,
                                                         FALSE);

            if (ScratchIoBuffer == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto HandleCacheWriteMissEnd;
            }

            ASSERT(ScratchIoBuffer->FragmentCount == 1);

            //
            // If this write does not start at the beginning of the page, zero
            // the contents before the write.
            //

            PageByteOffset = WriteContext->PageByteOffset;
            if (PageByteOffset != 0) {
                MmZeroIoBuffer(ScratchIoBuffer, 0, PageByteOffset);
            }

            //
            // Copy the contents of the source to the new I/O buffer.
            //

            Status = MmCopyIoBuffer(ScratchIoBuffer,
                                    PageByteOffset,
                                    WriteContext->SourceBuffer,
                                    WriteContext->SourceOffset,
                                    WriteContext->BytesThisRound);

            if (!KSUCCESS(Status)) {
                goto HandleCacheWriteMissEnd;
            }

            PageByteOffset += WriteContext->BytesThisRound;

            //
            // Zero the rest of the scratch buffer if the bytes this round did
            // not fill it. It should already be mapped and only be one
            // fragment long.
            //

            if (PageByteOffset < PageSize) {
                ZeroSize = PageSize - PageByteOffset;
                MmZeroIoBuffer(ScratchIoBuffer, PageByteOffset, ZeroSize);
            }

            SourceEntry = NULL;
            PhysicalAddress = ScratchIoBuffer->Fragment[0].PhysicalAddress;
            VirtualAddress = ScratchIoBuffer->Fragment[0].VirtualAddress;

        } else {
            PhysicalAddress = IoGetPageCacheEntryPhysicalAddress(SourceEntry);
            VirtualAddress = IoGetPageCacheEntryVirtualAddress(SourceEntry);
        }

        //
        // If the file object has a lock, it should be held exclusively and
        // this insert cannot race with a read or write. Consider it created.
        //

        IoLock = FileObject->Lock;
        if (IoLock != NULL) {

            ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoLock) != FALSE);

            FileOffset = WriteContext->FileOffset;
            PageCacheEntry = IopCreateAndInsertPageCacheEntry(FileObject,
                                                              VirtualAddress,
                                                              PhysicalAddress,
                                                              FileOffset,
                                                              SourceEntry,
                                                              TRUE);

            Created = TRUE;

        //
        // If there is no lock, then it could race with other inserts (e.g.
        // device read ahead). Use the create or lookup routine.
        //

        } else {
            FileOffset = WriteContext->FileOffset;
            PageCacheEntry = IopCreateOrLookupPageCacheEntry(FileObject,
                                                             VirtualAddress,
                                                             PhysicalAddress,
                                                             FileOffset,
                                                             SourceEntry,
                                                             TRUE,
                                                             &Created);
        }

        if (PageCacheEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto HandleCacheWriteMissEnd;
        }

        //
        // If something sneaked in and created the page cache entry before
        // this had a chance, treat it like it was a cache hit all along. The
        // cache hit routine handles synchronized I/O appropriately.
        //

        if (Created == FALSE) {
            Status = IopHandleCacheWriteHit(PageCacheEntry,
                                            WriteContext,
                                            Flags);

            if (!KSUCCESS(Status)) {
                goto HandleCacheWriteMissEnd;
            }

        //
        // Otherwise mark the new page cache entry dirty.
        //

        } else {
            IoMarkPageCacheEntryDirty(PageCacheEntry,
                                      WriteContext->PageByteOffset,
                                      WriteContext->BytesThisRound,
                                      FALSE);

            //
            // If the page cache entry was created from a physical page owned
            // by the scratch buffer, connect them.
            //

            if (ScratchIoBuffer != NULL) {
                MmSetIoBufferPageCacheEntry(ScratchIoBuffer, 0, PageCacheEntry);
            }

            //
            // If this is a synchronized I/O call then back the cache buffer
            // with this new page cache entry. It will be used for the pending
            // flush.
            //

            if ((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {

                ASSERT(WriteContext->CacheBuffer != NULL);

                MmIoBufferAppendPage(WriteContext->CacheBuffer,
                                     PageCacheEntry,
                                     NULL,
                                     INVALID_PHYSICAL_ADDRESS);
            }

            WriteContext->BytesCompleted += WriteContext->BytesThisRound;
        }
    }

HandleCacheWriteMissEnd:
    if (PageCacheBufferInitialized != FALSE) {
        MmFreeIoBuffer(&PageCacheBuffer);

    } else if (PageCacheEntry != NULL) {
        IoPageCacheEntryReleaseReference(PageCacheEntry);
    }

    if (ScratchIoBuffer != NULL) {
        MmFreeIoBuffer(ScratchIoBuffer);
    }

    return Status;
}

KSTATUS
IopHandleCacheWriteHit (
    PPAGE_CACHE_ENTRY PageCacheEntry,
    PIO_WRITE_CONTEXT WriteContext,
    ULONG Flags
    )

/*++

Routine Description:

    This routine handles cache hits when executing a write to the cache. It
    copies the data from the I/O context's source buffer to the page cache
    entry. If it is a synchornized I/O operation, then it also backs the cached
    buffer in the I/O context with the page cache entry.

Arguments:

    PageCacheEntry - Supplies a pointer to the page cache entry that was found
        at the current file offset, which is stored in the I/O context.

    WriteContext - Supplies a pointer to the I/O context that stores the
        current processing information for the write operation. This routine
        updates this information if it processes any bytes.

    Flags - Supplies a bitmask of flags for this I/O operation. See IO_FLAG_*
        for definitions.

Return Value:

    Status code.

--*/

{

    BOOL Linked;
    ULONG PageSize;
    PPAGE_CACHE_ENTRY SourceEntry;
    KSTATUS Status;

    //
    // If this is a full page aligned write and the source is backed by the
    // page cache, then try to share the source's physical page with the found
    // page cache entry.
    //

    Linked = FALSE;
    PageSize = MmPageSize();
    if ((WriteContext->PageByteOffset == 0) &&
        (WriteContext->BytesThisRound == PageSize) &&
        (IS_ALIGNED(WriteContext->SourceOffset, PageSize) != 0)) {

        SourceEntry = MmGetIoBufferPageCacheEntry(WriteContext->SourceBuffer,
                                                  WriteContext->SourceOffset);

        if (SourceEntry != NULL) {
            Linked = IopLinkPageCacheEntries(PageCacheEntry, SourceEntry);
        }
    }

    //
    // If the entries were not linked, copy the contents directly into the
    // cache and mark it dirty.
    //

    if (Linked == FALSE) {
        Status = IopCopyIoBufferToPageCacheEntry(PageCacheEntry,
                                                 WriteContext->PageByteOffset,
                                                 WriteContext->SourceBuffer,
                                                 WriteContext->SourceOffset,
                                                 WriteContext->BytesThisRound);

        if (!KSUCCESS(Status)) {
            goto HandleCacheWriteHitEnd;
        }

    //
    // Otherwise just mark the newly linked entry as dirty.
    //

    } else {
        IoMarkPageCacheEntryDirty(PageCacheEntry,
                                  WriteContext->PageByteOffset,
                                  WriteContext->BytesThisRound,
                                  FALSE);
    }

    WriteContext->BytesCompleted += WriteContext->BytesThisRound;

    //
    // If this is a synchronized operation, then initialize the cache buffer
    // with the cached page to prepare it for the flush.
    //

    if ((Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {

        ASSERT(WriteContext->CacheBuffer != NULL);

        MmIoBufferAppendPage(WriteContext->CacheBuffer,
                             PageCacheEntry,
                             NULL,
                             INVALID_PHYSICAL_ADDRESS);
    }

    Status = STATUS_SUCCESS;

HandleCacheWriteHitEnd:
    return Status;
}

KSTATUS
IopHandleCacheReadMiss (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine handles a cache miss. It performs an aligned read on the given
    handle at the miss offset and then caches the read data. It will update the
    given destination I/O buffer with physical pages from the page cache.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context for the cache miss.

Return Value:

    Status code.

--*/

{

    ULONGLONG BlockAlignedOffset;
    UINTN BlockAlignedSize;
    ULONG BlockByteOffset;
    ULONG BlockSize;
    UINTN BytesCopied;
    UINTN CopySize;
    ULONGLONG FileSize;
    ULONG PageSize;
    PIO_BUFFER ReadIoBuffer;
    IO_CONTEXT ReadIoContext;
    KSTATUS Status;

    ASSERT((FileObject->Properties.Type == IoObjectBlockDevice) ||
           (FileObject->Properties.Type == IoObjectRegularFile) ||
           (FileObject->Properties.Type == IoObjectSymbolicLink) ||
           (FileObject->Properties.Type == IoObjectSharedMemoryObject));

    PageSize = MmPageSize();
    IoContext->BytesCompleted = 0;

    ASSERT(IS_ALIGNED(IoContext->Offset, PageSize) != FALSE);

    //
    // Now read in the missed data. Make sure the offset and size are
    // block aligned. The offset is currently only page-aligned and the size
    // could be any amount.
    //

    BlockSize = FileObject->Properties.BlockSize;
    BlockAlignedOffset = ALIGN_RANGE_DOWN(IoContext->Offset, BlockSize);
    BlockAlignedSize = REMAINDER(IoContext->Offset, BlockSize) +
                       IoContext->SizeInBytes;

    BlockAlignedSize = ALIGN_RANGE_UP(BlockAlignedSize, BlockSize);
    BlockAlignedSize = ALIGN_RANGE_UP(BlockAlignedSize, PageSize);

    //
    // The block size should be either a power of 2 less than a page size,
    // making this already aligned, or a multiple of a page size. Therefore,
    // the block aligned offset better be page aligned.
    //

    ASSERT(IS_ALIGNED(BlockAlignedOffset, PageSize) != FALSE);

    //
    // If this is a miss for a device, read ahead some amount in anticipation
    // of accessing the next pages of the device in the near future. Don't read
    // ahead if system memory is low.
    //

    if (FileObject->Properties.Type == IoObjectBlockDevice) {
        READ_INT64_SYNC(&(FileObject->Properties.FileSize), &FileSize);
        if (MmGetPhysicalMemoryWarningLevel() == MemoryWarningLevelNone) {
            BlockAlignedSize = ALIGN_RANGE_UP(BlockAlignedSize, _128KB);
        }

        if (((BlockAlignedOffset + BlockAlignedSize) < BlockAlignedOffset) ||
            ((BlockAlignedOffset + BlockAlignedSize) > FileSize)) {

            BlockAlignedSize = FileSize - BlockAlignedOffset;
            BlockAlignedSize = ALIGN_RANGE_UP(BlockAlignedSize, PageSize);
        }

        ASSERT(IS_ALIGNED(_128KB, PageSize) != FALSE);
    }

    //
    // Allocate an I/O buffer that is not backed by any pages. The read will
    // either hit a caching layer and fill in the I/O buffer with page cache
    // entries or hit storage, which should validate the I/O buffer before use.
    // Validation will back the I/O buffer with memory.
    //

    ReadIoBuffer = MmAllocateUninitializedIoBuffer(BlockAlignedSize,
                                                   TRUE,
                                                   TRUE);

    if (ReadIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto HandleDefaultCacheReadMissEnd;
    }

    //
    // The file object's lock should already be held, if it exists.
    //

    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    //
    // This read needs to happen without re-acquiring the I/O lock. So directly
    // call the non-cached read routine.
    //

    ReadIoContext.IoBuffer = ReadIoBuffer;
    ReadIoContext.Offset = BlockAlignedOffset;
    ReadIoContext.SizeInBytes = BlockAlignedSize;
    ReadIoContext.BytesCompleted = 0;
    ReadIoContext.Flags = IoContext->Flags;
    ReadIoContext.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    ReadIoContext.Write = FALSE;
    Status = IopPerformNonCachedRead(FileObject, &ReadIoContext, NULL);
    if (!KSUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
        goto HandleDefaultCacheReadMissEnd;
    }

    //
    // The I/O buffer allocated above is large enough to accomodate the full
    // range of missed data, but the IRP might not have read into the entire
    // buffer. It could have reached the end of the file. So, zero any
    // remaining data in the buffer.
    //

    if (BlockAlignedSize != ReadIoContext.BytesCompleted) {
        Status = MmZeroIoBuffer(
                              ReadIoBuffer,
                              ReadIoContext.BytesCompleted,
                              BlockAlignedSize - ReadIoContext.BytesCompleted);

        if (!KSUCCESS(Status)) {
            goto HandleDefaultCacheReadMissEnd;
        }
    }

    //
    // Cache the entire read I/O buffer and copy the desired portions into the
    // I/O context's buffer.
    //

    BlockByteOffset = REMAINDER(IoContext->Offset, BlockSize);
    CopySize = ALIGN_RANGE_UP(IoContext->SizeInBytes, PageSize);
    Status = IopCopyAndCacheIoBuffer(FileObject,
                                     BlockAlignedOffset,
                                     IoContext->IoBuffer,
                                     CopySize,
                                     ReadIoBuffer,
                                     BlockAlignedSize,
                                     BlockByteOffset,
                                     IoContext->Write,
                                     &BytesCopied);

    if (!KSUCCESS(Status)) {
        goto HandleDefaultCacheReadMissEnd;
    }

    ASSERT(BytesCopied != 0);

    //
    // Report back the number of bytes copied but never more than the size
    // requested.
    //

    if (BytesCopied < IoContext->SizeInBytes) {
        IoContext->BytesCompleted = BytesCopied;

    } else {
        IoContext->BytesCompleted = IoContext->SizeInBytes;
    }

HandleDefaultCacheReadMissEnd:
    if (ReadIoBuffer != NULL) {
        MmFreeIoBuffer(ReadIoBuffer);
    }

    return Status;
}

KSTATUS
IopPerformCachedIoBufferWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs a write operation on an I/O buffer that is backed by
    page cache entries. This merely consists of marking the page cache entries
    dirty.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    UINTN BufferOffset;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    PPAGE_CACHE_ENTRY PageCacheEntry;
    ULONG PageSize;
    KSTATUS Status;

    BufferOffset = 0;
    BytesRemaining = IoContext->SizeInBytes;
    PageSize = MmPageSize();
    while (BytesRemaining != 0) {
        PageCacheEntry = MmGetIoBufferPageCacheEntry(IoContext->IoBuffer,
                                                     BufferOffset);

        BytesThisRound = PageSize;
        if (BytesThisRound > BytesRemaining) {
            BytesThisRound = BytesRemaining;
        }

        //
        // This routine should only be called with a valid page cached backed
        // I/O buffer.
        //

        ASSERT(PageCacheEntry != NULL);
        ASSERT(IopGetPageCacheEntryOffset(PageCacheEntry) ==
               (IoContext->Offset + BufferOffset));

        //
        // If this is a synchronized I/O call, then mark the pages clean, they
        // are about to be flushed. Otherwise mark them dirty.
        //

        if ((IoContext->Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {
            IopMarkPageCacheEntryClean(PageCacheEntry, TRUE);

        } else {
            IoMarkPageCacheEntryDirty(PageCacheEntry, 0, BytesThisRound, TRUE);
        }

        BufferOffset += BytesThisRound;
        BytesRemaining -= BytesThisRound;
    }

    //
    // If this is a synchronized I/O call, just flush the buffer immediately.
    //

    if ((IoContext->Flags & IO_FLAG_DATA_SYNCHRONIZED) != 0) {
        Status = IopPerformNonCachedWrite(FileObject, IoContext, NULL, TRUE);

        //
        // If this did not write out all the bytes then the pages were
        // incorrectly marked clean. Flip them back to being dirty now.
        //

        if (IoContext->BytesCompleted != IoContext->SizeInBytes) {
            BufferOffset = ALIGN_RANGE_DOWN(IoContext->BytesCompleted,
                                            PageSize);

            BytesRemaining = IoContext->SizeInBytes - BufferOffset;
            while (BytesRemaining != 0) {
                BytesThisRound = PageSize;
                if (BytesThisRound > BytesRemaining) {
                    BytesThisRound = BytesRemaining;
                }

                PageCacheEntry = MmGetIoBufferPageCacheEntry(
                                                           IoContext->IoBuffer,
                                                           BufferOffset);

                IoMarkPageCacheEntryDirty(PageCacheEntry,
                                          0,
                                          BytesThisRound,
                                          TRUE);

                BufferOffset += BytesThisRound;
                BytesRemaining -= BytesThisRound;
            }
        }

    //
    // Otherwise notify the page cache that something is dirty.
    //

    } else {
        IopNotifyPageCacheWrite();
        IoContext->BytesCompleted = IoContext->SizeInBytes;
        Status = STATUS_SUCCESS;
    }

    return Status;
}

KSTATUS
IopPerformDefaultNonCachedRead (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext
    )

/*++

Routine Description:

    This routine reads from the given file or device handle. It is assumed that
    the file lock is held in shared mode.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

Return Value:

    Status code. A failing status code does not necessarily mean no I/O made it
    in. Check the bytes completed value to find out how much occurred.

--*/

{

    PIO_BUFFER BlockAlignedIoBuffer;
    ULONGLONG BlockAlignedOffset;
    UINTN BlockAlignedSize;
    ULONG BlockSize;
    UINTN BytesToCopy;
    ULONG DestinationBlockOffset;
    PDEVICE Device;
    IRP_READ_WRITE Parameters;
    UINTN SizeInBytes;
    KSTATUS Status;

    IoContext->BytesCompleted = 0;
    SizeInBytes = IoContext->SizeInBytes;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(FileObject != NULL);
    ASSERT((FileObject->Properties.Type == IoObjectBlockDevice) ||
           (FileObject->Properties.Type == IoObjectRegularFile) ||
           (FileObject->Properties.Type == IoObjectSymbolicLink));

    //
    // This routine assumes the file object's lock is held in shared or
    // exclusive mode.
    //

    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    BlockSize = FileObject->Properties.BlockSize;

    //
    // Block-align the offset and size.
    //

    BlockAlignedOffset = ALIGN_RANGE_DOWN(IoContext->Offset, BlockSize);
    DestinationBlockOffset = REMAINDER(IoContext->Offset, BlockSize);
    BlockAlignedSize = SizeInBytes + DestinationBlockOffset;
    BlockAlignedSize = ALIGN_RANGE_UP(BlockAlignedSize, BlockSize);

    //
    // If the I/O request is block aligned in offset and size, then use the
    // provided I/O buffer. Otherwise allocate an uninitialized I/O buffer to
    // use for the read. Either a lower caching layer will fill it with page
    // cache pages or the backing storage will validate the I/O buffer before
    // use, causing it to initialize.
    //

    if ((DestinationBlockOffset != 0) ||
        (IS_ALIGNED(SizeInBytes, BlockSize) == FALSE)) {

        BlockAlignedIoBuffer = MmAllocateUninitializedIoBuffer(BlockAlignedSize,
                                                               TRUE,
                                                               TRUE);

        if (BlockAlignedIoBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PerformDefaultNonCachedReadEnd;
        }

    } else {
        BlockAlignedIoBuffer = IoContext->IoBuffer;
    }

    //
    // The aligned buffer is rounded up and down to full blocks. Read all
    // the data from the aligned offset.
    //

    if (DeviceContext != NULL) {
        Parameters.DeviceContext = DeviceContext;

    } else {
        Parameters.DeviceContext = FileObject->DeviceContext;
    }

    Parameters.Flags = IoContext->Flags;
    Parameters.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    Parameters.FileProperties = &(FileObject->Properties);
    Parameters.IoOffset = BlockAlignedOffset;
    Parameters.IoSizeInBytes = BlockAlignedSize;
    Parameters.IoBytesCompleted = 0;
    Parameters.NewIoOffset = Parameters.IoOffset;
    Parameters.IoBuffer = BlockAlignedIoBuffer;
    Device = FileObject->Device;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    //
    // Fire off the I/O!
    //

    Status = IopSendIoReadIrp(Device, &Parameters);
    if ((!KSUCCESS(Status)) && (Status != STATUS_END_OF_FILE)) {
        goto PerformDefaultNonCachedReadEnd;
    }

    //
    // If the original I/O buffer was not used for the read, copy the data from
    // the block aligned I/O buffer to the destination I/O buffer, up to the
    // completed number of bytes.
    //

    if (IoContext->IoBuffer != BlockAlignedIoBuffer) {
        BytesToCopy = Parameters.IoBytesCompleted;
        if (BytesToCopy < DestinationBlockOffset) {
            goto PerformDefaultNonCachedReadEnd;
        }

        BytesToCopy -= DestinationBlockOffset;
        if (IoContext->SizeInBytes < BytesToCopy) {
            BytesToCopy = IoContext->SizeInBytes;
        }

        Status = MmCopyIoBuffer(IoContext->IoBuffer,
                                0,
                                BlockAlignedIoBuffer,
                                DestinationBlockOffset,
                                BytesToCopy);

        if (!KSUCCESS(Status)) {
            goto PerformDefaultNonCachedReadEnd;
        }

        IoContext->BytesCompleted = BytesToCopy;

    } else {
        IoContext->BytesCompleted = Parameters.IoBytesCompleted;
    }

PerformDefaultNonCachedReadEnd:
    if ((BlockAlignedIoBuffer != NULL) &&
        (BlockAlignedIoBuffer != IoContext->IoBuffer)) {

        MmFreeIoBuffer(BlockAlignedIoBuffer);
    }

    return Status;
}

KSTATUS
IopPerformDefaultNonCachedWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext,
    BOOL UpdateFileSize
    )

/*++

Routine Description:

    This routine writes an I/O buffer to the given file or device. It is
    assumed that the file lock is held. This routine will always modify the
    file size in the the file properties and conditionally modify the file size
    in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

    DeviceContext - Supplies a pointer to the device context to use when
        writing to the backing device.

    UpdateFileSize - Supplies a boolean indicating whether or not this routine
        should update the file size in the file object. Only the page cache
        code should supply FALSE.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER AlignedIoBuffer;
    UINTN AlignedIoBufferSize;
    ULONGLONG AlignedOffset;
    ULONG BlockSize;
    UINTN BytesToWrite;
    PDEVICE Device;
    ULONGLONG FileSize;
    ULONGLONG Offset;
    IRP_READ_WRITE Parameters;
    IO_CONTEXT PartialContext;
    KSTATUS Status;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT((FileObject->Properties.Type == IoObjectBlockDevice) ||
           (FileObject->Properties.Type == IoObjectRegularFile) ||
           (FileObject->Properties.Type == IoObjectSymbolicLink));

    ASSERT(MmGetIoBufferSize(IoContext->IoBuffer) >= IoContext->SizeInBytes);
    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    BlockSize = FileObject->Properties.BlockSize;
    IoContext->BytesCompleted = 0;
    Offset = IoContext->Offset;
    Status = STATUS_SUCCESS;

    //
    // A partial write is needed for the first block if the given offset
    // is not block-aligned.
    //

    if (IS_ALIGNED(Offset, BlockSize) == FALSE) {
        BytesToWrite = BlockSize - REMAINDER(Offset, BlockSize);
        if (BytesToWrite > IoContext->SizeInBytes) {
            BytesToWrite = IoContext->SizeInBytes;
        }

        PartialContext.IoBuffer = IoContext->IoBuffer;
        PartialContext.Offset = Offset;
        PartialContext.SizeInBytes = BytesToWrite;
        PartialContext.Flags = IoContext->Flags;
        PartialContext.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
        PartialContext.Write = TRUE;
        Status = IopPerformDefaultPartialWrite(FileObject,
                                               &PartialContext,
                                               DeviceContext,
                                               IoContext->BytesCompleted,
                                               UpdateFileSize);

        IoContext->BytesCompleted += PartialContext.BytesCompleted;
        if (!KSUCCESS(Status)) {
            goto PerformDefaultNonCachedWriteEnd;
        }

        Offset += PartialContext.BytesCompleted;
    }

    //
    // With the first partial block handled, write as many full blocks as
    // possible. Make sure there wasn't any underflow in the subtraction of the
    // bytes written.
    //

    BytesToWrite = IoContext->SizeInBytes - IoContext->BytesCompleted;
    if ((IoContext->SizeInBytes > IoContext->BytesCompleted) &&
        (BytesToWrite >= BlockSize)) {

        ASSERT(IS_ALIGNED(Offset, BlockSize) != FALSE);

        //
        // Use the supplied buffer directly without validation. It is up to the
        // driver performing the I/O to validate the buffer.
        //

        AlignedOffset = Offset;
        AlignedIoBufferSize = ALIGN_RANGE_DOWN(BytesToWrite, BlockSize);
        AlignedIoBuffer = IoContext->IoBuffer;
        MmIoBufferIncrementOffset(AlignedIoBuffer, IoContext->BytesCompleted);

        //
        // Write the data out.
        //

        if (DeviceContext != NULL) {
            Parameters.DeviceContext = DeviceContext;

        } else {
            Parameters.DeviceContext = FileObject->DeviceContext;
        }

        Parameters.Flags = IoContext->Flags;
        Parameters.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
        Parameters.FileProperties = &(FileObject->Properties);
        Parameters.IoOffset = AlignedOffset;
        Parameters.IoSizeInBytes = AlignedIoBufferSize;
        Parameters.IoBytesCompleted = 0;
        Parameters.NewIoOffset = Parameters.IoOffset;
        Parameters.IoBuffer = AlignedIoBuffer;
        Device = FileObject->Device;

        ASSERT(IS_DEVICE_OR_VOLUME(Device));

        Status = IopSendIoIrp(Device, IrpMinorIoWrite, &Parameters);

        //
        // Roll the I/O buffer's offset back to where it was before this I/O.
        //

        MmIoBufferDecrementOffset(AlignedIoBuffer, IoContext->BytesCompleted);

        //
        // Update the file size if bytes were written.
        //

        if (Parameters.IoBytesCompleted != 0) {
            FileSize = AlignedOffset + Parameters.IoBytesCompleted;

            ASSERT(Parameters.IoBytesCompleted <= AlignedIoBufferSize);
            ASSERT(FileSize == Parameters.NewIoOffset);

            IopUpdateFileObjectFileSize(FileObject,
                                        FileSize,
                                        UpdateFileSize,
                                        TRUE);
        }

        IoContext->BytesCompleted += Parameters.IoBytesCompleted;
        if (!KSUCCESS(Status)) {
             goto PerformDefaultNonCachedWriteEnd;
        }

        Offset = Parameters.NewIoOffset;
        BytesToWrite = IoContext->SizeInBytes - IoContext->BytesCompleted;
    }

    //
    // Always check for a final partial block. Even if a big aligned chunk was
    // written or not. This also gets invoked for initial file writes (i.e.
    // small writes at the begining of a file). Make sure there wasn't any
    // underflow in the subtraction of the bytes written.
    //

    if ((IoContext->SizeInBytes > IoContext->BytesCompleted) &&
        (BytesToWrite != 0)) {

        PartialContext.IoBuffer = IoContext->IoBuffer;
        PartialContext.Offset = Offset;
        PartialContext.SizeInBytes = BytesToWrite;
        PartialContext.Flags = IoContext->Flags;
        PartialContext.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
        PartialContext.Write = TRUE;
        Status = IopPerformDefaultPartialWrite(FileObject,
                                               &PartialContext,
                                               DeviceContext,
                                               IoContext->BytesCompleted,
                                               UpdateFileSize);

        IoContext->BytesCompleted += PartialContext.BytesCompleted;
        Offset += PartialContext.BytesCompleted;
        if (!KSUCCESS(Status)) {
            goto PerformDefaultNonCachedWriteEnd;
        }
    }

    ASSERT(Offset > IoContext->Offset);

    READ_INT64_SYNC(&(FileObject->Properties.FileSize), &FileSize);

    ASSERT(FileSize > IoContext->Offset);

PerformDefaultNonCachedWriteEnd:
    return Status;
}

KSTATUS
IopPerformDefaultPartialWrite (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext,
    PVOID DeviceContext,
    UINTN IoBufferOffset,
    BOOL UpdateFileSize
    )

/*++

Routine Description:

    This routine completes a partial block write for a file or device. This
    routine will update the file size as necessary.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context for the partial write.

    DeviceContext - Supplies the I/O handle device context.

    IoBufferOffset - Supplies the offset into the context's I/O buffer to get
        data from.

    UpdateFileSize - Supplies a boolean indicating whether or not this routine
        should update the file size in the file object. Only the page cache
        code should supply FALSE.

Return Value:

    Status code.

--*/

{

    PIO_BUFFER AlignedIoBuffer;
    ULONGLONG AlignedOffset;
    ULONG BlockSize;
    ULONG ByteOffset;
    PDEVICE Device;
    ULONGLONG FileSize;
    IRP_READ_WRITE Parameters;
    KSTATUS Status;

    ASSERT((FileObject->Properties.Type == IoObjectBlockDevice) ||
           (FileObject->Properties.Type == IoObjectRegularFile) ||
           (FileObject->Properties.Type == IoObjectSymbolicLink));

    //
    // The lock really should be held exclusively, except that the page cache
    // worker may do partial writes with the lock held shared if the disk
    // block size is larger than a page. Since the page cache worker is single
    // threaded and everyone else acquires it exclusive, this is okay.
    //

    ASSERT((FileObject->Lock == NULL) ||
           (KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE));

    IoContext->BytesCompleted = 0;
    Device = FileObject->Device;

    ASSERT(IS_DEVICE_OR_VOLUME(Device));

    BlockSize = FileObject->Properties.BlockSize;
    AlignedIoBuffer = MmAllocateUninitializedIoBuffer(BlockSize, TRUE, TRUE);
    if (AlignedIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PerformDefaultPartialWriteEnd;
    }

    AlignedOffset = ALIGN_RANGE_DOWN(IoContext->Offset, BlockSize);

    //
    // Read in the block. If the read fails for any reason other than EOF, exit.
    //

    if (DeviceContext != NULL) {
        Parameters.DeviceContext = DeviceContext;

    } else {
        Parameters.DeviceContext = FileObject->DeviceContext;
    }

    Parameters.Flags = IoContext->Flags;
    Parameters.TimeoutInMilliseconds = IoContext->TimeoutInMilliseconds;
    Parameters.FileProperties = &(FileObject->Properties);
    Parameters.IoOffset = AlignedOffset;
    Parameters.IoSizeInBytes = BlockSize;
    Parameters.IoBytesCompleted = 0;
    Parameters.NewIoOffset = Parameters.IoOffset;
    Parameters.IoBuffer = AlignedIoBuffer;
    Status = IopSendIoReadIrp(Device, &Parameters);
    if (!KSUCCESS(Status) && (Status != STATUS_END_OF_FILE)) {
        goto PerformDefaultPartialWriteEnd;
    }

    //
    // Write the partial bytes to the read buffer. If the bytes read did not
    // reach all the way to the partial write offset within this block, then
    // zero out the bytes in between the read and where the write will start.
    //

    ByteOffset = REMAINDER(IoContext->Offset, BlockSize);

    ASSERT((ByteOffset + IoContext->SizeInBytes) <= BlockSize);

    if (Parameters.IoBytesCompleted < ByteOffset) {
        Status = MmZeroIoBuffer(AlignedIoBuffer,
                                Parameters.IoBytesCompleted,
                                ByteOffset - Parameters.IoBytesCompleted);

        if (!KSUCCESS(Status)) {
            goto PerformDefaultPartialWriteEnd;
        }
    }

    Status = MmCopyIoBuffer(AlignedIoBuffer,
                            ByteOffset,
                            IoContext->IoBuffer,
                            IoBufferOffset,
                            IoContext->SizeInBytes);

    if (!KSUCCESS(Status)) {
        goto PerformDefaultPartialWriteEnd;
    }

    //
    // Now write it back, but only up to the requested size.
    //

    Parameters.IoOffset = AlignedOffset;
    Parameters.IoSizeInBytes = ByteOffset + IoContext->SizeInBytes;
    Parameters.IoBytesCompleted = 0;
    Parameters.NewIoOffset = Parameters.IoOffset;
    Parameters.IoBuffer = AlignedIoBuffer;
    Status = IopSendIoIrp(Device, IrpMinorIoWrite, &Parameters);

    //
    // Determine how many of the bytes meant to be written were delivered.
    //

    if (ByteOffset == 0) {
        if (Parameters.IoBytesCompleted < IoContext->SizeInBytes) {
            IoContext->BytesCompleted = Parameters.IoBytesCompleted;

        } else {
            IoContext->BytesCompleted = IoContext->SizeInBytes;
        }

    } else {
        if (Parameters.IoBytesCompleted >= ByteOffset) {
            if (Parameters.IoBytesCompleted >=
                (ByteOffset + IoContext->SizeInBytes)) {

                IoContext->BytesCompleted = IoContext->SizeInBytes;

            } else {
                IoContext->BytesCompleted = Parameters.IoBytesCompleted -
                                            ByteOffset;
            }

        } else {

            ASSERT(IoContext->BytesCompleted == 0);

        }
    }

    if (IoContext->BytesCompleted != 0) {
        FileSize = IoContext->Offset + IoContext->BytesCompleted;
        IopUpdateFileObjectFileSize(FileObject, FileSize, UpdateFileSize, TRUE);
    }

PerformDefaultPartialWriteEnd:
    if (AlignedIoBuffer != NULL) {
        MmFreeIoBuffer(AlignedIoBuffer);
    }

    return Status;
}

