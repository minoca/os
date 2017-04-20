/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sed.c

Abstract:

    This module implements the sed (stream editor) utility.

Author:

    Evan Green 11-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sed.h"
#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SedProcessInput (
    PSED_CONTEXT Context
    );

INT
SedProcessInputLine (
    PSED_CONTEXT Context
    );

BOOL
SedDoesAddressMatch (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

BOOL
SedCheckAddress (
    PSED_CONTEXT Context,
    PSED_ADDRESS Address
    );

INT
SedExecuteCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SedLongOptions[] = {
    {"expression", required_argument, 0, 'e'},
    {"file", required_argument, 0, 'f'},
    {"quiet", no_argument, 0, 'n'},
    {"silent", no_argument, 0, 'n'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
SedMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the sed (stream editor)
    utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSED_APPEND_ENTRY AppendEntry;
    PSTR Argument;
    ULONG ArgumentIndex;
    SED_CONTEXT Context;
    PSTR FirstSource;
    PSED_INPUT InputEntry;
    INT Option;
    BOOL ReadFromStandardIn;
    BOOL Result;
    PSED_SCRIPT_FRAGMENT ScriptFragment;
    BOOL ScriptRead;
    PSTR SecondSource;
    int Status;
    int TotalStatus;
    PSED_WRITE_FILE WriteFile;

    FirstSource = NULL;
    ReadFromStandardIn = TRUE;
    ScriptRead = FALSE;
    SecondSource = NULL;
    Status = 0;
    TotalStatus = 0;
    memset(&Context, 0, sizeof(SED_CONTEXT));
    Context.PrintLines = TRUE;
    Context.HeadCommand.Function.Type = SedFunctionGroup;
    INITIALIZE_LIST_HEAD(&(Context.HeadCommand.Function.U.ChildList));
    INITIALIZE_LIST_HEAD(&(Context.ScriptList));
    INITIALIZE_LIST_HEAD(&(Context.InputList));
    INITIALIZE_LIST_HEAD(&(Context.AppendList));
    INITIALIZE_LIST_HEAD(&(Context.WriteFileList));
    Context.StandardOut.File = stdout;
    Context.StandardOut.LineTerminated = TRUE;
    Context.ScriptString = SedCreateString(NULL, 0, TRUE);
    if (Context.ScriptString == NULL) {
        TotalStatus = ENOMEM;
        goto MainEnd;
    }

    Context.PatternSpace = SedCreateString(NULL, 0, TRUE);
    if (Context.PatternSpace == NULL) {
        TotalStatus = ENOMEM;
        goto MainEnd;
    }

    Context.HoldSpace = SedCreateString(NULL, 0, TRUE);
    if (Context.HoldSpace == NULL) {
        TotalStatus = ENOMEM;
        goto MainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SED_OPTIONS_STRING,
                             SedLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            return Status;
        }

        switch (Option) {
        case 'e':
            ScriptRead = TRUE;
            Argument = optarg;

            assert(Argument != NULL);

            Result = SedAddScriptString(&Context, Argument);
            if (Result == FALSE) {
                TotalStatus = 1;
                goto MainEnd;
            }

            break;

        case 'f':
            ScriptRead = TRUE;
            Argument = optarg;

            assert(Argument != NULL);

            Result = SedAddScriptFile(&Context, Argument);
            if (Result == FALSE) {
                TotalStatus = 1;
                goto MainEnd;
            }

            break;

        case 'n':
            Context.PrintLines = FALSE;
            break;

        case 'V':
            SwPrintVersion(SED_VERSION_MAJOR, SED_VERSION_MINOR);
            return 1;

        case 'h':
            printf(SED_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            return Status;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex < ArgumentCount) {
        FirstSource = Arguments[ArgumentIndex];
        if (ArgumentIndex + 1 < ArgumentCount) {
            SecondSource = Arguments[ArgumentIndex + 1];
        }
    }

    //
    // If there were no scripts read, the first non-control argument is the
    // script.
    //

    if (ScriptRead == FALSE) {
        if (FirstSource == NULL) {
            SwPrintError(0, NULL, "Argument expected. Try --help for usage");
            TotalStatus = 1;
            goto MainEnd;
        }

        Result = SedAddScriptString(&Context, FirstSource);
        if (Result == FALSE) {
            goto MainEnd;
        }

        if (SecondSource != NULL) {
            ReadFromStandardIn = FALSE;
        }

    } else if (FirstSource != NULL) {
        ReadFromStandardIn = FALSE;
    }

    //
    // Parse the final script.
    //

    Result = SedParseScript(&Context, Context.ScriptString->Data);
    if (Result == FALSE) {
        goto MainEnd;
    }

    //
    // Reset the line number for the input files.
    //

    Context.LineNumber = 0;
    if (ReadFromStandardIn != FALSE) {

        //
        // Create a single input entry for standard in.
        //

        InputEntry = malloc(sizeof(SED_INPUT));
        if (InputEntry == NULL) {
            TotalStatus = ENOMEM;
            goto MainEnd;
        }

        InputEntry->File = stdin;
        INSERT_BEFORE(&(InputEntry->ListEntry), &(Context.InputList));
        Status = SedProcessInput(&Context);
        if (Status != 0) {
            TotalStatus = Status;
        }

        goto MainEnd;
    }

    //
    // Loop through the remaining arguments to create the input entries and
    // figure out which input file contains the last line.
    //

    while (ArgumentIndex < ArgumentCount) {
        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

        //
        // Skip over the script itself.
        //

        if ((ScriptRead == FALSE) && (Argument == FirstSource)) {
            continue;
        }

        InputEntry = malloc(sizeof(SED_INPUT));
        if (InputEntry == NULL) {
            TotalStatus = ENOMEM;
            goto MainEnd;
        }

        InputEntry->File = fopen(Argument, "r");
        if (InputEntry->File != NULL) {
            INSERT_BEFORE(&(InputEntry->ListEntry), &(Context.InputList));

        } else {
            TotalStatus = errno;
            SwPrintError(TotalStatus, Argument, "Cannot open");
            free(InputEntry);
        }
    }

    //
    // Let sed process all this.
    //

    Status = SedProcessInput(&Context);
    if (Status != 0) {
        TotalStatus = Status;
        goto MainEnd;
    }

MainEnd:
    if (Context.ScriptString != NULL) {
        SedDestroyString(Context.ScriptString);
    }

    if (Context.PatternSpace != NULL) {
        SedDestroyString(Context.PatternSpace);
    }

    if (Context.HoldSpace != NULL) {
        SedDestroyString(Context.HoldSpace);
    }

    SedDestroyCommands(&Context);
    while (LIST_EMPTY(&(Context.ScriptList)) == FALSE) {
        ScriptFragment = LIST_VALUE(Context.ScriptList.Next,
                                    SED_SCRIPT_FRAGMENT,
                                    ListEntry);

        LIST_REMOVE(&(ScriptFragment->ListEntry));
        free(ScriptFragment);
    }

    while (LIST_EMPTY(&(Context.InputList)) == FALSE) {
        InputEntry = LIST_VALUE(Context.InputList.Next, SED_INPUT, ListEntry);
        LIST_REMOVE(&(InputEntry->ListEntry));
        if ((InputEntry->File != NULL) && (InputEntry->File != stdin)) {
            fclose(InputEntry->File);
        }

        free(InputEntry);
    }

    while (LIST_EMPTY(&(Context.AppendList)) == FALSE) {
        AppendEntry = LIST_VALUE(Context.AppendList.Next,
                                 SED_APPEND_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(AppendEntry->ListEntry));
        free(AppendEntry);
    }

    while (LIST_EMPTY(&(Context.WriteFileList)) == FALSE) {
        WriteFile = LIST_VALUE(Context.WriteFileList.Next,
                               SED_WRITE_FILE,
                               ListEntry);

        LIST_REMOVE(&(WriteFile->ListEntry));
        if (WriteFile->Name != NULL) {
            SedDestroyString(WriteFile->Name);
        }

        if (WriteFile->File != NULL) {
            fclose(WriteFile->File);
        }

        free(WriteFile);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

INT
SedReadLine (
    PSED_CONTEXT Context
    )

/*++

Routine Description:

    This routine reads a new line into the pattern space, sans its trailing
    newline.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSED_APPEND_ENTRY AppendEntry;
    int Character;
    FILE *File;
    PSED_INPUT InputEntry;
    PSED_STRING Pattern;
    FILE *ReadFile;
    BOOL Result;
    INT Status;

    Context->TestResult = FALSE;
    Pattern = Context->PatternSpace;
    Status = 0;

    //
    // Deal with any appended stuff.
    //

    while (LIST_EMPTY(&(Context->AppendList)) == FALSE) {
        AppendEntry = LIST_VALUE(Context->AppendList.Next,
                                 SED_APPEND_ENTRY,
                                 ListEntry);

        LIST_REMOVE(&(AppendEntry->ListEntry));
        if (AppendEntry->Type == SedFunctionPrintTextAtLineEnd) {
            printf("%s\n", AppendEntry->StringOrPath->Data);

        } else {

            assert(AppendEntry->Type == SedFunctionReadFile);

            //
            // Write the file to standard out.
            //

            ReadFile = fopen(AppendEntry->StringOrPath->Data, "r");
            if (ReadFile != NULL) {
                while (TRUE) {
                    Character = fgetc(ReadFile);
                    if (Character == EOF) {
                        break;
                    }

                    fputc(Character, stdout);
                }

                fclose(ReadFile);
            }
        }

        free(AppendEntry);
    }

    //
    // If there is no current file, get the first one.
    //

    InputEntry = Context->CurrentInput;
    if (InputEntry == NULL) {
        if (LIST_EMPTY(&(Context->InputList)) != FALSE) {
            Context->Done = TRUE;
            goto ReadLineEnd;
        }

        InputEntry = LIST_VALUE(Context->InputList.Next, SED_INPUT, ListEntry);
        Context->CurrentInput = InputEntry;
    }

    File = InputEntry->File;

    //
    // If the last line variable is already set, then this is toast.
    //

    if (Context->LastLine != FALSE) {
        Context->Done = TRUE;
        goto ReadLineEnd;
    }

    Context->LineNumber += 1;
    while (TRUE) {
        Character = fgetc(File);
        if (Character == EOF) {

            //
            // Return this as a line for sure, but return it as the last line
            // if this is the last file in the input. Otherwise, move on to
            // the next input.
            //

            if (InputEntry->ListEntry.Next == &(Context->InputList)) {
                Context->LastLine = TRUE;

            } else {
                InputEntry = LIST_VALUE(InputEntry->ListEntry.Next,
                                        SED_INPUT,
                                        ListEntry);

                Context->CurrentInput = InputEntry;
                File = InputEntry->File;
            }

            //
            // If no input was received yet, keep going.
            //

            if (Pattern->Size == 1) {
                if (Context->LastLine != FALSE) {
                    Context->Done = TRUE;

                } else {
                    continue;
                }
            }

            Context->LineTerminator = EOF;
            break;
        }

        if ((Character == '\n') || (Character == '\0')) {
            Context->LineTerminator = Character;
            Character = fgetc(File);

            //
            // A newline at the end of a file does not count as a new line.
            // Examples: an empty file has zero lines, and a file with a single
            // newline in it has one. But a file with some characters and no
            // newline also has one line.
            //

            if (Character == EOF) {
                if (InputEntry->ListEntry.Next == &(Context->InputList)) {
                    Context->LastLine = TRUE;

                } else {
                    InputEntry = LIST_VALUE(InputEntry->ListEntry.Next,
                                            SED_INPUT,
                                            ListEntry);

                    Context->CurrentInput = InputEntry;
                }

            } else {
                ungetc(Character, File);
            }

            break;
        }

        Result = SedAppendString(Context->PatternSpace, (PSTR)&Character, 1);
        if (Result == FALSE) {
            Status = ENOMEM;
            goto ReadLineEnd;
        }
    }

ReadLineEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SedProcessInput (
    PSED_CONTEXT Context
    )

/*++

Routine Description:

    This routine runs the sed scripts against the input files.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Status;

    Status = 0;

    //
    // Loop processing lines.
    //

    while (Context->Quit == FALSE) {
        Status = SedReadLine(Context);
        if (Status != 0) {
            goto ProcessInputEnd;
        }

        if (Context->Done != FALSE) {
            break;
        }

        Context->SkipPrint = FALSE;
        Status = SedProcessInputLine(Context);
        if (Status != 0) {
            goto ProcessInputEnd;
        }

        //
        // Print the pattern space if the flag is set.
        //

        if ((Context->PrintLines != FALSE) &&
            (Context->SkipPrint == FALSE)) {

            SedPrint(Context,
                     Context->PatternSpace->Data,
                     Context->LineTerminator);
        }

        //
        // Clear the pattern space.
        //

        assert(Context->PatternSpace->Size != 0);

        Context->PatternSpace->Data[0] = '\0';
        Context->PatternSpace->Size = 1;
    }

ProcessInputEnd:
    return Status;
}

INT
SedProcessInputLine (
    PSED_CONTEXT Context
    )

/*++

Routine Description:

    This routine runs the loaded scripts on a single line of the pattern space.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSED_COMMAND Command;
    PSED_COMMAND NextCommand;
    INT Status;

    Status = 0;
    if (LIST_EMPTY(&(Context->HeadCommand.Function.U.ChildList)) != FALSE) {
        return 0;
    }

    Context->NextCommand =
                     LIST_VALUE(Context->HeadCommand.Function.U.ChildList.Next,
                                SED_COMMAND,
                                ListEntry);

    //
    // Loop processing commands.
    //

    while (Context->NextCommand != NULL) {
        if ((Context->Done != FALSE) || (Context->Quit != FALSE)) {
            break;
        }

        Command = Context->NextCommand;

        //
        // Fill in the next command by moving on to the next sibling, or up the
        // chain if necessary. Stop if the head command is reached.
        //

        NextCommand = Command;
        while (NextCommand != &(Context->HeadCommand)) {

            //
            // If there's a sibling, go to it.
            //

            if (NextCommand->ListEntry.Next !=
                &(NextCommand->Parent->Function.U.ChildList)) {

                NextCommand = LIST_VALUE(NextCommand->ListEntry.Next,
                                         SED_COMMAND,
                                         ListEntry);

                break;

            //
            // Move up to the parent.
            //

            } else {
                NextCommand = NextCommand->Parent;
            }
        }

        if (NextCommand == &(Context->HeadCommand)) {
            NextCommand = NULL;
        }

        Context->NextCommand = NextCommand;

        //
        // Process the command.
        //

        Status = SedExecuteCommand(Context, Command);
        if (Status != 0) {
            goto ProcessInputLineEnd;
        }
    }

ProcessInputLineEnd:
    return Status;
}

BOOL
SedDoesAddressMatch (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine determines if the given command matches the current address.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command potentially being executed.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOL Result;

    Result = FALSE;
    if (Command->AddressCount == 0) {
        Result = TRUE;

    } else if (Command->AddressCount == 1) {
        Result = SedCheckAddress(Context, &(Command->Address[0]));

    } else {

        assert(Command->AddressCount == 2);

        if (Command->Active != FALSE) {
            if (SedCheckAddress(Context, &(Command->Address[1])) != FALSE) {
                Command->Active = FALSE;
            }

            Result = TRUE;

        } else {
            if (SedCheckAddress(Context, &(Command->Address[0])) != FALSE) {
                Command->Active = TRUE;
                Result = TRUE;
            }
        }
    }

    if (Command->AddressNegated != FALSE) {
        Result = !Result;
    }

    return Result;
}

BOOL
SedCheckAddress (
    PSED_CONTEXT Context,
    PSED_ADDRESS Address
    )

/*++

Routine Description:

    This routine determines if the given address matches the current context.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a pointer to the address to check.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    int Status;

    switch (Address->Type) {
    case SedAddressNumber:
        if (Address->U.Line == Context->LineNumber) {
            return TRUE;
        }

        break;

    case SedAddressLastLine:
        if (Context->LastLine != FALSE) {
            return TRUE;
        }

        break;

    case SedAddressExpression:

        assert((Context->PatternSpace->Size != 0) &&
               (Context->PatternSpace->Data[Context->PatternSpace->Size - 1] ==
                '\0'));

        Status = regexec(&(Address->U.Expression),
                         Context->PatternSpace->Data,
                         0,
                         NULL,
                         0);

        if (Status == 0) {
            return TRUE;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return FALSE;
}

INT
SedExecuteCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine runs the given command on the current pattern space.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to run.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    BOOL AddressMatch;
    PSED_EXECUTE_FUNCTION ExecuteRoutine;

    //
    // Figure out if the address matches the line.
    //

    AddressMatch = SedDoesAddressMatch(Context, Command);
    if (AddressMatch == FALSE) {
        return 0;
    }

    assert((Command->Function.Type != SedFunctionInvalid) &&
           (Command->Function.Type < SedFunctionCount));

    ExecuteRoutine = SedFunctionTable[Command->Function.Type];
    return ExecuteRoutine(Context, Command);
}

