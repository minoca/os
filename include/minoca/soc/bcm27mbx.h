/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bcm27mbx.h

Abstract:

    This header contains OS definitions for Broadcom 2709 System on Chip
    family's mailbox messaging system.

Author:

    Chris Stevens 10-May-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write the mailbox registers.
//

#define BCM2709_READ_MAILBOX_REGISTER(_Base, _Register) \
    HlReadRegister32((_Base) + (_Register))

#define BCM2709_WRITE_MAILBOX_REGISTER(_Base, _Register, _Value) \
    HlWriteRegister32((_Base) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the header used when sending property messages to
    the BCM2709 mailbox.

Members:

    Size - Stores the size of the data being sent.

    Code - Stores the status code on return from the mailbox.

--*/

typedef struct _BCM2709_MAILBOX_HEADER {
    ULONG Size;
    ULONG Code;
} BCM2709_MAILBOX_HEADER, *PBCM2709_MAILBOX_HEADER;

/*++

Structure Description:

    This structure defines the header for a mailbox tag, that is, an individual
    property's message.

Members:

    Tag - Stores the tag that devices the nature of the mailbox message.

    Size - Stores the number of bytes in the message's buffer.

    Length - Stores the number of bytes sent to the mailbox in the message's
        buffer.

--*/

typedef struct _BCM2709_MAILBOX_TAG {
    ULONG Tag;
    ULONG Size;
    ULONG Length;
} BCM2709_MAILBOX_TAG, *PBCM2709_MAILBOX_TAG;

/*++

Structure Description:

    This structure defines a device state message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    DeviceId - Stores the identification number for the targeted device.

    State - Stores the desired state of the device.

--*/

typedef struct _BCM2709_MAILBOX_DEVICE_STATE {
    BCM2709_MAILBOX_TAG TagHeader;
    ULONG DeviceId;
    ULONG State;
} BCM2709_MAILBOX_DEVICE_STATE, *PBCM2709_MAILBOX_DEVICE_STATE;

/*++

Structure Description:

    This structure defines the data necessary to set a power state for a device.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    DeviceState - Stores a request to set the state for a particular device.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _BCM2709_MAILBOX_POWER {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_DEVICE_STATE DeviceState;
    ULONG EndTag;
} BCM2709_MAILBOX_POWER, *PBCM2709_MAILBOX_POWER;

/*++

Structure Description:

    This structure defines the gets or sets a clock state message for the
    BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ClockId - Stores the identification number for the clock.

    State - Stores the the clock state. See BCM2709_MAILBOX_CLOCK_STATE_* for
        definitions.

--*/

typedef struct _BCM2709_MAILBOX_CLOCK_STATE {
    BCM2709_MAILBOX_TAG TagHeader;
    ULONG ClockId;
    ULONG State;
} BCM2709_MAILBOX_CLOCK_STATE, *PBCM2709_MAILBOX_CLOCK_STATE;

/*++

Structure Description:

    This structure defines the get clock rate message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ClockId - Stores the identification number for the clock.

    Rate - Stores the frequency of the clock in Hz.

--*/

typedef struct _BCM2709_MAILBOX_GET_CLOCK_RATE {
    BCM2709_MAILBOX_TAG TagHeader;
    ULONG ClockId;
    ULONG Rate;
} BCM2709_MAILBOX_GET_CLOCK_RATE, *PBCM2709_MAILBOX_GET_CLOCK_RATE;

/*++

Structure Description:

    This structure defines the set clock rate message for the BCM2709 mailbox.

Members:

    TagHeader - Stores the identification tag header for the message.

    ClockId - Stores the identification number for the clock.

    Rate - Stores the frequency of the clock in Hz.

    SkipSettingTurbo - Stores a boolean indicating whether or not to skip
        setting other high performance ("turbo") settings when the ARM
        frequency is set above the default.

--*/

typedef struct _BCM2709_MAILBOX_SET_CLOCK_RATE {
    BCM2709_MAILBOX_TAG TagHeader;
    ULONG ClockId;
    ULONG Rate;
    ULONG SkipSettingTurbo;
} BCM2709_MAILBOX_SET_CLOCK_RATE, *PBCM2709_MAILBOX_SET_CLOCK_RATE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
