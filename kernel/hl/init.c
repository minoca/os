/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.c

Abstract:

    This module implements initialization routines for the hardware library.

Author:

    Evan Green 5-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/lib/bconf.h>
#include "intrupt.h"
#include "timer.h"
#include "clock.h"
#include "dbgdev.h"
#include "profiler.h"
#include "hlp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
HlpInitializeEfi (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

VOID
HlpModInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG ProcessorNumber
    );

KSTATUS
HlpInitializeInterrupts (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
HlpInitializeTimersPreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG ProcessorNumber
    );

KSTATUS
HlpInitializeTimers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
HlpInitializeCalendarTimers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
HlpInitializeCacheControllers (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Processor,
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice
    )

/*++

Routine Description:

    This routine implements extremely early hardware layer initialization. This
    routine is *undebuggable*, as it is called before the debugger is brought
    online.

Arguments:

    Parameters - Supplies an optional pointer to the kernel initialization
        parameters. This parameter may be NULL.

    Processor - Supplies the processor index of this processor.

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

Return Value:

    None.

--*/

{

    PBOOT_ENTRY BootEntry;
    ULONG DebugDeviceIndex;

    if (Processor == 0) {
        HlpInitializeEfi(Parameters);
        HlpModInitializePreDebugger(Parameters, Processor);

        //
        // Fire up a timer so the debugger can stall.
        //

        HlpInitializeTimersPreDebugger(Parameters, Processor);

        //
        // Fire up the debug device so the debugger can speak through it.
        //

        DebugDeviceIndex = 0;
        BootEntry = Parameters->BootEntry;
        if (BootEntry != NULL) {
            DebugDeviceIndex = BootEntry->DebugDevice;
        }

        HlpInitializeDebugDevices(DebugDeviceIndex, DebugDevice);
    }

    return;
}

KSTATUS
HlInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    )

/*++

Routine Description:

    This routine initializes the core system hardware. During phase 0, on
    application processors, this routine enters at low run level and exits at
    dispatch run level.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

    Phase - Supplies the initialization phase.

Return Value:

    Status code.

--*/

{

    ULONG ProcessorNumber;
    KSTATUS Status;

    //
    // Initialize core system resources like power, interrupts, and timers.
    //

    if (Phase == 0) {
        ProcessorNumber = KeGetCurrentProcessorNumber();
        if (ProcessorNumber == 0) {
            HlpTestUsbDebugInterface();
        }

        Status = HlpInitializeInterrupts(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        //
        // Raise all application processors to dispatch level before enabling
        // interrupts. Initializing timers for these processors will either
        // arm their clock interrupt or enable broadcast on P0's clock
        // interrupt, but the application processors are not prepared to handle
        // software interrupts until the process and thread subsystem is
        // initialized. (The clock interrupt tells a processor to check for
        // pending software interrupts the next time the run level lowers.)
        //

        if (ProcessorNumber != 0) {
            KeRaiseRunLevel(RunLevelDispatch);
        }

        ArEnableInterrupts();
        Status = HlpInitializeTimers(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = HlpInitializeCalendarTimers(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = HlpInitializeCacheControllers(Parameters);
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        Status = HlpInitializeRebootModules();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }

        RtlDebugPrint("Processor %d alive. 0x%x\n",
                      ProcessorNumber,
                      KeGetCurrentProcessorBlock());

    //
    // Switch the clock and profiler from stub routines to the real clock
    // and profiler interrupt handlers.
    //

    } else {

        ASSERT(Phase == 1);

        Status = HlpTimerActivateClock();
        if (!KSUCCESS(Status)) {
            goto InitializeEnd;
        }
    }

InitializeEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

