/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gc.c

Abstract:

    This module implements support for memory allocation and garbage collection
    in the Chalk environment.

Author:

    Evan Green 8-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include "compiler.h"
#include "debug.h"
#include <minoca/lib/status.h>
#include <minoca/lib/yy.h>
#include "lang.h"
#include "compsup.h"

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
CkpKissCompiler (
    PCK_VM Vm,
    PCK_COMPILER Compiler
    );

VOID
CkpKissValue (
    PCK_VM Vm,
    CK_VALUE Value
    );

VOID
CkpKissObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

VOID
CkpDeeplyKiss (
    PCK_VM Vm,
    PCK_OBJECT Head
    );

VOID
CkpCollectUnkissedObjects (
    PCK_VM Vm
    );

VOID
CkpKissClass (
    PCK_VM Vm,
    PCK_CLASS Class
    );

VOID
CkpKissClosure (
    PCK_VM Vm,
    PCK_CLOSURE Closure
    );

VOID
CkpKissDict (
    PCK_VM Vm,
    PCK_DICT Dict
    );

VOID
CkpKissFiber (
    PCK_VM Vm,
    PCK_FIBER Fiber
    );

VOID
CkpKissForeignData (
    PCK_VM Vm,
    PCK_FOREIGN_DATA ForeignData
    );

VOID
CkpKissFunction (
    PCK_VM Vm,
    PCK_FUNCTION Function
    );

VOID
CkpKissInstance (
    PCK_VM Vm,
    PCK_INSTANCE Instance
    );

VOID
CkpKissList (
    PCK_VM Vm,
    PCK_LIST List
    );

VOID
CkpKissModule (
    PCK_VM Vm,
    PCK_MODULE Module
    );

VOID
CkpKissRange (
    PCK_VM Vm,
    PCK_RANGE Range
    );

VOID
CkpKissString (
    PCK_VM Vm,
    PCK_STRING String
    );

VOID
CkpKissUpvalue (
    PCK_VM Vm,
    PCK_UPVALUE Upvalue
    );

VOID
CkpKissValueArray (
    PCK_VM Vm,
    PCK_VALUE_ARRAY Array
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

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

    UINTN Index;
    CK_OBJECT KissHead;

    //
    // Reset the number of bytes allocated, and have the kiss functions count
    // their allocations. This avoids the extra work of having to determine
    // the size of objects being freed. The tradeoff is that the bytes
    // allocated won't count non-object allocations, so it will be a bit low.
    //

    Vm->BytesAllocated = 0;
    Vm->GarbageRuns += 1;
    Vm->GarbageFreed = 0;

    //
    // Set up the head of the kiss list. Make it a circle so that the last
    // object added does not have a non-null pointer.
    //

    KissHead.Type = CkObjectInvalid;
    KissHead.Next = NULL;
    KissHead.NextKiss = &KissHead;
    Vm->KissList = &KissHead;
    CkpKissObject(Vm, &(Vm->Modules->Header));
    CkpKissObject(Vm, &(Vm->ModulePath->Header));
    for (Index = 0; Index < Vm->WorkingObjectCount; Index += 1) {
        CkpKissObject(Vm, Vm->WorkingObjects[Index]);
    }

    CkpKissObject(Vm, &(Vm->Fiber->Header));
    if (Vm->Compiler != NULL) {
        CkpKissCompiler(Vm, Vm->Compiler);
    }

    CkpKissObject(Vm, &(Vm->UnhandledException->Header));
    CkpDeeplyKiss(Vm, &KissHead);
    CkpCollectUnkissedObjects(Vm);

    //
    // Determine the next garbage collection time, expressed as an additional
    // percentage growth. Except rather than using percent 100 exactly, use
    // 1024 to avoid the divide. It looks nearly the same as percent times 10.
    //

    Vm->NextGarbageCollection =
             Vm->BytesAllocated +
             (Vm->BytesAllocated * Vm->Configuration.HeapGrowthPercent / 1024);

    if (Vm->NextGarbageCollection < Vm->Configuration.MinimumHeapSize) {
        Vm->NextGarbageCollection = Vm->Configuration.MinimumHeapSize;
    }

    return;
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

        //
        // If there's already a memory exception in progress, then this is
        // serious. Call the emergency function.
        //

        if (Vm->MemoryException != 0) {
            CkpError(Vm, CkErrorNoMemory, "Allocation failure");

        } else {
            Vm->MemoryException += 1;
            CkpRuntimeError(Vm, "MemoryError", "Allocation failure");
            Vm->MemoryException -= 1;
        }
    }

    return Allocation;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpKissCompiler (
    PCK_VM Vm,
    PCK_COMPILER Compiler
    )

/*++

Routine Description:

    This routine kisses a compiler, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Compiler - Supplies a pointer to the compiler to kiss.

Return Value:

    None.

--*/

{

    //
    // There's only ever one parser, no matter how many function compilers deep.
    //

    if (Compiler->Parser != NULL) {
        CkpKissObject(Vm, &(Compiler->Parser->Module->Header));
    }

    //
    // Kiss each compiler up the parent chain of functions being compiled.
    //

    while (Compiler != NULL) {
        CkpKissObject(Vm, &(Compiler->Function->Header));
        if (Compiler->EnclosingClass != NULL) {
            CkpKissValueArray(Vm, &(Compiler->EnclosingClass->Fields.List));
            CkpKissObject(Vm, &(Compiler->EnclosingClass->Fields.Dict->Header));
        }

        //
        // Most things in the compiler are allocated as local variables on the
        // stack. Only count those bytes that are actually dynamically
        // allocated.
        //

        Vm->BytesAllocated += (Compiler->LocalCapacity * sizeof(CK_LOCAL)) +
                              (Compiler->UpvalueCapacity *
                               sizeof(CK_COMPILER_UPVALUE));

        Compiler = Compiler->Parent;
    }

    return;
}

VOID
CkpKissValue (
    PCK_VM Vm,
    CK_VALUE Value
    )

/*++

Routine Description:

    This routine kisses a value, preventing it from being garbage collected
    during the garbage collection pass currently in progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Value - Supplies the value to kiss.

Return Value:

    None.

--*/

{

    if (CK_IS_OBJECT(Value)) {
        CkpKissObject(Vm, CK_AS_OBJECT(Value));
    }

    return;
}

VOID
CkpKissObject (
    PCK_VM Vm,
    PCK_OBJECT Object
    )

/*++

Routine Description:

    This routine kisses an object, preventing it from being garbage collected
    during the garbage collection pass currently in progress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to kiss.

Return Value:

    None.

--*/

{

    PCK_OBJECT End;

    if ((Object != NULL) && (Object->NextKiss == NULL)) {

        //
        // Wire the object in after the end of the list, and make it the new
        // end.
        //

        End = Vm->KissList;
        Object->NextKiss = End->NextKiss;
        End->NextKiss = Object;
        Vm->KissList = Object;
    }

    return;
}

VOID
CkpDeeplyKiss (
    PCK_VM Vm,
    PCK_OBJECT Head
    )

/*++

Routine Description:

    This routine performs a depth first traversal of the objects on the kiss
    list, kissing each of their components recursively.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Head - Supplies a pointer to a dummy object representing the head of the
        kiss list. This object itself is not kissed.

Return Value:

    None.

--*/

{

    PCK_OBJECT Object;

    //
    // Loop through all the objects on the kiss list. Kissing these objects
    // may cause more to get added to the end of the list.
    //

    Object = Head->NextKiss;
    while (Object != Head) {
        switch (Object->Type) {
        case CkObjectClass:
            CkpKissClass(Vm, (PCK_CLASS)Object);
            break;

        case CkObjectClosure:
            CkpKissClosure(Vm, (PCK_CLOSURE)Object);
            break;

        case CkObjectFiber:
            CkpKissFiber(Vm, (PCK_FIBER)Object);
            break;

        case CkObjectFunction:
            CkpKissFunction(Vm, (PCK_FUNCTION)Object);
            break;

        case CkObjectForeign:
            CkpKissForeignData(Vm, (PCK_FOREIGN_DATA)Object);
            break;

        case CkObjectInstance:
            CkpKissInstance(Vm, (PCK_INSTANCE)Object);
            break;

        case CkObjectList:
            CkpKissList(Vm, (PCK_LIST)Object);
            break;

        case CkObjectDict:
            CkpKissDict(Vm, (PCK_DICT)Object);
            break;

        case CkObjectModule:
            CkpKissModule(Vm, (PCK_MODULE)Object);
            break;

        case CkObjectRange:
            CkpKissRange(Vm, (PCK_RANGE)Object);
            break;

        case CkObjectString:
            CkpKissString(Vm, (PCK_STRING)Object);
            break;

        case CkObjectUpvalue:
            CkpKissUpvalue(Vm, (PCK_UPVALUE)Object);
            break;

        default:

            CK_ASSERT(FALSE);

            break;
        }

        Object = Object->NextKiss;
    }

    return;
}

VOID
CkpCollectUnkissedObjects (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine garbage collects any objects that have not been kissed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_OBJECT DeadAndAlone;
    ULONG DestroyCount;
    PCK_OBJECT *Object;

    DestroyCount = 0;
    Object = &(Vm->FirstObject);
    while (*Object != NULL) {

        //
        // Take this opportunity to ensure that all objects have classes.
        // Tack on a couple of conditions on the end to handle gaps during
        // early init.
        //

        CK_ASSERT(((*Object)->Class != NULL) ||
                  ((*Object)->Type == CkObjectFunction) ||
                  ((*Object)->Type == CkObjectUpvalue) ||
                  (Vm->Class.Class == NULL) ||
                  (Vm->Class.Class->Flags == 0));

        //
        // If the object has been kissed, then reset it for next time.
        //

        if ((*Object)->NextKiss != NULL) {
            (*Object)->NextKiss = NULL;
            Object = &((*Object)->Next);

        //
        // The object was never kissed. No one loves it, and it serves no
        // purpose.
        //

        } else {
            DeadAndAlone = *Object;
            *Object = DeadAndAlone->Next;
            CkpDestroyObject(Vm, DeadAndAlone);
            DestroyCount += 1;
        }
    }

    Vm->GarbageFreed = DestroyCount;
    if ((CK_VM_FLAG_SET(Vm, CK_CONFIGURATION_GC_STRESS)) &&
        (DestroyCount != 0)) {

        CkpDebugPrint(Vm, "%d objects destroyed\n", DestroyCount);
    }

    return;
}

VOID
CkpKissClass (
    PCK_VM Vm,
    PCK_CLASS Class
    )

/*++

Routine Description:

    This routine kisses a class object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Class - Supplies a pointer to the class.

Return Value:

    None.

--*/

{

    CkpKissObject(Vm, &(Class->Header.Class->Header));
    CkpKissObject(Vm, &(Class->Super->Header));
    CkpKissObject(Vm, &(Class->Methods->Header));
    CkpKissObject(Vm, &(Class->Name->Header));
    CkpKissObject(Vm, &(Class->Module->Header));
    Vm->BytesAllocated += sizeof(CK_CLASS);
    return;
}

VOID
CkpKissClosure (
    PCK_VM Vm,
    PCK_CLOSURE Closure
    )

/*++

Routine Description:

    This routine kisses a closure object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Closure - Supplies a pointer to the closure.

Return Value:

    None.

--*/

{

    CK_SYMBOL_INDEX Index;
    CK_SYMBOL_INDEX UpvalueCount;

    UpvalueCount = 0;
    CkpKissObject(Vm, &(Closure->Class->Header));
    switch (Closure->Type) {
    case CkClosureBlock:
        CkpKissObject(Vm, &(Closure->U.Block.Function->Header));
        UpvalueCount = Closure->U.Block.Function->UpvalueCount;
        for (Index = 0; Index < UpvalueCount; Index += 1) {
            CkpKissObject(Vm, &(Closure->Upvalues[Index]->Header));
        }

        break;

    case CkClosurePrimitive:
        CkpKissObject(Vm, &(Closure->U.Primitive.Name->Header));
        break;

    case CkClosureForeign:
        CkpKissObject(Vm, &(Closure->U.Foreign.Name->Header));
        CkpKissObject(Vm, &(Closure->U.Foreign.Module->Header));
        break;

    default:

        CK_ASSERT(FALSE);

        break;
    }

    Vm->BytesAllocated += sizeof(CK_CLOSURE) +
                          (UpvalueCount * sizeof(PCK_UPVALUE));

    return;
}

VOID
CkpKissDict (
    PCK_VM Vm,
    PCK_DICT Dict
    )

/*++

Routine Description:

    This routine kisses a dictionary object, preventing its components from
    being garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Dict - Supplies a pointer to the dictionary.

Return Value:

    None.

--*/

{

    PCK_DICT_ENTRY Entry;
    UINTN Index;

    for (Index = 0; Index < Dict->Capacity; Index += 1) {
        Entry = &(Dict->Entries[Index]);
        if (!CK_IS_UNDEFINED(Entry->Key)) {
            CkpKissValue(Vm, Entry->Key);
            CkpKissValue(Vm, Entry->Value);
        }
    }

    Vm->BytesAllocated += sizeof(CK_DICT) +
                          (Dict->Capacity * sizeof(CK_DICT_ENTRY));

    return;
}

VOID
CkpKissFiber (
    PCK_VM Vm,
    PCK_FIBER Fiber
    )

/*++

Routine Description:

    This routine kisses a fiber object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Fiber - Supplies a pointer to the fiber.

Return Value:

    None.

--*/

{

    UINTN Index;
    PCK_VALUE Stack;
    PCK_UPVALUE Upvalue;

    //
    // Kiss the call frames.
    //

    for (Index = 0; Index < Fiber->FrameCount; Index += 1) {
        CkpKissObject(Vm, &(Fiber->Frames[Index].Closure->Header));
    }

    //
    // Kiss the stack values.
    //

    Stack = Fiber->Stack;
    while (Stack < Fiber->StackTop) {
        CkpKissValue(Vm, *Stack);
        Stack += 1;
    }

    //
    // Kiss the open upvalues.
    //

    Upvalue = Fiber->OpenUpvalues;
    while (Upvalue != NULL) {
        CkpKissObject(Vm, &(Upvalue->Header));
        Upvalue = Upvalue->Next;
    }

    CkpKissObject(Vm, &(Fiber->Caller->Header));
    CkpKissValue(Vm, Fiber->Error);
    Vm->BytesAllocated += sizeof(CK_FIBER) +
                          (Fiber->FrameCapacity * sizeof(CK_CALL_FRAME)) +
                          (Fiber->TryCapacity * sizeof(CK_TRY_BLOCK)) +
                          (Fiber->StackCapacity * sizeof(CK_VALUE));

    return;
}

VOID
CkpKissForeignData (
    PCK_VM Vm,
    PCK_FOREIGN_DATA ForeignData
    )

/*++

Routine Description:

    This routine kisses a foreign data object, preventing its components from
    being garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ForeignData - Supplies a pointer to the foreign data object.

Return Value:

    None.

--*/

{

    Vm->BytesAllocated += sizeof(CK_FOREIGN_DATA);
    return;
}

VOID
CkpKissFunction (
    PCK_VM Vm,
    PCK_FUNCTION Function
    )

/*++

Routine Description:

    This routine kisses a function object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the function.

Return Value:

    None.

--*/

{

    CkpKissValueArray(Vm, &(Function->Constants));
    CkpKissObject(Vm, &(Function->Module->Header));
    CkpKissObject(Vm, &(Function->Debug.Name->Header));
    Vm->BytesAllocated += sizeof(CK_FUNCTION) +
                          (sizeof(UCHAR) * Function->Code.Capacity) +
                          (sizeof(UCHAR) *
                           Function->Debug.LineProgram.Capacity);

    return;
}

VOID
CkpKissInstance (
    PCK_VM Vm,
    PCK_INSTANCE Instance
    )

/*++

Routine Description:

    This routine kisses a class instance object, preventing its components from
    being garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Instance - Supplies a pointer to the class instance.

Return Value:

    None.

--*/

{

    CK_SYMBOL_INDEX Count;
    CK_SYMBOL_INDEX Index;

    CkpKissObject(Vm, &(Instance->Header.Class->Header));
    Count = Instance->Header.Class->FieldCount;
    for (Index = 0; Index < Count; Index += 1) {
        CkpKissValue(Vm, Instance->Fields[Index]);
    }

    Vm->BytesAllocated += sizeof(CK_INSTANCE) + (Count * sizeof(CK_VALUE));
    return;
}

VOID
CkpKissList (
    PCK_VM Vm,
    PCK_LIST List
    )

/*++

Routine Description:

    This routine kisses a list object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list.

Return Value:

    None.

--*/

{

    CkpKissValueArray(Vm, &(List->Elements));
    Vm->BytesAllocated += sizeof(CK_LIST);
    return;
}

VOID
CkpKissModule (
    PCK_VM Vm,
    PCK_MODULE Module
    )

/*++

Routine Description:

    This routine kisses a module object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module.

Return Value:

    None.

--*/

{

    CkpKissValueArray(Vm, &(Module->Variables));
    CkpKissObject(Vm, &(Module->VariableNames.Dict->Header));
    CkpKissValueArray(Vm, &(Module->VariableNames.List));
    CkpKissObject(Vm, &(Module->Strings.Dict->Header));
    CkpKissValueArray(Vm, &(Module->Strings.List));
    CkpKissObject(Vm, &(Module->Name->Header));
    CkpKissObject(Vm, &(Module->Path->Header));
    CkpKissObject(Vm, &(Module->Closure->Header));
    Vm->BytesAllocated = sizeof(CK_MODULE);
    return;
}

VOID
CkpKissRange (
    PCK_VM Vm,
    PCK_RANGE Range
    )

/*++

Routine Description:

    This routine kisses a range object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Range - Supplies a pointer to the range.

Return Value:

    None.

--*/

{

    Vm->BytesAllocated += sizeof(CK_RANGE);
    return;
}

VOID
CkpKissString (
    PCK_VM Vm,
    PCK_STRING String
    )

/*++

Routine Description:

    This routine kisses a string object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string.

Return Value:

    None.

--*/

{

    Vm->BytesAllocated += sizeof(CK_STRING) + String->Length + 1;
    return;
}

VOID
CkpKissUpvalue (
    PCK_VM Vm,
    PCK_UPVALUE Upvalue
    )

/*++

Routine Description:

    This routine kisses an upvalue object, preventing its components from being
    garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Upvalue - Supplies a pointer to the upvalue.

Return Value:

    None.

--*/

{

    CkpKissValue(Vm, Upvalue->Closed);
    Vm->BytesAllocated += sizeof(CK_UPVALUE);
    return;
}

VOID
CkpKissValueArray (
    PCK_VM Vm,
    PCK_VALUE_ARRAY Array
    )

/*++

Routine Description:

    This routine kisses each value in a value array, ensuring none of the
    values in the array get garbage collected. This routine also accounts for
    the array space in the VM's bytes allocated.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Array - Supplies a pointer to the value array to kiss.

Return Value:

    None.

--*/

{

    UINTN Count;
    UINTN Index;

    Count = Array->Count;
    for (Index = 0; Index < Count; Index += 1) {
        CkpKissValue(Vm, Array->Data[Index]);
    }

    Vm->BytesAllocated += Array->Capacity * sizeof(CK_VALUE);
    return;
}

