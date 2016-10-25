/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    arith.c

Abstract:

    This module implements arithmetic expansion for the sh shell.

Author:

    Evan Green 7-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "sh.h"
#include "shparse.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "../swlib.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro takes in a token type and returns non-zero if it is an assignment
// operator.
//

#define SHELL_ARITHMETIC_ASSIGN_OPERATOR(_TokenType)          \
    (((_TokenType) == SHELL_ARITHMETIC_MULTIPLY_ASSIGN) ||    \
     ((_TokenType) == SHELL_ARITHMETIC_DIVIDE_ASSIGN) ||      \
     ((_TokenType) == SHELL_ARITHMETIC_MODULO_ASSIGN) ||      \
     ((_TokenType) == SHELL_ARITHMETIC_ADD_ASSIGN) ||         \
     ((_TokenType) == SHELL_ARITHMETIC_SUBTRACT_ASSIGN) ||    \
     ((_TokenType) == SHELL_ARITHMETIC_LEFT_SHIFT_ASSIGN) ||  \
     ((_TokenType) == SHELL_ARITHMETIC_RIGHT_SHIFT_ASSIGN) || \
     ((_TokenType) == SHELL_ARITHMETIC_AND_ASSIGN) ||         \
     ((_TokenType) == SHELL_ARITHMETIC_OR_ASSIGN) ||          \
     ((_TokenType) == SHELL_ARITHMETIC_XOR_ASSIGN) ||         \
     ((_TokenType) == '='))

//
// This macro takes a token type and evaluates to non-zero if it could be a
// unary operator.
//

#define SHELL_ARITHMETIC_UNARY_OPERATOR(_TokenType)    \
    (((_TokenType) == '-') || ((_TokenType) == '+') || \
     ((_TokenType) == '~') || ((_TokenType) == '!'))   \

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the string buffer needed to convert an integer to a
// string.
//

#define SHELL_ARITHMETIC_INTEGER_STRING_BUFFER_SIZE 12

//
// Define the initial size of the arithmetic lexer's token buffer.
//

#define SHELL_ARITHMETIC_INITIAL_TOKEN_BUFFER_SIZE 256

//
// Define arithmetic lexer tokens.
//

#define SHELL_ARITHMETIC_END_OF_FILE           0
#define SHELL_ARITHMETIC_WORD                  600
#define SHELL_ARITHMETIC_NUMBER                601
#define SHELL_ARITHMETIC_SHIFT_LEFT            602
#define SHELL_ARITHMETIC_SHIFT_RIGHT           603
#define SHELL_ARITHMETIC_LESS_THAN_OR_EQUAL    604
#define SHELL_ARITHMETIC_GREATER_THAN_OR_EQUAL 605
#define SHELL_ARITHMETIC_EQUALITY              606
#define SHELL_ARITHMETIC_NOT_EQUAL             607
#define SHELL_ARITHMETIC_LOGICAL_AND           608
#define SHELL_ARITHMETIC_LOGICAL_OR            609
#define SHELL_ARITHMETIC_MULTIPLY_ASSIGN       610
#define SHELL_ARITHMETIC_DIVIDE_ASSIGN         611
#define SHELL_ARITHMETIC_MODULO_ASSIGN         612
#define SHELL_ARITHMETIC_ADD_ASSIGN            613
#define SHELL_ARITHMETIC_SUBTRACT_ASSIGN       614
#define SHELL_ARITHMETIC_LEFT_SHIFT_ASSIGN     615
#define SHELL_ARITHMETIC_RIGHT_SHIFT_ASSIGN    616
#define SHELL_ARITHMETIC_AND_ASSIGN            617
#define SHELL_ARITHMETIC_OR_ASSIGN             618
#define SHELL_ARITHMETIC_XOR_ASSIGN            619

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the state for the lexer of arithmetic expressions.

Members:

    Input - Supplies a pointer to the input string.

    InputSize - Supplies the size of the input string in bytes.

    InputOffset - Supplies the offset of the next character to fetch.

    TokenType - Stores the type of token this current token represents. See
        SHELL_ARITHMETIC_* definitions.

    TokenBuffer - Stores a pointer to the buffer containing the current token.

    TokenBufferCapacity - Stores the total size fo the token buffer.

    TokenBufferSize - Stores the number of valid bytes in the token buffer.

    LexerPrimed - Stores a boolean indicating whether the lexer token is valid
        (TRUE) or uninitialized (FALSE).

    TokensRead - Stores the number of tokens that have been read, including
        this one.

    AssignmentName - Stores the name of the destination assignment variable.

    AssignmentNameSize - Stores the size of the assignment name in bytes.

--*/

typedef struct _SHELL_ARITHMETIC_LEXER {
    PSTR Input;
    UINTN InputSize;
    UINTN InputOffset;
    ULONG TokenType;
    PSTR TokenBuffer;
    UINTN TokenBufferCapacity;
    UINTN TokenBufferSize;
    BOOL LexerPrimed;
    UINTN TokensRead;
    PSTR AssignmentName;
    UINTN AssignmentNameSize;
} SHELL_ARITHMETIC_LEXER, *PSHELL_ARITHMETIC_LEXER;

/*++

Structure Description:

    This structure defines an entry in the parse stack for arithmetic
    expressions.

Members:

    ListEntry - Stores the next and previous elements in the parse stack.
        Elements get pushed onto the front of the list, so following the
        next pointer goes to older elements.

    TokenType - Stores the type of this token.

    Value - Stores the value for numeric tokens.

--*/

typedef struct SHELL_ARITHMETIC_PARSE_ELEMENT {
    LIST_ENTRY ListEntry;
    ULONG TokenType;
    LONG Value;
} SHELL_ARITHMETIC_PARSE_ELEMENT, *PSHELL_ARITHMETIC_PARSE_ELEMENT;

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ShParseArithmeticExpression (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    BOOL Nested,
    PLONG ExpressionResult
    );

BOOL
ShGetNextArithmeticParseElement (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    PLIST_ENTRY Stack,
    BOOL Nested,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Element
    );

BOOL
ShArithmeticShiftOrReduce (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    PLIST_ENTRY Stack,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Next,
    PBOOL Shift
    );

BOOL
ShEvaluateArithmeticOperator (
    PSHELL Shell,
    PSHELL_ARITHMETIC_PARSE_ELEMENT LeftValue,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Operator,
    PSHELL_ARITHMETIC_PARSE_ELEMENT RightValue,
    PLONG Result
    );

ULONG
ShGetOperatorPrecedence (
    ULONG TokenType
    );

BOOL
ShAssignArithmeticResult (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    LONG Value
    );

BOOL
ShGetArithmeticToken (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer
    );

BOOL
ShAddCharacterToArithmeticToken (
    PSHELL_ARITHMETIC_LEXER Lexer,
    CHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

BOOL ShDebugArithmeticLexer = FALSE;
BOOL ShDebugArithmeticParser = FALSE;

PSTR ShArithmeticTokenStrings[] = {
    "ARITHMETIC_WORD",
    "ARITHMETIC_NUMBER",
    "ARITHMETIC_SHIFT_LEFT",
    "ARITHMETIC_SHIFT_RIGHT",
    "ARITHMETIC_LESS_THAN_OR_EQUAL",
    "ARITHMETIC_GREATER_THAN_OR_EQUAL",
    "ARITHMETIC_EQUALITY",
    "ARITHMETIC_NOT_EQUAL",
    "ARITHMETIC_LOGICAL_AND",
    "ARITHMETIC_LOGICAL_OR",
    "ARITHMETIC_MULTIPLY_ASSIGN",
    "ARITHMETIC_DIVIDE_ASSIGN",
    "ARITHMETIC_MODULO_ASSIGN",
    "ARITHMETIC_ADD_ASSIGN",
    "ARITHMETIC_SUBTRACT_ASSIGN",
    "ARITHMETIC_LEFT_SHIFT_ASSIGN",
    "ARITHMETIC_RIGHT_SHIFT_ASSIGN",
    "ARITHMETIC_AND_ASSIGN",
    "ARITHMETIC_OR_ASSIGN",
    "ARITHMETIC_XOR_ASSIGN"
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
ShEvaluateArithmeticExpression (
    PSHELL Shell,
    PSTR String,
    UINTN Length,
    PSTR *Answer,
    PUINTN AnswerSize
    )

/*++

Routine Description:

    This routine evaluates an arithmetic expression. It assumes that all
    expansions have already taken place except for variable names without a
    dollar sign.

Arguments:

    Shell - Supplies a pointer to the shell.

    String - Supplies a pointer to the input string.

    Length - Supplies the length of the input string in bytes.

    Answer - Supplies a pointer where the evaluation will be returned on
        success. The caller is responsible for freeing this memory.

    AnswerSize - Supplies a pointer where the size of the answer buffer will
        be returned including the null terminating byte on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AnswerBuffer;
    SHELL_ARITHMETIC_LEXER Lexer;
    LONG NumericAnswer;
    BOOL Result;

    AnswerBuffer = NULL;
    memset(&Lexer, 0, sizeof(SHELL_ARITHMETIC_LEXER));
    Lexer.TokenBufferCapacity = SHELL_ARITHMETIC_INITIAL_TOKEN_BUFFER_SIZE;
    Lexer.TokenBuffer = malloc(SHELL_ARITHMETIC_INITIAL_TOKEN_BUFFER_SIZE);
    if (Shell->Lexer.TokenBuffer == NULL) {
        Result = FALSE;
        goto EvaluateArithmeticExpressionEnd;
    }

    Lexer.Input = String;
    Lexer.InputSize = Length;
    Result = ShParseArithmeticExpression(Shell, &Lexer, FALSE, &NumericAnswer);
    if (Result == FALSE) {
        goto EvaluateArithmeticExpressionEnd;
    }

    //
    // Convert the answer to a string for the caller. So nice.
    //

    AnswerBuffer = malloc(SHELL_ARITHMETIC_INTEGER_STRING_BUFFER_SIZE);
    if (AnswerBuffer == NULL) {
        Result = FALSE;
        goto EvaluateArithmeticExpressionEnd;
    }

    *AnswerSize = snprintf(AnswerBuffer,
                           SHELL_ARITHMETIC_INTEGER_STRING_BUFFER_SIZE,
                           "%d",
                           NumericAnswer) + 1;

    Result = TRUE;

EvaluateArithmeticExpressionEnd:
    if (Lexer.TokenBuffer != NULL) {
        free(Lexer.TokenBuffer);
    }

    if (Lexer.AssignmentName != NULL) {
        free(Lexer.AssignmentName);
    }

    if (Result == FALSE) {
        if (AnswerBuffer != NULL) {
            free(AnswerBuffer);
            AnswerBuffer = NULL;
        }
    }

    *Answer = AnswerBuffer;
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ShParseArithmeticExpression (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    BOOL Nested,
    PLONG ExpressionResult
    )

/*++

Routine Description:

    This routine parses and evaluates an arithmetic expression.

Arguments:

    Shell - Supplies a pointer to the shell to operate on.

    Lexer - Supplies a pointer to the initialized and primed lexer.

    Nested - Supplies a boolean indicating whether this is the primary
        expression (FALSE) or whether this is nested as part of evaluating
        an expression inside parentheses or after a ? or :.

    ExpressionResult - Supplies a pointer where the evaluation will be returned
        on success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSHELL_ARITHMETIC_PARSE_ELEMENT Element;
    SHELL_ARITHMETIC_PARSE_ELEMENT NextElement;
    BOOL Result;
    BOOL Shift;
    LIST_ENTRY Stack;

    INITIALIZE_LIST_HEAD(&Stack);
    while (TRUE) {
        Result = ShGetNextArithmeticParseElement(Shell,
                                                 Lexer,
                                                 &Stack,
                                                 Nested,
                                                 &NextElement);

        if (Result == FALSE) {
            break;
        }

        //
        // Reduce as much as possible.
        //

        do {
            Result = ShArithmeticShiftOrReduce(Shell,
                                               Lexer,
                                               &Stack,
                                               &NextElement,
                                               &Shift);

            if (Result == FALSE) {
                goto ParseArithmeticExpressionEnd;
            }

        } while (Shift == FALSE);

        //
        // If this was the EOF token, then the parser should be done.
        //

        if (NextElement.TokenType == SHELL_ARITHMETIC_END_OF_FILE) {

            //
            // If there's not exactly one element on the list, it's a failure.
            //

            if ((LIST_EMPTY(&Stack) != FALSE) || (Stack.Next->Next != &Stack)) {
                Result = FALSE;
                *ExpressionResult = 0;
                break;

            } else {
                Element = LIST_VALUE(Stack.Next,
                                     SHELL_ARITHMETIC_PARSE_ELEMENT,
                                     ListEntry);

                if (Element->TokenType != SHELL_ARITHMETIC_NUMBER) {
                    Result = FALSE;
                    *ExpressionResult = 0;

                } else {
                    *ExpressionResult = Element->Value;
                    if (Lexer->AssignmentName != NULL) {
                        Result = ShAssignArithmeticResult(Shell,
                                                          Lexer,
                                                          Element->Value);

                        if (Result == FALSE) {
                            goto ParseArithmeticExpressionEnd;
                        }
                    }

                    Result = TRUE;
                    break;
                }
            }

        } else {
            Element = malloc(sizeof(SHELL_ARITHMETIC_PARSE_ELEMENT));
            if (Element == NULL) {
                Result = FALSE;
                goto ParseArithmeticExpressionEnd;
            }

            memcpy(Element,
                   &NextElement,
                   sizeof(SHELL_ARITHMETIC_PARSE_ELEMENT));

            INSERT_AFTER(&(Element->ListEntry), &Stack);
        }
    }

ParseArithmeticExpressionEnd:
    while (LIST_EMPTY(&Stack) == FALSE) {
        Element = LIST_VALUE(Stack.Next,
                             SHELL_ARITHMETIC_PARSE_ELEMENT,
                             ListEntry);

        LIST_REMOVE(&(Element->ListEntry));
        free(Element);
    }

    if (ShDebugArithmeticParser != FALSE) {
        if (Result != FALSE) {
            ShPrintTrace(Shell, "Arithmetic Result: %ld\n", *ExpressionResult);

        } else {
            ShPrintTrace(Shell,
                         "Error: Failed to parse arithmetic expression.\n");
        }
    }

    return Result;
}

BOOL
ShGetNextArithmeticParseElement (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    PLIST_ENTRY Stack,
    BOOL Nested,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Element
    )

/*++

Routine Description:

    This routine retrieves the next parse element for an arithmetic expression.

Arguments:

    Shell - Supplies a pointer to the shell to operate on.

    Lexer - Supplies a pointer to the initialized lexer.

    Stack - Supplies a pointer to the parsing stack.

    Nested - Supplies a boolean indicating whether this is the primary
        expression (FALSE) or whether this is nested as part of evaluating
        an expression inside parentheses or after a ? or :.

    Element - Supplies a pointer where the filled out element will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AfterScan;
    LONG FalseValue;
    BOOL IsName;
    BOOL Result;
    PSHELL_ARITHMETIC_PARSE_ELEMENT Top;
    LONG TrueValue;
    LONG Value;
    PSTR ValueString;
    UINTN ValueStringSize;

    Result = ShGetArithmeticToken(Shell, Lexer);
    if (Result == FALSE) {
        return FALSE;
    }

    if ((Lexer->TokenType == SHELL_ARITHMETIC_NUMBER) ||
        (Lexer->TokenType == SHELL_ARITHMETIC_WORD)) {

        if (Lexer->TokenType == SHELL_ARITHMETIC_WORD) {
            Result = ShGetVariable(Shell,
                                   Lexer->TokenBuffer,
                                   Lexer->TokenBufferSize,
                                   &ValueString,
                                   &ValueStringSize);

            if (Result == FALSE) {
                ValueString = NULL;
            }

            //
            // Variables need to be valid names.
            //

            IsName = ShIsName(Lexer->TokenBuffer, Lexer->TokenBufferSize);
            if (IsName == FALSE) {
                return FALSE;
            }

            //
            // If this is the first token and it's a variable name, save it
            // in case it's an assignment.
            //

            if (Lexer->TokensRead == 1) {

                assert(Lexer->AssignmentName == NULL);

                Lexer->AssignmentName = SwStringDuplicate(
                                                       Lexer->TokenBuffer,
                                                       Lexer->TokenBufferSize);

                if (Lexer->AssignmentName == NULL) {
                    return FALSE;
                }

                Lexer->AssignmentNameSize = Lexer->TokenBufferSize;
            }

        } else {
            ValueString = Lexer->TokenBuffer;
            ValueStringSize = Lexer->TokenBufferSize;
        }

        //
        // If there's a buffer, attempt to convert it to a number.
        //

        Value = 0;
        if (ValueString != NULL) {
            Value = strtol(ValueString, &AfterScan, 0);
            if ((UINTN)AfterScan != (UINTN)ValueString + ValueStringSize - 1) {
                Result = FALSE;
                return FALSE;
            }
        }

        Element->TokenType = SHELL_ARITHMETIC_NUMBER;
        Element->Value = Value;
        return TRUE;

    } else if (Lexer->TokenType == '(') {
        Result = ShParseArithmeticExpression(Shell, Lexer, TRUE, &Value);
        if (Result == FALSE) {
            return FALSE;
        }

        if (Lexer->TokenType != ')') {
            return FALSE;
        }

        Element->TokenType = SHELL_ARITHMETIC_NUMBER;
        Element->Value = Value;
        return TRUE;

    //
    // The question mark ternary operator is also recursive, as it requires two
    // full expressions. This cheats and does a bit of reducing which isn't
    // ideal architecturally but seeing as it's the only thing that does this
    // and I'm a little tipsy we'll let it slide.
    //

    } else if (Lexer->TokenType == '?') {
        Result = ShParseArithmeticExpression(Shell, Lexer, TRUE, &TrueValue);
        if (Result == FALSE) {
            return FALSE;
        }

        if (Lexer->TokenType != ':') {
            return FALSE;
        }

        Result = ShParseArithmeticExpression(Shell, Lexer, TRUE, &FalseValue);
        if (Result == FALSE) {
            return FALSE;
        }

        Top = LIST_VALUE(Stack->Next,
                         SHELL_ARITHMETIC_PARSE_ELEMENT,
                         ListEntry);

        if ((LIST_EMPTY(Stack) != FALSE) ||
            (Top->TokenType != SHELL_ARITHMETIC_NUMBER)) {

            return FALSE;
        }

        if (Top->Value != 0) {
            Value = TrueValue;

        } else {
            Value = FalseValue;
        }

        if (ShDebugArithmeticParser != FALSE) {
            ShPrintTrace(Shell,
                         "arith: %ld <== %ld ? %ld : %ld\n",
                         Value,
                         Top->Value,
                         TrueValue,
                         FalseValue);
        }

        Top->Value = Value;
        Element->TokenType = SHELL_ARITHMETIC_END_OF_FILE;
        Element->Value = 0;
        return TRUE;

    //
    // If this is a closing parentheses or colon and it's nested, that's the
    // end of the line for the inner nested one. If it's not nested, then this
    // is a bad random close parentheses.
    //

    } else if ((Lexer->TokenType == ')') || (Lexer->TokenType == ':')) {
        if (Nested != FALSE) {
            Element->TokenType = SHELL_ARITHMETIC_END_OF_FILE;
            Element->Value = 0;
            return TRUE;

        } else {
            return FALSE;
        }
    }

    Element->TokenType = Lexer->TokenType;
    Element->Value = 0;
    return TRUE;
}

BOOL
ShArithmeticShiftOrReduce (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    PLIST_ENTRY Stack,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Next,
    PBOOL Shift
    )

/*++

Routine Description:

    This routine attempts to reduce the given parse stack if possible, or
    directs the caller to shift.

Arguments:

    Shell - Supplies a pointer to the shell to operate on.

    Lexer - Supplies a pointer to the initialized lexer.

    Stack - Supplies a pointer to the head of the parse stack.

    Next - Supplies a pointer to the next element.

    Shift - Supplies a boolean where TRUE will be returned if more values
        should be shifted onto the stack. Or FALSE if the stack was reduced.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    LONG Answer;
    ULONG NextPrecedence;
    ULONG NextToken;
    PSHELL_ARITHMETIC_PARSE_ELEMENT Operator;
    BOOL Result;
    ULONG StackPrecedence;
    PSHELL_ARITHMETIC_PARSE_ELEMENT Top;
    PSHELL_ARITHMETIC_PARSE_ELEMENT TwoBack;

    *Shift = FALSE;
    if (LIST_EMPTY(Stack) != FALSE) {
        *Shift = TRUE;
        return TRUE;
    }

    Top = LIST_VALUE(Stack->Next, SHELL_ARITHMETIC_PARSE_ELEMENT, ListEntry);

    //
    // Assignment operators are only allowed as the second value in an
    // expression (with the first being an assignment word).
    //

    NextToken = Next->TokenType;

    assert(NextToken != SHELL_ARITHMETIC_WORD);

    if (SHELL_ARITHMETIC_ASSIGN_OPERATOR(NextToken)) {
        if ((Lexer->TokensRead != 2) || (Lexer->AssignmentName == NULL)) {
            return FALSE;
        }

    //
    // If it's token 2 and not an assignment operator, free up the assignment
    // word.
    //

    } else if (Lexer->TokensRead == 2) {
        if (Lexer->AssignmentName != NULL) {
            free(Lexer->AssignmentName);
            Lexer->AssignmentName = NULL;
        }
    }

    if (Top->TokenType == SHELL_ARITHMETIC_NUMBER) {

        //
        // If that's all that's on the list, definitely shift, unless this
        // next token is another number which is invalid.
        //

        if (Top->ListEntry.Next == Stack) {
            if (Next->TokenType == SHELL_ARITHMETIC_NUMBER) {
                return FALSE;
            }

            *Shift = TRUE;
            return TRUE;
        }

        //
        // Get the operator down there and let's find out what it is.
        //

        Operator = LIST_VALUE(Top->ListEntry.Next,
                              SHELL_ARITHMETIC_PARSE_ELEMENT,
                              ListEntry);

        TwoBack = LIST_VALUE(Operator->ListEntry.Next,
                             SHELL_ARITHMETIC_PARSE_ELEMENT,
                             ListEntry);

        if (Operator->ListEntry.Next == Stack) {
            TwoBack = NULL;
        }

        //
        // If it's a plus or a minus, this could be a unary plus or minus, in
        // which case it should be reduced. It's known to be a unary operator
        // if there's not a number behind it. It could also be a binary plus
        // or minus, in which case it's left alone.
        //

        if ((Operator->TokenType == '+') || (Operator->TokenType == '-')) {
            if ((TwoBack == NULL) ||
                (TwoBack->TokenType != SHELL_ARITHMETIC_NUMBER)) {

                //
                // Reduce the unary + or minus and return.
                //

                LIST_REMOVE(&(Top->ListEntry));
                if (Operator->TokenType == '-') {
                    Operator->Value = -Top->Value;

                } else {
                    Operator->Value = Top->Value;
                }

                Operator->TokenType = Top->TokenType;
                free(Top);
                return TRUE;
            }

        } else if (SHELL_ARITHMETIC_UNARY_OPERATOR(Operator->TokenType)) {
            TwoBack = NULL;
        }

        StackPrecedence = ShGetOperatorPrecedence(Operator->TokenType);

        assert(StackPrecedence != -1);

        NextPrecedence = ShGetOperatorPrecedence(Next->TokenType);

        //
        // If the next thing is not an operator, then fail now.
        //

        if (NextPrecedence == -1) {
            return FALSE;
        }

        //
        // If the operator on the stack has equal or higher precedence than the
        // next operator, then reduce, otherwise shift.
        //

        if (StackPrecedence >= NextPrecedence) {

            assert((TwoBack == NULL) ||
                   (TwoBack->TokenType == SHELL_ARITHMETIC_NUMBER));

            Result = ShEvaluateArithmeticOperator(Shell,
                                                  TwoBack,
                                                  Operator,
                                                  Top,
                                                  &Answer);

            if (Result == FALSE) {
                return FALSE;
            }

            LIST_REMOVE(&(Top->ListEntry));
            free(Top);
            if (TwoBack != NULL) {
                LIST_REMOVE(&(Operator->ListEntry));
                free(Operator);
                Operator = TwoBack;
            }

            Operator->TokenType = SHELL_ARITHMETIC_NUMBER;
            Operator->Value = Answer;

        } else {
            *Shift = TRUE;
        }

        return TRUE;
    }

    //
    // If what's coming next is a number, then let it on for sure.
    //

    if (NextToken == SHELL_ARITHMETIC_NUMBER) {
        *Shift = TRUE;
        return TRUE;
    }

    //
    // What's next is an operator. Only allow it if it is a unary operator.
    //

    if (!SHELL_ARITHMETIC_UNARY_OPERATOR(NextToken)) {
        return FALSE;
    }

    *Shift = TRUE;
    return TRUE;
}

BOOL
ShEvaluateArithmeticOperator (
    PSHELL Shell,
    PSHELL_ARITHMETIC_PARSE_ELEMENT LeftValue,
    PSHELL_ARITHMETIC_PARSE_ELEMENT Operator,
    PSHELL_ARITHMETIC_PARSE_ELEMENT RightValue,
    PLONG Result
    )

/*++

Routine Description:

    This routine performs an arithmetic operation on two values.

Arguments:

    Shell - Supplies a pointer to the shell.

    LeftValue - Supplies a pointer to the left operand.

    Operator - Supplies a pointer to the operator.

    RightValue - Supplies a pointer to the right operand.

    Result - Supplies a pointer where the resulting value will be returned on
        success.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    LONG Answer;
    CHAR DebugOperator[2];
    LONG Left;
    LONG Right;

    assert(((LeftValue == NULL) ||
            (LeftValue->TokenType == SHELL_ARITHMETIC_NUMBER)) &&
           (RightValue->TokenType == SHELL_ARITHMETIC_NUMBER));

    Left = 0;
    if (LeftValue != 0) {
        Left = LeftValue->Value;
    }

    Right = RightValue->Value;
    DebugOperator[0] = (CHAR)Operator->TokenType;
    if (Operator->TokenType < 0x100) {
        DebugOperator[1] = ' ';

    } else {
        DebugOperator[1] = '=';
    }

    switch (Operator->TokenType) {
    case SHELL_ARITHMETIC_LEFT_SHIFT_ASSIGN:
    case SHELL_ARITHMETIC_SHIFT_LEFT:
        DebugOperator[0] = '<';
        DebugOperator[1] = '<';
        Answer = Left << Right;
        break;

    case SHELL_ARITHMETIC_RIGHT_SHIFT_ASSIGN:
    case SHELL_ARITHMETIC_SHIFT_RIGHT:
        DebugOperator[0] = '>';
        DebugOperator[1] = '>';
        Answer = Left >> Right;
        break;

    case SHELL_ARITHMETIC_LESS_THAN_OR_EQUAL:
        DebugOperator[0] = '<';
        Answer = Left <= Right;
        break;

    case SHELL_ARITHMETIC_GREATER_THAN_OR_EQUAL:
        DebugOperator[0] = '>';
        Answer = Left >= Right;
        break;

    case SHELL_ARITHMETIC_EQUALITY:
        DebugOperator[0] = '=';
        Answer = Left == Right;
        break;

    case SHELL_ARITHMETIC_NOT_EQUAL:
        DebugOperator[0] = '!';
        Answer = Left != Right;
        break;

    case SHELL_ARITHMETIC_LOGICAL_AND:
        DebugOperator[0] = '&';
        DebugOperator[1] = '&';
        Answer = Left && Right;
        break;

    case SHELL_ARITHMETIC_LOGICAL_OR:
        DebugOperator[0] = '|';
        DebugOperator[1] = '|';
        Answer = Left || Right;
        break;

    case '~':
        Answer = ~Right;
        break;

    case '!':
        Answer = !Right;
        break;

    case SHELL_ARITHMETIC_MULTIPLY_ASSIGN:
    case '*':
        DebugOperator[0] = '*';
        Answer = Left * Right;
        break;

    case SHELL_ARITHMETIC_DIVIDE_ASSIGN:
    case '/':
        DebugOperator[0] = '/';
        Answer = Left / Right;
        break;

    case SHELL_ARITHMETIC_MODULO_ASSIGN:
    case '%':
        DebugOperator[0] = '%';
        Answer = Left % Right;
        break;

    case SHELL_ARITHMETIC_ADD_ASSIGN:
    case '+':
        DebugOperator[0] = '+';
        Answer = Left + Right;
        break;

    case SHELL_ARITHMETIC_SUBTRACT_ASSIGN:
    case '-':
        DebugOperator[0] = '-';
        Answer = Left - Right;
        break;

    case '<':
        Answer = Left < Right;
        break;

    case '>':
        Answer = Left > Right;
        break;

    case '=':
        Answer = Right;
        break;

    case SHELL_ARITHMETIC_AND_ASSIGN:
    case '&':
        DebugOperator[0] = '&';
        Answer = Left & Right;
        break;

    case SHELL_ARITHMETIC_OR_ASSIGN:
    case '|':
        DebugOperator[0] = '|';
        Answer = Left | Right;
        break;

    case SHELL_ARITHMETIC_XOR_ASSIGN:
    case '^':
        DebugOperator[0] = '^';
        Answer = Left ^ Right;
        break;

    default:

        assert(FALSE);

        return FALSE;
    }

    if (ShDebugArithmeticParser != FALSE) {
        ShPrintTrace(Shell,
                     "Arith: %ld <== %ld %c%c %ld\n",
                     Answer,
                     Left,
                     DebugOperator[0],
                     DebugOperator[1],
                     Right);
    }

    *Result = Answer;
    return TRUE;
}

ULONG
ShGetOperatorPrecedence (
    ULONG TokenType
    )

/*++

Routine Description:

    This routine gives a ranking for the given operator. Higher numbers mean
    greater precence.

Arguments:

    TokenType - Supplies the operator to rank.

Return Value:

    Returns a value representing the operator's ranking. Higher number bind
    tighter to their arguments.

    -1 if the token is not an operator.

--*/

{

    switch (TokenType) {
    case SHELL_ARITHMETIC_END_OF_FILE:
        return 0;

    case SHELL_ARITHMETIC_MULTIPLY_ASSIGN:
    case SHELL_ARITHMETIC_DIVIDE_ASSIGN:
    case SHELL_ARITHMETIC_MODULO_ASSIGN:
    case SHELL_ARITHMETIC_ADD_ASSIGN:
    case SHELL_ARITHMETIC_SUBTRACT_ASSIGN:
    case SHELL_ARITHMETIC_LEFT_SHIFT_ASSIGN:
    case SHELL_ARITHMETIC_RIGHT_SHIFT_ASSIGN:
    case SHELL_ARITHMETIC_AND_ASSIGN:
    case SHELL_ARITHMETIC_OR_ASSIGN:
    case SHELL_ARITHMETIC_XOR_ASSIGN:
    case '=':
        return 1;

    case '?':
        return 2;

    case ':':
        return 3;

    case SHELL_ARITHMETIC_LOGICAL_OR:
        return 4;

    case SHELL_ARITHMETIC_LOGICAL_AND:
        return 5;

    case '|':
        return 6;

    case '^':
        return 7;

    case '&':
        return 8;

    case SHELL_ARITHMETIC_NOT_EQUAL:
    case SHELL_ARITHMETIC_EQUALITY:
        return 9;

    case SHELL_ARITHMETIC_LESS_THAN_OR_EQUAL:
    case SHELL_ARITHMETIC_GREATER_THAN_OR_EQUAL:
    case '<':
    case '>':
        return 10;

    case SHELL_ARITHMETIC_SHIFT_LEFT:
    case SHELL_ARITHMETIC_SHIFT_RIGHT:
        return 11;

    case '+':
    case '-':
        return 12;

    case '*':
    case '/':
    case '%':
        return 13;

    case '~':
    case '!':
        return 14;

    default:
        break;
    }

    assert(FALSE);

    return 0;
}

BOOL
ShAssignArithmeticResult (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer,
    LONG Value
    )

/*++

Routine Description:

    This routine performs an assignment of an arithmetic result to a shell
    environment variable.

Arguments:

    Shell - Supplies a pointer to the shell to operate on.

    Lexer - Supplies a pointer to the initialized lexer.

    Value - Supplies the numeric value to assign.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;
    CHAR StringBuffer[SHELL_ARITHMETIC_INTEGER_STRING_BUFFER_SIZE];
    UINTN StringSize;

    StringSize = snprintf(StringBuffer,
                          SHELL_ARITHMETIC_INTEGER_STRING_BUFFER_SIZE,
                          "%d",
                          Value) + 1;

    assert((Lexer->AssignmentName != NULL) && (Lexer->AssignmentNameSize != 0));

    Result = ShSetVariable(Shell,
                           Lexer->AssignmentName,
                           Lexer->AssignmentNameSize,
                           StringBuffer,
                           StringSize);

    return Result;
}

BOOL
ShGetArithmeticToken (
    PSHELL Shell,
    PSHELL_ARITHMETIC_LEXER Lexer
    )

/*++

Routine Description:

    This routine gets a token for the arithmetic lexer.

Arguments:

    Shell - Supplies a pointer to the shell.

    Lexer - Supplies a pointer to the lexer to operate on.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    CHAR AddCharacter;
    CHAR Character;
    BOOL Delimit;
    CHAR LastCharacter;
    CHAR LastLastCharacter;
    BOOL Result;
    UINTN TokenStringIndex;
    BOOL Unput;

    Character = 0;
    LastCharacter = 0;
    LastLastCharacter = 0;
    Lexer->TokenType = -1;
    Lexer->TokenBufferSize = 0;
    Lexer->TokensRead += 1;
    Delimit = FALSE;
    while (TRUE) {
        AddCharacter = TRUE;
        Unput = FALSE;

        //
        // If the end of the buffer came around, delimit the current token, or
        // return the EOF token.
        //

        if ((Lexer->InputOffset == Lexer->InputSize) ||
            (Lexer->Input[Lexer->InputOffset] == '\0')) {

            AddCharacter = FALSE;
            Delimit = TRUE;
            if (Lexer->TokenBufferSize == 0) {
                Lexer->TokenType = SHELL_ARITHMETIC_END_OF_FILE;
            }

        } else {
            Character = Lexer->Input[Lexer->InputOffset];
            Lexer->InputOffset += 1;
            switch (Character) {

            //
            // These characters are operators all by themselves, and only ever
            // by themselves.
            //

            case '~':
            case '?':
            case ':':
            case '(':
            case ')':
                Delimit = TRUE;
                if (Lexer->TokenBufferSize != 0) {
                    AddCharacter = FALSE;
                    Unput = TRUE;
                }

                break;

            //
            // This next batch of symbols either stands by itself or can only
            // have an equals after it (and is only ever the first character
            // in a double symbol).
            //

            case '!':
            case '*':
            case '/':
            case '%':
            case '+':
            case '-':
            case '^':
                if (Lexer->TokenBufferSize != 0) {
                    Delimit = TRUE;
                    AddCharacter = FALSE;
                    Unput = TRUE;
                }

                break;

            //
            // The & and | symbols could either be on their own, with an
            // equals (&= |=) or with themselves (&& ||).
            //

            case '&':
            case '|':
                if (LastCharacter == Character) {
                    Delimit = TRUE;
                    if (Character == '&') {
                        Lexer->TokenType = SHELL_ARITHMETIC_LOGICAL_AND;

                    } else {

                        assert(Character == '|');

                        Lexer->TokenType = SHELL_ARITHMETIC_LOGICAL_OR;
                    }

                //
                // Besides && and ||, these symbols are always the first in an
                // operator, so if there's something in the buffer flush it
                // out.
                //

                } else if (Lexer->TokenBufferSize != 0) {
                    Delimit = TRUE;
                    AddCharacter = FALSE;
                    Unput = TRUE;
                }

                break;

            //
            // The > and < symbols could either be by themselves, doubled
            // (<< >>), with an equals after them (>= <=), or double and with
            // and equals after them (>>= <<=). The only way they can be
            // delimited for sure now is if the last character wasn't the same
            // as this one.
            //

            case '<':
            case '>':
                if ((Lexer->TokenBufferSize != 0) &&
                    (LastCharacter != Character)) {

                    Delimit = TRUE;
                    AddCharacter = FALSE;
                    Unput = TRUE;
                }

                break;

            //
            // The all important equal sign. It can be by itself, or with 1 or
            // 2 characters in front of it.
            //

            case '=':

                //
                // If it's not the first character in the buffer, it's always
                // the last.
                //

                if (Lexer->TokenBufferSize != 0) {
                    Delimit = TRUE;
                }

                switch (LastCharacter) {
                case '!':
                    Lexer->TokenType = SHELL_ARITHMETIC_NOT_EQUAL;
                    break;

                case '=':
                    Lexer->TokenType = SHELL_ARITHMETIC_EQUALITY;
                    break;

                case '&':
                    Lexer->TokenType = SHELL_ARITHMETIC_AND_ASSIGN;
                    break;

                case '|':
                    Lexer->TokenType = SHELL_ARITHMETIC_OR_ASSIGN;
                    break;

                case '+':
                    Lexer->TokenType = SHELL_ARITHMETIC_ADD_ASSIGN;
                    break;

                case '-':
                    Lexer->TokenType = SHELL_ARITHMETIC_SUBTRACT_ASSIGN;
                    break;

                case '*':
                    Lexer->TokenType = SHELL_ARITHMETIC_MULTIPLY_ASSIGN;
                    break;

                case '/':
                    Lexer->TokenType = SHELL_ARITHMETIC_DIVIDE_ASSIGN;
                    break;

                case '%':
                    Lexer->TokenType = SHELL_ARITHMETIC_MODULO_ASSIGN;
                    break;

                case '^':
                    Lexer->TokenType = SHELL_ARITHMETIC_XOR_ASSIGN;
                    break;

                case '>':
                    Lexer->TokenType = SHELL_ARITHMETIC_GREATER_THAN_OR_EQUAL;
                    if (LastLastCharacter == '>') {
                        Lexer->TokenType = SHELL_ARITHMETIC_RIGHT_SHIFT_ASSIGN;
                    }

                    break;

                case '<':
                    Lexer->TokenType = SHELL_ARITHMETIC_LESS_THAN_OR_EQUAL;
                    if (LastLastCharacter == '<') {
                        Lexer->TokenType = SHELL_ARITHMETIC_LEFT_SHIFT_ASSIGN;
                    }

                    break;

                //
                // If the last character was nothing, an operator that can't
                // have an equals sign glued onto it, or an average joe
                // character then either delimit the previous token or don't
                // delimit and see if another equals comes in.
                //

                default:
                    if (Lexer->TokenBufferSize != 0) {
                        AddCharacter = FALSE;
                        Unput = TRUE;
                    }

                    break;
                }

                break;

            //
            // This is an average joe character, either a digit, letter, or
            // space of some kind. Look to see if an operator that might have
            // been two or three characters but wasn't just ended.
            //

            default:
                AddCharacter = FALSE;
                switch (LastCharacter) {
                case '<':
                case '>':
                    Delimit = TRUE;
                    AddCharacter = FALSE;
                    Unput = TRUE;
                    if (LastCharacter == LastLastCharacter) {
                        if (LastCharacter == '<') {
                            Lexer->TokenType = SHELL_ARITHMETIC_SHIFT_LEFT;

                        } else {

                            assert(LastCharacter == '>');

                            Lexer->TokenType = SHELL_ARITHMETIC_SHIFT_RIGHT;
                        }
                    }

                    break;

                //
                // Check out characters that could stand as operators on their
                // own but were deferred to see if perhaps something else was
                // glued onto it.
                //

                case '!':
                case '*':
                case '/':
                case '%':
                case '+':
                case '-':
                case '^':
                case '&':
                case '|':
                case '=':
                    Delimit = TRUE;
                    AddCharacter = FALSE;
                    Unput = TRUE;
                    break;

                default:
                    AddCharacter = TRUE;
                    break;
                }

                //
                // If this character is still no longer on the table because
                // an operator is being delimited, stop now, this will come
                // back around next time.
                //

                if (AddCharacter == FALSE) {
                    break;
                }

                //
                // If this is a whitespace character, delimit if there's
                // anything in the buffer and throw the whitespace away.
                //

                if (isspace(Character)) {
                    AddCharacter = FALSE;
                    if (Lexer->TokenBufferSize != 0) {
                        Delimit = TRUE;
                    }

                } else if (Lexer->TokenBufferSize == 0) {
                    if ((Character >= '0') && (Character <= '9')) {
                        Lexer->TokenType = SHELL_ARITHMETIC_NUMBER;

                    } else {
                        Lexer->TokenType = SHELL_ARITHMETIC_WORD;
                    }
                }

                break;
            }
        }

        //
        // It's not expected that the someone wants to both add and unput the
        // character, as that would duplicate it.
        //

        assert((AddCharacter == FALSE) || (Unput == FALSE));

        //
        // Add, unput, or delimit the character as requested.
        //

        if (AddCharacter != FALSE) {

            assert(Character != 0);

            Result = ShAddCharacterToArithmeticToken(Lexer, Character);
            if (Result == FALSE) {
                return FALSE;
            }

            LastLastCharacter = LastCharacter;
            LastCharacter = Character;
        }

        if (Unput != FALSE) {

            assert(Lexer->InputOffset != 0);

            Lexer->InputOffset -= 1;
        }

        if (Delimit != FALSE) {
            if ((Lexer->TokenType == -1) && (Lexer->TokenBufferSize == 1)) {
                Lexer->TokenType = Lexer->TokenBuffer[0];
            }

            Result = ShAddCharacterToArithmeticToken(Lexer, '\0');
            if (Result == FALSE) {
                return FALSE;
            }

            break;
        }
    }

    assert(Lexer->TokenType != -1);

    if (ShDebugArithmeticLexer != FALSE) {
        if (Lexer->TokenType == SHELL_ARITHMETIC_END_OF_FILE) {
            ShPrintTrace(Shell, "Reached end of arithmetic expression.\n");

        } else if (Lexer->TokenType < 0xFF) {
            if (Lexer->TokenType < ' ') {
                if (Lexer->TokenType == '\n') {
                    ShPrintTrace(Shell, "%25s: \n", "<newline>");

                } else {
                    ShPrintTrace(Shell, "%25d: \n", Lexer->TokenType);
                }

            } else {
                ShPrintTrace(Shell,
                             "%25c: %s\n",
                             Lexer->TokenType,
                             Lexer->TokenBuffer);
            }

        } else {

            assert(Lexer->TokenType >= SHELL_ARITHMETIC_WORD);

            TokenStringIndex = Lexer->TokenType - SHELL_ARITHMETIC_WORD;
            ShPrintTrace(Shell,
                         "%25s: %s\n",
                         ShArithmeticTokenStrings[TokenStringIndex],
                         Lexer->TokenBuffer);
        }
    }

    return TRUE;
}

BOOL
ShAddCharacterToArithmeticToken (
    PSHELL_ARITHMETIC_LEXER Lexer,
    CHAR Character
    )

/*++

Routine Description:

    This routine adds the given character to the token buffer, expanding it if
    necessary.

Arguments:

    Lexer - Supplies a pointer to the lexer to operate on.

    Character - Supplies the character to add.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PVOID NewBuffer;
    UINTN NewCapacity;

    if (Lexer->TokenBufferSize < Lexer->TokenBufferCapacity) {
        Lexer->TokenBuffer[Lexer->TokenBufferSize] = Character;
        Lexer->TokenBufferSize += 1;
        return TRUE;
    }

    //
    // Bummer, the buffer needs to be reallocated.
    //

    NewCapacity = Lexer->TokenBufferCapacity * 2;
    NewBuffer = realloc(Lexer->TokenBuffer, NewCapacity);
    if (NewBuffer == NULL) {
        printf("Error: Failed to allocate %ld bytes for expanded token "
               "buffer.\n",
               NewCapacity);

        return FALSE;
    }

    Lexer->TokenBuffer = NewBuffer;

    //
    // Now add the byte.
    //

    Lexer->TokenBufferCapacity = NewCapacity;
    Lexer->TokenBuffer[Lexer->TokenBufferSize] = Character;
    Lexer->TokenBufferSize += 1;
    return TRUE;
}

