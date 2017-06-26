/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    capi.c

Abstract:

    This module implement the Chalk C API, which allows Chalk and C to
    interface naturally together.

Author:

    Evan Green 14-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros evaluate to non-zero if the stack has room for a push or a pop
// of the given size.
//

#define CK_CAN_PUSH(_Fiber, _Count) \
    ((_Fiber)->StackTop + (_Count) <= \
     ((_Fiber)->Stack + (_Fiber)->StackCapacity))

#define CK_CAN_POP(_Fiber, _Count) \
    ((_Fiber)->StackTop - (_Count) >= (_Fiber)->Stack)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PCK_VALUE
CkpGetStackIndex (
    PCK_VM Vm,
    INTN Index
    );

PCK_VALUE
CkpGetFieldIndex (
    PCK_VM Vm,
    UINTN FieldIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the mapping between built-in object type and API types.
//

const CK_API_TYPE CkApiObjectTypes[CkObjectTypeCount] = {
    CkTypeInvalid,  // CkObjectInvalid
    CkTypeObject,   // CkObjectClass
    CkTypeFunction, // CkObjectClosure
    CkTypeDict,     // CkObjectDict
    CkTypeObject,   // CkObjectFiber
    CkTypeData,     // CkObjectForeign
    CkTypeObject,   // CkObjectFunction
    CkTypeObject,   // CkObjectInstance
    CkTypeList,     // CkObjectList
    CkTypeObject,   // CkObjectModule
    CkTypeObject,   // CkObjectRange
    CkTypeString,   // CkObjectString
    CkTypeObject,   // CkObjectUpvalue
};

//
// ------------------------------------------------------------------ Functions
//

CK_API
PVOID
CkGetContext (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the context pointer stored inside the Chalk VM. This
    pointer is not used at all by Chalk, and can be used by the surrounding
    environment integrating Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the user context pointer.

--*/

{

    return Vm->Context;
}

CK_API
PVOID
CkSetContext (
    PCK_VM Vm,
    PVOID NewValue
    )

/*++

Routine Description:

    This routine sets the context pointer stored inside the Chalk VM. This
    pointer is not used at all by Chalk, and can be used by the surrounding
    environment integrating Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    NewValue - Supplies the new context pointer value to set.

Return Value:

    Returns the previous value.

--*/

{

    PVOID Previous;

    Previous = Vm->Context;
    Vm->Context = NewValue;
    return Previous;
}

CK_API
BOOL
CkPreloadForeignModule (
    PCK_VM Vm,
    PSTR ModuleName,
    PSTR Path,
    PVOID Handle,
    PCK_FOREIGN_FUNCTION LoadFunction
    )

/*++

Routine Description:

    This routine registers the availability of a foreign module that might not
    otherwise be reachable via the standard module load methods. This is often
    used for adding specialized modules in an embedded interpreter. The load
    function isn't called until someone actually imports the module from the
    interpreter. The loaded module is pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the full "dotted.module.name". A copy of
        this memory will be made.

    Path - Supplies an optional pointer to the full path of the module. A copy
        of this memory will be made.

    Handle - Supplies an optional pointer to a handle (usually a dynamic
        library handle) that is used if the module is unloaded.

    LoadFunction - Supplies a pointer to a C function to call to load the
        module symbols. The function will be called on a new fiber, with the
        module itself in slot zero.

Return Value:

    TRUE on success.

    FALSE on failure (usually allocation failure).

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;
    CK_VALUE ModuleValue;
    CK_VALUE NameString;
    CK_VALUE PathString;

    NameString = CkpStringCreate(Vm, ModuleName, strlen(ModuleName));
    if (CK_IS_NULL(NameString)) {
        return FALSE;
    }

    PathString = CkNullValue;
    if (Path != NULL) {
        CkpPushRoot(Vm, CK_AS_OBJECT(NameString));
        PathString = CkpStringCreate(Vm, Path, strlen(Path));
        CkpPopRoot(Vm);
        if (CK_IS_NULL(PathString)) {
            return FALSE;
        }
    }

    Module = CkpModuleLoadForeign(Vm,
                                  NameString,
                                  PathString,
                                  Handle,
                                  LoadFunction);

    if (Module == NULL) {
        return FALSE;
    }

    CK_OBJECT_VALUE(ModuleValue, Module);
    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_PUSH(Vm->Fiber, ModuleValue);
    return TRUE;
}

CK_API
BOOL
CkLoadModule (
    PCK_VM Vm,
    PCSTR ModuleName,
    PCSTR Path
    )

/*++

Routine Description:

    This routine loads (but does not run) the given module, and pushes it on
    the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies a pointer to the full "dotted.module.name". A copy of
        this memory will be made.

    Path - Supplies an optional pointer to the full path of the module. A copy
        of this memory will be made. If this is supplied, then this is the only
        path that is attempted when opening the module. If this is not supplied,
        then the standard load paths will be used. If a module by the given
        name is already loaded, this is ignored.

Return Value:

    TRUE on success.

    FALSE on failure. In this case, an exception will have been thrown. The
    caller should not modify the stack anymore, and should return as soon as
    possible.

--*/

{

    PCK_FIBER Fiber;
    CK_VALUE Module;
    CK_VALUE NameString;

    NameString = CkpStringCreate(Vm, ModuleName, strlen(ModuleName));
    if (CK_IS_NULL(NameString)) {
        return FALSE;
    }

    Module = CkpModuleLoad(Vm, NameString, Path);
    if (CK_IS_NULL(Module)) {
        return FALSE;
    }

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_PUSH(Vm->Fiber, Module);
    return TRUE;
}

CK_API
UINTN
CkGetStackSize (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the number of elements currently on the stack for the
    current frame.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the number of stack slots occupied by the current frame.

--*/

{

    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;

    Fiber = Vm->Fiber;
    if (Fiber == NULL) {
        return 0;
    }

    //
    // If there's a call frame, return the the number of stack slots occupied
    // by the current frame.
    //

    if (Fiber->FrameCount != 0) {
        Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
        return Fiber->StackTop - Frame->StackStart;
    }

    //
    // If there's no call frame, return the direct usage.
    //

    return Fiber->StackTop - Fiber->Stack;
}

CK_API
UINTN
CkGetStackRemaining (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns the number of free slots remaining on the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns the number of stack slots available to the C API.

--*/

{

    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    PCK_VALUE StackEnd;

    Fiber = Vm->Fiber;
    if (Fiber == NULL) {
        return 0;
    }

    //
    // If there's a call frame, return the number of slots available starting
    // at this frame.
    //

    if (Fiber->FrameCount != 0) {
        Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
        StackEnd = Fiber->Stack + Fiber->StackCapacity;

        CK_ASSERT(Frame->StackStart < StackEnd);

        return StackEnd - Frame->StackStart;
    }

    //
    // If there's no call frame, return the direct capacity.
    //

    return Fiber->StackCapacity;
}

CK_API
BOOL
CkEnsureStack (
    PCK_VM Vm,
    UINTN Size
    )

/*++

Routine Description:

    This routine ensures that there are at least the given number of
    stack slots currently available for the C API.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Size - Supplies the number of additional stack slots needed by the C API.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    PCK_FIBER Fiber;

    //
    // Initialize a fiber if needed.
    //

    if (Vm->Fiber == NULL) {
        Vm->Fiber = CkpFiberCreate(Vm, NULL);
        if (Vm->Fiber == NULL) {
            return FALSE;
        }
    }

    Fiber = Vm->Fiber;
    if (Fiber->StackTop + Size > Fiber->Stack + Fiber->StackCapacity) {
        CkpEnsureStack(Vm, Fiber, (Fiber->StackTop - Fiber->Stack) + Size);
        if (Fiber->StackTop + Size > Fiber->Stack + Fiber->StackCapacity) {
            return FALSE;
        }
    }

    return TRUE;
}

CK_API
VOID
CkPushValue (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine pushes a value already on the stack to the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the existing value to push.
        Negative values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_VALUE Source;

    Source = CkpGetStackIndex(Vm, StackIndex);
    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_PUSH(Vm->Fiber, *Source);
    return;
}

CK_API
VOID
CkStackRemove (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine removes a value from the stack, and shifts all the other
    values down.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the value to remove. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_VALUE Source;

    Fiber = Vm->Fiber;
    Source = CkpGetStackIndex(Vm, StackIndex);
    while (Source + 1 < Fiber->StackTop) {
        *Source = *(Source + 1);
        Source += 1;
    }

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    Fiber->StackTop -= 1;
    return;
}

CK_API
VOID
CkStackInsert (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine adds the element at the top of the stack into the given
    stack position, and shifts all remaining elements over.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index location to insert at. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

{

    PCK_VALUE Destination;
    PCK_FIBER Fiber;
    PCK_VALUE Move;

    Fiber = Vm->Fiber;
    Destination = CkpGetStackIndex(Vm, StackIndex);

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Move = Fiber->StackTop;
    while (Move > Destination) {
        *Move = *(Move - 1);
        Move -= 1;
    }

    *Destination = *(Fiber->StackTop);
    Fiber->StackTop += 1;
    return;
}

CK_API
VOID
CkStackReplace (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine pops the value from the top of the stack and replaces the
    value at the given stack index with it.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index to replace with the top of the stack.
        Negative values reference stack indices from the end of the stack. This
        is the stack index before the value is popped.

Return Value:

    None.

--*/

{

    PCK_VALUE Destination;
    PCK_FIBER Fiber;

    Destination = CkpGetStackIndex(Vm, StackIndex);
    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    *Destination = CK_POP(Fiber);
    return;
}

CK_API
CK_API_TYPE
CkGetType (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine returns the type of the value at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to query. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the stack type.

--*/

{

    PCK_OBJECT Object;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    switch (Value->Type) {
    case CkValueNull:
        return CkTypeNull;

    case CkValueInteger:
        return CkTypeInteger;

    case CkValueObject:
        Object = CK_AS_OBJECT(*Value);

        CK_ASSERT(Object->Type < CkObjectTypeCount);

        return CkApiObjectTypes[Object->Type];

    default:

        CK_ASSERT(FALSE);

        break;
    }

    return CkTypeInvalid;
}

CK_API
VOID
CkPushNull (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine pushes a null value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_PUSH(Fiber, CkNullValue);
    return;
}

CK_API
VOID
CkPushInteger (
    PCK_VM Vm,
    CK_INTEGER Integer
    )

/*++

Routine Description:

    This routine pushes an integer value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Integer - Supplies the integer to push.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_INT_VALUE(Value, Integer);
    CK_PUSH(Fiber, Value);
    return;
}

CK_API
CK_INTEGER
CkGetInteger (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine returns an integer at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the integer value.

    0 if the value at the stack is not an integer.

--*/

{

    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_INTEGER(*Value)) {
        return 0;
    }

    return CK_AS_INTEGER(*Value);
}

CK_API
VOID
CkPushString (
    PCK_VM Vm,
    PCSTR String,
    UINTN Length
    )

/*++

Routine Description:

    This routine pushes a string value on the top of the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string data to push. A copy of this
        string will be made.

    Length - Supplies the length of the string in bytes, not including the
        null terminator.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Value = CkpStringCreate(Vm, String, Length);
    CK_PUSH(Fiber, Value);
    return;
}

CK_API
PCSTR
CkGetString (
    PCK_VM Vm,
    UINTN StackIndex,
    PUINTN Length
    )

/*++

Routine Description:

    This routine returns a string at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

    Length - Supplies an optional pointer where the length of the string will
        be returned, not including a null terminator. If the value at the stack
        index is not a string, 0 is returned here.

Return Value:

    Returns a pointer to the string. The caller must not modify or free this
    value.

    NULL if the value at the specified stack index is not a string.

--*/

{

    PCK_STRING String;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_STRING(*Value)) {
        if (Length != NULL) {
            *Length = 0;
        }

        return NULL;
    }

    String = CK_AS_STRING(*Value);
    if (Length != NULL) {
        *Length = String->Length;
    }

    return String->Value;
}

CK_API
VOID
CkPushSubstring (
    PCK_VM Vm,
    INTN StackIndex,
    INTN Start,
    INTN End
    )

/*++

Routine Description:

    This routine creates a new string consisting of a portion of the string
    at the given stack index, and pushes it on the stack. If the value at the
    given stack index is not a string, then an empty string is pushed as the
    result. If either the start or end indices are out of range, they are
    adjusted to be in range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the string to slice. Negative
        values reference stack indices from the end of the stack.

    Start - Supplies the starting index of the substring, inclusive. Negative
        values reference from the end of the string, with -1 being after the
        last character of the string.

    End - Supplies the ending index of the substring, exclusive. Negative
        values reference from the end of the string, with -1 being after the
        last character of the string.

Return Value:

    None.

--*/

{

    UINTN EndIndex;
    PCK_FIBER Fiber;
    PCK_VALUE Source;
    UINTN StartIndex;
    PCK_STRING String;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Source = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_STRING(*Source)) {
        CkPushString(Vm, "", 0);
        return;
    }

    String = CK_AS_STRING(*Source);

    //
    // Make the values in range.
    //

    if (Start > String->Length) {
        Start = String->Length;

    } else if (Start < -String->Length) {
        Start = 0;
    }

    if (End > String->Length) {
        End = String->Length;

    } else if (End < -String->Length) {
        End = 0;
    }

    //
    // Convert the indices (which might be negative) into positive indices.
    //

    CK_INT_VALUE(Value, Start);
    StartIndex = CkpGetIndex(Vm, Value, String->Length);
    CK_INT_VALUE(Value, End);
    EndIndex = CkpGetIndex(Vm, Value, String->Length);

    CK_ASSERT((StartIndex <= String->Length) && (EndIndex <= String->Length));

    //
    // If the indices cross each other or are beyond the string, just push the
    // empty string.
    //

    if ((StartIndex >= String->Length) || (StartIndex >= EndIndex)) {
        CkPushString(Vm, "", 0);

    //
    // Otherwise, create the substring.
    //

    } else {
        CkPushString(Vm, String->Value + StartIndex, EndIndex - StartIndex);
    }

    return;
}

CK_API
VOID
CkStringConcatenate (
    PCK_VM Vm,
    UINTN Count
    )

/*++

Routine Description:

    This routine pops a given number of strings off the stack and concatenates
    them. The resulting string is then pushed on the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Count - Supplies the number of strings to pop off the stack.

Return Value:

    None.

--*/

{

    PSTR Destination;
    PCK_FIBER Fiber;
    UINTN Index;
    PCK_STRING NewString;
    CK_VALUE NewValue;
    UINTN Size;
    PCK_STRING Source;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(Count != 0);
    CK_ASSERT(CK_CAN_POP(Fiber, Count - 1));

    Value = Fiber->StackTop - 1;

    //
    // Loop through once to get the size.
    //

    Size = 0;
    for (Index = 0; Index < Count; Index += 1) {
        if (CK_IS_STRING(*Value)) {
            Source = CK_AS_STRING(*Value);
            Size += Source->Length;
        }
    }

    NewString = CkpStringAllocate(Vm, Size);
    if (NewString == NULL) {
        Fiber->StackTop -= Count;
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    //
    // Loop through again to create the concatenated string.
    //

    Destination = (PSTR)(NewString->Value);
    for (Index = 0; Index < Count; Index += 1) {
        if (CK_IS_STRING(*Value)) {
            Source = CK_AS_STRING(*Value);
            CkCopy(Destination, Source->Value, Source->Length);
            Destination += Source->Length;
        }
    }

    CkpStringHash(NewString);
    Fiber->StackTop -= Count;
    CK_OBJECT_VALUE(NewValue, NewString);
    CK_PUSH(Fiber, NewValue);
    return;
}

CK_API
PVOID
CkPushStringBuffer (
    PCK_VM Vm,
    UINTN MaxLength
    )

/*++

Routine Description:

    This routine creates an uninitialized string and pushes it on the top of
    the stack. The string must be finalized before use in the Chalk environment.
    Once finalized, the string buffer must not be modified.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MaxLength - Supplies the maximum length of the string buffer, not including
        a null terminator.

Return Value:

    Returns a pointer to the string buffer on success.

    NULL on allocation failure.

--*/

{

    PCK_FIBER Fiber;
    PCK_STRING String;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    String = CkpStringAllocate(Vm, MaxLength);
    if (String == NULL) {
        return NULL;
    }

    CK_OBJECT_VALUE(Value, String);
    CK_PUSH(Fiber, Value);
    return (PVOID)(String->Value);
}

CK_API
VOID
CkFinalizeString (
    PCK_VM Vm,
    INTN StackIndex,
    UINTN Length
    )

/*++

Routine Description:

    This routine finalizes a string that was previously created as a buffer.
    The string must not be modified after finalization.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the string to slice. Negative
        values reference stack indices from the end of the stack.

    Length - Supplies the final length of the string, not including the null
        terminator. This must not be greater than the initial maximum length
        provided when the string buffer was pushed.

Return Value:

    None.

--*/

{

    PCK_STRING String;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_STRING(*Value)) {

        CK_ASSERT(FALSE);

        return;
    }

    String = CK_AS_STRING(*Value);

    CK_ASSERT(Length <= String->Length);

    ((PSTR)String->Value)[Length] = '\0';
    String->Length = Length;
    CkpStringHash(String);
    return;
}

CK_API
VOID
CkPushDict (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine creates a new empty dictionary and pushes it onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_DICT Dict;
    PCK_FIBER Fiber;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Dict = CkpDictCreate(Vm);
    if (Dict == NULL) {
        CK_PUSH(Fiber, CkNullValue);

    } else {
        CK_OBJECT_VALUE(Value, Dict);
        CK_PUSH(Fiber, Value);
    }

    return;
}

CK_API
BOOL
CkDictGet (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine pops a key off the stack, and uses it to get the corresponding
    value for the dictionary stored at the given stack index. The resulting
    value is pushed onto the stack. If no value exists for the given key, then
    nothing is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before the key is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    TRUE if there was a value for that key.

    FALSE if the dictionary has no contents for that value.

--*/

{

    PCK_VALUE DictValue;
    PCK_FIBER Fiber;
    CK_VALUE Key;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    DictValue = CkpGetStackIndex(Vm, StackIndex);
    Key = CK_POP(Fiber);
    if (!CK_IS_DICT(*DictValue)) {
        return FALSE;
    }

    Value = CkpDictGet(CK_AS_DICT(*DictValue), Key);
    if (CK_IS_UNDEFINED(Value)) {
        return FALSE;
    }

    CK_PUSH(Fiber, Value);
    return TRUE;
}

CK_API
VOID
CkDictSet (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine pops a value and then a key off the stack, then sets that
    key-value pair in the dictionary at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before anything is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    None.

--*/

{

    PCK_VALUE DictValue;
    PCK_FIBER Fiber;
    PCK_VALUE Key;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 2));

    DictValue = CkpGetStackIndex(Vm, StackIndex);
    Key = Fiber->StackTop - 2;
    Value = Fiber->StackTop - 1;
    if (CK_IS_DICT(*DictValue)) {
        CkpDictSet(Vm, CK_AS_DICT(*DictValue), *Key, *Value);
    }

    Fiber->StackTop -= 2;
    return;
}

CK_API
VOID
CkDictRemove (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine pops a key off the stack, and removes that key and
    corresponding value from the dictionary. No error is raised if the key
    did not previously exist in the dictionary.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary (before anything is
        popped off). Negative values reference stack indices from the end of
        the stack.

Return Value:

    None.

--*/

{

    PCK_VALUE DictValue;
    PCK_FIBER Fiber;
    PCK_VALUE Key;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    DictValue = CkpGetStackIndex(Vm, StackIndex);
    Key = Fiber->StackTop - 1;
    if (CK_IS_DICT(*DictValue)) {
        CkpDictRemove(Vm, CK_AS_DICT(*DictValue), *Key);
    }

    Fiber->StackTop -= 1;
    return;
}

CK_API
UINTN
CkDictSize (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine returns the size of the dictionary at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary. Negative values
        reference stack indices from the end of the stack.

Return Value:

    Returns the number of elements in the dictionary.

    0 if the list is empty or the referenced item is not a dictionary.

--*/

{

    PCK_DICT Dict;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_DICT(*Value)) {
        return 0;
    }

    Dict = CK_AS_DICT(*Value);
    return Dict->Count;
}

CK_API
BOOL
CkDictIterate (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine advances a dictionary iterator at the top of the stack. It
    pushes the next key and then the next value onto the stack, if there are
    more elements in the dictionary. Callers should push a null value onto
    the stack as the initial iterator before calling this routine for the first
    time. Callers are responsible for popping the value, key, and potentially
    finished iterator off the stack. Callers should not modify a dictionary
    during iteration, as the results are undefined.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary. Negative values
        reference stack indices from the end of the stack.

Return Value:

    TRUE if the next key and value were pushed on.

    FALSE if there are no more elements, the iterator value is invalid, or the
    item at the given stack index is not a dictionary.

--*/

{

    PCK_DICT Dict;
    PCK_FIBER Fiber;
    CK_INTEGER Index;
    PCK_VALUE Iterator;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;
    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_DICT(*Value)) {
        return FALSE;
    }

    Dict = CK_AS_DICT(*Value);

    CK_ASSERT((CK_CAN_PUSH(Fiber, 2)) && (CK_CAN_POP(Fiber, 1)));

    Iterator = Fiber->StackTop - 1;
    Index = 0;
    if (!CK_IS_NULL(*Iterator)) {
        if (!CK_IS_INTEGER(*Iterator)) {
            return FALSE;
        }

        Index = CK_AS_INTEGER(*Iterator);
        if ((Index < 0) || (Index >= Dict->Capacity)) {
            *Iterator = CkNullValue;
            return FALSE;
        }

        Index += 1;
    }

    //
    // Find an occupied slot.
    //

    while (Index < Dict->Capacity) {
        if (!CK_IS_UNDEFINED(Dict->Entries[Index].Key)) {
            CK_INT_VALUE(*Iterator, Index);
            CK_PUSH(Fiber, Dict->Entries[Index].Key);
            CK_PUSH(Fiber, Dict->Entries[Index].Value);
            return TRUE;
        }

        Index += 1;
    }

    *Iterator = CkNullValue;
    return FALSE;
}

CK_API
VOID
CkPushList (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine creates a new empty list and pushes it onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_LIST List;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    List = CkpListCreate(Vm, 0);
    if (List == NULL) {
        CK_PUSH(Fiber, CkNullValue);

    } else {
        CK_OBJECT_VALUE(Value, List);
        CK_PUSH(Fiber, Value);
    }

    return;
}

CK_API
VOID
CkListGet (
    PCK_VM Vm,
    INTN StackIndex,
    INTN ListIndex
    )

/*++

Routine Description:

    This routine gets the value at the given list index, and pushes it on the
    stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

    ListIndex - Supplies the list index to get. If this index is out of bounds,
        the null will be pushed.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    UINTN Index;
    CK_VALUE IndexValue;
    PCK_LIST List;
    PCK_VALUE ListValue;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    ListValue = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_LIST(*ListValue)) {
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    List = CK_AS_LIST(*ListValue);
    if ((ListIndex >= (INTN)List->Elements.Count) ||
        (-ListIndex > (INTN)List->Elements.Count)) {

        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    CK_INT_VALUE(IndexValue, ListIndex);
    Index = CkpGetIndex(Vm, IndexValue, List->Elements.Count);

    CK_ASSERT(Index < List->Elements.Count);

    CK_PUSH(Fiber, List->Elements.Data[Index]);
    return;
}

CK_API
VOID
CkListSet (
    PCK_VM Vm,
    INTN StackIndex,
    INTN ListIndex
    )

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a specific
    index in a list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

    ListIndex - Supplies the list index to set. If this index is one beyond the
        end, then the value will be appended. If this index is otherwise out of
        bounds, the item at the top of the stack will simply be discarded.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    UINTN Index;
    CK_VALUE IndexValue;
    PCK_LIST List;
    PCK_VALUE ListValue;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    ListValue = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_LIST(*ListValue)) {
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    List = CK_AS_LIST(*ListValue);
    Value = *(Fiber->StackTop - 1);
    if (ListIndex == List->Elements.Count) {
        CkpArrayAppend(Vm, &(List->Elements), Value);

    } else if ((ListIndex < List->Elements.Count) ||
               (-ListIndex <= (INTN)List->Elements.Count)) {

        CK_INT_VALUE(IndexValue, ListIndex);
        Index = CkpGetIndex(Vm, IndexValue, List->Elements.Count);

        CK_ASSERT(Index < List->Elements.Count);

        List->Elements.Data[Index] = Value;
    }

    Fiber->StackTop -= 1;
    return;
}

CK_API
UINTN
CkListSize (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine returns the size of the list at the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the list. Negative values
        reference stack indices from the end of the stack.

Return Value:

    Returns the number of elements in the list.

    0 if the list is empty or the referenced item is not a list.

--*/

{

    PCK_LIST List;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_LIST(*Value)) {
        return 0;
    }

    List = CK_AS_LIST(*Value);
    return List->Elements.Count;
}

CK_API
BOOL
CkPushData (
    PCK_VM Vm,
    PVOID Data,
    PCK_DESTROY_DATA DestroyRoutine
    )

/*++

Routine Description:

    This routine pushes an opaque pointer onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Data - Supplies the pointer to encapsulate.

    DestroyRoutine - Supplies an optional pointer to a function to call if this
        value is garbage collected.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    PCK_FOREIGN_DATA DataObject;
    PCK_FIBER Fiber;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    DataObject = CkAllocate(Vm, sizeof(CK_FOREIGN_DATA));
    if (DataObject == NULL) {
        return FALSE;
    }

    CkpInitializeObject(Vm,
                        &(DataObject->Header),
                        CkObjectForeign,
                        Vm->Class.Null);

    DataObject->Data = Data;
    DataObject->Destroy = DestroyRoutine;
    CK_OBJECT_VALUE(Value, DataObject);
    CK_PUSH(Fiber, Value);
    return TRUE;
}

CK_API
PVOID
CkGetData (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine returns a data pointer that is stored the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the object to get. Negative
        values reference stack indices from the end of the stack.

Return Value:

    Returns the opaque pointer passed in when the object was created.

    NULL if the value at the stack was not a foreign data object.

--*/

{

    PCK_FOREIGN_DATA DataObject;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_FOREIGN(*Value)) {
        return NULL;
    }

    DataObject = CK_AS_FOREIGN(*Value);
    return DataObject->Data;
}

CK_API
VOID
CkPushClass (
    PCK_VM Vm,
    INTN ModuleIndex,
    ULONG FieldCount
    )

/*++

Routine Description:

    This routine pops a class and a string off the stack, creates a new class,
    and pushes it onto the stack. The popped class is the superclass of the
    new class, and the popped string is the name of the class.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleIndex - Supplies the stack index of the module to create the class in,
        before any items are popped from the stack.

    FieldCount - Supplies the number of fields to allocate for each instance of
        the class. When a new class is created, these fields start out as null.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 2));

    Value = CkpGetStackIndex(Vm, ModuleIndex);
    if (!CK_IS_MODULE(*Value)) {
        Fiber->StackTop -= 2;
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    Module = CK_AS_MODULE(*Value);
    CkpClassCreate(Vm, FieldCount, Module);
    return;
}

CK_API
VOID
CkPushFunction (
    PCK_VM Vm,
    PCK_FOREIGN_FUNCTION Function,
    PSTR Name,
    ULONG ArgumentCount,
    INTN ModuleIndex
    )

/*++

Routine Description:

    This routine pushes a C function onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Function - Supplies a pointer to the C function.

    Name - Supplies a pointer to a null terminated string containing the name
        of the function, used for debugging purposes. This name is not actually
        assigned in the Chalk namespace.

    ArgumentCount - Supplies the number of arguments the function takes, not
        including the receiver slot.

    ModuleIndex - Supplies the index of the module this function should be
        defined within. Functions must be tied to modules to ensure that the
        module containing the C function is not garbage collected and unloaded.

Return Value:

    None.

--*/

{

    PCK_CLOSURE Closure;
    CK_VALUE ClosureValue;
    PCK_FIBER Fiber;
    PCK_MODULE Module;
    CK_VALUE NameValue;
    PCK_VALUE Value;

    Value = CkpGetStackIndex(Vm, ModuleIndex);
    if (!CK_IS_MODULE(*Value)) {
        return;
    }

    Module = CK_AS_MODULE(*Value);
    NameValue = CkpStringCreate(Vm, Name, strlen(Name));
    if (CK_IS_NULL(NameValue)) {
        return;
    }

    Closure = CkpClosureCreateForeign(Vm,
                                      Function,
                                      Module,
                                      CK_AS_STRING(NameValue),
                                      ArgumentCount);

    if (Closure == NULL) {
        return;
    }

    CK_OBJECT_VALUE(ClosureValue, Closure);
    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    CK_PUSH(Fiber, ClosureValue);
    return;
}

CK_API
VOID
CkBindMethod (
    PCK_VM Vm,
    INTN ClassIndex
    )

/*++

Routine Description:

    This routine pops a string and then a function off the stack. It binds the
    function as a class method. The class is indicated by the given stack index
    (before either of the pops). The function may be either a C or Chalk
    function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ClassIndex - Supplies the stack index of the class to bind the function to.
        Negative values reference stack indices from the end of the stack.

Return Value:

    None.

--*/

{

    PCK_CLASS Class;
    PCK_VALUE ClassValue;
    PCK_CLOSURE Closure;
    CK_VALUE ClosureValue;
    PCK_FIBER Fiber;
    CHAR Name[CK_MAX_METHOD_SIGNATURE];
    UINTN NameSize;
    PCK_STRING NameString;
    CK_VALUE NameValue;
    CK_FUNCTION_SIGNATURE Signature;
    CK_SYMBOL_INDEX Symbol;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 2));

    //
    // Get the class, name, and function.
    //

    ClassValue = CkpGetStackIndex(Vm, ClassIndex);
    if (!CK_IS_CLASS(*ClassValue)) {
        goto BindMethodEnd;
    }

    Class = CK_AS_CLASS(*ClassValue);
    NameValue = *(Fiber->StackTop - 1);
    ClosureValue = *(Fiber->StackTop - 2);
    if ((!CK_IS_STRING(NameValue)) || (!CK_IS_CLOSURE(ClosureValue))) {
        goto BindMethodEnd;
    }

    //
    // Convert the name string into a signature string.
    //

    NameString = CK_AS_STRING(NameValue);
    Closure = CK_AS_CLOSURE(ClosureValue);
    Signature.Name = NameString->Value;
    Signature.Length = NameString->Length;
    Signature.Arity = CkpGetFunctionArity(Closure);
    NameSize = sizeof(Name);
    CkpPrintSignature(&Signature, Name, &NameSize);
    Symbol = CkpStringTableEnsure(Vm,
                                  &(Class->Module->Strings),
                                  Name,
                                  NameSize);

    if (Symbol < 0) {
        goto BindMethodEnd;
    }

    NameValue = Class->Module->Strings.List.Data[Symbol];
    CkpBindMethod(Vm, Class, NameValue, Closure);

BindMethodEnd:
    Fiber->StackTop -= 2;
    return;
}

CK_API
VOID
CkGetField (
    PCK_VM Vm,
    UINTN FieldIndex
    )

/*++

Routine Description:

    This routine gets the value from the instance field with the given index,
    and pushes it on the stack. This only applies to bound methods, and
    operates on the receiver ("this"). If the current method is not a bound
    method, or the field is out of bounds, null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldIndex - Supplies the field index of the instance to get.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Value = CkpGetFieldIndex(Vm, FieldIndex);
    if (Value == NULL) {
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    CK_PUSH(Fiber, *Value);
    return;
}

CK_API
VOID
CkSetField (
    PCK_VM Vm,
    UINTN FieldIndex
    )

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a specific
    field index in the function receiver. This function only applies to bound
    methods. If the current function is unbound or the field index is out of
    bounds, the value is popped and discarded.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldIndex - Supplies the field index of the intance to get.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    Value = CkpGetFieldIndex(Vm, FieldIndex);
    if (Value == NULL) {
        Fiber->StackTop -= 1;
        return;
    }

    *Value = CK_POP(Fiber);
    return;
}

CK_API
VOID
CkGetVariable (
    PCK_VM Vm,
    INTN StackIndex,
    PCSTR Name
    )

/*++

Routine Description:

    This routine gets a global variable and pushes it on the stack. If the
    variable does not exist in the given module, or the given stack index is
    not a module, then null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the module to look in. Negative
        values reference stack indices from the end of the stack.

    Name - Supplies a pointer to the null terminated string containing the
        name of the variable to get.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_MODULE(*Value)) {
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    Module = CK_AS_MODULE(*Value);
    Value = CkpFindModuleVariable(Vm, Module, Name, FALSE);
    if (Value != NULL) {
        CK_PUSH(Fiber, *Value);

    } else {
        CK_PUSH(Fiber, CkNullValue);
    }

    return;
}

CK_API
VOID
CkSetVariable (
    PCK_VM Vm,
    INTN StackIndex,
    PCSTR Name
    )

/*++

Routine Description:

    This routine pops the top value off the stack, and saves it to a global
    variable with the given name in the given module. If the variable did not
    exist previously, it is created.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the module to look in. Negative
        values reference stack indices from the end of the stack.

    Name - Supplies a pointer to the null terminated string containing the
        name of the variable to set.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_MODULE Module;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_POP(Fiber, 1));

    Value = CkpGetStackIndex(Vm, StackIndex);
    if (!CK_IS_MODULE(*Value)) {
        Fiber->StackTop -= 1;
        return;
    }

    Module = CK_AS_MODULE(*Value);
    Value = CkpFindModuleVariable(Vm, Module, Name, TRUE);
    if (Value != NULL) {
        *Value = CK_POP(Fiber);

    } else {
        Fiber->StackTop -= 1;
    }

    return;
}

CK_API
BOOL
CkCall (
    PCK_VM Vm,
    UINTN ArgumentCount
    )

/*++

Routine Description:

    This routine pops the given number of arguments off the stack, then pops
    a callable object or class, and executes that call. The return value is
    pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ArgumentCount - Supplies the number of arguments to the call. The callable
        object (either a function or a class) will also be popped after these
        arguments.

Return Value:

    TRUE on success.

    FALSE if an error occurred.

--*/

{

    CK_VALUE Callable;
    PCK_CLASS Class;
    PCK_CLOSURE Closure;
    PCK_FIBER Fiber;
    UINTN FrameCount;
    BOOL FramePushed;
    UINTN TryCount;

    Fiber = Vm->Fiber;
    TryCount = Fiber->TryCount;
    FrameCount = Fiber->FrameCount;

    CK_ASSERT(CK_CAN_POP(Fiber, ArgumentCount + 1));

    Callable = *(Fiber->StackTop - 1 - ArgumentCount);
    if (CK_IS_CLOSURE(Callable)) {
        Closure = CK_AS_CLOSURE(Callable);
        FramePushed = CkpCallFunction(Vm, Closure, ArgumentCount + 1);

    //
    // Calling a class is the official method of constructing a new object.
    // Run the interpreter to run the __init method that got pushed on.
    //

    } else if (CK_IS_CLASS(Callable)) {
        Class = CK_AS_CLASS(Callable);
        FramePushed = CkpInstantiateClass(Vm, Class, ArgumentCount + 1);

    } else {
        CkpRuntimeError(Vm, "TypeError", "Object is not callable");
        goto CallEnd;
    }

    if (!CK_IS_NULL(Fiber->Error)) {
        goto CallEnd;
    }

    //
    // If a new call frame was pushed, run the interpreter now.
    //

    if (FramePushed != FALSE) {
        CkpRunInterpreter(Vm, Fiber);
    }

CallEnd:

    //
    // The VM should not have allowed a fiber switch while the Fiber stack
    // is tied with the C stack.
    //

    CK_ASSERT((Vm->Fiber == Fiber) || (Vm->Fiber == NULL));

    if (CK_EXCEPTION_RAISED(Vm, Fiber, TryCount, FrameCount)) {
        return FALSE;
    }

    return TRUE;
}

CK_API
BOOL
CkCallMethod (
    PCK_VM Vm,
    PSTR MethodName,
    UINTN ArgumentCount
    )

/*++

Routine Description:

    This routine pops the given number of arguments off the stack, then pops
    an object, and executes the method with the given name on that object. The
    return value is pushed onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MethodName - Supplies a pointer to the null terminated string containing
        the name of the method to call.

    ArgumentCount - Supplies the number of arguments to the call. The class
        instance will also be popped after these arguments.

Return Value:

    TRUE on success.

    FALSE if an error occurred.

--*/

{

    PCK_CLASS Class;
    PCK_CLOSURE Closure;
    CK_STRING FakeString;
    PCK_FIBER Fiber;
    UINTN FrameCount;
    UINTN Length;
    CK_VALUE Method;
    CHAR Name[CK_MAX_METHOD_SIGNATURE];
    CK_VALUE NameValue;
    CK_FUNCTION_SIGNATURE Signature;
    UINTN TryCount;
    CK_VALUE Value;

    Fiber = Vm->Fiber;
    FrameCount = Fiber->FrameCount;
    TryCount = Fiber->TryCount;

    CK_ASSERT(CK_CAN_POP(Fiber, ArgumentCount + 1));

    Value = *(Fiber->StackTop - 1 - ArgumentCount);

    //
    // Create the init method signature function signature and invoke the
    // init method.
    //

    Signature.Name = MethodName;
    Signature.Length = strlen(MethodName);
    Signature.Arity = ArgumentCount;
    Length = sizeof(Name);
    CkpPrintSignature(&Signature, Name, &Length);
    CkpStringFake(&FakeString, Name, Length);
    CK_OBJECT_VALUE(NameValue, &FakeString);
    Class = CkpGetClass(Vm, Value);
    Method = CkpDictGet(Class->Methods, NameValue);
    if (CK_IS_UNDEFINED(Method)) {
        CkpRuntimeError(Vm,
                        "LookupError",
                        "Object of type %s does not implement method %s with "
                        "%d arguments",
                        Class->Name->Value,
                        MethodName,
                        ArgumentCount);

        goto CallMethodEnd;
    }

    //
    // Call the method. If it pushed a call frame, run the interpreter.
    //

    CK_ASSERT(CK_IS_CLOSURE(Method));

    Closure = CK_AS_CLOSURE(Method);
    if (CkpCallFunction(Vm, Closure, ArgumentCount + 1) != FALSE) {
        CkpRunInterpreter(Vm, Fiber);
    }

CallMethodEnd:

    //
    // The VM should not have allowed a fiber switch while the Fiber stack
    // is tied with the C stack.
    //

    if (CK_EXCEPTION_RAISED(Vm, Fiber, TryCount, FrameCount)) {
        return FALSE;
    }

    return TRUE;
}

CK_API
VOID
CkRaiseException (
    PCK_VM Vm,
    INTN StackIndex
    )

/*++

Routine Description:

    This routine raises an exception. The caller must not make any more
    modifications to the stack, and should return as soon as possible.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the exception to raise. Negative
        values reference stack indices from the end of the stack.

Return Value:

    None. The foreign function call frame is no longer on the execution stack.

--*/

{

    PCK_VALUE Exception;

    Exception = CkpGetStackIndex(Vm, StackIndex);
    if (!CkpObjectIsClass(CkpGetClass(Vm, *Exception), Vm->Class.Exception)) {
        CkpRuntimeError(Vm, "TypeError", "Expected an Exception");

    } else {
        CkpRaiseException(Vm, *Exception, 0);
    }

    return;
}

CK_API
VOID
CkRaiseBasicException (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR MessageFormat,
    ...
    )

/*++

Routine Description:

    This routine reports a runtime error in the current fiber. The caller must
    not make any more modifications to the stack, and should return as soon as
    possible.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Type - Supplies the name of a builtin exception type. This type must
        already be in scope.

    MessageFormat - Supplies the printf message format string. The total size
        of the resulting string is limited, so please be succinct.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, MessageFormat);
    CkpRaiseInternalException(Vm, Type, MessageFormat, ArgumentList);
    va_end(ArgumentList);
    return;
}

CK_API
VOID
CkPushModule (
    PCK_VM Vm,
    PSTR ModuleName
    )

/*++

Routine Description:

    This routine pushes the module with the given full.dotted.name onto the
    stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModuleName - Supplies the name of the module to push. If no module by the
        given name can be found, null is pushed.

Return Value:

    None.

--*/

{

    CK_VALUE Key;
    CK_STRING String;
    CK_VALUE Value;

    CK_ASSERT(CK_CAN_PUSH(Vm->Fiber, 1));

    Key = CkpStringFake(&String, ModuleName, strlen(ModuleName));
    Value = CkpDictGet(Vm->Modules, Key);
    if (CK_IS_UNDEFINED(Value)) {
        CK_PUSH(Vm->Fiber, CkNullValue);

    } else {
        CK_PUSH(Vm->Fiber, Value);
    }

    return;
}

CK_API
VOID
CkPushCurrentModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine pushes the module that the running function was defined in
    onto the stack. If no function is currently running, then null is pushed.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    if (Fiber->FrameCount == 0) {
        CK_PUSH(Fiber, CkNullValue);
        return;
    }

    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);

    CK_ASSERT(Frame->Closure->Type == CkClosureForeign);

    CK_OBJECT_VALUE(Value, Frame->Closure->U.Foreign.Module);
    CK_PUSH(Fiber, Value);
    return;
}

CK_API
VOID
CkPushModulePath (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine pushes the module path onto the stack.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    CK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(CK_CAN_PUSH(Fiber, 1));

    if (Vm->ModulePath == NULL) {
        Vm->ModulePath = CkpListCreate(Vm, 0);
        if (Vm->ModulePath == NULL) {
            CK_PUSH(Fiber, CkNullValue);
            return;
        }
    }

    CK_OBJECT_VALUE(Value, Vm->ModulePath);
    CK_PUSH(Fiber, Value);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PCK_VALUE
CkpGetStackIndex (
    PCK_VM Vm,
    INTN Index
    )

/*++

Routine Description:

    This routine returns a pointer into the stack for the given stack index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Index - Supplies the stack index to get. Negative values reference values
        from the end of the stack.

Return Value:

    Returns a pointer to the previous search path. The caller should not
    attempt to modify or free this value. It will be garbage collected.

--*/

{

    PCK_FIBER Fiber;
    PCK_VALUE Stack;
    PCK_VALUE Value;

    Fiber = Vm->Fiber;

    CK_ASSERT(Fiber != NULL);

    Stack = Fiber->Stack;
    if (Fiber->FrameCount != 0) {
        Stack = Fiber->Frames[Fiber->FrameCount - 1].StackStart;
    }

    if (Index >= 0) {
        Value = Stack + Index;

    } else {
        Value = Fiber->StackTop + Index;
    }

    CK_ASSERT((Value >= Stack) && (Value < Fiber->StackTop));

    return Value;
}

PCK_VALUE
CkpGetFieldIndex (
    PCK_VM Vm,
    UINTN FieldIndex
    )

/*++

Routine Description:

    This routine returns a pointer to the given field in the receiver.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    FieldIndex - Supplies the field index to get.

Return Value:

    Returns a pointer to the field on success.

    NULL if the current function is not a bound function or the field index is
    out of bounds.

--*/

{

    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    PCK_INSTANCE Instance;
    CK_VALUE Value;

    Fiber = Vm->Fiber;
    if (Fiber->FrameCount == 0) {
        return NULL;
    }

    Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);

    CK_ASSERT(Frame->Closure->Type == CkClosureForeign);

    if ((Frame->Closure->Class == NULL) ||
        (FieldIndex >= Frame->Closure->Class->FieldCount)) {

        return NULL;
    }

    Value = Frame->StackStart[0];

    CK_ASSERT(CK_IS_INSTANCE(Value));

    Instance = CK_AS_INSTANCE(Value);
    FieldIndex += Frame->Closure->Class->SuperFieldCount;

    CK_ASSERT(FieldIndex < Frame->Closure->Class->FieldCount);

    return &(Instance->Fields[FieldIndex]);
}

