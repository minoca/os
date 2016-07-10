/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

#include "chalkp.h"

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
    PSTR Name,
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

    CK_STRING_OBJECT FakeObject;
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
    PSTR Name,
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
    PSTR Name,
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

    CK_STRING_OBJECT FakeObject;
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
    PVOID Data,
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

//
// --------------------------------------------------------- Internal Functions
//

