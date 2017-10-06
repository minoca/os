/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    resource.h

Abstract:

    This header contains definitions for system resource operations.

Author:

    Evan Green 9-Jul-2014

--*/

#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/time.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Definitions for the "which" when getting resource usage.
//

//
// Get resource usage for a specified process ID.
//

#define PRIO_PROCESS 1

//
// Get resource usage for a specified process group.
//

#define PRIO_PGRP 2

//
// Get resource usage for a specified user ID.
//

#define PRIO_USER 3

//
// Define the highest possible limit value.
//

#define RLIM_INFINITY (-1UL)
#define RLIM64_INFINITY RLIM_INFINITY

//
// All values can be represented.
//

#define RLIM_SAVED_MAX RLIM_INFINITY
#define RLIM_SAVED_CUR RLIM_INFINITY

//
// Definitinos for the "who" when getting and setting resource usage.
//

//
// Return information about the current process. This is the sum of all the
// process threads.
//

#define RUSAGE_SELF 1

//
// Return information about the children of the current process.
//

#define RUSAGE_CHILDREN 2

//
// Return information about the current thread.
//

#define RUSAGE_THREAD 3

//
// Define the different kinds of resource limits.
//

//
// Limit the size of the core file. A value of 0 prevents the creation of core
// files.
//

#define RLIMIT_CORE 0

//
// Limit the CPU time per process. If this time is exceeded, a SIGXCPU signal
// is sent to the process once a second until the hard limit is reached.
//

#define RLIMIT_CPU 1

//
// Limit the data segment size (all memory used by the process). If this limit
// is exceeded, additional system memory allocation requests will fail with
// errno set to ENOMEM.
//

#define RLIMIT_DATA 2

//
// Limit the maximum file size. If a write or truncate operation would cause
// this limit to be exceeded, SIGXFSZ will be sent to the thread. Continued
// attempts to increase the file size beyond the limit will fail with EFBIG.
//

#define RLIMIT_FSIZE 3

//
// Limit the number of open files. This is one greater than the maximum value
// the system may assign to a newly created descriptor. If this limit is
// exceeded, functions that allocate a file descriptor shall fail with errno
// set to EMFILE.
//

#define RLIMIT_NOFILE 4

//
// Limit the stack size. If this limit is exceeded, SIGSEGV is generated for
// the thread. If the thread is blocking SIGSEGV, or the process is ignoring
// or catching the signal and has not made arrangements to use an alternate
// stack, the disposition of SIGSEGV shall be set to SIG_DFL before it is
// generated.
//

#define RLIMIT_STACK 5

//
// Limit the address space size.
//

#define RLIMIT_AS 6

//
// Limit the number of processes that can be created. Attempts to fork beyond
// this limit fail with errno set to EAGAIN.
//

#define RLIMIT_NPROC 7

//
// Limit the number of pending signals.
//

#define RLIMIT_SIGPENDING 8

//
// Limit the nice value.
//

#define RLIMIT_NICE 9

//
// Define the number of different resource limits. Any valid RLIMIT_* value
// must be less than this value.
//

#define RLIM_NLIMITS 10

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Type used for describing resource limit values.
//

typedef unsigned long rlim_t, rlim64_t;

/*++

Structure Description:

    This structure defines information about a resource limit.

Members:

    rlim_cur - Stores the current (soft) limit. This is the limit the system
        enforces.

    rlim_max - Stores the maximum value the soft limit can be set to.

--*/

struct rlimit {
    rlim_t rlim_cur;
    rlim_t rlim_max;
};

/*++

Structure Description:

    This structure defines information about the resource usage of a process,
    process group, or user. Not all fields are currently filled in. Unused
    fields will be set to zero.

Members:

    ru_utime - Stores the user time used.

    ru_stime - Stores the system time used.

    ru_maxrss - Stores the maximum resident set size used (in kilobytes). For
        RUSAGE_CHILDREN, this is the resident size of the largest child, not
        the sum total. Not currently used.

    ru_ixrss - Stores the integral shared memory size. Not currently used.

    ru_idrss - Stores the integral unshared memory data size. Not currently
        used.

    ru_isrss - Stores the integral unshared stack size. Not currently used.

    ru_minflt - Stores the number of page faults serviced without any I/O
        activity. Not currently used.

    ru_majflt - Stores the number of page faults serviced that required I/O
        activity. Not currently used.

    ru_nswap - Stores the number of times the process was swapped out. Not
        currently used.

    ru_inblock - Stores the number of times the file system performed input.
        Not currently used.

    ru_oublock - Stores the number of times the file system performed output.
        Not currently used.

    ru_msgsend - Stores the number of IPC messages sent. Not currently used.

    ru_msgrcv - Stores the number of IPC messages received. Not currently used.

    ru_nsignals - Stores the number of signals received. Not currently used.

    ru_nvcsw - Stores the number of voluntary context switches (yields).

    ru_nivcsw - Stores the number of involuntary context switches (preemptions).

--*/

struct rusage {
    struct timeval ru_utime;
    struct timeval ru_stime;
    long ru_maxrss;
    long ru_ixrss;
    long ru_idrss;
    long ru_isrss;
    long ru_minflt;
    long ru_majflt;
    long ru_nswap;
    long ru_inblock;
    long ru_oublock;
    long ru_msgsnd;
    long ru_msgrcv;
    long ru_nsignals;
    long ru_nvcsw;
    long ru_nivcsw;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
getpriority (
    int Which,
    id_t Who
    );

/*++

Routine Description:

    This routine returns the nice value of a process, process group, or user.

Arguments:

    Which - Supplies which entity to get the nice value of. See PRIO_*
        definitions, which allow the caller to get the nice value of the
        process, process group, or user.

    Who - Supplies the identifier of the process, process group, or user to
        query. A value of zero specifies the current process, process group, or
        user.

Return Value:

    Returns a value in the range of -NZERO to NZERO-1 representing the current
    nice value.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
setpriority (
    int Which,
    id_t Who,
    int Value
    );

/*++

Routine Description:

    This routine sets the nice value of a process, process group, or user.

Arguments:

    Which - Supplies which entity to set the nice value for. See PRIO_*
        definitions, which allow the caller to set the nice value of the
        process, process group, or user.

    Who - Supplies the identifier of the process, process group, or user to
        set. A value of zero specifies the current process, process group, or
        user.

    Value - Supplies the new nice value to set.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
getrlimit (
    int Resource,
    struct rlimit *Limit
    );

/*++

Routine Description:

    This routine returns the resource consumption limit of a given resource
    type.

Arguments:

    Resource - Supplies the type of resource to get the limit for.

    Limit - Supplies a pointer where the soft (current) and hard limits for
        the resource are returned. If resource enforcement is not enabled for
        the given resource, RLIM_INFINITY is returned in these members.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
setrlimit (
    int Resource,
    const struct rlimit *Limit
    );

/*++

Routine Description:

    This routine sets the resource consumption limit of a given resource
    type. Processes can adjust their soft limits between 0 and the hard limit
    (though for certain resource types the value may be adjusted). Processes
    can irreversibly decrease their hard limits. Only a process with
    appropriate permissions can increase the hard limit.

Arguments:

    Resource - Supplies the type of resource to get the limit for.

    Limit - Supplies a pointer where the soft (current) and hard limits for
        the resource are returned. If resource enforcement is not enabled for
        the given resource, RLIM_INFINITY is returned in these members.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

LIBC_API
int
getrusage (
    int Who,
    struct rusage *Usage
    );

/*++

Routine Description:

    This routine returns the usage information for a given process, process
    group, or user.

Arguments:

    Who - Supplies the entity or entities to get usage for. Valid values are
        RUSAGE_SELF to get resource usage for the current process, or
        RUSAGE_CHILDREN to get resource usage for terminated and waited for
        children of the current process. Additionally, RUSAGE_THREAD will get
        the usage information for the current thread, though this is a
        non-portable option.

    Usage - Supplies a pointer where the usage information is returned.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to contain more
    information.

--*/

#ifdef __cplusplus

}

#endif
#endif

