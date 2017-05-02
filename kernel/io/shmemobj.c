/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shmemobj.c

Abstract:

    This module implements shared memory objects.

Author:

    Chris Stevens 12-Mar-2014

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
// Define a maximum region size for shared memory objects. This prevents a
// large shared memory object from requiring a massive contiguous chunk of the
// backing image (page file) just to page out a few pages. 128KB is also the
// systems default I/O size, so hopefully this helps to generate contiguous I/O.
// Do not increase this beyon 128KB so that the backing region's dirty bitmap
// can remain one ULONG.
//

#define MAX_SHARED_MEMORY_BACKING_REGION_SIZE _128KB

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a shared memory object backing region.

Members:

    ListEntry - Stores pointers to the next and previous shared memory object
        backing regions.

    ImageBacking - Stores the image backing handle for this region of the
        shared memory object.

    Offset - Stores the shared memory object file offset where this backing
        region starts.

    Size - Stores the size of the region, in bytes.

    DirtyBitmap - Stores a bitmap of which pages in the region have actually
        been written to the backing image. Clean pages cannot be read from, as
        they would hand back uninitialized data from the disk.

--*/

typedef struct _SHARED_MEMORY_BACKING_REGION {
    LIST_ENTRY ListEntry;
    IMAGE_BACKING ImageBacking;
    IO_OFFSET Offset;
    ULONG Size;
    ULONG DirtyBitmap;
} SHARED_MEMORY_BACKING_REGION, *PSHARED_MEMORY_BACKING_REGION;

/*++

Structure Description:

    This structure defines a shared memory object.

Members:

    Header - Stores the object header.

    FileObject - Stores a pointer to the file object associated with the shared
        memory object.

    Lock - Stores a pointer to a shared exclusive lock that protects access to
        the shared memory object, including the backing region list.

    BackingRegionList - Stores the list of backing regions for this shared
        memory object.

    Properties - Stores the current properties of the shared memory object.

--*/

typedef struct _SHARED_MEMORY_OBJECT {
    OBJECT_HEADER Header;
    PFILE_OBJECT FileObject;
    PSHARED_EXCLUSIVE_LOCK Lock;
    LIST_ENTRY BackingRegionList;
    SHARED_MEMORY_PROPERTIES Properties;
} SHARED_MEMORY_OBJECT, *PSHARED_MEMORY_OBJECT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
IopDestroySharedMemoryObject (
    PVOID SharedMemoryObject
    );

PSHARED_MEMORY_BACKING_REGION
IopCreateSharedMemoryBackingRegion (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    PSHARED_MEMORY_BACKING_REGION NextRegion
    );

VOID
IopDestroySharedMemoryBackingRegion (
    PSHARED_MEMORY_BACKING_REGION Region
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the shared memory object root path point.
//

PATH_POINT IoSharedMemoryRoot;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopInitializeSharedMemoryObjectSupport (
    VOID
    )

/*++

Routine Description:

    This routine is called during system initialization to set up support for
    shared memory objects.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PFILE_OBJECT FileObject;
    PVOID Object;
    PPATH_ENTRY PathEntry;
    FILE_PROPERTIES Properties;
    KSTATUS Status;

    FileObject = NULL;

    //
    // Create the shared memory object directory.
    //

    Object = ObCreateObject(ObjectDirectory,
                            NULL,
                            "SharedMemoryObject",
                            sizeof("SharedMemoryObject"),
                            sizeof(OBJECT_HEADER),
                            NULL,
                            OBJECT_FLAG_USE_NAME_DIRECTLY,
                            FI_ALLOCATION_TAG);

    if (Object == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    //
    // Open a path point to this shared memory object root.
    //

    IopFillOutFilePropertiesForObject(&Properties, Object);
    Status = IopCreateOrLookupFileObject(&Properties,
                                         ObGetRootObject(),
                                         FILE_OBJECT_FLAG_EXTERNAL_IO_STATE,
                                         0,
                                         &FileObject,
                                         &Created);

    if (!KSUCCESS(Status)) {
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    ASSERT(Created != FALSE);

    KeSignalEvent(FileObject->ReadyEvent, SignalOptionSignalAll);
    PathEntry = IopCreatePathEntry(NULL, 0, 0, NULL, FileObject);
    if (PathEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeSharedMemoryObjectSupportEnd;
    }

    ASSERT(IoPathPointRoot.MountPoint != NULL);

    IoSharedMemoryRoot.PathEntry = PathEntry;
    IoSharedMemoryRoot.MountPoint = IoPathPointRoot.MountPoint;
    IoMountPointAddReference(IoSharedMemoryRoot.MountPoint);

InitializeSharedMemoryObjectSupportEnd:
    if (!KSUCCESS(Status)) {
        if (Object != NULL) {
            ObReleaseReference(Object);
        }

        //
        // If the file object was not created, an additional reference needs to
        // be release on the shared memory object (taken for the properties).
        //

        if (FileObject != NULL) {
            IopFileObjectReleaseReference(FileObject);

        } else {
            ObReleaseReference(Object);
        }
    }

    return Status;
}

PPATH_POINT
IopGetSharedMemoryDirectory (
    BOOL FromKernelMode
    )

/*++

Routine Description:

    This routine returns the current process' shared memory directory. This is
    the only place the current process is allowed to create shared memory
    objects.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

Return Value:

    Returns a pointer to the process' shared memory directory.

--*/

{

    PPATH_POINT Directory;
    PKPROCESS Process;

    //
    // The shared memory object directory can only be changed by
    // single-threaded processes. Thus the path lock does not need to be held.
    //

    if (FromKernelMode != FALSE) {
        Process = PsGetKernelProcess();

    } else {
        Process = PsGetCurrentProcess();
    }

    Directory = (PPATH_POINT)&(Process->Paths.SharedMemoryDirectory);
    if (Directory->PathEntry == NULL) {
        Directory = &IoSharedMemoryRoot;
    }

    return Directory;
}

KSTATUS
IopCreateSharedMemoryObject (
    BOOL FromKernelMode,
    PCSTR Name,
    ULONG NameSize,
    ULONG Flags,
    PCREATE_PARAMETERS Create,
    PFILE_OBJECT *FileObject
    )

/*++

Routine Description:

    This routine actually creates a new shared memory object.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not the request
        originated from kernel mode (TRUE) or user mode (FALSE).

    Name - Supplies an optional pointer to the shared memory object name. This
        is only used for shared memory objects created in the shared memory
        directory.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

    Flags - Supplies a bitfield of flags governing the behavior of the handle.
        See OPEN_FLAG_* definitions.

    Create - Supplies a pointer to the creation parameters.

    FileObject - Supplies a pointer where a pointer to a newly created pipe
        file object will be returned on success.

Return Value:

    Status code.

--*/

{

    BOOL Created;
    PFILE_OBJECT DirectoryFileObject;
    POBJECT_HEADER DirectoryObject;
    PPATH_POINT DirectoryPathPoint;
    PSHARED_MEMORY_OBJECT ExistingObject;
    FILE_ID FileId;
    ULONG FileObjectFlags;
    FILE_PROPERTIES FileProperties;
    PFILE_OBJECT NewFileObject;
    PSHARED_MEMORY_PROPERTIES Properties;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;
    PKTHREAD Thread;

    NewFileObject = NULL;
    SharedMemoryObject = NULL;
    Thread = KeGetCurrentThread();

    //
    // Get the shared memory object directory for the process.
    //

    DirectoryPathPoint = IopGetSharedMemoryDirectory(FromKernelMode);
    DirectoryFileObject = DirectoryPathPoint->PathEntry->FileObject;

    ASSERT(DirectoryFileObject->Properties.Type == IoObjectObjectDirectory);

    FileId = DirectoryFileObject->Properties.FileId;
    DirectoryObject = (POBJECT_HEADER)(UINTN)FileId;

    //
    // Make sure there is not already an existing shared memory object by the
    // same name. The caller should have the appropriate locks to make the
    // check and create synchronous.
    //

    if (Name != NULL) {
        ExistingObject = ObFindObject(Name, NameSize, DirectoryObject);
        if (ExistingObject != NULL) {
            ObReleaseReference(ExistingObject);
            Status = STATUS_FILE_EXISTS;
            goto CreateSharedMemoryObjectEnd;
        }
    }

    SharedMemoryObject = ObCreateObject(ObjectSharedMemoryObject,
                                        DirectoryObject,
                                        Name,
                                        NameSize,
                                        sizeof(SHARED_MEMORY_OBJECT),
                                        IopDestroySharedMemoryObject,
                                        0,
                                        IO_ALLOCATION_TAG);

    if (SharedMemoryObject == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedMemoryObjectEnd;
    }

    INITIALIZE_LIST_HEAD(&(SharedMemoryObject->BackingRegionList));
    Properties = &(SharedMemoryObject->Properties);
    Properties->CreatorPid = Thread->OwningProcess->Identifiers.ProcessId;
    KeGetSystemTime(&(Properties->ChangeTime));
    Properties->Permissions.Permissions = Create->Permissions;
    Properties->Permissions.OwnerUserId = Thread->Identity.EffectiveUserId;
    Properties->Permissions.CreatorUserId = Thread->Identity.EffectiveUserId;
    Properties->Permissions.OwnerGroupId = Thread->Identity.EffectiveGroupId;
    Properties->Permissions.CreatorGroupId = Thread->Identity.EffectiveGroupId;
    SharedMemoryObject->Lock = KeCreateSharedExclusiveLock();
    if (SharedMemoryObject->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedMemoryObjectEnd;
    }

    //
    // If necessary, create a file object that points to the memory object.
    //

    if (*FileObject == NULL) {
        RtlZeroMemory(&FileProperties, sizeof(FILE_PROPERTIES));
        IopFillOutFilePropertiesForObject(&FileProperties,
                                          &(SharedMemoryObject->Header));

        FileProperties.Permissions = Create->Permissions;
        FileProperties.BlockSize = IoGetCacheEntryDataSize();
        FileProperties.Type = IoObjectSharedMemoryObject;
        FileObjectFlags = FILE_OBJECT_FLAG_EXTERNAL_IO_STATE |
                          FILE_OBJECT_FLAG_HARD_FLUSH_REQUIRED;

        Status = IopCreateOrLookupFileObject(&FileProperties,
                                             ObGetRootObject(),
                                             FileObjectFlags,
                                             0,
                                             &NewFileObject,
                                             &Created);

        if (!KSUCCESS(Status)) {

            //
            // Release reference taken when filling out the file properties.
            //

            ObReleaseReference(SharedMemoryObject);
            goto CreateSharedMemoryObjectEnd;
        }

        ASSERT(Created != FALSE);

        *FileObject = NewFileObject;
    }

    //
    // If the shared memory object is named, then it is valid until it is
    // unlinked. So, add a reference to the file object to make sure the create
    // premissions stick around.
    //

    if (Name != NULL) {
        IopFileObjectAddReference(*FileObject);
        SharedMemoryObject->FileObject = *FileObject;
    }

    (*FileObject)->SpecialIo = SharedMemoryObject;
    Create->Created = TRUE;
    Status = STATUS_SUCCESS;

CreateSharedMemoryObjectEnd:

    //
    // On both success and failure, the file object's ready event needs to be
    // signaled. Other threads may be waiting on the event.
    //

    if (*FileObject != NULL) {
        KeSignalEvent(NewFileObject->ReadyEvent, SignalOptionSignalAll);
    }

    if (!KSUCCESS(Status)) {
        if ((SharedMemoryObject != NULL) &&
            (*FileObject != NULL) &&
            ((*FileObject)->SpecialIo != SharedMemoryObject)) {

            ObReleaseReference(SharedMemoryObject);
        }

        if (NewFileObject != NULL) {
            IopFileObjectReleaseReference(NewFileObject);
            NewFileObject = NULL;
        }
    }

    *FileObject = NewFileObject;
    return Status;
}

KSTATUS
IopTruncateSharedMemoryObject (
    PFILE_OBJECT FileObject,
    ULONGLONG NewSize
    )

/*++

Routine Description:

    This routine truncates a shared memory object. It assumes that the file's
    lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    NewSize - Supplies the new size to set.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    ULONGLONG FileSize;
    ULONG PageCount;
    ULONG PageShift;
    ULONG PageSize;
    PSHARED_MEMORY_BACKING_REGION Region;
    ULONG RegionSize;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;

    ASSERT(FileObject->Properties.Type == IoObjectSharedMemoryObject);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    SharedMemoryObject = FileObject->SpecialIo;
    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // If the file is decreasing in size, then free page file regions beyond
    // the end of the file size.
    //

    FileSize = FileObject->Properties.Size;
    if (FileSize > NewSize) {
        KeAcquireSharedExclusiveLockExclusive(SharedMemoryObject->Lock);
        CurrentEntry = SharedMemoryObject->BackingRegionList.Next;
        while (CurrentEntry != &(SharedMemoryObject->BackingRegionList)) {
            Region = LIST_VALUE(CurrentEntry,
                                SHARED_MEMORY_BACKING_REGION,
                                ListEntry);

            CurrentEntry = CurrentEntry->Next;

            //
            // If the region is beyond the end of the new file size, then the
            // whole region should be released from the page file.
            //

            if (Region->Offset >= NewSize) {
                LIST_REMOVE(&(Region->ListEntry));
                Region->ListEntry.Next = NULL;
                IopDestroySharedMemoryBackingRegion(Region);

            //
            // If only the end is beyond the new size, don't bother with a
            // partial page file free in case the file grows again. Just clear
            // the dirty bitmap for pages beyond the end of the file.
            //

            } else if ((Region->Offset + Region->Size) > NewSize) {
                RegionSize = (ULONG)(NewSize - Region->Offset);
                RegionSize = ALIGN_RANGE_UP(RegionSize, PageSize);
                PageCount = RegionSize >> PageShift;
                Region->DirtyBitmap &= (1 << PageCount) - 1;
            }
        }

        KeReleaseSharedExclusiveLockExclusive(SharedMemoryObject->Lock);
    }

    FileObject->Properties.Size = NewSize;
    SharedMemoryObject->Properties.Size = NewSize;
    KeGetSystemTime(&(SharedMemoryObject->Properties.ChangeTime));
    return STATUS_SUCCESS;
}

KSTATUS
IopUnlinkSharedMemoryObject (
    PFILE_OBJECT FileObject,
    PBOOL Unlinked
    )

/*++

Routine Description:

    This routine unlinks a shared memory object from the accessible namespace.

Arguments:

    FileObject - Supplies a pointer to the file object that owns the shared
        memory object.

    Unlinked - Supplies a pointer to a boolean that receives whether or not the
        shared memory object was successfully unlinked.

Return Value:

    Status code.

--*/

{

    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;

    ASSERT(FileObject->Properties.Type == IoObjectSharedMemoryObject);
    ASSERT(KeIsSharedExclusiveLockHeldExclusive(FileObject->Lock) != FALSE);

    SharedMemoryObject = FileObject->SpecialIo;
    *Unlinked = FALSE;
    Status = ObUnlinkObject(SharedMemoryObject);
    if (KSUCCESS(Status)) {

        //
        // Named shared memory objects hold on to the file object, so they
        // cannot disappear until they are unlinked. Unnamed shared memory
        // objects do not have a file object pointer, as those can go away as
        // soon as the last close happens. That said, don't fall over if an
        // unlink gets sent to an unnamed object.
        //

        if (SharedMemoryObject->FileObject != NULL) {

            ASSERT(SharedMemoryObject->FileObject == FileObject);

            IopFileObjectReleaseReference(SharedMemoryObject->FileObject);
            SharedMemoryObject->FileObject = NULL;
        }

        SharedMemoryObject->Properties.Permissions.Permissions |=
                                               SHARED_MEMORY_PROPERTY_UNLINKED;

        *Unlinked = TRUE;
    }

    return Status;
}

KSTATUS
IopPerformSharedMemoryIoOperation (
    PFILE_OBJECT FileObject,
    PIO_CONTEXT IoContext
    )

/*++

Routine Description:

    This routine performs a non-cached I/O operation on a shared memory object.
    It is assumed that the file lock is held. This routine will always modify
    the file size in the the file properties and conditionally modify the file
    size in the file object.

Arguments:

    FileObject - Supplies a pointer to the file object for the device or file.

    IoContext - Supplies a pointer to the I/O context.

Return Value:

    Status code.

--*/

{

    UINTN AlignedSize;
    UINTN BytesCompleted;
    UINTN BytesCompletedThisRound;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    PLIST_ENTRY CurrentEntry;
    IO_OFFSET CurrentOffset;
    ULONGLONG FileSize;
    IO_OFFSET IoEnd;
    BOOL LockHeld;
    PSHARED_MEMORY_OBJECT MemoryObject;
    UINTN OriginalIoBufferOffset;
    ULONG PageCount;
    ULONG PageIndex;
    ULONG PageMask;
    ULONG PageShift;
    ULONG PageSize;
    PSHARED_MEMORY_BACKING_REGION Region;
    IO_OFFSET RegionEnd;
    ULONG RegionOffset;
    KSTATUS Status;

    ASSERT(IoContext->IoBuffer != NULL);
    ASSERT(KeIsSharedExclusiveLockHeld(FileObject->Lock) != FALSE);
    ASSERT(IO_IS_FILE_OBJECT_CACHEABLE(FileObject) != FALSE);

    MemoryObject = FileObject->SpecialIo;
    Status = STATUS_SUCCESS;
    PageShift = MmPageShift();
    PageSize = MmPageSize();
    BytesCompleted = 0;
    LockHeld = FALSE;
    OriginalIoBufferOffset = MmGetIoBufferCurrentOffset(IoContext->IoBuffer);
    AlignedSize = ALIGN_RANGE_UP(IoContext->SizeInBytes, PageSize);

    //
    // If this is a write operation but not a hard flush request, just act like
    // the write succeeded. Don't actually preserve the data to disk.
    //

    LockHeld = TRUE;
    if (IoContext->Write != FALSE) {
        if ((IoContext->Flags & IO_FLAG_HARD_FLUSH) == 0) {
            BytesCompleted = IoContext->SizeInBytes;
            KeAcquireSharedExclusiveLockExclusive(MemoryObject->Lock);
            goto PerformSharedMemoryIoOperationEnd;
        }

        //
        // The backing file write is a no-allocate IRP path. Make sure the I/O
        // buffer is mapped before the write happens.
        //

        MmMapIoBuffer(IoContext->IoBuffer, FALSE, FALSE, FALSE);
        KeAcquireSharedExclusiveLockExclusive(MemoryObject->Lock);

    } else {

        //
        // The backing file read is a no-allocate IRP path. It is not allowed
        // to extend the I/O buffers. Zero the buffer.
        //

        MmZeroIoBuffer(IoContext->IoBuffer, 0, AlignedSize);
        KeAcquireSharedExclusiveLockShared(MemoryObject->Lock);
    }

    //
    // All I/O should be page aligned and less than the block-aligned file
    // size.
    //

    FileSize = FileObject->Properties.Size;

    ASSERT(IS_ALIGNED(IoContext->Offset, PageSize) != FALSE);
    ASSERT((IoContext->Offset + IoContext->SizeInBytes) <=
           ALIGN_RANGE_UP(FileSize, FileObject->Properties.BlockSize));

    BytesRemaining = AlignedSize;

    //
    // Look through the backing regions for the right areas of the backing
    // image (page file) to read from and write to.
    //

    CurrentOffset = IoContext->Offset;
    CurrentEntry = MemoryObject->BackingRegionList.Next;
    IoEnd = CurrentOffset + BytesRemaining;
    while (BytesRemaining != 0) {

        //
        // If the current entry is the head of the list, there are no more
        // backing regions. One will have to be allocated below.
        //

        if (CurrentEntry == &(MemoryObject->BackingRegionList)) {
            Region = NULL;
            BytesThisRound = BytesRemaining;

        //
        // Get the next region and determine if any of it overlaps with the
        // I/O offset and size.
        //

        } else {
            Region = LIST_VALUE(CurrentEntry,
                                SHARED_MEMORY_BACKING_REGION,
                                ListEntry);

            RegionEnd = Region->Offset + Region->Size;
            if (CurrentOffset >= RegionEnd) {
                CurrentEntry = CurrentEntry->Next;
                continue;
            }

            if (RegionEnd < IoEnd) {
                BytesThisRound = RegionEnd - CurrentOffset;

            } else {
                BytesThisRound = IoEnd - CurrentOffset;
            }
        }

        ASSERT(IS_ALIGNED(BytesThisRound, PageSize) != FALSE);

        //
        // If there is no region or the region starts beyond the current
        // offset, then there is a gap in the backing regions. If this is only
        // a read, the I/O buffer was zeroed above and the gap can be skipped.
        // If this is a write, then allocate a new region to fill the gap.
        //

        if ((Region == NULL) || (CurrentOffset < Region->Offset)) {
            if (IoContext->Write == FALSE) {
                MmIoBufferIncrementOffset(IoContext->IoBuffer, BytesThisRound);
                BytesRemaining -= BytesThisRound;
                BytesCompleted += BytesThisRound;
                CurrentOffset += BytesThisRound;

                ASSERT((Region != NULL) || (BytesRemaining == 0));

                if (Region != NULL) {
                    CurrentEntry = CurrentEntry->Next;
                }

            } else {
                Region = IopCreateSharedMemoryBackingRegion(FileObject,
                                                            CurrentOffset,
                                                            Region);

                if (Region == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto PerformSharedMemoryIoOperationEnd;
                }

                CurrentEntry = &(Region->ListEntry);
            }

            continue;
        }

        RegionOffset = (ULONG)(CurrentOffset - Region->Offset);

        //
        // On read, the backing entry may contain some invalid data. Only read
        // from the pages that were previously marked dirty by a write.
        //

        if (IoContext->Write == FALSE) {

            //
            // Skip clean pages, they were already zeroed above.
            //

            PageIndex = RegionOffset >> PageShift;
            PageCount = BytesThisRound >> PageShift;
            PageMask = (1 << PageCount) - 1;
            PageMask &= (Region->DirtyBitmap >> PageIndex);
            if (PageMask != 0) {
                PageCount = RtlCountTrailingZeros32(PageMask);
            }

            BytesThisRound = PageCount << PageShift;
            MmIoBufferIncrementOffset(IoContext->IoBuffer, BytesThisRound);
            BytesRemaining -= BytesThisRound;
            BytesCompleted += BytesThisRound;
            CurrentOffset += BytesThisRound;
            RegionOffset += BytesThisRound;

            //
            // Count the number of dirty pages that actually need to be read.
            // If they are all clean, skip ahead to the next entry if necessary.
            //

            BytesThisRound = 0;
            if (PageMask != 0) {
                PageCount = RtlCountSetBits32(PageMask);
                BytesThisRound = PageCount << PageShift;
            }

            if (BytesThisRound == 0) {

                ASSERT((CurrentOffset == RegionEnd) || (BytesRemaining == 0));

                CurrentEntry = CurrentEntry->Next;
                continue;
            }
        }

        Status = MmPageFilePerformIo(&(Region->ImageBacking),
                                     IoContext->IoBuffer,
                                     RegionOffset,
                                     BytesThisRound,
                                     IoContext->Flags,
                                     IoContext->TimeoutInMilliseconds,
                                     IoContext->Write,
                                     &BytesCompletedThisRound);

        if (!KSUCCESS(Status)) {
            goto PerformSharedMemoryIoOperationEnd;
        }

        ASSERT(BytesThisRound == BytesCompletedThisRound);

        //
        // Update the dirty bitmap on write, so that the next read will go get
        // the saved page(s).
        //

        if (IoContext->Write != FALSE) {
            PageIndex = RegionOffset >> PageShift;
            PageMask = (1 << (BytesCompletedThisRound >> PageShift)) - 1;
            Region->DirtyBitmap |= (PageMask << PageIndex);
        }

        MmIoBufferIncrementOffset(IoContext->IoBuffer, BytesCompletedThisRound);
        BytesRemaining -= BytesCompletedThisRound;
        BytesCompleted += BytesCompletedThisRound;
        CurrentOffset += BytesCompletedThisRound;
        if (CurrentOffset >= RegionEnd) {
            CurrentEntry = CurrentEntry->Next;
        }
    }

PerformSharedMemoryIoOperationEnd:

    //
    // The I/O size may have been aligned up to a page. Make sure the bytes
    // completed is not larger than the request.
    //

    if (BytesCompleted > IoContext->SizeInBytes) {
        BytesCompleted = IoContext->SizeInBytes;
    }

    if ((IoContext->Write != FALSE) && (BytesCompleted != 0)) {
        FileSize = IoContext->Offset + BytesCompleted;
        MemoryObject->Properties.Size = FileSize;
        IopUpdateFileObjectFileSize(FileObject, FileSize);
    }

    if (LockHeld != FALSE) {
        if (IoContext->Write != FALSE) {
            KeReleaseSharedExclusiveLockExclusive(MemoryObject->Lock);

        } else {
            KeReleaseSharedExclusiveLockShared(MemoryObject->Lock);
        }
    }

    MmSetIoBufferCurrentOffset(IoContext->IoBuffer, OriginalIoBufferOffset);
    IoContext->BytesCompleted = BytesCompleted;
    return Status;
}

KSTATUS
IopSharedMemoryNotifyFileMapping (
    PFILE_OBJECT FileObject,
    BOOL Mapping
    )

/*++

Routine Description:

    This routine is called to notify a shared memory object that it is being
    mapped into memory or unmapped.

Arguments:

    FileObject - Supplies a pointer to the file object being mapped.

    Mapping - Supplies a boolean indicating if a new mapping is being created
        (TRUE) or an old mapping is being destroyed (FALSE).

Return Value:

    Status code.

--*/

{

    UINTN Add;
    PKPROCESS Process;
    PSHARED_MEMORY_OBJECT SharedMemory;

    SharedMemory = FileObject->SpecialIo;
    if (Mapping != FALSE) {
        Add = 1;
        KeGetSystemTime(&(SharedMemory->Properties.AttachTime));

    } else {
        Add = (UINTN)-1L;
        KeGetSystemTime(&(SharedMemory->Properties.DetachTime));
    }

    RtlAtomicAdd(&(SharedMemory->Properties.AttachCount), Add);
    Process = PsGetCurrentProcess();
    SharedMemory->Properties.LastPid = Process->Identifiers.ProcessId;
    return STATUS_SUCCESS;
}

KSTATUS
IopSharedMemoryUserControl (
    PIO_HANDLE Handle,
    SHARED_MEMORY_COMMAND CodeNumber,
    BOOL FromKernelMode,
    PVOID ContextBuffer,
    UINTN ContextBufferSize
    )

/*++

Routine Description:

    This routine handles user control requests destined for a shared memory
    object.

Arguments:

    Handle - Supplies the open file handle.

    CodeNumber - Supplies the minor code of the request.

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    ContextBuffer - Supplies a pointer to the context buffer allocated by the
        caller for the request.

    ContextBufferSize - Supplies the size of the supplied context buffer.

Return Value:

    Status code.

--*/

{

    USER_ID EffectiveId;
    PFILE_OBJECT FileObject;
    BOOL Locked;
    BOOL LockedExclusive;
    FILE_PERMISSIONS Permissions;
    PSHARED_MEMORY_OBJECT SharedMemory;
    KSTATUS Status;
    PKTHREAD Thread;
    SHARED_MEMORY_PROPERTIES UserProperties;

    FileObject = Handle->FileObject;
    Locked = FALSE;
    LockedExclusive = FALSE;
    SharedMemory = FileObject->SpecialIo;
    Thread = KeGetCurrentThread();
    EffectiveId = Thread->Identity.EffectiveUserId;
    switch (CodeNumber) {

    //
    // Unlink the shared memory object if the effective user ID is the owner
    // or creator, or the process has special permission.
    //

    case SharedMemoryCommandUnlink:
        if ((EffectiveId != SharedMemory->Properties.Permissions.OwnerUserId) &&
            (EffectiveId !=
             SharedMemory->Properties.Permissions.CreatorUserId) &&
            (!KSUCCESS(PsCheckPermission(PERMISSION_IPC)))) {

            Status = STATUS_PERMISSION_DENIED;
            goto SharedMemoryUserControlEnd;
        }

        //
        // Delete as kernel mode, which skips the permission checks
        // (done above).
        //

        Status = IopDeleteByHandle(TRUE, Handle, DELETE_FLAG_SHARED_MEMORY);
        break;

    //
    // Set the owner user, owner group, and permission set of the object. The
    // caller must be the owner or creator, or must have special permission.
    //

    case SharedMemoryCommandSet:
        KeAcquireSharedExclusiveLockExclusive(FileObject->Lock);
        Locked = TRUE;
        LockedExclusive = TRUE;
        if ((EffectiveId != SharedMemory->Properties.Permissions.OwnerUserId) &&
            (EffectiveId !=
             SharedMemory->Properties.Permissions.CreatorUserId) &&
            (!KSUCCESS(PsCheckPermission(PERMISSION_IPC)))) {

            Status = STATUS_PERMISSION_DENIED;
            goto SharedMemoryUserControlEnd;
        }

        if (ContextBufferSize < sizeof(SHARED_MEMORY_PROPERTIES)) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto SharedMemoryUserControlEnd;
        }

        if (FromKernelMode != FALSE) {
            RtlCopyMemory(&UserProperties,
                          ContextBuffer,
                          sizeof(SHARED_MEMORY_PROPERTIES));

        } else {
            Status = MmCopyFromUserMode(&UserProperties,
                                        ContextBuffer,
                                        sizeof(SHARED_MEMORY_PROPERTIES));

            if (!KSUCCESS(Status)) {
                goto SharedMemoryUserControlEnd;
            }
        }

        Permissions = UserProperties.Permissions.Permissions &
                      FILE_PERMISSION_ALL;

        SharedMemory->Properties.Permissions.OwnerUserId =
                                       UserProperties.Permissions.OwnerUserId;

        SharedMemory->Properties.Permissions.OwnerGroupId =
                                      UserProperties.Permissions.OwnerGroupId;

        SharedMemory->Properties.Permissions.Permissions =
            (SharedMemory->Properties.Permissions.Permissions &
             ~FILE_PERMISSION_ALL) |
            Permissions;

        FileObject->Properties.UserId = UserProperties.Permissions.OwnerUserId;
        FileObject->Properties.GroupId =
                                      UserProperties.Permissions.OwnerGroupId;

        FileObject->Properties.Permissions = Permissions;
        KeGetSystemTime(&(FileObject->Properties.StatusChangeTime));
        SharedMemory->Properties.ChangeTime =
                                       FileObject->Properties.StatusChangeTime;

        break;

    //
    // Get the info. The caller must have read access to the object.
    //

    case SharedMemoryCommandStat:
        KeAcquireSharedExclusiveLockShared(FileObject->Lock);
        Locked = TRUE;
        Status = IopCheckPermissions(FALSE,
                                     &(Handle->PathPoint),
                                     IO_ACCESS_READ);

        if (!KSUCCESS(Status)) {
            goto SharedMemoryUserControlEnd;
        }

        if (ContextBufferSize < sizeof(SHARED_MEMORY_PROPERTIES)) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto SharedMemoryUserControlEnd;
        }

        if (FromKernelMode != FALSE) {
            RtlCopyMemory(ContextBuffer,
                          &(SharedMemory->Properties),
                          sizeof(SHARED_MEMORY_PROPERTIES));

        } else {
            Status = MmCopyToUserMode(ContextBuffer,
                                      &(SharedMemory->Properties),
                                      sizeof(SHARED_MEMORY_PROPERTIES));
        }

        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        goto SharedMemoryUserControlEnd;
    }

SharedMemoryUserControlEnd:
    if (Locked != FALSE) {
        if (LockedExclusive != FALSE) {
            KeReleaseSharedExclusiveLockExclusive(FileObject->Lock);

        } else {
            KeReleaseSharedExclusiveLockShared(FileObject->Lock);
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions

VOID
IopDestroySharedMemoryObject (
    PVOID Object
    )

/*++

Routine Description:

    This routine destroys the given shared memory object.

Arguments:

    Object - Supplies a pointer to the shared memory object to be destroyed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSHARED_MEMORY_BACKING_REGION Region;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;

    SharedMemoryObject = (PSHARED_MEMORY_OBJECT)Object;

    //
    // Release backing region resources.
    //

    CurrentEntry = SharedMemoryObject->BackingRegionList.Next;
    while (CurrentEntry != &(SharedMemoryObject->BackingRegionList)) {
        Region = LIST_VALUE(CurrentEntry,
                            SHARED_MEMORY_BACKING_REGION,
                            ListEntry);

        CurrentEntry = CurrentEntry->Next;
        Region->ListEntry.Next = NULL;
        IopDestroySharedMemoryBackingRegion(Region);
    }

    if (SharedMemoryObject->Lock != NULL) {
        KeDestroySharedExclusiveLock(SharedMemoryObject->Lock);
    }

    return;
}

PSHARED_MEMORY_BACKING_REGION
IopCreateSharedMemoryBackingRegion (
    PFILE_OBJECT FileObject,
    IO_OFFSET Offset,
    PSHARED_MEMORY_BACKING_REGION NextRegion
    )

/*++

Routine Description:

    This routine creates a new shared memory object backing region for the
    given file object and allocates the associated page file space. It inserts
    the new backing region on the end of the backing region list and assumes
    the file object's lock is held exclusively.

Arguments:

    FileObject - Supplies a pointer to the file object for the shared memory
        object.

    Offset - Supplies the desired file offset of the backing region.

    NextRegion - Supplies a pointer to the backing region before which the new
        region should be placed. This parameter should be NULL if the region is
        to be placed at the end of the list.

Return Value:

    Returns a pointer to the newly allocated shared memory object region on
    success. NULL on failure.

--*/

{

    PSHARED_MEMORY_BACKING_REGION NewRegion;
    ULONG PageSize;
    IO_OFFSET PreviousEnd;
    PSHARED_MEMORY_BACKING_REGION PreviousRegion;
    IO_OFFSET RegionEnd;
    PLIST_ENTRY RegionList;
    IO_OFFSET RegionOffset;
    UINTN RegionSize;
    ULONG RetryCount;
    PSHARED_MEMORY_OBJECT SharedMemoryObject;
    KSTATUS Status;

    SharedMemoryObject = FileObject->SpecialIo;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(SharedMemoryObject->Lock));

    NewRegion = MmAllocatePagedPool(sizeof(SHARED_MEMORY_BACKING_REGION),
                                   IO_ALLOCATION_TAG);

    if (NewRegion == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateSharedMemoryObjectBackingRegionEnd;
    }

    RtlZeroMemory(NewRegion, sizeof(SHARED_MEMORY_BACKING_REGION));
    NewRegion->ImageBacking.DeviceHandle = INVALID_HANDLE;

    //
    // The next region was supplied. Find the previous region.
    //

    RegionList = &(SharedMemoryObject->BackingRegionList);
    PreviousRegion = NULL;
    if (NextRegion == NULL) {
        if (LIST_EMPTY(RegionList) == FALSE) {
            PreviousRegion = LIST_VALUE(RegionList->Previous,
                                        SHARED_MEMORY_BACKING_REGION,
                                        ListEntry);
        }

    } else {
        if (NextRegion->ListEntry.Previous != RegionList) {
            PreviousRegion = LIST_VALUE(NextRegion->ListEntry.Previous,
                                        SHARED_MEMORY_BACKING_REGION,
                                        ListEntry);
        }
    }

    //
    // Try to allocate backing regions of the maximum size, aligning the region
    // off down. Fail gracefully if page file space appears to be tight. This
    // might extend beyond the end of the file, but that's OK. It will only get
    // touched if the file grows.
    //

    RetryCount = 0;
    RegionSize = MAX_SHARED_MEMORY_BACKING_REGION_SIZE;
    PageSize = MmPageSize();
    Status = STATUS_INSUFFICIENT_RESOURCES;
    while (RegionSize >= PageSize) {
        RegionOffset = ALIGN_RANGE_DOWN(Offset, RegionSize);

        //
        // Adjust the offset and size based on the previous and next regions.
        //

        if (PreviousRegion != NULL) {
            PreviousEnd = PreviousRegion->Offset + PreviousRegion->Size;
            if (PreviousEnd > RegionOffset) {
                RegionSize -= (PreviousEnd - RegionOffset);
                RegionOffset = PreviousEnd;
            }
        }

        if (NextRegion != NULL) {
            RegionEnd = RegionOffset + RegionSize;
            if (NextRegion->Offset < RegionEnd) {
                RegionSize -= (RegionEnd - NextRegion->Offset);
            }
        }

        ASSERT(RegionSize >= PageSize);

        Status = MmAllocatePageFileSpace(&(NewRegion->ImageBacking),
                                         RegionSize);

        if (KSUCCESS(Status)) {
            break;
        }

        //
        // Attempts to allocate a smaller portion of page file space are only
        // allowed if insufficient resources were reported.
        //

        if (Status != STATUS_INSUFFICIENT_RESOURCES) {
            goto CreateSharedMemoryObjectBackingRegionEnd;
        }

        ASSERT(IS_ALIGNED(RegionSize, PageSize) != FALSE);

        RetryCount += 1;
        RegionSize = MAX_SHARED_MEMORY_BACKING_REGION_SIZE >> RetryCount;
    }

    if (!KSUCCESS(Status)) {
        goto CreateSharedMemoryObjectBackingRegionEnd;
    }

    NewRegion->Offset = RegionOffset;
    NewRegion->Size = RegionSize;
    if (NextRegion != NULL) {
        INSERT_BEFORE(&(NewRegion->ListEntry), &(NextRegion->ListEntry));

    } else {
        INSERT_BEFORE(&(NewRegion->ListEntry),
                      &(SharedMemoryObject->BackingRegionList));
    }

    Status = STATUS_SUCCESS;

CreateSharedMemoryObjectBackingRegionEnd:
    if (!KSUCCESS(Status)) {
        if (NewRegion != NULL) {
            MmFreePagedPool(NewRegion);
            NewRegion = NULL;
        }
    }

    return NewRegion;
}

VOID
IopDestroySharedMemoryBackingRegion (
    PSHARED_MEMORY_BACKING_REGION Region
    )

/*++

Routine Description:

    This routine destroys a shared memory object backing region and its
    associated page file space.

Arguments:

    Region - Supplies a pointer to the shared memory object backing region to
        destroy.

Return Value:

    None.

--*/

{

    ASSERT(Region->ListEntry.Next == NULL);

    MmFreePageFileSpace(&(Region->ImageBacking), Region->Size);
    MmFreePagedPool(Region);
    return;
}

