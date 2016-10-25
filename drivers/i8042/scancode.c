/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    scancode.c

Abstract:

    This module impelements support for converting between scancodes used
    in keyboards to OS key abstractions.

Author:

    Evan Green 21-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "i8042.h"

//
// ---------------------------------------------------------------- Definitions
//

#define MAX_SCAN_CODE_1 0x7F
#define SCAN_CODE_1_KEY_UP 0x80
#define SCAN_CODE_1_EXTENDED_KEY_COUNT 41
#define SCAN_CODE_1_EXTENDED_2_KEY_COUNT 1

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EXTENDED_KEY_DESCRIPTION {
    UCHAR ScanCode;
    KEYBOARD_KEY Key;
} EXTENDED_KEY_DESCRIPTION, *PEXTENDED_KEY_DESCRIPTION;

typedef struct _EXTENDED_2_KEY_DESCRIPTION {
    UCHAR ScanCode1;
    UCHAR ScanCode2;
    KEYBOARD_KEY Key;
} EXTENDED_2_KEY_DESCRIPTION, *PEXTENDED_2_KEY_DESCRIPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

KEYBOARD_KEY I8042ScanCodeSet1KeyTable[MAX_SCAN_CODE_1] = {
    KeyboardKeyInvalid, /* 00 */
    KeyboardKeyEscape,
    KeyboardKey1,
    KeyboardKey2,
    KeyboardKey3,
    KeyboardKey4,
    KeyboardKey5,
    KeyboardKey6,
    KeyboardKey7,
    KeyboardKey8,
    KeyboardKey9,
    KeyboardKey0,
    KeyboardKeyDash,
    KeyboardKeyEquals,
    KeyboardKeyBackspace,
    KeyboardKeyTab,
    KeyboardKeyQ,       /* 10 */
    KeyboardKeyW,
    KeyboardKeyE,
    KeyboardKeyR,
    KeyboardKeyT,
    KeyboardKeyY,
    KeyboardKeyU,
    KeyboardKeyI,
    KeyboardKeyO,
    KeyboardKeyP,
    KeyboardKeyLeftBracket,
    KeyboardKeyRightBracket,
    KeyboardKeyEnter,
    KeyboardKeyLeftControl,
    KeyboardKeyA,
    KeyboardKeyS,
    KeyboardKeyD,       /* 20 */
    KeyboardKeyF,
    KeyboardKeyG,
    KeyboardKeyH,
    KeyboardKeyJ,
    KeyboardKeyK,
    KeyboardKeyL,
    KeyboardKeySemicolon,
    KeyboardKeyApostrophe,
    KeyboardKeyTilde,
    KeyboardKeyLeftShift,
    KeyboardKeyBackslash,
    KeyboardKeyZ,
    KeyboardKeyX,
    KeyboardKeyC,
    KeyboardKeyV,
    KeyboardKeyB,       /* 30 */
    KeyboardKeyN,
    KeyboardKeyM,
    KeyboardKeyComma,
    KeyboardKeyPeriod,
    KeyboardKeySlash,
    KeyboardKeyRightShift,
    KeyboardKeyKeypadAsterisk,
    KeyboardKeyLeftAlt,
    KeyboardKeySpace,
    KeyboardKeyCapsLock,
    KeyboardKeyF1,
    KeyboardKeyF2,
    KeyboardKeyF3,
    KeyboardKeyF4,
    KeyboardKeyF5,
    KeyboardKeyF6,      /* 40 */
    KeyboardKeyF7,
    KeyboardKeyF8,
    KeyboardKeyF9,
    KeyboardKeyF10,
    KeyboardKeyNumLock,
    KeyboardKeyScrollLock,
    KeyboardKeyKeypad7,
    KeyboardKeyKeypad8,
    KeyboardKeyKeypad9,
    KeyboardKeyKeypadMinus,
    KeyboardKeyKeypad4,
    KeyboardKeyKeypad5,
    KeyboardKeyKeypad6,
    KeyboardKeyKeypadPlus,
    KeyboardKeyKeypad1,
    KeyboardKeyKeypad2, /* 50 */
    KeyboardKeyKeypad3,
    KeyboardKeyKeypad0,
    KeyboardKeyKeypadPeriod,
    KeyboardKeySysRq,
    KeyboardKeyInvalid,
    KeyboardKeyInternational1,
    KeyboardKeyF11,
    KeyboardKeyF12,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyF13,
    KeyboardKeyF14,
    KeyboardKeyF15,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid, /* 60 */
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyF16,
    KeyboardKeyF17,
    KeyboardKeyF18,
    KeyboardKeyF19,
    KeyboardKeyF20,
    KeyboardKeyF21,
    KeyboardKeyF22,
    KeyboardKeyF23,
    KeyboardKeyF24,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyKatakana, /* 70 */
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInternational3,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyFurigana,
    KeyboardKeyInvalid,
    KeyboardKeyKanji,
    KeyboardKeyInvalid,
    KeyboardKeyHirijana,
    KeyboardKeyInvalid,
    KeyboardKeyInternational4,
    KeyboardKeyInternational5,
};

EXTENDED_KEY_DESCRIPTION
          I8042ScanCodeSet1ExtendedKeyTable[SCAN_CODE_1_EXTENDED_KEY_COUNT] = {

    {0x07, KeyboardKeyRedo},
    {0x08, KeyboardKeyUndo},
    {0x0A, KeyboardKeyPaste},
    {0x10, KeyboardKeySkipBack},
    {0x17, KeyboardKeyCut},
    {0x18, KeyboardKeyCopy},
    {0x19, KeyboardKeySkipForward},
    {0x1C, KeyboardKeyKeypadEnter},
    {0x1D, KeyboardKeyRightControl},
    {0x1E, KeyboardKeyMail},
    {0x20, KeyboardKeyMute},
    {0x22, KeyboardKeyPlay},
    {0x24, KeyboardKeyStop},
    {0x2C, KeyboardKeyEject},
    {0x2E, KeyboardKeyVolumeDown},
    {0x30, KeyboardKeyVolumeUp},
    {0x32, KeyboardKeyWeb},
    {0x35, KeyboardKeyKeypadSlash},
    {0x37, KeyboardKeyPrintScreen},
    {0x38, KeyboardKeyRightAlt},
    {0x3B, KeyboardKeyHelp},
    {0x3C, KeyboardKeyMusic},
    {0x46, KeyboardKeyBreak},
    {0x47, KeyboardKeyHome},
    {0x48, KeyboardKeyUp},
    {0x49, KeyboardKeyPageUp},
    {0x4B, KeyboardKeyLeft},
    {0x4D, KeyboardKeyRight},
    {0x4F, KeyboardKeyEnd},
    {0x50, KeyboardKeyDown},
    {0x51, KeyboardKeyPageDown},
    {0x52, KeyboardKeyInsert},
    {0x53, KeyboardKeyDelete},
    {0x5B, KeyboardKeyLeftWindows},
    {0x5C, KeyboardKeyRightWindows},
    {0x5D, KeyboardKeyMenu},
    {0x5E, KeyboardKeyPower},
    {0x5F, KeyboardKeySleep},
    {0x63, KeyboardKeyWake},
    {0x64, KeyboardKeyPictures},
    {0x6D, KeyboardKeyVideo},
};

EXTENDED_2_KEY_DESCRIPTION
       I8042ScanCodeSet1Extended2KeyTable[SCAN_CODE_1_EXTENDED_2_KEY_COUNT] = {

    {0x1D, 0x45, KeyboardKeyBreak},
};

//
// ------------------------------------------------------------------ Functions
//

KEYBOARD_KEY
I8042ConvertScanCodeToKey (
    UCHAR ScanCode1,
    UCHAR ScanCode2,
    UCHAR ScanCode3,
    PBOOL KeyUp
    )

/*++

Routine Description:

    This routine converts a scan code sequence into a key.

Arguments:

    ScanCode1 - Supplies the first scan code.

    ScanCode2 - Supplies an optional second scan code.

    ScanCode3 - Supplies an optional third scan code.

    KeyUp - Supplies a pointer where a boolean will be returned indicating
        whether the key is being released (TRUE) or pressed (FALSE).

Return Value:

    Returns the keyboard key code associated with this scan code sequence.

--*/

{

    ULONG ExtendedIndex;
    KEYBOARD_KEY Key;

    *KeyUp = FALSE;
    if (ScanCode1 == SCAN_CODE_1_EXTENDED_2_CODE) {
        if ((ScanCode2 & SCAN_CODE_1_KEY_UP) != 0) {
            *KeyUp = TRUE;
            ScanCode2 &= ~SCAN_CODE_1_KEY_UP;

            ASSERT((ScanCode3 & SCAN_CODE_1_KEY_UP) != 0);

            ScanCode3 &= ~ SCAN_CODE_1_KEY_UP;
        }

        for (ExtendedIndex = 0;
             ExtendedIndex < SCAN_CODE_1_EXTENDED_2_KEY_COUNT;
             ExtendedIndex += 1) {

            if ((I8042ScanCodeSet1Extended2KeyTable[ExtendedIndex].ScanCode1 ==
                 ScanCode2) &&
                (I8042ScanCodeSet1Extended2KeyTable[ExtendedIndex].ScanCode2 ==
                 ScanCode3)) {

                break;
            }
        }

        if (ExtendedIndex == SCAN_CODE_1_EXTENDED_2_KEY_COUNT) {
            Key = KeyboardKeyInvalid;

        } else {
            Key = I8042ScanCodeSet1Extended2KeyTable[ExtendedIndex].Key;
        }

    } else if (ScanCode1 == SCAN_CODE_1_EXTENDED_CODE) {
        if ((ScanCode2 & SCAN_CODE_1_KEY_UP) != 0) {
            *KeyUp = TRUE;
            ScanCode2 &= ~SCAN_CODE_1_KEY_UP;
        }

        for (ExtendedIndex = 0;
             ExtendedIndex < SCAN_CODE_1_EXTENDED_KEY_COUNT;
             ExtendedIndex += 1) {

            if (I8042ScanCodeSet1ExtendedKeyTable[ExtendedIndex].ScanCode ==
                ScanCode2) {

                break;
            }
        }

        if (ExtendedIndex == SCAN_CODE_1_EXTENDED_KEY_COUNT) {
            Key = KeyboardKeyInvalid;

        } else {
            Key = I8042ScanCodeSet1ExtendedKeyTable[ExtendedIndex].Key;
        }

    } else {
        if ((ScanCode1 & SCAN_CODE_1_KEY_UP) != 0) {
            *KeyUp = TRUE;
            ScanCode1 &= ~SCAN_CODE_1_KEY_UP;
        }

        Key = I8042ScanCodeSet1KeyTable[ScanCode1];
    }

    return Key;
}

//
// --------------------------------------------------------- Internal Functions
//

