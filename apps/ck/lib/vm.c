/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    vm.c

Abstract:

    This module implements support for the Chalk virtual machine.

Author:

    Evan Green 28-May-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include "core.h"
#include "compiler.h"
#include "vmsys.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

CK_ERROR_TYPE
CkpInterpret (
    PCK_VM Vm,
    PSTR ModuleName,
    PSTR Source,
    UINTN Length
    );

CK_ERROR_TYPE
CkpRunInterpreter (
    PCK_VM Vm,
    PCK_FIBER Fiber
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CK_API
VOID
CkInitializeConfiguration (
    PCK_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine initializes a Chalk configuration with its default values.

Arguments:

    Configuration - Supplies a pointer where the initialized configuration will
        be returned.

Return Value:

    None.

--*/

{

    CkCopy(Configuration, &CkDefaultConfiguration, sizeof(CK_CONFIGURATION));
    return;
}

CK_API
PCK_VM
CkCreateVm (
    PCK_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine creates a new Chalk virtual machine context. Each VM context
    is entirely independent.

Arguments:

    Configuration - Supplies an optional pointer to the configuration to use
        for this instance. If NULL, a default configuration will be provided.

Return Value:

    Returns a pointer to the new VM on success.

    NULL on allocation or if an invalid configuration was supplied.

--*/

{

    PCK_CONFIGURATION Default;
    PCK_REALLOCATE Reallocate;
    CK_ERROR_TYPE Status;
    PCK_VM Vm;

    Default = &CkDefaultConfiguration;
    Reallocate = Default->Reallocate;
    if (Configuration != NULL) {
        if (Configuration->Reallocate != NULL) {
            Reallocate = Configuration->Reallocate;
        }
    }

    Vm = Reallocate(NULL, sizeof(CK_VM));
    if (Vm == NULL) {
        return NULL;
    }

    CkZero(Vm, sizeof(Vm));
    if (Configuration == NULL) {
        CkCopy(&(Vm->Configuration), Default, sizeof(CK_CONFIGURATION));

    } else {
        CkCopy(&(Vm->Configuration), Configuration, sizeof(CK_CONFIGURATION));
        Vm->Configuration.Reallocate = Reallocate;
    }

    Vm->GrayCapacity = CK_INITIAL_GRAY_CAPACITY;
    Vm->Gray = CkRawAllocate(Vm, Vm->GrayCapacity * sizeof(PCK_OBJECT));
    if (Vm->Gray == NULL) {
        Status = CkErrorNoMemory;
        goto CreateVmEnd;
    }

    Vm->NextGarbageCollection = Vm->Configuration.InitialHeapSize;
    CkpSymbolTableInitialize(Vm, &(Vm->MethodNames));
    Vm->Modules = CkpDictCreate(Vm);
    if (Vm->Modules == NULL) {
        Status = CkErrorNoMemory;
        goto CreateVmEnd;
    }

    Status = CkpInitializeCore(Vm);
    if (Status != CkSuccess) {
        goto CreateVmEnd;
    }

    Status = CkSuccess;

CreateVmEnd:
    if (Status != CkSuccess) {
        if (Vm != NULL) {
            CkDestroyVm(Vm);
            Vm = NULL;
        }
    }

    return Vm;
}

CK_API
VOID
CkDestroyVm (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine destroys a Chalk virtual machine.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_OBJECT Next;
    PCK_OBJECT Object;
    PCK_REALLOCATE Reallocate;

    //
    // Notice double frees of the VM.
    //

    CK_ASSERT(Vm->Configuration.Reallocate != NULL);

    Object = Vm->FirstObject;
    while (Object != NULL) {
        Next = Object->Next;
        CkpDestroyObject(Vm, Object);
        Object = Next;
    }

    Vm->FirstObject = NULL;
    if (Vm->Gray != NULL) {
        CkRawFree(Vm, Vm->Gray);
        Vm->Gray = NULL;
    }

    CK_ASSERT(Vm->Handles == NULL);

    CkpSymbolTableClear(Vm, &(Vm->MethodNames));

    //
    // Null out the reallocate function to catch double frees.
    //

    Reallocate = Vm->Configuration.Reallocate;
    Vm->Configuration.Reallocate = NULL;
    Reallocate(Vm, 0);
    return;
}

CK_API
CK_ERROR_TYPE
CkInterpret (
    PCK_VM Vm,
    PSTR Source,
    UINTN Length
    )

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the "main" module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Chalk status.

--*/

{

    return CkpInterpret(Vm, "__main", Source, Length);
}

CK_API
VOID
CkCollectGarbage (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine performs garbage collection on the given Chalk instance,
    freeing up unused dynamic memory as appropriate.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a pointer to the newly allocated or reallocated memory on success.

    NULL on allocation failure or for free operations.

--*/

{

    //
    // TODO: Implement CkCollectGarbage.
    //

    return;
}

CK_SYMBOL_INDEX
CkpDeclareModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name,
    UINTN Length,
    INT Line
    )

/*++

Routine Description:

    This routine creates an undefined module-level variable (essentially a
    forward declaration). These are patched up before compilation is finished.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module that owns the variable.

    Name - Supplies a pointer to the variable name.

    Length - Supplies the length of the variable name, not including the null
        terminator.

    Line - Supplies the line number the forward reference is on.

Return Value:

    Returns the index of the new variable on success.

    -2 on allocation failure.

--*/

{

    CK_ERROR_TYPE Error;
    CK_SYMBOL_INDEX Symbol;
    CK_VALUE Value;

    if (Module->Variables.Count == CK_MAX_MODULE_VARIABLES) {
        return -2;
    }

    Symbol = CkpSymbolTableAdd(Vm, &(Module->VariableNames), Name, Length);
    if (Symbol == -1) {
        return -2;
    }

    CK_INT_VALUE(Value, Line);
    Error = CkpArrayAppend(Vm, &(Module->Variables), Value);
    if (Error != CkSuccess) {
        return -2;
    }

    return Symbol;
}

CK_SYMBOL_INDEX
CkpDefineModuleVariable (
    PCK_VM Vm,
    PCK_MODULE Module,
    PSTR Name,
    UINTN Length,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine creates and defines a module-level variable to the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module that owns the variable.

    Name - Supplies a pointer to the variable name.

    Length - Supplies the length of the variable name, not including the null
        terminator.

    Value - Supplies the value to assign the variable.

Return Value:

    Returns the index of the new variable on success.

    -1 if the variable is already defined.

    -2 on allocation failure.

--*/

{

    CK_ERROR_TYPE Error;
    CK_SYMBOL_INDEX Symbol;

    if (Module->Variables.Count == CK_MAX_MODULE_VARIABLES) {
        return -2;
    }

    if (CK_IS_OBJECT(Value)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Value));
    }

    Symbol = CkpSymbolTableFind(&(Module->VariableNames), Name, Length);

    //
    // Add a brand new symbol.
    //

    if (Symbol == -1) {
        Symbol = CkpSymbolTableAdd(Vm, &(Module->VariableNames), Name, Length);
        Error = CkpArrayAppend(Vm, &(Module->Variables), Value);
        if (Error != CkSuccess) {
            return -2;
        }

    //
    // If the variable was previously declared, it will have an integer value.
    // Now it can be defined for real.
    //

    } else if (CK_IS_INTEGER(Module->Variables.Data[Symbol])) {
        Module->Variables.Data[Symbol] = Value;

    //
    // Otherwise, the variable has been previously defined.
    //

    } else {
        Symbol = -1;
    }

    if (CK_IS_OBJECT(Value)) {
        CkpPopRoot(Vm);
    }

    return Symbol;
}

VOID
CkpPushRoot (
    PCK_VM Vm,
    PCK_OBJECT Object
    )

/*++

Routine Description:

    This routine pushes the given object onto a temporary stack to ensure that
    it will not be garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to push.

Return Value:

    None.

--*/

{

    CK_ASSERT(Object != NULL);
    CK_ASSERT(Vm->WorkingObjectCount < CK_MAX_WORKING_OBJECTS);

    Vm->WorkingObjects[Vm->WorkingObjectCount] = Object;
    Vm->WorkingObjectCount += 1;
    return;
}

VOID
CkpPopRoot (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine pops the top working object off of the temporary stack used to
    ensure that certain objects are not garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CK_ASSERT(Vm->WorkingObjectCount != 0);

    Vm->WorkingObjectCount -= 1;
    return;
}

PVOID
CkpReallocate (
    PCK_VM Vm,
    PVOID Memory,
    UINTN OldSize,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine performs a Chalk dynamic memory operation.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Memory - Supplies an optional pointer to the memory to resize or free.

    OldSize - Supplies the optional previous size of the allocation.

    NewSize - Supplies the new size of the allocation. Set this to 0 to free
        the memory.

Return Value:

    Returns a pointer to the newly allocated or reallocated memory on success.

    NULL on allocation failure or for free operations.

--*/

{

    PVOID Allocation;

    //
    // Add the new bytes to the total count. Ignore frees, since those get
    // handled during garbage collection.
    //

    Vm->BytesAllocated += NewSize - OldSize;

    //
    // Potentially perform garbage collection.
    //

    if ((NewSize > 0) &&
        ((Vm->BytesAllocated >= Vm->NextGarbageCollection) ||
         (CK_VM_FLAG_SET(Vm, CK_CONFIGURATION_GC_STRESS)))) {

        CkCollectGarbage(Vm);
    }

    Allocation = CkRawReallocate(Vm, Memory, NewSize);
    if ((Allocation == NULL) && (NewSize != 0)) {
        if (Vm->Compiler != NULL) {
            CkpCompileError(Vm->Compiler, NULL, "Allocation failure");

        } else {

            //
            // TODO: call the runtime error function.
            //

        }
    }

    return Allocation;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_ERROR_TYPE
CkpInterpret (
    PCK_VM Vm,
    PSTR ModuleName,
    PSTR Source,
    UINTN Length
    )

/*++

Routine Description:

    This routine interprets the given Chalk source string within the context of
    the given module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the module to interpret the source in.

    Source - Supplies a pointer to the null terminated string containing the
        source to interpret.

    Length - Supplies the length of the source string, not including the null
        terminator.

Return Value:

    Chalk status.

--*/

{

    PCK_FIBER Fiber;
    CK_VALUE NameValue;

    if (ModuleName != NULL) {
        NameValue = CkpStringFormat(Vm, "$", ModuleName);
        CkpPushRoot(Vm, CK_AS_OBJECT(NameValue));
    }

    Fiber = CkpModuleLoad(Vm, NameValue, Source, Length);
    if (ModuleName != NULL) {
        CkpPopRoot(Vm);
    }

    if (Fiber == NULL) {
        return CkErrorCompile;
    }

    return CkpRunInterpreter(Vm, Fiber);
}

CK_ERROR_TYPE
CkpRunInterpreter (
    PCK_VM Vm,
    PCK_FIBER Fiber
    )

/*++

Routine Description:

    This routine implements the heart of Chalk: the main execution loop.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the execution context to run.

Return Value:

    Chalk status.

--*/

{

    //
    // TODO: Implement interpreter.
    //

    return CkSuccess;
}

