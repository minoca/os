/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    path.c

Abstract:

    This module contains file path related functions for the C library, such
    as the libgen.h functions.

Author:

    Evan Green 16-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the minimum size for the path split buffer in bytes.
//

#define PATH_SPLIT_BUFFER_MINIMUM_SIZE 16

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
ClpPathSplit (
    PSTR Path,
    PSTR *DirectoryName,
    PSTR *BaseName
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global path split buffer.
//

PSTR ClPathSplitBuffer = NULL;
ULONG ClPathSplitBufferSize = 0;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
basename (
    char *Path
    )

/*++

Routine Description:

    This routine takes in a path and returns a pointer to the final component
    of the pathname, deleting any trailing '/' characters. This routine is
    neither re-entrant nor thread safe.

Arguments:

    Path - Supplies a pointer to the path to split.

Return Value:

    Returns a pointer to the final path component name on success.

    NULL on failure.

--*/

{

    PSTR BaseName;
    BOOL Result;

    Result = ClpPathSplit(Path, NULL, &BaseName);
    if (Result == FALSE) {
        return NULL;
    }

    return BaseName;
}

LIBC_API
char *
dirname (
    char *Path
    )

/*++

Routine Description:

    This routine takes in a path and returns a pointer to the pathname of the
    parent directory of that file, deleting any trailing '/' characters. If the
    path does not contain a '/', or is null or empty, then this routine returns
    a pointer to the string ".". This routine is neither re-entrant nor thread
    safe.

Arguments:

    Path - Supplies a pointer to the path to split.

Return Value:

    Returns a pointer to the name of the directory containing that file on
    success.

    NULL on failure.

--*/

{

    PSTR DirectoryName;
    BOOL Result;

    Result = ClpPathSplit(Path, &DirectoryName, NULL);
    if (Result == FALSE) {
        return NULL;
    }

    return DirectoryName;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
ClpPathSplit (
    PSTR Path,
    PSTR *DirectoryName,
    PSTR *BaseName
    )

/*++

Routine Description:

    This routine takes in a path and splits it into a directory component and
    a final name component. Paths that don't have any slashes in them will
    have a directory name of ".". Trailing slashes are not considered part of
    the path. This routine is neither re-entrant nor thread safe.

Arguments:

    Path - Supplies a pointer to the path to split.

    DirectoryName - Supplies a pointer where the directory portion of the path
        will be returned. This pointer is allocated from a global buffer, and
        must not be modified or freed by the caller. In addition, it may be
        invalidated by future calls to this routine.

    BaseName - Supplies a pointer where the final name portion of the path
        will be returned. This pointer is allocated from a global buffer, and
        must not be modified or freed by the caller. In addition, it may be
        invalidated by future calls to this routine.

Return Value:

    TRUE on success.

    FALSE on allocation failure.

--*/

{

    ULONG BufferSize;
    PSTR Directory;
    ULONG DirectoryEndIndex;
    BOOL DirectoryHadSlash;
    PSTR Name;
    ULONG NameEndIndex;
    ULONG NameStartIndex;
    PSTR NewBuffer;
    ULONG PathLength;
    BOOL Result;

    Directory = NULL;
    Name = NULL;
    PathLength = 0;
    if (Path != NULL) {
        PathLength = strlen(Path);
    }

    BufferSize = PathLength + sizeof(".") + 1;
    if (BufferSize < PATH_SPLIT_BUFFER_MINIMUM_SIZE) {
        BufferSize = PATH_SPLIT_BUFFER_MINIMUM_SIZE;
    }

    //
    // Reallocate the buffer if needed.
    //

    if (ClPathSplitBufferSize < BufferSize) {
        NewBuffer = malloc(BufferSize);
        if (NewBuffer == NULL) {
            Result = FALSE;
            goto PathSplitEnd;
        }

        if (ClPathSplitBuffer != NULL) {
            free(ClPathSplitBuffer);
        }

        ClPathSplitBuffer = NewBuffer;
        ClPathSplitBufferSize = BufferSize;
    }

    Directory = ClPathSplitBuffer;

    //
    // First handle the completely empty case.
    //

    if (PathLength == 0) {
        memcpy(Directory, ".", sizeof("."));
        Name = Directory + sizeof(".");
        *Name = '\0';
        Result = TRUE;
        goto PathSplitEnd;
    }

    //
    // Find the end of the name by backing up past trailing slashes.
    //

    NameEndIndex = PathLength - 1;
    while ((NameEndIndex != 0) && (Path[NameEndIndex] == '/')) {
        NameEndIndex -= 1;
    }

    NameStartIndex = NameEndIndex;
    if (Path[NameEndIndex] != '/') {
        NameEndIndex += 1;
    }

    //
    // Find the start of the name by backing up until the beginning or a slash
    // is found.
    //

    while ((NameStartIndex != 0) && (Path[NameStartIndex] != '/')) {
        NameStartIndex -= 1;
    }

    DirectoryEndIndex = NameStartIndex;
    if ((Path[NameStartIndex] == '/') && (NameStartIndex < NameEndIndex)) {
        NameStartIndex += 1;
    }

    //
    // Find the end of the directory by backing up over any trailing slashes.
    //

    DirectoryHadSlash = FALSE;
    while ((DirectoryEndIndex != 0) && (Path[DirectoryEndIndex] == '/')) {
        DirectoryHadSlash = TRUE;
        DirectoryEndIndex -= 1;
    }

    //
    // If there was a slash at the current index, it must be index zero to have
    // made the loop stop. Set the boolean to indicate there was a slash.
    //

    if (Path[DirectoryEndIndex] == '/') {
        DirectoryHadSlash = TRUE;
    }

    //
    // If there was a slash, the loop either went one too far or found a slash
    // at zero. Either way give it an extra one.
    //

    if (DirectoryHadSlash != FALSE) {
        DirectoryEndIndex += 1;
    }

    //
    // Ok, all the important indices are found. Copy the portions in.
    //

    if (DirectoryHadSlash == FALSE) {
        memcpy(Directory, ".", sizeof("."));
        Name = Directory + sizeof(".");

    } else {

        assert(DirectoryEndIndex != 0);

        memcpy(Directory, Path, DirectoryEndIndex);
        Directory[DirectoryEndIndex] = '\0';
        Name = Directory + DirectoryEndIndex + 1;
    }

    //
    // Copy the name.
    //

    if (NameEndIndex != NameStartIndex) {

        assert(NameEndIndex > NameStartIndex);

        memcpy(Name, Path + NameStartIndex, NameEndIndex - NameStartIndex);
        Name[NameEndIndex - NameStartIndex] = '\0';

    } else {
        *Name = '\0';
    }

    Result = TRUE;

PathSplitEnd:
    if (DirectoryName != NULL) {
        *DirectoryName = Directory;
    }

    if (BaseName != NULL) {
        *BaseName = Name;
    }

    return Result;
}

