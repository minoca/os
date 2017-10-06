/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    wait.h

Abstract:

    This header contains definitions for obtaining information about a child
    process that has stopped or terminated.

Author:

    Evan Green 30-Mar-2013

--*/

#ifndef _WAIT_H
#define _WAIT_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <signal.h>
#include <sys/resource.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Use this to get the exit status out of a child process.
//

#define WEXITSTATUS(_Status) (((_Status) & 0x0000FF00) >> 8)

//
// This macro evaluates to nonzero if the status indicates that the child has
// been continued.
//

#define WIFCONTINUED(_Status) (((_Status) & 0xFFFF) == 0xFFFF)

//
// This macro evaluates to nonzero if the status indicates that the child has
// exited.
//

#define WIFEXITED(_Status) (((_Status) & 0x7F) == 0)

//
// This macro evaluates to nonzero if the status indicates that the child has
// exited due to an uncaught signal. This constant matches up to flags in
// ksignals.h. The macro is looking to make sure that the lower 7 bits are not
// all 0 (exited) and are not all 1 (continued or stopped).
//

#define WIFSIGNALED(_Status) (((((_Status) + 1) >> 1) & 0x7F) != 0)

//
// This macro evaluates to nonzero if the status indicates that the child has
// stopped.
//

#define WIFSTOPPED(_Status) (((_Status) & 0xFF) == 0x7F)

//
// This macro returns the stop signal if the child was stopped.
//

#define WSTOPSIG(_Status) WEXITSTATUS(_Status)

//
// This macro returns the signal that caused the process to terminate, if it was
// terminated.
//

#define WTERMSIG(_Status) ((_Status) & 0x7F)

//
// This macro evaluates to nonzero if the child process terminated and produced
// a core dump file.
//

#define WCOREDUMP(_Status) ((_Status) & 0x80)

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this in the wait options to return immediately if no child process
// information is available instead of the usual behavior of blocking until it
// is.
//

#define WNOHANG 0x00000001

//
// Set this option to wait for a process that has just stopped.
//

#define WUNTRACED 0x00000002

//
// Set this option to wait for a process that has just continued.
//

#define WCONTINUED 0x00000004

//
// Set this option to wait for a process that has just exited.
//

#define WEXITED 0x00000008

//
// Set this option to keep the process whose status is returned in a waitable
// state.
//

#define WNOWAIT 0x00000010

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the ID type, which is used to identify whether an ID is a process ID,
// process group ID, or neither.
//

typedef enum {
    P_ALL  = 0,
    P_PID  = 1,
    P_PGID = 2
} idtype_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
pid_t
wait (
    int *Status
    );

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

LIBC_API
pid_t
waitpid (
    pid_t ProcessId,
    int *Status,
    int Options
    );

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

LIBC_API
int
waitid (
    idtype_t IdentifierType,
    id_t ProcessOrGroupIdentifier,
    siginfo_t *SignalInformation,
    int Options
    );

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

LIBC_API
pid_t
wait3 (
    int *Status,
    int Options,
    struct rusage *ResourceUsage
    );

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

LIBC_API
pid_t
wait4 (
    pid_t ProcessId,
    int *Status,
    int Options,
    struct rusage *ResourceUsage
    );

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

#ifdef __cplusplus

}

#endif
#endif

