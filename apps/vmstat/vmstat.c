/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    vmstat.c

Abstract:

    This module implements the vmstat application.

Author:

    Evan Green 5-Mar-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/lib/mlibc.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define VMSTAT_VERSION_MAJOR 1
#define VMSTAT_VERSION_MINOR 0

#define VMSTAT_USAGE                                                       \
    "usage: vmstat\n\n"                                                    \
    "The vmstat utility prints information about current system memory \n" \
    "usage. Options are:\n"                                                \
    "  --help -- Display this help text.\n"                                \
    "  --version -- Display the application version and exit.\n\n"

#define VMSTAT_OPTIONS_STRING "hV"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
VmstatPrintInformation (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

struct option VmstatLongOptions[] = {
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the vmstat user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONG ArgumentIndex;
    INT Option;
    INT ReturnValue;

    ReturnValue = 0;

    //
    // If there are no arguments, just print the memory information.
    //

    if (ArgumentCount == 1) {
        ReturnValue = VmstatPrintInformation();
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             VMSTAT_OPTIONS_STRING,
                             VmstatLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            ReturnValue = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'V':
            printf("vmstat version %d.%02d\n",
                   VMSTAT_VERSION_MAJOR,
                   VMSTAT_VERSION_MINOR);

            ReturnValue = 1;
            goto mainEnd;

        case 'h':
            printf(VMSTAT_USAGE);
            return 1;

        default:

            assert(FALSE);

            ReturnValue = 1;
            goto mainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        fprintf(stderr,
                "vmstat: Unexpected argument %s\n",
                Arguments[ArgumentIndex]);
    }

    ReturnValue = 0;

mainEnd:
    return ReturnValue;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
VmstatPrintInformation (
    VOID
    )

/*++

Routine Description:

    This routine prints system memory information.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    IO_CACHE_STATISTICS IoCache;
    ULONGLONG Megabytes;
    MM_STATISTICS MmStatistics;
    INT ReturnValue;
    UINTN Size;
    KSTATUS Status;
    UINTN Value;

    ReturnValue = 0;
    Size = sizeof(MM_STATISTICS);
    MmStatistics.Version = MM_STATISTICS_VERSION;
    Status = OsGetSetSystemInformation(SystemInformationMm,
                                       MmInformationSystemMemory,
                                       &MmStatistics,
                                       &Size,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        ReturnValue = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "Error: failed to get memory information: status 0x%08x: %s.\n",
                Status,
                strerror(ReturnValue));

        return ReturnValue;
    }

    Megabytes = (MmStatistics.PhysicalPages * MmStatistics.PageSize) / _1MB;
    printf("Total Physical Memory: %I64dMB\n", Megabytes);
    Megabytes = (MmStatistics.AllocatedPhysicalPages *
                 MmStatistics.PageSize) / _1MB;

    printf("Allocated Physical Memory: %I64dMB\n", Megabytes);
    Megabytes = (MmStatistics.NonPagedPhysicalPages *
                 MmStatistics.PageSize) / _1MB;

    printf("Non-Paged Physical Memory: %I64dMB\n", Megabytes);
    printf("Non Paged Pool:\n");
    printf("    Size: %ld\n", MmStatistics.NonPagedPool.TotalHeapSize);
    printf("    Maximum Size: %ld\n", MmStatistics.NonPagedPool.MaxHeapSize);
    Value = MmStatistics.NonPagedPool.TotalHeapSize -
            MmStatistics.NonPagedPool.FreeListSize;

    printf("    Allocated: %ld\n", Value);
    printf("    Allocation Count: %ld (lifetime %ld)\n",
           MmStatistics.NonPagedPool.Allocations,
           MmStatistics.NonPagedPool.TotalAllocationCalls);

    printf("    Failed Allocations: %ld\n",
           MmStatistics.NonPagedPool.FailedAllocations);

    printf("Paged Pool:\n");
    printf("    Size: %ld\n", MmStatistics.PagedPool.TotalHeapSize);
    printf("    Maximum Size: %ld\n", MmStatistics.PagedPool.MaxHeapSize);
    Value = MmStatistics.PagedPool.TotalHeapSize -
            MmStatistics.PagedPool.FreeListSize;

    printf("    Allocated: %ld\n", Value);
    printf("    Allocation Count: %ld (lifetime %ld)\n",
           MmStatistics.PagedPool.Allocations,
           MmStatistics.PagedPool.TotalAllocationCalls);

    printf("    Failed Allocations: %ld\n",
           MmStatistics.PagedPool.FailedAllocations);

    Size = sizeof(IO_CACHE_STATISTICS);
    IoCache.Version = IO_CACHE_STATISTICS_VERSION;
    Status = OsGetSetSystemInformation(SystemInformationIo,
                                       IoInformationCacheStatistics,
                                       &IoCache,
                                       &Size,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        ReturnValue = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "Error: failed to get I/O cache information: status %d: "
                "%s.\n",
                Status,
                strerror(ReturnValue));

        return ReturnValue;
    }

    Megabytes = (IoCache.PhysicalPageCount * MmStatistics.PageSize) / _1MB;
    printf("Page Cache Size: %lldMB\n", Megabytes);
    Megabytes = (IoCache.DirtyPageCount * MmStatistics.PageSize) / _1MB;
    printf("Dirty Page Cache Size: %lldMB\n", Megabytes);
    return ReturnValue;
}

