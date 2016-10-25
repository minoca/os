/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sysconf.c

Abstract:

    This module implements the sysconf function, which provides operating
    system limits and values to the application.

Author:

    Evan Green 24-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>

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
// ------------------------------------------------------------------ Functions
//

LIBC_API
long
sysconf (
    int Variable
    )

/*++

Routine Description:

    This routine gets the system value for the given variable index. These
    variables are not expected to change within a single invocation of a
    process, and therefore need only be queried once per process.

Arguments:

    Variable - Supplies the variable to get. See _SC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

{

    ULONGLONG BigValue;
    MM_STATISTICS MmStatistics;
    PROCESSOR_COUNT_INFORMATION ProcessorCountInformation;
    UINTN Size;
    KSTATUS Status;
    long Value;

    switch (Variable) {
    case _SC_CLK_TCK:
        Value = CLOCKS_PER_SEC;
        break;

    case _SC_PAGE_SIZE:
    case _SC_PHYS_PAGES:
    case _SC_AVPHYS_PAGES:
        MmStatistics.Version = MM_STATISTICS_VERSION;
        Size = sizeof(MM_STATISTICS);
        Status = OsGetSetSystemInformation(SystemInformationMm,
                                           MmInformationSystemMemory,
                                           &MmStatistics,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("Get MmStatistics failed: %d\n", Status);
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        if (Variable == _SC_PAGE_SIZE) {
            BigValue = MmStatistics.PageSize;

        } else if (Variable == _SC_PHYS_PAGES) {
            BigValue = MmStatistics.PhysicalPages;

        } else if (Variable == _SC_AVPHYS_PAGES) {
            BigValue = MmStatistics.AllocatedPhysicalPages;

        } else {

            assert(FALSE);

            BigValue = -1;
        }

        if (BigValue > MAX_LONG) {
            Value = MAX_LONG;

        } else {
            Value = (long)BigValue;
        }

        break;

    case _SC_ARG_MAX:
    case _SC_CHILD_MAX:
    case _SC_HOST_NAME_MAX:
    case _SC_LOGIN_NAME_MAX:
    case _SC_RE_DUP_MAX:
    case _SC_TTY_NAME_MAX:
    case _SC_EXPR_NEST_MAX:
    case _SC_LINE_MAX:
        Value = -1;
        break;

    case _SC_OPEN_MAX:
        Value = OB_MAX_HANDLES;
        break;

    case _SC_STREAM_MAX:
        Value = INT_MAX;
        break;

    case _SC_SYMLOOP_MAX:
        Value = MAX_SYMBOLIC_LINK_RECURSION;
        break;

    case _SC_TZNAME_MAX:
        Value = _POSIX_TZNAME_MAX;
        break;

    case _SC_VERSION:
        Value = _POSIX_VERSION;
        break;

    case _SC_2_VERSION:
        Value = _POSIX2_VERSION;
        break;

    case _SC_2_LOCALEDEF:
        Value = _POSIX2_LOCALEDEF;
        break;

    case _SC_2_SW_DEV:
        Value = _POSIX2_SW_DEV;
        break;

    case _SC_2_C_DEV:
        Value = _POSIX2_C_DEV;
        break;

    case _SC_BC_BASE_MAX:
    case _SC_BC_SCALE_MAX:
        Value = 99;
        break;

    case _SC_BC_DIM_MAX:
        Value = 2048;
        break;

    case _SC_BC_STRING_MAX:
        Value = 1000;
        break;

    case _SC_COLL_WEIGHTS_MAX:
        Value = -1;
        break;

    case _SC_2_FORT_DEV:
        Value = _POSIX2_FORT_DEV;
        break;

    case _SC_2_FORT_RUN:
        Value = _POSIX2_FORT_RUN;
        break;

    case _SC_NPROCESSORS_CONF:
    case _SC_NPROCESSORS_ONLN:
        Size = sizeof(PROCESSOR_COUNT_INFORMATION);
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationProcessorCount,
                                           &ProcessorCountInformation,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            errno = ClConvertKstatusToErrorNumber(Status);
            return -1;
        }

        if (Variable == _SC_NPROCESSORS_CONF) {
            Value = ProcessorCountInformation.MaxProcessorCount;

        } else if (Variable == _SC_NPROCESSORS_ONLN) {
            Value = ProcessorCountInformation.ActiveProcessorCount;

        } else {

            assert(FALSE);

            Value = -1;
        }

        break;

    case _SC_GETGR_R_SIZE_MAX:
        Value = USER_DATABASE_LINE_MAX;
        break;

    case _SC_GETPW_R_SIZE_MAX:
        Value = USER_DATABASE_LINE_MAX;
        break;

    case _SC_NGROUPS_MAX:
        Value = NGROUPS_MAX;
        break;

    case _SC_BARRIERS:
        Value = _POSIX_BARRIERS;
        break;

    case _SC_CLOCK_SELECTION:
        Value = _POSIX_CLOCK_SELECTION;
        break;

    case _SC_CPUTIME:
        Value = _POSIX_CPUTIME;
        break;

    case _SC_FSYNC:
        Value = _POSIX_FSYNC;
        break;

    case _SC_IPV6:
        Value = _POSIX_IPV6;
        break;

    case _SC_JOB_CONTROL:
        Value = _POSIX_JOB_CONTROL;
        break;

    case _SC_MAPPED_FILES:
        Value = _POSIX_MAPPED_FILES;
        break;

    case _SC_MEMLOCK:
        Value = _POSIX_MEMLOCK;
        break;

    case _SC_MEMLOCK_RANGE:
        Value = _POSIX_MEMLOCK_RANGE;
        break;

    case _SC_MEMORY_PROTECTION:
        Value = _POSIX_MEMORY_PROTECTION;
        break;

    case _SC_MESSAGE_PASSING:
        Value = _POSIX_MESSAGE_PASSING;
        break;

    case _SC_MONOTONIC_CLOCK:
        Value = _POSIX_MONOTONIC_CLOCK;
        break;

    case _SC_PRIORITIZED_IO:
        Value = _POSIX_PRIORITIZED_IO;
        break;

    case _SC_PRIORITY_SCHEDULING:
        Value = _POSIX_PRIORITY_SCHEDULING;
        break;

    case _SC_RAW_SOCKETS:
        Value = _POSIX_RAW_SOCKETS;
        break;

    case _SC_READER_WRITER_LOCKS:
        Value = _POSIX_READER_WRITER_LOCKS;
        break;

    case _SC_REALTIME_SIGNALS:
        Value = _POSIX_REALTIME_SIGNALS;
        break;

    case _SC_REGEXP:
        Value = _POSIX_REGEXP;
        break;

    case _SC_SAVED_IDS:
        Value = _POSIX_SAVED_IDS;
        break;

    case _SC_SEMAPHORES:
        Value = _POSIX_SEMAPHORES;
        break;

    case _SC_SHARED_MEMORY_OBJECTS:
        Value = _POSIX_SHARED_MEMORY_OBJECTS;
        break;

    case _SC_SHELL:
        Value = _POSIX_SHELL;
        break;

    case _SC_SPAWN:
        Value = _POSIX_SPAWN;
        break;

    case _SC_SPIN_LOCKS:
        Value = _POSIX_SPIN_LOCKS;
        break;

    case _SC_SPORADIC_SERVER:
        Value = _POSIX_SPORADIC_SERVER;
        break;

    case _SC_SYNCHRONIZED_IO:
        Value = _POSIX_SYNCHRONIZED_IO;
        break;

    case _SC_THREAD_ATTR_STACKADDR:
        Value = _POSIX_THREAD_ATTR_STACKADDR;
        break;

    case _SC_THREAD_ATTR_STACKSIZE:
        Value = _POSIX_THREAD_ATTR_STACKSIZE;
        break;

    case _SC_THREAD_CPUTIME:
        Value = _POSIX_THREAD_CPUTIME;
        break;

    case _SC_THREAD_PRIO_INHERIT:
        Value = _POSIX_THREAD_PRIO_INHERIT;
        break;

    case _SC_THREAD_PRIO_PROTECT:
        Value = _POSIX_THREAD_PRIO_PROTECT;
        break;

    case _SC_THREAD_PRIORITY_SCHEDULING:
        Value = _POSIX_THREAD_PRIORITY_SCHEDULING;
        break;

    case _SC_THREAD_PROCESS_SHARED:
        Value = _POSIX_THREAD_PROCESS_SHARED;
        break;

    case _SC_THREAD_ROBUST_PRIO_INHERIT:
        Value = _POSIX_THREAD_ROBUST_PRIO_INHERIT;
        break;

    case _SC_THREAD_ROBUST_PRIO_PROTECT:
        Value = _POSIX_THREAD_ROBUST_PRIO_PROTECT;
        break;

    case _SC_THREAD_SAFE_FUNCTIONS:
        Value = _POSIX_THREAD_SAFE_FUNCTIONS;
        break;

    case _SC_THREAD_SPORADIC_SERVER:
        Value = _POSIX_THREAD_SPORADIC_SERVER;
        break;

    case _SC_THREADS:
        Value = _POSIX_THREADS;
        break;

    case _SC_TIMEOUTS:
        Value = _POSIX_TIMEOUTS;
        break;

    case _SC_TIMERS:
        Value = _POSIX_TIMERS;
        break;

    case _SC_TRACE:
        Value = _POSIX_TRACE;
        break;

    case _SC_TRACE_EVENT_FILTER:
        Value = _POSIX_TRACE_EVENT_FILTER;
        break;

    case _SC_TRACE_INHERIT:
        Value = _POSIX_TRACE_INHERIT;
        break;

    case _SC_TRACE_LOG:
        Value = _POSIX_TRACE_LOG;
        break;

    case _SC_TYPED_MEMORY_OBJECTS:
        Value = _POSIX_TYPED_MEMORY_OBJECTS;
        break;

    case _SC_V6_ILP32_OFF32:
        Value = _POSIX_V6_ILP32_OFF32;
        break;

    case _SC_V6_ILP32_OFFBIG:
        Value = _POSIX_V6_ILP32_OFFBIG;
        break;

    case _SC_V6_LP64_OFF64:
        Value = _POSIX_V6_LP64_OFF64;
        break;

    case _SC_V6_LPBIG_OFFBIG:
        Value = _POSIX_V6_LPBIG_OFFBIG;
        break;

    case _SC_V7_ILP32_OFF32:
        Value = _POSIX_V7_ILP32_OFF32;
        break;

    case _SC_V7_ILP32_OFFBIG:
        Value = _POSIX_V7_ILP32_OFFBIG;
        break;

    case _SC_V7_LP64_OFF64:
        Value = _POSIX_V7_LP64_OFF64;
        break;

    case _SC_V7_LPBIG_OFFBIG:
        Value = _POSIX_V7_LPBIG_OFFBIG;
        break;

    case _SC_2_C_BIND:
        Value = _POSIX2_C_BIND;
        break;

    case _SC_2_CHAR_TERM:
        Value = _POSIX2_CHAR_TERM;
        break;

    case _SC_2_PBS:
        Value = _POSIX2_PBS;
        break;

    case _SC_2_PBS_ACCOUNTING:
        Value = _POSIX2_PBS_ACCOUNTING;
        break;

    case _SC_2_PBS_CHECKPOINT:
        Value = _POSIX2_PBS_CHECKPOINT;
        break;

    case _SC_2_PBS_LOCATE:
        Value = _POSIX2_PBS_LOCATE;
        break;

    case _SC_2_PBS_MESSAGE:
        Value = _POSIX2_PBS_MESSAGE;
        break;

    case _SC_2_PBS_TRACK:
        Value = _POSIX2_PBS_TRACK;
        break;

    case _SC_2_UPE:
        Value = _POSIX2_UPE;
        break;

    case _SC_XOPEN_CRYPT:
        Value = _XOPEN_CRYPT;
        break;

    case _SC_XOPEN_ENH_I18N:
        Value = _XOPEN_ENH_I18N;
        break;

    case _SC_XOPEN_REALTIME:
        Value = _XOPEN_REALTIME;
        break;

    case _SC_XOPEN_REALTIME_THREADS:
        Value = _XOPEN_REALTIME_THREADS;
        break;

    case _SC_XOPEN_SHM:
        Value = _XOPEN_SHM;
        break;

    case _SC_XOPEN_STREAMS:
        Value = _XOPEN_STREAMS;
        break;

    case _SC_XOPEN_UNIX:
        Value = _XOPEN_UNIX;
        break;

    case _SC_XOPEN_UUCP:
        Value = _XOPEN_UUCP;
        break;

    case _SC_XOPEN_VERSION:
        Value = _XOPEN_VERSION;
        break;

    default:
        fprintf(stderr, "SYSCONF called with unknown variable %d.\n", Variable);

        assert(FALSE);

        Value = -1;
        errno = EINVAL;
        break;
    }

    return Value;
}

LIBC_API
long
fpathconf (
    int FileDescriptor,
    int Variable
    )

/*++

Routine Description:

    This routine gets the current value of a configurable limit or option
    that is associated with the given open file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to query the value for.

    Variable - Supplies the variable to get. See _PC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

{

    long Result;

    //
    // TODO: Return real fpathconf information at some point.
    //

    switch (Variable) {
    case _PC_2_SYMLINKS:
        Result = 1;
        break;

    case _PC_ALLOC_SIZE_MIN:
        Result = 4096;
        break;

    case _PC_ASYNC_IO:
        Result = 0;
        break;

    case _PC_CHOWN_RESTRICTED:
        Result = _POSIX_CHOWN_RESTRICTED;
        break;

    case _PC_FILESIZEBITS:
        Result = 32;
        break;

    case _PC_LINK_MAX:
        Result = _POSIX_LINK_MAX;
        break;

    case _PC_MAX_CANON:
        Result = MAX_CANON;
        break;

    case _PC_MAX_INPUT:
        Result = MAX_INPUT;
        break;

    case _PC_NAME_MAX:
        Result = NAME_MAX;
        break;

    case _PC_NO_TRUNC:
        Result = _POSIX_NO_TRUNC;
        break;

    case _PC_PATH_MAX:
        Result = PATH_MAX;
        break;

    case _PC_PIPE_BUF:
        Result = PIPE_BUF;
        break;

    case _PC_PRIO_IO:
        Result = 0;
        break;

    case _PC_REC_INCR_XFER_SIZE:
        Result = 4096;
        break;

    case _PC_REC_MIN_XFER_SIZE:
        Result = 4096;
        break;

    case _PC_REC_XFER_ALIGN:
        Result = 4096;
        break;

    case _PC_SYMLINK_MAX:
        Result = _POSIX_SYMLINK_MAX;
        break;

    case _PC_SYNC_IO:
        Result = 1;
        break;

    case _PC_VDISABLE:
        Result = _POSIX_VDISABLE;
        break;

    default:
        Result = -1;
        errno = EINVAL;
        break;
    }

    return Result;
}

LIBC_API
long
pathconf (
    const char *Path,
    int Variable
    )

/*++

Routine Description:

    This routine gets the current value of a configurable limit or option
    that is associated with the given file or directory path.

Arguments:

    Path - Supplies a pointer to a null terminated string containing the file
        or directory path to get the configuration limit for.

    Variable - Supplies the variable to get. See _PC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

{

    //
    // TODO: Return real pathconf information eventually.
    //

    return fpathconf(-1, Variable);
}

LIBC_API
int
getdtablesize (
    void
    )

/*++

Routine Description:

    This routine returns the maximum number of file descriptors that are
    supported.

Arguments:

    None.

Return Value:

    Returns the maximum number of file descriptors that one process can have
    open.

--*/

{

    return OB_MAX_HANDLES;
}

LIBC_API
int
getpagesize (
    void
    )

/*++

Routine Description:

    This routine returns the number of bytes in the basic unit of memory
    allocation on the current machine, the page. This routine is provided for
    historical reasons, new applications should use sysconf(_SC_PAGESIZE)
    (which is in fact exactly what this routine turns around and does).

Arguments:

    None.

Return Value:

    Returns the number of bytes in a memory page.

--*/

{

    return sysconf(_SC_PAGESIZE);
}

//
// --------------------------------------------------------- Internal Functions
//

