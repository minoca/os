/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    testpar.c

Abstract:

    This module implements a test Chalk parser to verify the yacc port.

Author:

    Evan Green 20-May-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#define YY_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include "../../lang.h"
#include <minoca/lib/yy.h>
#include <stdio.h>
#include <minoca/lib/yygen.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define MIN(_Value1, _Value2) (((_Value1) < (_Value2)) ? (_Value1) : (_Value2))

#define YY_DIGITS "[0-9]"
#define YY_OCTAL_DIGITS "[0-7]"
#define YY_NAME0 "[a-zA-Z_]"
#define YY_HEX "[a-fA-F0-9]"

#define YY_TOKEN_OFFSET 257

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
CkgpGetToken (
    PVOID Lexer,
    PYY_VALUE Value
    );

INT
CkgpProcessSymbol (
    PVOID Context,
    YY_VALUE Symbol,
    PVOID Elements,
    INT ElementCount,
    PVOID ReducedElement
    );

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
    YY_NAME0 "(" YY_NAME0 "|" YY_DIGITS ")*", // identifier
    YY_DIGITS "+", // decimal integer
    "L?\"(\\\\.|[^\\\"])*\"", // string literal
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
    "ID", // identifier
    "DECINT", // decimal integer
    "STRING", // string literal
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
    NULL
};

extern YY_GRAMMAR CkGrammar;

YY_PARSER CkParser = {
    &CkGrammar,
    realloc,
    CkgpProcessSymbol,
    NULL,
    NULL,
    NULL,
    CkgpGetToken,
    sizeof(LEXER_TOKEN),
    0,
    "ck"
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements a test parser program for the Chalk grammar.

Arguments:

    ArgumentCount - Supplies the number of command line arguments.

    Arguments - Supplies the array of command line arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINTN AllocationSize;
    PSTR Buffer;
    ULONG Column;
    FILE *File;
    KSTATUS KStatus;
    LEXER Lexer;
    ULONG Line;
    ULONG Size;
    INT Status;

    if (ArgumentCount < 2) {
        printf("Usage: %s <file>\n", Arguments[0]);
        return 1;
    }

    if (ArgumentCount == 2) {
        File = fopen(Arguments[1], "r");
        if (File == NULL) {
            fprintf(stderr,
                    "Error: Failed to open %s: %s\n",
                    Arguments[1],
                    strerror(errno));

            return 1;
        }

    } else {
        File = stdin;
    }

    AllocationSize = 4096;
    Buffer = malloc(AllocationSize);
    if (Buffer == NULL) {
        return 2;
    }

    Size = 0;
    while (TRUE) {
        if (Size + 1 >= AllocationSize) {
            AllocationSize *= 2;
            Buffer = realloc(Buffer, AllocationSize);
            if (Buffer == NULL) {
                return 2;
            }
        }

        Status = fread(Buffer + Size, 1, AllocationSize - Size, File);
        if (Status <= 0) {
            break;
        }

        Size += Status;
    }

    if (Status < 0) {
        perror("Read failure");
        return 1;
    }

    Buffer[Size] = '\0';
    memset(&Lexer, 0, sizeof(Lexer));
    Lexer.Input = Buffer;
    Lexer.InputSize = Size;
    Lexer.Expressions = CkLexerExpressions;
    Lexer.IgnoreExpressions = CkLexerIgnoreExpressions;
    Lexer.ExpressionNames = CkLexerTokenNames;
    Lexer.TokenBase = YY_TOKEN_OFFSET;
    YyLexInitialize(&Lexer);
    CkParser.Lexer = &Lexer;
    Status = YyParseGrammar(&CkParser);

ParseScriptEnd:
    printf("Final Status: %d\n", Status);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
CkgpGetToken (
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

    CHAR Buffer[64];
    KSTATUS KStatus;
    ULONG Size;
    PLEXER_TOKEN Token;

    Token = (PLEXER_TOKEN)Value;
    KStatus = YyLexGetToken(Lexer, Token);
    if (KStatus == STATUS_END_OF_FILE) {
        printf("EOF");
        *Value = 0;
        return 0;

    } else if (!KSUCCESS(KStatus)) {
        printf("LexError %x\n", KStatus);
        return EINVAL;
    }

    assert(*Value >= YY_TOKEN_OFFSET);

    Size = MIN(Token->Size, sizeof(Buffer) - 1);
    strncpy(Buffer,
            ((PLEXER)Lexer)->Input + Token->Position,
            Size);

    Buffer[Size] = '\0';
    printf("%s", Buffer);
    return 0;
}

INT
CkgpProcessSymbol (
    PVOID Context,
    YY_VALUE Symbol,
    PVOID Elements,
    INT ElementCount,
    PVOID ReducedElement
    )

/*++

Routine Description:

    This routine is called for each grammar element that is successfully parsed.

Arguments:

    Context - Supplies the context pointer supplied to the parse function.

    Symbol - Supplies the non-terminal symbol that was reduced.

    Elements - Supplies a pointer to an array of values containing the child
        elements in the rule. The function should know how many child elements
        there are based on the rule.

    ElementCount - Supplies the number of elements in the right hand side of
        this rule.

    ReducedElement - Supplies a pointer where the function can specify the
        reduction value to push back on the stack. If untouched, the default
        action is to push the first element. When called, the buffer will be
        set up for that, or zeroed if the rule is empty.

Return Value:

    0 on success.

    Returns a non-zero value if the parser should abort and return.

--*/

{

    printf("Got %d, %d elements\n", Symbol, ElementCount);
    return 0;
}

