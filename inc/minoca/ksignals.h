/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    ksignals.h

Abstract:

    This header contains definitions for signals sent to user mode programs by
    the kernel or other user mode programs.

Author:

    Evan Green 29-Mar-2013

--*/

#ifndef _KSIGNALS_H
#define _KSIGNALS_H

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro adds the given signal into the signal set.
//

#define ADD_SIGNAL(_SignalSet, _SignalNumber) \
    ((_SignalSet) |= (1ULL << ((_SignalNumber) - 1)))

//
// This macro removes the given signal from the signal set.
//

#define REMOVE_SIGNAL(_SignalSet, _SignalNumber) \
    ((_SignalSet) &= ~(1ULL << ((_SignalNumber) - 1)))

//
// This macro evaluates to a non-zero condition if the given signal is set in
// the given signal set.
//

#define IS_SIGNAL_SET(_SignalSet, _SignalNumber) \
    (((_SignalSet) & (1ULL << ((_SignalNumber) - 1))) != 0)

//
// This macro initializes a signal set to have no signals set in it.
//

#define INITIALIZE_SIGNAL_SET(_SignalSet) ((_SignalSet) = 0)

//
// This macro ORs together two signal masks into a third. The result is where
// the union is written to, and the two sets are the inputs to combine.
//

#define OR_SIGNAL_SETS(_ResultSet, _Set1, _Set2) \
    ((_ResultSet) = (_Set1) | (_Set2))

//
// This macro removes the signals in the second set from the signals in the
// first, and writes the result back to the first set.
//

#define REMOVE_SIGNALS_FROM_SET(_DestinationSet, _SignalsToRemove) \
    ((_DestinationSet) &= ~(_SignalsToRemove))

//
// This macro returns non-zero if the set is empty.
//

#define IS_SIGNAL_SET_EMPTY(_SignalSet) ((_SignalSet) == 0)

//
// This macro returns a signal set with every signal set.
//

#define FILL_SIGNAL_SET(_SignalSet) ((_SignalSet) = -1)

//
// This macro returns non-zero if the default action for the given signal is
// to ignore it.
//

#define IS_SIGNAL_DEFAULT_IGNORE(_SignalNumber)             \
    (((_SignalNumber) == SIGNAL_CHILD_PROCESS_ACTIVITY) ||  \
     ((_SignalNumber) == SIGNAL_URGENT_DATA_AVAILABLE))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of standard signals (in the bitmask) before the real time
// signals begin.
//

#define STANDARD_SIGNAL_COUNT 32

//
// Define the number of signals supported by the system.
//

#define SIGNAL_COUNT 64

//
// Define user mode signals.
//

//
// This signal is sent when the application's controlling terminal is closed.
//

#define SIGNAL_CONTROLLING_TERMINAL_CLOSED 1

//
// This signal is sent when the keyboard interrupt combination is pressed.
//

#define SIGNAL_KEYBOARD_INTERRUPT 2

//
// This signal is sent asking the application to perform a core dump.
//

#define SIGNAL_REQUEST_CORE_DUMP 3

//
// This signal is sent to a thread that has just executed an illegal
// instruction.
//

#define SIGNAL_ILLEGAL_INSTRUCTION 4

//
// This signal is sent when a hardware breakpoint is reached in the program.
//

#define SIGNAL_TRAP 5

//
// This signal is sent when a fatal condition occurs in the application.
//

#define SIGNAL_ABORT 6

//
// This signal is sent when an application causes a bus error.
//

#define SIGNAL_BUS_ERROR 7

//
// This signal is sent when the application triggers a math error.
//

#define SIGNAL_MATH_ERROR 8

//
// This signal is sent to kill a process. This signal cannot be caught or
// handled by the application.
//

#define SIGNAL_KILL 9

//
// This signal is never sent by the system, and is available for applications
// to use.
//

#define SIGNAL_APPLICATION1 10

//
// This signal is sent to an application when it improperly accesses a region
// of memory.
//

#define SIGNAL_ACCESS_VIOLATION 11

//
// This signal is never sent by the system, and is available for applications
// to use.
//

#define SIGNAL_APPLICATION2 12

//
// This signal is sent to a process when it attempts to write to a pipe with no
// reader connected at the other end.
//

#define SIGNAL_BROKEN_PIPE 13

//
// This signal is sent to a process when a requested time limit has expired.
//

#define SIGNAL_TIMER 14

//
// This signal is sent to an application to politely request its termination.
//

#define SIGNAL_REQUEST_TERMINATION 15

//
// This signal is sent when a child process terminated, stopped, or continued.
//

#define SIGNAL_CHILD_PROCESS_ACTIVITY 16

//
// This signal is sent to resume a process that was previously stopped.
//

#define SIGNAL_CONTINUE 17

//
// When this signal is sent, it causes the destination process to suspend. This
// signal cannot be caught or ignored.
//

#define SIGNAL_STOP 18

//
// This signal is sent to politely request that the destination process
// suspend itself.
//

#define SIGNAL_REQUEST_STOP 19

//
// This signal is sent when a background process attempts to read from the
// terminal.
//

#define SIGNAL_BACKGROUND_TERMINAL_INPUT 20

//
// This signal is sent when a background process attempts to write to the
// terminal.
//

#define SIGNAL_BACKGROUND_TERMINAL_OUTPUT 21

//
// This signal is sent to indicate that high bandwidth data is available at a
// socket.
//

#define SIGNAL_URGENT_DATA_AVAILABLE 22

//
// This signal is sent to indicate that the destination process has neared or
// exceeded its CPU resource allocation limit.
//

#define SIGNAL_CPU_QUOTA_REACHED 23

//
// This signal is sent when a file size grows beyond the maximum allowed limit.
//

#define SIGNAL_FILE_SIZE_TOO_LARGE 24

//
// This signal is sent when a process executes for a specified duration of time.
//

#define SIGNAL_EXECUTION_TIMER_EXPIRED 25

//
// This signal is sent when a profiling timer expires.
//

#define SIGNAL_PROFILE_TIMER 26

//
// This signal is sent when the application's controlling terminal changes
// size.
//

#define SIGNAL_TERMINAL_WINDOW_CHANGE 27

//
// This signal is sent when asynchronous I/O is available.
//

#define SIGNAL_ASYNCHRONOUS_IO_COMPLETE 28

//
// This signal is sent when a bad system call is attempted.
//

#define SIGNAL_BAD_SYSTEM_CALL 29

//
// Define the child process signal reason codes.
//

//
// This code is used if the process exited naturally.
//

#define CHILD_SIGNAL_REASON_EXITED 1

//
// This code is used if the process was killed by a signal.
//

#define CHILD_SIGNAL_REASON_KILLED 2

//
// This code is used if the process aborted abnormally and a dump was created.
//

#define CHILD_SIGNAL_REASON_DUMPED 3

//
// This code is used if the process took a trap.
//

#define CHILD_SIGNAL_REASON_TRAPPED 4

//
// This code is used if the process is stopped.
//

#define CHILD_SIGNAL_REASON_STOPPED 5

//
// This code is used if the process has continued.
//

#define CHILD_SIGNAL_REASON_CONTINUED 6

//
// Define access violation signal codes.
//

#define ACCESS_VIOLATION_MAPPING_ERROR    1
#define ACCESS_VIOLATION_PERMISSION_ERROR 2

//
// Define user signal codes.
//

#define SIGNAL_CODE_NO_INFORMATION 0
#define SIGNAL_CODE_USER (-1)
#define SIGNAL_CODE_QUEUE (-2)
#define SIGNAL_CODE_TIMER (-3)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define a signal mask type, to be used for all signal bitmaps.
//

typedef ULONGLONG SIGNAL_SET, *PSIGNAL_SET;

/*++

Structure Description:

    This structure defines the signal information structure.

Members:

    SignalNumber - Stores the number of the signal being generated.

    SignalCode - Stores additional information about the signal. The meaning
        of this value is different for each signal.

    ErrorNumber - Stores an optional error number to send with the signal.

    SendingProcess - Stores the process ID of the process that sent this signal.

    FaultingAddress - Stores the faulting address that caused the signal, used
        for bus and segmentation fault signals.

    BandEvent - Stores the data direction that is available. Used by poll
        signals.

    OverflowCount - Stores the number of overflows that occurred. Used by the
        timers.

    SendingUserId - Stores the user ID of the process that generated the signal.

    Parameter - Stores the parameter, which is usually either the exit status
        or the user-defined parameter sent with the queued signal.

--*/

typedef struct _SIGNAL_PARAMETERS {
    USHORT SignalNumber;
    SHORT SignalCode;
    INT ErrorNumber;
    union {
        LONG SendingProcess;
        PVOID FaultingAddress;
        LONG BandEvent;
        ULONG OverflowCount;
    } FromU;

    ULONG SendingUserId;
    UINTN Parameter;
} SIGNAL_PARAMETERS, *PSIGNAL_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

