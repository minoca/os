/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    list.c

Abstract:

    This module implements the Chalk List class.

Author:

    Evan Green 16-Jul-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the capacity under which the list shrinks.
//

#define LIST_SHRINK_FACTOR 4

//
// Define the size under which lists aren't resized.
//

#define LIST_MIN_CAPACITY 64

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
CkpListAppend (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListClearPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListLength (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListInsertPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListRemoveIndexPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListContains (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListSlice (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

BOOL
CkpListSliceAssign (
    PCK_VM Vm,
    PCK_VALUE Arguments
    );

//
// -------------------------------------------------------------------- Globals
//

CK_PRIMITIVE_DESCRIPTION CkListPrimitives[] = {
    {"append@1", 1, CkpListAppend},
    {"__add@1", 1, CkpListAdd},
    {"clear@0", 0, CkpListClearPrimitive},
    {"length@0", 0, CkpListLength},
    {"insert@2", 2, CkpListInsertPrimitive},
    {"removeAt@1", 1, CkpListRemoveIndexPrimitive},
    {"contains@1", 1, CkpListContains},
    {"iterate@1", 1, CkpListIterate},
    {"iteratorValue@1", 1, CkpListIteratorValue},
    {"__slice@1", 1, CkpListSlice},
    {"__sliceAssign@2", 2, CkpListSliceAssign},
    {NULL, 0, NULL}
};

//
// ------------------------------------------------------------------ Functions
//

PCK_LIST
CkpListCreate (
    PCK_VM Vm,
    UINTN ElementCount
    )

/*++

Routine Description:

    This routine creates a new list object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ElementCount - Supplies the initial capacity and count to allocate. The
        caller is expected to fill these in with values, as the count is set
        to this number so the elements are live.

Return Value:

    Returns a pointer to the created list on success.

    NULL on allocation failure.

--*/

{

    PCK_VALUE Array;
    PCK_LIST List;

    Array = NULL;
    if (ElementCount != 0) {
        Array = CkAllocate(Vm, sizeof(CK_VALUE) * ElementCount);
        if (Array == NULL) {
            return NULL;
        }

        CkZero(Array, sizeof(CK_VALUE) * ElementCount);
    }

    List = CkAllocate(Vm, sizeof(CK_LIST));
    if (List == NULL) {
        if (Array != NULL) {
            CkFree(Vm, Array);
            return NULL;
        }
    }

    CkpInitializeObject(Vm, &(List->Header), CkObjectList, Vm->Class.List);
    List->Elements.Data = Array;
    List->Elements.Count = ElementCount;
    List->Elements.Capacity = ElementCount;
    return List;
}

VOID
CkpListDestroy (
    PCK_VM Vm,
    PCK_LIST List
    )

/*++

Routine Description:

    This routine destroys a list object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to destroy.

Return Value:

    None.

--*/

{

    CkpClearArray(Vm, &(List->Elements));
    return;
}

VOID
CkpListInsert (
    PCK_VM Vm,
    PCK_LIST List,
    CK_VALUE Element,
    UINTN Index
    )

/*++

Routine Description:

    This routine inserts an element into the list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to insert into.

    Element - Supplies the element to insert.

    Index - Supplies the index into the list to insert. Valid values are 0 to
        the current size of the list, inclusive.

Return Value:

    None.

--*/

{

    UINTN MoveIndex;
    CK_VALUE Value;

    CK_ASSERT(Index <= List->Elements.Count);

    if (CK_IS_OBJECT(Element)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Element));
    }

    Value = CkNullValue;
    CkpArrayAppend(Vm, &(List->Elements), Value);
    if (CK_IS_OBJECT(Element)) {
        CkpPopRoot(Vm);
    }

    if (Index >= List->Elements.Count) {
        return;
    }

    //
    // Move the remaining elements out of the way.
    //

    for (MoveIndex = List->Elements.Count - 1;
         MoveIndex > Index;
         MoveIndex -= 1) {

        List->Elements.Data[MoveIndex] = List->Elements.Data[MoveIndex - 1];
    }

    List->Elements.Data[Index] = Element;
    return;
}

CK_VALUE
CkpListRemoveIndex (
    PCK_VM Vm,
    PCK_LIST List,
    UINTN Index
    )

/*++

Routine Description:

    This routine removes the element at the given index.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to operate on.

    Index - Supplies the index into the list to remove. The elements after this
        one will be shifted down. Valid values are between 0 and the number of
        elements in the list, exclusive.

Return Value:

    Returns the element at the index that was removed.

--*/

{

    CK_VALUE Element;
    UINTN MoveIndex;
    PVOID NewBuffer;
    UINTN NewCapacity;

    CK_ASSERT(Index < List->Elements.Count);

    Element = List->Elements.Data[Index];
    if (CK_IS_OBJECT(Element)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Element));
    }

    //
    // Move the items afterwards down.
    //

    for (MoveIndex = Index;
         MoveIndex < List->Elements.Count - 1;
         MoveIndex += 1) {

        List->Elements.Data[MoveIndex] = List->Elements.Data[MoveIndex + 1];
    }

    //
    // Potentially shrink the list if it's gotten too small.
    //

    if ((List->Elements.Count > LIST_MIN_CAPACITY) &&
        (List->Elements.Count <
         (List->Elements.Capacity / LIST_SHRINK_FACTOR))) {

        NewCapacity = List->Elements.Capacity / LIST_SHRINK_FACTOR;
        NewBuffer = CkpReallocate(Vm,
                                  List->Elements.Data,
                                  List->Elements.Capacity * sizeof(CK_VALUE),
                                  NewCapacity * sizeof(CK_VALUE));

        if (NewBuffer != NULL) {
            List->Elements.Data = NewBuffer;
            List->Elements.Capacity = NewCapacity;
        }
    }

    if (CK_IS_OBJECT(Element)) {
        CkpPopRoot(Vm);
    }

    List->Elements.Count -= 1;
    return Element;
}

PCK_LIST
CkpListConcatenate (
    PCK_VM Vm,
    PCK_LIST Destination,
    PCK_LIST Source
    )

/*++

Routine Description:

    This routine concatenates two lists together. It can alternatively be used
    to copy a list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Destination - Supplies an optional pointer to the list to operate on. If
        this is NULL, then a new list is created.

    Source - Supplies the list to append to the end of the destination list.

Return Value:

    Returns the destination list on success. If no destination list was
    provided, returns a pointer to a new list.

    NULL on allocation failure.

--*/

{

    if (Destination == NULL) {
        Destination = CkpListCreate(Vm, Source->Elements.Count);
        if (Destination == NULL) {
            return NULL;
        }

        CkCopy(Destination->Elements.Data,
               Source->Elements.Data,
               Source->Elements.Count * sizeof(CK_VALUE));

        return Destination;
    }

    CkpFillArray(Vm,
                 &(Destination->Elements),
                 Source->Elements.Data,
                 Source->Elements.Count);

    return Destination;
}

VOID
CkpListClear (
    PCK_VM Vm,
    PCK_LIST List
    )

/*++

Routine Description:

    This routine resets a list to be empty.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    List - Supplies a pointer to the list to clear.

Return Value:

    None.

--*/

{

    List->Elements.Count = 0;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

//
// Primitive functions that implement List class methods
//

BOOL
CkpListAppend (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine adds the given element onto end of the given list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    CkpListInsert(Vm, List, Arguments[1], List->Elements.Count);
    return TRUE;
}

BOOL
CkpListAdd (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine adds two lists together.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_LIST Left;
    PCK_LIST Result;
    PCK_LIST Right;

    Left = CK_AS_LIST(Arguments[0]);
    if (!CK_IS_LIST(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected a list");
        return FALSE;
    }

    Right = CK_AS_LIST(Arguments[1]);
    Result = CkpListConcatenate(Vm, NULL, Left);
    if (Result == NULL) {
        return FALSE;
    }

    //
    // Move the result to the stack to avoid it getting released. The left
    // list is done with anyway.
    //

    CK_OBJECT_VALUE(Arguments[0], Result);
    if (CkpListConcatenate(Vm, Result, Right) == NULL) {
        return FALSE;
    }

    return TRUE;
}

BOOL
CkpListClearPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the clear primitive function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    CkpListClear(Vm, List);
    return TRUE;
}

BOOL
CkpListLength (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine adds two lists together.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    CK_INT_VALUE(Arguments[0], List->Elements.Count);
    return TRUE;
}

BOOL
CkpListInsertPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the insert list primitive method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);

    //
    // Allow "count" as a valid index to enable inserting at the end.
    //

    Index = CkpGetIndex(Vm, Arguments[1], List->Elements.Count + 1);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    CkpListInsert(Vm, List, Arguments[2], Index);
    return TRUE;
}

BOOL
CkpListRemoveIndexPrimitive (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine implements the remove at index list primitive method.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    Index = CkpGetIndex(Vm, Arguments[1], List->Elements.Count);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    CkpListRemoveIndex(Vm, List, Index);
    return TRUE;
}

BOOL
CkpListContains (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine determines if the given list contains the given value.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    for (Index = 0; Index < List->Elements.Count; Index += 1) {
        if (CkpAreValuesEqual(List->Elements.Data[Index], Arguments[1]) !=
            FALSE) {

            Arguments[0] = CkOneValue;
            return TRUE;
        }
    }

    Arguments[0] = CkZeroValue;
    return TRUE;
}

BOOL
CkpListIterate (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine initializes or advances a list iterator.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    CK_INTEGER Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);

    //
    // Initialize a new iterator.
    //

    if (CK_IS_NULL(Arguments[1])) {
        if (List->Elements.Count == 0) {
            Arguments[0] = CkNullValue;
            return TRUE;
        }

        Arguments[0] = CkZeroValue;
        return TRUE;
    }

    if (!CK_IS_INTEGER(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer");
        return FALSE;
    }

    Index = CK_AS_INTEGER(Arguments[1]);
    if ((Index < 0) || (Index >= List->Elements.Count - 1)) {
        Arguments[0] = CkNullValue;
        return TRUE;
    }

    CK_INT_VALUE(Arguments[0], Index + 1);
    return TRUE;
}

BOOL
CkpListIteratorValue (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine returns the value for the given iterator context.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);
    Index = CkpGetIndex(Vm, Arguments[1], List->Elements.Count);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    Arguments[0] = List->Elements.Data[Index];
    return TRUE;
}

BOOL
CkpListSlice (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine gets a subset of the given list.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN CopyIndex;
    UINTN Count;
    UINTN Index;
    PCK_LIST List;
    PCK_RANGE Range;
    PCK_LIST Result;
    UINTN Start;

    List = CK_AS_LIST(Arguments[0]);

    //
    // Get at a particular single index.
    //

    if (CK_IS_INTEGER(Arguments[1])) {
        Index = CkpGetIndex(Vm, Arguments[1], List->Elements.Count);
        if (Index == MAX_UINTN) {
            return FALSE;
        }

        Arguments[0] = List->Elements.Data[Index];
        return TRUE;
    }

    //
    // Create a sublist with the given range.
    //

    if (!CK_IS_RANGE(Arguments[1])) {
        CkpRuntimeError(Vm, "TypeError", "Expected an integer or range");
        return FALSE;
    }

    Range = CK_AS_RANGE(Arguments[1]);
    Count = List->Elements.Count;
    Start = CkpGetRange(Vm, Range, &Count);
    if (Start == MAX_UINTN) {
        return FALSE;
    }

    Result = CkpListCreate(Vm, Count);
    if (Result == NULL) {
        return FALSE;
    }

    //
    // Copy the portion of the original list into the new list, potentially
    // reversing if the caller asked for it that way.
    //

    for (CopyIndex = 0; CopyIndex < Count; CopyIndex += 1) {
        Result->Elements.Data[CopyIndex] =
                                        List->Elements.Data[Start + CopyIndex];
    }

    CK_OBJECT_VALUE(Arguments[0], Result);
    return TRUE;
}

BOOL
CkpListSliceAssign (
    PCK_VM Vm,
    PCK_VALUE Arguments
    )

/*++

Routine Description:

    This routine sets a member of the given list. Currently only integers are
    supported.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Arguments - Supplies the function arguments.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    UINTN Index;
    PCK_LIST List;

    List = CK_AS_LIST(Arguments[0]);

    //
    // Currently only integers are supported. Consider supporting assigning
    // list ranges if needed (but what does it mean to assign a list with a
    // negative step direction).
    //

    Index = CkpGetIndex(Vm, Arguments[1], List->Elements.Count);
    if (Index == MAX_UINTN) {
        return FALSE;
    }

    List->Elements.Data[Index] = Arguments[2];
    Arguments[0] = Arguments[2];
    return TRUE;
}

