/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    shparse.h

Abstract:

    This header contains definitions for the parser portion of the shell.

Author:

    Evan Green 5-Jun-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns non-zero if the given character is a valid first
// character of a name.
//

#define SHELL_NAME_FIRST_CHARACTER(_Character) \
     ((((_Character) >= 'a') && ((_Character) <= 'z')) || \
      (((_Character) >= 'A') && ((_Character) <= 'Z')) || \
      ((_Character) == '_'))

//
// This macro returns non-zero if the given character is a valid character in a
// name (at any position except the first, it may not be valid in the first
// position).
//

#define SHELL_NAME_CHARACTER(_Character) \
     ((((_Character) >= 'a') && ((_Character) <= 'z')) || \
      (((_Character) >= 'A') && ((_Character) <= 'Z')) || \
      (((_Character) >= '0') && ((_Character) <= '9')) || \
      ((_Character) == '_'))

//
// This macro returns non-zero if the given character is a special parameter.
//

#define SHELL_SPECIAL_PARAMETER_CHARACTER(_Character)     \
    ((((_Character) >= '0') && ((_Character) <= '9')) ||  \
     ((_Character) == '@') || ((_Character) == '*') ||    \
     ((_Character) == '#') || ((_Character) == '?') ||    \
     ((_Character) == '-') || ((_Character) == '$') ||    \
     ((_Character) == '!'))

//
// This macro determines if a token is a word or a reserved word. The parameter
// is a shell token type.
//

#define SHELL_TOKEN_WORD_LIKE(_Token) \
    (((_Token) == TOKEN_WORD) || ((_Token) == TOKEN_ASSIGNMENT_WORD) || \
     ((_Token) == TOKEN_NAME) || ((_Token) == TOKEN_IF) ||              \
     ((_Token) == TOKEN_THEN) || ((_Token) == TOKEN_ELSE) ||            \
     ((_Token) == TOKEN_ELIF) || ((_Token) == TOKEN_FI) ||              \
     ((_Token) == TOKEN_DO) || ((_Token) == TOKEN_DONE) ||              \
     ((_Token) == TOKEN_CASE) || ((_Token) == TOKEN_ESAC) ||            \
     ((_Token) == TOKEN_WHILE) || ((_Token) == TOKEN_UNTIL) ||          \
     ((_Token) == TOKEN_FOR) || ((_Token) == TOKEN_IN) ||               \
     ((_Token) == '{') || ((_Token) == '}') || ((_Token) == '!'))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the shell's lexical tokens.
//

#define TOKEN_END_OF_FILE            0
#define TOKEN_WORD                   500
#define TOKEN_ASSIGNMENT_WORD        501
#define TOKEN_NAME                   502
#define TOKEN_IO_NUMBER              503
#define TOKEN_DOUBLE_AND             504
#define TOKEN_DOUBLE_OR              505
#define TOKEN_DOUBLE_SEMICOLON       506
#define TOKEN_DOUBLE_LESS_THAN       507
#define TOKEN_DOUBLE_GREATER_THAN    508
#define TOKEN_LESS_THAN_AND          509
#define TOKEN_GREATER_THAN_AND       510
#define TOKEN_LESS_THAN_GREATER_THAN 511
#define TOKEN_DOUBLE_LESS_THAN_DASH  512
#define TOKEN_CLOBBER                513
#define TOKEN_IF                     514
#define TOKEN_THEN                   515
#define TOKEN_ELSE                   516
#define TOKEN_ELIF                   517
#define TOKEN_FI                     518
#define TOKEN_DO                     519
#define TOKEN_DONE                   520
#define TOKEN_CASE                   521
#define TOKEN_ESAC                   522
#define TOKEN_WHILE                  523
#define TOKEN_UNTIL                  524
#define TOKEN_FOR                    525
#define TOKEN_IN                     526

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SHELL_NODE_TYPE {
    ShellNodeInvalid,
    ShellNodeList,
    ShellNodeAndOr,
    ShellNodePipeline,
    ShellNodeSimpleCommand,
    ShellNodeFunction,
    ShellNodeIf,
    ShellNodeTerm,
    ShellNodeFor,
    ShellNodeBraceGroup,
    ShellNodeCase,
    ShellNodeWhile,
    ShellNodeUntil,
    ShellNodeSubshell,
} SHELL_NODE_TYPE, *PSHELL_NODE_TYPE;

typedef enum _SHELL_IO_REDIRECTION_TYPE {
    ShellRedirectInvalid,
    ShellRedirectRead,
    ShellRedirectReadFromDescriptor,
    ShellRedirectWrite,
    ShellRedirectWriteToDescriptor,
    ShellRedirectClobber,
    ShellRedirectAppend,
    ShellRedirectReadWrite,
    ShellRedirectHereDocument,
    ShellRedirectStrippedHereDocument,
} SHELL_IO_REDIRECTION_TYPE, *PSHELL_IO_REDIRECTION_TYPE;

/*++

Structure Description:

    This structure defines a here-document.

Members:

    ListEntry - Stores pointers to the next and previous here documents in
        whichever list this is on.

    StripLeadingTabs - Stores a boolean indicating whether or not to strip
        leading tabs from the text.

    EndWord - Stores the delimiting word of the here document.

    EndWordSize - Stores the size of the end word in bytes including the null
        terminator.

    EndWordWasQuoted - Stores a boolean indicating if the end word was quoted
        or not. If the end word was not quoted, the lexer recognizes and
        discards backslash newlines, otherwise it doesn't.

    Document - Stores the contents of the here document.

    DocumentSize - Stores the size of the here document in bytes, including the
        null terminator.

--*/

typedef struct _SHELL_HERE_DOCUMENT {
    LIST_ENTRY ListEntry;
    BOOL StripLeadingTabs;
    PSTR EndWord;
    UINTN EndWordSize;
    BOOL EndWordWasQuoted;
    PSTR Document;
    UINTN DocumentSize;
} SHELL_HERE_DOCUMENT, *PSHELL_HERE_DOCUMENT;

/*++

Structure Description:

    This structure defines the contents of a pipeline.

Members:

    Bang - Stores a boolean indicating whether or not an exclamation point
        preceded the pipeline.

--*/

typedef struct _SHELL_NODE_PIPELINE {
    BOOL Bang;
} SHELL_NODE_PIPELINE, *PSHELL_NODE_PIPELINE;

/*++

Structure Description:

    This structure defines the contents of a simple command.

Members:

    AssignmentList - Stores the head of the list of assignments for this
        command.

    Arguments - Stores a pointer to the string containing the arguments to the
        command, including a null terminator.

    ArgumentsBufferCapacity - Stores the size of the arguments buffer
        allocation.

--*/

typedef struct _SHELL_NODE_SIMPLE_COMMAND {
    LIST_ENTRY AssignmentList;
    PSTR Arguments;
    UINTN ArgumentsSize;
    UINTN ArgumentsBufferCapacity;
} SHELL_NODE_SIMPLE_COMMAND, *PSHELL_NODE_SIMPLE_COMMAND;

/*++

Structure Description:

    This structure defines the contents of a function definition.

Members:

    Name - Stores a pointer to the function name string.

    NameSize - Stores the size of the function name string in bytes including
        the null terminator.

--*/

typedef struct _SHELL_NODE_FUNCTION {
    PSTR Name;
    UINTN NameSize;
} SHELL_NODE_FUNCTION, *PSHELL_NODE_FUNCTION;

/*++

Structure Description:

    This structure defines the contents of a for loop.

Members:

    Name - Stores the name of the iteration variable.

    NameSize - Stores the size of the name buffer in bytes including the
        null terminator.

    WordListBuffer - Stores a pointer to the buffer of words to iterate over.

    WordListBufferSize - Stores the size of the word list buffer.

    WordListBufferCapacity - Stores the size of the allocation covering the
        word list buffer.

--*/

typedef struct _SHELL_NODE_FOR {
    PSTR Name;
    UINTN NameSize;
    PSTR WordListBuffer;
    UINTN WordListBufferSize;
    UINTN WordListBufferCapacity;
} SHELL_NODE_FOR, *PSHELL_NODE_FOR;

/*++

Structure Description:

    This structure defines a set of patterns in a case statement.

Members:

    ListEntry - Stores pointers to the next and previous pattern sets in the
        list.

    PatternEntryList - Stores the head of the list of patterns in the set.

    Action - Stores the compound list of commands to execute should one of these
        patterns match.

--*/

typedef struct _SHELL_CASE_PATTERN_SET {
    LIST_ENTRY ListEntry;
    LIST_ENTRY PatternEntryList;
    PSHELL_NODE Action;
} SHELL_CASE_PATTERN_SET, *PSHELL_CASE_PATTERN_SET;

/*++

Structure Description:

    This structure defines one pattern.

Members:

    ListEntry - Stores pointers to the next and previous patterns in the list.

    Pattern - Stores a pointer to the pattern string to check.

    PatternSize - Stores the size of the pattern string in bytes including the
        null terminator.

--*/

typedef struct _SHELL_CASE_PATTERN_ENTRY {
    LIST_ENTRY ListEntry;
    PSTR Pattern;
    UINTN PatternSize;
} SHELL_CASE_PATTERN_ENTRY, *PSHELL_CASE_PATTERN_ENTRY;

/*++

Structure Description:

    This structure defines the contents of a case statement.

Members:

    Name - Stores the name of the variable to be interrogated.

    NameSize - Stores the size of the input name variable in bytes including
        the null terminator.

    PatternList - Stores the head of the list of patterns to check.

--*/

typedef struct _SHELL_NODE_CASE {
    PSTR Name;
    UINTN NameSize;
    LIST_ENTRY PatternList;
} SHELL_NODE_CASE, *PSHELL_NODE_CASE;

/*++

Structure Description:

    This structure stores information about an I/O redirect.

Members:

    ListEntry - Stores pointers to the next and previous redirections in the
        list.

    Type - Stores the type of redirection.

    FileNumber - Stores the file number of the redirection.

    FileName - Stores the file name of the redirection.

    FileNameSize - Stores the length of the file name in bytes including the
        null terminator.

    HereDocument - Stores the here document in the case that the redirection is
        from a here document.

--*/

typedef struct _SHELL_IO_REDIRECT {
    LIST_ENTRY ListEntry;
    SHELL_IO_REDIRECTION_TYPE Type;
    INT FileNumber;
    PSTR FileName;
    UINTN FileNameSize;
    PSHELL_HERE_DOCUMENT HereDocument;
} SHELL_IO_REDIRECT, *PSHELL_IO_REDIRECT;

/*++

Structure Description:

    This structure stores information about a shell assignment (something at
    the beginning of the command in the form name=value).

Members:

    ListEntry - Stores pointers to the next and previous assignments in the
        list.

    Name - Stores the assignment variable name.

    NameSize - Stores the size of the name string in bytes including the null
        terminator.

    Value - Stores the assignment variable value.

    ValueSize - Stores the size of the value string in bytes including the null
        terminator.

--*/

typedef struct _SHELL_ASSIGNMENT {
    LIST_ENTRY ListEntry;
    PSTR Name;
    UINTN NameSize;
    PSTR Value;
    UINTN ValueSize;
} SHELL_ASSIGNMENT, *PSHELL_ASSIGNMENT;

/*++

Structure Description:

    This structure contains a shell language node.

Members:

    Type - Stores the type of node.

    ReferenceCount - Stores the reference count of the node.

    LineNumber - Stores the line number this node starts on.

    ExecutionStackEntry - Stores pointers to the older and newer nodes in the
        current execution stack.

    SiblingListEntry - Stores pointers to the next and previous nodes that are
        also children of the parent node.

    Children - Stores the list head of children of this node.

    RedirectList - Stores the head of the list of I/O redirections.

    RunInBackground - Stores a boolean indicating if this command should be
        run in the background.

    AndOr - Stores a valid token if this pipeline is part of an and-or list, or
        0 if this pipeline lives on its own.

    U - Stores the union of node information.

--*/

struct _SHELL_NODE {
    SHELL_NODE_TYPE Type;
    LONG ReferenceCount;
    ULONG LineNumber;
    LIST_ENTRY ExecutionStackEntry;
    LIST_ENTRY SiblingListEntry;
    LIST_ENTRY Children;
    LIST_ENTRY RedirectList;
    BOOL RunInBackground;
    ULONG AndOr;
    union {
        SHELL_NODE_PIPELINE Pipeline;
        SHELL_NODE_SIMPLE_COMMAND SimpleCommand;
        SHELL_NODE_FUNCTION Function;
        SHELL_NODE_FOR For;
        SHELL_NODE_CASE Case;
    } U;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
ShParse (
    PSHELL Shell,
    PSHELL_NODE *Node
    );

/*++

Routine Description:

    This routine attempts to parse a complete command out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell object.

    Node - Supplies a pointer where the parsed node representing the command
        will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

BOOL
ShInitializeLexer (
    PSHELL_LEXER_STATE Lexer,
    FILE *InputFile,
    PSTR InputBuffer,
    UINTN InputBufferSize
    );

/*++

Routine Description:

    This routine initializes the shell lexer state.

Arguments:

    Lexer - Supplies a pointer to the lexer state.

    InputFile - Supplies an optional pointer to the input file.

    InputBuffer - Supplies an optional pointer to the input buffer to use. If
        no buffer is provided one will be created, otherwise the provided one
        will be copied.

    InputBufferSize - Supplies the size of the provided input buffer in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

VOID
ShDestroyLexer (
    PSHELL_LEXER_STATE Lexer
    );

/*++

Routine Description:

    This routine tears down the shell lexer state.

Arguments:

    Lexer - Supplies a pointer to the lexer state.

Return Value:

    None.

--*/

BOOL
ShGetToken (
    PSHELL Shell,
    BOOL FirstCommandToken
    );

/*++

Routine Description:

    This routine fetches the next token out of the shell input.

Arguments:

    Shell - Supplies a pointer to the shell to read from.

    FirstCommandToken - Supplies a boolean indicating if this token could be
        the first word in a command, in which case alias substitution will be
        enabled.

Return Value:

    TRUE on success. The next token will be written into the shell structure.

    FALSE on failure.

--*/

BOOL
ShScanPastExpansion (
    PSTR String,
    UINTN StringSize,
    PUINTN ExpansionSize
    );

/*++

Routine Description:

    This routine is called to find the end of an expansion.

Arguments:

    String - Supplies a pointer to the string at an expansion to scan.

    StringSize - Supplies the number of bytes in the string.

    ExpansionSize - Supplies a pointer where the size of the expansion in
        bytes will be returned.

Return Value:

    TRUE on success. The extent of the expansion will be added to the token
        buffer.

    FALSE on failure.

--*/

VOID
ShDestroyHereDocument (
    PSHELL_HERE_DOCUMENT HereDocument
    );

/*++

Routine Description:

    This routine destroys a here document. It assumes the here document is
    already removed from any list it was on.

Arguments:

    HereDocument - Supplies a pointer to the here document to destroy.

Return Value:

    None.

--*/

VOID
ShRetainNode (
    PSHELL_NODE Node
    );

/*++

Routine Description:

    This routine increases the reference count on a node.

Arguments:

    Node - Supplies a pointer to the node to retain.

Return Value:

    None.

--*/

VOID
ShReleaseNode (
    PSHELL_NODE Node
    );

/*++

Routine Description:

    This routine releases a reference on a shell node. If this causes the
    reference count to drop to zero then the node is destroyed.

Arguments:

    Node - Supplies a pointer to the node to release.

Return Value:

    None.

--*/

BOOL
ShIsName (
    PSTR String,
    UINTN StringSize
    );

/*++

Routine Description:

    This routine determines if the given string is a valid NAME according to
    the shell grammar.

Arguments:

    String - Supplies a pointer to the candidate string.

    StringSize - Supplies the number of characters to check.

Return Value:

    TRUE if the string is a valid name.

    FALSE if the string is not a valid name.

--*/

