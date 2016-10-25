/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    termios.c

Abstract:

    This module implements terminal support.

Author:

    Evan Green 23-Jun-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

//
// ---------------------------------------------------------------------- Macro
//

//
// This macro asserts that the terminal flags are equivalent between the
// kernel and the C library.
//

#define ASSERT_TERMINAL_FLAGS_EQUIVALENT()                              \
    assert((BRKINT == TERMINAL_INPUT_SIGNAL_ON_BREAK) &&                \
           (ICRNL == TERMINAL_INPUT_CR_TO_NEWLINE) &&                   \
           (IGNBRK == TERMINAL_INPUT_IGNORE_BREAK) &&                   \
           (IGNCR == TERMINAL_INPUT_IGNORE_CR) &&                       \
           (IGNPAR == TERMINAL_INPUT_IGNORE_PARITY_ERRORS) &&           \
           (INLCR == TERMINAL_INPUT_NEWLINE_TO_CR) &&                   \
           (INPCK == TERMINAL_INPUT_ENABLE_PARITY_CHECK) &&             \
           (ISTRIP == TERMINAL_INPUT_STRIP) &&                          \
           (IXANY == TERMINAL_INPUT_ANY_CHARACTER_RESTARTS_OUTPUT) &&   \
           (IXOFF == TERMINAL_INPUT_ENABLE_INPUT_FLOW_CONTROL) &&       \
           (IXON == TERMINAL_INPUT_ENABLE_OUTPUT_FLOW_CONTROL) &&       \
           (IMAXBEL == TERMINAL_INPUT_MAX_BELL) &&                      \
           (PARMRK == TERMINAL_INPUT_MARK_PARITY_ERRORS) &&             \
           (OPOST == TERMINAL_OUTPUT_POST_PROCESS) &&                   \
           (ONLCR == TERMINAL_OUTPUT_NEWLINE_TO_CRLF) &&                \
           (OCRNL == TERMINAL_OUTPUT_CR_TO_NEWLINE) &&                  \
           (ONOCR == TERMINAL_OUTPUT_NO_CR_AT_COLUMN_ZERO) &&           \
           (ONLRET == TERMINAL_OUTPUT_NEWLINE_IS_CR) &&                 \
           (OFILL == TERMINAL_OUTPUT_USE_FILL_CHARACTERS) &&            \
           (NLDLY == TERMINAL_OUTPUT_NEWLINE_DELAY) &&                  \
           (NL0 == 0) &&                                                \
           (NL1 == TERMINAL_OUTPUT_NEWLINE_DELAY) &&                    \
           (CRDLY == TERMINAL_OUTPUT_CR_DELAY_MASK) &&                  \
           (CR0 == 0) &&                                                \
           (CR1 == TERMINAL_OUTPUT_CR_DELAY_1) &&                       \
           (CR2 == TERMINAL_OUTPUT_CR_DELAY_2) &&                       \
           (CR3 == TERMINAL_OUTPUT_CR_DELAY_3) &&                       \
           (TABDLY == TERMINAL_OUTPUT_TAB_DELAY_MASK) &&                \
           (TAB0 == 0) &&                                               \
           (TAB1 == TERMINAL_OUTPUT_TAB_DELAY_1) &&                     \
           (TAB2 == TERMINAL_OUTPUT_TAB_DELAY_2) &&                     \
           (TAB3 == TERMINAL_OUTPUT_TAB_DELAY_3) &&                     \
           (BSDLY == TERMINAL_OUTPUT_BACKSPACE_DELAY) &&                \
           (BS0 == 0) &&                                                \
           (BS1 == TERMINAL_OUTPUT_BACKSPACE_DELAY) &&                  \
           (VTDLY == TERMINAL_OUTPUT_VERTICAL_TAB_DELAY) &&             \
           (VT0 == 0) &&                                                \
           (VT1 == TERMINAL_OUTPUT_VERTICAL_TAB_DELAY) &&               \
           (FFDLY == TERMINAL_OUTPUT_FORM_FEED_DELAY) &&                \
           (FF0 == 0) &&                                                \
           (FF1 == TERMINAL_OUTPUT_FORM_FEED_DELAY) &&                  \
           (OFDEL == TERMINAL_OUTPUT_FILL_DEL) &&                       \
           (CSIZE == TERMINAL_CONTROL_CHARACTER_SIZE_MASK) &&           \
           (CS5 == TERMINAL_CONTROL_5_BITS_PER_CHARACTER) &&            \
           (CS6 == TERMINAL_CONTROL_6_BITS_PER_CHARACTER) &&            \
           (CS7 == TERMINAL_CONTROL_7_BITS_PER_CHARACTER) &&            \
           (CS8 == TERMINAL_CONTROL_8_BITS_PER_CHARACTER) &&            \
           (CSTOPB == TERMINAL_CONTROL_2_STOP_BITS) &&                  \
           (CREAD == TERMINAL_CONTROL_ENABLE_RECEIVE) &&                \
           (PARENB == TERMINAL_CONTROL_ENABLE_PARITY) &&                \
           (PARODD == TERMINAL_CONTROL_ODD_PARITY) &&                   \
           (HUPCL == TERMINAL_CONTROL_HANGUP_ON_CLOSE) &&               \
           (CLOCAL == TERMINAL_CONTROL_NO_HANGUP) &&                    \
           (ECHO == TERMINAL_LOCAL_ECHO) &&                             \
           (ECHOE == TERMINAL_LOCAL_ECHO_ERASE) &&                      \
           (ECHOK == TERMINAL_LOCAL_ECHO_KILL_NEWLINE) &&               \
           (ECHOKE == TERMINAL_LOCAL_ECHO_KILL_EXTENDED) &&             \
           (ECHONL == TERMINAL_LOCAL_ECHO_NEWLINE) &&                   \
           (ECHOCTL == TERMINAL_LOCAL_ECHO_CONTROL) &&                  \
           (ICANON == TERMINAL_LOCAL_CANONICAL) &&                      \
           (IEXTEN == TERMINAL_LOCAL_EXTENDED) &&                       \
           (ISIG == TERMINAL_LOCAL_SIGNALS) &&                          \
           (NOFLSH == TERMINAL_LOCAL_NO_FLUSH) &&                       \
           (TOSTOP == TERMINAL_LOCAL_STOP_BACKGROUND_WRITES))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
int
isatty (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine determines if the given file descriptor is backed by an
    interactive terminal device or not.

Arguments:

    FileDescriptor - Supplies the file descriptor to query.

Return Value:

    1 if the given file descriptor is backed by a terminal device.

    0 on error or if the file descriptor is not a terminal device. On error,
    the errno variable will be set to give more details.

--*/

{

    struct termios Settings;

    if (tcgetattr(FileDescriptor, &Settings) == 0) {
        return 1;
    }

    return 0;
}

LIBC_API
int
tcgetattr (
    int FileDescriptor,
    struct termios *Settings
    )

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

{

    return ioctl(FileDescriptor, TCGETS, Settings);
}

LIBC_API
int
tcsetattr (
    int FileDescriptor,
    int When,
    const struct termios *NewSettings
    )

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

{

    int Result;

    if (When == TCSANOW) {
        Result = ioctl(FileDescriptor, TCSETS, NewSettings);

    } else if (When == TCSADRAIN) {
        Result = ioctl(FileDescriptor, TCSETSW, NewSettings);

    } else if (When == TCSAFLUSH) {
        Result = ioctl(FileDescriptor, TCSETSF, NewSettings);

    } else {
        errno = EINVAL;
        Result = -1;
    }

    return Result;
}

LIBC_API
speed_t
cfgetispeed (
    const struct termios *Settings
    )

/*++

Routine Description:

    This routine gets the input baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

Return Value:

    Returns the input speed, in baud.

--*/

{

    return Settings->c_ispeed;
}

LIBC_API
speed_t
cfgetospeed (
    const struct termios *Settings
    )

/*++

Routine Description:

    This routine gets the output baud rate from the given terminal settings.

Arguments:

    Settings - Supplies a pointer to the terminal settings retrieved with a
        call to tcgetattr.

Return Value:

    Returns the output speed, in baud.

--*/

{

    return Settings->c_ospeed;
}

LIBC_API
int
cfsetispeed (
    struct termios *Settings,
    speed_t NewBaudRate
    )

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

{

    Settings->c_ispeed = NewBaudRate;
    return 0;
}

LIBC_API
int
cfsetospeed (
    struct termios *Settings,
    speed_t NewBaudRate
    )

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

{

    Settings->c_ospeed = NewBaudRate;
    return 0;
}

LIBC_API
void
cfmakeraw (
    struct termios *Settings
    )

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

{

    Settings->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR |
                           ICRNL | IXON | IMAXBEL);

    Settings->c_oflag &= ~OPOST;
    Settings->c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    Settings->c_cflag &= ~(CSIZE | PARENB);
    Settings->c_cflag |= CS8;
    Settings->c_cc[VMIN] = 1;
    Settings->c_cc[VTIME] = 0;
    return;
}

LIBC_API
int
tcflush (
    int FileDescriptor,
    int Selector
    )

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

{

    assert((TCIFLUSH == SYS_FLUSH_FLAG_READ) &&
           (TCOFLUSH == SYS_FLUSH_FLAG_WRITE) &&
           (TCIOFLUSH == (SYS_FLUSH_FLAG_READ | SYS_FLUSH_FLAG_WRITE)));

    return ioctl(FileDescriptor, TCFLSH, Selector);
}

LIBC_API
int
tcdrain (
    int FileDescriptor
    )

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

{

    //
    // TCSBRK with a non-zero value is undefined, but in this implementation
    // implements tcdrain behavior.
    //

    return ioctl(FileDescriptor, TCSBRK, 1);
}

LIBC_API
int
tcflow (
    int FileDescriptor,
    int Action
    )

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

{

    return ioctl(FileDescriptor, TCXONC, Action);
}

LIBC_API
int
tcsendbreak (
    int FileDescriptor,
    int Duration
    )

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

{

    return ioctl(FileDescriptor, TCSBRK, Duration);
}

LIBC_API
pid_t
tcgetsid (
    int FileDescriptor
    )

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

{

    pid_t ProcessGroup;
    int Status;

    ProcessGroup = -1;
    Status = ioctl(FileDescriptor, TIOCGSID, &ProcessGroup);
    if (Status != 0) {
        return (pid_t)-1;
    }

    return ProcessGroup;
}

LIBC_API
int
tcsetpgrp (
    int FileDescriptor,
    pid_t ProcessGroupId
    )

/*++

Routine Description:

    This routine sets the foreground process group ID associated with the
    given terminal file descriptor. The application shall ensure that the file
    associated with the given descriptor is the controlling terminal of the
    calling process and the controlling terminal is currently associated with
    the session of the calling process. The application shall ensure that the
    given process group ID is led by a process in the same session as the
    calling process.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

    ProcessGroupId - Supplies the process group ID to set for the terminal.

Return Value:

    0 on success.

    -1 on failure, and the errno will be set to contain more information.

--*/

{

    PROCESS_GROUP_ID Identifier;

    Identifier = ProcessGroupId;
    return ioctl(FileDescriptor, TIOCSPGRP, &Identifier);
}

LIBC_API
pid_t
tcgetpgrp (
    int FileDescriptor
    )

/*++

Routine Description:

    This routine returns the value of the process group ID of the foreground
    process associated with the given terminal. If ther is no foreground
    process group, this routine returns a value greater than 1 that does not
    match the process group ID of any existing process group.

Arguments:

    FileDescriptor - Supplies the file descriptor of the terminal.

Return Value:

    Returns the process group ID of the foreground process associated with the
    terminal on success.

    -1 on failure, and errno will be set to contain more information. Possible
    values of errno are:

    EBADF if the file descriptor is invalid.

    ENOTTY if the calling process does not having a controlling terminal, or
    the file is not the controlling terminal.

--*/

{

    PROCESS_GROUP_ID Identifier;
    int Result;

    Result = ioctl(FileDescriptor, TIOCGPGRP, &Identifier);
    if (Result < 0) {
        return Result;
    }

    return (pid_t)Identifier;
}

//
// --------------------------------------------------------- Internal Functions
//

