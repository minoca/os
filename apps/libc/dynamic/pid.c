/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pid.c

Abstract:

    This module implements C library functionality loosely tied to process and
    thread IDs.

Author:

    Evan Green 30-Mar-2013

Environment:

    User Mode C Library.

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

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
// TODO: These should be thread local.
//

BOOL ClThreadIdentityValid;
THREAD_IDENTITY ClThreadIdentity;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
pid_t
getpid (
    void
    )

/*++

Routine Description:

    This routine returns the current process identifier.

Arguments:

    None.

Return Value:

    Returns the process identifier.

--*/

{

    PROCESS_ID ProcessId;
    KSTATUS Status;

    ProcessId = 0;
    Status = OsGetProcessId(ProcessIdProcess, &ProcessId);

    ASSERT(KSUCCESS(Status));

    return ProcessId;
}

LIBC_API
pid_t
getppid (
    void
    )

/*++

Routine Description:

    This routine returns the current process' parent process identifier.

Arguments:

    None.

Return Value:

    Returns the parent process identifier.

--*/

{

    PROCESS_ID ProcessId;
    KSTATUS Status;

    ProcessId = 0;
    Status = OsGetProcessId(ProcessIdParentProcess, &ProcessId);

    ASSERT(KSUCCESS(Status));

    return ProcessId;
}

LIBC_API
pid_t
getpgid (
    pid_t ProcessId
    )

/*++

Routine Description:

    This routine returns the process group identifier of the process with
    the given ID, or the calling process.

Arguments:

    ProcessId - Supplies the process ID to return the process group for. Supply
        0 to return the process group ID of the calling process.

Return Value:

    Returns the process group ID of the given process (or the current process).

    (pid_t)-1 and errno will be set to EPERM if the desired process is out of
    this session and the implementation doesn't allow cross session requests,
    ESRCH if no such process exists, or EINVAL if the pid argument is invalid.

--*/

{

    PROCESS_ID Result;
    KSTATUS Status;

    Result = ProcessId;
    Status = OsGetProcessId(ProcessIdProcessGroup, &Result);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return (pid_t)-1;
    }

    return Result;
}

LIBC_API
pid_t
getpgrp (
    void
    )

/*++

Routine Description:

    This routine returns the process group identifier of the calling process.

Arguments:

    None.

Return Value:

    Returns the process group ID of the calling process.

--*/

{

    return getpgid(0);
}

LIBC_API
int
setpgid (
    pid_t ProcessId,
    pid_t ProcessGroupId
    )

/*++

Routine Description:

    This routine joins an existing process group or creates a new process group
    within the session of the calling process. The process group ID of a
    session leader will not change.

Arguments:

    ProcessId - Supplies the process ID of the process to put in a new process
        group. Supply 0 to use the current process.

    ProcessGroupId - Supplies the new process group to put the process in.
        Supply zero to set the process group ID to the same numerical value as
        the specified process ID.

Return Value:

    0 on success.

    -1 on failure and errno will be set to contain more information.

--*/

{

    KSTATUS Status;

    Status = OsSetProcessId(ProcessIdProcessGroup, ProcessId, ProcessGroupId);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
pid_t
setpgrp (
    void
    )

/*++

Routine Description:

    This routine sets the process group ID of the calling process to the
    process ID of the calling process. This routine has no effect if the
    calling process is a session leader.

Arguments:

    None.

Return Value:

    Returns the process group ID of the calling process.

--*/

{

    setpgid(0, 0);
    return getpgid(0);
}

LIBC_API
pid_t
getsid (
    pid_t ProcessId
    )

/*++

Routine Description:

    This routine returns the process group ID of the process that is the
    session leader of the given process. If the given parameter is 0, then
    the current process ID is used as the parameter.

Arguments:

    ProcessId - Supplies a process ID of the process whose session leader
        should be returned.

Return Value:

    Returns the process group ID of the session leader of the specified process.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    PROCESS_ID Result;
    KSTATUS Status;

    Result = ProcessId;
    Status = OsGetProcessId(ProcessIdSession, &Result);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return (pid_t)-1;
    }

    return Result;
}

LIBC_API
pid_t
setsid (
    void
    )

/*++

Routine Description:

    This routine creates a new session if the calling process is not a
    process group leader. The calling process will be the session leader of
    the new session, and will be the process group leader of a new process
    group, and will have no controlling terminal. The process group ID of the
    calling process will be set equal to the process ID of the calling process.
    The calling process will be the only process in the new process group and
    the only process in the new session.

Arguments:

    None.

Return Value:

    Returns the value of the new process group ID of the calling process.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    KSTATUS Status;

    Status = OsSetProcessId(ProcessIdSession, 0, 0);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return getpgid(0);
}

LIBC_API
pid_t
fork (
    void
    )

/*++

Routine Description:

    This routine creates a new process by copying the existing process.

Arguments:

    None.

Return Value:

    Returns 0 to the child process.

    Returns the process ID of the child process to the parent process.

    Returns -1 to the parent process on error, and the errno variable will be
    set to provide more information about the error.

--*/

{

    PROCESS_ID NewProcess;
    KSTATUS Status;

    ClpRunAtforkPrepareRoutines();
    fflush(NULL);
    Status = OsForkProcess(0, &NewProcess);
    if (NewProcess == 0) {
        ClpRunAtforkChildRoutines();

    //
    // Call the parent at-fork routines even on failure so that at least the
    // mutex is unlocked.
    //

    } else {
        ClpRunAtforkParentRoutines();
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return NewProcess;
}

LIBC_API
uid_t
getuid (
    void
    )

/*++

Routine Description:

    This routine returns the current real user ID.

Arguments:

    None.

Return Value:

    Returns the real user ID.

--*/

{

    KSTATUS Status;

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    return ClThreadIdentity.RealUserId;
}

LIBC_API
gid_t
getgid (
    void
    )

/*++

Routine Description:

    This routine returns the current real group ID.

Arguments:

    None.

Return Value:

    Returns the real group ID.

--*/

{

    KSTATUS Status;

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    return ClThreadIdentity.RealGroupId;
}

LIBC_API
uid_t
geteuid (
    void
    )

/*++

Routine Description:

    This routine returns the current effective user ID, which represents the
    privilege level with which this process can perform operations. Normally
    this is the same as the real user ID, but binaries with the setuid
    permission bit set the effective user ID to their own when they're run.

Arguments:

    None.

Return Value:

    Returns the effective user ID.

--*/

{

    KSTATUS Status;

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    return ClThreadIdentity.EffectiveUserId;
}

LIBC_API
gid_t
getegid (
    void
    )

/*++

Routine Description:

    This routine returns the current effective group ID, which represents the
    privilege level with which this process can perform operations. Normally
    this is the same as the real group ID, but binaries with the setgid
    permission bit set the effective group ID to their own when they're run.

Arguments:

    None.

Return Value:

    Returns the effective group ID.

--*/

{

    KSTATUS Status;

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    return ClThreadIdentity.EffectiveGroupId;
}

LIBC_API
int
setuid (
    uid_t UserId
    )

/*++

Routine Description:

    This routine sets the real user ID, effective user ID, and saved
    set-user-ID of the calling process to the given user ID. This only occurs
    if the process has the appropriate privileges to do this. If the process
    does not have appropriate privileges but the given user ID is equal to the
    real user ID or the saved set-user-ID, then this routine sets the effective
    user ID to the given user ID; the real user ID and saved set-user-ID remain
    unchanged.

Arguments:

    UserId - Supplies the user ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real user ID or saved set-user-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    ClThreadIdentity.RealUserId = UserId;
    ClThreadIdentity.EffectiveUserId = UserId;
    ClThreadIdentity.SavedUserId = UserId;
    Fields = THREAD_IDENTITY_FIELD_REAL_USER_ID |
             THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID |
             THREAD_IDENTITY_FIELD_SAVED_USER_ID;

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setgid (
    gid_t GroupId
    )

/*++

Routine Description:

    This routine sets the real group ID, effective group ID, and saved
    set-group-ID of the calling process to the given group ID. This only occurs
    if the process has the appropriate privileges to do this. If the process
    does not have appropriate privileges but the given group ID is equal to the
    real group ID or the saved set-group-ID, then this routine sets the
    effective group ID to the given group ID; the real group ID and saved
    set-group-ID remain unchanged.

Arguments:

    GroupId - Supplies the group ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real group ID or saved set-group-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    ClThreadIdentity.RealGroupId = GroupId;
    ClThreadIdentity.EffectiveGroupId = GroupId;
    ClThreadIdentity.SavedGroupId = GroupId;
    Fields = THREAD_IDENTITY_FIELD_REAL_GROUP_ID |
             THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID |
             THREAD_IDENTITY_FIELD_SAVED_GROUP_ID;

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
seteuid (
    uid_t UserId
    )

/*++

Routine Description:

    This routine sets the effective user ID of the calling process to the given
    user ID. The real user ID and saved set-user-ID remain unchanged. This only
    occurs if the process has appropriate privileges, or if the real user ID
    is equal to the saved set-user-ID.

Arguments:

    UserId - Supplies the effective user ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real user ID or saved set-user-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    ClThreadIdentity.EffectiveUserId = UserId;
    Fields = THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID;
    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setegid (
    gid_t GroupId
    )

/*++

Routine Description:

    This routine sets the effective group ID of the calling process to the
    given group ID. The real group ID and saved set-group-ID remain unchanged.
    This only occurs if the process has appropriate privileges, or if the real
    group ID is equal to the saved set-group-ID.

Arguments:

    GroupId - Supplies the effective group ID to change to.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if the group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given ID does not
    match the real group ID or saved set-group-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    ClThreadIdentity.EffectiveGroupId = GroupId;
    Fields = THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID;
    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setreuid (
    uid_t RealUserId,
    uid_t EffectiveUserId
    )

/*++

Routine Description:

    This routine sets the real and/or effective user IDs of the current process
    to the given values. This only occurs if the process has appropriate
    privileges. Unprivileged processes may only set the effective user ID to
    the real or saved user IDs. Unprivileged users may only set the real
    group ID to the saved or effective user IDs. If the real user ID is being
    set, or the effective user ID is being set to something other than the
    previous real user ID, then the saved user ID is also set to the new
    effective user ID.

Arguments:

    RealUserId - Supplies the real user ID to change to. If -1 is supplied, the
        real user ID will not be changed.

    EffectiveUserId - Supplies the effective user ID to change to. If -1 is
        supplied, the effective user ID will not be changed.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if a user ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given effective ID
    does not match the real user ID or saved set-user-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    Fields = 0;

    //
    // First just get the identity.
    //

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    ClThreadIdentity.EffectiveUserId = EffectiveUserId;
    if (EffectiveUserId != (uid_t)-1) {
        Fields |= THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID;
        if (EffectiveUserId != ClThreadIdentity.RealUserId) {
            ClThreadIdentity.SavedUserId = ClThreadIdentity.EffectiveUserId;
            Fields |= THREAD_IDENTITY_FIELD_SAVED_USER_ID;
        }
    }

    ClThreadIdentity.RealUserId = RealUserId;
    if (RealUserId != (uid_t)-1) {
        ClThreadIdentity.SavedUserId = ClThreadIdentity.EffectiveUserId;
        Fields |= THREAD_IDENTITY_FIELD_REAL_USER_ID |
                  THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID;
    }

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setregid (
    gid_t RealGroupId,
    gid_t EffectiveGroupId
    )

/*++

Routine Description:

    This routine sets the real and/or effective group IDs of the current process
    to the given values. This only occurs if the process has appropriate
    privileges. Unprivileged processes may only set the effective group ID to
    the real or saved group IDs. Unprivileged users may only set the real
    group ID to the saved or effective group IDs. If the real group ID is being
    set, or the effective group ID is being set to something other than the
    previous real group ID, then the saved group ID is also set to the new
    effective group ID.

Arguments:

    RealGroupId - Supplies the real group ID to change to. If -1 is supplied,
        the real group ID will not be changed.

    EffectiveGroupId - Supplies the effective group ID to change to. If -1 is
        supplied, the effective group ID will not be changed.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. Common
    errors include EINVAL if a group ID is not valid, and EPERM if the
    process does not have appropriate privileges and the given effective ID
    does not match the real group ID or saved set-group-ID.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    Fields = 0;

    //
    // First just get the identity.
    //

    if (ClThreadIdentityValid == FALSE) {
        Status = OsSetThreadIdentity(0, &ClThreadIdentity);
        if (!KSUCCESS(Status)) {
            return -1;
        }

        ClThreadIdentityValid = TRUE;
    }

    ClThreadIdentity.EffectiveGroupId = EffectiveGroupId;
    if (EffectiveGroupId != (gid_t)-1) {
        Fields |= THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID;
        if (EffectiveGroupId != ClThreadIdentity.RealGroupId) {
            ClThreadIdentity.SavedGroupId = ClThreadIdentity.EffectiveGroupId;
            Fields |= THREAD_IDENTITY_FIELD_SAVED_GROUP_ID;
        }
    }

    ClThreadIdentity.RealGroupId = RealGroupId;
    if (RealGroupId != (gid_t)-1) {
        ClThreadIdentity.SavedGroupId = ClThreadIdentity.EffectiveGroupId;
        Fields |= THREAD_IDENTITY_FIELD_REAL_GROUP_ID |
                  THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID;
    }

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setresuid (
    uid_t RealUserId,
    uid_t EffectiveUserId,
    uid_t SavedUserId
    )

/*++

Routine Description:

    This routine sets the real, effective, and saved user IDs of the calling
    thread. A unprivileged process may set each one of these to one of the
    current real, effective, or saved user ID. A process with the setuid
    permission may set these to any values.

Arguments:

    RealUserId - Supplies the real user ID to set, or -1 to leave the value
        unchanged.

    EffectiveUserId - Supplies the effective user ID to set, or -1 to leave the
        value unchanged.

    SavedUserId - Supplies the saved user ID to set, or -1 to leave the value
        unchanged.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. This may
    fail even for root, so the return value must always be checked.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    Fields = 0;
    if (RealUserId != (uid_t)-1) {
        ClThreadIdentity.RealUserId = RealUserId;
        Fields |= THREAD_IDENTITY_FIELD_REAL_USER_ID;
    }

    if (EffectiveUserId != (uid_t)-1) {
        ClThreadIdentity.EffectiveUserId = EffectiveUserId;
        Fields |= THREAD_IDENTITY_FIELD_EFFECTIVE_USER_ID;
    }

    if (SavedUserId != (uid_t)-1) {
        ClThreadIdentity.SavedUserId = SavedUserId;
        Fields |= THREAD_IDENTITY_FIELD_SAVED_USER_ID;
    }

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
setresgid (
    gid_t RealGroupId,
    gid_t EffectiveGroupId,
    gid_t SavedGroupId
    )

/*++

Routine Description:

    This routine sets the real, effective, and saved group IDs of the calling
    thread. A unprivileged process may set each one of these to one of the
    current real, effective, or saved group ID. A process with the setuid
    permission may set these to any values.

Arguments:

    RealGroupId - Supplies the real group ID to set, or -1 to leave the value
        unchanged.

    EffectiveGroupId - Supplies the effective group ID to set, or -1 to leave
        the value unchanged.

    SavedGroupId - Supplies the saved group ID to set, or -1 to leave the value
        unchanged.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information. This may
    fail even for root, so the return value must always be checked.

--*/

{

    ULONG Fields;
    KSTATUS Status;

    Fields = 0;
    if (RealGroupId != (gid_t)-1) {
        ClThreadIdentity.RealGroupId = RealGroupId;
        Fields |= THREAD_IDENTITY_FIELD_REAL_GROUP_ID;
    }

    if (EffectiveGroupId != (gid_t)-1) {
        ClThreadIdentity.EffectiveGroupId = EffectiveGroupId;
        Fields |= THREAD_IDENTITY_FIELD_EFFECTIVE_GROUP_ID;
    }

    if (SavedGroupId != (gid_t)-1) {
        ClThreadIdentity.SavedGroupId = SavedGroupId;
        Fields |= THREAD_IDENTITY_FIELD_SAVED_GROUP_ID;
    }

    Status = OsSetThreadIdentity(Fields, &ClThreadIdentity);
    if (!KSUCCESS(Status)) {
        ClThreadIdentityValid = FALSE;
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClThreadIdentityValid = TRUE;
    ClpSetThreadIdentityOnAllThreads(Fields, &ClThreadIdentity);
    return 0;
}

LIBC_API
int
getgroups (
    int ElementCount,
    gid_t GroupList[]
    )

/*++

Routine Description:

    This routine returns the array of supplementary groups that the current
    user belongs to.

Arguments:

    ElementCount - Supplies the size (in elements) of the supplied group list
        buffer.

    GroupList - Supplies a buffer where the user's supplementary groups will
        be returned.

Return Value:

    Returns the number of supplementary groups that the current user belongs to.
    The full count is returned even if the element count is less than that so
    that the caller can regroup (get it) and try again if the buffer allocated
    was too small.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UINTN Count;
    KSTATUS Status;

    Count = ElementCount;
    Status = OsSetSupplementaryGroups(FALSE, (PGROUP_ID)GroupList, &Count);
    if ((KSUCCESS(Status)) || (Status == STATUS_BUFFER_TOO_SMALL)) {
        return Count;
    }

    errno = ClConvertKstatusToErrorNumber(Status);
    return -1;
}

LIBC_API
int
setgroups (
    size_t ElementCount,
    const gid_t *GroupList
    )

/*++

Routine Description:

    This routine sets the supplementary group membership of the calling
    process. The caller must have sufficient privileges to set supplementary
    group IDs.

Arguments:

    ElementCount - Supplies the size (in elements) of the supplied group list
        buffer.

    GroupList - Supplies an array of supplementary group IDs.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    UINTN Count;
    KSTATUS Status;

    Count = ElementCount;
    Status = OsSetSupplementaryGroups(TRUE, (PGROUP_ID)GroupList, &Count);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpSetSupplementaryGroupsOnAllThreads((PGROUP_ID)GroupList, Count);
    return 0;
}

LIBC_API
int
nice (
    int Increment
    )

/*++

Routine Description:

    This routine adds the given value to the current process' nice value. A
    process' nice value is a non-negative number for which a more positive
    value results in less favorable scheduling. Valid nice values are between
    0 and 2 * NZERO - 1.

Arguments:

    Increment - Supplies the increment to add to the current nice value.

Return Value:

    Returns the new nice value minus NZERO. Note that this can result in a
    successful return value of -1. Callers checking for errors should set
    errno to 0 before calling this function, then check errno after.

    -1 on failure, and errno will be set to indicate more information. This may
    fail with EPERM if the increment is negative and the caller does not have
    appropriate privileges.

--*/

{

    //
    // TODO: Implement nice.
    //

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

