/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    termlib.h

Abstract:

    This header contains definitions for the terminal support library.

Author:

    Evan Green 24-Jul-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the maximum number of ANSI parameters.
//

#define TERMINAL_MAX_PARAMETERS 8
#define TERMINAL_MAX_COMMAND_CHARACTERS 4
#define TERMINAL_MAX_CONTROL_SEQUENCE 32
#define TERMINAL_MAX_KEY_CHARACTERS 5

//
// Define control characters.
//

#define TERMINAL_ESCAPE 0x1B
#define TERMINAL_INTRODUCER '['
#define TERMINAL_PARAMETER_SEPARATOR ';'
#define TERMINAL_RUBOUT 0x7F

//
// Define terminal command flags.
//

#define TERMINAL_COMMAND_SEEN_ESCAPE 0x00000001
#define TERMINAL_COMMAND_SEEN_PARAMETER 0x00000002

//
// Define terminal key data flags.
//

#define TERMINAL_KEY_FLAG_ALT 0x00000001
#define TERMINAL_KEY_FLAG_SHIFT 0x00000002

//
// Define known terminal mode values.
//

#define TERMINAL_MODE_KEYBOARD_LOCKED 2
#define TERMINAL_MODE_INSERT 4
#define TERMINAL_MODE_DISABLE_LOCAL_ECHO 12
#define TERMINAL_MODE_NEW_LINE 20

#define TERMINAL_PRIVATE_MODE_APPLICATION_CURSOR_KEYS 1
#define TERMINAL_PRIVATE_MODE_VT52 2
#define TERMINAL_PRIVATE_MODE_132_COLUMNS 3
#define TERMINAL_PRIVATE_MODE_SMOOTH_SCROLLING 4
#define TERMINAL_PRIVATE_MODE_REVERSE_VIDEO 5
#define TERMINAL_PRIVATE_MODE_ORIGIN 6
#define TERMINAL_PRIVATE_MODE_AUTO_WRAP 7
#define TERMINAL_PRIVATE_MODE_AUTO_REPEAT 8
#define TERMINAL_PRIVATE_MODE_BLINKING_CURSOR 12
#define TERMINAL_PRIVATE_MODE_FORM_FEED 18
#define TERMINAL_PRIVATE_MODE_PRINT_FULL_SCREEN 19
#define TERMINAL_PRIVATE_MODE_CURSOR 25
#define TERMINAL_PRIVATE_MODE_NATIONAL 42

#define TERMINAL_PRIVATE_MODE_ALTERNATE_SCREEN 1047
#define TERMINAL_PRIVATE_MODE_SAVE_CURSOR 1048
#define TERMINAL_PRIVATE_MODE_ALTERNATE_SCREEN_SAVE_CURSOR 1049

//
// Define terminal graphics rendition values.
//

#define TERMINAL_GRAPHICS_BOLD 1
#define TERMINAL_GRAPHICS_NEGATIVE 7
#define TERMINAL_GRAPHICS_FOREGROUND 30
#define TERMINAL_GRAPHICS_BACKGROUND 40

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TERMINAL_PARSE_RESULT {
    TerminalParseResultInvalid,
    TerminalParseResultNormalCharacter,
    TerminalParseResultPartialCommand,
    TerminalParseResultCompleteCommand,
} TERMINAL_PARSE_RESULT, *PTERMINAL_PARSE_RESULT;

typedef enum _TERMINAL_COMMAND {
    TerminalCommandInvalid,
    TerminalCommandCursorUp,
    TerminalCommandCursorDown,
    TerminalCommandCursorLeft,
    TerminalCommandCursorRight,
    TerminalCommandCursorMove,
    TerminalCommandSetCursorRowAbsolute,
    TerminalCommandSetCursorColumnAbsolute,
    TerminalCommandNextLine,
    TerminalCommandReverseLineFeed,
    TerminalCommandSaveCursorAndAttributes,
    TerminalCommandRestoreCursorAndAttributes,
    TerminalCommandSetHorizontalTab,
    TerminalCommandClearHorizontalTab,
    TerminalCommandSetTopAndBottomMargin,
    TerminalCommandEraseInDisplay,
    TerminalCommandEraseInDisplaySelective,
    TerminalCommandEraseInLine,
    TerminalCommandEraseInLineSelective,
    TerminalCommandInsertLines,
    TerminalCommandDeleteLines,
    TerminalCommandInsertCharacters,
    TerminalCommandDeleteCharacters,
    TerminalCommandEraseCharacters,
    TerminalCommandKeypadNumeric,
    TerminalCommandKeypadApplication,
    TerminalCommandSetMode,
    TerminalCommandClearMode,
    TerminalCommandSetPrivateMode,
    TerminalCommandClearPrivateMode,
    TerminalCommandSelectG0CharacterSet,
    TerminalCommandSelectG1CharacterSet,
    TerminalCommandSelectG2CharacterSet,
    TerminalCommandSelectG3CharacterSet,
    TerminalCommandSelectGraphicRendition,
    TerminalCommandReset,
    TerminalCommandSoftReset,
    TerminalCommandDeviceAttributesPrimary,
    TerminalCommandDeviceAttributesSecondary,
    TerminalCommandScrollUp,
    TerminalCommandScrollDown,
    TerminalCommandDoubleLineHeightTopHalf,
    TerminalCommandDoubleLineHeightBottomHalf,
    TerminalCommandSingleWidthLine,
    TerminalCommandDoubleWidthLine,
    TerminalCommandCursorForwardTabStops,
    TerminalCommandCursorBackwardTabStops,
} TERMINAL_COMMAND, *PTERMINAL_COMMAND;

typedef enum _TERMINAL_KEY {
    TerminalKeyInvalid,
    TerminalKeyInsert,
    TerminalKeyDelete,
    TerminalKeyHome,
    TerminalKeyEnd,
    TerminalKeyPageUp,
    TerminalKeyPageDown,
    TerminalKeyUp,
    TerminalKeyDown,
    TerminalKeyLeft,
    TerminalKeyRight,
    TerminalKeyF1,
    TerminalKeyF2,
    TerminalKeyF3,
    TerminalKeyF4,
    TerminalKeyF5,
    TerminalKeyF6,
    TerminalKeyF7,
    TerminalKeyF8,
    TerminalKeyF9,
    TerminalKeyF10,
    TerminalKeyF11,
    TerminalKeyF12,
} TERMINAL_KEY, *PTERMINAL_KEY;

/*++

Structure Description:

    This structure stores the state for parsing or generating a terminal
    command.

Members:

    Flags - Stores parsing state. See TERMINAL_COMMAND_* flags.

    CommandCharacters - Stores the command characters.

    CommandCharacterCount - Stores the number of valid characters in the
        command.

    Command - Stores the interpreted command.

    ParameterCount - Stores the number of valid parameters.

    ParameterIndex - Stores the current parameter number being parsed.

    Parameters - Stores the array of numeric parameters.

--*/

typedef struct _TERMINAL_COMMAND_DATA {
    ULONG Flags;
    UINTN PreParameterSize;
    UINTN PostParameterSize;
    CHAR PreParameter[TERMINAL_MAX_COMMAND_CHARACTERS];
    CHAR PostParameter[TERMINAL_MAX_COMMAND_CHARACTERS];
    LONG CommandCharacterCount;
    TERMINAL_COMMAND Command;
    LONG ParameterCount;
    LONG ParameterIndex;
    LONG Parameter[TERMINAL_MAX_PARAMETERS];
} TERMINAL_COMMAND_DATA, *PTERMINAL_COMMAND_DATA;

/*++

Structure Description:

    This structure stores the state for parsing or generating a terminal
    command.

Members:

    Flags - Stores parsing state. See TERMINAL_KEY_* flags.

    Buffer - Stores the buffer containing the keys so far.

    BufferSize - Stores the number of valid bytes in the buffer.

    Key - Stores the resulting key.

--*/

typedef struct _TERMINAL_KEY_DATA {
    ULONG Flags;
    CHAR Buffer[TERMINAL_MAX_KEY_CHARACTERS];
    ULONG BufferSize;
    TERMINAL_KEY Key;
} TERMINAL_KEY_DATA, *PTERMINAL_KEY_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

TERMINAL_PARSE_RESULT
TermProcessOutput (
    PTERMINAL_COMMAND_DATA Command,
    CHAR Character
    );

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

VOID
TermNormalizeParameters (
    PTERMINAL_COMMAND_DATA Command
    );

/*++

Routine Description:

    This routine normalizes the command parameters to their expected defaults
    and allowed.

Arguments:

    Command - Supplies a pointer to the complete command.

Return Value:

    None.

--*/

BOOL
TermCreateOutputSequence (
    PTERMINAL_COMMAND_DATA Command,
    PSTR Buffer,
    UINTN BufferSize
    );

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

TERMINAL_PARSE_RESULT
TermProcessInput (
    PTERMINAL_KEY_DATA KeyData,
    CHAR Character
    );

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

BOOL
TermCreateInputSequence (
    PTERMINAL_KEY_DATA KeyData,
    PSTR Buffer,
    UINTN BufferSize
    );

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

