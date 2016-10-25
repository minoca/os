/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    keycode.c

Abstract:

    This module contains tables that convert between USB HID key codes and OS
    key codes.

Author:

    Evan Green 20-Mar-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "usbkbd.h"

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

KEYBOARD_KEY UsbKbdControlKeys[BITS_PER_BYTE] = {
    KeyboardKeyLeftControl,
    KeyboardKeyLeftShift,
    KeyboardKeyLeftAlt,
    KeyboardKeyLeftWindows,
    KeyboardKeyRightControl,
    KeyboardKeyRightShift,
    KeyboardKeyRightAlt,
    KeyboardKeyRightWindows,
};

KEYBOARD_KEY UsbKbdKeys[USB_KEYCOARD_KEY_CODE_COUNT] = {
    KeyboardKeyInvalid,           /* 00 */
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyA,
    KeyboardKeyB,
    KeyboardKeyC,
    KeyboardKeyD,
    KeyboardKeyE,
    KeyboardKeyF,
    KeyboardKeyG,
    KeyboardKeyH,
    KeyboardKeyI,
    KeyboardKeyJ,
    KeyboardKeyK,
    KeyboardKeyL,
    KeyboardKeyM,                 /* 10 */
    KeyboardKeyN,
    KeyboardKeyO,
    KeyboardKeyP,
    KeyboardKeyQ,
    KeyboardKeyR,
    KeyboardKeyS,
    KeyboardKeyT,
    KeyboardKeyU,
    KeyboardKeyV,
    KeyboardKeyW,
    KeyboardKeyX,
    KeyboardKeyY,
    KeyboardKeyZ,
    KeyboardKey1,
    KeyboardKey2,
    KeyboardKey3,                 /* 20 */
    KeyboardKey4,
    KeyboardKey5,
    KeyboardKey6,
    KeyboardKey7,
    KeyboardKey8,
    KeyboardKey9,
    KeyboardKey0,
    KeyboardKeyEnter,
    KeyboardKeyEscape,
    KeyboardKeyBackspace,
    KeyboardKeyTab,
    KeyboardKeySpace,
    KeyboardKeyDash,
    KeyboardKeyEquals,
    KeyboardKeyLeftBracket,
    KeyboardKeyRightBracket,      /* 30 */
    KeyboardKeyBackslash,
    KeyboardKeyNonUsCurrency,
    KeyboardKeySemicolon,
    KeyboardKeyApostrophe,
    KeyboardKeyTilde,
    KeyboardKeyComma,
    KeyboardKeyPeriod,
    KeyboardKeySlash,
    KeyboardKeyCapsLock,
    KeyboardKeyF1,
    KeyboardKeyF2,
    KeyboardKeyF3,
    KeyboardKeyF4,
    KeyboardKeyF5,
    KeyboardKeyF6,
    KeyboardKeyF7,                /* 40 */
    KeyboardKeyF8,
    KeyboardKeyF9,
    KeyboardKeyF10,
    KeyboardKeyF11,
    KeyboardKeyF12,
    KeyboardKeyPrintScreen,
    KeyboardKeyScrollLock,
    KeyboardKeyBreak,
    KeyboardKeyInsert,
    KeyboardKeyHome,
    KeyboardKeyPageUp,
    KeyboardKeyDelete,
    KeyboardKeyEnd,
    KeyboardKeyPageDown,
    KeyboardKeyRight,
    KeyboardKeyLeft,              /* 50 */
    KeyboardKeyDown,
    KeyboardKeyUp,
    KeyboardKeyNumLock,
    KeyboardKeyKeypadSlash,
    KeyboardKeyKeypadAsterisk,
    KeyboardKeyKeypadMinus,
    KeyboardKeyKeypadPlus,
    KeyboardKeyKeypadEnter,
    KeyboardKeyKeypad1,
    KeyboardKeyKeypad2,
    KeyboardKeyKeypad3,
    KeyboardKeyKeypad4,
    KeyboardKeyKeypad5,
    KeyboardKeyKeypad6,
    KeyboardKeyKeypad7,
    KeyboardKeyKeypad8,           /* 60 */
    KeyboardKeyKeypad9,
    KeyboardKeyKeypad0,
    KeyboardKeyKeypadPeriod,
    KeyboardKeyNonUsBackslash,
    KeyboardKeyApplication,
    KeyboardKeyPower,
    KeyboardKeyKeypadEquals,
    KeyboardKeyF13,
    KeyboardKeyF14,
    KeyboardKeyF15,
    KeyboardKeyF16,
    KeyboardKeyF17,
    KeyboardKeyF18,
    KeyboardKeyF19,
    KeyboardKeyF20,
    KeyboardKeyF21,               /* 70 */
    KeyboardKeyF22,
    KeyboardKeyF23,
    KeyboardKeyF24,
    KeyboardKeyExecute,
    KeyboardKeyHelp,
    KeyboardKeyMenu,
    KeyboardKeySelect,
    KeyboardKeyStop,
    KeyboardKeyAgain,
    KeyboardKeyUndo,
    KeyboardKeyCut,
    KeyboardKeyCopy,
    KeyboardKeyPaste,
    KeyboardKeyFind,
    KeyboardKeyMute,
    KeyboardKeyVolumeUp,          /* 80 */
    KeyboardKeyVolumeDown,
    KeyboardKeyCapsLock,
    KeyboardKeyNumLock,
    KeyboardKeyScrollLock,
    KeyboardKeyKeypadComma,
    KeyboardKeyKeypadEquals,
    KeyboardKeyInternational1,
    KeyboardKeyInternational2,
    KeyboardKeyInternational3,
    KeyboardKeyInternational4,
    KeyboardKeyInternational5,
    KeyboardKeyInternational6,
    KeyboardKeyInternational7,
    KeyboardKeyInternational8,
    KeyboardKeyInternational9,
    KeyboardKeyHangul,            /* 90 */
    KeyboardKeyHanja,
    KeyboardKeyKatakana,
    KeyboardKeyHirijana,
    KeyboardKeyZenkaku,
    KeyboardKeyLanguage6,
    KeyboardKeyLanguage7,
    KeyboardKeyLanguage8,
    KeyboardKeyLanguage9,
    KeyboardKeyBackspace,
    KeyboardKeySysRq,
    KeyboardKeyCancel,
    KeyboardKeyClear,
    KeyboardKeyPrior,
    KeyboardKeyEnter,
    KeyboardKeySeparator,
    KeyboardKeyOut,               /* A0 */
    KeyboardKeyOperator,
    KeyboardKeyClear,
    KeyboardKeyCrSel,
    KeyboardKeyExSel,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyKeypad00,          /* B0 */
    KeyboardKeyKeypad000,
    KeyboardKeyThousandsSeparator,
    KeyboardKeyDecimalSeparator,
    KeyboardKeyCurrencyUnit,
    KeyboardKeyCurrencySubunit,
    KeyboardKeyKeypadOpenParentheses,
    KeyboardKeyKeypadCloseParentheses,
    KeyboardKeyKeypadOpenCurlyBrace,
    KeyboardKeyKeypadCloseCurlyBrace,
    KeyboardKeyKeypadTab,
    KeyboardKeyKeypadBackspace,
    KeyboardKeyKeypadA,
    KeyboardKeyKeypadB,
    KeyboardKeyKeypadC,
    KeyboardKeyKeypadD,
    KeyboardKeyKeypadE,           /* C0 */
    KeyboardKeyKeypadF,
    KeyboardKeyKeypadXor,
    KeyboardKeyKeypadCaret,
    KeyboardKeyKeypadPercent,
    KeyboardKeyKeypadLessThan,
    KeyboardKeyKeypadGreaterThan,
    KeyboardKeyKeypadAmpersand,
    KeyboardKeyKeypadDoubleAmpersand,
    KeyboardKeyKeypadPipe,
    KeyboardKeyKeypadDoublePipe,
    KeyboardKeyKeypadColon,
    KeyboardKeyKeypadHash,
    KeyboardKeyKeypadSpace,
    KeyboardKeyKeypadAt,
    KeyboardKeyKeypadExclamationPoint,
    KeyboardKeyKeypadMemoryStore, /* D0 */
    KeyboardKeyKeypadMemoryRecall,
    KeyboardKeyKeypadMemoryClear,
    KeyboardKeyKeypadMemoryAdd,
    KeyboardKeyKeypadMemorySubtract,
    KeyboardKeyKeypadMemoryMultiply,
    KeyboardKeyKeypadMemoryDivide,
    KeyboardKeyKeypadPlusMinus,
    KeyboardKeyKeypadClear,
    KeyboardKeyKeypadClearEntry,
    KeyboardKeyKeypadBinary,
    KeyboardKeyKeypadOctal,
    KeyboardKeyKeypadDecimal,
    KeyboardKeyKeypadHexadecimal,
    KeyboardKeyInvalid,
    KeyboardKeyInvalid,
    KeyboardKeyLeftControl,       /* E0 */
    KeyboardKeyLeftShift,
    KeyboardKeyLeftAlt,
    KeyboardKeyLeftWindows,
    KeyboardKeyRightControl,
    KeyboardKeyRightShift,
    KeyboardKeyRightAlt,
    KeyboardKeyRightWindows,
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

