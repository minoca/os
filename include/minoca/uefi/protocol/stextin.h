/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stextin.h

Abstract:

    This header contains definitions for the UEFI Simple Text Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID                 \
    {                                                       \
        0x387477C1, 0x69C7, 0x11D2,                         \
        {0x8E, 0x39, 0x0, 0xA0, 0xC9, 0x69, 0x72, 0x3B}     \
    }

//
// Protocol GUID name defined in EFI1.1.
//

#define SIMPLE_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID

//
// Required unicode control chars
//

#define CHAR_NULL            0x0000
#define CHAR_BACKSPACE       0x0008
#define CHAR_TAB             0x0009
#define CHAR_LINEFEED        0x000A
#define CHAR_CARRIAGE_RETURN 0x000D

//
// EFI Scan codes
//

#define SCAN_NULL      0x0000
#define SCAN_UP        0x0001
#define SCAN_DOWN      0x0002
#define SCAN_RIGHT     0x0003
#define SCAN_LEFT      0x0004
#define SCAN_HOME      0x0005
#define SCAN_END       0x0006
#define SCAN_INSERT    0x0007
#define SCAN_DELETE    0x0008
#define SCAN_PAGE_UP   0x0009
#define SCAN_PAGE_DOWN 0x000A
#define SCAN_F1        0x000B
#define SCAN_F2        0x000C
#define SCAN_F3        0x000D
#define SCAN_F4        0x000E
#define SCAN_F5        0x000F
#define SCAN_F6        0x0010
#define SCAN_F7        0x0011
#define SCAN_F8        0x0012
#define SCAN_F9        0x0013
#define SCAN_F10       0x0014
#define SCAN_ESC       0x0017

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL EFI_SIMPLE_TEXT_INPUT_PROTOCOL;

//
// Protocol name in EFI1.1 for backwards compatibility.
//

typedef struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL SIMPLE_INPUT_INTERFACE;

/*++

Structure Description:

    This structure defines the keystroke information for a pressed key.

Members:

    ScanCode - Stores the scan code of the key.

    UnicodeChar - Stores the Unicode character equivalent of the key.

--*/

typedef struct {
    UINT16 ScanCode;
    CHAR16 UnicodeChar;
} EFI_INPUT_KEY;

typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_RESET) (
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    BOOLEAN ExtendedVerification
    );

/*++

Routine Description:

    This routine resets the input device and optionally runs diagnostics.

Arguments:

    This - Supplies a pointer to the protocol instance.

    ExtendedVerification - Supplies a boolean indicating if the driver should
        perform diagnostics on reset.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    reset.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_INPUT_READ_KEY) (
    EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This,
    EFI_INPUT_KEY *Key
    );

/*++

Routine Description:

    This routine reads the next keystroke from the input device. The WaitForKey
    event can be used to test for the existence of a keystroke via the
    WaitForEvent call.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Key - Supplies a pointer where the keystroke information for the pressed
        key will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if there was no keystroke data available.

    EFI_DEVICE_ERROR if the device is not functioning properly and could not be
    read.

--*/

/*++

Structure Description:

    This structure defines the simple text protocol used on the ConsoleIn
    device. This is the minimum required protocol for console input.

Members:

    Reset - Stores a pointer to the reset device function.

    ReadKeyStroke - Stores a pointer to a function used to read input key
        information.

    WaitForKey - Stores the event that can be waited on to wait for a key to
        be available.

--*/

struct _EFI_SIMPLE_TEXT_INPUT_PROTOCOL {
    EFI_INPUT_RESET Reset;
    EFI_INPUT_READ_KEY ReadKeyStroke;
    EFI_EVENT WaitForKey;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
