/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tmpfile.c

Abstract:

    This module implements support for creating temporary files.

Author:

    Evan Green 12-Aug-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

#define TEMPORARY_FILE_RANDOM_CHARACTERS 5
#define TEMPORARY_FILE_LONG_RANDOM_CHARACTERS 8

//
// Define the number of times the mktemp functions will try to create a unique
// file name before giving up.
//

#define MKTEMP_TRY_COUNT MAX_ULONG

//
// Define the permissions on temporary files.
//

#define TEMPORARY_FILE_PERMISSIONS (S_IRUSR | S_IWUSR)

//
// Define the permissions on a temporary directory.
//

#define TEMPORARY_DIRECTORY_PERMISSIONS (S_IRUSR | S_IWUSR | S_IXUSR)

//
// Define the default temporary file name prefix.
//

#define TEMPORARY_FILE_PREFIX "tmp"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
ClpCreateRandomString (
    PSTR String,
    ULONG CharacterCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the global temporary name buffer.
//

char *ClTemporaryNameBuffer;

//
// Store the temporary name seed.
//

unsigned ClTemporaryNameSeed;

//
// ------------------------------------------------------------------ Functions
//

LIBC_API
char *
tmpnam (
    char *Buffer
    )

/*++

Routine Description:

    This routine generates a string that is a valid filename and is not the
    name of an existing file. This routine returns a different name each time
    it is called. Note that between the time the name is returned and when an
    application goes to create the file, the file may already be created.
    Applications may find the tmpfile function more robust and useful.

Arguments:

    Buffer - Supplies an optional pointer to a buffer where the name will be
        returned. This buffer is assumed to be at least L_tmpnam bytes large.
        If this buffer is not supplied, then a pointer to a global buffer will
        be returned. Subsequent calls to this routine will overwrite the
        contents of that returned buffer.

Return Value:

    Returns a pointer to a string containing the name of a temporary file. This
    returns the buffer if it is supplied, or a pointer to a global buffer
    otherwise.

--*/

{

    int OriginalError;
    char RandomBuffer[TEMPORARY_FILE_RANDOM_CHARACTERS + 1];
    int Result;
    struct stat Stat;
    ULONG Try;

    if (Buffer == NULL) {
        if (ClTemporaryNameBuffer == NULL) {
            ClTemporaryNameBuffer = malloc(L_tmpnam);
            if (ClTemporaryNameBuffer == NULL) {
                return NULL;
            }
        }

        Buffer = ClTemporaryNameBuffer;
    }

    //
    // Loop creating random names as long as they exist.
    //

    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        ClpCreateRandomString(RandomBuffer, sizeof(RandomBuffer) - 1);
        RandomBuffer[sizeof(RandomBuffer) - 1] = '\0';
        snprintf(Buffer, L_tmpnam, TEMPORARY_FILE_PREFIX "%s", RandomBuffer);
        OriginalError = errno;
        Result = stat(Buffer, &Stat);
        errno = OriginalError;
        if (Result != 0) {
            break;
        }
    }

    return Buffer;
}

LIBC_API
char *
tempnam (
    const char *Directory,
    const char *Prefix
    )

/*++

Routine Description:

    This routine generates path name that may be used for a temporary file.

Arguments:

    Directory - Supplies an optional pointer to a string containing the name of
        the directory in which the temporary file is to be created. If the
        directory is not supplied or is not an appropriate directory, then the
        path prefix defined as P_tmpdir in stdio.h shall be used.

    Prefix - Supplies a pointer to a string containing up to a five character
        prefix on the temporary file name.

Return Value:

    Returns a pointer to a string containing the name of a temporary file. The
    caller must call free when finished with this buffer to reclaim the memory
    allocated by this routine.

    NULL on failure, and errno will be set to contain more information.

--*/

{

    PSTR Buffer;
    size_t Length;
    int OriginalError;
    size_t RandomPartOffset;
    int Result;
    struct stat Stat;
    ULONG Try;

    //
    // Figure out the directory to use.
    //

    OriginalError = errno;
    if (Directory == NULL) {
        Directory = P_tmpdir;

    } else {
        Result = stat(Directory, &Stat);
        if ((Result != 0) || (!S_ISDIR(Stat.st_mode))) {
            Directory = P_tmpdir;
        }
    }

    //
    // Figure out the length of the final string, and allocate a buffer for it.
    // The form will be <directory>/<prefix><Random>.
    //

    if (Prefix == NULL) {
        Prefix = TEMPORARY_FILE_PREFIX;
    }

    RandomPartOffset = strlen(Directory) + 1 + strlen(Prefix);
    Length = RandomPartOffset + TEMPORARY_FILE_LONG_RANDOM_CHARACTERS + 1;
    Buffer = malloc(Length);
    if (Buffer == NULL) {
        return NULL;
    }

    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        snprintf(Buffer, Length, "%s/%s", Directory, Prefix);
        ClpCreateRandomString(Buffer + RandomPartOffset,
                              TEMPORARY_FILE_LONG_RANDOM_CHARACTERS);

        Buffer[Length - 1] = '\0';
        Result = stat(Buffer, &Stat);
        if (Result != 0) {
            break;
        }
    }

    //
    // Restore the errno variable.
    //

    errno = OriginalError;
    return Buffer;
}

LIBC_API
FILE *
tmpfile (
    void
    )

/*++

Routine Description:

    This routine creates a file and opens a corresponding stream. The file
    shall be automatically deleted when all references to the file are closed.
    The file is opened as in fopen for update ("w+").

Arguments:

    None.

Return Value:

    Returns an open file stream on success.

    NULL if a temporary file could not be created.

--*/

{

    PSTR Buffer;
    int Descriptor;
    PSTR Directory;
    FILE *File;
    size_t Length;
    int OriginalError;
    PSTR Prefix;
    size_t RandomPartOffset;
    ULONG Try;

    File = NULL;
    OriginalError = errno;
    Directory = P_tmpdir;
    Prefix = TEMPORARY_FILE_PREFIX;

    //
    // Allocate a buffer to hold the temporary file name.
    //

    RandomPartOffset = strlen(Directory) + 1 + strlen(Prefix);
    Length = RandomPartOffset + TEMPORARY_FILE_LONG_RANDOM_CHARACTERS + 1;
    Buffer = malloc(Length);
    if (Buffer == NULL) {
        return NULL;
    }

    //
    // Loop creating random names and trying to exclusively create them.
    //

    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        snprintf(Buffer, Length, "%s/%s", Directory, Prefix);
        ClpCreateRandomString(Buffer + RandomPartOffset,
                              TEMPORARY_FILE_LONG_RANDOM_CHARACTERS);

        Buffer[Length - 1] = '\0';

        //
        // Try to exclusively create the file. If that works, open a stream too.
        //

        Descriptor = open(Buffer, O_CREAT | O_EXCL, TEMPORARY_FILE_PERMISSIONS);
        if (Descriptor >= 0) {
            File = fdopen(Descriptor, "w+");
            if (File != NULL) {

                //
                // Unlink the file so that it is deleted whenever the file is
                // closed.
                //

                unlink(Buffer);
                break;
            }

            close(Descriptor);

            //
            // The file opened but not the stream. Stop, as something is going
            // on here like a low memory condition.
            //

            break;
        }

        //
        // Also stop if the error is anything other than some standard errors.
        //

        if ((errno != EEXIST) && (errno != EPERM) && (errno != EACCES)) {
            break;
        }
    }

    free(Buffer);

    //
    // Restore the errno variable.
    //

    errno = OriginalError;
    return File;
}

LIBC_API
char *
mktemp (
    char *Template
    )

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    filename.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns a pointer to the template string.

--*/

{

    size_t Length;
    int OriginalError;
    int Result;
    struct stat Stat;
    ULONG Try;

    //
    // Ensure the string ends in six X characters.
    //

    Length = strlen(Template);
    if ((Length < 6) || (strcmp(Template + Length - 6, "XXXXXX") != 0)) {
        errno = EINVAL;
        return NULL;
    }

    OriginalError = errno;
    errno = 0;
    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        ClpCreateRandomString(Template + Length - 6, 6);
        Result = stat(Template, &Stat);
        if (Result != 0) {
            break;
        }
    }

    //
    // If the error is "no such file", that's a good thing, it means the
    // string is available.
    //

    if (errno == ENOENT) {
        errno = OriginalError;

    //
    // Anything else is a failure.
    //

    } else {
        Template = NULL;
    }

    return Template;
}

LIBC_API
char *
mkdtemp (
    char *Template
    )

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    directory name, and attempts to create that directory.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns a pointer to the template string.

--*/

{

    size_t Length;
    int OriginalError;
    int Result;
    ULONG Try;

    //
    // Ensure the string ends in six X characters.
    //

    Length = strlen(Template);
    if ((Length < 6) || (strcmp(Template + Length - 6, "XXXXXX") != 0)) {
        errno = EINVAL;
        return NULL;
    }

    OriginalError = errno;
    errno = 0;
    Result = -1;
    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        ClpCreateRandomString(Template + Length - 6, 6);
        Result = mkdir(Template, TEMPORARY_DIRECTORY_PERMISSIONS);
        if (Result == 0) {
            break;
        }

        if (errno != EEXIST) {
            break;
        }
    }

    if (Result == 0) {
        errno = OriginalError;

    } else {
        Template = NULL;
    }

    return Template;
}

LIBC_API
int
mkstemp (
    char *Template
    )

/*++

Routine Description:

    This routine creates replaces the contents of the given string by a unique
    filename, and returns an open file descriptor to that file.

Arguments:

    Template - Supplies a pointer to a template string that will be modified
        in place. The string must end in six X characters. Each X character
        will be replaced by a random valid filename character.

Return Value:

    Returns the open file descriptor to the newly created file on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    size_t Length;
    int OriginalError;
    int Result;
    ULONG Try;

    //
    // Ensure the string ends in six X characters.
    //

    Length = strlen(Template);
    if ((Length < 6) || (strcmp(Template + Length - 6, "XXXXXX") != 0)) {
        errno = EINVAL;
        return -1;
    }

    Result = -1;
    OriginalError = errno;
    errno = 0;
    for (Try = 0; Try < MKTEMP_TRY_COUNT; Try += 1) {
        ClpCreateRandomString(Template + Length - 6, 6);
        Result = open(Template,
                      O_RDWR | O_CREAT | O_EXCL,
                      TEMPORARY_FILE_PERMISSIONS);

        if (Result >= 0) {
            break;
        }

        if ((errno != EEXIST) && (errno != EISDIR)) {
            break;
        }
    }

    if (Result >= 0) {
        errno = OriginalError;
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
ClpCreateRandomString (
    PSTR String,
    ULONG CharacterCount
    )

/*++

Routine Description:

    This routine creates random ASCII characters in the range of 0-9 and A-Z.

Arguments:

    String - Supplies a pointer where the random characters will be returned.
        This buffer will NOT be null terminated by this function.

    CharacterCount - Supplies the number of random characters to generate.

Return Value:

    None.

--*/

{

    ULONG RandomIndex;
    INT Value;

    if (ClTemporaryNameSeed == 0) {
        ClTemporaryNameSeed = time(NULL) ^ getpid();
    }

    for (RandomIndex = 0; RandomIndex < CharacterCount; RandomIndex += 1) {

        //
        // Create a random value using letters and numbers. Avoid relying
        // on case sensitivity just in case. For reference,
        // 36^5 is 60.4 million.
        //

        Value = rand_r(&ClTemporaryNameSeed) % 36;
        if (Value >= 10) {
            Value += 'A' - 10;

        } else {
            Value += '0';
        }

        String[RandomIndex] = Value;
    }

    return;
}

