/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    onering.h

Abstract:

    This header contains definitions for the USBLED and USB Relay devices.

Author:

    Evan Green 16-Jul-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ONE_RING_USB_RELAY_DEVICE_INFORMATION_UUID \
    {{0x992C1CE4, 0x66224B40, 0xA19BF473, 0xAF9EA3C8}}

#define ONE_RING_USB_LED_DEVICE_INFORMATION_UUID \
    {{0x992C1CE4, 0x66224B40, 0xA19BF473, 0xAF9EA3C9}}

#define ONE_RING_USB_LED_MINI_DEVICE_INFORMATION_UUID \
    {{0x992C1CE4, 0x66224B40, 0xA19BF473, 0xAF9EA3CA}}

//
// Define the length of the serial number.
//

#define ONE_RING_SERIAL_NUMBER_LENGTH 64

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ONE_RING_DEVICE_TYPE {
    OneRingDeviceInvalid,
    OneRingDeviceUsbLed,
    OneRingDeviceUsbLedMini,
    OneRingDeviceUsbRelay
} ONE_RING_DEVICE_TYPE, *PONE_RING_DEVICE_TYPE;

/*++

Structure Description:

    This structure stores the device information published by the device.

Members:

    DeviceType - Stores the device type.

    SerialNumber - Stores the device serial number.

--*/

typedef struct _ONE_RING_DEVICE_INFORMATION {
    ONE_RING_DEVICE_TYPE DeviceType;
    CHAR SerialNumber[ONE_RING_SERIAL_NUMBER_LENGTH];
} ONE_RING_DEVICE_INFORMATION, *PONE_RING_DEVICE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
