/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    int.c

Abstract:

    This module contains the builtin primitive functions for the integer class.

Author:

    Evan Green 12-Jul-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>
#include <stdlib.h>

#include "chalkp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
CkpIntFromString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntSubtract (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntMultiply (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntDivide (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntModulo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntAnd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntOr (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntXor (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntLeftShift (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntRightShift (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntLessThan (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntLessOrEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntGreaterThan (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntGreaterOrEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntNotEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntInclusiveRange (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntExclusiveRange (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntNegative (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntComplement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntIncrement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntDecrement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpIntToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeFrom (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeMin (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeMax (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeIsInclusive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpRangeToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

CK_VALUE
CkpIntegerToString (
    PCK_VM Vm,
    CK_INTEGER Integer
    );

//
// -------------------------------------------------------------------- Globals
//

CK_PRIMITIVE_DESCRIPTION CkIntPrimitives[] = {
    {"__add@1", CkpIntAdd},
    {"__sub@1", CkpIntSubtract},
    {"__mul@1", CkpIntMultiply},
    {"__div@1", CkpIntDivide},
    {"__mod@1", CkpIntModulo},
    {"__and@1", CkpIntAnd},
    {"__or@1", CkpIntOr},
    {"__xor@1", CkpIntXor},
    {"__leftShift@1", CkpIntLeftShift},
    {"__rightShift@1", CkpIntRightShift},
    {"__lt@1", CkpIntLessThan},
    {"__le@1", CkpIntLessOrEqualTo},
    {"__gt@1", CkpIntGreaterThan},
    {"__ge@1", CkpIntGreaterOrEqualTo},
    {"__eq@1", CkpIntEqualTo},
    {"__ne@1", CkpIntNotEqualTo},
    {"__rangeInclusive@1", CkpIntInclusiveRange},
    {"__rangeExclusive@1", CkpIntExclusiveRange},
    {"__neg@0", CkpIntNegative},
    {"__lnot@0", CkpIntLogicalNot},
    {"__compl@0", CkpIntComplement},
    {"__inc@0", CkpIntIncrement},
    {"__dec@0", CkpIntDecrement},
    {"__str@0", CkpIntToString},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkIntStaticPrimitives[] = {
    {"fromString@1", CkpIntFromString},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkRangePrimitives[] = {
    {"from@0", CkpRangeFrom},
    {"to@0", CkpRangeTo},
    {"min@0", CkpRangeMin},
    {"max@0", CkpRangeMax},
    {"isInclusive@0", CkpRangeIsInclusive},
    {"iterate@1", CkpRangeIterate},
    {"iteratorValue@1", CkpRangeIteratorValue},
    {"__str@0", CkpRangeToString},
    {NULL, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpRangeCreate (
    PCK_VM Vm,
    CK_INTEGER From,
    CK_INTEGER To,
    BOOL Inclusive
    )

/*++

Routine Description:

    This routine creates a range object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    From - Supplies the starting value.

    To - Supplies the ending value.

    Inclusive - Supplies a boolean indicating whether the range is inclusive
        or exclusive.

Return Value:

    Returns the range value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    PCK_RANGE Range;
    CK_VALUE Value;

    Range = CkAllocate(Vm, sizeof(CK_RANGE));
    if (Range == NULL) {
        return CkNullValue;
    }

    CkpInitializeObject(Vm, &(Range->Header), CkObjectRange, Vm->Class.Range);
    CK_OBJECT_VALUE(Value, Range);
    return Value;
}

BOOL
CkpIntFromString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts a string into an integer.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PSTR AfterScan;
    CK_INTEGER Integer;
    PCK_STRING StringObject;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected a string");
        return FALSE;
    }

    StringObject = CK_AS_STRING(Arguments[1]);
    Integer = strtoll(StringObject->Value, &AfterScan, 0);
    if (AfterScan != StringObject->Value + StringObject->Length) {
        CkpRuntimeError(Vm, "Cannot convert string to integer");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0], Integer);
    return TRUE;
}

BOOL
CkpIntAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine adds two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) + CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntSubtract (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine subtract two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) - CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntMultiply (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine multiplies two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) * CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntDivide (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine divides two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) / CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntModulo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine computes the modulus of an integer.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) % CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntAnd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine ANDs two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) & CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntOr (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine ORs two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) | CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntXor (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine XORx two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) ^ CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntLeftShift (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine shifts an integer left.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) << CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntRightShift (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine shifts an integer right.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) >> CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntLessThan (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is less than the
    second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) < CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntLessOrEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is less than or
    equal to the second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) <= CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntGreaterThan (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is greater than
    the second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) > CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntGreaterOrEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is greater than
    or equal to the second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) >= CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is equal to the
    second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        Arguments[0] = CkZeroValue;
        return TRUE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) == CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntNotEqualTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine evaluates to non-zero if the first argument is not equal to
    the second.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_INTEGER(Arguments[1])) {
        Arguments[0] = CkOneValue;
        return TRUE;
    }

    CK_INT_VALUE(Arguments[0],
                 CK_AS_INTEGER(Arguments[0]) != CK_AS_INTEGER(Arguments[1]));

    return TRUE;
}

BOOL
CkpIntInclusiveRange (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine creates an inclusive range object from the given two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE RangeValue;

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    RangeValue = CkpRangeCreate(Vm,
                                CK_AS_INTEGER(Arguments[0]),
                                CK_AS_INTEGER(Arguments[1]),
                                TRUE);

    Arguments[0] = RangeValue;
    return TRUE;
}

BOOL
CkpIntExclusiveRange (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine creates an exclusive range object from the given two integers.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE RangeValue;

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    RangeValue = CkpRangeCreate(Vm,
                                CK_AS_INTEGER(Arguments[0]),
                                CK_AS_INTEGER(Arguments[1]),
                                FALSE);

    Arguments[0] = RangeValue;
    return TRUE;
}

BOOL
CkpIntNegative (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the negative of the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INT_VALUE(Arguments[0], -CK_AS_INTEGER(Arguments[0]));
    return TRUE;
}

BOOL
CkpIntLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the logical NOT of the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INT_VALUE(Arguments[0], !CK_AS_INTEGER(Arguments[0]));
    return TRUE;
}

BOOL
CkpIntComplement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the bitwise NOT of the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INT_VALUE(Arguments[0], ~CK_AS_INTEGER(Arguments[0]));
    return TRUE;
}

BOOL
CkpIntIncrement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine increments the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INT_VALUE(Arguments[0], CK_AS_INTEGER(Arguments[0]) + 1);
    return TRUE;
}

BOOL
CkpIntDecrement (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine decrements the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INT_VALUE(Arguments[0], CK_AS_INTEGER(Arguments[0]) - 1);
    return TRUE;
}

BOOL
CkpIntToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts the given integer into a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE Value;

    Value = CkpIntegerToString(Vm, CK_AS_INTEGER(Arguments[0]));
    Arguments[0] = Value;
    return TRUE;
}

BOOL
CkpRangeFrom (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the left boundary of the given range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    CK_INT_VALUE(Arguments[0], Range->From.Int);
    return TRUE;
}

BOOL
CkpRangeTo (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the right boundary of the given range, which may be
    inclusive or exclusive.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    CK_INT_VALUE(Arguments[0], Range->To.Int);
    return TRUE;
}

BOOL
CkpRangeMin (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the lower of the from or to values of the range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    if (Range->From.Int < Range->To.Int) {
        CK_INT_VALUE(Arguments[0], Range->From.Int);

    } else {
        CK_INT_VALUE(Arguments[0], Range->To.Int);
    }

    return TRUE;
}

BOOL
CkpRangeMax (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the greater of the from or to values of the range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    if (Range->From.Int > Range->To.Int) {
        CK_INT_VALUE(Arguments[0], Range->From.Int);

    } else {
        CK_INT_VALUE(Arguments[0], Range->To.Int);
    }

    return TRUE;
}

BOOL
CkpRangeIsInclusive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns whether or not the given range is inclusive.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    CK_INT_VALUE(Arguments[0], Range->Inclusive);
    return TRUE;
}

BOOL
CkpRangeIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine creates or advances a range iterator.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER Integer;
    PCK_RANGE Range;

    Range = CK_AS_RANGE(Arguments[0]);
    if ((Range->From.Int == Range->To.Int) && (Range->Inclusive == FALSE)) {
        Arguments[0] = CkNullValue;
        return TRUE;
    }

    //
    // If null was passed in, return the initial iterator.
    //

    if (CK_IS_NULL(Arguments[1])) {
        CK_INT_VALUE(Arguments[0], Range->From.Int);
        return TRUE;
    }

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected an integer.");
        return FALSE;
    }

    //
    // Advance the iterator value.
    //

    Integer = CK_AS_INTEGER(Arguments[1]);
    if (Range->From.Int < Range->To.Int) {
        Integer += 1;
        if (Integer > Range->To.Int) {
            Arguments[0] = CkNullValue;
            return TRUE;
        }

    } else {
        Integer -= 1;
        if (Integer < Range->To.Int) {
            Arguments[0] = CkNullValue;
            return TRUE;
        }
    }

    //
    // If it's at the destination, whether it's returned depends on the
    // inclusiveness.
    //

    if ((Integer == Range->To.Int) && (Range->Inclusive == FALSE)) {
        Arguments[0] = CkNullValue;
        return TRUE;
    }

    CK_INT_VALUE(Arguments[0], Integer);
    return TRUE;
}

BOOL
CkpRangeIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the actual iterator value for the particular iterator
    position.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    //
    // The iterator is just the number itself, so simply return it.
    //

    Arguments[0] = Arguments[1];
    return TRUE;
}

BOOL
CkpRangeToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts a range to a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PSTR Dots;
    CK_VALUE From;
    PCK_RANGE Range;
    CK_VALUE To;

    Range = CK_AS_RANGE(Arguments[0]);
    From = CkpIntegerToString(Vm, Range->From.Int);
    if (!CK_IS_OBJECT(From)) {
        return FALSE;
    }

    CkpPushRoot(Vm, CK_AS_OBJECT(From));
    To = CkpIntegerToString(Vm, Range->To.Int);
    if (!CK_IS_OBJECT(To)) {
        CkpPopRoot(Vm);
        return FALSE;
    }

    CkpPushRoot(Vm, CK_AS_OBJECT(To));
    Dots = "..";
    if (Range->Inclusive != FALSE) {
        Dots = "...";
    }

    Arguments[0] = CkpStringFormat(Vm, "@$@", From, Dots, To);
    CkpPopRoot(Vm);
    CkpPopRoot(Vm);
    return TRUE;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_VALUE
CkpIntegerToString (
    PCK_VM Vm,
    CK_INTEGER Integer
    )

/*++

Routine Description:

    This routine converts an integer to a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to convert.

Return Value:

    Returns the string value on success.

    CK_NULL_VALUE on allocation failure.

--*/

{

    CHAR Buffer[100];
    UINTN Length;

    Length = snprintf(Buffer, sizeof(Buffer), "%lld", (LONGLONG)Integer);
    return CkpStringCreate(Vm, Buffer, Length);
}

