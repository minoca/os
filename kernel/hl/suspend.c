/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    suspend.c

Abstract:

    This module implements low level hardware layer interfaces for suspending
    and resuming processors and/or the platform.

Author:

    Evan Green 6-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "hlp.h"
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
HlpResume (
    PHL_SUSPEND_INTERFACE Interface,
    HL_SUSPEND_PHASE SuspendPhase
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
HlSuspend (
    PHL_SUSPEND_INTERFACE Interface
    )

/*++

Routine Description:

    This routine implements the low level primitive to suspend the processor
    and/or platform. This routine does not deal with device states at all, it
    simply takes the CPU/platform down.

Arguments:

    Interface - Supplies a pointer to the suspend interface to use going down.

Return Value:

    Status code. A failing status code indicates that the suspend did not
    occur.

--*/

{

    BOOL Enabled;
    HL_SUSPEND_PHASE NextPhase;
    HL_SUSPEND_PHASE Phase;
    ULONG Processor;
    PPROCESSOR_CONTEXT ProcessorContext;
    UINTN SaveResult;
    KSTATUS Status;
    volatile ULONG TimesThrough;

    Processor = KeGetCurrentProcessorNumber();
    ProcessorContext = NULL;
    Enabled = ArAreInterruptsEnabled();
    TimesThrough = 0;

    //
    // Call the callback to prepare before all internal hardware state is
    // saved.
    //

    Phase = HlSuspendPhaseInvalid;
    NextPhase = HlSuspendPhaseSuspendBegin;
    Status = Interface->Callback(Interface->Context, NextPhase);
    if (!KSUCCESS(Status)) {
        goto SuspendEnd;
    }

    Phase = NextPhase;
    ArDisableInterrupts();

    //
    // Save the interrupt controller state.
    //

    if ((Interface->Flags & HL_SUSPEND_RESTORE_INTERRUPTS) != 0) {
        Status = HlpInterruptSaveState();
        if (!KSUCCESS(Status)) {
            goto SuspendEnd;
        }
    }

    Status = HlpInterruptPrepareForProcessorResume(
                                               Processor,
                                               &ProcessorContext,
                                               &(Interface->ResumeAddress),
                                               FALSE);

    if (!KSUCCESS(Status)) {
        goto SuspendEnd;
    }

    //
    // Save the context. The return from this function is also where
    // resume picks up (though with a non-zero return value).
    //

    SaveResult = ArSaveProcessorContext(ProcessorContext);
    TimesThrough += 1;

    ASSERT(TimesThrough <= 2);

    //
    // Handle the remainder of going down now that the context is saved.
    //

    if (SaveResult == 0) {

        //
        // Flush the caches if needed.
        //

        ArSerializeExecution();
        ArCleanEntireCache();
        HlFlushCache(HL_CACHE_FLAG_CLEAN);

        //
        // This is where the CPU/system actually goes down.
        //

        NextPhase = HlSuspendPhaseSuspend;
        Status = Interface->Callback(Interface->Context, NextPhase);
        if (!KSUCCESS(Status)) {
            goto SuspendEnd;
        }

        //
        // Not failure, but also not in the normal resume path. This must not
        // have been a context loss.
        //

        Phase = NextPhase;
    }

    //
    // Below here the system is resuming or coming out of a failed suspend
    // attempt.
    //

    Phase = HlSuspendPhaseSuspend;
    Status = STATUS_SUCCESS;

SuspendEnd:
    if (!KSUCCESS(Status)) {
        Interface->Phase = Phase;

        //
        // Unprepare for resume if resume was prepared.
        //

        if (ProcessorContext != NULL) {
            HlpInterruptPrepareForProcessorResume(Processor, NULL, NULL, TRUE);
        }
    }

    HlpResume(Interface, Phase);
    if (Enabled != FALSE) {
        ArEnableInterrupts();
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
HlpResume (
    PHL_SUSPEND_INTERFACE Interface,
    HL_SUSPEND_PHASE SuspendPhase
    )

/*++

Routine Description:

    This routine resumes the processor/platform according to how far it got
    in sleep.

Arguments:

    Interface - Supplies a pointer to the suspend interface to use coming up.

    SuspendPhase - Supplies the phase of suspending the processor/system got
        to.

Return Value:

    None.

--*/

{

    HL_SUSPEND_PHASE Phase;
    KSTATUS Status;

    Phase = HlSuspendPhaseInvalid;

    //
    // Walk the suspend state backwards, calling each corresponding resume
    // phase.
    //

    if (SuspendPhase == HlSuspendPhaseSuspend) {
        Phase = HlSuspendPhaseResume;
        Status = Interface->Callback(Interface->Context, Phase);
        if (!KSUCCESS(Status)) {
            goto ResumeEnd;
        }

        SuspendPhase = HlSuspendPhaseSuspendBegin;
    }

    if (SuspendPhase == HlSuspendPhaseSuspendBegin) {

        //
        // Restore interrupt controller state.
        //

        if ((Interface->Flags & HL_SUSPEND_RESTORE_INTERRUPTS) != 0) {
            Status = HlpInterruptRestoreState();
            if (!KSUCCESS(Status)) {
                goto ResumeEnd;
            }
        }

        Phase = HlSuspendPhaseResumeEnd;
        Status = Interface->Callback(Interface->Context, Phase);
        if (!KSUCCESS(Status)) {
            goto ResumeEnd;
        }
    }

    Status = STATUS_SUCCESS;

ResumeEnd:
    if (!KSUCCESS(Status)) {
        KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                      HL_CRASH_RESUME_FAILURE,
                      (UINTN)Interface,
                      Phase,
                      Status);
    }

    return;
}

