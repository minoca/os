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
// Define the sound format bits.
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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

