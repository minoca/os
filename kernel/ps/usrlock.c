/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usrlock.c

Abstract:

    This module implements kernel support for user mode locking.

Author:

    Evan Green 24-Apr-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USER_LOCK_TYPE {
    UserLockTypeInvalid,
    UserLockTypeProcess,
    UserLockTypeImageSection,
    UserLockTypeFileObject,
} USER_LOCK_TYPE, *PUSER_LOCK_TYPE;

/*++

Structure Description:

    This structure defines a user mode lock, which is basically just a wait
    queue that can be looked up.

Members:

    TreeNode - Stores the accounting structure for keeping the entry in a
        Red-Black tree.

    Object - Stores a pointer to the object this lock is tied to. This is a
        process for a process local lock, an image section for a lock in a
        private memory region, or a file object in a shared memory region.

    Offset - Stores either 1) the offset into the file object, 2) the offset
        into the image section, or 3) the user mode address in the process
        address space, depending on the type of lock.

    ReferenceCount - Stores the reference count of the object.

    Type - Stores the object type, used when trying to release the lock.

    WaitQueue - Stores the wait queue itself.

--*/

typedef struct _USER_LOCK {
    RED_BLACK_TREE_NODE TreeNode;
    PVOID Object;
    UINTN Offset;
    USER_LOCK_TYPE Type;
    WAIT_QUEUE WaitQueue;
} USER_LOCK, *PUSER_LOCK;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PspUserLockWait (
    PSYSTEM_CALL_USER_LOCK Parameters
    );

KSTATUS
PspUserLockWake (
    PSYSTEM_CALL_USER_LOCK Parameters
    );

KSTATUS
PspInitializeUserLock (
    PVOID Address,
    BOOL Private,
    PUSER_LOCK Lock
    );

VOID
PspReleaseUserLockObject (
    PUSER_LOCK Lock
    );

COMPARISON_RESULT
PspCompareUserLocks (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    );

//
// -------------------------------------------------------------------- Globals
//

PQUEUED_LOCK PsUserLockLock;
RED_BLACK_TREE PsUserLockTree;

//
// ------------------------------------------------------------------ Functions
//

INTN
PsSysUserLock (
    PVOID SystemCallParameter
    )

/*++

Routine Description:

    This routine implements the system call for user mode locking.

Arguments:

    SystemCallParameter - Supplies a pointer to the parameters supplied with
        the system call. This structure will be a stack-local copy of the
        actual parameters passed from user-mode.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

{

    ULONG Operation;
    PSYSTEM_CALL_USER_LOCK Parameters;
    KSTATUS Status;

    Parameters = SystemCallParameter;
    Operation = Parameters->Operation & USER_LOCK_OPERATION_MASK;
    switch (Operation) {
    case UserLockWait:
        Status = PspUserLockWait(Parameters);
        break;

    case UserLockWake:
        Status = PspUserLockWake(Parameters);
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    return Status;
}

VOID
PspInitializeUserLocking (
    VOID
    )

/*++

Routine Description:

    This routine sets up the user locking subsystem.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PsUserLockLock = KeCreateQueuedLock();

    ASSERT(PsUserLockLock != NULL);

    RtlRedBlackTreeInitialize(&PsUserLockTree, 0, PspCompareUserLocks);
    return;
}

KSTATUS
PspUserLockWake (
    PSYSTEM_CALL_USER_LOCK Parameters
    )

/*++

Routine Description:

    This routine wakes up those blocked on the given user mode address.

Arguments:

    Parameters - Supplies a pointer to the wait parameters.

Return Value:

    Status code.

--*/

{

    PUSER_LOCK FoundLock;
    PRED_BLACK_TREE_NODE FoundNode;
    USER_LOCK Lock;
    BOOL Private;
    ULONG ProcessesReleased;
    KSTATUS Status;

    Private = FALSE;
    if ((Parameters->Operation & USER_LOCK_PRIVATE) != 0) {
        Private = TRUE;
    }

    Status = PspInitializeUserLock(Parameters->Address, Private, &Lock);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // Release the specified number of processes.
    //

    ProcessesReleased = 0;
    KeAcquireQueuedLock(PsUserLockLock);
    while (Parameters->Value != 0) {
        FoundNode = RtlRedBlackTreeSearch(&PsUserLockTree, &(Lock.TreeNode));
        if (FoundNode == NULL) {
            break;
        }

        //
        // Remove it from the tree first. The locks are stack allocated, so as
        // soon as the thread is made ready the memory could go invalid.
        //

        FoundLock = RED_BLACK_TREE_VALUE(FoundNode, USER_LOCK, TreeNode);
        RtlRedBlackTreeRemove(&PsUserLockTree, FoundNode);
        ObSignalQueue(&(FoundLock->WaitQueue), SignalOptionSignalAll);

        //
        // The object can go away as soon as it's known to be removed from the
        // tree. Make sure this thread is done touching the object before
        // indicating to the woken thread that it can destroy this memory.
        //

        FoundNode->Parent = NULL;
        ProcessesReleased += 1;
        if (Parameters->Value != MAX_ULONG) {
            Parameters->Value -= 1;
        }
    }

    KeReleaseQueuedLock(PsUserLockLock);
    PspReleaseUserLockObject(&Lock);
    Parameters->Value = ProcessesReleased;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PspUserLockWait (
    PSYSTEM_CALL_USER_LOCK Parameters
    )

/*++

Routine Description:

    This routine performs a wait on the user lock.

Arguments:

    Parameters - Supplies a pointer to the wait parameters.

Return Value:

    Status code.

--*/

{

    ULONGLONG ElapsedTimeInMilliseconds;
    ULONGLONG EndTime;
    ULONGLONG Frequency;
    USER_LOCK Lock;
    BOOL Private;
    ULONGLONG StartTime;
    KSTATUS Status;
    ULONG UserValue;

    Private = FALSE;
    if ((Parameters->Operation & USER_LOCK_PRIVATE) != 0) {
        Private = TRUE;
    }

    Status = PspInitializeUserLock(Parameters->Address, Private, &Lock);
    if (!KSUCCESS(Status)) {
        return Status;
    }

    ObInitializeWaitQueue(&(Lock.WaitQueue), NotSignaled);
    KeAcquireQueuedLock(PsUserLockLock);

    //
    // If the read failed, then bail out.
    //

    if (MmUserRead32(Parameters->Address, &UserValue) == FALSE) {
        Status = STATUS_ACCESS_VIOLATION;

    } else {

        //
        // If the value changed between the time user mode started to ask for
        // a wait and now, bail out.
        //

        if (UserValue != Parameters->Value) {
            Status = STATUS_OPERATION_WOULD_BLOCK;

        //
        // The value is the same, commit to going down.
        //

        } else {
            Status = STATUS_SUCCESS;
            RtlRedBlackTreeInsert(&PsUserLockTree, &(Lock.TreeNode));
        }
    }

    KeReleaseQueuedLock(PsUserLockLock);
    if (!KSUCCESS(Status)) {
        goto UserLockWaitEnd;
    }

    //
    // Wait for somebody to wake this thread (or a signal, or a timeout).
    //

    ASSERT(SYS_WAIT_TIME_INDEFINITE == WAIT_TIME_INDEFINITE);

    StartTime = 0;
    if (Parameters->TimeoutInMilliseconds != SYS_WAIT_TIME_INDEFINITE) {
        StartTime = KeGetRecentTimeCounter();
    }

    Status = ObWaitOnQueue(&(Lock.WaitQueue),
                           WAIT_FLAG_INTERRUPTIBLE,
                           Parameters->TimeoutInMilliseconds);

    //
    // If a user lock wait is interrupted by a signal, allow it to restart
    // after the signal is applied if the handler allows restarts. Update the
    // timeout, so the next round doesn't wait too long.
    //

    if (Status == STATUS_INTERRUPTED) {
        if (Parameters->TimeoutInMilliseconds != SYS_WAIT_TIME_INDEFINITE) {
            EndTime = KeGetRecentTimeCounter();
            Frequency = HlQueryTimeCounterFrequency();
            ElapsedTimeInMilliseconds = ((EndTime - StartTime) *
                                         MILLISECONDS_PER_SECOND) /
                                        Frequency;

            if (ElapsedTimeInMilliseconds < Parameters->TimeoutInMilliseconds) {
                Parameters->TimeoutInMilliseconds -= ElapsedTimeInMilliseconds;

            } else {
                Parameters->TimeoutInMilliseconds = 0;
            }
        }

        Status = STATUS_RESTART_AFTER_SIGNAL;
    }

    //
    // Remove the object from the tree, racing with the parent who may
    // have already done it to save the extra lock acquire.
    //

    if (Lock.TreeNode.Parent != NULL) {
        KeAcquireQueuedLock(PsUserLockLock);
        if (Lock.TreeNode.Parent != NULL) {
            RtlRedBlackTreeRemove(&PsUserLockTree, &(Lock.TreeNode));
            Lock.TreeNode.Parent = NULL;
        }

        KeReleaseQueuedLock(PsUserLockLock);
    }

UserLockWaitEnd:
    PspReleaseUserLockObject(&Lock);
    return Status;
}

KSTATUS
PspInitializeUserLock (
    PVOID Address,
    BOOL Private,
    PUSER_LOCK Lock
    )

/*++

Routine Description:

    This routine initializes the user lock state.

Arguments:

    Address - Supplies a pointer to the usermode address to contend on.

    Private - Supplies a boolean indicating whether or not the lock is
        private to the process (TRUE) or potentially shared between multiple
        processes (FALSE).

    Lock - Supplies a pointer where the initialized lock structure will be
        returned on success.

Return Value:

    Status code.

--*/

{

    BOOL Shared;

    if (Private != FALSE) {
        Lock->Object = PsGetCurrentProcess();
        Lock->Offset = (UINTN)Address;
        Lock->Type = UserLockTypeProcess;

    } else {
        Lock->Object = MmGetObjectForAddress(Address, &(Lock->Offset), &Shared);
        if (Lock->Object == NULL) {
            return STATUS_ACCESS_VIOLATION;
        }

        if (Shared != FALSE) {
            Lock->Type = UserLockTypeFileObject;

        } else {
            Lock->Type = UserLockTypeImageSection;
        }
    }

    return STATUS_SUCCESS;
}

VOID
PspReleaseUserLockObject (
    PUSER_LOCK Lock
    )

/*++

Routine Description:

    This routine releases the reference on a user lock backing object, which
    is either a process, image section, or file object.

Arguments:

    Lock - Supplies a pointer to the lock being torn down.

Return Value:

    None.

--*/

{

    BOOL Shared;

    Shared = FALSE;
    switch (Lock->Type) {
    case UserLockTypeProcess:
        break;

    case UserLockTypeFileObject:
        Shared = TRUE;

        //
        // Fall through.
        //

    case UserLockTypeImageSection:
        MmReleaseObjectReference(Lock->Object, Shared);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

COMPARISON_RESULT
PspCompareUserLocks (
    PRED_BLACK_TREE Tree,
    PRED_BLACK_TREE_NODE FirstNode,
    PRED_BLACK_TREE_NODE SecondNode
    )

/*++

Routine Description:

    This routine compares two Red-Black tree nodes that in this case are user
    mode lock objects.

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

    PUSER_LOCK FirstLock;
    PUSER_LOCK SecondLock;

    //
    // Compare raw pointers to the objects first.
    //

    FirstLock = RED_BLACK_TREE_VALUE(FirstNode, USER_LOCK, TreeNode);
    SecondLock = RED_BLACK_TREE_VALUE(SecondNode, USER_LOCK, TreeNode);
    if (FirstLock->Object < SecondLock->Object) {
        return ComparisonResultAscending;

    } else if (FirstLock->Object > SecondLock->Object) {
        return ComparisonResultDescending;

    } else if (FirstLock->Offset > SecondLock->Offset) {
        return ComparisonResultDescending;

    } else if (FirstLock->Offset < SecondLock->Offset) {
        return ComparisonResultAscending;
    }

    return ComparisonResultSame;
}

