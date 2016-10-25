/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pgroups.c

Abstract:

    This module implements support for process groups and sessions.

Author:

    Evan Green 14-May-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define PROCESS_GROUP_MAX_REFERENCE_COUNT 0x10000000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PspProcessGroupAddReference (
    PPROCESS_GROUP ProcessGroup
    );

VOID
PspProcessGroupReleaseReference (
    PPROCESS_GROUP ProcessGroup
    );

PPROCESS_GROUP
PspLookupProcessGroup (
    PROCESS_GROUP_ID ProcessGroupId
    );

VOID
PspSignalProcessGroup (
    PPROCESS_GROUP ProcessGroup,
    ULONG SignalNumber
    );

VOID
PspProcessGroupHandleLeavingProcess (
    PKPROCESS Process,
    PPROCESS_GROUP OldGroup,
    PPROCESS_GROUP NewGroup
    );

BOOL
PspIsOrphanedProcessGroupStopped (
    PPROCESS_GROUP ProcessGroup
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the list of process groups.
//

LIST_ENTRY PsProcessGroupList;

//
// Store the lock protecting the session and process group lists. The lock
// ordering requires that this lock be acquired before any process lock.
//

PQUEUED_LOCK PsProcessGroupListLock;

//
// ------------------------------------------------------------------ Functions
//

VOID
PsGetProcessGroup (
    PKPROCESS Process,
    PPROCESS_GROUP_ID ProcessGroupId,
    PSESSION_ID SessionId
    )

/*++

Routine Description:

    This routine returns the process group and session ID for the given process.

Arguments:

    Process - Supplies a pointer to the process whose process group and session
        IDs are desired. Supply NULL and the current process will be used.

    ProcessGroupId - Supplies an optional pointer where the process group ID of
        the requested process will be returned.

    SessionId - Supplies an optional pointer where the session ID of the
        requested process will be returned.

Return Value:

    None.

--*/

{

    if (Process == NULL) {
        Process = PsGetCurrentProcess();
    }

    //
    // Take a snap of the process' session ID and process group ID.
    //

    if (ProcessGroupId != NULL) {
        *ProcessGroupId = Process->Identifiers.ProcessGroupId;
    }

    if (SessionId != NULL) {
        *SessionId = Process->Identifiers.SessionId;
    }

    return;
}

BOOL
PsIsProcessGroupOrphaned (
    PROCESS_GROUP_ID ProcessGroupId
    )

/*++

Routine Description:

    This routine determines if a process group is orphaned.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to query.

Return Value:

    TRUE if the process group is orphaned or does not exist.

    FALSE if the process group has at least one parent within the session but
    outside the process group.

--*/

{

    PPROCESS_GROUP ProcessGroup;
    BOOL Result;

    Result = TRUE;
    KeAcquireQueuedLock(PsProcessGroupListLock);
    ProcessGroup = PspLookupProcessGroup(ProcessGroupId);
    if ((ProcessGroup != NULL) && (ProcessGroup->OutsideParents != 0)) {
        Result = FALSE;
    }

    KeReleaseQueuedLock(PsProcessGroupListLock);
    return Result;
}

BOOL
PsIsProcessGroupInSession (
    PROCESS_GROUP_ID ProcessGroupId,
    SESSION_ID SessionId
    )

/*++

Routine Description:

    This routine determines whether or not the given process group belongs to
    the given session.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to be tested.

    SessionId - Supplies the ID of the session to be searched for the given
        process group.

Return Value:

    Returns TRUE if the process group is in the given session.

--*/

{

    PPROCESS_GROUP ProcessGroup;
    BOOL Result;

    Result = FALSE;
    KeAcquireQueuedLock(PsProcessGroupListLock);
    ProcessGroup = PspLookupProcessGroup(ProcessGroupId);
    if ((ProcessGroup != NULL) &&
        (ProcessGroup->SessionId == SessionId)) {

        Result = TRUE;
    }

    KeReleaseQueuedLock(PsProcessGroupListLock);
    return Result;
}

KSTATUS
PsSignalProcessGroup (
    PROCESS_GROUP_ID ProcessGroupId,
    ULONG SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to every process in the given process group.

Arguments:

    ProcessGroupId - Supplies the ID of the process group to send the signal to.

    SignalNumber - Supplies the signal to send to each process in the group.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if there was no such process group.

--*/

{

    PPROCESS_GROUP ProcessGroup;
    KSTATUS Status;

    Status = STATUS_NOT_FOUND;
    KeAcquireQueuedLock(PsProcessGroupListLock);
    ProcessGroup = PspLookupProcessGroup(ProcessGroupId);
    if (ProcessGroup != NULL) {
        PspSignalProcessGroup(ProcessGroup, SignalNumber);
        Status = STATUS_SUCCESS;
    }

    KeReleaseQueuedLock(PsProcessGroupListLock);
    return Status;
}

KSTATUS
PspInitializeProcessGroupSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for process groups.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    INITIALIZE_LIST_HEAD(&PsProcessGroupList);

    ASSERT(PsProcessGroupListLock == NULL);

    PsProcessGroupListLock = KeCreateQueuedLock();
    if (PsProcessGroupListLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    return STATUS_SUCCESS;
}

KSTATUS
PspJoinProcessGroup (
    PKPROCESS Process,
    PROCESS_GROUP_ID ProcessGroupId,
    BOOL NewSession
    )

/*++

Routine Description:

    This routine creates a new process group and places the given process in it.

Arguments:

    Process - Supplies a pointer to the process to move to the new process
        group. Again, a process only gets to play this trick once; to play it
        again it needs to fork.

    ProcessGroupId - Supplies the identifier of the process group to join or
        create.

    NewSession - Supplies a boolean indicating whether to also join a new
        session or not.

Return Value:

    Status code.

--*/

{

    PKPROCESS CurrentProcess;
    PPROCESS_GROUP ExistingGroup;
    BOOL GroupLockHeld;
    PPROCESS_GROUP NewGroup;
    PPROCESS_GROUP OriginalGroup;
    PPROCESS_GROUP ProcessGroup;
    PROCESS_ID ProcessId;
    BOOL ProcessLockHeld;
    KSTATUS Status;

    CurrentProcess = PsGetCurrentProcess();
    GroupLockHeld = FALSE;
    NewGroup = NULL;
    OriginalGroup = NULL;
    ProcessGroup = NULL;
    ProcessId = Process->Identifiers.ProcessId;
    ProcessLockHeld = FALSE;

    //
    // If joining a new session, the process group ID better be the process ID.
    //

    ASSERT((NewSession == FALSE) || (ProcessGroupId == ProcessId));

    //
    // Fail if the process is not in the same session.
    //

    if (Process->Identifiers.SessionId !=
        CurrentProcess->Identifiers.SessionId) {

        return STATUS_PERMISSION_DENIED;
    }

    //
    // Do a quick exit check for success.
    //

    if (ProcessGroupId == Process->Identifiers.ProcessGroupId) {
        return STATUS_SUCCESS;
    }

    //
    // Create a new process group if this process is off to its own group.
    //

    if (ProcessGroupId == ProcessId) {
        NewGroup = MmAllocatePagedPool(sizeof(PROCESS_GROUP),
                                       PS_ALLOCATION_TAG);

        if (NewGroup == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto JoinProcessGroupEnd;
        }

        RtlZeroMemory(NewGroup, sizeof(PROCESS_GROUP));
        NewGroup->Identifier = ProcessGroupId;
        NewGroup->ReferenceCount = 1;
        NewGroup->SessionId = Process->Identifiers.SessionId;
        INITIALIZE_LIST_HEAD(&(NewGroup->ProcessListHead));
    }

    //
    // Acquire the process group list lock to synchronize with other
    // process group and session changes.
    //

    KeAcquireQueuedLock(PsProcessGroupListLock);
    GroupLockHeld = TRUE;

    //
    // If the process is already a session leader, then do not allow it to
    // join a new process group.
    //

    if (PsIsSessionLeader(Process)) {
        Status = STATUS_PERMISSION_DENIED;
        goto JoinProcessGroupEnd;
    }

    //
    // Fail if the process is exiting. Exempt new session creation because
    // 1) Kernel-initiated processes won't have any threads yet.
    // 2) Session creation can only happen by the process itself, so there's
    //    no way the process can exit.
    //

    if ((Process->ThreadCount == 0) && (NewSession == FALSE)) {
        Status = STATUS_NO_SUCH_PROCESS;
        goto JoinProcessGroupEnd;
    }

    //
    // See if it's already done.
    //

    if (ProcessGroupId == Process->Identifiers.ProcessGroupId) {
        Status = STATUS_SUCCESS;
        goto JoinProcessGroupEnd;
    }

    //
    // See if this process group exists already.
    //

    ExistingGroup = PspLookupProcessGroup(ProcessGroupId);
    if (ExistingGroup != NULL) {

        //
        // It's not possible to create a new session from an existing group.
        // This is the case that fails process group leaders trying to jump
        // sessions, as there will already be a pre-existing group for their
        // ID.
        //

        if (NewSession != FALSE) {
            Status = STATUS_PERMISSION_DENIED;
            goto JoinProcessGroupEnd;
        }

        //
        // The process group better have something in it if its still
        // hanging around.
        //

        ASSERT(LIST_EMPTY(&(ExistingGroup->ProcessListHead)) == FALSE);

        //
        // The process can only join the group if its calling process is in
        // the same session as a process with the given group ID.
        //

        if (CurrentProcess->Identifiers.SessionId != ExistingGroup->SessionId) {
            ExistingGroup = NULL;
            Status = STATUS_PERMISSION_DENIED;
            goto JoinProcessGroupEnd;
        }

        PspProcessGroupAddReference(ExistingGroup);
        ProcessGroup = ExistingGroup;

    //
    // There is no process group by that ID. If it's not trying to change to
    // the same ID as itself, then fail.
    //

    } else {
        if (NewGroup == NULL) {
            Status = STATUS_PERMISSION_DENIED;
            goto JoinProcessGroupEnd;
        }

        ProcessGroup = NewGroup;
        NewGroup = NULL;
    }

    //
    // Move to a new session if desired.
    //

    if (NewSession != FALSE) {
        ProcessGroup->SessionId = ProcessId;
        Process->Identifiers.SessionId = ProcessId;

        //
        // Clear the controlling terminal. The child process' controlling
        // terminals would only need to be cleared if this process was a
        // session leader. It is not, otherwise it could not become one now.
        //

        Process->ControllingTerminal = NULL;

    } else {

        //
        // If not creating a new session, moving process groups shouldn't
        // jump sessions.
        //

        ASSERT(ProcessGroup->SessionId == Process->Identifiers.SessionId);
    }

    //
    // Acquire the process lock and make sure that it has not executed an image
    // if there is no new session and it is not the current process.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    ProcessLockHeld = TRUE;
    if ((Process != CurrentProcess) &&
        ((Process->Flags & PROCESS_FLAG_EXECUTED_IMAGE) != 0)) {

        Status = STATUS_ACCESS_DENIED;
        goto JoinProcessGroupEnd;
    }

    //
    // If the process joining the new group brings with it a parent not
    // in the group but in the session, then the process group has a new tie to
    // the outside.
    //
    // Note that the parent's identifiers are only still valid if its
    // process group pointer is not NULL. It may be that the parent is on
    // its way out, having left its process group, but not quite orphaned
    // its children.
    //

    if ((Process->Parent != NULL) &&
        (Process->Parent->ProcessGroup != NULL) &&
        (Process->Parent->Identifiers.SessionId == ProcessGroup->SessionId) &&
        (Process->Parent->Identifiers.ProcessGroupId !=
         ProcessGroup->Identifier)) {

        ASSERT(NewSession == FALSE);

        ProcessGroup->OutsideParents += 1;
    }

    //
    // Pull the process off the old process group list.
    //

    OriginalGroup = Process->ProcessGroup;
    if (OriginalGroup != NULL) {
        LIST_REMOVE(&(Process->ProcessGroupListEntry));
    }

    //
    // If this is a new process group, add it to the global list and the
    // session.
    //

    if (ProcessGroup->ListEntry.Next == NULL) {
        INSERT_BEFORE(&(ProcessGroup->ListEntry), &PsProcessGroupList);
    }

    //
    // Add the process to its new process group's list and set the identifiers.
    //

    INSERT_BEFORE(&(Process->ProcessGroupListEntry),
                  &(ProcessGroup->ProcessListHead));

    Process->ProcessGroup = ProcessGroup;
    Process->Identifiers.ProcessGroupId = ProcessGroup->Identifier;

    //
    // Now that the process has officially switched process groups, release the
    // lock and let any execute image attempts proceed.
    //

    KeReleaseQueuedLock(Process->QueuedLock);
    ProcessLockHeld = FALSE;

    //
    // If the process has left behind a process group, handle that.
    //

    if (OriginalGroup != NULL) {
        PspProcessGroupHandleLeavingProcess(Process,
                                            OriginalGroup,
                                            ProcessGroup);
    }

    ProcessGroup = NULL;
    Status = STATUS_SUCCESS;

JoinProcessGroupEnd:
    if (ProcessLockHeld != FALSE) {
        KeReleaseQueuedLock(Process->QueuedLock);
    }

    if (GroupLockHeld != FALSE) {
        KeReleaseQueuedLock(PsProcessGroupListLock);
    }

    if (NewGroup != NULL) {
        PspProcessGroupReleaseReference(NewGroup);
    }

    if (ProcessGroup != NULL) {

        ASSERT(ProcessGroup != NewGroup);

        PspProcessGroupReleaseReference(ProcessGroup);
    }

    if (OriginalGroup != NULL) {
        PspProcessGroupReleaseReference(OriginalGroup);
    }

    return Status;
}

VOID
PspAddProcessToParentProcessGroup (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine adds the given new process to its parent's process group. The
    caller cannot have any of the process locks held.

Arguments:

    Process - Supplies a pointer to the new process.

Return Value:

    None.

--*/

{

    PPROCESS_GROUP ProcessGroup;

    ASSERT(Process->ProcessGroup == NULL);
    ASSERT(Process->Parent != NULL);

    //
    // The process groups outside parent count does not need changing because
    // the new process's parent will always be an inside parent. The child
    // inherits the parent's process group and session.
    //

    KeAcquireQueuedLock(PsProcessGroupListLock);
    ProcessGroup = Process->Parent->ProcessGroup;

    ASSERT((ProcessGroup != NULL) &&
           (Process->Identifiers.ProcessGroupId == ProcessGroup->Identifier) &&
           (Process->Identifiers.SessionId == ProcessGroup->SessionId));

    INSERT_BEFORE(&(Process->ProcessGroupListEntry),
                  &(ProcessGroup->ProcessListHead));

    Process->ProcessGroup = ProcessGroup;
    Process->Identifiers.ProcessGroupId = ProcessGroup->Identifier;
    Process->Identifiers.SessionId = ProcessGroup->SessionId;
    PspProcessGroupAddReference(ProcessGroup);
    KeReleaseQueuedLock(PsProcessGroupListLock);
    return;
}

VOID
PspRemoveProcessFromProcessGroup (
    PKPROCESS Process
    )

/*++

Routine Description:

    This routine removes a dying process from its process group, potentially
    orphaning its childrens' process groups. The process lock should not be
    held by the caller.

Arguments:

    Process - Supplies a pointer to the process that's being destroyed.

Return Value:

    None.

--*/

{

    PPROCESS_GROUP ProcessGroup;

    //
    // Acquire the process group list lock to prevent the parent or children
    // from changing process groups while the process leaves.
    //

    KeAcquireQueuedLock(PsProcessGroupListLock);
    ProcessGroup = Process->ProcessGroup;

    ASSERT(ProcessGroup != NULL);

    //
    // Remove the process from the group list.
    //

    if (Process->ProcessGroupListEntry.Next != NULL) {
        LIST_REMOVE(&(Process->ProcessGroupListEntry));
        Process->ProcessGroupListEntry.Next = NULL;
    }

    //
    // Fix up the process group as its process has left.
    //

    PspProcessGroupHandleLeavingProcess(Process, ProcessGroup, NULL);
    Process->ProcessGroup = NULL;
    KeReleaseQueuedLock(PsProcessGroupListLock);
    PspProcessGroupReleaseReference(ProcessGroup);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PspProcessGroupAddReference (
    PPROCESS_GROUP ProcessGroup
    )

/*++

Routine Description:

    This routine increments the reference count on a process group.

Arguments:

     ProcessGroup - Supplies a pointer to the process group whose reference
         count shall be incremented.

Return Value:

    None.

--*/

{

    ASSERT((ProcessGroup->ReferenceCount != 0) &&
           (ProcessGroup->ReferenceCount < PROCESS_GROUP_MAX_REFERENCE_COUNT));

    RtlAtomicAdd32(&(ProcessGroup->ReferenceCount), 1);
    return;
}

VOID
PspProcessGroupReleaseReference (
    PPROCESS_GROUP ProcessGroup
    )

/*++

Routine Description:

    This routine decrements the reference count on a process group. If it hits
    zero, the process group is destroyed.

Arguments:

     ProcessGroup - Supplies a pointer to the process group whose reference
         count shall be decremented.

Return Value:

    None.

--*/

{

    PKPROCESS KernelProcess;
    ULONG PreviousValue;

    ASSERT((ProcessGroup->ReferenceCount != 0) &&
           (ProcessGroup->ReferenceCount < PROCESS_GROUP_MAX_REFERENCE_COUNT));

    PreviousValue = RtlAtomicAdd32(&(ProcessGroup->ReferenceCount), -1);
    if (PreviousValue == 1) {

        ASSERT(LIST_EMPTY(&(ProcessGroup->ProcessListHead)) != FALSE);

        if (ProcessGroup->ListEntry.Next != NULL) {
            KeAcquireQueuedLock(PsProcessGroupListLock);
            if (ProcessGroup->ListEntry.Next != NULL) {
                LIST_REMOVE(&(ProcessGroup->ListEntry));
            }

            KeReleaseQueuedLock(PsProcessGroupListLock);
        }

        KernelProcess = PsGetKernelProcess();
        if (ProcessGroup->Identifier == KernelProcess->Identifiers.ProcessId) {
            MmFreeNonPagedPool(ProcessGroup);

        } else {
            MmFreePagedPool(ProcessGroup);
        }
    }

    return;
}

PPROCESS_GROUP
PspLookupProcessGroup (
    PROCESS_GROUP_ID ProcessGroupId
    )

/*++

Routine Description:

    This routine attempts to find the process group with the given identifier.
    This routine assumes the process list lock is already held.

Arguments:

    ProcessGroupId - Supplies the identifier of the process group.

Return Value:

    Returns a pointer to the process group on success.

    NULL if no such process group exists.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PPROCESS_GROUP ProcessGroup;

    ASSERT(KeIsQueuedLockHeld(PsProcessGroupListLock) != FALSE);

    CurrentEntry = PsProcessGroupList.Next;
    while (CurrentEntry != &PsProcessGroupList) {
        ProcessGroup = LIST_VALUE(CurrentEntry, PROCESS_GROUP, ListEntry);

        //
        // Ignore any process groups that no longer contain any processes. A
        // process group is only valid if it contains a process, but process
        // groups do not get removed from the global list until after the
        // reference count has gone to zero.
        //

        if ((ProcessGroup->Identifier == ProcessGroupId) &&
            (LIST_EMPTY(&(ProcessGroup->ProcessListHead)) == FALSE)) {

            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &PsProcessGroupList) {
        ProcessGroup = NULL;
    }

    return ProcessGroup;
}

VOID
PspSignalProcessGroup (
    PPROCESS_GROUP ProcessGroup,
    ULONG SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to every process in the given process group.
    This routine assumes that the process group list lock is already held.

Arguments:

    ProcessGroup - Supplies a pointer to the process group to signal.

    SignalNumber - Supplies the signal to send to each process in the group.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS Process;

    ASSERT(KeIsQueuedLockHeld(PsProcessGroupListLock) != FALSE);

    //
    // Loop through every process in the list.
    //

    CurrentEntry = ProcessGroup->ProcessListHead.Next;
    while (CurrentEntry != &(ProcessGroup->ProcessListHead)) {
        Process = LIST_VALUE(CurrentEntry, KPROCESS, ProcessGroupListEntry);
        PsSignalProcess(Process, SignalNumber, NULL);
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

VOID
PspProcessGroupHandleLeavingProcess (
    PKPROCESS Process,
    PPROCESS_GROUP OldGroup,
    PPROCESS_GROUP NewGroup
    )

/*++

Routine Description:

    This routine handles a process leaving the given old process group for the
    given (optional) new process group. It looks at all of the processes
    children and its parent to see if either the new or old group's outside
    ties have changed. The caller must not hold any process locks. This routine
    assumes the process group list lock is already head.

Arguments:

    Process - Supplies a pointer to the process that is leaving the old process
        group.

    OldGroup - Supplies a pointer to the process group the process is leaving.

    NewGroup - Supplies an optional pointer to the process group the process is
        joining.

Return Value:

    None.

--*/

{

    PLIST_ENTRY ChildEntry;
    PPROCESS_GROUP ChildGroup;
    PKPROCESS ChildProcess;
    PLIST_ENTRY CurrentEntry;
    BOOL DecrementOutsideParents;

    ASSERT(OldGroup != NewGroup);
    ASSERT(OldGroup != NULL);
    ASSERT(Process->ProcessGroup != NULL);
    ASSERT(KeIsQueuedLockHeld(PsProcessGroupListLock) != FALSE);

    //
    // If the process has no child and no parent, then there is nothing to do.
    //

    if ((LIST_EMPTY(&(Process->ChildListHead)) != FALSE) &&
        (Process->Parent == NULL)) {

        return;
    }

    //
    // Acquire the process' queued lock to safely iterate over the children.
    //

    KeAcquireQueuedLock(Process->QueuedLock);
    ChildEntry = Process->ChildListHead.Next;
    while (ChildEntry != &(Process->ChildListHead)) {
        ChildProcess = LIST_VALUE(ChildEntry, KPROCESS, SiblingListEntry);
        ChildEntry = ChildEntry->Next;

        //
        // A child may not have a process group. It may have already exited and
        // cleaned up its process group, but be awaiting destruction and
        // removal from its parent's child list. Or it may be a new child and
        // is waiting for the parent's process group to transition before
        // joining.
        //

        ChildGroup = ChildProcess->ProcessGroup;
        if (ChildGroup == NULL) {
            continue;
        }

        //
        // If the old parent group was the same as the child's and the new
        // parent group is not NULL, then the child's process group has a new
        // outside parent, as long as the new group is in the same session.
        //

        if (ChildGroup == OldGroup) {
            if ((NewGroup != NULL) &&
                (NewGroup->SessionId == ChildGroup->SessionId)) {

                ChildGroup->OutsideParents += 1;
            }

        //
        // Otherwise if the old parent group was in the same session, then it
        // was an outside parent. If the new parent is NULL or has the same
        // group as the child or is in a new session, then an outside parent
        // was lost.
        //

        } else if (ChildGroup->SessionId == OldGroup->SessionId) {
            if ((NewGroup == NULL) ||
                (ChildGroup == NewGroup) ||
                (ChildGroup->SessionId != NewGroup->SessionId)) {

                ChildGroup->OutsideParents -= 1;

                ASSERT(ChildGroup->OutsideParents <= 0x10000000);

                //
                // If the decremented outside parent count reached 0, then the
                // entire process group needs to be signaled if at least one
                // process is stopped. It is OK to temporarily release the
                // parent lock here and pick up from where the loop left off.
                // The child cannot go anywhere: a process does not remove
                // itself from it's sibling list until it is destroyed and a
                // process cannot be destroyed if it belongs to a process
                // group. And a parent does not remove its children until after
                // destroying its process group. So, as long as the global
                // process group lock is held, all processes in play here are
                // stuck.
                //

                if ((ChildGroup->OutsideParents == 0) &&
                    (PspIsOrphanedProcessGroupStopped(ChildGroup) != FALSE)) {

                    //
                    // The child is still in the group. The process list should
                    // not be empty.
                    //

                    ASSERT(LIST_EMPTY(&(ChildGroup->ProcessListHead)) == FALSE);

                    KeReleaseQueuedLock(Process->QueuedLock);
                    PspSignalProcessGroup(ChildGroup,
                                          SIGNAL_CONTROLLING_TERMINAL_CLOSED);

                    PspSignalProcessGroup(ChildGroup, SIGNAL_CONTINUE);
                    KeAcquireQueuedLock(Process->QueuedLock);

                    ASSERT(ChildProcess->ProcessGroup == ChildGroup);

                    CurrentEntry = ChildProcess->SiblingListEntry.Next;

                    ASSERT(CurrentEntry != NULL);
                }
            }
        }
    }

    //
    // If the process' parent belonged to a different group in the same
    // session, then the old process group has lost an outside tie.
    //
    // Note that the parent's identifiers are only still valid if its process
    // group pointer is not NULL. It may be that the parent is on its way out,
    // having left its process group, but not quite orphaned its children.
    //

    DecrementOutsideParents = FALSE;
    if ((Process->Parent != NULL) &&
        (Process->Parent->ProcessGroup != NULL) &&
        (Process->Parent->Identifiers.SessionId == OldGroup->SessionId) &&
        (Process->Parent->Identifiers.ProcessGroupId !=
         OldGroup->Identifier)) {

        ASSERT(Process->Parent->ProcessGroup->SessionId ==
               OldGroup->SessionId);

        ASSERT(Process->Parent->ProcessGroup != OldGroup);

        DecrementOutsideParents = TRUE;
    }

    KeReleaseQueuedLock(Process->QueuedLock);

    //
    // If an outside parent left the old group, decrement the count. If it goes
    // to zero and there is a stopped process in the group, signal the group.
    //

    if (DecrementOutsideParents != FALSE) {
        OldGroup->OutsideParents -= 1;

        ASSERT(OldGroup->OutsideParents <= 0x10000000);

        if ((OldGroup->OutsideParents == 0) &&
            (PspIsOrphanedProcessGroupStopped(OldGroup) != FALSE)) {

            PspSignalProcessGroup(OldGroup, SIGNAL_CONTROLLING_TERMINAL_CLOSED);
            PspSignalProcessGroup(OldGroup, SIGNAL_CONTINUE);
        }
    }

    return;
}

BOOL
PspIsOrphanedProcessGroupStopped (
    PPROCESS_GROUP ProcessGroup
    )

/*++

Routine Description:

    This routine determines if the given process group contains a stopped
    process. It assumes the process group list lock is held.

Arguments:

    ProcessGroup - Supplies a pointer to a process group.

Return Value:

    Returns TRUE if there is a process in the group that has stopped. Returns
    FALSE otherwise.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PKPROCESS Process;
    BOOL Stopped;

    ASSERT(KeIsQueuedLockHeld(PsProcessGroupListLock) != FALSE);
    ASSERT(ProcessGroup->OutsideParents == 0);

    //
    // Make sure that one of the processes is stopped.
    //

    Stopped = FALSE;
    CurrentEntry = ProcessGroup->ProcessListHead.Next;
    while (CurrentEntry != &(ProcessGroup->ProcessListHead)) {
        Process = LIST_VALUE(CurrentEntry, KPROCESS, ProcessGroupListEntry);
        if (Process->StoppedThreadCount != 0) {
            Stopped = TRUE;
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return Stopped;
}

