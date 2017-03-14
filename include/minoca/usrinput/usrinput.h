/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usrinput.h

Abstract:

    This header contains definitions for the User Input library.

Author:

    Evan Green 16-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef USER_INPUT_API

#define USER_INPUT_API __DLLIMPORT

#endif

//
// Define the name of the pipe where all user input is fed in to.
//

#define USER_INPUT_PIPE_NAME "/Pipe/UserInput"

//
// Define the current version of the user input keyboard device interface.
//

#define USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION 1

//
// Define the bitmask for a user input keyboard's LED state.
//

#define USER_INPUT_KEYBOARD_LED_NUM_LOCK    0x00000001
#define USER_INPUT_KEYBOARD_LED_CAPS_LOCK   0x00000002
#define USER_INPUT_KEYBOARD_LED_SCROLL_LOCK 0x00000004
#define USER_INPUT_KEYBOARD_LED_COMPOSE     0x00000008
#define USER_INPUT_KEYBOARD_LED_KANA        0x00000010

//
// Define the mouse event standard button flags.
//

#define MOUSE_BUTTON_LEFT                   0x00000001
#define MOUSE_BUTTON_RIGHT                  0x00000002
#define MOUSE_BUTTON_MIDDLE                 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USER_INPUT_DEVICE_TYPE {
    UserInputDeviceInvalid,
    UserInputDeviceKeyboard,
    UserInputDeviceMouse,
    UserInputDeviceTypeCount
} USER_INPUT_DEVICE_TYPE, *PUSER_INPUT_DEVICE_TYPE;

typedef enum _USER_INPUT_EVENT_TYPE {
    UserInputEventInvalid,
    UserInputEventKeyDown,
    UserInputEventKeyUp,
    UserInputEventMouse,
    UserInputEventCount
} USER_INPUT_EVENT_TYPE, *PUSER_INPUT_EVENT_TYPE;

typedef enum _KEYBOARD_KEY {
    KeyboardKeyInvalid,            /* 00 */
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
    KeyboardKeyQ,                  /* 10 */
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
    KeyboardKeyD,                  /* 20 */
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
    KeyboardKeyB,                  /* 30 */
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
    KeyboardKeyF6,                 /* 40 */
    KeyboardKeyF7,
    KeyboardKeyF8,
    KeyboardKeyF9,
    KeyboardKeyF10,
    KeyboardKeyF11,
    KeyboardKeyF12,
    KeyboardKeyF13,
    KeyboardKeyF14,
    KeyboardKeyF15,
    KeyboardKeyF16,
    KeyboardKeyF17,
    KeyboardKeyF18,
    KeyboardKeyF19,
    KeyboardKeyF20,
    KeyboardKeyF21,
    KeyboardKeyF22,                /* 50 */
    KeyboardKeyF23,
    KeyboardKeyF24,
    KeyboardKeyNumLock,
    KeyboardKeyScrollLock,
    KeyboardKeyKeypad0,
    KeyboardKeyKeypad1,
    KeyboardKeyKeypad2,
    KeyboardKeyKeypad3,
    KeyboardKeyKeypad4,
    KeyboardKeyKeypad5,
    KeyboardKeyKeypad6,
    KeyboardKeyKeypad7,
    KeyboardKeyKeypad8,
    KeyboardKeyKeypad9,
    KeyboardKeyKeypadMinus,
    KeyboardKeyKeypadPlus,         /* 60 */
    KeyboardKeyKeypadPeriod,
    KeyboardKeySysRq,
    KeyboardKeyInternational1,
    KeyboardKeyInternational2,
    KeyboardKeyInternational3,
    KeyboardKeyInternational4,
    KeyboardKeyInternational5,
    KeyboardKeyInternational6,
    KeyboardKeyInternational7,
    KeyboardKeyInternational8,
    KeyboardKeyInternational9,
    KeyboardKeyHangul,
    KeyboardKeyHanja,
    KeyboardKeyKatakana,
    KeyboardKeyFurigana,
    KeyboardKeyKanji,              /* 70 */
    KeyboardKeyHirijana,
    KeyboardKeyZenkaku,
    KeyboardKeyLanguage6,
    KeyboardKeyLanguage7,
    KeyboardKeyLanguage8,
    KeyboardKeyLanguage9,
    KeyboardKeyRedo,
    KeyboardKeyUndo,
    KeyboardKeyPaste,
    KeyboardKeySkipBack,
    KeyboardKeyCut,
    KeyboardKeyCopy,
    KeyboardKeySkipForward,
    KeyboardKeyKeypadEnter,
    KeyboardKeyRightControl,
    KeyboardKeyMail,               /* 80 */
    KeyboardKeyMute,
    KeyboardKeyPlay,
    KeyboardKeyStop,
    KeyboardKeyEject,
    KeyboardKeyVolumeDown,
    KeyboardKeyVolumeUp,
    KeyboardKeyWeb,
    KeyboardKeyRightAlt,
    KeyboardKeyHelp,
    KeyboardKeyMusic,
    KeyboardKeyHome,
    KeyboardKeyUp,
    KeyboardKeyPageUp,
    KeyboardKeyLeft,
    KeyboardKeyRight,
    KeyboardKeyEnd,                /* 90 */
    KeyboardKeyDown,
    KeyboardKeyPageDown,
    KeyboardKeyInsert,
    KeyboardKeyDelete,
    KeyboardKeyLeftWindows,
    KeyboardKeyRightWindows,
    KeyboardKeyMenu,
    KeyboardKeyPower,
    KeyboardKeySleep,
    KeyboardKeyWake,
    KeyboardKeyPictures,
    KeyboardKeyVideo,
    KeyboardKeyNonUsCurrency,
    KeyboardKeyBreak,
    KeyboardKeyKeypadSlash,
    KeyboardKeyNonUsBackslash,     /* A0 */
    KeyboardKeyApplication,
    KeyboardKeyKeypadEquals,
    KeyboardKeyExecute,
    KeyboardKeySelect,
    KeyboardKeyAgain,
    KeyboardKeyFind,
    KeyboardKeyKeypadComma,
    KeyboardKeyCancel,
    KeyboardKeyClear,
    KeyboardKeyPrior,
    KeyboardKeySeparator,
    KeyboardKeyOut,
    KeyboardKeyOperator,
    KeyboardKeyCrSel,
    KeyboardKeyExSel,
    KeyboardKeyKeypad00,           /* B0 */
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
    KeyboardKeyKeypadE,            /* C0 */
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
    KeyboardKeyKeypadMemoryStore,  /* D0 */
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
    KeyboardKeyPrintScreen,
    KeyboardKeyMax
} KEYBOARD_KEY, *PKEYBOARD_KEY;

typedef struct _USER_INPUT_DEVICE USER_INPUT_DEVICE, *PUSER_INPUT_DEVICE;

/*++

Structure Description:

    This structure describes a mouse movement event.

Members:

    MovementX - Stores the movement in the X direction.

    MovementY - Stores the movement in the Y direction.

    ScrollX - Stores the scroll wheel movement in the X direction.

    ScrollY - Stores the scroll whell movement in the Y direction.

    Flags - Stores additional flags.

    Buttons - Stores the button state.

--*/

typedef struct _MOUSE_EVENT {
    LONG MovementX;
    LONG MovementY;
    LONG ScrollX;
    LONG ScrollY;
    USHORT Flags;
    USHORT Buttons;
} MOUSE_EVENT, *PMOUSE_EVENT;

/*++

Structure Description:

    This structure describes a user input device event.

Members:

    EventIdentifier - Stores a unique event ID, assigned when the event is
        reported.

    DeviceIdentifier - Stores the unique identifier assigned to the device.

    DeviceType - Stores the type of device reporting the event.

    EventType - Stores the type of event occurring.

    Timestamp - Stores the time counter value when the event occurred.

    U - Stores the union of possible event data.

        Key - Stores the keyboard key being affected.

        Mouse - Stores the mouse event.

--*/

typedef struct _USER_INPUT_EVENT {
    ULONG EventIdentifier;
    ULONG DeviceIdentifier;
    USER_INPUT_DEVICE_TYPE DeviceType;
    USER_INPUT_EVENT_TYPE EventType;
    ULONGLONG Timestamp;
    union {
        KEYBOARD_KEY Key;
        MOUSE_EVENT Mouse;
    } U;

} USER_INPUT_EVENT, *PUSER_INPUT_EVENT;

typedef
KSTATUS
(*PUSER_INPUT_KEYBOARD_DEVICE_SET_LED_STATE) (
    PVOID Device,
    PVOID DeviceContext,
    ULONG LedState
    );

/*++

Routine Description:

    This routine sets a keyboard's LED state (e.g. Number lock, Caps lock and
    scroll lock). The state is absolute; the desired state for each LED must be
    supplied.

Arguments:

    Device - Supplies a pointer to the OS device representing the user input
        device.

    DeviceContext - Supplies the opaque device context supplied in the device
        description upon registration with the user input library.

    LedState - Supplies a bitmask of flags describing the desired LED state.
        See USER_INPUT_KEYBOARD_LED_* for definition.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines an interface for a keyboard device that allows the
    user input library to update keyboard state.

Members:

    SetLedState - Stores a pointer to a function used to set the keyboard's LED
        state.

--*/

typedef struct _USER_INPUT_KEYBOARD_DEVICE_INTERFACE {
    PUSER_INPUT_KEYBOARD_DEVICE_SET_LED_STATE SetLedState;
} USER_INPUT_KEYBOARD_DEVICE_INTERFACE, *PUSER_INPUT_KEYBOARD_DEVICE_INTERFACE;

/*++

Structure Description:

    This structure describes a user input device that is being registered with
    the user input library.

Members:

    Device - Stores a pointer to the OS device representing the user input
        device.

    DeviceContext - Stores an opaque token to device specific context.

    Type - Stores the type of user input device being described.

    InterfaceVersion - Stores the version of the device interface. For
        keyboards, set to USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION.

    KeyboardInterface - Stores the interface (function table) for a keyboard
        user input device.

--*/

typedef struct _USER_INPUT_DEVICE_DESCRIPTION {
    PVOID Device;
    PVOID DeviceContext;
    USER_INPUT_DEVICE_TYPE Type;
    ULONG InterfaceVersion;
    union {
        USER_INPUT_KEYBOARD_DEVICE_INTERFACE KeyboardInterface;
    } U;

} USER_INPUT_DEVICE_DESCRIPTION, *PUSER_INPUT_DEVICE_DESCRIPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

USER_INPUT_API
HANDLE
InRegisterInputDevice (
    PUSER_INPUT_DEVICE_DESCRIPTION Description
    );

/*++

Routine Description:

    This routine registers a new user input device.

Arguments:

    Description - Supplies a pointer to the description of the user input
        device being registered.

Return Value:

    Returns a handle to the user input device on success.

    INVALID_HANDLE on failure.

--*/

USER_INPUT_API
VOID
InDestroyInputDevice (
    HANDLE Handle
    );

/*++

Routine Description:

    This routine tears down state associated with a user input device created
    when the device was registered.

Arguments:

    Handle - Supplies the handle to the registered device. When this function
        returns, the handle will be invalid.

Return Value:

    None.

--*/

USER_INPUT_API
KSTATUS
InReportInputEvent (
    HANDLE Handle,
    PUSER_INPUT_EVENT Event
    );

/*++

Routine Description:

    This routine processes a new input event from the given device. This
    routine must be called at low level. The caller is expected to synchronize
    calls to report input for a device.

Arguments:

    Handle - Supplies the handle to the registered device reporting the event.

    Event - Supplies a pointer to the event that occurred. The caller must
        supply this buffer, but it will only be used for the duration of the
        routine (a copy will be made). The caller must fill out the event type
        and union, and should expect all other fields to be overwritten.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_FULL if the input pipe is full of events and this one was
        dropped.

--*/

