/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stextinx.h

Abstract:

    This header contains definitions for the UEFI Simple Text In Ex Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL_GUID              \
    {                                                       \
        0xDD9E7534, 0x7762, 0x4698,                         \
        {0x8C, 0x14, 0xF5, 0x85, 0x17, 0xA6, 0x25, 0xAA }   \
    }

//
// Any Shift or Toggle State that is valid should have
// high order bit set.
// Define shift states.
//

#define EFI_SHIFT_STATE_VALID     0x80000000
#define EFI_RIGHT_SHIFT_PRESSED   0x00000001
#define EFI_LEFT_SHIFT_PRESSED    0x00000002
#define EFI_RIGHT_CONTROL_PRESSED 0x00000004
#define EFI_LEFT_CONTROL_PRESSED  0x00000008
#define EFI_RIGHT_ALT_PRESSED     0x00000010
#define EFI_LEFT_ALT_PRESSED      0x00000020
#define EFI_RIGHT_LOGO_PRESSED    0x00000040
#define EFI_LEFT_LOGO_PRESSED     0x00000080
#define EFI_MENU_KEY_PRESSED      0x00000100
#define EFI_SYS_REQ_PRESSED       0x00000200

//
// Toggle state
//

#define EFI_TOGGLE_STATE_VALID    0x80
#define EFI_KEY_STATE_EXPOSED     0x40
#define EFI_SCROLL_LOCK_ACTIVE    0x01
#define EFI_NUM_LOCK_ACTIVE       0x02
#define EFI_CAPS_LOCK_ACTIVE      0x04

//
// EFI Scan codes
//

#define SCAN_F11                  0x0015
#define SCAN_F12                  0x0016
#define SCAN_PAUSE                0x0048
#define SCAN_F13                  0x0068
#define SCAN_F14                  0x0069
#define SCAN_F15                  0x006A
#define SCAN_F16                  0x006B
#define SCAN_F17                  0x006C
#define SCAN_F18                  0x006D
#define SCAN_F19                  0x006E
#define SCAN_F20                  0x006F
#define SCAN_F21                  0x0070
#define SCAN_F22                  0x0071
#define SCAN_F23                  0x0072
#define SCAN_F24                  0x0073
#define SCAN_MUTE                 0x007F
#define SCAN_VOLUME_UP            0x0080
#define SCAN_VOLUME_DOWN          0x0081
#define SCAN_BRIGHTNESS_UP        0x0100
#define SCAN_BRIGHTNESS_DOWN      0x0101
#define SCAN_SUSPEND              0x0102
#define SCAN_HIBERNATE            0x0103
#define SCAN_TOGGLE_DISPLAY       0x0104
#define SCAN_RECOVERY             0x0105
#define SCAN_EJECT                0x0106

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL;

//
// Define the EFI_KEY_TOGGLE_STATE type. Valid toggle states are:
// EFI_TOGGLE_STATE_VALID, EFI_SCROLL_LOCK_ACTIVE EFI_NUM_LOCK_ACTIVE, and
// EFI_CAPS_LOCK_ACTIVE.
//

typedef UINT8 EFI_KEY_TOGGLE_STATE;

typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_RESET_EX) (
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

/*++

Routine Description:

    This routine resets the input device hardware. As part of initialization
    process, the firmware/device will make a quick but reasonable attempt to
    verify that the device is functioning. If the ExtendedVerification flag is
    TRUE the firmware may take an extended amount of time to verify the device
    is operating on reset. Otherwise the reset operation is to occur as quickly
    as possible. The hardware verification process is not defined by this
    specification and is left up to the platform firmware or driver to
    implement.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating if the driver should
        perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    reset.

--*/

/*++

Structure Description:

    This structure defines the state of a keyboard key.

Members:

    KeyShiftState - Stores the state of the shift modifiers. The returned value
        is only valid if the high order bit has been set.

    KeyToggleState - Stores the current internal state of various toggled
        attributes. The returned value is only valid if the high order bit has
        been set.

--*/

typedef struct _EFI_KEY_STATE {
    UINT32 KeyShiftState;
    EFI_KEY_TOGGLE_STATE KeyToggleState;
} EFI_KEY_STATE;

/*++

Structure Description:

    This structure defines keyboard key data.

Members:

    Key - Stores the EFI scan code and unicode value returned from the input
        device.

    KeyState - Stores the current state of various toggled attributes as well
        as input modifier values.

--*/

typedef struct {
    EFI_INPUT_KEY Key;
    EFI_KEY_STATE KeyState;
} EFI_KEY_DATA;

typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_READ_KEY_EX) (
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    EFI_KEY_DATA *KeyData
    );

/*++

Routine Description:

    This routine reads the next keystroke from the input device. If there is no
    pending keystroke the function returns EFI_NOT_READY. If there is a pending
    keystroke, then KeyData.Key.ScanCode is the EFI scan code.
    The KeyData.Key.UnicodeChar is the actual printable character or is zero if
    the key does not represent a printable character (control key, function
    key, etc.). The KeyData.KeyState is shift state for the character
    reflected in KeyData.Key.UnicodeChar or KeyData.Key.ScanCode. When
    interpreting the data from this function, it should be noted that if a
    class of printable characters that are normally adjusted by shift
    modifiers (e.g. Shift Key + "f" key) would be presented solely as a
    KeyData.Key.UnicodeChar without the associated shift state. So in the
    previous example of a Shift Key + "f" key being pressed, the only pertinent
    data returned would be KeyData.Key.UnicodeChar with the value of "F".
    This of course would not typically be the case for non-printable characters
    such as the pressing of the Right Shift Key + F10 key since the
    corresponding returned data would be reflected both in the
    KeyData.KeyState.KeyShiftState and KeyData.Key.ScanCode values. UEFI
    drivers which implement the EFI_SIMPLE_TEXT_INPUT_EX protocol are required
    to return KeyData.Key and KeyData.KeyState values. These drivers must
    always return the most current state of KeyData.KeyState.KeyShiftState and
    KeyData.KeyState.KeyToggleState. It should also be noted that certain input
    devices may not be able to produce shift or toggle state information, and
    in those cases the high order bit in the respective Toggle and Shift state
    fields should not be active.

Arguments:

    This - Supplies a pointer to the protocol instance.

    KeyData - Supplies a pointer where the keystroke state data is returned on
        success.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if no keystroke data is available.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    read.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_SET_STATE) (
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    EFI_KEY_TOGGLE_STATE *KeyToggleState
    );

/*++

Routine Description:

    This routine adjusts the internal state of the input hardware.

Arguments:

    This - Supplies a pointer to the protocol instance.

    KeyToggleState - Supplies a pointer to the toggle state to set for the
        input device.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    set.

    EFI_UNSUPPORTED if the device does not support the ability to have its
    state set.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_KEY_NOTIFY_FUNCTION) (
    EFI_KEY_DATA *KeyData
    );

/*++

Routine Description:

    This routine implements the callback called when a registered keystroke
    sequence is entered.

Arguments:

    KeyData - Supplies a pointer to the typed key sequence.

Return Value:

    EFI Status code.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_REGISTER_KEYSTROKE_NOTIFY) (
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    EFI_KEY_DATA *KeyData,
    EFI_KEY_NOTIFY_FUNCTION KeyNotificationFunction,
    VOID **NotifyHandle
    );

/*++

Routine Description:

    This routine registers a function which will be called when a specified
    keystroke sequence is entered by the user.

Arguments:

    This - Supplies a pointer to the protocol instance.

    KeyData - Supplies a pointer to keystroke sequence to register for.

    KeyNotificationFunction - Supplies a pointer to the function to be called
        when the sequence occurs.

    NotifyHandle - Supplies a pointer where a handle will be returned
        identifying the connection between keystroke sequence and callback
        function.

Return Value:

    EFI_SUCCESS on success.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_UNREGISTER_KEYSTROKE_NOTIFY) (
    EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL *This,
    VOID *NotificationHandle
    );

/*++

Routine Description:

    This routine removes a previously registered keystroke handler.

Arguments:

    This - Supplies a pointer to the protocol instance.

    NotificationHandle - Supplies the handle returned when the keystroke was
        registered.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the notification handle is invalid.
--*/

/*++

Structure Description:

    This structure defines the UEFI Simple Text Input Ex Protocol. This is
    the protocol used on the ConsoleIn device. It is an extension to the
    Simple Text Input protocol which allows a variety of extended shift state
    information to be returned.

Members:

    Reset - Stores a pointer to a function used for resetting the input device.

    ReadKeyStrokeEx - Stores a pointer to a function used for reading keyboard
        input data.

    WaitForEventEx - Stores an event that can be waited on and will be signaled
        when key data is available.

    SetState - Stores a pointer to a function used to set the input controller
        state.

    RegisterKeyNotify - Stores a pointer to a function used to register for
        keystroke notifications.

    UnregisterKeyNotify - Stores a pointer to a function used to deregister
        a keyboard notification.

--*/

struct _EFI_SIMPLE_TEXT_INPUT_EX_PROTOCOL {
    EFI_INPUT_RESET_EX Reset;
    EFI_INPUT_READ_KEY_EX ReadKeyStrokeEx;
    EFI_EVENT WaitForKeyEx;
    EFI_SET_STATE SetState;
    EFI_REGISTER_KEYSTROKE_NOTIFY RegisterKeyNotify;
    EFI_UNREGISTER_KEYSTROKE_NOTIFY UnregisterKeyNotify;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

