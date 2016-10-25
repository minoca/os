/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    timer.h

Abstract:

    This header contains definitions for the timer library of the hardware
    layer subsystem. These definitions are internal to the hardware library.

Author:

    Evan Green 28-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Hardware timer flags.
//

//
// This flag is set once the timer has been successfully initialized.
//

#define TIMER_FLAG_INITIALIZED 0x00000001

//
// This flag is set if the timer has failed its initialization process.
//

#define TIMER_FLAG_FAILED 0x00000002

//
// This flag is set if the timer is in use backing a system interrupt. Timers
// in use for interrupts cannot be shared for any other purpose.
//

#define TIMER_FLAG_IN_USE_FOR_INTERRUPT 0x00000004

//
// This flag is set if the timer is in use backing a system counter. A timer
// used for counting can back multiple system services.
//

#define TIMER_FLAG_IN_USE_FOR_COUNTER 0x00000008

//
// The flag is set if the timer does not appear to be ticking.
//

#define TIMER_FLAG_NOT_TICKING 0x00000010

//
// --------------------------------------------------------------------- Macros
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to make a periodic timer out of
    a non-periodic, absolute timer.

Members:

    DueTime - Stores the current absolute time the timer is due to interrupt.

    Period - Stores the periodic rate, in time ticks, at which the timer should
        interrupt.

--*/

typedef struct _HARDWARE_TIMER_ABSOLUTE_DATA {
    ULONGLONG DueTime;
    ULONGLONG Period;
} HARDWARE_TIMER_ABSOLUTE_DATA, *PHARDWARE_TIMER_ABSOLUTE_DATA;

/*++

Structure Description:

    This structure defines a timer that has been registered with the system.

Members:

    ListEntry - Stores pointers to the next and previous timers in the system.

    CurrentCount - Stores the current state of the software rollover bits, as
        well as the current state of the most significant bit of the hardware
        counter. This member must be properly aligned to perform 64-bit atomic
        compare exchanges.

    CurrentCounts - Stores a pointer to an array of current count variables for
        timers that vary by processor. If this array is non-null, it replaces
        the current count value above.

    FunctionTable - Stores pointers to functions implemented by the hardware
        module abstracting this timer.

    Flags - Stores pointers to a bitfield of flags defining state of the
        controller. See TIMER_FLAG_* definitions.

    Identifier - Stores the unique hardware identifier of the timer.

    PrivateContext - Stores a pointer to the hardware module's private
        context.

    Features - Stores the features bits of this timer. See TIMER_FEATURE_*
        definitions.

    CounterFrequency - Stores the frequency of the counter, in Hertz.

    CounterBitWidth - Stores the number of bits implemented by the hardware
        counter.

    Interrupt - Stores information about the timer's interrupt, if applicable.

    InterruptRunLevel - Stores the execution run level for the ISR of the
        interrupt associated with this timer.

    SoftwareOffset - Stores a 64-bit software bias to apply to all readings.

    AbsoluteData - Stores an optional array of data for non-periodic absolute
        timers. There is one array entry of absolute data for each processor.

--*/

typedef struct _HARDWARE_TIMER {
    LIST_ENTRY ListEntry;
    volatile ULONGLONG CurrentCount;
    volatile ULONGLONG *CurrentCounts;
    TIMER_FUNCTION_TABLE FunctionTable;
    ULONG Identifier;
    ULONG Flags;
    PVOID PrivateContext;
    ULONG Features;
    ULONGLONG CounterFrequency;
    ULONG CounterBitWidth;
    TIMER_INTERRUPT Interrupt;
    RUNLEVEL InterruptRunLevel;
    INT64_SYNC SoftwareOffset;
    PHARDWARE_TIMER_ABSOLUTE_DATA AbsoluteData;
} HARDWARE_TIMER, *PHARDWARE_TIMER;

//
// -------------------------------------------------------------------- Globals
//

//
// Store pointers to the timers backing system services.
//

extern PHARDWARE_TIMER HlClockTimer;
extern PHARDWARE_TIMER HlProfilerTimer;
extern PHARDWARE_TIMER HlTimeCounter;
extern PHARDWARE_TIMER HlProcessorCounter;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HlpTimerRegisterHardware (
    PTIMER_DESCRIPTION TimerDescription
    );

/*++

Routine Description:

    This routine is called to register a new timer with the system.

Arguments:

    TimerDescription - Supplies a pointer to a structure describing the new
        timer.

Return Value:

    Status code.

--*/

KSTATUS
HlpTimerArm (
    PHARDWARE_TIMER Timer,
    TIMER_MODE Mode,
    ULONGLONG TickCount
    );

/*++

Routine Description:

    This routine arms a timer to fire an interrupt after the given interval.

Arguments:

    Timer - Supplies a pointer to the timer to arm.

    Mode - Supplies the mode to arm the timer in. The system will never request
        a mode not supported by the timer's feature bits. The mode dictates
        how the tick count argument is interpreted.

    TickCount - Supplies the number of timer ticks from now for the timer to
        fire in. In absolute mode, this supplies the time in timer ticks at
        which to fire an interrupt.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_SUPPORTED if the timer cannot support the requested mode.

    Other status codes on failure.

--*/

VOID
HlpTimerDisarm (
    PHARDWARE_TIMER Timer
    );

/*++

Routine Description:

    This routine disarms a timer, stopping it from firing interrupts.

Arguments:

    Timer - Supplies a pointer to the timer to disarm.

Return Value:

    None.

--*/

VOID
HlpTimerAcknowledgeInterrupt (
    PHARDWARE_TIMER Timer
    );

/*++

Routine Description:

    This routine acknowledges a timer interrupt. If the timer is a non-periodic
    absolute timer, this will rearm the timer if it is still in periodic mode.

Arguments:

    Timer - Supplies a pointer to the timer whose interrupt is to be
        acknowledged.

Return Value:

    None.

--*/

ULONGLONG
HlpTimerExtendedQuery (
    PHARDWARE_TIMER Timer
    );

/*++

Routine Description:

    This routine returns a 64-bit monotonically non-decreasing value based on
    the given timer. For this routine to ensure the non-decreasing part, this
    routine must be called at more than twice the rollover rate. The inner
    workings of this routine rely on observing the top bit of the hardware
    timer each time it changes.

Arguments:

    Timer - Supplies the timer to query.

Return Value:

    Returns the timers count, with software rollovers extended out to 64-bits
    if needed.

--*/

ULONGLONG
HlpTimerTimeToTicks (
    PHARDWARE_TIMER Timer,
    ULONGLONG TimeIn100ns
    );

/*++

Routine Description:

    This routine returns the tick count that best approximates the desired
    time interval on the given timer.

Arguments:

    Timer - Supplies a pointer to the timer on which this interval will be
        run.

    TimeIn100ns - Supplies the desired interval to fire in, with units of
        100 nanoseconds (10^-7 seconds).

Return Value:

    Returns the tick count that most closely approximates the requested tick
    count.

    0 on failure.

--*/

KSTATUS
HlpArchInitializeTimers (
    VOID
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the timer
    subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
HlpArchInitializeTimersPreDebugger (
    VOID
    );

/*++

Routine Description:

    This routine implements early timer initialization for the hardware module
    API layer. This routine is *undebuggable*, as it is called before the
    debugger is brought online.

Arguments:

    None.

Return Value:

    None.

--*/

