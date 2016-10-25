/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intlevel.c

Abstract:

    This module implements interrupt entry and exit, as well as hardware layer
    run level management.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlpLowerRunLevel (
    RUNLEVEL RunLevel,
    PTRAP_FRAME TrapFrame
    );

VOID
HlpInterruptReplay (
    PINTERRUPT_CONTROLLER Controller,
    ULONG Vector,
    ULONG MagicCandy
    );

INTERRUPT_STATUS
HlpRunIsr (
    PTRAP_FRAME TrapFrame,
    PPROCESSOR_BLOCK Processor,
    ULONG Vector,
    PINTERRUPT_CONTROLLER Controller
    );

VOID
HlpDeferInterrupt (
    PKINTERRUPT Interrupt,
    PINTERRUPT_CONTROLLER Controller
    );

VOID
HlpQueueInterruptDpc (
    PKINTERRUPT Interrupt,
    ULONG QueueFlags
    );

INTERRUPT_STATUS
HlpContinueIsr (
    PKINTERRUPT Interrupt
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
HlContinueInterrupt (
    HANDLE InterruptHandle,
    INTERRUPT_STATUS Status
    )

/*++

Routine Description:

    This routine continues an interrupt that was previously deferred at low
    level.

Arguments:

    InterruptHandle - Supplies the connected interrupt handle.

    Status - Supplies the final interrupt status that would have been returned
        had the interrupt not been deferred. This must either be claimed or
        not claimed.

Return Value:

    None.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    PKINTERRUPT Interrupt;

    Interrupt = InterruptHandle;

    ASSERT(Status != InterruptStatusDefer);

    //
    // If this is a deferred interrupt, continue calling ISRs.
    //

    if ((Status != InterruptStatusClaimed) ||
        (Interrupt->Mode != InterruptModeLevel)) {

        Status = HlpContinueIsr(Interrupt);
    }

    //
    // Unmask the line if this interrupt is complete.
    //

    if (Status != InterruptStatusDefer) {
        Controller = Interrupt->Controller;
        Controller->FunctionTable.MaskLine(Controller->PrivateContext,
                                           &(Interrupt->Line),
                                           TRUE);
    }

    return;
}

KERNEL_API
INTERRUPT_STATUS
HlSecondaryInterruptControllerService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements a standard interrupt service routine for an
    interrupt that is wired to another interrupt controller. It will call out
    to determine what fired, and begin a new secondary interrupt.

Arguments:

    Context - Supplies the context, which must be a pointer to the secondary
        interrupt controller that needs service.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    INTERRUPT_CAUSE Cause;
    PINTERRUPT_CONTROLLER Controller;
    INTERRUPT_STATUS InterruptStatus;
    ULONG MagicCandy;
    PPROCESSOR_BLOCK Processor;
    RUNLEVEL RunLevel;
    ULONG Vector;

    Controller = Context;
    RunLevel = KeGetRunLevel();

    //
    // The low run level flag better match up with how this ISR is being called.
    //

    ASSERT((((Controller->Features & INTERRUPT_FEATURE_LOW_RUN_LEVEL) != 0) &&
            (RunLevel == RunLevelLow)) ||
           (((Controller->Features & INTERRUPT_FEATURE_LOW_RUN_LEVEL) == 0) &&
            (RunLevel == Controller->RunLevel)));

    InterruptStatus = InterruptStatusClaimed;
    Cause = HlpInterruptAcknowledge(&Controller, &Vector, &MagicCandy);
    if (Cause == InterruptCauseLineFired) {
        if ((Controller->Features & INTERRUPT_FEATURE_LOW_RUN_LEVEL) != 0) {
            RunLevel = KeRaiseRunLevel(Controller->RunLevel);
        }

        ASSERT(KeGetRunLevel() >= RunLevelDispatch);

        Processor = KeGetCurrentProcessorBlock();
        HlpRunIsr(NULL, Processor, Vector, Controller);
        if (Processor->RunLevel != RunLevel) {
            KeLowerRunLevel(RunLevel);
        }

        Controller->FunctionTable.EndOfInterrupt(Controller->PrivateContext,
                                                 MagicCandy);

    } else if (Cause != InterruptCauseSpuriousInterrupt) {
        InterruptStatus = InterruptStatusNotClaimed;
    }

    return InterruptStatus;
}

VOID
HlDispatchInterrupt (
    ULONG Vector,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine determines the source of an interrupt and runs its ISR. It
    must be called with interrupts disabled, and will return with interrupts
    disabled.

Arguments:

    Vector - Supplies the vector this interrupt came in on.

    TrapFrame - Supplies a pointer to the machine state when the interrupt
        occurred.

Return Value:

    None.

--*/

{

    INTERRUPT_CAUSE Cause;
    PINTERRUPT_CONTROLLER Controller;
    PINTERRUPT_FAST_END_OF_INTERRUPT FastEndOfInterrupt;
    RUNLEVEL InterruptRunLevel;
    ULONG MagicCandy;
    RUNLEVEL OldRunLevel;
    PPENDING_INTERRUPT PendingInterrupt;
    ULONG PendingInterruptCount;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKTHREAD Thread;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    ProcessorBlock = KeGetCurrentProcessorBlock();
    Thread = ProcessorBlock->RunningThread;
    Controller = HlpInterruptGetCurrentProcessorController();

    //
    // Determine the source of the interrupt.
    //

    Cause = HlpInterruptAcknowledge(&Controller, &Vector, &MagicCandy);
    if (Cause != InterruptCauseLineFired) {
        goto DispatchInterruptEnd;
    }

    //
    // Determine the priority of the interrupt that came in and what it was
    // before.
    //

    InterruptRunLevel = VECTOR_TO_RUN_LEVEL(Vector);
    OldRunLevel = ProcessorBlock->RunLevel;

    //
    // If the interrupt should not have come in because the runlevel is too
    // high, queue the interrupt and return.
    //

    if (ProcessorBlock->RunLevel >= InterruptRunLevel) {
        PendingInterruptCount = ProcessorBlock->PendingInterruptCount;
        PendingInterrupt =
                   &(ProcessorBlock->PendingInterrupts[PendingInterruptCount]);

        PendingInterrupt->Vector = Vector;
        PendingInterrupt->MagicCandy = MagicCandy;
        PendingInterrupt->InterruptController = Controller;
        ProcessorBlock->PendingInterruptCount += 1;
        goto DispatchInterruptEnd;
    }

    //
    // Set the current run level to match this interrupt, and re-enable
    // interrupts at the processor core. Other interrupts can now come down on
    // top of this code with no problems, as the run level management has been
    // taken care of.
    //

    ProcessorBlock->RunLevel = InterruptRunLevel;

    //
    // Only re-enable interrupts if the controller hardware can properly
    // enforce that no interrupts of less than or equal priority will come down
    // on top of this one.
    //

    if (Controller->PriorityCount != 0) {
        ArEnableInterrupts();
    }

    HlpRunIsr(TrapFrame, ProcessorBlock, Vector, Controller);

    //
    // Disable interrupts at the processor core again to restore the state to
    // the pre-interrupting condition.
    //

    ArDisableInterrupts();

    //
    // EOI this interrupt, which pops the priority down to the next highest
    // pending interrupt.
    //

    FastEndOfInterrupt = Controller->FunctionTable.FastEndOfInterrupt;
    if (FastEndOfInterrupt != NULL) {
        FastEndOfInterrupt();

    } else {
        Controller->FunctionTable.EndOfInterrupt(Controller->PrivateContext,
                                                 MagicCandy);
    }

    //
    // Lower the interrupt runlevel down to what it was when this interrupt
    // occurred, which will replay any interrupts in the queue.
    //

    HlpLowerRunLevel(OldRunLevel, TrapFrame);

    //
    // Check for any pending signals, the equivalent of a user mode interrupt.
    //

    if ((OldRunLevel == RunLevelLow) &&
        (ArIsTrapFrameFromPrivilegedMode(TrapFrame) == FALSE)) {

        ArEnableInterrupts();
        PsCheckRuntimeTimers(Thread);
        PsDispatchPendingSignals(Thread, TrapFrame);
        ArDisableInterrupts();
    }

DispatchInterruptEnd:
    return;
}

RUNLEVEL
HlRaiseRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine raises the interrupt run level of the system.

Arguments:

    RunLevel - Supplies the run level to raise to. This must be greater than
        or equal to the current runlevel.

Return Value:

    Returns a pointer to the old run level.

--*/

{

    BOOL Enabled;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;

    Enabled = ArDisableInterrupts();
    ProcessorBlock = KeGetCurrentProcessorBlock();
    OldRunLevel = ProcessorBlock->RunLevel;

    ASSERT(RunLevel >= OldRunLevel);

    if (OldRunLevel >= RunLevel) {
        goto RaiseRunLevelEnd;
    }

    //
    // Raising the run level is easy. Just set it!
    //

    ProcessorBlock->RunLevel = RunLevel;

RaiseRunLevelEnd:
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return OldRunLevel;
}

VOID
HlLowerRunLevel (
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine lowers the interrupt run level of the system.

Arguments:

    RunLevel - Supplies the run level to lower to. This must be less than
        or equal to the current runlevel.

Return Value:

    Returns a pointer to the old run level.

--*/

{

    HlpLowerRunLevel(RunLevel, NULL);
    return;
}

VOID
HlpInterruptServiceDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine is called when an interrupt needs DPC service.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PKINTERRUPT Interrupt;
    ULONG OldFlags;

    Interrupt = Dpc->UserData;

    //
    // Deferred interrupts are only processed at low level, not dispatch.
    //

    ASSERT(((Interrupt->QueueFlags & INTERRUPT_QUEUE_DEFERRED) == 0) ||
           (Interrupt->LowLevelServiceRoutine != NULL));

    if (Interrupt->LowLevelServiceRoutine != NULL) {

        //
        // Set the work item queue flag before clearing the DPC queued flag so
        // there's never a region where it looks like nothings queued but
        // something is.
        //

        OldFlags = RtlAtomicOr32(&(Interrupt->QueueFlags),
                                 INTERRUPT_QUEUE_WORK_ITEM_QUEUED);

        RtlAtomicAnd32(&(Interrupt->QueueFlags), ~INTERRUPT_QUEUE_DPC_QUEUED);
        if ((OldFlags & INTERRUPT_QUEUE_WORK_ITEM_QUEUED) == 0) {
            KeQueueWorkItem(Interrupt->WorkItem);
        }

    } else {
        RtlAtomicAnd32(&(Interrupt->QueueFlags), ~INTERRUPT_QUEUE_DPC_QUEUED);
    }

    //
    // Call the dispatch level ISR if requested.
    //

    if (Interrupt->DispatchServiceRoutine != NULL) {
        Interrupt->DispatchServiceRoutine(Interrupt->Context);
    }

    return;
}

VOID
HlpInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine contains the generic interrupt service work item handler,
    which calls out to the low level service routine for the interrupt.

Arguments:

    Parameter - Supplies a context pointer, in this case a pointer to the
        KINTERRUPT.

Return Value:

    None.

--*/

{

    ULONG ClearFlags;
    PKINTERRUPT Interrupt;
    ULONG OldFlags;
    INTERRUPT_STATUS Status;

    Interrupt = Parameter;
    ClearFlags = INTERRUPT_QUEUE_WORK_ITEM_QUEUED |
                 INTERRUPT_QUEUE_DEFERRED;

    OldFlags = RtlAtomicAnd32(&(Interrupt->QueueFlags), ~ClearFlags);
    Status = Interrupt->LowLevelServiceRoutine(Interrupt->Context);
    if (Status == InterruptStatusDefer) {
        return;
    }

    //
    // If this is a deferred interrupt, continue calling ISRs.
    //

    if ((OldFlags & INTERRUPT_QUEUE_DEFERRED) != 0) {
        HlContinueInterrupt(Interrupt, Status);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlpLowerRunLevel (
    RUNLEVEL RunLevel,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine lowers the run level down to the given run level.

Arguments:

    RunLevel - Supplies the new run level to lower to. This must be less than
        or equal to the current run level.

    TrapFrame - Supplies an optional pointer to the trap frame of the interrupt
        about to be returned from.

Return Value:

    None.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    BOOL Enabled;
    RUNLEVEL HighestPendingRunLevel;
    ULONG HighestPendingVector;
    ULONG MagicCandy;
    ULONG PendingIndex;
    PPENDING_INTERRUPT PendingInterrupts;
    PPROCESSOR_BLOCK ProcessorBlock;

    //
    // Disable interrupts both to prevent scheduling to another core in the case
    // of lowering from below dispatch, and to prevent concurrency problems
    // while the pending interrupt queue is being accessed.
    //

    Enabled = ArDisableInterrupts();
    ProcessorBlock = KeGetCurrentProcessorBlock();

    ASSERT(RunLevel <= ProcessorBlock->RunLevel);

    if (ProcessorBlock->RunLevel <= RunLevel) {
        goto LowerRunLevelEnd;
    }

    PendingInterrupts =
                      (PPENDING_INTERRUPT)&(ProcessorBlock->PendingInterrupts);

    //
    // Replay all interrupts greater than the run level being lowered to.
    //

    while (ProcessorBlock->PendingInterruptCount != 0) {
        PendingIndex = ProcessorBlock->PendingInterruptCount - 1;
        HighestPendingVector = PendingInterrupts[PendingIndex].Vector;
        HighestPendingRunLevel = VECTOR_TO_RUN_LEVEL(HighestPendingVector);

        //
        // Stop looping if the highest pending interrupt will still be masked
        // by the new run level.
        //

        if (HighestPendingRunLevel <= RunLevel) {
            break;
        }

        //
        // Pop this off the queue and replay it.
        //

        Controller = PendingInterrupts[PendingIndex].InterruptController;
        MagicCandy = PendingInterrupts[PendingIndex].MagicCandy;
        ProcessorBlock->PendingInterruptCount = PendingIndex;
        ProcessorBlock->RunLevel = HighestPendingRunLevel;
        HlpInterruptReplay(Controller, HighestPendingVector, MagicCandy);
    }

    //
    // If lowering below dispatch level, check for software interrupts, and
    // play them if necessary. There is a case where the scheduler is lowering
    // the run level with interrupts disabled, which is detectable when
    // interrupts were disabled and the run level was at dispatch. Avoid
    // running software interrupts in that case (which means play them if
    // interrupts were enabled before or the run level is coming from an actual
    // interrupt run level).
    //

    if ((ProcessorBlock->PendingDispatchInterrupt != FALSE) &&
        (RunLevel < RunLevelDispatch) &&
        ((ProcessorBlock->RunLevel > RunLevelDispatch) || (Enabled != FALSE))) {

        //
        // Loop dispatching software interrupts. This must be done in a loop
        // because interrupts will be enabled allowing new DPCs to arrive.
        // Without the loop, the new arrivals would have to wait a clock period
        // to run. This is unnecessarily slow.
        //

        ProcessorBlock->RunLevel = RunLevelDispatch;
        while (ProcessorBlock->PendingDispatchInterrupt != FALSE) {
            ProcessorBlock->PendingDispatchInterrupt = FALSE;
            KeDispatchSoftwareInterrupt(RunLevelDispatch, TrapFrame);

            //
            // A dispatch interrupt may cause the scheduler to be invoked,
            // causing a switch to another processor. Reload the processor
            // block to avoid setting some other processor's runlevel.
            //

            ProcessorBlock = KeGetCurrentProcessorBlock();
        }
    }

    //
    // There are no more interrupts queued on this processor, at least above
    // the destination runlevel. Write it in and return.
    //

    ProcessorBlock->RunLevel = RunLevel;

LowerRunLevelEnd:

    //
    // Restore interrupts.
    //

    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return;
}

VOID
HlpInterruptReplay (
    PINTERRUPT_CONTROLLER Controller,
    ULONG Vector,
    ULONG MagicCandy
    )

/*++

Routine Description:

    This routine replays an interrupt at the given vector. It assumes that the
    run level is already that of the interrupt being replayed. This routine
    will send an EOI but will not manage the current run level in any way. It
    must be called with interrupts disabled, and will return with interrupts
    disabled (but will enable them during execution).

Arguments:

    Controller - Supplies a pointer to the controller that owns the interrupt.

    Vector - Supplies the vector of the interrupt to replay.

    MagicCandy - Supplies the magic candy that the interrupt controller plugin
        returned when the interrupt was initially accepted.

Return Value:

    None.

--*/

{

    PINTERRUPT_FAST_END_OF_INTERRUPT FastEndOfInterrupt;
    PPROCESSOR_BLOCK ProcessorBlock;

    ASSERT(KeGetRunLevel() == VECTOR_TO_RUN_LEVEL(Vector));
    ASSERT(ArAreInterruptsEnabled() == FALSE);

    ProcessorBlock = KeGetCurrentProcessorBlock();

    //
    // Only re-enable interrupts if the controller hardware can properly
    // enforce that no interrupts of less than or equal priority will come down
    // on top of this one.
    //

    if (Controller->PriorityCount != 0) {
        ArEnableInterrupts();
    }

    HlpRunIsr(NULL, ProcessorBlock, Vector, Controller);

    //
    // Disable interrupts again and send the EOI. The caller must deal with
    // getting the run-level back in sync after this EOI.
    //

    ArDisableInterrupts();
    FastEndOfInterrupt = Controller->FunctionTable.FastEndOfInterrupt;
    if (FastEndOfInterrupt != NULL) {
        FastEndOfInterrupt();

    } else {
        Controller->FunctionTable.EndOfInterrupt(Controller->PrivateContext,
                                                 MagicCandy);
    }

    return;
}

INTERRUPT_STATUS
HlpRunIsr (
    PTRAP_FRAME TrapFrame,
    PPROCESSOR_BLOCK Processor,
    ULONG Vector,
    PINTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine runs the interrupt services routines for a given interrupt
    vector.

Arguments:

    TrapFrame - Supplies an optional pointer to the trap frame.

    Processor - Supplies a pointer to the current processor block.

    Vector - Supplies the vector that fired.

    Controller - Supplies a pointer to the interrupt controller that fired.

Return Value:

    Returns an overall status from running some or all of the ISRs, depending
    on the mode of the interrupt and the results returned from each ISR.

--*/

{

    PVOID Context;
    PKINTERRUPT Interrupt;
    PKINTERRUPT *InterruptTable;
    ULONGLONG LastTimestamp;
    INTERRUPT_STATUS OverallStatus;
    ULONGLONG Seconds;
    INTERRUPT_STATUS Status;
    ULONGLONG TimeCounter;

    //
    // Run all ISRs associated with this interrupt.
    //

    ASSERT(Vector >= HlFirstConfigurableVector);

    OverallStatus = InterruptStatusNotClaimed;
    InterruptTable = (PKINTERRUPT *)(Processor->InterruptTable);
    Interrupt = InterruptTable[Vector - HlFirstConfigurableVector];
    if (Interrupt == NULL) {
        RtlDebugPrint("Unexpected Interrupt on vector 0x%x, processor %d.\n",
                      Vector,
                      Processor->ProcessorNumber);

        ASSERT(FALSE);
    }

    while (Interrupt != NULL) {
        Context = Interrupt->Context;
        if (Context == INTERRUPT_CONTEXT_TRAP_FRAME) {
            Context = TrapFrame;
        }

        ASSERT(Interrupt->RunLevel == Processor->RunLevel);

        //
        // Keep track of how many times this ISR has been called (not
        // worrying too much about increment races on other cores). Every
        // so often, take a time counter timestamp. If too many interrupts
        // have happened too close together, print out a storm warning.
        //

        Interrupt->InterruptCount += 1;
        if (((Interrupt->InterruptCount &
              INTERRUPT_STORM_COUNT_MASK) == 0) &&
            (Interrupt->RunLevel <= RunLevelClock)) {

            LastTimestamp = Interrupt->LastTimestamp;
            TimeCounter = KeGetRecentTimeCounter();
            Seconds = TimeCounter - LastTimestamp /
                      HlQueryTimeCounterFrequency();

            if ((LastTimestamp != 0) &&
                (Interrupt->LastTimestamp == LastTimestamp) &&
                (Seconds < INTERRUPT_STORM_DELTA_SECONDS)) {

                RtlDebugPrint("ISR: Possible storm on vector 0x%x, "
                              "KINTERRUPT 0x%x\n",
                              Vector,
                              Interrupt);
            }

            Interrupt->LastTimestamp = TimeCounter;
        }

        //
        // Run the ISR.
        //

        Status = Interrupt->InterruptServiceRoutine(Context);
        if (Status == InterruptStatusDefer) {
            OverallStatus = Status;
            HlpDeferInterrupt(Interrupt, Controller);
            break;

        } else if (Status == InterruptStatusClaimed) {
            OverallStatus = Status;

            //
            // This interrupt has things to do. If there are lower level
            // service routines to run, queue those up now.
            //

            if ((Interrupt->DispatchServiceRoutine != NULL) ||
                (Interrupt->LowLevelServiceRoutine != NULL)) {

                HlpQueueInterruptDpc(Interrupt, 0);
            }

            //
            // For level triggered interrupts, stop calling ISRs after the
            // first interrupt to respond. If it turns out multiple interrupt
            // sources were occurring, the line will stay asserted and the
            // interrupt will fire again.
            //

            if (Interrupt->Mode == InterruptModeLevel) {
                break;
            }
        }

        Interrupt = Interrupt->NextInterrupt;
    }

    return OverallStatus;
}

VOID
HlpDeferInterrupt (
    PKINTERRUPT Interrupt,
    PINTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine defers the given interrupt, masking it and queuing lover level
    routines.

Arguments:

    Interrupt - Supplies a pointer to the interrupt to defer.

    Controller - Supplies a pointer to the controller.

Return Value:

    Returns an overall status from running some or all of the ISRs, depending
    on the mode of the interrupt and the results returned from each ISR.

--*/

{

    //
    // Mask the interrupt line.
    //

    ASSERT((Controller->Identifier == Interrupt->Line.U.Local.Controller) &&
           (Controller == Interrupt->Controller));

    Controller->FunctionTable.MaskLine(Controller->PrivateContext,
                                       &(Interrupt->Line),
                                       FALSE);

    HlpQueueInterruptDpc(Interrupt, INTERRUPT_QUEUE_DEFERRED);
    return;
}

VOID
HlpQueueInterruptDpc (
    PKINTERRUPT Interrupt,
    ULONG QueueFlags
    )

/*++

Routine Description:

    This routine queues the DPC for the interrupt if it has not yet been queued.

Arguments:

    Interrupt - Supplies a pointer to the interrupt to queue.

    QueueFlags - Supplies any additional flags to OR in to the queue flags
        mask (other than DPC queued, which will be ORed in automatically).

Return Value:

    None.

--*/

{

    ULONG OldFlags;

    ASSERT(KeGetRunLevel() == Interrupt->RunLevel);
    ASSERT(Interrupt->Dpc != NULL);

    OldFlags = RtlAtomicOr32(&(Interrupt->QueueFlags),
                             QueueFlags | INTERRUPT_QUEUE_DPC_QUEUED);

    if ((OldFlags & INTERRUPT_QUEUE_DPC_QUEUED) == 0) {
        KeQueueDpc(Interrupt->Dpc);
    }

    return;
}

INTERRUPT_STATUS
HlpContinueIsr (
    PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine continues calling ISR routines after the given interrupt.

Arguments:

    Interrupt - Supplies a pointer to the interrupt that just completed its
        service.

Return Value:

    None.

--*/

{

    PVOID Context;
    RUNLEVEL OldRunLevel;
    INTERRUPT_STATUS OverallStatus;
    RUNLEVEL RunLevel;
    INTERRUPT_STATUS Status;

    OverallStatus = InterruptStatusNotClaimed;
    RunLevel = Interrupt->RunLevel;
    OldRunLevel = KeRaiseRunLevel(RunLevel);
    Interrupt = Interrupt->NextInterrupt;
    while (Interrupt != NULL) {
        Context = Interrupt->Context;
        if (Context == INTERRUPT_CONTEXT_TRAP_FRAME) {
            Context = NULL;
        }

        ASSERT(Interrupt->RunLevel == RunLevel);

        Status = Interrupt->InterruptServiceRoutine(Context);
        if (Status == InterruptStatusDefer) {
            OverallStatus = Status;
            HlpQueueInterruptDpc(Interrupt, INTERRUPT_QUEUE_DEFERRED);
            break;

        } else if (Status == InterruptStatusClaimed) {
            OverallStatus = Status;
            if (Interrupt->Mode == InterruptModeLevel) {
                break;
            }
        }

        Interrupt = Interrupt->NextInterrupt;
    }

    KeLowerRunLevel(OldRunLevel);
    return OverallStatus;
}

