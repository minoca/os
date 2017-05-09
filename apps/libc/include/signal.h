/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    signal.h

Abstract:

    This header contains signal definitions.

Author:

    Evan Green 29-Mar-2013

--*/

#ifndef _SIGNAL_H
#define _SIGNAL_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Hangup
//

#define SIGHUP 1

//
// Terminal interrupt signal
//

#define SIGINT 2

//
// Terminal quit signal
//

#define SIGQUIT 3

//
// Illegal instruction
//

#define SIGILL 4

//
// Trace/breakpoint trap
//

#define SIGTRAP 5

//
// Process abort signal
//

#define SIGABRT 6

//
// Access to an undefined portion of a memory object
//

#define SIGBUS 7

//
// Erroneous arithmetic operation
//

#define SIGFPE 8

//
// Kill (this signal cannot be caught or ignored)
//

#define SIGKILL 9

//
// Application defined signal one
//

#define SIGUSR1 10

//
// Invalid memory reference
//

#define SIGSEGV 11

//
// Application defined signal two
//

#define SIGUSR2 12

//
// Write to a pipe with no one to read it
//

#define SIGPIPE 13

//
// Alarm clock
//

#define SIGALRM 14

//
// Termination signal
//

#define SIGTERM 15

//
// Child process terminated, stopped, or continued
//

#define SIGCHLD 16

//
// Continue executing if stopped
//

#define SIGCONT 17

//
// Stop executing (this signal cannot be caught or ignored)
//

#define SIGSTOP 18

//
// Terminal stop signal
//

#define SIGTSTP 19

//
// Background process attempting read
//

#define SIGTTIN 20

//
// Background process attempting write
//

#define SIGTTOU 21

//
// High bandwidth data is available at a socket
//

#define SIGURG 22

//
// CPU time limit exceeded
//

#define SIGXCPU 23

//
// File size limit exceeded
//

#define SIGXFSZ 24

//
// Virtual timer expired
//

#define SIGVTALRM 25

//
// Profiling timer expired
//

#define SIGPROF 26

//
// Controlling terminal window size change
//

#define SIGWINCH 27

//
// Pollable event
//

#define SIGPOLL 28
#define SIGIO SIGPOLL

//
// Bad system call
//

#define SIGSYS 29

//
// Define the real time signal ranges, which are both inclusive. Reserve a
// couple of real-time signal numbers for the C library internally.
//

#define SIGRTMIN 34
#define SIGRTMAX 63

//
// Define the number of signals.
//

#define NSIG 64

//
// Define the operations that can be performed by the signal mask setting
// functions.
//

//
// Use this operation to add the signals from the given signal set into the
// mask of blocked signals.
//

#define SIG_BLOCK 0

//
// Use this operation to remove the signals from the given set from the mask
// of blocked signals.
//

#define SIG_UNBLOCK 1

//
// Use this operation to wholesale replace the mask of blocked signals with the
// new set.
//

#define SIG_SETMASK 2

//
// Define the code values that could come in with a child signal. These line up
// with CHILD_SIGNAL_REASON_* definitions.
//

#define CLD_EXITED 1
#define CLD_KILLED 2
#define CLD_DUMPED 3
#define CLD_TRAPPED 4
#define CLD_STOPPED 5
#define CLD_CONTINUED 6

//
// Define possible signal actions.
//

//
// This action specifies to take the default action for the signal.
//

#define SIG_DFL ((void (*)(int))0)

//
// This action requests that the signal be ignored.
//

#define SIG_IGN ((void (*)(int))1)

//
// This is the return value from the original signal function when there was
// an error.
//

#define SIG_ERR ((void (*)(int))2)

//
// Define signal action flags.
//

//
// When set, does not generate child signals for stopped or continued processes.
//

#define SA_NOCLDSTOP 0x00000001

//
// When set, causes the signal to be delivered on an alternate stack.
//

#define SA_ONSTACK 0x00000002
#define SS_ONSTACK SA_ONSTACK
#define SS_DISABLE 0

//
// When set, causes the signal disposition to get set back to its default once
// a signal is delivered.
//

#define SA_RESETHAND 0x00000004

//
// When set, causes functions that would return with EINTR to restart.
//

#define SA_RESTART 0x00000008

//
// When set, causes the three function prototype signal handler to get called.
// If not set, the only parameter to the signal handling function will be the
// signal number.
//

#define SA_SIGINFO 0x00000010

//
// When set, causes the system not to create zombie processes when a child dies.
//

#define SA_NOCLDWAIT 0x00000020

//
// When set, causes the signal not to be automatically blocked on signal entry.
//

#define SA_NODEFER 0x00000040

//
// This value specifies that a signal should be delivered in the sigevent
// type.
//

#define SIGEV_SIGNAL 0x00000000

//
// This value specifies that no signal nor thread should occur when the event
// happens.
//

#define SIGEV_NONE 0x00000001

//
// This value specifies that a new thread should be created when the event
// occurs.
//

#define SIGEV_THREAD 0x00000002

//
// This value specifies that a signal should be delivered to the thread
// indicated by the thread ID stored in the sigevent.
//

#define SIGEV_THREAD_ID 0x00000004

//
// Define macros that reach through the sigaction union. Applications don't
// consistently set sa_handler vs sa_siginfo, so they really do need to be
// unioned. The alternative to these defines is to use anonymous unions, which
// aren't standard-friendly.
//

#define sa_handler sa_u.sau_handler
#define sa_sigaction sa_u.sau_sigaction

//
// Define the signal codes that may come with an illegal instruction signal.
// These line up with ILLEGAL_INSTRUCTION_* definitions.
//

#define ILL_ILLOPC 1
#define ILL_ILLOPN 2
#define ILL_ILLADR 3
#define ILL_ILLTRP 4
#define ILL_PRVOPC 5
#define ILL_PRVREG 6
#define ILL_COPROC 7
#define ILL_BADSTK 8

//
// Define the signal codes that may come with an arithmetic error signal. These
// line up with MATH_ERROR_* definitions.
//

#define FPE_INTDIV 1
#define FPE_INTOVF 2
#define FPE_FLTDIV 3
#define FPE_FLTOVF 4
#define FPE_FLTUND 5
#define FPE_FLTRES 6
#define FPE_FLTINV 7
#define FPE_FLTSUB 8

//
// Define the signal codes that may come with an invalid memory reference
// signal (SIGSEGV). These line up with ACCESS_VIOLATION_* definitions.
//

#define SEGV_MAPERR 1
#define SEGV_ACCERR 2

//
// Define the signal codes that may come with bus error signal (SIGBUS). These
// line up with BUS_ERROR_* definitions.
//

#define BUS_ADRALN 1
#define BUS_ADRERR 2
#define BUS_OBJERR 3

//
// Define the signal codes that may come with a trap signal. These line up with
// TRAP_CODE_* definitions.
//

#define TRAP_BRKPT 1
#define TRAP_TRACE 2

//
// Define the signal codes that may come with a poll signal. These line up with
// POLL_CODE_* definitions.
//

#define POLL_IN 1
#define POLL_OUT 2
#define POLL_MSG 3
#define POLL_ERR 4
#define POLL_PRI 5
#define POLL_HUP 6

//
// Define signal codes that may come out of queued signals. Positive values
// are reserved for the kernel. These line up with SIGNAL_CODE_* definitions.
//

#define SI_KERNEL (128)
#define SI_USER (0)
#define SI_QUEUE (-1)
#define SI_TIMER (-2)
#define SI_SIGIO (-3)
#define SI_TKILL (-4)
#define SI_ASYNCIO (-5)
#define SI_MESGQ (-6)

//
// Define the minimum and default signal stack size.
//

#define MINSIGSTKSZ 2048
#define SIGSTKSZ 8192

//
// ------------------------------------------------------ Data Type Definitions
//

typedef int sig_atomic_t;

//
// Define the signal set data type.
//

typedef unsigned long long sigset_t;

/*++

Structure Description:

    This structure stores time in a form more accurate than one second.

Members:

    tv_sec - Stores the count of seconds.

    tv_nsec - Stores the count of nanoseconds.

--*/

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

/*++

Structure Description:

    This structure stores information about a C stack.

Members:

    ss_sp - Stores the stack pointer.

    ss_flags - Stores a bitfield of flags. See SS_* definitions.

    ss_size - Stores the size of the stack.

--*/

typedef struct sigaltstack {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;

//
// Define the type that is sent as a parameter with real time signals. It's
// always at least as big as the larger of an integer and a pointer.
//

union sigval {
    int sival_int;
    void *sival_ptr;
};

/*++

Structure Description:

    This structure describes information about a pending signal.

Members:

    si_signo - Stores the signal number.

    si_code - Stores the signal code, which usually contains details about the
        signal that are specific to each type of signal.

    si_errno - Stores an error number associated with this signal.

    si_pid - Stores the identifier of the process sending the signal.

    si_uid - Stores the real user ID of the sending process.

    si_addr - Stores the address of the faulting instruction for fault signals.

    si_status - Stores the exit status or signal number for child process
        signals.

    si_band - Stores the band event for poll signals.

    si_value - Stores the value of the signal for real time signals.

    si_fd - Stores the file descriptor that triggered the poll signal.

--*/

typedef struct {
    int si_signo;
    int si_code;
    int si_errno;
    pid_t si_pid;
    uid_t si_uid;
    void *si_addr;
    int si_status;
    long si_band;
    union sigval si_value;
    int si_fd;
} siginfo_t;

/*++

Structure Description:

    This structure contains enough information to perform a specific action
    when a signal arrives.

Members:

    sa_handler - Stores a pointer to a function to be called to handle the
        signal. It takes one parameter, the signal number.

    sa_sigaction - Stores a pointer to a function to be called to handle the
        signal. It takes three parameters: the signal number, a pointer to the
        signal information, and a pointer to the signal context.

    sa_mask - Stores the mask of signals to add to the mask of blocked signals
        when this handler is called.

    sa_flags - Stores additional flags. See SA_* definitions.

--*/

struct sigaction {
    union {
        void (*sau_handler)(int);
        void (*sau_sigaction)(int, siginfo_t *, void *);
    } sa_u;

    sigset_t sa_mask;
    int sa_flags;
};

/*++

Structure Description:

    This structure defines a signal event structure.

Members:

    sigev_notify - Stores the notification type. See SIGEV_* definitions.

    sigev_signo - Stores the signal number.

    sigev_value - Stores the signal value.

    sigev_notify_function - Stores a pointer to the function to call (and act
        as the thread entry point) for types of SIGEV_THREAD.

    sigev_notify_attributes - Stores the attributes associated with the notify
        function.

    sigev_notify_thread_id - Stores the kernel thread ID of the thread to
        signal for types of SIGEV_THREAD_ID.

--*/

typedef struct sigevent {
    int sigev_notify;
    int sigev_signo;
    union sigval sigev_value;
    void (*sigev_notify_function)(union sigval);
    void *sigev_notify_attributes;
    pid_t sigev_notify_thread_id;
} sigevent_t;

//
// -------------------------------------------------------------------- Globals
//

//
// Define an array of strings, indexed up to NSIG, that contain descriptions of
// the strings.
//

LIBC_API extern const char *sys_siglist[];

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
siginterrupt (
    int Signal,
    int Flag
    );

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

LIBC_API
int
sigaction (
    int SignalNumber,
    struct sigaction *NewAction,
    struct sigaction *OriginalAction
    );

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

LIBC_API
int
sigaddset (
    sigset_t *SignalSet,
    int SignalNumber
    );

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

LIBC_API
int
sigemptyset (
    sigset_t *SignalSet
    );

/*++

Routine Description:

    This routine initializes the given signal set to contain no signals, the
    empty set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to initialize.

Return Value:

    0 always to indicate success.

--*/

LIBC_API
int
sigdelset (
    sigset_t *SignalSet,
    int SignalNumber
    );

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

LIBC_API
int
sigfillset (
    sigset_t *SignalSet
    );

/*++

Routine Description:

    This routine initializes the given signal set to contain all signals set.

Arguments:

    SignalSet - Supplies a pointer to the signal set to initialize.

Return Value:

    0 always to indicate success.

--*/

LIBC_API
void (
*signal (
    int Signal,
    void (*SignalFunction)(int)
    ))(int);

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

LIBC_API
int
sigismember (
    const sigset_t *SignalSet,
    int SignalNumber
    );

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

LIBC_API
int
sigprocmask (
    int LogicalOperation,
    const sigset_t *SignalSet,
    sigset_t *OriginalSignalSet
    );

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

LIBC_API
int
pthread_sigmask (
    int LogicalOperation,
    const sigset_t *SignalSet,
    sigset_t *OriginalSignalSet
    );

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

LIBC_API
int
kill (
    pid_t ProcessId,
    int SignalNumber
    );

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

LIBC_API
int
killpg (
    pid_t ProcessGroupId,
    int SignalNumber
    );

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

LIBC_API
int
raise (
    int SignalNumber
    );

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

LIBC_API
int
sigqueue (
    pid_t ProcessId,
    int SignalNumber,
    union sigval Value
    );

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

LIBC_API
int
sigpending (
    sigset_t *SignalSet
    );

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

LIBC_API
int
sigsuspend (
    const sigset_t *SignalMask
    );

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

LIBC_API
int
sigwait (
    const sigset_t *SignalSet,
    int *SignalNumber
    );

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

LIBC_API
int
sigwaitinfo (
    const sigset_t *SignalSet,
    siginfo_t *SignalInformation
    );

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

LIBC_API
int
sigtimedwait (
    const sigset_t *SignalSet,
    siginfo_t *SignalInformation,
    const struct timespec *Timeout
    );

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

LIBC_API
void
psignal (
    int Signal,
    char *String
    );

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

#ifdef __cplusplus

}

#endif
#endif

