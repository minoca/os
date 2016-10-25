/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    compare.c

Abstract:

    This module implements the compare functions for sorting files in the ls
    utility.

Author:

    Evan Green 25-Jun-2013

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "ls.h"

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

int
LsCompareFilesByName (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by file name.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    PLS_FILE File1;
    PLS_FILE File2;
    PSTR Name1;
    PSTR Name2;
    ULONG NameIndex;

    File1 = *((PLS_FILE *)Item1);
    File2 = *((PLS_FILE *)Item2);
    Name1 = File1->Name;
    Name2 = File2->Name;
    NameIndex = 0;

    //
    // Scan once without regard to weight.
    //

    while ((tolower(Name1[NameIndex]) == tolower(Name2[NameIndex])) &&
           (Name1[NameIndex] != '\0')) {

        NameIndex += 1;
    }

    if (Name1[NameIndex] != Name2[NameIndex]) {
        if (tolower(Name1[NameIndex]) > tolower(Name2[NameIndex])) {
            return 1;

        } else {
            return -1;
        }
    }

    //
    // They're equal. Scan again without regard to case.
    //

    NameIndex = 0;
    while ((Name1[NameIndex] == Name2[NameIndex]) &&
           (Name1[NameIndex] != '\0')) {

        NameIndex += 1;
    }

    if (Name1[NameIndex] != Name2[NameIndex]) {
        if (Name1[NameIndex] > Name2[NameIndex]) {
            return 1;

        } else {
            return -1;
        }
    }

    return 0;
}

int
LsCompareFilesByReverseName (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by file name, in reverse.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    return -(LsCompareFilesByName(Item1, Item2));
}

int
LsCompareFilesByModificationDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by modification date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    PLS_FILE File1;
    PLS_FILE File2;

    File1 = *((PLS_FILE *)Item1);
    File2 = *((PLS_FILE *)Item2);
    if (File1->Stat.st_mtime < File2->Stat.st_mtime) {
        return 1;

    } else if (File1->Stat.st_mtime > File2->Stat.st_mtime) {
        return -1;
    }

    return LsCompareFilesByName(Item1, Item2);
}

int
LsCompareFilesByReverseModificationDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by reverse modification date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    return -(LsCompareFilesByModificationDate(Item1, Item2));
}

int
LsCompareFilesByStatusChangeDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by status change date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    PLS_FILE File1;
    PLS_FILE File2;

    File1 = *((PLS_FILE *)Item1);
    File2 = *((PLS_FILE *)Item2);
    if (File1->Stat.st_ctime < File2->Stat.st_ctime) {
        return -1;

    } else if (File1->Stat.st_ctime > File2->Stat.st_ctime) {
        return 1;
    }

    return LsCompareFilesByName(Item1, Item2);
}

int
LsCompareFilesByReverseStatusChangeDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by reverse status change date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    return -(LsCompareFilesByStatusChangeDate(Item1, Item2));
}

int
LsCompareFilesByAccessDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by its last access date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    PLS_FILE File1;
    PLS_FILE File2;

    File1 = *((PLS_FILE *)Item1);
    File2 = *((PLS_FILE *)Item2);
    if (File1->Stat.st_atime < File2->Stat.st_atime) {
        return -1;

    } else if (File1->Stat.st_atime > File2->Stat.st_atime) {
        return 1;
    }

    return LsCompareFilesByName(Item1, Item2);
}

int
LsCompareFilesByReverseAccessDate (
    const void *Item1,
    const void *Item2
    )

/*++

Routine Description:

    This routine compares two files by reverse access date.

Arguments:

    Item1 - Supplies a pointer to the first element, which is a pointer to an
        ls file structure.

    Item2 - Supplies a pointer to the second element, which is a pointer to an
        ls file structure.

Return Value:

    Less than zero if the first argument is less than the second.

    Zero if the first argument is equal to the second.

    Greater than zero if the first argument is greater than the second.

--*/

{

    return -(LsCompareFilesByAccessDate(Item1, Item2));
}

//
// --------------------------------------------------------- Internal Functions
//

