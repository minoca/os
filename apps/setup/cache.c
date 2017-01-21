/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cache.c

Abstract:

    This module implements a block level cache for the setup application.

Author:

    Evan Green 9-Oct-2014

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_CACHE_BLOCK_SIZE (64 * 1024)
#define SETUP_CACHE_BLOCK_SHIFT 16

#define SETUP_MAX_CACHE_SIZE (1024 * 1024 * 10)

//
// Define some max offset to ever expect to write to in order to debug stray
// writes.
//

#define SETUP_CACHE_MAX_OFFSET (16ULL * _1TB)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a handle to an I/O object in the setup app.

Members:

    Handle - Stores the device handle.

    Cached - Stores a boolean indicating whether caching is enabled or not.

    NextOffset - Stores the file position where the next I/O occurs.

    NextOsOffset - Stores the file position according to the OS handle.

    Cache - Stores the tree of cached data.

    CacheSize - Stores the number of entries in the cache.

    MaxCacheSize - Stores the maximum size the cache can grow, in entries.

    CheckBlock - Stores a single block's worth of buffer space, used to verify
        writes.

--*/

typedef struct _SETUP_HANDLE {
    PVOID Handle;
    BOOL Cached;
    LONGLONG NextOffset;
    LONGLONG NextOsOffset;
    RED_BLACK_TREE Cache;
    LIST_ENTRY CacheLruList;
    UINTN CacheSize;
    UINTN MaxCacheSize;
    PVOID CheckBlock;
} SETUP_HANDLE, *PSETUP_HANDLE;

/*++

Structure Description:

    This structure describes a cached block.

Members:

    Offset - Stores the offset of the data.

    TreeNode - Stores the red black tree node structure.

    ListEntry - Stores the LRU list structure.

    Dirty - Stores a boolean indicating if this entry is dirty.

    Data - Stores a pointer to the data.

--*/

typedef struct _SETUP_CACHE_DATA {
    ULONGLONG Offset;
    RED_BLACK_TREE_NODE TreeNode;
    LIST_ENTRY ListEntry;
    BOOL Dirty;
    PVOID Data;
} SETUP_CACHE_DATA, *PSETUP_CACHE_DATA;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
SetupDestroyCache (
    PSETUP_HANDLE Handle
    );

VOID
SetupAddCacheData (
    PSETUP_HANDLE Handle,
    ULONGLONG Offset,
    PVOID Buffer,
    BOOL Dirty
    );

INT
SetupCleanCacheData (
    PSETUP_HANDLE Handle,
    PSETUP_CACHE_DATA Data
    );

PSETUP_CACHE_DATA
SetupGetCacheData (
    PSETUP_HANDLE Handle,
    ULONGLONG Offset
    );

COMPARISON_RESULT
SetupCompareCacheTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this boolean to verify all writes.
//

BOOL SetupVerifyWrites = TRUE;

//
// ------------------------------------------------------------------ Functions
//

PVOID
SetupOpenDestination (
    PSETUP_DESTINATION Destination,
    INT Flags,
    INT CreatePermissions
    )

/*++

Routine Description:

    This routine opens a handle to a given destination.

Arguments:

    Destination - Supplies a pointer to the destination to open.

    Flags - Supplies open flags. See O_* definitions.

    CreatePermissions - Supplies optional create permissions.

Return Value:

    Returns a pointer to an opaque context on success.

    NULL on failure.

--*/

{

    UINTN AllocationSize;
    PSETUP_HANDLE IoHandle;

    AllocationSize = sizeof(SETUP_HANDLE) + SETUP_CACHE_BLOCK_SIZE;
    IoHandle = malloc(AllocationSize);
    if (IoHandle == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memset(IoHandle, 0, sizeof(SETUP_HANDLE));
    IoHandle->CheckBlock = (PVOID)(IoHandle + 1);
    if ((Destination->Type == SetupDestinationDisk) ||
        (Destination->Type == SetupDestinationPartition) ||
        (Destination->Type == SetupDestinationImage)) {

        IoHandle->Cached = TRUE;
        RtlRedBlackTreeInitialize(&(IoHandle->Cache),
                                  0,
                                  SetupCompareCacheTreeNodes);

        INITIALIZE_LIST_HEAD(&(IoHandle->CacheLruList));
        IoHandle->MaxCacheSize = SETUP_MAX_CACHE_SIZE / SETUP_CACHE_BLOCK_SIZE;
    }

    IoHandle->Handle = SetupOsOpenDestination(Destination,
                                              Flags,
                                              CreatePermissions);

    if (IoHandle->Handle == NULL) {
        free(IoHandle);
        IoHandle = NULL;
    }

    return IoHandle;
}

VOID
SetupClose (
    PVOID Handle
    )

/*++

Routine Description:

    This routine closes a handle.

Arguments:

    Handle - Supplies a pointer to the destination to open.

Return Value:

    None.

--*/

{

    PSETUP_HANDLE IoHandle;

    IoHandle = Handle;
    SetupDestroyCache(IoHandle);
    if (IoHandle->Handle != NULL) {
        SetupOsClose(IoHandle->Handle);
        IoHandle->Handle = NULL;
    }

    free(IoHandle);
    return;
}

ssize_t
SetupRead (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine reads from an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer where the read bytes will be returned.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes read.

    -1 on failure.

--*/

{

    ssize_t BytesRead;
    size_t BytesThisRound;
    PSETUP_CACHE_DATA CacheData;
    size_t CacheDataOffset;
    LONGLONG CacheOffset;
    PSETUP_HANDLE IoHandle;
    PVOID ReadBuffer;
    size_t TotalBytesRead;

    IoHandle = Handle;
    if (IoHandle->Cached == FALSE) {
        return SetupOsRead(IoHandle->Handle, Buffer, ByteCount);
    }

    TotalBytesRead = 0;
    while (ByteCount != 0) {
        CacheOffset = ALIGN_RANGE_DOWN(IoHandle->NextOffset,
                                       SETUP_CACHE_BLOCK_SIZE);

        CacheDataOffset = IoHandle->NextOffset - CacheOffset;
        BytesThisRound = SETUP_CACHE_BLOCK_SIZE - CacheDataOffset;
        if (BytesThisRound > ByteCount) {
            BytesThisRound = ByteCount;
        }

        CacheData = SetupGetCacheData(IoHandle, CacheOffset);
        if (CacheData != NULL) {
            memcpy(Buffer, CacheData->Data + CacheDataOffset, BytesThisRound);

        } else {
            if (IoHandle->NextOsOffset != CacheOffset) {
                IoHandle->NextOsOffset = SetupOsSeek(IoHandle->Handle,
                                                     CacheOffset);

                if (IoHandle->NextOsOffset != CacheOffset) {

                    assert(FALSE);

                    TotalBytesRead = -1;
                    break;
                }
            }

            //
            // Read directly into the destination buffer, or allocate a new
            // buffer if the destination is too small.
            //

            ReadBuffer = Buffer;
            if (BytesThisRound != SETUP_CACHE_BLOCK_SIZE) {
                ReadBuffer = malloc(SETUP_CACHE_BLOCK_SIZE);
                if (ReadBuffer == NULL) {
                    TotalBytesRead = -1;
                    break;
                }
            }

            BytesRead = SetupOsRead(IoHandle->Handle,
                                    ReadBuffer,
                                    SETUP_CACHE_BLOCK_SIZE);

            //
            // Allow a partial read in case the disk is actually a file that
            // hasn't grown all the way out.
            //

            if (BytesRead < 0) {
                perror("Read error");
                TotalBytesRead = -1;
                break;

            } else if (BytesRead < SETUP_CACHE_BLOCK_SIZE) {
                memset(ReadBuffer + BytesRead,
                       0,
                       SETUP_CACHE_BLOCK_SIZE - BytesRead);
            }

            IoHandle->NextOsOffset += BytesRead;
            SetupAddCacheData(IoHandle, CacheOffset, ReadBuffer, FALSE);
            if (ReadBuffer != Buffer) {
                memcpy(Buffer, ReadBuffer +CacheDataOffset, BytesThisRound);
                free(ReadBuffer);
            }
        }

        IoHandle->NextOffset += BytesThisRound;
        Buffer += BytesThisRound;
        ByteCount -= BytesThisRound;
        TotalBytesRead += BytesThisRound;
    }

    return TotalBytesRead;
}

ssize_t
SetupWrite (
    PVOID Handle,
    void *Buffer,
    size_t ByteCount
    )

/*++

Routine Description:

    This routine writes data to an open handle.

Arguments:

    Handle - Supplies the handle.

    Buffer - Supplies a pointer to the bytes to write.

    ByteCount - Supplies the number of bytes to read.

Return Value:

    Returns the number of bytes written.

    -1 on failure.

--*/

{

    ssize_t BytesRead;
    size_t BytesThisRound;
    PSETUP_CACHE_DATA CacheData;
    size_t CacheDataOffset;
    LONGLONG CacheOffset;
    PSETUP_HANDLE IoHandle;
    PVOID ReadBuffer;
    size_t TotalBytesWritten;

    IoHandle = Handle;
    if (IoHandle->Cached == FALSE) {
        return SetupOsWrite(IoHandle->Handle, Buffer, ByteCount);
    }

    ReadBuffer = NULL;
    TotalBytesWritten = 0;
    while (ByteCount != 0) {
        CacheOffset = ALIGN_RANGE_DOWN(IoHandle->NextOffset,
                                       SETUP_CACHE_BLOCK_SIZE);

        CacheDataOffset = IoHandle->NextOffset - CacheOffset;
        BytesThisRound = SETUP_CACHE_BLOCK_SIZE - CacheDataOffset;
        if (BytesThisRound > ByteCount) {
            BytesThisRound = ByteCount;
        }

        CacheData = SetupGetCacheData(IoHandle, CacheOffset);
        if (CacheData != NULL) {
            memcpy(CacheData->Data + CacheDataOffset, Buffer, BytesThisRound);
            CacheData->Dirty = TRUE;

        } else {

            //
            // The block was not in the cache. If it's a complete block, just
            // slam it in.
            //

            if ((BytesThisRound == SETUP_CACHE_BLOCK_SIZE) &&
                (CacheDataOffset == 0)) {

                SetupAddCacheData(IoHandle, CacheOffset, Buffer, TRUE);

            //
            // Go read the data first, then do the partial write.
            //

            } else {
                if (IoHandle->NextOsOffset != CacheOffset) {
                    IoHandle->NextOsOffset = SetupOsSeek(IoHandle->Handle,
                                                         CacheOffset);

                    if (IoHandle->NextOsOffset != CacheOffset) {

                        assert(FALSE);

                        TotalBytesWritten = -1;
                        break;
                    }
                }

                if (ReadBuffer == NULL) {
                    ReadBuffer = malloc(SETUP_CACHE_BLOCK_SIZE);
                    if (ReadBuffer == NULL) {
                        TotalBytesWritten = -1;
                        break;
                    }
                }

                BytesRead = SetupOsRead(IoHandle->Handle,
                                        ReadBuffer,
                                        SETUP_CACHE_BLOCK_SIZE);

                //
                // Allow a partial read in case the disk is actually a file that
                // hasn't grown all the way out.
                //

                if (BytesRead < 0) {
                    perror("Read error");
                    TotalBytesWritten = -1;
                    break;

                } else if (BytesRead < SETUP_CACHE_BLOCK_SIZE) {
                    memset(ReadBuffer + BytesRead,
                           0,
                           SETUP_CACHE_BLOCK_SIZE - BytesRead);
                }

                IoHandle->NextOsOffset += BytesRead;
                memcpy(ReadBuffer + CacheDataOffset, Buffer, BytesThisRound);
                SetupAddCacheData(IoHandle, CacheOffset, ReadBuffer, TRUE);
            }
        }

        IoHandle->NextOffset += BytesThisRound;
        Buffer += BytesThisRound;
        ByteCount -= BytesThisRound;
        TotalBytesWritten += BytesThisRound;
    }

    if (ReadBuffer != NULL) {
        free(ReadBuffer);
    }

    return TotalBytesWritten;
}

LONGLONG
SetupSeek (
    PVOID Handle,
    LONGLONG Offset
    )

/*++

Routine Description:

    This routine seeks in the current file or device.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the new offset to set.

Return Value:

    Returns the resulting file offset after the operation.

    -1 on failure, and errno will contain more information. The file offset
    will remain unchanged.

--*/

{

    PSETUP_HANDLE IoHandle;

    IoHandle = Handle;
    if (IoHandle->Cached == FALSE) {
        return SetupOsSeek(IoHandle->Handle, Offset);
    }

    IoHandle->NextOffset = Offset;
    return Offset;
}

INT
SetupFstat (
    PVOID Handle,
    PULONGLONG FileSize,
    time_t *ModificationDate,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine gets details for the given open file.

Arguments:

    Handle - Supplies the handle.

    FileSize - Supplies an optional pointer where the file size will be
        returned on success.

    ModificationDate - Supplies an optional pointer where the file's
        modification date will be returned on success.

    Mode - Supplies an optional pointer where the file's mode information will
        be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_HANDLE IoHandle;

    IoHandle = Handle;
    return SetupOsFstat(IoHandle->Handle, FileSize, ModificationDate, Mode);
}

INT
SetupFtruncate (
    PVOID Handle,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine sets the file size of the given file.

Arguments:

    Handle - Supplies the handle.

    NewSize - Supplies the new file size.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_HANDLE IoHandle;
    int Result;

    IoHandle = Handle;

    assert(IoHandle->Cached == FALSE);

    Result = SetupOsFtruncate(IoHandle->Handle, NewSize);
    return Result;
}

INT
SetupEnumerateDirectory (
    PVOID VolumeHandle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    )

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    VolumeHandle - Supplies the open volume handle.

    DirectoryPath - Supplies a pointer to a string containing the path to the
        directory to enumerate.

    Enumeration - Supplies a pointer where a pointer to a sequence of
        strings will be returned containing the files in the directory. The
        sequence will be terminated by an empty string. The caller is
        responsible for freeing this memory when done.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;

    Result = SetupOsEnumerateDirectory(VolumeHandle,
                                       DirectoryPath,
                                       Enumeration);

    return Result;
}

VOID
SetupDetermineExecuteBit (
    PVOID Handle,
    PCSTR Path,
    mode_t *Mode
    )

/*++

Routine Description:

    This routine determines whether the open file is executable.

Arguments:

    Handle - Supplies the open file handle.

    Path - Supplies the path the file was opened from (sometimes the file name
        is used as a hint).

    Mode - Supplies a pointer to the current mode bits. This routine may add
        the executable bit to user/group/other if it determines this file is
        executable.

Return Value:

    None.

--*/

{

    PSETUP_HANDLE IoHandle;

    IoHandle = Handle;

    assert(IoHandle->Cached == FALSE);

    SetupOsDetermineExecuteBit(IoHandle->Handle, Path, Mode);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
SetupDestroyCache (
    PSETUP_HANDLE Handle
    )

/*++

Routine Description:

    This routine destroys the data cache on a handle.

Arguments:

    Handle - Supplies the handle.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSETUP_CACHE_DATA Data;

    if (Handle->Cached == FALSE) {
        return;
    }

    assert(Handle->CacheLruList.Next != NULL);

    //
    // Rudely go through the list. Don't bother removing them from the list
    // or the tree, but do clean them.
    //

    CurrentEntry = Handle->CacheLruList.Previous;
    while (CurrentEntry != &(Handle->CacheLruList)) {
        Data = LIST_VALUE(CurrentEntry, SETUP_CACHE_DATA, ListEntry);
        if (Data->Dirty != FALSE) {
            SetupCleanCacheData(Handle, Data);
        }

        CurrentEntry = CurrentEntry->Previous;
        Handle->CacheSize -= 1;
        free(Data);
    }

    assert(Handle->CacheSize == 0);

    INITIALIZE_LIST_HEAD(&(Handle->CacheLruList));
    memset(&(Handle->Cache), 0, sizeof(RED_BLACK_TREE));
    return;
}

VOID
SetupAddCacheData (
    PSETUP_HANDLE Handle,
    ULONGLONG Offset,
    PVOID Buffer,
    BOOL Dirty
    )

/*++

Routine Description:

    This routine adds an entry to the cache. It's assumed the entry doesn't
    already exist.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the offset of the data.

    Buffer - Supplies a pointer to the buffer of the data. This data will be
        copied.

    Dirty - Supplies a boolean indicating whether or not the data is dirty
        already.

Return Value:

    None. On allocation failure, the data won't be cached.

--*/

{

    PSETUP_CACHE_DATA Data;

    assert(ALIGN_RANGE_DOWN(Offset, SETUP_CACHE_BLOCK_SIZE) == Offset);

    //
    // If the cache is at it's max, evict and reclaim the least recently used
    // entry.
    //

    if (Handle->CacheSize >= Handle->MaxCacheSize) {
        Data = LIST_VALUE(Handle->CacheLruList.Previous,
                          SETUP_CACHE_DATA,
                          ListEntry);

        LIST_REMOVE(&(Data->ListEntry));
        RtlRedBlackTreeRemove(&(Handle->Cache), &(Data->TreeNode));
        Handle->CacheSize -= 1;
        if (Data->Dirty != FALSE) {
            SetupCleanCacheData(Handle, Data);
        }

    } else {
        Data = malloc(sizeof(SETUP_CACHE_DATA) + SETUP_CACHE_BLOCK_SIZE);
        if (Data == NULL) {

            assert(FALSE);

            return;
        }
    }

    assert(Offset < SETUP_CACHE_MAX_OFFSET);

    Data->Offset = Offset;
    Data->Dirty = Dirty;
    Data->Data = Data + 1;
    memcpy(Data->Data, Buffer, SETUP_CACHE_BLOCK_SIZE);
    RtlRedBlackTreeInsert(&(Handle->Cache), &(Data->TreeNode));
    INSERT_AFTER(&(Data->ListEntry), &(Handle->CacheLruList));
    Handle->CacheSize += 1;
    return;
}

INT
SetupCleanCacheData (
    PSETUP_HANDLE Handle,
    PSETUP_CACHE_DATA Data
    )

/*++

Routine Description:

    This routine cleans dirty cache data, writing it out to the underlying
    handle.

Arguments:

    Handle - Supplies the handle.

    Data - Supplies the dirty cache entry to write out.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PUCHAR Bytes;
    ssize_t BytesWritten;
    UINTN Errors;
    UINTN FirstBad;
    UINTN Index;
    UINTN LastBad;
    PUCHAR ReadBytes;

    assert(Data->Dirty != FALSE);

    errno = 0;
    if (Data->Offset != Handle->NextOsOffset) {
        Handle->NextOsOffset = SetupOsSeek(Handle->Handle, Data->Offset);
        if (Handle->NextOsOffset != Data->Offset) {
            fprintf(stderr, "Error: Failed to seek.\n");
            return -1;
        }
    }

    BytesWritten = SetupOsWrite(Handle->Handle,
                                Data->Data,
                                SETUP_CACHE_BLOCK_SIZE);

    if (BytesWritten != SETUP_CACHE_BLOCK_SIZE) {
        if (errno != 0) {
            fprintf(stderr,
                    "Error: Write failed at offset %llx: %d bytes "
                    "written: %s.\n",
                    Data->Offset,
                    (int)BytesWritten,
                    strerror(errno));

            return -1;
        }
    }

    if (SetupVerifyWrites != FALSE) {
        Bytes = Data->Data;
        ReadBytes = Handle->CheckBlock;
        SetupOsSeek(Handle->Handle, Data->Offset);
        SetupOsRead(Handle->Handle, ReadBytes, SETUP_CACHE_BLOCK_SIZE);
        if (memcmp(ReadBytes, Bytes, SETUP_CACHE_BLOCK_SIZE) != 0) {
            FirstBad = SETUP_CACHE_BLOCK_SIZE;
            LastBad = 0;
            Errors = 0;
            for (Index = 0; Index < SETUP_CACHE_BLOCK_SIZE; Index += 1) {
                if (Bytes[Index] != ReadBytes[Index]) {
                    Errors += 1;
                    if (Errors < 10) {
                        fprintf(stderr,
                                "    Offset %lx: Got %02x, expected %02x\n",
                                Index,
                                ReadBytes[Index],
                                Bytes[Index]);
                    }

                    if (Index < FirstBad) {
                        FirstBad = Index;
                    }

                    if (Index > LastBad) {
                        LastBad = Index;
                    }
                }
            }

            fprintf(stderr,
                    "%ld errors (offsets %lx - %lx) at offset %llx\n",
                    Errors,
                    FirstBad,
                    LastBad,
                    Data->Offset);
        }
    }

    Handle->NextOsOffset += SETUP_CACHE_BLOCK_SIZE;
    Data->Dirty = FALSE;
    return 0;
}

PSETUP_CACHE_DATA
SetupGetCacheData (
    PSETUP_HANDLE Handle,
    ULONGLONG Offset
    )

/*++

Routine Description:

    This routine queries the data cache for an entry. If found, it also puts
    the cache entry at the head of the list.

Arguments:

    Handle - Supplies the handle.

    Offset - Supplies the desired offset.

Return Value:

    Returns a pointer to the cache entry on success.

    NULL if no cache entry exists.

--*/

{

    PSETUP_CACHE_DATA Data;
    PRED_BLACK_TREE_NODE FoundNode;
    SETUP_CACHE_DATA Search;

    assert(Offset < SETUP_CACHE_MAX_OFFSET);

    Search.Offset = Offset;
    FoundNode = RtlRedBlackTreeSearch(&(Handle->Cache), &(Search.TreeNode));
    if (FoundNode != NULL) {
        Data = RED_BLACK_TREE_VALUE(FoundNode, SETUP_CACHE_DATA, TreeNode);
        LIST_REMOVE(&(Data->ListEntry));
        INSERT_AFTER(&(Data->ListEntry), &(Handle->CacheLruList));
        return Data;
    }

    return NULL;
}

COMPARISON_RESULT
SetupCompareCacheTreeNodes (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes.

Arguments:

    Tree - Supplies a pointer to the Red-Black tree that owns both nodes.

    FirstNode - Supplies a pointer to the left side of the comparison.

    SecondNode - Supplies a pointer to the second side of the comparison.

Return Value:

    Same if the two nodes have the same value.

    Ascending if the first node is less than the second node.

    Descending if the second node is less than the first node.

--*/

{

    PSETUP_CACHE_DATA FirstEntry;
    PSETUP_CACHE_DATA SecondEntry;

    FirstEntry = RED_BLACK_TREE_VALUE(FirstNode, SETUP_CACHE_DATA, TreeNode);
    SecondEntry = RED_BLACK_TREE_VALUE(SecondNode, SETUP_CACHE_DATA, TreeNode);

    assert(FirstEntry->Offset < SETUP_CACHE_MAX_OFFSET);
    assert(SecondEntry->Offset < SETUP_CACHE_MAX_OFFSET);

    if (FirstEntry->Offset < SecondEntry->Offset) {
        return ComparisonResultAscending;

    } else if (FirstEntry->Offset > SecondEntry->Offset) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

