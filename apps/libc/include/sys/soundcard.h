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
// This ioctl returns the current input pointer information. This allows an
// application to determine where the hardware is currently operating within
// the audio buffer. It only makes sense to use together with mmap. It takes a
// count_info structure.
//

#define SNDCTL_DSP_GETIPTR 0x5004

//
// This ioctl returns the current output pointer information. This allows an
// application to determine where the hardware is currently operating within
// the audio buffer. It only makes sense to use together with mmap. It takes a
// count_info structure.
//

#define SNDCTL_DSP_GETOPTR 0x5005

//
// This ioctl returns the capabilities supported by the device. It returns a
// bitmask of PCM_CAP_* (or DSP_CAP_* for compatibility).
//

#define SNDCTL_DSP_GETCAPS 0x5006

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
// This ioctl enumerates the list of available output routes for a device. The
// enumerated info includes a list of route names. Use the desired route's
// index to select that route using SNDCTL_DSP_SET_PLAYTGT.
//

#define SNDCTL_DSP_GET_PLAYTGT_NAMES 0x500A

//
// This ioctl returns the index of the currently selected output route for a
// device. The name of the route associated with the index can be queried using
// the SNDCTL_DSP_GET_PLAYTGT_NAMES ioctl.
//

#define SNDCTL_DSP_GET_PLAYTGT 0x500B

//
// This ioctl gets the audio output volume. The returned volume is an integer
// encoded with one value for each of 2 channels where
// Volume = (RightVolume << 8) | LeftVolume. The valid values for each channel
// frange from 0 to 100.
//

#define SNDCTL_DSP_GETPLAYVOL 0x500C

//
// This ioctl enumerates the list of available input routes for a device. The
// enumerated info includes a list of route names. Use the desired route's
// index to select that route using SNDCTL_DSP_SET_RECSRC.
//

#define SNDCTL_DSP_GET_RECSRC_NAMES 0x500D

//
// This ioctl returns the index of the currently selected input route for a
// device. The name of the route associated with the index can be queried using
// the SNDCTL_DSP_GET_RECSRC_NAMES ioctl.
//

#define SNDCTL_DSP_GET_RECSRC 0x500E

//
// This ioctl gets the audio input volume. The returned volume is an integer
// encoded with one value value for each of 2 channels where
// Volume = (RightVolume << 8) | LeftVolume. The valid values for each channel
// frange from 0 to 100.
//

#define SNDCTL_DSP_GETRECVOL 0x500F

//
// This ioctl aborts any current sound recording on the device. This may or may
// not reset the device to a state in which its format, rate, and channel count
// can be changed.
//

#define SNDCTL_DSP_HALT_INPUT 0x5010
#define SNDCTL_DSP_RESET_INPUT SNDCTL_DSP_HALT_INPUT

//
// This ioctl aborts any current sound playback on the device. This may or may
// not reset the device to a state in which its format, rate, and channel count
// can be changed.
//

#define SNDCTL_DSP_HALT_OUTPUT 0x5011
#define SNDCTL_DSP_RESET_OUTPUT SNDCTL_DSP_HALT_OUTPUT

//
// This ioctl aborts any current sound playback or recording on the device.
// This may or may not reset the device to a state in which its format, rate,
// and channel count can be changed.
//

#define SNDCTL_DSP_HALT 0x5012
#define SNDCTL_DSP_RESET SNDCTL_DSP_HALT

//
// This ioctl sets the number of audio channels to use for I/O. On return, the
// ioctl will pass back the actual number of channels set for the device. This
// may differ from the requested channel count if the device cannot support
// that configuration. The argument is of size int.
//

#define SNDCTL_DSP_CHANNELS 0x5013

//
// This ioctl sets the low water mark, in bytes, that is required to be reached
// before an input device will signal that bytes are ready to read or before
// an output device will signal that empty bytes are available to write into.
//

#define SNDCTL_DSP_LOW_WATER 0x5014

//
// This ioctl forces the sound device into non-blocking mode, ignoring the
// file descriptor's O_NONBLOCK file mode flag's state. Using fcntl to
// manipulate O_NONBLOCK is preferred over this ioctl. There is no way to move
// the device handle out of non-blocking mode once this is set.
//

#define SNDCTL_DSP_NONBLOCK 0x5015

//
// This ioctl sets the "timing policy" for the devices. This really dictates
// the size and number of fragments use for the device's buffer. It is thought
// of as a simpler version of SNDCTL_DSP_SETFRAGMENT. The accepted values range
// from 0 (small fragments for low latency, with the caveat that this will
// generate more interrupts and CPU activity) and 10 (large fragments, no
// latency requirements). 5 is the default.
//

#define SNDCTL_DSP_POLICY 0x5016

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
// This ioctl sets the audio output route. Use SNDCTL_DSP_GET_PLAYTGT_NAMES to
// get the list of supported routes and then supply one of the route indices to
// this ioctl. It takes an int value.
//

#define SNDCTL_DSP_SET_PLAYTGT 0x5019

//
// This ioctl sets the audio output volume. The provided volume is an integer
// encoded with one value value for each of 2 channels where
// Volume = (RightVolume << 8) | LeftVolume. The valid values for each channel
// frange from 0 to 100.
//

#define SNDCTL_DSP_SETPLAYVOL 0x501A

//
// This ioctl sets the audio input route. Use SNDCTL_DSP_GET_RECSRC_NAMES to
// get the list of supported routes and then supply one of the route indices to
// this ioctl. It takes an int value.
//

#define SNDCTL_DSP_SET_RECSRC 0x501B

//
// This ioctl sets the audio input volume. The provided volume is an integer
// encoded with one value value for each of 2 channels where
// Volume = (RightVolume << 8) | LeftVolume. The valid values for each channel
// frange from 0 to 100.
//

#define SNDCTL_DSP_SETRECVOL 0x501C

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
// This ioctl starts the audio input or output engine of a sound device. By
// default, input and output will automatically be enabled once a read or write
// is issued. To manually enable an engine, the trigger enable bits must first
// be cleared and then set (i.e. this ioctl needs to be called twice).
//

#define SNDCTL_DSP_SETTRIGGER 0x501F

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
// Define the device capability bits.
//

#define PCM_CAP_REVISION   0x000000FF
#define PCM_CAP_ADMASK     0x00000F00
#define PCM_CAP_ANALOGIN   0x00000100
#define PCM_CAP_ANALOGOUT  0x00000200
#define PCM_CAP_DIGITALIN  0x00000400
#define PCM_CAP_DIGITALOUT 0x00000800
#define PCM_CAP_BATCH      0x00001000
#define PCM_CAP_BIND       0x00002000
#define PCM_CAP_COPROC     0x00004000
#define PCM_CAP_DEFAULT    0x00008000
#define PCM_CAP_DUPLEX     0x00010000
#define PCM_CAP_FREERATE   0x00020000
#define PCM_CAP_HIDDEN     0x00040000
#define PCM_CAP_INPUT      0x00080000
#define PCM_CAP_MMAP       0x00100000
#define PCM_CAP_MODEM      0x00200000
#define PCM_CAP_MULTI      0x00400000
#define PCM_CAP_OUTPUT     0x00800000
#define PCM_CAP_REALTIME   0x01000000
#define PCM_CAP_SHADOW     0x02000000
#define PCM_CAP_SPECIAL    0x04000000
#define PCM_CAP_TRIGGER    0x08000000
#define PCM_CAP_VIRTUAL    0x10000000
#define DSP_CH_MASK        0x60000000
#define DSP_CH_ANY         0x00000000
#define DSP_CH_MONO        0x20000000
#define DSP_CH_STEREO      0x40000000
#define DSP_CH_MULTI       0x60000000

//
// Define the old capability names.
//

#define DSP_CAP_REVISION   PCM_CAP_REVISION
#define DSP_CAP_ADMASK     PCM_CAP_ADMASK
#define DSP_CAP_ANALOGIN   PCM_CAP_ANALOGIN
#define DSP_CAP_ANALOGOUT  PCM_CAP_ANALOGOUT
#define DSP_CAP_DIGITALIN  PCM_CAP_DIGITALIN
#define DSP_CAP_DIGITALOUT PCM_CAP_DIGITALOUT
#define DSP_CAP_BATCH      PCM_CAP_BATCH
#define DSP_CAP_BIND       PCM_CAP_BIND
#define DSP_CAP_COPROC     PCM_CAP_COPROC
#define DSP_CAP_DEFAULT    PCM_CAP_DEFAULT
#define DSP_CAP_DUPLEX     PCM_CAP_DUPLEX
#define DSP_CAP_FREERATE   PCM_CAP_FREERATE
#define DSP_CAP_HIDDEN     PCM_CAP_HIDDEN
#define DSP_CAP_INPUT      PCM_CAP_INPUT
#define DSP_CAP_MMAP       PCM_CAP_MMAP
#define DSP_CAP_MODEM      PCM_CAP_MODEM
#define DSP_CAP_MULTI      PCM_CAP_MULTI
#define DSP_CAP_OUTPUT     PCM_CAP_OUTPUT
#define DSP_CAP_REALTIME   PCM_CAP_REALTIME
#define DSP_CAP_SHADOW     PCM_CAP_SHADOW
#define DSP_CAP_SPECIAL    PCM_CAP_SPECIAL
#define DSP_CAP_TRIGGER    PCM_CAP_TRIGGER
#define DSP_CAP_VIRTUAL    PCM_CAP_VIRTUAL

//
// Define the flags for the set trigger ioctl.
//

#define PCM_ENABLE_INPUT  0x00000001
#define PCM_ENABLE_OUTPUT 0x00000002

//
// Define the maximum number of enumerated devices.
//

#define OSS_ENUM_MAXVALUE 128

//
// Define the size of the device enumerate string buffer.
//

#define OSS_ENUM_STRINGSIZE 2048

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines OSS audio buffer information. It describes the
    amount of data available to read from an input sound device without
    blocking and the amount of space available to write to an output sound
    device without blocking.

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

/*++

Structure Description:

    This structure defines the current location of a sound device within its
    buffer and the amount of data processed by the device.

Members:

    bytes - Stores the total number of bytes processed by the device.

    blocks - Stores the number of fragments processed since the last time the
        count information was queried.

    ptr - Stores the current offset into the sound device buffer. This will be
        between 0 and the buffer size, minus one.

--*/

typedef struct count_info {
    unsigned int bytes;
    int blocks;
    int ptr;
} count_info;

/*++

Structure Description:

    This structure defines a set of enumerated audio devices. It stores a list
    of label names.

Members:

    dev - Stores the mixer device number.

    ctrl - Stores the mixer control number.

    nvalues - Stores the number of enumerated devices in the string index
        array.

    version - Stores the the sequence number of the list of devices. Zero
        indicates that the list is static. If it is non-zero, then the list
        is dynamic and if the version number changes on subsequent checks, then
        the device list has changed.

    strindex - Stores an array of offsets into the string array for the device
        names.

    strings - Stores an array that contains the actual strings. All strings are
        null terminated.

--*/

typedef struct oss_mixer_enuminfo {
    int dev;
    int ctrl;
    int nvalues;
    int version;
    short strindex[OSS_ENUM_MAXVALUE];
    char strings[OSS_ENUM_STRINGSIZE];
} oss_mixer_enuminfo;

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

