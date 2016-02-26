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

INT
MbgenParseTargetSpecifier (
    PMBGEN_CONTEXT Context,
    PSTR Name,
    MBGEN_DIRECTORY_TREE RelativeTree,
    PSTR RelativePath,
    PMBGEN_TARGET_SPECIFIER Target
    )

/*++

Routine Description:

    This routine breaks a target specifier string down into its components.

Arguments:

    Context - Supplies a pointer to the application context.

    Name - Supplies a pointer to the name string.

    RelativeTree - Supplies the tree type (usually source or build) that
        relative paths are rooted against.

    RelativePath - Supplies a pointer to the path to prepend to relative paths.

    Target - Supplies a pointer where the target will be returned on success.
        The caller will be responsible for freeing the string buffers.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR Colon;
    INT Status;

    assert(Target->Path == NULL);
    assert(Target->Target == NULL);

    Target->Path = NULL;
    if (MBGEN_IS_SOURCE_ROOT_RELATIVE(Name)) {
        Name += 2;
        Target->Root = MbgenSourceTree;

    } else if (MBGEN_IS_BUILD_ROOT_RELATIVE(Name)) {
        Name += 2;
        Target->Root = MbgenBuildTree;

    } else if (*Name == '/') {
        Target->Root = MbgenAbsolutePath;
        Name += 1;

    } else {

        //
        // A circumflex identifies the path as the opposite of its default.
        //

        while (*Name == '^') {
            Name += 1;
            if (RelativeTree == MbgenSourceTree) {
                RelativeTree = MbgenBuildTree;

            } else {

                assert(RelativeTree == MbgenBuildTree);

                RelativeTree = MbgenSourceTree;
            }
        }

        Target->Root = RelativeTree;
        Target->Path = MbgenAppendPaths(RelativePath, Name);
        if (Target->Path == NULL) {
            Status = ENOMEM;
            goto ParseTargetSpecifierEnd;
        }
    }

    if (Target->Path == NULL) {
        Target->Path = strdup(Name);
        if (Target->Path == NULL) {
            Status = ENOMEM;
            goto ParseTargetSpecifierEnd;
        }
    }

    Target->Target = NULL;
    Colon = strrchr(Target->Path, ':');
    if (Colon != NULL) {
        *Colon = '\0';
        if (Colon[1] != '\0') {
            Target->Target = Colon + 1;
        }
    }

    Status = 0;

ParseTargetSpecifierEnd:
    return Status;
}

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

    } else if (Path2 == NULL) {
        return strdup(Path1);
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
    PSTR Start;
    int Status;

    Start = getcwd(NULL, 0);
    if (Start == NULL) {
        return errno;
    }

    //
    // Get the absolute path of the build directory.
    //

    if (chdir(Context->BuildRoot) != 0) {
        Status = errno;
        fprintf(stderr,
                "Error: Invalid build root directory %s: %s.\n",
                Context->BuildRoot,
                strerror(Status));

        goto SetupRootDirectoriesEnd;
    }

    CurrentDirectory = getcwd(NULL, 0);
    if (CurrentDirectory == NULL) {
        Status = errno;
        goto SetupRootDirectoriesEnd;
    }

    assert(Context->BuildRoot != NULL);

    free(Context->BuildRoot);
    Context->BuildRoot = CurrentDirectory;

    //
    // Make sure it exists if it was specified by the user.
    //

    chdir(Start);
    if (Context->SourceRoot != NULL) {
        if (chdir(Context->SourceRoot) != 0) {
            Status = errno;
            fprintf(stderr,
                    "Error: Invalid source root directory %s: %s.\n",
                    Context->SourceRoot,
                    strerror(Status));

            goto SetupRootDirectoriesEnd;
        }

        //
        // Convert it to an absolute path.
        //

        SourceRoot = getcwd(NULL, 0);
        if (SourceRoot == NULL) {
            Status = errno;
            goto SetupRootDirectoriesEnd;
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

    Status = 0;

SetupRootDirectoriesEnd:
    if (Start != NULL) {
        free(Start);
    }

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

PSTR
MbgenSplitExtension (
    PSTR Path,
    PSTR *Extension
    )

/*++

Routine Description:

    This routine splits the extension portion off the end of a file path.

Arguments:

    Path - Supplies a pointer to the path to split.

    Extension - Supplies a pointer where a pointer to the extension will be
        returned on success. This memory will be part of the return value
        allocation, and does not need to be explicitly freed. This returns NULL
        if the path contains no extension or is a directory (ends in a slash).

Return Value:

    Returns a copy of the string, chopped before the last period. It is the
    caller's responsibility to free this memory.

    NULL on allocation failure.

--*/

{

    PSTR Copy;
    PSTR Dot;

    *Extension = NULL;
    Copy = strdup(Path);
    if (Copy == NULL) {
        return NULL;
    }

    Dot = strrchr(Copy, '.');
    if (Dot != NULL) {

        //
        // Make sure there are no path separators after the last dot.
        //

        if ((strchr(Dot, '/') == NULL) && (strchr(Dot, '\\') == NULL)) {
            *Dot = '\0';
            *Extension = Dot + 1;
        }
    }

    return Copy;
}

//
// --------------------------------------------------------- Internal Functions
//

