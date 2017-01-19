/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    scandir.c

Abstract:

    This module implements support for the scandir function, which scans a
    directory.

Author:

    Evan Green 17-Aug-2016

Environment:

    C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

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
int
alphasort (
    const struct dirent **Left,
    const struct dirent **Right
    )

/*++

Routine Description:

    This routine can be used as the comparison function for the scandir
    function. It will compare directory names in alphabetical order.

Arguments:

    Left - Supplies a pointer to a pointer to the left directory entry to
        compare.

    Right - Supplies a pointer to a pointer to the right directory entry to
        compare.

Return Value:

    < 0 if Left < Right.

    0 if Left == Right.

    > 0 if Left > Right.

--*/

{

    return strcmp((*Left)->d_name, (*Right)->d_name);
}

LIBC_API
int
scandir (
    const char *DirectoryPath,
    struct dirent ***NameList,
    int (*SelectFunction)(const struct dirent *),
    int (*CompareFunction)(const struct dirent **, const struct dirent **)
    )

/*++

Routine Description:

    This routine scans the given directory, and calls the select function for
    each entry. If the select function returns non-zero, then the entry is
    added to the list of entries to return. If the compare function is
    non-zero the resulting entries are sorted via qsort.

Arguments:

    DirectoryPath - Supplies a pointer to a null-terminated string containing
        the directory path to scan.

    NameList - Supplies a pointer where an array of pointers to selected
        directory entries will be returned on success. The caller is
        responsible for freeing both the returned array and each individual
        element in the array.

    SelectFunction - Supplies an optional pointer to a function used to
        determine whether or not a particular entry should end up in the
        final list. If the select function returns non-zero, then the entry
        is added to the final list. If this parameter is NULL, then all
        entries are selected.

    CompareFunction - Supplies an optional pointer to a function used to
        compare two directory entries during the final sorting process. If this
        is NULL, then the order of the names in the name list is unspecified.

Return Value:

    Returns the number of entries returned in the array on success.

    -1 on failure.

--*/

{

    UINTN Capacity;
    DIR *Directory;
    struct dirent *Entry;
    size_t EntrySize;
    size_t Index;
    struct dirent **List;
    size_t ListSize;
    size_t NewCapacity;
    struct dirent *NewEntry;
    struct dirent **NewList;
    int Status;

    Status = -1;
    *NameList = NULL;
    ListSize = 0;
    Directory = opendir(DirectoryPath);
    if (Directory == NULL) {
        return -1;
    }

    Capacity = 32;
    List = malloc(sizeof(void *) * Capacity);
    if (List == NULL) {
        goto scandirEnd;
    }

    while (TRUE) {
        Entry = readdir(Directory);
        if (Entry == NULL) {
            break;
        }

        //
        // Call the select function, and skip this entry if the function
        // doesn't want it.
        //

        if ((SelectFunction != NULL) && (SelectFunction(Entry) == 0)) {
            continue;
        }

        //
        // Reallocate the list if needed.
        //

        if (ListSize >= Capacity) {
            NewCapacity = Capacity * 2;
            if ((NewCapacity * sizeof(void *)) < (Capacity * sizeof(void *))) {
                goto scandirEnd;
            }

            NewList = realloc(List, NewCapacity * sizeof(void *));
            if (NewList == NULL) {
                goto scandirEnd;
            }

            Capacity = NewCapacity;
            List = NewList;
        }

        //
        // Allocate an entry just the right size.
        //

        EntrySize = Entry->d_reclen;
        NewEntry = malloc(EntrySize);
        if (NewEntry == NULL) {
            goto scandirEnd;
        }

        memcpy(NewEntry, Entry, EntrySize);
        List[ListSize] = NewEntry;
        ListSize += 1;
    }

    if (CompareFunction != NULL) {
        qsort(List, ListSize, sizeof(struct dirent *), (void *)CompareFunction);
    }

    Status = ListSize;

scandirEnd:
    if (Directory != NULL) {
        closedir(Directory);
    }

    if (Status < 0) {
        if (List != NULL) {
            for (Index = 0; Index < ListSize; Index += 1) {
                free(List[Index]);
            }

            free(List);
            List = NULL;
        }
    }

    *NameList = List;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

