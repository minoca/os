/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    string.c

Abstract:

    This module implements string functionality for the swiss function library.

Author:

    Evan Green 12-Jul-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "../swlib.h"

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

BOOL
SwAppendPath (
    PSTR Prefix,
    ULONG PrefixSize,
    PSTR Component,
    ULONG ComponentSize,
    PSTR *AppendedPath,
    PULONG AppendedPathSize
    )

/*++

Routine Description:

    This routine appends a path component to a path.

Arguments:

    Prefix - Supplies the initial path string. This can be null.

    PrefixSize - Supplies the size of the prefix string in bytes including the
        null terminator.

    Component - Supplies a pointer to the component string to add.

    ComponentSize - Supplies the size of the component string in bytes
        including a null terminator.

    AppendedPath - Supplies a pointer where the new path will be returned. The
        caller is responsible for freeing this memory.

    AppendedPathSize - Supplies a pointer where the size of the appended bath
        buffer in bytes including the null terminator will be returned.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL NeedSlash;
    PSTR NewPath;
    ULONG NewPathSize;
    BOOL Result;

    NeedSlash = FALSE;
    NewPath = NULL;
    NewPathSize = 0;

    //
    // Pull the trailing null off of the prefix string. If the prefix ends in
    // a slash then there's no need to append a slash.
    //

    if ((Prefix != NULL) && (*Prefix != '\0')) {

        assert(PrefixSize != 0);

        if (Prefix[PrefixSize - 1] == '\0') {
            PrefixSize -= 1;
            if (PrefixSize == 0) {
                Prefix = NULL;
            }
        }

        NeedSlash = TRUE;
        if ((Prefix != NULL) && (Prefix[PrefixSize - 1] == '/')) {
            NeedSlash = FALSE;
        }

        //
        // Get rid of any leading slashes in the component.
        //

        while ((ComponentSize != 0) && (*Component == '/')) {
            Component += 1;
            ComponentSize -= 1;
        }
    }

    if ((ComponentSize == 0) || (*Component == '\0')) {
        Result = FALSE;
        goto AppendPathEnd;
    }

    if (Component[ComponentSize - 1] != '\0') {
        ComponentSize += 1;
    }

    //
    // Allocate and create the new string.
    //

    NewPathSize = PrefixSize + ComponentSize;
    if (NeedSlash != 0) {
        NewPathSize += 1;
    }

    NewPath = malloc(NewPathSize);
    if (NewPath == NULL) {
        Result = FALSE;
        goto AppendPathEnd;
    }

    if (Prefix != NULL) {
        memcpy(NewPath, Prefix, PrefixSize);
    }

    if (NeedSlash != FALSE) {
        NewPath[PrefixSize] = '/';
        memcpy(NewPath + PrefixSize + 1, Component, ComponentSize);

    } else {
        memcpy(NewPath + PrefixSize, Component, ComponentSize);
    }

    NewPath[NewPathSize - 1] = '\0';
    Result = TRUE;

AppendPathEnd:
    if (Result == FALSE) {
        if (NewPath != NULL) {
            free(NewPath);
            NewPath = NULL;
        }

        NewPathSize = 0;
    }

    *AppendedPath = NewPath;
    *AppendedPathSize = NewPathSize;
    return Result;
}

PSTR
SwQuoteArgument (
    PSTR Argument
    )

/*++

Routine Description:

    This routine puts a backslash before every single quote and backslash in
    the given argument.

Arguments:

    Argument - Supplies a pointer to the argument to quote.

Return Value:

    Returns the original argument if it needs no quoting.

    Returns a newly allocated string if the argument needs quoting. The caller
    is responsible for freeing this string.

--*/

{

    ULONG DestinationIndex;
    ULONG Index;
    PSTR NewString;
    ULONG QuoteCount;

    //
    // Loop through once to see if there are any single quotes or backslashes.
    //

    Index = 0;
    QuoteCount = 0;
    while (Argument[Index] != '\0') {
        if ((Argument[Index] == '\'') || (Argument[Index] == '\\')) {
            QuoteCount += 1;
        }

        Index += 1;
    }

    if (QuoteCount == 0) {
        return Argument;
    }

    NewString = malloc(Index + QuoteCount + 1);
    if (NewString == NULL) {
        return Argument;
    }

    Index = 0;
    DestinationIndex = 0;
    while (Argument[Index] != '\0') {
        if ((Argument[Index] == '\'') || (Argument[Index] == '\\')) {
            NewString[DestinationIndex] = '\\';
            DestinationIndex += 1;
        }

        NewString[DestinationIndex] = Argument[Index];
        DestinationIndex += 1;
        Index += 1;
    }

    NewString[DestinationIndex] = '\0';
    return NewString;
}

PSTR
SwStringDuplicate (
    PSTR String,
    UINTN StringSize
    )

/*++

Routine Description:

    This routine copies a string.

Arguments:

    String - Supplies a pointer to the string to copy.

    StringSize - Supplies the size of the string buffer in bytes.

Return Value:

    Returns a pointer to the new string on success.

    NULL on failure.

--*/

{

    PSTR NewString;

    assert(StringSize != 0);

    NewString = malloc(StringSize);
    if (NewString == NULL) {
        return NULL;
    }

    memcpy(NewString, String, StringSize);
    NewString[StringSize - 1] = '\0';
    return NewString;
}

BOOL
SwStringReplaceRegion (
    PSTR *StringBufferAddress,
    PUINTN StringBufferSize,
    PUINTN StringBufferCapacity,
    UINTN SourceRegionBegin,
    UINTN SourceRegionEnd,
    PSTR Replacement,
    UINTN ReplacementSize
    )

/*++

Routine Description:

    This routine replaces a portion of the given string.

Arguments:

    StringBufferAddress - Supplies a pointer to the address of the allocated
        string buffer. This value may be changed if the string is reallocated.

    StringBufferSize - Supplies a pointer that on input contains the size of
        the current string in bytes, including the null terminator. On output,
        it will contain the updated size of the string in bytes.

    StringBufferCapacity - Supplies a pointer that on input supplies the total
        size of the buffer. It will contain the updated size of the buffer
        allocation on output.

    SourceRegionBegin - Supplies the index into the buffer where the
        replacement occurs.

    SourceRegionEnd - Supplies the index into the buffer where the expansion
        ends, exclusive.

    Replacement - Supplies a pointer to the string to replace that region with.

    ReplacementSize - Supplies the size of the replacement string in bytes
        including the null terminator.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Buffer;
    INTN CharacterIndex;
    UINTN NewBufferSize;
    UINTN OverlapCount;
    INTN OverlapIndex;
    UINTN RegionSize;
    UINTN SizeNeeded;

    //
    // This logic would need to be a little more careful if there wasn't
    // already at least one byte there.
    //

    assert(*StringBufferSize != 0);
    assert(*StringBufferCapacity >= *StringBufferSize);

    if (Replacement != NULL) {
        if (ReplacementSize == 0) {
            Replacement = NULL;

        } else {
            ReplacementSize -= 1;
        }
    }

    //
    // Figure out how big the final buffer needs to be, and reallocate if
    // needed.
    //

    RegionSize = SourceRegionEnd - SourceRegionBegin;
    SizeNeeded = *StringBufferSize - RegionSize + ReplacementSize;
    NewBufferSize = *StringBufferCapacity;
    while (NewBufferSize < SizeNeeded) {
        NewBufferSize *= 2;
    }

    if (NewBufferSize > *StringBufferCapacity) {
        Buffer = realloc(*StringBufferAddress, NewBufferSize);
        if (Buffer == NULL) {
            return FALSE;
        }

        *StringBufferAddress = Buffer;
        *StringBufferCapacity = NewBufferSize;

    } else {
        Buffer = *StringBufferAddress;
    }

    //
    // If the new string is bigger than the original region, shift everything
    // right.
    //

    if (ReplacementSize > RegionSize) {
        OverlapCount = ReplacementSize - RegionSize;
        OverlapIndex = SourceRegionBegin + ReplacementSize;
        for (CharacterIndex = SizeNeeded - 1;
             CharacterIndex >= OverlapIndex;
             CharacterIndex -= 1) {

            Buffer[CharacterIndex] = Buffer[CharacterIndex - OverlapCount];
        }

    //
    // If the string is smaller than the original region, snip out the
    // remainder.
    //

    } else if (ReplacementSize < RegionSize) {
        SwStringRemoveRegion(Buffer,
                             StringBufferSize,
                             SourceRegionBegin + ReplacementSize,
                             RegionSize - ReplacementSize);

        assert(*StringBufferSize == SizeNeeded);
    }

    //
    // Now copy the replacement in.
    //

    for (CharacterIndex = 0;
         CharacterIndex < ReplacementSize;
         CharacterIndex += 1) {

        Buffer[CharacterIndex + SourceRegionBegin] =
                                                   Replacement[CharacterIndex];
    }

    //
    // Even after all this moving around the string should still be null
    // terminated.
    //

    *StringBufferSize = SizeNeeded;
    return TRUE;
}

VOID
SwStringRemoveRegion (
    PSTR String,
    PUINTN StringSize,
    UINTN RemoveIndex,
    UINTN RemoveLength
    )

/*++

Routine Description:

    This routine removes a portion of the given string.

Arguments:

    String - Supplies a pointer to the string to remove a portion of.

    StringSize - Supplies a pointer that on input contains the size of the
        string in bytes including the null terminator. On output this value
        will be updated to reflect the removal.

    RemoveIndex - Supplies the starting index to remove.

    RemoveLength - Supplies the number of characters to remove.

Return Value:

    None.

--*/

{

    UINTN CharacterIndex;

    assert((RemoveIndex < *StringSize) &&
           (RemoveIndex + RemoveLength <= *StringSize));

    for (CharacterIndex = RemoveIndex;
         CharacterIndex < *StringSize;
         CharacterIndex += 1) {

        if (CharacterIndex + RemoveLength < *StringSize) {
            String[CharacterIndex] = String[CharacterIndex + RemoveLength];
        }
    }

    *StringSize -= RemoveLength;
    return;
}

BOOL
SwRotatePointerArray (
    PVOID *Array,
    ULONG ColumnCount,
    ULONG RowCount
    )

/*++

Routine Description:

    This routine rotates an array by row and column, so that an array that
    used to read 1 2 3 4 / 5 6 7 8 will now read 1 3 5 7 / 2 4 6 8. The row
    and column counts don't change, but after this transformation the elements
    can be read down the column instead of across the row.

Arguments:

    Array - Supplies the array of pointers to rotate.

    ColumnCount - Supplies the number of columns that the array is
        represented in.

    RowCount - Supplies the number of rows that the array is represented in.

Return Value:

    TRUE on success.

    FALSE on temporary allocation failure.

--*/

{

    ULONG DestinationColumn;
    ULONG DestinationIndex;
    ULONG DestinationRow;
    ULONG ElementCount;
    PVOID *Original;
    ULONG SourceIndex;

    if ((RowCount <= 1) || (ColumnCount <= 1)) {
        return TRUE;
    }

    ElementCount = ColumnCount * RowCount;
    Original = malloc(ElementCount * sizeof(PVOID));
    if (Original == NULL) {
        return FALSE;
    }

    memcpy(Original, Array, ElementCount * sizeof(PVOID));
    memset(Array, 0, ElementCount * sizeof(PVOID));
    for (SourceIndex = 0; SourceIndex < ElementCount; SourceIndex += 1) {

        //
        // Scanning across the source array, fill in down each column, moving
        // to a new column when the previous one is full.
        //

        DestinationColumn = SourceIndex / RowCount;
        DestinationRow = SourceIndex % RowCount;
        DestinationIndex = (DestinationRow * ColumnCount) + DestinationColumn;
        Array[DestinationIndex] = Original[SourceIndex];
    }

    free(Original);
    return TRUE;
}

INT
SwGetSignalNumberFromName (
    PSTR SignalName
    )

/*++

Routine Description:

    This routine parses a signal number. This can either be a numerical value,
    a string like TERM, or a string like SIGTERM. The string is matched without
    regard for case.

Arguments:

    SignalName - Supplies a pointer to the string to convert to a signal
        number.

Return Value:

    Returns the corresponding signal number on success.

    -1 on failure.

--*/

{

    PSTR AfterScan;
    PSTR Copy;
    PSWISS_SIGNAL_NAME Entry;
    INT Extra;
    PSTR Operator;
    INT Signal;

    Signal = -1;
    if (isdigit(*SignalName)) {
        Signal = strtoul(SignalName, &AfterScan, 10);
        if (AfterScan == SignalName) {
            Signal = -1;
        }

    } else {
        if (strncasecmp(SignalName, "SIG", 3) == 0) {
            SignalName += 3;
        }

        //
        // If there's a plus or minus on the end, chop it off and
        // get the extra part.
        //

        Copy = NULL;
        Extra = 0;
        Operator = strchr(SignalName, '+');
        if (Operator == NULL) {
            Operator = strchr(SignalName, '-');
        }

        if (Operator != NULL) {
            Extra = strtol(Operator, &AfterScan, 10);
            if (AfterScan == Operator) {
                return -1;
            }

            Copy = strdup(SignalName);
            if (Copy == NULL) {
                return -1;
            }

            Operator = Copy + (Operator - SignalName);
            *Operator = '\0';
            SignalName = Copy;
        }

        Entry = &(SwSignalMap[0]);
        while (Entry->SignalName != NULL) {
            if (strcasecmp(SignalName, Entry->SignalName) == 0) {
                Signal = Entry->SignalNumber + Extra;
                break;
            }

            Entry += 1;
        }

        if (Copy != NULL) {
            free(Copy);
        }
    }

    return Signal;
}

PSTR
SwGetSignalNameFromNumber (
    INT SignalNumber
    )

/*++

Routine Description:

    This routine returns a pointer to a constant string for the given signal
    number. This string does not have the SIG prefix appended to it.

Arguments:

    SignalNumber - Supplies the signal number to get the string for.

Return Value:

    Returns a pointer to a string containing the name of the signal on success.

    NULL if the signal number is invalid.

--*/

{

    PSWISS_SIGNAL_NAME Entry;

    Entry = &(SwSignalMap[0]);
    if (SIGRTMAX > SIGRTMIN) {
        if (SignalNumber == SIGRTMIN) {
            return "RTMIN";

        } else if (SignalNumber == SIGRTMAX) {
            return "RTMAX";
        }
    }

    while (Entry->SignalName != NULL) {
        if (Entry->SignalNumber == SignalNumber) {
            return Entry->SignalName;
        }

        Entry += 1;
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

