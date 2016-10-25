/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    swiss.c

Abstract:

    This module implements the Swiss utility, which contains many of the
    standard commands packaged into one binary.

Author:

    Evan Green 5-Jun-2013

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "swiss.h"
#include "swisscmd.h"
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SWISS_VERSION_MAJOR 1
#define SWISS_VERSION_MINOR 0

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

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the Swiss utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSWISS_COMMAND_ENTRY Command;
    ULONG CommandIndex;
    PSTR CommandName;
    PSTR Dot;
    id_t EffectiveId;
    PSTR LastSlash;
    id_t RealId;
    BOOL Result;
    INT ReturnValue;

    SwSetCurrentApplicationName("swiss");

    //
    // Figure out the command name. Something like c:/mydir/sh.exe should
    // become just 'sh'.
    //

    LastSlash = strrchr(Arguments[0], '/');
    if (LastSlash == NULL) {
        LastSlash = strrchr(Arguments[0], '\\');
    }

    if (LastSlash != NULL) {
        LastSlash += 1;

    } else {
        LastSlash = Arguments[0];
    }

    CommandName = strdup(LastSlash);
    if (CommandName == NULL) {
        ReturnValue = 1;
        goto mainEnd;
    }

    Dot = strchr(CommandName, '.');
    if (Dot != NULL) {
        *Dot = '\0';
    }

    //
    // Look for the command in either the last component of argv[0] or in
    // argv[1].
    //

    Command = SwissFindCommand(CommandName);
    if (Command == NULL) {
        if (ArgumentCount > 1) {
            Command = SwissFindCommand(Arguments[1]);
            if (Command != NULL) {
                Arguments += 1;
                ArgumentCount -= 1;
            }

        //
        // Default to sh if swiss was just run directly with no arguments.
        //

        } else if (strncasecmp(CommandName, "swiss", 5) == 0) {
            Command = SwissFindCommand(SH_COMMAND_NAME);
            if (Command != NULL) {
                SwPrintVersion(SWISS_VERSION_MAJOR, SWISS_VERSION_MINOR);
            }
        }
    }

    //
    // This is not a valid command.
    //

    if (Command == NULL) {
        ReturnValue = 1;

        //
        // The entry was not valid, so look at the next argument. If there is
        // no next argument, print the usage and exit.
        //

        if ((ArgumentCount < 2) || (strcmp(Arguments[1], "--help") == 0)) {
            printf("Usage: swiss <command> ...\n\nValid Commands:\n\n");
            CommandIndex = 0;
            while (SwissCommands[CommandIndex].CommandName != NULL) {
                if ((SwissCommands[CommandIndex].Flags &
                     SWISS_APP_HIDDEN) == 0) {

                    printf("%10s - %s\n",
                           SwissCommands[CommandIndex].CommandName,
                           SwissCommands[CommandIndex].CommandDescription);
                }

                CommandIndex += 1;
            }

            printf("\n");

        } else if (strcmp(Arguments[1], "--list") == 0) {
            CommandIndex = 0;
            while (SwissCommands[CommandIndex].CommandName != NULL) {
                if ((SwissCommands[CommandIndex].Flags &
                     SWISS_APP_HIDDEN) == 0) {

                    printf("%s\n", SwissCommands[CommandIndex].CommandName);
                }

                CommandIndex += 1;
            }

            ReturnValue = 0;

        } else {
            SwPrintError(0,
                         NULL,
                         "Command not found in either '%s' nor '%s'",
                         CommandName,
                         Arguments[1]);
        }

        goto mainEnd;
    }

    //
    // Drop setuid privileges unless the app wants to keep them.
    //

    if ((Command->Flags & SWISS_APP_SETUID_OK) == 0) {
        RealId = SwGetRealUserId();
        EffectiveId = SwGetEffectiveUserId();
        if ((RealId != 0) && (RealId != EffectiveId)) {
            ReturnValue = SwSetRealUserId(RealId);
            if (ReturnValue != 0) {
                SwPrintError(ReturnValue, NULL, "Failed to drop privileges");
                ReturnValue = 1;
                goto mainEnd;
            }

            assert(SwGetEffectiveUserId() == RealId);
        }

        RealId = SwGetRealGroupId();
        EffectiveId = SwGetEffectiveGroupId();
        if ((RealId != 0) && (RealId != EffectiveId)) {
            ReturnValue = SwSetRealGroupId(RealId);
            if (ReturnValue != 0) {
                SwPrintError(ReturnValue, NULL, "Failed to drop privileges");
                ReturnValue = 1;
                goto mainEnd;
            }

            assert(SwGetEffectiveGroupId() == RealId);
        }
    }

    //
    // Run the command.
    //

    Result = SwissRunCommand(Command,
                             Arguments,
                             ArgumentCount,
                             FALSE,
                             TRUE,
                             &ReturnValue);

    if (Result == FALSE) {
        ReturnValue = 1;
        goto mainEnd;
    }

mainEnd:
    if (CommandName != NULL) {
        free(CommandName);
    }

    return ReturnValue;
}

PSWISS_COMMAND_ENTRY
SwissFindCommand (
    PSTR Command
    )

/*++

Routine Description:

    This routine searches for a command in the global command list.

Arguments:

    Command - Supplies a pointer to the string of the command to look for.

Return Value:

    Returns a pointer to the command entry on success.

    NULL if the command could not be found.

--*/

{

    ULONG CommandIndex;

    //
    // Skip the dash which indicates a login process.
    //

    if (Command[0] == '-') {
        Command += 1;
    }

    CommandIndex = 0;
    while (SwissCommands[CommandIndex].CommandName != NULL) {
        if (strcmp(Command, SwissCommands[CommandIndex].CommandName) == 0) {
            return &(SwissCommands[CommandIndex]);
        }

        CommandIndex += 1;
    }

    return NULL;
}

BOOL
SwissRunCommand (
    PSWISS_COMMAND_ENTRY Command,
    CHAR **Arguments,
    ULONG ArgumentCount,
    BOOL SeparateProcess,
    BOOL Wait,
    PINT ReturnValue
    )

/*++

Routine Description:

    This routine runs a builtin command.

Arguments:

    Command - Supplies the command entry to run.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

    ArgumentCount - Supplies the number of arguments on the command line.

    SeparateProcess - Supplies a boolean indicating if the command should be
        executed in a separate process space.

    Wait - Supplies a boolean indicating if this routine should not return
        until the command has completed.

    ReturnValue - Supplies a pointer where the return value of the command
        will be returned on success.

Return Value:

    TRUE on success.

    FALSE if the command is not a recognized swiss builtin command.

--*/

{

    pid_t Child;
    PSTR ExecutablePath;
    PSTR OriginalApplication;
    INT Result;
    pid_t WaitPid;

    //
    // If the command needs to be run in a separate process, there's one of two
    // options. Either fork and run the command directly, or execute the same
    // process with the new arguments.
    //

    if (SeparateProcess != FALSE) {
        if (SwForkSupported != 0) {
            Child = SwFork();
            if (Child < 0) {
                return FALSE;

            //
            // If this is the child, run the command and then exit.
            //

            } else if (Child == 0) {
                OriginalApplication = SwSetCurrentApplicationName(
                                                         Command->CommandName);

                *ReturnValue = Command->MainFunction(ArgumentCount, Arguments);
                exit(*ReturnValue);

            //
            // If this is the parent, potentially wait for the child.
            //

            } else {
                WaitPid = SwWaitPid(Child, 0, ReturnValue);
                if (WaitPid == -1) {
                    return FALSE;
                }
            }

        //
        // Fork is not supported, so run the process again with different
        // arguments.
        //

        } else {
            ExecutablePath = SwGetExecutableName();

            assert(ExecutablePath != NULL);

            Result = SwRunCommand(ExecutablePath,
                                  Arguments,
                                  ArgumentCount,
                                  !Wait,
                                  ReturnValue);

            if (Result != 0) {
                return FALSE;
            }
        }

    //
    // Just run it directly.
    //

    } else {

        //
        // Asynchronous but same-process execution is not supported.
        //

        assert(Wait != FALSE);

        OriginalApplication = SwSetCurrentApplicationName(Command->CommandName);
        *ReturnValue = Command->MainFunction(ArgumentCount, Arguments);
        SwSetCurrentApplicationName(OriginalApplication);
    }

    fflush(NULL);
    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

