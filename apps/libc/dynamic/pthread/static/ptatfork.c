/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ptatfork.c

Abstract:

    This module implements the dynamic-object aware version of the
    pthread_atfork function.

Author:

    Evan Green 4-May-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <pthread.h>

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
// There exists a per-module pointer, whose address is unique to each dynamic
// module (or executable).
//

extern void *__dso_handle;

//
// ------------------------------------------------------------------ Functions
//

//
// This routine must be statically linked in to any shared library or
// application, as it references an object that is unique per dynamic library.
//

__HIDDEN
int
pthread_atfork (
    void (*PrepareRoutine)(void),
    void (*ParentRoutine)(void),
    void (*ChildRoutine)(void)
    )

/*++

Routine Description:

    This routine is called to register an at-fork handler, whose callbacks are
    called immediately before and after any fork operation.

Arguments:

    PrepareRoutine - Supplies an optional pointer to a routine to be called
        immediately before a fork operation.

    ParentRoutine - Supplies an optional pointer to a routine to be called
        after a fork in the parent process.

    ChildRoutine - Supplies an optional pointer to ao routine to be called
        after a fork in the child process.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    int Status;

    Status = __register_atfork(PrepareRoutine,
                               ParentRoutine,
                               ChildRoutine,
                               &__dso_handle);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

