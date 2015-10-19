/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    visit.h

Abstract:

    This header contains function prototypes for all the visit node functions.

Author:

    Evan Green 15-Oct-2015

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

typedef
INT
(*PSETUP_NODE_VISIT) (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine is called to visit a particular node in the Abstract Syntax
    Tree for the setup script interpreter.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
SetupVisitListElementList (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a list element list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitList (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a list constant.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitDictElement (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a dictionary element.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitDictElementList (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a dictionary element list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitDict (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a dictionary constant.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitPrimaryExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a primary expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitStatementList (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a statement list.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitTranslationUnit (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a translation unit.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitPostfixExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a postfix expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitUnaryExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a unary expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitUnaryOperator (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a unary operator.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitMultiplicativeExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a multiplicative expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitAdditiveExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an additive expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitShiftExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a shift expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitRelationalExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a relational expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitEqualityExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an equality expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitAndExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an and expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitExclusiveOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an exclusive or expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitInclusiveOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an inclusive or expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitLogicalAndExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a logical and expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitLogicalOrExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a logical or expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitConditionalExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates a conditional expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitAssignmentExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an assignment expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitAssignmentOperator (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an assignment operator.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitExpression (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an expression.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/

INT
SetupVisitExpressionStatement (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_NODE Node,
    PSETUP_OBJECT *Result
    );

/*++

Routine Description:

    This routine evaluates an expression statement.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Node - Supplies a pointer to the node.

    Result - Supplies a pointer where a pointer to the evaluation will be
        returned. It is the caller's responsibility to release this reference.

Return Value:

    0 on success.

    Returns an error number on catastrophic failure.

--*/
