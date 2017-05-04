/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    path.c

Abstract:

    This module implements support functionality for traversing paths.

Author:

    Evan Green 18-Jun-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default permissions for any object manager object.
//

#define OBJECT_DIRECTORY_PERMISSIONS \
    (FILE_PERMISSION_USER_READ | FILE_PERMISSION_USER_EXECUTE | \
     FILE_PERMISSION_GROUP_READ | FILE_PERMISSION_GROUP_EXECUTE | \
     FILE_PERMISSION_OTHER_READ | FILE_PERMISSION_OTHER_EXECUTE)

//
// Define the maximum size of the path entry cache, in percent of physical
// memory.
//

#define PATH_ENTRY_CACHE_MAX_MEMORY_PERCENT 30

//
// Define the prefix prepended to an unreachable path.
//

#define PATH_UNREACHABLE_PATH_PREFIX "(unreachable)/"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopPathWalkWorker (
    BOOL FromKernelMode,
    PPATH_POINT Start,
    PCSTR *Path,
    PULONG PathSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    ULONG RecursionLevel,
    PPATH_POINT Result
    );

KSTATUS
IopPathLookupThroughFileSystem (
    BOOL FromKernelMode,
    PPATH_POINT Directory,
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    );

KSTATUS
IopFollowSymbolicLink (
    BOOL FromKernelMode,
    ULONG OpenFlags,
    ULONG RecursionLevel,
    PPATH_POINT Directory,
    PPATH_POINT SymbolicLink,
    PPATH_POINT Result
    );

BOOL
IopArePathsEqual (
    PCSTR ExistingPath,
    PCSTR QueryPath,
    ULONG QuerySize
    );

BOOL
IopFindPathPoint (
    PPATH_POINT Parent,
    ULONG OpenFlags,
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    PPATH_POINT Result
    );

VOID
IopPathEntryReleaseReference (
    PPATH_ENTRY Entry,
    BOOL EnforceCacheSize,
    BOOL Destroy
    );

PPATH_ENTRY
IopDestroyPathEntry (
    PPATH_ENTRY Entry
    );

UINTN
IopGetPathEntryCacheTargetSize (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the root path point.
//

PATH_POINT IoPathPointRoot;

//
// Store the creation and modification dates used for object manager objects.
//

SYSTEM_TIME IoObjectManagerCreationTime;

//
// Store the LRU list of cached but unreferenced path entries.
//

PQUEUED_LOCK IoPathEntryListLock;
LIST_ENTRY IoPathEntryList;
UINTN IoPathEntryListSize;
UINTN IoPathEntryListMaxSize;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IoPathAppend (
    PCSTR Prefix,
    ULONG PrefixSize,
    PCSTR Component,
    ULONG ComponentSize,
    ULONG AllocationTag,
    PSTR *AppendedPath,
    PULONG AppendedPathSize
    )

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    Prefix - Supplies the initial path string. This can be null.

    PrefixSize - Supplies the size of the prefix string in bytes including the
        null terminator.

    Component - Supplies a pointer to the component string to add.

    ComponentSize - Supplies the size of the component string in bytes
        including a null terminator.

    AllocationTag - Supplies the tag to use for the combined allocation.

    AppendedPath - Supplies a pointer where the new path will be returned. The
        caller is responsible for freeing this memory.

    AppendedPathSize - Supplies a pointer where the size of the appended bath
        buffer in bytes including the null terminator will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL NeedSlash;
    PSTR NewPath;
    ULONG NewPathSize;
    KSTATUS Status;

    NeedSlash = FALSE;
    NewPath = NULL;
    NewPathSize = 0;

    //
    // Pull the trailing null off of the prefix string. If the prefix ends in
    // a slash then there's no need to append a slash.
    //

    if (Prefix != NULL) {

        ASSERT(PrefixSize != 0);

        if (Prefix[PrefixSize - 1] == '\0') {
            PrefixSize -= 1;
            if (PrefixSize == 0) {
                Prefix = NULL;
            }
        }

        NeedSlash = TRUE;
        if ((Prefix != NULL) && (Prefix[PrefixSize - 1] == '/')) {
            NeedSlash = FALSE;
        }
    }

    //
    // Get rid of any leading slashes in the component.
    //

    ASSERT(ComponentSize > 1);

    while ((ComponentSize != 0) && (*Component == '/')) {
        Component += 1;
        ComponentSize -= 1;
    }

    if ((ComponentSize == 0) || (*Component == '\0')) {
        Status = STATUS_INVALID_PARAMETER;
        goto PathAppendEnd;
    }

    if (Component[ComponentSize - 1] != '\0') {
        ComponentSize += 1;
    }

    //
    // Allocate and create the new string.
    //

    NewPathSize = PrefixSize + ComponentSize;
    if (NeedSlash != 0) {
        NewPathSize += 1;
    }

    NewPath = MmAllocatePagedPool(NewPathSize, AllocationTag);
    if (NewPath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PathAppendEnd;
    }

    if (Prefix != NULL) {
        RtlCopyMemory(NewPath, Prefix, PrefixSize);
    }

    if (NeedSlash != FALSE) {
        NewPath[PrefixSize] = '/';
        RtlCopyMemory(NewPath + PrefixSize + 1, Component, ComponentSize);

    } else {
        RtlCopyMemory(NewPath + PrefixSize, Component, ComponentSize);
    }

    NewPath[NewPathSize - 1] = '\0';
    Status = STATUS_SUCCESS;

PathAppendEnd:
    if (!KSUCCESS(Status)) {
        if (NewPath != NULL) {
            MmFreePagedPool(NewPath);
            NewPath = NULL;
        }

        NewPathSize = 0;
    }

    *AppendedPath = NewPath;
    *AppendedPathSize = NewPathSize;
    return Status;
}

PPATH_POINT
IoGetPathPoint (
    PIO_HANDLE IoHandle
    )

/*++

Routine Description:

    This routine returns the path point for the given handle.

Arguments:

    IoHandle - Supplies a pointer to the I/O handle to get the path point of.

Return Value:

    Returns a pointer to the path point corresponding to the given handle.

--*/

{

    PPAGING_IO_HANDLE PagingHandle;

    if (IoHandle->HandleType == IoHandleTypePaging) {
        PagingHandle = (PPAGING_IO_HANDLE)IoHandle;
        return &(PagingHandle->IoHandle->PathPoint);
    }

    return &(IoHandle->PathPoint);
}

VOID
IoPathEntryAddReference (
    PPATH_ENTRY Entry
    )

/*++

Routine Description:

    This routine increments the reference count of the given path entry.

Arguments:

    Entry - Supplies a pointer to the path entry.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Entry->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    //
    // If this brought the path entry back from the cache, then remove it from
    // the cache list.
    //

    if (OldReferenceCount == 0) {

        ASSERT(KeGetRunLevel() == RunLevelLow);

        KeAcquireQueuedLock(IoPathEntryListLock);

        ASSERT(Entry->CacheListEntry.Next != NULL);

        LIST_REMOVE(&(Entry->CacheListEntry));
        Entry->CacheListEntry.Next = NULL;
        IoPathEntryListSize -= 1;
        KeReleaseQueuedLock(IoPathEntryListLock);
    }

    return;
}

VOID
IoPathEntryReleaseReference (
    PPATH_ENTRY Entry
    )

/*++

Routine Description:

    This routine decrements the reference count of the given path entry. If the
    reference count drops to zero, the path entry will be destroyed.

Arguments:

    Entry - Supplies a pointer to the path entry.

Return Value:

    None.

--*/

{

    IopPathEntryReleaseReference(Entry, TRUE, FALSE);
    return;
}

KSTATUS
IopInitializePathSupport (
    VOID
    )

/*++

Routine Description:

    This routine is called at system initialization time to initialize support
    for path traversal. It connects the root of the object manager to the root
    of the path/mount system.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PFILE_OBJECT FileObject;
    ULONGLONG MaxMemory;
    PPATH_ENTRY PathEntry;
    FILE_PROPERTIES Properties;
    PVOID RootObject;
    KSTATUS Status;

    ASSERT(IoPathPointRoot.PathEntry == NULL);

    FileObject = NULL;
    RootObject = NULL;
    KeGetSystemTime(&IoObjectManagerCreationTime);
    IoPathEntryListLock = KeCreateQueuedLock();
    if (IoPathEntryListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePathSupportEnd;
    }

    INITIALIZE_LIST_HEAD(&IoPathEntryList);
    IoPathEntryListSize = 0;
    MaxMemory = MmGetTotalPhysicalPages() * MmPageSize();
    if (MaxMemory > (MAX_UINTN - (UINTN)KERNEL_VA_START + 1)) {
        MaxMemory = MAX_UINTN - (UINTN)KERNEL_VA_START + 1;
    }

    IoPathEntryListMaxSize = ((MaxMemory *
                               PATH_ENTRY_CACHE_MAX_MEMORY_PERCENT) / 100) /
                             sizeof(PATH_ENTRY);

    RootObject = ObGetRootObject();
    IopFillOutFilePropertiesForObject(&Properties, RootObject);
    Status = IopCreateOrLookupFileObject(&Properties,
                                         RootObject,
                                         FILE_OBJECT_FLAG_EXTERNAL_IO_STATE,
                                         0,
                                         &FileObject,
                                         &Created);

    if (!KSUCCESS(Status)) {
        goto InitializePathSupportEnd;
    }

    ASSERT(Created != FALSE);

    KeSignalEvent(FileObject->ReadyEvent, SignalOptionSignalAll);
    PathEntry = IopCreatePathEntry(NULL, 0, 0, NULL, FileObject);
    if (PathEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializePathSupportEnd;
    }

    IoPathPointRoot.PathEntry = PathEntry;

InitializePathSupportEnd:
    if (!KSUCCESS(Status)) {
        if (IoPathEntryListLock != NULL) {
            KeDestroyQueuedLock(IoPathEntryListLock);
            IoPathEntryListLock = NULL;
        }

        if (RootObject != NULL) {
            ObReleaseReference(RootObject);
        }

        if (FileObject != NULL) {
            IopFileObjectReleaseReference(FileObject);
        }
    }

    return Status;
}

KSTATUS
IopPathWalk (
    BOOL FromKernelMode,
    PPATH_POINT Directory,
    PCSTR *Path,
    PULONG PathSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine attempts to walk the given path.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        is coming directly from kernel mode (and should use the kernel's root).

    Directory - Supplies an optional pointer to a path point containing the
        directory to start from if the path is relative. Supply NULL to use the
        current working directory.

    Path - Supplies a pointer that on input contains a pointer to the string
        of the path to walk. This pointer will be advanced beyond the portion
        of the path that was successfully walked.

    PathSize - Supplies a pointer that on input contains the size of the
        input path string in bytes. This value will be updated to reflect the
        new size of the updated path string.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    ASSERT((PVOID)(*Path) >= KERNEL_VA_START);

    Status = IopPathWalkWorker(FromKernelMode,
                               Directory,
                               Path,
                               PathSize,
                               OpenFlags,
                               Create,
                               0,
                               Result);

    return Status;
}

VOID
IopFillOutFilePropertiesForObject (
    PFILE_PROPERTIES Properties,
    POBJECT_HEADER Object
    )

/*++

Routine Description:

    This routine fills out the file properties structure for an object manager
    object directory.

Arguments:

    Properties - Supplies a pointer to the file properties.

    Object - Supplies a pointer to the object.

Return Value:

    TRUE if the paths are equals.

    FALSE if the paths differ in some way.

--*/

{

    RtlZeroMemory(Properties, sizeof(FILE_PROPERTIES));
    Properties->DeviceId = OBJECT_MANAGER_DEVICE_ID;
    Properties->FileId = (FILE_ID)(UINTN)Object;
    ObAddReference(Object);
    Properties->Type = IoObjectObjectDirectory;
    Properties->HardLinkCount = 1;
    Properties->Permissions = OBJECT_DIRECTORY_PERMISSIONS;
    RtlCopyMemory(&(Properties->StatusChangeTime),
                  &IoObjectManagerCreationTime,
                  sizeof(SYSTEM_TIME));

    RtlCopyMemory(&(Properties->ModifiedTime),
                  &IoObjectManagerCreationTime,
                  sizeof(SYSTEM_TIME));

    RtlCopyMemory(&(Properties->AccessTime),
                  &IoObjectManagerCreationTime,
                  sizeof(SYSTEM_TIME));

    return;
}

PPATH_ENTRY
IopCreateAnonymousPathEntry (
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine creates a new path entry structure that is not connected to
    the global path tree.

Arguments:

    FileObject - Supplies a pointer to the file object backing this entry. This
        routine takes ownership of an assumed reference on the file object.

Return Value:

    Returns a pointer to the path entry on success.

    NULL on allocation failure.

--*/

{

    return IopCreatePathEntry(NULL, 0, 0, NULL, FileObject);
}

KSTATUS
IopPathSplit (
    PCSTR Path,
    ULONG PathSize,
    PSTR *DirectoryComponent,
    PULONG DirectoryComponentSize,
    PSTR *LastComponent,
    PULONG LastComponentSize
    )

/*++

Routine Description:

    This routine creates a new string containing the last component of the
    given path.

Arguments:

    Path - Supplies a pointer to the null terminated path string.

    PathSize - Supplies the size of the path string in bytes including the
        null terminator.

    DirectoryComponent - Supplies a pointer where a newly allocated string
        containing only the directory component will be returned on success.
        The caller is responsible for freeing this memory from paged pool.

    DirectoryComponentSize - Supplies a pointer where the size of the directory
        component buffer in bytes including the null terminator will be
        returned.

    LastComponent - Supplies a pointer where a newly allocated string
        containing only the last component will be returned on success. The
        caller is responsible for freeing this memory from paged pool.

    LastComponentSize - Supplies a pointer where the size of the last component
        buffer in bytes including the null terminator will be returned.

Return Value:

    Status code.

--*/

{

    ULONG EndIndex;
    ULONG Length;
    PSTR NewDirectoryComponent;
    ULONG NewDirectoryComponentSize;
    PSTR NewLastComponent;
    ULONG NewLastComponentSize;
    ULONG NextStartIndex;
    ULONG StartIndex;
    KSTATUS Status;

    NewDirectoryComponent = NULL;
    NewDirectoryComponentSize = 0;
    NewLastComponent = NULL;
    NewLastComponentSize = 0;
    if ((Path == NULL) || (PathSize == 0)) {
        Status = STATUS_INVALID_PARAMETER;
        goto PathSplitEnd;
    }

    //
    // Loop looking at path components.
    //

    EndIndex = 0;
    StartIndex = 0;
    NextStartIndex = 0;
    while (TRUE) {

        //
        // Get past any path separators stuck on the beginning.
        //

        while ((Path[NextStartIndex] == PATH_SEPARATOR) &&
               (StartIndex < PathSize)) {

            NextStartIndex += 1;
        }

        //
        // This next part is just a bunch of trailing slashes, so stop, as the
        // path ended without a next component.
        //

        if ((NextStartIndex == PathSize) || (Path[NextStartIndex] == '\0')) {
            break;
        }

        //
        // Officially advance to this as a valid component, and find its end.
        //

        StartIndex = NextStartIndex;
        EndIndex = StartIndex;
        while ((EndIndex < PathSize) && (Path[EndIndex] != PATH_SEPARATOR) &&
               (Path[EndIndex] != '\0')) {

            EndIndex += 1;
        }

        //
        // If the path ended abruptly, add one to account for a null terminator
        // that should have been there, and stop.
        //

        if (EndIndex == PathSize) {
            EndIndex += 1;
            break;
        }

        if (Path[EndIndex] == '\0') {
            break;
        }

        NextStartIndex = EndIndex;
    }

    ASSERT(EndIndex >= StartIndex);

    //
    // Allocate and initialize the new buffer containing only the last
    // component.
    //

    Length = EndIndex - StartIndex;
    NewLastComponentSize = Length + 1;
    NewLastComponent = MmAllocatePagedPool(NewLastComponentSize,
                                           PATH_ALLOCATION_TAG);

    if (NewLastComponent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PathSplitEnd;
    }

    if (Length != 0) {
        RtlCopyMemory(NewLastComponent, Path + StartIndex, Length);
    }

    NewLastComponent[Length] = '\0';

    //
    // Allocate and initialize the new buffer containing only the directory
    // component.
    //

    NewDirectoryComponentSize = StartIndex + 1;
    NewDirectoryComponent = MmAllocatePagedPool(NewDirectoryComponentSize,
                                                PATH_ALLOCATION_TAG);

    if (NewDirectoryComponent == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto PathSplitEnd;
    }

    if (StartIndex != 0) {
        RtlCopyMemory(NewDirectoryComponent, Path, StartIndex);
    }

    NewDirectoryComponent[StartIndex] = '\0';
    Status = STATUS_SUCCESS;

PathSplitEnd:
    if (!KSUCCESS(Status)) {
        if (NewLastComponent != NULL) {
            MmFreePagedPool(NewLastComponent);
            NewLastComponent = NULL;
        }

        NewLastComponentSize = 0;
        if (NewDirectoryComponent != NULL) {
            MmFreePagedPool(NewDirectoryComponent);
            NewDirectoryComponent = NULL;
        }

        NewDirectoryComponentSize = 0;
    }

    *DirectoryComponent = NewDirectoryComponent;
    *DirectoryComponentSize = NewDirectoryComponentSize;
    *LastComponent = NewLastComponent;
    *LastComponentSize = NewLastComponentSize;
    return Status;
}

PPATH_ENTRY
IopCreatePathEntry (
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    PPATH_ENTRY Parent,
    PFILE_OBJECT FileObject
    )

/*++

Routine Description:

    This routine creates a new path entry structure.

Arguments:

    Name - Supplies an optional pointer to the name to give this path entry. A
        copy of this name will be made.

    NameSize - Supplies the size of the name buffer in bytes including the
        null terminator.

    Hash - Supplies the hash of the name string.

    Parent - Supplies a pointer to the parent of this entry.

    FileObject - Supplies an optional pointer to the file object backing this
        entry. This routine takes ownership of an assumed reference on the file
        object.

Return Value:

    Returns a pointer to the path entry on success.

    NULL on allocation failure.

--*/

{

    ULONG AllocationSize;
    PPATH_ENTRY Entry;

    AllocationSize = sizeof(PATH_ENTRY);
    if (Name != NULL) {
        AllocationSize += NameSize;
    }

    Entry = MmAllocatePagedPool(AllocationSize, PATH_ALLOCATION_TAG);
    if (Entry == NULL) {
        return NULL;
    }

    RtlZeroMemory(Entry, sizeof(PATH_ENTRY));
    INITIALIZE_LIST_HEAD(&(Entry->ChildList));
    if (Name != NULL) {
        Entry->Name = (PSTR)(Entry + 1);
        RtlStringCopy(Entry->Name, Name, NameSize);
        Entry->NameSize = NameSize;
    }

    Entry->Hash = Hash;
    Entry->ReferenceCount = 1;
    if (Parent != NULL) {
        Entry->Parent = Parent;
        IoPathEntryAddReference(Parent);
    }

    if (FileObject != NULL) {
        Entry->FileObject = FileObject;

        //
        // The caller should have added an additional reference to the file
        // object before calling this routine.
        //

        ASSERT(FileObject->ReferenceCount >= 2);

        //
        // Increment the count of path entries that own the file object.
        //

        IopFileObjectAddPathEntryReference(Entry->FileObject);
    }

    return Entry;
}

ULONG
IopHashPathString (
    PCSTR String,
    ULONG StringSize
    )

/*++

Routine Description:

    This routine generates the hash associated with a path name. This hash is
    used to speed up comparisons.

Arguments:

    String - Supplies a pointer to the string to hash.

    StringSize - Supplies the size of the string, including the null terminator.

Return Value:

    Returns the hash of the given string.

--*/

{

    ASSERT(StringSize != 0);

    return RtlComputeCrc32(0, String, StringSize - 1);
}

BOOL
IopIsDescendantPath (
    PPATH_ENTRY Ancestor,
    PPATH_ENTRY DescendantEntry
    )

/*++

Routine Description:

    This routine determines whether or not the given descendant path entry is a
    descendent of the given path entry. This does not take mount points into
    account.

Arguments:

    Ancestor - Supplies a pointer to the possible ancestor path entry.

    DescendantEntry - Supplies a pointer to the possible descendant path entry.

Return Value:

    Returns TRUE if it is a descendant, or FALSE otherwise.

--*/

{

    if (Ancestor == NULL) {
        return TRUE;
    }

    ASSERT(DescendantEntry != NULL);

    //
    // A path entry is a descendant of itself.
    //

    while (DescendantEntry != NULL) {
        if (DescendantEntry == Ancestor) {
            return TRUE;
        }

        DescendantEntry = DescendantEntry->Parent;
    }

    return FALSE;
}

VOID
IopPathUnlink (
    PPATH_ENTRY Entry
    )

/*++

Routine Description:

    This routine unlinks the given path entry from the path hierarchy. In most
    cases the caller should hold both the path entry's file object lock (if it
    exists) and the parent path entry's file object lock exclusively.

Arguments:

    Entry - Supplies a pointer to the path entry that is to be unlinked from
        the path hierarchy.

Return Value:

    None.

--*/

{

    ASSERT(Entry->Parent != NULL);

    //
    // The path entry must be pulled out of the list (as opposed to converting
    // it to a negative entry) because I/O handles and mount points have
    // references/pointers to it.
    //

    if (Entry->SiblingListEntry.Next != NULL) {
        LIST_REMOVE(&(Entry->SiblingListEntry));
        Entry->SiblingListEntry.Next = NULL;
    }

    return;
}

KSTATUS
IoGetCurrentDirectory (
    BOOL FromKernelMode,
    BOOL Root,
    PSTR *Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine gets either the current working directory or the path of the
    current chroot environment.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not a kernel mode
        caller is requesting the directory information. This dictates how the
        given path buffer is treated.

    Root - Supplies a boolean indicating whether to get the path to the current
        working directory (FALSE) or to get the path of the current chroot
        environment (TRUE). If the caller does not have permission to escape a
        changed root, or the root has not been changed, then / is returned in
        the path argument.

    Path - Supplies a pointer to a buffer that will contain the desired path on
        output. If the call is from kernel mode and the pointer is NULL, then
        a buffer will be allocated.

    PathSize - Supplies a pointer to the size of the path buffer on input. On
        output it stores the required size of the path buffer. This includes
        the null terminator.

Return Value:

    Status code.

--*/

{

    PPATH_POINT CurrentDirectory;
    PATH_POINT CurrentDirectoryCopy;
    PPROCESS_PATHS Paths;
    PKPROCESS Process;
    PPATH_POINT RootDirectory;
    PATH_POINT RootDirectoryCopy;
    PSTR RootPath;
    UINTN RootPathSize;
    KSTATUS Status;

    Process = PsGetCurrentProcess();
    RootPath = NULL;
    RootPathSize = 0;

    ASSERT((FromKernelMode != FALSE) || (Process != PsGetKernelProcess()));

    //
    // Get the path entries for this process's current directory and root
    // directory.
    //

    CurrentDirectory = NULL;
    RootDirectory = NULL;
    Paths = &(Process->Paths);
    KeAcquireQueuedLock(Paths->Lock);
    if (Root != FALSE) {
        if (Paths->Root.PathEntry != NULL) {
            IO_COPY_PATH_POINT(&CurrentDirectoryCopy, &(Paths->Root));
            IO_PATH_POINT_ADD_REFERENCE(&CurrentDirectoryCopy);
            CurrentDirectory = &CurrentDirectoryCopy;
        }

        //
        // Leave the root NULL for now (i.e. the real root). It will get set to
        // the current directory if the caller does not have permission to
        // escape a changed root.
        //

    } else {

        ASSERT(Paths->CurrentDirectory.PathEntry != NULL);

        IO_COPY_PATH_POINT(&CurrentDirectoryCopy, &(Paths->CurrentDirectory));
        IO_PATH_POINT_ADD_REFERENCE(&CurrentDirectoryCopy);
        CurrentDirectory = &CurrentDirectoryCopy;
        if (Paths->Root.PathEntry != NULL) {
            IO_COPY_PATH_POINT(&RootDirectoryCopy, &(Paths->Root));
            IO_PATH_POINT_ADD_REFERENCE(&RootDirectoryCopy);
            RootDirectory = &RootDirectoryCopy;
        }
    }

    KeReleaseQueuedLock(Process->Paths.Lock);

    //
    // If the caller does not have permission to escape a changed root, then
    // pretend they're at the real root.
    //

    if ((Root != FALSE) &&
        (!KSUCCESS(PsCheckPermission(PERMISSION_ESCAPE_CHROOT)))) {

        RootDirectory = CurrentDirectory;
        if (RootDirectory != NULL) {
            IO_PATH_POINT_ADD_REFERENCE(RootDirectory);
        }
    }

    //
    // If the caller is from kernel mode and did not supply a buffer, pass an
    // allocated buffer back.
    //

    if (FromKernelMode != FALSE) {
        Status = IopGetPathFromRoot(CurrentDirectory,
                                    RootDirectory,
                                    &RootPath,
                                    &RootPathSize);

        if (!KSUCCESS(Status)) {
            goto GetCurrentDirectoryEnd;
        }

        if (*Path != NULL) {
            if (*PathSize < RootPathSize) {
                Status = STATUS_BUFFER_TOO_SMALL;
                goto GetCurrentDirectoryEnd;
            }

            RtlCopyMemory(*Path, RootPath, RootPathSize);

        } else {
            *Path = RootPath;
            RootPath = NULL;
        }

    //
    // The user mode path must always copy into the provided path buffer.
    //

    } else {
        Status = IopGetUserFilePath(CurrentDirectory,
                                    RootDirectory,
                                    *Path,
                                    PathSize);

        if (!KSUCCESS(Status)) {
            goto GetCurrentDirectoryEnd;
        }
    }

GetCurrentDirectoryEnd:
    if (CurrentDirectory != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(CurrentDirectory);
    }

    if (RootDirectory != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(RootDirectory);
    }

    if (FromKernelMode != FALSE) {
        if (RootPath != NULL) {
            MmFreePagedPool(RootPath);
        }

        *PathSize = RootPathSize;
    }

    return Status;
}

KSTATUS
IopGetUserFilePath (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR UserBuffer,
    PUINTN UserBufferSize
    )

/*++

Routine Description:

    This routine copies the full path of the given path entry (as seen from
    the given root) into the given user mode buffer.

Arguments:

    Entry - Supplies a pointer to the path point to get the full path of.

    Root - Supplies a pointer to the user's root.

    UserBuffer - Supplies a pointer to the user mode buffer where the full path
        should be returned.

    UserBufferSize - Supplies a pointer that on success contains the size of
        the user mode buffer. Returns the actual size of the file path, even if
        the supplied buffer was too small.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_PATH_NOT_FOUND if the path entry has no path.

    STATUS_ACCESS_VIOLATION if the buffer was invalid.

    STATUS_BUFFER_TOO_SMALL if the buffer was too small.

--*/

{

    PSTR Path;
    UINTN PathSize;
    KSTATUS Status;

    //
    // Create the path.
    //

    Path = NULL;
    PathSize = 0;
    Status = IopGetPathFromRoot(Entry,
                                Root,
                                &Path,
                                &PathSize);

    if (!KSUCCESS(Status)) {
        goto GetUserFilePathEnd;
    }

    //
    // If not enough space was supplied, then return the required size.
    //

    if (*UserBufferSize < PathSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto GetUserFilePathEnd;
    }

    //
    // Copy the path to the supplied buffer.
    //

    if (UserBuffer != NULL) {
        Status = MmCopyToUserMode(UserBuffer, Path, PathSize);
        if (!KSUCCESS(Status)) {
            goto GetUserFilePathEnd;
        }
    }

GetUserFilePathEnd:
    if (Path != NULL) {
        MmFreePagedPool(Path);
    }

    *UserBufferSize = PathSize;
    return Status;
}

KSTATUS
IopGetPathFromRoot (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR *Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine creates a string representing the path from the given root to
    the given entry. If the entry is not a descendent of the given root, then
    the full path is printed.

Arguments:

    Entry - Supplies a pointer to the path point where to stop the string.

    Root - Supplies a optional pointer to the path point to treat as root.

    Path - Supplies a pointer that receives the full path string.

    PathSize - Supplies a pointer that receives the size of the full path
        string, in bytes.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    KeAcquireSharedExclusiveLockShared(IoMountLock);
    Status = IopGetPathFromRootUnlocked(Entry, Root, Path, PathSize);
    KeReleaseSharedExclusiveLockShared(IoMountLock);
    return Status;
}

KSTATUS
IopGetPathFromRootUnlocked (
    PPATH_POINT Entry,
    PPATH_POINT Root,
    PSTR *Path,
    PUINTN PathSize
    )

/*++

Routine Description:

    This routine creates a string representing the path from the given root to
    the given entry. If the entry is not a descendent of the given root, then
    the full path is printed. This routine assumes that the mount lock is held
    in shared mode.

Arguments:

    Entry - Supplies a pointer to the path point where to stop the string.

    Root - Supplies a optional pointer to the path point to treat as root.

    Path - Supplies a pointer that receives the full path string.

    PathSize - Supplies a pointer that receives the size of the full path
        string, in bytes.

Return Value:

    Status code.

--*/

{

    PSTR Name;
    UINTN NameSize;
    UINTN Offset;
    PSTR PathBuffer;
    UINTN PathBufferSize;
    PATH_POINT PathPoint;
    UINTN PrefixSize;
    KSTATUS Status;
    PPATH_POINT TrueRoot;
    BOOL Unreachable;

    ASSERT(KeIsSharedExclusiveLockHeldShared(IoMountLock) != FALSE);

    ASSERT((Root == NULL) || (Root->PathEntry != NULL));

    TrueRoot = &IoPathPointRoot;
    if (Root == NULL) {
        Root = TrueRoot;
    }

    //
    // Do a quick check for NULL, root, and equal path points. If
    // this is the case then the path is just "/".
    //

    if ((Entry == Root) ||
        (Entry == NULL) ||
        (Entry->PathEntry == NULL) ||
        (Entry->MountPoint == NULL) ||
        (IO_ARE_PATH_POINTS_EQUAL(Entry, Root) != FALSE) ||
        (IO_ARE_PATH_POINTS_EQUAL(Entry, TrueRoot) != FALSE)) {

        PathBufferSize = sizeof(CHAR) * 2;
        PathBuffer = MmAllocatePagedPool(PathBufferSize, PATH_ALLOCATION_TAG);
        if (PathBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetPathFromRootEnd;
        }

        PathBuffer[0] = PATH_SEPARATOR;
        PathBuffer[1] = STRING_TERMINATOR;
        *Path = PathBuffer;
        *PathSize = PathBufferSize;
        Status = STATUS_SUCCESS;
        goto GetPathFromRootEnd;
    }

    //
    // Fail for an anonymous path entry unless it is a mount point.
    //

    if ((Entry->PathEntry->NameSize == 0) &&
        (IO_IS_MOUNT_POINT(Entry) == FALSE)) {

        Status = STATUS_PATH_NOT_FOUND;
        goto GetPathFromRootEnd;
    }

    //
    // Determine the size of the path.
    //

    Unreachable = FALSE;
    PathBufferSize = 0;
    IO_COPY_PATH_POINT(&PathPoint, Entry);
    while ((IO_ARE_PATH_POINTS_EQUAL(&PathPoint, Root) == FALSE) &&
           (IO_ARE_PATH_POINTS_EQUAL(&PathPoint, TrueRoot) == FALSE)) {

        if (IO_IS_MOUNT_POINT(&PathPoint) != FALSE) {
            if (PathPoint.MountPoint->Parent == NULL) {
                Unreachable = TRUE;
                break;
            }

            PathBufferSize += PathPoint.MountPoint->MountEntry->NameSize;
            PathPoint.PathEntry = PathPoint.MountPoint->MountEntry->Parent;
            PathPoint.MountPoint = PathPoint.MountPoint->Parent;

        } else {
            PathBufferSize += PathPoint.PathEntry->NameSize;
            PathPoint.PathEntry = PathPoint.PathEntry->Parent;
        }
    }

    //
    // If the path was found to be unreachable, add the appropriate prefix. If
    // the path point is equal to the original entry, then add space for the
    // null terminator.
    //

    if (Unreachable != FALSE) {
        PathBufferSize += RtlStringLength(PATH_UNREACHABLE_PATH_PREFIX);
        if (IO_ARE_PATH_POINTS_EQUAL(&PathPoint, Entry) != FALSE) {
            PathBufferSize += sizeof(CHAR);
        }

    //
    // Otherwise add space for the root slash.
    //

    } else {
        PathBufferSize += sizeof(CHAR);
    }

    //
    // Allocate a buffer for the path.
    //

    PathBuffer = MmAllocatePagedPool(PathBufferSize, PATH_ALLOCATION_TAG);
    if (PathBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetPathFromRootEnd;
    }

    //
    // Now roll through again and create the path, backwards. Because the mount
    // lock is held shared, this should get the exact same path result as above.
    //

    Offset = PathBufferSize;
    IO_COPY_PATH_POINT(&PathPoint, Entry);
    while ((IO_ARE_PATH_POINTS_EQUAL(&PathPoint, Root) == FALSE) &&
           (IO_ARE_PATH_POINTS_EQUAL(&PathPoint, TrueRoot) == FALSE)) {

        if (IO_IS_MOUNT_POINT(&PathPoint) != FALSE) {
            if (PathPoint.MountPoint->Parent == NULL) {

                ASSERT(Unreachable != FALSE);

                break;
            }

            NameSize = PathPoint.MountPoint->MountEntry->NameSize;
            Name = PathPoint.MountPoint->MountEntry->Name;
            PathPoint.PathEntry = PathPoint.MountPoint->MountEntry->Parent;
            PathPoint.MountPoint = PathPoint.MountPoint->Parent;

        } else {
            NameSize = PathPoint.PathEntry->NameSize;
            Name = PathPoint.PathEntry->Name;
            PathPoint.PathEntry = PathPoint.PathEntry->Parent;
        }

        //
        // Add the null terminator for the final entry.
        //

        if (Offset == PathBufferSize) {
            PathBuffer[Offset - 1] = STRING_TERMINATOR;

        //
        // Add path separators for the non-terminal entries.
        //

        } else {
            PathBuffer[Offset - 1] = PATH_SEPARATOR;
        }

        Offset -= NameSize;
        RtlCopyMemory(PathBuffer + Offset, Name, NameSize - 1);
    }

    //
    // If the path is unreachable, then prepend the string with the unreachable
    // string.
    //

    if (Unreachable != FALSE) {
        if (Offset == PathBufferSize) {
            Offset -= 1;
            PathBuffer[Offset] = STRING_TERMINATOR;
        }

        PrefixSize = RtlStringLength(PATH_UNREACHABLE_PATH_PREFIX);
        Offset -= PrefixSize;

        ASSERT(Offset == 0);

        RtlCopyMemory(PathBuffer + Offset,
                      PATH_UNREACHABLE_PATH_PREFIX,
                      PrefixSize);

    //
    // Otherwise add the last separator for the root.
    //

    } else {

        ASSERT(Offset == 1);

        Offset -= 1;
        PathBuffer[Offset] = PATH_SEPARATOR;
    }

    //
    // The string better be null terminated.
    //

    ASSERT(PathBuffer[PathBufferSize - 1] == STRING_TERMINATOR);

    *Path = PathBuffer;
    *PathSize = PathBufferSize;
    Status = STATUS_SUCCESS;

GetPathFromRootEnd:
    return Status;
}

KSTATUS
IopPathLookup (
    BOOL FromKernelMode,
    PPATH_POINT Root,
    PPATH_POINT Directory,
    BOOL DirectoryLockHeld,
    PCSTR Name,
    ULONG NameSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine attempts to look up a child with the given name in a directory.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether this request is
        originating from kernel mode (TRUE) or user mode (FALSE). Kernel mode
        requests are not subjected to permission checks.

    Root - Supplies a pointer to the caller's root path point.

    Directory - Supplies a pointer to the path point to search.

    DirectoryLockHeld - Supplies a boolean indicating whether or not the caller
        had already acquired the directory's lock (exclusively).

    Name - Supplies a pointer to the name string.

    NameSize - Supplies a pointer to the size of the string in bytes
        including an assumed null terminator.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished. This routine may return a
        path entry even on failing status codes, such as a negative path entry.

Return Value:

    Status code.

--*/

{

    PFILE_OBJECT DirectoryFileObject;
    BOOL FoundPathPoint;
    ULONG Hash;
    KSTATUS Status;

    Result->PathEntry = NULL;

    ASSERT(NameSize != 0);

    //
    // This had better be a directory of some kind.
    //

    DirectoryFileObject = Directory->PathEntry->FileObject;
    if ((DirectoryFileObject->Properties.Type != IoObjectRegularDirectory) &&
        (DirectoryFileObject->Properties.Type != IoObjectObjectDirectory)) {

        return STATUS_NOT_A_DIRECTORY;
    }

    //
    // Either it was specified that the directory lock was not held, or it
    // better be held.
    //

    ASSERT((DirectoryLockHeld == FALSE) ||
           (KeIsSharedExclusiveLockHeldExclusive(DirectoryFileObject->Lock)));

    //
    // First look for the . and .. values.
    //

    if (IopArePathsEqual(".", Name, NameSize) != FALSE) {
        if (Create != NULL) {
            if ((Create->Type == IoObjectRegularDirectory) ||
                (Create->Type == IoObjectSymbolicLink)) {

                return STATUS_FILE_EXISTS;
            }

            return STATUS_FILE_IS_DIRECTORY;
        }

        //
        // This add reference is safe without a lock because the caller should
        // already have an extra reference on the directory.
        //

        IO_COPY_PATH_POINT(Result, Directory);
        IO_PATH_POINT_ADD_REFERENCE(Result);
        return STATUS_SUCCESS;

    } else if (IopArePathsEqual("..", Name, NameSize) != FALSE) {
        if (Create != NULL) {
            if ((Create->Type == IoObjectRegularDirectory) ||
                (Create->Type == IoObjectSymbolicLink)) {

                return STATUS_FILE_EXISTS;
            }

            return STATUS_FILE_IS_DIRECTORY;
        }

        IopGetParentPathPoint(Root, Directory, Result);
        return STATUS_SUCCESS;
    }

    //
    // First cruise through the cached list looking for this entry. Successful
    // return adds a reference to the found entry.
    //

    if (DirectoryLockHeld == FALSE) {
        KeAcquireSharedExclusiveLockShared(DirectoryFileObject->Lock);
    }

    Hash = IopHashPathString(Name, NameSize);
    FoundPathPoint = IopFindPathPoint(Directory,
                                      OpenFlags,
                                      Name,
                                      NameSize,
                                      Hash,
                                      Result);

    if (DirectoryLockHeld == FALSE) {
        KeReleaseSharedExclusiveLockShared(DirectoryFileObject->Lock);
    }

    if (FoundPathPoint != FALSE) {

        //
        // If a negative cache entry was found, return "not found" unless the
        // caller is trying to create.
        //

        if (Result->PathEntry->Negative != FALSE) {
            if (Create == NULL) {
                return STATUS_PATH_NOT_FOUND;
            }

            ASSERT(DirectoryLockHeld == FALSE);

            IO_PATH_POINT_RELEASE_REFERENCE(Result);
            Result->PathEntry = NULL;

        //
        // A real path entry was found, return it.
        //

        } else {
            if ((Create != NULL) &&
                ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) != 0)) {

                return STATUS_FILE_EXISTS;
            }

            return STATUS_SUCCESS;
        }
    }

    //
    // Fine, do it the hard way.
    //

    if (DirectoryLockHeld == FALSE) {
        KeAcquireSharedExclusiveLockExclusive(DirectoryFileObject->Lock);
    }

    Status = IopPathLookupThroughFileSystem(FromKernelMode,
                                            Directory,
                                            Name,
                                            NameSize,
                                            Hash,
                                            OpenFlags,
                                            Create,
                                            Result);

    if (DirectoryLockHeld == FALSE) {
        KeReleaseSharedExclusiveLockExclusive(DirectoryFileObject->Lock);
    }

    return Status;
}

VOID
IopPathCleanCache (
    PPATH_ENTRY RootPath
    )

/*++

Routine Description:

    This routine attempts to destroy any cached path entries below the given
    root path. In the process of doing so, it unlinks the given root path
    (if necessary) and dismantles the tree of path entries below it.

Arguments:

    RootPath - Supplies a pointer to the root path entry.

    DoNotCache - Supplies a boolean indicating whether or not to mark open
        path entries as non-cacheable or not.

Return Value:

    None.

--*/

{

    PPATH_ENTRY ChildPath;
    PLIST_ENTRY CurrentEntry;
    PPATH_ENTRY CurrentPath;
    PFILE_OBJECT FileObject;
    LIST_ENTRY ProcessList;

    INITIALIZE_LIST_HEAD(&ProcessList);

    //
    // Unlink the current root so that it can be inserted on the local list.
    //

    if (RootPath->SiblingListEntry.Next != NULL) {

        ASSERT(RootPath->Parent != NULL);

        FileObject = RootPath->Parent->FileObject;
        KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
        IopPathUnlink(RootPath);
        KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
    }

    //
    // Do nothing if the root path has no children. There is no reason to add
    // and remove a reference on it.
    //

    if (LIST_EMPTY(&(RootPath->ChildList)) != FALSE) {
        return;
    }

    //
    // Reference the root path and add it to the list of path entries that are
    // to be processed.
    //

    ASSERT(RootPath->SiblingListEntry.Next == NULL);

    IoPathEntryAddReference(RootPath);
    INSERT_BEFORE(&(RootPath->SiblingListEntry), &ProcessList);

    //
    // Iterate over the list of path entries to process. This will "flatten"
    // the tree by adding more entries to the list as it goes. For any cached
    // path entries, it will add and release a reference after unlinking the
    // path entry, which will trigger destruction.
    //

    CurrentEntry = ProcessList.Next;
    while (CurrentEntry != &ProcessList) {
        CurrentPath = LIST_VALUE(CurrentEntry, PATH_ENTRY, SiblingListEntry);
        FileObject = NULL;
        if (CurrentPath->Negative == FALSE) {
            FileObject = CurrentPath->FileObject;
            KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
        }

        //
        // Process the children. An open child will get moved to the list being
        // processed. A cached child will either get added to the destroy list
        // or be left in the cache.
        //

        while (LIST_EMPTY(&(CurrentPath->ChildList)) == FALSE) {
            ChildPath = LIST_VALUE(CurrentPath->ChildList.Next,
                                   PATH_ENTRY,
                                   SiblingListEntry);

            IoPathEntryAddReference(ChildPath);
            IopPathUnlink(ChildPath);
            INSERT_BEFORE(&(ChildPath->SiblingListEntry), &ProcessList);
        }

        if (FileObject != NULL) {
            KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);
        }

        //
        // Release the reference taken for the active list and move to the next
        // entry (this must be done after the children are processed).
        // Releasing this reference may destroy the current path entry.
        //

        CurrentEntry = CurrentEntry->Next;
        CurrentPath->SiblingListEntry.Next = NULL;
        IoPathEntryReleaseReference(CurrentPath);
    }

    return;
}

VOID
IopPathEntryIncrementMountCount (
    PPATH_ENTRY PathEntry
    )

/*++

Routine Description:

    This routine increments the mount count for the given path entry.

Arguments:

    PathEntry - Supplies a pointer to a path entry.

Return Value:

    None.

--*/

{

    ULONG OldMountCount;

    OldMountCount = RtlAtomicAdd32(&(PathEntry->MountCount), 1);

    ASSERT(OldMountCount < 0x10000000);

    return;
}

VOID
IopPathEntryDecrementMountCount (
    PPATH_ENTRY PathEntry
    )

/*++

Routine Description:

    This routine decrements the mount count for the given path entry.

Arguments:

    PathEntry - Supplies a pointer to a path entry.

Return Value:

    None.

--*/

{

    ULONG OldMountCount;

    OldMountCount = RtlAtomicAdd32(&(PathEntry->MountCount), (ULONG)-1);

    ASSERT((OldMountCount != 0) && (OldMountCount < 0x10000000));

    return;
}

VOID
IopGetParentPathPoint (
    PPATH_POINT Root,
    PPATH_POINT PathPoint,
    PPATH_POINT ParentPathPoint
    )

/*++

Routine Description:

    This routine gets the parent path point of the given path point, correctly
    traversing mount points. This routine takes references on the parent path
    point's path entry and mount point.

Arguments:

    Root - Supplies an optional pointer to the caller's path point root. If
        supplied, then the parent will never be lower in the path tree than
        the root.

    PathPoint - Supplies a pointer to the path point whose parent is being
        queried.

    ParentPathPoint - Supplies a pointer to a path point that receives the
        parent path point's information.

Return Value:

    None.

--*/

{

    PMOUNT_POINT MountPoint;
    PPATH_ENTRY PathEntry;

    MountPoint = NULL;
    PathEntry = NULL;

    //
    // Prevent the caller from going above their root, if supplied.
    //

    if ((Root == NULL) ||
        (IO_ARE_PATH_POINTS_EQUAL(PathPoint, Root) == FALSE)) {

        //
        // If the path point is a mount point, then move out of the mount point
        // to the mount entry's parent. Be careful here as the parent might
        // disappear with a lazy unmount. If it does, just return the current
        // path point.
        //

        if (IO_IS_MOUNT_POINT(PathPoint) != FALSE) {
            MountPoint = IopGetMountPointParent(PathPoint->MountPoint);
            if (MountPoint == NULL) {
                PathEntry = NULL;

            } else {
                PathEntry = PathPoint->MountPoint->MountEntry->Parent;
            }

        //
        // Otherwise just move to the directory's parent, which belongs to the
        // same mount point. Be careful, as the root mount point does not have
        // a parent.
        //

        } else if (PathPoint->PathEntry->Parent != NULL) {
            PathEntry = PathPoint->PathEntry->Parent;
            MountPoint = PathPoint->MountPoint;
            IoMountPointAddReference(MountPoint);
        }
    }

    //
    // If nothing suitable was found, remain in the same directory.
    //

    if (PathEntry == NULL) {

        ASSERT(MountPoint == NULL);

        PathEntry = PathPoint->PathEntry;
        MountPoint = PathPoint->MountPoint;
        IoMountPointAddReference(MountPoint);
    }

    //
    // This add reference is safe because the caller has a reference on the
    // given path point, preventing the parent path from being released in
    // medias res.
    //

    IoPathEntryAddReference(PathEntry);
    ParentPathPoint->PathEntry = PathEntry;
    ParentPathPoint->MountPoint = MountPoint;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopPathWalkWorker (
    BOOL FromKernelMode,
    PPATH_POINT Start,
    PCSTR *Path,
    PULONG PathSize,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    ULONG RecursionLevel,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine attempts to walk the given path.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        is coming directly from kernel mode (and should use the kernel's root).

    Start - Supplies a pointer to the path point to start the walk from.

    Path - Supplies a pointer that on input contains a pointer to the string
        of the path to walk. This pointer will be advanced beyond the portion
        of the path that was successfully walked.

    PathSize - Supplies a pointer that on input contains the size of the
        input path string in bytes. This value will be updated to reflect the
        new size of the updated path string.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    RecursionLevel - Supplies the recursion level used internally to avoid
        symbolic link loops.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished.

Return Value:

    Status code.

--*/

{

    ULONG ComponentSize;
    PCSTR CurrentPath;
    ULONG CurrentPathSize;
    PATH_POINT Entry;
    PFILE_OBJECT FileObject;
    BOOL FollowLink;
    PATH_POINT LinkEntry;
    PATH_POINT NextEntry;
    PCSTR NextSeparator;
    PKPROCESS Process;
    ULONG RemainingSize;
    PPATH_POINT Root;
    PATH_POINT RootCopy;
    KSTATUS Status;
    PCREATE_PARAMETERS ThisCreate;
    ULONG ThisIterationOpenFlags;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    CurrentPath = *Path;
    CurrentPathSize = *PathSize;
    ThisCreate = NULL;

    //
    // Empty paths do not resolve to anything.
    //

    if ((CurrentPathSize <= 1) || (*CurrentPath == '\0')) {
        return STATUS_PATH_NOT_FOUND;
    }

    CurrentPathSize -= 1;

    //
    // For all components except the end, follow symbolic links.
    //

    FollowLink = TRUE;
    if (FromKernelMode != FALSE) {
        Process = PsGetKernelProcess();

    } else {
        Process = PsGetCurrentProcess();
    }

    //
    // Get the path entry to start with.
    //

    KeAcquireQueuedLock(Process->Paths.Lock);
    if ((OpenFlags & OPEN_FLAG_SHARED_MEMORY) != 0) {
        Root = (PPATH_POINT)&(Process->Paths.SharedMemoryDirectory);
        if (Root->PathEntry != NULL) {
            IO_COPY_PATH_POINT(&RootCopy, Root);
            Root = &RootCopy;

        } else {
            Root = &IoSharedMemoryRoot;
        }

    } else {
        Root = (PPATH_POINT)&(Process->Paths.Root);
        if (Root->PathEntry != NULL) {
            IO_COPY_PATH_POINT(&RootCopy, Root);
            Root = &RootCopy;

        } else {
            Root = &IoPathPointRoot;
        }

        if (*CurrentPath != PATH_SEPARATOR) {
            if (Start == NULL) {
                Start = (PPATH_POINT)&(Process->Paths.CurrentDirectory);
            }

        } else {
            Start = NULL;
        }
    }

    if ((Start == NULL) || (Start->PathEntry == NULL)) {
        Start = Root;
    }

    IO_COPY_PATH_POINT(&Entry, Start);

    //
    // This add reference is safe because the root will never be removed and
    // the current directory always has an additional reference preventing it
    // from being removed in the middle of this operation.
    //

    IO_PATH_POINT_ADD_REFERENCE(Root);
    IO_PATH_POINT_ADD_REFERENCE(&Entry);
    KeReleaseQueuedLock(Process->Paths.Lock);

    //
    // Loop walking path components.
    //

    while (CurrentPathSize != 0) {

        //
        // Get past any separators.
        //

        while ((CurrentPathSize != 0) && (*CurrentPath == PATH_SEPARATOR)) {
            CurrentPath += 1;
            CurrentPathSize -= 1;
        }

        if ((*CurrentPath == '\0') || (CurrentPathSize == 0)) {
            break;
        }

        //
        // Find the next separator. A trailing slash on the end of a final
        // path component is treated as if it's not the final component.
        //

        RemainingSize = CurrentPathSize;
        NextSeparator = CurrentPath;
        while ((*NextSeparator != PATH_SEPARATOR) && (*NextSeparator != '\0') &&
               (RemainingSize != 0)) {

            RemainingSize -= 1;
            NextSeparator += 1;
        }

        if ((*NextSeparator == '\0') || (RemainingSize == 0)) {
            NextSeparator = NULL;
        }

        ComponentSize = CurrentPathSize - RemainingSize;

        //
        // If it's a create operation and this is the last component, then
        // feed in the type override. Otherwise, this is just an open
        // operation of a directory along the way.
        //

        ThisIterationOpenFlags = OPEN_FLAG_DIRECTORY;
        if (NextSeparator == NULL) {
            ThisIterationOpenFlags = OpenFlags;
            ThisCreate = Create;

            //
            // If this is the end component and the caller wants the symbolic
            // link specifically, don't follow the link.
            //

            if ((OpenFlags & OPEN_FLAG_SYMBOLIC_LINK) != 0) {
                FollowLink = FALSE;
            }
        }

        //
        // Ensure the caller has permission to search in this directory. It is
        // the caller's responsibility to do the appropriate permission checks
        // on the final path entry.
        //

        if (FromKernelMode == FALSE) {
            Status = IopCheckPermissions(FromKernelMode,
                                         &Entry,
                                         IO_ACCESS_EXECUTE);

            if (!KSUCCESS(Status)) {
                goto PathWalkWorkerEnd;
            }
        }

        //
        // This routine takes a reference on a sucessfully returned entry.
        //

        Status = IopPathLookup(FromKernelMode,
                               Root,
                               &Entry,
                               FALSE,
                               CurrentPath,
                               ComponentSize + 1,
                               ThisIterationOpenFlags,
                               ThisCreate,
                               &NextEntry);

        if (!KSUCCESS(Status)) {
            if (NextEntry.PathEntry != NULL) {
                IO_PATH_POINT_RELEASE_REFERENCE(&NextEntry);
            }

            goto PathWalkWorkerEnd;
        }

        //
        // If this is a symbolic link and links should be followed this
        // iteration, then follow the link. This is recursive.
        //

        FileObject = NextEntry.PathEntry->FileObject;
        if (FollowLink != FALSE) {
            if (FileObject->Properties.Type == IoObjectSymbolicLink) {

                //
                // If this is the last component and the caller doesn't want
                // symbolic links, fail. Symbolic links in inner components
                // of the path are still followed. Also stop if too many
                // symbolic links were traversed.
                //

                if ((RecursionLevel > MAX_SYMBOLIC_LINK_RECURSION) ||
                    ((NextSeparator == NULL) &&
                     ((OpenFlags & OPEN_FLAG_NO_SYMBOLIC_LINK) != 0))) {

                    IO_PATH_POINT_RELEASE_REFERENCE(&NextEntry);
                    Status = STATUS_SYMBOLIC_LINK_LOOP;
                    goto PathWalkWorkerEnd;
                }

                Status = IopFollowSymbolicLink(FromKernelMode,
                                               ThisIterationOpenFlags,
                                               RecursionLevel,
                                               &Entry,
                                               &NextEntry,
                                               &LinkEntry);

                IO_PATH_POINT_RELEASE_REFERENCE(&NextEntry);
                IO_COPY_PATH_POINT(&NextEntry, &LinkEntry);
                if (!KSUCCESS(Status)) {

                    ASSERT(NextEntry.PathEntry == NULL);

                    goto PathWalkWorkerEnd;
                }

                //
                // Count a symbolic link traversal as "recursion" even though
                // in this case it's not recursive in a function call sense.
                // This protects against runaway paths with symbolic links that
                // loop back up the path tree (ie to ".", "..", etc).
                //

                RecursionLevel += 1;
                FileObject = NextEntry.PathEntry->FileObject;
            }
        }

        //
        // Move on to the next entry and release the reference on this entry.
        //

        IO_PATH_POINT_RELEASE_REFERENCE(&Entry);
        IO_COPY_PATH_POINT(&Entry, &NextEntry);

        //
        // Watch out for the end.
        //

        CurrentPath += ComponentSize;
        CurrentPathSize -= ComponentSize;
        if (NextSeparator == NULL) {
            break;
        }

        //
        // This new thing needs to be a directory as there are more components
        // to traverse (or a least a trailing slash, which should be treated
        // the same way).
        //

        if ((FileObject->Properties.Type != IoObjectRegularDirectory) &&
            (FileObject->Properties.Type != IoObjectObjectDirectory)) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto PathWalkWorkerEnd;
        }
    }

    Status = STATUS_SUCCESS;

PathWalkWorkerEnd:
    IO_PATH_POINT_RELEASE_REFERENCE(Root);
    if (!KSUCCESS(Status)) {
        if (Entry.PathEntry != NULL) {
            IO_PATH_POINT_RELEASE_REFERENCE(&Entry);
            Entry.PathEntry = NULL;
        }
    }

    if (Entry.PathEntry != NULL) {
        IO_COPY_PATH_POINT(Result, &Entry);
    }

    *Path = CurrentPath;
    *PathSize = CurrentPathSize + 1;
    return Status;
}

KSTATUS
IopPathLookupThroughFileSystem (
    BOOL FromKernelMode,
    PPATH_POINT Directory,
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    ULONG OpenFlags,
    PCREATE_PARAMETERS Create,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine attempts to look up a child with the given name in a
    directory by asking the file system. This routine assumes that the parent
    directory's I/O lock is already held.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Directory - Supplies a pointer to the path point to search.

    Name - Supplies a pointer to the name string.

    NameSize - Supplies a pointer to the size of the string in bytes
        including an assumed null terminator.

    Hash - Supplies the hash of the name string.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    Create - Supplies an optional pointer to the creation parameters.

    Result - Supplies a pointer to a path point that receives the resulting
         path entry and mount point on success. The path entry and mount point
         will come with an extra reference on them, so the caller must be sure
         to release the references when finished. A path point may be returned
         even on failing status codes (such as a negative path entry).

Return Value:

    Status code.

--*/

{

    POBJECT_HEADER Child;
    BOOL Created;
    PDEVICE DirectoryDevice;
    PPATH_ENTRY DirectoryEntry;
    PFILE_OBJECT DirectoryFileObject;
    BOOL DoNotCache;
    BOOL Equal;
    PFILE_OBJECT FileObject;
    ULONG FileObjectFlags;
    BOOL FoundPathPoint;
    ULONG MapFlags;
    BOOL Negative;
    POBJECT_HEADER Object;
    PPATH_ENTRY PathEntry;
    PDEVICE PathRoot;
    FILE_PROPERTIES Properties;
    PPATH_POINT ShmDirectory;
    KSTATUS Status;
    PKTHREAD Thread;

    Child = NULL;
    Created = FALSE;
    DirectoryEntry = Directory->PathEntry;
    DoNotCache = FALSE;
    FileObject = NULL;
    Negative = FALSE;
    PathEntry = NULL;
    PathRoot = DirectoryEntry->FileObject->Device;

    //
    // This had better be a directory of some kind.
    //

    DirectoryFileObject = DirectoryEntry->FileObject;

    ASSERT((DirectoryFileObject->Properties.Type == IoObjectObjectDirectory) ||
           (DirectoryFileObject->Properties.Type == IoObjectRegularDirectory));

    //
    // The directory's I/O lock should be held exclusively.
    //

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(DirectoryFileObject->Lock));

    //
    // If the hard link count on this directory has dropped since the caller
    // got a reference, then just exit.
    //

    if (DirectoryFileObject->Properties.HardLinkCount == 0) {
        Status = STATUS_PATH_NOT_FOUND;
        goto PathLookupThroughFileSystemEnd;
    }

    //
    // With the directory lock held exclusively, double check to make sure
    // something else didn't already create this path entry.
    //

    FoundPathPoint = IopFindPathPoint(Directory,
                                      OpenFlags,
                                      Name,
                                      NameSize,
                                      Hash,
                                      Result);

    if (FoundPathPoint != FALSE) {

        //
        // If negative path entry was found, fail unless it's a create
        // operation.
        //

        if (Result->PathEntry->Negative != FALSE) {
            if (Create == NULL) {
                Status = STATUS_PATH_NOT_FOUND;
                goto PathLookupThroughFileSystemEnd;
            }

        //
        // A real path entry was found. Return the path point.
        //

        } else {
            if ((Create != NULL) &&
                ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) != 0)) {

                Status = STATUS_FILE_EXISTS;

            } else {
                Status = STATUS_SUCCESS;
            }

            goto PathLookupThroughFileSystemEnd;
        }
    }

    //
    // Call out to the driver if the root is managed by it.
    //

    if (IS_DEVICE_OR_VOLUME(PathRoot)) {
        if (Create != NULL) {

            ASSERT(Create->Type != IoObjectInvalid);

            //
            // It's not obvious what user/group ID to put as the owner
            // if the creation comes from kernel mode. Assert that this create
            // request is from user mode.
            //

            ASSERT(FromKernelMode == FALSE);

            //
            // Check to make sure the caller has permission to create objects
            // in this directory.
            //

            Status = IopCheckPermissions(FromKernelMode,
                                         Directory,
                                         IO_ACCESS_WRITE);

            if (!KSUCCESS(Status)) {
                goto PathLookupThroughFileSystemEnd;
            }

            //
            // Send the create IRP. Set the file owner to the effective user ID
            // of the caller. If the sticky bit is set in the directory, set
            // the owning group to that of the directory.
            //

            Thread = KeGetCurrentThread();
            RtlZeroMemory(&Properties, sizeof(FILE_PROPERTIES));
            Properties.DeviceId = PathRoot->DeviceId;
            Properties.Type = Create->Type;
            Properties.UserId = Thread->Identity.EffectiveUserId;
            Properties.GroupId = Thread->Identity.EffectiveGroupId;
            if ((DirectoryFileObject->Properties.Permissions &
                 FILE_PERMISSION_SET_GROUP_ID) != 0) {

                Properties.GroupId = DirectoryFileObject->Properties.GroupId;
            }

            Properties.Permissions = Create->Permissions & FILE_PERMISSION_MASK;
            Properties.HardLinkCount = 1;
            KeGetSystemTime(&(Properties.AccessTime));
            Properties.ModifiedTime = Properties.AccessTime;
            Properties.StatusChangeTime = Properties.AccessTime;
            Status = IopSendCreateRequest(PathRoot,
                                          DirectoryFileObject,
                                          Name,
                                          NameSize,
                                          &Properties);

            //
            // If the create request worked, create a file object for it. If
            // this results in a create, then the reference on the path root is
            // passed to the file object. If this just results in a lookup,
            // then the path root needs to be released. This is handled below
            // when the create is evaluated.
            //

            if (KSUCCESS(Status)) {

                ASSERT(Properties.DeviceId == PathRoot->DeviceId);

                FileObjectFlags = 0;
                if ((OpenFlags & OPEN_FLAG_NO_PAGE_CACHE) != 0) {
                    FileObjectFlags |= FILE_OBJECT_FLAG_NO_PAGE_CACHE;
                }

                switch (Properties.Type) {
                case IoObjectRegularFile:
                case IoObjectRegularDirectory:
                case IoObjectObjectDirectory:
                case IoObjectSymbolicLink:
                case IoObjectSharedMemoryObject:
                case IoObjectSocket:
                    FileObjectFlags |= FILE_OBJECT_FLAG_EXTERNAL_IO_STATE;
                    break;

                default:
                    break;
                }

                Status = IopCreateOrLookupFileObject(&Properties,
                                                     PathRoot,
                                                     FileObjectFlags,
                                                     0,
                                                     &FileObject,
                                                     &Created);

                if (!KSUCCESS(Status)) {
                    goto PathLookupThroughFileSystemEnd;
                }

                ASSERT(Created != FALSE);

                Create->Created = Created;

            //
            // The creation request didn't work. It can only turn into an open
            // request if it's a regular file. The path root is no longer
            // needed, so release the reference.
            //

            } else {
                Create->Created = FALSE;
                if ((Status == STATUS_FILE_EXISTS) &&
                    (Create->Type == IoObjectRegularFile) &&
                    ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) == 0)) {

                    Create = NULL;

                } else {
                    goto PathLookupThroughFileSystemEnd;
                }
            }
        }

        //
        // No creation parameters, open an existing file.
        //

        if (Create == NULL) {
            DirectoryDevice = DirectoryFileObject->Device;

            ASSERT(IS_DEVICE_OR_VOLUME(DirectoryDevice));

            FileObjectFlags = 0;
            Status = IopSendLookupRequest(DirectoryDevice,
                                          DirectoryFileObject,
                                          Name,
                                          NameSize,
                                          &Properties,
                                          &FileObjectFlags,
                                          &MapFlags);

            if (!KSUCCESS(Status)) {
                if (Status == STATUS_PATH_NOT_FOUND) {
                    Negative = TRUE;

                } else {
                    goto PathLookupThroughFileSystemEnd;
                }

            //
            // Successful lookup, create or look up a file object.
            //

            } else {
                Properties.DeviceId = DirectoryFileObject->Properties.DeviceId;
                if ((OpenFlags & OPEN_FLAG_NO_PAGE_CACHE) != 0) {
                    FileObjectFlags |= FILE_OBJECT_FLAG_NO_PAGE_CACHE;
                }

                switch (Properties.Type) {
                case IoObjectRegularFile:
                case IoObjectRegularDirectory:
                case IoObjectObjectDirectory:
                case IoObjectSymbolicLink:
                case IoObjectSharedMemoryObject:
                case IoObjectSocket:
                    FileObjectFlags |= FILE_OBJECT_FLAG_EXTERNAL_IO_STATE;
                    break;

                default:
                    break;
                }

                //
                // Create a file object. If this results in a create, then the
                // reference on the path root is passed to the file object. If
                // this just results in a lookup, then the path root needs to be
                // released. This is handled below when the create is evaluated.
                //

                Status = IopCreateOrLookupFileObject(&Properties,
                                                     DirectoryDevice,
                                                     FileObjectFlags,
                                                     MapFlags,
                                                     &FileObject,
                                                     &Created);

                if (!KSUCCESS(Status)) {
                    goto PathLookupThroughFileSystemEnd;
                }
            }
        }

        //
        // An existing object was found. Check to make sure the caching flags
        // match.
        //

        if ((Created == FALSE) && (FileObject != NULL)) {
            if (((OpenFlags & OPEN_FLAG_NO_PAGE_CACHE) != 0) &&
                (IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE)) {

                Status = STATUS_RESOURCE_IN_USE;
                goto PathLookupThroughFileSystemEnd;
            }
        }

    //
    // The object manager handles this node.
    //

    } else {

        //
        // The file ID is actually a direct pointer to the object.
        //

        Object = (POBJECT_HEADER)(UINTN)DirectoryFileObject->Properties.FileId;

        //
        // Creates within the object manager are allowed only in very
        // restricted situations.
        //

        if (Create != NULL) {

            //
            // Pipes are allowed in the pipes directory.
            //

            switch (Create->Type) {
            case IoObjectPipe:
                if (IopGetPipeDirectory() != Object) {
                    break;
                }

                Status = IopCreatePipe(Name,
                                       NameSize,
                                       Create,
                                       &FileObject);

                if (!KSUCCESS(Status) &&
                    ((Status != STATUS_FILE_EXISTS) ||
                     ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) != 0))) {

                    goto PathLookupThroughFileSystemEnd;
                }

                break;

            //
            // Shared memory objects are allowed in the current process's
            // shared memory object directory.
            //

            case IoObjectSharedMemoryObject:
                ShmDirectory = IopGetSharedMemoryDirectory(FromKernelMode);
                Equal = IO_ARE_PATH_POINTS_EQUAL(Directory, ShmDirectory);
                if (Equal == FALSE) {
                    break;
                }

                Status = IopCreateSharedMemoryObject(FromKernelMode,
                                                     Name,
                                                     NameSize,
                                                     OpenFlags,
                                                     Create,
                                                     &FileObject);

                if (!KSUCCESS(Status) &&
                    ((Status != STATUS_FILE_EXISTS) ||
                     ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) != 0))) {

                    goto PathLookupThroughFileSystemEnd;
                }

                break;

            //
            // Directory creates are not permitted in the object manager system.
            //

            case IoObjectRegularDirectory:
                Status = STATUS_ACCESS_DENIED;
                goto PathLookupThroughFileSystemEnd;

            default:
                break;
            }
        }

        //
        // Attempt to open an existing object with the given name.
        //

        if (FileObject == NULL) {

            //
            // Try to find the child by name. This will take a reference on
            // the child. If the child is a volume or device and it wants to
            // own the path, then the reference will be passed on to the file
            // object. Otherwise, the reference will be transferred to the new
            // properties created below (see comments).
            //

            Child = ObFindObject(Name, NameSize, Object);
            if (Child == NULL) {

                //
                // Creates are generally not permitted in the object manager
                // system.
                //

                if (Create != NULL) {
                    Status = STATUS_ACCESS_DENIED;

                } else {
                    Status = STATUS_PATH_NOT_FOUND;
                }

                goto PathLookupThroughFileSystemEnd;
            }

            //
            // Fail the create call if the object exists.
            //

            if ((OpenFlags & OPEN_FLAG_FAIL_IF_EXISTS) != 0) {
                Status = STATUS_FILE_EXISTS;
                goto PathLookupThroughFileSystemEnd;
            }

            //
            // If the child is a device, send it a lookup to see if it wants to
            // own the path.
            //

            Status = STATUS_UNSUCCESSFUL;
            FileObjectFlags = 0;
            MapFlags = 0;
            if (Child->Type == ObjectDevice) {
                Status = IopSendLookupRequest((PDEVICE)Child,
                                              NULL,
                                              NULL,
                                              0,
                                              &Properties,
                                              &FileObjectFlags,
                                              &MapFlags);

                if (KSUCCESS(Status)) {
                    PathRoot = (PDEVICE)Child;
                    Properties.DeviceId = PathRoot->DeviceId;

                } else if (Status == STATUS_DEVICE_NOT_CONNECTED) {
                    goto PathLookupThroughFileSystemEnd;
                }
            }

            //
            // If the device didn't want it, create a file object for this
            // object. Give the reference added during the find object routine
            // to the file object structure.
            //

            if (!KSUCCESS(Status)) {
                DoNotCache = TRUE;
                IopFillOutFilePropertiesForObject(&Properties, Child);

                //
                // Update the properties to contain the appropriate type.
                //

                switch (Child->Type) {
                case ObjectPipe:
                    Properties.Type = IoObjectPipe;
                    break;

                case ObjectTerminalMaster:
                    Properties.Type = IoObjectTerminalMaster;
                    break;

                case ObjectTerminalSlave:
                    Properties.Type = IoObjectTerminalSlave;
                    break;

                case ObjectSharedMemoryObject:
                    Properties.Type = IoObjectSharedMemoryObject;
                    break;

                default:
                    break;
                }

                //
                // Take a reference on the path root to match the extra
                // reference the child would have, so that the path root can
                // just be dereferenced after creating the file object.
                //

                ASSERT(PathRoot != (PDEVICE)Child);

                ObAddReference(PathRoot);

                //
                // Release the reference taken on the child by the find object
                // routine, the file object took its own.
                //

                ObReleaseReference(Child);
                Child = NULL;
            }

            switch (Properties.Type) {
            case IoObjectRegularFile:
            case IoObjectRegularDirectory:
            case IoObjectObjectDirectory:
            case IoObjectSymbolicLink:
            case IoObjectSharedMemoryObject:
            case IoObjectSocket:
                FileObjectFlags |= FILE_OBJECT_FLAG_EXTERNAL_IO_STATE;
                break;

            default:
                break;
            }

            Status = IopCreateOrLookupFileObject(&Properties,
                                                 PathRoot,
                                                 FileObjectFlags,
                                                 MapFlags,
                                                 &FileObject,
                                                 &Created);

            ObReleaseReference(PathRoot);
            if (Create != NULL) {
                Create->Created = Created;
            }

            if (!KSUCCESS(Status)) {

                //
                // For volumes and devices that own the path, this does not
                // execute as the child is not NULL. For other objects, this
                // releases the reference taken on the child when the
                // properties were filled out.
                //

                if (Child == NULL) {
                    ObReleaseReference((PVOID)(UINTN)(Properties.FileId));
                }

                Child = NULL;
                goto PathLookupThroughFileSystemEnd;
            }

            //
            // If an existing file object was found, then the references in
            // the properties need to be released.
            //

            if (Created == FALSE) {

                //
                // The previous root lookup should have resulted in the same
                // set of file object flags.
                //

                ASSERT((FileObject->Flags & FileObjectFlags) ==
                       FileObjectFlags);

                //
                // For volumes and devices that own the path, this does not
                // execute as the child is not NULL. For other objects, this
                // releases the reference taken on the child when the
                // properties were filled out.
                //

                if (Child == NULL) {
                    ObReleaseReference((PVOID)(UINTN)(Properties.FileId));
                }
            }

            Child = NULL;
        }
    }

    //
    // If it's a special type of object potentially create the special sauce
    // for it. Note that with hard links several threads may be doing this at
    // once, but the file object ready event should provide the needed
    // synchronization.
    //

    if (FileObject != NULL) {
        switch (FileObject->Properties.Type) {
        case IoObjectPipe:
        case IoObjectSocket:
        case IoObjectTerminalMaster:
        case IoObjectTerminalSlave:
        case IoObjectSharedMemoryObject:
            if (FileObject->SpecialIo == NULL) {

                ASSERT((Create != NULL) &&
                       (Create->Type == FileObject->Properties.Type));

                Status = IopCreateSpecialIoObject(FromKernelMode,
                                                  OpenFlags,
                                                  Create,
                                                  &FileObject);

                if (!KSUCCESS(Status)) {
                    goto PathLookupThroughFileSystemEnd;
                }
            }

            break;

        default:
            break;
        }
    }

    //
    // If a path point was already found, it's a negative one. Convert it.
    //

    if (FoundPathPoint != FALSE) {

        ASSERT(Negative == FALSE);
        ASSERT((Result->PathEntry->Negative != FALSE) &&
               (Result->PathEntry->FileObject == NULL) &&
               (FileObject->Device == PathRoot) &&
               (Result->MountPoint == Directory->MountPoint));

        Result->PathEntry->Negative = FALSE;
        Result->PathEntry->DoNotCache = DoNotCache;

        ASSERT(FileObject != NULL);
        ASSERT(FileObject->ReferenceCount >= 2);

        Result->PathEntry->FileObject = FileObject;
        IopFileObjectAddPathEntryReference(Result->PathEntry->FileObject);

    //
    // Create and insert a new path entry.
    //

    } else {
        PathEntry = IopCreatePathEntry(Name,
                                       NameSize,
                                       Hash,
                                       DirectoryEntry,
                                       FileObject);

        if (PathEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto PathLookupThroughFileSystemEnd;
        }

        ASSERT((Negative == FALSE) || (FileObject == NULL));

        PathEntry->Negative = Negative;
        PathEntry->DoNotCache = DoNotCache;

        //
        // There should be at least one hard link count and it needs to get
        // inserted into the directory's list of children.
        //

        ASSERT((FileObject == NULL) ||
               (FileObject->Properties.HardLinkCount != 0));

        INSERT_BEFORE(&(PathEntry->SiblingListEntry),
                      &(DirectoryEntry->ChildList));

        Result->PathEntry = PathEntry;
        IoMountPointAddReference(Directory->MountPoint);
        Result->MountPoint = Directory->MountPoint;
    }

    FileObject = NULL;
    Status = STATUS_SUCCESS;

    //
    // If this was actually a negative path entry, return path not found.
    //

    if (Negative != FALSE) {
        Status = STATUS_PATH_NOT_FOUND;
    }

PathLookupThroughFileSystemEnd:
    if (Child != NULL) {
        ObReleaseReference(Child);
    }

    if (FileObject != NULL) {
        IopFileObjectReleaseReference(FileObject);
    }

    return Status;
}

KSTATUS
IopFollowSymbolicLink (
    BOOL FromKernelMode,
    ULONG OpenFlags,
    ULONG RecursionLevel,
    PPATH_POINT Directory,
    PPATH_POINT SymbolicLink,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine attempts to follow the destination of a symbolic link.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        is coming directly from kernel mode (and should use the kernel's root).

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        handle. See OPEN_FLAG_* definitions.

    RecursionLevel - Supplies the level of recursion reached for this path
        resolution. If this is greater than the maximum supported, then a
        failing loop status is returned.

    Directory - Supplies the path point of the directory containing the
        symbolic link.

    SymbolicLink - Supplies the path point to the symbolic link.

    Result - Supplies a pointer to a path point that receives the resulting
        path entry and mount point on success. The path entry and mount point
        will come with an extra reference on them, so the caller must be sure
        to release the references when finished.

Return Value:

    Status code.

--*/

{

    PIO_HANDLE Handle;
    ULONG LinkOpenFlags;
    PSTR LinkTarget;
    ULONG LinkTargetSize;
    PCSTR RemainingPath;
    ULONG RemainingPathSize;
    KSTATUS Status;

    Handle = NULL;
    LinkTarget = NULL;
    Result->PathEntry = NULL;

    //
    // The buck stops here with infinite recursion.
    //

    if (RecursionLevel >= MAX_SYMBOLIC_LINK_RECURSION) {
        Status = STATUS_SYMBOLIC_LINK_LOOP;
        goto FollowSymbolicLinkEnd;
    }

    LinkOpenFlags = OPEN_FLAG_NO_ACCESS_TIME | OPEN_FLAG_SYMBOLIC_LINK;
    Status = IopOpenPathPoint(SymbolicLink,
                              IO_ACCESS_READ,
                              LinkOpenFlags,
                              &Handle);

    if (!KSUCCESS(Status)) {
        goto FollowSymbolicLinkEnd;
    }

    Status = IoReadSymbolicLink(Handle,
                                PATH_ALLOCATION_TAG,
                                &LinkTarget,
                                &LinkTargetSize);

    if (!KSUCCESS(Status)) {
        goto FollowSymbolicLinkEnd;
    }

    RemainingPath = LinkTarget;
    RemainingPathSize = LinkTargetSize;

    //
    // Perform a path walk starting at the directory where the symlink was
    // found. This gets reset if the symlink destination starts with a slash.
    //

    Status = IopPathWalkWorker(FromKernelMode,
                               Directory,
                               &RemainingPath,
                               &RemainingPathSize,
                               OpenFlags,
                               NULL,
                               RecursionLevel + 1,
                               Result);

    if (!KSUCCESS(Status)) {
        goto FollowSymbolicLinkEnd;
    }

FollowSymbolicLinkEnd:
    if (Handle != NULL) {
        IoClose(Handle);
    }

    if (LinkTarget != NULL) {
        MmFreePagedPool(LinkTarget);
    }

    return Status;
}

BOOL
IopArePathsEqual (
    PCSTR ExistingPath,
    PCSTR QueryPath,
    ULONG QuerySize
    )

/*++

Routine Description:

    This routine compares two path components.

Arguments:

    ExistingPath - Supplies a pointer to the existing null terminated path
        string.

    QueryPath - Supplies a pointer the query string, which may not be null
        terminated.

    QuerySize - Supplies the size of the string including the assumed
        null terminator that is never checked.

Return Value:

    TRUE if the paths are equals.

    FALSE if the paths differ in some way.

--*/

{

    ASSERT(QuerySize != 0);

    if (RtlAreStringsEqual(ExistingPath, QueryPath, QuerySize - 1) == FALSE) {
        return FALSE;
    }

    if (ExistingPath[QuerySize - 1] != STRING_TERMINATOR) {
        return FALSE;
    }

    return TRUE;
}

BOOL
IopFindPathPoint (
    PPATH_POINT Parent,
    ULONG OpenFlags,
    PCSTR Name,
    ULONG NameSize,
    ULONG Hash,
    PPATH_POINT Result
    )

/*++

Routine Description:

    This routine loops through the given path point's child path entries
    looking for a child with the given name. It follows any mount points it
    encounters unless the open flags specify otherwise. This routine assumes
    the parent's file object lock is held.

Arguments:

    Parent - Supplies a pointer to the parent path point whose children should
        be searched. The parent's file object lock should already be held.

    OpenFlags - Supplies a bitfield of flags governing the behavior of the
        search. See OPEN_FLAG_* definitions.

    Name - Supplies a pointer the query string, which may not be null
        terminated.

    NameSize - Supplies the size of the string including the assumed null
        terminator that is never checked.

    Hash - Supplies the hash of the name query string.

    Result - Supplies a pointer to a path point that receives the found path
        entry and associated mount point on success. References are taken on
        both elements if found.

Return Value:

    Returns TRUE if a matching path point was found, or FALSE otherwise.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPATH_ENTRY Entry;
    PMOUNT_POINT FoundMountPoint;
    PPATH_ENTRY FoundPathEntry;
    PFILE_OBJECT ParentFileObject;
    BOOL ResultValid;

    ResultValid = FALSE;
    ParentFileObject = Parent->PathEntry->FileObject;

    ASSERT(NameSize != 0);
    ASSERT(KeIsSharedExclusiveLockHeld(ParentFileObject->Lock) != FALSE);

    //
    // Cruise through the cached list looking for this entry.
    //

    CurrentEntry = Parent->PathEntry->ChildList.Next;
    while (CurrentEntry != &(Parent->PathEntry->ChildList)) {
        Entry = LIST_VALUE(CurrentEntry, PATH_ENTRY, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Quickly skip entries without a name or with the wrong hash.
        //

        if ((Entry->Hash != Hash) || (Entry->Name == NULL)) {
            continue;
        }

        //
        // If the names are not equal, this isn't the winner.
        //

        if (IopArePathsEqual(Entry->Name, Name, NameSize) == FALSE) {
            continue;
        }

        //
        // If the found entry is a mount point, then the parent mount point's
        // children are searched for a matching mount point. Note that this
        // search may fail as the path entry is not necessarily a mount point
        // under the current mount tree. It takes a reference on success. Skip
        // this if the open flags dictate that the final mount point should not
        // be followed.
        //

        FoundMountPoint = NULL;
        if ((Entry->MountCount != 0) &&
            ((OpenFlags & OPEN_FLAG_NO_MOUNT_POINT) == 0)) {

            FoundMountPoint = IopFindMountPoint(Parent->MountPoint, Entry);
            if (FoundMountPoint != NULL) {
                FoundPathEntry = FoundMountPoint->TargetEntry;
            }
        }

        //
        // Use the found entry and the same mount point as the parent if the
        // entry was found to not be a mount point.
        //

        if (FoundMountPoint == NULL) {
            FoundPathEntry = Entry;
            FoundMountPoint = Parent->MountPoint;
            IoMountPointAddReference(FoundMountPoint);
        }

        IoPathEntryAddReference(FoundPathEntry);
        Result->PathEntry = FoundPathEntry;
        Result->MountPoint = FoundMountPoint;
        ResultValid = TRUE;
        break;
    }

    return ResultValid;
}

VOID
IopPathEntryReleaseReference (
    PPATH_ENTRY Entry,
    BOOL EnforceCacheSize,
    BOOL Destroy
    )

/*++

Routine Description:

    This routine decrements the reference count of the given path entry.

Arguments:

    Entry - Supplies a pointer to the path entry.

    EnforceCacheSize - Supplies a boolean indicating whether or not to destroy
        extra path entries if the cache is too large. This is used to prevent
        recursion of releasing references.

    Destroy - Supplies a boolean indicating this path entry should be destroyed
        regardless of the path entry cache. It is used by the path entry cache
        to clean out entries.

Return Value:

    None.

--*/

{

    UINTN CacheTarget;
    PLIST_ENTRY CurrentEntry;
    PPATH_ENTRY DestroyEntry;
    LIST_ENTRY DestroyList;
    BOOL Inserted;
    PPATH_ENTRY NextEntry;
    PFILE_OBJECT NextFileObject;
    ULONG OldReferenceCount;
    PPATH_ENTRY Parent;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Inserted = FALSE;
    NextFileObject = NULL;
    while (Entry != NULL) {

        //
        // Acquire the parent's lock to avoid a situation where this routine
        // decrements the reference count to zero, but before removing it
        // someone else increments, decrements, removes and frees the object.
        //

        NextEntry = Entry->Parent;
        if (NextEntry != NULL) {
            NextFileObject = NextEntry->FileObject;
            KeAcquireSharedExclusiveLockExclusive(NextFileObject->Lock);
        }

        OldReferenceCount = RtlAtomicAdd32(&(Entry->ReferenceCount), -1);

        ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

        if (OldReferenceCount == 1) {

            //
            // Look to see if this entry should stick around on the path entry
            // cache list, unless it's being forcefully destroyed. Also skip
            // caching if the path entry has been marked for unmount.
            //

            if ((Destroy == FALSE) &&
                (Entry->DoNotCache == FALSE)) {

                //
                // Stick this on the LRU list. Don't cache anonymous path
                // entries (like those created for pipes and sockets) and don't
                // cache unlinked entries.
                //

                if ((Entry->SiblingListEntry.Next != NULL) &&
                    (NextEntry != NULL)) {

                    ASSERT(Entry->CacheListEntry.Next == NULL);

                    KeAcquireQueuedLock(IoPathEntryListLock);
                    INSERT_BEFORE(&(Entry->CacheListEntry), &IoPathEntryList);
                    IoPathEntryListSize += 1;
                    KeReleaseQueuedLock(IoPathEntryListLock);
                    Inserted = TRUE;
                }
            }

            //
            // Don't destroy the entry if it's in the cache.
            //

            if (Inserted != FALSE) {

                ASSERT((Destroy == FALSE) && (Entry->DoNotCache == FALSE));

                KeReleaseSharedExclusiveLockExclusive(NextFileObject->Lock);
                break;
            }

            //
            // Destroy the object, then loop back up to release the
            // reference on the parent.
            //

            Parent = IopDestroyPathEntry(Entry);

            ASSERT(Parent == NextEntry);

            //
            // The file object lock was released by the destory path entry
            // routine.
            //

            Entry = NextEntry;

        } else {
            if (NextEntry != NULL) {
                KeReleaseSharedExclusiveLockExclusive(NextFileObject->Lock);
            }

            Entry = NULL;
        }
    }

    //
    // If an entry was inserted in the cache and the cache size is to be
    // enforced, iterate over the cache, pulling off any inactive path entries.
    // This is not done above where the path entry list lock is acquired in
    // order to release the parent's file object lock first. It is held in
    // exclusive mode above.
    //

    if ((EnforceCacheSize != FALSE) && (Inserted != FALSE)) {
        INITIALIZE_LIST_HEAD(&DestroyList);
        KeAcquireQueuedLock(IoPathEntryListLock);
        CacheTarget = IopGetPathEntryCacheTargetSize();
        CurrentEntry = IoPathEntryList.Next;
        while ((IoPathEntryListSize > CacheTarget) &&
               (CurrentEntry != &IoPathEntryList)) {

            DestroyEntry = LIST_VALUE(CurrentEntry, PATH_ENTRY, CacheListEntry);
            CurrentEntry = CurrentEntry->Next;

            //
            // Add a reference to prevent others from manipulating the cache
            // list entry (which they would do if the reference count went up
            // to 1 and back down to 0).
            //

            OldReferenceCount = RtlAtomicAdd32(&(DestroyEntry->ReferenceCount),
                                               1);

            //
            // If this was not the first reference on the cached entry, then
            // another thread is working to resurrect it. Don't add it to the
            // list for destruction.
            //

            if (OldReferenceCount != 0) {
                RtlAtomicAdd32(&(DestroyEntry->ReferenceCount), -1);

            } else {
                LIST_REMOVE(&(DestroyEntry->CacheListEntry));
                INSERT_BEFORE(&(DestroyEntry->CacheListEntry), &DestroyList);
                IoPathEntryListSize -= 1;
            }
        }

        KeReleaseQueuedLock(IoPathEntryListLock);

        //
        // Destroy (or at least attempt to destroy) the entries on the destroy
        // list. This doesn't infinitely recurse because the forceful destroy
        // flag is set, circumventing this path.
        //

        CurrentEntry = DestroyList.Next;
        while (CurrentEntry != &DestroyList) {
            DestroyEntry = LIST_VALUE(CurrentEntry, PATH_ENTRY, CacheListEntry);
            CurrentEntry = CurrentEntry->Next;

            ASSERT(DestroyEntry->ReferenceCount >= 1);

            DestroyEntry->CacheListEntry.Next = NULL;
            IopPathEntryReleaseReference(DestroyEntry, FALSE, TRUE);
        }
    }

    return;
}

PPATH_ENTRY
IopDestroyPathEntry (
    PPATH_ENTRY Entry
    )

/*++

Routine Description:

    This routine frees the resources associated with the given path entry.
    This entry requires that the parent's file object lock is held exclusive
    upon entry. This routine will release that file object lock.

Arguments:

    Entry - Supplies a pointer to the path entry to actually destroy.

Return Value:

    Returns a pointer to the parent path entry whose reference now needs to be
    decremented.

    NULL if the path entry has no parent.

--*/

{

    PPATH_ENTRY Parent;
    PFILE_OBJECT ParentFileObject;

    ParentFileObject = NULL;

    //
    // Acquire the parent's lock to avoid a situation where this routine
    // decrements the reference count to zero, but before removing it
    // someone else increments, decrements, removes and frees the object.
    //

    Parent = Entry->Parent;
    if (Parent != NULL) {
        ParentFileObject = Parent->FileObject;

        //
        // The caller should have acquired the parent file object lock
        // exclusive.
        //

        ASSERT(KeIsSharedExclusiveLockHeldExclusive(ParentFileObject->Lock));

    }

    ASSERT(Entry->ReferenceCount == 0);

    //
    // Destroy the object.
    //

    ASSERT(LIST_EMPTY(&(Entry->ChildList)) != FALSE);
    ASSERT(Entry->CacheListEntry.Next == NULL);
    ASSERT(Entry != IoPathPointRoot.PathEntry);

    if (Parent != NULL) {

        //
        // If a path entry is created but never actually added because
        // someone beat it to the punch then it could have a parent
        // but not be on the list. Hence the check for null. This is
        // also necessary when releasing unmounted mount point path
        // entries.
        //

        if (Entry->SiblingListEntry.Next != NULL) {
            LIST_REMOVE(&(Entry->SiblingListEntry));
            Entry->SiblingListEntry.Next = NULL;
        }

        ASSERT(ParentFileObject != NULL);

        KeReleaseSharedExclusiveLockExclusive(ParentFileObject->Lock);
    }

    //
    // By the time a path entry gets destroyed, it should not be mounted
    // anywhere.
    //

    ASSERT(Entry->MountCount == 0);

    //
    // Release the file object and then the path root object. If they exist.
    //

    ASSERT((Entry->Negative != FALSE) || (Entry->FileObject != NULL));

    //
    // Decrement the count of path entries that own the file object.
    //

    if (Entry->Negative == FALSE) {
        IopFileObjectReleasePathEntryReference(Entry->FileObject);
        IopFileObjectReleaseReference(Entry->FileObject);
    }

    MmFreePagedPool(Entry);
    return Parent;
}

UINTN
IopGetPathEntryCacheTargetSize (
    VOID
    )

/*++

Routine Description:

    This routine returns the target size of the path entry cache.

Arguments:

    None.

Return Value:

    Returns the target size of the path entry cache.

--*/

{

    UINTN CurrentSize;
    MEMORY_WARNING_LEVEL WarningLevel;

    WarningLevel = MmGetPhysicalMemoryWarningLevel();
    if (WarningLevel == MemoryWarningLevelNone) {
        return IoPathEntryListMaxSize;

    //
    // At memory warning level one, start shrinking the path entry cache.
    //

    } else if (WarningLevel == MemoryWarningLevel1) {
        CurrentSize = IoPathEntryListSize;
        if (CurrentSize > IoPathEntryListMaxSize) {
            CurrentSize = IoPathEntryListMaxSize;
        }

        if (CurrentSize <= 1) {
            return CurrentSize;
        }

        return CurrentSize - 2;
    }

    //
    // At higher memory warning levels, dump the path entry cache.
    //

    return 0;
}

