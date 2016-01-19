/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

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

#include <usrinput.h>

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
