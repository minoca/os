/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    goec.h

Abstract:

    This header contains internal definitions for the Google Embedded
    Controller driver.

Author:

    Evan Green 26-Aug-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/usrinput/usrinput.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of keyboard columns to support.
//

#define GOEC_MAX_COLUMNS 13

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the translation between OS keyboard keys and the matrix keyboard
// row/column.
//

extern KEYBOARD_KEY GoecKeyMap[GOEC_MAX_COLUMNS][BITS_PER_BYTE];

//
// -------------------------------------------------------- Function Prototypes
//
