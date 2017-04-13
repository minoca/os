/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stty.c

Abstract:

    This module implements the stty utility.

Author:

    Evan Green 24-May-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <minoca/lib/tty.h>

#include "swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts a character into a control character.
//

#define STTY_CONTROL(_Character) ((_Character) & 0x1F)

//
// ---------------------------------------------------------------- Definitions
//

#define STTY_VERSION_MAJOR 1
#define STTY_VERSION_MINOR 0

#define STTY_USAGE                                                             \
    "usage: stty [-F device] [-a|-g] [settings...]\n"                          \
    "The stty utility changes attributes of the terminal. Options are:\n"      \
    "  -a, --all -- Print all current settings in human-readable form.\n"      \
    "  -F, --file=device -- Operate on the given device instead of stdin.\n"   \
    "  -g, --save -- Print all current settings in a utility-specific form \n" \
    "      that can be sent back to stty.\n"                                   \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define STTY_OPTIONS_STRING "agF:hV"

//
// Set this flag to print all options in a human readable format.
//

#define STTY_OPTION_ALL_HUMAN 0x00000001

//
// Set this flag to print all options in an stty-readable format.
//

#define STTY_OPTION_ALL_MACHINE 0x00000002
#define STTY_OPTION_PRINT_MASK (STTY_OPTION_ALL_HUMAN | STTY_OPTION_ALL_MACHINE)

//
// This flag is set if an option cannot be negated with '-'.
//

#define STTY_NO_NEGATE 0x00000001

//
// This flag is set if the option is not displayed by default.
//

#define STTY_HIDDEN 0x00000002

//
// This flag is set if sane mode enables this option.
//

#define STTY_SANE_SET 0x00000004

//
// This flag is set if sane mode clears/negates this option.
//

#define STTY_SANE_CLEAR 0x00000008

#define STTY_DEFAULT_INTR STTY_CONTROL('C')
#define STTY_DEFAULT_ERASE 127
#define STTY_DEFAULT_KILL STTY_CONTROL('U')

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _STTY_TERMIOS_MEMBER {
    SttyTermiosInvalid,
    SttyTermiosInput,
    SttyTermiosOutput,
    SttyTermiosControl,
    SttyTermiosLocal,
    SttyTermiosCharacter,
    SttyTermiosTime,
    SttyTermiosCombination
} STTY_TERMIOS_MEMBER, *PSTTY_TERMIOS_MEMBER;

/*++

Structure Description:

    This structure stores an stty flag option that can be set or cleared.

Members:

    Name - Stores the name of the option.

    Member - Stores the member within the termios structure where this goes.

    Value - Stores the flags to set for this member. For characters, this
        stores the offset into the c_cc member of termios.

    Mask - Stores the mask of bits that get affected by this operation (for
        multibit fields). For characters, this stores the sane value.

    Flags - Stores attributes about this option. See STTY_* definitions.

--*/

typedef struct _STTY_MEMBER {
    PSTR Name;
    STTY_TERMIOS_MEMBER Member;
    tcflag_t Value;
    tcflag_t Mask;
    ULONG Flags;
} STTY_MEMBER, *PSTTY_MEMBER;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SttySetOption (
    struct termios *Termios,
    PSTTY_MEMBER Member,
    BOOL Negated
    );

VOID
SttySetBaudRate (
    struct termios *Termios,
    PSTR String,
    BOOL Input,
    BOOL Output
    );

VOID
SttySetWindowSize (
    struct winsize *WindowSize,
    PSTR String,
    BOOL Row
    );

BOOL
SttyLoadMachineSettings (
    struct termios *Termios,
    struct winsize *WindowSize,
    PSTR String
    );

VOID
SttyPrintAll (
    struct termios *Termios,
    struct winsize *WindowSize
    );

VOID
SttyPrintMachine (
    struct termios *Termios,
    struct winsize *WindowSize
    );

VOID
SttyPrintDelta (
    struct termios *Termios,
    struct winsize *WindowSize
    );

VOID
SttyPrintBaudRate (
    struct termios *Termios,
    BOOL Short
    );

ULONG
SttyConvertBaudValueToRate (
    speed_t Value
    );

speed_t
SttyConvertRateToBaudValue (
    PSTR String
    );

VOID
SttyPrintCharacter (
    cc_t Character
    );

VOID
SttySanitizeSettings (
    struct termios *Termios
    );

tcflag_t *
SttyGetTermiosMember (
    struct termios *Termios,
    STTY_TERMIOS_MEMBER MemberType
    );

VOID
SttySetCharacter (
    struct termios *Termios,
    PSTTY_MEMBER Member,
    PSTR Argument
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SttyLongOptions[] = {
    {"all", no_argument, 0, 'a'},
    {"file", no_argument, 0, 'F'},
    {"save", no_argument, 0, 'g'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

STTY_MEMBER SttyOptions[] = {
    {"ignbreak", SttyTermiosInput, IGNBRK, 0, STTY_SANE_CLEAR},
    {"brkint", SttyTermiosInput, BRKINT, 0, STTY_SANE_SET},
    {"ignpar", SttyTermiosInput, IGNPAR, 0, 0},
    {"parmrk", SttyTermiosInput, PARMRK, 0, 0},
    {"inpck", SttyTermiosInput, INPCK, 0, 0},
    {"istrip", SttyTermiosInput, ISTRIP, 0, 0},
    {"inlcr", SttyTermiosInput, INLCR, 0, STTY_SANE_CLEAR},
    {"igncr", SttyTermiosInput, IGNCR, 0, STTY_SANE_CLEAR},
    {"icrnl", SttyTermiosInput, ICRNL, 0, STTY_SANE_SET},
    {"ixon", SttyTermiosInput, IXON, 0, 0},
    {"ixoff", SttyTermiosInput, IXOFF, 0, STTY_SANE_CLEAR},
    {"ixany", SttyTermiosInput, IXANY, 0, STTY_SANE_CLEAR},
    {"imaxbel", SttyTermiosInput, IMAXBEL, 0, STTY_SANE_SET},
    {"opost", SttyTermiosOutput, OPOST, 0, STTY_SANE_SET},
    {"ocrnl", SttyTermiosOutput, OCRNL, 0, STTY_SANE_CLEAR},
    {"onlcr", SttyTermiosOutput, ONLCR, 0, STTY_SANE_SET},
    {"onocr", SttyTermiosOutput, ONOCR, 0, STTY_SANE_CLEAR},
    {"onlret", SttyTermiosOutput, ONLRET, 0, STTY_SANE_CLEAR},
#ifdef OFILL
    {"ofill", SttyTermiosOutput, OFILL, 0, STTY_SANE_CLEAR},
#endif
#ifdef OFDEL
    {"ofdel", SttyTermiosOutput, OFDEL, 0, STTY_SANE_CLEAR},
#endif
#ifdef CR0
    {"cr0", SttyTermiosOutput, CR0, CRDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#endif
#ifdef CR1
    {"cr1", SttyTermiosOutput, CR1, CRDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef CR2
    {"cr2", SttyTermiosOutput, CR2, CRDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef CR3
    {"cr3", SttyTermiosOutput, CR3, CRDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef NL0
    {"nl0", SttyTermiosOutput, NL0, NLDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#endif
#ifdef NL1
    {"nl1", SttyTermiosOutput, NL1, NLDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
    {"tab0", SttyTermiosOutput, TAB0, TABDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#ifdef TAB1
    {"tab1", SttyTermiosOutput, TAB1, TABDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef TAB2
    {"tab2", SttyTermiosOutput, TAB2, TABDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
    {"tab3", SttyTermiosOutput, TAB3, TABDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
    {"tabs", SttyTermiosOutput, TAB0, TABDLY, STTY_NO_NEGATE | STTY_HIDDEN},
#ifdef BS0
    {"bs0", SttyTermiosOutput, BS0, BSDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#endif
#ifdef BS1
    {"bs1", SttyTermiosOutput, BS1, BSDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef FF0
    {"ff0", SttyTermiosOutput, FF0, FFDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#endif
#ifdef FF1
    {"ff1", SttyTermiosOutput, FF1, FFDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
#ifdef VT0
    {"vt0", SttyTermiosOutput, VT0, VTDLY, STTY_NO_NEGATE | STTY_SANE_SET},
#endif
#ifdef VT1
    {"vt1", SttyTermiosOutput, VT1, VTDLY, STTY_NO_NEGATE | STTY_SANE_CLEAR},
#endif
    {"parenb", SttyTermiosControl, PARENB, 0, 0},
    {"parodd", SttyTermiosControl, PARODD, 0, 0},
    {"cs5", SttyTermiosControl, CS5, CSIZE, STTY_NO_NEGATE},
    {"cs6", SttyTermiosControl, CS6, CSIZE, STTY_NO_NEGATE},
    {"cs7", SttyTermiosControl, CS7, CSIZE, STTY_NO_NEGATE},
    {"cs8", SttyTermiosControl, CS8, CSIZE, STTY_NO_NEGATE},
    {"hupcl", SttyTermiosControl, HUPCL, 0, 0},
    {"hup", SttyTermiosControl, HUPCL, 0, STTY_HIDDEN},
    {"cstopb", SttyTermiosControl, CSTOPB, 0, 0},
    {"cread", SttyTermiosControl, CREAD, 0, STTY_SANE_SET},
    {"clocal", SttyTermiosControl, CLOCAL, 0, 0},
    {"isig", SttyTermiosLocal, ISIG, 0, STTY_SANE_SET},
    {"icanon", SttyTermiosLocal, ICANON, 0, STTY_SANE_SET},
    {"iexten", SttyTermiosLocal, IEXTEN, 0, STTY_SANE_SET},
    {"echo", SttyTermiosLocal, ECHO, 0, STTY_SANE_SET},
    {"echoe", SttyTermiosLocal, ECHOE, 0, STTY_SANE_SET},
    {"echok", SttyTermiosLocal, ECHOK, 0, STTY_SANE_SET},
    {"echoke", SttyTermiosLocal, ECHOKE, 0, STTY_SANE_SET},
    {"echonl", SttyTermiosLocal, ECHONL, 0, STTY_SANE_CLEAR},
    {"noflsh", SttyTermiosLocal, NOFLSH, 0, STTY_SANE_CLEAR},
    {"tostop", SttyTermiosLocal, TOSTOP, 0, STTY_SANE_CLEAR},
    {"eof", SttyTermiosCharacter, VEOF, STTY_CONTROL('D'), 0},
    {"eol", SttyTermiosCharacter, VEOL, _POSIX_VDISABLE, 0},
    {"erase", SttyTermiosCharacter, VERASE, STTY_DEFAULT_ERASE, 0},
    {"intr", SttyTermiosCharacter, VINTR, STTY_DEFAULT_INTR, 0},
    {"kill", SttyTermiosCharacter, VKILL, STTY_DEFAULT_KILL, 0},
    {"quit", SttyTermiosCharacter, VQUIT, 28, 0},
    {"susp", SttyTermiosCharacter, VSUSP, STTY_CONTROL('Z'), 0},
    {"start", SttyTermiosCharacter, VSTART, STTY_CONTROL('Q'), 0},
    {"stop", SttyTermiosCharacter, VSTOP, STTY_CONTROL('S'), 0},
    {"min", SttyTermiosTime, VMIN, 1, 0},
    {"time", SttyTermiosTime, VTIME, 0, 0},
    {"evenp", SttyTermiosCombination, 0, 0},
    {"parity", SttyTermiosCombination, 0, 0},
    {"oddp", SttyTermiosCombination, 0, 0},
    {"nl", SttyTermiosCombination, 0, 0},
    {"ek", SttyTermiosCombination, 0, STTY_NO_NEGATE},
    {"sane", SttyTermiosCombination, 0, STTY_NO_NEGATE},
    {"cooked", SttyTermiosCombination, 0, 0},
    {"raw", SttyTermiosCombination, 0, 0},
    {"pass8", SttyTermiosCombination, 0, 0},
    {"litout", SttyTermiosCombination, 0, 0},
    {"cbreak", SttyTermiosCombination, 0, 0},
    {"decctlq", SttyTermiosCombination, 0, 0},
    {"crt", SttyTermiosCombination, 0, STTY_NO_NEGATE},
    {"dec", SttyTermiosCombination, 0, STTY_NO_NEGATE},
    {NULL, SttyTermiosInvalid, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
SttyMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the stty utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    struct termios ConfirmedMode;
    PSTTY_MEMBER Member;
    BOOL Negated;
    INT Option;
    ULONG Options;
    int Status;
    int Terminal;
    struct termios Termios;
    struct winsize WindowSize;

    Options = 0;
    Terminal = -1;

    //
    // Don't print error message for unknown options (as getopt would see
    // something like -echo as -e -c -h -o.
    //

    opterr = 0;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             STTY_OPTIONS_STRING,
                             SttyLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            break;
        }

        switch (Option) {
        case 'a':
            Options |= STTY_OPTION_ALL_HUMAN;
            break;

        case 'g':
            Options |= STTY_OPTION_ALL_MACHINE;
            break;

        case 'F':
            Terminal = SwOpen(optarg, O_RDONLY | O_NONBLOCK, 0);
            if (Terminal < 0) {
                Status = 1;
                SwPrintError(errno, optarg, "Unable to open");
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(STTY_VERSION_MAJOR, STTY_VERSION_MINOR);
            return 1;

        case 'h':
            printf(STTY_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    if (((Options & STTY_OPTION_ALL_HUMAN) != 0) &&
        ((Options & STTY_OPTION_ALL_MACHINE) != 0)) {

        SwPrintError(0, NULL, "-a and -g cannot be specified together");
        Status = 1;
        goto MainEnd;
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if ((ArgumentIndex != ArgumentCount) &&
        ((Options & STTY_OPTION_PRINT_MASK) != 0)) {

        SwPrintError(0, NULL, "Options cannot be specified when printing");
        Status = 1;
        goto MainEnd;
    }

    //
    // If the terminal was not explicitly set, try to open it.
    //

    if (Terminal < 0) {
        Terminal = SwOpen("/dev/tty", O_RDWR, 0);
        if (Terminal < 0) {
            Terminal = STDIN_FILENO;
        }
    }

    Status = tcgetattr(Terminal, &Termios);
    if (Status < 0) {
        SwPrintError(errno, NULL, "Unable to get terminal attributes");
        Status = 1;
        goto MainEnd;
    }

    Status = ioctl(Terminal, TIOCGWINSZ, &WindowSize);
    if (Status < 0) {
        SwPrintError(errno, NULL, "Warning: Unable to get window size");
        memset(&WindowSize, 0, sizeof(WindowSize));
    }

    //
    // If there are no arguments, the current values and exit.
    //

    if (ArgumentIndex == ArgumentCount) {
        if ((Options & STTY_OPTION_ALL_HUMAN) != 0) {
            SttyPrintAll(&Termios, &WindowSize);

        } else if ((Options & STTY_OPTION_ALL_MACHINE) != 0) {
            SttyPrintMachine(&Termios, &WindowSize);

        } else {
            SttyPrintDelta(&Termios, &WindowSize);
        }

        Status = 0;
        goto MainEnd;
    }

    //
    // Loop through and operate on all the arguments.
    //

    while (ArgumentIndex != ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;
        Negated = FALSE;
        if (*Argument == '-') {
            Negated = TRUE;
            Argument += 1;
        }

        Status = TRUE;
        Member = SttyOptions;
        while (Member->Name != NULL) {
            if (strcmp(Argument, Member->Name) == 0) {

                //
                // Handle a c_cc character.
                //

                if ((Member->Member == SttyTermiosCharacter) ||
                    (Member->Member == SttyTermiosTime)) {

                    if (ArgumentIndex == ArgumentCount) {
                        SwPrintError(0, Argument, "Missing operand");
                        Status = 1;
                        goto MainEnd;
                    }

                    if (Negated != FALSE) {
                        SwPrintError(0, NULL, "Cannot negate character");
                        Status = 1;
                        goto MainEnd;
                    }

                    Argument = Arguments[ArgumentIndex];
                    ArgumentIndex += 1;
                    SttySetCharacter(&Termios, Member, Argument);

                //
                // Handle a named option or combination.
                //

                } else {
                    Status = SttySetOption(&Termios, Member, Negated);
                }

                break;
            }

            Member += 1;
        }

        if (Status == FALSE) {
            SwPrintError(0, Argument, "Argument cannot be negated");
            Status = 1;
            goto MainEnd;
        }

        //
        // Avoid the rest of the searching if the previous loop matched.
        //

        if (Member->Name != NULL) {
            continue;
        }

        //
        // Try to get a speed out of the argument.
        //

        if (SttyConvertRateToBaudValue(Argument) != (speed_t)-1) {
            SttySetBaudRate(&Termios, Argument, TRUE, TRUE);
            continue;
        }

        //
        // Try to get the entire mode as a machine readable mode.
        //

        if (SttyLoadMachineSettings(&Termios, &WindowSize, Argument) != FALSE) {
            continue;
        }

        //
        // Print out the window size if requested.
        //

        if (strcmp(Argument, "size") == 0) {
            printf("%lu %lu\n",
                   (unsigned long)(WindowSize.ws_row),
                   (unsigned long)(WindowSize.ws_col));

            continue;
        }

        //
        // Print out the speed if requested.
        //

        if (strcmp(Argument, "speed") == 0) {
            SttyPrintBaudRate(&Termios, TRUE);
            continue;
        }

        //
        // For everything else (all the speeds and sizes) an argument is
        // required.
        //

        if (ArgumentIndex == ArgumentCount) {
            SwPrintError(0, Argument, "Invalid argument");
            Status = 1;
            goto MainEnd;
        }

        if (strcmp(Argument, "ispeed") == 0) {
            SttySetBaudRate(&Termios, Arguments[ArgumentIndex], TRUE, FALSE);

        } else if (strcmp(Argument, "ospeed") == 0) {
            SttySetBaudRate(&Termios, Arguments[ArgumentIndex], FALSE, TRUE);

        } else if (strcmp(Argument, "rows") == 0) {
            SttySetWindowSize(&WindowSize, Arguments[ArgumentIndex], TRUE);

        } else if (strcmp(Argument, "cols") == 0) {
            SttySetWindowSize(&WindowSize, Arguments[ArgumentIndex], FALSE);

        } else {
            SwPrintError(0, Argument, "Invalid argument");
            Status = 1;
            goto MainEnd;
        }

        //
        // Skip over the extra argument that was the speed/size.
        //

        ArgumentIndex += 1;
    }

    Status = tcsetattr(Terminal, TCSADRAIN, &Termios);
    if (Status < 0) {
        SwPrintError(errno, NULL, "Failed to set terminal attributes");
        Status = 1;
        goto MainEnd;
    }

    Status = ioctl(Terminal, TIOCSWINSZ, &WindowSize);
    if (Status < 0) {
        SwPrintError(errno, NULL, "Warning: Failed to set window size");
    }

    memset(&ConfirmedMode, 0, sizeof(ConfirmedMode));
    Status = tcgetattr(Terminal, &ConfirmedMode);
    if (Status < 0) {
        SwPrintError(errno, NULL, "Unable to get terminal attributes");
        Status = 1;
        goto MainEnd;
    }

    if (memcmp(&Termios, &ConfirmedMode, sizeof(struct termios)) != 0) {
        SwPrintError(0, NULL, "Unable to set all attributes");
        printf("Desired attributes: ");
        SttyPrintMachine(&Termios, &WindowSize);
        printf("Actual attributes:  ");
        SttyPrintMachine(&ConfirmedMode, &WindowSize);
        Status = 1;

    } else {
        Status = 0;
    }

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SttySetOption (
    struct termios *Termios,
    PSTTY_MEMBER Member,
    BOOL Negated
    )

/*++

Routine Description:

    This routine attempts to apply the given argument.

Arguments:

    Termios - Supplies a pointer to the terminal settings.

    Member - Supplies the member to set.

    Negated - Supplies a boolean indicating if the option was negated or not.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    tcflag_t *Field;
    tcflag_t Mask;
    PSTR Name;

    if ((Negated != FALSE) && ((Member->Flags & STTY_NO_NEGATE) != 0)) {
        return FALSE;
    }

    //
    // Handle combination platters.
    //

    Name = Member->Name;
    if (Member->Member == SttyTermiosCombination) {
        if ((strcmp(Name, "evenp") == 0) || (strcmp(Name, "parity") == 0)) {
            if (Negated != FALSE) {
                Termios->c_cflag &= ~(PARENB | CSIZE);
                Termios->c_cflag |= CS8;

            } else {
                Termios->c_cflag &= ~(PARODD | CSIZE);
                Termios->c_cflag |= CS7;
            }

        } else if (strcmp(Name, "oddp") == 0) {
            if (Negated != FALSE) {
                Termios->c_cflag &= ~(PARENB | CSIZE);
                Termios->c_cflag |= CS8;

            } else {
                Termios->c_cflag &= ~CSIZE;
                Termios->c_cflag |= PARODD | PARENB | CS7;
            }

        } else if (strcmp(Name, "nl") == 0) {
            if (Negated != FALSE) {
                Termios->c_iflag &= ~(INLCR | IGNCR);
                Termios->c_iflag |= ICRNL;
                Termios->c_oflag &= ~(OCRNL | ONLRET);
                Termios->c_oflag |= ONLCR;

            } else {
                Termios->c_iflag &= ~ICRNL;
                Termios->c_oflag &= ~ONLCR;
            }

        } else if (strcmp(Name, "ek") == 0) {
            Termios->c_cc[VERASE] = STTY_DEFAULT_ERASE;
            Termios->c_cc[VKILL] = STTY_DEFAULT_KILL;

        } else if (strcmp(Name, "sane") == 0) {
            SttySanitizeSettings(Termios);

        } else if (strcmp(Name, "cbreak") == 0) {
            if (Negated != FALSE) {
                Termios->c_lflag |= ICANON;

            } else {
                Termios->c_lflag &= ~ICANON;
            }

        } else if (strcmp(Name, "pass8") == 0) {
            if (Negated != FALSE) {
                Termios->c_cflag &= ~CSIZE;
                Termios->c_cflag |= PARENB | CS7;
                Termios->c_iflag |= ISTRIP;

            } else {
                Termios->c_cflag &= ~(PARENB | CSIZE);
                Termios->c_cflag |= CS8;
                Termios->c_iflag &= ~ISTRIP;
            }

        } else if (strcmp(Name, "litout") == 0) {
            if (Negated != FALSE) {
                Termios->c_cflag &= ~CSIZE;
                Termios->c_cflag |= PARENB | CS7;
                Termios->c_iflag |= ISTRIP;
                Termios->c_oflag |= OPOST;

            } else {
                Termios->c_cflag &= ~(PARENB | CSIZE);
                Termios->c_cflag |= CS8;
                Termios->c_iflag &= ~ISTRIP;
                Termios->c_oflag &= ~OPOST;
            }

        } else if ((strcmp(Name, "raw") == 0) ||
                   (strcmp(Name, "cooked") == 0)) {

            //
            // Set cooked mode.
            //

            if (((*Name == 'c') && (Negated == FALSE)) ||
                ((*Name == 'r') && (Negated != FALSE))) {

                Termios->c_iflag |= BRKINT | IGNPAR | ISTRIP | ICRNL | IXON;
                Termios->c_oflag |= OPOST;
                Termios->c_lflag |= ISIG | ICANON;

            //
            // Set raw mode.
            //

            } else {
                Termios->c_iflag = 0;
                Termios->c_oflag &= ~OPOST;
                Termios->c_lflag &= ~(ISIG | ICANON);
                Termios->c_cc[VMIN] = 1;
                Termios->c_cc[VTIME] = 0;
            }

        } else if (strcmp(Name, "decctlq") == 0) {
            if (Negated != FALSE) {
                Termios->c_iflag |= IXANY;

            } else {
                Termios->c_iflag &= ~IXANY;
            }

        } else if (strcmp(Name, "crt") == 0) {
            Termios->c_lflag |= ECHOE | ECHOCTL | ECHOKE;

        } else if (strcmp(Name, "dec") == 0) {
            Termios->c_cc[VINTR] = STTY_DEFAULT_INTR;
            Termios->c_cc[VERASE] = STTY_DEFAULT_ERASE;
            Termios->c_cc[VKILL] = STTY_DEFAULT_KILL;
            Termios->c_lflag |= ECHOE | ECHOCTL | ECHOKE;
            Termios->c_iflag &= ~IXANY;
        }

    } else {
        Field = SttyGetTermiosMember(Termios, Member->Member);
        if (Field != NULL) {
            Mask = Member->Mask;
            if (Negated != FALSE) {
                *Field &= ~Mask;
                *Field &= ~(Member->Value);

            } else {
                *Field &= ~Mask;
                *Field |= Member->Value;
            }
        }
    }

    return TRUE;
}

VOID
SttySetBaudRate (
    struct termios *Termios,
    PSTR String,
    BOOL Input,
    BOOL Output
    )

/*++

Routine Description:

    This routine attempts to set the given baud rate in the given terminal
    settings.

Arguments:

    Termios - Supplies a pointer to the terminal settings.

    String - Supplies the string baud rate argument.

    Input - Supplies a boolean indicating whether or not to set the input
        speed.

    Output - Supplies a boolean indicating whether or not to set the output
        speed.

Return Value:

    None.

--*/

{

    speed_t Value;

    Value = SttyConvertRateToBaudValue(String);
    if (Value == (speed_t)-1) {
        return;
    }

    if (Input != FALSE) {
        cfsetispeed(Termios, Value);
    }

    if (Output != FALSE) {
        cfsetospeed(Termios, Value);
    }

    return;
}

VOID
SttySetWindowSize (
    struct winsize *WindowSize,
    PSTR String,
    BOOL Row
    )

/*++

Routine Description:

    This routine attempts to set the given window size in the given structure.

Arguments:

    WindowSize - Supplies a pointer to the window size structure.

    String - Supplies a pointer to the value string.

    Row - Supplies a boolean indicating whether to set the rows (TRUE) or
        columns (FALSE).

Return Value:

    None.

--*/

{

    LONG Value;

    Value = strtoul(String, NULL, 10);
    if (Value < 0) {
        return;
    }

    if (Row != FALSE) {
        WindowSize->ws_row = Value;

    } else {
        WindowSize->ws_col = Value;
    }

    return;
}

BOOL
SttyLoadMachineSettings (
    struct termios *Termios,
    struct winsize *WindowSize,
    PSTR String
    )

/*++

Routine Description:

    This routine attempts to load settings previously printed by this utility.

Arguments:

    Termios - Supplies a pointer to the terminal settings.

    WindowSize - Supplies a pointer to the window size to set.

    String - Supplies the settings argument.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AfterScan;
    tcflag_t Flags[4];
    ULONG Index;

    for (Index = 0; Index < 4; Index += 1) {
        Flags[Index] = strtoul(String, &AfterScan, 16);
        if (*AfterScan != ':') {
            return FALSE;
        }

        String = AfterScan + 1;
    }

    Termios->c_iflag = Flags[0];
    Termios->c_oflag = Flags[1];
    Termios->c_cflag = Flags[2];
    Termios->c_lflag = Flags[3];
    for (Index = 0; Index < NCCS; Index += 1) {
        Termios->c_cc[Index] = strtoul(String, &AfterScan, 16);
        if (*AfterScan != ':') {
            return FALSE;
        }

        String = AfterScan + 1;
    }

    WindowSize->ws_row = strtoul(String, &AfterScan, 16);
    if (*AfterScan != ':') {
        return FALSE;
    }

    String = AfterScan + 1;
    WindowSize->ws_col = strtoul(String, &AfterScan, 16);
    if (*AfterScan != '\0') {
        return FALSE;
    }

    return TRUE;
}

VOID
SttyPrintAll (
    struct termios *Termios,
    struct winsize *WindowSize
    )

/*++

Routine Description:

    This routine displays all terminal settings in human readable form.

Arguments:

    Termios - Supplies a pointer to the current terminal settings.

    WindowSize - Supplies a pointer to the current window size.

Return Value:

    None.

--*/

{

    tcflag_t *Field;
    tcflag_t Mask;
    PSTTY_MEMBER Member;
    STTY_TERMIOS_MEMBER MemberType;
    ULONG Offset;

    SttyPrintBaudRate(Termios, FALSE);
    printf("rows %d; columns %d;\n", WindowSize->ws_row, WindowSize->ws_col);

    //
    // Print the characters and times.
    //

    Member = SttyOptions;
    while (Member->Name != NULL) {
        if (Member->Member == SttyTermiosCharacter) {
            Offset = Member->Value;
            printf("%s = ", Member->Name);
            SttyPrintCharacter(Termios->c_cc[Offset]);
            printf("; ");

        } else if (Member->Member == SttyTermiosTime) {
            Offset = Member->Value;
            printf("%s = %lu; ",
                   Member->Name,
                   (unsigned long)Termios->c_cc[Offset]);
        }

        Member += 1;
    }

    printf("\n");

    //
    // Print out all the flags.
    //

    Member = SttyOptions;
    MemberType = Member->Member;
    while (Member->Name != NULL) {
        if ((Member->Member >= SttyTermiosCharacter) ||
            ((Member->Flags & STTY_HIDDEN) != 0)) {

            Member += 1;
            continue;
        }

        //
        // Delineate the different fields with a newline.
        //

        if (Member->Member != MemberType) {
            MemberType = Member->Member;
            printf("\n");
        }

        Field = SttyGetTermiosMember(Termios, MemberType);
        if (Field == NULL) {
            break;
        }

        Mask = Member->Mask;
        if (Mask == 0) {
            Mask = Member->Value;
        }

        //
        // Print the field if it is set to the value.
        //

        if ((*Field & Mask) == Member->Value) {
            printf("%s ", Member->Name);

        //
        // Either don't print the field or print the negation.
        //

        } else {
            if ((Member->Flags & STTY_NO_NEGATE) == 0) {
                printf("-%s ", Member->Name);
            }
        }

        Member += 1;
    }

    printf("\n");
    return;
}

VOID
SttyPrintMachine (
    struct termios *Termios,
    struct winsize *WindowSize
    )

/*++

Routine Description:

    This routine displays all terminal settings in stty-readable form.

Arguments:

    Termios - Supplies a pointer to the current terminal settings.

    WindowSize - Supplies a pointer to the current window size.

Return Value:

    None.

--*/

{

    ULONG Index;

    printf("%lx:%lx:%lx:%lx",
           (unsigned long)(Termios->c_iflag),
           (unsigned long)(Termios->c_oflag),
           (unsigned long)(Termios->c_cflag),
           (unsigned long)(Termios->c_lflag));

    for (Index = 0; Index < NCCS; Index += 1) {
        printf(":%lx", (unsigned long)(Termios->c_cc[Index]));
    }

    printf(":%lx:%lx\n",
           (unsigned long)(WindowSize->ws_row),
           (unsigned long)(WindowSize->ws_col));

    return;
}

VOID
SttyPrintDelta (
    struct termios *Termios,
    struct winsize *WindowSize
    )

/*++

Routine Description:

    This routine displays the difference between the given settings and the
    sane settings, in human readable form.

Arguments:

    Termios - Supplies a pointer to the current terminal settings.

    WindowSize - Supplies a pointer to the current window size.

Return Value:

    None.

--*/

{

    tcflag_t *Field;
    tcflag_t Mask;
    PSTTY_MEMBER Member;
    STTY_TERMIOS_MEMBER MemberType;
    ULONG Offset;
    BOOL PrintedSomething;

    SttyPrintBaudRate(Termios, FALSE);
    printf("\n");

    //
    // Print any changes from the sane character values. The sane values are
    // stored in the mask for characters.
    //

    PrintedSomething = FALSE;
    Member = SttyOptions;
    while (Member->Name != NULL) {
        if (Member->Member == SttyTermiosCharacter) {
            Offset = Member->Value;
            if (Termios->c_cc[Offset] != Member->Mask) {
                PrintedSomething = TRUE;
                printf("%s = ", Member->Name);
                SttyPrintCharacter(Termios->c_cc[Offset]);
                printf("; ");
            }

        } else if (Member->Member == SttyTermiosTime) {
            if ((Termios->c_lflag & ICANON) == 0) {
                PrintedSomething = TRUE;
                Offset = Member->Value;
                printf("%s = %lu; ",
                       Member->Name,
                       (unsigned long)Termios->c_cc[Offset]);
            }
        }

        Member += 1;
    }

    if (PrintedSomething != FALSE) {
        printf("\n");
    }

    //
    // Go through again and print the flags that are different from what sane
    // enforces.
    //

    PrintedSomething = FALSE;
    Member = SttyOptions;
    MemberType = Member->Member;
    while (Member->Name != NULL) {
        if ((Member->Member >= SttyTermiosCharacter) ||
            ((Member->Flags & STTY_HIDDEN) != 0)) {

            Member += 1;
            continue;
        }

        //
        // Delineate the different fields with a newline.
        //

        if (Member->Member != MemberType) {
            MemberType = Member->Member;
            if (PrintedSomething != FALSE) {
                printf("\n");
                PrintedSomething = FALSE;
            }
        }

        Field = SttyGetTermiosMember(Termios, MemberType);
        if (Field == NULL) {
            break;
        }

        Mask = Member->Mask;
        if (Mask == 0) {
            Mask = Member->Value;
        }

        //
        // If the field has it set to the value and the sane flags would have
        // unset it, print out the setting.
        //

        if ((*Field & Mask) == Member->Value) {
            if ((Member->Flags & STTY_SANE_CLEAR) != 0) {
                PrintedSomething = TRUE;
                printf("%s ", Member->Name);
            }

        //
        // If the field is not set and the sane flags would have it set, print
        // the unsetting.
        //

        } else {
            if (((Member->Flags & STTY_SANE_SET) != 0) &&
                ((Member->Flags & STTY_NO_NEGATE) == 0)) {

                PrintedSomething = TRUE;
                printf("-%s ", Member->Name);
            }
        }

        Member += 1;
    }

    if (PrintedSomething != FALSE) {
        printf("\n");
    }

    return;
}

VOID
SttyPrintBaudRate (
    struct termios *Termios,
    BOOL Short
    )

/*++

Routine Description:

    This routine prints the baud rates given terminal settings.

Arguments:

    Termios - Supplies a pointer to the terminal settings to print.

    Short - Supplies a boolean indicating whether to just print the numbers
        (TRUE) or to print with text (FALSE).

Return Value:

    None.

--*/

{

    ULONG IRate;
    speed_t Ispeed;
    ULONG ORate;
    speed_t Ospeed;
    speed_t Speed;

    Ispeed = cfgetispeed(Termios);
    Ospeed = cfgetospeed(Termios);
    if ((Ispeed == Ospeed) || (Ispeed == B0) || (Ospeed == B0)) {
        Speed = Ispeed;
        if (Speed == B0) {
            Speed = Ospeed;
        }

        IRate = SttyConvertBaudValueToRate(Speed);
        if (Short != FALSE) {
            printf("%d\n", IRate);

        } else {
            printf("speed %u baud; ", IRate);
        }

    } else {
        IRate = SttyConvertBaudValueToRate(Ispeed);
        ORate = SttyConvertBaudValueToRate(Ospeed);
        if (Short != FALSE) {
            printf("%u %u\n", IRate, ORate);

        } else {
            printf("ispeed %u baud; ospeed %u baud; ", IRate, ORate);
        }
    }

    return;
}

ULONG
SttyConvertBaudValueToRate (
    speed_t Value
    )

/*++

Routine Description:

    This routine converts a baud value into its numeric rate.

Arguments:

    Value - Supplies the baud value to convert.

Return Value:

    Returns the baud rate corresponding to the given value.

    0 if no known baud rate matches the given value.

--*/

{

    PTTY_BAUD_RATE Entry;

    Entry = TtyBaudRates;
    while (Entry->Name != NULL) {
        if (Entry->Value == Value) {
            return Entry->Rate;
        }

        Entry += 1;
    }

    return 0;
}

speed_t
SttyConvertRateToBaudValue (
    PSTR String
    )

/*++

Routine Description:

    This routine converts a baud rate into its encoded value.

Arguments:

    String - Supplies the string to convert to a baud speed.

Return Value:

    Returns the corresponding speed.

    -1 if nothing matches.

--*/

{

    PTTY_BAUD_RATE Entry;

    Entry = TtyBaudRates;
    while (Entry->Name != NULL) {
        if (strcmp(String, Entry->Name) == 0) {
            return Entry->Value;
        }

        Entry += 1;
    }

    return -1;
}

VOID
SttyPrintCharacter (
    cc_t Character
    )

/*++

Routine Description:

    This routine prints a character, including control prefixes.

Arguments:

    Character - Supplies the character to print.

Return Value:

    None.

--*/

{

    if (Character == _POSIX_VDISABLE) {
        printf("<undef>");
        return;
    }

    if (Character < ' ') {
        printf("^%c", Character + '@');

    } else if (Character < 0x7F) {
        printf("%c", Character);

    } else if (Character == 0x7F) {
        printf("^?");

    } else {
        printf("M-");
        if ((UCHAR)Character < 0x80 + ' ') {
            printf("^%c", Character - 0x80 + '@');

        } else if ((UCHAR)Character < 0x80 + 0x7F) {
            printf("%c", Character - 0x80);

        } else {
            printf("^?");
        }
    }

    return;
}

VOID
SttySanitizeSettings (
    struct termios *Termios
    )

/*++

Routine Description:

    This routine adjusts the given terminal settings to sane mode.

Arguments:

    Termios - Supplies a pointer to the terminal settings.

Return Value:

    None.

--*/

{

    tcflag_t *Field;
    PSTTY_MEMBER Member;
    STTY_TERMIOS_MEMBER Type;

    Member = SttyOptions;
    while (Member->Name != NULL) {
        Type = Member->Member;
        if ((Type == SttyTermiosCharacter) || (Type == SttyTermiosTime)) {
            Termios->c_cc[Member->Value] = Member->Mask;

        } else {
            if ((Member->Flags & STTY_SANE_SET) != 0) {
                Field = SttyGetTermiosMember(Termios, Type);
                *Field &= ~(Member->Mask);
                *Field |= Member->Value;

            } else if ((Member->Flags & STTY_SANE_CLEAR) != 0) {
                Field = SttyGetTermiosMember(Termios, Type);
                *Field &= ~(Member->Mask);
                *Field &= ~(Member->Value);
            }
        }

        Member += 1;
    }

    return;
}

tcflag_t *
SttyGetTermiosMember (
    struct termios *Termios,
    STTY_TERMIOS_MEMBER MemberType
    )

/*++

Routine Description:

    This routine returns a pointer to the terminal structure member
    corresponding to the given member type.

Arguments:

    Termios - Supplies a pointer to the terminal settings.

    MemberType - Supplies the member to get.

Return Value:

    Returns a pointer to the designated membe within the structure.

--*/

{

    switch (MemberType) {
    case SttyTermiosInput:
        return &(Termios->c_iflag);

    case SttyTermiosOutput:
        return &(Termios->c_oflag);

    case SttyTermiosControl:
        return &(Termios->c_cflag);

    case SttyTermiosLocal:
        return &(Termios->c_lflag);

    default:
        break;
    }

    return NULL;
}

VOID
SttySetCharacter (
    struct termios *Termios,
    PSTTY_MEMBER Member,
    PSTR Argument
    )

/*++

Routine Description:

    This routine attempts to apply the given control character argument.

Arguments:

    Termios - Supplies a pointer to the terminal settings to change.

    Member - Supplies the member to set.

    Argument - Supplies a pointer to the control character string.

Return Value:

    None.

--*/

{

    unsigned long Value;

    if (Member->Member == SttyTermiosTime) {
        Value = strtoul(Argument, NULL, 0);

    } else {

        assert(Member->Member == SttyTermiosCharacter);

        if ((Argument[0] == '\0') || (Argument[1] == '\0')) {
            Value = (UCHAR)(Argument[0]);

        } else if ((strcmp(Argument, "^-") == 0) ||
                   (strcmp(Argument, "undef") == 0)) {

            Value = _POSIX_VDISABLE;

        } else if (Argument[0] == '^') {
            Argument += 1;
            if (*Argument == '?') {
                Value = 0x7F;

            } else {

                //
                // Strip the upper and lower case bits to get something in the
                // range of 0-32.
                //

                Value = STTY_CONTROL(*Argument);
            }

        } else {
            Value = strtol(Argument, NULL, 0);
        }
    }

    Termios->c_cc[Member->Value] = Value;
    return;
}

