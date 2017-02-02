/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    test.c

Abstract:

    This module implements the test (aka left square bracket [) program.

Author:

    Evan Green 15-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_VERSION_MAJOR 1
#define TEST_VERSION_MINOR 0

#define TEST_USAGE                                                             \
    "usage: test [<test>]\n"                                                   \
    "       [ <test> ]\n"                                                      \
    "The test utility performs basic file, integer, and string tests to \n"    \
    "augment the shell's functionality. Options are: \n"                       \
    "  --help -- Show this help and exit.\n"                                   \
    "  --version -- Show the application version and exit.\n\n"                \
    "Valid tests are:\n"                                                       \
    "  -b file -- The file exists and is a block device.\n"                    \
    "  -c file -- The file exists and is a character device.\n"                \
    "  -d file -- The file exists and is a directory.\n"                       \
    "  -f file -- The file exists and is a regular file.\n"                    \
    "  -g file -- The file exists and has its set-group-ID flag set.\n"        \
    "  -h file -- The file exists and is a symbolic link.\n"                   \
    "  -L file -- The file exists and is a symbolic link (same as -h).\n"      \
    "  -p file -- The file exists and is a FIFO.\n"                            \
    "  -r file -- The file exists and is readable.\n"                          \
    "  -S file -- The file exists and is a socket.\n"                          \
    "  -s file -- The file exists and has a size greater than zero.\n"         \
    "  -t file_descriptor -- The file descriptor is valid and points to a \n"  \
    "      terminal device.\n"                                                 \
    "  -u file -- The file exists and has its set-user-ID flag set.\n"         \
    "  -w file -- The file exists and is writable.\n"                          \
    "  -x file -- The file exists and is executable.\n"                        \
    "  file1 -ef file2 -- True if file1 and file2 have the same device and \n" \
    "      file serial numbers.\n"                                             \
    "  file1 -nt file2 -- True if file1 has a later modification date than \n" \
    "      file2.\n"                                                           \
    "  file1 -ot file2 -- True if file1 has an earlier modification date \n"   \
    "      than file2.\n"                                                      \
    "  -n string -- True if the length of the given string is non-zero.\n"     \
    "  -z string -- True if the length of the string is zero.\n"               \
    "  string -- True if the string is not the null string.\n"                 \
    "  string1 = string2 -- True if the two strings are identical.\n"          \
    "  string1 != string2 -- True if the two strings are not identical.\n"     \
    "  number1 -eq number2 -- True if the two numbers are equal.\n"            \
    "  number1 -ne number2 -- True if the two numbers are not equal.\n"        \
    "  number1 -gt number2 -- True if number1 is greater than number2.\n"      \
    "  number1 -ge number2 -- True if number1 is greater than or equal to \n"  \
    "      number2.\n"                                                         \
    "  number1 -lt number2 -- True if number1 is less than number2.\n"         \
    "  number1 -le number2 -- True if number1 is less than or equal to "       \
    "number2.\n\n"                                                             \
    "Additionally, tests can be combined in the following ways:\n"             \
    "  expression1 -a expression2 -- True if both expression1 and \n"          \
    "      expression2 are true. This has a higher precedence than -o.\n"      \
    "  expression1 -o expression2 -- True if either expression1 or \n"         \
    "      expression2 are true.\n"                                            \
    "  ! expression - True if the expression is false.\n"                      \
    "  ( expression ) - True if the inner expression is True. Parentheses \n"  \
    "      can be used to alter the normal associativity and precedence.\n\n"  \

//
// Define away the test utility return values since they're backwards and
// therefore a little tough to look at.
//

#define TEST_UTILITY_FALSE 1
#define TEST_UTILITY_TRUE 0
#define TEST_UTILITY_ERROR 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TEST_UTILITY_TEST {
    TestUtilityInvalid,
    TestUtilityBang,
    TestUtilityOpenParentheses,
    TestUtilityCloseParentheses,
    TestUtilityAnd,
    TestUtilityOr,
    TestFileMinValue,
    TestFileIsBlockDevice,
    TestFileIsCharacterDevice,
    TestFileIsDirectory,
    TestFileExists,
    TestFileIsRegularFile,
    TestFileHasSetGroupId,
    TestFileIsSymbolicLink,
    TestFileIsFifo,
    TestFileCanRead,
    TestFileIsSocket,
    TestFileIsNonEmpty,
    TestFileDescriptorIsTerminal,
    TestFileHasSetUserId,
    TestFileCanWrite,
    TestFileCanExecute,
    TestFileEqual,
    TestFileNewer,
    TestFileOlder,
    TestFileMaxValue,
    TestStringMinValue,
    TestStringNonZeroLength,
    TestStringZeroLength,
    TestStringEquals,
    TestStringNotEquals,
    TestStringMaxValue,
    TestIntegerMinValue,
    TestIntegerEquals,
    TestIntegerNotEquals,
    TestIntegerGreaterThan,
    TestIntegerGreaterThanOrEqualTo,
    TestIntegerLessThan,
    TestIntegerLessThanOrEqualTo,
    TestIntegerMaxValue,
} TEST_UTILITY_TEST, *PTEST_UTILITY_TEST;

typedef enum _TEST_PARSE_ELEMENT_TYPE {
    TestParseElementInvalid,
    TestParseElementOperator,
    TestParseElementToken,
    TestParseElementResult,
    TestParseElementEnd,
} TEST_PARSE_ELEMENT_TYPE, *PTEST_PARSE_ELEMENT_TYPE;

typedef struct _TEST_PARSE_ELEMENT {
    TEST_PARSE_ELEMENT_TYPE Type;
    PSTR Token;
    TEST_UTILITY_TEST Operator;
    INT Result;
} TEST_PARSE_ELEMENT, *PTEST_PARSE_ELEMENT;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
TestEvaluateExpression (
    INT ArgumentCount,
    CHAR **Arguments
    );

BOOL
TestShiftOrReduce (
    PTEST_PARSE_ELEMENT Stack,
    ULONG StackSize,
    PTEST_PARSE_ELEMENT NextElement,
    PBOOL Shift
    );

BOOL
TestReduce (
    PTEST_PARSE_ELEMENT Stack,
    PULONG StackSize
    );

TEST_UTILITY_TEST
TestGetOperator (
    PSTR String
    );

ULONG
TestGetOperandCount (
    TEST_UTILITY_TEST Operator
    );

ULONG
TestGetOperatorPrecedence (
    TEST_UTILITY_TEST Operator
    );

INT
TestEvaluateUnaryOperator (
    TEST_UTILITY_TEST Operator,
    PSTR Operand
    );

INT
TestEvaluateBinaryOperator (
    TEST_UTILITY_TEST Operator,
    PSTR LeftOperand,
    PSTR RightOperand
    );

INT
TestEvaluateStringTest (
    TEST_UTILITY_TEST Operator,
    PSTR String1,
    PSTR String2
    );

INT
TestEvaluateIntegerTest (
    TEST_UTILITY_TEST Operator,
    PSTR LeftIntegerString,
    PSTR RightIntegerString
    );

INT
TestEvaluateAndOr (
    TEST_UTILITY_TEST Operator,
    INT Left,
    INT Right
    );

VOID
TestConvertTokenToResult (
    PTEST_PARSE_ELEMENT Element
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR TestOperatorStrings[] = {
    NULL,
    "!",
    "(",
    ")",
    "-a",
    "-o",
    NULL,
    "-b",
    "-c",
    "-d",
    "-e",
    "-f",
    "-g",
    "-h",
    "-p",
    "-r",
    "-S",
    "-s",
    "-t",
    "-u",
    "-w",
    "-x",
    "-ef",
    "-nt",
    "-ot",
    NULL,
    NULL,
    "-n",
    "-z",
    "=",
    "!=",
    NULL,
    NULL,
    "-eq",
    "-ne",
    "-gt",
    "-ge",
    "-lt",
    "-le",
    NULL,
};

BOOL TestDebugPrintEvaluations = FALSE;

//
// ------------------------------------------------------------------ Functions
//

INT
TestMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the test application entry point.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 or 1 if the evaluation succeeds.

    >1 on failure.

--*/

{

    PSTR LastSlash;
    INT Result;

    //
    // Look for --help or --version as the only argument.
    //

    if (ArgumentCount == 2) {
        if (strcmp(Arguments[1], "--help") == 0) {
            printf(TEST_USAGE);
            return TEST_UTILITY_ERROR;

        } else if (strcmp(Arguments[1], "--version") == 0) {
            SwPrintVersion(TEST_VERSION_MAJOR, TEST_VERSION_MINOR);
            return TEST_UTILITY_ERROR;
        }
    }

    //
    // Look for an open bracket.
    //

    if (ArgumentCount == 0) {
        return TEST_UTILITY_ERROR;
    }

    LastSlash = strrchr(Arguments[0], '/');
    if (LastSlash == NULL) {
        LastSlash = Arguments[0];

    } else {
        LastSlash += 1;
    }

    if (strchr(LastSlash, '[') != NULL) {

        //
        // There had better be a closing bracket.
        //

        if (strcmp(Arguments[ArgumentCount - 1], "]") != 0) {
            SwPrintError(0, Arguments[ArgumentCount - 1], "Expected ']'");
            return TEST_UTILITY_ERROR;
        }

        ArgumentCount -= 1;
    }

    if (ArgumentCount == 0) {
        return TEST_UTILITY_FALSE;
    }

    Result = TestEvaluateExpression(ArgumentCount - 1, Arguments + 1);
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
TestEvaluateExpression (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine evaluates an expression for the test application.

Arguments:

    ArgumentCount - Supplies the number of remaining arguments on the command
        line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

    ArgumentsConsumed - Supplies a pointer where the number of arguments
        consumed will be returned.

Return Value:

    0 or 1 if the evaluation succeeds.

    >1 on failure.

--*/

{

    ULONG ArgumentIndex;
    TEST_PARSE_ELEMENT NextElement;
    TEST_UTILITY_TEST Operator;
    PTEST_PARSE_ELEMENT ParseStack;
    BOOL Result;
    INT ReturnValue;
    BOOL Shift;
    ULONG StackIndex;

    ParseStack = NULL;

    //
    // Zero arguments represents an implicit test for a non-null string
    // failing.
    //

    if (ArgumentCount == 0) {
        return TEST_UTILITY_FALSE;

    //
    // For a single argument test to see if it's non-null.
    //

    } else if (ArgumentCount == 1) {
        if (*(Arguments[0]) != '\0') {
            return TEST_UTILITY_TRUE;

        } else {
            return TEST_UTILITY_FALSE;
        }

    //
    // For two arguments, process a unary operator or a bang plus an implicit
    // non-null test.
    //

    } else if (ArgumentCount == 2) {
        Operator = TestGetOperator(Arguments[0]);
        if (Operator == TestUtilityBang) {
            if (*(Arguments[1]) != '\0') {
                return TEST_UTILITY_FALSE;

            } else {
                return TEST_UTILITY_TRUE;
            }
        }

        if ((Operator == TestUtilityInvalid) ||
            (TestGetOperandCount(Operator) != 1)) {

            SwPrintError(0, NULL, "%s: Unary operator expected", Arguments[0]);
            return TEST_UTILITY_ERROR;
        }

        return TestEvaluateUnaryOperator(Operator, Arguments[1]);

    //
    // For three arguments:
    // * If $2 is a binary primary, perform the binary test of $1 and $3.
    // * If $1 is '!', negate the two-argument test of $2 and $3.
    // * If $1 is '(' and $3 is ')', perfomr the unary test of $2.
    //

    } else if (ArgumentCount == 3) {
        Operator = TestGetOperator(Arguments[1]);
        if ((Operator != TestUtilityInvalid) &&
            (TestGetOperandCount(Operator) == 2)) {

            ReturnValue = TestEvaluateBinaryOperator(Operator,
                                                     Arguments[0],
                                                     Arguments[2]);

            return ReturnValue;

        } else {
            Operator = TestGetOperator(Arguments[0]);
            if (Operator == TestUtilityBang) {
                ReturnValue = TestEvaluateExpression(ArgumentCount - 1,
                                                     Arguments + 1);

                if (ReturnValue == TEST_UTILITY_FALSE) {
                    return TEST_UTILITY_TRUE;

                } else if (ReturnValue == TEST_UTILITY_TRUE) {
                    return TEST_UTILITY_FALSE;

                } else {
                    return ReturnValue;
                }

            } else if (Operator == TestUtilityOpenParentheses) {
                Operator = TestGetOperator(Arguments[2]);
                if (Operator != TestUtilityCloseParentheses) {
                    SwPrintError(0,
                                 NULL,
                                 "%s: Close parentheses expected",
                                 Arguments[2]);

                    return TEST_UTILITY_ERROR;
                }

                ReturnValue = TestEvaluateExpression(ArgumentCount - 2,
                                                     Arguments + 1);

                return ReturnValue;
            }
        }

    //
    // For four arguments:
    // * If $1 is '!', negate the three-argument test of $2, $3, and $4.
    // * If $1 is '(' and $4 is ')', perform the two argument test of $2 and $3.
    //

    } else if (ArgumentCount == 4) {
        Operator = TestGetOperator(Arguments[0]);
        if (Operator == TestUtilityBang) {
            ReturnValue = TestEvaluateExpression(ArgumentCount - 1,
                                                 Arguments + 1);

            if (ReturnValue == TEST_UTILITY_FALSE) {
                return TEST_UTILITY_TRUE;

            } else if (ReturnValue == TEST_UTILITY_TRUE) {
                return TEST_UTILITY_FALSE;

            } else {
                return ReturnValue;
            }

        } else if (Operator == TestUtilityOpenParentheses) {
            Operator = TestGetOperator(Arguments[3]);
            if (Operator == TestUtilityCloseParentheses) {
                ReturnValue = TestEvaluateExpression(ArgumentCount - 2,
                                                     Arguments + 1);

                return ReturnValue;
            }
        }
    }

    //
    // Allocate the space for a basic parse stack.
    //

    ParseStack = malloc(sizeof(TEST_PARSE_ELEMENT) * ArgumentCount);
    if (ParseStack == NULL) {
        SwPrintError(errno, NULL, "Failed to allocate");
        return TEST_UTILITY_ERROR;
    }

    memset(ParseStack, 0, sizeof(TEST_PARSE_ELEMENT) * ArgumentCount);
    ArgumentIndex = 0;
    StackIndex = 0;
    while (TRUE) {
        if (ArgumentIndex == ArgumentCount) {
            NextElement.Type = TestParseElementEnd;

        } else {

            //
            // Get the next parse stack element.
            //

            Operator = TestGetOperator(Arguments[ArgumentIndex]);
            if (Operator != TestUtilityInvalid) {
                NextElement.Type = TestParseElementOperator;
                NextElement.Operator = Operator;

            } else {
                NextElement.Type = TestParseElementToken;
            }

            NextElement.Token = Arguments[ArgumentIndex];

            //
            // Ensure that if this is a binary operator, there is a token
            // before it.
            //

            if ((Operator != TestUtilityInvalid) &&
                (TestGetOperandCount(Operator) == 2)) {

                if ((StackIndex == 0) ||
                    (ParseStack[StackIndex - 1].Type !=
                     TestParseElementToken)) {

                    SwPrintError(0,
                                 Arguments[ArgumentIndex],
                                 "Binary operator used without left argument");

                    ReturnValue = TEST_UTILITY_ERROR;
                    goto EvaluateExpressionEnd;
                }
            }
        }

        while (TRUE) {
            Result = TestShiftOrReduce(ParseStack,
                                       StackIndex,
                                       &NextElement,
                                       &Shift);

            if (Result == FALSE) {
                ReturnValue = TEST_UTILITY_ERROR;
                goto EvaluateExpressionEnd;
            }

            if (Shift != FALSE) {
                break;
            }

            Result = TestReduce(ParseStack, &StackIndex);
            if (Result == FALSE) {
                ReturnValue = TEST_UTILITY_ERROR;
                goto EvaluateExpressionEnd;
            }

            if ((NextElement.Type == TestParseElementEnd) &&
                (StackIndex == 1) &&
                (ParseStack[0].Type == TestParseElementResult)) {

                ReturnValue = ParseStack[0].Result;
                goto EvaluateExpressionEnd;
            }
        }

        //
        // Shift this next element onto the stack.
        //

        memcpy(&(ParseStack[StackIndex]),
               &NextElement,
               sizeof(TEST_PARSE_ELEMENT));

        StackIndex += 1;
        ArgumentIndex += 1;
    }

EvaluateExpressionEnd:
    if (ParseStack != NULL) {
        free(ParseStack);
    }

    return ReturnValue;
}

BOOL
TestShiftOrReduce (
    PTEST_PARSE_ELEMENT Stack,
    ULONG StackSize,
    PTEST_PARSE_ELEMENT NextElement,
    PBOOL Shift
    )

/*++

Routine Description:

    This routine determines whether the test parser should shift another
    element onto the stack or reduce what it's got.

Arguments:

    Stack - Supplies a pointer to the parse stack.

    StackSize - Supplies the number of elements on the stack.

    NextElement - Supplies a pointer to the next element that would be
        shifted onto the stack.

    Shift - Supplies a pointer where a boolean will be returned indicating
        whether or not to shift (TRUE) or reduce (FALSE).

Return Value:

    TRUE on success.

    FALSE if there was a parsing error.

--*/

{

    TEST_UTILITY_TEST NextOperator;
    TEST_UTILITY_TEST Operator;

    if (StackSize == 0) {
        *Shift = TRUE;
        return TRUE;
    }

    if (NextElement->Type == TestParseElementEnd) {
        *Shift = FALSE;
        return TRUE;
    }

    switch (Stack[StackSize - 1].Type) {
    case TestParseElementOperator:
        Operator = Stack[StackSize - 1].Operator;
        if (Operator == TestUtilityOpenParentheses) {
            *Shift = TRUE;
            return TRUE;
        }

        if (Operator == TestUtilityCloseParentheses) {
            *Shift = FALSE;
            return TRUE;
        }

        //
        // If it's an and, or, or bang, just shift.
        //

        if ((Operator == TestUtilityAnd) || (Operator == TestUtilityOr) ||
            (Operator == TestUtilityBang)) {

            *Shift = TRUE;
            return TRUE;
        }

        //
        // If it's an operator, then all other operators are turned off
        // except for bang.
        //

        if ((NextElement->Type == TestParseElementOperator) &&
            (NextElement->Operator == TestUtilityBang)) {

            *Shift = TRUE;
            return TRUE;
        }

        NextElement->Type = TestParseElementToken;
        *Shift = TRUE;
        return TRUE;

    case TestParseElementToken:

        //
        // If the next thing isn't an operator this is an error.
        //

        if (NextElement->Type != TestParseElementOperator) {
            SwPrintError(0,
                         NULL,
                         "Expected an operator, got %s",
                         NextElement->Token);

            return FALSE;
        }

        //
        // If there's only one element on the stack just shift. That next
        // argument had better be a binary argument.
        //

        NextOperator = NextElement->Operator;
        if (StackSize == 1) {
            if (TestGetOperandCount(NextOperator) != 2) {
                SwPrintError(0, NULL, "Expected a binary operator");
            }

            *Shift = TRUE;
            return TRUE;
        }

        //
        // There's more than one element on the stack, so get the operator
        // back there and compare with this upcoming operator.
        //

        assert(Stack[StackSize - 2].Type == TestParseElementOperator);

        Operator = Stack[StackSize - 2].Operator;
        if (Operator == TestUtilityOpenParentheses) {
            *Shift = TRUE;

        } else if (TestGetOperatorPrecedence(Operator) >=
                   TestGetOperatorPrecedence(NextOperator)) {

            *Shift = FALSE;

        } else {
            *Shift = TRUE;
        }

        return TRUE;

    case TestParseElementResult:

        //
        // If the next thing isn't an operator this is an error.
        //

        if (NextElement->Type != TestParseElementOperator) {
            SwPrintError(0,
                         NULL,
                         "Expected an operator, got %s",
                         NextElement->Token);

            return FALSE;
        }

        //
        // The upcoming operator had better be an and or an or.
        //

        NextOperator = NextElement->Operator;
        if ((NextOperator != TestUtilityAnd) &&
            (NextOperator != TestUtilityOr) &&
            (NextOperator != TestUtilityCloseParentheses)) {

            SwPrintError(0, NULL, "Expected end of expression");
            return FALSE;
        }

        if (StackSize == 1) {
            *Shift = TRUE;
            return TRUE;
        }

        //
        // The operator back there had better be an and, or, or bang.
        //

        Operator = Stack[StackSize - 2].Operator;

        assert((Operator == TestUtilityAnd) || (Operator == TestUtilityOr) ||
               (Operator == TestUtilityBang) ||
               (Operator == TestUtilityOpenParentheses));

        //
        // Figure out which is more important and shift or reduce.
        //

        if (Operator == TestUtilityOpenParentheses) {
            *Shift = TRUE;

        } else if (TestGetOperatorPrecedence(Operator) >=
                   TestGetOperatorPrecedence(NextOperator)) {

            *Shift = FALSE;

        } else {
            *Shift = TRUE;
        }

        return TRUE;

    default:
        break;
    }

    assert(FALSE);

    return FALSE;
}

BOOL
TestReduce (
    PTEST_PARSE_ELEMENT Stack,
    PULONG StackSize
    )

/*++

Routine Description:

    This routine attempts to reduce the stack by performing the topmost
    operation.

Arguments:

    Stack - Supplies a pointer to the parse stack.

    StackSize - Supplies the number of elements on the stack.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    ULONG OperandCount;
    TEST_UTILITY_TEST Operator;
    BOOL Result;
    INT ReturnValue;

    assert(*StackSize != 0);

    //
    // If there's a single element on the stack or the stack looks like
    // ( something, then just evaluate the top item.
    //

    if ((*StackSize == 1) ||
        ((Stack[*StackSize - 1].Type == TestParseElementToken) &&
         (Stack[*StackSize - 2].Type == TestParseElementOperator) &&
         (Stack[*StackSize - 2].Operator == TestUtilityOpenParentheses))) {

        if (Stack[0].Type != TestParseElementToken) {
            SwPrintError(0, NULL, "Expected token");
            return FALSE;
        }

        TestConvertTokenToResult(&(Stack[0]));
    }

    //
    // If the topmost thing is a close parentheses, reduce until finding the
    // corresponding open parentheses.
    //

    if (Stack[*StackSize - 1].Type == TestParseElementOperator) {
        if (Stack[*StackSize - 1].Operator != TestUtilityCloseParentheses) {
            SwPrintError(0, NULL, "Argument expected");
            return FALSE;
        }

        *StackSize -= 1;
        while (TRUE) {
            if (*StackSize < 2) {
                SwPrintError(0, NULL, "Unexpected close parentheses");
                return FALSE;
            }

            if ((Stack[*StackSize - 1].Type == TestParseElementResult) &&
                (Stack[*StackSize - 2].Type == TestParseElementOperator) &&
                (Stack[*StackSize - 2].Operator ==
                 TestUtilityOpenParentheses)) {

                *StackSize -= 1;
                Stack[*StackSize - 1].Type = TestParseElementResult;
                Stack[*StackSize - 1].Result = Stack[*StackSize].Result;
                return TRUE;
            }

            Result = TestReduce(Stack, StackSize);
            if (Result == FALSE) {
                return FALSE;
            }
        }
    }

    assert(Stack[*StackSize - 2].Type == TestParseElementOperator);

    //
    // Handle the logical operators !, AND, and OR right here.
    //

    Operator = Stack[*StackSize - 2].Operator;
    if (Operator == TestUtilityBang) {

        assert(Stack[*StackSize - 1].Type == TestParseElementResult);

        ReturnValue = Stack[*StackSize - 1].Result;
        if (ReturnValue == TEST_UTILITY_TRUE) {
            ReturnValue = TEST_UTILITY_FALSE;

        } else if (ReturnValue == TEST_UTILITY_FALSE) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        if (TestDebugPrintEvaluations != FALSE) {
            SwPrintError(0,
                         NULL,
                         "%d <== [!] %d\n",
                         ReturnValue,
                         Stack[*StackSize - 1].Result);
        }

        *StackSize -= 1;
        Stack[*StackSize - 1].Result = ReturnValue;
        Stack[*StackSize - 1].Type = TestParseElementResult;
        return TRUE;

    } else if ((Operator == TestUtilityAnd) || (Operator == TestUtilityOr)) {
        TestConvertTokenToResult(&(Stack[*StackSize - 3]));
        TestConvertTokenToResult(&(Stack[*StackSize - 1]));

        assert(Stack[*StackSize - 3].Type == TestParseElementResult);
        assert(Stack[*StackSize - 1].Type == TestParseElementResult);

        ReturnValue = TestEvaluateAndOr(Operator,
                                        Stack[*StackSize - 3].Result,
                                        Stack[*StackSize - 1].Result);

        *StackSize -= 2;
        Stack[*StackSize - 1].Result = ReturnValue;
        Stack[*StackSize - 1].Type = TestParseElementResult;
        return TRUE;
    }

    //
    // It's not logical, it must be an expression with tokens.
    //

    assert(Stack[*StackSize - 1].Type == TestParseElementToken);

    //
    // Try for a unary operator.
    //

    OperandCount = TestGetOperandCount(Operator);
    if (OperandCount == 1) {
        ReturnValue = TestEvaluateUnaryOperator(Operator,
                                                Stack[*StackSize - 1].Token);

        *StackSize -= 1;

    //
    // Process a binary operator.
    //

    } else {

        assert(OperandCount == 2);

        ReturnValue = TestEvaluateBinaryOperator(Operator,
                                                 Stack[*StackSize - 3].Token,
                                                 Stack[*StackSize - 1].Token);

        *StackSize -= 2;
    }

    Stack[*StackSize - 1].Result = ReturnValue;
    Stack[*StackSize - 1].Type = TestParseElementResult;
    return TRUE;
}

TEST_UTILITY_TEST
TestGetOperator (
    PSTR String
    )

/*++

Routine Description:

    This routine converts the given string into an operator.

Arguments:

    String - Supplies the argument string.

Return Value:

    0 or 1 if the evaluation succeeds.

    >1 on failure.

--*/

{

    if (String[0] == '\0') {
        return TestUtilityInvalid;
    }

    if ((strcmp(String, "=") == 0) || (strcmp(String, "==") == 0)) {
        return TestStringEquals;
    }

    if (strcmp(String, "(") == 0) {
        return TestUtilityOpenParentheses;
    }

    if (strcmp(String, ")") == 0) {
        return TestUtilityCloseParentheses;
    }

    if (strcmp(String, "!") == 0) {
        return TestUtilityBang;
    }

    if (strcmp(String, "!=") == 0) {
        return TestStringNotEquals;
    }

    //
    // All others start with a -.
    //

    if (String[0] != '-') {
        return TestUtilityInvalid;
    }

    String += 1;
    if (strcmp(String, "a") == 0) {
        return TestUtilityAnd;
    }

    if (strcmp(String, "b") == 0) {
        return TestFileIsBlockDevice;
    }

    if (strcmp(String, "c") == 0) {
        return TestFileIsCharacterDevice;
    }

    if (strcmp(String, "d") == 0) {
        return TestFileIsDirectory;
    }

    if (strcmp(String, "e") == 0) {
        return TestFileExists;
    }

    if (strcmp(String, "f") == 0) {
        return TestFileIsRegularFile;
    }

    if (strcmp(String, "g") == 0) {
        return TestFileHasSetGroupId;
    }

    if (strcmp(String, "h") == 0) {
        return TestFileIsSymbolicLink;
    }

    if (strcmp(String, "L") == 0) {
        return TestFileIsSymbolicLink;
    }

    if (strcmp(String, "o") == 0) {
        return TestUtilityOr;
    }

    if (strcmp(String, "p") == 0) {
        return TestFileIsFifo;
    }

    if (strcmp(String, "r") == 0) {
        return TestFileCanRead;
    }

    if (strcmp(String, "S") == 0) {
        return TestFileIsSocket;
    }

    if (strcmp(String, "s") == 0) {
        return TestFileIsNonEmpty;
    }

    if (strcmp(String, "t") == 0) {
        return TestFileDescriptorIsTerminal;
    }

    if (strcmp(String, "u") == 0) {
        return TestFileHasSetUserId;
    }

    if (strcmp(String, "w") == 0) {
        return TestFileCanWrite;
    }

    if (strcmp(String, "x") == 0) {
        return TestFileCanExecute;
    }

    if (strcmp(String, "ef") == 0) {
        return TestFileEqual;
    }

    if (strcmp(String, "nt") == 0) {
        return TestFileNewer;
    }

    if (strcmp(String, "ot") == 0) {
        return TestFileOlder;
    }

    if (strcmp(String, "n") == 0) {
        return TestStringNonZeroLength;
    }

    if (strcmp(String, "z") == 0) {
        return TestStringZeroLength;
    }

    if (strcmp(String, "eq") == 0) {
        return TestIntegerEquals;
    }

    if (strcmp(String, "ne") == 0) {
        return TestIntegerNotEquals;
    }

    if (strcmp(String, "gt") == 0) {
        return TestIntegerGreaterThan;
    }

    if (strcmp(String, "ge") == 0) {
        return TestIntegerGreaterThanOrEqualTo;
    }

    if (strcmp(String, "lt") == 0) {
        return TestIntegerLessThan;
    }

    if (strcmp(String, "le") == 0) {
        return TestIntegerLessThanOrEqualTo;
    }

    return TestUtilityInvalid;
}

ULONG
TestGetOperandCount (
    TEST_UTILITY_TEST Operator
    )

/*++

Routine Description:

    This routine returns the number of operands needed for the given operator.

Arguments:

    Operator - Supplies the operator.

Return Value:

    0 on failure.

    1 or 2 for unary and binary operators.

--*/

{

    if ((Operator > TestIntegerMinValue) && (Operator < TestIntegerMaxValue)) {
        return 2;
    }

    if ((Operator == TestStringEquals) || (Operator == TestStringNotEquals) ||
        (Operator == TestUtilityAnd) || (Operator == TestUtilityOr)) {

        return 2;
    }

    if ((Operator > TestFileMinValue) && (Operator < TestFileMaxValue)) {
        if ((Operator == TestFileEqual) || (Operator == TestFileNewer) ||
            (Operator == TestFileOlder)) {

            return 2;
        }

        return 1;
    }

    if ((Operator == TestStringZeroLength) ||
        (Operator == TestStringNonZeroLength) ||
        (Operator == TestUtilityBang)) {

        return 1;
    }

    return 0;
}

ULONG
TestGetOperatorPrecedence (
    TEST_UTILITY_TEST Operator
    )

/*++

Routine Description:

    This routine returns precedence of the given operator.

Arguments:

    Operator - Supplies the operator.

Return Value:

    Returns the precedence. Higher values are higher precedence.

--*/

{

    if (Operator == TestUtilityCloseParentheses) {
        return 0;
    }

    if (Operator == TestUtilityOr) {
        return 1;
    }

    if (Operator == TestUtilityAnd) {
        return 2;
    }

    if (Operator == TestUtilityBang) {
        return 3;
    }

    if ((Operator == TestStringEquals) || (Operator == TestStringNotEquals)) {
        return 6;
    }

    if (Operator == TestUtilityOpenParentheses) {
        return 7;
    }

    if (TestGetOperandCount(Operator) == 2) {
        return 4;
    }

    return 5;
}

INT
TestEvaluateUnaryOperator (
    TEST_UTILITY_TEST Operator,
    PSTR Operand
    )

/*++

Routine Description:

    This routine evaluates a unary operator.

Arguments:

    Operator - Supplies the operator.

    Operand - Supplies the operand.

Return Value:

    Returns the result of the operation.

--*/

{

    INT Error;
    SWISS_FILE_TEST FileTest;
    INT Result;
    INT ReturnValue;

    if ((Operator > TestFileMinValue) && (Operator < TestFileMaxValue)) {
        switch (Operator) {
        case TestFileIsBlockDevice:
            FileTest = FileTestIsBlockDevice;
            break;

        case TestFileIsCharacterDevice:
            FileTest = FileTestIsCharacterDevice;
            break;

        case TestFileIsDirectory:
            FileTest = FileTestIsDirectory;
            break;

        case TestFileExists:
            FileTest = FileTestExists;
            break;

        case TestFileIsRegularFile:
            FileTest = FileTestIsRegularFile;
            break;

        case TestFileHasSetGroupId:
            FileTest = FileTestHasSetGroupId;
            break;

        case TestFileIsSymbolicLink:
            FileTest = FileTestIsSymbolicLink;
            break;

        case TestFileIsFifo:
            FileTest = FileTestIsFifo;
            break;

        case TestFileCanRead:
            FileTest = FileTestCanRead;
            break;

        case TestFileIsSocket:
            FileTest = FileTestIsSocket;
            break;

        case TestFileIsNonEmpty:
            FileTest = FileTestIsNonEmpty;
            break;

        case TestFileDescriptorIsTerminal:
            FileTest = FileTestDescriptorIsTerminal;
            break;

        case TestFileHasSetUserId:
            FileTest = FileTestHasSetUserId;
            break;

        case TestFileCanWrite:
            FileTest = FileTestCanWrite;
            break;

        case TestFileCanExecute:
            FileTest = FileTestCanExecute;
            break;

        default:

            assert(FALSE);

            return TEST_UTILITY_ERROR;
        }

        ReturnValue = TEST_UTILITY_FALSE;
        Result = SwEvaluateFileTest(FileTest, Operand, &Error);
        if (Error != 0) {
            ReturnValue = TEST_UTILITY_ERROR;

        } else if (Result != FALSE) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

    } else if ((Operator == TestStringZeroLength) ||
               (Operator == TestStringNonZeroLength)) {

        ReturnValue = TestEvaluateStringTest(Operator, Operand, NULL);

    } else {

        assert(FALSE);

        ReturnValue = TEST_UTILITY_ERROR;
    }

    if (TestDebugPrintEvaluations != FALSE) {
        SwPrintError(0,
                     NULL,
                     "%d <== [%s] \"%s\"\n",
                     ReturnValue,
                     TestOperatorStrings[Operator],
                     Operand);
    }

    return ReturnValue;
}

INT
TestEvaluateBinaryOperator (
    TEST_UTILITY_TEST Operator,
    PSTR LeftOperand,
    PSTR RightOperand
    )

/*++

Routine Description:

    This routine evaluates a binary operator.

Arguments:

    Operator - Supplies the operator.

    LeftOperand - Supplies the left operand.

    RightOperand - Supplies the right operand.

Return Value:

    Returns the result of the operation.

--*/

{

    INT LeftResult;
    struct stat LeftStat;
    INT ReturnValue;
    INT RightResult;
    struct stat RightStat;

    if ((Operator > TestFileMinValue) && (Operator < TestFileMaxValue)) {
        memset(&LeftStat, 0, sizeof(struct stat));
        memset(&RightStat, 0, sizeof(struct stat));
        SwStat(LeftOperand, TRUE, &LeftStat);
        SwStat(RightOperand, TRUE, &RightStat);
        ReturnValue = TEST_UTILITY_FALSE;
        switch (Operator) {
        case TestFileEqual:
            if ((LeftStat.st_dev == RightStat.st_dev) &&
                (LeftStat.st_ino == RightStat.st_ino)) {

                ReturnValue = TEST_UTILITY_TRUE;

                //
                // MinGW's inodes always return zero. Compare the other fields
                // for equality. This isn't perfect as 1) files can
                // accidentally match on these fields and 2) files can change
                // in between the two stats, but what else can be done really.
                //

                if (LeftStat.st_ino == 0) {
                    if ((LeftStat.st_size != RightStat.st_size) ||
                        (LeftStat.st_mtime != RightStat.st_mtime) ||
                        (LeftStat.st_ctime != RightStat.st_ctime) ||
                        (LeftStat.st_mode != RightStat.st_mode)) {

                        ReturnValue = TEST_UTILITY_FALSE;
                    }
                }
            }

            break;

        case TestFileNewer:
            if (LeftStat.st_mtime > RightStat.st_mtime) {
                ReturnValue = TEST_UTILITY_TRUE;
            }

            break;

        case TestFileOlder:
            if (LeftStat.st_mtime < RightStat.st_mtime) {
                ReturnValue = TEST_UTILITY_TRUE;
            }

            break;

        default:

            assert(FALSE);

            ReturnValue = TEST_UTILITY_ERROR;
            break;
        }

    } else if ((Operator > TestStringMinValue) &&
               (Operator < TestStringMaxValue)) {

        ReturnValue = TestEvaluateStringTest(Operator,
                                             LeftOperand,
                                             RightOperand);

    } else if ((Operator > TestIntegerMinValue) &&
               (Operator < TestIntegerMaxValue)) {

        ReturnValue = TestEvaluateIntegerTest(Operator,
                                              LeftOperand,
                                              RightOperand);

    } else if ((Operator == TestUtilityAnd) || (Operator == TestUtilityOr)) {
        LeftResult = TEST_UTILITY_FALSE;
        RightResult = TEST_UTILITY_FALSE;
        if (*LeftOperand != '\0') {
            LeftResult = TEST_UTILITY_TRUE;
        }

        if (*RightOperand != '\0') {
            RightResult = TEST_UTILITY_TRUE;
        }

        ReturnValue = TestEvaluateAndOr(Operator, LeftResult, RightResult);

    } else {

        assert(FALSE);

        ReturnValue = TEST_UTILITY_ERROR;
    }

    if (TestDebugPrintEvaluations != FALSE) {
        SwPrintError(0,
                     NULL,
                     "%d <== \"%s\" [%s] \"%s\"\n",
                     ReturnValue,
                     LeftOperand,
                     TestOperatorStrings[Operator],
                     RightOperand);
    }

    return ReturnValue;
}

INT
TestEvaluateStringTest (
    TEST_UTILITY_TEST Operator,
    PSTR String1,
    PSTR String2
    )

/*++

Routine Description:

    This routine evaluates a string test.

Arguments:

    Operator - Supplies the operator.

    String1 - Supplies a pointer to the first string operand.

    String2 - Supplies a pointer to the second string operand. Not all
        operators require a second operand.

Return Value:

    Returns the result of the operation.

--*/

{

    INT ReturnValue;

    ReturnValue = TEST_UTILITY_FALSE;
    switch (Operator) {
    case TestStringNonZeroLength:
        if (*String1 != '\0') {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestStringZeroLength:
        if (*String1 == '\0') {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestStringEquals:

        assert((String1 != NULL) && (String2 != NULL));

        if (strcmp(String1, String2) == 0) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestStringNotEquals:

        assert((String1 != NULL) && (String2 != NULL));

        if (strcmp(String1, String2) != 0) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return ReturnValue;
}

INT
TestEvaluateIntegerTest (
    TEST_UTILITY_TEST Operator,
    PSTR LeftIntegerString,
    PSTR RightIntegerString
    )

/*++

Routine Description:

    This routine evaluates an integer test.

Arguments:

    Operator - Supplies the operator.

    LeftIntegerString - Supplies a pointer to the string containing the left
        numeric value.

    RightIntegerString - Supplies a pointer to the string containing the right
        numeric value.

Return Value:

    Returns the result of the operation.

--*/

{

    PSTR AfterScan;
    INT Left;
    INT ReturnValue;
    INT Right;

    //
    // Convert the strings to integers.
    //

    Left = strtol(LeftIntegerString, &AfterScan, 10);
    if ((Left == 0) && (AfterScan == LeftIntegerString)) {
        SwPrintError(0, NULL, "%s: Invalid integer", LeftIntegerString);
        return TEST_UTILITY_ERROR;
    }

    Right = strtol(RightIntegerString, &AfterScan, 10);
    if ((Right == 0) && (AfterScan == RightIntegerString)) {
        SwPrintError(0, NULL, "%s: Invalid integer", RightIntegerString);
        return TEST_UTILITY_ERROR;
    }

    ReturnValue = TEST_UTILITY_FALSE;
    switch (Operator) {
    case TestIntegerEquals:
        if (Left == Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestIntegerNotEquals:
        if (Left != Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestIntegerGreaterThan:
        if (Left > Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestIntegerGreaterThanOrEqualTo:
        if (Left >= Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestIntegerLessThan:
        if (Left < Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    case TestIntegerLessThanOrEqualTo:
        if (Left <= Right) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

        break;

    default:

        assert(FALSE);

        break;
    }

    return ReturnValue;
}

INT
TestEvaluateAndOr (
    TEST_UTILITY_TEST Operator,
    INT Left,
    INT Right
    )

/*++

Routine Description:

    This routine evaluates and AND (-a) or and OR (-o) operator.

Arguments:

    Operator - Supplies the operator, which should be AND or OR.

    Left - Supplies the left result, a TEST_UTILITY_* value.

    Right - Supplies the right result, a TEST_UTILITY_* value.

Return Value:

    Returns the TEST_UTILITY_* result.

--*/

{

    INT ReturnValue;

    assert((Operator == TestUtilityAnd) || (Operator == TestUtilityOr));

    ReturnValue = TEST_UTILITY_FALSE;
    if (Operator == TestUtilityAnd) {
        if ((Left == TEST_UTILITY_TRUE) && (Right == TEST_UTILITY_TRUE)) {
            ReturnValue = TEST_UTILITY_TRUE;
        }

    } else {
        if ((Left == TEST_UTILITY_TRUE) || (Right == TEST_UTILITY_TRUE)) {
            ReturnValue = TEST_UTILITY_TRUE;
        }
    }

    if (TestDebugPrintEvaluations != FALSE) {
        SwPrintError(0,
                     NULL,
                     "%d <== %d [%s] %d\n",
                     ReturnValue,
                     Left,
                     TestOperatorStrings[Operator],
                     Right);
    }

    return ReturnValue;
}

VOID
TestConvertTokenToResult (
    PTEST_PARSE_ELEMENT Element
    )

/*++

Routine Description:

    This routine converts a token type parse element into a result element,
    which is non-zero if the given token has a non-zero length.

Arguments:

    Element - Supplies a pointer to the token element to convert into a result.

Return Value:

    None.

--*/

{

    //
    // Skip it if it's already a result.
    //

    if (Element->Type == TestParseElementResult) {
        return;
    }

    //
    // Only tokens are handled here, there's no converting an operator to a
    // result.
    //

    assert(Element->Type == TestParseElementToken);

    if (*(Element->Token) != '\0') {
        Element->Result = TEST_UTILITY_TRUE;

    } else {
        Element->Result = TEST_UTILITY_FALSE;
    }

    Element->Type = TestParseElementResult;
    return;
}

