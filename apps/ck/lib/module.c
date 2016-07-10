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

PCK_MODULE
CkpModuleGet (
    PCK_VM Vm,
    CK_VALUE Name
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

PCK_FIBER
CkpModuleLoad (
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

    Returns a pointer to a newly created fiber to execute on success.

    NULL on allocation failure.

--*/

{

    PCK_CLOSURE Closure;
    PCK_MODULE CoreModule;
    PCK_FIBER Fiber;
    PCK_FUNCTION Function;
    CK_SYMBOL_INDEX Index;
    PCK_MODULE Module;
    PCK_STRING_OBJECT String;
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
    Closure = CkpClosureCreate(Vm, Function);
    if (Closure == NULL) {
        CkpPopRoot(Vm);
        return NULL;
    }

    CkpPushRoot(Vm, &(Closure->Header));
    Fiber = CkpFiberCreate(Vm, Closure);
    CkpPopRoot(Vm);
    CkpPopRoot(Vm);
    return Fiber;
}

PCK_MODULE
CkpModuleCreate (
    PCK_VM Vm,
    PCK_STRING_OBJECT Name
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

    //
    // TODO: Make a builtin class for modules, since they can be used as
    // objects.
    //

    CkpInitializeObject(Vm, &(Module->Header), CkObjectModule, NULL);
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

ModuleCreateEnd:
    CkpPopRoot(Vm);
    return Module;
}

//
// --------------------------------------------------------- Internal Functions
//

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

