/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
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

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

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
    PVOID Context;
    PINTERRUPT_CONTROLLER Controller;
    PINTERRUPT_FAST_END_OF_INTERRUPT FastEndOfInterrupt;
    PKINTERRUPT Interrupt;
    RUNLEVEL InterruptRunLevel;
    PKINTERRUPT *InterruptTable;
    ULONG MagicCandy;
    RUNLEVEL OldRunLevel;
    ULONG PendingInterruptCount;
    PPROCESSOR_BLOCK ProcessorBlock;
    PKTHREAD Thread;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    ProcessorBlock = KeGetCurrentProcessorBlock();
    Thread =  ProcessorBlock->RunningThread;
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
        ProcessorBlock->PendingInterruptVectors[PendingInterruptCount] = Vector;
        ProcessorBlock->PendingInterruptMagicCandy[PendingInterruptCount] =
                                                                    MagicCandy;

        ProcessorBlock->PendingInterruptController[PendingInterruptCount] =
                                                                    Controller;

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

    //
    // Run all ISRs associated with this interrupt.
    //

    ASSERT(Vector >= HlFirstConfigurableVector);

    InterruptTable = (PKINTERRUPT *)ProcessorBlock->InterruptTable;
    Interrupt = InterruptTable[Vector - HlFirstConfigurableVector];
    if (Interrupt == NULL) {
        RtlDebugPrint("Unexpected Interrupt on vector 0x%x, processor %d\n",
                      Vector,
                      ProcessorBlock->ProcessorNumber);

        ASSERT(FALSE);

    } else {
        while (Interrupt != NULL) {
            Context = Interrupt->Context;
            if (Context == INTERRUPT_CONTEXT_TRAP_FRAME) {
                Context = TrapFrame;
            }

            Interrupt->ServiceRoutine(Context);
            Interrupt = Interrupt->NextInterrupt;
        }
    }

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

    if ((OldRunLevel == RunLevelLow) && (Thread != NULL)) {
        ArEnableInterrupts();
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
    PULONG PendingVectors;
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

    PendingVectors = (PULONG)&(ProcessorBlock->PendingInterruptVectors);

    //
    // Replay all interrupts greater than the run level being lowered to.
    //

    while (ProcessorBlock->PendingInterruptCount != 0) {
        PendingIndex = ProcessorBlock->PendingInterruptCount - 1;
        HighestPendingVector = PendingVectors[PendingIndex];
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

        Controller = ProcessorBlock->PendingInterruptController[PendingIndex];
        MagicCandy = ProcessorBlock->PendingInterruptMagicCandy[PendingIndex];
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

        ProcessorBlock->RunLevel = RunLevelDispatch;
        while (ProcessorBlock->PendingDispatchInterrupt != FALSE) {
            ProcessorBlock->PendingDispatchInterrupt = FALSE;
            ArEnableInterrupts();
            KeDispatchSoftwareInterrupt(RunLevelDispatch, TrapFrame);
            ArDisableInterrupts();
        }

        //
        // A dispatch interrupt may cause the scheduler to be invoked, causing
        // a switch to another processor. Reload the processor block to avoid
        // setting some other processor's runlevel.
        //

        ProcessorBlock = KeGetCurrentProcessorBlock();
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

    PVOID Context;
    PINTERRUPT_FAST_END_OF_INTERRUPT FastEndOfInterrupt;
    PKINTERRUPT Interrupt;
    PKINTERRUPT *InterruptTable;
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

    //
    // Run all ISRs associated with this interrupt.
    //

    ASSERT(Vector >= HlFirstConfigurableVector);

    InterruptTable = (PKINTERRUPT *)ProcessorBlock->InterruptTable;
    Interrupt = InterruptTable[Vector - HlFirstConfigurableVector];
    if (Interrupt == NULL) {
        RtlDebugPrint("Unexpected Interrupt on vector 0x%x, processor %d.\n",
                      Vector,
                      ProcessorBlock->ProcessorNumber);

        ASSERT(FALSE);

    } else {
        while (Interrupt != NULL) {
            Context = Interrupt->Context;
            if (Context == INTERRUPT_CONTEXT_TRAP_FRAME) {
                Context = NULL;
            }

            Interrupt->ServiceRoutine(Context);
            Interrupt = Interrupt->NextInterrupt;
        }
    }

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

