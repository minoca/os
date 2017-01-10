/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// This macro ensures there is a given amount of capacity in one of the
// CK_*_ARRAY types.
//

#define CkpSizeArray(_Vm, _Array, _Capacity) \
    CkpSizeGenericArray((_Vm), (_Array), sizeof(*((_Array)->Data)), (_Capacity))

//
// This macro appends a single ement to one of the CK_*_ARRAY types.
//

#define CkpArrayAppend(_Vm, _Array, _Element) \
    CkpFillArray((_Vm), (_Array), &(_Element), 1)

//
// This macro evaluates to the offset in bytes between two pointers
// (left - right).
//

#define CK_POINTER_DIFFERENCE(_Left, _Right) \
    ((UINTN)(_Left) - (UINTN)(_Right))

//
// This macro evaluates to a pointer adjusted by the given number of bytes.
//

#define CK_POINTER_ADD(_Pointer, _Count) \
    (PVOID)((UINTN)(_Pointer) + (_Count))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a function signature.

Members:

    Name - Stores a pointer to the name of the function.

    Length - Stores the length of the function name, not including the null
        terminator.

    Arity - Stores the number of arguments the function takes.

--*/

typedef struct _CK_FUNCTION_SIGNATURE {
    PCSTR Name;
    UINTN Length;
    CK_ARITY Arity;
} CK_FUNCTION_SIGNATURE, *PCK_FUNCTION_SIGNATURE;

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
    PCSTR Name,
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
    PCSTR Name,
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
    PCSTR Name,
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
    PCVOID Data,
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

CK_ERROR_TYPE
CkpSizeGenericArray (
    PCK_VM Vm,
    PVOID Array,
    UINTN ElementSize,
    UINTN Capacity
    );

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

UINTN
CkpGetRange (
    PCK_VM Vm,
    PCK_RANGE Range,
    PUINTN Count
    );

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

UINTN
CkpGetIndex (
    PCK_VM Vm,
    CK_VALUE Index,
    UINTN Count
    );

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

CK_SYMBOL_INDEX
CkpGetInitMethodSymbol (
    PCK_VM Vm,
    PCK_MODULE Module,
    CK_ARITY Arity
    );

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

VOID
CkpPrintSignature (
    PCK_FUNCTION_SIGNATURE Signature,
    PSTR Name,
    PUINTN Length
    );

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

