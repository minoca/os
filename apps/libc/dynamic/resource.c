/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    resource.c

Abstract:

    This module implements support for resource management.

Author:

    Evan Green 8-Jul-2014

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
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#define ASSERT_RESOURCE_LIMITS_EQUIVALENT() \
    assert((RLIMIT_CORE == ResourceLimitCore) && \
           (RLIMIT_CPU == ResourceLimitCpuTime) && \
           (RLIMIT_DATA == ResourceLimitData) && \
           (RLIMIT_FSIZE == ResourceLimitFileSize) && \
           (RLIMIT_NOFILE == ResourceLimitFileCount) && \
           (RLIMIT_STACK == ResourceLimitStack) && \
           (RLIMIT_AS == ResourceLimitAddressSpace) && \
           (RLIMIT_NPROC == ResourceLimitProcessCount) && \
           (RLIMIT_SIGPENDING == ResourceLimitSignals) && \
           (RLIMIT_NICE == ResourceLimitNice) && \
           (sizeof(RESOURCE_LIMIT) == sizeof(struct rlimit)) && \
           (sizeof(rlim_t) == sizeof(UINTN)))

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
int
getpriority (
    int Which,
    id_t Who
    )

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

{

    //
    // TODO: Implement getpriority.
    //

    return 0;
}

LIBC_API
int
setpriority (
    int Which,
    id_t Who,
    int Value
    )

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

{

    //
    // TODO: Implement setpriority.
    //

    errno = ENOSYS;
    return -1;
}

LIBC_API
int
getrlimit (
    int Resource,
    struct rlimit *Limit
    )

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

{

    KSTATUS Status;

    ASSERT_RESOURCE_LIMITS_EQUIVALENT();

    Status = OsSetResourceLimit(Resource, NULL, (PRESOURCE_LIMIT)Limit);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
setrlimit (
    int Resource,
    const struct rlimit *Limit
    )

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

{

    KSTATUS Status;

    ASSERT_RESOURCE_LIMITS_EQUIVALENT();

    Status = OsSetResourceLimit(Resource, (PRESOURCE_LIMIT)Limit, NULL);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

LIBC_API
int
getrusage (
    int Who,
    struct rusage *Usage
    )

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

{

    ULONGLONG Frequency;
    RESOURCE_USAGE_REQUEST Request;
    RESOURCE_USAGE ResourceUsage;
    KSTATUS Status;

    if (Who == RUSAGE_SELF) {
        Request = ResourceUsageRequestProcess;

    } else if (Who == RUSAGE_CHILDREN) {
        Request = ResourceUsageRequestProcessChildren;

    } else if (Who == RUSAGE_THREAD) {
        Request = ResourceUsageRequestThread;

    } else {
        errno = EINVAL;
        return -1;
    }

    Status = OsGetResourceUsage(Request, -1, &ResourceUsage, &Frequency);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    ClpConvertResourceUsage(&ResourceUsage, Frequency, Usage);
    return 0;
}

VOID
ClpConvertResourceUsage (
    PRESOURCE_USAGE KernelUsage,
    ULONGLONG Frequency,
    struct rusage *LibraryUsage
    )

/*++

Routine Description:

    This routine converts a kernel resource usage structure into a struct
    rusage.

Arguments:

    KernelUsage - Supplies a a pointer to the structure to convert.

    Frequency - Supplies the frequency of the processor for converting cycle
        counts.

    LibraryUsage - Supplies a pointer where the library usage structure will
        be returned.

Return Value:

    None.

--*/

{

    ULONGLONG Value;

    memset(LibraryUsage, 0, sizeof(struct rusage));
    ClpConvertCounterToTimeValue(KernelUsage->UserCycles,
                                 Frequency,
                                 &(LibraryUsage->ru_utime));

    ClpConvertCounterToTimeValue(KernelUsage->KernelCycles,
                                 Frequency,
                                 &(LibraryUsage->ru_stime));

    Value = KernelUsage->Yields;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_nvcsw = Value;
    Value = KernelUsage->Preemptions;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_nivcsw = Value;
    Value = KernelUsage->MaxResidentSet;
    Value = (Value * (UINTN)sysconf(_SC_PAGE_SIZE)) / _1KB;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_maxrss = Value;

    ASSERT(KernelUsage->HardPageFaults <= KernelUsage->PageFaults);

    Value = KernelUsage->PageFaults - KernelUsage->HardPageFaults;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_minflt = Value;
    Value = KernelUsage->HardPageFaults;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_majflt = Value;
    Value = KernelUsage->DeviceReads;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_inblock = Value;
    Value = KernelUsage->DeviceWrites;
    if (Value > LONG_MAX) {
        Value = LONG_MAX;
    }

    LibraryUsage->ru_oublock = Value;
    return;
}

LIBC_API
int
getloadavg (
    double LoadAverage[],
    int ElementCount
    )

/*++

Routine Description:

    This routine returns the number of processes in the system run queue
    averaged over various periods of time. The elements returned (up to three)
    return the number of processes over the past one, five, and fifteen minutes.

Arguments:

    LoadAverage - Supplies a pointer where up to three load average values
        will be returned on success.

    ElementCount - Supplies the number of elements in the supplied array.

Return Value:

    Returns the number of elements returned on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    //
    // Consider implementing getloadavg, although it's not clear how useful of
    // an API it really is.
    //

    errno = ENOSYS;
    return -1;
}

//
// --------------------------------------------------------- Internal Functions
//

