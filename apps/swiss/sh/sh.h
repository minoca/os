/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sh.h

Abstract:

    This header contains definitions for the shell application.

Author:

    Evan Green 5-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <stdio.h>
#include <sys/types.h>
#include "shos.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro prints the given string to standard error.
//

#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__)

//
// ---------------------------------------------------------------- Definitions
//

#define SH_VERSION_MAJOR 1
#define SH_VERSION_MINOR 0

//
// Define the shell options.
//

//
// This option is set to indicate whether all variable assignments should be
// exported.
//

#define SHELL_OPTION_EXPORT_ALL 0x00000001

//
// This option is set to indicate whether job notifications should come in
// asynchronously or not.
//

#define SHELL_OPTION_ASYNCHRONOUS_JOB_NOTIFICATION 0x00000002

//
// This option is set to indicate that the output redirector '>' should not
// overwrite existing files.
//

#define SHELL_OPTION_NO_CLOBBER 0x00000004

//
// This option is set to indicate that any error that is not part of a
// compound list following an if, while, or until, is not part of an And-Or
// list, and is not preceded by a pipeline Bang (!) should cause the shell to
// exit immediately.
//

#define SHELL_OPTION_EXIT_ON_FAILURE 0x00000008

//
// This option is set to indicate whether to disable automatic pathname
// expansion.
//

#define SHELL_OPTION_NO_PATHNAME_EXPANSION 0x00000010

//
// This option is set to indicate indicating that utilities invoked by
// functions should be located and remembered when the function is defined.
// Normally utilities are located when the function is executed.
//

#define SHELL_OPTION_LOCATE_UTILITIES_IN_DECLARATION 0x00000020

//
// This option is set to indicate whether all jobs should be run in their
// own process group or not.
//

#define SHELL_OPTION_RUN_JOBS_IN_SEPARATE_PROCESS_GROUP 0x00000040

//
// This option is set to indicate whether the shell should read but not
// execute any commands, like a dry run.
//

#define SHELL_OPTION_NO_EXECUTE 0x00000080

//
// This option is set to indicate whether or not to exit if the shell tries
// to expand any variable that is not set.
//

#define SHELL_OPTION_EXIT_ON_UNSET_VARIABLE 0x00000100

//
// This option is set to indicate that the shell should write its input to
// standard error as it is read.
//

#define SHELL_OPTION_DISPLAY_INPUT 0x00000200

//
// This option is set to indicate whether the shell should write a trace of
// each command after it expands the command to standard error before it
// executes the command.
//

#define SHELL_OPTION_TRACE_COMMAND 0x0000400

//
// This option is set to indicate whether this shell is interactive or not.
//

#define SHELL_OPTION_INTERACTIVE 0x00000800

//
// Define the options that get switched together with the -i argument.
//

#define SHELL_INTERACTIVE_OPTIONS (SHELL_OPTION_INTERACTIVE |   \
                                   SHELL_OPTION_PRINT_PROMPTS)

//
// This option is set to read commands from standard input.
//

#define SHELL_OPTION_READ_FROM_STDIN 0x00001000

//
// This option is set on an interactive shell to ignore EOF inputs from
// standard in.
//

#define SHELL_OPTION_IGNORE_EOF 0x00002000

//
// This option is set to prevent creating of a history list.
//

#define SHELL_OPTION_NO_COMMAND_HISTORY 0x00004000

//
// This internal option is set to print prompts. Usually it matches the
// state of the interactive flag. Occasionally it does not (eg executing the
// eval statement).
//

#define SHELL_OPTION_PRINT_PROMPTS 0x00008000

//
// This internal option is set to enable the shell's input processing and
// echoing.
//

#define SHELL_OPTION_RAW_INPUT 0x00010000

//
// This option enables debugging spew for the shell app.
//

#define SHELL_OPTION_DEBUG 0x00020000

//
// Set this option to stop after the input buffer is finished.
//

#define SHELL_OPTION_INPUT_BUFFER_ONLY 0x00040000

//
// Define shell execution node flags.
//

//
// This flag is set indicating that the body of a statement is running. For
// functions, this means the function is executing as opposed to being
// declared. For while and until loops this means the body is running and not
// the condition. And for if statements this means the if or else is running
// and not the condition.
//

#define SHELL_EXECUTION_BODY 0x00000001

//
// This flag is set if the node needs to restore shell options.
//

#define SHELL_EXECUTION_RESTORE_OPTIONS 0x00000002

//
// Define the error value for when the shell can't open a script or executable
// file.
//

#define SHELL_ERROR_OPEN 127

//
// Define the error value for when the shell can't execute a script or binary.
//

#define SHELL_ERROR_EXECUTE 126

//
// Define the default size of the input buffer in bytes.
//

#define DEFAULT_INPUT_BUFFER_SIZE 1024

//
// Define the default size of the token buffer.
//

#define DEFAULT_TOKEN_BUFFER_SIZE 256

//
// Define expansion behavior options.
//

#define SHELL_EXPANSION_OPTION_NO_QUOTE_REMOVAL   0x00000001
#define SHELL_EXPANSION_OPTION_NO_FIELD_SPLIT     0x00000002
#define SHELL_EXPANSION_OPTION_NO_TILDE_EXPANSION 0x00000004
#define SHELL_EXPANSION_OPTION_NO_PATH_EXPANSION  0x00000008

//
// Define dequoting behaviors.
//

//
// Set this flag to convert escape control characters to backslashes, so that
// the pattern matching function treats the following character as a literal.
//

#define SHELL_DEQUOTE_FOR_PATTERN_MATCHING 0x00000001

//
// Define special variable names.
//

#define SHELL_CDPATH "CDPATH"
#define SHELL_ENV "ENV"
#define SHELL_HOME "HOME"
#define SHELL_IFS "IFS"
#define SHELL_LINE_NUMBER "LINENO"
#define SHELL_OLDPWD "OLDPWD"
#define SHELL_PATH "PATH"
#define SHELL_PS1 "PS1"
#define SHELL_PS2 "PS2"
#define SHELL_PS4 "PS4"
#define SHELL_PWD "PWD"
#define SHELL_RANDOM "RANDOM"
#define SHELL_OPTION_INDEX "OPTIND"
#define SHELL_OPTION_ARGUMENT "OPTARG"
#define SHELL_OPTION_ERROR "OPTERR"

#define SHELL_IFS_DEFAULT " \t\n"

#define SHELL_GLOBAL_PROFILE_PATH "/etc/profile"
#define SHELL_USER_PROFILE_PATH ".profile"

//
// Define shell control characters.
//

#define SHELL_CONTROL_ESCAPE ((CHAR)(-127))
#define SHELL_CONTROL_QUOTE ((CHAR)(-126))

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SHELL_NODE SHELL_NODE, *PSHELL_NODE;

typedef enum _SHELL_EXPANSION_TYPE {
    ShellExpansionInvalid,
    ShellExpansionFieldSplit,
    ShellExpansionNoFieldSplit,
    ShellExpansionSplitOnNull
} SHELL_EXPANSION_TYPE, *PSHELL_EXPANSION_TYPE;

typedef enum _SHELL_UNSET_TYPE {
    ShellUnsetDefault,
    ShellUnsetVariable,
    ShellUnsetFunction
} SHELL_UNSET_TYPE, *PSHELL_UNSET_TYPE;

/*++

Structure Description:

    This structure defines a shell signal action.

Members:

    ListEntry - Stores pointers to the next and previous signal actions in the
        shell.

    SignalNumber - Stores the signal number associated with this action.

    Action - Stores a pointer to the action to perform upon receiving this
        signal.

    ActionSize - Stores the size of the action buffer in bytes including the
        null terminator.

--*/

typedef struct _SHELL_SIGNAL_ACTION {
    LIST_ENTRY ListEntry;
    SHELL_SIGNAL SignalNumber;
    PSTR Action;
    UINTN ActionSize;
} SHELL_SIGNAL_ACTION, *PSHELL_SIGNAL_ACTION;

/*++

Structure Description:

    This structure defines a shell alias entry.

Members:

    ListEntry - Stores pointers to the next and previous aliases in the shell.

    Name - Stores a pointer to the name that triggers the alias replacement.

    NameSize - Stores the size of the name buffer in bytes including the null
        terminator.

    Value - Stores a pointer to the string containing the replacement text.

    ValueSize - Stores the size of the replacement value in bytes.

--*/

typedef struct _SHELL_ALIAS {
    LIST_ENTRY ListEntry;
    PSTR Name;
    UINTN NameSize;
    PSTR Value;
    UINTN ValueSize;
} SHELL_ALIAS, *PSHELL_ALIAS;

/*++

Structure Description:

    This structure defines a directory entry.

Members:

    Name - Supplies a pointer to the name of the file.

    NameSize - Supplies the size of the name in bytes including the null
        terminator.

--*/

typedef struct _SHELL_DIRECTORY_ENTRY {
    PSTR Name;
    UINTN NameSize;
} SHELL_DIRECTORY_ENTRY, *PSHELL_DIRECTORY_ENTRY;

/*++

Structure Description:

    This structure defines an environment variable.

Members:

    ListEntry - Stores pointers to the next and previous environment variables
        in this node or shell.

    Hash - Stores the hash of the variable name.

    Name - Stores a pointer to the name string.

    NameSize - Stores the size of the name in bytes including the null
        terminator.

    Value - Stores a pointer to the value string.

    ValueSize - Stores the size of the value string in bytes including the
        null terminator.

    OriginalValue - Stores the original value of this variable for exported
        variables.

    OriginalValueSize - Stores the size of the original value buffer in bytes
        including the null terminator.

    Exported - Stores a boolean indicating if the variable is marked for export.

    ReadOnly - Stores a boolean indicating if the variable cannot be unset or
        modified.

    Set - Stores a boolean indicating if the given variable is set or not.
        Unset variables can exist if they're marked as exported or read-only.

--*/

typedef struct _SHELL_VARIABLE {
    LIST_ENTRY ListEntry;
    ULONG Hash;
    PSTR Name;
    UINTN NameSize;
    PSTR Value;
    UINTN ValueSize;
    PSTR OriginalValue;
    ULONG OriginalValueSize;
    BOOL Exported;
    BOOL ReadOnly;
    BOOL Set;
} SHELL_VARIABLE, *PSHELL_VARIABLE;

/*++

Structure Description:

    This structure defines a declared function.

Members:

    ListEntry - Stores pointers to the next and previous functions in the
        global list.

    Name - Stores a pointer to the function name string.

    NameSize - Stores the size of the function name string in bytes including
        the null terminator.

    Node - Stores a pointer to the shell node containing the function
        definition.

--*/

typedef struct _SHELL_FUNCTION {
    LIST_ENTRY ListEntry;
    PSHELL_NODE Node;
} SHELL_FUNCTION, *PSHELL_FUNCTION;

/*++

Structure Description:

    This structure defines an execution node in the shell.

Members:

    ListEntry - Stores pointers to the next and previous execution nodes in
        the system. The next pointer points at older nodes on the stack.

    VariableList - Stores the head of the list of variables assigned in this
        scope.

    ArgumentList - Stores the list of arguments this function was invoked with.

    ActiveRedirectList - Stores the list of active I/O redirections.

    Node - Stores a pointer to the shell node being run in this stack frame.

    Flags - Stores a bitfield of flags describing the state of the node. See
        SHELL_EXECUTION_* definitions.

    SavedOptions - Stores the shell options to restore when the node is popped
        off the execution stack.

    ReturnValue - Stores the return value of the node.

--*/

typedef struct _SHELL_EXECUTION_NODE {
    LIST_ENTRY ListEntry;
    LIST_ENTRY VariableList;
    LIST_ENTRY ArgumentList;
    LIST_ENTRY ActiveRedirectList;
    PSHELL_NODE Node;
    ULONG Flags;
    ULONG SavedOptions;
    INT ReturnValue;
} SHELL_EXECUTION_NODE, *PSHELL_EXECUTION_NODE;

/*++

Structure Description:

    This structure defines an argument to the shell or function.

Members:

    ListEntry - Stores pointers to the next and previous arguments in the
        command (or function).

    Name - Stores a pointer to the argument string.

    NameSize - Stores the size of the argument buffer in bytes including the
        null terminator.

--*/

typedef struct _SHELL_ARGUMENT {
    LIST_ENTRY ListEntry;
    PSTR Name;
    UINTN NameSize;
} SHELL_ARGUMENT, *PSHELL_ARGUMENT;

/*++

Structure Description:

    This structure stores information about a range in a string that has
    undergone an expansion.

Members:

    ListEntry - Stores pointers to the next and previous expansion ranges.

    Type - Stores the type of expansion this range represents.

    Index - Stores the starting index to the expansion.

    Length - Stores the number of bytes in the expansion.

--*/

typedef struct _SHELL_EXPANSION_RANGE {
    LIST_ENTRY ListEntry;
    SHELL_EXPANSION_TYPE Type;
    INTN Index;
    UINTN Length;
} SHELL_EXPANSION_RANGE, *PSHELL_EXPANSION_RANGE;

/*++

Structure Description:

    This structure defines the lexer state for a shell.

Members:

    InputFile - Stores the open file handle where the shell should draw more
        input from.

    InputBuffer - Stores the buffer of input currently held.

    InputBufferCapacity - Stores the total size of the input buffer.

    InputBufferSize - Stores the number of valid bytes currently in the input
        buffer.

    InputBufferNextIndex - Stores the index into the input buffer where the
        next character will be fetched.

    LineNumber - Stores the line number the lexer is working on.

    TokenType - Stores the type of token this current token represents. See
        TOKEN_* definitions.

    TokenBuffer - Stores a pointer to the buffer containing the current token.

    TokenBufferCapacity - Stores the total size fo the token buffer.

    TokenBufferSize - Stores the number of valid bytes in the token buffer.

    UnputCharacter - Stores the character that got put back into the input.

    UnputCharacterValid - Stores a boolean indicating whether or not the unput
        character is valid.

    LexerPrimed - Stores a boolean indicating whether the lexer token is valid
        (TRUE) or uninitialized (FALSE).

    LastAlias - Stores a pointer to the last alias that was processed.

    HereDocumentList - Stores the list of here documents that have yet
        to be collected by the parser.

--*/

typedef struct _SHELL_LEXER_STATE {
    FILE *InputFile;
    PSTR InputBuffer;
    UINTN InputBufferCapacity;
    UINTN InputBufferSize;
    UINTN InputBufferNextIndex;
    ULONG LineNumber;
    ULONG TokenType;
    PSTR TokenBuffer;
    UINTN TokenBufferCapacity;
    UINTN TokenBufferSize;
    CHAR UnputCharacter;
    BOOL UnputCharacterValid;
    BOOL LexerPrimed;
    PSHELL_ALIAS LastAlias;
    LIST_ENTRY HereDocumentList;
} SHELL_LEXER_STATE, *PSHELL_LEXER_STATE;

/*++

Structure Description:

    This structure defines a shell interpreter.

Members:

    Lexer - Stores the lexer state.

    Parser - Stores the parser state.

    VariableList - Stores the head of the list of environment variables for
        this shell.

    ExecutionStack - Stores the stack of nodes being executed. The next pointer
        points to the newest thing (items are pushed onto the front of the
        list).

    ArgumentList - Stores the list of arguments this shell was invoked with.

    FunctionList - Stores the list of functions owned in this shell.

    AliasList - Stores the list of aliases in the shell.

    SignalActionList - Stores the list of signal actions for this shell.

    CommandName - Stores a pointer to the command name string.

    CommandNameSize - Stores the size of the command name string in bytes
        including the null terminator.

    ReturnValue - Stores the return value of the currently executing pipeline.

    LastReturnValue - Stores the return value of the previously executed
        pipeline.

    ProcessId - Stores the process ID of she shell.

    LastBackgroundProcessId - Stores the process ID of the most recent
        background process.

    Options - Stores the bitfield of shell behavior options. See SHELL_OPTION_*
        definitions.

    ExecutingLineNumber - Stores the line number currently being executed.

    Exited - Stores a boolean indicating whether the shell has exited or not.

    SkipExitSignal - Stores a boolean indicating whether to skip processing
        the exit signal (when emulating something like an exec) or not.

    LastSignalCount - Stores the total signal count the last time the shell
        looked for pending signals.

    OriginalUmask - Stores the umask of the shell when it was created.

    NonStandardError - Stores a pointer to the stream containing the original
        standard error.

    ActiveRedirectList - Stores the shell-wide redirect list, which gets put
        back when the shell is restored.

    Prompt - Stores a pointer to the last printed prompt.

    PostForkCloseDescriptor - Stores a file descriptor to close after forking.
        This is needed so that a shell process in a pipeline waiting on a
        child subprocess to finish doesn't hold the read end open.

--*/

typedef struct _SHELL {
    SHELL_LEXER_STATE Lexer;
    LIST_ENTRY VariableList;
    LIST_ENTRY ExecutionStack;
    LIST_ENTRY ArgumentList;
    LIST_ENTRY FunctionList;
    LIST_ENTRY AliasList;
    LIST_ENTRY SignalActionList;
    PSTR CommandName;
    UINTN CommandNameSize;
    INT ReturnValue;
    INT LastReturnValue;
    LONG ProcessId;
    LONG LastBackgroundProcessId;
    ULONG Options;
    ULONG ExecutingLineNumber;
    BOOL Exited;
    BOOL SkipExitSignal;
    INT LastSignalCount;
    mode_t OriginalUmask;
    FILE *NonStandardError;
    LIST_ENTRY ActiveRedirectList;
    PSTR Prompt;
    INT PostForkCloseDescriptor;
} SHELL, *PSHELL;

typedef
INT
(*PSHELL_BUILTIN_COMMAND) (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the entry point for a builtin shell command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the integer return value from the built-in command, which will be
    placed in the node on top of the execution stack. This may not be the node
    that executed this command, as this command may have popped things off the
    execution stack (such as a return, break, or continue will do).

--*/

/*++

Structure Description:

    This structure defines an active I/O redirection. It contains the
    information needed to restore the redirect to its original state.

Members:

    ListEntry - Stores pointers to the next and previous active redirections in
        the node.

    FileNumber - Stores the descriptor of the file number that was overridden
        by the redirection.

    OriginalDescriptor - Stores another handle representing the original
        descriptor before the redirection took it over.

    ChildProcessId - Stores the ID of the child process created if creating
        this redirect involved creating a child process to push the I/O.

--*/

typedef struct _SHELL_ACTIVE_REDIRECT {
    LIST_ENTRY ListEntry;
    INT FileNumber;
    INT OriginalDescriptor;
    pid_t ChildProcessId;
} SHELL_ACTIVE_REDIRECT, *PSHELL_ACTIVE_REDIRECT;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the various globals that enable debug spew if the shell seems to be
// misunderstanding something.
//

extern BOOL ShDebugAlias;
extern BOOL ShDebugArithmeticLexer;
extern BOOL ShDebugArithmeticParser;
extern BOOL ShDebugLexer;
extern BOOL ShDebugPrintParseTree;

//
// Set this variable if swiss command should be recognized even before
// searching the path.
//

extern BOOL ShUseSwissBuiltins;

//
// Define the set of characters that need to be escaped if inside double quotes.
//

extern CHAR ShQuoteEscapeCharacters[];

//
// Define some editing characters that vary across systems and terminals.
//

extern CHAR ShBackspaceCharacter;
extern CHAR ShKillLineCharacter;

//
// -------------------------------------------------------- Function Prototypes
//

//
// Application level functions.
//

BOOL
ShSetOptions (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize,
    BOOL LongForm,
    BOOL Set,
    PBOOL HasC
    );

/*++

Routine Description:

    This routine sets shell behavior options.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string containing the options.

    StringSize - Supplies the size of the string in bytes including the null
        terminator.

    LongForm - Supplies a boolean indicating if this is a long form option,
        where the name of the option is spelled out.

    Set - Supplies whether or not the longform argument is a set (-o) or clear
        (+o) operation. For non longform arguments, this parameter is ignored.

    HasC - Supplies an optional boolean indicating if the option string has -c
        int it somewhere. If NULL, then -c is not allowed.

Return Value:

    TRUE on success.

    FALSE if an invalid option was supplied.

--*/

//
// Execution functions
//

BOOL
ShExecute (
    PSHELL Shell,
    PINT ReturnValue
    );

/*++

Routine Description:

    This routine executes commands from the input of the shell.

Arguments:

    Shell - Supplies a pointer to the shell whose input should be read and
        executed.

    ReturnValue - Supplies a pointer where the return value of the shell will
        be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShRestoreRedirections (
    PSHELL Shell,
    PLIST_ENTRY ActiveRedirectList
    );

/*++

Routine Description:

    This routine restores all active redirections back to their previous state.

Arguments:

    Shell - Supplies a pointer to the shell.

    ActiveRedirectList - Supplies a pointer to the active redirect list.

Return Value:

    None.

--*/

VOID
ShSetTerminalMode (
    PSHELL Shell,
    BOOL Raw
    );

/*++

Routine Description:

    This routine potentially sets the terminal input mode one way or another.
    If the interactive flag is not set, this does nothing.

Arguments:

    Shell - Supplies a pointer to the shell. If the interactive flag is not set
        in this shell, then nothing happens.

    Raw - Supplies a boolean indicating whether to set it in raw mode (TRUE)
        or it's previous original mode (FALSE).

Return Value:

    None.

--*/

INT
ShRunCommand (
    PSHELL Shell,
    PSTR Command,
    PSTR *Arguments,
    INT ArgumentCount,
    INT Asynchronous,
    PINT ReturnValue
    );

/*++

Routine Description:

    This routine is called to run a basic command for the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command name to run.

    Arguments - Supplies a pointer to an array of command argument strings.
        This includes the first argument, the command name.

    ArgumentCount - Supplies the number of arguments on the command line.

    Asynchronous - Supplies 0 if the shell should wait until the command is
        finished, or 1 if the function should return immediately with a
        return value of 0.

    ReturnValue - Supplies a pointer where the return value from the executed
        program will be returned.

Return Value:

    0 if the executable was successfully launched.

    Non-zero if there was trouble launching the executable.

--*/

//
// Utility functions
//

PSHELL
ShCreateShell (
    PSTR CommandName,
    UINTN CommandNameSize
    );

/*++

Routine Description:

    This routine creates a new shell object.

Arguments:

    CommandName - Supplies a pointer to the string containing the name of the
        command that invoked the shell. A copy of this buffer will be made.

    CommandNameSize - Supplies the length of the command name buffer in bytes
        including the null terminator.

Return Value:

    Returns a pointer to the allocated shell on success.

    NULL on failure.

--*/

VOID
ShDestroyShell (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine destroys a new shell object.

Arguments:

    Shell - Supplies a pointer to the shell to destroy.

Return Value:

    None.

--*/

PSHELL
ShCreateSubshell (
    PSHELL Shell,
    PSTR Input,
    UINTN InputSize,
    BOOL DequoteForSubshell
    );

/*++

Routine Description:

    This routine creates a subshell based on the given parent shell.

Arguments:

    Shell - Supplies a pointer to the shell to copy.

    Input - Supplies a pointer to the input to feed the subshell with. This
        string will be copied, and potentially dequoted.

    InputSize - Supplies the size of the input string in bytes including the
        null terminator.

    DequoteForSubshell - Supplies a boolean indicating if backslashes that
        follow $, `, or \ should be removed.

Return Value:

    Returns a pointer to the initialized subshell on success.

--*/

BOOL
ShExecuteSubshell (
    PSHELL ParentShell,
    PSHELL Subshell,
    BOOL Asynchronous,
    PSTR *Output,
    PUINTN OutputSize,
    PINT ReturnValue
    );

/*++

Routine Description:

    This routine executes a subshell.

Arguments:

    ParentShell - Supplies a pointer to the parent shell that's executing this
        subshell.

    Subshell - Supplies a pointer to the subshell to execute.

    Asynchronous - Supplies a boolean indicating whether or not the execution
        should occur in the background.

    Output - Supplies an optional pointer that receives the contents of
        standard output. The caller is responsible for freeing this memory.

    OutputSize - Supplies a pointer where the size of the output in bytes
        will be returned, with no null terminator.

    ReturnValue - Supplies a pointer where the return value of the subshell
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShPrintPrompt (
    PSHELL Shell,
    ULONG PromptNumber
    );

/*++

Routine Description:

    This routine prints a shell prompt.

Arguments:

    Shell - Supplies a pointer to the shell.

    PromptNumber - Supplies the prompt number to print. Valid values are 1, 2,
        and 4 for PS1, PS2, and PS4.

Return Value:

    None. Failure to print the prompt is not considered fatal.

--*/

VOID
ShStringDequote (
    PSTR String,
    UINTN StringSize,
    ULONG Options,
    PUINTN NewStringSize
    );

/*++

Routine Description:

    This routine performs an in-place removal of all shell control characters.

Arguments:

    String - Supplies a pointer to the string to de-quote.

    StringSize - Supplies the size of the string in bytes including the null
        terminator. On output this value will be updated to reflect the removal.

    Options - Supplies the bitfield of expansion options.
        See SHELL_DEQUOTE_* definitions.

    NewStringSize - Supplies a pointer where the adjusted size of the string
        will be returned.

Return Value:

    None.

--*/

BOOL
ShStringAppend (
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    PSTR Component,
    UINTN ComponentSize
    );

/*++

Routine Description:

    This routine adds a string onto the end of another string, separated by a
    space.

Arguments:

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    Component - Supplies a pointer to the component string to append after
        first appending a space to the original string buffer.

    ComponentSize - Supplies the size of the component string in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

BOOL
ShStringFormatForReentry (
    PSTR String,
    UINTN StringSize,
    PSTR *FormattedString,
    PUINTN FormattedStringSize
    );

/*++

Routine Description:

    This routine creates a formatted version of the given string suitable for
    re-entry into the shell. This is accomplished by surrounding the string in
    single quotes, handling single quotes in the input string specially.

Arguments:

    String - Supplies a pointer to the string to format.

    StringSize - Supplies the size of the input string in bytes including the
        null terminator. If there is no null terminator, one will be added.

    FormattedString - Supplies a pointer where the formatted string will be
        returned on success. The caller will be responsible for freeing this
        memory.

    FormattedStringSize - Supplies a pointer where the formatted string size
        will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShFieldSplit (
    PSHELL Shell,
    PSTR *StringBuffer,
    PUINTN StringBufferSize,
    PLIST_ENTRY ExpansionList,
    ULONG MaxFieldCount,
    PSTR **FieldsArray,
    PULONG FieldsArrayCount
    );

/*++

Routine Description:

    This routine performs field splitting on the given string.

Arguments:

    Shell - Supplies a pointer to the shell instance.

    StringBuffer - Supplies a pointer where the address of the fields string
        buffer is on input. On output, this may contain a different buffer that
        all the fields point into.

    StringBufferSize - Supplies a pointer that contains the size of the fields
        string buffer. This value will be updated to reflect the new size.

    ExpansionList - Supplies a pointer to the list of expansions within this
        string.

    MaxFieldCount - Supplies a maximum number of fields to create. When this
        number is reached, the last field contains the rest of the string.
        Supply 0 to indicate no limit.

    FieldsArray - Supplies a pointer where the array of pointers to the fields
        will be returned. This array will contain a NULL entry at the end of it,
        though that entry will not be included in the field count. The caller
        is responsible for freeing this memory.

    FieldsArrayCount - Supplies a pointer where the number of elements in the
        returned field array will be returned on success. This number does not
        include the null terminator entry.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShDeNullExpansions (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize
    );

/*++

Routine Description:

    This routine removes the null separators from any expansion range that
    specified to split on nulls. This is called if field splitting didn't end
    up occurring.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the string to de-null. This buffer will be
        modified.

    StringSize - Supplies the size of the input string in bytes including the
        null terminator.

Return Value:

    None.

--*/

VOID
ShPrintTrace (
    PSHELL Shell,
    PSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints out tracing information to standard error (as it was
    when the process was created). This avoids printing trace information to
    commands that have redirected standard error.

Arguments:

    Shell - Supplies a pointer to the shell.

    Format - Supplies the printf style format string.

    ... - Supplies the remaining arguments as dictated by the format.

Return Value:

    None.

--*/

int
ShDup (
    PSHELL Shell,
    int FileDescriptor,
    int Inheritable
    );

/*++

Routine Description:

    This routine duplicates the given file descriptor.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to duplicate.

    Inheritable - Supplies a boolean (zero or non-zero) indicating if the new
        handle should be inheritable outside this process.

Return Value:

    Returns the new file descriptor which represents a copy of the original
    file descriptor.

    -1 on failure, and errno will be set to contain more information.

--*/

int
ShDup2 (
    PSHELL Shell,
    int FileDescriptor,
    int CopyDescriptor
    );

/*++

Routine Description:

    This routine duplicates the given file descriptor to the destination
    descriptor, closing the original destination descriptor file along the way.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to duplicate.

    CopyDescriptor - Supplies the descriptor number of returned copy.

Return Value:

    Returns the new file descriptor which represents a copy of the original,
    which is also equal to the input copy descriptor parameter.

    -1 on failure, and errno will be set to contain more information.

--*/

int
ShClose (
    PSHELL Shell,
    int FileDescriptor
    );

/*++

Routine Description:

    This routine closes a file descriptor.

Arguments:

    Shell - Supplies a pointer to the shell.

    FileDescriptor - Supplies the file descriptor to close.

Return Value:

    0 on success.

    -1 if the file could not be closed properly. The state of the file
    descriptor is undefined, but in many cases is still open. The errno
    variable will be set to contain more detailed information.

--*/

//
// Variable expansion functions.
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
    );

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

PLIST_ENTRY
ShGetCurrentArgumentList (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine returns active argument list, which is either the current
    function executing or the shell's list.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    Returns a pointer to the active argument list.

--*/

BOOL
ShExpandPrompt (
    PSHELL Shell,
    PSTR String,
    UINTN StringSize,
    PSTR *ExpandedString,
    PUINTN ExpandedStringSize
    );

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

//
// Environment variable functions
//

BOOL
ShInitializeVariables (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine performs some variable initialization in the shell, as well as
    handling the ENV variable.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShGetVariable (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR *Value,
    PUINTN ValueSize
    );

/*++

Routine Description:

    This routine gets the value of the given environment variable.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies a pointer where a pointer to the value string of the
        variable will be returned on success. The caller does not own this
        memory and should not edit or free it.

    ValueSize - Supplies an optional pointer where the the size of the value
        string in bytes including the null terminator will be returned.

Return Value:

    TRUE if the variable is set (null or not).

    FALSE if the variable is unset.

--*/

BOOL
ShSetVariable (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize
    );

/*++

Routine Description:

    This routine sets a shell variable in the proper scope.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShSetVariableWithProperties (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    PSTR Value,
    UINTN ValueSize,
    BOOL Exported,
    BOOL ReadOnly,
    BOOL Set
    );

/*++

Routine Description:

    This routine sets a shell variable in the proper scope.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to set.
        A copy of this string will be made.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Value - Supplies the value string of the variable to set. This may get
        expanded. A copy of this string will be made.

    ValueSize - Supplies the size of the value string in bytes, including the
        null terminator.

    Exported - Supplies a boolean indicating if the variable should be marked
        for export.

    ReadOnly - Supplies a boolean indicating if the variable should be marked
        read-only.

    Set - Supplies a boolean indicating if the variable should be set or not.
        If this value is FALSE, the value parameter will be ignored.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShUnsetVariableOrFunction (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize,
    SHELL_UNSET_TYPE Type
    );

/*++

Routine Description:

    This routine unsets an environment variable or function.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to
        unset.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

    Type - Supplies the type of unset to perform: either the default behavior
        to try to unset a variable and then a function, or to just try to unset
        either a variable or a function.

Return Value:

    TRUE if the variable was successfully unset.

    FALSE if the variable already doesn't exist or is read-only.

--*/

BOOL
ShExecuteVariableAssignments (
    PSHELL Shell,
    PSHELL_EXECUTION_NODE ExecutionNode
    );

/*++

Routine Description:

    This routine performs any variable assignments in the given node.

Arguments:

    Shell - Supplies a pointer to the shell.

    ExecutionNode - Supplies a pointer to the node containing the assignments.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShCopyVariables (
    PSHELL Source,
    PLIST_ENTRY DestinationList
    );

/*++

Routine Description:

    This routine copies all the variables visible in the current shell over to
    the new shell.

Arguments:

    Source - Supplies a pointer to the shell containing the variables to copy.

    DestinationList - Supplies a pointer to the head of the list where the
        copies will be put.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShDestroyVariableList (
    PLIST_ENTRY List
    );

/*++

Routine Description:

    This routine destroys an environment variable list.

Arguments:

    List - Supplies a pointer to the list to destroy.

Return Value:

    None.

--*/

PSHELL_FUNCTION
ShGetFunction (
    PSHELL Shell,
    PSTR Name,
    UINTN NameSize
    );

/*++

Routine Description:

    This routine returns a pointer to the function information for a function
    of the given name.

Arguments:

    Shell - Supplies a pointer to the shell.

    Name - Supplies a pointer to the string of the name of the variable to get.

    NameSize - Supplies the size of the name string buffer in bytes including
        the null terminator.

Return Value:

    Returns a pointer to the variable on success.

    NULL if the variable could not be found.

--*/

BOOL
ShDeclareFunction (
    PSHELL Shell,
    PSHELL_NODE Function
    );

/*++

Routine Description:

    This routine sets a function declaration in the given shell.

Arguments:

    Shell - Supplies an optional pointer to the shell.

    Function - Supplies a pointer to the function to set.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShCopyFunctionList (
    PSHELL Source,
    PSHELL Destination
    );

/*++

Routine Description:

    This routine copies the list of declared functions from one shell to
    another.

Arguments:

    Source - Supplies a pointer to the shell containing the function
        definitions.

    Destination - Supplies a pointer to the shell where the function
        definitions should be copied to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShDestroyFunctionList (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine cleans up the list of functions on the given shell.

Arguments:

    Shell - Supplies a pointer to the dying shell.

Return Value:

    None.

--*/

BOOL
ShCreateArgumentList (
    PSTR *Arguments,
    ULONG ArgumentCount,
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine creates an argument list based on the command arguments.

Arguments:

    Arguments - Supplies a pointer to an array of strings representing the
        arguments. Even the first value in this array is supplied as an
        argument, so adjust this variable if passing parameters directly from
        the command line as the first parameter is usually the command name,
        not an argument.

    ArgumentCount - Supplies the number of arguments in the argument list.

    ListHead - Supplies a pointer to the initialized list head where the
        arguments should be placed.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShCopyArgumentList (
    PLIST_ENTRY SourceList,
    PLIST_ENTRY DestinationList
    );

/*++

Routine Description:

    This routine copies an existing argument list to a new one.

Arguments:

    SourceList - Supplies a pointer to the argument list to copy.

    DestinationList - Supplies a pointer where the copied entries will be
        added.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShDestroyArgumentList (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine destroys an argument list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of arguments.

Return Value:

    None.

--*/

INT
ShBuiltinSet (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin set command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were unset.

    Returns greater than zero if one or more variables could not be unset.

--*/

INT
ShBuiltinUnset (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin unset command for unsetting variables
    or functions.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments can be -v to unset a variable (the default), or -f
        to unset a function. This is followed by the variable or function name.

Return Value:

    Returns 0 if the variables were unset.

    Returns greater than zero if one or more variables could not be unset.

--*/

INT
ShBuiltinExport (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin export command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were exported.

    Returns greater than zero if one or more variables could not be exported.

--*/

INT
ShBuiltinReadOnly (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin readonly command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were made read-only.

    Returns greater than zero if one or more variables could not be made
    read-only.

--*/

INT
ShBuiltinLocal (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin local command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns 0 if the variables were made local.

    Returns greater than zero if one or more variables could not be made local.

--*/

//
// Arithmetic functions
//

BOOL
ShEvaluateArithmeticExpression (
    PSHELL Shell,
    PSTR String,
    UINTN Length,
    PSTR *Answer,
    PUINTN AnswerSize
    );

/*++

Routine Description:

    This routine evaluates an arithmetic expression. It assumes that all
    expansions have already taken place except for variable names without a
    dollar sign.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the input string.

    Length - Supplies the length of the input string in bytes.

    Answer - Supplies a pointer where the evaluation will be returned on
        success. The caller is responsible for freeing this memory.

    AnswerSize - Supplies a pointer where the size of the answer buffer will
        be returned including the null terminating byte on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

//
// Path related functions
//

BOOL
ShGetCurrentDirectory (
    PSTR *Directory,
    PUINTN DirectorySize
    );

/*++

Routine Description:

    This routine gets a listing of the files in the current directory.

Arguments:

    Directory - Supplies a pointer where the current directory path will be
        returned. The caller is responsible for freeing this memory.

    DirectorySize - Supplies a pointer where the size of the directory string
        including the null terminator will be returned on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShGetDirectoryListing (
    PSTR DirectoryPath,
    PSTR *FileNamesBuffer,
    PSHELL_DIRECTORY_ENTRY *Elements,
    PULONG ElementCount
    );

/*++

Routine Description:

    This routine gets a listing of the files in the current directory.

Arguments:

    DirectoryPath - Supplies a pointer to the string containing the directory
        to list.

    FileNamesBuffer - Supplies a pointer where a pointer to the files names
        will be returned on success. The elements array will contain pointers
        into the buffer. The caller is responsible for freeing this memory. A
        size is not returned because the caller is not expected to dereference
        into this memory directly.

    Elements - Supplies a pointer where the array of file names will be
        returned on success. The caller is responsible for freeing this buffer,
        which can be accomplished by freeing the first element.

    ElementCount - Supplies a pointer where the number of elements in the array
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShPerformPathExpansions (
    PSHELL Shell,
    PSTR *StringBuffer,
    PUINTN StringBufferSize,
    PSTR **FieldArray,
    PULONG FieldArrayCount
    );

/*++

Routine Description:

    This routine performs pathname expansion on the fields in the given field
    array.

Arguments:

    Shell - Supplies a pointer to the shell.

    StringBuffer - Supplies a pointer where the address of the fields string
        buffer is on input. On output, this may contain a different buffer that
        all the fields point into.

    StringBufferSize - Supplies a pointer that contains the size of the fields
        string buffer. This value will be updated to reflect the new size.

    FieldArray - Supplies a pointer to the array of string pointers of the
        fields on input. This array may get replaced if more elements need to
        be added for paths.

    FieldArrayCount - Supplies a pointer that contains the number of field
        elements on input. This value will be updated to reflect the number of
        fields after pathname expansion.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShLocateCommand (
    PSHELL Shell,
    PSTR Command,
    ULONG CommandSize,
    BOOL MustBeExecutable,
    PSTR *FullCommand,
    PULONG FullCommandSize,
    PINT ReturnValue
    );

/*++

Routine Description:

    This routine locates a command using the PATH environment variable.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command as seen from the command line.

    CommandSize - Supplies the size of the command string in bytes.

    MustBeExecutable - Supplies a boolean indicating if the given file must be
        executable or not.

    FullCommand - Supplies a pointer where a pointer to the full command string
        will be returned on success. If this is not the same pointer as the
        command string then the caller is responsible for freeing this buffer.

    FullCommandSize - Supplies a pointer where the size of the full command
        string will be returned.

    ReturnValue - Supplies a pointer where a premature return value will be
        returned. If this is not zero, then it contains the value that should
        be returned without trying to execute the command. On success, this
        variable will not be touched.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

INT
ShBuiltinPwd (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin pwd (print working directory) command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
ShBuiltinCd (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin cd (change directory) command.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// Alias support functions
//

BOOL
ShPerformAliasSubstitution (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine destroys all the aliases in a shell. It is usually called
    during cleanup.

Arguments:

    Shell - Supplies a pointer to the shell whose aliases will be cleaned up.

Return Value:

    TRUE on success (either looking it up and substituting or looking it up and
    finding nothing).

    FALSE on failure.

--*/

VOID
ShDestroyAliasList (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine destroys all the aliases in a shell. It is usually called
    during cleanup.

Arguments:

    Shell - Supplies a pointer to the shell whose aliases will be cleaned up.

Return Value:

    None.

--*/

INT
ShBuiltinAlias (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin alias statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments are in the form name=value, where name will get
        substituted for value when it is found as the first word in a command.

Return Value:

    0 on success.

    1 if an alias was not found.

--*/

INT
ShBuiltinUnalias (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the builtin unalias statement.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument. Arguments are in the form name=value, where name will get
        substituted for value when it is found as the first word in a command.

Return Value:

    0 on success.

    1 if an alias was not found.

--*/

PSHELL_ALIAS
ShLookupAlias (
    PSHELL Shell,
    PSTR Name,
    ULONG NameSize
    );

/*++

Routine Description:

    This routine looks up the given name and tries to find an alias for it.

Arguments:

    Shell - Supplies a pointer to the shell to search.

    Name - Supplies a pointer to the name to search for.

    NameSize - Supplies the size of the name buffer in bytes including the
        null terminator.

Return Value:

    Returns a pointer to the alias matching the given name on success.

    NULL if no alias could be found matching the given name.

--*/

BOOL
ShCopyAliases (
    PSHELL Source,
    PSHELL Destination
    );

/*++

Routine Description:

    This routine copies the list of declared aliases from one shell to
    another.

Arguments:

    Source - Supplies a pointer to the shell containing the aliases to copy.

    Destination - Supplies a pointer to the shell where the aliases should be
        copied to.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

//
// Signal/trap support functions
//

VOID
ShInitializeSignals (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine initializes the shell signals.

Arguments:

    Shell - Supplies a pointer to the shell.

Return Value:

    None.

--*/

VOID
ShCheckForSignals (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine checks for any pending signals and runs their associated
    actions if they're set.

Arguments:

    Shell - Supplies a pointer to the shell to run any pending signals on.

Return Value:

    None.

--*/

VOID
ShRunAtExitSignal (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine runs the "exit" trap handler on signal 0 if one was set.

Arguments:

    Shell - Supplies a pointer to the shell to run any pending signals on.

Return Value:

    None.

--*/

VOID
ShSetAllSignalDispositions (
    PSHELL Shell
    );

/*++

Routine Description:

    This routine sets all the signal dispositions in accordance with the
    given shell. This is usually called when entering or exiting a subshell.

Arguments:

    Shell - Supplies a pointer to the shell to operate in accordance with.

Return Value:

    None.

--*/

VOID
ShDestroySignalActionList (
    PLIST_ENTRY ActionList
    );

/*++

Routine Description:

    This routine destroys all the signal actions on the given signal action
    list.

Arguments:

    ActionList - Supplies a pointer to the head of the action list.

Return Value:

    None.

--*/

INT
ShBuiltinTrap (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the trap command, which handles signal catching
    within the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    1 on failure.

--*/

//
// Line input functions
//

BOOL
ShReadLine (
    PSHELL Shell,
    PSTR *ReturnedCommand,
    PULONG ReturnedCommandLength
    );

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

//
// Support for executing builtin functions
//

PSHELL_BUILTIN_COMMAND
ShIsBuiltinCommand (
    PSTR Command
    );

/*++

Routine Description:

    This routine determines if the given command name is a built in command,
    and returns a pointer to the command function if it is.

Arguments:

    Command - Supplies the null terminated string of the command.

Return Value:

    Returns a pointer to the command entry point function if the given string
    is a built-in command.

    NULL if the command is not a built-in command.

--*/

INT
ShRunBuiltinCommand (
    PSHELL Shell,
    PSHELL_BUILTIN_COMMAND Command,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine runs a shell builtin command.

Arguments:

    Shell - Supplies a pointer to the shell.

    Command - Supplies a pointer to the command function to run.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    0 on success.

    1 on failure.

--*/

INT
ShBuiltinEval (
    PSHELL Shell,
    INT ArgumentCount,
    PSTR *Arguments
    );

/*++

Routine Description:

    This routine implements the eval command, which collects all the parameters
    together separated by spaces and reexecutes them in the shell.

Arguments:

    Shell - Supplies a pointer to the shell.

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies the array of pointers to strings representing each
        argument.

Return Value:

    Returns the return value of the command it executes.

--*/

INT
ShRunScriptInContext (
    PSHELL Shell,
    PSTR FilePath,
    ULONG FilePathSize
    );

/*++

Routine Description:

    This routine executes the given script in the current context.

Arguments:

    Shell - Supplies a pointer to the shell being run in.

    FilePath - Supplies a pointer to the path of the script to run.

    FilePathSize - Supplies the size of the file path script in bytes,
        including the null terminator.

Return Value:

    Returns the return value from running (or failing to load) the script.

--*/

