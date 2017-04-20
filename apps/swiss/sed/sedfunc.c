/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sedfunc.c

Abstract:

    This module implements the actual editing functions for the sed utility.

Author:

    Evan Green 12-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sed.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of columns to print when displaying with the 'l' (ell)
// command.
//

#define SED_PRINT_COLUMNS 80

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SedExecuteGroup (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteAppend (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteBranchOrTest (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteDeleteAndPrintText (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteDelete (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteHoldSpaceToPattern (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecutePatternSpaceToHold (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecutePrint (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecutePrintEscapedText (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteMoveToNextLine (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteWritePatternSpace (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteQuit (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteSubstitute (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteWriteFile (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteExchangePatternAndHold (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteSubstituteCharacters (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteNop (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

INT
SedExecuteWriteLineNumber (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

//
// -------------------------------------------------------------------- Globals
//

PSED_EXECUTE_FUNCTION SedFunctionTable[SedFunctionCount] = {
    NULL,
    SedExecuteGroup,
    SedExecuteAppend,
    SedExecuteBranchOrTest,
    SedExecuteDeleteAndPrintText,
    SedExecuteDelete,
    SedExecuteDelete,
    SedExecuteHoldSpaceToPattern,
    SedExecuteHoldSpaceToPattern,
    SedExecutePatternSpaceToHold,
    SedExecutePatternSpaceToHold,
    SedExecutePrint,
    SedExecutePrintEscapedText,
    SedExecuteMoveToNextLine,
    SedExecuteMoveToNextLine,
    SedExecuteWritePatternSpace,
    SedExecuteWritePatternSpace,
    SedExecuteQuit,
    SedExecuteAppend,
    SedExecuteSubstitute,
    SedExecuteBranchOrTest,
    SedExecuteWriteFile,
    SedExecuteExchangePatternAndHold,
    SedExecuteSubstituteCharacters,
    SedExecuteNop,
    SedExecuteWriteLineNumber,
    SedExecuteNop,
};

//
// ------------------------------------------------------------------ Functions
//

INT
SedExecuteGroup (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a group command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    assert(Command->Function.Type == SedFunctionGroup);

    //
    // Set the next command to be the first child.
    //

    if (LIST_EMPTY(&(Command->Function.U.ChildList)) == FALSE) {
        Context->NextCommand = LIST_VALUE(Command->Function.U.ChildList.Next,
                                          SED_COMMAND,
                                          ListEntry);
    }

    return 0;
}

INT
SedExecuteAppend (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes "print text at line end" or "read file" command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    PSED_APPEND_ENTRY AppendEntry;

    assert((Command->Function.Type == SedFunctionPrintTextAtLineEnd) ||
           (Command->Function.Type == SedFunctionReadFile));

    if (Command->Function.U.StringArgument == NULL) {
        return 0;
    }

    //
    // Allocate and fill out an append entry structure.
    //

    AppendEntry = malloc(sizeof(SED_APPEND_ENTRY));
    if (AppendEntry == NULL) {
        return ENOMEM;
    }

    AppendEntry->Type = Command->Function.Type;
    AppendEntry->StringOrPath = Command->Function.U.StringArgument;

    //
    // Stick the command on the end of the append list.
    //

    INSERT_BEFORE(&(AppendEntry->ListEntry), &(Context->AppendList));
    return 0;
}

INT
SedExecuteBranchOrTest (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a branch command or a test command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    int Match;
    PSED_STRING Name;
    PSED_COMMAND NextCommand;

    assert((Command->Function.Type == SedFunctionBranch) ||
           (Command->Function.Type == SedFunctionTest));

    Name = Command->Function.U.StringArgument;

    //
    // For tests, check the test result, which indicates whether anything has
    // been substituted since the last line was read from the input or the
    // last test command.
    //

    if (Command->Function.Type == SedFunctionTest) {
        if (Context->TestResult == FALSE) {
            return 0;
        }

        Context->TestResult = FALSE;
    }

    //
    // Loop looking for an entry with the given label.
    //

    NextCommand = Command;
    Command = &(Context->HeadCommand);
    while (NextCommand != NULL) {
        if ((Command->Function.Type == SedFunctionGroup) &&
            (LIST_EMPTY(&(Command->Function.U.ChildList)) == FALSE)) {

            NextCommand = LIST_VALUE(Command->Function.U.ChildList.Next,
                                     SED_COMMAND,
                                     ListEntry);

        } else {

            //
            // Fill in the next command by moving on to the next sibling, or up
            // the chain if necessary. Stop if the head command is reached.
            //

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
        }

        if (NextCommand == &(Context->HeadCommand)) {
            NextCommand = NULL;
        }

        //
        // Check it out if this is a label.
        //

        if (Command->Function.Type == SedFunctionLabel) {

            //
            // If they're both null, this is a match.
            //

            if ((Name == NULL) &&
                (Command->Function.U.StringArgument == NULL)) {

                break;
            }

            //
            // If they're both not null, test to see if the strings are equal.
            //

            if ((Name != NULL) &&
                (Command->Function.U.StringArgument != NULL)) {

                Match = strcmp(Name->Data,
                               Command->Function.U.StringArgument->Data);

                if (Match == 0) {
                    break;
                }
            }

        }

        //
        // Move on to the next command.
        //

        Command = NextCommand;
    }

    //
    // If the label wasn't found, it branches to the end of the script, which
    // is null.
    //

    assert((Command == NULL) || (Command->Function.Type == SedFunctionLabel));

    Context->NextCommand = Command;
    return 0;
}

INT
SedExecuteDeleteAndPrintText (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "delete and print text", which deletes the current
    pattern space. For a 0 or 1 address match, it prints the text. For a 2
    address match, it prints the text at the end of the range.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    assert(Command->Function.Type == SedFunctionDeleteAndPrintText);

    //
    // Delete the pattern space.
    //

    assert(Context->PatternSpace->Size != 0);

    Context->PatternSpace->Data[0] = '\0';
    Context->PatternSpace->Size = 1;

    //
    // Print the text if this isn't a 2-address form or if the active flag is
    // off in the two address form.
    //

    if ((Command->AddressCount < 2) || (Command->Active == FALSE)) {
        if (Command->Function.U.StringArgument != NULL) {
            SedPrint(Context, Command->Function.U.StringArgument->Data, EOF);
        }
    }

    Context->NextCommand = NULL;
    Context->SkipPrint = TRUE;
    return 0;
}

INT
SedExecuteDelete (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "delete pattern space and start next cycle" command
    or a "delete pattern space up to the next newline and start next cycle"
    command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    PSED_STRING Pattern;
    UINTN Size;

    assert((Command->Function.Type == SedFunctionDelete) ||
           (Command->Function.Type == SedFunctionDeleteToNewline));

    Pattern = Context->PatternSpace;

    assert(Pattern->Size != 0);

    //
    // Delete up to the next newline in the pattern space.
    //

    if (Command->Function.Type == SedFunctionDeleteToNewline) {
        Size = 0;
        while ((Size < Pattern->Size) && (Pattern->Data[Size] != '\n')) {
            Size += 1;
        }

        if (Size == Pattern->Size) {
            Pattern->Data[0] = '\0';
            Pattern->Size = 1;

        } else {
            SwStringRemoveRegion(Pattern->Data, &(Pattern->Size), 0, Size + 1);
        }

    //
    // Just delete the whole line.
    //

    } else {
        Pattern->Data[0] = '\0';
        Pattern->Size = 1;
    }

    //
    // If there's nothing left, go to the end of this cycle.
    //

    if (Pattern->Size == 1) {
        Context->NextCommand = NULL;
        Context->SkipPrint = TRUE;

    //
    // Move to the top of the cycle with the input that's there.
    //

    } else {
        Context->NextCommand = LIST_VALUE(
                                Context->HeadCommand.Function.U.ChildList.Next,
                                SED_COMMAND,
                                ListEntry);
    }

    return 0;
}

INT
SedExecuteHoldSpaceToPattern (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "replace pattern space with hold space" or
    "append newline plus hold space to pattern space" command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    PSED_STRING Hold;
    PSED_STRING Pattern;
    BOOL Result;

    assert((Command->Function.Type == SedFunctionReplacePatternWithHold) ||
           (Command->Function.Type == SedFunctionAppendHoldToPattern));

    Hold = Context->HoldSpace;
    Pattern = Context->PatternSpace;

    assert(Hold->Size != 0);
    assert(Pattern->Size != 0);

    //
    // If appending, add a newline.
    //

    if (Command->Function.Type == SedFunctionAppendHoldToPattern) {
        Result = SedAppendString(Pattern, "\n", 1);
        if (Result == FALSE) {
            return ENOMEM;
        }

    //
    // If replacing, delete the pattern space.
    //

    } else {
        Pattern->Data[0] = '\0';
        Pattern->Size = 1;
    }

    //
    // Now append the hold space.
    //

    Result = SedAppendString(Pattern, Hold->Data, Hold->Size);
    if (Result == FALSE) {
        return ENOMEM;
    }

    return 0;
}

INT
SedExecutePatternSpaceToHold (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "replace hold space with pattern space" or
    "append newline plus pattern space to hold space" command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    PSED_STRING Hold;
    PSED_STRING Pattern;
    BOOL Result;

    assert((Command->Function.Type == SedFunctionReplaceHoldWithPattern) ||
           (Command->Function.Type == SedFunctionAppendPatternToHold));

    Hold = Context->HoldSpace;
    Pattern = Context->PatternSpace;

    assert(Hold->Size != 0);
    assert(Pattern->Size != 0);

    //
    // If appending, add a newline.
    //

    if (Command->Function.Type == SedFunctionAppendPatternToHold) {
        Result = SedAppendString(Hold, "\n", 1);
        if (Result == FALSE) {
            return ENOMEM;
        }

    //
    // If replacing, delete the pattern space.
    //

    } else {
        Hold->Data[0] = '\0';
        Hold->Size = 1;
    }

    //
    // Now append the pattern space.
    //

    Result = SedAppendString(Hold, Pattern->Data, Pattern->Size);
    if (Result == FALSE) {
        return ENOMEM;
    }

    return 0;
}

INT
SedExecutePrint (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a print (i) command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    assert(Command->Function.Type == SedFunctionPrintText);

    if (Command->Function.U.StringArgument != NULL) {
        SedPrint(Context, Command->Function.U.StringArgument->Data, '\n');
    }

    return 0;
}

INT
SedExecutePrintEscapedText (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a print (i) command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    CHAR Character;
    ULONG Column;
    UINTN Index;
    PSED_STRING Pattern;

    assert(Command->Function.Type == SedFunctionWritePatternEscaped);

    Pattern = Context->PatternSpace;

    assert(Pattern->Size != 0);

    Column = 0;
    for (Index = 0; Index < Pattern->Size - 1; Index += 1) {
        Character = Pattern->Data[Index];

        //
        // If it's printable, just print it out straight up.
        //

        if (isprint(Character)) {
            if (Column >= SED_PRINT_COLUMNS) {
                printf("\\\n");
                Column = 0;
            }

            printf("%c", Character);
            Column += 1;

        //
        // Print out an escape sequence if it's one of the common ones.
        //

        } else if ((Character == '\\') || (Character == '\a') ||
                   (Character == '\b') || (Character == '\f') ||
                   (Character == '\r') || (Character == '\v') ||
                   (Character == '\n')) {

            if (Column + 1 >= SED_PRINT_COLUMNS) {
                printf("\\\n");
                Column = 0;
            }

            if (Character == '\\') {
                Character = '\\';

            } else if (Character == '\a') {
                Character = 'a';

            } else if (Character == '\b') {
                Character = 'b';

            } else if (Character == '\f') {
                Character = 'f';

            } else if (Character == '\r') {
                Character = 'r';

            } else if (Character == '\t') {
                Character = 't';

            } else if (Character == '\v') {
                Character = 'v';

            } else if (Character == '\n') {
                Character = 'n';
            }

            printf("\\%c", Character);
            Column += 2;

        //
        // It's a weird character, print out its octal representation.
        //

        } else {
            if (Column + 3 > SED_PRINT_COLUMNS) {
                printf("\\\n");
                Column = 0;
            }

            printf("\\%03o", Character);
        }
    }

    printf("$\n");
    Context->StandardOut.LineTerminated = TRUE;
    return 0;
}

INT
SedExecuteMoveToNextLine (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "move to next line" command, which breaks out of
    the current cycle and moves to the next.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    CHAR Character;
    PSED_STRING Pattern;
    INT Status;

    assert((Command->Function.Type == SedFunctionMoveToNextLine) ||
           (Command->Function.Type == SedFunctionAppendNextLine));

    Pattern = Context->PatternSpace;
    if (Command->Function.Type == SedFunctionMoveToNextLine) {

        //
        // If directed, print the pattern space.
        //

        if (Context->PrintLines != FALSE) {
            SedPrint(Context, Pattern->Data, Context->LineTerminator);
        }

        //
        // Clear the current pattern space.
        //

        Pattern->Data[0] = '\0';
        Pattern->Size = 1;

    } else {
        Character = '\n';
        Status = SedAppendString(Pattern, &Character, 1);
        if (Status == FALSE) {
            return ENOMEM;
        }
    }

    //
    // Append the next line.
    //

    Status = SedReadLine(Context);
    if (Status != 0) {
        return Status;
    }

    //
    // If there was no more input, then move to the end of the script.
    //

    if (Context->Done != FALSE) {
        Context->NextCommand = NULL;
        Context->SkipPrint = TRUE;
    }

    return 0;
}

INT
SedExecuteWritePatternSpace (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a command to write all (p) or part (P) of the pattern
    space to standard out.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    UINTN Index;
    PSED_STRING Pattern;

    assert((Command->Function.Type == SedFunctionWritePattern) ||
           (Command->Function.Type == SedFunctionWritePatternToNewline));

    Pattern = Context->PatternSpace;

    assert(Pattern->Size != 0);

    if (Command->Function.Type == SedFunctionWritePattern) {
        printf("%s\n", Pattern->Data);

    } else {
        Index = 0;
        while ((Index < Pattern->Size) && (Pattern->Data[Index] != '\n')) {
            Index += 1;
        }

        if (Index == Pattern->Size) {
            SedWrite(&(Context->StandardOut),
                     Pattern->Data,
                     Pattern->Size - 1,
                     '\n');

        } else {
            SedWrite(&(Context->StandardOut),
                     Pattern->Data,
                     Index,
                     '\n');
        }
    }

    return 0;
}

INT
SedExecuteQuit (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a quit command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    Context->Quit = TRUE;
    return 0;
}

INT
SedExecuteSubstitute (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a substitute command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    UINTN InputEnd;
    UINTN InputStart;
    ULONG Occurrence;
    PSED_STRING Pattern;
    UINTN PatternOffset;
    UINTN PreviousEnd;
    PSTR Replace;
    ULONG ReplaceEnd;
    PSED_STRING Replacement;
    CHAR ReplacementCharacter;
    UINTN ReplacementIndex;
    UINTN ReplacementSize;
    ULONG ReplaceStart;
    int Result;
    LONG Subexpression;
    PSED_SUBSTITUTE Substitute;
    BOOL SubstitutionMade;
    UINTN TotalEndOffset;
    UINTN TotalStartOffset;
    BOOL WasBackslash;

    Pattern = Context->PatternSpace;
    PreviousEnd = 0;
    ReplaceEnd = 0;
    Replacement = NULL;
    ReplaceStart = 0;
    Substitute = &(Command->Function.U.Substitute);
    SubstitutionMade = FALSE;

    assert(Command->Function.Type == SedFunctionSubstitute);

    //
    // Loop making subsitutions.
    //

    Occurrence = 0;
    PatternOffset = 0;
    while (PatternOffset < Pattern->Size) {
        Result = regexec(&(Substitute->Expression),
                         Pattern->Data + PatternOffset,
                         Substitute->MatchCount,
                         Substitute->Matches,
                         0);

        //
        // If there was no match, stop now.
        //

        if (Result != 0) {
            break;
        }

        //
        // If there's a specific occurrence number and this isn't it, continue
        // on.
        //

        Occurrence += 1;
        if ((Substitute->OccurrenceNumber != 0) &&
            (Substitute->OccurrenceNumber != Occurrence)) {

            PatternOffset += Substitute->Matches[0].rm_eo;
            continue;
        }

        //
        // If this is an empty match right after a substitution, ignore it for
        // compatibility and move forward one.
        //

        if ((Substitute->Matches[0].rm_so == Substitute->Matches[0].rm_eo) &&
            (SubstitutionMade != FALSE) &&
            (PatternOffset == PreviousEnd)) {

            PatternOffset += 1;
            continue;
        }

        //
        // Generate the replacement. Start off by creating a copy of the
        // replacement template.
        //

        Replacement = SedCreateString(Substitute->Replacement->Data,
                                      Substitute->Replacement->Size,
                                      TRUE);

        //
        // Loop through the replacement converting as necessary.
        //

        ReplacementIndex = 0;
        WasBackslash = FALSE;
        Replace = Replacement->Data;
        ReplacementCharacter = 0;
        InputStart = 0;
        InputEnd = 0;
        while (ReplacementIndex < Replacement->Size) {
            if (WasBackslash != FALSE) {
                if (*Replace == '\\') {
                    ReplacementCharacter = '\\';

                } else if (*Replace == 'a') {
                    ReplacementCharacter = '\a';

                } else if (*Replace == 'b') {
                    ReplacementCharacter = '\b';

                } else if (*Replace == 'f') {
                    ReplacementCharacter = '\f';

                } else if (*Replace == 'r') {
                    ReplacementCharacter = '\r';

                } else if (*Replace == 't') {
                    ReplacementCharacter = '\t';

                } else if (*Replace == 'v') {
                    ReplacementCharacter = '\v';

                } else if (*Replace == 'n') {
                    ReplacementCharacter = '\n';

                } else if (isdigit(*Replace)) {
                    Subexpression = *Replace - '0';
                    if ((Substitute->Matches[Subexpression].rm_so != -1) &&
                        (Substitute->Matches[Subexpression].rm_eo != -1)) {

                        InputStart = Substitute->Matches[Subexpression].rm_so;
                        InputEnd = Substitute->Matches[Subexpression].rm_eo;

                    } else {
                        InputStart = 0;
                        InputEnd = 0;
                    }

                    ReplaceStart = ReplacementIndex - 1;
                    ReplaceEnd = ReplacementIndex + 1;

                } else {
                    ReplacementCharacter = *Replace;
                }

                if (ReplacementCharacter != 0) {
                    Result = SwStringReplaceRegion(&(Replacement->Data),
                                                   &(Replacement->Size),
                                                   &(Replacement->Capacity),
                                                   ReplacementIndex - 1,
                                                   ReplacementIndex + 1,
                                                   &ReplacementCharacter,
                                                   2);

                    if (Result == FALSE) {
                        Result = ENOMEM;
                        goto ExecuteSubstituteEnd;
                    }

                    //
                    // Back up since new characters just shifted down.
                    //

                    ReplacementIndex -= 1;
                    Replace = Replacement->Data + ReplacementIndex;
                    ReplacementCharacter = 0;
                }

            //
            // If it wasn't a backslash, the only special character is an
            // ampersand, which matches the whole thing.
            //

            } else {
                if (*Replace == '&') {

                    assert((Substitute->Matches[0].rm_so != -1) &&
                           (Substitute->Matches[0].rm_eo != -1));

                    InputStart = Substitute->Matches[0].rm_so;
                    InputEnd = Substitute->Matches[0].rm_eo;
                    ReplaceStart = ReplacementIndex;
                    ReplaceEnd = ReplacementIndex + 1;
                }
            }

            //
            // If someone requested a replacement with part of the input
            // string, go for it.
            //

            if (ReplaceStart != ReplaceEnd) {

                assert((InputStart + PatternOffset < Pattern->Size) &&
                       (InputEnd + PatternOffset <= Pattern->Size));

                //
                // The pattern offset is added because the regular expression
                // was searched with the input plus the pattern offset.
                //

                TotalStartOffset = InputStart + PatternOffset;
                Result = SwStringReplaceRegion(&(Replacement->Data),
                                               &(Replacement->Size),
                                               &(Replacement->Capacity),
                                               ReplaceStart,
                                               ReplaceEnd,
                                               Pattern->Data + TotalStartOffset,
                                               InputEnd - InputStart + 1);

                if (Result == FALSE) {
                    Result = ENOMEM;
                    goto ExecuteSubstituteEnd;
                }

                //
                // Reset the current index as the string was just moved around
                // and may have been replaced entirely.
                //

                ReplacementIndex = ReplaceStart + (InputEnd - InputStart) - 1;
                Replace = Replacement->Data + ReplacementIndex;
                ReplaceStart = 0;
                ReplaceEnd = 0;
            }

            if (*Replace == '\\') {
                WasBackslash = !WasBackslash;

            } else {
                WasBackslash = FALSE;
            }

            Replace += 1;
            ReplacementIndex += 1;
        }

        //
        // Now that the replacement string has finally been created, replace
        // the portion of the pattern that matched with the replacement string.
        // Again, the matches are all relative to the pattern offset.
        //

        ReplacementSize = Replacement->Size;

        assert((ReplacementSize != 0) &&
               (Substitute->Matches[0].rm_so != -1) &&
               (Substitute->Matches[0].rm_eo != -1));

        TotalStartOffset = Substitute->Matches[0].rm_so + PatternOffset;
        TotalEndOffset = Substitute->Matches[0].rm_eo + PatternOffset;
        Result = SwStringReplaceRegion(&(Pattern->Data),
                                       &(Pattern->Size),
                                       &(Pattern->Capacity),
                                       TotalStartOffset,
                                       TotalEndOffset,
                                       Replacement->Data,
                                       ReplacementSize);

        if (Result == FALSE) {
            Result = ENOMEM;
            goto ExecuteSubstituteEnd;
        }

        //
        // Move to the end of the replacement for the next substitution.
        //

        PatternOffset = TotalStartOffset + ReplacementSize - 1;
        SubstitutionMade = TRUE;
        SedDestroyString(Replacement);
        Replacement = NULL;

        //
        // If the global flag is off, stop.
        //

        if ((Substitute->Flags & SED_SUBSTITUTE_FLAG_GLOBAL) == 0) {
            break;
        }

        PreviousEnd = PatternOffset;
    }

    //
    // If a substitution was made and the caller wants it printed or written
    // to a file, do that now.
    //

    if ((SubstitutionMade != FALSE) &&
        ((Substitute->Flags & SED_SUBSTITUTE_FLAG_PRINT) != 0)) {

        SedPrint(Context, Pattern->Data, Context->LineTerminator);
    }

    if ((SubstitutionMade != FALSE) &&
        ((Substitute->Flags & SED_SUBSTITUTE_FLAG_WRITE) != 0)) {

        Result = SedWrite(Substitute->WriteFile,
                          Pattern->Data,
                          Pattern->Size - 1,
                          Context->LineTerminator);

        if (Result != 0) {
            goto ExecuteSubstituteEnd;
        }
    }

    //
    // Mark if a substitution was made for any future test commands.
    //

    Context->TestResult |= SubstitutionMade;
    Result = 0;

ExecuteSubstituteEnd:
    if (Replacement != NULL) {
        SedDestroyString(Replacement);
    }

    return Result;
}

INT
SedExecuteWriteFile (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "write to file" command, which writes the pattern
    space plus a newline.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    INT Result;
    PSED_WRITE_FILE WriteFile;

    assert(Command->Function.Type == SedFunctionWriteFile);
    assert(Context->PatternSpace->Size != 0);

    WriteFile = Command->Function.U.WriteFile;
    Result = SedWrite(WriteFile,
                      Context->PatternSpace->Data,
                      Context->PatternSpace->Size - 1,
                      Context->LineTerminator);

    return Result;
}

INT
SedExecuteExchangePatternAndHold (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes the "exchange pattern space and hold space" command,
    which does exactly what it sounds like.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    PSED_STRING Swap;

    assert(Command->Function.Type == SedFunctionExchangePatternAndHold);

    Swap = Context->PatternSpace;
    Context->PatternSpace = Context->HoldSpace;
    Context->HoldSpace = Swap;
    return 0;
}

INT
SedExecuteSubstituteCharacters (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes the "substitute characters" command, which replaces
    every occurrence of a character found in the first set with the replacement
    character specified by the second set.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    UINTN CharacterCount;
    UINTN CharacterIndex;
    PSTR Characters;
    PSTR Pattern;
    PSTR Replacement;

    assert(Command->Function.Type == SedFunctionSubstituteCharacters);

    Pattern = Context->PatternSpace->Data;
    CharacterCount =
                  Command->Function.U.CharacterSubstitute.Characters->Size - 1;

    assert(CharacterCount ==
           Command->Function.U.CharacterSubstitute.Replacement->Size - 1);

    Characters = Command->Function.U.CharacterSubstitute.Characters->Data;
    Replacement = Command->Function.U.CharacterSubstitute.Replacement->Data;
    while (*Pattern != '\0') {
        for (CharacterIndex = 0;
             CharacterIndex < CharacterCount;
             CharacterIndex += 1) {

            if (*Pattern == Characters[CharacterIndex]) {
                *Pattern = Replacement[CharacterIndex];

                assert(*Pattern != '\0');

                break;
            }
        }

        Pattern += 1;
    }

    return 0;
}

INT
SedExecuteNop (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "no-op" command, which does nothing.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    assert((Command->Function.Type == SedFunctionNop) ||
           (Command->Function.Type == SedFunctionLabel));

    return 0;
}

INT
SedExecuteWriteLineNumber (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine executes a "write line number", which prints out the decimal
    line number of the input plus a newline.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

{

    CHAR LineNumberBuffer[100];

    assert(Command->Function.Type == SedFunctionWriteLineNumber);

    snprintf(LineNumberBuffer,
             sizeof(LineNumberBuffer),
             "%lld",
             Context->LineNumber);

    SedPrint(Context, LineNumberBuffer, '\n');
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

