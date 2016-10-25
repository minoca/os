/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archintr.c

Abstract:

    This module implements ARMv6 system interrupt functionality.

Author:

    Chris Stevens 2-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include <minoca/kernel/kdebug.h>
#include "../hlp.h"
#include "../intrupt.h"
#include "../profiler.h"
#include "../clock.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of IPI lines needed for normal system operation on ARMv6
// processors.
//

#define REQUIRED_IPI_LINE_COUNT 0

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Builtin hardware module function prototypes.
//

VOID
HlpBcm2709InterruptModuleEntry (
    VOID
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinModules[] = {
    HlpBcm2709InterruptModuleEntry,
};

//
// Store the first vector number of the processor's interrupt array.
//

ULONG HlFirstConfigurableVector = MINIMUM_VECTOR;

//
// Stores a pointer to the internal clock and profiler interrupts.
//

PKINTERRUPT HlClockKInterrupt;
PKINTERRUPT HlProfilerKInterrupt;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HlpArchInitializeInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine performs architecture-specific initialization for the
    interrupt subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG ModuleCount;
    PHARDWARE_MODULE_ENTRY ModuleEntry;
    ULONG ModuleIndex;
    KSTATUS Status;

    //
    // Connect some built-in vectors.
    //

    HlClockKInterrupt = HlpCreateAndConnectInternalInterrupt(
                                                 VECTOR_CLOCK_INTERRUPT,
                                                 RunLevelClock,
                                                 HlpEarlyClockInterruptHandler,
                                                 NULL);

    if (HlClockKInterrupt == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    HlProfilerKInterrupt = HlpCreateAndConnectInternalInterrupt(
                                                 VECTOR_PROFILER_INTERRUPT,
                                                 RunLevelHigh,
                                                 HlpProfilerInterruptHandler,
                                                 INTERRUPT_CONTEXT_TRAP_FRAME);

    if (HlProfilerKInterrupt == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    //
    // Loop through and initialize every built in hardware module.
    //

    ModuleCount = sizeof(HlBuiltinModules) / sizeof(HlBuiltinModules[0]);
    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        ModuleEntry = HlBuiltinModules[ModuleIndex];
        ModuleEntry();
    }

    Status = STATUS_SUCCESS;

ArchInitializeInterruptsEnd:
    return Status;
}

ULONG
HlpInterruptGetIpiVector (
    IPI_TYPE IpiType
    )

/*++

Routine Description:

    This routine determines the architecture-specific hardware vector to use
    for the given IPI type.

Arguments:

    IpiType - Supplies the IPI type to send.

Return Value:

    Returns the vector that the given IPI type runs on.

--*/

{

    //
    // Implement this if SMP support is available for ARMv6.
    //

    ASSERT(FALSE);

    return 0;
}

ULONG
HlpInterruptGetRequiredIpiLineCount (
    VOID
    )

/*++

Routine Description:

    This routine determines the number of "software only" interrupt lines that
    are required for normal system operation. This routine is architecture
    dependent.

Arguments:

    None.

Return Value:

    Returns the number of software IPI lines needed for system operation.

--*/

{

    //
    // Implement this if SMP support is available for ARMv6.
    //

    ASSERT(FALSE);

    return REQUIRED_IPI_LINE_COUNT;
}

ULONG
HlpInterruptGetIpiLineIndex (
    IPI_TYPE IpiType
    )

/*++

Routine Description:

    This routine determines which of the IPI lines should be used for the
    given IPI type.

Arguments:

    IpiType - Supplies the type of IPI to be sent.

Return Value:

    Returns the IPI line index corresponding to the given IPI type.

--*/

{

    //
    // Implement this if SMP support is available for ARMv6.
    //

    ASSERT(FALSE);

    return 0;
}

VOID
HlpInterruptGetStandardCpuLine (
    PINTERRUPT_LINE Line
    )

/*++

Routine Description:

    This routine determines the architecture-specific standard CPU interrupt
    line that most interrupts get routed to.

Arguments:

    Line - Supplies a pointer where the standard CPU interrupt line will be
        returned.

Return Value:

    None.

--*/

{

    Line->Type = InterruptLineControllerSpecified;
    Line->U.Local.Controller = INTERRUPT_CPU_IDENTIFIER;
    Line->U.Local.Line = INTERRUPT_CPU_IRQ_PIN;
    return;
}

INTERRUPT_CAUSE
HlpInterruptAcknowledge (
    PINTERRUPT_CONTROLLER *ProcessorController,
    PULONG Vector,
    PULONG MagicCandy
    )

/*++

Routine Description:

    This routine begins an interrupt, acknowledging its receipt into the
    processor.

Arguments:

    ProcessorController - Supplies a pointer where on input the interrupt
        controller that owns this processor will be supplied. This pointer may
        pointer to NULL, in which case the interrupt controller that fired the
        interrupt will be returned.

    Vector - Supplies a pointer to the vector on input. For non-vectored
        architectures, the vector corresponding to the interrupt that fired
        will be returned.

    MagicCandy - Supplies a pointer where an opaque token regarding the
        interrupt will be returned. This token is only used by the interrupt
        controller hardware module.

Return Value:

    Returns the cause of the interrupt.

--*/

{

    INTERRUPT_CAUSE Cause;
    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;
    INTERRUPT_LINE Line;
    PINTERRUPT_LINES Lines;
    ULONG Offset;
    KSTATUS Status;

    //
    // If there is a controller associated with this processor, use it.
    //

    Controller = *ProcessorController;
    if (Controller != NULL) {
        Cause = Controller->FunctionTable.BeginInterrupt(
                                                     Controller->PrivateContext,
                                                     &Line,
                                                     MagicCandy);

    //
    // There is no controller, so loop through all the controllers seeing if
    // anyone responds.
    //

    } else {
        ControllerCount = HlInterruptControllerCount;
        for (ControllerIndex = 0;
             ControllerIndex < ControllerCount;
             ControllerIndex += 1) {

            Controller = HlInterruptControllers[ControllerIndex];
            if (Controller == NULL) {
                continue;
            }

            Cause = Controller->FunctionTable.BeginInterrupt(
                                                     Controller->PrivateContext,
                                                     &Line,
                                                     MagicCandy);

            if ((Cause == InterruptCauseLineFired) ||
                (Cause == InterruptCauseSpuriousInterrupt)) {

                break;
            }
        }

        if (ControllerIndex == ControllerCount) {
            Cause = InterruptCauseNoInterruptHere;
        }
    }

    //
    // Determine the vector corresponding to the interrupt lines that fired.
    //

    if (Cause == InterruptCauseLineFired) {

        ASSERT(Line.Type == InterruptLineControllerSpecified);

        Status = HlpInterruptFindLines(&Line,
                                       ProcessorController,
                                       &Lines,
                                       &Offset);

        ASSERT(KSUCCESS(Status));

        *Vector = Lines->State[Offset].PublicState.Vector;
    }

    //
    // Ensure all writes to the interrupt controller complete before interrupts
    // are enabled at the processor.
    //

    if (Cause != InterruptCauseNoInterruptHere) {
        ArSerializeExecution();
    }

    return Cause;
}

PKINTERRUPT
HlpInterruptGetClockKInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine returns the clock timer's KINTERRUPT structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the clock KINTERRUPT.

--*/

{

    return HlClockKInterrupt;
}

PKINTERRUPT
HlpInterruptGetProfilerKInterrupt (
    VOID
    )

/*++

Routine Description:

    This routine returns the profiler timer's KINTERRUPT structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the profiler KINTERRUPT.

--*/

{

    return HlProfilerKInterrupt;
}

//
// --------------------------------------------------------- Internal Functions
//

