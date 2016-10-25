/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kill.c

Abstract:

    This module implements the kill (process termination and signalling)
    utility.

Author:

    Chris Stevens 19-Aug-2014

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define KILL_VERSION_MAJOR 1
#define KILL_VERSION_MINOR 0

#define KILL_USAGE                                                             \
    "usage: kill -s signal_name pid...\n"                                      \
    "       kill -l [exit_status...]\n"                                        \
    "       kill [-signal_name] pid...\n"                                      \
    "       kill [-signal_number] pid...\n\n"                                  \
    "The kill utility sends a signal to one or more processes. Options are:\n" \
    "  -l --list -- Lists all supported values for signal_name if an "         \
    "exit_status\n"                                                            \
    "        is not supplied. If an exit_status is supplied and it is a\n"     \
    "        signal_number, then the corresponding signal_name is written.\n"  \
    "        If the exit_status the '?' special shell character for a\n"       \
    "        terminated process, then signal_name of the signal that "         \
    "terminated\n"                                                             \
    "        the process is written.\n"                                        \
    "  -s --signal <signal_name> -- Specify a signal to send using a signal\n" \
    "        name.\n"                                                          \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"

#define KILL_OPTIONS_STRING "+ls:hV"

//
// Define kill options.
//

//
// Set this option if the utility should list signal names and values.
//

#define KILL_OPTIONS_LIST_SIGNALS 0x00000001

//
// Set this option if the utility should send signals.
//

#define KILL_OPTIONS_SEND_SIGNALS 0x00000002

//
// Define the default signal number to send.
//

#define KILL_DEFAULT_SIGNAL_NUMBER SIGTERM

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KillpPrintSignals (
    VOID
    );

INT
KillpPrintSignal (
    PSTR Argument
    );

INT
KillpParseSignalName (
    PSTR Argument,
    PINT SignalNumber
    );

//
// -------------------------------------------------------------------- Globals
//

struct option KillLongOptions[] = {
    {"list", no_argument, 0, 'l'},
    {"signal", required_argument, 0, 's'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
KillMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the kill utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, non-zero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Argument;
    ULONG ArgumentIndex;
    BOOL ContinueLoop;
    ULONG Index;
    INT Option;
    ULONG Options;
    INT PreviousOptionIndex;
    pid_t ProcessId;
    PSTR SignalName;
    INT SignalNumber;
    INT Status;

    Status = 0;

    //
    // Disable option errors for the kill utility as it allows some
    // non-traditional options.
    //

    opterr = 0;

    //
    // The default is to send SIGTERM signals to all process IDs listed.
    //

    Options = KILL_OPTIONS_SEND_SIGNALS;
    SignalNumber = KILL_DEFAULT_SIGNAL_NUMBER;

    //
    // Process the control arguments.
    //

    SignalName = NULL;
    ContinueLoop = TRUE;
    while (ContinueLoop != FALSE) {
        PreviousOptionIndex = optind;
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             KILL_OPTIONS_STRING,
                             KillLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if (Option == ':') {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case '?':

            //
            // If a signal name has already been found, then stop processing
            // and roll back the index.
            //

            if (SignalName != NULL) {
                optind = PreviousOptionIndex;
                ContinueLoop = FALSE;
                break;
            }

            //
            // Otherwise there may be a signal name at the previous index. If
            // it was just a '-9' then 'optind' incremented. If it was '-kill',
            // then 'optind' did not increment. So, go with the previous option
            // index, saving the signal name for later.
            //

            Argument = Arguments[PreviousOptionIndex];

            assert(*Argument == '-');

            Argument += 1;
            SignalName = Argument;
            Options |= KILL_OPTIONS_SEND_SIGNALS;

            //
            // Also, skip over the whole option if it did not move forward.
            //

            if (optind == PreviousOptionIndex) {
                optind += 1;
            }

            break;

        case 'l':
            Options |= KILL_OPTIONS_LIST_SIGNALS;
            break;

        case 's':

            //
            // Check to see if this is a direct signal name. For example,
            // "kill -sigkill 12345" should kill process 12345. If the option
            // index only advanced 1, then the argument is attached to the 's'.
            // Test these cases for '-sig*'.
            //

            if ((optind - 1) == PreviousOptionIndex) {

                //
                // If a signal name has already been found, exit and roll back
                // the option.
                //

                if (SignalName != NULL) {
                    optind = PreviousOptionIndex;
                    ContinueLoop = FALSE;
                    break;
                }

                Argument = optarg;

                //
                // Move back a character to include the initial 's' and save it
                // for later.
                //

                Argument -= 1;

                assert(*Argument == 's');

                SignalName = Argument;
                Options |= KILL_OPTIONS_SEND_SIGNALS;

            //
            // Otherwise, the argument is in a separate array entry that the
            // 's' option. Save it for later.
            //

            } else {
                Argument = optarg;

                assert(Argument != NULL);

                SignalName = Argument;
                Options |= KILL_OPTIONS_SEND_SIGNALS;
            }

            break;

        case 'V':
            SwPrintVersion(KILL_VERSION_MAJOR, KILL_VERSION_MINOR);
            return 1;

        case 'h':
            printf(KILL_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    //
    // One of the kill options should be sent.
    //

    assert(((Options & KILL_OPTIONS_LIST_SIGNALS) != 0) ||
           ((Options & KILL_OPTIONS_SEND_SIGNALS) != 0));

    //
    // Get the current argument index, making sure it is not too big.
    //

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        Status = 1;
        goto MainEnd;
    }

    //
    // If listing the signal numbers was requested, then prefer that over
    // sending signals.
    //

    if ((Options & KILL_OPTIONS_LIST_SIGNALS) != 0) {
        if (ArgumentIndex == ArgumentCount) {
            KillpPrintSignals();

        } else {
            for (Index = ArgumentIndex; Index < ArgumentCount; Index += 1) {
                Status = KillpPrintSignal(Arguments[Index]);
                if (Status != 0) {
                    goto MainEnd;
                }
            }
        }

    //
    // Sending signals is the default. Send signals to all remaining arguments,
    // interpreting them as process IDs or process groups.
    //

    } else {

        //
        // Figure out the signal to use.
        //

        if (SignalName != NULL) {
            Status = KillpParseSignalName(SignalName, &SignalNumber);
            if (Status != 0) {
                goto MainEnd;
            }
        }

        //
        // Exit and print the help if no process IDs were provided.
        //

        if (ArgumentIndex == ArgumentCount) {
            printf(KILL_USAGE);
            Status = 1;
            goto MainEnd;
        }

        for (Index = ArgumentIndex; Index < ArgumentCount; Index += 1) {
            ProcessId = (pid_t)strtol(Arguments[Index], &AfterScan, 10);
            if ((AfterScan == Arguments[Index]) || (*AfterScan != '\0')) {
                SwPrintError(0,
                             NULL,
                             "Invalid process ID '%s'",
                             Arguments[Index]);

                continue;
            }

            Status = SwKill(ProcessId, SignalNumber);
            if (Status != 0) {
                SwPrintError(errno,
                             NULL,
                             "Failed to signal process %lu",
                             ProcessId);
            }
        }
    }

    Status = 0;

MainEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KillpPrintSignals (
    VOID
    )

/*++

Routine Description:

    This routine prints out all the allowable signal names and signal numbers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSTR Name;
    INT Signal;

    //
    // Print all signals, skipping the zero signal.
    //

    for (Signal = 1; Signal <= NSIG; Signal += 1) {
        if ((SIGRTMAX > SIGRTMIN) &&
            (Signal >= SIGRTMIN) && (Signal <= SIGRTMAX)) {

            if (Signal == SIGRTMIN) {
                printf("%d) SIGRTMIN\n", Signal);

            } else if (Signal == SIGRTMAX) {
                printf("%d) SIGRTMAX\n", Signal);

            } else {
                printf("%d) SIGRTMIN+%d\n", Signal, Signal - SIGRTMIN);
            }

        } else {
            Name = SwGetSignalNameFromNumber(Signal);
            if (Name != NULL) {
                printf("%d) %s\n", Signal, Name);
            }
        }
    }

    return;
}

INT
KillpPrintSignal (
    PSTR Argument
    )

/*++

Routine Description:

    This routine prints the signal name for a given signal number string or
    prints the signal number for a given signal name string.

Arguments:

    Argument - Supplies the string representation of either a signal name or
        signal number.

Return Value:

    0 on success.

    EINVAL if the signal argument is invalid.

--*/

{

    PSTR Name;
    INT SignalNumber;
    INT Status;

    Status = 0;
    SignalNumber = SwGetSignalNumberFromName(Argument);
    if (SignalNumber == -1) {
        SwPrintError(0, Argument, "Invalid signal specification");
        Status = EINVAL;

    } else {
        if (!isdigit(*Argument)) {
            printf("%d\n", SignalNumber);

        } else {
            if ((SIGRTMAX > SIGRTMIN) &&
                (SignalNumber >= SIGRTMIN) &&
                (SignalNumber <= SIGRTMAX)) {

                if (SignalNumber == SIGRTMIN) {
                    printf("RTMIN\n");

                } else if (SignalNumber < SIGRTMAX) {
                    printf("RTMAX\n");

                } else {
                    printf("RTMIN+%d", SignalNumber - SIGRTMIN);
                }

            } else {
                Name = SwGetSignalNameFromNumber(SignalNumber);
                if (Name == NULL) {
                    SwPrintError(0, Argument, "Invalid signal specification");
                    Status = EINVAL;

                } else {
                    printf("%s\n", Name);
                }
            }
        }
    }

    return Status;
}

INT
KillpParseSignalName (
    PSTR Argument,
    PINT SignalNumber
    )

/*++

Routine Description:

    This routine parses the signal name argument to validate it and conver it
    into a signal number.

Arguments:

    Argument - Supplies the argument string to parse for the signal name.

    SignalNumber - Supplies a pointer that receives the numeric signal value.

Return Value:

    0 on succes. Non-zero on failure.

--*/

{

    *SignalNumber = SwGetSignalNumberFromName(Argument);
    if (*SignalNumber == -1) {
        return EINVAL;
    }

    return 0;
}

