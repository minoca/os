/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include <stdarg.h>
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

VOID
CkpReportRuntimeError (
    PCK_VM Vm,
    PSTR MessageFormat,
    va_list ArgumentList
    );

PCK_CLASS
CkpDefineCoreClass (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name
    );

VOID
CkpCoreAddPrimitives (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCK_CLASS Class,
    PCK_PRIMITIVE_DESCRIPTION Primitives
    );

VOID
CkpCoreAddPrimitive (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCK_CLASS Class,
    PSTR Name,
    PCK_PRIMITIVE_METHOD Function
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

//
// -------------------------------------------------------------------- Globals
//

extern PVOID _binary_ckcore_ck_start;
extern PVOID _binary_ckcore_ck_end;

CK_PRIMITIVE_DESCRIPTION CkObjectPrimitives[] = {
    {"__init@0", CkpObjectInit},
    {"__lnot@0", CkpObjectLogicalNot},
    {"__eq@1", CkpObjectIsEqual},
    {"__ne@1", CkpObjectIsNotEqual},
    {"__is@1", CkpObjectIs},
    {"__str@0", CkpObjectToString},
    {"type@0", CkpObjectType},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkObjectMetaPrimitives[] = {
    {"same@2", CkpObjectMetaSame},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkClassPrimitives[] = {
    {"name@0", CkpClassName},
    {"superType@0", CkpClassSuper},
    {"__str@0", CkpClassName},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkNullPrimitives[] = {
    {"__lnot@0", CkpNullLogicalNot},
    {"__str@0", CkpNullToString},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkFunctionPrimitives[] = {
    {"arity@0", CkpFunctionArity},
    {"module@0", CkpFunctionModule},
    {"stackUsage@0", CkpFunctionStackUsage},
    {NULL, NULL}
};

CK_PRIMITIVE_DESCRIPTION CkCorePrimitives[] = {
    {"gc@0", CkpCoreGarbageCollect},
    {"importModule@1", CkpCoreImportModule},
    {"write@1", CkpCoreWrite},
    {NULL, NULL}
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

    CoreModule = CkpModuleCreate(Vm, NULL);
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
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Object, CkObjectPrimitives);

    //
    // Create the root Class class, which inherits from Object (like everything
    // does), and whose class is itself.
    //

    Classes->Class = CkpDefineCoreClass(Vm, CoreModule, "Class");
    if (Classes->Class == NULL) {
        return CkErrorNoMemory;
    }

    CkpBindSuperclass(Vm, Classes->Class, Classes->Object);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Class, CkClassPrimitives);

    //
    // Create the Object metaclass, which inherits from Class.
    //

    ObjectMeta = CkpDefineCoreClass(Vm, CoreModule, "Object Metaclass");
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
    Error = CkpInterpret(Vm, NULL, (PSTR)&_binary_ckcore_ck_start, Size);
    if (Error != CkSuccess) {
        return Error;
    }

    //
    // Wire up the primitives to the core classes.
    //

    Value = CkpFindModuleVariable(Vm, CoreModule, "Fiber");
    Classes->Fiber = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Fiber, CkFiberPrimitives);
    CkpCoreAddPrimitives(Vm,
                         CoreModule,
                         Classes->Fiber->Header.Class,
                         CkFiberStaticPrimitives);

    Value = CkpFindModuleVariable(Vm, CoreModule, "Null");
    Classes->Null = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Null, CkNullPrimitives);
    Value = CkpFindModuleVariable(Vm, CoreModule, "Int");
    Classes->Int = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Int, CkIntPrimitives);
    CkpCoreAddPrimitives(Vm,
                         CoreModule,
                         Classes->Int->Header.Class,
                         CkIntStaticPrimitives);

    Value = CkpFindModuleVariable(Vm, CoreModule, "String");
    Classes->String = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->String, CkStringPrimitives);
    CkpCoreAddPrimitives(Vm,
                         CoreModule,
                         Classes->String->Header.Class,
                         CkStringStaticPrimitives);

    Value = CkpFindModuleVariable(Vm, CoreModule, "Function");
    Classes->Function = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm,
                         CoreModule,
                         Classes->Function,
                         CkFunctionPrimitives);

    Value = CkpFindModuleVariable(Vm, CoreModule, "List");
    Classes->List = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->List, CkListPrimitives);
    Value = CkpFindModuleVariable(Vm, CoreModule, "Dict");
    Classes->Dict = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Dict, CkDictPrimitives);
    Value = CkpFindModuleVariable(Vm, CoreModule, "Range");
    Classes->Range = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Range, CkRangePrimitives);
    Value = CkpFindModuleVariable(Vm, CoreModule, "Core");
    Classes->Core = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm,
                         CoreModule,
                         Classes->Core->Header.Class,
                         CkCorePrimitives);

    Value = CkpFindModuleVariable(Vm, CoreModule, "Module");
    Classes->Module = CK_AS_CLASS(Value);
    CkpCoreAddPrimitives(Vm, CoreModule, Classes->Core, CkModulePrimitives);

    //
    // Some strings may have been created without being assigned to string
    // class. Go through all objects and patch those up.
    //

    Object = Vm->FirstObject;
    while (Object != NULL) {
        if (Object->Type == CkObjectString) {
            Object->Class = Classes->String;
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

VOID
CkpRuntimeError (
    PCK_VM Vm,
    PSTR MessageFormat,
    ...
    )

/*++

Routine Description:

    This routine reports a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MessageFormat - Supplies the printf message format string.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, MessageFormat);
    CkpReportRuntimeError(Vm, MessageFormat, ArgumentList);
    va_end(ArgumentList);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpReportRuntimeError (
    PCK_VM Vm,
    PSTR MessageFormat,
    va_list ArgumentList
    )

/*++

Routine Description:

    This routine reports a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MessageFormat - Supplies the printf message format string.

    ArgumentList - Supplies the argument list.

Return Value:

    None.

--*/

{

    UINTN Length;
    CHAR Message[CK_MAX_ERROR_MESSAGE];

    CK_ASSERT(Vm->Fiber != NULL);

    if ((Vm->Fiber == NULL) || (!CK_IS_NULL(Vm->Fiber->Error))) {
        return;
    }

    Length = vsnprintf(Message, sizeof(Message), MessageFormat, ArgumentList);
    Message[sizeof(Message) - 1] = '\0';
    Vm->Fiber->Error = CkpStringCreate(Vm, Message, Length);

    //
    // If string allocation happened to fail, it's important that some sort of
    // error get set, so just initialize the error to an integer.
    //

    if (CK_IS_NULL(Vm->Fiber->Error)) {
        CK_INT_VALUE(Vm->Fiber->Error, 1);
    }

    return;
}

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
    Class = CkpClassAllocate(Vm, Module, 0, NameString);
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
    PCK_MODULE Module,
    PCK_CLASS Class,
    PCK_PRIMITIVE_DESCRIPTION Primitives
    )

/*++

Routine Description:

    This routine adds multiple primitive functions to one of the builtin
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the class lives in.

    Class - Supplies a pointer to the class to add a method to.

    Primitives - Supplies a pointer to a null-terminated array of primitive
        descriptions.

Return Value:

    None.

--*/

{

    while (Primitives->Name != NULL) {
        CkpCoreAddPrimitive(Vm,
                            Module,
                            Class,
                            Primitives->Name,
                            Primitives->Primitive);

        Primitives += 1;
    }

    return;
}

VOID
CkpCoreAddPrimitive (
    PCK_VM Vm,
    PCK_MODULE Module,
    PCK_CLASS Class,
    PSTR Name,
    PCK_PRIMITIVE_METHOD Function
    )

/*++

Routine Description:

    This routine adds a primitive function to one of the builtin classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the class lives in.

    Class - Supplies a pointer to the class to add a method to.

    Name - Supplies a pointer to the null terminated name of the method.

    Function - Supplies a pointer to the C function to attach to this method.

Return Value:

    None.

--*/

{

    CK_SYMBOL_INDEX Index;

    Index = CkpStringTableEnsure(Vm, &(Module->Strings), Name, strlen(Name));
    if (Index == -1) {
        return;
    }

    CkpBindMethod(Vm, Module, Class, Index, CkMethodPrimitive, Function);
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

    PCK_CLASS ObjectClass;
    PCK_CLASS QueryClass;

    if (!CK_IS_CLASS(Arguments[1])) {
        CkpRuntimeError(Vm, "is: Argument must be a class.");
        return FALSE;
    }

    QueryClass = CK_AS_CLASS(Arguments[1]);
    ObjectClass = CkpGetClass(Vm, Arguments[0]);

    //
    // Walk up the class hierarchy comparing to the class in question.
    //

    while (TRUE) {
        if (ObjectClass == QueryClass) {
            Arguments[0] = CkOneValue;
            return TRUE;

        } else if ((ObjectClass->Super == ObjectClass) ||
                   (ObjectClass == NULL)) {

            break;
        }

        ObjectClass = ObjectClass->Super;
    }

    Arguments[0] = CkZeroValue;
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

    PCK_CLOSURE Closure;
    PCK_FUNCTION Function;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    Function = Closure->Function;
    CK_INT_VALUE(Arguments[0], Function->Arity);
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
    PCK_FUNCTION Function;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    Function = Closure->Function;
    CK_OBJECT_VALUE(Arguments[0], Function->Module);
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
    PCK_FUNCTION Function;

    CK_ASSERT(CK_IS_CLOSURE(Arguments[0]));

    Closure = CK_AS_CLOSURE(Arguments[0]);
    Function = Closure->Function;
    CK_INT_VALUE(Arguments[0], Function->MaxStack);
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

    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected a string");
        return FALSE;
    }

    Arguments[0] = CkpModuleLoad(Vm, Arguments[1]);
    if (CK_IS_NULL(Arguments[0])) {
        return FALSE;
    }

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
        CkpRuntimeError(Vm, "Expected a string");
        return FALSE;
    }

    String = CK_AS_STRING(Arguments[1]);
    if (Vm->Configuration.Write != NULL) {
        Vm->Configuration.Write(Vm, String->Value);
    }

    return TRUE;
}

