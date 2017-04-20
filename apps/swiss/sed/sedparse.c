/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sedparse.c

Abstract:

    This module implements the script parsing functions for the sed utility.

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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SED_REGEX_COMPILE_ERROR_STRING "Failed to compile regular expression: "

#define SED_BRACE_RECURSION_MAX 150

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
SedParseCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Parent,
    ULONG RecursionDepth,
    PSTR *ScriptPointer
    );

VOID
SedDestroyCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

BOOL
SedParseAddress (
    PSED_CONTEXT Context,
    PSTR *ScriptPointer,
    PSED_ADDRESS Address,
    PBOOL AddressFound
    );

VOID
SedDestroyAddress (
    PSED_ADDRESS Address
    );

BOOL
SedFindRegularExpressionEnd (
    PSED_CONTEXT Context,
    CHAR Delimiter,
    PSTR *ScriptPointer
    );

BOOL
SedParseFunction (
    PSED_CONTEXT Context,
    ULONG RecursionDepth,
    PSTR *ScriptPointer,
    PSED_COMMAND Command,
    PBOOL FunctionFound
    );

VOID
SedDestroyFunction (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function
    );

BOOL
SedCreateRegularExpression (
    PSED_CONTEXT Context,
    PSTR ExpressionString,
    UINTN Size,
    CHAR Delimiter,
    regex_t *Expression
    );

BOOL
SedAdvancePastBlanks (
    PSED_CONTEXT Context,
    PSTR *ScriptPointer
    );

BOOL
SedParseText (
    PSED_CONTEXT Context,
    BOOL AllowEscapes,
    BOOL EndAtSpace,
    PSTR *ScriptPointer,
    PSED_STRING *Text
    );

BOOL
SedParseSubstitute (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function,
    PSTR *ScriptPointer
    );

BOOL
SedParseCharacterSubstitution (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function,
    PSTR *ScriptPointer
    );

VOID
SedParseError (
    PSED_CONTEXT Context,
    PSTR QuotedArgument,
    PSTR String
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Convert the commands back to characters for debugging purposes.
//

CHAR SedCommandToCharacterTable[] = {
    '?',
    '{',
    'a',
    'b',
    'c',
    'd',
    'D',
    'g',
    'G',
    'h',
    'H',
    'i',
    'l',
    'n',
    'N',
    'p',
    'P',
    'q',
    'r',
    's',
    't',
    'w',
    'x',
    'y',
    ':',
    '=',
    '#',
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
SedAddScriptFile (
    PSED_CONTEXT Context,
    PSTR Path
    )

/*++

Routine Description:

    This routine loads a sed script contained in the file at the given path.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to a string containing the file path of the
        script file to load.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSED_STRING FileContents;
    PSED_SCRIPT_FRAGMENT Fragment;
    BOOL Result;

    Fragment = NULL;

    //
    // Read the file into a null terminated string buffer.
    //

    FileContents = SedReadFileIn(Path, TRUE);
    if (FileContents == NULL) {
        Result = FALSE;
        goto ParseScriptFileEnd;
    }

    //
    // Allocate and initialize the fragment so that if an error occurs during
    // parsing it can be tracked.
    //

    Fragment = malloc(sizeof(SED_SCRIPT_FRAGMENT));
    if (Fragment == NULL) {
        Result = FALSE;
        goto ParseScriptFileEnd;
    }

    memset(Fragment, 0, sizeof(SED_SCRIPT_FRAGMENT));
    Fragment->FileName = Path;
    Fragment->Offset = Context->ScriptString->Size;
    Fragment->Size = FileContents->Size;
    Result = SedAppendString(Context->ScriptString,
                             FileContents->Data,
                             Fragment->Size);

    if (Result == FALSE) {
        goto ParseScriptFileEnd;
    }

    SedAppendString(Context->ScriptString, "\n", 2);
    INSERT_BEFORE(&(Fragment->ListEntry), &(Context->ScriptList));
    Result = TRUE;

ParseScriptFileEnd:
    if (FileContents != NULL) {
        SedDestroyString(FileContents);
    }

    if (Result == FALSE) {
        if (Fragment != NULL) {
            free(Fragment);
        }
    }

    return Result;
}

BOOL
SedAddScriptString (
    PSED_CONTEXT Context,
    PSTR Script
    )

/*++

Routine Description:

    This routine loads a sed script into the current context.

Arguments:

    Context - Supplies a pointer to the application context.

    Script - Supplies a pointer to the null terminated string containing the
        script to load.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSED_SCRIPT_FRAGMENT Fragment;
    BOOL Result;

    //
    // Allocate and initialize the fragment so that if an error occurs during
    // parsing it can be tracked.
    //

    Fragment = malloc(sizeof(SED_SCRIPT_FRAGMENT));
    if (Fragment == NULL) {
        Result = FALSE;
        goto SedParseScriptStringEnd;
    }

    memset(Fragment, 0, sizeof(SED_SCRIPT_FRAGMENT));
    Context->CommandLineExpressionCount += 1;
    Fragment->ExpressionNumber = Context->CommandLineExpressionCount;
    Fragment->Offset = Context->ScriptString->Size;
    Fragment->Size = strlen(Script);
    Result = SedAppendString(Context->ScriptString, Script, Fragment->Size);
    if (Result == FALSE) {
        goto SedParseScriptStringEnd;
    }

    SedAppendString(Context->ScriptString, "\n", 2);
    INSERT_BEFORE(&(Fragment->ListEntry), &(Context->ScriptList));
    Result = TRUE;

SedParseScriptStringEnd:
    if (Result == FALSE) {
        if (Fragment != NULL) {
            free(Fragment);
        }
    }

    return Result;
}

BOOL
SedParseScript (
    PSED_CONTEXT Context,
    PSTR Script
    )

/*++

Routine Description:

    This routine loads a sed script into the current context.

Arguments:

    Context - Supplies a pointer to the application context.

    Script - Supplies a pointer to the null terminated string containing the
        script to load.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    Context->CharacterNumber = 1;

    //
    // If the first two characters of a script are #n, that's like turning on
    // the -n flag. The rest of the line is a comment.
    //

    if ((*Script == '#') && (*(Script + 1) == 'n')) {
        Context->PrintLines = FALSE;
    }

    //
    // Loop parsing commands.
    //

    Result = TRUE;
    while (TRUE) {
        if (*Script == '\0') {
            break;
        }

        Result = SedParseCommand(Context, &(Context->HeadCommand), 0, &Script);
        if (Result == FALSE) {
            goto ParseScriptEnd;
        }
    }

ParseScriptEnd:
    return Result;
}

VOID
SedDestroyCommands (
    PSED_CONTEXT Context
    )

/*++

Routine Description:

    This routine destroys any commands on the given context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

{

    PSED_COMMAND Command;

    while (LIST_EMPTY(&(Context->HeadCommand.Function.U.ChildList)) == FALSE) {
        Command = LIST_VALUE(Context->HeadCommand.Function.U.ChildList.Next,
                             SED_COMMAND,
                             ListEntry);

        LIST_REMOVE(&(Command->ListEntry));
        SedDestroyCommand(Context, Command);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
SedParseCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Parent,
    ULONG RecursionDepth,
    PSTR *ScriptPointer
    )

/*++

Routine Description:

    This routine parses a single sed command.

Arguments:

    Context - Supplies a pointer to the application context.

    Parent - Supplies an optional pointer to a parent command to put this new
        command under.

    RecursionDepth - Supplies the recursion depth of this command.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        command. On output, this pointer will be advanced beyond the command
        parsed.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL AddressFound;
    PSED_COMMAND Command;
    BOOL FunctionFound;
    BOOL Result;
    PSTR Script;

    Command = NULL;
    Script = *ScriptPointer;

    //
    // Avoid recursing too deeply.
    //

    if (RecursionDepth >= SED_BRACE_RECURSION_MAX) {
        SedParseError(Context, NULL, "Max brace recursion exceeded");
        return FALSE;
    }

    //
    // Advance past any blanks, semicolons, and newlines.
    //

    while (TRUE) {
        Result = SedAdvancePastBlanks(Context, &Script);
        if (Result == FALSE) {
            Result = TRUE;
            goto ParseCommandEnd;
        }

        if ((*Script == ';') || (*Script == '\n') || (*Script == '\r') ||
            (*Script == '\v') || (*Script == '\f')) {

            Script += 1;
            Context->CharacterNumber += 1;
            continue;
        }

        break;
    }

    //
    // Create the command structure.
    //

    Command = malloc(sizeof(SED_COMMAND));
    if (Command == NULL) {
        goto ParseCommandEnd;
    }

    memset(Command, 0, sizeof(SED_COMMAND));

    //
    // Attempt to parse the first address.
    //

    Result = SedParseAddress(Context,
                             &Script,
                             &(Command->Address[0]),
                             &AddressFound);

    if (Result == FALSE) {
        goto ParseCommandEnd;
    }

    //
    // If a first address was found, look for a comma and a second address.
    //

    if (AddressFound != FALSE) {
        Command->AddressCount = 1;
        SedAdvancePastBlanks(Context, &Script);
        if (*Script == ',') {
            Script += 1;
            Context->CharacterNumber += 1;
            SedAdvancePastBlanks(Context, &Script);
            Result = SedParseAddress(Context,
                                     &Script,
                                     &(Command->Address[1]),
                                     &AddressFound);

            if (Result == FALSE) {
                goto ParseCommandEnd;
            }

            if (AddressFound != FALSE) {
                Command->AddressCount += 1;
                SedAdvancePastBlanks(Context, &Script);
            }
        }
    }

    //
    // Look out for a bang, which seems to affect a command as a whole and
    // not a function specifically within a group (though the syntax allows it).
    //

    if (*Script == '!') {
        Command->AddressNegated = TRUE;
        Script += 1;
        Context->CharacterNumber += 1;
        SedAdvancePastBlanks(Context, &Script);
    }

    if ((*Script == '\0') || (*Script == '\n')) {
        SedParseError(Context, NULL, "Missing command");
    }

    //
    // Parse a function.
    //

    FunctionFound = FALSE;
    Result = SedParseFunction(Context,
                              RecursionDepth,
                              &Script,
                              Command,
                              &FunctionFound);

    if (Result == FALSE) {
        goto ParseCommandEnd;
    }

    //
    // Get past any blanks and semicolons on the end.
    //

    while ((*Script == ';') || (isspace(*Script))) {
        Script += 1;
        Context->CharacterNumber += 1;
        SedAdvancePastBlanks(Context, &Script);
    }

    if (FunctionFound == FALSE) {
        goto ParseCommandEnd;
    }

    //
    // Add the function either to the parent or the root context.
    //

    assert(Parent->Function.Type == SedFunctionGroup);

    Command->Parent = Parent;
    INSERT_BEFORE(&(Command->ListEntry), &(Parent->Function.U.ChildList));
    Result = TRUE;

ParseCommandEnd:
    if (Result == FALSE) {
        if (Command != NULL) {
            SedDestroyCommand(Context, Command);
            Command = NULL;
        }
    }

    *ScriptPointer = Script;
    return Result;
}

VOID
SedDestroyCommand (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    )

/*++

Routine Description:

    This routine destroys a sed command.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command.

Return Value:

    None.

--*/

{

    SedDestroyAddress(&(Command->Address[0]));
    SedDestroyAddress(&(Command->Address[1]));
    if (Command->Function.Type != SedFunctionInvalid) {
        SedDestroyFunction(Context, &(Command->Function));
    }

    free(Command);
    return;
}

BOOL
SedParseAddress (
    PSED_CONTEXT Context,
    PSTR *ScriptPointer,
    PSED_ADDRESS Address,
    PBOOL AddressFound
    )

/*++

Routine Description:

    This routine parses a sed address.

Arguments:

    Context - Supplies a pointer to the application context.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        command. On output, this pointer will be advanced beyond the command
        parsed.

    Address - Supplies a pointer to the address structure to fill in.

    AddressFound - Supplies a boolean indicating whether an address was
        successfully parsed or not.

Return Value:

    TRUE on success.

    FALSE on a real failure.

--*/

{

    PSTR AfterMatch;
    CHAR Delimiter;
    PSTR ExpressionBegin;
    BOOL Result;
    PSTR Script;

    *AddressFound = FALSE;
    Result = FALSE;
    Script = *ScriptPointer;

    //
    // If it's a dollar sign, that means the last input line.
    //

    if (*Script == '$') {
        *AddressFound = TRUE;
        Address->Type = SedAddressLastLine;
        Script += 1;

    //
    // If it's a digit, then this is a specific line number.
    //

    } else if (isdigit(*Script)) {
        Address->Type = SedAddressNumber;
        Address->U.Line = strtoll(Script, &AfterMatch, 10);
        if ((Address->U.Line < 0) || (AfterMatch == Script)) {
            SedParseError(Context, NULL, "Invalid address");
            goto ParseAddressEnd;
        }

        Script = AfterMatch;
        *AddressFound = TRUE;

    //
    // If it's a slash of some kind, this is a basic regular expression.
    //

    } else if ((*Script == '/') || (*Script == '\\')) {
        Delimiter = *Script;
        Script += 1;
        if (Delimiter == '\\') {
            Delimiter = *Script;
            Script += 1;
        }

        if ((Delimiter == '\\') || (Delimiter == '\n')) {
            SedParseError(Context, NULL, "Invalid address delimiter");
            goto ParseAddressEnd;
        }

        ExpressionBegin = Script;

        //
        // Find the ending delimiter.
        //

        Result = SedFindRegularExpressionEnd(Context, Delimiter, &Script);
        if (Result == FALSE) {
            SedParseError(Context, NULL, "Unterminated address expression");
            goto ParseAddressEnd;
        }

        assert(*Script == Delimiter);

        //
        // Make a regular expression out of this.
        //

        Result = SedCreateRegularExpression(
                                        Context,
                                        ExpressionBegin,
                                        (UINTN)Script - (UINTN)ExpressionBegin,
                                        Delimiter,
                                        &(Address->U.Expression));

        if (Result == FALSE) {
            goto ParseAddressEnd;
        }

        Address->Type = SedAddressExpression;

        //
        // Move over that delimiter.
        //

        Script += 1;
        *AddressFound = TRUE;
    }

    Result = TRUE;

ParseAddressEnd:
    Context->CharacterNumber += (UINTN)Script - (UINTN)(*ScriptPointer);
    *ScriptPointer = Script;
    return Result;
}

VOID
SedDestroyAddress (
    PSED_ADDRESS Address
    )

/*++

Routine Description:

    This routine destroys a sed address.

Arguments:

    Address - Supplies a pointer to the address structure to destroy.

Return Value:

    None.

--*/

{

    if (Address->Type == SedAddressExpression) {
        regfree(&(Address->U.Expression));
    }

    Address->Type = SedAddressInvalid;
    return;
}

BOOL
SedFindRegularExpressionEnd (
    PSED_CONTEXT Context,
    CHAR Delimiter,
    PSTR *ScriptPointer
    )

/*++

Routine Description:

    This routine finds the end of a regular expression.

Arguments:

    Context - Supplies a pointer to the application context.

    Delimiter - Supplies the delimiter to search for.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        command. On output, this pointer will be advanced beyond the command
        parsed.

Return Value:

    TRUE on success.

    FALSE on a real failure.

--*/

{

    ULONG BracketCount;
    CHAR FunkyBracket;
    BOOL Result;
    PSTR Script;
    BOOL WasBackslash;

    BracketCount = 0;
    FunkyBracket = 0;
    Script = *ScriptPointer;
    WasBackslash = FALSE;
    while (((*Script != '\n') || (WasBackslash != FALSE)) &&
           (*Script != '\0')) {

        //
        // Found the end if this is the delimiter.
        //

        if ((WasBackslash == FALSE) && (BracketCount == 0) &&
            (*Script == Delimiter)) {

            break;
        }

        //
        // If this is an open bracket, up the bracket count.
        //

        if ((WasBackslash == FALSE) && (*Script == '[') && (BracketCount < 2)) {
            BracketCount += 1;

            //
            // If not already in a funky bracket, look to see if this is a
            // funky bracket [: [= or [..
            //

            if (FunkyBracket == 0) {
                if ((*(Script + 1) == ':') || (*(Script + 1) == '=') ||
                    (*(Script + 1) == '.')) {

                    FunkyBracket = *(Script + 1);
                    Script += 1;

                //
                // This is not the opening of a funky bracket.
                //

                } else {

                    //
                    // There can only be two nested levels of brackets (regular
                    // and funky), and since this is not funky it must have
                    // just been a [ opener or [ inside a regular bracket
                    // expression.
                    //

                    BracketCount = 1;

                    //
                    // Skip a circumflex and closing brace if it comes right on
                    // the heels of the open bracket.
                    //

                    if (*(Script + 1) == '^') {
                        Script += 1;
                    }

                    if (*(Script + 1) == ']') {
                        Script += 1;
                    }
                }
            }
        }

        //
        // If this is a close brace, handle it. Don't bother validating that
        // it's the right funky bracket, but know that if there is a funky
        // bracket this is definitely it, as the nesting combos can only be
        // "regular" [asdf] , "regular, funky" [as[.d.]f], or "funky" [=a=].
        //

        if ((BracketCount != 0) && (*Script == ']')) {
            BracketCount -= 1;
            FunkyBracket = 0;
        }

        if ((BracketCount == 0) && (*Script == '\\')) {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        Script += 1;
    }

    Result = FALSE;
    if (*Script == Delimiter) {
        Result = TRUE;
    }

    *ScriptPointer = Script;
    return Result;
}

BOOL
SedParseFunction (
    PSED_CONTEXT Context,
    ULONG RecursionDepth,
    PSTR *ScriptPointer,
    PSED_COMMAND Command,
    PBOOL FunctionFound
    )

/*++

Routine Description:

    This routine parses a sed function (ie a command with the address already
    stripped off).

Arguments:

    Context - Supplies a pointer to the application context.

    RecursionDepth - Supplies the current recursion depth.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        command. On output, this pointer will be advanced beyond the command
        parsed.

    Command - Supplies a pointer to the command containing this function.

    FunctionFound - Supplies a pointer where a boolean will be returned
        indicating if a function was found or not.

Return Value:

    TRUE on success.

    FALSE on a real failure.

--*/

{

    ULONGLONG CharacterNumber;
    CHAR CommandString[2];
    PSED_STRING FileName;
    PSED_FUNCTION Function;
    BOOL Result;
    PSTR Script;
    INT Status;

    Function = &(Command->Function);
    *FunctionFound = TRUE;
    Result = TRUE;
    Script = *ScriptPointer;

    //
    // Get the command.
    //

    CommandString[0] = *Script;
    Script += 1;
    Context->CharacterNumber += 1;
    if ((CommandString[0] != 's') && (CommandString[0] != 'y')) {
        SedAdvancePastBlanks(Context, &Script);
    }

    switch (CommandString[0]) {
    case '{':
        Function->Type = SedFunctionGroup;
        INITIALIZE_LIST_HEAD(&(Function->U.ChildList));
        CharacterNumber = Context->CharacterNumber - 1;
        while ((*Script != '\0') && (*Script != '}')) {
            Result = SedParseCommand(Context,
                                     Command,
                                     RecursionDepth + 1,
                                     &Script);

            if (Result == FALSE) {
                goto ParseFunctionEnd;
            }

            //
            // Get past any blanks and semicolons.
            //

            while (*Script == ';') {
                Script += 1;
                Context->CharacterNumber += 1;
                SedAdvancePastBlanks(Context, &Script);
            }
        }

        if (*Script == '\0') {
            Context->CharacterNumber = CharacterNumber;
            SedParseError(Context, NULL, "Unterminated brace argument");
            Result = FALSE;
            goto ParseFunctionEnd;
        }

        //
        // Advance over the closing brace.
        //

        Script += 1;
        Context->CharacterNumber += 1;
        break;

    case '}':
        SedParseError(Context, NULL, "Unexpected closing brace '}'");
        Result = FALSE;
        goto ParseFunctionEnd;

    case 'a':
        Function->Type = SedFunctionPrintTextAtLineEnd;
        Result = SedParseText(Context,
                              TRUE,
                              FALSE,
                              &Script,
                              &(Function->U.StringArgument));

        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case 'b':
        Function->Type = SedFunctionBranch;
        if (*Script != '\0') {
            Result = SedParseText(Context,
                                  FALSE,
                                  TRUE,
                                  &Script,
                                  &(Function->U.StringArgument));

            if (Result == FALSE) {
                goto ParseFunctionEnd;
            }

        } else {
            Function->U.StringArgument = NULL;
        }

        break;

    case 'c':
        Function->Type = SedFunctionDeleteAndPrintText;
        Result = SedParseText(Context,
                              TRUE,
                              FALSE,
                              &Script,
                              &(Function->U.StringArgument));

        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case 'd':
        Function->Type = SedFunctionDelete;
        break;

    case 'D':
        Function->Type = SedFunctionDeleteToNewline;
        break;

    case 'g':
        Function->Type = SedFunctionReplacePatternWithHold;
        break;

    case 'G':
        Function->Type = SedFunctionAppendHoldToPattern;
        break;

    case 'h':
        Function->Type = SedFunctionReplaceHoldWithPattern;
        break;

    case 'H':
        Function->Type = SedFunctionAppendPatternToHold;
        break;

    case 'i':
        Function->Type = SedFunctionPrintText;
        Result = SedParseText(Context,
                              TRUE,
                              FALSE,
                              &Script,
                              &(Function->U.StringArgument));

        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case 'l':
        Function->Type = SedFunctionWritePatternEscaped;
        break;

    case 'n':
        Function->Type = SedFunctionMoveToNextLine;
        break;

    case 'N':
        Function->Type = SedFunctionAppendNextLine;
        break;

    case 'p':
        Function->Type = SedFunctionWritePattern;
        break;

    case 'P':
        Function->Type = SedFunctionWritePatternToNewline;
        break;

    case 'q':
        Function->Type = SedFunctionQuit;
        break;

    case 'r':
        Function->Type = SedFunctionReadFile;
        Result = SedParseText(Context,
                              FALSE,
                              FALSE,
                              &Script,
                              &(Function->U.StringArgument));

        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case 's':
        Function->Type = SedFunctionSubstitute;
        Result = SedParseSubstitute(Context, Function, &Script);
        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case 't':
        Function->Type = SedFunctionTest;
        if (*Script != '\0') {
            Result = SedParseText(Context,
                                  FALSE,
                                  TRUE,
                                  &Script,
                                  &(Function->U.StringArgument));

            if (Result == FALSE) {
                goto ParseFunctionEnd;
            }

        } else {
            Function->U.StringArgument = NULL;
        }

        break;

    case 'w':
        Function->Type = SedFunctionWriteFile;
        Result = SedParseText(Context,
                              FALSE,
                              FALSE,
                              &Script,
                              &FileName);

        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        Status = SedOpenWriteFile(Context, FileName, &(Function->U.WriteFile));
        SedDestroyString(FileName);
        if (Status != 0) {
            Result = FALSE;
            goto ParseFunctionEnd;
        }

        break;

    case 'x':
        Function->Type = SedFunctionExchangePatternAndHold;
        break;

    case 'y':
        Function->Type = SedFunctionSubstituteCharacters;
        Result = SedParseCharacterSubstitution(Context, Function, &Script);
        if (Result == FALSE) {
            goto ParseFunctionEnd;
        }

        break;

    case ':':
        Function->Type = SedFunctionLabel;
        if (*Script != '\0') {
            Result = SedParseText(Context,
                                  FALSE,
                                  TRUE,
                                  &Script,
                                  &(Function->U.StringArgument));

            if (Result == FALSE) {
                goto ParseFunctionEnd;
            }

        } else {
            Function->U.StringArgument = NULL;
        }

        break;

    case '=':
        Function->Type = SedFunctionWriteLineNumber;
        break;

    //
    // Handle a comment.
    //

    case '#':
        while ((*Script != '\n') && (*Script != '\0')) {
            Script += 1;
            Context->CharacterNumber += 1;
        }

        if (*Script == '\n') {
            Script += 1;
            Context->CharacterNumber += 1;
        }

        *FunctionFound = FALSE;
        break;

    case '\n':
        Script += 1;
        Context->CharacterNumber += 1;
        *FunctionFound = FALSE;
        break;

    case ';':
        Script += 1;
        Context->CharacterNumber += 1;
        *FunctionFound = FALSE;
        break;

    default:
        Context->CharacterNumber -= 1;
        CommandString[1] = '\0';
        SedParseError(Context, CommandString, "Unknown command");
        *FunctionFound = FALSE;
        Result = FALSE;
        break;
    }

ParseFunctionEnd:
    *ScriptPointer = Script;
    return Result;
}

VOID
SedDestroyFunction (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function
    )

/*++

Routine Description:

    This routine destroys a function.

Arguments:

    Context - Supplies a pointer to the application context.

    Function - Supplies a pointer to the function to destroy. It's assumed this
        function has already been removed from any list it's on.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSED_COMMAND Child;

    switch (Function->Type) {
    case SedFunctionGroup:
        while (LIST_EMPTY(&(Function->U.ChildList)) == FALSE) {
            Child = LIST_VALUE(Function->U.ChildList.Next,
                               SED_COMMAND,
                               ListEntry);

            LIST_REMOVE(&(Child->ListEntry));
            SedDestroyCommand(Context, Child);
        }

        break;

    case SedFunctionPrintTextAtLineEnd:
    case SedFunctionBranch:
    case SedFunctionDeleteAndPrintText:
    case SedFunctionPrintText:
    case SedFunctionReadFile:
    case SedFunctionTest:
    case SedFunctionLabel:
        if (Function->U.StringArgument != NULL) {
            SedDestroyString(Function->U.StringArgument);
        }

        break;

    case SedFunctionSubstitute:
        regfree(&(Function->U.Substitute.Expression));
        if (Function->U.Substitute.Replacement != NULL) {
            SedDestroyString(Function->U.Substitute.Replacement);
        }

        if (Function->U.Substitute.Matches != NULL) {
            free(Function->U.Substitute.Matches);
        }

        break;

    case SedFunctionSubstituteCharacters:
        if (Function->U.CharacterSubstitute.Characters != NULL) {
            SedDestroyString(Function->U.CharacterSubstitute.Characters);
        }

        if (Function->U.CharacterSubstitute.Replacement != NULL) {
            SedDestroyString(Function->U.CharacterSubstitute.Replacement);
        }

        break;

    case SedFunctionWriteFile:
    case SedFunctionDelete:
    case SedFunctionDeleteToNewline:
    case SedFunctionReplacePatternWithHold:
    case SedFunctionAppendHoldToPattern:
    case SedFunctionReplaceHoldWithPattern:
    case SedFunctionAppendPatternToHold:
    case SedFunctionWritePatternEscaped:
    case SedFunctionMoveToNextLine:
    case SedFunctionAppendNextLine:
    case SedFunctionWritePattern:
    case SedFunctionWritePatternToNewline:
    case SedFunctionQuit:
    case SedFunctionExchangePatternAndHold:
    case SedFunctionWriteLineNumber:
    case SedFunctionNop:
        break;

    default:

        assert(FALSE);

        break;
    }

    return;
}

BOOL
SedCreateRegularExpression (
    PSED_CONTEXT Context,
    PSTR ExpressionString,
    UINTN Size,
    CHAR Delimiter,
    regex_t *Expression
    )

/*++

Routine Description:

    This routine creates a regular expression from a string.

Arguments:

    Context - Supplies a pointer to the application context.

    ExpressionString - Supplies a pointer to the beginning of the string
        containing the regular expression.

    Size - Supplies the size of the string in bytes.

    Delimiter - Supplies the delimiter character.

    Expression - Supplies a pointer where the compiled expression will be
        returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    CHAR Character;
    UINTN CopyIndex;
    CHAR ErrorString[512];
    UINTN Index;
    ULONG RemainingSize;
    BOOL Result;
    int Status;
    PSED_STRING String;
    PSED_STRING SwapString;

    Result = FALSE;
    if (Size == 0) {
        String = Context->PreviousRegularExpression;
        if (String == NULL) {
            SedParseError(Context, NULL, "No previous regular expression");
            goto CreateRegularExpressionEnd;
        }

    } else {

        //
        // Create a copy of the expression string. This also null terminates it.
        //

        String = SedCreateString(ExpressionString, Size, TRUE);
        if (String == NULL) {
            goto CreateRegularExpressionEnd;
        }

        //
        // Go through the expression and 1) remove any backslashes followed by
        // the delimiter and 2) replace \n with the newline character.
        //

        Index = 0;
        while (Index < String->Size - 1) {
            if (String->Data[Index] == '\\') {
                Character = String->Data[Index + 1];
                if ((Character == 'n') || (Character == '\n') ||
                    (Character == Delimiter)) {

                    if (Character != Delimiter) {
                        Character = '\n';
                    }

                    //
                    // Replace the combination with the replacement character.
                    //

                    String->Data[Index] = Character;

                    //
                    // Move all the other characters down by one, and shrink
                    // the size by one.
                    //

                    for (CopyIndex = Index + 1;
                         CopyIndex < String->Size - 1;
                         CopyIndex += 1) {

                        String->Data[CopyIndex] = String->Data[CopyIndex + 1];
                    }

                    String->Size -= 1;
                }
            }

            Index += 1;
        }
    }

    //
    // A string was found. Compile that regular expression.
    //

    Status = regcomp(Expression, String->Data, 0);
    if (Status != 0) {

        assert(sizeof(ErrorString) > sizeof(SED_REGEX_COMPILE_ERROR_STRING));

        strcpy(ErrorString, SED_REGEX_COMPILE_ERROR_STRING);
        RemainingSize = sizeof(ErrorString) -
                        sizeof(SED_REGEX_COMPILE_ERROR_STRING) + 1;

        regerror(Status,
                 Expression,
                 ErrorString + sizeof(SED_REGEX_COMPILE_ERROR_STRING) - 1,
                 RemainingSize);

        SedParseError(Context, ExpressionString, ErrorString);
        goto CreateRegularExpressionEnd;
    }

    //
    // Make this the new previous regular expression if it isn't already.
    //

    if (String != Context->PreviousRegularExpression) {
        SwapString = String;
        String = Context->PreviousRegularExpression;
        Context->PreviousRegularExpression = SwapString;

    } else {
        String = NULL;
    }

    Result = TRUE;

CreateRegularExpressionEnd:
    if (String != NULL) {
        SedDestroyString(String);
    }

    return Result;
}

BOOL
SedAdvancePastBlanks (
    PSED_CONTEXT Context,
    PSTR *ScriptPointer
    )

/*++

Routine Description:

    This routine advances beyond any whitespace in the string.

Arguments:

    Context - Supplies a pointer to the application context.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        blanks. On output, this pointer will be advanced beyond any blanks.

Return Value:

    TRUE if there is more to the string.

    FALSE if the string terminated before non-blanks were found.

--*/

{

    PSTR Script;

    Script = *ScriptPointer;
    while (isblank(*Script)) {
        Context->CharacterNumber += 1;
        Script += 1;
    }

    *ScriptPointer = Script;
    if (*Script == '\0') {
        return FALSE;
    }

    return TRUE;
}

BOOL
SedParseText (
    PSED_CONTEXT Context,
    BOOL AllowEscapes,
    BOOL EndAtSpace,
    PSTR *ScriptPointer,
    PSED_STRING *Text
    )

/*++

Routine Description:

    This routine parses a single text argument, used by many commands.

Arguments:

    Context - Supplies a pointer to the application context.

    AllowEscapes - Supplies a boolean indicating whether backslash escapes
        text or not.

    EndAtSpace - Supplies a boolean indicating whether the text ends at the
        first space or not.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        blanks. On output, this pointer will be advanced beyond any blanks.

    Text - Supplies a pointer where a pointer to the allocated text to print
        will be returned on success. It is the caller's responsibility to
        destroy this string.

Return Value:

    TRUE on success.

    FALSE if the string terminated before non-blanks were found.

--*/

{

    CHAR Character;
    BOOL Result;
    PSTR Script;
    PSED_STRING String;
    BOOL TextAdded;
    BOOL WasBackslash;

    Result = FALSE;
    Script = *ScriptPointer;
    TextAdded = FALSE;
    String = SedCreateString(NULL, 0, TRUE);
    if (String == NULL) {
        goto ParseTextEnd;
    }

    //
    // If escapes are allowed and the first thing is an escaped newline, skip
    // it.
    //

    if ((AllowEscapes != FALSE) &&
        (*Script == '\\') && (*(Script + 1) == '\n')) {

        Script += 2;
        Context->LineNumber += 1;
    }

    WasBackslash = FALSE;
    while (TRUE) {
        Character = *Script;
        if (Character == '\0') {
            break;
        }

        Script += 1;
        if (Character == '\n') {
            Context->LineNumber += 1;
        }

        if ((WasBackslash != FALSE) && (AllowEscapes != FALSE)) {

            //
            // Certain characters can be escaped into fancier characters.
            //

            if (Character == 'n') {
                Character = '\n';

            } else if (Character == 'v') {
                Character = '\v';

            } else if (Character == 'f') {
                Character = '\f';

            } else if (Character == 't') {
                Character = '\t';

            } else if (Character == 'r') {
                Character = '\r';

            } else if (Character == 'b') {
                Character = '\b';

            } else if (Character == 'a') {
                Character = '\a';
            }

        } else {

            //
            // A non-escaped newline or sometimes a space is the end of the
            // text.
            //

            if ((Character == '\n') ||
                ((EndAtSpace != FALSE) &&
                 ((Character == ' ') || (Character == ';') ||
                  (Character == '#')))) {

                Script -= 1;
                break;
            }
        }

        //
        // Add this character as long as it wasn't a backslash (or escapes are
        // not allowed).
        //

        if ((AllowEscapes == FALSE) ||
            (Character != '\\') ||
            (WasBackslash != FALSE)) {

            Result = SedAppendString(String, &Character, 1);
            if (Result == FALSE) {
                goto ParseTextEnd;
            }

            TextAdded = TRUE;
        }

        //
        // Keep track of whether the previous character was a backslash.
        //

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }
    }

    Result = TRUE;

ParseTextEnd:
    Context->CharacterNumber += (UINTN)Script - (UINTN)(*ScriptPointer);
    *ScriptPointer = Script;
    if ((Result == FALSE) || (TextAdded == FALSE)) {
        if (String != NULL) {
            SedDestroyString(String);
            String = NULL;
        }
    }

    *Text = String;
    return Result;
}

BOOL
SedParseSubstitute (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function,
    PSTR *ScriptPointer
    )

/*++

Routine Description:

    This routine parses a substitute (s) function.

Arguments:

    Context - Supplies a pointer to the application context.

    Function - Supplies a pointer to the function to fill in.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        blanks. On output, this pointer will be advanced beyond any blanks.

Return Value:

    TRUE on success.

    FALSE if the string terminated before non-blanks were found.

--*/

{

    ULONG AllocationSize;
    BOOL Beginner;
    CHAR Delimiter;
    PSTR ExpressionBegin;
    ULONG Flags;
    CHAR FlagsString[2];
    PSED_STRING Replacement;
    PSTR ReplacementBegin;
    BOOL Result;
    PSTR Script;
    UINTN Size;
    int Status;
    BOOL WasBackslash;
    PSTR WriteFileBegin;
    PSED_STRING WriteFileName;

    assert(Function->Type == SedFunctionSubstitute);

    Beginner = FALSE;
    Result = FALSE;
    Script = *ScriptPointer;

    //
    // Get the delimiter.
    //

    Delimiter = *Script;
    Script += 1;
    if (Delimiter == '\0') {
        SedParseError(Context, NULL, "Expected argument for command s");
        goto ParseSubstituteEnd;
    }

    if ((Delimiter == '\\') || (Delimiter == '\n')) {
        SedParseError(Context, NULL, "Illegal delimiter for s command");
        goto ParseSubstituteEnd;
    }

    ExpressionBegin = Script;
    Result = SedFindRegularExpressionEnd(Context, Delimiter, &Script);
    if (Result == FALSE) {
        SedParseError(Context,
                      NULL,
                      "Unterminated regular expression in s command");

        goto ParseSubstituteEnd;
    }

    Size = (UINTN)Script - (UINTN)ExpressionBegin;
    if ((Size != 0) && (*ExpressionBegin == '^')) {
        Beginner = TRUE;
    }

    //
    // Create the regular expression.
    //

    Result = SedCreateRegularExpression(Context,
                                        ExpressionBegin,
                                        Size,
                                        Delimiter,
                                        &(Function->U.Substitute.Expression));

    if (Result == FALSE) {
        goto ParseSubstituteEnd;
    }

    //
    // Advance beyond the delimiter into the substitute string.
    //

    Script += 1;
    WasBackslash = FALSE;
    Size = 0;
    ReplacementBegin = Script;
    while (TRUE) {
        if (((*Script == '\n') && (WasBackslash == FALSE)) ||
            (*Script == '\0')) {

            SedParseError(Context,
                          NULL,
                          "Unterminated replacement text in s command");

            Result = FALSE;
            goto ParseSubstituteEnd;
        }

        if (WasBackslash == FALSE) {
            if (*Script == Delimiter) {
                break;
            }
        }

        if (*Script == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        Size += 1;
        Script += 1;
    }

    //
    // Create the replacement string.
    //

    Replacement = SedCreateString(ReplacementBegin, Size, TRUE);
    if (Replacement == NULL) {
        Result = FALSE;
        goto ParseSubstituteEnd;
    }

    Function->U.Substitute.Replacement = Replacement;

    //
    // Advance beyond the replacement into the flags.
    //

    Flags = 0;
    Script += 1;
    while (TRUE) {

        //
        // Watch out for the end.
        //

        if ((isblank(*Script)) || (*Script == '\n') || (*Script == '\0') ||
            (*Script == ';')) {

            break;

        //
        // If it's a digit, replace only the nth occurrence.
        //

        } else if (isdigit(*Script)) {
            Function->U.Substitute.OccurrenceNumber = *Script - '0';
            Script += 1;

        } else if (*Script == 'g') {
            if (Beginner == FALSE) {
                Flags |= SED_SUBSTITUTE_FLAG_GLOBAL;
            }

            Script += 1;

        } else if (*Script == 'p') {
            Flags |= SED_SUBSTITUTE_FLAG_PRINT;
            Script += 1;

        } else if (*Script == 'w') {
            Flags |= SED_SUBSTITUTE_FLAG_WRITE;
            Script += 1;

        } else {
            FlagsString[0] = *Script;
            FlagsString[1] = '\0';
            SedParseError(Context, FlagsString, "Unknown flag");
            Result = FALSE;
            goto ParseSubstituteEnd;
        }
    }

    Function->U.Substitute.Flags = Flags;
    while (isblank(*Script)) {
        Script += 1;
    }

    //
    // If the write flag is set, then the next argument is a file name.
    //

    if ((Flags & SED_SUBSTITUTE_FLAG_WRITE) != 0) {
        if ((*Script == '\0') || (*Script == '\n') || (*Script == ';')) {
            SedParseError(Context, NULL, "Expected write file name");
            Result = FALSE;
            goto ParseSubstituteEnd;
        }

        WriteFileBegin = Script;
        Size = 0;
        while ((*Script != '\0') && (*Script != '\n') && (*Script != ';')) {
            Script += 1;
            Size += 1;
        }

        WriteFileName = SedCreateString(WriteFileBegin, Size, TRUE);
        if (WriteFileName == NULL) {
            goto ParseSubstituteEnd;
        }

        Status = SedOpenWriteFile(Context,
                                  WriteFileName,
                                  &(Function->U.Substitute.WriteFile));

        SedDestroyString(WriteFileName);
        if (Status != 0) {
            Result = FALSE;
            goto ParseSubstituteEnd;
        }
    }

    //
    // Allocate a match array.
    //

    AllocationSize = 10 * sizeof(regmatch_t);
    Function->U.Substitute.Matches = malloc(AllocationSize);
    if (Function->U.Substitute.Matches == NULL) {
        Result = FALSE;
        goto ParseSubstituteEnd;
    }

    Function->U.Substitute.MatchCount = 10;
    Result = TRUE;

ParseSubstituteEnd:
    Context->CharacterNumber += (UINTN)Script - (UINTN)(*ScriptPointer);
    *ScriptPointer = Script;
    return Result;
}

BOOL
SedParseCharacterSubstitution (
    PSED_CONTEXT Context,
    PSED_FUNCTION Function,
    PSTR *ScriptPointer
    )

/*++

Routine Description:

    This routine parses the argument for a character replacement command.

Arguments:

    Context - Supplies a pointer to the application context.

    Function - Supplies a pointer to the character replacement function.

    ScriptPointer - Supplies a pointer to a pointer to the beginning of the
        blanks. On output, this pointer will be advanced beyond any blanks.

Return Value:

    TRUE on success.

    FALSE if the string terminated before non-blanks were found.

--*/

{

    CHAR Character;
    CHAR Delimiter;
    BOOL Result;
    PSTR Script;
    PSED_STRING String[2];
    ULONG StringIndex;
    BOOL WasBackslash;

    assert(Function->Type == SedFunctionSubstituteCharacters);

    Result = FALSE;
    Script = *ScriptPointer;
    String[0] = NULL;
    String[1] = NULL;

    //
    // Get the delimiter.
    //

    Delimiter = *Script;
    if ((Delimiter == '\0') || (Delimiter == '\n') || (Delimiter == '\\')) {
        SedParseError(Context,
                      NULL,
                      "Invalid character substitution delimimiter");

        goto ParseCharacterSubsitutionEnd;
    }

    Script += 1;

    //
    // Loop getting the two character arrays.
    //

    for (StringIndex = 0; StringIndex < 2; StringIndex += 1) {
        String[StringIndex] = SedCreateString(NULL, 0, TRUE);
        if (String[StringIndex] == NULL) {
            goto ParseCharacterSubsitutionEnd;
        }

        WasBackslash = FALSE;
        while (TRUE) {
            Character = *Script;
            Script += 1;
            if ((Character == '\0') || (Character == '\n')) {
                SedParseError(Context,
                              NULL,
                              "Unterminated character substitution");

                Result = FALSE;
                goto ParseCharacterSubsitutionEnd;
            }

            if (WasBackslash != FALSE) {

                //
                // Certain characters can be escaped into fancier characters.
                //

                if (Character == 'n') {
                    Character = '\n';

                } else if (Character == 'v') {
                    Character = '\v';

                } else if (Character == 'f') {
                    Character = '\f';

                } else if (Character == 't') {
                    Character = '\t';

                } else if (Character == 'r') {
                    Character = '\r';

                } else if (Character == 'b') {
                    Character = '\b';

                } else if (Character == 'a') {
                    Character = '\a';
                }

            } else {

                //
                // A non-escaped delimiter is the end.
                //

                if (Character == Delimiter) {
                    break;
                }
            }

            //
            // Add this character as long as it wasn't a backslash.
            //

            if ((Character != '\\') || (WasBackslash != FALSE)) {
                Result = SedAppendString(String[StringIndex], &Character, 1);
                if (Result == FALSE) {
                    goto ParseCharacterSubsitutionEnd;
                }
            }

            //
            // Keep track of whether the previous character was a backslash.
            //

            if (Character == '\\') {
                WasBackslash = !WasBackslash;

            } else {
                WasBackslash = FALSE;
            }
        }
    }

    //
    // The strings really need to be the same length.
    //

    if (String[0]->Size != String[1]->Size) {
        SedParseError(Context,
                      NULL,
                      "Character strings for 'y' are different lengths");

        Result = FALSE;
        goto ParseCharacterSubsitutionEnd;
    }

    Result = TRUE;

ParseCharacterSubsitutionEnd:
    Context->CharacterNumber += (UINTN)Script - (UINTN)(*ScriptPointer);
    *ScriptPointer = Script;
    if (Result == FALSE) {
        if (String[0] != NULL) {
            SedDestroyString(String[0]);
            String[0] = NULL;
        }

        if (String[1] != NULL) {
            SedDestroyString(String[1]);
            String[1] = NULL;
        }
    }

    Function->U.CharacterSubstitute.Characters = String[0];
    Function->U.CharacterSubstitute.Replacement = String[1];
    return Result;
}

VOID
SedParseError (
    PSED_CONTEXT Context,
    PSTR QuotedArgument,
    PSTR String
    )

/*++

Routine Description:

    This routine prints a sed script parsing error.

Arguments:

    Context - Supplies a pointer to the application context.

    QuotedArgument - Supplies an optional quoted string that will get put at
        the end of the error string.

    String - Supplies a pointer to a string containing the error to print.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PSTR CurrentString;
    PSED_SCRIPT_FRAGMENT Fragment;
    UINTN FragmentOffset;
    ULONG Line;
    UINTN Offset;

    Offset = Context->CharacterNumber;

    assert(LIST_EMPTY(&(Context->ScriptList)) == FALSE);

    //
    // Figure out which fragment this error originated in.
    //

    CurrentEntry = Context->ScriptList.Next;
    while (CurrentEntry != &(Context->ScriptList)) {
        Fragment = LIST_VALUE(CurrentEntry, SED_SCRIPT_FRAGMENT, ListEntry);
        if (Fragment->Offset + Fragment->Size > Offset) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    if (CurrentEntry == &(Context->ScriptList)) {
        Fragment = LIST_VALUE(Context->ScriptList.Previous,
                              SED_SCRIPT_FRAGMENT,
                              ListEntry);
    }

    //
    // The found fragment should really have started before the error.
    //

    assert(Fragment->Offset <= Offset);

    //
    // Figure out what line number the error was in.
    //

    CurrentString = Context->ScriptString->Data + Fragment->Offset - 1;
    FragmentOffset = 0;
    Line = 1;
    while (Fragment->Offset + FragmentOffset < Offset) {
        if (*CurrentString == '\n') {
            Line += 1;
        }

        FragmentOffset += 1;
        CurrentString += 1;
    }

    if (Fragment->ExpressionNumber != 0) {
        if (Line != 1) {
            SwPrintError(0,
                         QuotedArgument,
                         "Error parsing expression #%d, char %d, line %d: %s",
                         Fragment->ExpressionNumber,
                         (ULONG)FragmentOffset + 1,
                         Line,
                         String);

        } else {
            SwPrintError(0,
                         QuotedArgument,
                         "Error parsing expression #%d, char %d: %s",
                         Fragment->ExpressionNumber,
                         (ULONG)FragmentOffset + 1,
                         String);
        }

    } else {
        SwPrintError(0,
                     QuotedArgument,
                     "Error parsing file '%s', line %d, char %d: %s",
                     Fragment->FileName,
                     Line,
                     (ULONG)FragmentOffset + 1,
                     String);
    }

    return;
}

