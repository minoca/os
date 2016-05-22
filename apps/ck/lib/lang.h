/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    lang.h

Abstract:

    This header contains language definitions for the Chalk scripting language.

Author:

    Evan Green 8-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CK_SYMBOL {
    CkTokenEndOfFile,
    CkTokenError,
    CkTokenBreak,
    CkTokenContinue,
    CkTokenDo,
    CkTokenElse,
    CkTokenFor,
    CkTokenIf,
    CkTokenReturn,
    CkTokenWhile,
    CkTokenFunction,
    CkTokenIn,
    CkTokenNull,
    CkTokenTrue,
    CkTokenFalse,
    CkTokenVariable,
    CkTokenClass,
    CkTokenIs,
    CkTokenStatic,
    CkTokenSuper,
    CkTokenThis,
    CkTokenImport,
    CkTokenFrom,
    CkTokenIdentifier,
    CkTokenConstant,
    CkTokenString,
    CkTokenRightAssign,
    CkTokenLeftAssign,
    CkTokenAddAssign,
    CkTokenSubtractAssign,
    CkTokenMultiplyAssign,
    CkTokenDivideAssign,
    CkTokenModuloAssign,
    CkTokenAndAssign,
    CkTokenXorAssign,
    CkTokenOrAssign,
    CkTokenNullAssign,
    CkTokenRightShift,
    CkTokenLeftShift,
    CkTokenIncrement,
    CkTokenDecrement,
    CkTokenLogicalAnd,
    CkTokenLogicalOr,
    CkTokenLessOrEqual,
    CkTokenGreaterOrEqual,
    CkTokenIsEqual,
    CkTokenIsNotEqual,
    CkTokenSemicolon,
    CkTokenOpenBrace,
    CkTokenCloseBrace,
    CkTokenComma,
    CkTokenColon,
    CkTokenAssign,
    CkTokenOpenParentheses,
    CkTokenCloseParentheses,
    CkTokenOpenBracket,
    CkTokenCloseBracket,
    CkTokenBitAnd,
    CkTokenLogicalNot,
    CkTokenBitNot,
    CkTokenMinus,
    CkTokenPlus,
    CkTokenAsterisk,
    CkTokenDivide,
    CkTokenModulo,
    CkTokenLessThan,
    CkTokenGreaterThan,
    CkTokenXor,
    CkTokenBitOr,
    CkTokenQuestion,
    CkTokenDot,
    CkTokenDotDot,
    CkTokenDotDotDot,

    CkNodeStart,
    CkNodeListElementList,
    CkNodeList,
    CkNodeDictElement,
    CkNodeDictElementList,
    CkNodeDict,
    CkNodePrimaryExpression,
    CkNodePostfixExpression,
    CkNodeArgumentExpressionList,
    CkNodeUnaryExpression,
    CkNodeUnaryOperator,
    CkNodeMultiplicativeExpression,
    CkNodeAdditiveExpression,
    CkNodeRangeExpression,
    CkNodeShiftExpression,
    CkNodeAndExpression,
    CkNodeExclusiveOrExpression,
    CkNodeInclusiveOrExpression,
    CkNodeRelationalExpression,
    CkNodeEqualityExpression,
    CkNodeLogicalAndExpression,
    CkNodeLogicalOrExpression,
    CkNodeConditionalExpression,
    CkNodeAssignmentExpression,
    CkNodeAssignmentOperator,
    CkNodeExpression,
    CkNodeVariableSpecifier,
    CkNodeVariableDeclaration,
    CkNodeVariableDefinition,
    CkNodeStatement,
    CkNodeCompoundStatement,
    CkNodeStatementList,
    CkNodeExpressionStatement,
    CkNodeSelectionStatement,
    CkNodeIterationStatement,
    CkNodeJumpStatement,
    CkNodeIdentifierList,
    CkNodeFunctionDefinition,
    CkNodeClassMember,
    CkNodeClassMemberList,
    CkNodeClassBody,
    CkNodeClassDefinition,
    CkNodeModuleName,
    CkNodeImportStatement,
    CkNodeExternalDeclaration,
    CkNodeTranslationUnit,
    CkSymbolCount
} CK_SYMBOL, *PCK_SYMBOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
