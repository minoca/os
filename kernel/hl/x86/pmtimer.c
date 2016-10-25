/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/ioport.h>

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
// Store the PM timer port.
//

USHORT HlPmTimerPort;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpPmTimerModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the PM Timer hardware module. Its role
    is to detect and report the prescense of an ACPI PM Timer.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PFADT FadtTable;
    TIMER_DESCRIPTION PmTimer;
    KSTATUS Status;

    //
    // Find the FADT table. If there is no FADT table, then this not an ACPI
    // compliant machine, so there is probably no PM timer either.
    //

    FadtTable = HlGetAcpiTable(FADT_SIGNATURE, NULL);
    if (FadtTable == NULL) {
        goto PmTimerModuleEntryEnd;
    }

    HlPmTimerPort = (USHORT)FadtTable->PmTimerBlock;
    if (HlPmTimerPort == 0) {
        goto PmTimerModuleEntryEnd;
    }

    RtlZeroMemory(&PmTimer, sizeof(TIMER_DESCRIPTION));
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

    Status = HlRegisterHardware(HardwareModuleTimer, &PmTimer);
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

    return HlIoPortInLong(HlPmTimerPort);
}

