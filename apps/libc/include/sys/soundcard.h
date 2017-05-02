/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    soundcard.h

Abstract:

    This header contains definitions for sending and receiving IOCTLs to
    sound devices. This is meant to be compatible with the Open Sound System
    APIs.

Author:

    Chris Stevens 19-Apr-2017

--*/

#ifndef _SYS_SOUNDCARD_H
#define _SYS_SOUNDCARD_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define sound device ioctls.
//

//
// This ioctl reports the native sample formats supported by the device. It
// returns a bitmask of AFMT_* values. The bitmask is the size of an int.
//

#define SNDCTL_DSP_GETFMTS 0x5007

//
// This ioctl reports the amount of input data that is available to read before
// the read will block.
//

#define SNDCTL_DSP_GETISPACE 0x5008

//
// This ioctl reports the amount of output buffer space available to write into
// before the write will block.
//

#define SNDCTL_DSP_GETOSPACE 0x5009

//
// This ioctl sets the number of audio channels to use for I/O. On return, the
// ioctl will pass back the actual number of channels set for the device. This
// may differ from the requested channel count if the device cannot support
// that configuration. The argument is of size int.
//

#define SNDCTL_DSP_CHANNELS 0x5013

//
// This ioctl sets the desired sample format for the device. It takes a bitmask
// of size int that should contain one of the AFMT_* format values. On return,
// the ioctl will pass back the actual sample format for the device. It may
// differ from the requested format if the requested format is not supported.
//

#define SNDCTL_DSP_SETFMT 0x5017

//
// This ioctl sets the buffer fragment size hint. The argument is a 32-bit
// value. The upper 16 bits store the maximum number of fragments that can be
// allocated between 0x2 and 0x7fff. The latter value signifies unlimited
// fragment allocations. The lower 16 bits store the power of 2 fragment size
// between encoded as a selector 'S' where the fragment size is (1 << S).
//

#define SNDCTL_DSP_SETFRAGMENT 0x5018

//
// This ioctl sets the sampling rate in Hz. The sound device will select the
// closest supported sampling rate and report it upon return. The caller should
// check this return value for the true sampling rate.
//

#define SNDCTL_DSP_SPEED 0x501D

//
// This ioctl sets the device into stereo (2 channels) or mono (1 channel).
// This has been replaced by SNDCTL_DSP_CHANNELS, but older applications still
// use it. Supply and integer value of 1 to select stereo mode or 0 to select
// mono mode.
//

#define SNDCTL_DSP_STEREO 0x501E

//
// Define the audio format bits.
//

#define AFMT_S8         0x00000001
#define AFMT_U8         0x00000002
#define AFMT_S16_BE     0x00000004
#define AFMT_S16_LE     0x00000008
#define AFMT_U16_BE     0x00000010
#define AFMT_U16_LE     0x00000020
#define AFMT_S24_BE     0x00000040
#define AFMT_S24_LE     0x00000080
#define AFMT_S32_BE     0x00000100
#define AFMT_S32_LE     0x00000200
#define AFMT_A_LAW      0x00000400
#define AFMT_MU_LAW     0x00000800
#define AFMT_AC3        0x00001000
#define AFMT_FLOAT      0x00002000
#define AFMT_S24_PACKED 0x00004000
#define AFMT_SPDIF_RAW  0x00008000

#define AFMT_U16_NE AFMT_U16_LE
#define AFMT_U16_OE AFMT_U16_BE
#define AFMT_S16_NE AFMT_S16_LE
#define AFMT_S16_OE AFMT_S16_BE
#define AFMT_S24_NE AFMT_S24_LE
#define AFMT_S24_OE AFMT_S24_BE
#define AFMT_S32_NE AFMT_S32_LE
#define AFMT_S32_OE AFMT_S32_BE

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the amount of data available to read from an input
    sound device without blocking and the amount of space available to write to
    an output sound device without blocking.

Members:

    bytes - Stores the number of bytes that can be read or written without
        blocking.

    fragments - Stores the number of fragments that can be read or written
        without blocking. This member is obsolete.

    fragsize - Stores the fragment size in the requested I/O direction.

    fragstotal - Stores the total number of fragments allocated for the
        requested I/O direction.

--*/

typedef struct audio_buf_info {
    int bytes;
    int fragments;
    int fragsize;
    int fragstotal;
} audio_buf_info;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

