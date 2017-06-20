/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sound.h

Abstract:

    This header contains the device information structure format for sound
    controllers.

Author:

    Chris Stevens 20-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define SOUND_DEVICE_INFORMATION_UUID \
    {{0x751ebb92, 0x25e711e7, 0xad290401, 0x0fdd7401}}

#define SOUND_DEVICE_INFORMATION_VERSION 0x00000001

//
// Define sound device information flags.
//

#define SOUND_DEVICE_INFORMATION_FLAG_PRIMARY 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the information published by sound controller.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to SOUND_DEVICE_INFORMATION_VERSION.

    Flags - Stores a bitfield of flags describing the sound controller. See
        SOUND_DEVICE_FLAG_* definitions.

    InputDeviceCount - Stores the number of input devices connected to the
        sound controller.

    OutputDeviceCount - Stores the number of output devices connected to the
        sound controller.

--*/

typedef struct _SOUND_DEVICE_INFORMATION {
    ULONG Version;
    ULONG Flags;
    ULONG InputDeviceCount;
    ULONG OutputDeviceCount;
} SOUND_DEVICE_INFORMATION, *PSOUND_DEVICE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

