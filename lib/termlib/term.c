/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    term.c

Abstract:

    This module implements common terminal support. It understands roughly the
    VT220 terminal command set, with some xterm support in there too.

Author:

    Evan Green 24-Jul-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/rtl.h>
#include <minoca/lib/termlib.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the decoding information for a terminal escape
    sequence.

Members:

    PreParameterString - Stores an optional pointer to the sequence of
        characters occurring before the parameter.

    PostParameterString - Stores the sequence of strings occurring after the
        parameters.

    Command - Stores the resulting command.

--*/

typedef struct _TERMINAL_DECODE_ENTRY {
    PSTR PreParameterString;
    PSTR PostParameterString;
    TERMINAL_COMMAND Command;
} TERMINAL_DECODE_ENTRY, *PTERMINAL_DECODE_ENTRY;

/*++

Structure Description:

    This structure stores the decoding information for a terminal escape
    sequence.

Members:

    Sequence - Stores a pointer to a string containing the escape sequence
        (after the escape) corresponding to this key.

    Control - Stores the control bits for this entry. See TERMINAL_KEY_FLAG_*
        definitions.

    Key - Stores the corresponding key code for this sequence.

--*/

typedef struct _TERMINAL_KEY_ENTRY {
    PSTR Sequence;
    UCHAR Control;
    TERMINAL_KEY Key;
} TERMINAL_KEY_ENTRY, *PTERMINAL_KEY_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
TermpMatchCommand (
    PSTR PreString,
    UINTN PreStringSize,
    PSTR PostString,
    UINTN PostStringSize,
    PTERMINAL_DECODE_ENTRY DecodeEntry,
    PBOOL PartialMatch
    );

//
// -------------------------------------------------------------------- Globals
//

TERMINAL_DECODE_ENTRY TermCommandTable[] = {
    {"[", "A", TerminalCommandCursorUp},
    {"[", "B", TerminalCommandCursorDown},
    {"[", "C", TerminalCommandCursorRight},
    {"[", "D", TerminalCommandCursorLeft},
    {"[", "f", TerminalCommandCursorMove},
    {"[", "H", TerminalCommandCursorMove},
    {"[", "I", TerminalCommandCursorForwardTabStops},
    {"[", "d", TerminalCommandSetCursorRowAbsolute},
    {"[", "e", TerminalCommandCursorDown},
    {"[", "G", TerminalCommandSetCursorColumnAbsolute},
    {"[", "Z", TerminalCommandCursorBackwardTabStops},
    {"", "c", TerminalCommandReset},
    {"", "D", TerminalCommandCursorDown},
    {"", "E", TerminalCommandNextLine},
    {"", "M", TerminalCommandReverseLineFeed},
    {"", "7", TerminalCommandSaveCursorAndAttributes},
    {"", "8", TerminalCommandRestoreCursorAndAttributes},
    {"", "H", TerminalCommandSetHorizontalTab},
    {"[", "g", TerminalCommandClearHorizontalTab},
    {"[", "r", TerminalCommandSetTopAndBottomMargin},
    {"[", "J", TerminalCommandEraseInDisplay},
    {"[?", "J", TerminalCommandEraseInDisplaySelective},
    {"[", "K", TerminalCommandEraseInLine},
    {"[?", "K", TerminalCommandEraseInLineSelective},
    {"[", "L", TerminalCommandInsertLines},
    {"[", "M", TerminalCommandDeleteLines},
    {"[", "@", TerminalCommandInsertCharacters},
    {"[", "P", TerminalCommandDeleteCharacters},
    {"[", "X", TerminalCommandEraseCharacters},
    {"", ">", TerminalCommandKeypadNumeric},
    {"", "=", TerminalCommandKeypadApplication},
    {"[", "l", TerminalCommandClearMode},
    {"[", "h", TerminalCommandSetMode},
    {"[?", "l", TerminalCommandClearPrivateMode},
    {"[?", "h", TerminalCommandSetPrivateMode},
    {"(", "", TerminalCommandSelectG0CharacterSet},
    {")", "", TerminalCommandSelectG1CharacterSet},
    {"*", "", TerminalCommandSelectG2CharacterSet},
    {"+", "", TerminalCommandSelectG3CharacterSet},
    {"[", "m", TerminalCommandSelectGraphicRendition},
    {"", "c", TerminalCommandReset},
    {"[", "!p", TerminalCommandSoftReset},
    {"[", "c", TerminalCommandDeviceAttributesPrimary},
    {"[", ">c", TerminalCommandDeviceAttributesSecondary},
    {"[", "S", TerminalCommandScrollUp},
    {"[", "T", TerminalCommandScrollDown},
    {"#", "3", TerminalCommandDoubleLineHeightTopHalf},
    {"#", "4", TerminalCommandDoubleLineHeightBottomHalf},
    {"#", "5", TerminalCommandSingleWidthLine},
    {"#", "6", TerminalCommandDoubleWidthLine},
};

TERMINAL_KEY_ENTRY TermKeyTable[] = {
    {"[A", 0, TerminalKeyUp},
    {"[B", 0, TerminalKeyDown},
    {"[C", 0, TerminalKeyRight},
    {"[D", 0, TerminalKeyLeft},
    {"[A", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyUp},
    {"[B", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyDown},
    {"[C", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyRight},
    {"[D", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyLeft},
    {"[2~", 0, TerminalKeyInsert},
    {"[3~", 0, TerminalKeyDelete},
    {"[1~", 0, TerminalKeyHome},
    {"[H", 0, TerminalKeyHome},
    {"[4~", 0, TerminalKeyEnd},
    {"[F", 0, TerminalKeyEnd},
    {"[5~", 0, TerminalKeyPageUp},
    {"[6~", 0, TerminalKeyPageDown},
    {"[11~", 0, TerminalKeyF1},
    {"[12~", 0, TerminalKeyF2},
    {"[13~", 0, TerminalKeyF3},
    {"[14~", 0, TerminalKeyF4},
    {"[15~", 0, TerminalKeyF5},
    {"[17~", 0, TerminalKeyF6},
    {"[18~", 0, TerminalKeyF7},
    {"[19~", 0, TerminalKeyF8},
    {"[20~", 0, TerminalKeyF9},
    {"[21~", 0, TerminalKeyF10},
    {"[23~", 0, TerminalKeyF11},
    {"[24~", 0, TerminalKeyF12},
    {"[23~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF1},
    {"[24~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF2},
    {"[25~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF3},
    {"[26~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF4},
    {"[28~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF5},
    {"[29~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF6},
    {"[31~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF7},
    {"[32~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF8},
    {"[33~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF9},
    {"[34~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF10},
    {"[11~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF11},
    {"[12~", TERMINAL_KEY_FLAG_SHIFT, TerminalKeyF12},
};

//
// ------------------------------------------------------------------ Functions
//

TERMINAL_PARSE_RESULT
TermProcessOutput (
    PTERMINAL_COMMAND_DATA Command,
    CHAR Character
    )

/*++

Routine Description:

    This routine processes a character destined for the terminal output.

Arguments:

    Command - Supplies a pointer to the current command state. If this is the
        first character ever, zero out the command before calling this function.

    Character - Supplies the input character.

Return Value:

    Returns a terminal output result code indicating if the character is just a
    normal display character, part of a command, or the last character in a
    complete command.

--*/

{

    UINTN CommandCount;
    UINTN CommandIndex;
    PTERMINAL_DECODE_ENTRY DecodeEntry;
    BOOL Match;
    UINTN ParameterIndex;
    BOOL PartialMatch;

    //
    // An escape character always starts a new command.
    //

    if (Character == TERMINAL_ESCAPE) {
        Command->Flags = TERMINAL_COMMAND_SEEN_ESCAPE;
        Command->CommandCharacterCount = 0;
        Command->ParameterCount = 0;
        Command->ParameterIndex = 0;
        Command->Parameter[0] = 0;
        Command->PreParameterSize = 0;
        Command->PostParameterSize = 0;
        Command->Command = TerminalCommandInvalid;
        return TerminalParseResultPartialCommand;
    }

    //
    // If an escape hasn't been seen then this is just an ordinary character.
    //

    if ((Command->Flags & TERMINAL_COMMAND_SEEN_ESCAPE) == 0) {
        return TerminalParseResultNormalCharacter;
    }

    //
    // If it's a control character, return it as normal.
    //

    if ((Character < ' ') || ((UCHAR)Character > 0x7F)) {
        return TerminalParseResultNormalCharacter;
    }

    //
    // If this is a digit, then it's either a parameter for a CSI (^[) sequence
    // or it's a command of its own (like ^7 or ^8). If a CSI has been seen,
    // treat it as a parameter, otherwise, treat it like a command character.
    //

    if ((Character >= '0') && (Character <= '9')) {
        if ((Command->PreParameterSize != 0) &&
            (Command->PreParameter[0] == TERMINAL_INTRODUCER)) {

            Command->Flags |= TERMINAL_COMMAND_SEEN_PARAMETER;
            ParameterIndex = Command->ParameterIndex;

            //
            // If this is the first time a digit for a parameter is specified,
            // then bump up the parameter count. Watch out for too many
            // parameters.
            //

            if (Command->ParameterCount < ParameterIndex + 1) {
                if (ParameterIndex >= TERMINAL_MAX_PARAMETERS) {
                    Command->Flags = 0;
                    return TerminalParseResultNormalCharacter;
                }

                Command->ParameterCount = ParameterIndex + 1;
                Command->Parameter[ParameterIndex] = 0;
            }

            Command->Parameter[ParameterIndex] *= 10;
            Command->Parameter[ParameterIndex] += Character - '0';
            return TerminalParseResultPartialCommand;
        }

    //
    // Move to the next parameter slot.
    //

    } else if (Character == TERMINAL_PARAMETER_SEPARATOR) {
        Command->ParameterIndex += 1;
        if (Command->ParameterIndex < TERMINAL_MAX_PARAMETERS) {
            Command->Parameter[Command->ParameterIndex] = 0;
        }

        return TerminalParseResultPartialCommand;
    }

    //
    // If the character was not a parameter, then add it to the command buffer.
    // Add it to the beginning or end depending on whether or not a parameter
    // was seen.
    //

    if ((Command->Flags & TERMINAL_COMMAND_SEEN_PARAMETER) != 0) {
        if (Command->PostParameterSize >= TERMINAL_MAX_COMMAND_CHARACTERS) {
            Command->Flags = 0;
            return TerminalParseResultNormalCharacter;
        }

        Command->PostParameter[Command->PostParameterSize] = Character;
        Command->PostParameterSize += 1;

    } else {
        if (Command->PreParameterSize >= TERMINAL_MAX_COMMAND_CHARACTERS) {
            Command->Flags = 0;
            return TerminalParseResultNormalCharacter;
        }

        Command->PreParameter[Command->PreParameterSize] = Character;
        Command->PreParameterSize += 1;
    }

    //
    // As a shortcut to prevent the following loop in common cases, skip the
    // test if this is the introducer.
    //

    if (Character == TERMINAL_INTRODUCER) {
        return TerminalParseResultPartialCommand;
    }

    //
    // Look to see if the command matches anything completely or partially.
    //

    PartialMatch = FALSE;
    CommandCount = sizeof(TermCommandTable) / sizeof(TermCommandTable[0]);
    for (CommandIndex = 0; CommandIndex < CommandCount; CommandIndex += 1) {
        DecodeEntry = &(TermCommandTable[CommandIndex]);
        Match = TermpMatchCommand(Command->PreParameter,
                                  Command->PreParameterSize,
                                  Command->PostParameter,
                                  Command->PostParameterSize,
                                  DecodeEntry,
                                  &PartialMatch);

        if (Match != FALSE) {
            break;
        }

        //
        // If there is no post parameter and the decode entry's pre-parameter
        // string is empty, try matching the pre-parameter string against the
        // post-parameter decode entry string.
        //

        if ((DecodeEntry->PreParameterString[0] == '\0') &&
            (Command->PostParameterSize == 0)) {

            Match = TermpMatchCommand(NULL,
                                      0,
                                      Command->PreParameter,
                                      Command->PreParameterSize,
                                      DecodeEntry,
                                      &PartialMatch);

            if (Match != FALSE) {
                break;
            }
        }
    }

    //
    // If the loop made it to the end, then no command matched exactly.
    //

    if (CommandIndex == CommandCount) {
        if (PartialMatch != FALSE) {
            return TerminalParseResultPartialCommand;
        }

        Command->Flags = 0;
        return TerminalParseResultNormalCharacter;
    }

    Command->Command = DecodeEntry->Command;
    Command->Flags = 0;
    return TerminalParseResultCompleteCommand;
}

VOID
TermNormalizeParameters (
    PTERMINAL_COMMAND_DATA Command
    )

/*++

Routine Description:

    This routine normalizes the command parameters to their expected defaults
    and allowed.

Arguments:

    Command - Supplies a pointer to the complete command.

Return Value:

    None.

--*/

{

    UINTN Index;

    switch (Command->Command) {
    case TerminalCommandCursorUp:
    case TerminalCommandCursorDown:
    case TerminalCommandCursorLeft:
    case TerminalCommandCursorRight:
    case TerminalCommandScrollUp:
    case TerminalCommandScrollDown:
    case TerminalCommandSetCursorRowAbsolute:
    case TerminalCommandSetCursorColumnAbsolute:
    case TerminalCommandCursorForwardTabStops:
    case TerminalCommandCursorBackwardTabStops:
        if (Command->ParameterCount == 0) {
            Command->Parameter[0] = 1;
        }

         Command->ParameterCount = 1;
        if (Command->Parameter[0] == 0) {
            Command->Parameter[0] = 1;
        }

        break;

    case TerminalCommandCursorMove:
        for (Index = 0; Index < 2; Index += 1) {
            if (Index >= Command->ParameterCount) {
                Command->Parameter[Index] = 1;

            } else if (Command->Parameter[Index] == 0) {
                Command->Parameter[Index] = 1;
            }
        }

        Command->ParameterCount = 2;
        break;

    case TerminalCommandNextLine:
    case TerminalCommandReverseLineFeed:
    case TerminalCommandSaveCursorAndAttributes:
    case TerminalCommandRestoreCursorAndAttributes:
    case TerminalCommandSetHorizontalTab:
    case TerminalCommandKeypadNumeric:
    case TerminalCommandKeypadApplication:
    case TerminalCommandReset:
    case TerminalCommandSoftReset:
    case TerminalCommandDeviceAttributesPrimary:
    case TerminalCommandDeviceAttributesSecondary:
    case TerminalCommandDoubleLineHeightTopHalf:
    case TerminalCommandDoubleLineHeightBottomHalf:
    case TerminalCommandSingleWidthLine:
    case TerminalCommandDoubleWidthLine:
        Command->ParameterCount = 0;
        break;

    case TerminalCommandClearHorizontalTab:
    case TerminalCommandEraseInDisplay:
    case TerminalCommandEraseInLine:
        if (Command->ParameterCount == 0) {
            Command->Parameter[0] = 0;
        }

        Command->ParameterCount = 1;
        break;

    case TerminalCommandInsertLines:
    case TerminalCommandDeleteLines:
    case TerminalCommandInsertCharacters:
    case TerminalCommandDeleteCharacters:
    case TerminalCommandEraseCharacters:
        if (Command->ParameterCount == 0) {
            Command->Parameter[0] = 1;
        }

        Command->ParameterCount = 1;
        break;

    case TerminalCommandSetTopAndBottomMargin:
    case TerminalCommandSetMode:
    case TerminalCommandClearMode:
    case TerminalCommandSelectG0CharacterSet:
    case TerminalCommandSelectG1CharacterSet:
    case TerminalCommandSelectG2CharacterSet:
    case TerminalCommandSelectG3CharacterSet:
    case TerminalCommandSelectGraphicRendition:
    default:
        break;
    }

    return;
}

BOOL
TermCreateOutputSequence (
    PTERMINAL_COMMAND_DATA Command,
    PSTR Buffer,
    UINTN BufferSize
    )

/*++

Routine Description:

    This routine creates a terminal command sequence for a given command.

Arguments:

    Command - Supplies a pointer to the complete command.

    Buffer - Supplies a pointer where the null-terminated command sequence will
        be returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PTERMINAL_DECODE_ENTRY DecodeEntry;
    UINTN EntryCount;
    UINTN EntryIndex;
    UINTN FinalLength;
    PSTR Format;
    UINTN ParameterIndex;

    DecodeEntry = NULL;
    EntryCount = sizeof(TermCommandTable) / sizeof(TermCommandTable[0]);
    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        DecodeEntry = &(TermCommandTable[EntryIndex]);
        if (DecodeEntry->Command == Command->Command) {
            break;
        }
    }

    if (EntryIndex == EntryCount) {
        return FALSE;
    }

    //
    // If the post parameter string is NULL, then the final sequence is a
    // single character.
    //

    if (DecodeEntry->PostParameterString[0] == '\0') {

        ASSERT(Command->PostParameterSize == 1);

        FinalLength = RtlPrintToString(Buffer,
                                       BufferSize,
                                       CharacterEncodingAscii,
                                       "%c%s%c",
                                       TERMINAL_ESCAPE,
                                       DecodeEntry->PreParameterString,
                                       Command->PostParameter[0]);

        if (FinalLength > BufferSize) {
            return FALSE;
        }

    //
    // Output the format ^<prestring><parameters><poststring>, where ^ is the
    // escape character (0x1B), and parameters are a sequence of
    // <number>;...;<number>.
    //

    } else {
        FinalLength = RtlPrintToString(Buffer,
                                       BufferSize,
                                       CharacterEncodingAscii,
                                       "%c%s",
                                       TERMINAL_ESCAPE,
                                       DecodeEntry->PreParameterString);

        if (FinalLength >= BufferSize) {
            return FALSE;
        }

        Buffer += FinalLength - 1;
        BufferSize -= FinalLength - 1;
        for (ParameterIndex = 0;
             ParameterIndex < Command->ParameterCount;
             ParameterIndex += 1) {

            if (ParameterIndex == Command->ParameterCount - 1) {
                Format = "%d";

            } else {
                Format = "%d;";
            }

            FinalLength = RtlPrintToString(Buffer,
                                           BufferSize,
                                           CharacterEncodingAscii,
                                           Format,
                                           Command->Parameter[ParameterIndex]);

            if (FinalLength >= BufferSize) {
                return FALSE;
            }

            Buffer += FinalLength - 1;
            BufferSize -= FinalLength - 1;
        }

        RtlPrintToString(Buffer,
                         BufferSize,
                         CharacterEncodingAscii,
                         "%s",
                         DecodeEntry->PostParameterString);
    }

    return TRUE;
}

TERMINAL_PARSE_RESULT
TermProcessInput (
    PTERMINAL_KEY_DATA KeyData,
    CHAR Character
    )

/*++

Routine Description:

    This routine processes a character destined for the terminal input.

Arguments:

    KeyData - Supplies a pointer to the key parsing state. If this is the first
        time calling this function, zero out this structure.

    Character - Supplies the input character.

Return Value:

    Returns a terminal parse result code indicating if the character is just a
    normal input character, part of a command, or the last character in a
    complete command.

--*/

{

    UINTN CharacterIndex;
    UINTN DecodeCount;
    PTERMINAL_KEY_ENTRY DecodeEntry;
    UINTN DecodeIndex;
    BOOL PartialMatch;

    //
    // An escape character always starts a new command.
    //

    if (Character == TERMINAL_ESCAPE) {

        //
        // Two escapes in a row means ALT was held down here.
        //

        if ((KeyData->Buffer[0] == TERMINAL_ESCAPE) &&
            (KeyData->BufferSize == 1)) {

            KeyData->Flags |= TERMINAL_KEY_FLAG_ALT;
            return TerminalParseResultPartialCommand;
        }

        KeyData->Buffer[0] = Character;
        KeyData->BufferSize = 1;
        KeyData->Flags = 0;
        return TerminalParseResultPartialCommand;
    }

    if (KeyData->BufferSize == 0) {
        return TerminalParseResultNormalCharacter;
    }

    if (KeyData->BufferSize == TERMINAL_MAX_KEY_CHARACTERS) {

        ASSERT(FALSE);

        KeyData->BufferSize = 0;
        return TerminalParseResultNormalCharacter;
    }

    KeyData->Buffer[KeyData->BufferSize] = Character;
    KeyData->BufferSize += 1;
    PartialMatch = FALSE;
    DecodeCount = sizeof(TermKeyTable) / sizeof(TermKeyTable[0]);
    for (DecodeIndex = 0; DecodeIndex < DecodeCount; DecodeIndex += 1) {
        DecodeEntry = &(TermKeyTable[DecodeIndex]);
        for (CharacterIndex = 0;
             CharacterIndex < KeyData->BufferSize - 1;
             CharacterIndex += 1) {

            if (DecodeEntry->Sequence[CharacterIndex] == '\0') {
                break;
            }

            if (DecodeEntry->Sequence[CharacterIndex] !=
                KeyData->Buffer[CharacterIndex + 1]) {

                break;
            }
        }

        //
        // If not all the input characters were processed, this doesn't match.
        //

        if (CharacterIndex != KeyData->BufferSize - 1) {
            continue;
        }

        //
        // If everything matched but the sequence isn't finished, this is a
        // partial match.
        //

        if (DecodeEntry->Sequence[CharacterIndex] != '\0') {
            PartialMatch = TRUE;
            continue;
        }

        //
        // Everything matches, this is the key.
        //

        break;
    }

    if (DecodeIndex == DecodeCount) {
        if (PartialMatch != FALSE) {
            return TerminalParseResultPartialCommand;
        }

        KeyData->BufferSize = 0;
        return TerminalParseResultNormalCharacter;
    }

    KeyData->Key = DecodeEntry->Key;
    KeyData->BufferSize = 0;
    return TerminalParseResultCompleteCommand;
}

BOOL
TermCreateInputSequence (
    PTERMINAL_KEY_DATA KeyData,
    PSTR Buffer,
    UINTN BufferSize
    )

/*++

Routine Description:

    This routine creates a terminal keyboard sequence for a given key.

Arguments:

    KeyData - Supplies the complete key data.

    Buffer - Supplies a pointer where the null-terminated control sequence will
        be returned.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    UCHAR Control;
    UINTN DecodeCount;
    PTERMINAL_KEY_ENTRY DecodeEntry;
    UINTN DecodeIndex;

    Control = KeyData->Flags & ~TERMINAL_KEY_FLAG_ALT;
    DecodeCount = sizeof(TermKeyTable) / sizeof(TermKeyTable[0]);
    for (DecodeIndex = 0; DecodeIndex < DecodeCount; DecodeIndex += 1) {
        DecodeEntry = &(TermKeyTable[DecodeIndex]);
        if ((DecodeEntry->Key == KeyData->Key) &&
            (DecodeEntry->Control == Control)) {

            break;
        }
    }

    if (DecodeIndex == DecodeCount) {
        return FALSE;
    }

    if (BufferSize == 0) {
        return FALSE;
    }

    //
    // Stick an extra escape on the front if the ALT flag is set.
    //

    if ((KeyData->Flags & TERMINAL_KEY_FLAG_ALT) != 0) {
        *Buffer = TERMINAL_ESCAPE;
        Buffer += 1;
        BufferSize -= 1;
    }

    if (BufferSize == 0) {
        return FALSE;
    }

    *Buffer = TERMINAL_ESCAPE;
    Buffer += 1;
    BufferSize -= 1;
    RtlStringCopy(Buffer, DecodeEntry->Sequence, BufferSize);
    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
TermpMatchCommand (
    PSTR PreString,
    UINTN PreStringSize,
    PSTR PostString,
    UINTN PostStringSize,
    PTERMINAL_DECODE_ENTRY DecodeEntry,
    PBOOL PartialMatch
    )

/*++

Routine Description:

    This routine attempts to match the current input characters with the given
    command.

Arguments:

    PreString - Supplies a pointer to the pre-parameter characters seen so far.

    PreStringSize - Supplies the size of the pre-parameter string in bytes.

    PostString - Supplies a pointer to the post-parameter characters seen so
        far.

    PostStringSize - Supplies the size of the post parameter string in bytes.

    DecodeEntry - Supplies a pointer to the decode entry to match against.

    PartialMatch - Supplies a pointer that is left alone if the entry matches
        or does not match, and is set to TRUE if the entry partially matches
        but needs more characters to fully match.

Return Value:

    TRUE if the input matches the decode entry fully.

    FALSE if the input does not match or only partially matches the decode
    entry.

--*/

{

    PSTR PreStringTail;
    UINTN PreStringTailSize;
    UINTN StringIndex;

    //
    // Match the pre-parameter string.
    //

    for (StringIndex = 0; StringIndex < PreStringSize; StringIndex += 1) {
        if ((DecodeEntry->PreParameterString[StringIndex] == '\0') ||
            (DecodeEntry->PreParameterString[StringIndex] !=
             PreString[StringIndex])) {

            break;
        }
    }

    if (StringIndex != PreStringSize) {

        //
        // In the case where there were no parameters, the final character
        // may have been glommed on to the pre-parameter string. Try to
        // match the rest of the string with the post parameter string.
        //

        if ((DecodeEntry->PreParameterString[StringIndex] == 0) &&
            (PostStringSize == 0)) {

            //
            // If the post parameter string is empty, then any character
            // matches. The "Select Character Set" commands have a form like
            // this: ^({final}, where {final} is the desired hard character set.
            //

            if (DecodeEntry->PostParameterString[0] == '\0') {
                return TRUE;
            }

            PreStringTail = PreString + StringIndex;
            PreStringTailSize = PreStringSize - StringIndex;
            for (StringIndex = 0;
                 StringIndex < PreStringTailSize;
                 StringIndex += 1) {

                if ((DecodeEntry->PostParameterString[StringIndex] ==
                     '\0') ||
                    (DecodeEntry->PostParameterString[StringIndex] !=
                     PreStringTail[StringIndex])) {

                    break;
                }
            }

            if (StringIndex == PreStringTailSize) {
                return TRUE;
            }
        }

        return FALSE;
    }

    if (DecodeEntry->PreParameterString[StringIndex] != '\0') {
        *PartialMatch = TRUE;
        return FALSE;
    }

    //
    // If the post-parameter string is empty, return a partial match. The next
    // character (which should get glommed on to the pre-parameter string)
    // will make it complete.
    //

    if (DecodeEntry->PostParameterString[0] == '\0') {
        *PartialMatch = TRUE;
        return FALSE;
    }

    //
    // Match the post-parameter string.
    //

    for (StringIndex = 0; StringIndex < PostStringSize; StringIndex += 1) {
        if ((DecodeEntry->PostParameterString[StringIndex] == '\0') ||
            (DecodeEntry->PostParameterString[StringIndex] !=
             PostString[StringIndex])) {

            break;
        }
    }

    if (StringIndex != PostStringSize) {
        return FALSE;
    }

    if (DecodeEntry->PostParameterString[StringIndex] != '\0') {
        *PartialMatch = TRUE;
        return FALSE;
    }

    return TRUE;
}

