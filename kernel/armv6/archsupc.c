/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv6 processor architecture features.

Author:

    Chris Stevens 3-Feb-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

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

VOID
ArSetUpUserSharedDataFeatures (
    VOID
    )

/*++

Routine Description:

    This routine initialize the user shared data processor specific features.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ArInitializeVfpSupport();
    return;
}

VOID
ArpInitializePerformanceMonitor (
    VOID
    )

/*++

Routine Description:

    This routine initializes the system's performance monitor.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG ControlRegister;

    //
    // Disable performance monitor interrupts, and access to the performance
    // monitors in user mode.
    //

    ControlRegister = ArGetPerformanceControlRegister();
    if (ControlRegister != 0) {
        ControlRegister &= ~ARMV6_PERF_MONITOR_INTERRUPT_MASK;
        ArSetPerformanceControlRegister(ControlRegister);
        ArSetPerformanceUserEnableRegister(0);
    }

    return;
}

VOID
ArSetThreadPointer (
    PVOID Thread,
    PVOID NewThreadPointer
    )

/*++

Routine Description:

    This routine sets the new thread pointer value.

Arguments:

    Thread - Supplies a pointer to the thread to set the thread pointer for.

    NewThreadPointer - Supplies the new thread pointer value to set.

Return Value:

    None.

--*/

{

    PULONG LowPointer;
    RUNLEVEL OldRunLevel;
    PKTHREAD TypedThread;

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);

    //
    // Only set the low 32-bits, the upper 32-bits are used to hold the
    // read/write thread pointer.
    //

    TypedThread = Thread;
    LowPointer = (PULONG)&(TypedThread->ThreadPointer);
    *LowPointer = (ULONG)NewThreadPointer;
    if (Thread == KeGetCurrentThread()) {
        ArSetThreadPointerUserReadOnly(NewThreadPointer);
    }

    KeLowerRunLevel(OldRunLevel);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

