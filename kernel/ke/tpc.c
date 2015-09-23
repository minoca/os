/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    tpc.c

Abstract:

    This module implements support for Thread Procedure Calls.

Author:

    Chris Stevens 14-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TPC_ALLOCATION_TAG 0x21637054 // '!cpT'

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
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
KeInitializeTpc (
    PTPC Tpc,
    PTPC_ROUTINE TpcRoutine,
    PVOID UserData
    )

/*++

Routine Description:

    This routine initializes the given TPC with the routine and context data.

Arguments:

    Tpc - Supplies a pointer to the TPC to be initialized.

    TpcRoutine - Supplies a pointer to the routine to call when the TPC fires.

    UserData - Supplies a context pointer that can be passed to the routine via
        the TPC when it is called.

Return Value:

    None.

--*/

{

    RtlZeroMemory(Tpc, sizeof(TPC));
    Tpc->TpcRoutine = TpcRoutine;
    Tpc->UserData = UserData;
    return;
}

KERNEL_API
PTPC
KeCreateTpc (
    PTPC_ROUTINE TpcRoutine,
    PVOID UserData
    )

/*++

Routine Description:

    This routine creates a new TPC with the given routine and context data.

Arguments:

    TpcRoutine - Supplies a pointer to the routine to call when the TPC fires.

    UserData - Supplies a context pointer that can be passed to the routine via
        the TPC when it is called.

Return Value:

    Returns a pointer to the allocated and initialized (but not queued) TPC.

--*/

{

    PTPC Tpc;

    Tpc = MmAllocateNonPagedPool(sizeof(TPC), TPC_ALLOCATION_TAG);
    if (Tpc == NULL) {
        return NULL;
    }

    KeInitializeTpc(Tpc, TpcRoutine, UserData);
    return Tpc;
}

KERNEL_API
VOID
KeDestroyTpc (
    PTPC Tpc
    )

/*++

Routine Description:

    This routine destroys a TPC. It will flush the TPC if it is queued, and
    wait for it to finish if it is running. This routine must be called from
    low level.

Arguments:

    Tpc - Supplies a pointer to the TPC to destroy.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Flush the TPC and then destroy it.
    //

    KeFlushTpc(Tpc);
    MmFreeNonPagedPool(Tpc);
    return;
}

KERNEL_API
VOID
KeQueueTpc (
    PTPC Tpc,
    PKTHREAD Thread
    )

/*++

Routine Description:

    This routine queues the given TPC. If the caller does not provide a thread,
    then it is expected that the TPC was previously bound to another thread.

Arguments:

    Tpc - Supplies a pointer to the TPC to be queued on the given thread.

    Thread - Supplies an optional pointer to thread on which the TPC should be
        queued.

Return Value:

    None.

--*/

{

    PKTHREAD CurrentThread;
    BOOL Enabled;
    ULONG OldCount;
    BOOL Outstanding;
    PTHREAD_TPC_CONTEXT TpcContext;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    if (Tpc->ListEntry.Next != NULL) {
        KeCrashSystem(CRASH_TPC_FAILURE,
                      TpcCrashReasonDoubleQueueTpc,
                      (UINTN)Tpc,
                      0,
                      0);
    }

    if (Tpc->TpcRoutine == NULL) {
        KeCrashSystem(CRASH_TPC_FAILURE,
                      TpcCrashReasonNullRoutine,
                      (UINTN)Tpc,
                      0,
                      0);
    }

    if ((Tpc->Thread == NULL) && (Thread == NULL)) {
        KeCrashSystem(CRASH_TPC_FAILURE,
                      TpcCrashReasonNoThread,
                      (UINTN)Tpc,
                      0,
                      0);
    }

    //
    // If the TPC has already been tied to a thread, that thread marked it as
    // outstanding. Record this so the outstanding TPC count can be adjusted
    // appropriately.
    //

    Outstanding = FALSE;
    if (Tpc->Thread != NULL) {
        if ((Thread != NULL) && (Tpc->Thread != Thread)) {
            KeCrashSystem(CRASH_TPC_FAILURE,
                          TpcCrashReasonBadThread,
                          (UINTN)Tpc,
                          (UINTN)Thread,
                          0);
        }

        Outstanding = TRUE;
        Thread = Tpc->Thread;

    } else {
        Tpc->Thread = Thread;
    }

    TpcContext = &(Thread->TpcContext);

    ASSERT((Outstanding == FALSE) || (TpcContext->OutstandingCount != 0));

    //
    // If this TPC is being queued on the current thread and said thread can
    // run it immediately, go for it!
    //

    CurrentThread = KeGetCurrentThread();
    if ((Thread == CurrentThread) && (KeGetRunLevel() < RunLevelDispatch)) {
        Tpc->TpcRoutine(Tpc);
        Tpc->Thread = NULL;
        if (Outstanding != FALSE) {
            OldCount = RtlAtomicAdd32(&(TpcContext->OutstandingCount), -1);

            ASSERT((OldCount != 0) && (OldCount < 0x10000000));
        }

    //
    // Otherwise, queue it on the thread and, if it's not being queued on the
    // current thread, try to wake the target thread.
    //

    } else {
        if (Outstanding == FALSE) {
            OldCount = RtlAtomicAdd32(&(TpcContext->OutstandingCount), 1);

            ASSERT(OldCount < 0x10000000);
        }

        //
        // Take a reference on the thread in case it needs to be woken after
        // the TPC is inserted. As soon as the TPC is inserted, the thread may
        // run, exit, and be destroyed. This reference is necessary.
        //

        if (Thread != CurrentThread) {
            ObAddReference(Thread);
        }

        Enabled = ArDisableInterrupts();
        KeAcquireSpinLock(&(TpcContext->Lock));
        INSERT_BEFORE(&(Tpc->ListEntry), &(TpcContext->ListHead));
        KeReleaseSpinLock(&(TpcContext->Lock));
        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        if (Thread != CurrentThread) {
            ObWakeBlockedThread(Thread, FALSE);
            ObReleaseReference(Thread);
        }
    }

    return;
}

KERNEL_API
VOID
KePrepareTpc (
    PTPC Tpc,
    PKTHREAD Thread,
    BOOL Prepare
    )

/*++

Routine Description:

    This routine prepares a TPC to run on the given thread at some point in the
    future.

Arguments:

    Tpc - Supplies a pointer to the TPC that is to be pended for later use.

    Thread - Supplies an optional pointer to the target thread. If NULL is
        supplied, the TPC will be prepared to run on the current thread.

    Prepare - Supplies a boolean indicating whether the TPC should be prepared
        to use the thread or disassociated with the thread.

Return Value:

    None.

--*/

{

    ULONG OldCount;
    PTHREAD_TPC_CONTEXT TpcContext;

    if (Prepare != FALSE) {

        ASSERT(Tpc->Thread == NULL);

        Tpc->Thread = Thread;
        if (Tpc->Thread == NULL) {
            Tpc->Thread = KeGetCurrentThread();
        }

        TpcContext = &(Tpc->Thread->TpcContext);
        OldCount = RtlAtomicAdd32(&(TpcContext->OutstandingCount), 1);

        ASSERT(OldCount < 0x10000000);

    } else {
        TpcContext = &(Tpc->Thread->TpcContext);
        Tpc->Thread = NULL;
        OldCount = RtlAtomicAdd32(&(TpcContext->OutstandingCount), -1);

        ASSERT((OldCount != 0) && (OldCount < 0x10000000));
    }

    return;
}

KERNEL_API
VOID
KeFlushTpc (
    PTPC Tpc
    )

/*++

Routine Description:

    This routine does not return until the given TPC is out of the system. This
    means that the TPC is neither queued nor running. This routine can only be
    called below dispatch level.

Arguments:

    Tpc - Supplies a pointer to the TPC to flush.

Return Value:

    Status code.

--*/

{

    volatile PKTHREAD *Thread;

    ASSERT(KeGetRunLevel() < RunLevelDispatch);

    Thread = &(Tpc->Thread);
    while (*Thread != NULL) {
        KeYield();
    }

    return;
}

VOID
KeFlushTpcs (
    VOID
    )

/*++

Routine Description:

    This routine flushes all of the current thread's outstanding TPC's. It does
    not return until they have been executed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PKTHREAD Thread;
    PTHREAD_TPC_CONTEXT TpcContext;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Thread = KeGetCurrentThread();
    TpcContext = &(Thread->TpcContext);

    //
    // Wait until all the outstanding TPCs have executed.
    //

    while (TpcContext->OutstandingCount != 0) {
        KeYield();
    }

    //
    // There should be no new expected TPCs and the TPC list should be empty.
    //

    ASSERT(TpcContext->OutstandingCount == 0);
    ASSERT(LIST_EMPTY(&(TpcContext->ListHead)) != FALSE);

    return;
}

VOID
KepExecutePendingTpcs (
    VOID
    )

/*++

Routine Description:

    This routine executes the current thread's pending TPCs. It must be called
    with interrupts disabled, but will temporarily enable interrupts while
    executing the TPCs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Count;
    PLIST_ENTRY CurrentEntry;
    LIST_ENTRY LocalList;
    ULONG OldCount;
    PKTHREAD Thread;
    PTPC Tpc;
    PTHREAD_TPC_CONTEXT TpcContext;

    ASSERT(KeGetRunLevel() == RunLevelTpc);

    Thread = KeGetCurrentThread();
    TpcContext = &(Thread->TpcContext);

    //
    // Return immediately if the list is empty.
    //

    if (LIST_EMPTY(&(TpcContext->ListHead)) != FALSE) {
        return;
    }

    INITIALIZE_LIST_HEAD(&LocalList);

    //
    // Acquire the lock long enough to move the list off of the TPC context
    // list and mark that each entry is no longer queued on said list. This
    // routine should only be called while dispatching software interrupts with
    // hardware interrupts disabled. The processor is effectivly at high run
    // level. Given that this is the only place that entries are removed from
    // the list and the list was seen as not empty above, it is safe to assume
    // that the list is still not empty.
    //

    KeAcquireSpinLock(&(TpcContext->Lock));

    ASSERT(LIST_EMPTY(&(TpcContext->ListHead)) == FALSE);

    MOVE_LIST(&(TpcContext->ListHead), &LocalList);
    INITIALIZE_LIST_HEAD(&(TpcContext->ListHead));
    KeReleaseSpinLock(&(TpcContext->Lock));

    //
    // Now execute all pending TPCs with interrupts enabled.
    //

    ArEnableInterrupts();
    Count = 0;
    while (LIST_EMPTY(&LocalList) == FALSE) {
        CurrentEntry = LocalList.Next;
        Tpc = LIST_VALUE(CurrentEntry, TPC, ListEntry);

        //
        // Pull the TPC off the local list and set it's next pointer to NULL to
        // indicate that it is not queued.
        //

        LIST_REMOVE(CurrentEntry);
        Tpc->ListEntry.Next = NULL;

        //
        // Call the TPC routine and that disassociated it with the thread.
        //

        Tpc->TpcRoutine(Tpc);
        Tpc->Thread = NULL;
        Count += 1;
    }

    //
    // Now that all the TPCs are executed, decrement the outstanding count in
    // one go.
    //

    OldCount = RtlAtomicAdd32(&(TpcContext->OutstandingCount), -Count);

    ASSERT((OldCount != 0) && (OldCount < 0x10000000));

    ArDisableInterrupts();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

