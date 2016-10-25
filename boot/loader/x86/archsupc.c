/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements miscellaneous architecture specific support in the
    loader.

Author:

    Evan Green 7-Apr-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/x86.h>
#include <minoca/kernel/ioport.h>
#include "firmware.h"
#include "bootlib.h"
#include "paging.h"
#include "loader.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the warmup stall duration if using firmware services. This happens to
// coincide with one tick of the PC/AT BIOS timer. On EFI systems it may just
// waste time, but it also may not if their stall is driven by the same
// interrupt.
//

#define X86_FIRMWARE_WARMUP_STALL_DURATION 54925

//
// Define the total stall duration when using firmware services. This happens
// to align to the PCAT BIOS timer services (int 0x1A function 0, also
// reflected in the BIOS Data Area, offset 0x6C).
//

#define X86_FIRMWARE_MEASURING_STALL_DURATION (3 * 54925)

//
// Define the minimum realistic frequency one can expect from a machine. If
// the measurement appears to be below this then either the cycle counter is
// not ticking or the stall returned immediately. This tick count corresponds
// to about 100MHz. Anything below that and it's assumed to be wrong.
//

#define X86_FIRMWARE_MINIMUM_TICK_DELTA 16477500

//
// Define the amount of time to stall against the PM timer. 1/8 of a second is
// 447443.125, so call it close enough.
//

#define PM_TIMER_MEASURING_TICK_COUNT 447443
#define PM_TIMER_MEASURING_FACTOR 8

//
// Define the time threshold between successive reads above which the PM timer
// is acting suspiciously like it might be broken. This is about 1.5 seconds,
// so even the worst of SMIs shouldn't trigger this.
//

#define PM_TIMER_SUSPICIOUS_JUMP_COUNT 0x555555

//
// Watch out for multipication overflow.
//

#define MULTIPLY_BY_1000000_MAX 0x10C6F7A0B5EDULL

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONGLONG
BopMeasureCycleCounterUsingPmTimer (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoArchMapNeededHardwareRegions (
    VOID
    )

/*++

Routine Description:

    This routine maps architecture-specific pieces of hardware needed for very
    early kernel initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
BoArchMeasureCycleCounter (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine attempts to measure the processor cycle counter.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None. The cycle counter frequency (or zero on failure) will be placed in
    the parameter block.

--*/

{

    ULONGLONG Begin;
    ULONGLONG Delta;
    ULONGLONG End;
    ULONGLONG Frequency;
    KSTATUS Status;

    Frequency = 0;

    //
    // Get the tubes warm with a practice read.
    //

    ArReadTimeStampCounter();
    Status = FwStall(X86_FIRMWARE_WARMUP_STALL_DURATION);
    if (!KSUCCESS(Status)) {
        goto ArchMeasureCycleCounterEnd;
    }

    //
    // Perform the real stall.
    //

    Begin = ArReadTimeStampCounter();
    Status = FwStall(X86_FIRMWARE_MEASURING_STALL_DURATION);
    if (!KSUCCESS(Status)) {
        goto ArchMeasureCycleCounterEnd;
    }

    End = ArReadTimeStampCounter();
    Delta = End - Begin;

    //
    // If the tick count is too small, then the firmware probably returned
    // immediately without actually stalling. Throw away the result.
    //

    if (Delta < X86_FIRMWARE_MINIMUM_TICK_DELTA ) {
        Status = STATUS_NOT_SUPPORTED;
        goto ArchMeasureCycleCounterEnd;
    }

    //
    // Determine the speed in Herts, watching out for overflow.
    //

    if (Delta >= MULTIPLY_BY_1000000_MAX) {
        Frequency = (Delta / X86_FIRMWARE_MEASURING_STALL_DURATION) *
                    MICROSECONDS_PER_SECOND;

    } else {
        Frequency = (Delta * MICROSECONDS_PER_SECOND) /
                    X86_FIRMWARE_MEASURING_STALL_DURATION;
    }

    Status = STATUS_SUCCESS;

ArchMeasureCycleCounterEnd:
    if (!KSUCCESS(Status)) {
        Frequency = BopMeasureCycleCounterUsingPmTimer();
    }

    Parameters->CycleCounterFrequency = Frequency;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONGLONG
BopMeasureCycleCounterUsingPmTimer (
    VOID
    )

/*++

Routine Description:

    This routine attempts to measure the processor cycle counter using the PM
    timer.

Arguments:

    None.

Return Value:

    Returns the cycle counter frequency on success.

    0 on failure.

--*/

{

    ULONGLONG Begin;
    ULONGLONG Delta;
    ULONGLONG End;
    PFADT Fadt;
    ULONGLONG Frequency;
    USHORT Port;
    ULONG PreviousTime;
    ULONG Time;
    ULONG TimeDelta;
    ULONG TimeSeen;
    ULONG Width;

    Fadt = BoGetAcpiTable(FADT_SIGNATURE, NULL);
    if (Fadt == NULL) {
        return 0;
    }

    Port = Fadt->PmTimerBlock;
    if (Port == 0) {
        return 0;
    }

    Width = 24;
    if ((Fadt->Flags & FADT_FLAG_PM_TIMER_32_BITS) != 0) {
        Width = 32;
    }

    TimeSeen = 0;

    //
    // Perform a warmup read of both timers.
    //

    ArReadTimeStampCounter();
    HlIoPortInLong(Port);
    HlIoPortInLong(Port);

    //
    // Perform the stall.
    //

    Begin = ArReadTimeStampCounter();
    PreviousTime = HlIoPortInLong(Port);
    while (TimeSeen < PM_TIMER_MEASURING_TICK_COUNT) {
        Time = HlIoPortInLong(Port);
        if (Width == 32) {
            TimeDelta = Time - PreviousTime;

        } else {
            if (Time >= PreviousTime) {
                TimeDelta = Time - PreviousTime;

            } else {
                TimeDelta = (0x1000000 - PreviousTime) + Time;
            }
        }

        //
        // In a weak attempt to not get completely boondoggled by broken
        // PM timers, throw out any delta that seems to be too big given that
        // this is a tight loop with interrupts disabled.
        //

        if (TimeDelta < PM_TIMER_SUSPICIOUS_JUMP_COUNT) {
            TimeSeen += TimeDelta;
        }

        PreviousTime = Time;
    }

    End = ArReadTimeStampCounter();
    Delta = End - Begin;
    Frequency = Delta * PM_TIMER_MEASURING_FACTOR;
    return Frequency;
}

