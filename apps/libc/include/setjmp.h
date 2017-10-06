/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    setjmp.h

Abstract:

    This header contains definitions for the setjmp and longjmp functions,
    which provide non-local goto support.

Author:

    Evan Green 22-Jul-2013

--*/

#ifndef _SETJMP_H
#define _SETJMP_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

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
// Define the mystery jump buffer type, which is a platform-depenent structure,
// usually used to hold non-volatile variables.
//

typedef long jmp_buf[16];
typedef long sigjmp_buf[16];

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
setjmp (
    jmp_buf Environment
    );

/*++

Routine Description:

    This routine saves the calling environment into the given buffer for
    later use by longjmp.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        application context in.

Return Value:

    0 if this was the direct call to setjmp.

    Non-zero if this was a call from longjmp.

--*/

LIBC_API
int
_setjmp (
    jmp_buf Environment
    );

/*++

Routine Description:

    This routine saves the calling environment into the given buffer for
    later use by longjmp.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        application context in.

Return Value:

    0 if this was the direct call to setjmp.

    Non-zero if this was a call from longjmp.

--*/

LIBC_API
int
sigsetjmp (
    sigjmp_buf Environment,
    int SaveMask
    );

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

LIBC_API
void
longjmp (
    jmp_buf Environment,
    int Value
    );

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

LIBC_API
void
_longjmp (
    jmp_buf Environment,
    int Value
    );

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

LIBC_API
void
siglongjmp (
    sigjmp_buf Environment,
    int Value
    );

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

#ifdef __cplusplus

}

#endif
#endif

