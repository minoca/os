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
CkpStringTableInitialize (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable
    );

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

VOID
CkpStringTableClear (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable
    );

/*++

Routine Description:

    This routine resets a string table to empty.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table to reset.

Return Value:

    None.

--*/

CK_SYMBOL_INDEX
CkpStringTableEnsure (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    PSTR Name,
    UINTN Size
    );

/*++

Routine Description:

    This routine returns the index of the given string table. If it did not
    exist before, it is added.

Arguments:

    Vm - Supplies a pointer to the VM.

    StringTable - Supplies a pointer to the string table.

    Name - Supplies a pointer to the name of the string to locate or add.

    Size - Supplies the size of the string, not including the null terminator.

Return Value:

    Returns the index of the new string on success.

    -1 on allocation failure.

--*/

CK_SYMBOL_INDEX
CkpStringTableEnsureValue (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    CK_VALUE String
    );

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

CK_SYMBOL_INDEX
CkpStringTableAdd (
    PCK_VM Vm,
    PCK_STRING_TABLE StringTable,
    PSTR Name,
    UINTN Size
    );

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

CK_SYMBOL_INDEX
CkpStringTableFind (
    PCK_STRING_TABLE StringTable,
    PSTR Name,
    UINTN Size
    );

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

