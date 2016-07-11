/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    sched.h

Abstract:

    This header contains scheduling related definitions.

Author:

    Chris Stevens 11-Jul-2016

--*/

#ifndef _SCHED_H
#define _SCHED_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
sched_yield (
    void
    );

/*++

Routine Description:

    This routine causes the current thread to yield execution of the processor.

Arguments:

    None.

Return Value:

    0 on success.

    -1 on error, and the errno variable will contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

