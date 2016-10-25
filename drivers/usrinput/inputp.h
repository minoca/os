/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    inputp.h

Abstract:

    This header contains internal definitions for the user input driver.

Author:

    Evan Green 13-Mar-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Redefine the API define into an export.
//

#define USER_INPUT_API __DLLEXPORT

#include <minoca/kernel/driver.h>
#include <minoca/usrinput/usrinput.h>

//
// ---------------------------------------------------------------- Definitions
//

#define ANSI_ESCAPE_CODE 0x1B

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern UCHAR InKeyboardCharacters[KeyboardKeyMax];
extern UCHAR InShiftedKeyboardCharacters[KeyboardKeyMax];

//
// -------------------------------------------------------- Function Prototypes
//
