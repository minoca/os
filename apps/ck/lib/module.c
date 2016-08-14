/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    module.c

Abstract:

    This module implements support for module in Chalk.

Author:

    Evan Green 29-May-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include "compiler.h"

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
CkpModuleRun (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpModuleGetVariable (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

//
// -------------------------------------------------------------------- Globals
//

CK_PRIMITIVE_DESCRIPTION CkModulePrimitives[] = {
    {"run@0", 0, CkpModuleRun},
    {"get@1", 1, CkpModuleGetVariable},
    {NULL, 0, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

CK_VALUE
CkpModuleLoad (
    PCK_VM Vm,
    CK_VALUE ModuleName
    )

/*++

Routine Description:

    This routine loads the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

Return Value:

    Returns the newly loaded module value on success.

    CK_NULL_VALUE on failure.

--*/

{

    PCK_MODULE Module;
    PCK_STRING NameString;
    UINTN Size;
    PSTR Source;
    CK_VALUE Value;

    CK_ASSERT(CK_IS_STRING(ModuleName));

    NameString = CK_AS_STRING(ModuleName);

    //
    // If the module already exists, just return it.
    //

    Module = CkpModuleGet(Vm, ModuleName);
    if (Module != NULL) {
        CK_OBJECT_VALUE(Value, Module);
        return Value;
    }

    if (Vm->Configuration.LoadModule == NULL) {
        CkpRuntimeError(Vm, "Module load not supported");
        return CkNullValue;
    }

    //
    // Call out to the big city to actually go get the module.
    //

    Source = Vm->Configuration.LoadModule(Vm, NameString->Value, &Size);
    if (Source == NULL) {
        CkpRuntimeError(Vm, "Module load error: %s", NameString->Value);
        return CkNullValue;
    }

    Module = CkpModuleLoadSource(Vm, ModuleName, Source, Size);
    if (Module == NULL) {
        CkpRuntimeError(Vm, "Module compile error: %s", NameString->Value);
        return CkNullValue;
    }

    CK_OBJECT_VALUE(Value, Module);
    return Value;
}

PCK_MODULE
CkpModuleLoadSource (
    PCK_VM Vm,
    CK_VALUE ModuleName,
    PSTR Source,
    UINTN Length
    )

/*++

Routine Description:

    This routine loads the given source under the given module name.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the module name value.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Returns a pointer to the newly loaded module on success.

    NULL on failure.

--*/

{

    PCK_CLOSURE Closure;
    PCK_MODULE CoreModule;
    PCK_FUNCTION Function;
    CK_SYMBOL_INDEX Index;
    PCK_MODULE Module;
    PCK_STRING String;
    CK_VALUE Value;

    Module = CkpModuleGet(Vm, ModuleName);
    if (Module == NULL) {
        Module = CkpModuleCreate(Vm, CK_AS_STRING(ModuleName));
        if (Module == NULL) {
            return NULL;
        }

        CK_OBJECT_VALUE(Value, Module);
        CkpDictSet(Vm, Vm->Modules, ModuleName, Value);

        //
        // Load up the core module an add all its variables into the current
        // namespace.
        //

        CoreModule = CkpModuleGet(Vm, CK_NULL_VALUE);
        for (Index = 0; Index < CoreModule->Variables.Count; Index += 1) {
            String = CK_AS_STRING(CoreModule->VariableNames.List.Data[Index]);
            CkpDefineModuleVariable(Vm,
                                    Module,
                                    String->Value,
                                    String->Length,
                                    CoreModule->Variables.Data[Index]);
        }
    }

    Function = CkpCompile(Vm, Module, Source, Length, TRUE);
    if (Function == NULL) {
        return NULL;
    }

    CkpPushRoot(Vm, &(Function->Header));
    Closure = CkpClosureCreate(Vm, Function, NULL);
    if (Closure == NULL) {
        CkpPopRoot(Vm);
        return NULL;
    }

    CkpPushRoot(Vm, &(Closure->Header));
    Module->Fiber = CkpFiberCreate(Vm, Closure);
    CkpPopRoot(Vm);
    CkpPopRoot(Vm);
    return Module;
}

PCK_MODULE
CkpModuleCreate (
    PCK_VM Vm,
    PCK_STRING Name
    )

/*++

Routine Description:

    This routine creates a new module object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies a pointer to the module name.

Return Value:

    Returns a pointer to the newly allocated module on success.

    NULL on allocation failure.

--*/

{

    CK_ERROR_TYPE Error;
    PCK_MODULE Module;

    Module = CkAllocate(Vm, sizeof(CK_MODULE));
    if (Module == NULL) {
        return NULL;
    }

    CkZero(Module, sizeof(CK_MODULE));
    CkpInitializeObject(Vm,
                        &(Module->Header),
                        CkObjectModule,
                        Vm->Class.Module);

    CkpPushRoot(Vm, &(Module->Header));
    Error = CkpStringTableInitialize(Vm, &(Module->VariableNames));
    if (Error != CkSuccess) {
        Module = NULL;
        goto ModuleCreateEnd;
    }

    Error = CkpStringTableInitialize(Vm, &(Module->Strings));
    if (Error != CkSuccess) {
        Module = NULL;
        goto ModuleCreateEnd;
    }

    CkpInitializeArray(&(Module->Variables));
    Module->Name = Name;
    Module->Fiber = NULL;

ModuleCreateEnd:
    CkpPopRoot(Vm);
    return Module;
}

PCK_MODULE
CkpModuleGet (
    PCK_VM Vm,
    CK_VALUE Name
    )

/*++

Routine Description:

    This routine attempts to find a previously loaded module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Name - Supplies the module name value.

Return Value:

    Returns a pointer to the module with the given name on success.

    NULL if no such module exists.

--*/

{

    CK_VALUE Module;

    Module = CkpDictGet(Vm->Modules, Name);
    if (CK_IS_UNDEFINED(Module)) {
        return NULL;
    }

    return CK_AS_MODULE(Module);
}

//
// --------------------------------------------------------- Internal Functions
//

//
// Module class primitives
//

BOOL
CkpModuleRun (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine executes the contents of a module, if not already run.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;

    Module = CK_AS_MODULE(Arguments[0]);
    Fiber = Module->Fiber;
    if (Fiber == NULL) {
        Arguments[0] = CkNullValue;
        return TRUE;
    }

    //
    // Clear out and switch to the module contents fiber.
    //

    CK_ASSERT((Fiber->FrameCount != 0) && (CK_IS_NULL(Fiber->Error)));

    Module->Fiber = NULL;
    Fiber->Caller = Vm->Fiber;
    Vm->Fiber = Fiber;

    //
    // Return false to indicate a fiber switch.
    //

    return FALSE;
}

BOOL
CkpModuleGetVariable (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns a module level variable.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_MODULE Module;
    PCK_STRING Name;

    Module = CK_AS_MODULE(Arguments[0]);
    if (!CK_IS_STRING(Arguments[1])) {
        CkpRuntimeError(Vm, "Expected a string");
        return FALSE;
    }

    Name = CK_AS_STRING(Arguments[1]);
    Arguments[0] = CkpFindModuleVariable(Vm, Module, Name->Value);
    if (CK_IS_UNDEFINED(Arguments[0])) {
        CkpRuntimeError(Vm,
                        "No such variable '%s' in module '%s'",
                        Name->Value,
                        Module->Name->Value);

        return FALSE;
    }

    return TRUE;
}

