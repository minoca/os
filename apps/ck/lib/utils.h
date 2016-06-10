/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    utils.h

Abstract:

    This header contains definitions for general Chalk core utility functions.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro initializes one of the CK_*_ARRAY types. All of the array
// structures are the same size.
//

#define CkpInitializeArray(_Array) CkZero((_Array), sizeof(CK_INT_ARRAY))

//
// This macro resets one of the CK_*_ARRAY types.
//

#define CkpClearArray(_Vm, _Array)      \
    {                                   \
        CkFree((_Vm), (_Array)->Data);  \
        CkpInitializeArray(_Array);     \
    }

//
// This macro sets multiple elements of one of the CK_*_ARRAY types.
//

#define CkpFillArray(_Vm, _Array, _Data, _Count)    \
    CkpFillGenericArray((_Vm),                      \
                        (_Array),                   \
                        (_Data),                    \
                        sizeof(*((_Array)->Data)),  \
                        (_Count))

//
// This macro appends a single ement to one of the CK_*_ARRAY types.
//

#define CkpArrayAppend(_Vm, _Array, _Element) \
    CkpFillArray((_Vm), (_Array), &(_Element), 1)

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

CK_ERROR_TYPE
CkpSymbolTableInitialize (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable
    );

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

VOID
CkpSymbolTableClear (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable
    );

/*++

Routine Description:

    This routine resets a symbol table to empty.

Arguments:

    Vm - Supplies a pointer to the VM.

    SymbolTable - Supplies a pointer to the symbol table to reset.

Return Value:

    None.

--*/

CK_SYMBOL_INDEX
CkpSymbolTableEnsure (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    );

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

CK_SYMBOL_INDEX
CkpSymbolTableAdd (
    PCK_VM Vm,
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    );

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

CK_SYMBOL_INDEX
CkpSymbolTableFind (
    PCK_SYMBOL_TABLE SymbolTable,
    PSTR Name,
    UINTN Size
    );

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

CK_ERROR_TYPE
CkpFillGenericArray (
    PCK_VM Vm,
    PVOID Array,
    PVOID Data,
    UINTN ElementSize,
    UINTN Count
    );

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

