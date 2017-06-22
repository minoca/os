/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sound.h

Abstract:

    This header contains definitions for the sound core library driver.

Author:

    Chris Stevens 18-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Redefine the API define into an export.
//

#define SOUND_API __DLLEXPORT

#include <minoca/sound/sndcore.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used across the networking core library.
//

#define SOUND_CORE_ALLOCATION_TAG 0x43646E53 // 'CdnS'

//
// This flag is atomically set if the sound device is currently in use. Only
// one handle can be taken on a sound device unless it is a mixer.
//

#define SOUND_DEVICE_FLAG_INTERNAL_BUSY 0x80000000

//
// These flags dictate whether the device is automatically started on the first
// read or write. If they are not set, then the device can only be started
// via an ioctl. They are set by default on device open.
//

#define SOUND_DEVICE_FLAG_INTERNAL_ENABLE_INPUT  0x40000000
#define SOUND_DEVICE_FLAG_INTERNAL_ENABLE_OUTPUT 0x20000000

//
// Define the default fragment size, in bytes.
//

#define SOUND_FRAGMENT_SIZE_DEFAULT _2KB

//
// Define the default fragment size shift.
//

#define SOUND_FRAGMENT_SIZE_DEFAULT_SHIFT 11

//
// Define the default fragment count.
//

#define SOUND_FRAGMENT_COUNT_DEFAULT 2

//
// Define the minimum number of fragments allowed.
//

#define SOUND_FRAGMENT_COUNT_MINIMUM 2

//
// Define the default sample rate, in Hz.
//

#define SOUND_SAMPLE_RATE_DEFAULT 48000

//
// Define the number of channels used in stereo sound.
//

#define SOUND_STEREO_CHANNEL_COUNT 2

//
// Define the number of channels used in mono sound.
//

#define SOUND_MONO_CHANNEL_COUNT 1

//
// Default to volumes of 50-50 in both channels.
//

#define SOUND_VOLUME_DEFAULT                   \
    (75 << SOUND_VOLUME_RIGHT_CHANNEL_SHIFT) | \
    (75 << SOUND_VOLUME_LEFT_CHANNEL_SHIFT);

//
// Define the bits for the sound device handle flags.
//

#define SOUND_DEVICE_HANDLE_FLAG_NON_BLOCKING  0x00000001
#define SOUND_DEVICE_HANDLE_FLAG_LOW_WATER_SET 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the sound core library's internal controller
    information.

Members:

    Host - Stores the host controller's information.

    ReferenceCount - Stores the reference count on the controller.

    CreationTime - Stores the system time when the device was created.

--*/

struct _SOUND_CONTROLLER {
    SOUND_CONTROLLER_INFORMATION Host;
    volatile ULONG ReferenceCount;
    SYSTEM_TIME CreationTime;
};

/*++

Structure Description:

    This structure defines the sound cores representation of a device.

Members:

    Controller - Stores a pointer to the controller to which the device
        belongs.

    Device - Stores an optional pointer to the sound device attached to this
        handle. If this is NULL then this is a handle to the controller itself.

    Lock - Store a pointer to a queued lock that serializes access to the
        device.

    State - Stores the current state of the sound device.

    Buffer - Stores the sound buffer information for this handle.

    Flags - Stores a bitmask of sound device handle flags. See
        SOUND_DEVICE_HANDLE_FLAG_* for definitions.

    Format - Stores the current stream format for the device. See
        SOUND_FORMAT_* for definitions.

    ChannelCount - Stores the current channel count for the device.

    SampleRate - Stores the current sample rate, in Hz.

    Volume - Stores the device volume. This stores both the left and right
        channel volume. If the device does not support separate channel volume
        control, it should use the left channel volume. See SOUND_VOLUME_* for
        mask definitions.

    CurrentRoute - Stores the index of current route for the device. The
        default is route 0. The route information is stored in the sound
        device structure.

--*/

struct _SOUND_DEVICE_HANDLE {
    PSOUND_CONTROLLER Controller;
    PSOUND_DEVICE Device;
    PQUEUED_LOCK Lock;
    SOUND_DEVICE_STATE State;
    SOUND_IO_BUFFER Buffer;
    volatile ULONG Flags;
    ULONG Format;
    ULONG ChannelCount;
    ULONG SampleRate;
    ULONG Volume;
    ULONG Route;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

