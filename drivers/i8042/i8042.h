/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    i8042.h

Abstract:

    This header contains definitions for the 8042 keyboard controller support.

Author:

    Evan Green 21-Dec-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/usrinput/usrinput.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define commands that can be sent to the 8042 keyboard controller (on the
// Control port).
//

#define I8042_COMMAND_READ_COMMAND_BYTE  0x20
#define I8042_COMMAND_WRITE_COMMAND_BYTE 0x60
#define I8042_COMMAND_DISABLE_MOUSE_PORT 0xA7
#define I8042_COMMAND_ENABLE_MOUSE_PORT  0xA8
#define I8042_COMMAND_TEST_MOUSE_PORT    0xA9
#define I8042_COMMAND_WRITE_TO_MOUSE     0xD4
#define I8042_COMMAND_SELF_TEST          0xAA
#define I8042_COMMAND_INTERFACE_TEST     0xAB
#define I8042_COMMAND_DISABLE_KEYBOARD   0xAD
#define I8042_COMMAND_ENABLE_KEYBOARD    0xAE
#define I8042_COMMAND_READ_INPUT_PORT    0xC0
#define I8042_COMMAND_READ_OUTPUT_PORT   0xD0
#define I8042_COMMAND_WRITE_OUTPUT_PORT  0xD1
#define I8042_COMMAND_READ_TEST_INPUTS   0xE0
#define I8042_COMMAND_RESET              0xFE

//
// Define the amount of time to wait for a command in milliseconds.
//

#define I8042_COMMAND_TIMEOUT 250
#define I8042_SELF_TEST_SUCCESS 0x55
#define I8042_PORT_TEST_SUCCESS 0x00
#define I8042_PORT_TEST_CLOCK_STUCK_LOW 0x01
#define I8042_PORT_TEST_CLOCK_STUCK_HIGH 0x02
#define I8042_PORT_TEST_DATA_STUCK_LOW 0x03
#define I8042_PORT_TEST_DATA_STUCK_HIGH 0x04

//
// Define commands that can be sent to the keyboard.
//

#define KEYBOARD_COMMAND_SET_LEDS 0xED
#define KEYBOARD_COMMAND_ECHO 0xEE
#define KEYBOARD_COMMAND_GET_SET_SCAN_SET 0xF0
#define KEYBOARD_COMMAND_IDENTIFY 0xF2
#define KEYBOARD_COMMAND_SET_TYPEMATIC 0xF3
#define KEYBOARD_COMMAND_ENABLE 0xF4
#define KEYBOARD_COMMAND_RESET_AND_DISABLE 0xF5
#define KEYBOARD_COMMAND_SET_DEFAULTS 0xF6
#define KEYBOARD_COMMAND_RESEND 0xFE
#define KEYBOARD_COMMAND_RESET 0xFF

//
// Define the parameter value that indicates "no parameter".
//

#define KEYBOARD_COMMAND_NO_PARAMETER 0xFF
#define MOUSE_COMMAND_NO_PARAMETER 0xFF

//
// Define commands that can be sent to the mouse.
//

#define MOUSE_COMMAND_SET_1_1_SCALING 0xE6
#define MOUSE_COMMAND_SET_2_1_SCALING 0xE7
#define MOUSE_COMMAND_SET_RESOLUTION 0xE8
#define MOUSE_COMMAND_GET_STATUS 0xE9
#define MOUSE_COMMAND_REQUEST_PACKET 0xEB
#define MOUSE_COMMAND_GET_MOUSE_ID 0xF2
#define MOUSE_COMMAND_SET_SAMPLE_RATE 0xF3
#define MOUSE_COMMAND_ENABLE 0xF4
#define MOUSE_COMMAND_DISABLE 0xF5
#define MOUSE_COMMAND_SET_DEFAULTS 0xF6
#define MOUSE_COMMAND_RESEND 0xFE
#define MOUSE_COMMAND_RESET 0xFF

//
// Define mouse return codes.
//

#define MOUSE_STATUS_ACKNOWLEDGE 0xFA

//
// Define typematic rate and delay values. Rates are defined with decimal
// places, so 26_7 means 26.7 reports per second.
//

#define TYPEMATIC_DELAY_250MS  (0 << 5)
#define TYPEMATIC_DELAY_500MS  (1 << 5)
#define TYPEMATIC_DELAY_750MS  (2 << 5)
#define TYPEMATIC_DELAY_1000MS (3 << 5)
#define TYPEMATIC_RATE_30_0 0
#define TYPEMATIC_RATE_26_7 1
#define TYPEMATIC_RATE_24_0 2
#define TYPEMATIC_RATE_21_8 3
#define TYPEMATIC_RATE_20_0 4
#define TYPEMATIC_RATE_18_5 5
#define TYPEMATIC_RATE_17_1 6
#define TYPEMATIC_RATE_16_0 7
#define TYPEMATIC_RATE_15_0 8
#define TYPEMATIC_RATE_13_3 9
#define TYPEMATIC_RATE_12_0 10
#define TYPEMATIC_RATE_10_9 11
#define TYPEMATIC_RATE_10_0 12
#define TYPEMATIC_RATE_9_2 13
#define TYPEMATIC_RATE_8_6 14
#define TYPEMATIC_RATE_8_0 15
#define TYPEMATIC_RATE_7_5 16
#define TYPEMATIC_RATE_6_7 17
#define TYPEMATIC_RATE_6_0 18
#define TYPEMATIC_RATE_5_5 19
#define TYPEMATIC_RATE_5_0 20
#define TYPEMATIC_RATE_4_6 21
#define TYPEMATIC_RATE_4_3 22
#define TYPEMATIC_RATE_4_0 23
#define TYPEMATIC_RATE_3_7 24
#define TYPEMATIC_RATE_3_3 25
#define TYPEMATIC_RATE_3_0 26
#define TYPEMATIC_RATE_2_7 27
#define TYPEMATIC_RATE_2_5 28
#define TYPEMATIC_RATE_2_3 29
#define TYPEMATIC_RATE_2_1 30
#define TYPEMATIC_RATE_2_0 31

//
// Define the default typematic rate and delay value.
//

#define DEFAULT_TYPEMATIC_VALUE (TYPEMATIC_DELAY_250MS | TYPEMATIC_RATE_30_0)

//
// Define keyboard return codes.
//

#define KEYBOARD_STATUS_INVALID 0x00
#define KEYBOARD_STATUS_ACKNOWLEDGE 0xFA
#define KEYBOARD_STATUS_RESEND 0xFE
#define KEYBOARD_STATUS_OVERRUN 0xFF

#define KEYBOARD_BAT_PASS 0xAA

//
// Define keyboard LED state bits.
//

#define KEYBOARD_LED_SCROLL_LOCK 0x01
#define KEYBOARD_LED_NUM_LOCK    0x02
#define KEYBOARD_LED_CAPS_LOCK   0x04

//
// Define identify command responses that come from mice.
//

#define PS2_STANDARD_MOUSE 0x00
#define PS2_MOUSE_WITH_SCROLL_WHEEL 0x03
#define PS2_FIVE_BUTTON_MOUSE 0x04

//
// Define mouse report flags.
//

#define PS2_MOUSE_REPORT_LEFT_BUTTON 0x01
#define PS2_MOUSE_REPORT_RIGHT_BUTTON 0x02
#define PS2_MOUSE_REPORT_MIDDLE_BUTTON 0x04
#define PS2_MOUSE_REPORT_X_OVERFLOW 0x80
#define PS2_MOUSE_REPORT_X_NEGATIVE 0x10
#define PS2_MOUSE_REPORT_Y_NEGATIVE 0x20
#define PS2_MOUSE_REPORT_Y_OVERFLOW 0x40

#define PS2_MOUSE_REPORT_OVERFLOW \
    (PS2_MOUSE_REPORT_X_OVERFLOW | PS2_MOUSE_REPORT_Y_OVERFLOW)

#define PS2_MOUSE_REPORT_BUTTONS \
    (PS2_MOUSE_REPORT_MIDDLE_BUTTON | PS2_MOUSE_REPORT_RIGHT_BUTTON | \
     PS2_MOUSE_REPORT_LEFT_BUTTON);

//
// Define the scan code for set 1 that means 2 bytes are required.
//

#define SCAN_CODE_1_EXTENDED_CODE 0xE0

//
// Define the scan code for set 1 that means 3 bytes are required.
//

#define SCAN_CODE_1_EXTENDED_2_CODE 0xE1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KEYBOARD_KEY
I8042ConvertScanCodeToKey (
    UCHAR ScanCode1,
    UCHAR ScanCode2,
    UCHAR ScanCode3,
    PBOOL KeyUp
    );

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

