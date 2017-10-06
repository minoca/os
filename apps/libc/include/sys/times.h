/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    times.h

Abstract:

    This header contains definitions for getting process execution times.

Author:

    Evan Green 23-Jun-2013

--*/

#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H

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
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes information about process running time.

Members:

    tms_utime - Stores the number of clock ticks of user mode time this process
        has accumulated.

    tms_stime - Stores the number of clock ticks of kernel mode time this
        process has accumulated.

    tms_cutime - Stores the number of clock ticks of user mode time terminated
        child processes of this process have accumulated.

    tms_cstime - Stores the number of clock ticks of kernel mode time
        terminated child processes of this process have accumulated.

--*/

struct tms {
    clock_t tms_utime;
    clock_t tms_stime;
    clock_t tms_cutime;
    clock_t tms_cstime;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
clock_t
times (
    struct tms *Times
    );

/*++

Routine Description:

    This routine returns the running time for the current process and its
    children.

Arguments:

    Times - Supplies a pointer where the running time information will be
        returned.

Return Value:

    On success, returns the elapsed real time, in clock ticks, since an
    arbitrary time in the past (like boot time). This point does not change
    from one invocation of times within the process to another. On error, -1
    will be returned and errno will be set to indicate the error.

--*/

#ifdef __cplusplus

}

#endif
#endif

