/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    termios.h

Abstract:

    This header contains definitions for terminal control.

Author:

    Evan Green 23-Jun-2013

--*/

#ifndef _TERMIOS_H
#define _TERMIOS_H

//
// ------------------------------------------------------------------- Includes
//

#include <sys/types.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define input control flags.
//

//
// Set this flag to ignore break conditions.
//

#define IGNBRK 0x00000001

//
// Set this flag to signal an interrupt on break.
//

#define BRKINT 0x00000002

//
// Set this flag to ignore characters with parity errors.
//

#define IGNPAR 0x00000004

//
// Set this flag to mark parity errors.
//

#define PARMRK 0x00000008

//
// Set this flag to enable input parity checking.
//

#define INPCK 0x00000010

//
// Set this flag to strip characters.
//

#define ISTRIP 0x00000020

//
// Set this flag to map newlines (\n) to carriage returns (\r) on input.
//

#define INLCR 0x00000040

//
// Set this flag to ignore carriage returns.
//

#define IGNCR 0x00000080

//
// Set this flag to map carriage return (\r) characters to newlines (\n) on
// input.
//

#define ICRNL 0x00000100

//
// Set this flag to enable start/stop output control.
//

#define IXON 0x00000200

//
// Set this flag to enable start/stop input control.
//

#define IXOFF 0x00000400

//
// Set this flag to enable any character to restart output.
//

#define IXANY 0x00000800

//
// Set this flag to cause a bell character to be sent to the output if the
// input buffer is full. If this flag is not set and a new character is
// received when the input queue is full, then the entire current input and
// output queue is discarded.
//

#define IMAXBEL 0x00001000

//
// Define output control flags.
//

//
// Set this flag to post-process output.
//

#define OPOST 0x00000001

//
// Set this flag to map newlines (\n) or CR-NL (\r\n) on output.
//

#define ONLCR 0x00000002

//
// Set this flag to map carriage returns (\r) to newlines (\n) on output.
//

#define OCRNL 0x00000004

//
// Set this flag to avoid carriage return output at column 0.
//

#define ONOCR 0x00000008

//
// Set this flag to have newline perform carriage return functionality.
//

#define ONLRET 0x00000010

//
// Set this flag to use fill characters for delay.
//

#define OFILL 0x00000020

//
// Set this flag to enable newline delays, type 0 or mode 1 (0.1 seconds).
//

#define NLDLY (NL0 | NL1)
#define NL0 0x00000000
#define NL1 0x00000040

//
// Set this flag to select carriage return delays, types 0 through 3.
// Type 1 delays for an amount dependent on column position. Type 2 is about
// 0.1 seconds, and type 3 is about 0.15 seconds. If OFILL is set, type 1
// transmits two fill characters and type 2 transmits four fill characters.
//

#define CRDLY (CR0 | CR1 | CR2 | CR3)
#define CR0 0x00000000
#define CR1 0x00000080
#define CR2 0x00000100
#define CR3 0x00000180

//
// Set this flag to enable tab delays, types 0 through 3.
// Type 1 is dependent on column positions, type 2 is 0.1 seconds, and type 3
// is "expand tabs to spaces". If OFILL is set, any delay transmits two fill
// characters.
//

#define TABDLY (TAB0 | TAB1 | TAB2 | TAB3)
#define TAB0 0x00000000
#define TAB1 0x00000200
#define TAB2 0x00000400
#define TAB3 0x00000600

//
// Set this flag to enable backspace delays, type 0 or type 1. Type 1 is
// 0.05 seconds or one fill character.
//

#define BSDLY (BS0 | BS1)
#define BS0 0x00000000
#define BS1 0x00000800

//
// Set this flag to enable vertical tab delays, type 0 or 1. Type 1 lasts two
// seconds.
//

#define VTDLY (VT0 | VT1)
#define VT0 0x00000000
#define VT1 0x00001000

//
// Set this flag to enable form fee delays, type 0 or 1. Type 1 lasts two
// seconds.
//

#define FFDLY (FF0 | FF1)
#define FF0 0x00000000
#define FF1 0x00002000

//
// Set this flag to set the fill character to ASCII DEL (127). If this flag is
// not set, the fill character is NUL (0).
//

#define OFDEL 0x00004000

//
// Define valid baud rates.
//

//
// Hang up.
//

#define B0 0

//
// The remainder correspond to their exact baud rates, except for B134 which
// corresponds to a baud rate of 134.5. Because I know you cared a lot about
// that one.
//

#define B50 50
#define B75 75
#define B110 110
#define B134 134
#define B150 150
#define B200 200
#define B300 300
#define B600 600
#define B1200 1200
#define B1800 1800
#define B2400 2400
#define B4800 4800
#define B9600 9600
#define B19200 19200
#define B38400 38400
#define B57600 57600
#define B115200 115200
#define B230400 230400
#define B460800 460800
#define B500000 500000
#define B576000 576000
#define B921600 921600
#define B1000000 1000000
#define B1152000 1152000
#define B1500000 1500000
#define B2000000 2000000
#define B2500000 2500000
#define B3000000 3000000
#define B3500000 3500000
#define B4000000 4000000

//
// Define control mode flags.
//

//
// Set this field to set the number of bits per character.
//

#define CSIZE (CS5 | CS6 | CS7 | CS8)
#define CS5 0x00000000
#define CS6 0x00000001
#define CS7 0x00000002
#define CS8 0x00000003

//
// Set this bit to send two stop bits (without it set one stop bit is sent).
//

#define CSTOPB 0x00000004

//
// Set this bit to enable the receiver.
//

#define CREAD 0x00000008

//
// Set this bit to enable a parity bit.
//

#define PARENB 0x00000010

//
// Set this bit to enable odd parity (without this bit set even parity is used).
//

#define PARODD 0x00000020

//
// Set this bit to send a hangup signal when the terminal is closed.
//

#define HUPCL 0x00000040

//
// Set this bit to ignore modem status lines (and do not send a hangup signal).
//

#define CLOCAL 0x00000080

//
// Define local mode flags.
//

//
// Set this bit enable automatically echoing input characters to the output.
//

#define ECHO 0x00000001

//
// Set this bit to enable echoing the erase character.
//

#define ECHOE 0x00000002

//
// Set this bit to enable echoing the kill character.
//

#define ECHOK 0x00000004

//
// Set this bit to enable echoing newline (\n) characters.
//

#define ECHONL 0x00000008

//
// Set this bit to enable canonical mode, which enables a handful of
// automatic line processing features.
//

#define ICANON 0x00000010

//
// Set this bit to enable extended processing. With extended processing,
// the erase, kill, and end of file characters can be preceded by a backslash
// to remove their special meaning.
//

#define IEXTEN 0x00000020

//
// Set this bit to enable extended input character processing.
//

#define ISIG 0x00000040

//
// Set this bit to disable flushing after an interrupt or quit character.
//

#define NOFLSH 0x00000080

//
// Set this bit to send a SIGTTOU signal for background output.
//

#define TOSTOP 0x00000100

//
// Set this bit to enable echoing the kill character as visually erasing the
// entire line. If this is not set, then the ECHOK flag controls behavior.
//

#define ECHOKE 0x00000200

//
// Set this bit to enable echoing control characters with '^' followed by their
// corresponding text character.
//

#define ECHOCTL 0x00000400

//
// Define constants used to determine when an attribute change will take
// effect.
//

//
// Define the value used to enact changes immediately.
//

#define TCSANOW 0

//
// Define the value used to enact changes when the output has drained.
//

#define TCSADRAIN 1

//
// Define the value used to enact changes when the output has drained, also
// flush pending input.
//

#define TCSAFLUSH 2

//
// Define flush actions.
//

//
// Use this value to flush pending input.
//

#define TCIFLUSH 0x2

//
// Use this value to flush untransmitted output.
//

#define TCOFLUSH 0x4

//
// Use this value to flush both pending input and untransmitted output.
//

#define TCIOFLUSH (TCIFLUSH | TCOFLUSH)

//
// Define flow control constants.
//

//
// Use this constant to transmit a STOP character, intended to suspend input
// data.
//

#define TCIOFF 0

//
// Use this constant to transmit a START character intended to restart input
// data.
//

#define TCION 1

//
// Use this constant to suspend output.
//

#define TCOOFF 2

//
// Use this constant to restart output.
//

#define TCOON 3

//
// Define the indices of various control characters in the array.
//

//
// Define the End Of File character index.
//

#define VEOF 0

//
// Define the End of Line character index.
//

#define VEOL 1

//
// Define the erase character index.
//

#define VERASE 2

//
// Define the interrupt character index.
//

#define VINTR 3

//
// Define the kill character index.
//

#define VKILL 4

//
// Define the minimum character count to flush index.
//

#define VMIN 5

//
// Define the quit character index.
//

#define VQUIT 6

//
// Define the start flow control character index.
//

#define VSTART 7

//
// Define the stop flow control character index.
//

#define VSTOP 8

//
// Define the suspend character index.
//

#define VSUSP 9

//
// Define the time before flushing anyway index.
//

#define VTIME 10

//
// Define the array size for control characters.
//

#define NCCS 11

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the type used for terminal special characters.
//

typedef unsigned char cc_t;

//
// Define the type used for baud rates.
//

typedef unsigned speed_t;

//
// Define the type used for terminal modes.
//

typedef unsigned tcflag_t;

/*++

Structure Description:

    This structure stores terminal behavior and control data.

Members:

    c_iflag - Stores the input mode flags.

    c_oflag - Stores the output mode flags.

    c_cflag - Stores the control mode flags.

    c_lflag - Stores the local control flags.

    c_cc - Stores the characters to use for control characters.

    c_ispeed - Stores the input baud rate.

    c_ospeed - Stores the output baud rate.

--*/

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[NCCS];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
tcgetattr (
    int FileDescriptor,
    struct termios *Settings
    );

/*++

Routine Description:

    This routine gets the current terminal settings.

Arguments:

    FileDescriptor - Supplies the file descriptor for the terminal.

    Settings - Supplies a pointer where the terminal settings will be returned
        on success.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to provide more
    information.

--*/

LIBC_API
int
tcsetattr (
    int FileDescriptor,
    int When,
    const struct termios *NewSettings
    );

/*++

Routine Description:

    This routine sets the given terminal's attributes.

Arguments:

    FileDescriptor - Supplies the file descriptor for the terminal.

    When - Supplies more information about when the new settings should take
        effect. See TCSA* definitions.

    NewSettings - Supplies a pointer to the new settings.

Return Value:

    0 on success.

    -1 on failure, and the errno variable will be set to provide more
    information.

--*/

LIBC_API
speed_t
cfgetispeed (
    const struct termios *Settings
    );

/*++

Routine Description:

    This routine gets the input baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

Return Value:

    Returns the input speed, in baud.

--*/

LIBC_API
speed_t
cfgetospeed (
    const struct termios *Settings
    );

/*++

Routine Description:

    This routine gets the output baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

Return Value:

    Returns the output speed, in baud.

--*/

LIBC_API
int
cfsetispeed (
    struct termios *Settings,
    speed_t NewBaudRate
    );

/*++

Routine Description:

    This routine sets the input baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

    NewBaudRate - Supplies the new input baud rate to set.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to EINVAL to indicate the given
    baud rate is invalid or not achievable.

--*/

LIBC_API
int
cfsetospeed (
    struct termios *Settings,
    speed_t NewBaudRate
    );

/*++

Routine Description:

    This routine sets the output baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

    NewBaudRate - Supplies the new output baud rate to set.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to EINVAL to indicate the given
    baud rate is invalid or not achievable.

--*/

LIBC_API
void
cfmakeraw (
    struct termios *Settings
    );

/*++

Routine Description:

    This routine sets the given settings to "raw" mode, disabling all line
    processing features and making the terminal as basic as possible.

Arguments:

    Settings - Supplies a pointer to the terminal settings to adjust to raw
        mode.

Return Value:

    None.

--*/

LIBC_API
int
tcflush (
    int FileDescriptor,
    int Selector
    );

/*++

Routine Description:

    This routine discards data written to the the given terminal, data received
    but not yet read from the terminal, or both.

    Attempts to use this function from a process which is a member of the
    background process group on the given terminal will cause the process group
    to be sent a SIGTTOU. If the calling process is blocking or ignoring
    SIGTTOU, the process shall be allowed to perform the operation, and no
    signal is sent.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal to flush.

    Selector - Supplies the type of flush to perform. Valid values are
        TCIFLUSH to flush data received but not read, TCOFLUSH to flush data
        written but not transmitted, and TCIOFLUSH to flush both types.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

LIBC_API
int
tcdrain (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine waits until all output written to the terminal at the given
    file descriptor is written.

    Attempts to use this function from a process which is a member of the
    background process group on the given terminal will cause the process group
    to be sent a SIGTTOU. If the calling process is blocking or ignoring
    SIGTTOU, the process shall be allowed to perform the operation, and no
    signal is sent.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal to drain.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

LIBC_API
int
tcflow (
    int FileDescriptor,
    int Action
    );

/*++

Routine Description:

    This routine suspends or restarts transmission of data on the given
    terminal.

    Attempts to use this function from a process which is a member of the
    background process group on the given terminal will cause the process group
    to be sent a SIGTTOU. If the calling process is blocking or ignoring
    SIGTTOU, the process shall be allowed to perform the operation, and no
    signal is sent.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

    Action - Supplies the action to perform. Valid values are:

        TCOOFF - Suspends output.

        TCOON - Resumes suspended output.

        TCIOFF - Causes the system to transmit a STOP character, which is
            intended to cause the terminal device to stop transmitting data to
            this system.

        TCION - Causes the system to transmit a START character, which is
            intended to restart the sending of data to this terminal.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

LIBC_API
int
tcsendbreak (
    int FileDescriptor,
    int Duration
    );

/*++

Routine Description:

    This routine sends a continuous stream of zero-valued bits for a specific
    duration if the given terminal is using asynchronous serial data
    transmission. If the terminal is not using asynchronous serial data
    transmission, this routine returns without performing any action.

    Attempts to use this function from a process which is a member of the
    background process group on the given terminal will cause the process group
    to be sent a SIGTTOU. If the calling process is blocking or ignoring
    SIGTTOU, the process shall be allowed to perform the operation, and no
    signal is sent.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

    Duration - Supplies a value that if zero causes the duration to be between
        0.25 and 0.5 seconds. If duration is not zero, it sends zero-valued
        bits for an implementation-defined length of time.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

LIBC_API
pid_t
tcgetsid (
    int FileDescriptor
    );

/*++

Routine Description:

    This routine gets the process group ID of the session for which the
    terminal specified by the given file descriptor is the controlling terminal.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

Return Value:

    Returns the process group ID associated with the terminal on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

