/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    expr.c

Abstract:

    This module implements the "expr" (evaluate expression) utility.

Author:

    Evan Green 16-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EXPR_INTEGER_STRING_SIZE 64

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EXPR_OPERATOR {
    ExprInvalid,
    ExprOr,
    ExprAnd,
    ExprEqual,
    ExprGreaterThan,
    ExprGreaterThanOrEqual,
    ExprLessThan,
    ExprLessThanOrEqual,
    ExprNotEqual,
    ExprPlus,
    ExprMinus,
    ExprMultiply,
    ExprDivide,
    ExprModulo,
    ExprMatch,
} EXPR_OPERATOR, *PEXPR_OPERATOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
ExprEvaluate (
    PINT ArgumentIndex,
    INT ArgumentCount,
    CHAR **Arguments,
    PSTR *ResultValue
    );

EXPR_OPERATOR
ExprGetOperator (
    PSTR Argument,
    PULONG Precedence
    );

INT
ExprEvaluateOperator (
    PSTR Left,
    EXPR_OPERATOR Operator,
    PSTR Right,
    PSTR *AnswerString
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
ExprMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the expr utility.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 if the expression returned true, non-null, or non-zero.

    1 if the expression returned false.

    Other codes on failure.

--*/

{

    PSTR Answer;
    INT Index;
    INT Status;

    if ((ArgumentCount > 1) && (strcmp(Arguments[1], "--") == 0)) {
        Arguments += 1;
        ArgumentCount -= 1;
    }

    if (ArgumentCount == 1) {
        SwPrintError(0, NULL, "Invalid argument count");
        Status = EINVAL;
        goto MainEnd;
    }

    Index = 1;
    if (ArgumentCount == 2) {
        Answer = Arguments[1];
        Status = 0;

    } else {
        Status = ExprEvaluate(&Index, ArgumentCount, Arguments, &Answer);
        if (Status != 0) {
            goto MainEnd;
        }
    }

    printf("%s\n", Answer);
    if ((*Answer == '\0') || (strcmp(Answer, "0") == 0)) {
        Status = 1;
    }

    if (ArgumentCount != 2) {
        free(Answer);
    }

MainEnd:

    //
    // Expr returns 0 if the result is non-zero and non-null, 1 if the result
    // is 0 or null, 2 if the expression is invalid, and 3 on any other error.
    //

    if (Status > 1) {
        if (Status == EINVAL) {
            Status = 2;

        } else {
            Status = 3;
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
ExprEvaluate (
    PINT ArgumentIndex,
    INT ArgumentCount,
    CHAR **Arguments,
    PSTR *ResultValue
    )

/*++

Routine Description:

    This routine evaluates an expression for the expr routine.

Arguments:

    ArgumentIndex - Supplies a pointer that on input contains the starting
        argument to evaluate. This will be updated to reflect the arguments
        evaluated.

    ArgumentCount - Supplies the number of arguments in the arguments array.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

    ResultValue - Supplies a pointer where a string will be returned containing
        the result value. The caller is responsible for freeing this string.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Answer;
    INT Index;
    PSTR Left;
    BOOL LeftAllocated;
    EXPR_OPERATOR NextOperator;
    ULONG NextOperatorPrecedence;
    EXPR_OPERATOR Operator;
    ULONG OperatorPrecedence;
    PSTR Right;
    BOOL RightAllocated;
    INT Status;

    Answer = NULL;
    LeftAllocated = FALSE;
    RightAllocated = FALSE;
    Index = *ArgumentIndex;
    if (Index >= ArgumentCount) {
        SwPrintError(0, NULL, "Syntax error");
        Status = EINVAL;
        goto EvaluateEnd;
    }

    Status = 0;
    Left = Arguments[Index];
    Index += 1;
    if (strcmp(Left, "(") == 0) {
        Status = ExprEvaluate(&Index, ArgumentCount, Arguments, &Left);
        if (Status != 0) {
            goto EvaluateEnd;
        }

        LeftAllocated = TRUE;
        Answer = Left;
    }

    while (Index < ArgumentCount) {

        //
        // If the next token is a close parentheses, finish this.
        //

        if (strcmp(Arguments[Index], ")") == 0) {
            if (LeftAllocated != FALSE) {
                Answer = Left;

            } else {
                Answer = SwStringDuplicate(Left, strlen(Left) + 1);
                if (Answer == NULL) {
                    Status = ENOMEM;
                    goto EvaluateEnd;
                }
            }

            Index += 1;
            break;
        }

        Operator = ExprGetOperator(Arguments[Index], &OperatorPrecedence);
        if (Operator == ExprInvalid) {
            SwPrintError(0, Arguments[Index], "Invalid operator");
            Status = EINVAL;
            goto EvaluateEnd;
        }

        Index += 1;
        if (Index == ArgumentCount) {
            SwPrintError(0, NULL, "Missing operand");
            Status = EINVAL;
            goto EvaluateEnd;
        }

        //
        // If the right side is an open parentheses, recurse to figure out
        // what it is.
        //

        RightAllocated = FALSE;
        if (strcmp(Arguments[Index], "(") == 0) {
            Index += 1;
            Status = ExprEvaluate(&Index, ArgumentCount, Arguments, &Right);
            if (Status != 0) {
                goto EvaluateEnd;
            }

            RightAllocated = TRUE;

        //
        // If there are more arguments, get the next operator and figure out if
        // it's of higher precedence than this one. If it is, recurse to
        // figure out that answer first, and then use it as the right operand.
        //

        } else if (ArgumentCount - Index > 1) {
            NextOperator = ExprGetOperator(Arguments[Index + 1],
                                           &NextOperatorPrecedence);

            if ((NextOperator != ExprInvalid) &&
                (NextOperatorPrecedence > OperatorPrecedence)) {

                Status = ExprEvaluate(&Index, ArgumentCount, Arguments, &Right);
                if (Status != 0) {
                    goto EvaluateEnd;
                }

                RightAllocated = TRUE;
            }
        }

        if (RightAllocated == FALSE) {
            Right = Arguments[Index];
            Index += 1;
        }

        //
        // Evaluate the expression.
        //

        Status = ExprEvaluateOperator(Left, Operator, Right, &Answer);
        if (Status != 0) {
            goto EvaluateEnd;
        }

        //
        // If left or right was allocated, free them.
        //

        if (LeftAllocated != FALSE) {
            free(Left);
        }

        if (RightAllocated != FALSE) {
            free(Right);
        }

        //
        // Set the answer to be the new left.
        //

        Left = Answer;
        LeftAllocated = TRUE;
    }

EvaluateEnd:
    *ArgumentIndex = Index;
    *ResultValue = Answer;
    return Status;
}

EXPR_OPERATOR
ExprGetOperator (
    PSTR Argument,
    PULONG Precedence
    )

/*++

Routine Description:

    This routine returns the operator for the given argument.

Arguments:

    Argument - Supplies a pointer to the string containing the operator.

    Precedence - Supplies a pointer where the precedence of the operator will
        be returned. Higher values are more tightly binding.

Return Value:

    Returns the operator on success.

    Invalid on failure.

--*/

{

    EXPR_OPERATOR Operator;

    Operator = ExprInvalid;
    *Precedence = 0;
    if (strcmp(Argument, "|") == 0) {
        Operator = ExprOr;
        *Precedence = 1;

    } else if (strcmp(Argument, "&") == 0) {
        Operator = ExprAnd;
        *Precedence = 2;

    } else if (strcmp(Argument, "=") == 0) {
        Operator = ExprEqual;
        *Precedence = 3;

    } else if (strcmp(Argument, ">") == 0) {
        Operator = ExprGreaterThan;
        *Precedence = 3;

    } else if (strcmp(Argument, ">=") == 0) {
        Operator = ExprGreaterThanOrEqual;
        *Precedence = 3;

    } else if (strcmp(Argument, "<") == 0) {
        Operator = ExprLessThan;
        *Precedence = 3;

    } else if (strcmp(Argument, "<=") == 0) {
        Operator = ExprLessThanOrEqual;
        *Precedence = 3;

    } else if (strcmp(Argument, "!=") == 0) {
        Operator = ExprNotEqual;
        *Precedence = 3;

    } else if (strcmp(Argument, "+") == 0) {
        Operator = ExprPlus;
        *Precedence = 4;

    } else if (strcmp(Argument, "-") == 0) {
        Operator = ExprMinus;
        *Precedence = 4;

    } else if (strcmp(Argument, "*") == 0) {
        Operator = ExprMultiply;
        *Precedence = 5;

    } else if (strcmp(Argument, "/") == 0) {
        Operator = ExprDivide;
        *Precedence = 5;

    } else if (strcmp(Argument, "%") == 0) {
        Operator = ExprModulo;
        *Precedence = 5;

    } else if (strcmp(Argument, ":") == 0) {
        Operator = ExprMatch;
        *Precedence = 6;
    }

    return Operator;
}

INT
ExprEvaluateOperator (
    PSTR Left,
    EXPR_OPERATOR Operator,
    PSTR Right,
    PSTR *AnswerString
    )

/*++

Routine Description:

    This routine evaluates a single operator.

Arguments:

    Left - Supplies a pointer to the left operand.

    Operator - Supplies the operator to evaluate.

    Right - Supplies a pointer to the right operand.

    AnswerString - Supplies a pointer where a pointer to the newly allocated
        answer string will be returned on success. The caller is responsible for
        freeing this string.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    PSTR Answer;
    PSTR ErrorString;
    size_t ErrorStringSize;
    BOOL GotIntegerAnswer;
    BOOL GotIntegers;
    INT IntegerAnswer;
    BOOL IntegersOptional;
    BOOL IntegersRequired;
    INT LeftInteger;
    regmatch_t Match[2];
    regex_t RegularExpression;
    INT RightInteger;
    INT Status;

    Answer = NULL;
    LeftInteger = 0;
    IntegerAnswer = 0;
    GotIntegerAnswer = FALSE;
    RightInteger = 0;
    Status = 0;
    IntegersRequired = FALSE;
    IntegersOptional = FALSE;
    if ((Operator == ExprPlus) || (Operator == ExprMinus) ||
        (Operator == ExprMultiply) || (Operator == ExprDivide) ||
        (Operator == ExprModulo)) {

        IntegersRequired = TRUE;
    }

    if ((Operator == ExprEqual) || (Operator == ExprGreaterThan) ||
        (Operator == ExprGreaterThanOrEqual) || (Operator == ExprLessThan) ||
        (Operator == ExprLessThanOrEqual) || (Operator == ExprNotEqual)) {

        IntegersOptional = TRUE;
    }

    //
    // Try to convert the arguments to integers if the operator is interested.
    //

    GotIntegers = FALSE;
    if ((IntegersRequired != FALSE) || (IntegersOptional != FALSE)) {
        GotIntegers = TRUE;
        LeftInteger = strtol(Left, &AfterScan, 10);
        if ((AfterScan == Left) || (*AfterScan != '\0')) {
            if (IntegersRequired != FALSE) {
                SwPrintError(0, Left, "Invalid number");
                Status = EINVAL;
                goto EvaluateOperatorEnd;
            }

            GotIntegers = FALSE;
        }

        RightInteger = strtol(Right, &AfterScan, 10);
        if ((AfterScan == Left) || (*AfterScan != '\0')) {
            if (IntegersRequired != FALSE) {
                SwPrintError(0, Right, "Invalid number");
                Status = EINVAL;
                goto EvaluateOperatorEnd;
            }

            GotIntegers = FALSE;
        }
    }

    switch (Operator) {

    //
    // Return the left if it is not null or zero, or the right if it is not
    // null or zero, or 0 if both are zero.
    //

    case ExprOr:
        if ((*Left != '\0') &&
            ((GotIntegers == FALSE) || (LeftInteger != 0))) {

            Answer = SwStringDuplicate(Left, strlen(Left) + 1);

        } else if ((*Right != '\0') &&
                   ((GotIntegers == FALSE) || (RightInteger != 0))) {

            Answer = SwStringDuplicate(Right, strlen(Right) + 1);

        } else {
            IntegerAnswer = 0;
            GotIntegerAnswer = TRUE;
        }

        break;

    //
    // Returns left if neither expression evaluates to zero; otherwise,
    // returns zero.
    //

    case ExprAnd:
        if ((*Left != '\0') &&
            ((GotIntegers == FALSE) || (LeftInteger != 0)) &&
            (*Right != '\0') &&
            ((GotIntegers == FALSE) || (RightInteger != 0))) {

            Answer = SwStringDuplicate(Left, strlen(Left) + 1);

        } else {
            GotIntegerAnswer = TRUE;
        }

        break;

    case ExprEqual:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger == RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) == 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprGreaterThan:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger > RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) > 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprGreaterThanOrEqual:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger >= RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) >= 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprLessThan:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger < RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) < 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprLessThanOrEqual:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger <= RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) <= 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprNotEqual:
        GotIntegerAnswer = TRUE;
        if (GotIntegers != FALSE) {
            if (LeftInteger != RightInteger) {
                IntegerAnswer = 1;
            }

        } else {
            if (strcmp(Left, Right) != 0) {
                IntegerAnswer = 1;
            }
        }

        break;

    case ExprPlus:
        GotIntegerAnswer = TRUE;
        IntegerAnswer = LeftInteger + RightInteger;
        break;

    case ExprMinus:
        GotIntegerAnswer = TRUE;
        IntegerAnswer = LeftInteger - RightInteger;
        break;

    case ExprMultiply:
        GotIntegerAnswer = TRUE;
        IntegerAnswer = LeftInteger * RightInteger;
        break;

    case ExprDivide:
        GotIntegerAnswer = TRUE;
        if (RightInteger == 0) {
            SwPrintError(0, NULL, "Divide by zero");
            Status = EDOM;
            goto EvaluateOperatorEnd;
        }

        IntegerAnswer = LeftInteger / RightInteger;
        break;

    case ExprModulo:
        GotIntegerAnswer = TRUE;
        IntegerAnswer = LeftInteger % RightInteger;
        break;

    //
    // Attempt to match the left string with the regular expression in the
    // right argument. The match is only successful if it matches the beginning.
    //

    case ExprMatch:
        Status = regcomp(&RegularExpression, Right, 0);
        if (Status != 0) {
            ErrorStringSize = regerror(Status, &RegularExpression, NULL, 0);
            ErrorString = malloc(ErrorStringSize);
            if (ErrorString != NULL) {
                regerror(Status,
                         &RegularExpression,
                         ErrorString,
                         ErrorStringSize);

                SwPrintError(0,
                             Right,
                             "Invalid regular expression: %s",
                             ErrorString);

                free(ErrorString);
            }

            goto EvaluateOperatorEnd;
        }

        Status = regexec(&RegularExpression, Left, 2, Match, 0);

        //
        // If there's a subexpression, return the first one.
        //

        if (RegularExpression.re_nsub != 0) {

            //
            // Return the empty string if it didn't match.
            //

            if ((Status != 0) || (Match[0].rm_so != 0) ||
                (Match[1].rm_so == -1)) {

                Answer = strdup("");

            //
            // Return the first subgroup if it did match.
            //

            } else {

                assert(Match[0].rm_eo != -1);

                Answer = malloc(Match[1].rm_eo - Match[1].rm_so + 1);
                if (Answer == NULL) {
                    Status = ENOMEM;
                    goto EvaluateOperatorEnd;
                }

                memcpy(Answer,
                       Left + Match[1].rm_so,
                       Match[1].rm_eo - Match[1].rm_so);

                Answer[Match[1].rm_eo - Match[1].rm_so] = '\0';
            }

        //
        // There were no subexpressions, return the number of characters that
        // matched.
        //

        } else {
            GotIntegerAnswer = TRUE;
            if (Match[0].rm_so == 0) {
                IntegerAnswer = Match[0].rm_eo - Match[0].rm_so;
            }
        }

        regfree(&RegularExpression);
        Status = 0;
        break;

    default:

        assert(FALSE);

        Status = EINVAL;
        goto EvaluateOperatorEnd;
    }

    //
    // If there's an integer answer, create a string for it.
    //

    if (GotIntegerAnswer != FALSE) {
        Answer = malloc(EXPR_INTEGER_STRING_SIZE);
        if (Answer == NULL) {
            Status = ENOMEM;
            goto EvaluateOperatorEnd;
        }

        snprintf(Answer,
                 EXPR_INTEGER_STRING_SIZE,
                 "%d",
                 IntegerAnswer);

    } else {

        assert(Answer != NULL);

    }

EvaluateOperatorEnd:
    *AnswerString = Answer;
    return Status;
}

