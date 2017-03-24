/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mount.c

Abstract:

    This module implements support functionality for mounting and unmounting.

Author:

    Chris Stevens 30-Jul-2013

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
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
IopMount (
    BOOL FromKernelMode,
    PPATH_POINT Mount,
    PPATH_POINT Target,
    ULONG Flags
    );

KSTATUS
IopUnmount (
    PMOUNT_POINT MountPoint,
    ULONG Flags
    );

BOOL
IopIsMountPointBusy (
    PMOUNT_POINT MountPoint,
    PMOUNT_POINT OriginalMountPoint,
    ULONG Flags
    );

PMOUNT_POINT
IopCreateMountPoint (
    PPATH_POINT Mount,
    PPATH_POINT Target,
    PSTR TargetPath,
    UINTN TargetPathSize,
    ULONG Flags
    );

VOID
IopDestroyMountPoint (
    PMOUNT_POINT MountPoint
    );

KSTATUS
IopCreateAndCopyMountPoint (
    PPATH_POINT Mount,
    PPATH_POINT Target,
    PLIST_ENTRY MountList,
    PSTR TargetPath,
    UINTN TargetPathSize,
    ULONG Flags
    );

KSTATUS
IopCopyMountTree (
    PMOUNT_POINT NewRoot,
    PPATH_POINT Target,
    ULONG Flags
    );

VOID
IopDestroyMountTree (
    PMOUNT_POINT Root,
    PLIST_ENTRY DestroyList
    );

KSTATUS
IopLinkMountPoint (
    PMOUNT_POINT AutoMount,
    PPATH_POINT Target,
    PLIST_ENTRY MountList
    );

VOID
IopDestroyLinkedMountPoints (
    PMOUNT_POINT Mount,
    PLIST_ENTRY DestroyList
    );

KSTATUS
IopGetMountPointsFromTree (
    PPATH_POINT ProcessRoot,
    PMOUNT_POINT TreeRoot,
    PVOID *BufferOffset,
    PUINTN BytesRemaining,
    PUINTN RequiredSize
    );

//
// -------------------------------------------------------------------- Globals
//

PSHARED_EXCLUSIVE_LOCK IoMountLock;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopInitializeMountPointSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes the support for mount points.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PMOUNT_POINT MountPoint;
    KSTATUS Status;

    ASSERT(IoPathPointRoot.MountPoint == NULL);
    ASSERT(IoPathPointRoot.PathEntry != NULL);

    IoMountLock = KeCreateSharedExclusiveLock();
    if (IoMountLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeMountPointsSupportEnd;
    }

    //
    // Use the partially initialized root path point to create the root mount
    // point. The root path point's path entry is the "target" of this mount
    // point and there is no "mount", as it is the root of all mount points.
    //

    MountPoint = IopCreateMountPoint(NULL, &IoPathPointRoot, NULL, 0, 0);
    if (MountPoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeMountPointsSupportEnd;
    }

    IoPathPointRoot.MountPoint = MountPoint;
    Status = STATUS_SUCCESS;

InitializeMountPointsSupportEnd:
    if (!KSUCCESS(Status)) {
        if (IoMountLock != NULL) {
            KeDestroySharedExclusiveLock(IoMountLock);
        }
    }

    return Status;
}

KERNEL_API
KSTATUS
IoMount (
    BOOL FromKernelMode,
    PCSTR MountPointPath,
    ULONG MountPointPathSize,
    PCSTR TargetPath,
    ULONG TargetPathSize,
    ULONG MountFlags,
    ULONG AccessFlags
    )

/*++

Routine Description:

    This routine attempts to mount the given target on the given mount point.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    MountPointPath - Supplies a pointer to the string containing the path to
        where the target is to be mounted.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    TargetPath - Supplies a pointer to the string containing the path to the
        target file, directory, volume, pipe, socket, or device that is to be
        mounted.

    TargetPathSize - Supplies the size of the target path string in bytes,
        including the null terminator.

    MountFlags - Supplies the flags associated with the mount operation. See
        MOUNT_FLAG_* for definitions.

    AccessFlags - Supplies the flags associated with the mount point's access
        permissions. See IO_ACCESS_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PDEVICE Device;
    PFILE_OBJECT MountFileObject;
    IO_OBJECT_TYPE MountFileType;
    PATH_POINT MountPathPoint;
    KSTATUS Status;
    PFILE_OBJECT TargetFileObject;
    PATH_POINT TargetPathPoint;
    PVOLUME Volume;

    MountPathPoint.PathEntry = NULL;
    TargetPathPoint.PathEntry = NULL;
    Volume = NULL;

    //
    // Permission check for mounts.
    //

    if (FromKernelMode == FALSE) {
        Status = PsCheckPermission(PERMISSION_MOUNT);
        if (!KSUCCESS(Status)) {
            goto MountEnd;
        }
    }

    //
    // Open the mount point path point, but do not follow any mount points in
    // the final component.
    //

    Status = IopPathWalk(FromKernelMode,
                         NULL,
                         &MountPointPath,
                         &MountPointPathSize,
                         OPEN_FLAG_NO_MOUNT_POINT,
                         NULL,
                         &MountPathPoint);

    //
    // If the entry does not exist, fail.
    //

    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // Open the target path point and if it exists, try to mount the target at
    // the mount point.
    //

    Status = IopPathWalk(FromKernelMode,
                         NULL,
                         &TargetPath,
                         &TargetPathSize,
                         0,
                         NULL,
                         &TargetPathPoint);

    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // Get the mount point file object for validation below.
    //

    MountFileObject = MountPathPoint.PathEntry->FileObject;
    MountFileType = MountFileObject->Properties.Type;

    //
    // Get the target's file object and determine what type of object it is.
    // Act according to the type.
    //

    TargetFileObject = TargetPathPoint.PathEntry->FileObject;
    switch (TargetFileObject->Properties.Type) {
    case IoObjectBlockDevice:
        Device = TargetFileObject->Device;

        ASSERT(IS_DEVICE_OR_VOLUME(Device));

        //
        // Bind calls are only allowed on block devices if the mount point is
        // not a directory.
        //

        if ((MountFlags & MOUNT_FLAG_BIND) != 0) {
            if ((MountFileType == IoObjectRegularDirectory) ||
                (MountFileType == IoObjectObjectDirectory)) {

                Status = STATUS_FILE_IS_DIRECTORY;
                goto MountEnd;
            }

            Status = IopMount(FromKernelMode,
                              &MountPathPoint,
                              &TargetPathPoint,
                              MountFlags);

            goto MountEnd;
        }

        //
        // The target file object must be a directory.
        //

        if ((MountFileType != IoObjectRegularDirectory) &&
            ((FromKernelMode == FALSE) ||
             (MountFileType != IoObjectObjectDirectory))) {

            Status = STATUS_NOT_A_DIRECTORY;
            goto MountEnd;
        }

        //
        // If the device is not mountable, then quit.
        //

        if ((Device->Flags & DEVICE_FLAG_MOUNTABLE) == 0) {
            Status = STATUS_NOT_MOUNTABLE;
            goto MountEnd;
        }

        //
        // Get the volume for this device.
        //

        Status = IopCreateOrLookupVolume(Device, &Volume);
        if (!KSUCCESS(Status)) {
            goto MountEnd;
        }

        ASSERT((MountFlags & MOUNT_FLAG_RECURSIVE) == 0);
        ASSERT(Volume->PathEntry != NULL);
        ASSERT(TargetPathPoint.PathEntry != NULL);

        //
        // The volume stores the anonymous path entry of its root directory.
        // Use that as a mount target. The volume holds a reference on the path
        // entry, so there is no need to acquire an additional reference.
        //

        IO_PATH_POINT_RELEASE_REFERENCE(&TargetPathPoint);
        TargetPathPoint.PathEntry = Volume->PathEntry;
        TargetPathPoint.MountPoint = NULL;

        //
        // Attempt to mount the target volume onto the mount point.
        //

        Status = IopMount(FromKernelMode,
                          &MountPathPoint,
                          &TargetPathPoint,
                          MountFlags);

        TargetPathPoint.PathEntry = NULL;
        break;

    case IoObjectPipe:
    case IoObjectSocket:
    case IoObjectCharacterDevice:
    case IoObjectTerminalMaster:
    case IoObjectTerminalSlave:

        //
        // Only allow bind calls to proceed for these type of target files as
        // the allowed mounts are all considered busy.
        //

        if ((MountFlags & MOUNT_FLAG_BIND) == 0) {
            Status = STATUS_NOT_BLOCK_DEVICE;
            goto MountEnd;
        }

        //
        // These types of objects are not allowed to be mounted on directories.
        //

        if ((MountFileType == IoObjectRegularDirectory) ||
            (MountFileType == IoObjectObjectDirectory)) {

            Status = STATUS_FILE_IS_DIRECTORY;
            goto MountEnd;
        }

        //
        // Attempt to mount the target to the mount location.
        //

        Status = IopMount(FromKernelMode,
                          &MountPathPoint,
                          &TargetPathPoint,
                          MountFlags);

        break;

    case IoObjectObjectDirectory:
    case IoObjectRegularDirectory:
    case IoObjectRegularFile:
        Device = TargetFileObject->Device;

        //
        // Only allow bind calls to proceed for these type of target files as
        // the allowed mounts are all considered busy.
        //

        if ((MountFlags & MOUNT_FLAG_BIND) == 0) {
            Status = STATUS_RESOURCE_IN_USE;
            goto MountEnd;
        }

        //
        // The target and mount point file types must be compatible. Do not
        // allow mount points on top of object directories.
        //

        if ((TargetFileObject->Properties.Type == IoObjectRegularDirectory) ||
            (TargetFileObject->Properties.Type == IoObjectObjectDirectory)) {

            if (MountFileType != IoObjectRegularDirectory) {
                Status = STATUS_NOT_A_DIRECTORY;
                goto MountEnd;
            }

        } else {

            ASSERT(TargetFileObject->Properties.Type == IoObjectRegularFile);

            if ((MountFileType == IoObjectRegularDirectory) ||
                (MountFileType == IoObjectObjectDirectory)) {

                Status = STATUS_FILE_IS_DIRECTORY;
                goto MountEnd;
            }
        }

        //
        // Attempt to mount the target to the mount location.
        //

        Status = IopMount(FromKernelMode,
                          &MountPathPoint,
                          &TargetPathPoint,
                          MountFlags);

        break;

    //
    // Symbolic links fall through to the default cause because they should
    // never be the final result of a path walk that doesn't have the symbolic
    // link flag set.
    //

    case IoObjectSymbolicLink:
    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

MountEnd:
    if (Volume != NULL) {
        IoVolumeReleaseReference(Volume);
    }

    if (MountPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&MountPathPoint);
    }

    if (TargetPathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&TargetPathPoint);
    }

    return Status;
}

KERNEL_API
KSTATUS
IoUnmount (
    BOOL FromKernelMode,
    PCSTR MountPointPath,
    ULONG MountPointPathSize,
    ULONG MountFlags,
    ULONG AccessFlags
    )

/*++

Routine Description:

    This routine attempts to remove a mount point at the given path.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    MountPointPath - Supplies a pointer to the string containing the path to
        where the unmount should take place.

    MountPointPathSize - Supplies the size of the mount point path string in
        bytes, including the null terminator.

    MountFlags - Supplies the flags associated with the mount operation. See
        MOUNT_FLAG_* for definitions.

    AccessFlags - Supplies the flags associated with the mount point's access
        permissions. See IO_ACCESS_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PATH_POINT PathPoint;
    KSTATUS Status;

    PathPoint.PathEntry = NULL;

    //
    // Permission check for unmounting.
    //

    if (FromKernelMode == FALSE) {
        Status = PsCheckPermission(PERMISSION_MOUNT);
        if (!KSUCCESS(Status)) {
            goto UnmountEnd;
        }
    }

    //
    // Open the mount point's path point.
    //

    Status = IopPathWalk(FromKernelMode,
                         NULL,
                         &MountPointPath,
                         &MountPointPathSize,
                         0,
                         NULL,
                         &PathPoint);

    //
    // If the entry does not exist, fail.
    //

    if (!KSUCCESS(Status)) {
        goto UnmountEnd;
    }

    //
    // If this target is not a mount point, fail.
    //

    if (IO_IS_MOUNT_POINT(&PathPoint) == FALSE) {
        Status = STATUS_NOT_A_MOUNT_POINT;
        goto UnmountEnd;
    }

    //
    // Go ahead and unmount the mount point.
    //

    Status = IopUnmount(PathPoint.MountPoint, MountFlags);
    if (!KSUCCESS(Status)) {
        goto UnmountEnd;
    }

UnmountEnd:
    if (PathPoint.PathEntry != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(&PathPoint);
    }

    return Status;
}

KSTATUS
IopGetSetMountPointInformation (
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets mount point information.

Arguments:

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    if (Set != FALSE) {
        *DataSize = 0;
        return STATUS_NOT_SUPPORTED;
    }

    return IoGetMountPoints(Data, DataSize);
}

KERNEL_API
KSTATUS
IoGetMountPoints (
    PVOID Buffer,
    PUINTN BufferSize
    )

/*++

Routine Description:

    This routine returns the list of mount points for the current process,
    filling the supplied buffer with the data.

Arguments:

    Buffer - Supplies a pointer to a buffer that receives the mount point data.

    BufferSize - Supplies a pointer to the size of the buffer. Upon return this
        either holds the number of bytes actually used or if the buffer is
        to small, it receives the expected buffer size.

Return Value:

    Status code.

--*/

{

    UINTN BytesRemaining;
    BOOL CheckChildren;
    PLIST_ENTRY CurrentEntry;
    PVOID CurrentOffset;
    BOOL DescendantPath;
    PMOUNT_POINT MountPoint;
    PKPROCESS Process;
    UINTN RequiredSize;
    PPATH_POINT Root;
    PATH_POINT RootCopy;
    PMOUNT_POINT RootMount;
    KSTATUS Status;
    UINTN TreeRequiredSize;

    BytesRemaining = *BufferSize;
    CurrentOffset = Buffer;
    RequiredSize = 0;
    Root = NULL;

    //
    // Get the caller's root.
    //

    Process = PsGetCurrentProcess();
    KeAcquireQueuedLock(Process->Paths.Lock);
    if (Process->Paths.Root.PathEntry != NULL) {
        IO_COPY_PATH_POINT(&RootCopy, &(Process->Paths.Root));
        IO_PATH_POINT_ADD_REFERENCE(&RootCopy);
        Root = &RootCopy;
    }

    KeReleaseQueuedLock(Process->Paths.Lock);

    //
    // If the process does not have a root return all mounts points under the
    // root.
    //

    CheckChildren = FALSE;
    if (Root == NULL) {
        RootMount = IoPathPointRoot.MountPoint;

    //
    // Otherwise be careful to only return the mount points visible to the
    // caller. Keep in mind that the process's root path point might not be the
    // root of a mount point.
    //

    } else {
        RootMount = Root->MountPoint;
        if (IO_IS_MOUNT_POINT(Root) == FALSE) {
            CheckChildren = TRUE;
        }
    }

    KeAcquireSharedExclusiveLockShared(IoMountLock);

    //
    // If the process does not have a root, skip the root mount and process
    // only its children; it isn't a real mount point. If the process root is
    // not a mount point, then only the correct descendant mount points should
    // be processed.
    //

    if ((Root == NULL) || (CheckChildren != FALSE)) {
        CurrentEntry = RootMount->ChildListHead.Previous;
        while (CurrentEntry != &(RootMount->ChildListHead)) {
            MountPoint = LIST_VALUE(CurrentEntry,
                                    MOUNT_POINT,
                                    SiblingListEntry);

            CurrentEntry = CurrentEntry->Previous;
            if (CheckChildren != FALSE) {
                DescendantPath = IopIsDescendantPath(Root->PathEntry,
                                                     MountPoint->MountEntry);

                if (DescendantPath == FALSE) {
                    continue;
                }
            }

            Status = IopGetMountPointsFromTree(Root,
                                               MountPoint,
                                               &CurrentOffset,
                                               &BytesRemaining,
                                               &TreeRequiredSize);

            RequiredSize += TreeRequiredSize;
            if (!KSUCCESS(Status)) {
                goto GetMountPointsEnd;
            }
        }

    //
    // Otherwise the process root is a mount point. Just run through the whole
    // tree.
    //

    } else {
        Status = IopGetMountPointsFromTree(Root,
                                           RootMount,
                                           &CurrentOffset,
                                           &BytesRemaining,
                                           &RequiredSize);

        if (!KSUCCESS(Status)) {
            goto GetMountPointsEnd;
        }
    }

    //
    // If the required size ended up being bigger than the buffer size. Fail.
    //

    if (RequiredSize > *BufferSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto GetMountPointsEnd;
    }

    Status = STATUS_SUCCESS;

GetMountPointsEnd:
    KeReleaseSharedExclusiveLockShared(IoMountLock);
    if (Root != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(Root);
    }

    //
    // Always return the required size to the caller. This is either the amount
    // of data written to the buffer, or the size the buffer needs to be.
    //

    *BufferSize = RequiredSize;

    //
    // Handle failure cases, zeroing out the buffer to prevent handing partial
    // data back to user-mode.
    //

    if (!KSUCCESS(Status)) {
        RtlZeroMemory(Buffer, *BufferSize);
    }

    return Status;
}

VOID
IopRemoveMountPoints (
    PPATH_POINT RootPath
    )

/*++

Routine Description:

    This routine lazily unmounts all the mount points that exist under the
    given root path point, including itself.

Arguments:

    RootPath - Supplies a pointer to the root path point, under which all
        mount points will be removed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMOUNT_POINT CurrentMount;
    BOOL DescendantPath;
    LIST_ENTRY DestroyList;

    //
    // If the root is a mount point, then it is as simple as calling unmount.
    //

    if (IO_IS_MOUNT_POINT(RootPath) != FALSE) {
        IopUnmount(RootPath->MountPoint,
                   MOUNT_FLAG_DETACH | MOUNT_FLAG_RECURSIVE);

    //
    // Otherwise unmount each mount point that is a descendant of the root.
    //

    } else {
        INITIALIZE_LIST_HEAD(&DestroyList);
        KeAcquireSharedExclusiveLockExclusive(IoMountLock);
        CurrentEntry = RootPath->MountPoint->ChildListHead.Previous;
        while (CurrentEntry != &(RootPath->MountPoint->ChildListHead)) {
            CurrentMount = LIST_VALUE(CurrentEntry,
                                      MOUNT_POINT,
                                      SiblingListEntry);

            CurrentEntry = CurrentEntry->Previous;
            DescendantPath = IopIsDescendantPath(RootPath->PathEntry,
                                                 CurrentMount->MountEntry);

            if (DescendantPath == FALSE) {
                continue;
            }

            IopDestroyMountTree(CurrentMount, &DestroyList);
        }

        KeReleaseSharedExclusiveLockExclusive(IoMountLock);

        //
        // Go through and destroy each mount point by releasing the original
        // reference and decrementing the mount count on the path entry.
        //

        CurrentEntry = DestroyList.Next;
        while (CurrentEntry != &DestroyList) {
            CurrentMount = LIST_VALUE(CurrentEntry,
                                      MOUNT_POINT,
                                      SiblingListEntry);

            CurrentEntry = CurrentEntry->Next;
            CurrentMount->SiblingListEntry.Next = NULL;
            IopPathEntryDecrementMountCount(CurrentMount->MountEntry);
            IoMountPointReleaseReference(CurrentMount);
        }
    }

    return;
}

PMOUNT_POINT
IopFindMountPoint (
    PMOUNT_POINT Parent,
    PPATH_ENTRY PathEntry
    )

/*++

Routine Description:

    This routine searches for a child mount point of the given parent whose
    mount path entry matches the given path entry. If found, a reference is
    taken on the returned mount point.

Arguments:

    Parent - Supplies a pointer to a parent mount point whose children are
        searched for a mount point that matches the path entry.

    PathEntry - Supplies a pointer to a path entry to search for.

Return Value:

    Returns a pointer to the found mount point on success, or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMOUNT_POINT FoundMountPoint;
    PMOUNT_POINT MountPoint;

    //
    // Do nothing if the path entry is not mounted anywhere or if the mount
    // point has no children to search.
    //

    if ((PathEntry->MountCount == 0) ||
        (LIST_EMPTY(&(Parent->ChildListHead)) != FALSE)) {

        return NULL;
    }

    //
    // Search over the list of child mount points looking for one whose mount
    // path entry matches the given path entry. Search from beginning to end
    // to find the most recent mount point using the given path entry.
    //

    FoundMountPoint = NULL;
    KeAcquireSharedExclusiveLockShared(IoMountLock);
    CurrentEntry = Parent->ChildListHead.Next;
    while (CurrentEntry != &(Parent->ChildListHead)) {
        MountPoint = LIST_VALUE(CurrentEntry, MOUNT_POINT, SiblingListEntry);
        if (MountPoint->MountEntry == PathEntry) {
            FoundMountPoint = MountPoint;
            IoMountPointAddReference(FoundMountPoint);
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSharedExclusiveLockShared(IoMountLock);
    return FoundMountPoint;
}

PMOUNT_POINT
IopGetMountPointParent (
    PMOUNT_POINT MountPoint
    )

/*++

Routine Description:

    This routine gets a mount point's parent. The parent can disappear at any
    moment with a lazy unmount, so this routine acquires the mount lock in
    shared mode to check the parent.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    Returns a pointer to the parent mount point on success, or NULL otherwise.

--*/

{

    PMOUNT_POINT Parent;

    if (MountPoint->Parent == NULL) {
        return NULL;
    }

    KeAcquireSharedExclusiveLockShared(IoMountLock);
    Parent = MountPoint->Parent;
    if (Parent != NULL) {
        IoMountPointAddReference(MountPoint->Parent);
    }

    KeReleaseSharedExclusiveLockShared(IoMountLock);
    return Parent;
}

VOID
IoMountPointAddReference (
    PMOUNT_POINT MountPoint
    )

/*++

Routine Description:

    This routine increments the reference count for the given mount point.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    if (MountPoint != NULL) {
        OldReferenceCount = RtlAtomicAdd32(&(MountPoint->ReferenceCount), 1);

        ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));
    }

    return;
}

VOID
IoMountPointReleaseReference (
    PMOUNT_POINT MountPoint
    )

/*++

Routine Description:

    This routine decrements the reference count for the given mount point.

Arguments:

    MountPoint - Supplies a pointer to a mount point.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    if (MountPoint != NULL) {
        OldReferenceCount = RtlAtomicAdd32(&(MountPoint->ReferenceCount),
                                           (ULONG)-1);

        ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

        if (OldReferenceCount == 1) {
            IopDestroyMountPoint(MountPoint);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
IopMount (
    BOOL FromKernelMode,
    PPATH_POINT Mount,
    PPATH_POINT Target,
    ULONG Flags
    )

/*++

Routine Description:

    This routine mounts the target path point on the mount point entry.

Arguments:

    FromKernelMode - Supplies a boolean indicating the request is coming from
        kernel mode.

    Mount - Supplies a pointer to the path point that is to be mounted on.

    Target - Supplies a pointer to the path point that is to be mounted at the
        mount point.

    Flags - Supplies a bitmaks of mount flags. See MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    LIST_ENTRY DestroyList;
    PFILE_OBJECT FileObject;
    BOOL FirstMount;
    BOOL LockHeld;
    BOOL MountCountIncremented;
    LIST_ENTRY MountList;
    PMOUNT_POINT MountPoint;
    PKPROCESS Process;
    PPATH_POINT Root;
    PATH_POINT RootCopy;
    KSTATUS Status;
    PSTR TargetPath;
    UINTN TargetPathSize;

    LockHeld = FALSE;
    MountCountIncremented = FALSE;
    INITIALIZE_LIST_HEAD(&MountList);

    //
    // The mount supplied should not be the root of a mount point. Otherwise
    // the new mount point would be the child of the wrong mount point.
    //

    ASSERT(IO_IS_MOUNT_POINT(Mount) == FALSE);

    //
    // Get the caller's root.
    //

    Root = NULL;
    if (FromKernelMode == FALSE) {
        Process = PsGetCurrentProcess();
        KeAcquireQueuedLock(Process->Paths.Lock);
        if (Process->Paths.Root.PathEntry != NULL) {
            IO_COPY_PATH_POINT(&RootCopy, &(Process->Paths.Root));
            IO_PATH_POINT_ADD_REFERENCE(&RootCopy);
            Root = &RootCopy;
        }

        KeReleaseQueuedLock(Process->Paths.Lock);
    }

    MountPoint = NULL;
    Status = IopGetPathFromRoot(Target,
                                Root,
                                &TargetPath,
                                &TargetPathSize);

    if (Root != NULL) {
        IO_PATH_POINT_RELEASE_REFERENCE(Root);
    }

    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // Increment the mount count for the path entry on top of which the mount
    // will be placed. Due to lock ordering with the mount lock, this is done
    // first to potentially increment the path entry's mount count from 0 to 1.
    // This allows the routine to freely increment the path entry's mount count
    // while the mount lock is held without also acquiring the file object
    // lock, which would be an order inversion. A path entry cannot be deleted
    // or renamed while it has a non-zero mount count, but it's up to the
    // caller to synchronize those actions with mount.
    //

    FileObject = Mount->PathEntry->FileObject;
    KeAcquireSharedExclusiveLockShared(FileObject->Lock);
    IopPathEntryIncrementMountCount(Mount->PathEntry);
    MountCountIncremented = TRUE;
    KeReleaseSharedExclusiveLockShared(FileObject->Lock);

    //
    // Acquire the mount lock exclusively throughout the whole mount process.
    //

    KeAcquireSharedExclusiveLockExclusive(IoMountLock);
    LockHeld = TRUE;

    //
    // Allocate the new mount point and copy the child mount points of the
    // target path point as necessary.
    //

    Status = IopCreateAndCopyMountPoint(Mount,
                                        Target,
                                        &MountList,
                                        TargetPath,
                                        TargetPathSize,
                                        Flags);

    if (!KSUCCESS(Status)) {
        goto MountEnd;
    }

    //
    // If this is a linked mount point, it should be propogated to all other
    // locations in the namespace where the mount path entry may be found. The
    // mount point created above should be first on the list.
    //

    if ((Flags & MOUNT_FLAG_LINKED) != 0) {

        ASSERT(LIST_EMPTY(&MountList) == FALSE);

        MountPoint = LIST_VALUE(MountList.Next, MOUNT_POINT, SiblingListEntry);
        Status = IopLinkMountPoint(MountPoint, Target, &MountList);
        if (!KSUCCESS(Status)) {
            goto MountEnd;
        }
    }

    //
    // All newly created mount points are on the local mount list. Run through
    // the list and add them to their parents' lists of children. The first one
    // on the list is the initial mount point requested by the caller and any
    // additional mount points are a result of propogating a linked mount
    // request to other portions of the mount tree. As a result, the first
    // mount gets placed first on its parent's list of children and subsequent
    // mounts get placed last of their parents' lists of children.
    //

    FirstMount = TRUE;
    while (LIST_EMPTY(&MountList) == FALSE) {
        MountPoint = LIST_VALUE(MountList.Next, MOUNT_POINT, SiblingListEntry);
        LIST_REMOVE(&(MountPoint->SiblingListEntry));

        ASSERT(MountPoint->Parent != NULL);

        if (FirstMount != FALSE) {
            INSERT_AFTER(&(MountPoint->SiblingListEntry),
                         &(MountPoint->Parent->ChildListHead));

            FirstMount = FALSE;

        } else {
            INSERT_BEFORE(&(MountPoint->SiblingListEntry),
                          &(MountPoint->Parent->ChildListHead));
        }
    }

MountEnd:
    if (LockHeld != FALSE) {
        KeReleaseSharedExclusiveLockExclusive(IoMountLock);
    }

    //
    // If the mount attempt was not successful, all of the created mount points
    // need to be destroyed. None of them should be live in the tree of mount
    // points. Run through the mount list and destroy each entry and its
    // descendants.
    //

    if (!KSUCCESS(Status)) {
        INITIALIZE_LIST_HEAD(&DestroyList);
        while (LIST_EMPTY(&MountList) == FALSE) {
            MountPoint = LIST_VALUE(MountList.Next,
                                    MOUNT_POINT,
                                    SiblingListEntry);

            IopDestroyMountTree(MountPoint, &DestroyList);
        }

        while (LIST_EMPTY(&DestroyList) == FALSE) {
            MountPoint = LIST_VALUE(DestroyList.Next,
                                    MOUNT_POINT,
                                    SiblingListEntry);

            LIST_REMOVE(&(MountPoint->SiblingListEntry));
            MountPoint->SiblingListEntry.Next = NULL;
            IopPathEntryDecrementMountCount(MountPoint->MountEntry);
            IoMountPointReleaseReference(MountPoint);
        }
    }

    //
    // Always decrement the mount count if it was incremented before. The count
    // was additionally incremented when any mounts were created.
    //

    if (MountCountIncremented != FALSE) {
        IopPathEntryDecrementMountCount(Mount->PathEntry);
    }

    if (TargetPath != NULL) {
        MmFreePagedPool(TargetPath);
    }

    return Status;
}

KSTATUS
IopUnmount (
    PMOUNT_POINT MountPoint,
    ULONG Flags
    )

/*++

Routine Description:

    This routine unmounts the given mount point.

Arguments:

    MountPoint - Supplies a pointer to the mount point that is to be unmounted.

    Flags - Supplies a bitmask of unmount flags. See MOUNT_FLAG_* for
        definitions.

Return Value:

    Status code.

--*/

{

    ULONG BusyFlags;
    PLIST_ENTRY CurrentEntry;
    PMOUNT_POINT CurrentMount;
    LIST_ENTRY DestroyList;
    KSTATUS Status;

    INITIALIZE_LIST_HEAD(&DestroyList);

    //
    // Synchronize the whole unmount operation with mounts and other unmounts.
    //

    KeAcquireSharedExclusiveLockExclusive(IoMountLock);

    //
    // A different lazy (detach) unmount may have beat this to the punch.
    //

    if (MountPoint->Parent == NULL) {

        ASSERT(LIST_EMPTY(&(MountPoint->ChildListHead)) != FALSE);

        Status = STATUS_NOT_A_MOUNT_POINT;
        goto UnmountEnd;
    }

    //
    // If the call is not lazy, then make sure there are no references on the
    // mount point, its children, or any linked mount points before it is
    // removed.
    //

    if ((Flags & MOUNT_FLAG_DETACH) == 0) {
        BusyFlags = Flags | (MountPoint->Flags & MOUNT_FLAG_LINKED);
        if (IopIsMountPointBusy(MountPoint, MountPoint, BusyFlags) != FALSE) {
            Status = STATUS_RESOURCE_IN_USE;
            goto UnmountEnd;
        }
    }

    //
    // Destroy the the mount tree. If this is a linked mount point, then also
    // destroy the other instances of the mount.
    //

    IopDestroyMountTree(MountPoint, &DestroyList);
    if ((MountPoint->Flags & MOUNT_FLAG_LINKED) != 0) {
        IopDestroyLinkedMountPoints(MountPoint, &DestroyList);
    }

    Status = STATUS_SUCCESS;

UnmountEnd:
    KeReleaseSharedExclusiveLockExclusive(IoMountLock);

    //
    // Destroy any mount points that were plucked off the tree.
    //

    CurrentEntry = DestroyList.Next;
    while (CurrentEntry != &DestroyList) {
        CurrentMount = LIST_VALUE(CurrentEntry, MOUNT_POINT, SiblingListEntry);
        CurrentEntry = CurrentEntry->Next;
        CurrentMount->SiblingListEntry.Next = NULL;
        IopPathEntryDecrementMountCount(CurrentMount->MountEntry);
        IoMountPointReleaseReference(CurrentMount);
    }

    return Status;
}

BOOL
IopIsMountPointBusy (
    PMOUNT_POINT MountPoint,
    PMOUNT_POINT OriginalMountPoint,
    ULONG Flags
    )

/*++

Routine Description:

    This routine determines whether or not the given mount point is busy. It
    takes all children and linked mount points into consideration depending on
    the supplied set of mount flags.

Arguments:

    MountPoint - Supplies a pointer to the mount point to be checked.

    OriginalMountPoint - Supplies a pointer to the mount point that was
        originally being checked for busy state (before this routine recurses).

    Flags - Supplies a bitmask of flags used to determine which child or linked
        mount points should also be checked for busy status. Seet MOUNT_FLAG_*
        for definitions.

Return Value:

    Returns TRUE if the mount point is busy or FALSE otherwise.

--*/

{

    BOOL Busy;
    BOOL CheckChildren;
    ULONG CheckFlags;
    PMOUNT_POINT CurrentMount;
    PMOUNT_POINT TreeRoot;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoMountLock) != FALSE);

    Busy = FALSE;

    //
    // If the current mount point is the original mount point, then it is busy
    // if it has more than two references (the original reference and the
    // reference taken by the caller).
    //

    if (MountPoint == OriginalMountPoint) {
        if (MountPoint->ReferenceCount > 2) {
            Busy = TRUE;
            goto IsMountPointBusyEnd;
        }

    //
    // Other mount points are considered busy if they have more than the
    // original base reference.
    //

    } else {
        if (MountPoint->ReferenceCount > 1) {
            Busy = TRUE;
            goto IsMountPointBusyEnd;
        }
    }

    //
    // Handle any child mount points. They do not take a reference on their
    // parent, so their existence alone makes the parent busy.
    //

    if (LIST_EMPTY(&(MountPoint->ChildListHead)) == FALSE) {

        //
        // If this is not a recursive unmount then it is too busy to unmount
        // only if there are any non-linked descendants or a linked descendant
        // with a reference.
        //

        CheckFlags = 0;
        if ((Flags & MOUNT_FLAG_RECURSIVE) == 0) {
            CheckFlags = MOUNT_FLAG_LINKED;
        }

        //
        // Check the children to make sure they all have one reference. A
        // recursive call can only succeed if all mount points can be removed.
        // Non-recursive calls can only succeed if there are no non-linked
        // children and if all the linked children only have one reference.
        //

        CurrentMount = LIST_VALUE(MountPoint->ChildListHead.Next,
                                  MOUNT_POINT,
                                  SiblingListEntry);

        while (CurrentMount != MountPoint) {
            if (CurrentMount != OriginalMountPoint) {
                if ((CurrentMount->Flags & CheckFlags) != CheckFlags) {
                    Busy = TRUE;
                    goto IsMountPointBusyEnd;
                }

                if (CurrentMount->ReferenceCount > 1) {
                    Busy = TRUE;
                    goto IsMountPointBusyEnd;
                }

                //
                // Iterate to the current mount point's first child if it
                // exists.
                //

                if (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE) {
                    CurrentMount = LIST_VALUE(CurrentMount->ChildListHead.Next,
                                              MOUNT_POINT,
                                              SiblingListEntry);

                    continue;
                }
            }

            //
            // Move to a sibling or ancestor's sibling.
            //

            while (CurrentMount != MountPoint) {
                if (CurrentMount->SiblingListEntry.Next !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                           CurrentMount->SiblingListEntry.Next,
                                           MOUNT_POINT,
                                           SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
            }
        }
    }

    //
    // If the mount point is linked, then it may be busy if its other instances
    // have references or descendants with references.
    //

    if ((Flags & MOUNT_FLAG_LINKED) != 0) {

        //
        // Remove the linked flag so that this routine does not recurse more
        // than one level.
        //

        Flags &= ~MOUNT_FLAG_LINKED;

        //
        // Iterate over the tree of mount points starting at the root.
        //

        TreeRoot = IoPathPointRoot.MountPoint;
        CurrentMount = TreeRoot;
        do {
            CheckChildren = TRUE;
            if ((CurrentMount != MountPoint) &&
                ((CurrentMount->Flags & MOUNT_FLAG_LINKED) != 0) &&
                (CurrentMount->TargetEntry == MountPoint->TargetEntry)) {

                Busy = IopIsMountPointBusy(CurrentMount,
                                           OriginalMountPoint,
                                           Flags);

                if (Busy != FALSE) {
                    goto IsMountPointBusyEnd;
                }

                CheckChildren = FALSE;
            }

            if ((CheckChildren != FALSE) &&
                (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE)) {

                CurrentMount = LIST_VALUE(CurrentMount->ChildListHead.Previous,
                                          MOUNT_POINT,
                                          SiblingListEntry);

                continue;
            }

            while (CurrentMount != TreeRoot) {
                if (CurrentMount->SiblingListEntry.Previous !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                       CurrentMount->SiblingListEntry.Previous,
                                       MOUNT_POINT,
                                       SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
            }

        } while (CurrentMount != TreeRoot);
    }

IsMountPointBusyEnd:
    return Busy;
}

PMOUNT_POINT
IopCreateMountPoint (
    PPATH_POINT Mount,
    PPATH_POINT Target,
    PSTR TargetPath,
    UINTN TargetPathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine creates a mount point entry.

Arguments:

    Mount - Supplies an optional pointer to the path point that is to be
        mounted on.

    Target - Supplies a pointer to the path point that is to be mounted at the
        mount point.

    TargetPath - Supplies a string containing the path to target.

    TargetPathSize - Supplies the size of the target string, in bytes.

    Flags - Supplies a bitmask of flags for the mount point. See MOUNT_FLAG_*
        for definitions.

Return Value:

    Returns a pointer to a new mount source on success, or NULL on failure.

--*/

{

    UINTN AllocationSize;
    PMOUNT_POINT MountPoint;
    POBJECT_HEADER TargetRootObject;

    AllocationSize = sizeof(MOUNT_POINT) + TargetPathSize;
    MountPoint = MmAllocatePagedPool(AllocationSize, IO_ALLOCATION_TAG);
    if (MountPoint == NULL) {
        return NULL;
    }

    //
    // With potential failures out of the way, initialize the mount point entry.
    // Note that a mount point does not take a reference on its parent. It's
    // the parent's duty to detect if any child mount points are present before
    // being unmounted (unless it is a lazy unmount).
    //

    RtlZeroMemory(MountPoint, sizeof(MOUNT_POINT));
    INITIALIZE_LIST_HEAD(&(MountPoint->ChildListHead));
    if (Mount != NULL) {
        MountPoint->Parent = Mount->MountPoint;
        MountPoint->MountEntry = Mount->PathEntry;
        IoPathEntryAddReference(MountPoint->MountEntry);

        //
        // This should not be the first mount count added to the path entry.
        //

        ASSERT(MountPoint->MountEntry->MountCount != 0);

        IopPathEntryIncrementMountCount(MountPoint->MountEntry);
    }

    //
    // If the target's object root is a volume, add a reference to the volume.
    //

    TargetRootObject = (POBJECT_HEADER)(Target->PathEntry->FileObject->Device);
    if (TargetRootObject->Type == ObjectVolume) {
        IoVolumeAddReference((PVOLUME)TargetRootObject);
    }

    MountPoint->TargetEntry = Target->PathEntry;
    IoPathEntryAddReference(MountPoint->TargetEntry);
    MountPoint->Flags = Flags;
    MountPoint->ReferenceCount = 1;
    if (TargetPathSize != 0) {
        MountPoint->TargetPath = (PSTR)(MountPoint + 1);
        RtlStringCopy(MountPoint->TargetPath, TargetPath, TargetPathSize);
    }

    return MountPoint;
}

VOID
IopDestroyMountPoint (
    PMOUNT_POINT MountPoint
    )

/*++

Routine Description:

    This routine destroys a mount point, releasing any references it took.

Arguments:

    MountPoint - Supplies a pointer to the mount point that is to be destroyed.

Return Value:

    None.

--*/

{

    POBJECT_HEADER TargetRootObject;

    ASSERT(MountPoint->SiblingListEntry.Next == NULL);
    ASSERT(LIST_EMPTY(&(MountPoint->ChildListHead)) != FALSE);
    ASSERT(MountPoint->MountEntry != NULL);
    ASSERT(MountPoint->TargetEntry != NULL);
    ASSERT(MountPoint->ReferenceCount == 0);

    //
    // In case this is a mount point whose target's root object is a volume,
    // save the target path's root object for dereferencing.
    //

    TargetRootObject =
                 (POBJECT_HEADER)(MountPoint->TargetEntry->FileObject->Device);

    IoPathEntryReleaseReference(MountPoint->MountEntry);
    IoPathEntryReleaseReference(MountPoint->TargetEntry);
    MmFreePagedPool(MountPoint);

    //
    // Decrement the volume reference count.
    //

    if (TargetRootObject->Type == ObjectVolume) {
        IoVolumeReleaseReference((PVOLUME)TargetRootObject);
    }

    return;
}

KSTATUS
IopCreateAndCopyMountPoint (
    PPATH_POINT Mount,
    PPATH_POINT Target,
    PLIST_ENTRY MountList,
    PSTR TargetPath,
    UINTN TargetPathSize,
    ULONG Flags
    )

/*++

Routine Description:

    This routine creates a mount point and copies the given targets children
    mount points to the new mount point, if necessary.

Arguments:

    Mount - Supplies an optional pointer to the path point that is to be
        mounted on.

    Target - Supplies a pointer to the path point that is to be mounted at the
        mount point.

    MountList - Supplies a pointer to the head of the list to which any newly
        created mount points will be added.

    TargetPath - Supplies a string containing the path to target.

    TargetPathSize - Supplies the size of the target string, in bytes.

    Flags - Supplies a bitmask of flags for the mount point. See MOUNT_FLAG_*
        for definitions.

Return Value:

    Returns a pointer to a new mount source on success, or NULL on failure.

--*/

{

    ULONG CopyFlags;
    PMOUNT_POINT CurrentMount;
    PMOUNT_POINT FoundMount;
    PMOUNT_POINT MountPoint;
    KSTATUS Status;
    PMOUNT_POINT TreeRoot;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoMountLock) != FALSE);

    FoundMount = NULL;

    //
    // Allocate any resources that might be needed for the new mount point.
    //

    MountPoint = IopCreateMountPoint(Mount,
                                     Target,
                                     TargetPath,
                                     TargetPathSize,
                                     Flags);

    if (MountPoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndInsertMountPointEnd;
    }

    //
    // If this is a recursive bind call, find all the mount points under the
    // target mount and create the appropriate mount points under the new
    // mount. Otherwise, just copy all the automatic mount points under the
    // target mount.
    //

    if (((Flags & MOUNT_FLAG_BIND) != 0) &&
        ((Flags & MOUNT_FLAG_RECURSIVE) != 0)) {

        CopyFlags = 0;

    } else {
        CopyFlags = MOUNT_FLAG_LINKED;
    }

    //
    // Be careful as the target may not have an associated mount point. If it
    // does not, attempt to find another place it is mounted and copy that tree.
    //

    if (Target->MountPoint == NULL) {

        ASSERT(CopyFlags == MOUNT_FLAG_LINKED);

        TreeRoot = IoPathPointRoot.MountPoint;
        CurrentMount = TreeRoot;
        do {
            if (CurrentMount->TargetEntry == Target->PathEntry) {
                FoundMount = CurrentMount;
                Target->MountPoint = FoundMount;
                break;
            }

            if (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE) {
                CurrentMount = LIST_VALUE(CurrentMount->ChildListHead.Previous,
                                          MOUNT_POINT,
                                          SiblingListEntry);

                continue;
            }

            while (CurrentMount != TreeRoot) {
                if (CurrentMount->SiblingListEntry.Previous !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                       CurrentMount->SiblingListEntry.Previous,
                                       MOUNT_POINT,
                                       SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
            }

        } while (CurrentMount != TreeRoot);
    }

    Status = IopCopyMountTree(MountPoint, Target, CopyFlags);
    if (!KSUCCESS(Status)) {
        goto CreateAndInsertMountPointEnd;
    }

CreateAndInsertMountPointEnd:
    if (FoundMount != NULL) {
        Target->MountPoint = NULL;
    }

    //
    // If a new mount point was created, insert it on the list of mounts, even
    // if this routine failed creating a copy of its chldren. The mount point
    // and its children cannot be destroyed while the mount lock is held, so it
    // is up to the caller to do so.
    //

    if (MountPoint != NULL) {
        INSERT_BEFORE(&(MountPoint->SiblingListEntry), MountList);
    }

    return Status;
}

KSTATUS
IopCopyMountTree (
    PMOUNT_POINT NewRoot,
    PPATH_POINT Target,
    ULONG Flags
    )

/*++

Routine Description:

    This routine copies an mount points that exist below the given target path
    point to the given mount point root.

Arguments:

    NewRoot - Supplies a pointer to the mount point that is the root of the new
        tree.

    Target - Supplies a pointer to the path point under which any mount points
        are to be copied.

    Flags - Supplies a bitmask of mount flags. See MOUNT_FLAG_* for definitions.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PMOUNT_POINT CurrentMount;
    PATH_POINT CurrentPathPoint;
    PMOUNT_POINT CurrentRoot;
    BOOL DescendantPath;
    PMOUNT_POINT NewMountParent;
    PMOUNT_POINT NewMountPoint;
    PATH_POINT NewPathPoint;
    PMOUNT_POINT OldRoot;
    KSTATUS Status;
    ULONG TargetPathSize;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoMountLock) != FALSE);

    //
    // The new root should not be live in the mount tree.
    //

    ASSERT(NewRoot->SiblingListEntry.Next == NULL);

    OldRoot = Target->MountPoint;
    if (OldRoot == NULL) {
        return STATUS_SUCCESS;
    }

    //
    // If the old root has no children, then this is quick work.
    //

    if (LIST_EMPTY(&(OldRoot->ChildListHead)) != FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Iterate over the list backwards. This causes older mount points to get
    // inserted into the child lists before newer mount points, keeping things
    // in the correct order. Acquiring the lock in shared mode is OK because
    // the routine is not altering the live mount tree, just iterating over it
    // to copy mount points onto a new mount tree that will be exclusively
    // linked later.
    //

    CurrentEntry = OldRoot->ChildListHead.Previous;
    while (CurrentEntry != &(OldRoot->ChildListHead)) {
        CurrentRoot = LIST_VALUE(CurrentEntry, MOUNT_POINT, SiblingListEntry);
        CurrentEntry = CurrentEntry->Previous;

        //
        // Make sure the copy flags match.
        //

        if ((CurrentRoot->Flags & Flags) != Flags) {
            continue;
        }

        //
        // Check to make sure this mount point is a descendant of the target
        // path entry only if the target path entry isn't the root of the old
        // mount point.
        //

        if (IO_IS_MOUNT_POINT(Target) == FALSE) {
            DescendantPath = IopIsDescendantPath(Target->PathEntry,
                                                 CurrentRoot->MountEntry);

            if (DescendantPath == FALSE) {
                continue;
            }
        }

        //
        // Now copy the entire mount tree under the old mount point.
        //

        CurrentMount = CurrentRoot;
        NewMountParent = NewRoot;
        do {
            if ((CurrentMount->Flags & Flags) == Flags) {
                CurrentPathPoint.PathEntry = CurrentMount->TargetEntry;
                CurrentPathPoint.MountPoint = CurrentMount;
                NewPathPoint.PathEntry = CurrentMount->MountEntry;
                NewPathPoint.MountPoint = NewMountParent;
                TargetPathSize = RtlStringLength(CurrentMount->TargetPath) + 1;
                NewMountPoint = IopCreateMountPoint(&NewPathPoint,
                                                    &CurrentPathPoint,
                                                    CurrentRoot->TargetPath,
                                                    TargetPathSize,
                                                    CurrentMount->Flags);

                if (NewMountPoint == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto CopyMountTreeEnd;
                }

                ASSERT(NewMountPoint->Parent == NewMountParent);

                INSERT_AFTER(&(NewMountPoint->SiblingListEntry),
                             &(NewMountParent->ChildListHead));

                //
                // Iterate to the current mount point's last child if it exists.
                //

                if (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE) {
                    CurrentMount = LIST_VALUE(
                                          CurrentMount->ChildListHead.Previous,
                                          MOUNT_POINT,
                                          SiblingListEntry);

                    NewMountParent = NewMountPoint;
                    continue;
                }
            }

            //
            // Move to a sibling or ancestor's sibling.
            //

            while (CurrentMount != CurrentRoot) {
                if (CurrentMount->SiblingListEntry.Previous !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                       CurrentMount->SiblingListEntry.Previous,
                                       MOUNT_POINT,
                                       SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
                NewMountParent = NewMountParent->Parent;
            }

        } while (CurrentMount != CurrentRoot);
    }

    Status = STATUS_SUCCESS;

CopyMountTreeEnd:
    return Status;
}

VOID
IopDestroyMountTree (
    PMOUNT_POINT Root,
    PLIST_ENTRY DestroyList
    )

/*++

Routine Description:

    This routine destroys the tree of mounts starting at the root. It does not
    check reference counts. As such, it is useful for lazy recursive unmounts.
    It appends all the destroyed mount points to the end of the given destroy
    list. This transfers the original reference on the mount from the tree to
    given list. The caller should iterate over the list, dereferencing each
    element and decrementing the mount count.

Arguments:

    Root - Supplies a pointer to the root path entry.

    DestroyList - Supplies a pointer to the head of a list where all the
        destroyed mount points will be placed.

Return Value:

    None.

--*/

{

    PLIST_ENTRY ChildEntry;
    PMOUNT_POINT ChildMount;
    PLIST_ENTRY CurrentEntry;
    PMOUNT_POINT CurrentMount;
    LIST_ENTRY ProcessList;

    INITIALIZE_LIST_HEAD(&ProcessList);

    //
    // Add the root mount point onto the process list. Remove it from its
    // parent list first. It always gets destroyed; the flags don't matter.
    //

    if (Root->SiblingListEntry.Next != NULL) {
        LIST_REMOVE(&(Root->SiblingListEntry));
    }

    INSERT_BEFORE(&(Root->SiblingListEntry), &ProcessList);

    //
    // Now iterate over the process list, processing and adding the children of
    // each element to the end of the destroy list to be processed.
    //

    CurrentEntry = ProcessList.Next;
    while (CurrentEntry != &ProcessList) {
        CurrentMount = LIST_VALUE(CurrentEntry, MOUNT_POINT, SiblingListEntry);

        //
        // Erase any memory of its parent now that it is out of the tree.
        //

        ASSERT(CurrentMount->Parent != NULL);

        CurrentMount->Parent = NULL;

        //
        // Now process any of its children, adding them to the end of the
        // list to be processed.
        //

        ChildEntry = CurrentMount->ChildListHead.Next;
        while (ChildEntry != &(CurrentMount->ChildListHead)) {
            ChildMount = LIST_VALUE(ChildEntry, MOUNT_POINT, SiblingListEntry);
            ChildEntry = ChildEntry->Next;
            INSERT_BEFORE(&(ChildMount->SiblingListEntry), &ProcessList);
        }

        //
        // All the children are on the process list. Re-initialize the curent
        // mount's child list.
        //

        INITIALIZE_LIST_HEAD(&(CurrentMount->ChildListHead));
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Now append the process list to the destroy list.
    //

    APPEND_LIST(&ProcessList, DestroyList);
    return;
}

KSTATUS
IopLinkMountPoint (
    PMOUNT_POINT MountPoint,
    PPATH_POINT Target,
    PLIST_ENTRY MountList
    )

/*++

Routine Description:

    This routine links the given mount point to other locations in the mount
    tree where its mount path entry can be found. At those locations, it will
    create new mount points that join the mount path entry to the given target.
    This routine assumes the global mount lock is held exclusively.

Arguments:

    MountPoint - Supplies a pointer to the mount point that is to be linked.

    Target - Supplies a pointer to the target path point for the linked mount
        points.

    MountList - Supplies a pointer to the head of the list where all newly
        created mount points will be stored, on both success and failure, so
        that the caller can either insert them into the live mount tree or
        destroy them once the mount lock is released.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PMOUNT_POINT *Array;
    ULONG ArrayCount;
    PMOUNT_POINT CurrentMount;
    PPATH_ENTRY CurrentPathEntry;
    ULONG Index;
    PATH_POINT Mount;
    ULONG MountCount;
    KSTATUS Status;
    PSTR TargetPath;
    ULONG TargetPathSize;
    PMOUNT_POINT TreeRoot;

    ASSERT(KeIsSharedExclusiveLockHeldExclusive(IoMountLock) != FALSE);
    ASSERT(MountPoint->Parent != NULL);
    ASSERT((MountPoint->Flags & MOUNT_FLAG_LINKED) != 0);
    ASSERT(MountPoint->TargetEntry == Target->PathEntry);

    Array = NULL;
    ArrayCount = 0;
    Mount.PathEntry = NULL;
    MountCount = 0;
    TargetPath = MountPoint->TargetPath;
    TargetPathSize = RtlStringLength(MountPoint->TargetPath) + 1;
    TreeRoot = IoPathPointRoot.MountPoint;

    //
    // Walk up the mount point's mount path entry tree searching for other
    // locations where a path element is mounted. For each mount point found,
    // add a linked mount point.
    //

    CurrentPathEntry = MountPoint->MountEntry->Parent;
    while (CurrentPathEntry != NULL) {

        //
        // Make the array big enough to match the number of mounts.
        //

        if (MountCount > ArrayCount) {
            if (Array != NULL) {
                MmFreePagedPool(Array);
            }

            ArrayCount = MountCount;
            AllocationSize = ArrayCount * sizeof(MOUNT_POINT);
            Array = MmAllocatePagedPool(AllocationSize, IO_ALLOCATION_TAG);
            if (Array == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto LinkMountPointEnd;
            }
        }

        //
        // Find the mounts that target the current path entry. Skip the given
        // mount's parent to avoid duplicates.
        //

        MountCount = 0;
        CurrentMount = TreeRoot;
        do {
            if ((CurrentMount->TargetEntry == CurrentPathEntry) &&
                (CurrentMount != MountPoint->Parent)) {

                if (MountCount < ArrayCount) {
                    Array[MountCount] = CurrentMount;
                }

                MountCount += 1;
            }

            if (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE) {
                CurrentMount = LIST_VALUE(CurrentMount->ChildListHead.Previous,
                                          MOUNT_POINT,
                                          SiblingListEntry);

                continue;
            }

            while (CurrentMount != TreeRoot) {
                if (CurrentMount->SiblingListEntry.Previous !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                       CurrentMount->SiblingListEntry.Previous,
                                       MOUNT_POINT,
                                       SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
            }

        } while (CurrentMount != TreeRoot);

        //
        // Skip to the next path entry if no mount points were found.
        //

        if (MountCount == 0) {
            CurrentPathEntry = CurrentPathEntry->Parent;
            continue;
        }

        //
        // If the mount count is greater than the array count, then the array
        // was not big enough. Try again with the appropriate sized array.
        //

        if (MountCount > ArrayCount) {
            continue;
        }

        //
        // For each mount point in the array, create and insert a new mount
        // point below the current mount point and on top of the same path
        // entry as the given mount. The new mounts are added to the mount list
        // and will be inserted into the live mount tree by the caller.
        //

        for (Index = 0; Index < MountCount; Index += 1) {
            Mount.PathEntry = MountPoint->MountEntry;
            Mount.MountPoint = Array[Index];
            Status = IopCreateAndCopyMountPoint(&Mount,
                                                Target,
                                                MountList,
                                                TargetPath,
                                                TargetPathSize,
                                                MountPoint->Flags);

            if (!KSUCCESS(Status)) {
                goto LinkMountPointEnd;
            }
        }

        CurrentPathEntry = CurrentPathEntry->Parent;
    }

    Status = STATUS_SUCCESS;

LinkMountPointEnd:
    if (Array != NULL) {
        MmFreePagedPool(Array);
    }

    return Status;
}

VOID
IopDestroyLinkedMountPoints (
    PMOUNT_POINT Mount,
    PLIST_ENTRY DestroyList
    )

/*++

Routine Description:

    This routine destroys any mount points linked to the given mount point.

Arguments:

    Mount - Supplies a pointer to the mount point whose linked mounts are to
        be destroyed.

    DestroyList - Supplies a pointer to the head of a list where all the
        destroyed mount points will be placed.

Return Value:

    None.

--*/

{

    PMOUNT_POINT CurrentMount;
    PMOUNT_POINT NextMount;
    PMOUNT_POINT TreeRoot;

    TreeRoot = IoPathPointRoot.MountPoint;

    //
    // Iterate over the tree of mount points starting at the root in search of
    // linked mount points that have a target path entry matching that of the
    // given mount point.
    //

    CurrentMount = TreeRoot;
    do {
        if ((CurrentMount->TargetEntry == Mount->TargetEntry) &&
            ((CurrentMount->Flags & MOUNT_FLAG_LINKED) != 0)) {

            //
            // This mount point is about to be removed from the tree. Get its
            // next sibling or one if its ancestor's siblings.
            //

            NextMount = CurrentMount;
            while (NextMount != TreeRoot) {
                if (NextMount->SiblingListEntry.Previous !=
                    &(NextMount->Parent->ChildListHead)) {

                    NextMount = LIST_VALUE(NextMount->SiblingListEntry.Previous,
                                           MOUNT_POINT,
                                           SiblingListEntry);

                    break;
                }

                NextMount = NextMount->Parent;
            }

            IopDestroyMountTree(CurrentMount, DestroyList);
            CurrentMount = NextMount;

        //
        // Check the children for any linked mount points.
        //

        } else if (LIST_EMPTY(&(CurrentMount->ChildListHead)) == FALSE) {
            CurrentMount = LIST_VALUE(CurrentMount->ChildListHead.Previous,
                                      MOUNT_POINT,
                                      SiblingListEntry);

        //
        // Otherwise back up to a sibling or ancestor's sibling.
        //

        } else {
            while (CurrentMount != TreeRoot) {
                if (CurrentMount->SiblingListEntry.Previous !=
                    &(CurrentMount->Parent->ChildListHead)) {

                    CurrentMount = LIST_VALUE(
                                       CurrentMount->SiblingListEntry.Previous,
                                       MOUNT_POINT,
                                       SiblingListEntry);

                    break;
                }

                CurrentMount = CurrentMount->Parent;
            }
        }

    } while (CurrentMount != TreeRoot);

    return;
}

KSTATUS
IopGetMountPointsFromTree (
    PPATH_POINT ProcessRoot,
    PMOUNT_POINT TreeRoot,
    PVOID *BufferOffset,
    PUINTN BytesRemaining,
    PUINTN RequiredSize
    )

/*++

Routine Description:

    This routine converts all the mount points in the given mount tree into
    mount point entries. That is, it collects the mount point and target paths
    and stores them into the given buffer starting at the given offset.

Arguments:

    ProcessRoot - Supplies the root path point of the calling process.

    TreeRoot - Supplies the root mount point of the mount tree that is to be
        converted.

    BufferOffset - Supplies a pointer to the buffer that is to store the
        converted data. This routine increments the pointer as it writes to the
        buffer.

    BytesRemaining - Supplies a pointer to the number of bytes remaining in the
        given buffer. This routine decrements the bytes remaining if it writes
        to the buffer.

    RequiredSize - Supplies a pointer that receives the total number of bytes
        required to convert the given mount tree.

Return Value:

    Status code.

--*/

{

    PSTR Destination;
    PMOUNT_POINT MountPoint;
    PMOUNT_POINT_ENTRY MountPointEntry;
    UINTN MountPointEntrySize;
    PSTR MountPointPath;
    UINTN MountPointPathSize;
    PATH_POINT PathPoint;
    KSTATUS Status;
    UINTN TargetPathSize;

    *RequiredSize = 0;
    MountPointPath = NULL;

    //
    // Iterate over the tree of mount points starting at the root.
    //

    MountPoint = TreeRoot;
    do {

        //
        // Get the path to the mount point.
        //

        PathPoint.PathEntry = MountPoint->TargetEntry;
        PathPoint.MountPoint = MountPoint;
        Status = IopGetPathFromRootUnlocked(&PathPoint,
                                            ProcessRoot,
                                            &MountPointPath,
                                            &MountPointPathSize);

        if (!KSUCCESS(Status)) {
            goto GetMountPointsFromTreeEnd;
        }

        //
        // Calculate the size of this mount point.
        //

        ASSERT(MountPoint->TargetPath != NULL);

        TargetPathSize = RtlStringLength(MountPoint->TargetPath) + 1;
        MountPointEntrySize = sizeof(MOUNT_POINT_ENTRY) +
                              MountPointPathSize +
                              TargetPathSize;

        //
        // Write the mount point to the buffer if it is big enough.
        //

        if (*BytesRemaining >= MountPointEntrySize) {
            MountPointEntry = (PMOUNT_POINT_ENTRY)*BufferOffset;
            MountPointEntry->Flags = 0;
            if ((MountPoint->Flags & MOUNT_FLAG_BIND) != 0) {
                MountPointEntry->Flags |= SYS_MOUNT_FLAG_BIND;
            }

            if ((MountPoint->Flags & MOUNT_FLAG_RECURSIVE) != 0) {
                MountPointEntry->Flags |= SYS_MOUNT_FLAG_RECURSIVE;
            }

            if ((MountPoint->TargetEntry->Parent != NULL) &&
                (MountPoint->TargetEntry->SiblingListEntry.Next == NULL)) {

                MountPointEntry->Flags |= SYS_MOUNT_FLAG_TARGET_UNLINKED;
            }

            MountPointEntry->MountPointPathOffset = sizeof(MOUNT_POINT_ENTRY);
            MountPointEntry->TargetPathOffset = sizeof(MOUNT_POINT_ENTRY) +
                                                MountPointPathSize;

            Destination = (PVOID)MountPointEntry +
                          MountPointEntry->MountPointPathOffset;

            RtlStringCopy(Destination, MountPointPath, MountPointPathSize);
            Destination = (PVOID)MountPointEntry +
                          MountPointEntry->TargetPathOffset;

            RtlStringCopy(Destination, MountPoint->TargetPath, TargetPathSize);
            *BufferOffset += MountPointEntrySize;
            *BytesRemaining -= MountPointEntrySize;
        }

        //
        // Release the target path.
        //

        MmFreePagedPool(MountPointPath);
        MountPointPath = NULL;

        //
        // Even if the buffer is not big enough, increment the required size
        // and continue.
        //

        *RequiredSize += MountPointEntrySize;

        //
        // If the current mount point has children, then the next mount point
        // is the first child.
        //

        if (LIST_EMPTY(&(MountPoint->ChildListHead)) == FALSE) {
            MountPoint = LIST_VALUE(MountPoint->ChildListHead.Previous,
                                    MOUNT_POINT,
                                    SiblingListEntry);

            continue;
        }

        //
        // Otherwise get the next sibling or ancestor's sibling.
        //

        while (MountPoint != TreeRoot) {
            if (MountPoint->SiblingListEntry.Previous !=
                &(MountPoint->Parent->ChildListHead)) {

                MountPoint = LIST_VALUE(MountPoint->SiblingListEntry.Previous,
                                        MOUNT_POINT,
                                        SiblingListEntry);

                break;
            }

            MountPoint = MountPoint->Parent;
        }

    } while (MountPoint != TreeRoot);

    Status = STATUS_SUCCESS;

GetMountPointsFromTreeEnd:
    if (MountPointPath != NULL) {
        MmFreePagedPool(MountPointPath);
    }

    return Status;
}

