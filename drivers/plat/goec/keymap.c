/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    keymap.c

Abstract:

    This module contains the keymap for the Google Embedded Controller Matrix
    Keyboard.

Author:

    Evan Green 26-Aug-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "goec.h"

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

KEYBOARD_KEY GoecKeyMap[GOEC_MAX_COLUMNS][BITS_PER_BYTE] = {
    {
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
        KeyboardKeyLeftControl,
        KeyboardKeyInvalid,
        KeyboardKeyRightControl,
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
    },
    {
        KeyboardKeyLeftWindows, // Meta
        KeyboardKeyEscape,
        KeyboardKeyTab,
        KeyboardKeyTilde,
        KeyboardKeyA,
        KeyboardKeyZ,
        KeyboardKey1,
        KeyboardKeyQ,
    },
    {
        KeyboardKeyF1,
        KeyboardKeyF4,
        KeyboardKeyF3,
        KeyboardKeyF2,
        KeyboardKeyD,
        KeyboardKeyC,
        KeyboardKey3,
        KeyboardKeyE,
    },
    {
        KeyboardKeyB,
        KeyboardKeyG,
        KeyboardKeyT,
        KeyboardKey5,
        KeyboardKeyF,
        KeyboardKeyV,
        KeyboardKey4,
        KeyboardKeyR,
    },
    {
        KeyboardKeyF10,
        KeyboardKeyF7,
        KeyboardKeyF6,
        KeyboardKeyF5,
        KeyboardKeyS,
        KeyboardKeyX,
        KeyboardKey2,
        KeyboardKeyW,
    },
    {
        KeyboardKeyInvalid, // RO
        KeyboardKeyInvalid,
        KeyboardKeyRightBracket,
        KeyboardKeyInvalid,
        KeyboardKeyK,
        KeyboardKeyComma,
        KeyboardKey8,
        KeyboardKeyI,
    },
    {
        KeyboardKeyN,
        KeyboardKeyH,
        KeyboardKeyY,
        KeyboardKey6,
        KeyboardKeyJ,
        KeyboardKeyM,
        KeyboardKey7,
        KeyboardKeyU,
    },
    {
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
        KeyboardKeyBackslash, // 102nd
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
        KeyboardKeyLeftShift,
        KeyboardKeyInvalid,
        KeyboardKeyRightShift,
    },
    {
        KeyboardKeyEquals,
        KeyboardKeyApostrophe,
        KeyboardKeyLeftBracket,
        KeyboardKeyDash,
        KeyboardKeySemicolon,
        KeyboardKeySlash,
        KeyboardKey0,
        KeyboardKeyP,
    },
    {
        KeyboardKeyInvalid,
        KeyboardKeyF9,
        KeyboardKeyF8,
        KeyboardKeyF13,
        KeyboardKeyL,
        KeyboardKeyPeriod,
        KeyboardKey9,
        KeyboardKeyO,
    },
    {
        KeyboardKeyRightAlt,
        KeyboardKeyInvalid,
        KeyboardKeyInvalid, // Yen
        KeyboardKeyInvalid,
        KeyboardKeyBackslash,
        KeyboardKeyInvalid,
        KeyboardKeyLeftAlt,
        KeyboardKeyInvalid,
    },
    {
        KeyboardKeyInvalid,
        KeyboardKeyBackspace,
        KeyboardKeyInvalid,
        KeyboardKeyBackslash,
        KeyboardKeyEnter,
        KeyboardKeySpace,
        KeyboardKeyDown,
        KeyboardKeyUp,
    },
    {
        KeyboardKeyInvalid,
        KeyboardKeyInvalid, // Henkan
        KeyboardKeyInvalid,
        KeyboardKeyInvalid, // Muhenkan
        KeyboardKeyInvalid,
        KeyboardKeyInvalid,
        KeyboardKeyRight,
        KeyboardKeyLeft,
    },
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

