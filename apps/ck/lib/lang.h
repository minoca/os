/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    CkTokenMultilineComment,
    CkTokenSingleLineComment,
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
    CkTokenTry,
    CkTokenExcept,
    CkTokenAs,
    CkTokenFinally,
    CkTokenIdentifier,
    CkTokenConstant,
    CkTokenHexConstant,
    CkTokenBinaryConstant,
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
    CkNodeStringLiteralList,
    CkNodePrimaryExpression,
    CkNodePostfixExpression,
    CkNodeArgumentExpressionList,
    CkNodeUnaryExpression,
    CkNodeUnaryOperator,
    CkNodeBinaryExpression,
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
    CkNodeTryEnding,
    CkNodeExceptStatement,
    CkNodeExceptStatementList,
    CkNodeTryStatement,
    CkNodeIdentifierList,
    CkNodeFunctionDefinition,
    CkNodeFunctionDeclaration,
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

/*++

Structure Description:

    This structure defines an Abstract Syntax Tree node of the Chalk grammar.

Members:

    Symbol - Stores the Chalk symbol. This will always be greater than
        the max token.

    Descendents - Stores the total number of child nodes in this node.

    Depth - Stores the maximum depth under this tree.

    ChildIndex - Stores the index into the giant array of nodes where the
        children of this node reside.

    Parent - Stores the index of the parent node.

    Line - Stores the line number the node starts on.

--*/

typedef struct _CK_AST_NODE {
    CK_SYMBOL Symbol;
    ULONG Children;
    ULONG Descendants;
    ULONG Depth;
    ULONG ChildIndex;
    ULONG Parent;
    ULONG Line;
} CK_AST_NODE, *PCK_AST_NODE;

/*++

Union Description:

    This union defines the storage size needed to hold a lexer token or an
    AST node.

Members:

    Symbol - Stores the Chalk symbol. Whether this symbol is a token or
        non-terminal defines which of the members contains the complete
        information.

    Token - Stores the token form of the element.

    Node - Stores the node form of the element.

--*/

typedef union _CK_SYMBOL_UNION {
    CK_SYMBOL Symbol;
    LEXER_TOKEN Token;
    CK_AST_NODE Node;
} CK_SYMBOL_UNION, *PCK_SYMBOL_UNION;

/*++

Structure Description:

    This structure defines the context for parsing the Chalk grammar.

Members:

    Vm - Stores a pointer to the virtual machine.

    Module - Stores a pointer to the module the source is being compiled into.

    Source - Stores a pointer to the input source being compiled.

    SourceLength - Stores the length of the source file in bytes, not including
        the null terminator.

    Nodes - Stores a pointer to the flat abstract syntax tree nodes.

    NodeCount - Stores the number of valid elements in the node array.

    NodeCapacity - Stores the maximum number of nodes that can be stored before
        the nodes array will need to be reallocated.

    Lexer - Stores the lexer context.

    TokenPosition - Stores the position of the last token successfully read.

    TokenSize - Stores the size of the last token successfully read.

    Line - Stores the line number the last token successfully read was on.

    PreviousPosition - Stores the position of the token before the last one
        previously read.

    PreviousSize - Stores the size of the token before the last one
        successfully read.

    PreviousLine - Stores the line number of the previous token.

    Parser - Stores the grammar parser context.

    Errors - Stores the number of errors that have occurred.

    PrintErrors - Stores a boolean indicating whether or not to print errors.

--*/

typedef struct _CK_PARSER {
    PCK_VM Vm;
    PCK_MODULE Module;
    PCSTR Source;
    UINTN SourceLength;
    PCK_SYMBOL_UNION Nodes;
    UINTN NodeCount;
    UINTN NodeCapacity;
    LEXER Lexer;
    UINTN TokenPosition;
    UINTN TokenSize;
    ULONG Line;
    UINTN PreviousPosition;
    ULONG PreviousSize;
    ULONG PreviousLine;
    YY_PARSER Parser;
    ULONG Errors;
    BOOL PrintErrors;
} CK_PARSER, *PCK_PARSER;

//
// -------------------------------------------------------------------- Globals
//

//
// The grammar tables exist in a source file that is generated by the yygen
// library, an LALR(1) parser generator.
//

extern YY_GRAMMAR CkGrammar;

//
// -------------------------------------------------------- Function Prototypes
//
