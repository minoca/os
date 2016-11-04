/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    linein.c

Abstract:

    This module implements user input functionality.

Author:

    Evan Green 16-Mar-2013

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sh.h"
#include "../swlib.h"

#include <minoca/lib/termlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the string initially allocated for the line. This should
// be big enough that "most" commands succeed without expansion.
//

#define INITIAL_COMMAND_LENGTH 10

//
// Define the maximum number of bytes that could constitute one (control)
// character.
//

#define MAX_CHARACTER_LENGTH 10

//
// Define the number of lines page up and page down scroll by.
//

#define SCROLL_LINE_COUNT 10

//
// Define the Control+C character.
//

#define CONTROL_C_CHARACTER 3

//
// Define the default size of the command history.
//

#define DEFAULT_COMMAND_HISTORY_SIZE 50

#define INITIAL_STRING_ARRAY_SIZE 16
#define COMPLETION_COLUMN_PADDING 2
#define COMPLETION_DEFAULT_TERMINAL_WIDTH 80

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ShCompleteFilePath (
    PSHELL Shell,
    PSTR *Command,
    PULONG CommandLength,
    PULONG CommandSize,
    PULONG CompletionPosition,
    PULONG Position
    );

VOID
ShGetFileMatches (
    PSTR File,
    PSTR **Matches,
    PULONG MatchCount
    );

VOID
ShGetFileMatchesInDirectory (
    PSTR FileName,
    PSTR DirectoryName,
    PSTR **Matches,
    PULONG MatchCount
    );

BOOL
ShGetFileCompletionPortion (
    PSHELL Shell,
    PSTR Command,
    PULONG CompletionPosition,
    ULONG CommandLength,
    PULONG FileStartPosition,
    PSTR *FileString,
    PSTR *PreviousGuess
    );

PSTR
ShGetFileReplacementString (
    PSTR UserString,
    PSTR PreviousGuess,
    PSTR *Matches,
    ULONG MatchCount
    );

BOOL
ShQuoteString (
    PSTR String,
    PSTR *QuotedString
    );

VOID
ShAddCommandHistoryEntry (
    PSTR Command
    );

PSTR
ShGetCommandHistoryEntry (
    LONG Offset
    );

VOID
ShCleanLine (
    FILE *Output,
    ULONG Position,
    ULONG CommandLength
    );

VOID
ShPrintSpaces (
    FILE *Output,
    INT Count
    );

BOOL
ShAddStringToArray (
    PSTR **Array,
    PSTR Entry,
    PULONG ArraySize,
    PULONG ArrayCapacity
    );

BOOL
ShMergeStringArrays (
    PSTR **Array1,
    PULONG Array1Size,
    PULONG Array1Capacity,
    PSTR *Array2,
    ULONG Array2Size
    );

VOID
ShRemoveDuplicateFileMatches (
    PSTR *Array,
    PULONG ArraySize
    );

VOID
ShDestroyStringArray (
    PSTR *Array,
    ULONG ArraySize
    );

int
ShCompareStringArrayElements (
    const void *LeftElement,
    const void *RightElement
    );

//
// -------------------------------------------------------------------- Globals
//

CHAR ShBackspaceCharacter = 0x7F;
CHAR ShKillLineCharacter = 0x0B;

//
// Define the size of the command history.
//

LONG ShCommandHistorySize = DEFAULT_COMMAND_HISTORY_SIZE;
PSTR *ShCommandHistory;
LONG ShCommandHistoryIndex;

//
// Set this flag to enable printing of file completion suggestions in color.
//

BOOL ShColorFileSuggestions = TRUE;

//
// Set this flag to "guess" when there are multiple file matches, and then
// cycle through the guesses.
//

BOOL ShGuessFileMatch = TRUE;

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShReadLine (
    PSHELL Shell,
    PSTR *ReturnedCommand,
    PULONG ReturnedCommandLength
    )

/*++

Routine Description:

    This routine reads a command in from the user.

Arguments:

    Shell - Supplies a pointer to the shell.

    ReturnedCommand - Supplies a pointer where a pointer to a null terminated
        string (allocated via malloc) containing the command will be returned.
        This may be set to NULL if the end of file indicator was returned from
        stdin.

    ReturnedCommandLength - Supplies a pointer where the size of the command
        being returned will be returned, including the null terminator, in
        bytes.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG BaseOffset;
    int Byte;
    ULONG ByteIndex;
    PSTR Command;
    ULONG CommandLength;
    ULONG CommandSize;
    ULONG CompletionPosition;
    PSTR HistoryEntry;
    LONG HistoryOffset;
    TERMINAL_KEY_DATA KeyData;
    PSTR NewBuffer;
    ULONG NewCommandSize;
    FILE *Output;
    TERMINAL_PARSE_RESULT ParseResult;
    ULONG Position;
    BOOL Result;

    CommandLength = 0;
    CompletionPosition = (ULONG)-1;
    HistoryEntry = NULL;
    HistoryOffset = 0;
    Output = Shell->NonStandardError;
    Result = TRUE;
    Position = 0;
    memset(&KeyData, 0, sizeof(TERMINAL_KEY_DATA));
    ShSetTerminalMode(Shell, TRUE);

    //
    // Allocate the initial command buffer.
    //

    CommandSize = INITIAL_COMMAND_LENGTH;
    Command = malloc(CommandSize);
    if (Command == NULL) {
        Result = FALSE;
        goto ReadLineEnd;
    }

    Command[0] = '\0';

    //
    // Loop reading in characters.
    //

    while (TRUE) {
        fflush(Output);
        Byte = SwReadInputCharacter();
        if (Byte == -1) {
            printf("sh: Input error: %s\n", strerror(errno));
            break;
        }

        //
        // A newline signals the end of the command.
        //

        if ((Byte == '\n') || (Byte == '\r')) {
            break;
        }

        //
        // A Control-C aborts the current command.
        //

        if (Byte == CONTROL_C_CHARACTER) {
            fputs("^C\n", Output);
            Result = FALSE;
            goto ReadLineEnd;
        }

        ParseResult = TermProcessInput(&KeyData, Byte);
        switch (ParseResult) {
        case TerminalParseResultNormalCharacter:

            //
            // Handle a backspace.
            //

            if (Byte == ShBackspaceCharacter) {
                Byte = -1;

                //
                // If there is already nothing, throw this out.
                //

                if (Position == 0) {
                    continue;
                }

                //
                // Adjust the buffer for the backspace. The standard memory
                // copy function cannot be used since the buffers are
                // overlapping and so copy order matters.
                //

                Position -= 1;
                CommandLength -= 1;
                for (ByteIndex = Position;
                     ByteIndex < CommandLength;
                     ByteIndex += 1) {

                    Command[ByteIndex] = Command[ByteIndex + 1];
                }

                //
                // Go back one and print the shifted command, plus a space to
                // cover up the old ending character.
                //

                assert(Command[CommandLength + 1] == '\0');

                Command[CommandLength] = ' ';
                SwMoveCursorRelative(Output, -1, NULL);
                fwrite(Command + Position,
                       CommandLength - Position + 1,
                       1,
                       Output);

                Command[CommandLength] = '\0';

                //
                // But oh dear, now the cursor is at the end of the line. Print
                // backspaces to get it back.
                //

                SwMoveCursorRelative(Output,
                                     -(CommandLength - Position + 1),
                                     NULL);

            //
            // Tabs do file completion.
            //

            } else if (Byte == '\t') {
                ShCompleteFilePath(Shell,
                                   &Command,
                                   &CommandLength,
                                   &CommandSize,
                                   &CompletionPosition,
                                   &Position);

                Byte = -1;

                //
                // In the case where the returned completion position is not the
                // same as the current position, continue directly to avoid
                // updating it, so that pressing tab again guesses from the
                // previous completion position.
                //

                if (CompletionPosition != (ULONG)-1) {
                    continue;
                }

            //
            // Handle the kill line character.
            //

            } else if (Byte == ShKillLineCharacter) {
                ShCleanLine(Output, Position, CommandLength);
                Position = 0;
                CommandLength = 0;
                Command[0] = '\0';
                Byte = -1;

            } else if (iscntrl(Byte)) {
                Byte = -1;
            }

            break;

        case TerminalParseResultPartialCommand:
            Byte = -1;
            break;

        case TerminalParseResultCompleteCommand:
            Byte = -1;

            //
            // Handle command history entries.
            //

            switch (KeyData.Key) {
            case TerminalKeyUp:
                HistoryEntry = ShGetCommandHistoryEntry(HistoryOffset + 1);
                if (HistoryEntry != NULL) {
                    HistoryOffset += 1;
                }

                break;

            case TerminalKeyDown:
                HistoryEntry = ShGetCommandHistoryEntry(HistoryOffset - 1);
                if (HistoryEntry != NULL) {
                    HistoryOffset -= 1;
                }

                break;

            case TerminalKeyRight:
                if (Position < CommandLength) {
                    SwMoveCursorRelative(Output, 1, &(Command[Position]));
                    Position += 1;
                }

                break;

            case TerminalKeyLeft:
                if (Position != 0) {
                    Position -= 1;
                    SwMoveCursorRelative(Output, -1, NULL);
                }

                break;

            case TerminalKeyHome:
                SwMoveCursorRelative(Output, -Position, NULL);
                Position = 0;
                break;

            case TerminalKeyEnd:
                SwMoveCursorRelative(Output,
                                     CommandLength - Position,
                                     &(Command[Position]));

                Position = CommandLength;
                break;

            case TerminalKeyDelete:
                if ((CommandLength == 0) || (Position == CommandLength)) {
                    break;
                }

                CommandLength -= 1;
                for (ByteIndex = Position;
                     ByteIndex < CommandLength;
                     ByteIndex += 1) {

                    Command[ByteIndex] = Command[ByteIndex + 1];
                }

                //
                // Go back one and print the shifted command, plus a space
                // to cover up the old ending character.
                //

                assert(Command[CommandLength + 1] == '\0');

                Command[CommandLength] = ' ';
                fwrite(Command + Position,
                       CommandLength - Position + 1,
                       1,
                       Output);

                Command[CommandLength] = '\0';

                //
                // But oh dear, now the cursor is at the end of the line.
                // Print backspaces to get it back.
                //

                SwMoveCursorRelative(Output,
                                     -(CommandLength - Position + 1),
                                     NULL);

                break;

            case TerminalKeyPageUp:
                SwScrollTerminal(-SCROLL_LINE_COUNT);
                break;

            case TerminalKeyPageDown:
                SwScrollTerminal(SCROLL_LINE_COUNT);
                break;

            default:
                break;
            }

            break;

        default:

            assert(FALSE);

            break;
        }

        CompletionPosition = (ULONG)-1;

        //
        // If a history entry was found, then kill the current line and use it.
        //

        if (HistoryEntry != NULL) {
            Byte = -1;
            ShCleanLine(Output, Position, CommandLength);
            if (Command != NULL) {
                free(Command);
            }

            Command = HistoryEntry;
            CommandLength = strlen(Command);
            CommandSize = CommandLength + 1;
            Position = CommandLength;
            fwrite(Command, 1, CommandLength, Output);
            HistoryEntry = NULL;
        }

        //
        // If after all the processing there are no new characters, loop on to
        // get more.
        //

        if (Byte == -1) {
            continue;
        }

        //
        // There are characters in the character buffer, move them to the
        // command. Start by seeing if adding these characters would overrun
        // the buffer, and expand if so.
        //

        if (CommandLength + 2 > CommandSize) {
            NewCommandSize = CommandSize * 2;
            if (NewCommandSize < INITIAL_COMMAND_LENGTH) {
                NewCommandSize = INITIAL_COMMAND_LENGTH;
            }

            assert(CommandLength + 2 < NewCommandSize);

            NewBuffer = realloc(Command, NewCommandSize);
            if (NewBuffer == NULL) {
                Result = FALSE;
                goto ReadLineEnd;
            }

            CommandSize = NewCommandSize;
            Command = NewBuffer;
        }

        //
        // If the current position is not at the end of the string, then
        // room will need to be made. The standard memory copy routines cannot
        // be used since the copy buffers are overlapping, and therefore copy
        // order matters.
        //

        if (Position != CommandLength) {
            BaseOffset = CommandLength - 1;
            for (ByteIndex = 0;
                 ByteIndex < CommandLength - Position;
                 ByteIndex += 1) {

                Command[BaseOffset + 1 - ByteIndex] =
                                               Command[BaseOffset - ByteIndex];
            }
        }

        Command[Position] = Byte;

        //
        // Write out the remainder of the line.
        //

        CommandLength += 1;
        fwrite(Command + Position, CommandLength - Position, 1, Output);
        Position += 1;
        if (Position != CommandLength) {
            SwMoveCursorRelative(Output, -(CommandLength - Position), 0);
        }

        Byte = -1;
        Command[CommandLength] = '\0';
    }

    assert(Command[CommandLength] == '\0');

    //
    // If the current position is not the end, move to the end.
    //

    if (Position != CommandLength) {
        SwMoveCursorRelative(Output,
                             CommandLength - Position,
                             &(Command[Position]));
    }

    fputc('\n', Output);
    CommandLength += 1;
    ShAddCommandHistoryEntry(Command);

ReadLineEnd:
    fflush(Output);
    ShSetTerminalMode(Shell, FALSE);
    if (Result == FALSE) {
        CommandLength = 0;
        if (Command != NULL) {
            free(Command);
            Command = NULL;
        }
    }

    *ReturnedCommand = Command;
    *ReturnedCommandLength = CommandLength;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ShCompleteFilePath (
    PSHELL Shell,
    PSTR *Command,
    PULONG CommandLength,
    PULONG CommandSize,
    PULONG CompletionPosition,
    PULONG Position
    )

/*++

Routine Description:

    This routine performs file path completion in response to a tab character.

Arguments:

    Shell - Supplies a pointer to the shell context.

    Command - Supplies a pointer that on input contains a pointer to the
        command so far. On output this will return the potentially reallocated
        completed command.

    CommandLength - Supplies a pointer that on input contains the command
        length. This will be updated to reflect any changes.

    CommandSize - Supplies a pointer that on input contains the size of the
        command buffer's allocation. On output this will contain the updated
        value if the command was reallocated.

    CompletionPosition - Supplies a pointer that on input contains the index to
        stem completions off of. This may be different from the current
        position if cycling through guesses. This may be updated if a
        definitive selection is chosen.

    Position - Supplies a pointer that on input contains the position within
        the command. On output this may be updated.

Return Value:

    None.

--*/

{

    UINTN BigCommandCapacity;
    UINTN BigCommandSize;
    ULONG ColumnCount;
    ULONG ColumnIndex;
    size_t ColumnSize;
    int ConsoleWidth;
    ULONG FileStartIndex;
    PSTR Match;
    ULONG MatchCount;
    PSTR *Matches;
    ULONG MatchIndex;
    size_t MatchLength;
    LONG Offset;
    FILE *Output;
    PSTR PreviousGuess;
    ULONG ReplacedSize;
    PSTR Replacement;
    size_t ReplacementSize;
    BOOL Result;
    ULONG RoundedCount;
    PSTR *RoundedMatches;
    ULONG RowCount;
    PSTR UserString;

    Matches = NULL;
    MatchCount = 0;
    Output = Shell->NonStandardError;
    PreviousGuess = NULL;
    Replacement = NULL;
    RoundedMatches = NULL;
    UserString = NULL;

    //
    // Restore the terminal mode since the shell may execute expansions.
    //

    ShSetTerminalMode(Shell, FALSE);
    Result = ShGetFileCompletionPortion(Shell,
                                        *Command,
                                        CompletionPosition,
                                        *Position,
                                        &FileStartIndex,
                                        &UserString,
                                        &PreviousGuess);

    if (Result == FALSE) {
        goto CompleteFilePathEnd;
    }

    ShGetFileMatches(UserString, &Matches, &MatchCount);
    if ((Matches == NULL) || (MatchCount == 0)) {
        goto CompleteFilePathEnd;
    }

    //
    // Sort the match array alphabetically.
    //

    if (MatchCount > 1) {
        qsort(Matches, MatchCount, sizeof(PSTR), ShCompareStringArrayElements);
        ShRemoveDuplicateFileMatches(Matches, &MatchCount);
    }

    //
    // Using the matches, attempt to come up with a replacement string.
    //

    Replacement = ShGetFileReplacementString(UserString,
                                             PreviousGuess,
                                             Matches,
                                             MatchCount);

    //
    // Perform the replacement if there was one.
    //

    if (Replacement != NULL) {
        BigCommandSize = *CommandLength;
        BigCommandCapacity = *CommandSize;
        if (BigCommandSize < BigCommandCapacity) {
            BigCommandSize += 1;
        }

        ReplacedSize = *Position - FileStartIndex;
        ReplacementSize = strlen(Replacement);

        assert((*Command)[*CommandLength] == '\0');

        Result = SwStringReplaceRegion(Command,
                                       &BigCommandSize,
                                       &BigCommandCapacity,
                                       FileStartIndex,
                                       FileStartIndex + ReplacedSize,
                                       Replacement,
                                       ReplacementSize + 1);

        if (Result == FALSE) {
            goto CompleteFilePathEnd;
        }

        *CommandLength = BigCommandSize - 1;
        *CommandSize = BigCommandCapacity;

        assert((*Command)[*CommandLength] == '\0');

        //
        // Redraw the command. Go back to the beginning of the replacement
        // region, print the command, print spaces for any leftover parts, then
        // back up over the spaces.
        //

        SwMoveCursorRelative(Output, -(*Position - FileStartIndex), NULL);
        fprintf(Output, "%s", *Command + FileStartIndex);
        Offset = 0;
        if (ReplacedSize > ReplacementSize) {
            Offset = ReplacedSize - ReplacementSize;
            ShPrintSpaces(Output, Offset);
        }

        //
        // The negative of the offset gets back to the end of the new command.
        // Back up to the beginning, then forward to the end of the replacement.
        //

        Offset += *CommandLength - (FileStartIndex + ReplacementSize);
        if (Offset != 0) {
            SwMoveCursorRelative(Output, -Offset, NULL);
        }

        *Position += ReplacementSize - ReplacedSize;
    }

    //
    // If the match was ambiguous and this is the first time taking a stab at
    // it, print the options.
    //

    if ((MatchCount > 1) && (PreviousGuess == NULL)) {

        //
        // Figure out the column size to display these matches.
        //

        ColumnSize = COMPLETION_COLUMN_PADDING;
        for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex += 1) {
            MatchLength = strlen(Matches[MatchIndex]);
            if (ColumnSize < MatchLength + COMPLETION_COLUMN_PADDING) {
                ColumnSize = MatchLength + COMPLETION_COLUMN_PADDING;
            }
        }

        Result = SwGetTerminalDimensions(&ConsoleWidth, NULL);
        if (Result != 0) {
            ConsoleWidth = COMPLETION_DEFAULT_TERMINAL_WIDTH;
        }

        ColumnCount = (ConsoleWidth - 1) / ColumnSize;
        if (ColumnCount == 0) {
            ColumnCount = 1;
        }

        RowCount = MatchCount / ColumnCount;
        if ((MatchCount % ColumnCount) != 0) {
            RowCount += 1;
        }

        //
        // Potentially reallocate a rounded up rectangular array.
        //

        RoundedMatches = Matches;
        RoundedCount = MatchCount;
        if (MatchCount > ColumnCount) {
            RoundedCount = RowCount * ColumnCount;
            if (RoundedCount > MatchCount) {
                RoundedMatches = malloc(RoundedCount * sizeof(PSTR));
                if (RoundedMatches == NULL) {
                    goto CompleteFilePathEnd;
                }

                memcpy(RoundedMatches, Matches, MatchCount * sizeof(PSTR));
                memset(RoundedMatches + MatchCount,
                       0,
                       (RoundedCount - MatchCount) * sizeof(PSTR));
            }

            Result = SwRotatePointerArray((PVOID *)RoundedMatches,
                                          ColumnCount,
                                          RowCount);

            if (Result == FALSE) {
                goto CompleteFilePathEnd;
            }
        }

        if (*Position != *CommandLength) {
            SwMoveCursorRelative(Output,
                                 *CommandLength - *Position,
                                 *Command + *Position);
        }

        fputc('\n', Output);
        ColumnIndex = 0;
        for (MatchIndex = 0; MatchIndex < RoundedCount; MatchIndex += 1) {
            Match = RoundedMatches[MatchIndex];
            if (Match == NULL) {
                if ((MatchIndex == 0) ||
                    (RoundedMatches[MatchIndex - 1] != NULL)) {

                    fputc('\n', Output);
                }

                ColumnIndex = 0;
                continue;
            }

            MatchLength = strlen(Match);
            if ((MatchLength != 0) && (Match[MatchLength - 1] == '/') &&
                (ShColorFileSuggestions != FALSE)) {

                SwPrintInColor(ConsoleColorDefault,
                               ConsoleColorBlue,
                               "%-*s",
                               ColumnSize,
                               Match);

            } else {
                fprintf(Output, "%-*s", (int)ColumnSize, Match);
            }

            ColumnIndex += 1;
            if (ColumnIndex == ColumnCount) {
                fputc('\n', Output);
                ColumnIndex = 0;
            }
        }

        if (ColumnIndex != 0) {
            fputc('\n', Output);
        }

        //
        // Reprint the command.
        //

        fprintf(Output, "%s", Shell->Prompt);
        fprintf(Output, "%s", *Command);
        if (*Position != *CommandLength) {
            SwMoveCursorRelative(Output, -(*CommandLength - *Position), NULL);
        }
    }

CompleteFilePathEnd:

    assert((*Command)[*CommandLength] == '\0');

    ShSetTerminalMode(Shell, TRUE);
    if (UserString != NULL) {
        free(UserString);
    }

    if (PreviousGuess != NULL) {
        free(PreviousGuess);
    }

    if ((RoundedMatches != NULL) && (RoundedMatches != Matches)) {
        free(RoundedMatches);
    }

    if (Matches != NULL) {
        ShDestroyStringArray(Matches, MatchCount);
    }

    if (Replacement != NULL) {
        free(Replacement);
    }

    return;
}

VOID
ShGetFileMatches (
    PSTR File,
    PSTR **Matches,
    PULONG MatchCount
    )

/*++

Routine Description:

    This routine returns any files that match the given file name.

Arguments:

    File - Supplies a pointer to the string containing the file prefix.

    Matches - Supplies a pointer where an array of matches will be returned on
        success, or NULL if nothing matches.

    MatchCount - Supplies a pointer where the number of elements in the
        match array will be returned.

Return Value:

    None.

--*/

{

    PSTR BaseName;
    PSTR CurrentPath;
    PSTR Directory;
    PSTR *DirectoryMatchArray;
    ULONG DirectoryMatchArraySize;
    PSTR FileCopy;
    size_t FileLength;
    PSTR *MatchArray;
    ULONG MatchArrayCapacity;
    ULONG MatchArraySize;
    PSTR NextSeparator;
    PSTR Path;
    PSTR PathCopy;
    CHAR Separator;

    BaseName = NULL;
    Directory = NULL;
    MatchArray = NULL;
    MatchArrayCapacity = 0;
    MatchArraySize = 0;
    PathCopy = NULL;
    FileCopy = strdup(File);
    if (FileCopy == NULL) {
        goto GetFileMatchesEnd;
    }

    FileLength = strlen(FileCopy);

    //
    // Get the directory portion and the file name portion of the string.
    // If the last character is a slash, then treat the whole thing like the
    // directory name.
    //

    if ((FileLength != 0) && (FileCopy[FileLength - 1] == '/')) {
        BaseName = strdup("");
        Directory = FileCopy;
        FileCopy = NULL;

    } else {
        BaseName = basename(FileCopy);
        if (BaseName == NULL) {
            goto GetFileMatchesEnd;
        }

        BaseName = strdup(BaseName);
        Directory = dirname(FileCopy);
        if (Directory == NULL) {
            goto GetFileMatchesEnd;
        }

        Directory = strdup(Directory);
    }

    if ((BaseName == NULL) || (Directory == NULL)) {
        goto GetFileMatchesEnd;
    }

    //
    // Search the directory as specified directly.
    //

    ShGetFileMatchesInDirectory(BaseName,
                                Directory,
                                &MatchArray,
                                &MatchArraySize);

    MatchArrayCapacity = MatchArraySize;

    //
    // If the path has a slash in it, then it's fully specified, so this set is
    // it.
    //

    if (strchr(File, '/') != NULL) {
        goto GetFileMatchesEnd;
    }

    //
    // Get the path and create a copy to tokenize over.
    //

    Path = getenv("PATH");
    if (Path == NULL) {
        goto GetFileMatchesEnd;
    }

    PathCopy = strdup(Path);
    if (PathCopy == NULL) {
        goto GetFileMatchesEnd;
    }

    CurrentPath = PathCopy;
    Separator = PATH_LIST_SEPARATOR;
    while (TRUE) {
        NextSeparator = strchr(CurrentPath, Separator);
        if (NextSeparator != NULL) {
            *NextSeparator = '\0';
        }

        DirectoryMatchArray = NULL;
        DirectoryMatchArraySize = 0;
        ShGetFileMatchesInDirectory(BaseName,
                                    CurrentPath,
                                    &DirectoryMatchArray,
                                    &DirectoryMatchArraySize);

        ShMergeStringArrays(&MatchArray,
                            &MatchArraySize,
                            &MatchArrayCapacity,
                            DirectoryMatchArray,
                            DirectoryMatchArraySize);

        if (NextSeparator == NULL) {
            break;
        }

        CurrentPath = NextSeparator + 1;
    }

GetFileMatchesEnd:
    if (FileCopy != NULL) {
        free(FileCopy);
    }

    if (BaseName != NULL) {
        free(BaseName);
    }

    if (Directory != NULL) {
        free(Directory);
    }

    if (PathCopy != NULL) {
        free(PathCopy);
    }

    *Matches = MatchArray;
    *MatchCount = MatchArraySize;
    return;
}

VOID
ShGetFileMatchesInDirectory (
    PSTR FileName,
    PSTR DirectoryName,
    PSTR **Matches,
    PULONG MatchCount
    )

/*++

Routine Description:

    This routine returns any files that match the given file name in the given
    directory.

Arguments:

    FileName - Supplies a pointer to the string containing the file prefix.

    DirectoryName - Supplies a pointer to a string containing the directory to
        search in.

    Matches - Supplies a pointer where an array of matches will be returned on
        success, or NULL if nothing matches.

    MatchCount - Supplies a pointer where the number of elements in the
        match array will be returned.

Return Value:

    None.

--*/

{

    DIR *Directory;
    size_t DirectoryLength;
    struct dirent *Entry;
    size_t EntryLength;
    size_t FileNameLength;
    PSTR FullPath;
    PSTR *MatchArray;
    ULONG MatchArrayCapacity;
    ULONG MatchArraySize;
    int Result;
    int SlashSize;
    struct stat Stat;

    MatchArray = NULL;
    MatchArraySize = 0;
    MatchArrayCapacity = 0;
    Directory = opendir(DirectoryName);
    if (Directory == NULL) {
        goto GetFileMatchesInDirectoryEnd;
    }

    DirectoryLength = strlen(DirectoryName);
    FileNameLength = strlen(FileName);

    //
    // Loop reading directory entries.
    //

    while (TRUE) {
        errno = 0;
        Entry = readdir(Directory);
        if (Entry == NULL) {
            Result = errno;
            if (Result != 0) {
                goto GetFileMatchesInDirectoryEnd;
            }

            break;
        }

        if ((strcmp(Entry->d_name, ".") == 0) ||
            (strcmp(Entry->d_name, "..") == 0)) {

            continue;
        }

        if (strncasecmp(FileName, Entry->d_name, FileNameLength) == 0) {
            EntryLength = strlen(Entry->d_name);
            FullPath = malloc(DirectoryLength + EntryLength + 3);
            if (FullPath == NULL) {
                continue;
            }

            memcpy(FullPath, DirectoryName, DirectoryLength);
            SlashSize = 0;
            if ((DirectoryLength != 0) &&
                (DirectoryName[DirectoryLength - 1] != '/')) {

                FullPath[DirectoryLength] = '/';
                SlashSize = 1;
            }

            strcpy(FullPath + DirectoryLength + SlashSize, Entry->d_name);
            Result = SwStat(FullPath, TRUE, &Stat);
            if ((Result == 0) && (S_ISDIR(Stat.st_mode))) {
                strcpy(FullPath + DirectoryLength + SlashSize + EntryLength,
                       "/");
            }

            ShAddStringToArray(&MatchArray,
                               FullPath + DirectoryLength + SlashSize,
                               &MatchArraySize,
                               &MatchArrayCapacity);

            free(FullPath);
        }
    }

GetFileMatchesInDirectoryEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    *Matches = MatchArray;
    *MatchCount = MatchArraySize;
    return;
}

BOOL
ShGetFileCompletionPortion (
    PSHELL Shell,
    PSTR Command,
    PULONG CompletionPosition,
    ULONG CommandLength,
    PULONG FileStartPosition,
    PSTR *FileString,
    PSTR *PreviousGuess
    )

/*++

Routine Description:

    This routine gets the portion of the end of the command that is eligible
    for file path expansion.

Arguments:

    Shell - Supplies a pointer to the shell context.

    Command - Supplies a pointer to the command.

    CompletionPosition - Supplies a pointer to the position to base completions
        off of. This may be different than the current position
        (supplied command length) if cycling through guesses. If -1 is supplied,
        then the completion position will be filled in for the expanded current
        path.

    CommandLength - Supplies the length of the command, not including the null
        terminating byte. Usually the current position is passed in here.

    FileStartPosition - Supplies a pointer where the index into the command
        where the beginning of the file path replacement will be returned.

    FileString - Supplies a pointer where a newly allocated string
        will be returned containing the portion of the command that should be
        expanded.

    PreviousGuess - Supplies a pointer where the previous guess will be
        returned if the completion portion and command length are not the same.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR BaseName;
    size_t BaseNameLength;
    CHAR Character;
    ULONG CommandEnd;
    PSTR ExpandedField;
    UINTN ExpandedFieldSize;
    ULONG ExpandOptions;
    PSTR Guess;
    PSTR GuessBaseName;
    UINTN GuessSize;
    ULONG Index;
    PSTR LastField;
    ULONG LastFieldIndex;
    UINTN LastFieldLength;
    CHAR Quote;
    BOOL Result;
    BOOL TrailingSlash;
    BOOL WasBackslash;
    BOOL WasBlank;

    ExpandedField = NULL;
    Guess = NULL;
    GuessBaseName = NULL;
    LastField = NULL;
    Quote = 0;
    WasBackslash = FALSE;
    WasBlank = FALSE;
    LastFieldIndex = 0;
    Result = FALSE;
    if (*CompletionPosition == (ULONG)-1) {
        CommandEnd = CommandLength;

    } else {
        CommandEnd = *CompletionPosition;
    }

    //
    // Loop to find the start of the last field, honoring quotes.
    //

    Index = 0;
    while (Index < CommandEnd) {
        Character = Command[Index];
        if (Quote != 0) {
            if (Quote == '\'') {
                if (Character == '\'') {
                    Quote = 0;
                }

            } else if (Quote == '"') {
                if ((Character == '"') && (WasBackslash == FALSE)) {
                    Quote = 0;
                }
            }

        } else {
            if (WasBackslash == FALSE) {

                //
                // An unquoted unescaped blank starts a new field.
                //

                if (isblank(Character)) {
                    WasBlank = TRUE;

                //
                // This is not a blank. If the last character was, then this is
                // the start of the new field.
                //

                } else {
                    if (WasBlank != FALSE) {
                        LastFieldIndex = Index;
                    }

                    WasBlank = FALSE;

                    //
                    // Look to see if quotes are beginning.
                    //

                    if ((Character == '\'') || (Character == '"')) {
                        Quote = Character;
                    }
                }
            }
        }

        if (Character == '\\') {
            WasBackslash = !WasBackslash;

        } else {
            WasBackslash = FALSE;
        }

        Index += 1;
    }

    if (WasBlank != FALSE) {
        LastFieldIndex = CommandEnd;
    }

    //
    // Create a copy of the last field. If the last field is zero in size, use
    // the current directory.
    //

    if ((Command[LastFieldIndex] == '\0') ||
        (LastFieldIndex == CommandEnd)) {

        LastField = strdup("./");
        if (LastField == NULL) {
            goto GetFileCompletionPositionEnd;
        }

        LastFieldLength = 2;

    } else {
        LastField = strdup(Command + LastFieldIndex);
        if (LastField == NULL) {
            goto GetFileCompletionPositionEnd;
        }

        LastFieldLength = CommandEnd - LastFieldIndex;
        LastField[LastFieldLength] = '\0';
    }

    //
    // Escape and control-quote the string. Since it's only tab completion,
    // there's no need for this to be perfect.
    //

    Index = 0;
    WasBackslash = FALSE;
    while (LastField[Index] != '\0') {

        //
        // Handle a double quote region.
        //

        if (LastField[Index] == '\'') {
            LastField[Index] = SHELL_CONTROL_QUOTE;
            do {
                Index += 1;

            } while ((LastField[Index] != '\'') && (LastField[Index] != '\0'));

            if (LastField[Index] == '\'') {
                LastField[Index] = SHELL_CONTROL_QUOTE;
                Index += 1;
            }

        //
        // Handle a single quote region.
        //

        } else if (LastField[Index] == '"') {
            LastField[Index] = SHELL_CONTROL_QUOTE;
            while ((LastField[Index] != '\0') && (LastField[Index] != '"')) {
                if (LastField[Index] == '\\') {
                    Index += 1;
                    if ((LastField[Index] == '$') ||
                        (LastField[Index] == '`') ||
                        (LastField[Index] == '"') ||
                        (LastField[Index] == '\\') ||
                        (LastField[Index] == '\r') ||
                        (LastField[Index] == '\n')) {

                        LastField[Index - 1] = SHELL_CONTROL_ESCAPE;
                    }
                }

                Index += 1;
            }

            if (LastField[Index] == '"') {
                LastField[Index] = SHELL_CONTROL_QUOTE;
                Index += 1;
            }

        } else {
            if (LastField[Index] == '\\') {
                LastField[Index] = SHELL_CONTROL_ESCAPE;
                Index += 1;
            }

            Index += 1;
        }
    }

    //
    // Expand the string.
    //

    ExpandOptions = SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT |
                    SHELL_EXPANSION_OPTION_NO_PATH_EXPANSION;

    Result = ShPerformExpansions(Shell,
                                 LastField,
                                 LastFieldLength + 1,
                                 ExpandOptions,
                                 &ExpandedField,
                                 &ExpandedFieldSize,
                                 NULL,
                                 NULL);

    if (Result == FALSE) {
        goto GetFileCompletionPositionEnd;
    }

    //
    // If the completion position has not been set, this is the first time
    // trying.
    //

    if (*CompletionPosition == (ULONG)-1) {
        *CompletionPosition = LastFieldIndex + ExpandedFieldSize - 1;
        Result = TRUE;
        goto GetFileCompletionPositionEnd;
    }

    GuessSize = CommandLength - LastFieldIndex + 1;
    Guess = malloc(GuessSize);
    if (Guess == NULL) {
        goto GetFileCompletionPositionEnd;
    }

    memcpy(Guess, Command + LastFieldIndex, GuessSize - 1);
    Guess[GuessSize - 1] = '\0';

    //
    // Temporarily remove a trailing slash as it gets basename all flustered.
    //

    TrailingSlash = FALSE;
    if ((GuessSize > 1) && (Guess[GuessSize - 2] == '/')) {
        GuessSize -= 1;
        Guess[GuessSize - 1] = '\0';
        TrailingSlash = TRUE;
    }

    BaseName = basename(Guess);
    if (BaseName == NULL) {
        goto GetFileCompletionPositionEnd;
    }

    BaseNameLength = strlen(BaseName);
    GuessBaseName = malloc(BaseNameLength + 2);
    if (GuessBaseName == NULL) {
        goto GetFileCompletionPositionEnd;
    }

    strcpy(GuessBaseName, BaseName);
    if (TrailingSlash != FALSE) {
        GuessBaseName[BaseNameLength] = '/';
        GuessBaseName[BaseNameLength + 1] = '\0';
    }

    Result = TRUE;

GetFileCompletionPositionEnd:
    if (LastField != NULL) {
        free(LastField);
    }

    if (Guess != NULL) {
        free(Guess);
    }

    *FileStartPosition = LastFieldIndex;
    *FileString = ExpandedField;
    *PreviousGuess = GuessBaseName;
    return Result;
}

PSTR
ShGetFileReplacementString (
    PSTR UserString,
    PSTR PreviousGuess,
    PSTR *Matches,
    ULONG MatchCount
    )

/*++

Routine Description:

    This routine attempts to get the replacement string to the file path
    completion if there is one.

Arguments:

    UserString - Supplies the query string.

    PreviousGuess - Supplies a pointer to a string containing the previous
        guess for this query.

    Matches - Supplies the array of strings that match.

    MatchCount - Supplies the number of elements in the match array.

Return Value:

    Returns a pointer to a string representing the complete replacement, which
    will be the first match if there is only one, or the common prefix of all
    matches if there is one and it's longer than the input.

--*/

{

    PSTR BaseName;
    size_t BaseNameLength;
    ULONG CharacterIndex;
    ULONG CombinedLength;
    PSTR CombinedString;
    PSTR CurrentMatch;
    PSTR Directory;
    size_t DirectoryLength;
    ULONG LongestPrefix;
    PSTR Match;
    ULONG MatchIndex;
    PSTR QuotedString;
    PSTR UserCopy;
    size_t UserStringLength;

    BaseName = NULL;
    CombinedString = NULL;
    Directory = NULL;
    Match = NULL;
    UserCopy = NULL;
    QuotedString = NULL;
    if ((Matches == NULL) || (MatchCount == 0)) {
        goto GetFileReplacementStringEnd;
    }

    //
    // Get the directory portion and the file name portion of the string.
    //

    UserCopy = strdup(UserString);
    if (UserCopy == NULL) {
        goto GetFileReplacementStringEnd;
    }

    UserStringLength = strlen(UserCopy);
    if ((UserStringLength != 0) && (UserString[UserStringLength - 1] == '/')) {
        BaseName = strdup("");
        Directory = UserCopy;
        UserCopy = NULL;

    } else {
        BaseName = basename(UserCopy);
        if (BaseName == NULL) {
            goto GetFileReplacementStringEnd;
        }

        BaseName = strdup(BaseName);

        //
        // If the given path doesn't have a slash in it, then use an empty
        // directory name as the found results may come from the path.
        //

        if (strchr(UserString, '/') == NULL) {
            Directory = "";

        } else {
            Directory = dirname(UserCopy);
            if (Directory == NULL) {
                goto GetFileReplacementStringEnd;
            }
        }

        Directory = strdup(Directory);
    }

    if ((BaseName == NULL) || (Directory == NULL)) {
        goto GetFileReplacementStringEnd;
    }

    //
    // If there's only one match, use that.
    //

    if (MatchCount == 1) {
        Match = strdup(Matches[0]);

    //
    // If guessing, come up with a guess. If there is no previous guess, then
    // just pick the first one. Otherwise, find the previous guess in the list
    // and then guess the next thing.
    //

    } else if (ShGuessFileMatch != FALSE) {
        if (PreviousGuess == NULL) {
            Match = strdup(Matches[0]);

        } else {
            for (MatchIndex = 0; MatchIndex < MatchCount; MatchIndex += 1) {
                CurrentMatch = Matches[MatchIndex];
                if (strcmp(CurrentMatch, PreviousGuess) == 0) {
                    break;
                }
            }

            //
            // Advance to the next index. If it goes beyond the end (or was
            // already beyond the end because no match was found, start at the
            // beginning.
            //

            MatchIndex += 1;
            if (MatchIndex >= MatchCount) {
                MatchIndex = 0;
            }

            Match = strdup(Matches[MatchIndex]);
        }

    //
    // There are a bunch of matches. Find the longest common prefix of all
    // the matches.
    //

    } else {
        BaseNameLength = strlen(BaseName);
        LongestPrefix = strlen(Matches[0]);
        for (MatchIndex = 1; MatchIndex < MatchCount; MatchIndex += 1) {
            CurrentMatch = Matches[MatchIndex];
            CharacterIndex = 0;
            while ((CharacterIndex < LongestPrefix) &&
                   (CurrentMatch[CharacterIndex] ==
                    Matches[0][CharacterIndex])) {

                CharacterIndex += 1;
            }

            //
            // If this match can't keep up for as long, then the new common
            // prefix is his length.
            //

            if (CharacterIndex < LongestPrefix) {
                LongestPrefix = CharacterIndex;
            }

            //
            // Stop bothering to look if it's no longer than the basename.
            //

            if (LongestPrefix <= BaseNameLength) {
                break;
            }
        }

        //
        // If the longest prefix is no longer than the base name, don't return
        // anything.
        //

        if (LongestPrefix <= BaseNameLength) {
            goto GetFileReplacementStringEnd;
        }

        //
        // Create a copy of the longest prefix.
        //

        Match = strdup(Matches[0]);
        if (Match == NULL) {
            goto GetFileReplacementStringEnd;
        }

        Match[LongestPrefix] = '\0';
    }

    //
    // If there's no match, then just cut out now.
    //

    if (Match == NULL) {
        goto GetFileReplacementStringEnd;
    }

    //
    // Create the combined string, which is the user supplied directory name
    // and this match.
    //

    DirectoryLength = strlen(Directory);
    CombinedLength = DirectoryLength + 1 + strlen(Match) + 2;
    CombinedString = malloc(CombinedLength);
    if (CombinedString == NULL) {
        goto GetFileReplacementStringEnd;
    }

    memcpy(CombinedString, Directory, DirectoryLength);
    if ((DirectoryLength != 0) && (Directory[DirectoryLength - 1] != '/')) {
        CombinedString[DirectoryLength] = '/';
        DirectoryLength += 1;
    }

    strcpy(CombinedString + DirectoryLength, Match);

    //
    // Quote the string.
    //

    ShQuoteString(CombinedString, &QuotedString);

GetFileReplacementStringEnd:
    if (UserCopy != NULL) {
        free(UserCopy);
    }

    if (BaseName != NULL) {
        free(BaseName);
    }

    if (Directory != NULL) {
        free(Directory);
    }

    if (Match != NULL) {
        free(Match);
    }

    if (CombinedString != NULL) {
        free(CombinedString);
    }

    return QuotedString;
}

BOOL
ShQuoteString (
    PSTR String,
    PSTR *QuotedString
    )

/*++

Routine Description:

    This routine quotes a given string.

Arguments:

    String - Supplies a pointer to the string to quote.

    QuotedString - Supplies a pointer where an allocated string will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    CHAR Character;
    PSTR Final;
    ULONG FinalIndex;
    ULONG InputIndex;
    size_t InputLength;
    BOOL NeedsBackslash;

    *QuotedString = NULL;
    InputLength = strlen(String);

    //
    // Allocate a new buffer for the final string. In the worst case every
    // character needs a backslash.
    //

    Final = malloc(InputLength * 2);
    if (Final == NULL) {
        return FALSE;
    }

    FinalIndex = 0;
    InputIndex = 0;
    while (InputIndex < InputLength) {
        Character = String[InputIndex];
        InputIndex += 1;
        NeedsBackslash = TRUE;
        if ((isalnum(Character)) || (Character == '.') || (Character == '_') ||
            (Character == '/') || (Character == ',') || (Character == '+') ||
            (Character == '-')) {

            NeedsBackslash = FALSE;
        }

        if (NeedsBackslash != FALSE) {
            Final[FinalIndex] = '\\';
            FinalIndex += 1;
        }

        Final[FinalIndex] = Character;
        FinalIndex += 1;
    }

    Final[FinalIndex] = '\0';

    assert(InputIndex < InputLength * 2);

    *QuotedString = Final;
    return TRUE;
}

VOID
ShAddCommandHistoryEntry (
    PSTR Command
    )

/*++

Routine Description:

    This routine adds a command to the command history.

Arguments:

    Command - Supplies a pointer to the command string to add.

Return Value:

    None.

--*/

{

    PSTR Copy;
    INT PreviousIndex;
    INT Result;

    Copy = NULL;
    Result = ENOMEM;
    if (ShCommandHistory == NULL) {
        if (ShCommandHistorySize == 0) {
            Result = 0;
            goto ShAddCommandHistoryEntryEnd;
        }

        ShCommandHistory = malloc(ShCommandHistorySize * sizeof(PSTR));
        if (ShCommandHistory == NULL) {
            goto ShAddCommandHistoryEntryEnd;
        }

        memset(ShCommandHistory, 0, ShCommandHistorySize * sizeof(PSTR));
    }

    assert(ShCommandHistoryIndex < ShCommandHistorySize);

    //
    // Don't add the command if it's exactly the same as the last one.
    //

    PreviousIndex = ShCommandHistoryIndex - 1;
    if (PreviousIndex < 0) {
        PreviousIndex += ShCommandHistorySize;
    }

    if ((ShCommandHistory[PreviousIndex] != NULL) &&
        (strcmp(Command, ShCommandHistory[PreviousIndex]) == 0)) {

        Result = 0;
        goto ShAddCommandHistoryEntryEnd;
    }

    Copy = strdup(Command);
    if (Copy == NULL) {
        goto ShAddCommandHistoryEntryEnd;
    }

    if (ShCommandHistory[ShCommandHistoryIndex] != NULL) {
        free(ShCommandHistory[ShCommandHistoryIndex]);
    }

    ShCommandHistory[ShCommandHistoryIndex] = Copy;
    ShCommandHistoryIndex += 1;
    if (ShCommandHistoryIndex >= ShCommandHistorySize) {
        ShCommandHistoryIndex = 0;
    }

    Result = 0;

ShAddCommandHistoryEntryEnd:
    if (Result != 0) {
        if (Copy != NULL) {
            free(Copy);
        }
    }

    return;
}

PSTR
ShGetCommandHistoryEntry (
    LONG Offset
    )

/*++

Routine Description:

    This routine cleans the current line and resets the position to zero.

Arguments:

    Offset - Supplies the offset from the most recent command to reach for.

Return Value:

    Returns a pointer to a newly allocated string containing the historical
    command on success.

    NULL on failure or if there is no command that far back.

--*/

{

    PSTR Copy;
    LONG Index;

    if ((Offset < 0) || (Offset > ShCommandHistorySize)) {
        return NULL;
    }

    if (ShCommandHistory == NULL) {
        return NULL;
    }

    Index = ShCommandHistoryIndex - Offset;
    while (Index < 0) {
        Index += ShCommandHistorySize;
    }

    if (ShCommandHistory[Index] == NULL) {
        return NULL;
    }

    Copy = strdup(ShCommandHistory[Index]);
    return Copy;
}

VOID
ShCleanLine (
    FILE *Output,
    ULONG Position,
    ULONG CommandLength
    )

/*++

Routine Description:

    This routine cleans the current line and resets the position to zero.

Arguments:

    Output - Supplies a pointer to the output file stream.

    Position - Supplies the current position index.

    CommandLength - Supplies the length of the command.

Return Value:

    None.

--*/

{

    if (CommandLength != 0) {
        if (Position != 0) {
            SwMoveCursorRelative(Output, -Position, NULL);
            Position = 0;
        }

        ShPrintSpaces(Output, CommandLength);
        SwMoveCursorRelative(Output, -CommandLength, NULL);
        CommandLength = 0;
    }

    return;
}

VOID
ShPrintSpaces (
    FILE *Output,
    INT Count
    )

/*++

Routine Description:

    This routine simply prints spaces.

Arguments:

    Output - Supplies a pointer to the output stream to write to.

    Count - Supplies the number of spaces to print.

Return Value:

    None.

--*/

{

    INT FillCount;
    CHAR Spaces[50];

    FillCount = sizeof(Spaces);
    if (FillCount > Count) {
        FillCount = Count;
    }

    memset(Spaces, ' ', FillCount);
    while (Count > FillCount) {
        fwrite(Spaces, 1, FillCount, Output);
        Count -= FillCount;
    }

    fwrite(Spaces, 1, Count, Output);
    return;
}

BOOL
ShAddStringToArray (
    PSTR **Array,
    PSTR Entry,
    PULONG ArraySize,
    PULONG ArrayCapacity
    )

/*++

Routine Description:

    This routine adds a string to the given string array.

Arguments:

    Array - Supplies a pointer that on input contains a pointer to the string
        array. On output, this may be updated if the string array is
        reallocated.

    Entry - Supplies the string entry to add. A copy of this string will be
        made.

    ArraySize - Supplies a pointer that on input contains the size of the array,
        in elements. This value will be incremented on success.

    ArrayCapacity - Supplies a pointer that on input contains the array
        capacity. This value will be updated if the array is reallocated.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR EntryCopy;
    PSTR *NewBuffer;
    ULONG NewCapacity;

    if (*ArraySize >= *ArrayCapacity) {
        if (*ArrayCapacity == 0) {
            NewCapacity = INITIAL_STRING_ARRAY_SIZE;

        } else {
            NewCapacity = *ArrayCapacity * 2;
        }

        NewBuffer = realloc(*Array, NewCapacity * sizeof(PSTR));
        if (NewBuffer == NULL) {
            return FALSE;
        }

        *Array = NewBuffer;
        *ArrayCapacity = NewCapacity;
    }

    EntryCopy = strdup(Entry);
    if (EntryCopy == NULL) {
        return FALSE;
    }

    (*Array)[*ArraySize] = EntryCopy;
    *ArraySize += 1;
    return TRUE;
}

BOOL
ShMergeStringArrays (
    PSTR **Array1,
    PULONG Array1Size,
    PULONG Array1Capacity,
    PSTR *Array2,
    ULONG Array2Size
    )

/*++

Routine Description:

    This routine merges two string arrays.

Arguments:

    Array1 - Supplies a pointer that on input contains a pointer to the
        destination string array. On output, this may be updated if the string
        array is reallocated.

    Array1Size - Supplies a pointer that on input contains the size of the
        primary array, in elements. This value will be incremented on success.

    Array1Capacity - Supplies a pointer that on input contains the primary array
        capacity. This value will be updated if the array is reallocated.

    Array2 - Supplies the array of strings to add to the primary array. After
        this function this array will be destroyed.

    Array2Size - Supplies the number of elements in the second array.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG Array2Index;
    PSTR *NewBuffer;
    ULONG NewCapacity;

    if (Array2 == NULL) {
        return TRUE;
    }

    if (*Array1Size + Array2Size >= *Array1Capacity) {
        NewCapacity = INITIAL_STRING_ARRAY_SIZE;
        while (NewCapacity < *Array1Size + Array2Size) {
            NewCapacity *= 2;
        }

        NewBuffer = realloc(*Array1, NewCapacity * sizeof(PSTR));
        if (NewBuffer == NULL) {
            return FALSE;
        }

        *Array1 = NewBuffer;
        *Array1Capacity = NewCapacity;
    }

    for (Array2Index = 0; Array2Index < Array2Size; Array2Index += 1) {
        (*Array1)[*Array1Size + Array2Index] = Array2[Array2Index];
        Array2[Array2Index] = NULL;
    }

    free(Array2);
    *Array1Size += Array2Size;
    return TRUE;
}

VOID
ShRemoveDuplicateFileMatches (
    PSTR *Array,
    PULONG ArraySize
    )

/*++

Routine Description:

    This routine removes duplicates from the given array. This routine assumes
    the array has already been sorted.

Arguments:

    Array - Supplies the array of strings.

    ArraySize - Supplies a pointer that on input contains the number of
        elements in the string array. On output, returns the potentially
        reduced number after duplicates have been removed.

Return Value:

    None.

--*/

{

    ULONG Index;
    ULONG MoveIndex;

    for (Index = 1; Index < *ArraySize; Index += 1) {
        while ((Index < *ArraySize) &&
               (strcmp(Array[Index], Array[Index - 1]) == 0)) {

            free(Array[Index]);

            //
            // Move the other entries down.
            //

            for (MoveIndex = Index;
                 MoveIndex < *ArraySize - 1;
                 MoveIndex += 1) {

                Array[MoveIndex] = Array[MoveIndex + 1];
            }

            *ArraySize -= 1;
        }
    }

    return;
}

VOID
ShDestroyStringArray (
    PSTR *Array,
    ULONG ArraySize
    )

/*++

Routine Description:

    This routine destroys a string array. This routine will free both the
    array and each element.

Arguments:

    Array - Supplies a pointer to the array of strings.

    ArraySize - Supplies the number of elements in the string array.

Return Value:

    None.

--*/

{

    ULONG Index;

    if (Array == NULL) {
        return;
    }

    for (Index = 0; Index < ArraySize; Index += 1) {
        if (Array[Index] != NULL) {
            free(Array[Index]);
        }
    }

    free(Array);
    return;
}

int
ShCompareStringArrayElements (
    const void *LeftElement,
    const void *RightElement
    )

/*++

Routine Description:

    This routine compares two pointers to strings. This prototype is
    compatible with the qsort function compare routine.

Arguments:

    LeftElement - Supplies a pointer where a pointer to the left string resides.

    RightElement - Supplies a pointer where a pointer to the right string
        resides.

Return Value:

    <0 if the left is less than the right.

    0 if the left is equal to the right.

    >0 if th left is greater than the right.

--*/

{

    PSTR LeftString;
    int Result;
    PSTR RightString;

    LeftString = *((PSTR *)LeftElement);
    RightString = *((PSTR *)RightElement);
    Result = strcasecmp(LeftString, RightString);
    return Result;
}

