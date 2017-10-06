/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    ioctl.h

Abstract:

    This header contains definitions for sending and receiving IOCTLs to
    file descriptors.

Author:

    Evan Green 5-Nov-2013

--*/

#ifndef _SYS_IOCTL_H
#define _SYS_IOCTL_H

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
// Define terminal ioctls.
//

//
// This ioctl is equivalent to tcgetattr(). It takes a struct termios pointer
// as its argument.
//

#define TCGETS 0x7401

//
// This ioctl is equivalent to tcsetattr(fd, TCSANOW, arg). It takes a struct
// termios pointer as its argument.
//

#define TCSETS 0x7402

//
// This ioctl is equivalent to tcsetattr(fd, TCSADRAIN, arg). It takes a
// struct termios pointer as its argument.
//

#define TCSETSW 0x7403

//
// This ioctl is equivalent to tcsetattr(fd, TCSAFLUSH, arg). It takes a
// struct termios pointer as its argument.
//

#define TCSETSF 0x7404

//
// The next four ioctls are just like TCGETS, TCSETS, TCSETSW, and TCSETSF,
// except that they take a struct termio pointer instead of a struct termios
// pointer.
//

#define TCGETA 0x7405
#define TCSETA 0x7406
#define TCSETAW 0x7407
#define TCSETAF 0x7408

//
// This ioctl is equivalent to tcsendbreak(fd, arg). It takes an integer
// argument, which when zero sends a break (stream of zero bits) for between
// 0.25 and 0.5 seconds. When the argument is non-zero, the results are
// undefined. In this implementation, it is treated as tcdrain(fd).
//

#define TCSBRK 0x7409

//
// This ioctl is equivalent to tcflow(fd, arg). It takes an integer argument.
//

#define TCXONC 0x740A

//
// This ioctl is equivalent to tcflush(fd, arg). It takes an integer argument.
//

#define TCFLSH 0x740B

//
// This ioctl puts the terminal into exclusive mode. All further open
// operations will fail with EBUSY, except if the caller is root. This ioctl
// takes no argument.
//

#define TIOCEXCL 0x740C

//
// This ioctl disables exclusive mode. It takes no argument.
//

#define TIOCNXCL 0x740D

//
// This ioctl makes the given terminal the controlling terminal of the calling
// process. The calling process must be a session leader and must not already
// have a controlling terminal. If this terminal is already the controlling
// terminal of a different session then the ioctl fails with EPERM, unless
// the caller is root and the integer argument is one.
//

#define TIOCSCTTY 0x740E

//
// This ioctl is equivalent to *arg = tcgetpgrp(fd). The argument is a pid_t
// pointer.
//

#define TIOCGPGRP 0x740F

//
// This ioctl is equivalent to tcsetpgrp(fd, *argp). The argument is a pointer
// to a pid_t.
//

#define TIOCSPGRP 0x7410

//
// This ioctl returns the number of bytes in the output buffer. The argument
// is a pointer to an integer.
//

#define TIOCOUTQ 0x7411

//
// This ioctl inserts the given byte in the input queue. The argument is a
// pointer to a char.
//

#define TIOCSTI 0x7412

//
// This ioctl gets the window size. The argument is a pointer to a struct
// winsize.
//

#define TIOCGWINSZ 0x7413

//
// This ioctl sets the window size. The argument is a pointer to a struct
// winsize.
//

#define TIOCSWINSZ 0x7414

//
// This ioctl gets the modem status bits. The argument is a pointer to an int.
//

#define TIOCMGET 0x7415

//
// This ioctl ORs in the given modem status bits. The argument is a pointer to
// and int.
//

#define TIOCMBIS 0x7416

//
// This ioctl clears the given modem status bits. The argument is a pointer to
// an int.
//

#define TIOCMBIC 0x7417

//
// This ioctl sets the modem status bits. The argument is a pointer to an int.
//

#define TIOCMSET 0x7418

//
// This ioctl gets the software carrier flag. It gets the status of the CLOCAL
// flag in the c_cflag field of the termios structure. The argument is a
// pointer to an int.
//

#define TIOCGSOFTCAR 0x7419

//
// This ioctl sets the software carrier flag. The argument is a pointer to an
// int. If the CLOCAL flag for a line is off, the hardware carrier detect (DCD)
// signal is significant. An open of the corresponding terminal will block
// until DCD is asserted, unless the O_NONBLOCK flag is given. If CLOCAL is
// set, the line behaves as if DCD is always asserted. If the value at the
// given argument is non-zero, set the CLOCAL flag, otherwise clear the CLOCAL
// flag.
//

#define TIOCSSOFTCAR 0x741A

//
// This ioctl returns the number of bytes in the input buffer. The argument is
// a pointer to an int.
//

#define FIONREAD 0x741B
#define TIOCINQ FIONREAD

//
// This ioctl redirects output that would have gone to the video console or
// primary terminal to the given terminal. The caller must be root to do this.
// This ioctl takes no argument.
//

#define TIOCCONS 0x741D

//
// This ioctl enables or disables packet mode. The argument is a pointer to an
// int which enables (int is non-zero) or disables (int is zero) packet mode.
// In packet mode, each subsequent read of the master side returns a single
// non-zero control byte, and optionally data written by the slave side of the
// terminal. See TIOCPKT_* definitions for possible values and meanings of the
// first control byte.
//

#define TIOCPKT 0x7420

//
// This ioctl enables or disables non-blocking mode on the given descriptor. It
// is the same as using fcntl to enable non-blocking mode. The argument is an
// int, which if zero disables non-blocking mode, or otherwise enables it.
//

#define FIONBIO 0x7421

//
// This ioctl gives up the controlling terminal if the given terminal was
// the controlling terminal of the calling process. If this process was the
// session leader, SIGHUP and SIGCONT are sent to the foreground process group
// and all processes in the current session lose their controlling terminal.
// This ioctl takes no argument.
//

#define TIOCNOTTY 0x7422

//
// This ioctl is the POSIX version of TCSBRK. It treats a non-zero argument
// as a time interval measured in deciseconds, and does nothing when the driver
// does not support breaks. Like TCSBRK, it takes an integer argument.
//

#define TCSBRKP 0x7425

//
// This ioctl turns break on (starts sending zero bits). It takes no arguments.
//

#define TIOCSBRK 0x7427

//
// This ioctl turns break off (stops sending zero bits). It takes no arguments.
//

#define TIOCCBRK 0x7428

//
// This ioctl returns the session ID of the given terminal. This will fail
// with ENOTTY if the terminal is not a master pseudoterminal and not the
// calling process' controlling terminal. The argument is a pointer to a pid_t.
//

#define TIOCGSID 0x7429

//
// This ioctl sets the async mode on the given descriptor. It is the same as
// using fcntl to enable asynchronous mode. The argument is an int, which if
// zero disables asynchronous mode, or otherwise enables it.
//

#define FIOASYNC 0x7452

//
// Define the possible bits for the control byte in packet mode.
//

//
// This byte is returned when there's data to be read (or no data).
//

#define TIOCPKT_DATA 0x00

//
// This flag is returned when the read queue for the terminal is flushed.
//

#define TIOCPKT_FLUSHREAD 0x01

//
// This flag is returned when the write queue for the terminal is flushed.
//

#define TIOCPKT_FLUSHWRITE 0x02

//
// This flag is returned when output to the terminal is stopped.
//

#define TIOCPKT_STOP 0x04

//
// This flag is returned when output to the terminal is restarted.
//

#define TIOCPKT_START 0x08

//
// This flag is returned when the start and stop characters are not ^S and ^Q.
//

#define TIOCPKT_NOSTOP 0x10

//
// This flag is returned when the start and stop characters are ^S and ^Q.
//

#define TIOCPKT_DOSTOP 0x20

//
// This flag is returned when an IOCTL has come in and changed the state of
// the terminal.
//

#define TIOCPKT_IOCTL 0x40

//
// Define modem status bits.
//

//
// Data set ready (line enable).
//

#define TIOCM_LE 0x0001

//
// Data terminal ready.
//

#define TIOCM_DTR 0x0002

//
// Request to send.
//

#define TIOCM_RTS 0x0004

//
// Secondary transmit.
//

#define TIOCM_ST 0x0008

//
// Secondary receive.
//

#define TIOCM_SR 0x0010

//
// Clear to send.
//

#define TIOCM_CTS 0x0020

//
// Data carrier detect (DCD).
//

#define TIOCM_CAR 0x0040
#define TIOCM_CD TIOCM_CAR

//
// Ring.
//

#define TIOCM_RNG 0x0080
#define TIOCM_RI TIOCM_RNG

//
// Data Set Ready.
//

#define TIOCM_DSR 0x0100

//
// Define the size of the control character array for the old termio structure.
//

#define NCC 8

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the old format of the terminal behavior and settings.
    New applications should use struct termios in termios.h.

Members:

    c_iflag - Stores the input mode flags.

    c_oflag - Stores the output mode flags.

    c_cflag - Stores the control mode flags.

    c_lflag - Stores the local control flags.

    c_line - Stores the line discipline.

    c_cc - Stores the characters to use for control characters.

    c_ispeed - Stores the input baud rate.

    c_ospeed - Stores the output baud rate.

--*/

struct termio {
    unsigned short int c_iflag;
    unsigned short int c_oflag;
    unsigned short int c_cflag;
    unsigned short int c_lflag;
    unsigned char c_line;
    unsigned char c_cc[NCC];
};

/*++

Structure Description:

    This structure stores the window size structure passed by the TIOCGWINSZ
    and TIOCSWINSZ ioctls.

Members:

    ws_row - Stores the count of rows in the window.

    ws_col - Stores the count of columns in the window.

    ws_xpixel - Stores an unused value. Defined for compatibility sake.

    ws_ypixel - Stores an unused value. Defined for compatilibity sake.

--*/

struct winsize {
    unsigned short int ws_row;
    unsigned short int ws_col;
    unsigned short int ws_xpixel;
    unsigned short int ws_ypixel;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
ioctl (
    int FileDescriptor,
    int Request,
    ...
    );

/*++

Routine Description:

    This routine sends an I/O control request to the given file descriptor.

Arguments:

    FileDescriptor - Supplies the file descriptor to send the control request
        to.

    Request - Supplies the numeric request to send. This is device-specific.

    ... - Supplies a variable number of arguments for historical reasons, but
        this routine expects there to be exactly one more argument, a pointer
        to memory. The size of this memory is request-specific, but can be no
        larger than 4096 bytes. The native version of this routine can specify
        larger values.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

