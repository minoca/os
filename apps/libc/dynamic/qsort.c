/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    qsort.c

Abstract:

    This module implements the QuickSort standard C library function.

Author:

    Evan Green 26-Jun-2013

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
// --------------------------------------------------------------------- Macros
//

//
// This macro is a helper to the quicksort swap macro. It gets a pointer to a
// void pointer at the given index.
//

#define QUICKSORT_ELEMENT(_Base, _ElementSize, _Index) \
    (*((void **)((_Base) + ((_ElementSize) * (_Index)))))

//
// This macro either performs an exchange directly for pointer sized elements,
// or calls the bytewise swap function to exchange byte for byte. It assumes
// the presence of a void pointer local called SwapPointer.
//

#define QUICKSORT_SWAP(_Base, _ElementSize, _FirstIndex, _SecondIndex)         \
    {                                                                          \
        if ((_ElementSize) == sizeof(void *)) {                                \
            SwapPointer = QUICKSORT_ELEMENT(_Base, _ElementSize, _FirstIndex); \
            QUICKSORT_ELEMENT(_Base, _ElementSize, _FirstIndex) =              \
                        QUICKSORT_ELEMENT(_Base, _ElementSize, _SecondIndex);  \
                                                                               \
            QUICKSORT_ELEMENT(_Base, _ElementSize, _SecondIndex) =             \
                                                                  SwapPointer; \
        } else {                                                               \
            ClpQuickSortSwap((_Base),                                          \
                             (_ElementSize),                                   \
                             (_FirstIndex),                                    \
                             (_SecondIndex));                                  \
        }                                                                      \
    }

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
ClpQuickSort (
    void *ArrayBase,
    size_t ElementSize,
    ssize_t StartIndex,
    ssize_t EndIndex,
    int (*CompareFunction)(const void *, const void *)
    );

VOID
ClpQuickSortSwap (
    void *ArrayBase,
    size_t ElementSize,
    ssize_t FirstIndex,
    ssize_t SecondIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
void
qsort (
    void *ArrayBase,
    size_t ElementCount,
    size_t ElementSize,
    int (*CompareFunction)(const void *, const void *)
    )

/*++

Routine Description:

    This routine sorts an array of items in place using the QuickSort algorithm.

Arguments:

    ArrayBase - Supplies a pointer to the array of items that will get pushed
        around.

    ElementCount - Supplies the number of elements in the array.

    ElementSize - Supplies the size of one of the elements.

    CompareFunction - Supplies a pointer to a function that will be used to
        compare elements. The function takes in two pointers that will point
        within the array. It returns less than zero if the first element is
        less than the second, zero if the first element is equal to the second,
        and greater than zero if the first element is greater than the second.
        The routine must not modify the array itself or inconsistently
        report comparisons, otherwise the sorting will not come out correctly.

Return Value:

    None.

--*/

{

    assert(ElementCount < (((size_t)-1) >> 1));
    assert(ElementSize < (((size_t)-1) >> 1));

    if (ElementCount > 1) {
        ClpQuickSort(ArrayBase,
                     ElementSize,
                     0,
                     ElementCount - 1,
                     CompareFunction);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpQuickSort (
    void *ArrayBase,
    size_t ElementSize,
    ssize_t StartIndex,
    ssize_t EndIndex,
    int (*CompareFunction)(const void *, const void *)
    )

/*++

Routine Description:

    This routine sorts an array of items in place using the QuickSort algorithm.

Arguments:

    ArrayBase - Supplies a pointer to the array of items that will get pushed
        around.

    ElementSize - Supplies the size of one of the elements.

    StartIndex - Supplies the starting index of the array to sort, inclusive.

    EndIndex - Supplies the ending index of the array to sort, inclusive.

    CompareFunction - Supplies a pointer to a function that will be used to
        compare elements. The function takes in two pointers that will point
        within the array. It returns less than zero if the first element is
        less than the second, zero if the first element is equal to the second,
        and greater than zero if the first element is greater than the second.
        The routine must not modify the array itself or inconsistently
        report comparisons, otherwise the sorting will not come out correctly.

Return Value:

    None.

--*/

{

    int CompareResult;
    ssize_t EqualIndex;
    ssize_t LargerIndex;
    ssize_t LargestIndex;
    void *LastElement;
    ssize_t SmallerIndex;
    ssize_t SmallestIndex;
    void *SwapPointer;

    if (EndIndex <= StartIndex) {
        return;
    }

    LastElement = ArrayBase + (EndIndex * ElementSize);
    SmallerIndex = StartIndex - 1;
    SmallestIndex = StartIndex - 1;
    LargerIndex = EndIndex;
    LargestIndex = EndIndex;

    //
    // Loop swapping elements that are on the wrong side of the pivot (which is
    // selected to be the last element). The loop stops when the indices cross.
    //

    while (TRUE) {

        //
        // Scan along as long as the current value is less than the last value.
        //

        while (TRUE) {
            SmallerIndex += 1;
            if (SmallerIndex == EndIndex) {
                break;
            }

            CompareResult =
                      CompareFunction(ArrayBase + (SmallerIndex * ElementSize),
                                      LastElement);

            if (CompareResult >= 0) {
                break;
            }
        }

        //
        // Scan down as long as the current value is greater than the last
        // value.
        //

        while (TRUE) {
            LargerIndex -= 1;
            CompareResult =
                      CompareFunction(LastElement,
                                      ArrayBase + (LargerIndex * ElementSize));

            if (CompareResult >= 0) {
                break;
            }

            if (LargerIndex == StartIndex) {
                break;
            }
        }

        //
        // Stop if the paths crossed, as that means everything's on the correct
        // side of the pivot if the pivot were here.
        //

        if (SmallerIndex >= LargerIndex) {
            break;
        }

        //
        // Exchange the two, as they're both on the wrong side of the pivot.
        //

        if (SmallerIndex != LargerIndex) {
            QUICKSORT_SWAP(ArrayBase, ElementSize, SmallerIndex, LargerIndex);
        }

        //
        // Move keys equal to the partitioning element over to the ends of the
        // arrays. This is the beginning of what's called Bentley-McIlroy 3-way
        // partitioning, and it helps to handle an array with lots of
        // elements equal to the pivot. So the array is going to look like this:
        // | equal | less |  xxx (unsorted)  | greater | equal |
        // At the end, swap the equals to the center so that it looks like:
        // | less | equal | greater |
        // This has the benefit that it always uses N - 1 three way compares,
        // there's no extra overhead if there are no equal keys, and only one
        // extra exchange per equal key.
        //

        CompareResult =
                      CompareFunction(ArrayBase + (SmallerIndex * ElementSize),
                                      LastElement);

        if (CompareResult == 0) {
            SmallestIndex += 1;
            QUICKSORT_SWAP(ArrayBase, ElementSize, SmallestIndex, SmallerIndex);
        }

        CompareResult =
                      CompareFunction(LastElement,
                                      ArrayBase + (LargerIndex * ElementSize));

        if (CompareResult == 0) {
            LargestIndex -= 1;
            QUICKSORT_SWAP(ArrayBase, ElementSize, LargerIndex, LargestIndex);
        }
    }

    //
    // Put the pivot into place.
    //

    QUICKSORT_SWAP(ArrayBase, ElementSize, SmallerIndex, EndIndex);

    //
    // Move lower equal elements back up to the middle, and remove them from
    // the partition of things that need to be resorted.
    //

    LargerIndex = SmallerIndex + 1;
    SmallerIndex -= 1;
    for (EqualIndex = StartIndex; EqualIndex < SmallestIndex; EqualIndex += 1) {
        QUICKSORT_SWAP(ArrayBase,
                       ElementSize,
                       EqualIndex,
                       SmallerIndex);

        SmallerIndex -= 1;
    }

    //
    // Move upper equal elements back down to the middle, and remove them from
    // the partition of things that need to be resorted.
    //

    for (EqualIndex = EndIndex - 1;
         EqualIndex > LargestIndex;
         EqualIndex -= 1) {

        QUICKSORT_SWAP(ArrayBase,
                       ElementSize,
                       LargerIndex,
                       EqualIndex);

        LargerIndex += 1;
    }

    //
    // Sort the bottom half and then the top half.
    //

    if (SmallerIndex > StartIndex) {
        ClpQuickSort(ArrayBase,
                     ElementSize,
                     StartIndex,
                     SmallerIndex,
                     CompareFunction);
    }

    if (EndIndex > LargerIndex) {
        ClpQuickSort(ArrayBase,
                     ElementSize,
                     LargerIndex,
                     EndIndex,
                     CompareFunction);
    }

    return;
}

VOID
ClpQuickSortSwap (
    void *ArrayBase,
    size_t ElementSize,
    ssize_t FirstIndex,
    ssize_t SecondIndex
    )

/*++

Routine Description:

    This routine swaps two elements in the given array.

Arguments:

    ArrayBase - Supplies a pointer to the array of items that will get pushed
        around.

    ElementSize - Supplies the size of one of the elements.

    FirstIndex - Supplies the first index of the indices to exchange.

    SecondIndex - Supplies the second index of the indices to exchange.

Return Value:

    None.

--*/

{

    ssize_t ByteIndex;
    unsigned char *FirstElement;
    unsigned char *SecondElement;
    unsigned char Swap;

    FirstElement = (unsigned char *)ArrayBase + (ElementSize * FirstIndex);
    SecondElement = (unsigned char *)ArrayBase + (ElementSize * SecondIndex);
    for (ByteIndex = 0; ByteIndex < ElementSize; ByteIndex += 1) {
        Swap = FirstElement[ByteIndex];
        FirstElement[ByteIndex] = SecondElement[ByteIndex];
        SecondElement[ByteIndex] = Swap;
    }

    return;
}

