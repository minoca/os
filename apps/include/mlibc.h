/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    mlibc.h

Abstract:

    This header contains definitions for implementation-specific functions
    provided by the Minoca C library.

Author:

    Evan Green 19-Nov-2013

--*/

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
INT
ClConvertKstatusToErrorNumber (
    KSTATUS Status
    );

/*++

Routine Description:

    This routine converts a status code to a C library error number.

Arguments:

    Status - Supplies the status code.

Return Value:

    Returns the appropriate error number for the given status code.

--*/

#ifdef __cplusplus

}

#endif

