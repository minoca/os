/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bsearch.c

Abstract:

    This module implements support for the binary search function.

Author:

    Evan Green 20-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

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

LIBC_API
void *
bsearch (
    const void *Key,
    const void *Base,
    size_t ElementCount,
    size_t ElementSize,
    int (*CompareFunction)(const void *, const void *)
    )

/*++

Routine Description:

    This routine searches an array of sorted objects for one matching the given
    key.

Arguments:

    Key - Supplies a pointer to the element to match against in the given
        array.

    Base - Supplies a pointer to the base of the array to search.

    ElementCount - Supplies the number of elements in the array. Searching an
        element with a count of zero shall return NULL.

    ElementSize - Supplies the size of each element in the array.

    CompareFunction - Supplies a pointer to a function that will be called to
        compare elements. The function takes in two pointers that will point
        to elements within the array. It shall return less than zero if the
        left element is considered less than the right object, zero if the left
        object is considered equal to the right object, and greater than zero
        if the left object is considered greater than the right object.

Return Value:

    Returns a pointer to the element within the array matching the given key.

    NULL if no such element exists or the element count was zero.

--*/

{

    size_t CompareIndex;
    const void *ComparePointer;
    int CompareResult;
    size_t Distance;
    size_t Maximum;
    size_t Minimum;

    if (ElementCount == 0) {
        return NULL;
    }

    Minimum = 0;
    Maximum = ElementCount;

    //
    // Loop as long as the indices don't cross. The maximum index is exclusive
    // (so 0,1 includes only 0).
    //

    while (Minimum < Maximum) {
        Distance = (Maximum - Minimum) / 2;
        CompareIndex = Minimum + Distance;
        ComparePointer = Base + (CompareIndex * ElementSize);
        CompareResult = CompareFunction(Key, ComparePointer);
        if (CompareResult == 0) {
            return (void *)ComparePointer;

        } else if (CompareResult > 0) {
            Minimum = CompareIndex + 1;

        } else {
            Maximum = CompareIndex;
        }
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

