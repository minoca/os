/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbkbd.h

Abstract:

    This header contains internal definitions for the USB keyboard driver.

Author:

    Evan Green 20-Mar-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/usb/usb.h>
#include <minoca/usrinput/usrinput.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used throughout the USB keyboard driver.
//

#define USB_KEYBOARD_ALLOCATION_TAG 0x4B627355 // 'KbsU'

//
// Define the number of keys in the keycode array of the standard HID boot
// keyboard report.
//

#define USB_KEYBOARD_REPORT_KEY_COUNT 6

//
// Define the minimum valid keycode and keycode count.
//

#define USB_KEYBOARD_INVALID_KEY_CODE 1
#define USB_KEYBOARD_FIRST_VALID_KEY_CODE 4
#define USB_KEYCOARD_KEY_CODE_COUNT 0xE8

//
// Define the modifier key bits.
//

#define USB_KEYBOARD_MODIFIER_LEFT_CONTROL  0x01
#define USB_KEYBOARD_MODIFIER_LEFT_SHIFT    0x02
#define USB_KEYBOARD_MODIFIER_LEFT_ALT      0x04
#define USB_KEYBOARD_MODIFIER_LEFT_GUI      0x08
#define USB_KEYBOARD_MODIFIER_RIGHT_CONTROL 0x10
#define USB_KEYBOARD_MODIFIER_RIGHT_SHIFT   0x20
#define USB_KEYBOARD_MODIFIER_RIGHT_ALT     0x40
#define USB_KEYBOARD_MODIFIER_RIGHT_GUI     0x80

//
// Define the LED bits.
//

#define USB_KEYBOARD_LED_NUM_LOCK    0x01
#define USB_KEYBOARD_LED_CAPS_LOCK   0x02
#define USB_KEYBOARD_LED_SCROLL_LOCK 0x04
#define USB_KEYBOARD_LED_COMPOSE     0x08
#define USB_KEYBOARD_LED_KANA        0x10

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the standard report format for a USB HID keyboard
    that conforms to the boot protocol.

Members:

    ModifierKeys - Stores a bitfield of modifier keys (control, shift, etc).

    Reserved - Stores an unused byte.

    Keycode - Stores the array of keys that are pressed down.

--*/

#pragma pack(push, 1)

typedef struct _USB_KEYBOARD_REPORT {
    UCHAR ModifierKeys;
    UCHAR Reserved;
    UCHAR Keycode[USB_KEYBOARD_REPORT_KEY_COUNT];
} PACKED USB_KEYBOARD_REPORT, *PUSB_KEYBOARD_REPORT;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// Define the conversion tables that get between HID usages and OS keyboard key
// codes.
//

extern KEYBOARD_KEY UsbKbdControlKeys[BITS_PER_BYTE];
extern KEYBOARD_KEY UsbKbdKeys[USB_KEYCOARD_KEY_CODE_COUNT];

//
// -------------------------------------------------------- Function Prototypes
//
