/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    swiss.h

Abstract:

    This header contains definitions for the Swiss utility.

Author:

    Evan Green 13-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "swisscmd.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Set this flag to keep setuid privileges.
//

#define SWISS_APP_SETUID_OK 0x00000001

//
// Set this flag to avoid showing the app in --list.
//

#define SWISS_APP_HIDDEN 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
INT
(*PSWISS_COMMAND_ENTRY_POINT) (
    INT ArgumentCount,
    CHAR **Arguments
    );

/*++

Routine Description:

    This routine implements the entry point for a Swiss command.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines the structure of a Swiss command entry point.

Members:

    CommandName - Stores a pointer to a string containing the name of the
        command.

    CommandDescription - Stores a pointer to a null terminated string
        containing a short one-liner describing the utility.

    MainFunction - Stores a pointer to the function to call to run the
        command.

    Flags - Stores a bitfield of flags about the command. See SWISS_APP_*
        definitions.

--*/

typedef struct _SWISS_COMMAND_ENTRY {
    PSTR CommandName;
    PSTR CommandDescription;
    PSWISS_COMMAND_ENTRY_POINT MainFunction;
    ULONG Flags;
} SWISS_COMMAND_ENTRY, *PSWISS_COMMAND_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

extern SWISS_COMMAND_ENTRY SwissCommands[];

//
// -------------------------------------------------------- Function Prototypes
//

PSWISS_COMMAND_ENTRY
SwissFindCommand (
    PSTR Command
    );

/*++

Routine Description:

    This routine searches for a command in the global command list.

Arguments:

    Command - Supplies a pointer to the string of the command to look for.

Return Value:

    Returns a pointer to the command entry on success.

    NULL if the command could not be found.

--*/

BOOL
SwissRunCommand (
    PSWISS_COMMAND_ENTRY Command,
    CHAR **Arguments,
    ULONG ArgumentCount,
    BOOL SeparateProcess,
    BOOL Wait,
    PINT ReturnValue
    );

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

