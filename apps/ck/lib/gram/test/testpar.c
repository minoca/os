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

#define YY_TOKEN_OFFSET 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _CK_NODE {
    CK_SYMBOL Symbol;
    ULONG Children;
    ULONG Descendants;
    ULONG Depth;
    ULONG ChildIndex;
} CK_NODE, *PCK_NODE;

typedef union _CK_SYMBOL_UNION {
    CK_SYMBOL Symbol;
    LEXER_TOKEN Token;
    CK_NODE Node;
} CK_SYMBOL_UNION, *PCK_SYMBOL_UNION;

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

VOID
CkgpPrintToken (
    PLEXER Lexer,
    PLEXER_TOKEN Token
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

PSTR CkNodeNames[] = {
    "Start",
    "ListElementList",
    "List",
    "DictElement",
    "DictElementList",
    "Dict",
    "PrimaryExpression",
    "PostfixExpression",
    "ArgumentExpressionList",
    "UnaryExpression",
    "UnaryOperator",
    "MultiplicativeExpression",
    "AdditiveExpression",
    "RangeExpression",
    "ShiftExpression",
    "AndExpression",
    "ExclusiveOrExpression",
    "InclusiveOrExpression",
    "RelationalExpression",
    "EqualityExpression",
    "LogicalAndExpression",
    "LogicalOrExpression",
    "ConditionalExpression",
    "AssignmentExpression",
    "AssignmentOperator",
    "Expression",
    "VariableSpecifier",
    "VariableDeclaration",
    "VariableDefinition",
    "Statement",
    "CompoundStatement",
    "StatementList",
    "ExpressionStatement",
    "SelectionStatement",
    "IterationStatement",
    "JumpStatement",
    "IdentifierList",
    "FunctionDefinition",
    "ClassMember",
    "ClassMemberList",
    "ClassBody",
    "ClassDefinition",
    "ModuleName",
    "ImportStatement",
    "ExternalDeclaration",
    "TranslationUnit",
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
    sizeof(CK_SYMBOL_UNION),
    0,
    NULL, //"ck"
};

PCK_SYMBOL_UNION CkSymbols;
ULONG CkSymbolsCount;
ULONG CkSymbolsCapacity;
ULONG CkMaxDepth;

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
    PCK_SYMBOL_UNION Child;
    ULONG ChildIndex;
    ULONG ChildNode;
    ULONG Column;
    FILE *File;
    KSTATUS KStatus;
    LEXER Lexer;
    ULONG Line;
    ULONG Size;
    INT Status;
    PCK_SYMBOL_UNION Value;

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
    if (Status != 0) {
        goto ParseScriptEnd;
    }

    if (CkSymbolsCount == 0) {
        goto ParseScriptEnd;
    }

    //
    // Print the abstract syntax tree.
    //

    printf("\n");
    Value = CkSymbols + CkSymbolsCount - 1;
    while (Value >= CkSymbols) {
        if (Value->Symbol < CkNodeStart) {
            Value -= 1;
            continue;
        }

        assert(Value->Node.Depth <= CkMaxDepth);

        printf("%*s (%2d/%2d/%2d): ",
               CkMaxDepth - Value->Node.Depth + 20,
               CkNodeNames[Value->Symbol - CkNodeStart],
               Value->Node.Children,
               Value->Node.Descendants,
               Value->Node.Depth);

        ChildNode = 1;
        Child = CkSymbols + Value->Node.ChildIndex;
        for (ChildIndex = 0;
             ChildIndex < Value->Node.Children;
             ChildIndex += 1) {

            if (Child->Symbol < CkNodeStart) {
                CkgpPrintToken(&Lexer, &(Child->Token));
                printf(" ");

            } else {
                printf("$%d ", ChildNode);
                ChildNode += 1;
            }

            Child += 1;
        }

        printf("\n");
        Value -= 1;
    }

ParseScriptEnd:
    if (CkSymbols != NULL) {
        free(CkSymbols);
    }

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

    KSTATUS KStatus;
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

    CkgpPrintToken(Lexer, Token);
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

    PCK_SYMBOL_UNION Child;
    ULONG ChildIndex;
    PVOID NewBuffer;
    ULONG NewCapacity;
    PCK_NODE NewNode;

    if (CkSymbolsCount + ElementCount >= CkSymbolsCapacity) {
        if (CkSymbolsCapacity == 0) {
            NewCapacity = 16;

        } else {
            NewCapacity = CkSymbolsCapacity * 2;
        }

        while (NewCapacity < CkSymbolsCount + ElementCount) {
            NewCapacity *= 2;
        }

        NewBuffer = realloc(CkSymbols, NewCapacity * sizeof(CK_SYMBOL_UNION));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        CkSymbols = NewBuffer;
        CkSymbolsCapacity = NewCapacity;
    }

    //
    // Set up the new node by counting the total child descendents.
    //

    NewNode = ReducedElement;
    NewNode->ChildIndex = CkSymbolsCount;
    NewNode->Symbol = Symbol;
    NewNode->Children = ElementCount;
    NewNode->Descendants = 0;
    NewNode->Depth = 0;

    //
    // Copy the new child elements into the stream.
    //

    memcpy(CkSymbols + CkSymbolsCount,
           Elements,
           ElementCount * sizeof(CK_SYMBOL_UNION));

    CkSymbolsCount += ElementCount;

    //
    // Sum the descendents.
    //

    Child = Elements;
    for (ChildIndex = 0; ChildIndex < ElementCount; ChildIndex += 1) {
        if (Child->Symbol >= CkNodeStart) {
            NewNode->Descendants += Child->Node.Children +
                                    Child->Node.Descendants;

            if (Child->Node.Depth + 1 > NewNode->Depth) {
                NewNode->Depth = Child->Node.Depth + 1;
            }
        }

        Child += 1;
    }

    if (NewNode->Depth > CkMaxDepth) {
        CkMaxDepth = NewNode->Depth;
    }

    printf("Got %s, %d elements, %d Descendants, depth %d\n",
           CkNodeNames[Symbol - CkNodeStart],
           ElementCount,
           NewNode->Descendants,
           NewNode->Depth);

    return 0;
}

VOID
CkgpPrintToken (
    PLEXER Lexer,
    PLEXER_TOKEN Token
    )

/*++

Routine Description:

    This routine prints a token value.

Arguments:

    Lexer - Supplies a pointer to the lexer context.

    Token - Supplies a pointer to the token.

Return Value:

    0 on success, including EOF.

    Returns a non-zero value if there was an error reading the token.

--*/

{

    CHAR Buffer[64];
    ULONG Size;

    Size = MIN(Token->Size, sizeof(Buffer) - 1);
    strncpy(Buffer, ((PLEXER)Lexer)->Input + Token->Position, Size);
    Buffer[Size] = '\0';
    printf("%s (%d:%d)", Buffer, Token->Line, Token->Column);
    return;
}

