/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    value.c

Abstract:

    This module implements common functions for manipulating Chalk objects and
    values.

Author:

    Evan Green 28-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpFiberReset (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure
    );

//
// -------------------------------------------------------------------- Globals
//

const CK_VALUE CkNullValue = {CkValueNull, {0}};
const CK_VALUE CkUndefinedValue = {CkValueUndefined, {0}};
const CK_VALUE CkZeroValue = {CkValueInteger, {0}};
const CK_VALUE CkOneValue = {CkValueInteger, {1}};

//
// ------------------------------------------------------------------ Functions
//

PCK_CLOSURE
CkpClosureCreate (
    PCK_VM Vm,
    PCK_FUNCTION Function
    )

/*++

Routine Description:

    This routine creates a new closure object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function the closure encloses.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

{

    PCK_CLOSURE Closure;
    UINTN UpvalueSize;

    UpvalueSize = Function->UpvalueCount * sizeof(PCK_UPVALUE);
    Closure = CkAllocate(Vm, sizeof(CK_CLOSURE) + UpvalueSize);
    if (Closure == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(Closure->Header),
                        CkObjectClosure,
                        Vm->Class.Function);

    Closure->Function = Function;
    Closure->Upvalues = (PCK_UPVALUE *)(Closure + 1);
    CkZero(Closure->Upvalues, UpvalueSize);
    return Closure;
}

PCK_FIBER
CkpFiberCreate (
    PCK_VM Vm,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine creates a new fiber object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Closure - Supplies a pointer to the closure to execute.

Return Value:

    Returns a pointer to the new fiber on success.

    NULL on allocation failure.

--*/

{

    PCK_CALL_FRAME CallFrames;
    PCK_FIBER Fiber;
    PCK_VALUE Stack;
    UINTN StackCapacity;

    //
    // Allocate the call frames first in case it triggers a garbage collection.
    //

    CallFrames = CkAllocate(Vm, sizeof(CK_CALL_FRAME) * CK_INITIAL_CALL_FRAMES);
    if (CallFrames == NULL) {
        return NULL;
    }

    StackCapacity = CK_INITIAL_CALL_FRAMES;
    if (Closure != NULL) {
        while (StackCapacity < Closure->Function->MaxStack + 1) {
            StackCapacity <<= 1;
        }
    }

    Stack = CkAllocate(Vm, StackCapacity * sizeof(CK_VALUE));
    if (Stack == NULL) {
        CkFree(Vm, CallFrames);
        return NULL;
    }

    Fiber = CkAllocate(Vm, sizeof(CK_FIBER));
    if (Fiber == NULL) {
        CkFree(Vm, CallFrames);
        CkFree(Vm, Stack);
    }

    CkpInitializeObject(Vm, &(Fiber->Header), CkObjectFiber, Vm->Class.Fiber);
    Fiber->Frames = CallFrames;
    Fiber->FrameCapacity = CK_INITIAL_CALL_FRAMES;
    Fiber->Stack = Stack;
    Fiber->StackCapacity = StackCapacity;
    CkpFiberReset(Vm, Fiber, Closure);
    return Fiber;
}

PCK_FUNCTION
CkpFunctionCreate (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_SYMBOL_INDEX StackSize
    )

/*++

Routine Description:

    This routine creates a new function object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the function is in.

    StackSize - Supplies the number of stack slots used by the function.

Return Value:

    Returns a pointer to the new function on success.

    NULL on allocation failure.

--*/

{

    PCK_FUNCTION Function;

    Function = CkAllocate(Vm, sizeof(CK_FUNCTION));
    if (Function == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(Function->Header),
                        CkObjectFunction,
                        Vm->Class.Function);

    CkpInitializeArray(&(Function->Constants));
    CkpInitializeArray(&(Function->Code));
    Function->Module = Module;
    Function->MaxStack = StackSize;
    Function->UpvalueCount = 0;
    Function->Arity = 0;
    return Function;
}

VOID
CkpFunctionSetDebugName (
    PCK_VM Vm,
    PCK_FUNCTION Function,
    PSTR Name,
    UINTN Length
    )

/*++

Routine Description:

    This routine sets the name of the function used when printing stack traces.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function to set the name of.

    Name - Supplies a pointer to the name of the function. A copy of this
        memory will be made.

    Length - Supplies the length of the function name, not including the null
        terminator.

Return Value:

    None.

--*/

{

    CK_ASSERT(Function->Debug.Name == NULL);

    Function->Debug.Name = CkAllocate(Vm, Length + 1);
    if (Function->Debug.Name != NULL) {
        memcpy(Function->Debug.Name, Name, Length);
        Function->Debug.Name[Length] = '\0';
    }

    return;
}

VOID
CkpDestroyObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys a Chalk object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to destroy.

Return Value:

    None.

--*/

{

    PCK_FUNCTION Function;
    PCK_MODULE Module;

    switch (Object->Type) {
    case CkObjectClass:
        CkpClearArray(Vm, &(((PCK_CLASS)Object)->Methods));
        break;

    case CkObjectFiber:

        //
        // TODO: Destroy fiber.
        //

        CK_ASSERT(FALSE);

        break;

    case CkObjectFunction:
        Function = (PCK_FUNCTION)Object;
        CkpClearArray(Vm, &(Function->Constants));
        CkpClearArray(Vm, &(Function->Code));
        CkpClearArray(Vm, &(Function->Debug.LineProgram));
        CkFree(Vm, Function->Debug.Name);
        break;

    case CkObjectForeign:

        //
        // TODO: Destroy foreign object.
        //

        CK_ASSERT(FALSE);

        break;

    case CkObjectList:
        CkpClearArray(Vm, &(((PCK_LIST)Object)->Elements));
        break;

    case CkObjectDict:
        CkFree(Vm, ((PCK_DICT)Object)->Entries);
        break;

    case CkObjectModule:
        Module = (PCK_MODULE)Object;
        CkpSymbolTableClear(Vm, &(Module->VariableNames));
        CkpClearArray(Vm, &(Module->Variables));
        break;

    case CkObjectClosure:
    case CkObjectInstance:
    case CkObjectRange:
    case CkObjectString:
    case CkObjectUpvalue:
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    CkFree(Vm, Object);
    return;
}

VOID
CkpInitializeObject (
    PCK_VM Vm,
    PCK_OBJECT Object,
    CK_OBJECT_TYPE Type,
    PCK_CLASS Class
    )

/*++

Routine Description:

    This routine initializes a new Chalk object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to initialize.

    Type - Supplies the type of object being created.

    Class - Supplies a pointer to the object class.

Return Value:

    None.

--*/

{

    Object->Type = Type;
    Object->Mark = FALSE;
    Object->Class = Class;
    Object->Next = Vm->FirstObject;
    Vm->FirstObject = Object;
    return;
}

BOOL
CkpAreValuesEqual (
    CK_VALUE Left,
    CK_VALUE Right
    )

/*++

Routine Description:

    This routine determines if two objects are equal.

Arguments:

    Left - Supplies the left value.

    Right - Supplies the right value.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal.

--*/

{

    INT Compare;
    PCK_OBJECT LeftObject;
    PCK_RANGE LeftRange;
    PCK_STRING_OBJECT LeftString;
    PCK_OBJECT RightObject;
    PCK_RANGE RightRange;
    PCK_STRING_OBJECT RightString;

    if (CkpAreValuesIdentical(Left, Right) != FALSE) {
        return TRUE;
    }

    if ((!CK_IS_OBJECT(Left)) || (!CK_IS_OBJECT(Right))) {
        return FALSE;
    }

    LeftObject = CK_AS_OBJECT(Left);
    RightObject = CK_AS_OBJECT(Right);
    if (LeftObject->Type != RightObject->Type) {
        return FALSE;
    }

    switch (LeftObject->Type) {
    case CkObjectRange:
        LeftRange = (PCK_RANGE)LeftObject;
        RightRange = (PCK_RANGE)RightObject;
        if ((LeftRange->From.Int == RightRange->From.Int) &&
            (LeftRange->To.Int == RightRange->To.Int) &&
            (LeftRange->Inclusive == RightRange->Inclusive)) {

            return TRUE;
        }

        break;

    case CkObjectString:
        LeftString = (PCK_STRING_OBJECT)LeftObject;
        RightString = (PCK_STRING_OBJECT)RightObject;
        if ((LeftString->Hash == RightString->Hash) &&
            (LeftString->Length == RightString->Length)) {

            Compare = CkCompareMemory(LeftString->Value,
                                      RightString->Value,
                                      LeftString->Length);

            if (Compare == 0) {
                return TRUE;
            }
        }

        break;

    default:
        break;
    }

    return FALSE;
}

BOOL
CkpAreValuesIdentical (
    CK_VALUE Left,
    CK_VALUE Right
    )

/*++

Routine Description:

    This routine determines if two objects are strictly the same.

Arguments:

    Left - Supplies the left value.

    Right - Supplies the right value.

Return Value:

    TRUE if the values are equal.

    FALSE if the values are not equal.

--*/

{

    if (Left.Type != Right.Type) {
        return FALSE;
    }

    if (Left.Type == CkValueInteger) {
        return Left.U.Integer == Right.U.Integer;
    }

    return Left.U.Object == Right.U.Object;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpFiberReset (
    PCK_VM Vm,
    PCK_FIBER Fiber,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine reinitializes a fiber object for fresh execution.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber to reset.

    Closure - Supplies a pointer to the closure to execute.

Return Value:

    None.

--*/

{

    Fiber->StackTop = Fiber->Stack;
    Fiber->OpenUpvalues = NULL;
    Fiber->Caller = NULL;
    Fiber->Error = CK_NULL_VALUE;
    Fiber->FrameCount = 0;
    return;
}

