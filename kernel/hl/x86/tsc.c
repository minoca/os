/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    tsc.c

Abstract:

    This module implements the hardware module for the PC processor TSC
    (Time Stamp Counter).

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/acpitabs.h>
#include <minoca/hmod.h>
#include <minoca/arch.h>
#include <minoca/x86.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpTscInitialize (
    PVOID Context
    );

VOID
HlpTscWrite (
    PVOID Context,
    ULONGLONG NewCount
    );

KSTATUS
HlpTscDetermineCharacteristics (
    PBOOL Invariant
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the system services table.
//

PHARDWARE_MODULE_KERNEL_SERVICES HlTscSystemServices = NULL;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpTscModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for the TSC hardware module. Its role is to
    report the TSC.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    KSTATUS Status;
    BOOL TscInvariant;
    TIMER_DESCRIPTION TscTimer;

    HlTscSystemServices = Services;
    HlTscSystemServices->ZeroMemory(&TscTimer, sizeof(TIMER_DESCRIPTION));
    TscTimer.TableVersion = TIMER_DESCRIPTION_VERSION;
    TscTimer.FunctionTable.Initialize = HlpTscInitialize;
    TscTimer.FunctionTable.ReadCounter =
                                   (PTIMER_READ_COUNTER)ArReadTimeStampCounter;

    TscTimer.FunctionTable.WriteCounter = HlpTscWrite;
    TscTimer.FunctionTable.Arm = NULL;
    TscTimer.FunctionTable.Disarm = NULL;
    TscTimer.FunctionTable.AcknowledgeInterrupt = NULL;
    TscTimer.Context = NULL;
    TscTimer.Features = TIMER_FEATURE_PER_PROCESSOR |
                        TIMER_FEATURE_READABLE |
                        TIMER_FEATURE_WRITABLE |
                        TIMER_FEATURE_PROCESSOR_COUNTER;

    //
    // Determine if the TSC varies with processor power management states.
    //

    Status = HlpTscDetermineCharacteristics(&TscInvariant);
    if (!KSUCCESS(Status)) {
        goto TscTimerModuleEntryEnd;
    }

    if (TscInvariant == FALSE) {
        TscTimer.Features |= TIMER_FEATURE_SPEED_VARIES;
    }

    //
    // The timer's frequency is not hardcoded, as it runs at the main CPU speed,
    // which must be measured.
    //

    TscTimer.CounterFrequency = 0;
    TscTimer.CounterBitWidth = 64;

    //
    // Register the TSC with the system.
    //

    Status = HlTscSystemServices->Register(HardwareModuleTimer, &TscTimer);
    if (!KSUCCESS(Status)) {
        goto TscTimerModuleEntryEnd;
    }

TscTimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpTscInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes the TSC.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    //
    // The TSC is already running, no action is needed.
    //

    return STATUS_SUCCESS;
}

VOID
HlpTscWrite (
    PVOID Context,
    ULONGLONG NewCount
    )

/*++

Routine Description:

    This routine writes to the timer's hardware counter.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewCount - Supplies the value to write into the counter. It is expected that
        the counter will not stop after the write.

Return Value:

    None.

--*/

{

    //
    // Not yet implemented.
    //

    return;
}

KSTATUS
HlpTscDetermineCharacteristics (
    PBOOL Invariant
    )

/*++

Routine Description:

    This routine queries the characterics of the TSC counter with respect to
    whether or not the counter stops during deep idle states.

Arguments:

    Invariant - Supplies a pointer where a boolean will be returned indicating
        whether or not the TSC is invariant (meaning it runs in all deep idle
        states).

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;

    *Invariant = FALSE;
    Eax = X86_CPUID_ADVANCED_POWER_MANAGEMENT;
    Ebx = 0;
    Ecx = 0;
    Edx = 0;
    HlTscSystemServices->Cpuid(&Eax, &Ebx, &Ecx, &Edx);
    if ((Edx & X86_CPUID_ADVANCED_POWER_EDX_TSC_INVARIANT) != 0) {
        *Invariant = TRUE;
    }

    return STATUS_SUCCESS;
}

