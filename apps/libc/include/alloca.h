/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    alloca.h

Abstract:

    This header contains definitions for the automatic allocation routine.

Author:

    Evan Green 2-Aug-2013

--*/

#ifndef _ALLOCA_H
#define _ALLOCA_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <stddef.h>

//
// --------------------------------------------------------------------- Macros
//

#define alloca(_Size) __builtin_alloca(_Size)

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
void *
_alloca (
    int Size
    );

/*++

Routine Description:

    This routine allocates temporary space on the stack. This space will be
    freed automatically when the function returns.

Arguments:

    Size - Supplies the number of bytes to allocate.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

