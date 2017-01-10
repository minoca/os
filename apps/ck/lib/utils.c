/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    utils.c

Abstract:

    This module implements general Chalk core utility functions.

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
#include "compiler.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CK_INITIAL_ARRAY_CAPACITY 32

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

CK_ERROR_TYPE
CkpStringTableInitialize (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable
    )

/*++

Routine Description:

    This routine initializes a string table.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table to initialize.

Return Value:

    CkSuccess on success.

    CkErrorNoMemory on allocation failure.

--*/

{

    CkpInitializeArray(&(StringTable->List));
    StringTable->Dict = CkpDictCreate(Vm);
    if (StringTable->Dict == NULL) {
        return CkErrorNoMemory;
    }

    return CkSuccess;
}

VOID
CkpStringTableClear (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable
    )

/*++

Routine Description:

    This routine resets a string table to empty.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table to reset.

Return Value:

    None.

--*/

{

    CkpClearArray(Vm, &(StringTable->List));
    CkpDictClear(Vm, StringTable->Dict);
    return;
}

CK_SYMBOL_INDEX
CkpStringTableEnsure (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    PCSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the index of the given string in the table. If it did
    not exist before, it is added.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table.

    Name - Supplies a pointer to the name of the string to locate or add.

    Size - Supplies the size of the string, not including the null terminator.

Return Value:

    Returns the index of the new string on success.

    -1 on allocation failure.

--*/

{

    CK_STRING FakeObject;
    CK_VALUE Index;
    CK_VALUE String;

    String = CkpStringFake(&FakeObject, Name, Size);
    Index = CkpDictGet(StringTable->Dict, String);
    if (CK_IS_INTEGER(Index)) {
        return CK_AS_INTEGER(Index);
    }

    return CkpStringTableAdd(Vm, StringTable, Name, Size);
}

CK_SYMBOL_INDEX
CkpStringTableEnsureValue (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    CK_VALUE String
    )

/*++

Routine Description:

    This routine returns the index of the given string in the table. If it did
    not exist before, it is added.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table.

    String - Supplies the string to add.

Return Value:

    Returns the index of the new string on success.

    -1 on allocation failure.

--*/

{

    CK_VALUE Index;

    Index = CkpDictGet(StringTable->Dict, String);
    if (CK_IS_INTEGER(Index)) {
        return CK_AS_INTEGER(Index);
    }

    CK_INT_VALUE(Index, StringTable->List.Count);
    CkpDictSet(Vm, StringTable->Dict, String, Index);
    CkpArrayAppend(Vm, &(StringTable->List), String);
    return CK_AS_INTEGER(Index);
}

CK_SYMBOL_INDEX
CkpStringTableAdd (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    PCSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine unconditionally adds a string to the given string table.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table.

    Name - Supplies a pointer to the name of the string to add.

    Size - Supplies the size of the string, not including the null terminator.

Return Value:

    Returns the index of the new string on success.

    -1 on allocation failure.

--*/

{

    CK_VALUE Index;
    CK_VALUE String;

    String = CkpStringCreate(Vm, Name, Size);
    if (CK_IS_NULL(String)) {
        return -1;
    }

    CK_INT_VALUE(Index, StringTable->List.Count);
    CkpDictSet(Vm, StringTable->Dict, String, Index);
    CkpArrayAppend(Vm, &(StringTable->List), String);
    return CK_AS_INTEGER(Index);
}

CK_SYMBOL_INDEX
CkpStringTableFind (
    PCK_STRING_TABLE StringTable,
    PCSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the index of the given string table.

Arguments:

    StringTable - Supplies a pointer to the string table.

    Name - Supplies a pointer to the name of the string to find.

    Size - Supplies the size of the string, not including the null terminator.

Return Value:

    Returns the index of the new string on success.

    -1 if the string does not exist in the table.

--*/

{

    CK_STRING FakeObject;
    CK_VALUE Index;
    CK_VALUE String;

    String = CkpStringFake(&FakeObject, Name, Size);
    Index = CkpDictGet(StringTable->Dict, String);
    if (CK_IS_INTEGER(Index)) {
        return CK_AS_INTEGER(Index);
    }

    return -1;
}

CK_ERROR_TYPE
CkpFillGenericArray (
    PCK_VM Vm,
    PVOID Array,
    PCVOID Data,
    UINTN ElementSize,
    UINTN Count
    )

/*++

Routine Description:

    This routine fills one of the CK_*_ARRAY types.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Array - Supplies a pointer to the array to fill.

    Data - Supplies a pointer to the first data element to fill with.

    ElementSize - Supplies the size of a single element.

    Count - Supplies the number of elements to fill.

Return Value:

    CkSuccess on success.

    CkErrorNoMemory on allocation failure.

--*/

{

    PCK_BYTE_ARRAY ByteArray;
    PVOID NewBuffer;
    UINTN NewCapacity;
    UINTN RequiredCapacity;

    //
    // Assume the thing is a byte array, even though it may not be. The fields
    // of each array type alias on top of each other.
    //

    ByteArray = Array;
    RequiredCapacity = ByteArray->Count + Count;
    if (ByteArray->Capacity < RequiredCapacity) {
        NewCapacity = ByteArray->Capacity;
        if (NewCapacity == 0) {
            NewCapacity = CK_INITIAL_ARRAY_CAPACITY;
        }

        while (NewCapacity < RequiredCapacity) {
            NewCapacity *= 2;
        }

        NewBuffer = CkpReallocate(Vm,
                                  ByteArray->Data,
                                  ByteArray->Capacity * ElementSize,
                                  NewCapacity * ElementSize);

        if (NewBuffer == NULL) {
            return CkErrorNoMemory;
        }

        ByteArray->Data = NewBuffer;
        ByteArray->Capacity = NewCapacity;
    }

    //
    // Copy the new elements in.
    //

    if (Data != NULL) {
        CkCopy(ByteArray->Data + (ByteArray->Count * ElementSize),
               Data,
               Count * ElementSize);

    } else {
        CkZero(ByteArray->Data + (ByteArray->Count * ElementSize),
               Count * ElementSize);
    }

    ByteArray->Count += Count;
    return CkSuccess;
}

CK_ERROR_TYPE
CkpSizeGenericArray (
    PCK_VM Vm,
    PVOID Array,
    UINTN ElementSize,
    UINTN Capacity
    )

/*++

Routine Description:

    This routine ensures a certain amount of capacity in one of the CK_*_ARRAY
    types.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Array - Supplies a pointer to the array to size.

    ElementSize - Supplies the size of a single element.

    Capacity - Supplies the capacity to ensure.

Return Value:

    CkSuccess on success.

    CkErrorNoMemory on allocation failure.

--*/

{

    PCK_BYTE_ARRAY ByteArray;
    PVOID NewBuffer;
    UINTN NewCapacity;

    //
    // Assume the thing is a byte array, even though it may not be. The fields
    // of each array type alias on top of each other.
    //

    ByteArray = Array;
    if (ByteArray->Capacity < Capacity) {
        NewCapacity = Capacity;
        if (NewCapacity < CK_INITIAL_ARRAY_CAPACITY) {
            NewCapacity = CK_INITIAL_ARRAY_CAPACITY;
        }

        NewBuffer = CkpReallocate(Vm,
                                  ByteArray->Data,
                                  ByteArray->Capacity * ElementSize,
                                  NewCapacity * ElementSize);

        if (NewBuffer == NULL) {
            return CkErrorNoMemory;
        }

        ByteArray->Data = NewBuffer;
        ByteArray->Capacity = NewCapacity;
    }

    return CkSuccess;
}

UINTN
CkpGetRange (
    PCK_VM Vm,
    PCK_RANGE Range,
    PUINTN Count
    )

/*++

Routine Description:

    This routine computes the starting index and length from a given range.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Range - Supplies a pointer to the range to get dimensions from.

    Count - Supplies a pointer that on input contains the number of elements
        possible to iterate over. On output, returns the number of elements to
        iterate over will be returned.

Return Value:

    Returns the index to start with on success.

    MAX_UINTN on validation failure. A runtime error will the thrown.

--*/

{

    CK_INTEGER From;
    CK_INTEGER To;

    //
    // If the thing is size zero, then it's easy.
    //

    if (*Count == 0) {
        return 0;
    }

    From = Range->From;
    if (From < 0) {
        From += *Count;
    }

    To = Range->To;
    if (To < 0) {
        To += *Count;
    }

    if (Range->Inclusive != FALSE) {
        To += 1;
    }

    if (From >= To) {
        *Count = 0;
        return 0;
    }

    if ((To < 0) || (From >= *Count)) {
        *Count = 0;
        return 0;
    }

    if (To > *Count) {
        To = *Count;
    }

    if (From < 0) {
        From = 0;
    }

    *Count = To - From;
    return From;
}

UINTN
CkpGetIndex (
    PCK_VM Vm,
    CK_VALUE Index,
    UINTN Count
    )

/*++

Routine Description:

    This routine returns an index value, handling invalid cases and converting
    negative values.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Index - Supplies the index value given. It will be validated that this is
        an integer.

    Count - Supplies the number of elements in the actual array, for
        validatation and negative purposes.

Return Value:

    Returns an index in the range of 0 to Count on success.

    MAX_UINTN on validation failure. A runtime error will the thrown.

--*/

{

    CK_INTEGER IndexValue;

    if (!CK_IS_INTEGER(Index)) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return MAX_UINTN;
    }

    IndexValue = CK_AS_INTEGER(Index);
    if (IndexValue < 0) {
        IndexValue += Count;
    }

    if ((IndexValue < 0) || (IndexValue >= Count) || (IndexValue > MAX_INTN)) {
        CkpRuntimeError(Vm,
                        "IndexError",
                        "Index %lld out of range",
                        (LONGLONG)IndexValue);

        return MAX_UINTN;
    }

    return IndexValue;
}

CK_SYMBOL_INDEX
CkpGetInitMethodSymbol (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_ARITY Arity
    )

/*++

Routine Description:

    This routine returns the string table index for the init method with the
    given arity.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Module - Supplies a pointer to the module the function is defined in.

    Arity - Supplies the number of arguments passed to this init function.

Return Value:

    Returns the index into the module string table for this function signature.

    -1 if no such string exists.

--*/

{

    UINTN Length;
    CHAR Name[CK_MAX_METHOD_SIGNATURE];

    Length = snprintf(Name, CK_MAX_METHOD_SIGNATURE - 1, "__init@%d", Arity);
    return CkpStringTableFind(&(Module->Strings), Name, Length);
}

VOID
CkpPrintSignature (
    PCK_FUNCTION_SIGNATURE Signature,
    PSTR Name,
    PUINTN Length
    )

/*++

Routine Description:

    This routine converts function signature information into a single string
    that contains the equivalent signature information.

Arguments:

    Signature - Supplies a pointer to the signature information.

    Name - Supplies a pointer to the function name.

    Length - Supplies a pointer that on input contains the size of the buffer.
        On output, this will contain the size of the signature string, not
        including the null terminator.

Return Value:

    None.

--*/

{

    UINTN CopySize;

    if (*Length == 0) {
        return;
    }

    CopySize = Signature->Length;
    if (*Length < CopySize) {
        CopySize = *Length - 1;
    }

    CkCopy(Name, Signature->Name, CopySize);
    *Length = snprintf(Name + CopySize,
                       *Length - CopySize,
                       "@%d",
                       (INT)(Signature->Arity));

    *Length += CopySize;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

