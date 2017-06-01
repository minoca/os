/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    core.c

Abstract:

    This module implements the core of the Chalk runtime, including the base
    classes.

Author:

    Evan Green 28-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdio.h>

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

PCK_CLASS
CkpDefineCoreClass (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name
    );

VOID
CkpCoreAddPrimitives (
    PCK_VM Vm,
    PCK_CLASS Class,
    PCK_PRIMITIVE_DESCRIPTION Primitives
    );

VOID
CkpCoreAddPrimitive (
    PCK_VM Vm,
    PCK_CLASS Class,
    PSTR Name,
    CK_ARITY Arity,
    PCK_PRIMITIVE_FUNCTION Function
    );

BOOL
CkpObjectLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectInit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectIsEqual (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectIsNotEqual (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectIs (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectGet (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectSet (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectImplements (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectType (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpObjectMetaSame (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpClassName (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpClassSuper (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpNullLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpNullToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFunctionArity (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFunctionModule (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpFunctionStackUsage (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreGarbageCollect (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreImportModule (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreWrite (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreGetModules (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreGetModulePath (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreSetModulePath (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreRaise (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpCoreImportAllSymbols (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

//
// -------------------------------------------------------------------- Globals
//

extern PVOID _binary_ckcore_ck_start;
extern PVOID _binary_ckcore_ck_end;

CK_PRIMITIVE_DESCRIPTION CkObjectPrimitives[] = {
    {"__init@0", 0, CkpObjectInit},
    {"__lnot@0", 0, CkpObjectLogicalNot},
    {"__eq@1", 1, CkpObjectIsEqual},
    {"__ne@1", 1, CkpObjectIsNotEqual},
    {"__is@1", 1, CkpObjectIs},
    {"__str@0", 0, CkpObjectToString},
    {"__repr@0", 0, CkpObjectToString},
    {"__get@1", 1, CkpObjectGet},
    {"__set@2", 2, CkpObjectSet},
    {"implements@2", 2, CkpObjectImplements},
    {"type@0", 0, CkpObjectType},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkObjectMetaPrimitives[] = {
    {"same@2", 2, CkpObjectMetaSame},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkClassPrimitives[] = {
    {"name@0", 0, CkpClassName},
    {"superType@0", 0, CkpClassSuper},
    {"__repr@0", 0, CkpClassName},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkNullPrimitives[] = {
    {"__lnot@0", 0, CkpNullLogicalNot},
    {"__str@0", 0, CkpNullToString},
    {"__repr@0", 0, CkpNullToString},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkFunctionPrimitives[] = {
    {"arity@0", 0, CkpFunctionArity},
    {"module@0", 0, CkpFunctionModule},
    {"stackUsage@0", 0, CkpFunctionStackUsage},
    {NULL, 0, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkCorePrimitives[] = {
    {"gc@0", 0, CkpCoreGarbageCollect},
    {"importModule@1", 1, CkpCoreImportModule},
    {"_write@1", 1, CkpCoreWrite},
    {"modules@0", 0, CkpCoreGetModules},
    {"modulePath@0", 0, CkpCoreGetModulePath},
    {"setModulePath@1", 1, CkpCoreSetModulePath},
    {"raise@1", 1, CkpCoreRaise},
    {"importAllSymbols@1", 1, CkpCoreImportAllSymbols},
    {NULL, 0, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

CK_ERROR_TYPE
CkpInitializeCore (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine initialize the Chalk VM, creating and wiring up the root
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Chalk status.

--*/

{

    PCK_BUILTIN_CLASSES Classes;
    PCK_MODULE CoreModule;
    CK_ERROR_TYPE Error;
    PCK_OBJECT Object;
    PCK_CLASS ObjectMeta;
    UINTN Size;
    CK_VALUE Value;

    Value = CkpStringCreate(Vm, "<core>", 6);
    if (CK_IS_NULL(Value)) {
        return CkErrorNoMemory;
    }

    CoreModule = CkpModuleAllocate(Vm, CK_AS_STRING(Value), NULL);
    if (CoreModule == NULL) {
        return CkErrorNoMemory;
    }

    CK_OBJECT_VALUE(Value, CoreModule);
    CkpDictSet(Vm, Vm->Modules, CK_NULL_VALUE, Value);

    //
    // Create the root Object class, which inherits from itself, and whose type
    // is the Object metaclass.
    //

    Classes = &(Vm->Class);
    Classes->Object = CkpDefineCoreClass(Vm, CoreModule, "Object");
    if (Classes->Object == NULL) {
        return CkErrorNoMemory;
    }

    Classes->Object->Super = Classes->Object;
    CkpCoreAddPrimitives(Vm, Classes->Object, CkObjectPrimitives);

    //
    // Create the root Class class, which inherits from Object (like everything
    // does), and whose class is itself.
    //

    Classes->Class = CkpDefineCoreClass(Vm, CoreModule, "Class");
    if (Classes->Class == NULL) {
        return CkErrorNoMemory;
    }

    CkpBindSuperclass(Vm, Classes->Class, Classes->Object);
    CkpCoreAddPrimitives(Vm, Classes->Class, CkClassPrimitives);

    //
    // Create the Object metaclass, which inherits from Class.
    //

    ObjectMeta = CkpDefineCoreClass(Vm, CoreModule, "ObjectMeta");
    if (ObjectMeta == NULL) {
        return CkErrorNoMemory;
    }

    Classes->Object->Header.Class = ObjectMeta;
    ObjectMeta->Header.Class = Classes->Class;
    Classes->Class->Header.Class = Classes->Class;
    CkpBindSuperclass(Vm, ObjectMeta, Classes->Class);

    //
    // Define the rest of the classes using normal source.
    //

    Size = (UINTN)&_binary_ckcore_ck_end - (UINTN)&_binary_ckcore_ck_start;
    Error = CkpInterpret(Vm,
                         NULL,
                         NULL,
                         (PSTR)&_binary_ckcore_ck_start,
                         Size,
                         1,
                         0);

    if (Error != CkSuccess) {
        return Error;
    }

    //
    // Wire up the primitives to the core classes.
    //

    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Fiber", FALSE));
    Classes->Fiber = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Fiber, CkFiberPrimitives);
    CkpCoreAddPrimitives(Vm,
                         Classes->Fiber->Header.Class,
                         CkFiberStaticPrimitives);

    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Null", FALSE));
    Classes->Null = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Null, CkNullPrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Int", FALSE));
    Classes->Int = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Int, CkIntPrimitives);
    CkpCoreAddPrimitives(Vm, Classes->Int->Header.Class, CkIntStaticPrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "String", FALSE));
    Classes->String = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->String, CkStringPrimitives);
    CkpCoreAddPrimitives(Vm,
                         Classes->String->Header.Class,
                         CkStringStaticPrimitives);

    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Function", FALSE));
    Classes->Function = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Function, CkFunctionPrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "List", FALSE));
    Classes->List = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->List, CkListPrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Dict", FALSE));
    Classes->Dict = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Dict, CkDictPrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Range", FALSE));
    Classes->Range = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Range, CkRangePrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Core", FALSE));
    Classes->Core = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Core->Header.Class, CkCorePrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Module", FALSE));
    Classes->Module = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, Classes->Module, CkModulePrimitives);
    Value = *(CkpFindModuleVariable(Vm, CoreModule, "Exception", FALSE));
    Classes->Exception = CK_AS_CLASS(Value);

    //
    // Patch up any of the core objects that may have been created before their
    // associated classes existed.
    //

    Object = Vm->FirstObject;
    while (Object != NULL) {
        if (Object->Type == CkObjectString) {
            Object->Class = Classes->String;

        } else if (Object->Type == CkObjectClosure) {
            Object->Class = Classes->Function;

        } else if (Object->Type == CkObjectDict) {
            Object->Class = Classes->Dict;

        } else if (Object->Type == CkObjectFiber) {
            Object->Class = Classes->Fiber;
        }

        Object = Object->Next;
    }

    CoreModule->Header.Class = Classes->Module;

    //
    // Set some flags on the special builtin classes.
    //

    Classes->Class->Flags |= CK_CLASS_SPECIAL_CREATION;
    Classes->Fiber->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->Function->Flags |= CK_CLASS_UNINHERITABLE |
                                CK_CLASS_SPECIAL_CREATION;

    Classes->List->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->Dict->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->Null->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->Int->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->Range->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    Classes->String->Flags |=
                            CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;

    Classes->Module->Flags |=
                            CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;

    Classes->Core->Flags |= CK_CLASS_UNINHERITABLE | CK_CLASS_SPECIAL_CREATION;
    return CkSuccess;
}

CK_ARITY
CkpGetFunctionArity (
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine returns the number of argument required to pass to the given
    function.

Arguments:

    Closure - Supplies a pointer to the closure.

Return Value:

    Returns the arity of the function.

--*/

{

    switch (Closure->Type) {
    case CkClosureBlock:
        return Closure->U.Block.Function->Arity;

    case CkClosurePrimitive:
        return Closure->U.Primitive.Arity;

    case CkClosureForeign:
        return Closure->U.Foreign.Arity;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return 0;
}

PCK_STRING
CkpGetFunctionName (
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine returns the original name for a function.

Arguments:

    Closure - Supplies a pointer to the closure.

Return Value:

    Returns a pointer to a string containing the name of the function.

--*/

{

    switch (Closure->Type) {
    case CkClosureBlock:
        return Closure->U.Block.Function->Debug.Name;

    case CkClosurePrimitive:
        return Closure->U.Primitive.Name;

    case CkClosureForeign:
        return Closure->U.Foreign.Name;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return 0;
}

BOOL
CkpObjectIsClass (
    PCK_CLASS ObjectClass,
    PCK_CLASS QueryClass
    )

/*++

Routine Description:

    This routine determines if the given object is of the given type.

Arguments:

    ObjectClass - Supplies a pointer to the object class to query.

    QueryClass - Supplies a pointer to the class to check membership for.

Return Value:

    TRUE if the object class is a subclass of the query class.

    FALSE if the object class is not a member of the query class.

--*/

{

    //
    // Walk up the class hierarchy comparing to the class in question.
    //

    while (TRUE) {
        if (ObjectClass == QueryClass) {
            return TRUE;

        } else if ((ObjectClass == NULL) ||
                   (ObjectClass->Super == ObjectClass)) {

            break;
        }

        ObjectClass = ObjectClass->Super;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

PCK_CLASS
CkpDefineCoreClass (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name
    )

/*++

Routine Description:

    This routine creates a new class object for one of the base core classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the class is defined in.

    Name - Supplies a pointer to the null terminated name of the class.

Return Value:

    Returns a pointer to the class on success.

    NULL on failure.

--*/

{

    PCK_CLASS Class;
    PCK_STRING NameString;
    CK_VALUE Value;

    Value = CkpStringFormat(Vm, "$", Name);
    if (!CK_IS_OBJECT(Value)) {
        return NULL;
    }

    NameString = CK_AS_STRING(Value);
    CkpPushRoot(Vm, &(NameString->Header));

    //
    // Allocate one field for the built-in dictionary that all objects have.
    //

    Class = CkpClassAllocate(Vm, Module, 1, NameString);
    if (Class == NULL) {
        CkpPopRoot(Vm);
        return NULL;
    }

    CK_OBJECT_VALUE(Value, Class);
    CkpDefineModuleVariable(Vm,
                            Module,
                            NameString->Value,
                            NameString->Length,
                            Value);

    CkpPopRoot(Vm);
    return Class;
}

VOID
CkpCoreAddPrimitives (
    PCK_VM Vm,
    PCK_CLASS Class,
    PCK_PRIMITIVE_DESCRIPTION Primitives
    )

/*++

Routine Description:

    This routine adds multiple primitive functions to one of the builtin
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class to add a method to.

    Primitives - Supplies a pointer to a null-terminated array of primitive
        descriptions.

Return Value:

    None.

--*/

{

    while (Primitives->Name != NULL) {
        CkpCoreAddPrimitive(Vm,
                            Class,
                            Primitives->Name,
                            Primitives->Arity,
                            Primitives->Primitive);

        Primitives += 1;
    }

    return;
}

VOID
CkpCoreAddPrimitive (
    PCK_VM Vm,
    PCK_CLASS Class,
    PSTR Name,
    CK_ARITY Arity,
    PCK_PRIMITIVE_FUNCTION Function
    )

/*++

Routine Description:

    This routine adds a primitive function to one of the builtin classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the class lives in.

    Class - Supplies a pointer to the class to add a method to.

    Name - Supplies a pointer to the null terminated name of the method.

    Arity - Supplies the number of arguments the function takes.

    Function - Supplies a pointer to the C function to attach to this method.

Return Value:

    None.

--*/

{

    PCK_CLOSURE Closure;
    CK_SYMBOL_INDEX Index;
    PCK_MODULE Module;
    CK_VALUE NameString;

    Module = Class->Module;
    Index = CkpStringTableEnsure(Vm, &(Module->Strings), Name, strlen(Name));
    if (Index == -1) {
        return;
    }

    NameString = Module->Strings.List.Data[Index];
    Closure = CkpClosureCreatePrimitive(Vm,
                                        Function,
                                        Class,
                                        CK_AS_STRING(NameString),
                                        Arity);

    CkpBindMethod(Vm, Class, NameString, Closure);
    return;
}

BOOL
CkpObjectInit (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine contains a dummy init function that allows any object to
    be initialized with zero arguments. In this case all fields are null.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    return TRUE;
}

BOOL
CkpObjectLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines the logical not of an object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (CkpGetValueBoolean(Arguments[0])) {
        Arguments[0] = CkZeroValue;

    } else {
        Arguments[0] = CkOneValue;
    }

    return TRUE;
}

BOOL
CkpObjectIsEqual (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if two objects are equal.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (CkpAreValuesEqual(Arguments[0], Arguments[1])) {
        Arguments[0] = CkOneValue;

    } else {
        Arguments[0] = CkZeroValue;
    }

    return TRUE;
}

BOOL
CkpObjectIsNotEqual (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if two objects are not equal.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (CkpAreValuesEqual(Arguments[0], Arguments[1])) {
        Arguments[0] = CkZeroValue;

    } else {
        Arguments[0] = CkOneValue;
    }

    return TRUE;
}

BOOL
CkpObjectIs (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if the given object is of the given type.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    BOOL Is;
    PCK_LIST List;
    PCK_CLASS ObjectClass;
    PCK_CLASS QueryClass;
    CK_VALUE Value;

    ObjectClass = CkpGetClass(Vm, Arguments[0]);

    //
    // If a list was passed in, determine if the object is an instance of any
    // of the classes in the list.
    //

    if (CK_IS_LIST(Arguments[1])) {
        List = CK_AS_LIST(Arguments[1]);
        Is = FALSE;
        for (Index = 0; Index < List->Elements.Count; Index += 1) {
            Value = List->Elements.Data[Index];
            if (!CK_IS_CLASS(Value)) {
                CkpRuntimeError(Vm,
                                "TypeError",
                                "Expected a class");

                return FALSE;
            }

            QueryClass = CK_AS_CLASS(Value);
            Is = CkpObjectIsClass(ObjectClass, QueryClass);
            if (Is != FALSE) {
                break;
            }
        }

    //
    // If a class was passed in, see if the object is an instance of the class.
    //

    } else if (CK_IS_CLASS(Arguments[1])) {
        QueryClass = CK_AS_CLASS(Arguments[1]);
        Is = CkpObjectIsClass(ObjectClass, QueryClass);

    } else {
        CkpRuntimeError(Vm, "TypeError", "Expected a class");
        return FALSE;
    }

    if (Is != FALSE) {
        Arguments[0] = CkOneValue;

    } else {
        Arguments[0] = CkZeroValue;
    }

    return TRUE;
}

BOOL
CkpObjectToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine creates a default string representation of the given object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Length;
    PCK_OBJECT Object;
    CHAR String[CK_MAX_NAME + 30];

    Object = CK_AS_OBJECT(Arguments[0]);
    Length = snprintf(String,
                      sizeof(String),
                      "<%s at 0x%p>",
                      Object->Class->Name->Value,
                      Object);

    String[sizeof(String) - 1] = '\0';
    Arguments[0] = CkpStringCreate(Vm, String, Length);
    return TRUE;
}

BOOL
CkpObjectGet (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the default object get method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_DICT Dict;
    PCK_INSTANCE Instance;

    if (!CK_IS_INSTANCE(Arguments[0])) {
        CkpRuntimeError(Vm,
                        "TypeError",
                        "Builtin type does not implement __get");

        return FALSE;
    }

    Instance = CK_AS_INSTANCE(Arguments[0]);
    if (!CK_IS_NULL(Instance->Fields[0])) {
        Dict = CK_AS_DICT(Instance->Fields[0]);
        Arguments[0] = CkpDictGet(Dict, Arguments[1]);

    } else {
        Arguments[0] = CkUndefinedValue;
    }

    if (CK_IS_UNDEFINED(Arguments[0])) {
        CkpRuntimeError(Vm, "KeyError", "Key is not defined");
        return FALSE;
    }

    return TRUE;
}

BOOL
CkpObjectSet (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the default object set method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_DICT Dict;
    PCK_INSTANCE Instance;

    if (!CK_IS_INSTANCE(Arguments[0])) {
        CkpRuntimeError(Vm,
                        "TypeError",
                        "Builtin type does not implement __set");

        return FALSE;
    }

    Instance = CK_AS_INSTANCE(Arguments[0]);
    if (CK_IS_NULL(Instance->Fields[0])) {
        Dict = CkpDictCreate(Vm);
        if (Dict == NULL) {
            return FALSE;
        }

        CK_OBJECT_VALUE(Instance->Fields[0], Dict);

    } else {
        Dict = CK_AS_DICT(Instance->Fields[0]);
    }

    CkpDictSet(Vm, Dict, Arguments[1], Arguments[2]);
    return TRUE;
}

BOOL
CkpObjectImplements (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if the given object implements the given method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CHAR Buffer[CK_MAX_METHOD_SIGNATURE];
    UINTN BufferSize;
    PCK_CLASS Class;
    CK_STRING FakeString;
    PCK_STRING NameString;
    CK_FUNCTION_SIGNATURE Signature;
    CK_VALUE Value;

    Class = CkpGetClass(Vm, Arguments[0]);
    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    NameString = CK_AS_STRING(Arguments[1]);
    if (!CK_IS_INTEGER(Arguments[2])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    Signature.Name = NameString->Value;
    Signature.Length = NameString->Length;
    Signature.Arity = CK_AS_INTEGER(Arguments[2]);
    BufferSize = sizeof(Buffer);
    CkpPrintSignature(&Signature, Buffer, &BufferSize);
    Value = CkpStringFake(&FakeString, Buffer, BufferSize);
    if (!CK_IS_UNDEFINED(CkpDictGet(Class->Methods, Value))) {
        Arguments[0] = CkOneValue;

    } else {
        Arguments[0] = CkZeroValue;
    }

    return TRUE;
}

BOOL
CkpObjectType (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the class of the given object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLASS Class;
    CK_VALUE Value;

    Class = CkpGetClass(Vm, Arguments[0]);
    CK_OBJECT_VALUE(Value, Class);
    Arguments[0] = Value;
    return TRUE;
}

BOOL
CkpObjectMetaSame (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if two objects passed in as arguments are equal.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (CkpAreValuesEqual(Arguments[1], Arguments[2])) {
        Arguments[0] = CkOneValue;

    } else {
        Arguments[0] = CkZeroValue;
    }

    return TRUE;
}

BOOL
CkpClassName (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the name of the class object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLASS Class;

    Class = CK_AS_CLASS(Arguments[0]);
    CK_OBJECT_VALUE(Arguments[0], Class->Name);
    return TRUE;
}

BOOL
CkpClassSuper (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the superclass of the given class object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLASS Class;

    Class = CK_AS_CLASS(Arguments[0]);
    CK_OBJECT_VALUE(Arguments[0], Class->Super);
    return TRUE;
}

BOOL
CkpNullLogicalNot (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines the logical not of a null class instance.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    Arguments[0] = CkOneValue;
    return TRUE;
}

BOOL
CkpNullToString (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine converts a null instance into a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    Arguments[0] = CkpStringCreate(Vm, "null", 4);
    return TRUE;
}

BOOL
CkpFunctionArity (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the number of argument required to pass to the given
    function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_ARITY Arity;
    PCK_CLOSURE Closure;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    Arity = CkpGetFunctionArity(Closure);
    CK_INT_VALUE(Arguments[0], Arity);
    return TRUE;
}

BOOL
CkpFunctionModule (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the module the function was defined in.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLOSURE Closure;
    PCK_MODULE Module;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    switch (Closure->Type) {
    case CkClosureBlock:
        Module = Closure->U.Block.Function->Module;
        break;

    case CkClosurePrimitive:
        Module = CkpModuleGet(Vm, CK_NULL_VALUE);
        break;

    case CkClosureForeign:
        Module = Closure->U.Foreign.Module;
        break;

    default:

        CK_ASSERT(FALSE);

        Module = NULL;
        break;
    }

    CK_OBJECT_VALUE(Arguments[0], Module);
    return TRUE;
}

BOOL
CkpFunctionStackUsage (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the amount of stack a given function takes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLOSURE Closure;
    CK_SYMBOL_INDEX MaxStack;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    switch (Closure->Type) {
    case CkClosureBlock:
        MaxStack = Closure->U.Block.Function->MaxStack;
        break;

    case CkClosurePrimitive:
    case CkClosureForeign:
        MaxStack = 0;
        break;

    default:

        CK_ASSERT(FALSE);

        MaxStack = 0;
        break;
    }

    CK_INT_VALUE(Arguments[0], MaxStack);
    return TRUE;
}

BOOL
CkpCoreGarbageCollect (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the primitive to activate garbage collection.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CkCollectGarbage(Vm);
    return TRUE;
}

BOOL
CkpCoreImportModule (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the primitive to activate garbage collection.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_VALUE Result;
    UINTN StackIndex;

    StackIndex = Arguments - Vm->Fiber->Stack;
    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    //
    // Don't save the return value directly on the stack yet, as the stack
    // might get reallocated during the module load.
    //

    Result = CkpModuleLoad(Vm, Arguments[1], NULL);
    if (CK_IS_NULL(Result)) {
        return FALSE;
    }

    Vm->Fiber->Stack[StackIndex] = Result;
    return TRUE;
}

BOOL
CkpCoreWrite (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine writes a string to the interpreter output. It's possible no
    output is wired up, in which case this is a no-op.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_STRING String;

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a string");
        return FALSE;
    }

    String = CK_AS_STRING(Arguments[1]);
    if (Vm->Configuration.Write != NULL) {
        Vm->Configuration.Write(Vm, String->Value);
    }

    return TRUE;
}

BOOL
CkpCoreGetModules (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the modules dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_OBJECT_VALUE(Arguments[0], Vm->Modules);
    return TRUE;
}

BOOL
CkpCoreGetModulePath (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the current module path.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (Vm->ModulePath == NULL) {
        Vm->ModulePath = CkpListCreate(Vm, 0);
    }

    if (Vm->ModulePath != NULL) {
        CK_OBJECT_VALUE(Arguments[0], Vm->ModulePath);

    } else {
        Arguments[0] = CkNullValue;
    }

    return TRUE;
}

BOOL
CkpCoreSetModulePath (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine sets the current module path.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    if (!CK_IS_LIST(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a list");
        return FALSE;
    }

    Vm->ModulePath = CK_AS_LIST(Arguments[1]);
    return TRUE;
}

BOOL
CkpCoreRaise (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine raises an exception.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    FALSE always to indicate an exception was raised.

--*/

{

    if (!CkpObjectIsClass(CkpGetClass(Vm, Arguments[1]), Vm->Class.Exception)) {
        CkpRuntimeError(Vm, "TypeError", "Expected an Exception");

    } else {
        CkpRaiseException(Vm, Arguments[1], 1);
    }

    return FALSE;
}

BOOL
CkpCoreImportAllSymbols (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine imports all module level symbols from the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_CLOSURE Closure;
    PCK_MODULE CurrentModule;
    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    CK_SYMBOL_INDEX Index;
    PCK_MODULE Module;
    PCK_STRING String;

    if (!CK_IS_MODULE(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a module");
        return FALSE;
    }

    Fiber = Vm->Fiber;

    CK_ASSERT(Fiber->FrameCount != 0);

    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
    Closure = Frame->Closure;

    CK_ASSERT(Closure->Type == CkClosureBlock);

    CurrentModule = Closure->U.Block.Function->Module;
    Module = CK_AS_MODULE(Arguments[1]);
    for (Index = 0; Index < Module->Variables.Count; Index += 1) {
        String = CK_AS_STRING(Module->VariableNames.List.Data[Index]);

        //
        // Import everything that does not start with an underscore.
        //

        if ((String->Length >= 1) && (String->Value[0] != '_')) {
            CkpDefineModuleVariable(Vm,
                                    CurrentModule,
                                    String->Value,
                                    String->Length,
                                    Module->Variables.Data[Index]);
        }
    }

    return TRUE;
}

