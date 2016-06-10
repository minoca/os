/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    lex.c

Abstract:

    This module implements support for the Chalk lexer.

Author:

    Evan Green 29-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include "compiler.h"
#include "lang.h"
#include "compsup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define YY_DIGITS "[0-9]"
#define YY_OCTAL_DIGITS "[0-7]"
#define YY_NAME0 "[a-zA-Z_]"
#define YY_HEX "[a-fA-F0-9]"

//
// Token 0 is reserved for EOF, and token 1 is reserved for Error, so token 2
// is the first one defined by the lexer.
//

#define YY_TOKEN_OFFSET 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PSTR CkLexerExpressions[] = {
    "break",
    "continue",
    "do",
    "else",
    "for",
    "if",
    "return",
    "while",
    "function",
    "in",
    "null",
    "true",
    "false",
    "var",
    "class",
    "is",
    "static",
    "super",
    "this",
    "import",
    "from",
    YY_NAME0 "(" YY_NAME0 "|" YY_DIGITS ")*",
    YY_DIGITS "+",
    "\"(\\\\.|[^\\\"])*\"",
    ">>=",
    "<<=",
    "\\+=",
    "-=",
    "\\*=",
    "/=",
    "%=",
    "&=",
    "^=",
    "\\|=",
    "?=",
    ">>",
    "<<",
    "\\+\\+",
    "--",
    "&&",
    "\\|\\|",
    "<=",
    ">=",
    "==",
    "!=",
    ";",
    "\\{",
    "}",
    ",",
    ":",
    "=",
    "\\(",
    "\\)",
    "\\[",
    "]",
    "&",
    "!",
    "~",
    "-",
    "\\+",
    "*",
    "/",
    "%",
    "<",
    ">",
    "^",
    "\\|",
    "\\?",
    "\\.",
    "\\.\\.",
    "\\.\\.\\.",
    NULL,
};

PSTR CkLexerTokenNames[] = {
    "break",
    "continue",
    "do",
    "else",
    "for",
    "if",
    "return",
    "while",
    "function",
    "in",
    "null",
    "true",
    "false",
    "var",
    "class",
    "is",
    "static",
    "super",
    "this",
    "import",
    "from",
    "ID",
    "CONSTANT",
    "STRING",
    ">>=",
    "<<=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "^=",
    "|=",
    "?=",
    ">>",
    "<<",
    "++",
    "--",
    "&&",
    "||",
    "<=",
    ">=",
    "==",
    "!=",
    ";",
    "{",
    "}",
    ",",
    ":",
    "=",
    "(",
    ")",
    "[",
    "]",
    "&",
    "!",
    "~",
    "-",
    "+",
    "*",
    "/",
    "%",
    "<",
    ">",
    "^",
    "|",
    "?",
    ".",
    "..",
    "...",
    NULL
};

PSTR CkLexerIgnoreExpressions[] = {
    "[ \t\v\r\n\f]",
//    "//[^\n]*",
//    "/\*.*?\*/",
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpInitializeLexer (
    PLEXER Lexer,
    PSTR Source,
    UINTN Length
    )

/*++

Routine Description:

    This routine initializes the Chalk lexer.

Arguments:

    Lexer - Supplies a pointer to the lexer to initialize.

    Source - Supplies a pointer to the null terminated source string to lex.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    CkZero(Lexer, sizeof(LEXER));
    Lexer->Input = Source;
    Lexer->InputSize = Length;
    Lexer->Expressions = CkLexerExpressions;
    Lexer->IgnoreExpressions = CkLexerIgnoreExpressions;
    Lexer->ExpressionNames = CkLexerTokenNames;
    Lexer->TokenBase = YY_TOKEN_OFFSET;
    Status = YyLexInitialize(Lexer);

    CK_ASSERT(KSUCCESS(Status));

    return;
}

INT
CkpLexerGetToken (
    PVOID Lexer,
    PYY_VALUE Value
    )

/*++

Routine Description:

    This routine is called to get a new token from the input.

Arguments:

    Lexer - Supplies a pointer to the lexer context.

    Value - Supplies a pointer where the new token will be returned. The token
        data may be larger than simply a value, but it returns at least a
        value.

Return Value:

    0 on success, including EOF.

    Returns a non-zero value if there was an error reading the token.

--*/

{

    KSTATUS KStatus;
    PCK_PARSER Parser;
    PLEXER_TOKEN Token;

    Parser = PARENT_STRUCTURE(Lexer, CK_PARSER, Lexer);
    Token = (PLEXER_TOKEN)Value;
    KStatus = YyLexGetToken(Lexer, Token);
    if (KStatus == STATUS_END_OF_FILE) {
        Parser->TokenPosition = Parser->SourceLength;
        Parser->TokenSize = 0;
        *Value = CkTokenEndOfFile;
        return 0;

    } else if (!KSUCCESS(KStatus)) {
        return YyStatusLexError;
    }

    Parser->PreviousPosition = Parser->TokenPosition;
    Parser->PreviousSize = Parser->TokenSize;
    Parser->PreviousLine = Parser->Line;
    Parser->TokenPosition = Token->Position;
    Parser->TokenSize = Token->Size;
    Parser->Line = Token->Line;

    assert(*Value >= YY_TOKEN_OFFSET);

    return YyStatusSuccess;
}

//
// --------------------------------------------------------- Internal Functions
//

