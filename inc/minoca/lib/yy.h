/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    yy.h

Abstract:

    This header contains definitions for the basic Lexer/Parser library.

Author:

    Evan Green 9-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef YY_API

#define YY_API DLLIMPORT

#endif

//
// Define lexer flags.
//

#define YY_LEX_FLAG_IGNORE_UNKNOWN 0x00000001

//
// Define parser flags.
//

//
// Set this flag to debug print every node the parser is attempting to parser.
//

#define YY_PARSE_FLAG_DEBUG 0x00000001

//
// Set this flag to debug match successes.
//

#define YY_PARSE_FLAG_DEBUG_MATCHES 0x00000002

//
// Set this flag to debug match failures (produces a lot of output).
//

#define YY_PARSE_FLAG_DEBUG_NON_MATCHES 0x00000004

//
// Define parser grammar element flags.
//

//
// Set this flag to replace the given element with its child node if there is
// only one node and zero tokens.
//

#define YY_GRAMMAR_COLLAPSE_ONE 0x00000001

//
// Set this flag to indicate that additional matches should not be added on
// the end of a left recursive rule list-style, but should instead be nested
// nodes.
//

#define YY_GRAMMAR_NEST_LEFT_RECURSION 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Lexer definitions
//

/*++

Structure Description:

    This structure stores the state for the lexer. To initialize this, zero it
    out.

Members:

    Flags - Stores a bitfield of flags governing the lexer behavior. See
        YY_LEX_FLAG_* definitions.

    Allocate - Stores a pointer to a function used to allocate memory.

    Free - Stores a pointer to a function used to free allocated memory.

    Input - Stores the input buffer to lex.

    InputSize - Stores the size of the input buffer in bytes, including the
        null terminator if present.

    Position - Stores the current character position.

    Line - Stores the current one-based line number.

    Column - Stores the zero-based column number.

    TokenCount - Stores the number of tokens processed so far.

    LargestToken - Stores the size member of the largest single token seen so
        far. Note that this does not include space for a null terminator.

    TokenStringsSize - Stores the total number of bytes to allocate for strings
        for all tokens seen so far, including a null terminator on each one.

    Literals - Stores a pointer to a null-terminated string containing
        characters to pass through literally as individual tokens.

    Expressions - Stores an array of expression strings to match against. This
        must be terminated by a NULL entry.

    IgnoreExpressions - Stores an array of expression strings that should not
        produce tokens if they match. This is a place to put things like
        comment expressions.

    ExpressionNames - Stores an optional pointer to an array of strings that
        names each of the expressions. Useful for debugging, but not mandatory.

    TokenBase - Stores the value to assign for the first expression. 512 is
        usually a good value, as it won't alias with the literal characters.

--*/

typedef struct _LEXER {
    ULONG Flags;
    PSTR Input;
    ULONG InputSize;
    ULONG Position;
    ULONG Line;
    ULONG Column;
    ULONG TokenCount;
    ULONG LargestToken;
    ULONG TokenStringsSize;
    PSTR Literals;
    PSTR *Expressions;
    PSTR *IgnoreExpressions;
    PSTR *ExpressionNames;
    ULONG TokenBase;
} LEXER, *PLEXER;

/*++

Structure Description:

    This structure stores a lexer token.

Members:

    Value - Stores the lexer token value. This may be a literal byte or a
        token value.

    Position - Stores the position of the token.

    Size - Stores the number of characters in the token.

    Line - Stores the line number of start of the token.

    Column - Stores the column number of the start of the token.

    String - Stores a pointer to the string of input text this token
        corresponds to. The lexer does not fill this out, but the member is
        provided here for convenience.

--*/

typedef struct _LEXER_TOKEN {
    ULONG Value;
    ULONG Position;
    ULONG Size;
    ULONG Line;
    ULONG Column;
    PSTR String;
} LEXER_TOKEN, *PLEXER_TOKEN;

//
// Parser definitions
//

typedef struct _PARSER_NODE PARSER_NODE, *PPARSER_NODE;

typedef
PVOID
(*PYY_ALLOCATE) (
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the lex/parse library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
VOID
(*PYY_FREE) (
    PVOID Memory
    );

/*++

Routine Description:

    This routine is called when the lex/parse library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PYY_GET_TOKEN) (
    PVOID Context,
    PLEXER_TOKEN Token
    );

/*++

Routine Description:

    This routine gets the next token for the parser.

Arguments:

    Context - Supplies a context pointer initialized in the parser.

    Token - Supplies a pointer where the next token will be filled out and
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if the file end was reached.

    STATUS_MALFORMED_DATA_STREAM if the given input matched no rule in the
    lexer and the lexer was not configured to ignore such things.

--*/

typedef
VOID
(*PYY_NODE_CALLBACK) (
    PVOID Context,
    PPARSER_NODE Node,
    BOOL Create
    );

/*++

Routine Description:

    This routine is called when a node is being created or destroyed. This
    callback must be prepared to create and destroy a node multiple times, as
    recursive descent parsers explore paths that ultimately prove to be
    incorrect. Unless the parser feeds back into the lexer (like in C), it is
    not recommended to use this callback.

Arguments:

    Context - Supplies a context pointer initialized in the parser.

    Node - Supplies a pointer to the node being created or destroyed.

    Create - Supplies a boolean indicating if the node is being created (TRUE)
        or destroyed (FALSE).

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a grammar element in the parser grammar.

Members:

    Name - Stores an optional pointer to the name of this grammar element. This
        is not used during parsing, but can be useful during debugging.

    Flags - Stores a bitfield of flags about this node. See YY_GRAMMAR_*
        definitions.

    Components - Stores a sequence of rule elements. Each element is either a
        token value or a rule value, determined by the grammar base and grammar
        end values in the parser. Each form of a grammar expression is
        terminated by a zero value. The next alternate form starts after the
        zero. Terminate the sequence with an additional zero to end the
        node form.

--*/

typedef struct _PARSER_GRAMMAR_ELEMENT {
    PSTR Name;
    ULONG Flags;
    PULONG Components;
} PARSER_GRAMMAR_ELEMENT, *PPARSER_GRAMMAR_ELEMENT;

/*++

Structure Description:

    This structure stores a parsed node.

Members:

    GrammarElement - Stores the type of grammar element this node represents.

    GrammarIndex - Stores the index of the rule that applied for this grammar
        node.

    StartToken - Stores a pointer to the token where parsing of this node began.

    Tokens - Stores a pointer to the tokens in the node.

    Nodes - Stores a pointer to the child nodes in the node. In the free list,
        the first element stores the pointer to the next element in the free
        list.

    TokenCount - Stores the number of valid tokens in the token array.

    NodeCount - Stores the number of vaild nodes in the node array.

    TokenCapacity - Stores the maximum number of tokens the array can store
        before it must be reallocated.

    NodeCapacity - Stores the maximum number of nodes the array can store
        before it must be reallocated.

--*/

struct _PARSER_NODE {
    ULONG GrammarElement;
    ULONG GrammarIndex;
    PLEXER_TOKEN StartToken;
    PLEXER_TOKEN *Tokens;
    PPARSER_NODE *Nodes;
    ULONG TokenCount;
    ULONG NodeCount;
    ULONG TokenCapacity;
    ULONG NodeCapacity;
};

/*++

Structure Description:

    This structure stores the state for the parser. To initialize this, zero it
    out and call the initialize function.

Members:

    Flags - Stores a bitfield of flags governing the lexer behavior. See
        YY_LEX_FLAG_* definitions.

    Context - Stores a context pointer that is passed to the get token function.

    Allocate - Stores a pointer to a function used allocate memory.

    Free - Stores a pointer to a function used to free memory.

    GetToken - Stores a pointer to a function used to get the next lexical
        token.

    NodeCallback - Stores an optional pointer to a function called when nodes
        are created or destroyed. Note that this callback needs to be prepared
        to create and destroy nodes potentially multiple times, as recursive
        descent parsers explore paths that may ultimately not be correct. Use
        of this callback is not recommended unless required (for languages like
        C where the parser feeds back into the lexer).

    Grammar - Stores a pointer to the grammar, which is defined as an array
        of grammar elements.

    GrammarBase - Stores the start of the range of component values that
        specify grammar elements themselves.

    GrammarEnd - Stores the end index of grammar elements, exclusive.
        Every rule component outside the range of grammar base to grammar size
        is assumed to be a lexer token.

    GrammarStart - Stores the starting element to parse.

    MaxRecursion - Stores the maximum allowed recursion depth. Supply 0 to
        allow infinite recursion.

    Lexer - Stores an optional pointer to the lexer, which can be used to print
        token names during debug.

    TokenArrays - Stores a pointer to an array of ever-doubling arrays of
        lexer tokens.

    TokenCount - Stores the total number of tokens stored in the token arrays.

    TokenCapacity - Stores the total number of tokens that can fit in the
        arrays before they will need to be resized.

    NextTokenIndex - Stores the next token index to process.

    NextToken - Stores a pointer to the next token, for fast access.

    FreeNodes - Stores a pointer to the singly linked list of free nodes.

    RecursionDepth - Stores the current recursion depth.

--*/

typedef struct _PARSER {
    ULONG Flags;
    PVOID Context;
    PYY_ALLOCATE Allocate;
    PYY_FREE Free;
    PYY_GET_TOKEN GetToken;
    PYY_NODE_CALLBACK NodeCallback;
    PARSER_GRAMMAR_ELEMENT *Grammar;
    ULONG GrammarBase;
    ULONG GrammarEnd;
    ULONG GrammarStart;
    ULONG MaxRecursion;
    PLEXER Lexer;
    PLEXER_TOKEN *TokenArrays;
    ULONG TokenCount;
    ULONG TokenCapacity;
    ULONG NextTokenIndex;
    PLEXER_TOKEN NextToken;
    PPARSER_NODE FreeNodes;
    ULONG RecursionDepth;
} PARSER, *PPARSER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Lexer functions
//

YY_API
KSTATUS
YyLexInitialize (
    PLEXER Lexer
    );

/*++

Routine Description:

    This routine initializes a lexer.

Arguments:

    Lexer - Supplies a pointer to the lexer to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the required fields of the lexer are not filled
    in.

--*/

YY_API
KSTATUS
YyLexGetToken (
    PLEXER Lexer,
    PLEXER_TOKEN Token
    );

/*++

Routine Description:

    This routine gets the next token from the lexer.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Token - Supplies a pointer where the next token will be filled out and
        returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_END_OF_FILE if the file end was reached.

    STATUS_MALFORMED_DATA_STREAM if the given input matched no rule in the
    lexer and the lexer was not configured to ignore such things.

--*/

//
// Parser functions
//

YY_API
KSTATUS
YyParserInitialize (
    PPARSER Parser
    );

/*++

Routine Description:

    This routine initializes a parser. This routine assumes the parser was
    initially zeroed out, and is currently being initialized or reset after use.

Arguments:

    Parser - Supplies a pointer to the parser to initialize.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the required fields of the lexer are not filled
    in.

--*/

YY_API
VOID
YyParserReset (
    PPARSER Parser
    );

/*++

Routine Description:

    This routine resets a parser, causing it to return to its initial state but
    not forget the tokens it has seen.

Arguments:

    Parser - Supplies a pointer to the parser to reset.

Return Value:

    None.

--*/

YY_API
VOID
YyParserDestroy (
    PPARSER Parser
    );

/*++

Routine Description:

    This routine frees all the resources associated with a given parser.

Arguments:

    Parser - Supplies a pointer to the parser.

Return Value:

    None.

--*/

YY_API
KSTATUS
YyParse (
    PPARSER Parser,
    PPARSER_NODE *Tree
    );

/*++

Routine Description:

    This routine attempts to parse input grammatically based on a set of
    grammar rules and lexer input tokens. It will build an abstract syntax
    tree, which will be returned as a node.

Arguments:

    Parser - Supplies a pointer to the parser. The caller must have filled in
        the grammar rules and support function pointers.

    Tree - Supplies a pointer where the abstract syntax tree will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_INVALID_SEQUENCE upon parse failure.

    Other errors on other failures (such as lexer errors).

--*/

YY_API
VOID
YyDestroyNode (
    PPARSER Parser,
    PPARSER_NODE Node
    );

/*++

Routine Description:

    This routine destroys a parser node.

Arguments:

    Parser - Supplies a pointer to the parser.

    Node - Supplies a pointer to the node to destroy.

Return Value:

    None.

--*/

