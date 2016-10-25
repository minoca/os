/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sed.h

Abstract:

    This header contains internal definitions for the sed (stream editor)
    utility.

Author:

    Evan Green 11-Jul-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <regex.h>
#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

#define SED_VERSION_MAJOR 1
#define SED_VERSION_MINOR 0

#define SED_USAGE                                                              \
    "usage: sed [-n] script [file...]\n"                                       \
    "       sed [-n] [-e script]... [-f scriptfile]... [file]...\n"            \
    "The sed (stream editor) utility processes text. Options are:\n"           \
    "  -e, --expression <expression> -- Use the given argument as a sed "      \
    "script.\n"                                                                \
    "  -f, --file <file> -- Read in the sed script contained in the given "    \
    "file.\n"                                                                  \
    "  -n, --quiet, --silent -- Suppress the default printing of input \n"     \
    "       lines to standard out.\n\n"                                        \
    "  --help -- Display this help screen and exit.\n"                         \
    "  --version -- Display the application version and exit.\n\n"             \
    "Script format:\n"                                                         \
    "  [address1[,address2]]function\n\n"                                      \
    "Addresses can be:\n"                                                      \
    "  Nothing, which matches every line in the input.\n"                      \
    "  A decimal line number, which matches a single line.\n"                  \
    "  A basic regular expression in the form /BRE/, which will match any \n"  \
    "  line that the expression matches.\n\n"                                  \
    "If two addresses are supplied, the function is executing for all lines \n"\
    "in between the two addresses, inclusive.\n\n"                             \
    "Available functions:\n"                                                   \
    "  { function...} -- Groups a block of functions together.\n"              \
    "  a\\\n"                                                                  \
    "  text -- Write text to standard out at the end of the current line.\n"   \
    "  b[label] -- Branch to the ':' function bearing the given label.\n"      \
    "  c\\\n"                                                                  \
    "  text -- Delete the pattern space. With zero or one addresses, or at \n" \
    "          the end of the range for two addresses, print the given "       \
    "text.\n"                                                                  \
    "  d -- Delete the pattern space and start the next cycle.\n"              \
    "  D -- Delete the pattern space up to the first newline and start the \n" \
    "       next cycle.\n"                                                     \
    "  g -- Replace the pattern space with the hold space.\n"                  \
    "  G -- Append a newline plus the hold space to the pattern space.\n"      \
    "  h -- Replace the hold space with the pattern space.\n"                  \
    "  H -- Append a newline plus the pattern space to the hold space.\n"      \
    "  i\\\n"                                                                  \
    "  text  -- Write the text to standard out.\n"                             \
    "  l -- Write the pattern space to standard out in a visually \n"          \
    "       unambiguous way. Non-printable characters are escaped, long \n"    \
    "       lines are folded, and a $ is written at the end of every line.\n"  \
    "  n -- Write the pattern space to standard out (unless -n is \n"          \
    "       specified), and replace the pattern space with the next line \n"   \
    "       less its ending newline.\n"                                        \
    "  N -- Append the next line of input less its trailing newline to the\n"  \
    "       pattern space, embedding a newline before the appended text.\n"    \
    "  p -- Write the pattern space to standard out.\n"                        \
    "  P -- Write the pattern space up to the first newline to standard out.\n"\
    "  q -- Branch to the end of the script and quit.\n"                       \
    "  r rfile -- Copy the contents of rfile to standard out. If rfile \n"     \
    "       cannot be opened, treat it like an empty file.\n"                  \
    "  s/BRE/replacement/flags -- Replace the first occurrence of text \n"     \
    "       matching the regular expression BRE in the hold space with the \n" \
    "       given replacement text. Use & in the replacement to specify the\n" \
    "       input text matching the BRE. Use \n (where n is 1 through 9) to \n"\
    "       specify the text matching the given subexpression. Flags are:\n"   \
    "       n -- Substitute only the nth occurrence.\n"                        \
    "       g -- Substitute every non-overlapping occurrence.\n"               \
    "       p -- Write to standard out if a replacement was made.\n"           \
    "       w wfile -- Append (write) to the given wfile if a replacement \n"  \
    "       was made.\n"                                                       \
    "  t[label] -- Branch to the given label if any substitutions have been \n"\
    "       made since reading an input line or executing a t.\n"              \
    "  w wfile -- Append (write) the pattern space to the given wfile.\n"      \
    "  x -- Exchange the pattern and hold spaces.\n"                           \
    "  y/string1/string2 -- Replace all occurrences of characters in string1\n"\
    "       with characters from string2. Use \\n for newline.\n"              \
    "  :label -- Do nothing. This denotes a label that can be jumped to.\n"    \
    "  = -- Write the current line number to standard out.\n"                  \
    "  # -- Comment. Ignore anything after this unless the first two \n"       \
    "       characters of a script are #n, which is equivalent to turning on\n"\
    "       the -n option.\n\n"

#define SED_OPTIONS_STRING "ne:f:"

#define SED_INITIAL_STRING_SIZE 32

#define SED_SUBSTITUTE_FLAG_GLOBAL 0x00000001
#define SED_SUBSTITUTE_FLAG_PRINT  0x00000002
#define SED_SUBSTITUTE_FLAG_WRITE  0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SED_ADDRESS_TYPE {
    SedAddressInvalid,
    SedAddressNumber,
    SedAddressExpression,
    SedAddressLastLine,
} SED_ADDRESS_TYPE, *PSED_ADDRESS_TYPE;

typedef enum _SED_FUNCTION_TYPE {
    SedFunctionInvalid,
    SedFunctionGroup,                   // {
    SedFunctionPrintTextAtLineEnd,      // a
    SedFunctionBranch,                  // b
    SedFunctionDeleteAndPrintText,      // c
    SedFunctionDelete,                  // d
    SedFunctionDeleteToNewline,         // D
    SedFunctionReplacePatternWithHold,  // g
    SedFunctionAppendHoldToPattern,     // G
    SedFunctionReplaceHoldWithPattern,  // h
    SedFunctionAppendPatternToHold,     // H
    SedFunctionPrintText,               // i
    SedFunctionWritePatternEscaped,     // l
    SedFunctionMoveToNextLine,          // n
    SedFunctionAppendNextLine,          // N
    SedFunctionWritePattern,            // p
    SedFunctionWritePatternToNewline,   // P
    SedFunctionQuit,                    // q
    SedFunctionReadFile,                // r
    SedFunctionSubstitute,              // s
    SedFunctionTest,                    // t
    SedFunctionWriteFile,               // w
    SedFunctionExchangePatternAndHold,  // x
    SedFunctionSubstituteCharacters,    // y
    SedFunctionLabel,                   // :
    SedFunctionWriteLineNumber,         // =
    SedFunctionNop,                     // #
    SedFunctionCount,
} SED_FUNCTION_TYPE, *PSED_FUNCTION_TYPE;

typedef struct _SED_COMMAND SED_COMMAND, *PSED_COMMAND;

/*++

Structure Description:

    This structure defines a mutable string in the sed utility.

Members:

    Data - Supplies a pointer to the malloced string buffer itself.

    Size - Supplies the number of valid bytes in the buffer including the
        null terminator if there is one.

    Capacity - Supplies the size of the buffer allocation.

--*/

typedef struct _SED_STRING {
    PSTR Data;
    UINTN Size;
    UINTN Capacity;
} SED_STRING, *PSED_STRING;

/*++

Structure Description:

    This structure defines a sed address, which is used to determine if the
    given command should be processed or not.

Members:

    Type - Stores the type of address (line number, last line, or regular
        expression).

    Expression - Stores the regular expression for expression based addresses.

    Line - Stores the line number for line based addresses.

--*/

typedef struct _SED_ADDRESS {
    SED_ADDRESS_TYPE Type;
    union {
        regex_t Expression;
        LONGLONG Line;
    } U;

} SED_ADDRESS, *PSED_ADDRESS;

/*++

Structure Description:

    This structure defines a sed write file entry.

Members:

    ListEntry - Stores pointers to the next and previous write file entries.

    File - Stores the open file descriptor.

    Name - Stores a pointer to the file name.

    LineTerminated - Stores a boolean indicating whether the previous line
        written to this file was terminated or not.

--*/

typedef struct _SED_WRITE_FILE {
    LIST_ENTRY ListEntry;
    FILE *File;
    PSED_STRING Name;
    BOOL LineTerminated;
} SED_WRITE_FILE, *PSED_WRITE_FILE;

/*++

Structure Description:

    This structure defines the parameters for a sed substitute (s) command.

Members:

    Expression - Stores the regular expression to substitute.

    Replacement - Stores the replacement string.

    Flags - Stores the flags coming with the substitute command. See
        SED_SUBSTITUTE_FLAG_* definitions.

    OccurrenceNumber - Stores the occurrence number to replace if supplied. Set
        to zero if none was supplied.

    WriteFile - Stores a pointer to the write file structure if there is a
        write file.

    Matches - Stores a pointer to an array of match structures to be used when
        executing the regular expression.

    MatchCount - Stores the number of elements in the match array.

--*/

typedef struct _SED_SUBSTITUTE {
    regex_t Expression;
    PSED_STRING Replacement;
    ULONG Flags;
    ULONG OccurrenceNumber;
    PSED_WRITE_FILE WriteFile;
    regmatch_t *Matches;
    ULONG MatchCount;
} SED_SUBSTITUTE, *PSED_SUBSTITUTE;

/*++

Structure Description:

    This structure defines the parameters for a sed characater substitute (y)
    command.

Members:

    Character - Stores a pointer to the string containing the characters to
        replace.

    Replacement - Stores a pointer to the replacement characters corresponding
        to each character.

--*/

typedef struct _SED_CHARACTER_SUBSTITUTE {
    PSED_STRING Characters;
    PSED_STRING Replacement;
} SED_CHARACTER_SUBSTITUTE, *PSED_CHARACTER_SUBSTITUTE;

/*++

Structure Description:

    This structure defines the parameters for a sed characater substitute (y)
    command.

Members:

    ListEntry - Stores the list entry of the next and previous commands on
        the context's append list.

    StringOrPath - Stores either the string to append or the path of the file
        to read.

--*/

typedef struct _SED_APPEND_ENTRY {
    LIST_ENTRY ListEntry;
    SED_FUNCTION_TYPE Type;
    PSED_STRING StringOrPath;
} SED_APPEND_ENTRY, *PSED_APPEND_ENTRY;

/*++

Structure Description:

    This structure defines a sed action.

Members:

    Type - Stores the type of the function.

    ChildList - Stores the list of children for a group of functions.

    StringArgument - Stores the argument for functions that take a single
        string argument (a, c, i, b, t, :, r, w).

    Substitute - Stores the paramters for a substitute command.

    CharacterSubstitute - Stores the parameters for a character substituteion
        (y) command.

    WriteFile - Stores a pointer to the open file for write file operations.

--*/

typedef struct _SED_FUNCTION {
    SED_FUNCTION_TYPE Type;
    union {
        LIST_ENTRY ChildList;
        PSED_STRING StringArgument;
        SED_SUBSTITUTE Substitute;
        SED_CHARACTER_SUBSTITUTE CharacterSubstitute;
        PSED_WRITE_FILE WriteFile;
    } U;

} SED_FUNCTION, *PSED_FUNCTION;

/*++

Structure Description:

    This structure defines a single sed command.

Members:

    ListEntry - Stores pointers to the next and previous commands in the cycle.

    Parent - Stores a pointer to the parent sed command, or NULL if this is the
        dummy command.

    AddressCount - Stores the number of valid addresses in the command.

    Address - Stores the start and ending addresses for the command.

    Active - Stores a boolean indicating if the current command has been
        activated by its start address range and is now looking for the stop
        address to deactivate.

    AddressNegated - Stores a boolean indicating that the function should be
        executed when the address does not match.

    Function - Stores the primary function represented by this command.

--*/

struct _SED_COMMAND {
    LIST_ENTRY ListEntry;
    PSED_COMMAND Parent;
    ULONG AddressCount;
    SED_ADDRESS Address[2];
    BOOL Active;
    BOOL AddressNegated;
    SED_FUNCTION Function;
};

/*++

Structure Description:

    This structure defines a sed input file.

Members:

    ListEntry - Stores pointers to the next and previous input entries.

    File - Stores the open file pointer, or NULL if the file could not be
        opened.

--*/

typedef struct _SED_INPUT {
    LIST_ENTRY ListEntry;
    FILE *File;
} SED_INPUT, *PSED_INPUT;

/*++

Structure Description:

    This structure defines a portion of a sed script, either an expression
    from the command line (-e), or an input file (-f).

Members:

    ListEntry - Stores pointers to the next and previous script fragments.

    ExpressionNumber - Stores the expression number if this scrap came from the
        command line. If it did not, this will be zero.

    FileName - Stores the name of the file if this scrap came form a script
        file. If it did not, this will be NULL.

    Offset - Stores the character offset where the fragment begins in the
        global script, inclusive.

    Size - Stores the number of bytes in the script fragment.

--*/

typedef struct _SED_SCRIPT_FRAGMENT {
    LIST_ENTRY ListEntry;
    ULONG ExpressionNumber;
    PSTR FileName;
    UINTN Offset;
    UINTN Size;
} SED_SCRIPT_FRAGMENT, *PSED_SCRIPT_FRAGMENT;

/*++

Structure Description:

    This structure defines the context for an instantiation of the sed
    application.

Members:

    PrintLines - Stores a boolean indicating whether or not to print each line.
        This is enabled by default and is shut off with the -n argument.

    LineNumber - Stores the line number, either of the input file being
        processed or the script file.

    CharacterNumber - Stores the character index of the script expression
        being processed.

    CommandLineExpressionCount - Stores the number of expressions that were
        entered via the command line.

    PreviousRegularExpression - Stores a pointer to the previous regular
        expression used in a context address or substitution. If a given
        regular expression is empty, this previous one is used.

    StandardOut - Stores the write file representing standard out.

    HeadCommand - Stores the initial group command that contains all other
        commands.

    ScriptList - Stores the head of the list of script fragments.

    ScriptString - Stores the complete script string.

    InputList - Stores the head of the list of input files.

    CurrentInputFile - Stores a pointer to the current input file.

    PatternSpace - Stores a pointer to the pattern space string.

    HoldSpace - Stores a pointer to the hold space string.

    AppendList - Stores the head of the list of things to append to the end of
        the line after further processing is complete.

    WriteFileList - Stores the head of the list of write file entries.

    NextCommand - Stores a pointer to the next command to be executed.

    TestResult - Stores a boolean indicating if a test command executed now
        would succeed.

    LastLine - Stores a boolean indicating whether this is the last line of
        the input or not.

    LineTerminator - Stores the stripped terminating character for this line, or
        EOF if there was none.

    Quit - Stores a boolean indicating if a quit command was executed.

    Done - Stores a boolean indicating if there is no more input left.

    SkipPrint - Stores a boolean indicating if the main routine should skip
        printing the line (assuming the print lines boolean is on).

--*/

typedef struct _SED_CONTEXT {
    BOOL PrintLines;
    ULONGLONG LineNumber;
    ULONGLONG CharacterNumber;
    ULONG CommandLineExpressionCount;
    PSED_STRING PreviousRegularExpression;
    SED_WRITE_FILE StandardOut;
    SED_COMMAND HeadCommand;
    LIST_ENTRY ScriptList;
    PSED_STRING ScriptString;
    LIST_ENTRY InputList;
    PSED_INPUT CurrentInput;
    PSED_STRING PatternSpace;
    PSED_STRING HoldSpace;
    LIST_ENTRY AppendList;
    LIST_ENTRY WriteFileList;
    PSED_COMMAND NextCommand;
    BOOL TestResult;
    BOOL LastLine;
    INT LineTerminator;
    BOOL Quit;
    BOOL Done;
    BOOL SkipPrint;
} SED_CONTEXT, *PSED_CONTEXT;

typedef
INT
(*PSED_EXECUTE_FUNCTION) (
    PSED_CONTEXT Context,
    PSED_COMMAND Command
    );

/*++

Routine Description:

    This routine executes a sed function.

Arguments:

    Context - Supplies a pointer to the application context.

    Command - Supplies a pointer to the command to execute.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

extern PSED_EXECUTE_FUNCTION SedFunctionTable[SedFunctionCount];

//
// -------------------------------------------------------- Function Prototypes
//

//
// Application level functions
//

INT
SedReadLine (
    PSED_CONTEXT Context
    );

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

//
// Parsing functions
//

BOOL
SedAddScriptFile (
    PSED_CONTEXT Context,
    PSTR Path
    );

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

BOOL
SedAddScriptString (
    PSED_CONTEXT Context,
    PSTR Script
    );

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

BOOL
SedParseScript (
    PSED_CONTEXT Context,
    PSTR Script
    );

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

VOID
SedDestroyCommands (
    PSED_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys any commands on the given context.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    None.

--*/

//
// Utility functions
//

PSED_STRING
SedReadFileIn (
    PSTR Path,
    BOOL MustSucceed
    );

/*++

Routine Description:

    This routine reads a file into a null terminated sed string.

Arguments:

    Path - Supplies a pointer to a string containing the path of the file to
        read in.

    MustSucceed - Supplies a boolean indicating whether or not the open and
        read calls must succeed, or should return a partial or empty string
        on failure.

Return Value:

    Returns a pointer to the string containing the contents of the file on
    success.

    NULL on failure.

--*/

PSED_STRING
SedCreateString (
    PSTR Data,
    UINTN Size,
    BOOL NullTerminate
    );

/*++

Routine Description:

    This routine creates a sed string.

Arguments:

    Data - Supplies an optional pointer to the initial data. This data will be
        copied.

    Size - Supplies the size of the initial data. Supply 0 if no data was
        supplied.

    NullTerminate - Supplies a boolean indicating if the string should be
        null terminated if it is not already.

Return Value:

    Returns a pointer to the allocated string structure on success. The caller
    is responsible for destroying this string structure.

    NULL on allocation failure.

--*/

BOOL
SedAppendString (
    PSED_STRING String,
    PSTR Data,
    UINTN Size
    );

/*++

Routine Description:

    This routine appends a string of characters to the given string. If the
    original string was null terminated, the resulting string will also be
    null terminated on success.

Arguments:

    String - Supplies a pointer to the string to append to.

    Data - Supplies a pointer to the bytes to append.

    Size - Supplies the number of bytes to append.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
SedDestroyString (
    PSED_STRING String
    );

/*++

Routine Description:

    This routine destroys a sed string structure.

Arguments:

    String - Supplies a pointer to the string to destroy.

Return Value:

    None.

--*/

INT
SedOpenWriteFile (
    PSED_CONTEXT Context,
    PSED_STRING Path,
    PSED_WRITE_FILE *WriteFile
    );

/*++

Routine Description:

    This routine opens up a write file, sharing descriptors between
    duplicate write file names.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the string containing the path of the write
        file.

    WriteFile - Supplies a pointer where the pointer to the write file will
        be returned on success.

Return Value:

    0 on success.

    Error code on failure.

--*/

INT
SedPrint (
    PSED_CONTEXT Context,
    PSTR String,
    INT LineTerminator
    );

/*++

Routine Description:

    This routine prints a null terminated string to standard out.

Arguments:

    Context - Supplies a pointer to the application context.

    String - Supplies the null terminated string to print.

    LineTerminator - Supplies the character that terminates this line. If this
        is EOF, then that tells this routine the line is not terminated.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

INT
SedWrite (
    PSED_WRITE_FILE WriteFile,
    PVOID Buffer,
    UINTN Size,
    INT LineTerminator
    );

/*++

Routine Description:

    This routine write the given buffer out to the given file descriptor.

Arguments:

    WriteFile - Supplies a pointer to the file to write to.

    Buffer - Supplies the buffer to write.

    Size - Supplies the number of characters in the buffer.

    LineTerminator - Supplies the character that terminates this line. If this
        is EOF, then that tells this routine the line is not terminated.

Return Value:

    0 on success.

    Non-zero error code on failure.

--*/

