/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    spnwin32.h

Abstract:

    This header contains spawn module definitions for Windows.

Author:

    Evan Green 21-Jun-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define SIGKILL 9
#define SIGTERM 15

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

int
pipe (
    int Descriptors[2]
    );

/*++

Routine Description:

    This routine creates a new pipe.

Arguments:

    Descriptors - Supplies an array where the read and write descriptors for
        the pipe will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

