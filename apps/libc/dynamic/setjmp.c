/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    setjmp.c

Abstract:

    This module implements the setjmp and longjmp functions used for non-local
    goto statements.

Author:

    Evan Green 28-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <setjmp.h>
#include <signal.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

void
ClpLongJump (
    jmp_buf Environment,
    int Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
longjmp (
    jmp_buf Environment,
    int Value
    )

/*++

Routine Description:

    This routine restores the environment saved by the most recent invocation
    of setjmp with the given environment buffer. If there is no such invocation,
    or if the function containing the invocation of setjmp has terminated in
    the interim, the behavior is undefined. Most likely, the app will crash
    spectacularly.

Arguments:

    Environment - Supplies the pointer to the previously saved environment to
        restore.

    Value - Supplies the value to set as the return value from the setjmp
        function. If this value is 0, it will be set to 1.

Return Value:

    None, this routine does not return.

--*/

{

    if (Value == 0) {
        Value = 1;
    }

    return ClpLongJump(Environment, Value);
}

LIBC_API
void
_longjmp (
    jmp_buf Environment,
    int Value
    )

/*++

Routine Description:

    This routine restores the environment saved by the most recent invocation
    of setjmp with the given environment buffer. If there is no such invocation,
    or if the function containing the invocation of setjmp has terminated in
    the interim, the behavior is undefined. Most likely, the app will crash
    spectacularly.

Arguments:

    Environment - Supplies the pointer to the previously saved environment to
        restore.

    Value - Supplies the value to set as the return value from the setjmp
        function. If this value is 0, it will be set to 1.

Return Value:

    None, this routine does not return.

--*/

{

    if (Value == 0) {
        Value = 1;
    }

    return ClpLongJump(Environment, Value);
}

LIBC_API
void
siglongjmp (
    sigjmp_buf Environment,
    int Value
    )

/*++

Routine Description:

    This routine restores the environment saved by the most recent invocation
    of setjmp with the given environment buffer. It works just like the longjmp
    function, except it also restores the signal mask if the given environment
    buffer was initialized with sigsetjmp with a non-zero save mask value.

Arguments:

    Environment - Supplies the pointer to the previously saved environment to
        restore.

    Value - Supplies the value to set as the return value from the setjmp
        function. If this value is 0, it will be set to 1.

Return Value:

    None, this routine does not return.

--*/

{

    if (Environment[0] != 0) {
        sigprocmask(SIG_SETMASK, (sigset_t *)&(Environment[1]), NULL);
    }

    if (Value == 0) {
        Value = 1;
    }

    return ClpLongJump(Environment, Value);
}

void
ClpSetJump (
    jmp_buf Environment,
    int SaveMask
    )

/*++

Routine Description:

    This routine saves the calling environment into the given buffer for
    later use by longjmp.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        application context in.

    SaveMask - Supplies a value indicating if the caller would like the
        current signal mask to be saved in the environment as well.

Return Value:

    0 if this was the direct call to setjmp.

    Non-zero if this was a call from longjmp.

--*/

{

    Environment[0] = SaveMask;
    if (SaveMask != 0) {

        ASSERT(sizeof(sigset_t) <= sizeof(unsigned long long));

        sigprocmask(SIG_BLOCK, NULL, (sigset_t *)&(Environment[1]));
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

