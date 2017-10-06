/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    fenv.h

Abstract:

    This header contains definitions for the C library floating point
    environment.

Author:

    Evan Green 18-Dec-2013

--*/

#ifndef _FENV_H
#define _FENV_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

#if defined(__i386) || defined(__amd64)

#define FE_INVALID      0x0001
#define FE_DENORM       0x0002
#define FE_DIVBYZERO    0x0004
#define FE_OVERFLOW     0x0008
#define FE_UNDERFLOW    0x0010
#define FE_INEXACT      0x0020

#define FE_ALL_EXCEPT                                                       \
    (FE_INVALID | FE_DENORM | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW |   \
     FE_INEXACT)

#define FE_TONEAREST    0x0000
#define FE_DOWNWARD     0x0400
#define FE_UPWARD       0x0800
#define FE_TOWARDZERO   0x0C00

#define FE_DFL_ENV ((const fenv_t *)-1)

#elif defined(__arm__)

//
// Define the bits based on VFPv3 FPSCR.
//

#define FE_INVALID      0x0001
#define FE_DIVBYZERO    0x0002
#define FE_OVERFLOW     0x0004
#define FE_UNDERFLOW    0x0008
#define FE_INEXACT      0x0010
#define FE_DENORM       0x0080

#define FE_EXCEPT_SHIFT 8

#define FE_ALL_EXCEPT \
    (FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT | \
     FE_DENORM)

#define FE_TONEAREST    0x00000000
#define FE_UPWARD       0x00400000
#define FE_DOWNWARD     0x00800000
#define FE_TOWARDZERO   0x00C00000

#define FE_DFL_ENV ((const fenv_t *)-1)

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

#if defined(__i386) || defined(__amd64)

typedef unsigned short fexcept_t;

typedef struct {
    unsigned int Control;
    unsigned int Status;
    unsigned int Tag;
    unsigned int InstructionPointer;
    unsigned short int CsSelector;
    unsigned short int Opcode;
    unsigned int OperandPointer;
    unsigned int OperandSelector;
} fenv_t;

#elif defined(__arm__)

typedef unsigned int fexcept_t;

typedef struct {
    unsigned int Fpscr;
} fenv_t;

#endif

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#if defined(__i386) || defined(__amd64) || defined(__arm__)

LIBC_API
int
fegetenv (
    fenv_t *Environment
    );

/*++

Routine Description:

    This routine stores the current floating point machine environment into the
    given environment pointer.

Arguments:

    Environment - Supplies the pointer to the environment to save the
        floating point context in.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
fesetenv (
    const fenv_t *Environment
    );

/*++

Routine Description:

    This routine sets the current machine floating point environment to that of
    the given saved environment.

Arguments:

    Environment - Supplies the pointer to the environment to load into the
        execution state.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
fegetexceptflag (
    fexcept_t *Destination,
    int Mask
    );

/*++

Routine Description:

    This routine stores an implementation defined representation of the
    exception flags indicated by the given mask into the given destination.

Arguments:

    Destination - Supplies a pointer where the implementation-defined
        representation of the current flags masked with the given value.

    Mask - Supplies a mask of the exceptions the caller is interested in. See
        FE_* definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
fesetexceptflag (
    const fexcept_t *Source,
    int Mask
    );

/*++

Routine Description:

    This routine attempts to store an implementation-defined representation of
    the given floating point status flags into the current machine state. This
    function does not raise exceptions, it only sets the flags.

Arguments:

    Source - Supplies a pointer to the implementation-defined representation of
        the floating point status to set.

    Mask - Supplies a mask of the exceptions to operate on. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
feclearexcept (
    int Exceptions
    );

/*++

Routine Description:

    This routine attempts to clear the given floating point exceptions from the
    current machine state.

Arguments:

    Exceptions - Supplies a mask of the exceptions to clear. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
feraiseexcept (
    int Exceptions
    );

/*++

Routine Description:

    This routine attempts to raise the given supported floating point
    exceptions. The order in which these exceptions are raised is unspecified.

Arguments:

    Exceptions - Supplies a mask of the exceptions to raise. See FE_*
        definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
fetestexcept (
    int Exceptions
    );

/*++

Routine Description:

    This routine determines which of a specified subset of the floating-point
    exceptions are currently set.

Arguments:

    Exceptions - Supplies a mask of the exceptions to query. See FE_*
        definitions.

Return Value:

    Returns the bitmask of which of the specified exceptions are currently
    raised.

--*/

LIBC_API
int
fegetround (
    void
    );

/*++

Routine Description:

    This routine returns the current rounding direction of the floating point
    unit.

Arguments:

    None.

Return Value:

    Returns the current rounding mode on success. See FE_* definitions.

    Returns a negative number on failure.

--*/

LIBC_API
int
fesetround (
    int Mode
    );

/*++

Routine Description:

    This routine attempts to set the rounding mode of the floating point unit.

Arguments:

    Mode - Supplies the new mode to set. See FE_* definitions.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
feholdexcept (
    fenv_t *Environment
    );

/*++

Routine Description:

    This routine saves the current floating point environment, clears the
    status flags, and installs a non-stop (continue on floating-point
    exceptions) mode, if available, for all floating point exceptions.

Arguments:

    Environment - Supplies a pointer where the environment will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

LIBC_API
int
feupdateenv (
    fenv_t *Environment
    );

/*++

Routine Description:

    This routine saves the currently raised floating-point exceptions, loads
    the given floating-point environment, and then raises the saved floating
    point exceptions.

Arguments:

    Environment - Supplies a pointer to the environment to load.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

#endif

#ifdef __cplusplus

}

#endif
#endif

