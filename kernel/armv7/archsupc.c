/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv7 processor architecture features.

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

    ULONG Architecture;
    PUSER_SHARED_DATA Data;
    ULONG MainId;

    Data = MmGetUserSharedData();
    MainId = ArGetMainIdRegister();
    Architecture = (MainId & ARM_MAIN_ID_ARCHITECTURE_MASK) >>
                   ARM_MAIN_ID_ARCHITECTURE_SHIFT;

    if (Architecture == ARM_MAIN_ID_ARCHITECTURE_CPUID) {
        Data->ProcessorFeatures |= ARM_FEATURE_V7;
    }

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

    //
    // Disable performance monitor interrupts, and access to the performance
    // monitors in user mode.
    //

    if (ArGetPerformanceControlRegister() != 0) {
        ArClearPerformanceInterruptRegister(PERF_MONITOR_COUNTER_MASK);
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

