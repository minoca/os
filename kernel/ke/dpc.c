/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dpc.c

Abstract:

    This module implements support for Deferred Procedure Calls.

Author:

    Evan Green 23-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "kep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DPC_ALLOCATION_TAG 0x21637044 // '!cpD'

//
// Define the default initial entropy mask.
//

#define DPC_ENTROPY_MASK_DEFAULT 0x0000001F

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KepQueueDpc (
    PDPC Dpc,
    PPROCESSOR_BLOCK Processor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define how often a DPC is timestamped to add entropy to the system. If all
// the bits in the mask are zero of the processor's DPC count, then the DPC
// is timestamped and entropy is added. This is a relatively heavy operation,
// so it shouldn't occur too often.
//

UINTN KeDpcEntropyMask = DPC_ENTROPY_MASK_DEFAULT;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
PDPC
KeCreateDpc (
    PDPC_ROUTINE DpcRoutine,
    PVOID UserData
    )

/*++

Routine Description:

    This routine creates a new DPC with the given routine and context data.

Arguments:

    DpcRoutine - Supplies a pointer to the routine to call when the DPC fires.

    UserData - Supplies a context pointer that can be passed to the routine via
        the DPC when it is called.

Return Value:

    Returns a pointer to the allocated and initialized (but not queued) DPC.

--*/

{

    PDPC Dpc;

    Dpc = MmAllocateNonPagedPool(sizeof(DPC), DPC_ALLOCATION_TAG);
    if (Dpc == NULL) {
        return NULL;
    }

    RtlZeroMemory(Dpc, sizeof(DPC));
    Dpc->DpcRoutine = DpcRoutine;
    Dpc->UserData = UserData;
    return Dpc;
}

KERNEL_API
VOID
KeDestroyDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine destroys a DPC. It will cancel the DPC if it is queued, and
    wait for it to finish if it is running. This routine must be called from
    low level.

Arguments:

    Dpc - Supplies a pointer to the DPC to destroy.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (!KSUCCESS(KeCancelDpc(Dpc))) {
        KeFlushDpc(Dpc);
    }

    MmFreeNonPagedPool(Dpc);
    return;
}

KERNEL_API
VOID
KeQueueDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine queues a DPC on the current processor.

Arguments:

    Dpc - Supplies a pointer to the DPC to queue.

Return Value:

    None.

--*/

{

    KepQueueDpc(Dpc, NULL);
    return;
}

KERNEL_API
VOID
KeQueueDpcOnProcessor (
    PDPC Dpc,
    ULONG ProcessorNumber
    )

/*++

Routine Description:

    This routine queues a DPC on the given processor.

Arguments:

    Dpc - Supplies a pointer to the DPC to queue.

    ProcessorNumber - Supplies the processor number of the processor to queue
        the DPC on.

Return Value:

    None.

--*/

{

    ASSERT(ProcessorNumber < KeGetActiveProcessorCount());

    KepQueueDpc(Dpc, KeProcessorBlocks[ProcessorNumber]);
    return;
}

KERNEL_API
KSTATUS
KeCancelDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine attempts to cancel a DPC that has been queued.

Arguments:

    Dpc - Supplies a pointer to the DPC to cancel.

Return Value:

    STATUS_SUCCESS if the DPC was successfully pulled out of a queue.

    STATUS_TOO_LATE if the DPC has already started running.

--*/

{

    BOOL Enabled;
    ULONG Processor;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorCount;
    KSTATUS Status;

    ProcessorCount = KeGetActiveProcessorCount();
    Status = STATUS_TOO_LATE;

    //
    // Attempt to chase the DPC around whichever processor it's running on as
    // long as it's queued on a processor's list.
    //

    while ((Dpc->UseCount != 0) &&
           ((Dpc->Flags & DPC_FLAG_QUEUED_ON_PROCESSOR) != 0)) {

        Processor = Dpc->Processor;
        if (Processor >= ProcessorCount) {
            KeCrashSystem(CRASH_DPC_FAILURE,
                          DpcCrashReasonCorrupt,
                          (UINTN)Dpc,
                          Processor,
                          ProcessorCount);
        }

        //
        // Grab the DPC lock for the processor the DPC is on. If the DPC is
        // still active for that same processor and is still on the queue, pull
        // it off the queue. It may have been pulled off the processor's DPC
        // list and be on a local list for execution. If that's the case, then
        // it is too late to cancel the DPC.
        //

        ProcessorBlock = KeProcessorBlocks[Processor];
        Enabled = ArDisableInterrupts();
        KeAcquireSpinLock(&(ProcessorBlock->DpcLock));
        if ((Dpc->UseCount != 0) &&
            (Dpc->Processor == Processor) &&
            ((Dpc->Flags & DPC_FLAG_QUEUED_ON_PROCESSOR) != 0)) {

            LIST_REMOVE(&(Dpc->ListEntry));
            Dpc->Flags &= ~DPC_FLAG_QUEUED_ON_PROCESSOR;
            Dpc->ListEntry.Next = NULL;
            Status = STATUS_SUCCESS;
        }

        KeReleaseSpinLock(&(ProcessorBlock->DpcLock));
        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        //
        // If the DPC was successfully pulled off the queue, decrement its use
        // count and return successfully.
        //

        if (KSUCCESS(Status)) {
            RtlAtomicAdd32(&(Dpc->UseCount), -1);
            break;
        }
    }

    return Status;
}

KERNEL_API
VOID
KeFlushDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine does not return until the given DPC is out of the system. This
    means that the DPC is neither queued nor running. It's worth noting that
    this routine busy spins at dispatch level, and should therefore be used
    only sparingly. This routine can only be called from low level.

Arguments:

    Dpc - Supplies a pointer to the DPC to wait for.

Return Value:

    None.

--*/

{

    //
    // If the runlevel were dispatch or higher and the DPC was queued on this
    // processor, it would never run. It's OK if the runlevel is dispatch and
    // the DPC is queued on another processor.
    //

    ASSERT((KeGetRunLevel() == RunLevelLow) ||
           ((KeGetRunLevel() == RunLevelDispatch) &&
            (Dpc->Processor != KeGetCurrentProcessorNumber())));

    while (Dpc->UseCount != 0) {
        ArProcessorYield();
    }

    return;
}

VOID
KepExecutePendingDpcs (
    VOID
    )

/*++

Routine Description:

    This routine executes any pending DPCs on the current processor. This
    routine should only be executed internally by the scheduler. It must be
    called at dispatch level. Interrupts must be disabled upon entry, but will
    be enabled on exit.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PDPC Dpc;
    LIST_ENTRY LocalList;
    CYCLE_ACCOUNT PreviousPeriod;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT(KeGetRunLevel() == RunLevelDispatch);

    ProcessorBlock = KeGetCurrentProcessorBlock();

    //
    // Return immediately if the list is empty.
    //

    if (LIST_EMPTY(&(ProcessorBlock->DpcList)) != FALSE) {
        ArEnableInterrupts();
        return;
    }

    PreviousPeriod = KeBeginCycleAccounting(CycleAccountInterrupt);
    INITIALIZE_LIST_HEAD(&LocalList);

    //
    // Acquire the lock long enough to move the list off of the processor block
    // list and mark that each entry is no longer queued on said list.
    //

    KeAcquireSpinLock(&(ProcessorBlock->DpcLock));
    if (LIST_EMPTY(&(ProcessorBlock->DpcList)) == FALSE) {
        MOVE_LIST(&(ProcessorBlock->DpcList), &LocalList);
        INITIALIZE_LIST_HEAD(&(ProcessorBlock->DpcList));
        CurrentEntry = LocalList.Next;
        while (CurrentEntry != &LocalList) {
            Dpc = LIST_VALUE(CurrentEntry, DPC, ListEntry);
            Dpc->Flags &= ~DPC_FLAG_QUEUED_ON_PROCESSOR;
            CurrentEntry = CurrentEntry->Next;
        }
    }

    KeReleaseSpinLock(&(ProcessorBlock->DpcLock));
    ArEnableInterrupts();

    //
    // Set the clock to periodic mode before executing the DPCs. A DPC may
    // depend on the clock making forward progress (e.g. a timeout may be
    // implemented using recent snaps of the time counter rather than querying
    // the hardware directly).
    //

    if (LIST_EMPTY(&LocalList) == FALSE) {
        KepSetClockToPeriodic(ProcessorBlock);
    }

    //
    // Now execute all pending DPCs.
    //

    ASSERT(ProcessorBlock->DpcInProgress == NULL);

    while (LIST_EMPTY(&LocalList) == FALSE) {
        CurrentEntry = LocalList.Next;
        Dpc = LIST_VALUE(CurrentEntry, DPC, ListEntry);
        ProcessorBlock->DpcInProgress = Dpc;

        //
        // Pull the DPC off the local list and set it's next pointer to NULL to
        // indicate that it is not queued.
        //

        LIST_REMOVE(CurrentEntry);
        Dpc->ListEntry.Next = NULL;

        //
        // Call the DPC routine.
        //

        Dpc->DpcRoutine(Dpc);

        //
        // Decrement the use count to indicate that the system (or at least
        // this processor on this iteration) is done looking at this thing.
        //

        RtlAtomicAdd32(&(Dpc->UseCount), -1);

        //
        // Add one to the DPC counter, and potentially add entropy.
        //

        ProcessorBlock->DpcCount += 1;
        if ((ProcessorBlock->DpcCount & KeDpcEntropyMask) == 0) {
            KepAddTimePointEntropy();
        }
    }

    ProcessorBlock->DpcInProgress = NULL;
    KeBeginCycleAccounting(PreviousPeriod);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepQueueDpc (
    PDPC Dpc,
    PPROCESSOR_BLOCK Processor
    )

/*++

Routine Description:

    This routine queues a DPC on the given processor. If the DPC is being
    queued on the current processor, and the current runlevel is less than or
    equal to dispatch, then the DPC routine is run immediately. This is useful
    for things like timer expiration.

Arguments:

    Dpc - Supplies a pointer to the DPC to queue.

    Processor - Supplies a pointer to the processor block.

Return Value:

    None.

--*/

{

    PPROCESSOR_BLOCK CurrentProcessor;
    BOOL Enabled;
    RUNLEVEL OldRunLevel;

    Enabled = ArDisableInterrupts();
    CurrentProcessor = KeGetCurrentProcessorBlock();
    if (Processor == NULL) {
        Processor = CurrentProcessor;
    }

    if (Dpc->ListEntry.Next != NULL) {
        KeCrashSystem(CRASH_DPC_FAILURE,
                      DpcCrashReasonDoubleQueueDpc,
                      (UINTN)Dpc,
                      0,
                      0);
    }

    ASSERT((Dpc->Flags & DPC_FLAG_QUEUED_ON_PROCESSOR) == 0);

    if (Dpc->DpcRoutine == NULL) {
        KeCrashSystem(CRASH_DPC_FAILURE,
                      DpcCrashReasonNullRoutine,
                      (UINTN)Dpc,
                      0,
                      0);
    }

    //
    // Run the DPC directly if it's on the current processor and the runlevel
    // is at or below dispatch.
    //

    if ((Processor == CurrentProcessor) &&
        (Processor->RunLevel <= RunLevelDispatch) &&
        (Enabled != FALSE)) {

        RtlAtomicAdd32(&(Dpc->UseCount), 1);
        OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        Dpc->DpcRoutine(Dpc);
        KeLowerRunLevel(OldRunLevel);
        RtlAtomicAdd32(&(Dpc->UseCount), -1);

    //
    // Really queue the DPC on the destination processor.
    //

    } else {
        RtlAtomicAdd32(&(Dpc->UseCount), 1);
        Dpc->Processor = Processor->ProcessorNumber;
        KeAcquireSpinLock(&(Processor->DpcLock));
        INSERT_BEFORE(&(Dpc->ListEntry), &(Processor->DpcList));
        Dpc->Flags |= DPC_FLAG_QUEUED_ON_PROCESSOR;
        KeReleaseSpinLock(&(Processor->DpcLock));
        Processor->PendingDispatchInterrupt = TRUE;

        //
        // Raise to dispatch before enabling interrupts to ensure a processor
        // switch doesn't happen before poking the clock.
        //

        OldRunLevel = RunLevelCount;
        if (CurrentProcessor->RunLevel < RunLevelDispatch) {
            OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
        }

        if (Enabled != FALSE) {
            ArEnableInterrupts();
        }

        //
        // Ensure the processor is awake to go handle a DPC.
        //

        if (Processor != CurrentProcessor) {
            KepSetClockToPeriodic(Processor);
        }

        if (OldRunLevel != RunLevelCount) {
            KeLowerRunLevel(OldRunLevel);
        }
    }

    return;
}

