/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    perm.c

Abstract:

    This module handles permission and access rights management in the I/O
    subsystem.

Author:

    Evan Green 10-Dec-2014

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

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to break whenever access denied is returned.
//

BOOL IoBreakOnAccessDenied = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
IopCheckPermissions (
    BOOL FromKernelMode,
    PPATH_POINT PathPoint,
    ULONG Access
    )

/*++

Routine Description:

    This routine performs a permission check for the current user at the given
    path point.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether the request actually
        originates from kernel mode or not.

    PathPoint - Supplies a pointer to the path point to check.

    Access - Supplies the desired access the user needs.

Return Value:

    STATUS_SUCCESS if the user has permission to access the given object in
    the requested way.

    STATUS_ACCESS_DENIED if the permission was not granted.

--*/

{

    PFILE_OBJECT FileObject;
    ULONG Rights;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    FileObject = PathPoint->PathEntry->FileObject;
    Rights = 0;

    //
    // If the caller wants execute permissions and none of the execute bits are
    // set, then even fancy override permissions can't make it succeed. This
    // doesn't apply to directories.
    //

    if ((FileObject->Properties.Type != IoObjectRegularDirectory) &&
        (FileObject->Properties.Type != IoObjectObjectDirectory)) {

        if (((Access & IO_ACCESS_EXECUTE) != 0) &&
            ((FileObject->Properties.Permissions &
              FILE_PERMISSION_ALL_EXECUTE) == 0)) {

            if (IoBreakOnAccessDenied != FALSE) {
                RtlDebugBreak();
            }

            Status = STATUS_ACCESS_DENIED;
            goto CheckPermissionsEnd;
        }
    }

    //
    // If this is kernel mode, then none of the other checks apply.
    //

    if (FromKernelMode != FALSE) {
        Status = STATUS_SUCCESS;
        goto CheckPermissionsEnd;
    }

    //
    // Determine whether to use the access bits of the user, group, or other.
    //

    Rights = FileObject->Properties.Permissions >> FILE_PERMISSION_OTHER_SHIFT;
    if (FileObject->Properties.UserId == Thread->Identity.EffectiveUserId) {
        Rights = FileObject->Properties.Permissions >>
                 FILE_PERMISSION_USER_SHIFT;

    } else if (PsIsUserInGroup(FileObject->Properties.GroupId) != FALSE) {
        Rights = FileObject->Properties.Permissions >>
                 FILE_PERMISSION_GROUP_SHIFT;
    }

    //
    // Check the rights. Exit out if they succeed on their own.
    //

    if ((Rights & Access & FILE_PERMISSION_ACCESS_MASK) == Access) {
        Status = STATUS_SUCCESS;
        goto CheckPermissionsEnd;
    }

    //
    // Succeed and exit if the user has file system override permissions.
    //

    if (KSUCCESS(PsCheckPermission(PERMISSION_FILE_ACCESS))) {
        Status = STATUS_SUCCESS;
        goto CheckPermissionsEnd;
    }

    //
    // If the user has the read/search permission, then succeed for:
    // 1) Read permissions on anything.
    // 2) Read/execute permissions on directories.
    //

    if (KSUCCESS(PsCheckPermission(PERMISSION_READ_SEARCH))) {
        if (Access == IO_ACCESS_READ) {
            Status = STATUS_SUCCESS;
            goto CheckPermissionsEnd;
        }

        if (((Access & IO_ACCESS_WRITE) == 0) &&
            ((FileObject->Properties.Type == IoObjectRegularDirectory) ||
             (FileObject->Properties.Type == IoObjectObjectDirectory))) {

            Status = STATUS_SUCCESS;
            goto CheckPermissionsEnd;
        }
    }

    //
    // Sorry, no access this time.
    //

    if (IoBreakOnAccessDenied != FALSE) {
        RtlDebugBreak();
    }

    Status = STATUS_ACCESS_DENIED;

CheckPermissionsEnd:
    return Status;
}

KSTATUS
IopCheckDeletePermission (
    BOOL FromKernelMode,
    PPATH_POINT DirectoryPathPoint,
    PPATH_POINT FilePathPoint
    )

/*++

Routine Description:

    This routine performs a permission check for the current user at the given
    path point, in preparation for removing a directory entry during a rename
    or unlink operation.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this
        request actually originated from kernel mode.

    DirectoryPathPoint - Supplies a pointer to the directory path point the
        file resides in.

    FilePathPoint - Supplies a pointer to the file path point being deleted or
        renamed.

Return Value:

    STATUS_SUCCESS if the user has permission to access the given object in
    the requested way.

    STATUS_ACCESS_DENIED if the permission was not granted.

--*/

{

    PFILE_OBJECT DirectoryFileObject;
    PFILE_OBJECT FileObject;
    KSTATUS Status;
    PKTHREAD Thread;

    Thread = KeGetCurrentThread();
    DirectoryFileObject = DirectoryPathPoint->PathEntry->FileObject;

    //
    // First ensure the caller has write access to the directory.
    //

    Status = IopCheckPermissions(FromKernelMode,
                                 DirectoryPathPoint,
                                 IO_ACCESS_WRITE);

    if (!KSUCCESS(Status)) {
        goto CheckDeletePermissionEnd;
    }

    //
    // If the restricted bit is set, then only the file owner can rename or
    // delete the file, even though the caller has write permission in the
    // directory. This is often used on temporary directories to prevent users
    // from deleting each others files.
    //

    if ((DirectoryFileObject->Properties.Permissions &
         FILE_PERMISSION_RESTRICTED) != 0) {

        FileObject = FilePathPoint->PathEntry->FileObject;
        if (Thread->Identity.EffectiveUserId != FileObject->Properties.UserId) {
            if (!KSUCCESS(PsCheckPermission(PERMISSION_FILE_ACCESS))) {
                Status = STATUS_ACCESS_DENIED;
                goto CheckDeletePermissionEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

CheckDeletePermissionEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

