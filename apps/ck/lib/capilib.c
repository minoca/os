/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    capilib.c

Abstract:

    This module implements higher level helper functions on top of the base
    Chalk C API.

Author:

    Evan Green 20-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include <stdarg.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

PCSTR CkApiTypeNames[CkTypeCount] = {
    "INVALID",  // CkTypeInvalid
    "null",     // CkTypeNull
    "integer",  // CkTypeInteger
    "string",   // CkTypeString
    "dict",     // CkTypeDict
    "list",     // CkTypeList
    "function", // CkTypeFunction
    "object",   // CkTypeObject
};

//
// ------------------------------------------------------------------ Functions
//

CK_API
BOOL
CkCheckArguments (
    PCK_VM Vm,
    UINTN Count,
    ...
    )

/*++

Routine Description:

    This routine validates that the given arguments are of the correct type. If
    any of them are not, it throws a nicely formatted error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Count - Supplies the number of arguments coming next.

    ... - Supplies the remaining type arguments.

Return Value:

    TRUE if the given arguments match the required type.

    FALSE if an argument is not of the right type. In that case, an error
    will be created.

--*/

{

    va_list ArgumentList;
    INTN Index;
    CK_API_TYPE Type;
    BOOL Valid;

    Valid = TRUE;
    va_start(ArgumentList, Count);
    for (Index = 0; Index < Count; Index += 1) {
        Type = va_arg(ArgumentList, CK_API_TYPE);
        Valid = CkCheckArgument(Vm, Index + 1, Type);
        if (Valid == FALSE) {
            break;
        }
    }

    va_end(ArgumentList);
    return Valid;
}

CK_API
BOOL
CkCheckArgument (
    PCK_VM Vm,
    INTN StackIndex,
    CK_API_TYPE Type
    )

/*++

Routine Description:

    This routine validates that the given argument is of the correct type. If
    it is not, it throws a nicely formatted error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index to check. Remember that 1 is the
        first argument index.

    Type - Supplies the type to check.

Return Value:

    TRUE if the given argument matches the required type.

    FALSE if the argument is not of the right type. In that case, an error
    will be created.

--*/

{

    PCK_CLOSURE Closure;
    CK_VALUE Error;
    PCK_FIBER Fiber;
    CK_API_TYPE FoundType;
    PCK_CALL_FRAME Frame;
    PSTR Name;

    FoundType = CkGetType(Vm, StackIndex);

    CK_ASSERT((FoundType < CkTypeCount) && (Type < CkTypeCount));

    if (FoundType == Type) {
        return TRUE;
    }

    Fiber = Vm->Fiber;

    CK_ASSERT((Fiber != NULL) && (Fiber->FrameCount != 0));

    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
    Closure = Frame->Closure;
    Name = CkpGetFunctionName(Closure);
    Error = CkpStringFormat(Vm,
                            "%s expects %s for argument %d, got %s",
                            Name,
                            CkApiTypeNames[Type],
                            (INT)StackIndex,
                            CkApiTypeNames[FoundType]);

    Fiber->Error = Error;
    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

