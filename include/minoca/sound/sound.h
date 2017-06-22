/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sound.h

Abstract:

    This header contains definitions for the sound system.

Author:

    Chris Stevens 19-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the sound format bits. These match the OSS format definitions.
//

#define SOUND_FORMAT_8_BIT_SIGNED                  0x00000001
#define SOUND_FORMAT_8_BIT_UNSIGNED                0x00000002
#define SOUND_FORMAT_16_BIT_SIGNED_BIG_ENDIAN      0x00000004
#define SOUND_FORMAT_16_BIT_SIGNED_LITTLE_ENDIAN   0x00000008
#define SOUND_FORMAT_16_BIT_UNSIGNED_BIG_ENDIAN    0x00000010
#define SOUND_FORMAT_16_BIT_UNSIGNED_LITTLE_ENDIAN 0x00000020
#define SOUND_FORMAT_24_BIT_SIGNED_BIG_ENDIAN      0x00000040
#define SOUND_FORMAT_24_BIT_SIGNED_LITTLE_ENDIAN   0x00000080
#define SOUND_FORMAT_32_BIT_SIGNED_BIG_ENDIAN      0x00000100
#define SOUND_FORMAT_32_BIT_SIGNED_LITTLE_ENDIAN   0x00000200
#define SOUND_FORMAT_A_LAW                         0x00000400
#define SOUND_FORMAT_MU_LAW                        0x00000800
#define SOUND_FORMAT_AC3                           0x00001000
#define SOUND_FORMAT_FLOAT                         0x00002000
#define SOUND_FORMAT_24_BIT_SIGNED_PACKED          0x00004000
#define SOUND_FORMAT_SPDIF_RAW                     0x00008000

//
// Define the bit masks for the buffer size hint IOCTL.
//

#define SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_MASK  0xFFFF0000
#define SOUND_BUFFER_SIZE_HINT_FRAGMENT_COUNT_SHIFT 16
#define SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_MASK   0x0000FFFF
#define SOUND_BUFFER_SIZE_HINT_FRAGMENT_SIZE_SHIFT  0

//
// Define the sound volume masks. Each channel holds values between 0 (mute)
// and 100 (full volume).
//

#define SOUND_VOLUME_RIGHT_CHANNEL_MASK  0x0000FF00
#define SOUND_VOLUME_RIGHT_CHANNEL_SHIFT 8
#define SOUND_VOLUME_LEFT_CHANNEL_MASK   0x000000FF
#define SOUND_VOLUME_LEFT_CHANNEL_SHIFT  0

#define SOUND_VOLUME_MAXIMUM 100

//
// Define the device capability bits. These must match the OSS format.
//

#define SOUND_CAPABILITY_REVISION              0x000000FF
#define SOUND_CAPABILITY_INTERFACE_MASK        0x00000F00
#define SOUND_CAPABILITY_INTERFACE_ANALOG_IN   0x00000100
#define SOUND_CAPABILITY_INTERFACE_ANALOG_OUT  0x00000200
#define SOUND_CAPABILITY_INTERFACE_DIGITAL_IN  0x00000400
#define SOUND_CAPABILITY_INTERFACE_DIGITAL_OUT 0x00000800
#define SOUND_CAPABILITY_BATCH                 0x00001000
#define SOUND_CAPABILITY_BIND                  0x00002000
#define SOUND_CAPABILITY_COPROCESSOR           0x00004000
#define SOUND_CAPABILITY_DEFAULT               0x00008000
#define SOUND_CAPABILITY_DUPLEX                0x00010000
#define SOUND_CAPABILITY_VARIABLE_RATES        0x00020000
#define SOUND_CAPABILITY_HIDDEN                0x00040000
#define SOUND_CAPABILITY_INPUT                 0x00080000
#define SOUND_CAPABILITY_MMAP                  0x00100000
#define SOUND_CAPABILITY_MODEM                 0x00200000
#define SOUND_CAPABILITY_MULTIPLE_ENGINES      0x00400000
#define SOUND_CAPABILITY_OUTPUT                0x00800000
#define SOUND_CAPABILITY_REALTIME              0x01000000
#define SOUND_CAPABILITY_SHADOW                0x02000000
#define SOUND_CAPABILITY_SPECIAL               0x04000000
#define SOUND_CAPABILITY_MANUAL_ENABLE         0x08000000
#define SOUND_CAPABILITY_VIRTUAL               0x10000000
#define SOUND_CAPABILITY_CHANNEL_MASK          0x60000000
#define SOUND_CAPABILITY_CHANNEL_ANY           0x00000000
#define SOUND_CAPABILITY_CHANNEL_MONO          0x20000000
#define SOUND_CAPABILITY_CHANNEL_STEREO        0x40000000
#define SOUND_CAPABILITY_CHANNEL_MULTI         0x60000000

//
// Define the enable bits. These must match the OSS format.
//

#define SOUND_ENABLE_INPUT  0x00000001
#define SOUND_ENABLE_OUTPUT 0x00000002

//
// Define the maximum timing policy with the least latency requirements.
//

#define SOUND_TIMING_POLICY_MAX 10

//
// Define the maximum number of device routes.
//

#define SOUND_ROUTE_COUNT_MAX 128

//
// Define the size of the device enumeration string buffer.
//

#define SOUND_ROUTE_NAME_SIZE 2048

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the possible sound IOCTL numbers.
//

typedef enum _SOUND_CONTROL {
    SoundGetVersion = 0x5000,
    SoundGetAudioDeviceInformation = 0x5001,
    SoundGetCardInformation = 0x5002,
    SoundGetSystemInformation = 0x5003,
    SoundGetCurrentInputPosition = 0x5004,
    SoundGetCurrentOutputPosition = 0x5005,
    SoundGetDeviceCapabilities = 0x5006,
    SoundGetSupportedFormats = 0x5007,
    SoundGetInputQueueSize = 0x5008,
    SoundGetOutputQueueSize = 0x5009,
    SoundGetSupportedOutputRoutes = 0x500A,
    SoundGetOutputRoute = 0x500B,
    SoundGetOutputVolume = 0x500C,
    SoundGetSupportedInputRoutes = 0x500D,
    SoundGetInputRoute = 0x500E,
    SoundGetInputVolume = 0x500F,
    SoundStopInput = 0x5010,
    SoundStopOutput = 0x5011,
    SoundStopAll = 0x5012,
    SoundSetChannelCount = 0x5013,
    SoundSetLowThreshold = 0x5014,
    SoundSetNonBlock = 0x5015,
    SoundSetTimingPolicy = 0x5016,
    SoundSetFormat = 0x5017,
    SoundSetBufferSizeHint = 0x5018,
    SoundSetOutputRoute = 0x5019,
    SoundSetOutputVolume = 0x501A,
    SoundSetInputRoute = 0x501B,
    SoundSetInputVolume = 0x501C,
    SoundSetSampleRate = 0x501D,
    SoundSetStereo = 0x501E,
    SoundEnableDevice = 0x501F,
} SOUND_CONTROL, *PSOUND_CONTROL;

/*++

Structure Description:

    This structure describes the amount of data available to read from an input
    sound device queue without blocking and the amount of space available to
    write to an output sound device queue without blocking.

Members:

    BytesAvailable - Stores the number of bytes that can be read or written
        without blocking.

    FragmentsAvailable - Stores the number of fragments that can be read or
        written without blocking. This member is obsolete.

    FragmentSize - Stores the fragment size in the requested I/O direction.

    FragmentCount - Stores the total number of fragments allocated for the
        requested I/O direction.

--*/

typedef struct _SOUND_QUEUE_INFORMATION {
    LONG BytesAvailable;
    LONG FragmentsAvailable;
    LONG FragmentSize;
    LONG FragmentCount;
} SOUND_QUEUE_INFORMATION, *PSOUND_QUEUE_INFORMATION;

/*++

Structure Description:

    This structure describes the current position of a sound device within its
    buffer and the amount of data processed by the device.

Members:

    TotalBytes - Stores the total number of bytes processed by the device.

    FragmentCount - Stores the number of fragments processed since the last
        time the count information was queried.

    Offset - Stores the current offset into the sound device buffer. This will
        be between 0 and the buffer size, minus one.

--*/

typedef struct _SOUND_POSITION_INFORMATION {
    ULONG TotalBytes;
    LONG FragmentCount;
    LONG Offset;
} SOUND_POSITION_INFORMATION, *PSOUND_POSITION_INFORMATION;

/*++

Structure Description:

    This structure defines a set of available routes that can be set for a
    given audio device.

Members:

    DeviceNumber - Stores the device number.

    DeviceControl - Stores the device's control number.

    RouteCount - Stores the number of routes available on the device.

    SequenceNumber - Stores the the sequence number of the list of routes. Zero
        indicates that the list is static. If it is non-zero, then the list
        is dynamic and if the sequence number changes on subsequent checks,
        then the route list has changed.

    RouteIndex - Stores an array of offsets into the routes array for the
        route names.

    RouteName - Stores an array that contains the actual strings for the route
        names. All strings are null terminated.

--*/

typedef struct _SOUND_DEVICE_ROUTE_INFORMATION {
    LONG DeviceNumber;
    LONG ControlNumber;
    LONG RouteCount;
    LONG SequenceNumber;
    SHORT RouteIndex[SOUND_ROUTE_COUNT_MAX];
    CHAR RouteName[SOUND_ROUTE_NAME_SIZE];
} SOUND_DEVICE_ROUTE_INFORMATION, *PSOUND_DEVICE_ROUTE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

