/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtc.c

Abstract:

    This module implements support for speaking to the RTC module on the
    AM335x SoC.

Author:

    Evan Green 23-Sep-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <minoca/soc/am335x.h>
#include "../bbonefw.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_RTC(_Register) \
        *(volatile UINT32 *)(EfiAm335RtcBase + (_Register))

#define AM3_WRITE_RTC(_Register, _Value) \
        *((volatile UINT32 *)(EfiAm335RtcBase + (_Register))) = (_Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default cutoff year guess between the twentieth and twenty first
// century.
//

#define AM3_CENTURY_CUTOFF_YEAR 70

//
// Define some constants to stuff into scratch 0 that indicate the time zone
// minutes are being stored there.
//

#define AM3_SCRATCH0_MAGIC 0x5F4A0000
#define AM3_SCRATCH0_MAGIC_MASK 0xFFFF0000
#define AM3_SCRATCH0_TIME_ZONE_MASK 0x0000FFFF

//
// Define some constants to stuff into scratch 1 that indicate the daylight bit
// is stored there.
//

#define AM3_SCRATCH1_MAGIC 0xB13C0000
#define AM3_SCRATCH1_MAGIC_MASK 0xFFFF0000
#define AM3_SCRATCH1_DAYLIGHT 0x00008000
#define AM3_SCRATCH1_CENTURY_MASK 0x000000FF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipAm335WaitForNonBusyEdge (
    VOID
    );

VOID
EfipAm335LockRtc (
    BOOLEAN Lock
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the pointer to the RTC base, which will get virtualized when going
// to runtime.
//

VOID *EfiAm335RtcBase = (VOID *)AM335_RTC_BASE;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
EfipAm335GetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    )

/*++

Routine Description:

    This routine returns the current time and dat information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    UINT32 Control;
    UINT32 Value;
    UINT32 Year;

    if (Capabilities != NULL) {
        Capabilities->Resolution = 1;
        Capabilities->Accuracy = 0;
        Capabilities->SetsToZero = FALSE;
    }

    Control = AM3_READ_RTC(Am335RtcControl);

    //
    // The RTC cannot be turned back on once it's off.
    //

    if ((Control & AM335_RTC_CONTROL_RTC_DISABLE) != 0) {
        return EFI_DEVICE_ERROR;
    }

    //
    // Values are in BCD, and all values snap as soon as the seconds register
    // is read, so there's no need to worry about tearing.
    //

    Value = AM3_READ_RTC(Am335RtcSeconds);
    Time->Second = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcMinutes);
    Time->Minute = EFI_BCD_TO_BINARY(Value);

    //
    // Handle post meridiem, which have value in the range 1-12, or 24-hour
    // mode, which will just be 0-23.
    //

    Value = AM3_READ_RTC(Am335RtcHours);
    Time->Hour = EFI_BCD_TO_BINARY(Value & ~AM335_RTC_HOURS_PM);
    if ((Control & AM335_RTC_CONTROL_12_HOUR_MODE) != 0) {
        if (Time->Hour == 12) {
            Time->Hour = 0;
        }

        if ((Value & AM335_RTC_HOURS_PM) != 0) {
            Time->Hour += 12;
        }
    }

    Value = AM3_READ_RTC(Am335RtcDays);
    Time->Day = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcMonths);
    Time->Month = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcYears);
    Year = EFI_BCD_TO_BINARY(Value);

    //
    // The time zone might be stored in scratch 0 if it was this firmware that
    // wrote it last.
    //

    Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
    Value = AM3_READ_RTC(Am335RtcScratch0);
    if ((Value & AM3_SCRATCH0_MAGIC_MASK) == AM3_SCRATCH0_MAGIC) {
        Time->TimeZone = (INT16)(Value & AM3_SCRATCH0_TIME_ZONE_MASK);
    }

    //
    // The daylight bit and century might be stored in scratch 1.
    //

    Time->Daylight = 0;
    Value = AM3_READ_RTC(Am335RtcScratch1);
    if ((Value & AM3_SCRATCH1_MAGIC_MASK) == AM3_SCRATCH1_MAGIC) {
        if ((Value & AM3_SCRATCH1_DAYLIGHT) != 0) {
            Time->Daylight = 1;
        }

        Time->Year = ((Value & AM3_SCRATCH1_CENTURY_MASK) * 100) + Year;

    //
    // Scratch 1 doesn't have any known data in it, so take a guess.
    //

    } else {
        if (Year >= AM3_CENTURY_CUTOFF_YEAR) {
            Time->Year = 1900 + Year;

        } else {
            Time->Year = 2000 + Year;
        }
    }

    Time->Nanosecond = 0;
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipAm335SetTime (
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine sets the current local time and date information.

Arguments:

    Time - Supplies a pointer to the time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    UINT32 Century;
    UINT32 Control;
    UINT32 Status;
    UINT32 Value;
    UINT32 Year;

    Control = AM3_READ_RTC(Am335RtcControl);
    if ((Control & AM335_RTC_CONTROL_RTC_DISABLE) != 0) {
        return EFI_DEVICE_ERROR;
    }

    EfipAm335LockRtc(FALSE);

    //
    // Stop the clock, wait for it to really stop, program the time,
    // start the clock.
    //

    Control = 0;
    AM3_WRITE_RTC(Am335RtcControl, 0);
    do {
        Status = AM3_READ_RTC(Am335RtcStatus);

    } while ((Status & AM335_RTC_STATUS_RUN) != 0);

    Century = 19;
    Year = Time->Year - 1900;
    while (Year >= 100) {
        Century += 1;
        Year -= 100;
    }

    Value = AM3_SCRATCH0_MAGIC | (Time->TimeZone & AM3_SCRATCH0_TIME_ZONE_MASK);
    AM3_WRITE_RTC(Am335RtcScratch0, Value);
    Value = AM3_SCRATCH1_MAGIC | (Century & AM3_SCRATCH1_CENTURY_MASK);
    if (Time->Daylight != 0) {
        Value |= AM3_SCRATCH1_DAYLIGHT;
    }

    AM3_WRITE_RTC(Am335RtcScratch1, Value);
    AM3_WRITE_RTC(Am335RtcYears, EFI_BINARY_TO_BCD(Year));
    AM3_WRITE_RTC(Am335RtcMonths, EFI_BINARY_TO_BCD(Time->Month));
    AM3_WRITE_RTC(Am335RtcDays, EFI_BINARY_TO_BCD(Time->Day));
    AM3_WRITE_RTC(Am335RtcHours, EFI_BINARY_TO_BCD(Time->Hour));
    AM3_WRITE_RTC(Am335RtcMinutes, EFI_BINARY_TO_BCD(Time->Minute));
    AM3_WRITE_RTC(Am335RtcSeconds, EFI_BINARY_TO_BCD(Time->Second));
    Control = AM335_RTC_CONTROL_RUN;
    AM3_WRITE_RTC(Am335RtcControl, Control);
    do {
        Status = AM3_READ_RTC(Am335RtcStatus);

    } while ((Status & AM335_RTC_STATUS_RUN) == 0);

    EfipAm335LockRtc(TRUE);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipAm335GetWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine gets the current wake alarm setting.

Arguments:

    Enabled - Supplies a pointer that receives a boolean indicating if the
        alarm is currently enabled or disabled.

    Pending - Supplies a pointer that receives a boolean indicating if the
        alarm signal is pending and requires acknowledgement.

    Time - Supplies a pointer that receives the current wake time.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    UINT32 Control;
    UINT32 Interrupts;
    UINT32 Status;
    UINT32 Value;
    UINT32 Year;

    Control = AM3_READ_RTC(Am335RtcControl);
    if ((Control & AM335_RTC_CONTROL_RTC_DISABLE) != 0) {
        return EFI_DEVICE_ERROR;
    }

    //
    // Unlock the RTC for that one potential access to clear the status bit.
    //

    EfipAm335LockRtc(FALSE);
    EfipAm335WaitForNonBusyEdge();

    //
    // Values are in BCD, and all values snap as soon as the seconds register
    // is read, so there's no need to worry about tearing.
    //

    Value = AM3_READ_RTC(Am335RtcAlarmSeconds);
    Time->Second = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcAlarmMinutes);
    Time->Minute = EFI_BCD_TO_BINARY(Value);

    //
    // Handle post meridiem, which have value in the range 1-12, or 24-hour
    // mode, which will just be 0-23.
    //

    Value = AM3_READ_RTC(Am335RtcAlarmHours);
    Time->Hour = EFI_BCD_TO_BINARY(Value & ~AM335_RTC_HOURS_PM);
    if ((Control & AM335_RTC_CONTROL_12_HOUR_MODE) != 0) {
        if (Time->Hour == 12) {
            Time->Hour = 0;
        }

        if ((Value & AM335_RTC_HOURS_PM) != 0) {
            Time->Hour += 12;
        }
    }

    Value = AM3_READ_RTC(Am335RtcAlarmDays);
    Time->Day = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcAlarmMonths);
    Time->Month = EFI_BCD_TO_BINARY(Value);
    Value = AM3_READ_RTC(Am335RtcAlarmYears);
    Year = EFI_BCD_TO_BINARY(Value);
    Time->TimeZone = EFI_UNSPECIFIED_TIMEZONE;
    Time->Daylight = 0;
    if (Year >= AM3_CENTURY_CUTOFF_YEAR) {
        Time->Year = 1900 + Year;

    } else {
        Time->Year = 2000 + Year;
    }

    Time->Nanosecond = 0;
    Interrupts = AM3_READ_RTC(Am335RtcInterruptEnable);
    *Enabled = FALSE;
    if ((Interrupts & AM335_RTC_INTERRUPT_ALARM) != 0) {
        *Enabled = TRUE;
    }

    Status = AM3_READ_RTC(Am335RtcStatus);
    *Pending = FALSE;
    if ((Status & AM335_RTC_STATUS_ALARM) != 0) {
        *Pending = TRUE;
        Status &= ~AM335_RTC_STATUS_ALARM;
        AM3_WRITE_RTC(Am335RtcStatus, Status);
    }

    EfipAm335LockRtc(FALSE);
    return EFI_SUCCESS;
}

EFIAPI
EFI_STATUS
EfipAm335SetWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    )

/*++

Routine Description:

    This routine sets the current wake alarm setting.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

{

    UINT32 Control;
    UINT32 Hour;
    UINT32 Interrupts;
    UINT32 Year;

    Control = AM3_READ_RTC(Am335RtcControl);
    if ((Control & AM335_RTC_CONTROL_RTC_DISABLE) != 0) {
        return EFI_DEVICE_ERROR;
    }

    if ((Control & AM335_RTC_CONTROL_12_HOUR_MODE) != 0) {
        Hour = Time->Hour;
        if (Hour == 0) {
            Hour = EFI_BINARY_TO_BCD(12);

        } else if (Hour >= 12) {
            Hour = EFI_BINARY_TO_BCD(Hour - 12);
            Hour |= AM335_RTC_HOURS_PM;
        }

    } else {
        Hour = EFI_BINARY_TO_BCD(Time->Hour);
    }

    Year = Time->Year - 1900;
    while (Year >= 100) {
        Year -= 100;
    }

    EfipAm335LockRtc(FALSE);
    EfipAm335WaitForNonBusyEdge();
    AM3_WRITE_RTC(Am335RtcAlarmYears, EFI_BINARY_TO_BCD(Year));
    AM3_WRITE_RTC(Am335RtcAlarmMonths, EFI_BINARY_TO_BCD(Time->Month));
    AM3_WRITE_RTC(Am335RtcAlarmDays, EFI_BINARY_TO_BCD(Time->Day));
    AM3_WRITE_RTC(Am335RtcAlarmHours, Hour);
    AM3_WRITE_RTC(Am335RtcAlarmMinutes, EFI_BINARY_TO_BCD(Time->Minute));
    AM3_WRITE_RTC(Am335RtcAlarmSeconds, EFI_BINARY_TO_BCD(Time->Second));
    Interrupts = AM3_READ_RTC(Am335RtcInterruptEnable);
    Interrupts &= ~AM335_RTC_INTERRUPT_ALARM;
    if (Enable != FALSE) {
        Interrupts |= AM335_RTC_INTERRUPT_ALARM;
    }

    AM3_WRITE_RTC(Am335RtcInterruptEnable, Interrupts);
    EfipAm335LockRtc(TRUE);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipAm335WaitForNonBusyEdge (
    VOID
    )

/*++

Routine Description:

    This routine waits for the falling edge of a the busy bit in the RTC. This
    could take up to two seconds if a falling edge was just missed.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Status;

    //
    // The busy bit will never go up if it's not running.
    //

    Status = AM3_READ_RTC(Am335RtcStatus);
    if ((Status & AM335_RTC_STATUS_RUN) == 0) {
        return;
    }

    //
    // Wait for a the busy bit to go high so that the start of a falling edge
    // can be observed.
    //

    do {
        Status = AM3_READ_RTC(Am335RtcStatus);

    } while ((Status & AM335_RTC_STATUS_BUSY) == 0);

    //
    // Now wait for a falling edge.
    //

    do {
        Status = AM3_READ_RTC(Am335RtcStatus);

    } while ((Status & AM335_RTC_STATUS_BUSY) != 0);

    return;
}

VOID
EfipAm335LockRtc (
    BOOLEAN Lock
    )

/*++

Routine Description:

    This routine locks or unlocks write access to the RTC.

Arguments:

    Lock - Supplies a boolean indicating whether to prevent write access (TRUE)
        or allow it (FALSE).

Return Value:

    None.

--*/

{

    //
    // To lock it, write the correct kick 0 value, but the incorrect kick 1
    // value. According to the state machine diagram, that's the best way to
    // get to locked, even if the current state is somehow unlocked.
    //

    AM3_WRITE_RTC(Am335RtcKick0, AM335_RTC_KICK0_KEY);
    if (Lock != FALSE) {
        AM3_WRITE_RTC(Am335RtcKick1, 0xFFFFFFFF);

    //
    // Write the correct kick value to unlock the RTC.
    //

    } else {
        AM3_WRITE_RTC(Am335RtcKick1, AM335_RTC_KICK1_KEY);
    }

    return;
}

