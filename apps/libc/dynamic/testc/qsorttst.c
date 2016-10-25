/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    qsorttst.c

Abstract:

    This module tests the qsort function a bit.

Author:

    Evan Green 11-Jul-2013

Environment:

    Test

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define this so it doesn't get defined to an import.
//

#define LIBC_API

#include <minoca/lib/types.h>

#include <stdio.h>
#include <stdlib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_QUICKSORT_ARRAY_COUNT 1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestQuickSortCase (
    ULONG TestIndex,
    PULONG Array,
    ULONG ArrayCount,
    BOOL ExactSet
    );

int
TestQuickSortCompare (
    const void *Left,
    const void *Right
    );

//
// -------------------------------------------------------------------- Globals
//

ULONG TestQuickSortArray[TEST_QUICKSORT_ARRAY_COUNT];

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestQuickSort (
    VOID
    )

/*++

Routine Description:

    This routine implements the entry point for the quicksort test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    PULONG Array;
    ULONG Case;
    ULONG Failures;
    ULONG Index;

    Array = TestQuickSortArray;
    Case = 0;
    Failures = 0;

    //
    // Try something small and simple.
    //

    Array[0] = 0;
    Array[1] = 1;
    Array[2] = 2;
    Array[3] = 4;
    Array[4] = 3;
    Failures += TestQuickSortCase(Case, Array, 5, TRUE);
    Case += 1;

    //
    // Try something else small and exactly out of order.
    //

    for (Index = 0; Index < 6; Index += 1) {
        Array[Index] = 6 - Index - 1;
    }

    Failures += TestQuickSortCase(Case, Array, 6, TRUE);
    Case += 1;

    //
    // Try something small that's all the same.
    //

    for (Index = 0; Index < 6; Index += 1) {
        Array[Index] = 0;
    }

    Failures += TestQuickSortCase(Case, Array, 6, FALSE);
    Case += 1;

    //
    // Try something small with lots of duplicates.
    //

    for (Index = 0; Index < 12; Index += 1) {
        Array[Index] = (12 - Index - 1) / 4;
    }

    Failures += TestQuickSortCase(Case, Array, 6, FALSE);
    Case += 1;

    //
    // Put everything exactly out of order.
    //

    for (Index = 0; Index < TEST_QUICKSORT_ARRAY_COUNT; Index += 1) {
        Array[Index] = TEST_QUICKSORT_ARRAY_COUNT - Index - 1;
    }

    Failures += TestQuickSortCase(Case,
                                  Array,
                                  TEST_QUICKSORT_ARRAY_COUNT,
                                  TRUE);

    Case += 1;

    //
    // Put everything in order.
    //

    for (Index = 0; Index < TEST_QUICKSORT_ARRAY_COUNT; Index += 1) {
        Array[Index] = Index;
    }

    Failures += TestQuickSortCase(Case,
                                  Array,
                                  TEST_QUICKSORT_ARRAY_COUNT,
                                  TRUE);

    Case += 1;

    //
    // Fill it with random numbers that are very likely to repeat.
    //

    for (Index = 0; Index < TEST_QUICKSORT_ARRAY_COUNT; Index += 1) {
        Array[Index] = rand() % (TEST_QUICKSORT_ARRAY_COUNT / 4);
    }

    Failures += TestQuickSortCase(Case,
                                  Array,
                                  TEST_QUICKSORT_ARRAY_COUNT,
                                  FALSE);

    Case += 1;

    //
    // Fill it with random numbers that are likely to repeat a few times.
    //

    for (Index = 0; Index < TEST_QUICKSORT_ARRAY_COUNT; Index += 1) {
        Array[Index] = rand() % TEST_QUICKSORT_ARRAY_COUNT;
    }

    Failures += TestQuickSortCase(Case,
                                  Array,
                                  TEST_QUICKSORT_ARRAY_COUNT,
                                  FALSE);

    Case += 1;

    //
    // Fill it with random numbers that probably won't repeat.
    //

    for (Index = 0; Index < TEST_QUICKSORT_ARRAY_COUNT; Index += 1) {
        Array[Index] = rand();
    }

    Failures += TestQuickSortCase(Case,
                                  Array,
                                  TEST_QUICKSORT_ARRAY_COUNT,
                                  FALSE);

    Case += 1;
    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestQuickSortCase (
    ULONG TestIndex,
    PULONG Array,
    ULONG ArrayCount,
    BOOL ExactSet
    )

/*++

Routine Description:

    This routine runs quicksort on the given array and validates the results.

Arguments:

    TestIndex - Supplies the test case number for error printing.

    Array - Supplies a pointer to the array of integers.

    ArrayCount - Supplies the size of the array in elements.

    ExactSet - Supplies a boolean if the array contains exactly the integers
        0 through count - 1.

Return Value:

    Returns the number of failures (zero or one).

--*/

{

    ULONG Index;
    BOOL PrintedYet;
    ULONG WrongCount;

    PrintedYet = FALSE;
    WrongCount = 0;
    qsort(Array, ArrayCount, sizeof(ULONG), TestQuickSortCompare);
    for (Index = 0; Index < ArrayCount; Index += 1) {
        if (ExactSet != FALSE) {
            if (Array[Index] != Index) {
                if (PrintedYet == FALSE) {
                    printf("Error: Test case %d failed.\n", TestIndex);
                    PrintedYet = TRUE;
                }

                printf("Error: Index %4d had %4d in it.\n",
                       Index,
                       Array[Index]);

                WrongCount += 1;
            }

        } else {
            if ((Index != 0) && (Array[Index] < Array[Index - 1])) {
                if (PrintedYet == FALSE) {
                    printf("Error: Test case %d failed.\n", TestIndex);
                    PrintedYet = TRUE;
                }

                printf("Error: Index %4d had %4d in it, but previous value "
                       "was %d.\n",
                       Index,
                       Array[Index],
                       Array[Index - 1]);

                WrongCount += 1;
            }
        }
    }

    if (WrongCount != 0) {
        printf("%d values out of order.\n", WrongCount);
        return 1;
    }

    return 0;
}

int
TestQuickSortCompare (
    const void *Left,
    const void *Right
    )

/*++

Routine Description:

    This routine compares two test array elements. It is used by the quicksort
    function.

Arguments:

    Left - Supplies a pointer into the array of the left side of the comparison.

    Right - Supplies a pointer into the array of the right side of the
        comparison.

Return Value:

    <0 if the left is less than the right.

    0 if the two elements are equal.

    >0 if the left element is greater than the right.

--*/

{

    ULONG LeftNumber;
    ULONG RightNumber;

    LeftNumber = *((PULONG)Left);
    RightNumber = *((PULONG)Right);
    if (LeftNumber < RightNumber) {
        return -1;
    }

    if (LeftNumber > RightNumber) {
        return 1;
    }

    return 0;
}

