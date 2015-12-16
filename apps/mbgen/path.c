/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    path.c

Abstract:

    This module implements path utility functions for the Minoca Build
    Generator.

Author:

    Evan Green 3-Dec-2015

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbgen.h"

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

PSTR
MbgenAppendPaths3 (
    PSTR Path1,
    PSTR Path2,
    PSTR Path3
    )

/*++

Routine Description:

    This routine appends three paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

    Path3 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

{

    PSTR Intermediate;
    PSTR Result;

    Intermediate = MbgenAppendPaths(Path1, Path2);
    if (Intermediate == NULL) {
        return NULL;
    }

    Result = MbgenAppendPaths(Intermediate, Path3);
    free(Intermediate);
    return Result;
}

PSTR
MbgenAppendPaths (
    PSTR Path1,
    PSTR Path2
    )

/*++

Routine Description:

    This routine appends two paths to one another.

Arguments:

    Path1 - Supplies a pointer to the first path.

    Path2 - Supplies a pointer to the second path.

Return Value:

    Returns a pointer to a newly created combined path on success. The caller
    is responsible for freeing this new path.

    NULL on allocation failure.

--*/

{

    size_t Length1;
    size_t Length2;
    PSTR NewPath;

    if (Path1 == NULL) {
        return strdup(Path2);
    }

    Length1 = strlen(Path1);
    Length2 = strlen(Path2);
    NewPath = malloc(Length1 + Length2 + 2);
    if (NewPath == NULL) {
        return NULL;
    }

    strcpy(NewPath, Path1);
    if ((Length1 != 0) && (Path1[Length1 - 1] != '/')) {
        NewPath[Length1] = '/';
        Length1 += 1;
    }

    strcpy(NewPath + Length1, Path2);
    return NewPath;
}

INT
MbgenSetupRootDirectories (
    PMBGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine finds or validates the source root directory, and validates
    the build directory.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentDirectory;
    int Descriptor;
    PSTR PreviousDirectory;
    PSTR SourceRoot;
    int Status;

    //
    // Make sure it exists if it was specified by the user.
    //

    if (Context->SourceRoot != NULL) {
        if (chdir(Context->SourceRoot) != 0) {
            Status = errno;
            fprintf(stderr,
                    "Error: Invalid source root directory %s: %s.\n",
                    Context->SourceRoot,
                    strerror(Status));

            return Status;
        }

        //
        // Convert it to an absolute path.
        //

        SourceRoot = getcwd(NULL, 0);
        if (SourceRoot == NULL) {
            return errno;
        }

        free(Context->SourceRoot);
        Context->SourceRoot = SourceRoot;

    } else {

        //
        // Attempt to open a project root file in each directory.
        //

        PreviousDirectory = NULL;
        while (TRUE) {
            CurrentDirectory = getcwd(NULL, 0);
            if (CurrentDirectory == NULL) {
                break;
            }

            Descriptor = open(Context->ProjectFileName, O_RDONLY);
            if (Descriptor >= 0) {
                close(Descriptor);
                Context->SourceRoot = CurrentDirectory;
                break;
            }

            //
            // Check to see if this directory is the same as the previous
            // directory, indicating that the file system root has been hit.
            //

            if (PreviousDirectory != NULL) {
                if (strcmp(CurrentDirectory, PreviousDirectory) == 0) {
                    errno = ENOENT;
                    break;
                }

                free(PreviousDirectory);
            }

            PreviousDirectory = CurrentDirectory;
            if (chdir("..") != 0) {
                break;
            }
        }

        if (PreviousDirectory != NULL) {
            free(PreviousDirectory);
        }
    }

    if (Context->SourceRoot != NULL) {
        errno = 0;

    } else {
        fprintf(stderr,
                "Error: Failed to find project root file %s.\n",
                Context->ProjectFileName);

        Status = errno;
        if (Status == 0) {
            Status = EINVAL;
        }

        goto SetupRootDirectoriesEnd;
    }

    //
    // Also get the absolute path of the build directory.
    //

    if (chdir(Context->BuildRoot) != 0) {
        Status = errno;
        fprintf(stderr,
                "Error: Invalid build root directory %s: %s.\n",
                Context->BuildRoot,
                strerror(Status));

        return Status;
    }

    CurrentDirectory = getcwd(NULL, 0);
    if (CurrentDirectory == NULL) {
        return errno;
    }

    free(Context->BuildRoot);
    Context->BuildRoot = CurrentDirectory;
    Status = 0;

SetupRootDirectoriesEnd:
    return Status;
}

PSTR
MbgenPathForTree (
    PMBGEN_CONTEXT Context,
    MBGEN_DIRECTORY_TREE Tree
    )

/*++

Routine Description:

    This routine returns the path for the given tree root.

Arguments:

    Context - Supplies a pointer to the context.

    Tree - Supplies the root to return.

Return Value:

    Returns the path of the given tree. The caller does not own this memory.

--*/

{

    if (Tree == MbgenSourceTree) {
        return Context->SourceRoot;

    } else if (Tree == MbgenBuildTree) {
        return Context->BuildRoot;
    }

    assert(Tree == MbgenAbsolutePath);

    return "/";
}

//
// --------------------------------------------------------- Internal Functions
//

