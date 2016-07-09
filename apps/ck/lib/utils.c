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
CkpSymbolTableInitialize (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable
    )

/*++

Routine Description:

    This routine initializes a symbol table.

Arguments:

    Vm - Supplies a pointer to the VM.

    SymbolTable - Supplies a pointer to the symbol table to initialize.

Return Value:

    CkSuccess on success.

    CkErrorNoMemory on allocation failure.

--*/

{

    CkpInitializeArray(SymbolTable);
    return CkSuccess;
}

VOID
CkpSymbolTableClear (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable
    )

/*++

Routine Description:

    This routine resets a symbol table to empty.

Arguments:

    Vm - Supplies a pointer to the VM.

    SymbolTable - Supplies a pointer to the symbol table to reset.

Return Value:

    None.

--*/

{

    LONG Index;

    for (Index = 0; Index < SymbolTable->Count; Index += 1) {
        CkFree(Vm, SymbolTable->Data[Index].Data);
    }

    CkpClearArray(Vm, SymbolTable);
    return;
}

CK_SYMBOL_INDEX
CkpSymbolTableEnsure (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the index of the given symbol table. If it did not
    exist before, it is added.

Arguments:

    Vm - Supplies a pointer to the VM.

    SymbolTable - Supplies a pointer to the symbol table.

    Name - Supplies a pointer to the name of the symbol to locate or add.

    Size - Supplies the size of the symbol, not including the null terminator.

Return Value:

    Returns the symbol index of the new symbol on success.

    -1 on allocation failure.

--*/

{

    CK_SYMBOL_INDEX Existing;

    Existing = CkpSymbolTableFind(SymbolTable, Name, Size);
    if (Existing != -1) {
        return Existing;
    }

    return CkpSymbolTableAdd(Vm, SymbolTable, Name, Size);
}

CK_SYMBOL_INDEX
CkpSymbolTableAdd (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine unconditionally adds a symbol to the given symbol table.

Arguments:

    Vm - Supplies a pointer to the VM.

    SymbolTable - Supplies a pointer to the symbol table.

    Name - Supplies a pointer to the name of the symbol to add.

    Size - Supplies the size of the symbol, not including the null terminator.

Return Value:

    Returns the symbol index of the new symbol on success.

    -1 on allocation failure.

--*/

{

    CK_ERROR_TYPE Status;
    CK_RAW_STRING String;

    String.Data = CkAllocate(Vm, Size + 1);
    if (String.Data == NULL) {
        return -1;
    }

    CkCopy(String.Data, Name, Size);
    String.Data[Size] = '\0';
    String.Size = Size;
    Status = CkpArrayAppend(Vm, SymbolTable, String);
    if (Status != CkSuccess) {
        return -1;
    }

    return SymbolTable->Count - 1;
}

CK_SYMBOL_INDEX
CkpSymbolTableFind (
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns the index of the given symbol table.

Arguments:

    SymbolTable - Supplies a pointer to the symbol table.

    Name - Supplies a pointer to the name of the symbol to find.

    Size - Supplies the size of the symbol, not including the null terminator.

Return Value:

    Returns the symbol index of the new symbol on success.

    -1 if the symbol does not exist in the table.

--*/

{

    CK_SYMBOL_INDEX Index;
    PCK_RAW_STRING String;

    String = SymbolTable->Data;
    for (Index = 0; Index < SymbolTable->Count; Index += 1) {
        if ((String->Size == Size) &&
            (CkCompareMemory(String->Data, Name, Size) == 0)) {

            return Index;
        }

        String += 1;
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

