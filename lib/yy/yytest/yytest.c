/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    yytest.c

Abstract:

    This module tests the lexer/parser library.

Author:

    Evan Green 9-Oct-2015

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#define YY_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define YY_TOKEN_BASE 512

#define YY_DIGITS "[0-9]"
#define YY_OCTAL_DIGITS "[0-7]"
#define YY_NAME0 "[a-zA-Z_]"
#define YY_HEX "[a-fA-F0-9]"
#define YY_EXP "[Ee][+-]?" YY_DIGITS "+"
#define YY_FLOAT_END "(f|F|l|L)"
#define YY_INT_END "(u|U|l|L)*"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the possible tokens, which must line up with the lexer expressions
// array.
//

typedef enum _C_TOKENS {
    CTokenMultilineComment = YY_TOKEN_BASE,
    CTokenComment,
    CTokenBuiltinVaList,
    CTokenAttribute,
    CTokenAuto,
    CTokenBreak,
    CTokenCase,
    CTokenChar,
    CTokenConst,
    CTokenContinue,
    CTokenDefault,
    CTokenDo,
    CTokenDouble,
    CTokenElse,
    CTokenEnum,
    CTokenExtern,
    CTokenFloat,
    CTokenFor,
    CTokenGoto,
    CTokenIf,
    CTokenInt,
    CTokenLong,
    CTokenRegister,
    CTokenReturn,
    CTokenShort,
    CTokenSigned,
    CTokenSizeof,
    CTokenStatic,
    CTokenStruct,
    CTokenSwitch,
    CTokenTypedef,
    CTokenUnion,
    CTokenUnsigned,
    CTokenVoid,
    CTokenVolatile,
    CTokenWhile,
    CTokenIdentifier,
    CTokenHexInteger,
    CTokenOctalInteger,
    CTokenDecimalInteger,
    CTokenCharacterConstant,
    CTokenFloatConstant,
    CTokenFloatConstant2,
    CTokenFloatConstant3,
    CTokenStringLiteral,
    CTokenEllipsis,
    CTokenRightAssign,
    CTokenLeftAssign,
    CTokenAddAssign,
    CTokenSubtractAssign,
    CTokenMultiplyAssign,
    CTokenDivideAssign,
    CTokenModuloAssign,
    CTokenAndAssign,
    CTokenXorAssign,
    CTokenOrAssign,
    CTokenRightShift,
    CTokenLeftShift,
    CTokenIncrement,
    CTokenDecrement,
    CTokenPointerOp,
    CTokenLogicalAnd,
    CTokenLogicalOr,
    CTokenLessEqual,
    CTokenGreaterEqual,
    CTokenEqualOp,
    CTokenNotEqual,
    CTokenSemicolon,
    CTokenOpenBrace,
    CTokenCloseBrace,
    CTokenComma,
    CTokenColon,
    CTokenAssign,
    CTokenOpenParentheses,
    CTokenCloseParentheses,
    CTokenOpenBracket,
    CTokenCloseBracket,
    CTokenDot,
    CTokenBitAnd,
    CTokenLogicalNot,
    CTokenBitNot,
    CTokenMinus,
    CTokenPlus,
    CTokenAsterisk,
    CTokenDivide,
    CTokenModulo,
    CTokenLessThan,
    CTokenGreaterThan,
    CTokenXor,
    CTokenBitOr,
    CTokenQuestion,
    CTokenPreprocessorDefine,
    CTokenPreprocessorInclude,
    CTokenPreprocessor,

    CTokenTypeName
} C_TOKENS, *PC_TOKENS;

typedef enum _C_NODE {
    CNodeStart = 1024,
    CNodeStringLiteral = CNodeStart,
    CNodePrimaryExpression,
    CNodePostfixExpression,
    CNodeArgumentExpressionList,
    CNodeUnaryExpression,
    CNodeUnaryOperator,
    CNodeCastExpression,
    CNodeMultiplicativeExpression,
    CNodeAdditiveExpression,
    CNodeShiftExpression,
    CNodeRelationalExpression,
    CNodeEqualityExpression,
    CNodeAndExpression,
    CNodeExclusiveOrExpression,
    CNodeInclusiveOrExpression,
    CNodeLogicalAndExpression,
    CNodeLogicalOrExpression,
    CNodeConditionalExpression,
    CNodeAssignmentExpression,
    CNodeAssignmentOperator,
    CNodeExpression,
    CNodeConstantExpression,
    CNodeDeclaration,
    CNodeDeclarationSpecifiers,
    CNodeInitDeclaratorList,
    CNodeInitDeclarator,
    CNodeStorageClassSpecifier,
    CNodeTypeSpecifier,
    CNodeStructOrUnionSpecifier,
    CNodeStructOrUnion,
    CNodeStructDeclarationList,
    CNodeStructDeclaration,
    CNodeSpecifierQualifierList,
    CNodeStructDeclaratorList,
    CNodeStructDeclarator,
    CNodeEnumSpecifier,
    CNodeEnumeratorList,
    CNodeEnumerator,
    CNodeTypeQualifier,
    CNodeDeclarator,
    CNodeDirectDeclarator,
    CNodePointer,
    CNodeTypeQualifierList,
    CNodeParameterTypeList,
    CNodeParameterList,
    CNodeParameterDeclaration,
    CNodeIdentifierList,
    CNodeTypeName,
    CNodeAbstractDeclarator,
    CNodeDirectAbstractDeclarator,
    CNodeInitializer,
    CNodeInitializerList,
    CNodeStatement,
    CNodeLabeledStatement,
    CNodeCompoundStatement,
    CNodeDeclarationList,
    CNodeStatementList,
    CNodeExpressionStatement,
    CNodeSelectionStatement,
    CNodeIterationStatement,
    CNodeJumpStatement,
    CNodeTranslationUnit,
    CNodeExternalDeclaration,
    CNodeFunctionDefinition,
    CNodeEnd
} C_NODE, *PC_NODE;

/*++

Structure Description:

    This structure stores a C typedef value.

Members:

    ListEntry - Stores pointers to the next and previous types on the list.

    Token - Stores a pointer to the token containing the identifier for that
        type.

--*/

typedef struct _C_TYPE {
    LIST_ENTRY ListEntry;
    PLEXER_TOKEN Token;
} C_TYPE, *PC_TYPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
YyTestParse (
    PSTR Path
    );

VOID
YyTestNodeCallback (
    PVOID Context,
    PPARSER_NODE Node,
    BOOL Create
    );

VOID
YyTestVisitDeclarator (
    PVOID Context,
    PPARSER_NODE Declarator,
    BOOL Create
    );

VOID
YyTestAddType (
    PLEXER Lexer,
    PLEXER_TOKEN Token,
    BOOL Create
    );

PC_TYPE
YyTestFindType (
    PLEXER Lexer,
    PLEXER_TOKEN Identifier
    );

VOID
YyTestClearTypes (
    VOID
    );

KSTATUS
YyTestGetToken (
    PVOID Context,
    PLEXER_TOKEN Token
    );

ULONG
YyTestLex (
    PSTR Path,
    PLEXER Lexer
    );

INT
YyTestReadFile (
    PSTR Path,
    PSTR *FileBuffer,
    PULONG Size
    );

VOID
YyTestPrintTree (
    PLEXER Lexer,
    PPARSER_NODE Node,
    ULONG RecursionDepth
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR YyTestCLexerExpressions[] = {
    "/\\*.*?\\*/", // Multiline comment
    "//(\\\\.|[^\n])*", // single line comment
    "__builtin_va_list",
    "__attribute__[ \t]*\\(\\([^()]*(\\(.*?\\))?\\)\\)", // GCC attribute
    "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "int",
    "long",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
    YY_NAME0 "(" YY_NAME0 "|" YY_DIGITS ")*", // identifier
    "0[xX]" YY_HEX "+" YY_INT_END "?", // hex integer
    "0" YY_OCTAL_DIGITS "+" YY_INT_END "?", // octal integer
    YY_DIGITS "+" YY_INT_END "?", // decimal integer
    "L?'(\\\\.|[^\\'])+'", // character constant
    YY_DIGITS "+" YY_EXP YY_FLOAT_END "?", // floating point integer
    YY_DIGITS "*\\." YY_DIGITS "+(" YY_EXP ")?" YY_FLOAT_END "?", // float
    YY_DIGITS "+\\." YY_DIGITS "*(" YY_EXP ")?" YY_FLOAT_END "?", // float
    "L?\"(\\\\.|[^\\\"])*\"", // string literal
    "\\.\\.\\.",
    ">>=",
    "<<=",
    "+=",
    "-=",
    "*=",
    "/=",
    "%=",
    "&=",
    "^=",
    "\\|=",
    ">>",
    "<<",
    "\\+\\+",
    "--",
    "->",
    "&&",
    "\\|\\|",
    "<=",
    ">=",
    "==",
    "!=",
    ";",
    "({|<%)", // {
    "(}|%>)", // }
    ",",
    ":",
    "=",
    "\\(",
    "\\)",
    "(\\[|<:)", // [
    "(]|:>)", // ]
    "\\.",
    "&",
    "!",
    "~",
    "-",
    "+",
    "\\*",
    "/",
    "%",
    "<",
    ">",
    "^",
    "\\|",
    "\\?",
    "#[ \t]*define[ \t]+(\\\\.|[^\n])+", // define
    "#[ \t]*include[ \t]+[<\"].*?[>\"][^\n]*", // include
    "#[ \t]*(\\\\.|[^\n])+", // generic preprocessor
    NULL
};

PSTR YyTestCLexerTokenNames[] = {
    "Multiline comment",
    "Comment",
    "__builtin_va_list",
    "__attribute__",
    "auto",
    "break",
    "case",
    "char",
    "const",
    "continue",
    "default",
    "do",
    "double",
    "else",
    "enum",
    "extern",
    "float",
    "for",
    "goto",
    "if",
    "int",
    "long",
    "register",
    "return",
    "short",
    "signed",
    "sizeof",
    "static",
    "struct",
    "switch",
    "typedef",
    "union",
    "unsigned",
    "void",
    "volatile",
    "while",
    "id",
    "hex int",
    "octal int",
    "decimal int",
    "character",
    "float1",
    "float2",
    "float3",
    "string",
    "...",
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
    "->",
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
    ".",
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
    "#define",
    "#include",
    "preprocessor",
    "type",
    NULL
};

PSTR YyTestCLexerIgnoreExpressions[] = {
    "[ \t\v\r\n\f]",
    NULL
};

//
// Grammar element definitions.
//

ULONG YyTestCStringLiteral[] = {
    CTokenStringLiteral, 0,
    CNodeStringLiteral, CTokenStringLiteral, 0,
    0,
};

ULONG YyTestCPrimaryExpression[] = {
    CTokenIdentifier, 0,
    CTokenHexInteger, 0,
    CTokenOctalInteger, 0,
    CTokenDecimalInteger, 0,
    CTokenCharacterConstant, 0,
    CTokenFloatConstant, 0,
    CTokenFloatConstant2, 0,
    CTokenFloatConstant3, 0,
    CNodeStringLiteral, 0,
    CTokenOpenParentheses, CNodeExpression, CTokenCloseParentheses, 0,
    0
};

ULONG YyTestCPostfixExpression[] = {
    CNodePrimaryExpression, 0,
    CNodePostfixExpression, CTokenOpenBracket, CNodeExpression,
        CTokenCloseBracket, 0,

    CNodePostfixExpression, CTokenOpenParentheses,
        CTokenCloseParentheses, 0,

    CNodePostfixExpression, CTokenOpenParentheses,
        CNodeArgumentExpressionList, CTokenCloseParentheses, 0,

    CNodePostfixExpression, CTokenDot, CTokenIdentifier, 0,
    CNodePostfixExpression, CTokenPointerOp, CTokenIdentifier, 0,
    CNodePostfixExpression, CTokenIncrement, 0,
    CNodePostfixExpression, CTokenDecrement, 0,
    0
};

ULONG YyTestCArgumentExpressionList[] = {
    CNodeAssignmentExpression, 0,
    CNodeArgumentExpressionList, CTokenComma, CNodeAssignmentExpression, 0,
    0
};

ULONG YyTestCUnaryExpression[] = {
    CNodePostfixExpression, 0,
    CTokenIncrement, CNodeUnaryExpression, 0,
    CTokenDecrement, CNodeUnaryExpression, 0,
    CNodeUnaryOperator, CNodeCastExpression, 0,
    CTokenSizeof, CNodeUnaryExpression, 0,
    CTokenSizeof, CTokenOpenParentheses, CNodeTypeName,
        CTokenCloseParentheses, 0,

    0
};

ULONG YyTestCUnaryOperator[] = {
    CTokenBitAnd, 0,
    CTokenAsterisk, 0,
    CTokenPlus, 0,
    CTokenMinus, 0,
    CTokenBitNot, 0,
    CTokenLogicalNot, 0,
    0
};

ULONG YyTestCCastExpression[] = {
    CNodeUnaryExpression, 0,
    CTokenOpenParentheses, CNodeTypeName, CTokenCloseParentheses,
        CNodeCastExpression, 0,

    0
};

ULONG YyTestCMultiplicativeExpression[] = {
    CNodeCastExpression, 0,
    CNodeMultiplicativeExpression, CTokenAsterisk,
        CNodeCastExpression, 0,

    CNodeMultiplicativeExpression, CTokenDivide,
        CNodeCastExpression, 0,

    CNodeMultiplicativeExpression, CTokenModulo,
        CNodeCastExpression, 0,

    0
};

ULONG YyTestCAdditiveExpression[] = {
    CNodeMultiplicativeExpression, 0,
    CNodeAdditiveExpression, CTokenPlus,
        CNodeMultiplicativeExpression, 0,

    CNodeAdditiveExpression, CTokenMinus,
        CNodeMultiplicativeExpression, 0,

    0
};

ULONG YyTestCShiftExpression[] = {
    CNodeAdditiveExpression, 0,
    CNodeShiftExpression, CTokenLeftShift, CNodeAdditiveExpression, 0,
    CNodeShiftExpression, CTokenRightShift, CNodeAdditiveExpression,
        0,

    0
};

ULONG YyTestCRelationalExpression[] = {
    CNodeShiftExpression, 0,
    CNodeRelationalExpression, CTokenLessThan, CNodeShiftExpression,
        0,

    CNodeRelationalExpression, CTokenGreaterThan, CNodeShiftExpression,
        0,

    CNodeRelationalExpression, CTokenLessEqual, CNodeShiftExpression,
        0,

    CNodeRelationalExpression, CTokenGreaterEqual,
        CNodeShiftExpression, 0,

    0
};

ULONG YyTestCEqualityExpression[] = {
    CNodeRelationalExpression, 0,
    CNodeEqualityExpression, CTokenEqualOp, CNodeRelationalExpression,
        0,

    CNodeEqualityExpression, CTokenNotEqual, CNodeRelationalExpression,
        0,

    0
};

ULONG YyTestCAndExpression[] = {
    CNodeEqualityExpression, 0,
    CNodeAndExpression, CTokenBitAnd, CNodeEqualityExpression, 0,
    0
};

ULONG YyTestCExclusiveOrExpression[] = {
    CNodeAndExpression, 0,
    CNodeExclusiveOrExpression, CTokenXor, CNodeAndExpression, 0,
    0
};

ULONG YyTestCInclusiveOrExpression[] = {
    CNodeExclusiveOrExpression, 0,
    CNodeInclusiveOrExpression, CTokenBitOr,
        CNodeExclusiveOrExpression, 0,

    0
};

ULONG YyTestCLogicalAndExpression[] = {
    CNodeInclusiveOrExpression, 0,
    CNodeLogicalAndExpression, CTokenLogicalAnd,
        CNodeInclusiveOrExpression, 0,

    0
};

ULONG YyTestCLogicalOrExpression[] = {
    CNodeLogicalAndExpression, 0,
    CNodeLogicalOrExpression, CTokenLogicalOr,
        CNodeLogicalAndExpression, 0,

    0
};

ULONG YyTestCConditionalExpression[] = {
    CNodeLogicalOrExpression, CTokenQuestion, CNodeExpression,
        CTokenColon, CNodeConditionalExpression, 0,

    CNodeLogicalOrExpression, 0,
    0
};

ULONG YyTestCAlignmentExpression[] = {
    CNodeUnaryExpression, CNodeAssignmentOperator,
        CNodeAssignmentExpression, 0,

    CNodeConditionalExpression, 0,
    0
};

ULONG YyTestCAssignmentOperator[] = {
    CTokenAssign, 0,
    CTokenMultiplyAssign, 0,
    CTokenDivideAssign, 0,
    CTokenModuloAssign, 0,
    CTokenAddAssign, 0,
    CTokenSubtractAssign, 0,
    CTokenLeftAssign, 0,
    CTokenRightAssign, 0,
    CTokenAndAssign, 0,
    CTokenXorAssign, 0,
    CTokenOrAssign, 0,
    0
};

ULONG YyTestCExpression[] = {
    CNodeAssignmentExpression, 0,
    CNodeExpression, CTokenComma, CNodeAssignmentExpression, 0,
    0
};

ULONG YyTestCConstantExpression[] = {
    CNodeConditionalExpression, 0,
    0
};

ULONG YyTestCDeclaration[] = {
    CNodeDeclarationSpecifiers, CTokenSemicolon, 0,
    CNodeDeclarationSpecifiers, CNodeInitDeclaratorList,
        CTokenSemicolon, 0,

    0
};

ULONG YyTestCDeclarationSpecifiers[] = {
    CNodeStorageClassSpecifier, CNodeDeclarationSpecifiers, 0,
    CNodeStorageClassSpecifier, 0,
    CNodeTypeSpecifier, CNodeDeclarationSpecifiers, 0,
    CNodeTypeSpecifier, 0,
    CNodeTypeQualifier, CNodeDeclarationSpecifiers, 0,
    CNodeTypeQualifier, 0,
    0
};

ULONG YyTestCInitDeclaratorList[] = {
    CNodeInitDeclarator, 0,
    CNodeInitDeclaratorList, CTokenComma, CNodeInitDeclarator, 0,
    0
};

ULONG YyTestCInitDeclarator[] = {
    CNodeDeclarator, CTokenAssign, CNodeInitializer, 0,
    CNodeDeclarator, 0,
    0
};

ULONG YyTestCStorageClassSpecifier[] = {
    CTokenTypedef, 0,
    CTokenExtern, 0,
    CTokenStatic, 0,
    CTokenAuto, 0,
    CTokenRegister, 0,
    0
};

ULONG YyTestCTypeSpecifier[] = {
    CTokenVoid, 0,
    CTokenChar, 0,
    CTokenShort, 0,
    CTokenInt, 0,
    CTokenLong, 0,
    CTokenFloat, 0,
    CTokenDouble, 0,
    CTokenSigned, 0,
    CTokenUnsigned, 0,
    CNodeStructOrUnionSpecifier, 0,
    CNodeEnumSpecifier, 0,
    CTokenTypeName, 0,
    0
};

ULONG YyTestCStructOrUnionSpecifier[] = {
    CNodeStructOrUnion, CTokenIdentifier, CTokenOpenBrace,
        CNodeStructDeclarationList, CTokenCloseBrace, 0,

    CNodeStructOrUnion, CTokenOpenBrace,
        CNodeStructDeclarationList, CTokenCloseBrace, 0,

    CNodeStructOrUnion, CTokenIdentifier, 0,
    0
};

ULONG YyTestCStructOrUnion[] = {
    CTokenStruct, 0,
    CTokenUnion, 0,
    0
};

ULONG YyTestCStructDeclarationList[] = {
    CNodeStructDeclaration, 0,
    CNodeStructDeclarationList, CNodeStructDeclaration, 0,
    0
};

ULONG YyTestCStructDeclaration[] = {
    CNodeSpecifierQualifierList, CNodeStructDeclaratorList,
        CTokenSemicolon, 0,

    0
};

ULONG YyTestCSpecifierQualifierList[] = {
    CNodeTypeSpecifier, CNodeSpecifierQualifierList, 0,
    CNodeTypeSpecifier, 0,
    CNodeTypeQualifier, CNodeSpecifierQualifierList, 0,
    CNodeTypeQualifier, 0,
    0
};

ULONG YyTestCStructDeclaratorList[] = {
    CNodeStructDeclarator, 0,
    CNodeStructDeclaratorList, CTokenComma, CNodeStructDeclarator, 0,
    0
};

ULONG YyTestCStructDeclarator[] = {
    CNodeDeclarator, 0,
    CTokenColon, CNodeConstantExpression, 0,
    CNodeDeclarator, CTokenColon, CNodeConstantExpression, 0,
    0
};

ULONG YyTestCEnumSpecifier[] = {
    CTokenEnum, CTokenOpenBrace, CNodeEnumeratorList,
        CTokenCloseBrace, 0,

    CTokenEnum, CTokenIdentifier, CTokenOpenBrace, CNodeEnumeratorList,
        CTokenCloseBrace, 0,

    CTokenEnum, CTokenIdentifier, CTokenOpenBrace, CNodeEnumeratorList,
        CTokenComma, CTokenCloseBrace, 0,

    CTokenEnum, CTokenIdentifier, 0,
    0
};

ULONG YyTestCEnumeratorList[] = {
    CNodeEnumerator, 0,
    CNodeEnumeratorList, CTokenComma, CNodeEnumerator, 0,
    0
};

ULONG YyTestCEnumerator[] = {
    CTokenIdentifier, CTokenAssign, CNodeConstantExpression, 0,
    CTokenIdentifier, 0,
    0
};

ULONG YyTestCTypeQualifier[] = {
    CTokenConst, 0,
    CTokenVolatile, 0,
    0
};

ULONG YyTestCDeclarator[] = {
    CNodePointer, CNodeDirectDeclarator, 0,
    CNodeDirectDeclarator, 0,
    0
};

ULONG YyTestCDirectDeclarator[] = {
    CTokenIdentifier, 0,
    CTokenOpenParentheses, CNodeDeclarator, CTokenCloseParentheses, 0,
    CNodeDirectDeclarator, CTokenOpenBracket,
        CNodeConstantExpression, CTokenCloseBracket, 0,

    CNodeDirectDeclarator, CTokenOpenBracket, CTokenCloseBracket, 0,
    CNodeDirectDeclarator, CTokenOpenParentheses,
        CNodeParameterTypeList, CTokenCloseParentheses, 0,

    CNodeDirectDeclarator, CTokenOpenParentheses,
        CNodeIdentifierList, CTokenCloseParentheses, 0,

    CNodeDirectDeclarator, CTokenOpenParentheses,
        CTokenCloseParentheses, 0,

    0
};

ULONG YyTestCPointer[] = {
    CTokenAsterisk, CNodeTypeQualifierList, 0,
    CTokenAsterisk, CNodePointer, 0,
    CTokenAsterisk, CNodeTypeQualifierList, CNodePointer, 0,
    CTokenAsterisk, 0,
    0
};

ULONG YyTestCTypeQualifierList[] = {
    CNodeTypeQualifier, 0,
    CNodeTypeQualifierList, CNodeTypeQualifier, 0,
    0
};

ULONG YyTestCParameterTypeList[] = {
    CNodeParameterList, CTokenComma, CTokenEllipsis, 0,
    CNodeParameterList, 0,
    0
};

ULONG YyTestCParameterList[] = {
    CNodeParameterDeclaration, 0,
    CNodeParameterList, CTokenComma, CNodeParameterDeclaration, 0,
    0
};

ULONG YyTestCParameterDeclaration[] = {
    CNodeDeclarationSpecifiers, CNodeDeclarator, 0,
    CNodeDeclarationSpecifiers, CNodeAbstractDeclarator, 0,
    CNodeDeclarationSpecifiers, 0,
    0
};

ULONG YyTestCIdentifierList[] = {
    CTokenIdentifier, 0,
    CNodeIdentifierList, CTokenComma, CTokenIdentifier, 0,
    0
};

ULONG YyTestCTypeName[] = {
    CNodeSpecifierQualifierList, CNodeAbstractDeclarator, 0,
    CNodeSpecifierQualifierList, 0,
    0
};

ULONG YyTestCAbstractDeclarator[] = {
    CNodeDirectAbstractDeclarator, 0,
    CNodePointer, CNodeDirectAbstractDeclarator, 0,
    CNodePointer, 0,
    0
};

ULONG YyTestCDirectAbstractDeclarator[] = {
    CTokenOpenParentheses, CNodeAbstractDeclarator,
        CTokenCloseParentheses, 0,

    CTokenOpenBracket, CTokenCloseBracket, 0,
    CTokenOpenBracket, CNodeConstantExpression, CTokenCloseBracket, 0,
    CNodeDirectAbstractDeclarator, CTokenOpenBracket,
        CTokenCloseBracket, 0,

    CNodeDirectAbstractDeclarator, CTokenOpenBracket,
        CNodeConstantExpression, CTokenCloseBracket, 0,

    CTokenOpenParentheses, CTokenCloseParentheses, 0,
    CTokenOpenParentheses, CNodeParameterTypeList,
        CTokenCloseParentheses, 0,

    CNodeDirectAbstractDeclarator, CTokenOpenParentheses,
        CTokenCloseParentheses, 0,

    CNodeDirectAbstractDeclarator, CTokenOpenParentheses,
        CNodeParameterTypeList, CTokenCloseParentheses, 0,

    0
};

ULONG YyTestCInitializer[] = {
    CNodeAssignmentExpression, 0,
    CTokenOpenBrace, CNodeInitializerList, CTokenCloseBrace, 0,
    CTokenOpenBrace, CNodeInitializerList, CTokenComma,
        CTokenCloseBrace, 0,

    0
};

ULONG YyTestCInitializerList[] = {
    CNodeInitializer, 0,
    CNodeInitializerList, CTokenComma, CNodeInitializer, 0,
    0
};

ULONG YyTestCStatement[] = {
    CNodeLabeledStatement, 0,
    CNodeCompoundStatement, 0,
    CNodeExpressionStatement, 0,
    CNodeSelectionStatement, 0,
    CNodeIterationStatement, 0,
    CNodeJumpStatement, 0,
    0
};

ULONG YyTestCLabeledStatement[] = {
    CTokenIdentifier, CTokenColon, CNodeStatement, 0,
    CTokenCase, CNodeConstantExpression, CTokenColon, CNodeStatement,
        0,

    CTokenDefault, CTokenColon, CNodeStatement, 0,
    0
};

ULONG YyTestCCompoundStatement[] = {
    CTokenOpenBrace, CTokenCloseBrace, 0,
    CTokenOpenBrace, CNodeStatementList, CTokenCloseBrace, 0,
    CTokenOpenBrace, CNodeDeclarationList, CTokenCloseBrace, 0,
    CTokenOpenBrace, CNodeDeclarationList, CNodeStatementList,
        CTokenCloseBrace, 0,

    0
};

ULONG YyTestCDeclarationList[] = {
    CNodeDeclaration, 0,
    CNodeDeclarationList, CNodeDeclaration, 0,
    0
};

ULONG YyTestCStatementList[] = {
    CNodeStatement, 0,
    CNodeStatementList, CNodeStatement, 0,
    0
};

ULONG YyTestCExpressionStatement[] = {
    CTokenSemicolon, 0,
    CNodeExpression, CTokenSemicolon, 0,
    0
};

ULONG YyTestCSelectionStatement[] = {
    CTokenIf, CTokenOpenParentheses, CNodeExpression,
        CTokenCloseParentheses, CNodeStatement, CTokenElse,
        CNodeStatement, 0,

    CTokenIf, CTokenOpenParentheses, CNodeExpression,
        CTokenCloseParentheses, CNodeStatement, 0,

    CTokenSwitch, CTokenOpenParentheses, CNodeExpression,
        CTokenCloseParentheses, CNodeStatement, 0,

    0
};

ULONG YyTestCIterationStatement[] = {
    CTokenWhile, CTokenOpenParentheses, CNodeExpression,
        CTokenCloseParentheses, CNodeStatement, 0,

    CTokenDo, CNodeStatement, CTokenWhile, CTokenOpenParentheses,
        CNodeExpression, CTokenCloseParentheses, CTokenSemicolon, 0,

    CTokenFor, CTokenOpenParentheses, CNodeExpressionStatement,
        CNodeExpressionStatement, CTokenCloseParentheses,
        CNodeStatement, 0,

    CTokenFor, CTokenOpenParentheses, CNodeExpressionStatement,
        CNodeExpressionStatement, CNodeExpression,
        CTokenCloseParentheses, CNodeStatement, 0,

    0
};

ULONG YyTestCJumpStatement[] = {
    CTokenGoto, CTokenIdentifier, CTokenSemicolon, 0,
    CTokenContinue, CTokenSemicolon, 0,
    CTokenBreak, CTokenSemicolon, 0,
    CTokenReturn, CTokenSemicolon, 0,
    CTokenReturn, CNodeExpression, CTokenSemicolon, 0,
    0
};

ULONG YyTestCTranslationUnit[] = {
    CNodeExternalDeclaration, 0,
    CNodeTranslationUnit, CNodeExternalDeclaration, 0,
    0
};

ULONG YyTestCExternalDeclaration[] = {
    CNodeFunctionDefinition, 0,
    CNodeDeclaration, 0,
    0
};

ULONG YyTestCFunctionDefinition[] = {
    CNodeDeclarationSpecifiers, CNodeDeclarator, CNodeDeclarationList,
        CNodeCompoundStatement, 0,

    CNodeDeclarationSpecifiers, CNodeDeclarator,
        CNodeCompoundStatement, 0,

    CNodeDeclarator, CNodeDeclarationList, CNodeCompoundStatement, 0,
    CNodeDeclarator, CNodeCompoundStatement, 0,
    0
};

//
// Grammar specification glue
//

PARSER_GRAMMAR_ELEMENT YyTestCGrammar[] = {
    {"StringLiteral", 0, YyTestCStringLiteral},
    {"PrimaryExpression", 0, YyTestCPrimaryExpression},
    {"PostfixExpression", 0, YyTestCPostfixExpression},
    {"ArgumentExpressionList", 0, YyTestCArgumentExpressionList},
    {"UnaryExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCUnaryExpression},
    {"UnaryOperator", 0, YyTestCUnaryOperator},
    {"CastExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCCastExpression},
    {"MultiplicativeExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCMultiplicativeExpression},

    {"AdditiveExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCAdditiveExpression},
    {"ShiftExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCShiftExpression},
    {"RelationalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCRelationalExpression},

    {"EqualityExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCEqualityExpression},
    {"AndExpression", YY_GRAMMAR_COLLAPSE_ONE, YyTestCAndExpression},
    {"ExclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCExclusiveOrExpression},

    {"InclusiveOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCInclusiveOrExpression},

    {"LogicalAndExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCLogicalAndExpression},

    {"LogicalOrExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCLogicalOrExpression},

    {"ConditionalExpression",
     YY_GRAMMAR_COLLAPSE_ONE,
     YyTestCConditionalExpression},

    {"AssignmentExpression", 0, YyTestCAlignmentExpression},
    {"AssignmentOperator", 0, YyTestCAssignmentOperator},
    {"Expression", 0, YyTestCExpression},
    {"ConstantExpression", 0, YyTestCConstantExpression},
    {"Declaration", 0, YyTestCDeclaration},
    {"DeclarationSpecifiers", 0, YyTestCDeclarationSpecifiers},
    {"InitDeclaratorList", 0, YyTestCInitDeclaratorList},
    {"InitDeclarator", 0, YyTestCInitDeclarator},
    {"StorageClassSpecifier", 0, YyTestCStorageClassSpecifier},
    {"TypeSpecifier", 0, YyTestCTypeSpecifier},
    {"StructOrUnionSpecifier", 0, YyTestCStructOrUnionSpecifier},
    {"StructOrUnion", 0, YyTestCStructOrUnion},
    {"StructDeclarationList", 0, YyTestCStructDeclarationList},
    {"StructDeclaration", 0, YyTestCStructDeclaration},
    {"SpecifierQualifierList", 0, YyTestCSpecifierQualifierList},
    {"StructDeclaratorList", 0, YyTestCStructDeclaratorList},
    {"StructDeclarator", 0, YyTestCStructDeclarator},
    {"EnumSpecifier", 0, YyTestCEnumSpecifier},
    {"EnumeratorList", 0, YyTestCEnumeratorList},
    {"Enumerator", 0, YyTestCEnumerator},
    {"TypeQualifier", 0, YyTestCTypeQualifier},
    {"Declarator", 0, YyTestCDeclarator},
    {"DirectDeclarator", 0, YyTestCDirectDeclarator},
    {"Pointer", 0, YyTestCPointer},
    {"TypeQualifierList", 0, YyTestCTypeQualifierList},
    {"ParameterTypeList", 0, YyTestCParameterTypeList},
    {"ParameterList", 0, YyTestCParameterList},
    {"ParameterDeclaration", 0, YyTestCParameterDeclaration},
    {"IdentifierList", 0, YyTestCIdentifierList},
    {"TypeName", 0, YyTestCTypeName},
    {"AbstractDeclarator", 0, YyTestCAbstractDeclarator},
    {"DirectAbstractDeclarator", 0, YyTestCDirectAbstractDeclarator},
    {"Initializer", 0, YyTestCInitializer},
    {"InitializerList", 0, YyTestCInitializerList},
    {"Statement", 0, YyTestCStatement},
    {"LabeledStatement", 0, YyTestCLabeledStatement},
    {"CompoundStatement", 0, YyTestCCompoundStatement},
    {"DeclarationList", 0, YyTestCDeclarationList},
    {"StatementList", 0, YyTestCStatementList},
    {"ExpressionStatement", 0, YyTestCExpressionStatement},
    {"SelectionStatement", 0, YyTestCSelectionStatement},
    {"IterationStatement", 0, YyTestCIterationStatement},
    {"JumpStatement", 0, YyTestCJumpStatement},
    {"TranslationUnit", 0, YyTestCTranslationUnit},
    {"ExternalDeclaration", 0, YyTestCExternalDeclaration},
    {"FunctionDefinition", 0, YyTestCFunctionDefinition},
};

LIST_ENTRY YyTestTypeList;

BOOL YyTestVerbose = FALSE;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the entry point for the lexer/parser test program. It
    executes the tests.

Arguments:

    ArgumentCount - Supplies the number of arguments specified on the command
        line.

    Arguments - Supplies an array of strings representing the command line
        arguments.

Return Value:

    returns 0 on success, or nonzero on failure.

--*/

{

    PSTR Argument;
    ULONG ArgumentIndex;
    ULONG TestsFailed;

    INITIALIZE_LIST_HEAD(&YyTestTypeList);
    if (ArgumentCount < 2) {
        fprintf(stderr, "Error: Specify path of files to parse.\n");
        return 1;
    }

    srand(time(NULL));
    TestsFailed = 0;
    for (ArgumentIndex = 1; ArgumentIndex < ArgumentCount; ArgumentIndex += 1) {
        Argument = Arguments[ArgumentIndex];
        TestsFailed += YyTestParse(Argument);
        YyTestClearTypes();
    }

    if (TestsFailed != 0) {
        printf("\n*** %d failures in Parse/Lex test. ***\n", TestsFailed);
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
YyTestParse (
    PSTR Path
    )

/*++

Routine Description:

    This routine tests the parser and the lexer.

Arguments:

    Path - Supplies a pointer to the file to parse.

Return Value:

    Returns the number of test failures.

--*/

{

    ULONG Failures;
    PSTR Input;
    KSTATUS KStatus;
    LEXER Lexer;
    PARSER Parser;
    ULONG Size;
    INT Status;
    PPARSER_NODE TranslationUnit;

    Failures = 0;
    Input = NULL;
    TranslationUnit = NULL;
    memset(&Lexer, 0, sizeof(LEXER));
    memset(&Parser, 0, sizeof(PARSER));
    Status = YyTestReadFile(Path, &Input, &Size);
    if (Status != 0) {
        Failures += 1;
        goto TestParseEnd;
    }

    Lexer.Input = Input;
    Lexer.InputSize = Size;
    Lexer.Expressions = YyTestCLexerExpressions;
    Lexer.IgnoreExpressions = YyTestCLexerIgnoreExpressions;
    Lexer.ExpressionNames = YyTestCLexerTokenNames;
    Lexer.TokenBase = YY_TOKEN_BASE;
    KStatus = YyLexInitialize(&Lexer);
    if (!KSUCCESS(KStatus)) {
        Failures += 1;
        goto TestParseEnd;
    }

    Failures += YyTestLex(Path, &Lexer);
    Status = YyLexInitialize(&Lexer);
    if (!KSUCCESS(Status)) {
        Failures += 1;
        goto TestParseEnd;
    }

    Parser.Flags = 0;
    if (YyTestVerbose != FALSE) {
        Parser.Flags |= YY_PARSE_FLAG_DEBUG;
    }

    Parser.Context = &Lexer;
    Parser.Allocate = (PYY_ALLOCATE)malloc;
    Parser.Free = (PYY_FREE)free;
    Parser.GetToken = YyTestGetToken;
    Parser.NodeCallback = YyTestNodeCallback;
    Parser.Grammar = YyTestCGrammar;
    Parser.GrammarBase = CNodeStart;
    Parser.GrammarEnd = CNodeEnd;
    Parser.GrammarStart = CNodeTranslationUnit;
    Parser.Lexer = &Lexer;

    //
    // Try to parse the file, guessing at ID vs type.
    //

    KStatus = YyParse(&Parser, &TranslationUnit);
    if (!KSUCCESS(KStatus)) {
        fprintf(stderr,
                "Parser error %s:%d:%d: %d\n",
                Path,
                Parser.NextToken->Line,
                Parser.NextToken->Column,
                KStatus);

        Failures += 1;

    } else {
        if (YyTestVerbose != FALSE) {
            YyTestPrintTree(&Lexer, TranslationUnit, 0);
        }
    }

TestParseEnd:
    if (TranslationUnit != NULL) {
        YyDestroyNode(&Parser, TranslationUnit);
    }

    YyParserDestroy(&Parser);
    if (Input != NULL) {
        free(Input);
    }

    return Failures;
}

VOID
YyTestNodeCallback (
    PVOID Context,
    PPARSER_NODE Node,
    BOOL Create
    )

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

{

    PPARSER_NODE DeclarationSpecifiers;
    PPARSER_NODE Declarator;
    ULONG DeclaratorListIndex;
    PPARSER_NODE InitDeclarator;
    PPARSER_NODE InitDeclaratorList;
    BOOL IsTypedef;
    PPARSER_NODE StorageClassSpecifier;

    IsTypedef = FALSE;
    if ((Node->GrammarElement == CNodeDeclaration) &&
        (Node->NodeCount == 2)) {

        DeclarationSpecifiers = Node->Nodes[0];

        assert(DeclarationSpecifiers->GrammarElement ==
               CNodeDeclarationSpecifiers);

        if (DeclarationSpecifiers->NodeCount != 0) {
            StorageClassSpecifier = DeclarationSpecifiers->Nodes[0];
            if (StorageClassSpecifier->GrammarElement ==
                CNodeStorageClassSpecifier) {

                assert(StorageClassSpecifier->TokenCount == 1);

                if (StorageClassSpecifier->Tokens[0]->Value == CTokenTypedef) {
                    IsTypedef = TRUE;
                }
            }
        }
    }

    if (IsTypedef == FALSE) {
        return;
    }

    InitDeclaratorList = Node->Nodes[1];

    assert(InitDeclaratorList->GrammarElement == CNodeInitDeclaratorList);

    for (DeclaratorListIndex = 0;
         DeclaratorListIndex < InitDeclaratorList->NodeCount;
         DeclaratorListIndex += 1) {

        InitDeclarator = InitDeclaratorList->Nodes[DeclaratorListIndex];

        assert(InitDeclarator->GrammarElement == CNodeInitDeclarator);
        assert(InitDeclarator->NodeCount != 0);

        Declarator = InitDeclarator->Nodes[0];
        YyTestVisitDeclarator(Context, Declarator, Create);
    }

    return;
}

VOID
YyTestVisitDeclarator (
    PVOID Context,
    PPARSER_NODE Declarator,
    BOOL Create
    )

/*++

Routine Description:

    This routine visits a declarator node.

Arguments:

    Context - Supplies a context pointer initialized in the parser.

    Declarator - Supplies a pointer to the node being created or destroyed.

    Create - Supplies a boolean indicating if the node is being created (TRUE)
        or destroyed (FALSE).

Return Value:

    None.

--*/

{

    ULONG DeclaratorIndex;
    PPARSER_NODE DirectDeclarator;
    PLEXER_TOKEN Token;

    assert(Declarator->GrammarElement == CNodeDeclarator);

    for (DeclaratorIndex = 0;
         DeclaratorIndex < Declarator->NodeCount;
         DeclaratorIndex += 1) {

        DirectDeclarator = Declarator->Nodes[DeclaratorIndex];
        if (DirectDeclarator->GrammarElement != CNodeDirectDeclarator) {
            continue;
        }

        assert(DirectDeclarator->TokenCount != 0);

        Token = DirectDeclarator->Tokens[0];
        if (Token->Value == CTokenIdentifier) {
            YyTestAddType(Context, Token, Create);

        } else {

            assert(Token->Value == CTokenOpenParentheses);
            assert(DirectDeclarator->NodeCount != 0);

            YyTestVisitDeclarator(Context, DirectDeclarator->Nodes[0], Create);
        }
    }

    return;
}

VOID
YyTestAddType (
    PLEXER Lexer,
    PLEXER_TOKEN Token,
    BOOL Create
    )

/*++

Routine Description:

    This routine adds or removes a C type.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Token - Supplies a pointer to the identifier token containing the type name.

    Create - Supplies a boolean indicating if the node is being created (TRUE)
        or destroyed (FALSE).

Return Value:

    None.

--*/

{

    PC_TYPE ExistingType;
    PC_TYPE Type;

    ExistingType = YyTestFindType(Lexer, Token);
    if (ExistingType != NULL) {

        assert(Create == FALSE);

        LIST_REMOVE(&(ExistingType->ListEntry));
        free(ExistingType);

    } else {

        assert(Create != FALSE);

        Type = malloc(sizeof(C_TYPE));
        if (Type == NULL) {
            return;
        }

        memset(Type, 0, sizeof(C_TYPE));
        Type->Token = Token;
        INSERT_AFTER(&(Type->ListEntry), &YyTestTypeList);
    }

    return;
}

PC_TYPE
YyTestFindType (
    PLEXER Lexer,
    PLEXER_TOKEN Identifier
    )

/*++

Routine Description:

    This routine finds a C type.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Identifier - Supplies a pointer to the lexer token identifier.

Return Value:

    Returns a pointer to the type on success.

    NULL if no type could be found.

--*/

{

    INT Compare;
    PLIST_ENTRY CurrentEntry;
    PCSTR Input;
    PC_TYPE Type;

    Input = Lexer->Input;
    CurrentEntry = YyTestTypeList.Next;
    while (CurrentEntry != &YyTestTypeList) {
        Type = LIST_VALUE(CurrentEntry, C_TYPE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Identifier->Size != Type->Token->Size) {
            continue;
        }

        Compare = memcmp(Input + Identifier->Position,
                         Input + Type->Token->Position,
                         Identifier->Size);

        if (Compare == 0) {
            return Type;
        }
    }

    return NULL;
}

VOID
YyTestClearTypes (
    VOID
    )

/*++

Routine Description:

    This routine clears all types.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PC_TYPE Type;

    while (!LIST_EMPTY(&YyTestTypeList)) {
        Type = LIST_VALUE(YyTestTypeList.Next, C_TYPE, ListEntry);
        LIST_REMOVE(&(Type->ListEntry));
        free(Type);
    }
}

KSTATUS
YyTestGetToken (
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

        if ((Token->Value == CTokenMultilineComment) ||
            (Token->Value == CTokenComment) ||
            (Token->Value == CTokenPreprocessorDefine) ||
            (Token->Value == CTokenPreprocessorInclude) ||
            (Token->Value == CTokenPreprocessor) ||
            (Token->Value == CTokenAttribute)) {

            continue;
        }

        if (Token->Value == CTokenBuiltinVaList) {
            Token->Value = CTokenTypeName;
        }

        //
        // Determine if the identifier is a type.
        //

        if (Token->Value == CTokenIdentifier) {
            if (YyTestFindType(Lexer, Token) != NULL) {
                Token->Value = CTokenTypeName;
            }
        }

        break;
    }

    return Status;
}

ULONG
YyTestLex (
    PSTR Path,
    PLEXER Lexer
    )

/*++

Routine Description:

    This routine tests the lexer.

Arguments:

    Path - Supplies a pointer to the file path.

    Lexer - Supplies a pointer to the initialized lexer to run.

Return Value:

    Returns the number of test failures.

--*/

{

    ULONG Failures;
    PSTR Input;
    KSTATUS KStatus;
    ULONG Line;
    CHAR OriginalCharacter;
    LEXER_TOKEN Token;

    Failures = 0;
    Input = (PSTR)(Lexer->Input);
    KStatus = YyLexInitialize(Lexer);
    if (!KSUCCESS(KStatus)) {
        Failures += 1;
        goto TestLexEnd;
    }

    Line = 0;
    while (TRUE) {
        KStatus = YyLexGetToken(Lexer, &Token);
        if (KStatus == STATUS_END_OF_FILE) {
            break;

        } else if (!KSUCCESS(KStatus)) {
            fprintf(stderr,
                    "Lex failure around %s:%d:%d\n",
                    Path,
                    Lexer->Line,
                    Lexer->Column);

            Failures += 1;
            break;
        }

        if (YyTestVerbose != FALSE) {
            OriginalCharacter = Input[Token.Position + Token.Size];
            Input[Token.Position + Token.Size] = '\0';
            if (Token.Line != Line) {
                printf("\n%5d: ", Token.Line);
                Line = Token.Line;
            }

            if (strcmp(YyTestCLexerTokenNames[Token.Value - YY_TOKEN_BASE],
                       Input + Token.Position) == 0) {

                printf("\"%s\" ", Input + Token.Position);

            } else {
                printf("%s \"%s\" ",
                       YyTestCLexerTokenNames[Token.Value - YY_TOKEN_BASE],
                       Input + Token.Position);
            }

            Input[Token.Position + Token.Size] = OriginalCharacter;
        }
    }

TestLexEnd:
    return Failures;
}

INT
YyTestReadFile (
    PSTR Path,
    PSTR *FileBuffer,
    PULONG Size
    )

/*++

Routine Description:

    This routine reads a file into memory.

Arguments:

    Path - Supplies a pointer to the file to open.

    FileBuffer - Supplies a pointer where the file buffer will be returned on
        success. It is the caller's responsibility to free this buffer.

    Size - Supplies a pointer where the size of the file will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Buffer;
    FILE *File;
    ssize_t Read;
    struct stat Stat;
    INT Status;
    size_t TotalRead;

    File = NULL;
    Buffer = NULL;
    Status = stat(Path, &Stat);
    if (Status != 0) {
        fprintf(stderr, "Cannot open %s\n", Path);
        Status = errno;
        goto ReadFileEnd;
    }

    Buffer = malloc(Stat.st_size + 1);
    if (Buffer == NULL) {
        fprintf(stderr, "malloc failure.\n");
        Status = errno;
        goto ReadFileEnd;
    }

    File = fopen(Path, "r");
    if (File == NULL) {
        fprintf(stderr, "Cannot open %s\n", Path);
        Status = errno;
        goto ReadFileEnd;
    }

    TotalRead = 0;
    while (TotalRead < Stat.st_size) {
        Read = fread(Buffer + TotalRead, 1, Stat.st_size - TotalRead, File);

        //
        // Allow the file to end early due to line ending conversions.
        //

        if (Read == 0) {
            if (feof(File)) {
                Stat.st_size = TotalRead;
                break;
            }
        }

        if (Read <= 0) {
            fprintf(stderr, "Cannot read\n");
            goto ReadFileEnd;
        }

        TotalRead += Read;
    }

    Status = 0;

ReadFileEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Status != 0) {
        perror("Error");
        if (Buffer != NULL) {
            free(Buffer);
            Buffer = NULL;
        }

        Stat.st_size = 0;
    }

    *FileBuffer = Buffer;
    *Size = Stat.st_size;
    return Status;
}

VOID
YyTestPrintTree (
    PLEXER Lexer,
    PPARSER_NODE Node,
    ULONG RecursionDepth
    )

/*++

Routine Description:

    This routine recursively prints a tree node.

Arguments:

    Lexer - Supplies a pointer to the lexer.

    Node - Supplies a pointer to the node to print.

    RecursionDepth - Supplies the recursion depth.

Return Value:

    None.

--*/

{

    PPARSER_GRAMMAR_ELEMENT Grammar;
    ULONG Index;
    PSTR Input;
    UCHAR Original;
    PLEXER_TOKEN Token;

    Grammar = &(YyTestCGrammar[Node->GrammarElement - CNodeStart]);
    printf("%*s%s\n", RecursionDepth, "", Grammar->Name);
    for (Index = 0; Index < Node->TokenCount; Index += 1) {
        Token = Node->Tokens[Index];
        Input = (PSTR)(Lexer->Input + Token->Position);
        Original = Input[Token->Size];
        Input[Token->Size] = '\0';
        printf("%*s%s (%d:%d)\n",
               RecursionDepth + 1,
               "",
               Input,
               Token->Line,
               Token->Column);

        Input[Token->Size] = Original;
    }

    for (Index = 0; Index < Node->NodeCount; Index += 1) {
        YyTestPrintTree(Lexer, Node->Nodes[Index], RecursionDepth + 1);
    }

    return;
}

