/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    pmtimer.c

Abstract:

    This module implements the hardware module for ACPI PM Timer.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/acpitabs.h>
#include <minoca/kernel/hmod.h>

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
HlpPmTimerInitialize (
    PVOID Context
    );

ULONGLONG
HlpPmTimerRead (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the system services table.
//

PHARDWARE_MODULE_KERNEL_SERVICES HlPmTimerServices = NULL;

//
// Store the PM timer port.
//

USHORT HlPmTimerPort;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpPmTimerModuleEntry (
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for the PM Timer hardware module. Its role
    is to detect and report the prescense of an ACPI PM Timer.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module.

Return Value:

    None.

--*/

{

    PFADT FadtTable;
    TIMER_DESCRIPTION PmTimer;
    KSTATUS Status;

    HlPmTimerServices = Services;

    //
    // Find the FADT table. If there is no FADT table, then this not an ACPI
    // compliant machine, so there is probably no PM timer either.
    //

    FadtTable = HlPmTimerServices->GetAcpiTable(FADT_SIGNATURE, NULL);
    if (FadtTable == NULL) {
        goto PmTimerModuleEntryEnd;
    }

    HlPmTimerPort = (USHORT)FadtTable->PmTimerBlock;
    if (HlPmTimerPort == 0) {
        goto PmTimerModuleEntryEnd;
    }

    HlPmTimerServices->ZeroMemory(&PmTimer, sizeof(TIMER_DESCRIPTION));
    PmTimer.TableVersion = TIMER_DESCRIPTION_VERSION;
    PmTimer.FunctionTable.Initialize = HlpPmTimerInitialize;
    PmTimer.FunctionTable.ReadCounter = HlpPmTimerRead;
    PmTimer.FunctionTable.WriteCounter = NULL;
    PmTimer.FunctionTable.Arm = NULL;
    PmTimer.FunctionTable.Disarm = NULL;
    PmTimer.FunctionTable.AcknowledgeInterrupt = NULL;
    PmTimer.Context = NULL;
    PmTimer.Features = TIMER_FEATURE_READABLE;

    //
    // The timer's frequency is not hardcoded, as it runs at the main CPU speed,
    // which must be measured.
    //

    PmTimer.CounterFrequency = PM_TIMER_FREQUENCY;
    PmTimer.CounterBitWidth = 24;
    if ((FadtTable->Flags & FADT_FLAG_PM_TIMER_32_BITS) != 0) {
        PmTimer.CounterBitWidth = 32;
    }

    //
    // Register the PM timer with the system.
    //

    Status = HlPmTimerServices->Register(HardwareModuleTimer, &PmTimer);
    if (!KSUCCESS(Status)) {
        goto PmTimerModuleEntryEnd;
    }

PmTimerModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpPmTimerInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes the PM Timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    //
    // The PM Timer is already running, no action is needed.
    //

    return STATUS_SUCCESS;
}

ULONGLONG
HlpPmTimerRead (
    PVOID Context
    )

/*++

Routine Description:

    This routine returns the hardware counter's raw value.

Arguments:

    Context - Supplies the pointer to the timer's context.

Return Value:

    Returns the timer's current count.

--*/

{

    return HlPmTimerServices->ReadPort32(HlPmTimerPort);
}

