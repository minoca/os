/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    gramgen.c

Abstract:

    This module implements the Chalk grammar generator program, whose input
    is a set of grammar rules and whose output is a C source file containing
    the LALR(1) grammar state table.

Author:

    Evan Green 8-May-2016

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../chalkp.h"

#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include <stdio.h>
#include <minoca/lib/yygen.h>
#include "../lang.h"

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define CKG_GRAMMAR_GEN_USAGE \
    "Usage: gramgen [options] output\n"                                        \
    "This program generates the Chalk grammar data source file. Options are:\n"\
    "  -d, --debug -- Enables debug information.\n"                            \
    "  -v, --verbose -- Enable a verbose file output at <output>.out.\n"       \
    "  -h, --help -- Prints this help.\n"                                      \

#define CKG_GRAMMAR_GEN_OPTIONS "dhv"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

YY_VALUE CkgListElementList[] = {
    CkNodeConditionalExpression, -1,
    CkNodeListElementList, CkTokenComma, CkNodeConditionalExpression, -1,
    0
};

YY_VALUE CkgList[] = {
    CkTokenOpenBracket, CkTokenCloseBracket, -1,
    CkTokenOpenBracket, CkNodeListElementList, CkTokenCloseBracket, -1,
    CkTokenOpenBracket, CkNodeListElementList, CkTokenComma,
        CkTokenCloseBracket, -1,

    0
};

YY_VALUE CkgDictElement[] = {
    CkNodeExpression, CkTokenColon, CkNodeConditionalExpression, -1,
    0
};

YY_VALUE CkgDictElementList[] = {
    CkNodeDictElement, -1,
    CkNodeDictElementList, CkTokenComma, CkNodeDictElement, -1,
    0
};

YY_VALUE CkgDict[] = {
    CkTokenOpenBrace, CkTokenCloseBrace, -1,
    CkTokenOpenBrace, CkNodeDictElementList, CkTokenCloseBrace, -1,
    CkTokenOpenBrace, CkNodeDictElementList, CkTokenComma,
        CkTokenCloseBrace, -1,

    0
};

YY_VALUE CkgPrimaryExpression[] = {
    CkTokenIdentifier, -1,
    CkTokenConstant, -1,
    CkTokenString, -1,
    CkTokenNull, -1,
    CkTokenThis, -1,
    CkTokenSuper, -1,
    CkTokenTrue, -1,
    CkTokenFalse, -1,
    CkNodeDict, -1,
    CkNodeList, -1,
    CkTokenOpenParentheses, CkNodeExpression, CkTokenCloseParentheses, -1,
    0
};

YY_VALUE CkgPostfixExpression[] = {
    CkNodePrimaryExpression, -1,
    CkNodePostfixExpression, CkTokenDot, CkTokenIdentifier,
        CkTokenOpenParentheses, CkNodeArgumentExpressionList,
        CkTokenCloseParentheses, -1,

    CkNodePostfixExpression, CkTokenOpenBracket, CkNodeExpression,
        CkTokenCloseBracket, -1,

    CkNodePostfixExpression, CkTokenOpenParentheses,
        CkNodeArgumentExpressionList, CkTokenCloseParentheses, -1,

    CkNodePostfixExpression, CkTokenIncrement, -1,
    CkNodePostfixExpression, CkTokenDecrement, -1,
    0
};

YY_VALUE CkgArgumentExpressionList[] = {
    CkNodeAssignmentExpression, -1,
    CkNodeArgumentExpressionList, CkTokenComma,
        CkNodeAssignmentExpression, -1,

    -1,
    0
};

YY_VALUE CkgUnaryExpression[] = {
    CkNodePostfixExpression, -1,
    CkTokenIncrement, CkNodeUnaryExpression, -1,
    CkTokenDecrement, CkNodeUnaryExpression, -1,
    CkNodeUnaryOperator, CkNodeUnaryExpression, -1,
    0
};

YY_VALUE CkgUnaryOperator[] = {
    CkTokenMinus, -1,
    CkTokenBitNot, -1,
    CkTokenLogicalNot, -1,
    0
};

YY_VALUE CkgMultiplicativeExpression[] = {
    CkNodeUnaryExpression, -1,
    CkNodeMultiplicativeExpression, CkTokenAsterisk,
        CkNodeUnaryExpression, -1,

    CkNodeMultiplicativeExpression, CkTokenDivide,
        CkNodeUnaryExpression, -1,

    CkNodeMultiplicativeExpression, CkTokenModulo,
        CkNodeUnaryExpression, -1,

    0
};

YY_VALUE CkgAdditiveExpression[] = {
    CkNodeMultiplicativeExpression, -1,
    CkNodeAdditiveExpression, CkTokenPlus,
        CkNodeMultiplicativeExpression, -1,

    CkNodeAdditiveExpression, CkTokenMinus,
        CkNodeMultiplicativeExpression, -1,

    0
};

YY_VALUE CkgRangeExpression[] = {
    CkNodeAdditiveExpression, -1,
    CkNodeRangeExpression, CkTokenDotDot, CkNodeAdditiveExpression, -1,
    CkNodeRangeExpression, CkTokenDotDotDot, CkNodeAdditiveExpression, -1,
    0
};

YY_VALUE CkgShiftExpression[] = {
    CkNodeRangeExpression, -1,
    CkNodeShiftExpression, CkTokenLeftShift, CkNodeRangeExpression, -1,
    CkNodeShiftExpression, CkTokenRightShift, CkNodeRangeExpression, -1,
    0
};

YY_VALUE CkgAndExpression[] = {
    CkNodeShiftExpression, -1,
    CkNodeAndExpression, CkTokenBitAnd, CkNodeShiftExpression, -1,
    0
};

YY_VALUE CkgExclusiveOrExpression[] = {
    CkNodeAndExpression, -1,
    CkNodeExclusiveOrExpression, CkTokenXor, CkNodeAndExpression, -1,
    0
};

YY_VALUE CkgInclusiveOrExpression[] = {
    CkNodeExclusiveOrExpression, -1,
    CkNodeInclusiveOrExpression, CkTokenBitOr, CkNodeExclusiveOrExpression, -1,
    0
};

YY_VALUE CkgRelationalExpression[] = {
    CkNodeInclusiveOrExpression, -1,
    CkNodeRelationalExpression, CkTokenLessThan,
        CkNodeInclusiveOrExpression, -1,

    CkNodeRelationalExpression, CkTokenGreaterThan,
        CkNodeInclusiveOrExpression, -1,

    CkNodeRelationalExpression, CkTokenLessOrEqual,
        CkNodeInclusiveOrExpression, -1,

    CkNodeRelationalExpression, CkTokenGreaterOrEqual,
        CkNodeInclusiveOrExpression, -1,

    0
};

YY_VALUE CkgEqualityExpression[] = {
    CkNodeRelationalExpression, -1,
    CkNodeEqualityExpression, CkTokenIs, CkNodeRelationalExpression, -1,
    CkNodeEqualityExpression, CkTokenIsEqual, CkNodeRelationalExpression, -1,
    CkNodeEqualityExpression, CkTokenIsNotEqual, CkNodeRelationalExpression, -1,
    0
};

YY_VALUE CkgLogicalAndExpression[] = {
    CkNodeEqualityExpression, -1,
    CkNodeLogicalAndExpression, CkTokenLogicalAnd, CkNodeEqualityExpression, -1,
    0
};

YY_VALUE CkgLogicalOrExpression[] = {
    CkNodeLogicalAndExpression, -1,
    CkNodeLogicalOrExpression, CkTokenLogicalOr, CkNodeLogicalAndExpression, -1,
    0
};

YY_VALUE CkgConditionalExpression[] = {
    CkNodeLogicalOrExpression, -1,
    CkNodeLogicalOrExpression, CkTokenQuestion, CkNodeExpression,
        CkTokenColon, CkNodeConditionalExpression, -1,

    0
};

YY_VALUE CkgAssignmentExpression[] = {
    CkNodeConditionalExpression, -1,
    CkNodeUnaryExpression, CkNodeAssignmentOperator,
        CkNodeAssignmentExpression, -1,

    0
};

YY_VALUE CkgAssignmentOperator[] = {
    CkTokenAssign, -1,
    CkTokenMultiplyAssign, -1,
    CkTokenDivideAssign, -1,
    CkTokenModuloAssign, -1,
    CkTokenAddAssign, -1,
    CkTokenSubtractAssign, -1,
    CkTokenLeftAssign, -1,
    CkTokenRightAssign, -1,
    CkTokenAndAssign, -1,
    CkTokenXorAssign, -1,
    CkTokenOrAssign, -1,
    CkTokenNullAssign, -1,
    0
};

YY_VALUE CkgExpression[] = {
    CkNodeAssignmentExpression, -1,
    CkNodeExpression, CkTokenComma, CkNodeAssignmentExpression, -1,
    0
};

YY_VALUE CkgVariableSpecifier[] = {
    CkTokenStatic, CkTokenVariable, CkTokenIdentifier, -1,
    CkTokenVariable, CkTokenIdentifier, -1,
    0
};

YY_VALUE CkgVariableDeclaration[] = {
    CkNodeVariableSpecifier, CkTokenSemicolon, -1,
    0
};

YY_VALUE CkgVariableDefinition[] = {
    CkNodeVariableDeclaration, -1,
    CkNodeVariableSpecifier, CkTokenAssign, CkNodeExpression,
        CkTokenSemicolon, -1,

    0
};

YY_VALUE CkgStatement[] = {
    CkNodeFunctionDefinition, -1,
    CkNodeVariableDefinition, -1,
    CkNodeExpressionStatement, -1,
    CkNodeSelectionStatement, -1,
    CkNodeIterationStatement, -1,
    CkNodeJumpStatement, -1,
    0
};

YY_VALUE CkgCompoundStatement[] = {
    CkTokenOpenBrace, CkTokenCloseBrace, -1,
    CkTokenOpenBrace, CkNodeStatementList, CkTokenCloseBrace, -1,
    0
};

YY_VALUE CkgStatementList[] = {
    CkNodeStatement, -1,
    CkNodeStatementList, CkNodeStatement, -1,
    0
};

YY_VALUE CkgExpressionStatement[] = {
    CkTokenSemicolon, -1,
    CkNodeExpression, CkTokenSemicolon, -1,
    0
};

YY_VALUE CkgSelectionStatement[] = {
    CkTokenIf, CkTokenOpenParentheses, CkNodeExpression,
        CkTokenCloseParentheses, CkNodeCompoundStatement, CkTokenElse,
        CkNodeSelectionStatement, -1,

    CkTokenIf, CkTokenOpenParentheses, CkNodeExpression,
        CkTokenCloseParentheses, CkNodeCompoundStatement, CkTokenElse,
        CkNodeCompoundStatement, -1,

    CkTokenIf, CkTokenOpenParentheses, CkNodeExpression,
        CkTokenCloseParentheses, CkNodeCompoundStatement, -1,

    0
};

YY_VALUE CkgIterationStatement[] = {
    CkTokenWhile, CkTokenOpenParentheses, CkNodeExpression,
        CkTokenCloseParentheses, CkNodeCompoundStatement, -1,

    CkTokenDo, CkNodeCompoundStatement, CkTokenWhile,
        CkTokenOpenParentheses, CkNodeExpression,
        CkTokenCloseParentheses, CkTokenSemicolon, -1,

    CkTokenFor, CkTokenOpenParentheses, CkTokenIdentifier,
        CkTokenIn, CkNodeExpression, CkTokenCloseParentheses,
        CkNodeCompoundStatement, -1,

    CkTokenFor, CkTokenOpenParentheses, CkNodeStatement,
        CkNodeExpression, CkTokenSemicolon, CkTokenCloseParentheses,
        CkNodeCompoundStatement, -1,

    CkTokenFor, CkTokenOpenParentheses, CkNodeStatement,
        CkNodeExpression, CkTokenSemicolon, CkNodeExpression,
        CkTokenCloseParentheses, CkNodeCompoundStatement, -1,

    0
};

YY_VALUE CkgJumpStatement[] = {
    CkTokenContinue, CkTokenSemicolon, -1,
    CkTokenBreak, CkTokenSemicolon, -1,
    CkTokenReturn, CkTokenSemicolon, -1,
    CkTokenReturn, CkNodeExpression, CkTokenSemicolon, -1,
    0
};

YY_VALUE CkgIdentifierList[] = {
    CkTokenIdentifier, -1,
    CkNodeIdentifierList, CkTokenComma, CkTokenIdentifier, -1,
    -1,
    0
};

YY_VALUE CkgFunctionDefinition[] = {
    CkTokenFunction, CkTokenIdentifier, CkTokenOpenParentheses,
        CkNodeIdentifierList, CkTokenCloseParentheses,
        CkNodeCompoundStatement, -1,

    CkTokenStatic, CkTokenFunction, CkTokenIdentifier, CkTokenOpenParentheses,
        CkNodeIdentifierList, CkTokenCloseParentheses,
        CkNodeCompoundStatement, -1,

    0
};

YY_VALUE CkgClassMember[] = {
    CkNodeFunctionDefinition, -1,
    CkNodeVariableDeclaration, -1,
    0
};

YY_VALUE CkgClassMemberList[] = {
    CkNodeClassMember, -1,
    CkNodeClassMemberList, CkNodeClassMember, -1,
    0
};

YY_VALUE CkgClassBody[] = {
    CkTokenOpenBrace, CkTokenCloseBrace, -1,
    CkTokenOpenBrace, CkNodeClassMemberList, CkTokenCloseBrace, -1,
    0
};

YY_VALUE CkgClassDefinition[] = {
    CkTokenClass, CkTokenIdentifier, CkNodeClassBody, -1,
    CkTokenClass, CkTokenIdentifier, CkTokenIs, CkNodeExpression,
        CkNodeClassBody, -1,

    0,
};

YY_VALUE CkgModuleName[] = {
    CkTokenIdentifier, -1,
    CkNodeModuleName, CkTokenDot, CkTokenIdentifier, -1,
    0
};

YY_VALUE CkgImportStatement[] = {
    CkTokenImport, CkNodeModuleName, CkTokenSemicolon, -1,
    CkTokenFrom, CkNodeModuleName, CkTokenImport, CkNodeIdentifierList,
        CkTokenSemicolon, -1,

    CkTokenFrom, CkNodeModuleName, CkTokenImport, CkTokenAsterisk,
        CkTokenSemicolon, -1,

    0
};

YY_VALUE CkgExternalDeclaration[] = {
    CkNodeClassDefinition, -1,
    CkNodeImportStatement, -1,
    CkNodeStatement, -1,
    0
};

YY_VALUE CkgTranslationUnit[] = {
    CkNodeExternalDeclaration, -1,
    CkNodeTranslationUnit, CkNodeExternalDeclaration, -1,
    0
};

YY_ELEMENT CkgGrammarElements[CkSymbolCount] = {
    {"EndOfFile", 0, 0, NULL},
    {"Error", 0, 0, NULL},
    {"MultiComment", 0, 0, NULL},
    {"Comment", 0, 0, NULL},
    {"break", 0, 0, NULL},
    {"continue", 0, 0, NULL},
    {"do", 0, 0, NULL},
    {"else", 0, 0, NULL},
    {"for", 0, 0, NULL},
    {"if", 0, 0, NULL},
    {"return", 0, 0, NULL},
    {"while", 0, 0, NULL},
    {"function", 0, 0, NULL},
    {"in", 0, 0, NULL},
    {"null", 0, 0, NULL},
    {"true", 0, 0, NULL},
    {"false", 0, 0, NULL},
    {"var", 0, 0, NULL},
    {"class", 0, 0, NULL},
    {"is", 0, 0, NULL},
    {"static", 0, 0, NULL},
    {"super", 0, 0, NULL},
    {"this", 0, 0, NULL},
    {"import", 0, 0, NULL},
    {"from", 0, 0, NULL},
    {"Identifier", 0, 0, NULL},
    {"Constant", 0, 0, NULL},
    {"String", 0, 0, NULL},
    {">>=", 0, 0, NULL},
    {"<<=", 0, 0, NULL},
    {"+=", 0, 0, NULL},
    {"-=", 0, 0, NULL},
    {"*=", 0, 0, NULL},
    {"/=", 0, 0, NULL},
    {"%=", 0, 0, NULL},
    {"&=", 0, 0, NULL},
    {"^=", 0, 0, NULL},
    {"|=", 0, 0, NULL},
    {"?=", 0, 0, NULL},
    {">>", 0, 0, NULL},
    {"<<", 0, 0, NULL},
    {"++", 0, 0, NULL},
    {"--", 0, 0, NULL},
    {"&&", 0, 0, NULL},
    {"||", 0, 0, NULL},
    {"<=", 0, 0, NULL},
    {">=", 0, 0, NULL},
    {"==", 0, 0, NULL},
    {"!=", 0, 0, NULL},
    {";", 0, 0, NULL},
    {"{", 0, 0, NULL},
    {"}", 0, 0, NULL},
    {",", 0, 0, NULL},
    {":", 0, 0, NULL},
    {"=", 0, 0, NULL},
    {"(", 0, 0, NULL},
    {")", 0, 0, NULL},
    {"[", 0, 0, NULL},
    {"]", 0, 0, NULL},
    {"&", 0, 0, NULL},
    {"!", 0, 0, NULL},
    {"~", 0, 0, NULL},
    {"-", 0, 0, NULL},
    {"+", 0, 0, NULL},
    {"*", 0, 0, NULL},
    {"/", 0, 0, NULL},
    {"%", 0, 0, NULL},
    {"<", 0, 0, NULL},
    {">", 0, 0, NULL},
    {"^", 0, 0, NULL},
    {"|", 0, 0, NULL},
    {"?", 0, 0, NULL},
    {".", 0, 0, NULL},
    {"..", 0, 0, NULL},
    {"...", 0, 0, NULL},
    {"Start", 0, 0, NULL},

    {"ListElementList", 0, 0, CkgListElementList},
    {"List", 0, 0, CkgList},
    {"DictElement", 0, 0, CkgDictElement},
    {"DictElementList", 0, 0, CkgDictElementList},
    {"Dict", 0, 0, CkgDict},
    {"PrimaryExpression", 0, 0, CkgPrimaryExpression},
    {"PostfixExpression", 0, 0, CkgPostfixExpression},
    {"ArgumentExpressionList", 0, 0, CkgArgumentExpressionList},
    {"UnaryExpression", 0, 0, CkgUnaryExpression},
    {"UnaryOperator", 0, 0, CkgUnaryOperator},
    {"MultiplicativeExpression", 0, 0, CkgMultiplicativeExpression},
    {"AdditiveExpression", 0, 0, CkgAdditiveExpression},
    {"RangeExpression", 0, 0, CkgRangeExpression},
    {"ShiftExpression", 0, 0, CkgShiftExpression},
    {"AndExpression", 0, 0, CkgAndExpression},
    {"ExclusiveOrExpression", 0, 0, CkgExclusiveOrExpression},
    {"InclusiveOrExpression", 0, 0, CkgInclusiveOrExpression},
    {"RelationalExpression", 0, 0, CkgRelationalExpression},
    {"EqualityExpression", 0, 0, CkgEqualityExpression},
    {"LogicalAndExpression", 0, 0, CkgLogicalAndExpression},
    {"LogicalOrExpression", 0, 0, CkgLogicalOrExpression},
    {"ConditionalExpression", 0, 0, CkgConditionalExpression},
    {"AssignmentExpression", 0, 0, CkgAssignmentExpression},
    {"AssignmentOperator", 0, 0, CkgAssignmentOperator},
    {"Expression", 0, 0, CkgExpression},
    {"VariableSpecifier", 0, 0, CkgVariableSpecifier},
    {"VariableDeclaration", 0, 0, CkgVariableDeclaration},
    {"VariableDefinition", 0, 0, CkgVariableDefinition},
    {"Statement", 0, 0, CkgStatement},
    {"CompoundStatement", 0, 0, CkgCompoundStatement},
    {"StatementList", 0, 0, CkgStatementList},
    {"ExpressionStatement", 0, 0, CkgExpressionStatement},
    {"SelectionStatement", 0, 0, CkgSelectionStatement},
    {"IterationStatement", 0, 0, CkgIterationStatement},
    {"JumpStatement", 0, 0, CkgJumpStatement},
    {"IdentifierList", 0, 0, CkgIdentifierList},
    {"FunctionDefinition", 0, 0, CkgFunctionDefinition},
    {"ClassMember", 0, 0, CkgClassMember},
    {"ClassMemberList", 0, 0, CkgClassMemberList},
    {"ClassBody", 0, 0, CkgClassBody},
    {"ClassDefinition", 0, 0, CkgClassDefinition},
    {"ModuleName", 0, 0, CkgModuleName},
    {"ImportStatement", 0, 0, CkgImportStatement},
    {"ExternalDeclaration", 0, 0, CkgExternalDeclaration},
    {"TranslationUnit", YY_ELEMENT_START, 0, CkgTranslationUnit}
};

YY_GRAMMAR_DESCRIPTION CkgGrammarDescription = {
    CkgGrammarElements,
    CkNodeStart,
    CkSymbolCount,
    0,
    0,
    "Ck",
    NULL
};

struct option CkgGrammarGenLongOptions[] = {
    {"debug", no_argument, 0, 'd'},
    {"help", no_argument, 0, 'h'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

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

    This routine implements the main entry point for the Chalk grammar
    generator program. It generates the C source file containing the Chalk
    grammar state machine.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT ArgumentIndex;
    PYYGEN_CONTEXT Context;
    PSTR FileName;
    ULONG Flags;
    INT Option;
    FILE *OutputFile;
    PSTR OutputPath;
    INT OutputPathLength;
    INT Status;
    BOOL Verbose;
    FILE *VerboseFile;
    PSTR VerbosePath;
    YY_STATUS YyStatus;

    Context = NULL;
    FileName = NULL;
    Flags = 0;
    Verbose = FALSE;
    VerboseFile = NULL;
    Status = 1;
    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CKG_GRAMMAR_GEN_OPTIONS,
                             CkgGrammarGenLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            goto MainEnd;
        }

        switch (Option) {
        case 'd':
            Flags |= YYGEN_FLAG_DEBUG;
            break;

        case 'v':
            Verbose = TRUE;
            break;

        case 'h':
            printf(CKG_GRAMMAR_GEN_USAGE);
            return 1;

        default:

            assert(FALSE);

            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentCount - ArgumentIndex != 1) {
        fprintf(stderr, "Error: Expected output file argument.\n");
        goto MainEnd;
    }

    OutputPath = Arguments[ArgumentIndex];
    FileName = basename(OutputPath);
    if (FileName == NULL) {
        goto MainEnd;
    }

    FileName = strdup(FileName);
    if (FileName == NULL) {
        goto MainEnd;
    }

    CkgGrammarDescription.OutputFileName = FileName;
    YyStatus = YyGenerateGrammar(&CkgGrammarDescription, Flags, &Context);
    if (YyStatus != YyStatusSuccess) {
        fprintf(stderr, "Error: Failed to generate grammar: %d\n", YyStatus);
        goto MainEnd;
    }

    if (Verbose != FALSE) {
        OutputPathLength = strlen(OutputPath);
        VerbosePath = malloc(OutputPathLength + 5);
        if (VerbosePath == NULL) {
            goto MainEnd;
        }

        snprintf(VerbosePath, OutputPathLength + 5, "%s.out", OutputPath);
        VerboseFile = fopen(VerbosePath, "w+");
        if (VerboseFile == NULL) {
            fprintf(stderr,
                    "Failed to open %s: %s.\n",
                    VerbosePath,
                    strerror(errno));

            free(VerbosePath);
            goto MainEnd;
        }

        free(VerbosePath);
        YyPrintParserState(Context, VerboseFile);
        fclose(VerboseFile);
    }

    OutputFile = fopen(OutputPath, "w+");
    if (OutputFile == NULL) {
        fprintf(stderr,
                "Failed to open %s: %s.\n",
                OutputPath,
                strerror(errno));

        goto MainEnd;
    }

    YyStatus = YyOutputParserSource(Context, OutputFile);
    fclose(OutputFile);
    if (YyStatus != YyStatusSuccess) {
        fprintf(stderr, "Error: Failed to generate output: %d.\n", YyStatus);
        goto MainEnd;
    }

    Status = 0;

MainEnd:
    if (Context != NULL) {
        YyDestroyGeneratorContext(Context);
    }

    if (FileName != NULL) {
        free(FileName);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

