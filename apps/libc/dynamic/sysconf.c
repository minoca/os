/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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
            RtlDebugPrint("Get MmStatistics failed: %x\n", Status);
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
    case _SC_2_VERSION:
    case _SC_2_LOCALEDEF:
    case _SC_2_SW_DEV:
    case _SC_2_C_DEV:
        Value = _POSIX_VERSION;
        break;

    case _SC_BC_BASE_MAX:
    case _SC_BC_SCALE_MAX:
        Value = 99;

    case _SC_BC_DIM_MAX:
        Value = 2048;

    case _SC_BC_STRING_MAX:
        Value = 1000;
        break;

    case _SC_COLL_WEIGHTS_MAX:
        Value = -1;
        break;

    case _SC_2_FORT_DEV:
    case _SC_2_FORT_RUN:
        Value = -1;
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
        Result = 1;
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
        Result = 0;
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

