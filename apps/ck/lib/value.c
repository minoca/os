/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
    PCK_FUNCTION Function,
    PCK_CLASS Class
    )

/*++

Routine Description:

    This routine creates a new closure object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function the closure encloses.

    Class - Supplies a pointer to the class the closure was defined in.

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

    Closure->Type = CkClosureBlock;
    Closure->U.Block.Function = Function;
    Closure->Class = Class;
    Closure->Upvalues = (PCK_UPVALUE *)(Closure + 1);
    CkZero(Closure->Upvalues, UpvalueSize);
    return Closure;
}

PCK_CLOSURE
CkpClosureCreatePrimitive (
    PCK_VM Vm,
    PCK_PRIMITIVE_FUNCTION Function,
    PCK_CLASS Class,
    PCK_STRING Name,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine creates a new closure object for a primitive function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the primitive C function.

    Class - Supplies a pointer to the class the closure was defined in.

    Name - Supplies a pointer to the function name string.

    Arity - Supplies the function arity.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

{

    PCK_CLOSURE Closure;

    Closure = CkAllocate(Vm, sizeof(CK_CLOSURE));
    if (Closure == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(Closure->Header),
                        CkObjectClosure,
                        Vm->Class.Function);

    Closure->Type = CkClosurePrimitive;
    Closure->U.Primitive.Function = Function;
    Closure->U.Primitive.Arity = Arity;
    Closure->U.Primitive.Name = Name;
    Closure->Class = Class;
    Closure->Upvalues = NULL;
    return Closure;
}

PCK_CLOSURE
CkpClosureCreateForeign (
    PCK_VM Vm,
    PCK_FOREIGN_FUNCTION Function,
    PCK_MODULE Module,
    PCK_STRING Name,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine creates a new closure object for a foreign function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the foreign C function pointer.

    Module - Supplies a pointer to the module the function was defined in.

    Name - Supplies a pointer to the function name string.

    Arity - Supplies the function arity.

Return Value:

    Returns a pointer to the new closure on success.

    NULL on allocation failure.

--*/

{

    PCK_CLOSURE Closure;

    CkpPushRoot(Vm, &(Module->Header));
    CkpPushRoot(Vm, &(Name->Header));
    Closure = CkAllocate(Vm, sizeof(CK_CLOSURE));
    CkpPopRoot(Vm);
    CkpPopRoot(Vm);
    if (Closure == NULL) {
        return NULL;
    }

    CkpInitializeObject(Vm,
                        &(Closure->Header),
                        CkObjectClosure,
                        Vm->Class.Function);

    Closure->Type = CkClosureForeign;
    Closure->U.Foreign.Function = Function;
    Closure->U.Foreign.Arity = Arity;
    Closure->U.Foreign.Name = Name;
    Closure->U.Foreign.Module = Module;
    Closure->Class = NULL;
    Closure->Upvalues = NULL;
    return Closure;
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

    CkZero(Function, sizeof(CK_FUNCTION));
    CkpInitializeObject(Vm, &(Function->Header), CkObjectFunction, NULL);
    CkpInitializeArray(&(Function->Constants));
    CkpInitializeArray(&(Function->Code));
    Function->Module = Module;
    Function->MaxStack = StackSize;
    Function->UpvalueCount = 0;
    Function->Arity = 0;
    return Function;
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

    PCK_FOREIGN_DATA Foreign;
    PCK_FUNCTION Function;

    switch (Object->Type) {
    case CkObjectFiber:
        CkpFiberDestroy(Vm, (PCK_FIBER)Object);
        break;

    case CkObjectFunction:
        Function = (PCK_FUNCTION)Object;
        CkpClearArray(Vm, &(Function->Constants));
        CkpClearArray(Vm, &(Function->Code));
        CkpClearArray(Vm, &(Function->Debug.LineProgram));
        break;

    case CkObjectForeign:
        Foreign = (PCK_FOREIGN_DATA)Object;

        //
        // The object's going down, call the callback.
        //

        if (Foreign->Destroy != NULL) {
            Foreign->Destroy(Foreign->Data);
        }

        break;

    case CkObjectList:
        CkpListDestroy(Vm, (PCK_LIST)Object);
        break;

    case CkObjectDict:
        CkFree(Vm, ((PCK_DICT)Object)->Entries);
        break;

    case CkObjectModule:
        CkpModuleDestroy(Vm, (PCK_MODULE)Object);
        break;

    case CkObjectClass:
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
    Object->NextKiss = NULL;
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
    PCK_STRING LeftString;
    PCK_OBJECT RightObject;
    PCK_RANGE RightRange;
    PCK_STRING RightString;

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
        if ((LeftRange->From == RightRange->From) &&
            (LeftRange->To == RightRange->To) &&
            (LeftRange->Inclusive == RightRange->Inclusive)) {

            return TRUE;
        }

        break;

    case CkObjectString:
        LeftString = (PCK_STRING)LeftObject;
        RightString = (PCK_STRING)RightObject;
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

    } else if (Left.Type == CkValueNull) {
        return TRUE;
    }

    return Left.U.Object == Right.U.Object;
}

BOOL
CkpGetValueBoolean (
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine determines if the given value "is" or "isn't".

Arguments:

    Value - Supplies the value to evaluate.

Return Value:

    FALSE if the value is undefined, Null, or zero.

    TRUE otherwise.

--*/

{

    switch (Value.Type) {
    case CkValueNull:
    case CkValueUndefined:
        return FALSE;

    case CkValueInteger:
        return CK_AS_INTEGER(Value) != 0;

    case CkValueObject:
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return TRUE;
}

PCK_CLASS
CkpGetClass (
    PCK_VM Vm,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine returns the class of the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Value - Supplies a pointer to the class object.

Return Value:

    NULL for the undefined value.

    Returns a pointer to the class for all other values.

--*/

{

    switch (Value.Type) {
    case CkValueNull:
        return Vm->Class.Null;

    case CkValueInteger:
        return Vm->Class.Int;

    case CkValueObject:
        return CK_AS_OBJECT(Value)->Class;

    case CkValueUndefined:
    default:

        CK_ASSERT(FALSE);

        break;
    }

    return NULL;
}

PCK_CLASS
CkpClassAllocate (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_SYMBOL_INDEX FieldCount,
    PCK_STRING Name
    )

/*++

Routine Description:

    This routine allocates a new class object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module to define the class in.

    FieldCount - Supplies the number of fields the class has.

    Name - Supplies a pointer to the name of the class.

Return Value:

    Returns a pointer to the newly allocated class object on success.

    NULL on allocation failure.

--*/

{

    PCK_CLASS Class;

    Class = CkAllocate(Vm, sizeof(CK_CLASS));
    if (Class == NULL) {
        return NULL;
    }

    CkZero(Class, sizeof(CK_CLASS));
    CkpInitializeObject(Vm, &(Class->Header), CkObjectClass, Vm->Class.Object);
    Class->FieldCount = FieldCount;
    Class->Name = Name;
    Class->Module = Module;
    CkpPushRoot(Vm, &(Class->Header));
    Class->Methods = CkpDictCreate(Vm);
    CkpPopRoot(Vm);
    if (Class->Methods == NULL) {
        return NULL;
    }

    return Class;
}

VOID
CkpBindMethod (
    PCK_VM Vm,
    PCK_CLASS Class,
    CK_VALUE Signature,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine binds a method to a class.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class to bind the method to.

    Signature - Supplies a name string value of the function signature to bind
        to the class.

    Closure - Supplies a pointer to the closure to bind.

Return Value:

    None.

--*/

{

    CK_VALUE Value;

    CK_OBJECT_VALUE(Value, Closure);
    CkpDictSet(Vm, Class->Methods, Signature, Value);

    //
    // Bind the closure to the class, so that when it's run it knows 1) where
    // its fields start and 2) what its superclass is.
    //

    Closure->Class = Class;
    return;
}

VOID
CkpBindSuperclass (
    PCK_VM Vm,
    PCK_CLASS Class,
    PCK_CLASS Super
    )

/*++

Routine Description:

    This routine binds a class to its superclass.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the subclass.

    Super - Supplies a pointer to the superclass.

Return Value:

    None.

--*/

{

    Class->Super = Super;
    Class->SuperFieldCount = Super->FieldCount;

    //
    // Copy all the methods in the superclass to this class.
    //

    CkpDictCombine(Vm, Class->Methods, Super->Methods);
    return;
}

CK_VALUE
CkpCreateInstance (
    PCK_VM Vm,
    PCK_CLASS Class
    )

/*++

Routine Description:

    This routine creates a new class instance.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class.

Return Value:

    Returns an instance of the given class on success.

--*/

{

    UINTN AllocationSize;
    PCK_DICT Dict;
    PCK_FIBER Fiber;
    CK_SYMBOL_INDEX FieldIndex;
    PCK_INSTANCE Instance;
    PCK_LIST List;
    CK_VALUE Value;

    //
    // Classes with custom structures are created specially.
    //

    if ((Class->Flags & CK_CLASS_SPECIAL_CREATION) != 0) {
        Value = CkNullValue;
        if (Class == Vm->Class.Fiber) {
            Fiber = CkpFiberCreate(Vm, NULL);
            if (Fiber != NULL) {
                CK_OBJECT_VALUE(Value, Fiber);
            }

        } else if (Class == Vm->Class.List) {
            List = CkpListCreate(Vm, 0);
            if (List != NULL) {
                CK_OBJECT_VALUE(Value, List);
            }

        } else if (Class == Vm->Class.Dict) {
            Dict = CkpDictCreate(Vm);
            if (Dict != NULL) {
                CK_OBJECT_VALUE(Value, Dict);
            }

        } else if (Class == Vm->Class.Int) {
            CK_INT_VALUE(Value, 0);

        } else if (Class == Vm->Class.Range) {
            Value = CkpRangeCreate(Vm, 0, 0, FALSE);

        } else if (Class == Vm->Class.String) {
            Value = CkpStringCreate(Vm, "", 0);
        }

    } else {
        AllocationSize = sizeof(CK_INSTANCE) +
                         (Class->FieldCount * sizeof(CK_VALUE));

        Instance = CkAllocate(Vm, AllocationSize);
        if (Instance == NULL) {
            Value = CkNullValue;

        } else {
            CkpInitializeObject(Vm,
                                &(Instance->Header),
                                CkObjectInstance,
                                Class);

            Instance->Fields = (PCK_VALUE)(Instance + 1);
            for (FieldIndex = 0;
                 FieldIndex < Class->FieldCount;
                 FieldIndex += 1) {

                Instance->Fields[FieldIndex] = CkNullValue;
            }

            CK_OBJECT_VALUE(Value, Instance);
        }
    }

    return Value;
}

//
// --------------------------------------------------------- Internal Functions
//

