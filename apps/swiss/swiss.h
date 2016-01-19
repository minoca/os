/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    swiss.h

Abstract:

    This header contains definitions for the Swiss Army Knife utility.

Author:

    Evan Green 13-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
SwissRunCommand (
    PSTR Command,
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

    Command - Supplies the command string to run.

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
