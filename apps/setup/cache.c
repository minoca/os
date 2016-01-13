/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_CACHE_BLOCK_SIZE 512
#define SETUP_CACHE_BLOCK_SHIFT 9

#define SETUP_MAX_CACHE_SIZE (1024 * 1024 * 10)

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

--*/

typedef struct _SETUP_HANDLE {
    PVOID Handle;
    BOOL Cached;
    ULONGLONG NextOffset;
    ULONGLONG NextOsOffset;
    RED_BLACK_TREE Cache;
    LIST_ENTRY CacheLruList;
    UINTN CacheSize;
    UINTN MaxCacheSize;
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

    PSETUP_HANDLE IoHandle;

    IoHandle = malloc(sizeof(SETUP_HANDLE));
    if (IoHandle == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    memset(IoHandle, 0, sizeof(SETUP_HANDLE));
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
    PSETUP_CACHE_DATA CacheData;
    PSETUP_HANDLE IoHandle;
    size_t TotalBytesRead;

    IoHandle = Handle;
    if (IoHandle->Cached == FALSE) {
        return SetupOsRead(IoHandle->Handle, Buffer, ByteCount);
    }

    assert(((ByteCount >> SETUP_CACHE_BLOCK_SHIFT) <<
            SETUP_CACHE_BLOCK_SHIFT) == ByteCount);

    TotalBytesRead = 0;
    while (ByteCount != 0) {
        CacheData = SetupGetCacheData(IoHandle, IoHandle->NextOffset);
        if (CacheData != NULL) {
            memcpy(Buffer, CacheData->Data, SETUP_CACHE_BLOCK_SIZE);

        } else {
            if (IoHandle->NextOffset != IoHandle->NextOsOffset) {
                IoHandle->NextOsOffset = SetupOsSeek(IoHandle->Handle,
                                                     IoHandle->NextOffset);

                if (IoHandle->NextOsOffset != IoHandle->NextOffset) {

                    assert(FALSE);

                    break;
                }
            }

            BytesRead = SetupOsRead(IoHandle->Handle,
                                    Buffer,
                                    SETUP_CACHE_BLOCK_SIZE);

            if (BytesRead != SETUP_CACHE_BLOCK_SIZE) {

                assert(FALSE);

                break;
            }

            IoHandle->NextOsOffset += BytesRead;
            SetupAddCacheData(IoHandle, IoHandle->NextOffset, Buffer, FALSE);
        }

        IoHandle->NextOffset += SETUP_CACHE_BLOCK_SIZE;
        Buffer += SETUP_CACHE_BLOCK_SIZE;
        ByteCount -= SETUP_CACHE_BLOCK_SIZE;
        TotalBytesRead += SETUP_CACHE_BLOCK_SIZE;
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

    PSETUP_CACHE_DATA CacheData;
    PSETUP_HANDLE IoHandle;
    size_t TotalBytesWritten;

    IoHandle = Handle;
    if (IoHandle->Cached == FALSE) {
        return SetupOsWrite(IoHandle->Handle, Buffer, ByteCount);
    }

    assert(((ByteCount >> SETUP_CACHE_BLOCK_SHIFT) <<
            SETUP_CACHE_BLOCK_SHIFT) == ByteCount);

    TotalBytesWritten = 0;
    while (ByteCount != 0) {
        CacheData = SetupGetCacheData(IoHandle, IoHandle->NextOffset);
        if (CacheData != NULL) {
            memcpy(CacheData->Data, Buffer, SETUP_CACHE_BLOCK_SIZE);
            CacheData->Dirty = TRUE;

        } else {
            SetupAddCacheData(IoHandle, IoHandle->NextOffset, Buffer, TRUE);
        }

        IoHandle->NextOffset += SETUP_CACHE_BLOCK_SIZE;
        Buffer += SETUP_CACHE_BLOCK_SIZE;
        ByteCount -= SETUP_CACHE_BLOCK_SIZE;
        TotalBytesWritten += SETUP_CACHE_BLOCK_SIZE;
    }

    return TotalBytesWritten;
}

ULONGLONG
SetupSeek (
    PVOID Handle,
    ULONGLONG Offset
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
    PVOID Handle,
    PSTR DirectoryPath,
    PSTR *Enumeration
    )

/*++

Routine Description:

    This routine enumerates the contents of a given directory.

Arguments:

    Handle - Supplies the open volume handle.

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

    PSETUP_HANDLE IoHandle;
    INT Result;

    IoHandle = Handle;

    assert(IoHandle->Cached == FALSE);

    Result = SetupOsEnumerateDirectory(IoHandle->Handle,
                                       DirectoryPath,
                                       Enumeration);

    return Result;
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

    ssize_t BytesWritten;

    assert(Data->Dirty != FALSE);

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
        fprintf(stderr, "Error: Write failed.\n");
        return -1;
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
    if (FirstEntry->Offset < SecondEntry->Offset) {
        return ComparisonResultAscending;

    } else if (FirstEntry->Offset > SecondEntry->Offset) {
        return ComparisonResultDescending;
    }

    return ComparisonResultSame;
}

