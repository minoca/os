/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    lang.c

Abstract:

    This module implements the language specification for the setup scripting
    language.

Author:

    Evan Green 14-Oct-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../setup.h"
#include <minoca/lib/yy.h>

//
// ---------------------------------------------------------------- Definitions
//

#define YY_DIGITS "[0-9]"
#define YY_OCTAL_DIGITS "[0-7]"
#define YY_NAME0 "[a-zA-Z_]"
#define YY_HEX "[a-fA-F0-9]"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
SetupLexGetToken (
    PVOID Context,
    PLEXER_TOKEN Token
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR SetupLexerExpressions[] = {
    "/\\*.*?\\*/", // Multiline comment
    "//(\\\\.|[^\n])*", // single line comment
    YY_NAME0 "(" YY_NAME0 "|" YY_DIGITS ")*", // identifier
    "0[xX]" YY_HEX "+", // hex integer
    "0" YY_OCTAL_DIGITS "+", // octal integer
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
    NULL
};

PSTR SetupLexerTokenNames[] = {
    "MultilineComment", // Multiline comment
    "Comment", // single line comment
    "ID", // identifier
    "HEXINT", // hex integer
    "OCTINT", // octal integer
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
    NULL
};

PSTR SetupLexerIgnoreExpressions[] = {
    "[ \t\v\r\n\f]",
    NULL
};

ULONG SetupGrammarListElementList[] = {
    SetupNodeConditionalExpression, 0,
    SetupNodeListElementList, SetupTokenComma,
        SetupNodeConditionalExpression, 0,

    0
};

ULONG SetupGrammarList[] = {
    SetupTokenOpenBracket, SetupTokenCloseBracket, 0,
    SetupTokenOpenBracket, SetupNodeListElementList, SetupTokenCloseBracket, 0,
    0
};

ULONG SetupGrammarDictElement[] = {
    SetupNodeExpression, SetupTokenColon, SetupNodeConditionalExpression, 0,
    0
};

ULONG SetupGrammarDictElementList[] = {
    SetupNodeDictElement, 0,
    SetupNodeDictElementList, SetupTokenComma, SetupNodeDictElement, 0,
    0
};

ULONG SetupGrammarDict[] = {
    SetupTokenOpenBrace, SetupTokenCloseBrace, 0,
    SetupTokenOpenBrace, SetupNodeDictElementList, SetupTokenCloseBrace, 0,
    SetupTokenOpenBrace, SetupNodeDictElementList, SetupTokenComma,
        SetupTokenCloseBrace, 0,

    0
};

ULONG SetupGrammarPrimaryExpression[] = {
    SetupTokenIdentifier, 0,
    SetupTokenHexInteger, 0,
    SetupTokenOctalInteger, 0,
    SetupTokenDecimalInteger, 0,
    SetupTokenString, 0,
    SetupNodeDict, 0,
    SetupNodeList, 0,
    SetupTokenOpenParentheses, SetupNodeExpression, SetupTokenCloseParentheses,
        0,

    0
};

ULONG SetupGrammarPostfixExpression[] = {
    SetupNodePrimaryExpression, 0,
    SetupNodePostfixExpression, SetupTokenOpenBracket, SetupNodeExpression,
        SetupTokenCloseBracket, 0,

    SetupNodePostfixExpression, SetupTokenIncrement, 0,
    SetupNodePostfixExpression, SetupTokenDecrement, 0,
    0
};

ULONG SetupGrammarUnaryExpression[] = {
    SetupNodePostfixExpression, 0,
    SetupTokenIncrement, SetupNodeUnaryExpression, 0,
    SetupTokenDecrement, SetupNodeUnaryExpression, 0,
    SetupNodeUnaryOperator, SetupNodeUnaryExpression, 0,
    0
};

ULONG SetupGrammarUnaryOperator[] = {
    SetupTokenPlus, 0,
    SetupTokenMinus, 0,
    SetupTokenBitNot, 0,
    SetupTokenLogicalNot, 0,
    0
};

ULONG SetupGrammarMultiplicativeExpression[] = {
    SetupNodeUnaryExpression, 0,
    SetupNodeMultiplicativeExpression, SetupTokenAsterisk,
        SetupNodeUnaryExpression, 0,

    SetupNodeMultiplicativeExpression, SetupTokenDivide,
        SetupNodeUnaryExpression, 0,

    SetupNodeMultiplicativeExpression, SetupTokenModulo,
        SetupNodeUnaryExpression, 0,

    0,
};

ULONG SetupGrammarAdditiveExpression[] = {
    SetupNodeMultiplicativeExpression, 0,
    SetupNodeAdditiveExpression, SetupTokenPlus,
        SetupNodeMultiplicativeExpression, 0,

    SetupNodeAdditiveExpression, SetupTokenMinus,
        SetupNodeMultiplicativeExpression, 0,

    0
};

ULONG SetupGrammarShiftExpression[] = {
    SetupNodeAdditiveExpression, 0,
    SetupNodeShiftExpression, SetupTokenLeftShift,
        SetupNodeAdditiveExpression, 0,

    SetupNodeShiftExpression, SetupTokenRightShift,
        SetupNodeAdditiveExpression, 0,

    0
};

ULONG SetupGrammarRelationalExpression[] = {
    SetupNodeShiftExpression, 0,
    SetupNodeRelationalExpression, SetupTokenLessThan,
        SetupNodeShiftExpression, 0,

    SetupNodeRelationalExpression, SetupTokenGreaterThan,
        SetupNodeShiftExpression, 0,

    SetupNodeRelationalExpression, SetupTokenLessOrEqual,
        SetupNodeShiftExpression, 0,

    SetupNodeRelationalExpression, SetupTokenGreaterOrEqual,
        SetupNodeShiftExpression, 0,

    0
};

ULONG SetupGrammarEqualityExpression[] = {
    SetupNodeRelationalExpression, 0,
    SetupNodeEqualityExpression, SetupTokenIsEqual,
        SetupNodeRelationalExpression, 0,

    SetupNodeEqualityExpression, SetupTokenIsNotEqual,
        SetupNodeRelationalExpression, 0,

    0
};

ULONG SetupGrammarAndExpression[] = {
    SetupNodeEqualityExpression, 0,
    SetupNodeAndExpression, SetupTokenBitAnd, SetupNodeEqualityExpression, 0,
    0
};

ULONG SetupGrammarExclusiveOrExpression[] = {
    SetupNodeAndExpression, 0,
    SetupNodeExclusiveOrExpression, SetupTokenXor, SetupNodeAndExpression, 0,
    0
};

ULONG SetupGrammarInclusiveOrExpression[] = {
    SetupNodeExclusiveOrExpression, 0,
    SetupNodeInclusiveOrExpression, SetupTokenBitOr,
        SetupNodeExclusiveOrExpression, 0,

    0
};

ULONG SetupGrammarLogicalAndExpression[] = {
    SetupNodeInclusiveOrExpression, 0,
    SetupNodeLogicalAndExpression, SetupTokenLogicalAnd,
        SetupNodeExclusiveOrExpression, 0,

    0
};

ULONG SetupGrammarLogicalOrExpression[] = {
    SetupNodeLogicalAndExpression, 0,
    SetupNodeLogicalOrExpression, SetupTokenLogicalOr,
        SetupNodeLogicalAndExpression, 0,

    0
};

ULONG SetupGrammarConditionalExpression[] = {
    SetupNodeLogicalOrExpression, SetupTokenQuestion, SetupNodeExpression,
        SetupNodeConditionalExpression, 0,

    SetupNodeLogicalOrExpression, 0,
    0
};

ULONG SetupGrammarAssignmentExpression[] = {
    SetupNodeUnaryExpression, SetupNodeAssignmentOperator,
        SetupNodeAssignmentExpression, 0,

    SetupNodeConditionalExpression, 0,
    0
};

ULONG SetupGrammarAssignmentOperator[] = {
    SetupTokenAssign, 0,
    SetupTokenMultiplyAssign, 0,
    SetupTokenDivideAssign, 0,
    SetupTokenModuloAssign, 0,
    SetupTokenAddAssign, 0,
    SetupTokenSubtractAssign, 0,
    SetupTokenLeftAssign, 0,
    SetupTokenRightAssign, 0,
    SetupTokenAndAssign, 0,
    SetupTokenXorAssign, 0,
    SetupTokenOrAssign, 0,
    0
};

ULONG SetupGrammarExpression[] = {
    SetupNodeAssignmentExpression, 0,
    SetupNodeExpression, SetupTokenComma, SetupNodeAssignmentExpression, 0,
    0
};

ULONG SetupGrammarStatementList[] = {
    SetupNodeExpressionStatement, 0,
    SetupNodeStatementList, SetupNodeExpressionStatement, 0,
    0
};

ULONG SetupGrammarExpressionStatement[] = {
    SetupTokenSemicolon, 0,
    SetupNodeExpression, SetupTokenSemicolon, 0,
    0
};

ULONG SetupGrammarTranslationUnit[] = {
    SetupNodeStatementList, 0,
    0
};

PARSER_GRAMMAR_ELEMENT SetupGrammar[] = {
    {"ListElementList", 0, SetupGrammarListElementList},
    {"List", 0, SetupGrammarList},
    {"DictElement", 0, SetupGrammarDictElement},
    {"DictElementList", 0, SetupGrammarDictElementList},
    {"Dict", 0, SetupGrammarDict},
    {"PrimaryExpression", 0, SetupGrammarPrimaryExpression},
    {"PostfixExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarPostfixExpression},

    {"UnaryExpression", YY_GRAMMAR_COLLAPSE_ONE, SetupGrammarUnaryExpression},
    {"UnaryOperator", 0, SetupGrammarUnaryOperator},
    {"MultiplicativeExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarMultiplicativeExpression},

    {"AdditiveExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarAdditiveExpression},

    {"ShiftExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarShiftExpression},

    {"RelationalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarRelationalExpression},

    {"EqualityExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarEqualityExpression},

    {"AndExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarAndExpression},

    {"ExclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarExclusiveOrExpression},

    {"InclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarInclusiveOrExpression},

    {"LogicalAndExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarLogicalAndExpression},

    {"LogicalOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarLogicalOrExpression},

    {"ConditionalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarConditionalExpression},

    {"AssignmentExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     SetupGrammarAssignmentExpression},

    {"AssignmentOperator", 0, SetupGrammarAssignmentOperator},
    {"Expression", 0, SetupGrammarExpression},
    {"StatementList", 0, SetupGrammarStatementList},
    {"ExpressionStatement", 0, SetupGrammarExpressionStatement},
    {"TranslationUnit", 0, SetupGrammarTranslationUnit},
};

PARSER SetupParser;

//
// ------------------------------------------------------------------ Functions
//

INT
SetupParseScript (
    PSETUP_SCRIPT Script,
    PVOID *TranslationUnit
    )

/*++

Routine Description:

    This routine lexes and parses the given script data.

Arguments:

    Script - Supplies a pointer to the script to parse.

    TranslationUnit - Supplies a pointer where the translation unit will be
        returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG Column;
    KSTATUS KStatus;
    LEXER Lexer;
    ULONG Line;
    INT Status;

    memset(&Lexer, 0, sizeof(Lexer));
    Lexer.Input = Script->Data;
    Lexer.InputSize = Script->Size;
    Lexer.Expressions = SetupLexerExpressions;
    Lexer.IgnoreExpressions = SetupLexerIgnoreExpressions;
    Lexer.ExpressionNames = SetupLexerTokenNames;
    Lexer.TokenBase = SETUP_TOKEN_BASE;
    YyLexInitialize(&Lexer);
    SetupParser.Context = &Lexer;
    if (SetupParser.GetToken == NULL) {
        SetupParser.Flags = 0;
        SetupParser.Allocate = (PYY_ALLOCATE)malloc;
        SetupParser.Free = free;
        SetupParser.GetToken = SetupLexGetToken;
        SetupParser.Grammar = SetupGrammar;
        SetupParser.GrammarBase = SetupNodeBegin;
        SetupParser.GrammarEnd = SetupNodeEnd;
        SetupParser.GrammarStart = SetupNodeTranslationUnit;
        SetupParser.MaxRecursion = 500;
        SetupParser.Lexer = &Lexer;
    }

    YyParserInitialize(&SetupParser);
    KStatus = YyParse(&SetupParser, (PPARSER_NODE *)TranslationUnit);
    if (!KSUCCESS(KStatus)) {
        Column = 0;
        Line = 0;
        if (SetupParser.NextToken != NULL) {
            Column = SetupParser.NextToken->Column;
            Line = SetupParser.NextToken->Line;
        }

        fprintf(stderr,
                "Parsing script %s failed at line %d:%d: %x\n",
                Script->Path,
                Line,
                Column,
                KStatus);

        Status = EILSEQ;
        goto ParseScriptEnd;
    }

    Status = 0;

ParseScriptEnd:
    return Status;
}

VOID
SetupDestroyParseTree (
    PVOID TranslationUnit
    )

/*++

Routine Description:

    This routine destroys the translation unit returned when a script was
    parsed.

Arguments:

    TranslationUnit - Supplies a pointer to the translation unit to destroy.

Return Value:

    None.

--*/

{

    YyDestroyNode(&SetupParser, TranslationUnit);
    return;
}

PSTR
SetupGetNodeGrammarName (
    PSETUP_NODE Node
    )

/*++

Routine Description:

    This routine returns the grammatical element name for the given node.

Arguments:

    Node - Supplies a pointer to the node.

Return Value:

    Returns a pointer to a constant string of the name of the grammar element
    represented by this execution node.

--*/

{

    PPARSER_NODE ParseNode;

    ParseNode = Node->ParseNode;
    return SetupGrammar[ParseNode->GrammarElement - SetupNodeBegin].Name;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
SetupLexGetToken (
    PVOID Context,
    PLEXER_TOKEN Token
    )

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

{

    PLEXER Lexer;
    KSTATUS Status;

    Lexer = Context;
    while (TRUE) {
        Status = YyLexGetToken(Lexer, Token);
        if (!KSUCCESS(Status)) {
            break;
        }

        if ((Token->Value == SetupTokenMultilineComment) ||
            (Token->Value == SetupTokenComment)) {

            continue;
        }

        break;
    }

    return Status;
}

