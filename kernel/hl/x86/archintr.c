/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archintr.c

Abstract:

    This module implements x86 system interrupt functionality.

Author:

    Evan Green 3-Jul-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86defs.h>
#include "../hlp.h"
#include "../intrupt.h"
#include "../profiler.h"
#include "../clock.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of IPI lines needed for normal system operation on PC
// processors.
//

#define REQUIRED_IPI_LINE_COUNT 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// System interrupt service routines.
//

INTERRUPT_STATUS
KeIpiServiceRoutine (
    PVOID Context
    );

INTERRUPT_STATUS
MmTlbInvalidateIpiServiceRoutine (
    PVOID Context
    );

//
// Builtin hardware module function prototypes.
//

VOID
HlpApicModuleEntry (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Built-in hardware modules.
//

PHARDWARE_MODULE_ENTRY HlBuiltinModules[] = {
    HlpApicModuleEntry,
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

    ULONG LineIndex;
    ULONG ModuleCount;
    PHARDWARE_MODULE_ENTRY ModuleEntry;
    ULONG ModuleIndex;
    KSTATUS Status;

    //
    // Connect some built-in vectors.
    //

    LineIndex = HlpInterruptGetIpiLineIndex(IpiTypePacket);
    HlIpiKInterrupt[LineIndex] = HlpCreateAndConnectInternalInterrupt(
                                                          VECTOR_IPI_INTERRUPT,
                                                          RunLevelIpi,
                                                          KeIpiServiceRoutine,
                                                          NULL);

    if (HlIpiKInterrupt[LineIndex] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    LineIndex = HlpInterruptGetIpiLineIndex(IpiTypeTlbFlush);
    HlIpiKInterrupt[LineIndex] = HlpCreateAndConnectInternalInterrupt(
                                              VECTOR_TLB_IPI,
                                              RunLevelIpi,
                                              MmTlbInvalidateIpiServiceRoutine,
                                              NULL);

    if (HlIpiKInterrupt[LineIndex] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    //
    // Save a copy of the profiler interrupt since all the IPIs run on the
    // same line for x86.
    //

    HlProfilerKInterrupt = HlpCreateAndConnectInternalInterrupt(
                                                 VECTOR_PROFILER_INTERRUPT,
                                                 RunLevelHigh,
                                                 HlpProfilerInterruptHandler,
                                                 INTERRUPT_CONTEXT_TRAP_FRAME);

    LineIndex = HlpInterruptGetIpiLineIndex(IpiTypeProfiler);
    HlIpiKInterrupt[LineIndex] = HlProfilerKInterrupt;
    if (HlIpiKInterrupt[LineIndex] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    LineIndex = HlpInterruptGetIpiLineIndex(IpiTypeClock);
    HlIpiKInterrupt[LineIndex] = HlpCreateAndConnectInternalInterrupt(
                                                            VECTOR_CLOCK_IPI,
                                                            RunLevelClock,
                                                            HlpClockIpiHandler,
                                                            NULL);

    if (HlIpiKInterrupt[LineIndex] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ArchInitializeInterruptsEnd;
    }

    HlClockKInterrupt = HlpCreateAndConnectInternalInterrupt(
                                                 VECTOR_CLOCK_INTERRUPT,
                                                 RunLevelClock,
                                                 HlpEarlyClockInterruptHandler,
                                                 NULL);

    if (HlClockKInterrupt == NULL) {
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

    switch (IpiType) {
    case IpiTypePacket:
        return VECTOR_IPI_INTERRUPT;

    case IpiTypeTlbFlush:
        return VECTOR_TLB_IPI;

    case IpiTypeNmi:
        return VECTOR_NMI;

    case IpiTypeProfiler:
        return VECTOR_PROFILER_INTERRUPT;

    case IpiTypeClock:
        return VECTOR_CLOCK_INTERRUPT;

    default:
        break;
    }

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
    Line->U.Local.Line = INTERRUPT_CPU_LINE_NORMAL_INTERRUPT;
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

    //
    // The vector is already known, so there's no need to go querying
    // controllers and looking up lines. In PIC mode, the processor won't have
    // a controller, so just set it to the first controller.
    //

    if (*ProcessorController == NULL) {
        *ProcessorController = HlInterruptControllers[0];
    }

    return InterruptCauseLineFired;
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

