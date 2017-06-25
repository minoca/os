/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rtc.c

Abstract:

    This module implements the hardware module for the a timer based on the
    CMOS Real Time Counter (RTC).

Author:

    Chris Stevens 1-Jul-2013

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
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define RTC_ALLOCATION_TAG 0x21637452 // '!ctR'

//
// Define the default pre-programmed frequency of the RTC.
//

#define RTC_TIMER_FIXED_FREQUENCY 32768

//
// Define the RTC Interrupt timer's bit width. The counter value is not
// accessible. 16 is chosen because the maximum frequency is 32768 ticks per
// second. 16 bits holds that value.
//

#define RTC_TIMER_COUNTER_BIT_WIDTH 16

//
// Define the Global System Interrupt number of the RTC Interrupt. The RTC
// Interrupt is IRQ8 on the PIC.
//

#define RTC_INTERRUPT_GSI_NUMBER 8

//
// Define CMOS access ports.
//

#define CMOS_SELECT_PORT 0x70
#define CMOS_DATA_PORT 0x71

//
// Define values for the CMOS select port.
//

#define CMOS_NMI_SELECT         0x80
#define CMOS_REGISTER_SECOND    0x00
#define CMOS_REGISTER_MINUTE    0x02
#define CMOS_REGISTER_HOUR      0x04
#define CMOS_REGISTER_WEEKDAY   0x06
#define CMOS_REGISTER_DAY       0x07
#define CMOS_REGISTER_MONTH     0x08
#define CMOS_REGISTER_YEAR      0x09
#define CMOS_REGISTER_A         0x0A
#define CMOS_REGISTER_B         0x0B
#define CMOS_REGISTER_C         0x0C

//
// CMOS Register A values.
//

#define RTC_TIMER_MAX_DIVISOR 0xF
#define CMOS_REGISTER_A_RATE_MASK 0x0F
#define CMOS_REGISTER_A_UPDATE_IN_PROGRESS 0x80

//
// CMOS Register B values.
//

#define CMOS_REGISTER_B_24_HOUR 0x02
#define CMOS_REGISTER_B_BCD 0x04
#define CMOS_REGISTER_B_RTC_PERIODIC_INTERRUPT 0x40

//
// CMOS Register C values.
//

#define CMOS_REGISTER_C_RTC_PERIODIC_INTERRUPT 0x40
#define CMOS_REGISTER_C_RTC_INTERRUPT_FLAG 0x80

//
// Define the PM flag in the hour register.
//

#define CMOS_HOUR_PM 0x80

//
// Define the number of times to try to read or write the calendar time before
// giving up.
//

#define RTC_CALENDAR_TRY_COUNT 100

//
// Define the year dividing the twentieth and twenty first centuries for
// RTCs that do not have a century byte.
//

#define RTC_AUTOMATIC_CENTURY_YEAR 70

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the context used by an RTC module.

Members:

    Lock - Stores the high level lock that synchronizes access.

    CenturyRegister - Stores the CMOS register used for the century byte.

--*/

typedef struct _RTC_CONTEXT {
    HARDWARE_MODULE_LOCK Lock;
    UCHAR CenturyRegister;
} RTC_CONTEXT, *PRTC_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpRtcInitialize (
    PVOID Context
    );

KSTATUS
HlpRtcArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

VOID
HlpRtcDisarm (
    PVOID Context
    );

VOID
HlpRtcAcknowledgeInterrupt (
    PVOID Context
    );

KSTATUS
HlpRtcReadCalendarTime (
    PVOID Context,
    PHARDWARE_MODULE_TIME CurrentTime
    );

KSTATUS
HlpRtcWriteCalendarTime (
    PVOID Context,
    PHARDWARE_MODULE_TIME NewTime
    );

KSTATUS
HlpRtcSetInterruptFrequency (
    ULONG Frequency
    );

UCHAR
HlpRtcReadRegister (
    UCHAR Register
    );

VOID
HlpRtcWriteRegister (
    UCHAR Register,
    UCHAR Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpRtcModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the RTC hardware module. Its role is to
    report the RTC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    CALENDAR_TIMER_DESCRIPTION CalendarTimer;
    PRTC_CONTEXT Context;
    PFADT Fadt;
    TIMER_DESCRIPTION RtcTimer;
    KSTATUS Status;

    RtlZeroMemory(&RtcTimer, sizeof(TIMER_DESCRIPTION));
    Context = HlAllocateMemory(sizeof(RTC_CONTEXT),
                               RTC_ALLOCATION_TAG,
                               FALSE,
                               NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RtcModuleEntryEnd;
    }

    RtlZeroMemory(Context, sizeof(RTC_CONTEXT));
    HlInitializeLock(&(Context->Lock));
    RtcTimer.TableVersion = TIMER_DESCRIPTION_VERSION;
    RtcTimer.FunctionTable.Initialize = HlpRtcInitialize;
    RtcTimer.FunctionTable.ReadCounter = NULL;
    RtcTimer.FunctionTable.WriteCounter = NULL;
    RtcTimer.FunctionTable.Arm = HlpRtcArm;
    RtcTimer.FunctionTable.Disarm = HlpRtcDisarm;
    RtcTimer.FunctionTable.AcknowledgeInterrupt = HlpRtcAcknowledgeInterrupt;
    RtcTimer.Context = Context;
    RtcTimer.Features = TIMER_FEATURE_PERIODIC;

    //
    // The RTC timer runs at a fixed frequency. Arming the timer will slow down
    // the interrupt rate based on a divisor.
    //

    RtcTimer.CounterFrequency = RTC_TIMER_FIXED_FREQUENCY;

    //
    // The maximum frequency of the timer is 32768 Hz. Let this dictate the
    // counter bit width. The RTC interrupt counter is not accessible.
    //

    RtcTimer.CounterBitWidth = RTC_TIMER_COUNTER_BIT_WIDTH;

    //
    // The interrupt line for the RTC timer is IRQ8, this is GSI 8 of the I/O
    // APIC. Wire this up, preventing the need for an IDT entry for IRQ8.
    //

    RtcTimer.Interrupt.Line.Type = InterruptLineGsi;
    RtcTimer.Interrupt.Line.U.Gsi = RTC_INTERRUPT_GSI_NUMBER;
    RtcTimer.Interrupt.TriggerMode = InterruptModeUnknown;
    RtcTimer.Interrupt.ActiveLevel = InterruptActiveLevelUnknown;

    //
    // Register the RTC timer with the system.
    //

    Status = HlRegisterHardware(HardwareModuleTimer, &RtcTimer);
    if (!KSUCCESS(Status)) {
        goto RtcModuleEntryEnd;
    }

    //
    // Try to get the FADT to find the century register.
    //

    Fadt = HlGetAcpiTable(FADT_SIGNATURE, NULL);
    if (Fadt != NULL) {
        Context->CenturyRegister = Fadt->Century;
    }

    //
    // Register the calendar timer portion as well.
    //

    RtlZeroMemory(&CalendarTimer, sizeof(CALENDAR_TIMER_DESCRIPTION));
    CalendarTimer.TableVersion = CALENDAR_TIMER_DESCRIPTION_VERSION;
    CalendarTimer.Context = Context;
    CalendarTimer.Features = CALENDAR_TIMER_FEATURE_WANT_CALENDAR_FORMAT;
    CalendarTimer.FunctionTable.Read = HlpRtcReadCalendarTime;
    CalendarTimer.FunctionTable.Write = HlpRtcWriteCalendarTime;
    Status = HlRegisterHardware(HardwareModuleCalendarTimer, &CalendarTimer);
    if (!KSUCCESS(Status)) {
        goto RtcModuleEntryEnd;
    }

RtcModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpRtcInitialize (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes the RTC timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    Status code.

--*/

{

    //
    // The RTC is already running.
    //

    return STATUS_SUCCESS;
}

KSTATUS
HlpRtcArm (
    PVOID Context,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    )

/*++

Routine Description:

    This routine arms the timer to fire an interrupt after the specified number
    of ticks.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    Mode - Supplies the mode to arm the timer in. The system will never request
        a mode not supported by the timer's feature bits. The mode dictates
        how the tick count argument is interpreted.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in. In absolute mode, this supplies the time in timer ticks at
        which to fire an interrupt.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONGLONG Frequency;
    UCHAR OriginalSelection;
    BYTE RegisterB;
    PRTC_CONTEXT RtcContext;
    KSTATUS Status;

    RtcContext = (PRTC_CONTEXT)Context;
    if (Mode != TimerModePeriodic) {
        return STATUS_INVALID_PARAMETER;
    }

    if (TickCount == 0) {
        TickCount = 1;
    }

    HlAcquireLock(&(RtcContext->Lock));
    OriginalSelection = HlIoPortInByte(CMOS_SELECT_PORT);

    //
    // Set the RTC periodic interrupt frequency.
    //

    Frequency = RTC_TIMER_FIXED_FREQUENCY / TickCount;
    Status = HlpRtcSetInterruptFrequency(Frequency);
    if (!KSUCCESS(Status)) {
        goto RtcArmEnd;
    }

    //
    // Enable the RTC periodic interrupt by programming register B.
    //

    RegisterB = HlpRtcReadRegister(CMOS_REGISTER_B);
    RegisterB |= CMOS_REGISTER_B_RTC_PERIODIC_INTERRUPT;
    HlpRtcWriteRegister(CMOS_REGISTER_B, RegisterB);
    Status = STATUS_SUCCESS;

RtcArmEnd:
    HlIoPortOutByte(CMOS_SELECT_PORT, OriginalSelection);
    HlReleaseLock(&(RtcContext->Lock));
    return Status;
}

VOID
HlpRtcDisarm (
    PVOID Context
    )

/*++

Routine Description:

    This routine disarms the timer, stopping interrupts from firing.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    BYTE Mask;
    UCHAR OriginalSelection;
    BYTE RegisterB;
    BYTE RegisterC;
    PRTC_CONTEXT RtcContext;

    RtcContext = (PRTC_CONTEXT)Context;

    //
    // Disable the RTC periodic interrupt by programming register B.
    //

    HlAcquireLock(&(RtcContext->Lock));
    OriginalSelection = HlIoPortInByte(CMOS_SELECT_PORT);
    RegisterB = HlpRtcReadRegister(CMOS_REGISTER_B);
    RegisterB &= ~CMOS_REGISTER_B_RTC_PERIODIC_INTERRUPT;
    HlpRtcWriteRegister(CMOS_REGISTER_B, RegisterB);

    //
    // Loop until the periodic timer is disabled.
    //

    Mask = CMOS_REGISTER_C_RTC_PERIODIC_INTERRUPT;
    Mask |= CMOS_REGISTER_C_RTC_INTERRUPT_FLAG;
    while (TRUE) {
        RegisterC = HlpRtcReadRegister(CMOS_REGISTER_C);
        if ((RegisterC & Mask) == 0) {
            break;
        }
    }

    HlIoPortOutByte(CMOS_SELECT_PORT, OriginalSelection);
    HlReleaseLock(&(RtcContext->Lock));
    return;
}

VOID
HlpRtcAcknowledgeInterrupt (
    PVOID Context
    )

/*++

Routine Description:

    This routine performs any actions necessary upon reciept of a timer's
    interrupt. This may involve writing to an acknowledge register to re-enable
    the timer to fire again, or other hardware specific actions.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    PRTC_CONTEXT RtcContext;

    RtcContext = (PRTC_CONTEXT)Context;

    //
    // Read register C to acknowledge the interrupt and re-enable it.
    //

    HlAcquireLock(&(RtcContext->Lock));
    HlIoPortOutByte(CMOS_SELECT_PORT, CMOS_REGISTER_C);
    HlIoPortInByte(CMOS_DATA_PORT);
    HlReleaseLock(&(RtcContext->Lock));
    return;
}

KSTATUS
HlpRtcReadCalendarTime (
    PVOID Context,
    PHARDWARE_MODULE_TIME CurrentTime
    )

/*++

Routine Description:

    This routine returns the calendar timer's current value.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    CurrentTime - Supplies a pointer where the read current time will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PCALENDAR_TIME CalendarTime;
    UCHAR Century;
    UCHAR Century2;
    UCHAR Day;
    UCHAR Day2;
    UCHAR Hour;
    UCHAR Hour2;
    UCHAR Minute;
    UCHAR Minute2;
    UCHAR Month;
    UCHAR Month2;
    UCHAR OriginalSelection;
    UCHAR RegisterB;
    PRTC_CONTEXT RtcContext;
    UCHAR Second;
    UCHAR Second2;
    KSTATUS Status;
    ULONG Try;
    UCHAR Weekday;
    UCHAR Weekday2;
    UCHAR Year;
    UCHAR Year2;

    Century = 0;
    Century2 = 0;
    RtcContext = (PRTC_CONTEXT)Context;
    HlAcquireLock(&(RtcContext->Lock));
    OriginalSelection = HlIoPortInByte(CMOS_SELECT_PORT);
    RegisterB = HlpRtcReadRegister(CMOS_REGISTER_B);

    //
    // Loop trying this procedure until it comes out right.
    //

    Status = STATUS_DEVICE_IO_ERROR;
    for (Try = 0; Try < RTC_CALENDAR_TRY_COUNT; Try += 1) {

        //
        // Wait for the update in progress bit to go down on the off chance it's
        // updating now.
        //

        while ((HlpRtcReadRegister(CMOS_REGISTER_A) &
               CMOS_REGISTER_A_UPDATE_IN_PROGRESS) != 0) {

            NOTHING;
        }

        Second = HlpRtcReadRegister(CMOS_REGISTER_SECOND);
        Minute = HlpRtcReadRegister(CMOS_REGISTER_MINUTE);
        Hour = HlpRtcReadRegister(CMOS_REGISTER_HOUR);
        Day = HlpRtcReadRegister(CMOS_REGISTER_DAY);
        Weekday = HlpRtcReadRegister(CMOS_REGISTER_WEEKDAY);
        Month = HlpRtcReadRegister(CMOS_REGISTER_MONTH);
        Year = HlpRtcReadRegister(CMOS_REGISTER_YEAR);
        if (RtcContext->CenturyRegister != 0) {
            Century = HlpRtcReadRegister(RtcContext->CenturyRegister);
        }

        //
        // Now read it all again to make sure the update didn't occur in the
        // middle of the read.
        //

        while ((HlpRtcReadRegister(CMOS_REGISTER_A) &
               CMOS_REGISTER_A_UPDATE_IN_PROGRESS) != 0) {

            NOTHING;
        }

        Second2 = HlpRtcReadRegister(CMOS_REGISTER_SECOND);
        Minute2 = HlpRtcReadRegister(CMOS_REGISTER_MINUTE);
        Hour2 = HlpRtcReadRegister(CMOS_REGISTER_HOUR);
        Day2 = HlpRtcReadRegister(CMOS_REGISTER_DAY);
        Weekday2 = HlpRtcReadRegister(CMOS_REGISTER_WEEKDAY);
        Month2 = HlpRtcReadRegister(CMOS_REGISTER_MONTH);
        Year2 = HlpRtcReadRegister(CMOS_REGISTER_YEAR);
        if (RtcContext->CenturyRegister != 0) {
            Century2 = HlpRtcReadRegister(RtcContext->CenturyRegister);
        }

        if ((Second == Second2) && (Minute == Minute2) && (Hour == Hour2) &&
            (Day == Day2) && (Weekday == Weekday2) && (Month == Month2) &&
            (Year == Year2) && (Century == Century2)) {

            Status = STATUS_SUCCESS;
            break;
        }

    }

    if (!KSUCCESS(Status)) {
        goto ReadCalendarTimeEnd;
    }

    CurrentTime->IsCalendarTime = TRUE;
    CalendarTime = &(CurrentTime->U.CalendarTime);
    CalendarTime->Second = BCD_TO_BINARY(Second);
    CalendarTime->Minute = BCD_TO_BINARY(Minute);
    CalendarTime->Weekday = BCD_TO_BINARY(Weekday);
    CalendarTime->Month = BCD_TO_BINARY(Month) - 1;
    CalendarTime->Day = BCD_TO_BINARY(Day);

    //
    // The year is just a two digit year. If a century was read then add that
    // in as well. If there was no century register then guess based on the
    // two digit year.
    //

    Year = BCD_TO_BINARY(Year);
    CalendarTime->Year = Year + (100 * BCD_TO_BINARY(Century));
    if (RtcContext->CenturyRegister == 0) {
        CalendarTime->Year += 1900;
        if (Year < RTC_AUTOMATIC_CENTURY_YEAR) {
            CalendarTime->Year += 100;
        }
    }

    //
    // 12 hour mode is a bit of a pain. Midnight is represented as 12, then
    // noon through 11pm is represented by the hour plus the highest bit.
    //

    if ((RegisterB & CMOS_REGISTER_B_24_HOUR) == 0) {
        if (BCD_TO_BINARY(Hour) == 12) {
            Hour = 0;

        } else if ((Hour & CMOS_HOUR_PM) != 0) {
            Hour &= ~CMOS_HOUR_PM;
            Hour = 12 + BCD_TO_BINARY(Hour);

        } else {
            Hour = BCD_TO_BINARY(Hour);
        }

    } else {
        Hour = BCD_TO_BINARY(Hour);
    }

    CalendarTime->Hour = Hour;
    Status = STATUS_SUCCESS;

ReadCalendarTimeEnd:
    HlIoPortOutByte(CMOS_SELECT_PORT, OriginalSelection);
    HlReleaseLock(&(RtcContext->Lock));
    return Status;
}

KSTATUS
HlpRtcWriteCalendarTime (
    PVOID Context,
    PHARDWARE_MODULE_TIME NewTime
    )

/*++

Routine Description:

    This routine writes to the calendar timer.

Arguments:

    Context - Supplies the pointer to the timer's context, provided by the
        hardware module upon initialization.

    NewTime - Supplies a pointer to the new time to set. The hardware module
        should set this as quickly as possible. The system will supply either
        a calendar time or a system time in here based on which type the timer
        requested at registration.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PCALENDAR_TIME CalendarTime;
    UCHAR Century;
    UCHAR Century2;
    UCHAR Day;
    UCHAR Day2;
    UCHAR Hour;
    UCHAR Hour2;
    UCHAR Minute;
    UCHAR Minute2;
    UCHAR Month;
    UCHAR Month2;
    UCHAR OriginalSelection;
    UCHAR RegisterB;
    PRTC_CONTEXT RtcContext;
    UCHAR Second;
    UCHAR Second2;
    KSTATUS Status;
    ULONG Try;
    UCHAR Weekday;
    UCHAR Weekday2;
    UCHAR Year;
    UCHAR Year2;

    RtcContext = (PRTC_CONTEXT)Context;
    if (NewTime->IsCalendarTime == FALSE) {
        return STATUS_NOT_SUPPORTED;
    }

    CalendarTime = &(NewTime->U.CalendarTime);
    Second = BINARY_TO_BCD(CalendarTime->Second);
    Minute = BINARY_TO_BCD(CalendarTime->Minute);
    Hour = BINARY_TO_BCD(CalendarTime->Hour);
    Weekday = BINARY_TO_BCD(CalendarTime->Weekday);
    Day = BINARY_TO_BCD(CalendarTime->Day);
    Month = BINARY_TO_BCD(CalendarTime->Month + 1);
    Year = BINARY_TO_BCD(CalendarTime->Year % 100);
    Century = BINARY_TO_BCD(CalendarTime->Year / 100);
    Century2 = Century;
    HlAcquireLock(&(RtcContext->Lock));
    OriginalSelection = HlIoPortInByte(CMOS_SELECT_PORT);
    RegisterB = HlpRtcReadRegister(CMOS_REGISTER_B);

    //
    // Convert the hour to 12 hour time if necessary.
    //

    if ((RegisterB & CMOS_REGISTER_B_24_HOUR) == 0) {
        if (CalendarTime->Hour == 0) {
            Hour = BINARY_TO_BCD(12);

        } else if (CalendarTime->Hour == 12) {
            Hour = BINARY_TO_BCD(12) | CMOS_HOUR_PM;

        } else if (CalendarTime->Hour > 12) {
            Hour = BINARY_TO_BCD(CalendarTime->Hour - 12) | CMOS_HOUR_PM;
        }
    }

    //
    // Loop trying this procedure until it comes out right.
    //

    Status = STATUS_DEVICE_IO_ERROR;
    for (Try = 0; Try < RTC_CALENDAR_TRY_COUNT; Try += 1) {

        //
        // Wait for the update in progress bit to go down on the off chance it's
        // updating now.
        //

        while ((HlpRtcReadRegister(CMOS_REGISTER_A) &
               CMOS_REGISTER_A_UPDATE_IN_PROGRESS) != 0) {

            NOTHING;
        }

        //
        // Write in the new time.
        //

        HlpRtcWriteRegister(CMOS_REGISTER_SECOND, Second);
        HlpRtcWriteRegister(CMOS_REGISTER_MINUTE, Minute);
        HlpRtcWriteRegister(CMOS_REGISTER_HOUR, Hour);
        HlpRtcWriteRegister(CMOS_REGISTER_DAY, Day);
        HlpRtcWriteRegister(CMOS_REGISTER_WEEKDAY, Weekday);
        HlpRtcWriteRegister(CMOS_REGISTER_MONTH, Month);
        HlpRtcWriteRegister(CMOS_REGISTER_YEAR, Year);
        if (RtcContext->CenturyRegister != 0) {
            HlpRtcWriteRegister(RtcContext->CenturyRegister, Century);
        }

        //
        // Now read it all to make sure the complete value was written.
        //

        while ((HlpRtcReadRegister(CMOS_REGISTER_A) &
               CMOS_REGISTER_A_UPDATE_IN_PROGRESS) != 0) {

            NOTHING;
        }

        Second2 = HlpRtcReadRegister(CMOS_REGISTER_SECOND);
        Minute2 = HlpRtcReadRegister(CMOS_REGISTER_MINUTE);
        Hour2 = HlpRtcReadRegister(CMOS_REGISTER_HOUR);
        Day2 = HlpRtcReadRegister(CMOS_REGISTER_DAY);
        Weekday2 = HlpRtcReadRegister(CMOS_REGISTER_WEEKDAY);
        Month2 = HlpRtcReadRegister(CMOS_REGISTER_MONTH);
        Year2 = HlpRtcReadRegister(CMOS_REGISTER_YEAR);
        if (RtcContext->CenturyRegister != 0) {
            Century2 = HlpRtcReadRegister(RtcContext->CenturyRegister);
        }

        if ((Second == Second2) && (Minute == Minute2) && (Hour == Hour2) &&
            (Day == Day2) && (Weekday == Weekday2) && (Month == Month2) &&
            (Year == Year2) && (Century == Century2)) {

            Status = STATUS_SUCCESS;
            break;
        }

    }

    if (!KSUCCESS(Status)) {
        goto ReadCalendarTimeEnd;
    }

ReadCalendarTimeEnd:
    HlIoPortOutByte(CMOS_SELECT_PORT, OriginalSelection);
    HlReleaseLock(&(RtcContext->Lock));
    return Status;
}

KSTATUS
HlpRtcSetInterruptFrequency (
    ULONG Frequency
    )

/*++

Routine Description:

    This routine sets the frequency of the RTC interrupts. This routine assumes
    the RTC lock is already held.

Arguments:

    Frequency - Supplies the desired interrupt frequency for the RTC timer.

Return Value:

    Status code.

--*/

{

    ULONG Quotient;
    BYTE Rate;
    BYTE RegisterA;

    //
    // Do not allow invalid input.
    //

    if (Frequency > RTC_TIMER_FIXED_FREQUENCY) {
        Frequency = RTC_TIMER_FIXED_FREQUENCY;
    }

    //
    // Convert the frequency to a valid register rate. A frequency of 0
    // sets the RTC to its slowest rate.
    //
    // The formula for the rate that gets programmed in the regiser is:
    //
    //     Rate = log2(TimerFrequency / Frequency) + 1
    //

    Rate = 0;
    if (Frequency != 0) {
        Quotient = RTC_TIMER_FIXED_FREQUENCY / Frequency;
        while (Quotient != 0) {
            Rate += 1;
            Quotient >>= 1;
        }

        if (RTC_TIMER_FIXED_FREQUENCY >> (Rate - 1) < Frequency) {
            Rate += 1;
        }

    } else {
        Rate = RTC_TIMER_MAX_DIVISOR;
    }

    if (Rate > RTC_TIMER_MAX_DIVISOR) {
        Rate = RTC_TIMER_MAX_DIVISOR;
    }

    //
    // Write the new rate to CMOS RTC.
    //

    RegisterA = HlpRtcReadRegister(CMOS_REGISTER_A);
    RegisterA = (RegisterA & ~CMOS_REGISTER_A_RATE_MASK) | Rate;
    HlpRtcWriteRegister(CMOS_REGISTER_A, RegisterA);
    return STATUS_SUCCESS;
}

UCHAR
HlpRtcReadRegister (
    UCHAR Register
    )

/*++

Routine Description:

    This routine reads an RTC register.

Arguments:

    Register - Supplies the register to read.

Return Value:

    Returns the value of the selected register.

--*/

{

    HlIoPortOutByte(CMOS_SELECT_PORT, Register | CMOS_NMI_SELECT);
    return HlIoPortInByte(CMOS_DATA_PORT);
}

VOID
HlpRtcWriteRegister (
    UCHAR Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes an RTC register.

Arguments:

    Register - Supplies the register to write.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    HlIoPortOutByte(CMOS_SELECT_PORT, Register | CMOS_NMI_SELECT);
    HlIoPortOutByte(CMOS_DATA_PORT, Value);
    return;
}

