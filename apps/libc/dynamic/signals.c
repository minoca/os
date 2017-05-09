/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    signals.c

Abstract:

    This module implements signal handling functionality for the C library.

Author:

    Evan Green 28-Mar-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro asserts that the C library wait flags are equivalent to the
// kernel wait flags.
//

#define ASSERT_WAIT_FLAGS_EQUIVALENT()                                  \
    ASSERT((WNOHANG == SYSTEM_CALL_WAIT_FLAG_RETURN_IMMEDIATELY) &&     \
           (WUNTRACED == SYSTEM_CALL_WAIT_FLAG_STOPPED_CHILDREN) &&     \
           (WCONTINUED == SYSTEM_CALL_WAIT_FLAG_CONTINUED_CHILDREN) &&  \
           (WEXITED == SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN) &&        \
           (WNOWAIT == SYSTEM_CALL_WAIT_FLAG_DONT_DISCARD_CHILD))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the required size of the signal description buffer.
//

#define SIGNAL_DESCRIPTION_BUFFER_SIZE 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ClpHandleSignal (
    PSIGNAL_PARAMETERS SignalInformation,
    PSIGNAL_CONTEXT Context
    );

int
ClpConvertToWaitStatus (
    USHORT Reason,
    UINTN Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an array of strings, indexed up to NSIG, that contain descriptions of
// the strings.
//

LIBC_API const char *sys_siglist[NSIG] = {
    NULL,
    "Hangup",
    "Interrupt",
    "Quit",
    "Illegal instruction",
    "Trace/breakpoint trap",
    "Aborted",
    "Bus error",
    "Floating point exception",
    "Killed",
    "User defined signal 1",
    "Segmentation fault",
    "User defined signal 2",
    "Broken pipe",
    "Alarm clock",
    "Terminated",
    "Child exited",
    "Continued",
    "Stopped (signal)",
    "Stopped",
    "Stopped (tty input)",
    "Stopped (tty output)",
    "Urgent I/O condition",
    "CPU time limit exceeded",
    "File size limit exceeded",
    "Virtual timer expired",
    "Profiling timer expired",
    "Window changed",
    "I/O possible",
    "Bad system call",
};

//
// Define the process-wide array of signal handlers.
//

struct sigaction ClSignalHandlers[SIGNAL_COUNT];

//
// Store a pointer to the signal description buffer, allocated on demand.
//

char *ClSignalDescriptionBuffer = NULL;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
siginterrupt (
    int Signal,
    int Flag
    )

/*++

Routine Description:

    This routine modifies the behavior of system calls interrupted by a given
    signal.

Arguments:

    Signal - Supplies the signal number to change restart behavior of.

    Flag - Supplies a boolean that if zero, will mean that system calls
        interrupted by this signal will be restarted if no data is transferred.
        This is the default. If non-zero, system calls interrupted by the given
        signal that have not transferred any data yet will return with EINTR.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    struct sigaction Action;
    int Result;

    Result = sigaction(Signal, NULL, &Action);
    if (Result != 0) {
        return Result;
    }

    if (Flag != 0) {
        Action.sa_flags &= ~SA_RESTART;

    } else {
        Action.sa_flags |= SA_RESTART;
    }

    return Result;
}

LIBC_API
int
sigaction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    )

/*++

Routine Description:

    This routine sets a new signal action for the given signal number.

Arguments:

    SignalNumber - Supplies the signal number that will be affected.

    NewAction - Supplies an optional pointer to the new signal action to
        perform upon receiving that signal. If this pointer is NULL, then no
        change will be made to the signal's action.

    OriginalAction - Supplies a pointer where the original signal action will
        be returned.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    //
    // Just pretend everything is fine, but ignore changes to the signals
    // needed by the C library.
    //

    if ((SignalNumber == SIGNAL_PTHREAD) || (SignalNumber == SIGNAL_SETID)) {
        return 0;
    }

    return ClpSetSignalAction(SignalNumber, NewAction, OriginalAction);
}

LIBC_API
int
sigaddset (
    sigset_t *SignalSet,
    int SignalNumber
    )

/*++

Routine Description:

    This routine adds the specified individual signal into the given signal set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to add the signal to.

    SignalNumber - Supplies the signal number to add.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information. The only
    error returned an invalid parameter error returned when an invalid signal
    number is passed in.

--*/

{

    ASSERT(sizeof(sigset_t) == sizeof(SIGNAL_SET));

    if (SignalNumber > SIGNAL_COUNT) {
        errno = EINVAL;
        return -1;
    }

    ADD_SIGNAL(*SignalSet, SignalNumber);
    return 0;
}

LIBC_API
int
sigemptyset (
    sigset_t *SignalSet
    )

/*++

Routine Description:

    This routine initializes the given signal set to contain no signals, the
    empty set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to initialize.

Return Value:

    0 always to indicate success.

--*/

{

    ASSERT(sizeof(sigset_t) == sizeof(SIGNAL_SET));

    INITIALIZE_SIGNAL_SET(*SignalSet);
    return 0;
}

LIBC_API
int
sigdelset (
    sigset_t *SignalSet,
    int SignalNumber
    )

/*++

Routine Description:

    This routine removes the specified signal number from the given signal set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to remove the signal from.

    SignalNumber - Supplies the signal number to remove.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information. The only
    error returned an invalid parameter error returned when an invalid signal
    number is passed in.

--*/

{

    ASSERT(sizeof(sigset_t) == sizeof(SIGNAL_SET));

    if (SignalNumber > SIGNAL_COUNT) {
        errno = EINVAL;
        return -1;
    }

    REMOVE_SIGNAL(*SignalSet, SignalNumber);
    return 0;
}

LIBC_API
int
sigfillset (
    sigset_t *SignalSet
    )

/*++

Routine Description:

    This routine initializes the given signal set to contain all signals set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to initialize.

Return Value:

    0 always to indicate success.

--*/

{

    ASSERT(sizeof(sigset_t) == sizeof(SIGNAL_SET));

    FILL_SIGNAL_SET(*SignalSet);
    return 0;
}

LIBC_API
void (
*signal (
    int Signal,
    void (*SignalFunction)(int)
    ))(int)

/*++

Routine Description:

    This routine changes a signal's disposition and handler. This method is
    deprecated in favor of sigaction because this function's implementation
    behavior is to reset the signal handler to the default value after the
    signal handler is called, causing nasty race conditions.

Arguments:

    Signal - Supplies the signal to change.

    SignalFunction - Supplies a pointer to the signal function.

Return Value:

    Returns a pointer to the original function wired up for this signal.

--*/

{

    struct sigaction Action;
    struct sigaction OriginalAction;
    int Result;

    Action.sa_handler = SignalFunction;
    Action.sa_flags = SA_RESETHAND | SA_NODEFER;
    sigemptyset(&(Action.sa_mask));
    Result = sigaction(Signal, &Action, &OriginalAction);
    if (Result == -1) {
        return SIG_ERR;
    }

    return OriginalAction.sa_handler;
}

LIBC_API
int
sigismember (
    const sigset_t *SignalSet,
    int SignalNumber
    )

/*++

Routine Description:

    This routine tests whether the specified signal is in the given signal set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to test.

    SignalNumber - Supplies the signal number to check.

Return Value:

    0 if the signal is not set in the given signal set.

    1 if the signal is set in the given signal set.

    -1 on error, and the errno variable will contain more information. The only
    error returned an invalid parameter error returned when an invalid signal
    number is passed in.

--*/

{

    ASSERT(sizeof(sigset_t) == sizeof(SIGNAL_SET));

    if (SignalNumber > SIGNAL_COUNT) {
        errno = EINVAL;
        return -1;
    }

    if (IS_SIGNAL_SET(*SignalSet, SignalNumber) != FALSE) {
        return 1;
    }

    return 0;
}

LIBC_API
int
sigprocmask (
    int LogicalOperation,
    const sigset_t *SignalSet,
    sigset_t *OriginalSignalSet
    )

/*++

Routine Description:

    This routine sets the process' blocked signal mask, assuming there's only
    one thread in the process.

Arguments:

    LogicalOperation - Supplies the operation to use when combining the new
        signal set with the existing. Valid values are:

        SIG_BLOCK - Add the given signals to the mask of blocked signals.

        SIG_SETMASK - Replace the mask of blocked signals wholesale with the
            given mask.

        SIG_UNBLOCK - Remove the set of signals given from the mask of blocked
            signals.

    SignalSet - Supplies an optional pointer to the signal set parameter that
        will in some way become the new signal mask (the effect it has depends
        on the operation parameter).

    OriginalSignalSet - Supplies an optional pointer that will receieve the
        signal set that was in effect before this call.

Return Value:

    0 if a signal mask was sucessfully sent.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    SIGNAL_SET NewSet;
    SIGNAL_MASK_OPERATION Operation;
    SIGNAL_SET PreviousSet;

    //
    // Don't allow the internal signals used by the C library to become blocked.
    //

    INITIALIZE_SIGNAL_SET(NewSet);
    if (SignalSet != NULL) {
        NewSet = *SignalSet;
        REMOVE_SIGNAL(NewSet, SIGNAL_PTHREAD);
        REMOVE_SIGNAL(NewSet, SIGNAL_SETID);
    }

    if (SignalSet == NULL) {
        Operation = SignalMaskOperationNone;

    } else if (LogicalOperation == SIG_BLOCK) {
        Operation = SignalMaskOperationSet;

    } else if (LogicalOperation == SIG_SETMASK) {
        Operation = SignalMaskOperationOverwrite;

    } else if (LogicalOperation == SIG_UNBLOCK) {
        Operation = SignalMaskOperationClear;

    } else {
        errno = EINVAL;
        return -1;
    }

    ASSERT(sizeof(SIGNAL_SET) == sizeof(sigset_t));

    PreviousSet = OsSetSignalBehavior(SignalMaskBlocked,
                                      Operation,
                                      &NewSet);

    if (OriginalSignalSet != NULL) {
        *OriginalSignalSet = PreviousSet;
    }

    return 0;
}

LIBC_API
int
pthread_sigmask (
    int LogicalOperation,
    const sigset_t *SignalSet,
    sigset_t *OriginalSignalSet
    )

/*++

Routine Description:

    This routine sets the current thread's blocked signal mask.

Arguments:

    LogicalOperation - Supplies the operation to use when combining the new
        signal set with the existing set. Valid values are:

        SIG_BLOCK - Add the given signals to the mask of blocked signals.

        SIG_SETMASK - Replace the mask of blocked signals wholesale with the
            given mask.

        SIG_UNBLOCK - Remove the set of signals given from the mask of blocked
            signals.

    SignalSet - Supplies an optional pointer to the signal set parameter that
        will in some way become the new signal mask (the effect it has depends
        on the operation parameter).

    OriginalSignalSet - Supplies an optional pointer that will receieve the
        signal set that was in effect before this call.

Return Value:

    0 if a signal mask was sucessfully sent.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    //
    // The specification says that the original mask function must only be used
    // in a single threaded environment, and that using that routine with
    // multiple threads is undefined. In this system's implementation, the
    // original function sets the current thread's mask, so just call that.
    //

    return sigprocmask(LogicalOperation, SignalSet, OriginalSignalSet);
}

LIBC_API
int
kill (
    pid_t ProcessId,
    int SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to a process or group of processes.

Arguments:

    ProcessId - Supplies the process ID of the process to send the signal to.
        If zero is supplied, then the signal will be sent to all processes in
        the process group. If the process ID is -1, the signal will be sent to
        all processes the signal can reach. If the process ID is negative but
        not negative 1, then the signal will be sent to all processes whose
        process group ID is equal to the absolute value of the process ID (and
        for which the process has permission to send the signal to).

    SignalNumber - Supplies the signal number to send. This value is expected
        to be one of the standard signal numbers (i.e. not a real time signal
        number).

Return Value:

    0 if a signal was actually sent to any processes.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    KSTATUS Status;
    SIGNAL_TARGET_TYPE TargetType;

    if (ProcessId == 0) {
        TargetType = SignalTargetCurrentProcessGroup;

    } else if (ProcessId == -1) {
        TargetType = SignalTargetAllProcesses;

    } else if (ProcessId < 0) {
        TargetType = SignalTargetProcessGroup;
        ProcessId = -ProcessId;

    } else {
        TargetType = SignalTargetProcess;
    }

    Status = OsSendSignal(TargetType,
                          ProcessId,
                          SignalNumber,
                          SIGNAL_CODE_USER,
                          0);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
killpg (
    pid_t ProcessGroupId,
    int SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to a group of processes.

Arguments:

    ProcessGroupId - Supplies the process group ID of the process group to
        signal. If zero is supplied, then the signal is sent to all processes
        in the current process group. If the process group ID is 1, then the
        signal will be sent to all processs the signal can reach.

    SignalNumber - Supplies the signal number to send. This value is expected
        to be one of the standard signal numbers (i.e. not a real time signal
        number).

Return Value:

    0 if a signal was actually sent to any processes.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    if (ProcessGroupId < 0) {
        errno = EINVAL;
        return -1;
    }

    return kill(-ProcessGroupId, SignalNumber);
}

LIBC_API
int
raise (
    int SignalNumber
    )

/*++

Routine Description:

    This routine sends a signal to the current process.

Arguments:

    SignalNumber - Supplies the signal number to send. This value is expected
        to be one of the standard signal numbers (ie not a real time signal
        number).

Return Value:

    0 if a signal was actually sent to any processes.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    KSTATUS Status;

    Status = OsSendSignal(SignalTargetCurrentProcess,
                          0,
                          SignalNumber,
                          SIGNAL_CODE_USER,
                          0);

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
sigqueue (
    pid_t ProcessId,
    int SignalNumber,
    union sigval Value
    )

/*++

Routine Description:

    This routine sends a real time signal to the given process.

Arguments:

    ProcessId - Supplies the process ID to send the signal to.

    SignalNumber - Supplies the signal number to send. This is expected to be
        in the real time range, not one of the standard lower signal numbers.

    Value - Supplies the value to send off with the signal.

Return Value:

    0 if a signal was actually sent to any processes.

    -1 on error, and the errno variable will contain more information about the
    error.

--*/

{

    KSTATUS Status;

    ASSERT(sizeof(void *) >= sizeof(int));

    Status = OsSendSignal(SignalTargetProcess,
                          ProcessId,
                          SignalNumber,
                          SIGNAL_CODE_QUEUE,
                          (UINTN)(Value.sival_ptr));

    if (KSUCCESS(Status)) {
        return 0;

    } else {
        errno = ClConvertKstatusToErrorNumber(Status);
    }

    return -1;
}

LIBC_API
int
pause (
    void
    )

/*++

Routine Description:

    This routine suspends execution until a signal is caught and handled by the
    application. This routine is known to be frought with timing problems, as
    the most common use for it involves checking if a signal has occurred, and
    calling pause if not. Unfortunately that doesn't work as a signal can come
    in after the check but before the call to pause. Pause is really only
    useful if the entirety of the application functionality is implemented
    inside signal handlers.

Arguments:

    None.

Return Value:

    -1 always. A return rather negatively is thought of as a failure of the
    function. The errno variable will be set to indicate the "error".

--*/

{

    OsSuspendExecution(SignalMaskOperationNone,
                       NULL,
                       NULL,
                       SYS_WAIT_TIME_INDEFINITE);

    errno = EINTR;
    return -1;
}

LIBC_API
int
sigpending (
    sigset_t *SignalSet
    )

/*++

Routine Description:

    This routine returns the current set of signals that are blocked from
    delivery to the current calling thread and that are pending on the process
    or calling thread.

Arguments:

    SignalSet - Supplies a pointer where the mask of pending signals will be
        returned.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    INITIALIZE_SIGNAL_SET(*SignalSet);
    *SignalSet = OsSetSignalBehavior(SignalMaskPending,
                                     SignalMaskOperationNone,
                                     (PSIGNAL_SET)SignalSet);

    return 0;
}

LIBC_API
int
sigsuspend (
    const sigset_t *SignalMask
    )

/*++

Routine Description:

    This routine temporarily replaces the current thread's signal mask with
    the given signal mask, then suspends the thread's execution until an
    unblocked signal comes in.

Arguments:

    SignalMask - Supplies a pointer to the mask of signals to block during the
        suspend.

Return Value:

    -1 always. A return rather negatively is thought of as a failure of the
    function. The errno variable will be set to indicate the "error".

--*/

{

    OsSuspendExecution(SignalMaskOperationOverwrite,
                       (PSIGNAL_SET)SignalMask,
                       NULL,
                       SYS_WAIT_TIME_INDEFINITE);

    //
    // Obviously if execution is back, a signal must have occurred that was
    // caught by the application.
    //

    errno = EINTR;
    return -1;
}

LIBC_API
int
sigwait (
    const sigset_t *SignalSet,
    int *SignalNumber
    )

/*++

Routine Description:

    This routine waits for a signal from the given set and returns the number
    of the received signal.

Arguments:

    SignalSet - Supplies a pointer to a set of signals on which to wait. This
        set of signals shall have been blocked prior to calling this routine.

    SignalNumber - Supplies a pointer that receives the signal number of the
        received signal.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    KSTATUS KernelStatus;
    SIGNAL_PARAMETERS SignalParameters;

    if (SignalNumber == NULL) {
        return EINVAL;
    }

    do {
        KernelStatus = OsSuspendExecution(SignalMaskOperationClear,
                                          (PSIGNAL_SET)SignalSet,
                                          &SignalParameters,
                                          SYS_WAIT_TIME_INDEFINITE);

    } while (KernelStatus == STATUS_INTERRUPTED);

    if (KSUCCESS(KernelStatus)) {
        *SignalNumber = SignalParameters.SignalNumber;
    }

    return ClConvertKstatusToErrorNumber(KernelStatus);
}

LIBC_API
int
sigwaitinfo (
    const sigset_t *SignalSet,
    siginfo_t *SignalInformation
    )

/*++

Routine Description:

    This routine waits for a signal from the given set and returns the signal
    information for the received signal. If an unblocked signal outside the
    given set arrives, this routine will return EINTR.

Arguments:

    SignalSet - Supplies a pointer to a set of signals on which to wait. This
        set of signals shall have been blocked prior to calling this routine.

    SignalInformation - Supplies an optional pointer that receives the signal
        information for the selected singal.

Return Value:

    The selected signal number on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    return sigtimedwait(SignalSet, SignalInformation, NULL);
}

LIBC_API
int
sigtimedwait (
    const sigset_t *SignalSet,
    siginfo_t *SignalInformation,
    const struct timespec *Timeout
    )

/*++

Routine Description:

    This routine waits for a signal from the given set and returns the signal
    information for the received signal. If the timeout is reached without a
    signal, then the routine will fail with EAGAIN set in errno. If an
    unblocked signal outside the given set arrives, this routine will return
    EINTR.

Arguments:

    SignalSet - Supplies a pointer to a set of signals on which to wait. This
        set of signals shall have been blocked prior to calling this routine.

    SignalInformation - Supplies an optional pointer that receives the signal
        information for the selected singal.

    Timeout - Supplies an optional timeout interval to wait for one of the set
        of signals to become pending. If no signal becomes set within the
        timeout interval EAGAIN will be returned. If NULL is supplied, the
        routine will wait indefinitely for a signal to arrive.

Return Value:

    The selected signal number on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    KSTATUS KernelStatus;
    INT Result;
    INT SignalNumber;
    SIGNAL_PARAMETERS SignalParameters;
    ULONG TimeoutInMilliseconds;

    Result = ClpConvertSpecificTimeoutToSystemTimeout(Timeout,
                                                      &TimeoutInMilliseconds);

    if (Result != 0) {
        errno = Result;
        return -1;
    }

    KernelStatus = OsSuspendExecution(SignalMaskOperationClear,
                                      (PSIGNAL_SET)SignalSet,
                                      &SignalParameters,
                                      TimeoutInMilliseconds);

    if (KSUCCESS(KernelStatus)) {
        SignalNumber = SignalParameters.SignalNumber;
        if (SignalInformation != NULL) {
            RtlZeroMemory(SignalInformation, sizeof(siginfo_t));
            SignalInformation->si_signo = SignalNumber;
            SignalInformation->si_code = SignalParameters.SignalCode;
            SignalInformation->si_errno = SignalParameters.ErrorNumber;
            SignalInformation->si_pid = SignalParameters.FromU.SendingProcess;
            SignalInformation->si_uid = SignalParameters.SendingUserId;
            SignalInformation->si_addr = SignalParameters.FromU.FaultingAddress;
            SignalInformation->si_status = SignalParameters.Parameter;
            SignalInformation->si_band = SignalParameters.FromU.Poll.BandEvent;
            SignalInformation->si_value.sival_int = SignalParameters.Parameter;
            SignalInformation->si_fd =
                            (int)(UINTN)SignalParameters.FromU.Poll.Descriptor;
        }

    } else {
        SignalNumber = -1;
        if (KernelStatus == STATUS_TIMEOUT) {
            errno = EAGAIN;

        } else {
            errno = ClConvertKstatusToErrorNumber(KernelStatus);
        }
    }

    return SignalNumber;
}

LIBC_API
pid_t
wait (
    int *Status
    )

/*++

Routine Description:

    This routine obtains status information about one of the caller's
    terminated child processes. This routine blocks until such status
    information becomes available or until the calling process receives a
    terminating signal.

Arguments:

    Status - Supplies an optional pointer where the child process' exit status
        information will be returned.

Return Value:

    Returns the process ID of the child process that just experienced a state
    change.

    -1 on failure, and the errno variable will contain more information.

--*/

{

    UINTN ChildExitValue;
    PROCESS_ID ChildPid;
    KSTATUS KernelStatus;
    ULONG Reason;

    ChildPid = -1;
    Reason = 0;
    KernelStatus = OsWaitForChildProcess(SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN,
                                         &ChildPid,
                                         &Reason,
                                         &ChildExitValue,
                                         NULL);

    if (!KSUCCESS(KernelStatus)) {
        errno = ClConvertKstatusToErrorNumber(KernelStatus);
        return -1;
    }

    if (Status != NULL) {
        *Status = ClpConvertToWaitStatus(Reason, ChildExitValue);
    }

    return ChildPid;
}

LIBC_API
pid_t
waitpid (
    pid_t ProcessId,
    int *Status,
    int Options
    )

/*++

Routine Description:

    This routine obtains status information about one of the caller's child
    processes. This routine can block waiting for any child process to change,
    or can wait for a specific process.

Arguments:

    ProcessId - Supplies the process ID of the process to wait for. The
        various valid values are as follows:

        If equal to -1, then this routine will be equivalent to the original
            routine, it will return when any process has status information.

        If greater than 0, then the specific process ID will be waited for.

        If 0, then any child whose process process group ID is equal to that of
            the calling process will satisfy the wait.

        If less than negative one, then any child process whose process group ID
            is equal to the absolute value of this parameter will satisfy the
            wait.

    Status - Supplies an optional pointer where the child process' exit status
        information will be returned.

    Options - Supplies a bitfield of options. This field may contain one or
        more of the following options:

        WCONTINUED - Wait for a process that just continued.

        WNOHANG - Return immediately if no child process information is
            currently available.

        WUNTRACED - Wait for a process that just stopped.

Return Value:

    Returns the process ID of the child process that just experienced a state
    change.

    -1 on failure, and the errno variable will contain more information.

--*/

{

    UINTN ChildExitValue;
    PROCESS_ID ChildPid;
    ULONG Flags;
    KSTATUS KernelStatus;
    ULONG Reason;
    ULONG ValidOptions;

    ChildPid = ProcessId;
    Reason = 0;

    //
    // Only accept valid options.
    //

    ValidOptions = WCONTINUED | WUNTRACED | WNOHANG;
    if ((Options & ~ValidOptions) != 0) {
        errno = EINVAL;
        return -1;
    }

    ASSERT_WAIT_FLAGS_EQUIVALENT();

    Flags = Options | SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN;
    KernelStatus = OsWaitForChildProcess(Flags,
                                         &ChildPid,
                                         &Reason,
                                         &ChildExitValue,
                                         NULL);

    if (KernelStatus == STATUS_NO_DATA_AVAILABLE) {

        assert((Options & WNOHANG) != 0);

        return 0;
    }

    if (!KSUCCESS(KernelStatus)) {
        errno = ClConvertKstatusToErrorNumber(KernelStatus);
        return -1;
    }

    if (Status != NULL) {
        *Status = ClpConvertToWaitStatus(Reason, ChildExitValue);
    }

    return ChildPid;
}

LIBC_API
int
waitid (
    idtype_t IdentifierType,
    id_t ProcessOrGroupIdentifier,
    siginfo_t *SignalInformation,
    int Options
    )

/*++

Routine Description:

    This routine suspends execution until a child process of this process
    changes state.

Arguments:

    IdentifierType - Supplies a value indicating whether the process or group
        identifier identifies a process, group, or nothing. If nothing, then
        any child process changing state will satisfy the wait.

    ProcessOrGroupIdentifier - Supplies a process or process group identifier
        to wait for. If the identifier type indicates neither, then this
        parameter is ignored.

    SignalInformation - Supplies a pointer where the child signal information
        will be returned.

    Options - Supplies a bitfield of options. Valid values are WEXITED,
        WSTOPPED, WCONTINUED, WNOHANG, and WNOWAIT. One or more of WEXITED,
        WSTOPPED or WCONTINUED must be supplied.

Return Value:

    0 if WNOHANG was specified and no child was waiting to report status
    information.

    0 on success (child information was returned).

    -1 on failure, and the errno variable will be set to indicate the error.

--*/

{

    UINTN ChildExitValue;
    PROCESS_ID ChildPid;
    ULONG Flags;
    ULONG Reason;
    KSTATUS Status;

    RtlZeroMemory(SignalInformation, sizeof(siginfo_t));
    if (IdentifierType == P_PID) {
        ChildPid = ProcessOrGroupIdentifier;

    } else if (IdentifierType == P_PGID) {
        ChildPid = -ProcessOrGroupIdentifier;

    } else if (IdentifierType == P_ALL) {
        ChildPid = -1;

    } else {
        errno = EINVAL;
        return -1;
    }

    //
    // Gather the required options.
    //

    ASSERT_WAIT_FLAGS_EQUIVALENT();

    Flags = Options;

    //
    // There must be one or more of WEXITED, WCONTINUED, and WUNTRACED
    // specified.
    //

    if ((Options & (WEXITED | WCONTINUED | WUNTRACED)) == 0) {
        errno = EINVAL;
        return -1;
    }

    Status = OsWaitForChildProcess(Flags,
                                   &ChildPid,
                                   &Reason,
                                   &ChildExitValue,
                                   NULL);

    if (Status == STATUS_NO_DATA_AVAILABLE) {

        assert((Options & WNOHANG) != 0);

        return 0;
    }

    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    SignalInformation->si_signo = SIGCHLD;
    SignalInformation->si_code = Reason;
    SignalInformation->si_pid = ChildPid;
    SignalInformation->si_status = ChildExitValue;
    SignalInformation->si_uid = 0;
    return ChildPid;
}

LIBC_API
pid_t
wait3 (
    int *Status,
    int Options,
    struct rusage *ResourceUsage
    )

/*++

Routine Description:

    This routine is equivalent to the wait function, except it can obtain
    resource usage about the reaped child. This function is provided for
    compatibility with existing applications. New applications should use the
    waitpid function.

Arguments:

    Status - Supplies an optional pointer where the child process' exit status
        information will be returned.

    Options - Supplies a bitfield of options. See the waitpid function for
        more details.

    ResourceUsage - Supplies an optional pointer where the resource usage of
        the process will be returned on success.

Return Value:

    Returns the process ID of the child process that just experienced a state
    change.

    -1 on failure, and the errno variable will contain more information.

--*/

{

    return wait4(-1, Status, Options, ResourceUsage);
}

LIBC_API
pid_t
wait4 (
    pid_t ProcessId,
    int *Status,
    int Options,
    struct rusage *ResourceUsage
    )

/*++

Routine Description:

    This routine is equivalent to the waitpid function, except it can obtain
    resource usage about the reaped child. This function is provided for
    compatibility with existing applications. New applications should use the
    waitpid function.

Arguments:

    ProcessId - Supplies the process ID to wait for. See waitpid for more
        information.

    Status - Supplies an optional pointer where the child process' exit status
        information will be returned.

    Options - Supplies a bitfield of options. See the waitpid function for
        more details.

    ResourceUsage - Supplies an optional pointer where the resource usage of
        the process will be returned on success.

Return Value:

    Returns the process ID of the child process that just experienced a state
    change.

    -1 on failure, and the errno variable will contain more information.

--*/

{

    UINTN ChildExitValue;
    PROCESS_ID ChildPid;
    ULONG Flags;
    ULONGLONG Frequency;
    KSTATUS KernelStatus;
    ULONG Reason;
    RESOURCE_USAGE Resources;
    PRESOURCE_USAGE ResourcesPointer;
    ULONG ValidOptions;

    ChildPid = ProcessId;
    Reason = 0;

    //
    // Only accept valid options.
    //

    ValidOptions = WCONTINUED | WUNTRACED | WNOHANG;
    if ((Options & ~ValidOptions) != 0) {
        errno = EINVAL;
        return -1;
    }

    ASSERT_WAIT_FLAGS_EQUIVALENT();

    Flags = Options | SYSTEM_CALL_WAIT_FLAG_EXITED_CHILDREN;
    ResourcesPointer = NULL;
    if (ResourceUsage != NULL) {
        ResourcesPointer = &Resources;
    }

    KernelStatus = OsWaitForChildProcess(Flags,
                                         &ChildPid,
                                         &Reason,
                                         &ChildExitValue,
                                         ResourcesPointer);

    if (KernelStatus == STATUS_NO_DATA_AVAILABLE) {

        assert((Options & WNOHANG) != 0);

        return 0;
    }

    if (!KSUCCESS(KernelStatus)) {
        errno = ClConvertKstatusToErrorNumber(KernelStatus);
        return -1;
    }

    if (ResourceUsage != NULL) {
        OsGetResourceUsage(ResourceUsageRequestInvalid, -1, NULL, &Frequency);
        ClpConvertResourceUsage(&Resources, Frequency, ResourceUsage);
    }

    if (Status != NULL) {
        *Status = ClpConvertToWaitStatus(Reason, ChildExitValue);
    }

    return ChildPid;
}

LIBC_API
void
psignal (
    int Signal,
    char *String
    )

/*++

Routine Description:

    This routine prints to stderr the given string, a colon, a space, and
    a description of the given signal number.

Arguments:

    Signal - Supplies the signal number to describe on standard error.

    String - Supplies an optional pointer to a string. If this is NULL, then
        the colon and the space will be omitted.

Return Value:

    None.

--*/

{

    PSTR SignalString;

    if (String != NULL) {
        fprintf(stderr, "%s: ", String);
    }

    SignalString = NULL;
    if ((Signal >= 0) && (Signal < NSIG)) {
        SignalString = (PSTR)(sys_siglist[Signal]);
    }

    if (SignalString == NULL) {
        if ((Signal >= SIGRTMIN) && (Signal <= SIGRTMAX)) {
            fprintf(stderr, "Real-time signal %d", Signal);

        } else {
            fprintf(stderr, "Unknown signal %d", Signal);
        }

    } else {
        fprintf(stderr, "%s", SignalString);
    }

    return;
}

LIBC_API
char *
strsignal (
    int Signal
    )

/*++

Routine Description:

    This routine returns a pointer to a string containing a descriptive message
    for the given signal number. This routine is neither thread safe nor
    reentrant.

Arguments:

    Signal - Supplies the signal number to return a description for.

Return Value:

    Returns a pointer to a signal description. This buffer may be used until
    the next call to this routine.

--*/

{

    PSTR SignalString;

    SignalString = NULL;
    if ((Signal >= 0) && (Signal < NSIG)) {
        SignalString = (PSTR)(sys_siglist[Signal]);
    }

    if (SignalString != NULL) {
        return SignalString;
    }

    if (ClSignalDescriptionBuffer == NULL) {
        ClSignalDescriptionBuffer = malloc(SIGNAL_DESCRIPTION_BUFFER_SIZE);
        if (ClSignalDescriptionBuffer == NULL) {
            return "Unknown signal";
        }
    }

    if ((Signal >= SIGRTMIN) && (Signal <= SIGRTMAX)) {
        snprintf(ClSignalDescriptionBuffer,
                 SIGNAL_DESCRIPTION_BUFFER_SIZE,
                 "Real-time signal %d",
                 Signal);

    } else {
        snprintf(ClSignalDescriptionBuffer,
                 SIGNAL_DESCRIPTION_BUFFER_SIZE,
                 "Unknown signal %d",
                 Signal);
    }

    return ClSignalDescriptionBuffer;
}

VOID
ClpInitializeSignals (
    VOID
    )

/*++

Routine Description:

    This routine initializes signal handling functionality for the C library.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PPROCESS_ENVIRONMENT Environment;
    SIGNAL_SET IgnoredSignals;
    ULONG Signal;

    //
    // Mark the signals that were left ignored by the parent process.
    //

    Environment = OsGetCurrentEnvironment();
    IgnoredSignals = Environment->StartData->IgnoredSignals;
    Signal = 1;
    while ((!IS_SIGNAL_SET_EMPTY(IgnoredSignals)) && (Signal < SIGNAL_COUNT)) {
        if (IS_SIGNAL_SET(IgnoredSignals, Signal)) {
            ClSignalHandlers[Signal].sa_handler = SIG_IGN;
            REMOVE_SIGNAL(IgnoredSignals, Signal);
        }

        Signal += 1;
    }

    OsSetSignalHandler(ClpHandleSignal);
    return;
}

int
ClpSetSignalAction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    )

/*++

Routine Description:

    This routine sets a new signal action for the given signal number.

Arguments:

    SignalNumber - Supplies the signal number that will be affected.

    NewAction - Supplies an optional pointer to the new signal action to
        perform upon receiving that signal. If this pointer is NULL, then no
        change will be made to the signal's action.

    OriginalAction - Supplies a pointer where the original signal action will
        be returned.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    struct sigaction OriginalCopy;
    SIGNAL_SET SignalSet;

    if ((NewAction != NULL) &&
        ((SignalNumber == SIGKILL) || (SignalNumber == SIGSTOP))) {

        errno = EINVAL;
        return -1;
    }

    if (SignalNumber > SIGNAL_COUNT) {
        errno = EINVAL;
        return -1;
    }

    OriginalCopy = ClSignalHandlers[SignalNumber];
    if (NewAction != NULL) {
        ClSignalHandlers[SignalNumber].sa_handler = SIG_DFL;
        RtlMemoryBarrier();
        ClSignalHandlers[SignalNumber].sa_mask = NewAction->sa_mask;
        ClSignalHandlers[SignalNumber].sa_flags = NewAction->sa_flags;
        RtlMemoryBarrier();
        ClSignalHandlers[SignalNumber].sa_handler = NewAction->sa_handler;
        RtlMemoryBarrier();
    }

    if (OriginalAction != NULL) {
        *OriginalAction = OriginalCopy;
    }

    //
    // Set this up in the kernel as well.
    //

    INITIALIZE_SIGNAL_SET(SignalSet);
    ADD_SIGNAL(SignalSet, SignalNumber);
    if (ClSignalHandlers[SignalNumber].sa_handler == SIG_DFL) {
        OsSetSignalBehavior(SignalMaskHandled,
                            SignalMaskOperationClear,
                            &SignalSet);

    } else if (ClSignalHandlers[SignalNumber].sa_handler == SIG_IGN) {
        OsSetSignalBehavior(SignalMaskIgnored,
                            SignalMaskOperationSet,
                            &SignalSet);

    } else {
        OsSetSignalBehavior(SignalMaskHandled,
                            SignalMaskOperationSet,
                            &SignalSet);
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ClpHandleSignal (
    PSIGNAL_PARAMETERS SignalInformation,
    PSIGNAL_CONTEXT Context
    )

/*++

Routine Description:

    This routine is called whenever a signal occurs for the current process or
    thread.

Arguments:

    SignalInformation - Supplies a pointer to the signal information.

    Context - Supplies a pointer to the signal context, including the machine
        state before the signal was applied.

Return Value:

    TRUE if an interrupted function can restart.

    FALSE otherwise.

--*/

{

    struct sigaction *Action;
    int Flags;
    siginfo_t HandlerInformation;
    struct sigaction OriginalAction;
    struct sigaction ResetAction;
    ULONG Signal;
    SIGNAL_SET SignalMask;

    Signal = SignalInformation->SignalNumber;
    Action = &(ClSignalHandlers[Signal]);
    Flags = Action->sa_flags;

    //
    // Add to the signal mask if desired. SA_RESETHAND behaves like SA_NODEFER.
    //

    if ((IS_SIGNAL_SET_EMPTY(Action->sa_mask) == FALSE) ||
        ((Flags & (SA_NODEFER | SA_RESETHAND)) == 0)) {

        SignalMask = Action->sa_mask;
        if ((Flags & (SA_NODEFER | SA_RESETHAND)) == 0) {
            ADD_SIGNAL(SignalMask, Signal);
        }

        SignalMask = OsSetSignalBehavior(SignalMaskBlocked,
                                         SignalMaskOperationSet,
                                         &SignalMask);
    }

    //
    // Reset the disposition if requested.
    //

    if (((Flags & SA_RESETHAND) != 0) &&
        (Signal != SIGILL) && (Signal != SIGTRAP)) {

        RtlZeroMemory(&ResetAction, sizeof(struct sigaction));
        ClpSetSignalAction(Signal, &ResetAction, &OriginalAction);
        Action = &OriginalAction;
    }

    //
    // If no handler is installed, this handler shouldn't have been called.
    // reset to the default action and reraise.
    //

    if (Action->sa_handler == SIG_DFL) {
        signal(SignalInformation->SignalNumber, SIG_DFL);
        raise(SignalInformation->SignalNumber);

    //
    // If the caller specified to ignore the signal, there's nothing to do.
    // If it wasn't one of those two, then it's a real handler that needs to be
    // called.
    //

    } else if (Action->sa_handler != SIG_IGN) {
        if ((Flags & SA_SIGINFO) != 0) {
            HandlerInformation.si_signo = Signal;
            HandlerInformation.si_code = SignalInformation->SignalCode;
            HandlerInformation.si_errno = SignalInformation->ErrorNumber;
            HandlerInformation.si_pid = SignalInformation->FromU.SendingProcess;
            HandlerInformation.si_uid = SignalInformation->SendingUserId;
            HandlerInformation.si_addr =
                                      SignalInformation->FromU.FaultingAddress;

            HandlerInformation.si_status = SignalInformation->Parameter;
            HandlerInformation.si_band =
                                       SignalInformation->FromU.Poll.BandEvent;

            HandlerInformation.si_value.sival_int =
                                                  SignalInformation->Parameter;

            HandlerInformation.si_fd =
                          (int)(UINTN)SignalInformation->FromU.Poll.Descriptor;

            Action->sa_sigaction(Signal, &HandlerInformation, Context);

        } else {
            Action->sa_handler(Signal);
        }
    }

    //
    // Report whether or not the restart flag was set so that the system can
    // restart a function if required.
    //

    if ((Flags & SA_RESTART) != 0) {
        return TRUE;
    }

    return FALSE;
}

int
ClpConvertToWaitStatus (
    USHORT Reason,
    UINTN Value
    )

/*++

Routine Description:

    This routine converts a reason code and exit status into a wait status.

Arguments:

    Reason - Supplies the reason for the child event.

    Value - Supplies the exit status or terminating signal.

Return Value:

    Returns the encoded wait status that is returned by the wait family of
    functions.

--*/

{

    int Status;

    Status = 0;
    switch (Reason) {

    //
    // If stopped, set the signal portion to all ones, but the status portion
    // to the stop signal.
    //

    case CHILD_SIGNAL_REASON_STOPPED:

        //
        // Fall through.
        //

        Status |= 0x7F;

    case CHILD_SIGNAL_REASON_EXITED:

        //
        // Add in the exit status or stop signal.
        //

        Status |= (Value << 8) & 0xFF00;
        break;

    case CHILD_SIGNAL_REASON_DUMPED:

        //
        // Add the dumped flag.
        //

        Status |= 0x80;

        //
        // Fall through.
        //

    case CHILD_SIGNAL_REASON_KILLED:
    case CHILD_SIGNAL_REASON_TRAPPED:

        //
        // The terminating signal goes in the low byte.
        //

        ASSERT(Value < 0x7F);

        Status |= Value & 0x7F;
        break;

    case CHILD_SIGNAL_REASON_CONTINUED:

        //
        // Continued gets its own special value.
        //

        Status = 0xFFFF;
        break;
    }

    return Status;
}

