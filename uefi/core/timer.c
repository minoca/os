/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.c

Abstract:

    This module implements support for hardware timer services in the UEFI
    core, including the periodic tick and time counter.

Author:

    Evan Green 3-Mar-3014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

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
// Define timer services data.
//

UINT32 EfiClockTimerInterruptNumber;
EFI_PLATFORM_SERVICE_TIMER_INTERRUPT EfiClockTimerServiceRoutine;

EFI_PLATFORM_READ_TIMER EfiReadTimerRoutine;
UINT64 EfiReadTimerFrequency;
UINT32 EfiReadTimerWidth;

UINT64 EfiTimeCounterValue;
UINTN EfiClockInterruptCount;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfiCoreGetNextMonotonicCount (
    UINT64 *Count
    )

/*++

Routine Description:

    This routine returns a monotonically increasing count for the platform.

Arguments:

    Count - Supplies a pointer where the next count is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the count is NULL.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

{

    if (Count == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    if (EfiReadTimerRoutine == NULL) {
        return EFI_UNSUPPORTED;
    }

    *Count = EfiCoreReadTimeCounter();
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreStall (
    UINTN Microseconds
    )

/*++

Routine Description:

    This routine induces a fine-grained delay.

Arguments:

    Microseconds - Supplies the number of microseconds to stall execution for.

Return Value:

    EFI_SUCCESS on success.

--*/

{

    UINT64 CurrentTime;
    UINT64 EndTime;
    UINT64 Frequency;

    if (EfiReadTimerRoutine == NULL) {
        return EFI_UNSUPPORTED;
    }

    CurrentTime = EfiCoreReadTimeCounter();
    Frequency = EfiCoreGetTimeCounterFrequency();
    if (Frequency == 0) {
        return EFI_DEVICE_ERROR;
    }

    EndTime = CurrentTime + ((Microseconds * Frequency) / 1000000ULL);
    while (EfiCoreReadTimeCounter() < EndTime) {
        NOTHING;
    }

    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfiCoreSetWatchdogTimer (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    )

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

{

    EFI_STATUS Status;

    Status = EfiPlatformSetWatchdogTimer(Timeout,
                                         WatchdogCode,
                                         DataSize,
                                         WatchdogData);

    return Status;
}

UINT64
EfiCoreReadTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine reads the current time counter value.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value.

    0 if the time counter is not implemented.

--*/

{

    BOOLEAN Enabled;
    UINT64 HardwareMask;
    UINT64 HardwareValue;
    UINT64 HighBit;
    UINT64 Value;

    if (EfiReadTimerRoutine == NULL) {
        return 0;
    }

    Enabled = EfiDisableInterrupts();
    HardwareValue = EfiReadTimerRoutine();
    HardwareMask = (1ULL << EfiReadTimerWidth) - 1;
    HighBit = 1ULL << (EfiReadTimerWidth - 1);

    //
    // If the high bit flipped from one to zero, add one to the software
    // part.
    //

    if (((EfiTimeCounterValue & HighBit) != 0) &&
        ((HardwareValue & HighBit) == 0)) {

        EfiTimeCounterValue += 1ULL << EfiReadTimerWidth;
    }

    Value = (EfiTimeCounterValue & (~HardwareMask)) | HardwareValue;
    EfiTimeCounterValue = Value;
    if (Enabled != FALSE) {
        EfiEnableInterrupts();
    }

    return Value;
}

UINT64
EfiCoreReadRecentTimeCounter (
    VOID
    )

/*++

Routine Description:

    This routine reads a relatively recent but not entirely up to date version
    of the time counter.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value.

    0 if the time counter is not implemented.

--*/

{

    BOOLEAN Enabled;
    UINT64 Value;

    Enabled = EfiDisableInterrupts();
    Value = EfiTimeCounterValue;
    if (Enabled != FALSE) {
        EfiEnableInterrupts();
    }

    return Value;
}

UINT64
EfiCoreGetTimeCounterFrequency (
    VOID
    )

/*++

Routine Description:

    This routine returns the frequency of the time counter.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter.

    0 if the time counter is not implemented.

--*/

{

    return EfiReadTimerFrequency;
}

VOID
EfiCoreServiceClockInterrupt (
    UINT32 InterruptNumber
    )

/*++

Routine Description:

    This routine is called to service the clock interrupt.

Arguments:

    InterruptNumber - Supplies the interrupt number that fired.

Return Value:

    None.

--*/

{

    UINT64 NewTime;

    ASSERT(InterruptNumber == EfiClockTimerInterruptNumber);
    ASSERT(EfiClockTimerServiceRoutine != NULL);
    ASSERT(EfiAreInterruptsEnabled() == FALSE);

    EfiClockInterruptCount += 1;

    //
    // Read the time counter to keep it up to date.
    //

    NewTime = EfiCoreReadTimeCounter();
    EfiClockTimerServiceRoutine(InterruptNumber);
    EfipCoreTimerTick(NewTime);
    return;
}

EFI_STATUS
EfiCoreInitializeTimerServices (
    VOID
    )

/*++

Routine Description:

    This routine initializes platform timer services, including the periodic
    tick and time counter.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    BOOLEAN Enabled;
    EFI_STATUS Status;

    Enabled = EfiDisableInterrupts();
    Status = EfiPlatformInitializeTimers(&EfiClockTimerInterruptNumber,
                                         &EfiClockTimerServiceRoutine,
                                         &EfiReadTimerRoutine,
                                         &EfiReadTimerFrequency,
                                         &EfiReadTimerWidth);

    if (EFI_ERROR(Status)) {
        goto CoreInitializeTimerServicesEnd;
    }

    ASSERT((EfiReadTimerRoutine != NULL) &&
           (EfiReadTimerFrequency != 0) &&
           (EfiReadTimerWidth > 1));

    //
    // Perform an initial read of the counter to get a baseline.
    //

    EfiCoreReadTimeCounter();
    Status = EFI_SUCCESS;

CoreInitializeTimerServicesEnd:
    if (Enabled != FALSE) {
        EfiEnableInterrupts();
    }

    return Status;
}

VOID
EfiCoreTerminateTimerServices (
    VOID
    )

/*++

Routine Description:

    This routine terminates timer services in preparation for the termination
    of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfiPlatformTerminateTimers();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

