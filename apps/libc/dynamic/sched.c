/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sched.c

Abstract:

    This module implements scheduling functionality for the C library.

Author:

    Chris Stevens 11-Jul-2016

Environment:

    User Mode C Library.

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <sched.h>
#include <errno.h>

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
sched_yield (
    void
    )

/*++

Routine Description:

    This routine causes the current thread to yield execution of the processor.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

{

    KSTATUS Status;

    //
    // A zero second delay is equivalent to a yield.
    //

    Status = OsDelayExecution(FALSE, 0);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

