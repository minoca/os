/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    expand.c

Abstract:

    This module implements variable expansion for the shell.

Author:

    Evan Green 10-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include "../swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the string buffer needed to convert the argument count
// integer to a string.
//

#define SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE 12
#define SHELL_ARGUMENT_LENGTH_STRING_BUFFER_SIZE 12

//
// Define the maximum size of the options string.
//

#define SHELL_OPTION_STRING_SIZE 15

//
// Define the maximum size of a shell prompt expansion.
//

#define SHELL_PROMPT_EXPANSION_MAX 255

//
// Define the maximum size of the time format buffer.
//

#define SHELL_PROMPT_TIME_FORMAT_MAX 50

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SHELL_PARAMETER_MODIFIER {
    ShellParameterModifierInvalid,
    ShellParameterModifierNone,
    ShellParameterModifierLength,
    ShellParameterModifierUseDefault,
    ShellParameterModifierAssignDefault,
    ShellParameterModifierError,
    ShellParameterModifierAlternative,
    ShellParameterModifierRemoveSmallestSuffix,
    ShellParameterModifierRemoveLargestSuffix,
    ShellParameterModifierRemoveSmallestPrefix,
    ShellParameterModifierRemoveLargestPrefix,
} SHELL_PARAMETER_MODIFIER, *PSHELL_PARAMETER_MODIFIER;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShPerformExpansionsCore (
    PSHELL Shell,
    ULONG Options,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShExpandNormalParameter (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShExpandSpecialParameter (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShExpandSubshell (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShExpandTilde (
    PSHELL Shell,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShExpandArithmeticExpression (
    PSHELL Shell,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    );

BOOL
ShCreateAllParametersString (
    PSHELL Shell,
    CHAR Separator,
    PSTR *NewString,
    PUINTN NewStringSize
    );

BOOL
ShCreateParameterCountString (
    PSHELL Shell,
    PSTR *NewString,
    PUINTN NewStringSize
    );

BOOL
ShCreateOptionsString (
    PSHELL Shell,
    PSTR *NewString,
    PUINTN NewStringSize
    );

VOID
ShGetPositionalArgument (
    PSHELL Shell,
    ULONG ArgumentNumber,
    PSTR *Argument,
    PUINTN ArgumentSize
    );

BOOL
ShAddExpansionRangeEntry (
    PLIST_ENTRY ListHead,
    SHELL_EXPANSION_TYPE Type,
    UINTN Index,
    UINTN Length
    );

VOID
ShTrimVariableValue (
    PSTR *Value,
    PUINTN ValueSize,
    PSTR Pattern,
    UINTN PatternSize,
    BOOL Prefix,
    BOOL Longest
    );

BOOL
ShEscapeSpecialCharacters (
    BOOL Quoted,
    PSTR *Value,
    PUINTN ValueSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShPerformExpansions (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize,
    ULONG Options,
    PSTR *ExpandedString,
    PUINTN ExpandedStringSize,
    PSTR **Fields,
    PULONG FieldCount
    )

/*++

Routine Description:

    This routine performs expansion on a given string.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string to expand.

    StringSize - Supplies a pointer to the size of the string in bytes
        including an assumed null terminator at the end.

    Options - Supplies the options for which expansions to perform. By default
        all expansions are performed, see SHELL_EXPANSION_OPTION_* for more
        details.

    ExpandedString - Supplies a pointer where a pointer to the expanded string
        will be returned on success. The caller is responsible for freeing this
        memory.

    ExpandedStringSize - Supplies a pointer where the size of the expanded
        string will be returned on success.

    Fields - Supplies a pointer where an array of pointers to strings will be
        returned representing the fields after field separator. This parameter
        is optional. The caller is responsible for freeing this memory.

    FieldCount - Supplies a pointer where the count of fields will be returned
        on success. This pointer is optional, but if the fields parameter is
        supplied this must be supplied as well.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN BufferCapacity;
    UINTN EndIndex;
    PSHELL_EXPANSION_RANGE Expansion;
    LIST_ENTRY ExpansionList;
    PSTR Field;
    ULONG FieldIndex;
    UINTN Index;
    BOOL Result;

    INITIALIZE_LIST_HEAD(&ExpansionList);
    if (Fields != NULL) {
        *Fields = NULL;
    }

    String = SwStringDuplicate(String, StringSize);
    if (String == NULL) {
        Result = FALSE;
        goto PerformExpansionsEnd;
    }

    //
    // Do most of the work of substituting the expansions and keeping a list of
    // them.
    //

    BufferCapacity = StringSize;
    Index = 0;
    EndIndex = StringSize;
    Result = ShPerformExpansionsCore(Shell,
                                     Options,
                                     &String,
                                     &StringSize,
                                     &BufferCapacity,
                                     &Index,
                                     &EndIndex,
                                     &ExpansionList);

    if (Result == FALSE) {
        goto PerformExpansionsEnd;
    }

    //
    // Perform field splitting and path expansion.
    //

    if ((Options & SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT) == 0) {

        assert((Fields != NULL) && (FieldCount != NULL));

        Result = ShFieldSplit(Shell,
                              &String,
                              &StringSize,
                              &ExpansionList,
                              0,
                              Fields,
                              FieldCount);

        if (Result == FALSE) {
            goto PerformExpansionsEnd;
        }

        if (((Shell->Options & SHELL_OPTION_NO_PATHNAME_EXPANSION) == 0) &&
            ((Options & SHELL_EXPANSION_OPTION_NO_PATH_EXPANSION) == 0)) {

            Result = ShPerformPathExpansions(Shell,
                                             &String,
                                             &StringSize,
                                             Fields,
                                             FieldCount);

            if (Result == FALSE) {
                goto PerformExpansionsEnd;
            }
        }

    } else {

        assert((Fields == NULL) && (FieldCount == NULL));

        ShDeNullExpansions(Shell, String, StringSize);
    }

    //
    // Perform quote removal.
    //

    if ((Options & SHELL_EXPANSION_OPTION_NO_QUOTE_REMOVAL) == 0) {
        if (Fields == NULL) {
            ShStringDequote(String, StringSize, Options, &StringSize);

        } else {
            for (FieldIndex = 0; FieldIndex < *FieldCount; FieldIndex += 1) {
                Field = (*Fields)[FieldIndex];
                ShStringDequote(Field, strlen(Field) + 1, Options, NULL);
            }
        }
    }

    Result = TRUE;

PerformExpansionsEnd:
    if (Result == FALSE) {
        if (String != NULL) {
            free(String);
            String = NULL;
        }

        if ((Fields != NULL) && (*Fields != NULL)) {
            free(*Fields);
            *FieldCount = 0;
        }

        StringSize = 0;
    }

    //
    // Free up the expansion list.
    //

    while (LIST_EMPTY(&ExpansionList) == FALSE) {
        Expansion = LIST_VALUE(ExpansionList.Next,
                               SHELL_EXPANSION_RANGE,
                               ListEntry);

        LIST_REMOVE(&(Expansion->ListEntry));
        free(Expansion);
    }

    *ExpandedString = String;
    *ExpandedStringSize = StringSize;
    return Result;
}

PLIST_ENTRY
ShGetCurrentArgumentList (
    PSHELL Shell
    )

/*++

Routine Description:

    This routine returns active argument list, which is either the current
    function executing or the shell's list.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    Returns a pointer to the active argument list.

--*/

{

    PLIST_ENTRY ArgumentList;
    PLIST_ENTRY CurrentEntry;
    PSHELL_EXECUTION_NODE ExecutionNode;

    ArgumentList = &(Shell->ArgumentList);

    //
    // If there's a function running, use that set of parameters, otherwise
    // use what the shell was invoked with.
    //

    CurrentEntry = Shell->ExecutionStack.Next;
    while (CurrentEntry != &(Shell->ExecutionStack)) {
        ExecutionNode = LIST_VALUE(CurrentEntry,
                                   SHELL_EXECUTION_NODE,
                                   ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if (ExecutionNode->Node->Type == ShellNodeFunction) {
            ArgumentList = &(ExecutionNode->ArgumentList);
            break;
        }
    }

    return ArgumentList;
}

BOOL
ShExpandPrompt (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize,
    PSTR *ExpandedString,
    PUINTN ExpandedStringSize
    )

/*++

Routine Description:

    This routine performs special prompt expansions on the given value.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string to expand.

    StringSize - Supplies a pointer to the size of the string in bytes
        including an assumed null terminator at the end.

    ExpandedString - Supplies a pointer where a pointer to the expanded string
        will be returned on success. The caller is responsible for freeing this
        memory.

    ExpandedStringSize - Supplies a pointer where the size of the expanded
        string will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN CharacterIndex;
    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    INT Difference;
    PSTR Expansion;
    CHAR ExpansionBuffer[SHELL_PROMPT_EXPANSION_MAX];
    INTN ExpansionSize;
    PSTR Home;
    UINTN HomeSize;
    UINTN Index;
    PSTR Period;
    UINTN RangeSize;
    BOOL Result;
    CHAR Specifier;
    INT Status;
    time_t Time;
    struct tm *TimeFields;
    PSTR TimeFormat;
    CHAR TimeFormatBuffer[SHELL_PROMPT_TIME_FORMAT_MAX];
    PSTR UserName;
    PSTR Working;
    UINTN WorkingCapacity;
    UINTN WorkingSize;

    UserName = NULL;
    WorkingSize = StringSize;
    WorkingCapacity = StringSize;
    Working = SwStringDuplicate(String, StringSize);
    if (Working == NULL) {
        Result = FALSE;
        goto ExpandPromptEnd;
    }

    Index = 0;

    //
    // Don't process the null terminator, or the very last character. Loop
    // processing all other characters.
    //

    while (Index + 2 < WorkingSize) {
        if (Working[Index] == '\\') {
            RangeSize = 2;
            Specifier = Working[Index + 1];
            Expansion = ExpansionBuffer;
            ExpansionSize = -1;

            //
            // \a is a bell character.
            //

            if (Specifier == 'a') {
                ExpansionBuffer[0] = '\a';
                ExpansionSize = 1;

            //
            // \e is an escape character.
            //

            } else if (Specifier == 'e') {
                ExpansionBuffer[0] = 0x1B;
                ExpansionSize = 1;

            //
            // \n is a newline.
            //

            } else if (Specifier == 'n') {
                ExpansionBuffer[0] = '\n';
                ExpansionSize = 1;

            //
            // \r is a carriage return.
            //

            } else if (Specifier == 'r') {
                ExpansionBuffer[0] = '\r';
                ExpansionSize = 1;

            //
            // \ is a literal backslash.
            //

            } else if (Specifier == '\\') {
                ExpansionBuffer[0] = SHELL_CONTROL_ESCAPE;
                ExpansionBuffer[1] = '\\';
                ExpansionSize = 2;

            //
            // \NNN is a character specified by the next one to three octal
            // characters.
            //

            } else if ((Specifier >= '0') && (Specifier <= '7')) {
                ExpansionBuffer[0] = 0;
                for (CharacterIndex = 0;
                     CharacterIndex < 3;
                     CharacterIndex += 1) {

                    if (Index + 1 + CharacterIndex >= WorkingSize) {
                        break;
                    }

                    Specifier = Working[Index + 1 + CharacterIndex];
                    if (!((Specifier >= '0') && (Specifier <= '7'))) {
                        break;
                    }

                    ExpansionBuffer[0] *= 8;
                    ExpansionBuffer[0] += Specifier - '0';
                }

                RangeSize = CharacterIndex + 1;
                ExpansionSize = 1;

            //
            // \xNN is a character specified by the next one to two hexadecimal
            // digits.
            //

            } else if (Specifier == 'x') {
                ExpansionBuffer[0] = SHELL_CONTROL_ESCAPE;
                ExpansionBuffer[1] = 0;
                for (CharacterIndex = 0;
                     CharacterIndex < 2;
                     CharacterIndex += 1) {

                    if (Index + 2 + CharacterIndex >= WorkingSize) {
                        break;
                    }

                    Specifier = Working[Index + 2 + CharacterIndex];
                    if (isdigit(Specifier)) {
                        Specifier -= '0';

                    } else if ((Specifier >= 'A') && (Specifier <= 'F')) {
                        Specifier = Specifier - 'A' + 0xA;

                    } else if ((Specifier >= 'a') && (Specifier <= 'f')) {
                        Specifier = Specifier - 'a' + 0xA;

                    } else {
                        break;
                    }

                    ExpansionBuffer[1] *= 16;
                    ExpansionBuffer[1] += Specifier;
                }

                RangeSize = CharacterIndex + 2;
                ExpansionSize = 2;

            //
            // $ comes out to # if the effective user ID is 0, or $ otherwise.
            // Since expansions haven't been performed yet, escape the
            // character.
            //

            } else if (Specifier == '$') {
                ExpansionBuffer[0] = SHELL_CONTROL_ESCAPE;
                ExpansionBuffer[1] = '$';
                ExpansionSize = 2;
                if (SwGetEffectiveUserId() == 0) {
                    ExpansionBuffer[1] = '#';
                }

            //
            // w comes out to the current working directory, with $HOME
            // abbreviated with a tilde. W comes out to the same except only
            // the basename the directory.
            //

            } else if ((Specifier == 'w') || (Specifier == 'W')) {
                Result = ShGetVariable(Shell,
                                       SHELL_HOME,
                                       sizeof(SHELL_HOME),
                                       &Home,
                                       &HomeSize);

                if (Result == FALSE) {
                    Home = NULL;
                    HomeSize = 0;
                }

                Result = ShGetVariable(Shell,
                                       SHELL_PWD,
                                       sizeof(SHELL_PWD),
                                       &CurrentDirectory,
                                       &CurrentDirectorySize);

                if (Result != FALSE) {
                    Difference = 1;

                    //
                    // Determine if home is a prefix of the current directory.
                    //

                    if ((HomeSize != 0) && (HomeSize <= CurrentDirectorySize)) {
                        Difference = strncmp(Home,
                                             CurrentDirectory,
                                             HomeSize - 1);
                    }

                    //
                    // If the user is at home, then it's just a ~ by itself.
                    //

                    if ((Difference == 0) &&
                        (CurrentDirectorySize == HomeSize)) {

                        ExpansionBuffer[0] = SHELL_CONTROL_ESCAPE;
                        ExpansionBuffer[1] = '~';
                        ExpansionSize = 2;

                    //
                    // If it's W, then the result is the basename.
                    //

                    } else if (Specifier == 'W') {
                        Expansion = basename(CurrentDirectory);
                        ExpansionSize = strlen(Expansion);

                    //
                    // The expansion is either the current directory directly,
                    // or ~/remainder.
                    //

                    } else {
                        if (Difference == 0) {
                            ExpansionBuffer[0] = SHELL_CONTROL_ESCAPE;
                            ExpansionBuffer[1] = '~';
                            strncpy(ExpansionBuffer + 2,
                                    CurrentDirectory + HomeSize - 1,
                                    SHELL_PROMPT_EXPANSION_MAX - 2);

                            ExpansionBuffer[SHELL_PROMPT_EXPANSION_MAX - 1] =
                                                                          '\0';

                            ExpansionSize = strlen(ExpansionBuffer);

                        } else {
                            Expansion = CurrentDirectory;
                            ExpansionSize = CurrentDirectorySize;
                            if (ExpansionSize != 0) {
                                ExpansionSize -= 1;
                            }
                        }
                    }
                }

            //
            // h is the hostname up to the first period. H is the complete
            // hostname.
            //

            } else if ((Specifier == 'h') || (Specifier == 'H')) {
                Status = SwGetHostName(ExpansionBuffer,
                                       SHELL_PROMPT_EXPANSION_MAX);

                if (Status != 0) {
                    ExpansionSize = 0;

                } else {
                    if (Specifier == 'h') {
                        Period = strchr(ExpansionBuffer, '.');
                        if (Period != NULL) {
                            *Period = '\0';
                        }
                    }

                    ExpansionSize = strlen(ExpansionBuffer);
                }

            //
            // u is the username.
            //

            } else if (Specifier == 'u') {
                Status = SwGetUserNameFromId(SwGetEffectiveUserId(), &UserName);
                if ((Status == 0) && (UserName != NULL)) {
                    Expansion = UserName;
                    ExpansionSize = strlen(UserName);

                } else {
                    ExpansionSize = 0;
                }

            //
            // [ and ] are used by bash to delineate control characters for
            // line counting purposes. This shell doesn't bother with that.
            //

            } else if ((Specifier == '[') || (Specifier == ']')) {
                ExpansionSize = 0;

            //
            // T is the current time in 12-hour HH:MM:SS format.
            // @ is the current time in 12-hour AM/PM format.
            // A is the current time in 24-hour HH:MM format.
            // t is the current time in 24-hour HH:MM:SS format.
            // d is the current date in weekday format: Tue Dec 9.
            // D is a custom format in {}.
            //

            } else if ((Specifier == 'T') || (Specifier == '@') ||
                       (Specifier == 'A') || (Specifier == 't') ||
                       (Specifier == 'd') || (Specifier == 'D')) {

                TimeFormat = "";
                if (Specifier == 'T') {
                    TimeFormat = "%I:%M:%S";

                } else if (Specifier == '@') {
                    TimeFormat = "%H:%M %p";

                } else if (Specifier == 'A') {
                    TimeFormat = "%H:%M";

                } else if (Specifier == 't') {
                    TimeFormat = "%H:%M:%S";

                } else if (Specifier == 'd') {
                    TimeFormat = "%a %b %d";

                } else if (Specifier == 'D') {
                    if ((Index + 2 <= WorkingSize) &&
                        (Working[Index + 2] == '{')) {

                        CharacterIndex = 0;
                        while ((CharacterIndex + 1 <
                                SHELL_PROMPT_TIME_FORMAT_MAX) &&
                               ((Index + 3 + CharacterIndex) <
                                 WorkingSize)) {

                            Specifier = Working[Index + 3 + CharacterIndex];
                            if (Specifier == '}') {
                                break;
                            }

                            TimeFormatBuffer[CharacterIndex] = Specifier;
                            CharacterIndex += 1;
                        }

                        TimeFormatBuffer[CharacterIndex] = '\0';
                        TimeFormat = TimeFormatBuffer;
                        RangeSize = CharacterIndex + 4;
                    }

                } else {

                    assert(FALSE);
                }

                Time = time(NULL);
                TimeFields = localtime(&Time);
                ExpansionBuffer[0] = '\0';
                strftime(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         TimeFormat,
                         TimeFields);

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // ! prints the history number.
            // # prints the command number. Currently both are the same.
            //

            } else if ((Specifier == '!') || (Specifier == '#')) {
                snprintf(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         "%d",
                         Shell->Lexer.LineNumber);

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // L prints the currently executing line number.
            //

            } else if (Specifier == 'L') {
                snprintf(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         "%d",
                         Shell->ExecutingLineNumber);

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // j prints the current number of active jobs.
            //

            } else if (Specifier == 'j') {

                //
                // TODO: This should be the number of active jobs.
                //

                snprintf(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         "%d",
                         0);

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // l prints the basename of the shell's terminal device.
            //

            } else if (Specifier == 'l') {
                ExpansionSize = 0;

            //
            // s prints the basename of $0 (the name of the shell).
            //

            } else if (Specifier == 's') {
                Expansion = basename(Shell->CommandName);
                ExpansionSize = strlen(Expansion);

            //
            // V prints the version, including revision number.
            //

            } else if (Specifier == 'V') {
                snprintf(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         "%d.%d.%d",
                         SH_VERSION_MAJOR,
                         SH_VERSION_MINOR,
                         SwGetSerialVersion());

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // v prints just the major and minor version numbers.
            //

            } else if (Specifier == 'v') {
                snprintf(ExpansionBuffer,
                         SHELL_PROMPT_EXPANSION_MAX,
                         "%d.%d",
                         SH_VERSION_MAJOR,
                         SH_VERSION_MINOR);

                ExpansionSize = strlen(ExpansionBuffer);

            //
            // This is an unrecognized expansion, just leave it as is.
            //

            } else {
                ExpansionSize = 0;
                RangeSize = 1;
            }

            //
            // Replace the string.
            //

            if (ExpansionSize != -1) {
                Result = SwStringReplaceRegion(&Working,
                                               &WorkingSize,
                                               &WorkingCapacity,
                                               Index,
                                               Index + RangeSize,
                                               Expansion,
                                               ExpansionSize + 1);

                if (Result == FALSE) {
                    goto ExpandPromptEnd;
                }

                Index += ExpansionSize;

            } else {
                Index += RangeSize;
            }

        } else {
            Index += 1;
        }
    }

    Result = TRUE;

ExpandPromptEnd:
    if (UserName != NULL) {
        free(UserName);
    }

    if (Result == FALSE) {
        if (Working != NULL) {
            free(Working);
            Working = NULL;
        }

        WorkingSize = 0;
    }

    *ExpandedString = Working;
    *ExpandedStringSize = WorkingSize;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShPerformExpansionsCore (
    PSHELL Shell,
    ULONG Options,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs expansion on a given string.

Arguments:

    Shell - Supplies a pointer to the shell.

    Options - Supplies the options for which expansions to perform. By default
        all expansions are performed, see SHELL_EXPANSION_OPTION_* for more
        details.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    CHAR Character;
    CHAR NextCharacter;
    BOOL Quoted;
    BOOL Result;
    UINTN Start;
    BOOL TildeExpansion;
    BOOL ValidFirstName;

    Start = *ExpansionIndex;
    TildeExpansion = TRUE;
    if ((Options & SHELL_EXPANSION_OPTION_NO_TILDE_EXPANSION) != 0) {
        TildeExpansion = FALSE;
    }

    Quoted = FALSE;
    while (*ExpansionIndex < *ExpansionEndIndex) {
        Character = *(*StringBufferAddress + *ExpansionIndex);

        //
        // If it's an escape control character, skip this character and the
        // next one.
        //

        if (Character == SHELL_CONTROL_ESCAPE) {
            *ExpansionIndex += 2;

            assert(*ExpansionIndex <= *ExpansionEndIndex);

            continue;

        //
        // Remember whether or not this portion of the string is inside a
        // quoted region.
        //

        } else if (Character == SHELL_CONTROL_QUOTE) {
            Quoted = !Quoted;
        }

        //
        // Handle a dollar sign expansion.
        //

        if (Character == '$') {
            if (*ExpansionIndex + 1 < *ExpansionEndIndex) {
                NextCharacter = *(*StringBufferAddress + *ExpansionIndex + 1);
                ValidFirstName = SHELL_NAME_FIRST_CHARACTER(NextCharacter);

                //
                // If it was a digit or a special parameter, then it's a
                // parameter expansion.
                //

                if (SHELL_SPECIAL_PARAMETER_CHARACTER(NextCharacter)) {
                    Result = ShExpandSpecialParameter(Shell,
                                                      Quoted,
                                                      StringBufferAddress,
                                                      StringBufferSize,
                                                      StringBufferCapacity,
                                                      ExpansionIndex,
                                                      ExpansionEndIndex,
                                                      ExpansionList);

                    if (Result == FALSE) {
                        goto PerformExpansionsCoreEnd;
                    }

                //
                // A single curly or a valid first name character is a
                // parameter expansion.
                //

                } else if ((ValidFirstName != FALSE) ||
                           (NextCharacter == '{')) {

                    Result = ShExpandNormalParameter(Shell,
                                                     Quoted,
                                                     StringBufferAddress,
                                                     StringBufferSize,
                                                     StringBufferCapacity,
                                                     ExpansionIndex,
                                                     ExpansionEndIndex,
                                                     ExpansionList);

                    if (Result == FALSE) {
                        goto PerformExpansionsCoreEnd;
                    }

                //
                // Note if it's a single parentheses. It could also be a double
                // parentheses, which would be arithmetic expansion.
                //

                } else if (NextCharacter == '(') {
                    if (*ExpansionIndex + 2 < *ExpansionEndIndex) {
                        NextCharacter =
                                 *(*StringBufferAddress + *ExpansionIndex + 2);

                        if (NextCharacter == '(') {
                            Result = ShExpandArithmeticExpression(
                                                          Shell,
                                                          StringBufferAddress,
                                                          StringBufferSize,
                                                          StringBufferCapacity,
                                                          ExpansionIndex,
                                                          ExpansionEndIndex,
                                                          ExpansionList);

                        } else {
                            Result = ShExpandSubshell(Shell,
                                                      Quoted,
                                                      StringBufferAddress,
                                                      StringBufferSize,
                                                      StringBufferCapacity,
                                                      ExpansionIndex,
                                                      ExpansionEndIndex,
                                                      ExpansionList);
                        }

                        if (Result == FALSE) {
                            goto PerformExpansionsCoreEnd;
                        }
                    }

                } else {
                    *ExpansionIndex += 1;
                }

            } else {
                *ExpansionIndex += 1;
            }

        //
        // If this is an unquoted tilde then it's the beginning of tilde
        // expansion. Tildes are only expanded at the start of the expansion or
        // right after a space.
        //

        } else if ((Character == '~') && (TildeExpansion != FALSE) &&
                   ((*ExpansionIndex == Start) ||
                    (isspace(*(*StringBufferAddress + *ExpansionIndex - 1))))) {

            assert(Quoted == FALSE);

            Result = ShExpandTilde(Shell,
                                   StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   ExpansionIndex,
                                   ExpansionEndIndex,
                                   ExpansionList);

            if (Result == FALSE) {
                goto PerformExpansionsCoreEnd;
            }

        //
        // If this is an unquoted backquote then it's the beginning of command
        // substitution.
        //

        } else if (Character == '`') {
            Result = ShExpandSubshell(Shell,
                                      Quoted,
                                      StringBufferAddress,
                                      StringBufferSize,
                                      StringBufferCapacity,
                                      ExpansionIndex,
                                      ExpansionEndIndex,
                                      ExpansionList);

            if (Result == FALSE) {
                goto PerformExpansionsCoreEnd;
            }

        //
        // No expansion, just move to the next character.
        //

        } else {
            *ExpansionIndex += 1;
        }
    }

    Result = TRUE;

PerformExpansionsCoreEnd:
    return Result;
}

BOOL
ShExpandNormalParameter (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs parameter substitution.

Arguments:

    Shell - Supplies a pointer to the shell.

    Quoted - Supplies a boolean indicating if the expansion is happening
        underneath double quotes or not.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AllocatedValue;
    UINTN ArgumentEnd;
    BOOL Curly;
    PLIST_ENTRY CurrentEntry;
    UINTN CurrentIndex;
    UINTN Delta;
    BOOL EndModifierFound;
    PSHELL_EXPANSION_RANGE Expansion;
    UINTN ExpansionInnerBegin;
    UINTN ExpansionInnerEnd;
    UINTN ExpansionOuterBegin;
    UINTN ExpansionOuterEnd;
    SHELL_EXPANSION_TYPE ExpansionType;
    CHAR FirstCharacter;
    UINTN Length;
    CHAR LengthBuffer[SHELL_ARGUMENT_LENGTH_STRING_BUFFER_SIZE];
    SHELL_PARAMETER_MODIFIER Modifier;
    UINTN ModifierBegin;
    LIST_ENTRY ModifierExpansionList;
    PSTR ModifierWord;
    UINTN ModifierWordSize;
    BOOL NullIsUnset;
    ULONG Options;
    UINTN OriginalEnd;
    LONG ParameterNumber;
    BOOL PropagateExpansions;
    BOOL Result;
    PSTR String;
    BOOL UsedModifier;
    PSTR Value;
    UINTN ValueCapacity;
    UINTN ValueSize;
    PSTR VariableName;
    PSTR VariableNameCopy;
    UINTN VariableNameSize;

    AllocatedValue = NULL;
    ExpansionOuterBegin = *ExpansionIndex;
    ExpansionType = ShellExpansionFieldSplit;
    Modifier = ShellParameterModifierNone;
    INITIALIZE_LIST_HEAD(&ModifierExpansionList);
    ModifierWord = NULL;
    ModifierWordSize = 0;
    ModifierBegin = 0;
    NullIsUnset = FALSE;
    ParameterNumber = -1;
    PropagateExpansions = FALSE;
    String = *StringBufferAddress;
    UsedModifier = FALSE;
    Value = NULL;
    ValueSize = 0;
    VariableName = NULL;
    VariableNameSize = 0;

    assert(ExpansionOuterBegin < *StringBufferSize);
    assert(String[ExpansionOuterBegin] == '$');

    CurrentIndex = ExpansionOuterBegin + 1;
    ExpansionInnerBegin = CurrentIndex;
    if (CurrentIndex >= *StringBufferSize) {
        Result = FALSE;
        goto ExpandNormalParameterEnd;
    }

    Result = ShScanPastExpansion(String + ExpansionOuterBegin,
                                 *StringBufferSize - ExpansionOuterBegin,
                                 &ExpansionOuterEnd);

    if (Result == FALSE) {
        goto ExpandNormalParameterEnd;
    }

    ExpansionOuterEnd += ExpansionOuterBegin;

    //
    // Remember if there's a curly at the beginning.
    //

    assert((String[CurrentIndex] == '{') ||
           (SHELL_NAME_FIRST_CHARACTER(String[CurrentIndex]) != FALSE));

    Curly = FALSE;
    if (String[CurrentIndex] == '{') {
        Curly = TRUE;
        ExpansionInnerBegin += 1;

        //
        // If there's a pound sign right after the curly, then it's actually
        // a request for the length of this expansion. But watch out for ${#}
        // on its own.
        //

        CurrentIndex += 1;
        if ((CurrentIndex + 1 < *StringBufferSize) &&
            (String[CurrentIndex] == '#') &&
            (String[CurrentIndex + 1] != '}')) {

            Modifier = ShellParameterModifierLength;
            ExpansionInnerBegin += 1;
            CurrentIndex += 1;
        }
    }

    //
    // Get the span of the name.
    //

    ExpansionInnerEnd = CurrentIndex;
    if ((CurrentIndex < *StringBufferSize) &&
        (SHELL_SPECIAL_PARAMETER_CHARACTER(String[CurrentIndex]))) {

        CurrentIndex += 1;
        ExpansionInnerEnd = CurrentIndex;

    } else {
        while (CurrentIndex < *StringBufferSize) {
            if ((SHELL_NAME_CHARACTER(String[CurrentIndex]) == FALSE) ||
                (String[CurrentIndex] == '#')) {

                break;
            }

            CurrentIndex += 1;
            ExpansionInnerEnd = CurrentIndex;
        }
    }

    if (CurrentIndex == *StringBufferSize) {
        Result = FALSE;
        goto ExpandNormalParameterEnd;
    }

    if (ExpansionInnerBegin == ExpansionInnerEnd) {
        Result = FALSE;
        goto ExpandNormalParameterEnd;
    }

    //
    // Look for modifiers if this is in a curly.
    //

    if (Curly != FALSE) {

        //
        // If there's an optional colon, then null is the same thing as unset
        // to other modifiers.
        //

        if (String[CurrentIndex] == ':') {
            NullIsUnset = TRUE;
            CurrentIndex += 1;
            if (CurrentIndex == *StringBufferSize) {
                Result = FALSE;
                goto ExpandNormalParameterEnd;
            }
        }

        //
        // Look for other modifiers.
        //

        EndModifierFound = TRUE;
        if (String[CurrentIndex] == '-') {
            PropagateExpansions = TRUE;
            Modifier = ShellParameterModifierUseDefault;

        } else if (String[CurrentIndex] == '=') {
            Modifier = ShellParameterModifierAssignDefault;

        } else if (String[CurrentIndex] == '?') {
            Modifier = ShellParameterModifierError;

        } else if (String[CurrentIndex] == '+') {
            PropagateExpansions = TRUE;
            Modifier = ShellParameterModifierAlternative;

        } else if (String[CurrentIndex] == '%') {
            Modifier = ShellParameterModifierRemoveSmallestSuffix;
            if ((CurrentIndex + 1 < *StringBufferSize) &&
                (String[CurrentIndex + 1] == '%')) {

                Modifier = ShellParameterModifierRemoveLargestSuffix;
                CurrentIndex += 1;
            }

        } else if (String[CurrentIndex] == '#') {
            Modifier = ShellParameterModifierRemoveSmallestPrefix;
            if ((CurrentIndex + 1 < *StringBufferSize) &&
                (String[CurrentIndex + 1] == '#')) {

                Modifier = ShellParameterModifierRemoveLargestPrefix;
                CurrentIndex += 1;
            }

        } else {
            EndModifierFound = FALSE;
        }

        //
        // If a modifier was found on the end, advance the string past that
        // character.
        //

        if (EndModifierFound != FALSE) {
            CurrentIndex += 1;
            if (CurrentIndex >= *StringBufferSize) {
                Result = FALSE;
                goto ExpandNormalParameterEnd;
            }
        }

        ArgumentEnd = ExpansionOuterEnd - 1;

        //
        // If there is an argument, expand it in place. Put the expansions on a
        // separate modifier list which will get merged onto the main list if
        // the modifier is chosen. The expansion list entries are important
        // because something like ${1+"$@"} does indeed split into multiple
        // fields.
        //

        Options = SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT;
        if (ArgumentEnd != CurrentIndex) {
            ModifierBegin = CurrentIndex;
            OriginalEnd = ArgumentEnd;
            Result = ShPerformExpansionsCore(Shell,
                                             Options,
                                             StringBufferAddress,
                                             StringBufferSize,
                                             StringBufferCapacity,
                                             &CurrentIndex,
                                             &ArgumentEnd,
                                             &ModifierExpansionList);

            if (Result == FALSE) {
                goto ExpandNormalParameterEnd;
            }

            String = *StringBufferAddress;
            *ExpansionEndIndex += ArgumentEnd - OriginalEnd;
            ExpansionOuterEnd += ArgumentEnd - OriginalEnd;

            //
            // Copy the modifier word.
            //

            ModifierWordSize = ArgumentEnd - ModifierBegin + 1;
            ModifierWord = SwStringDuplicate(String + ModifierBegin,
                                             ModifierWordSize);

            if (ModifierWord == NULL) {
                Result = FALSE;
                goto ExpandNormalParameterEnd;
            }

            //
            // Dequote the word if it's being used for pattern matching or
            // assignment, but not for the "use if (not) set".
            //

            if ((PropagateExpansions == FALSE) || (Quoted != FALSE)) {
                ShStringDequote(ModifierWord,
                                ModifierWordSize,
                                0,
                                &ModifierWordSize);
            }

            if (PropagateExpansions == FALSE) {
                ShDeNullExpansions(Shell, ModifierWord, ModifierWordSize);
            }
        }

        String = *StringBufferAddress;
        CurrentIndex = ArgumentEnd + 1;
    }

    //
    // If the first character was a digit, then this is a positional parameter.
    // Otherwise, it's a regular variable name.
    //

    FirstCharacter = String[ExpansionInnerBegin];
    if ((FirstCharacter >= '0') && (FirstCharacter <= '9')) {
        ParameterNumber = strtol(String + ExpansionInnerBegin, NULL, 10);
        if (ParameterNumber < 0) {
            return FALSE;
        }

        ShGetPositionalArgument(Shell, ParameterNumber, &Value, &ValueSize);

    //
    // If the first character was a special parameter, expand that. Tell the
    // special parameter expansion that no escaping is necessary because that's
    // going to be done here.
    //

    } else if (SHELL_SPECIAL_PARAMETER_CHARACTER(FirstCharacter)) {
        ValueSize = sizeof("$$");
        Value = malloc(ValueSize);
        if (Value == NULL) {
            Result = FALSE;
            goto ExpandNormalParameterEnd;
        }

        snprintf(Value, ValueSize, "$%c", FirstCharacter);
        ValueCapacity = ValueSize;
        Result = ShExpandSpecialParameter(Shell,
                                          FALSE,
                                          &Value,
                                          &ValueSize,
                                          &ValueCapacity,
                                          NULL,
                                          NULL,
                                          NULL);

        AllocatedValue = Value;
        if (Result == FALSE) {
            goto ExpandNormalParameterEnd;
        }

        if (FirstCharacter == '@') {
            ExpansionType = ShellExpansionSplitOnNull;
        }

    } else {
        VariableName = *StringBufferAddress + ExpansionInnerBegin;
        VariableNameSize = ExpansionInnerEnd - ExpansionInnerBegin + 1;
        ShGetVariable(Shell,
                      VariableName,
                      VariableNameSize,
                      &Value,
                      &ValueSize);
    }

    //
    // Run the value through any modifiers.
    //

    switch (Modifier) {
    case ShellParameterModifierNone:
        break;

    case ShellParameterModifierLength:
        Length = 0;
        if (Value != NULL) {
            Length = strlen(Value);
        }

        ValueSize = snprintf(LengthBuffer,
                             SHELL_ARGUMENT_LENGTH_STRING_BUFFER_SIZE,
                             "%d",
                             (LONG)Length) + 1;

        Value = LengthBuffer;
        if (AllocatedValue != NULL) {
            free(AllocatedValue);
            AllocatedValue = NULL;
        }

        break;

    case ShellParameterModifierUseDefault:
        if ((Value == NULL) || ((NullIsUnset != FALSE) && (ValueSize == 1))) {
            Value = ModifierWord;
            ValueSize = ModifierWordSize;
        }

        break;

    case ShellParameterModifierAssignDefault:
        if ((Value == NULL) || ((NullIsUnset != FALSE) && (ValueSize == 1))) {

            //
            // Only real variable names can be set with assignment.
            //

            if (VariableName == NULL) {
                Result = FALSE;
                goto ExpandNormalParameterEnd;
            }

            Result = ShSetVariable(Shell,
                                   VariableName,
                                   VariableNameSize,
                                   ModifierWord,
                                   ModifierWordSize);

            if (Result == FALSE) {
                goto ExpandNormalParameterEnd;
            }

            Value = ModifierWord;
            ValueSize = ModifierWordSize;
        }

        break;

    case ShellParameterModifierError:
        if ((Value == NULL) || ((NullIsUnset != FALSE) && (ValueSize == 1))) {
            if (VariableName != NULL) {
                VariableNameCopy = SwStringDuplicate(VariableName,
                                                     VariableNameSize);

                if (ModifierWord != NULL) {
                    fprintf(stderr, "%s: %s\n", VariableNameCopy, ModifierWord);

                } else {
                    fprintf(stderr,
                            "%s: parameter null or not set\n",
                            VariableNameCopy);
                }

                if (VariableNameCopy != NULL) {
                    free(VariableNameCopy);
                }

            } else {

                assert(ParameterNumber >= 0);

                if (ModifierWord != NULL) {
                    fprintf(stderr, "%d: %s\n", ParameterNumber, ModifierWord);

                } else {
                    fprintf(stderr,
                            "%d: parameter null or not set\n",
                            ParameterNumber);
                }
            }

            Result = FALSE;

            //
            // If ":?" or "?" expansion fails, a non-interactive shell is
            // supposed to exit. Set a non-zero return value. The
            // non-interactive check happens further up the stack.
            //

            Shell->ReturnValue = 1;
            goto ExpandNormalParameterEnd;
        }

        break;

    case ShellParameterModifierAlternative:
        if (!((Value == NULL) ||
              ((NullIsUnset != FALSE) && (ValueSize == 1)))) {

            Value = ModifierWord;
            ValueSize = ModifierWordSize;
        }

        break;

    case ShellParameterModifierRemoveSmallestSuffix:
        ShTrimVariableValue(&Value,
                            &ValueSize,
                            ModifierWord,
                            ModifierWordSize,
                            FALSE,
                            FALSE);

        break;

    case ShellParameterModifierRemoveLargestSuffix:
        ShTrimVariableValue(&Value,
                            &ValueSize,
                            ModifierWord,
                            ModifierWordSize,
                            FALSE,
                            TRUE);

        break;

    case ShellParameterModifierRemoveSmallestPrefix:
        ShTrimVariableValue(&Value,
                            &ValueSize,
                            ModifierWord,
                            ModifierWordSize,
                            TRUE,
                            FALSE);

        break;

    case ShellParameterModifierRemoveLargestPrefix:
        ShTrimVariableValue(&Value,
                            &ValueSize,
                            ModifierWord,
                            ModifierWordSize,
                            TRUE,
                            TRUE);

        break;

    default:

        assert(FALSE);

        return FALSE;
    }

    if (Value == ModifierWord) {
        UsedModifier = TRUE;
    }

    //
    // Ensure the value is heap allocated.
    //

    if ((Value != AllocatedValue) && (Value != ModifierWord)) {

        assert(AllocatedValue == NULL);

        Value = SwStringDuplicate(Value, ValueSize);
        if (Value == NULL) {
            Result = FALSE;
            goto ExpandNormalParameterEnd;
        }
    }

    //
    // Careful with the heap management here. The escape special characters
    // function may free the value and allocate something different.
    //

    if (Value == ModifierWord) {
        ModifierWord = NULL;
    }

    assert((AllocatedValue == NULL) || (AllocatedValue == Value));

    //
    // Don't escape character if it's an unquoted modifier word, as quote
    // removal was never performed on the original modifier.
    //

    if ((UsedModifier == FALSE) || (Quoted != FALSE)) {
        Result = ShEscapeSpecialCharacters(Quoted, &Value, &ValueSize);
    }

    AllocatedValue = Value;
    if (Result == FALSE) {
        goto ExpandNormalParameterEnd;
    }

    //
    // Replace the expansion region with the final value.
    //

    Result = SwStringReplaceRegion(StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   ExpansionOuterBegin,
                                   ExpansionOuterEnd,
                                   Value,
                                   ValueSize);

    if (Result == FALSE) {
        return FALSE;
    }

    if (ValueSize != 0) {
        ValueSize -= 1;
    }

    //
    // Either use the expansions from the modifier, or create one.
    //

    if ((UsedModifier != FALSE) && (PropagateExpansions != FALSE)) {
        while (LIST_EMPTY(&ModifierExpansionList) == FALSE) {
            CurrentEntry = ModifierExpansionList.Next;
            LIST_REMOVE(CurrentEntry);
            Expansion = LIST_VALUE(CurrentEntry,
                                   SHELL_EXPANSION_RANGE,
                                   ListEntry);

            //
            // Shift the expansion down. For the expansion ${x+...}, the
            // expanded range ... needs to be shifted down by "${x+".
            //

            assert(ModifierBegin > ExpansionOuterBegin);

            Expansion->Index -= ModifierBegin - ExpansionOuterBegin;
            INSERT_BEFORE(CurrentEntry, ExpansionList);
        }

    } else {
        Result = ShAddExpansionRangeEntry(ExpansionList,
                                          ExpansionType,
                                          ExpansionOuterBegin,
                                          ValueSize);

        if (Result == FALSE) {
            return FALSE;
        }
    }

    *ExpansionIndex += ValueSize;
    Delta = ValueSize - (ExpansionOuterEnd - ExpansionOuterBegin);
    *ExpansionEndIndex += Delta;

ExpandNormalParameterEnd:
    if (ModifierWord != NULL) {
        free(ModifierWord);
    }

    if (AllocatedValue != NULL) {
        free(AllocatedValue);
    }

    while (LIST_EMPTY(&ModifierExpansionList) == FALSE) {
        CurrentEntry = ModifierExpansionList.Next;
        LIST_REMOVE(CurrentEntry);
        Expansion = LIST_VALUE(CurrentEntry, SHELL_EXPANSION_RANGE, ListEntry);
        free(Expansion);
    }

    return Result;
}

BOOL
ShExpandSpecialParameter (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs parameter substitution for a special parameter.

Arguments:

    Shell - Supplies a pointer to the shell.

    Quoted - Supplies a boolean indicating if the expansion is happening
        underneath double quotes or not.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AllocatedValue;
    UINTN Delta;
    UINTN ExpansionOuterBegin;
    UINTN ExpansionOuterEnd;
    CHAR LocalBuffer[SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE];
    BOOL Result;
    CHAR SpecialCharacter;
    PSTR String;
    SHELL_EXPANSION_TYPE Type;
    PSTR Value;
    UINTN ValueSize;

    AllocatedValue = NULL;
    Result = FALSE;
    Type = ShellExpansionFieldSplit;
    Value = NULL;
    ValueSize = 0;
    String = *StringBufferAddress;
    ExpansionOuterBegin = 0;
    if (ExpansionIndex != NULL) {
        ExpansionOuterBegin = *ExpansionIndex;
    }

    ExpansionOuterEnd = ExpansionOuterBegin + 2;

    assert(ExpansionOuterEnd <= *StringBufferSize);
    assert(String[ExpansionOuterBegin] == '$');

    SpecialCharacter = String[ExpansionOuterBegin + 1];
    if ((SpecialCharacter >= '0') && (SpecialCharacter <= '9')) {
        ShGetPositionalArgument(Shell,
                                SpecialCharacter - '0',
                                &Value,
                                &ValueSize);

        Result = TRUE;

    } else {
        switch (SpecialCharacter) {

        //
        // The @ character expands to all positional parameters starting from
        // one. This is the only parameter that will split into separate fields
        // even if inside a double quote.
        //

        case '@':
            Type = ShellExpansionSplitOnNull;
            Result = ShCreateAllParametersString(Shell,
                                                 '\0',
                                                 &Value,
                                                 &ValueSize);

            AllocatedValue = Value;
            break;

        //
        // The * parameter expands to all positional parameters starting from
        // one If inside double quotes, it will expand all to one field.
        //

        case '*':
            Type = ShellExpansionFieldSplit;
            Result = ShCreateAllParametersString(Shell,
                                                 ' ',
                                                 &Value,
                                                 &ValueSize);

            AllocatedValue = Value;
            break;

        //
        // The # character expands to the decimal number of parameters, not
        // counting 0 (the command name).
        //

        case '#':
            Result = ShCreateParameterCountString(Shell, &Value, &ValueSize);
            AllocatedValue = Value;
            break;

        //
        // The ? character expands to the decimal exit status of the most
        // recent pipeline.
        //

        case '?':
            ValueSize = snprintf(LocalBuffer,
                                 SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE,
                                 "%d",
                                 Shell->LastReturnValue);

            if (ValueSize == -1) {
                goto ExpandSpecialParameterEnd;
            }

            ValueSize += 1;
            Value = LocalBuffer;
            Result = TRUE;
            break;

        //
        // The hyphen character expands to the current option flags (the
        // single letter option names concatenated into a string) as specified
        // on invocation, by the set command, or implicitly by the shell.
        //

        case '-':
            Result = ShCreateOptionsString(Shell, &Value, &ValueSize);
            AllocatedValue = Value;
            break;

        //
        // The dollar sign expands to the decimal process ID of the invoked
        // shell. Subshells retain the same process ID as their parent shells.
        //

        case '$':
            ValueSize = snprintf(LocalBuffer,
                                 SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE,
                                 "%d",
                                 Shell->ProcessId);

            if (ValueSize == -1) {
                goto ExpandSpecialParameterEnd;
            }

            ValueSize += 1;
            Value = LocalBuffer;
            Result = TRUE;
            break;

        //
        // The ! character expands to the decimal process ID of the most
        // recent background command executed from the current shell.
        // Background commands in subshells don't affect this parameter.
        //

        case '!':
            ValueSize = snprintf(LocalBuffer,
                                 SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE,
                                 "%d",
                                 Shell->LastBackgroundProcessId);

            if (ValueSize == -1) {
                goto ExpandSpecialParameterEnd;
            }

            ValueSize += 1;
            Value = LocalBuffer;
            Result = TRUE;
            break;

        default:

            assert(FALSE);

            return FALSE;
        }
    }

    if (Result == FALSE) {
        goto ExpandSpecialParameterEnd;
    }

    //
    // Make sure the value is heap allocated.
    //

    if ((AllocatedValue == NULL) && (Value != NULL)) {
        AllocatedValue = SwStringDuplicate(Value, ValueSize);
        if (AllocatedValue == NULL) {
            Result = FALSE;
            goto ExpandSpecialParameterEnd;
        }

        Value = AllocatedValue;
    }

    Result = ShEscapeSpecialCharacters(Quoted, &Value, &ValueSize);
    if (Result == FALSE) {
        goto ExpandSpecialParameterEnd;
    }

    AllocatedValue = Value;

    //
    // Replace the expansion region with the final value.
    //

    Result = SwStringReplaceRegion(StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   ExpansionOuterBegin,
                                   ExpansionOuterEnd,
                                   Value,
                                   ValueSize);

    if (Result == FALSE) {
        goto ExpandSpecialParameterEnd;
    }

    if (ValueSize != 0) {
        ValueSize -= 1;
    }

    //
    // Take note of the expansion if requested.
    //

    Result = ShAddExpansionRangeEntry(ExpansionList,
                                      Type,
                                      ExpansionOuterBegin,
                                      ValueSize);

    if (Result == FALSE) {
        goto ExpandSpecialParameterEnd;
    }

    if (ExpansionIndex != NULL) {
        *ExpansionIndex += ValueSize;
    }

    Delta = ValueSize - (ExpansionOuterEnd - ExpansionOuterBegin);
    if (ExpansionEndIndex != NULL) {
        *ExpansionEndIndex += Delta;
    }

ExpandSpecialParameterEnd:
    if (AllocatedValue != NULL) {
        free(AllocatedValue);
    }

    return Result;
}

BOOL
ShExpandSubshell (
    PSHELL Shell,
    BOOL Quoted,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs command substitution.

Arguments:

    Shell - Supplies a pointer to the shell.

    Quoted - Supplies a boolean indicating whether the expansion is in double
        quotes or not.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN Delta;
    BOOL DequoteForSubshell;
    PSTR Input;
    UINTN InputIndex;
    UINTN InputSize;
    UINTN OuterEndIndex;
    PSTR Output;
    UINTN OutputSize;
    UINTN QuoteIndex;
    BOOL Result;
    INT ReturnValue;
    PSTR String;
    PSHELL Subshell;
    BOOL WasBackslash;

    assert(*ExpansionIndex <= *StringBufferSize);

    Input = NULL;
    Output = NULL;
    OutputSize = 0;
    String = *StringBufferAddress;
    Subshell = NULL;
    InputIndex = *ExpansionIndex;
    Result = ShScanPastExpansion(String + InputIndex,
                                 *ExpansionEndIndex - InputIndex,
                                 &InputSize);

    if (Result == FALSE) {
        return FALSE;
    }

    assert(InputSize > 0);

    OuterEndIndex = InputIndex + InputSize;

    //
    // Move the inner string in to remove the `...` or $(...).
    //

    DequoteForSubshell = FALSE;
    if (String[InputIndex] == '`') {
        InputIndex += 1;
        InputSize -= 2;
        DequoteForSubshell = TRUE;

    } else {

        assert(String[InputIndex] == '$');

        InputIndex += 2;
        InputSize -= 3;
    }

    //
    // Create a copy of the input.
    //

    InputSize += 1;
    Input = SwStringDuplicate(String + InputIndex, InputSize);
    if (Input == NULL) {
        Result = FALSE;
        goto ExpandSubshellEnd;
    }

    //
    // If already inside of double quotes, remove any backslashes in a \"
    // combination.
    //

    if ((Quoted != FALSE) && (DequoteForSubshell != FALSE)) {
        WasBackslash = FALSE;
        for (QuoteIndex = 0; QuoteIndex < InputSize; QuoteIndex += 1) {
            if ((WasBackslash != FALSE) && (Input[QuoteIndex] == '"')) {
                SwStringRemoveRegion(Input, &InputSize, QuoteIndex - 1, 1);
                QuoteIndex -= 1;
                WasBackslash = FALSE;
                continue;
            }

            if (Input[QuoteIndex] == '\\') {
                WasBackslash = !WasBackslash;

            } else {
                WasBackslash = FALSE;
            }
        }
    }

    //
    // Create and execute a subshell.
    //

    Subshell = ShCreateSubshell(Shell, Input, InputSize, DequoteForSubshell);
    if (Subshell == NULL) {
        Result = FALSE;
        goto ExpandSubshellEnd;
    }

    Result = ShExecuteSubshell(Shell,
                               Subshell,
                               FALSE,
                               &Output,
                               &OutputSize,
                               &ReturnValue);

    if (Result == FALSE) {
        goto ExpandSubshellEnd;
    }

    //
    // Save the subshell's result as the most recent result in this parent
    // shell.
    //

    Shell->ReturnValue = ReturnValue;

    //
    // Remove any trailing newlines from the output.
    //

    if (OutputSize != 0) {
        while ((OutputSize != 0) &&
               ((Output[OutputSize - 1] == '\n') ||
                (Output[OutputSize - 1] == '\r'))) {

            OutputSize -= 1;
        }

        //
        // Add one for the presumed (but not used) null terminator.
        //

        OutputSize += 1;
    }

    //
    // Escape any fancy characters that shouldn't get interpreted by the shell.
    //

    Result = ShEscapeSpecialCharacters(Quoted, &Output, &OutputSize);
    if (Result == FALSE) {
        goto ExpandSubshellEnd;
    }

    //
    // Now replace the expansion with the output.
    //

    Result = SwStringReplaceRegion(StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   *ExpansionIndex,
                                   OuterEndIndex,
                                   Output,
                                   OutputSize);

    if (Result == FALSE) {
        goto ExpandSubshellEnd;
    }

    if (OutputSize != 0) {
        OutputSize -= 1;
    }

    //
    // Take note of the expansion if requested.
    //

    Result = ShAddExpansionRangeEntry(ExpansionList,
                                      ShellExpansionFieldSplit,
                                      *ExpansionIndex,
                                      OutputSize);

    if (Result == FALSE) {
        goto ExpandSubshellEnd;
    }

    Delta = OutputSize - (OuterEndIndex - *ExpansionIndex);
    *ExpansionEndIndex += Delta;
    *ExpansionIndex += OutputSize;

ExpandSubshellEnd:
    if (Subshell != NULL) {
        ShDestroyShell(Subshell);
    }

    if (Input != NULL) {
        free(Input);
    }

    if (Output != NULL) {
        free(Output);
    }

    return Result;
}

BOOL
ShExpandTilde (
    PSHELL Shell,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs tilde expansion.

Arguments:

    Shell - Supplies a pointer to the shell.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AllocatedHome;
    UINTN Delta;
    BOOL DidExpansion;
    PSTR Home;
    INT HomeSize;
    UINTN HomeSizeBig;
    UINTN InputIndex;
    UINTN InputSize;
    UINTN OuterEndIndex;
    BOOL Result;
    PSTR String;

    assert(*ExpansionIndex <= *StringBufferSize);

    AllocatedHome = NULL;
    DidExpansion = FALSE;
    Home = NULL;
    HomeSize = 0;
    String = *StringBufferAddress;
    InputIndex = *ExpansionIndex;

    assert(String[InputIndex] == '~');

    Result = ShScanPastExpansion(String + InputIndex,
                                 *ExpansionEndIndex - InputIndex,
                                 &InputSize);

    if (Result == FALSE) {
        return FALSE;
    }

    assert(InputSize > 0);

    OuterEndIndex = InputIndex + InputSize;

    //
    // Move the inner string in to remove the ~
    //

    InputIndex += 1;
    InputSize -= 1;

    //
    // If there was no user specified, just get the value of the home variable.
    //

    if (InputSize == 0) {
        Result = ShGetVariable(Shell,
                               SHELL_HOME,
                               sizeof(SHELL_HOME),
                               &Home,
                               &HomeSizeBig);

        if (Result == FALSE) {
            Result = TRUE;
            goto ExpandTildeEnd;
        }

        HomeSize = HomeSizeBig;

    //
    // Get the home directory of a specific user.
    //

    } else {
        Result = ShGetHomeDirectory(String + InputIndex,
                                    InputSize + 1,
                                    &AllocatedHome,
                                    &HomeSize);

        if (Result == FALSE) {
            Result = TRUE;
            goto ExpandTildeEnd;
        }

        Home = AllocatedHome;
    }

    //
    // Now replace the expansion with the home path.
    //

    Result = SwStringReplaceRegion(StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   *ExpansionIndex,
                                   OuterEndIndex,
                                   Home,
                                   HomeSize);

    if (Result == FALSE) {
        goto ExpandTildeEnd;
    }

    if (HomeSize != 0) {
        HomeSize -= 1;
    }

    //
    // Take note of the expansion if requested.
    //

    Result = ShAddExpansionRangeEntry(ExpansionList,
                                      ShellExpansionFieldSplit,
                                      *ExpansionIndex,
                                      HomeSize);

    if (Result == FALSE) {
        goto ExpandTildeEnd;
    }

    Delta = HomeSize - (OuterEndIndex - *ExpansionIndex);
    *ExpansionEndIndex += Delta;
    *ExpansionIndex += HomeSize;
    DidExpansion = TRUE;

ExpandTildeEnd:

    //
    // If the expansion wasn't done, move the current expansion index past the
    // expansion so this routine doesn't get called again for the same spot.
    //

    if ((Result != FALSE) && (DidExpansion == FALSE)) {
        *ExpansionIndex = OuterEndIndex;
    }

    if (AllocatedHome != NULL) {
        free(AllocatedHome);
    }

    return Result;
}

BOOL
ShExpandArithmeticExpression (
    PSHELL Shell,
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PUINTN ExpansionIndex,
    PUINTN ExpansionEndIndex,
    PLIST_ENTRY ExpansionList
    )

/*++

Routine Description:

    This routine performs arithmetic expression expansion.

Arguments:

    Shell - Supplies a pointer to the shell.

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    ExpansionIndex - Supplies a pointer that on input contains the index of
        the first character of the expansion. On output, will contain the index
        immediately after the expansion.

    ExpansionEndIndex - Supplies a pointer that on input contains the ending
        index to search for expansions. On output, this will be updated to
        reflect any expansions.

    ExpansionList - Supplies an optional pointer to the list of expansions
        that have occurred on this buffer. Any expansions here will be added
        to the end of this list.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UINTN Delta;
    PSTR ExpandedString;
    UINTN ExpandedStringSize;
    UINTN InputIndex;
    UINTN InputSize;
    ULONG Options;
    UINTN OuterIndex;
    PSTR Output;
    UINTN OutputSize;
    BOOL Result;
    PSTR String;

    assert(*ExpansionIndex <= *StringBufferSize);

    ExpandedString = NULL;
    Output = NULL;
    OutputSize = 0;
    String = *StringBufferAddress;
    InputIndex = *ExpansionIndex;
    Result = ShScanPastExpansion(String + InputIndex,
                                 *ExpansionEndIndex - InputIndex,
                                 &InputSize);

    if (Result == FALSE) {
        return FALSE;
    }

    assert(InputSize > 5);

    //
    // Move the input beyond the $((, and decrease the size to remove both $((
    // and )).
    //

    OuterIndex = InputIndex + InputSize;
    InputIndex += 3;
    InputSize -= 5;

    //
    // Expand anything inside the expression.
    //

    Options = SHELL_EXPANSION_OPTION_NO_TILDE_EXPANSION |
              SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT;

    Result = ShPerformExpansions(Shell,
                                 String + InputIndex,
                                 InputSize + 1,
                                 Options,
                                 &ExpandedString,
                                 &ExpandedStringSize,
                                 NULL,
                                 NULL);

    if (Result == FALSE) {
        goto ExpandArithmeticExpressionEnd;
    }

    //
    // Evaluate the arithmetic statement.
    //

    Result = ShEvaluateArithmeticExpression(Shell,
                                            ExpandedString,
                                            ExpandedStringSize,
                                            &Output,
                                            &OutputSize);

    if (Result == FALSE) {
        goto ExpandArithmeticExpressionEnd;
    }

    //
    // Now replace the expansion with the output.
    //

    Result = SwStringReplaceRegion(StringBufferAddress,
                                   StringBufferSize,
                                   StringBufferCapacity,
                                   *ExpansionIndex,
                                   OuterIndex,
                                   Output,
                                   OutputSize);

    if (Result == FALSE) {
        goto ExpandArithmeticExpressionEnd;
    }

    if (OutputSize != 0) {
        OutputSize -= 1;
    }

    //
    // Take note of the expansion if requested.
    //

    Result = ShAddExpansionRangeEntry(ExpansionList,
                                      ShellExpansionFieldSplit,
                                      *ExpansionIndex,
                                      OutputSize);

    if (Result == FALSE) {
        goto ExpandArithmeticExpressionEnd;
    }

    Delta = OutputSize - (OuterIndex - *ExpansionIndex);
    *ExpansionEndIndex += Delta;
    *ExpansionIndex += OutputSize;

ExpandArithmeticExpressionEnd:
    if (Output != NULL) {
        free(Output);
    }

    if (ExpandedString != NULL) {
        free(ExpandedString);
    }

    return Result;
}

BOOL
ShCreateAllParametersString (
    PSHELL Shell,
    CHAR Separator,
    PSTR *NewString,
    PUINTN NewStringSize
    )

/*++

Routine Description:

    This routine creates a string containing all the positional arguments, not
    including the command name.

Arguments:

    Shell - Supplies a pointer to the shell.

    Separator - Supplies the separator character to use between each
        positional arguments.

    NewString - Supplies a pointer where a pointer to the string containing
        all the arguments will be returned on success. The caller is
        responsible for freeing this memory.

    NewStringSize - Supplies a pointer where the size of the new string buffer
        including the null terminator will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_ARGUMENT Argument;
    PLIST_ENTRY ArgumentList;
    UINTN BufferSize;
    PLIST_ENTRY CurrentEntry;
    PSTR CurrentString;
    PSTR Line;
    BOOL NoSeparator;
    BOOL Result;
    PSTR Separators;
    UINTN SeparatorsSize;

    ArgumentList = ShGetCurrentArgumentList(Shell);
    Line = NULL;
    BufferSize = 0;
    if (LIST_EMPTY(ArgumentList) != FALSE) {
        Result = TRUE;
        goto CreateAllParametersStringEnd;
    }

    NoSeparator = FALSE;
    BufferSize = 0;

    //
    // If the separator is space, then it's a $* expansion. Use the first
    // character of the IFS variable, none if IFS is set to NULL, or a space if
    // IFS is unset.
    //

    if (Separator == ' ') {
        Result = ShGetVariable(Shell,
                               SHELL_IFS,
                               sizeof(SHELL_IFS),
                               &Separators,
                               &SeparatorsSize);

        if (Result != FALSE) {

            assert(SeparatorsSize != 0);

            Separator = Separators[0];
            if (Separator == '\0') {
                NoSeparator = TRUE;
            }
        }
    }

    Result = FALSE;

    //
    // Loop through once to figure out how big this buffer needs to be.
    //

    CurrentEntry = ArgumentList->Next;
    while (CurrentEntry != ArgumentList) {
        Argument = LIST_VALUE(CurrentEntry, SHELL_ARGUMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        assert(Argument->NameSize != 0);

        //
        // The buffer space needed is the size of the string minus one because
        // of the null terminator plus one because of space.
        //

        BufferSize += Argument->NameSize - 1;
        if (NoSeparator == FALSE) {
            BufferSize += 1;
        }
    }

    //
    // Add one for the null terminator if the superfluous separator wasn't
    // added.
    //

    if (NoSeparator != FALSE) {
        BufferSize += 1;
    }

    //
    // Allocate the buffer.
    //

    Line = malloc(BufferSize);
    if (Line == NULL) {
        goto CreateAllParametersStringEnd;
    }

    CurrentString = Line;

    //
    // Loop through again and copy the parameters in, separated by spaces.
    //

    CurrentEntry = ArgumentList->Next;
    while (CurrentEntry != ArgumentList) {
        Argument = LIST_VALUE(CurrentEntry, SHELL_ARGUMENT, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        assert(Argument->NameSize != 0);

        strcpy(CurrentString, Argument->Name);
        CurrentString += Argument->NameSize - 1;
        if (NoSeparator == FALSE) {
            *CurrentString = Separator;
            CurrentString += 1;
        }
    }

    //
    // That last space isn't needed so back it out and make it the null
    // terminator.
    //

    if (NoSeparator == FALSE) {
        CurrentString -= 1;
    }

    *CurrentString = '\0';
    CurrentString += 1;

    assert((UINTN)CurrentString - (UINTN)Line == BufferSize);

    Result = TRUE;

CreateAllParametersStringEnd:
    if (Result == FALSE) {
        BufferSize = 0;
        if (Line != NULL) {
            free(Line);
            Line = NULL;
        }
    }

    *NewString = Line;
    *NewStringSize = BufferSize;
    return Result;
}

BOOL
ShCreateParameterCountString (
    PSHELL Shell,
    PSTR *NewString,
    PUINTN NewStringSize
    )

/*++

Routine Description:

    This routine creates a string containing the number of command arguments to
    the most recent function or shell invocation.

Arguments:

    Shell - Supplies a pointer to the shell.

    NewString - Supplies a pointer where a pointer to the string containing
        all the arguments will be returned on success. The caller is
        responsible for freeing this memory.

    NewStringSize - Supplies a pointer where the size of the new string buffer
        including the null terminator will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG ArgumentCount;
    PLIST_ENTRY ArgumentList;
    UINTN BufferSize;
    PLIST_ENTRY CurrentEntry;
    PSTR Line;
    CHAR LocalBuffer[SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE];
    BOOL Result;

    ArgumentCount = 0;
    ArgumentList = ShGetCurrentArgumentList(Shell);
    Line = NULL;
    Result = FALSE;

    //
    // Loop through to count arguments.
    //

    CurrentEntry = ArgumentList->Next;
    while (CurrentEntry != ArgumentList) {
        CurrentEntry = CurrentEntry->Next;
        ArgumentCount += 1;
    }

    //
    // Convert that number using a local buffer.
    //

    BufferSize = snprintf(LocalBuffer,
                          SHELL_ARGUMENT_COUNT_STRING_BUFFER_SIZE,
                          "%d",
                          ArgumentCount);

    if (BufferSize == -1) {
        goto CreateParameterCountStringEnd;
    }

    //
    // Include one for the null terminator and copy the string.
    //

    BufferSize += 1;
    Line = SwStringDuplicate(LocalBuffer, BufferSize);
    if (Line == NULL) {
        goto CreateParameterCountStringEnd;
    }

    Result = TRUE;

CreateParameterCountStringEnd:
    if (Result == FALSE) {
        BufferSize = 0;
        if (Line != NULL) {
            free(Line);
            Line = NULL;
        }
    }

    *NewString = Line;
    *NewStringSize = BufferSize;
    return Result;
}

BOOL
ShCreateOptionsString (
    PSHELL Shell,
    PSTR *NewString,
    PUINTN NewStringSize
    )

/*++

Routine Description:

    This routine creates a string containing the single letter options used or
    the current shell invocation.

Arguments:

    Shell - Supplies a pointer to the shell.

    NewString - Supplies a pointer where a pointer to the string containing
        all the options will be returned on success. The caller is
        responsible for freeing this memory.

    NewStringSize - Supplies a pointer where the size of the new string buffer
        including the null terminator will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Options;
    PSTR OptionsString;
    UINTN OutputIndex;

    OptionsString = malloc(SHELL_OPTION_STRING_SIZE);
    if (OptionsString == NULL) {
        *NewString = NULL;
        *NewStringSize = 0;
        return FALSE;
    }

    memset(OptionsString, 0, SHELL_OPTION_STRING_SIZE);
    Options = Shell->Options;
    OutputIndex = 0;

    //
    // Look at each option.
    //

    if ((Options & SHELL_OPTION_EXPORT_ALL) != 0) {
        OptionsString[OutputIndex] = 'a';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_ASYNCHRONOUS_JOB_NOTIFICATION) != 0) {
        OptionsString[OutputIndex] = 'b';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_NO_CLOBBER) != 0) {
        OptionsString[OutputIndex] = 'C';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_DEBUG) != 0) {
        OptionsString[OutputIndex] = 'd';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_EXIT_ON_FAILURE) != 0) {
        OptionsString[OutputIndex] = 'e';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_NO_PATHNAME_EXPANSION) != 0) {
        OptionsString[OutputIndex] = 'f';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_LOCATE_UTILITIES_IN_DECLARATION) != 0) {
        OptionsString[OutputIndex] = 'h';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_INTERACTIVE) != 0) {
        OptionsString[OutputIndex] = 'i';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_RUN_JOBS_IN_SEPARATE_PROCESS_GROUP) != 0) {
        OptionsString[OutputIndex] = 'm';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_NO_EXECUTE) != 0) {
        OptionsString[OutputIndex] = 'n';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_READ_FROM_STDIN) != 0) {
        OptionsString[OutputIndex] = 's';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_EXIT_ON_UNSET_VARIABLE) != 0) {
        OptionsString[OutputIndex] = 'u';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_DISPLAY_INPUT) != 0) {
        OptionsString[OutputIndex] = 'v';
        OutputIndex += 1;
    }

    if ((Options & SHELL_OPTION_TRACE_COMMAND) != 0) {
        OptionsString[OutputIndex] = 'x';
        OutputIndex += 1;
    }

    assert(OutputIndex < SHELL_OPTION_STRING_SIZE);

    *NewString = OptionsString;
    *NewStringSize = OutputIndex + 1;
    return TRUE;
}

VOID
ShGetPositionalArgument (
    PSHELL Shell,
    ULONG ArgumentNumber,
    PSTR *Argument,
    PUINTN ArgumentSize
    )

/*++

Routine Description:

    This routine returns the value for a positional argument.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentNumber - Supplies the argument number to get. Zero gets the command,
        one gets the first positional argument.

    Argument - Supplies a pointer where a pointer to the argument string will
        be returned on success. The caller does not own this buffer, and must
        not free it.

    ArgumentSize - Supplies a pointer where the size of the returned argument
        buffer in bytes including the null terminator will be returned.

Return Value:

    None.

--*/

{

    PSHELL_ARGUMENT ArgumentEntry;
    ULONG ArgumentIndex;
    PLIST_ENTRY ArgumentList;
    PLIST_ENTRY CurrentEntry;

    if (ArgumentNumber == 0) {
        *Argument = Shell->CommandName;
        *ArgumentSize = Shell->CommandNameSize;
        return;
    }

    ArgumentList = ShGetCurrentArgumentList(Shell);
    ArgumentIndex = 1;
    CurrentEntry = ArgumentList->Next;
    while (CurrentEntry != ArgumentList) {
        ArgumentEntry = LIST_VALUE(CurrentEntry, SHELL_ARGUMENT, ListEntry);
        if (ArgumentIndex == ArgumentNumber) {
            *Argument = ArgumentEntry->Name;
            *ArgumentSize = ArgumentEntry->NameSize;
            return;
        }

        ArgumentIndex += 1;
        CurrentEntry = CurrentEntry->Next;
    }

    *Argument = NULL;
    *ArgumentSize = 0;
    return;
}

BOOL
ShAddExpansionRangeEntry (
    PLIST_ENTRY ListHead,
    SHELL_EXPANSION_TYPE Type,
    UINTN Index,
    UINTN Length
    )

/*++

Routine Description:

    This routine allocates an expansion range entry, initializes it, and
    places it on the end of the given list.

Arguments:

    ListHead - Supplies an optional pointer to the head of the list. If NULL,
        this routine is a no-op.

    Type - Supplies the expansion type.

    Index - Supplies the byte offset index of the expansion.

    Length - Supplies the length of the expansion in bytes.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_EXPANSION_RANGE Range;

    if ((ListHead == NULL) ||
        ((Length == 0) && (Type != ShellExpansionSplitOnNull))) {

        return TRUE;
    }

    Range = malloc(sizeof(SHELL_EXPANSION_RANGE));
    if (Range == NULL) {
        return FALSE;
    }

    Range->Type = Type;
    Range->Index = Index;
    Range->Length = Length;
    INSERT_BEFORE(&(Range->ListEntry), ListHead);
    return TRUE;
}

VOID
ShTrimVariableValue (
    PSTR *Value,
    PUINTN ValueSize,
    PSTR Pattern,
    UINTN PatternSize,
    BOOL Prefix,
    BOOL Longest
    )

/*++

Routine Description:

    This routine trims off the shortest or longest prefix or suffix pattern
    from the given value.

Arguments:

    Value - Supplies a pointer that on input contains the value to trim. On
        output, this pointer may be moved up to trim a prefix.

    ValueSize - Supplies a pointer that on input contains the size of the
        value string in bytes, including the null terminator. On output, this
        value may be decreased to represent the trim.

    Pattern - Supplies a pointer to the pattern to remove.

    PatternSize - Supplies the size of the pattern buffer in bytes including
        the null terminating character.

    Prefix - Supplies a boolean indicating whether to trim the prefix (TRUE) or
        suffix (FALSE).

    Longest - Supplies a boolean indicating whether to trim the longest
        matching term (TRUE) or the shortest matching term (FALSE).

Return Value:

    None.

--*/

{

    BOOL Match;
    UINTN Size;
    UINTN ValueIndex;

    if (*ValueSize <= 1) {
        return;
    }

    if (PatternSize == 0) {
        return;
    }

    //
    // Determine where to start matching for patterns.
    //

    if (Prefix != FALSE) {
        if (Longest != FALSE) {
            ValueIndex = 0;
            Size = *ValueSize;

        } else {
            ValueIndex = 0;
            Size = 1;
        }

    } else {
        if (Longest != FALSE) {
            ValueIndex = 0;
            Size = *ValueSize;

        } else {
            ValueIndex = *ValueSize - 1;
            Size = 1;
        }
    }

    //
    // Loop looking for a match.
    //

    while (TRUE) {

        //
        // If the given pattern matches, then return that trimmed value.
        //

        Match = SwDoesPatternMatch(*Value + ValueIndex,
                                   Size,
                                   Pattern,
                                   PatternSize);

        if (Match != FALSE) {
            if (Prefix != FALSE) {
                *Value += Size - 1;
                *ValueSize -= Size - 1;

            } else {
                *ValueSize = ValueIndex + 1;
            }

            return;
        }

        //
        // Move the size and/or index to get the next slightly less aggressive
        // combination.
        //

        if (Prefix != FALSE) {
            if (Longest != FALSE) {
                Size -= 1;
                if (Size == 0) {
                    break;
                }

            } else {
                if (Size == *ValueSize) {
                    break;
                }

                Size += 1;
            }

        } else {
            if (Longest != FALSE) {
                Size -= 1;
                ValueIndex += 1;
                if (ValueIndex == *ValueSize - 1) {
                    break;
                }

            } else {
                if (ValueIndex == 0) {
                    break;
                }

                ValueIndex -= 1;
                Size += 1;
            }
        }
    }

    return;
}

BOOL
ShEscapeSpecialCharacters (
    BOOL Quoted,
    PSTR *Value,
    PUINTN ValueSize
    )

/*++

Routine Description:

    This routine adds escape control characters in front of every control
    character, as well as any character that might be interpreted by the shell
    if not surrounded by double quotes.

Arguments:

    Quoted - Supplies a boolean indicating this region is inside double quotes.

    Value - Supplies a pointer that on input contains the value to trim. This
        value must be heap allocated. On output, this pointer may be
        reallocated.

    ValueSize - Supplies a pointer that on input contains the size of the
        value string in bytes, including the null terminator. On output, this
        value may be adjusted to accomodate the escape characters.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    CHAR Character;
    PSTR NeedsEscaping;
    PSTR NewString;
    UINTN NewStringSize;
    PSTR Source;
    UINTN SourceIndex;
    UINTN SourceSize;

    Source = *Value;
    SourceSize = *ValueSize;
    if (SourceSize == 0) {
        return TRUE;
    }

    SourceSize -= 1;
    NewString = NULL;
    NewStringSize = 0;
    for (SourceIndex = 0; SourceIndex < SourceSize; SourceIndex += 1) {
        Character = Source[SourceIndex];
        if (Quoted != FALSE) {
            if (Character == '\0') {
                NeedsEscaping = NULL;

            } else if (Character == '\\') {
                NeedsEscaping = (PVOID)-1;

            } else {
                NeedsEscaping = strchr(ShQuoteEscapeCharacters, Character);
            }

        //
        // In a non-quoted environment, only the control characters themselves
        // need escaping.
        //

        } else {
            NeedsEscaping = NULL;
            if ((Character == SHELL_CONTROL_QUOTE) ||
                (Character == SHELL_CONTROL_ESCAPE)) {

                NeedsEscaping = (PVOID)-1;
            }
        }

        //
        // If the new string hasn't been allocated and this character also
        // doesn't need quoting, just keep going.
        //

        if (NewStringSize == 0) {
            if (NeedsEscaping == NULL) {
                continue;
            }

            //
            // Oh, here's a character that needs escaping. Allocate the new
            // string and copy all the standard characters so far.
            //

            NewString = malloc((SourceSize * 2) + 1);
            if (NewString == NULL) {
                return FALSE;
            }

            memcpy(NewString, Source, SourceIndex);
            NewStringSize = SourceIndex;
        }

        if (NeedsEscaping != NULL) {
            NewString[NewStringSize] = SHELL_CONTROL_ESCAPE;
            NewStringSize += 1;
        }

        NewString[NewStringSize] = Character;
        NewStringSize += 1;
    }

    //
    // If there were never any fancy characters, then no memory was allocated,
    // and the original string can be returned.
    //

    if (NewStringSize == 0) {
        return TRUE;
    }

    NewString[NewStringSize] = '\0';
    NewStringSize += 1;

    //
    // Return the new string.
    //

    free(Source);
    *Value = NewString;
    *ValueSize = NewStringSize;
    return TRUE;
}

