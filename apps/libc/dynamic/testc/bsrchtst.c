/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bsrchtst.c

Abstract:

    This module tests the binary search function in the C library.

Author:

    Evan Green 20-Aug-2013

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

#define TEST_BINARY_SEARCH_ARRAY_COUNT 1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

ULONG
TestBinarySearchCase (
    ULONG ArraySize,
    LONG DesiredIndex
    );

int
TestBinarySearchCompare (
    const void *LeftPointer,
    const void *RightPointer
    );

//
// -------------------------------------------------------------------- Globals
//

LONG TestBinarySearchArray[TEST_BINARY_SEARCH_ARRAY_COUNT];

//
// ------------------------------------------------------------------ Functions
//

ULONG
TestBinarySearch (
    VOID
    )

/*++

Routine Description:

    This routine implements the entry point for the binary search test.

Arguments:

    None.

Return Value:

    Returns the count of test failures.

--*/

{

    ULONG Failures;
    LONG Index;
    ULONG Size;

    //
    // Initialize the sorted array.
    //

    for (Index = 0; Index < TEST_BINARY_SEARCH_ARRAY_COUNT; Index += 1) {
        TestBinarySearchArray[Index] = Index;
    }

    //
    // Perform the tests. Start with every possibility between 0 and 10.
    //

    Failures = 0;
    for (Size = 0; Size < 10; Size += 1) {
        for (Index = 0; Index <= Size; Index += 1) {
            Failures += TestBinarySearchCase(Size, Index);
        }
    }

    //
    // Test some slightly bigger ones.
    //

    Failures += TestBinarySearchCase(50, 25);
    Failures += TestBinarySearchCase(50, 48);
    Failures += TestBinarySearchCase(50, 49);
    Failures += TestBinarySearchCase(50, 0);
    Failures += TestBinarySearchCase(50, 1);
    Failures += TestBinarySearchCase(50, 3);
    Failures += TestBinarySearchCase(50, 12);
    Failures += TestBinarySearchCase(50, -1);
    Failures += TestBinarySearchCase(50, 51);
    Failures += TestBinarySearchCase(500, 25);
    Failures += TestBinarySearchCase(500, 250);
    Failures += TestBinarySearchCase(500, 1);
    Failures += TestBinarySearchCase(500, 2);
    Failures += TestBinarySearchCase(500, -1);
    Failures += TestBinarySearchCase(500, 60);
    Failures += TestBinarySearchCase(500, 61);
    Failures += TestBinarySearchCase(500, 497);
    Failures += TestBinarySearchCase(500, 498);
    Failures += TestBinarySearchCase(500, 499);
    Failures += TestBinarySearchCase(501, 499);

    //
    // Try the big ones.
    //

    for (Index = -1; Index <= TEST_BINARY_SEARCH_ARRAY_COUNT; Index += 1) {
        Failures += TestBinarySearchCase(TEST_BINARY_SEARCH_ARRAY_COUNT, Index);
    }

    return Failures;
}

//
// --------------------------------------------------------- Internal Functions
//

ULONG
TestBinarySearchCase (
    ULONG ArraySize,
    LONG DesiredIndex
    )

/*++

Routine Description:

    This routine implements a binary search test.

Arguments:

    ArraySize - Supplies the supposed size of the array, up to
        TEST_BINARY_SEARCH_ARRAY_COUNT.

    DesiredIndex - Supplies the desired index to search for. If this is greater
        than or equal to the array size, then the test fails if the element is
        found. Otherwise the test fails if the element is not found.

Return Value:

    0 if the test passed.

    1 if the test failed.

--*/

{

    PLONG FoundValue;

    FoundValue = bsearch(&DesiredIndex,
                         TestBinarySearchArray,
                         ArraySize,
                         sizeof(LONG),
                         TestBinarySearchCompare);

    if (DesiredIndex < ArraySize) {
        if (FoundValue == NULL) {
            printf("bsearch: Failed to find element %d in array of size %d.\n",
                   DesiredIndex,
                   ArraySize);

            return 1;
        }

        if (*FoundValue != DesiredIndex) {
            printf("bsearch: Found wrong value %d. Should have found %d. "
                   "Array size was %d.\n",
                   *FoundValue,
                   DesiredIndex,
                   ArraySize);

            return 1;
        }

    } else {
        if (FoundValue != NULL) {
            printf("bsearch: Found value %d (desired %d) in array of "
                   "size %d that should not have had that element.\n",
                   *FoundValue,
                   DesiredIndex,
                   ArraySize);

            return 1;
        }
    }

    return 0;
}

int
TestBinarySearchCompare (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares two elements in a binary search test.

Arguments:

    LeftPointer - Supplies a pointer to the left element of the comparison.

    RightPointer - Supplies a pointer to the right element of the comparison.

Return Value:

    -1 if Left < Right.

    0 if Left == Right.

    1 if Left > Right.

--*/

{

    LONG Left;
    LONG Right;

    Left = *((PLONG)LeftPointer);
    Right = *((PLONG)RightPointer);
    if (Left < Right) {
        return -1;
    }

    if (Left > Right) {
        return 1;
    }

    return 0;
}

