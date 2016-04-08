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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mbgen.h"

//
// --------------------------------------------------------------------- Macros
//

#ifdef _WIN32

#define mkdir(_Path, _Permissions) mkdir(_Path)

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

int
MingenComparePaths (
    const void *LeftPointer,
    const void *RightPointer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
MingenParsePath (
    PMINGEN_CONTEXT Context,
    PSTR Name,
    MINGEN_DIRECTORY_TREE RelativeTree,
    PSTR RelativePath,
    PMINGEN_PATH Target
    )

/*++

Routine Description:

    This routine breaks a mingen path string into its components.

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

    memset(Target, 0, sizeof(MINGEN_PATH));
    if (MINGEN_IS_SOURCE_ROOT_RELATIVE(Name)) {
        Name += 2;
        Target->Root = MingenSourceTree;

    } else if (MINGEN_IS_BUILD_ROOT_RELATIVE(Name)) {
        Name += 2;
        Target->Root = MingenBuildTree;

    } else if (MINGEN_IS_ABSOLUTE_PATH(Name)) {
        Target->Root = MingenAbsolutePath;

    } else {

        //
        // A circumflex identifies the path as the opposite of its default.
        //

        while (*Name == '^') {
            Name += 1;
            if (RelativeTree == MingenSourceTree) {
                RelativeTree = MingenBuildTree;

            } else {

                assert(RelativeTree == MingenBuildTree);

                RelativeTree = MingenSourceTree;
            }
        }

        Target->Root = RelativeTree;
        Target->Path = MingenAppendPaths(RelativePath, Name);
        if (Target->Path == NULL) {
            Status = ENOMEM;
            goto ParsePathEnd;
        }
    }

    if (Target->Path == NULL) {
        Target->Path = strdup(Name);
        if (Target->Path == NULL) {
            Status = ENOMEM;
            goto ParsePathEnd;
        }
    }

    Target->Target = NULL;
    Colon = strrchr(Target->Path, ':');
    if (Colon != NULL) {
        *Colon = '\0';
        Target->Target = Colon + 1;

        //
        // Remove trailing slashes.
        //

        while ((Colon != Target->Path) &&
               ((*(Colon - 1) == '/') || (*(Colon - 1) == '\\'))) {

            Colon -= 1;
            *Colon = '\0';
        }
    }

    Status = 0;

ParsePathEnd:
    return Status;
}

PSTR
MingenAppendPaths3 (
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

    Intermediate = MingenAppendPaths(Path1, Path2);
    if (Intermediate == NULL) {
        return NULL;
    }

    Result = MingenAppendPaths(Intermediate, Path3);
    free(Intermediate);
    return Result;
}

PSTR
MingenAppendPaths (
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
MingenFindProjectFile (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine finds the top level project file.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentDirectory;
    int Descriptor;
    PSTR FileName;
    PSTR PreviousDirectory;
    PSTR Start;
    int Status;

    FileName = MINGEN_PROJECT_FILE;
    Start = getcwd(NULL, 0);
    if (Start == NULL) {
        return errno;
    }

    //
    // Attempt to open a project root file in each directory.
    //

    PreviousDirectory = NULL;
    while (TRUE) {
        CurrentDirectory = MingenGetAbsoluteDirectory(".");
        if (CurrentDirectory == NULL) {
            break;
        }

        Descriptor = open(FileName, O_RDONLY);
        if (Descriptor >= 0) {
            close(Descriptor);
            Context->ProjectFilePath = MingenAppendPaths(CurrentDirectory,
                                                        FileName);

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

    if (CurrentDirectory != NULL) {
        free(CurrentDirectory);
    }

    if (Context->ProjectFilePath != NULL) {
        errno = 0;

    } else {
        fprintf(stderr,
                "Error: Failed to find project root file %s in the current "
                "directory or any parent directory.\n",
                FileName);

        Status = errno;
        if (Status == 0) {
            Status = ENOENT;
        }

        goto SetupRootDirectoriesEnd;
    }

    Status = 0;

SetupRootDirectoriesEnd:
    if (Start != NULL) {
        chdir(Start);
        free(Start);
    }

    return Status;
}

INT
MingenFindSourceRoot (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine nails down the source root directory.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedPath;
    PSTR ProjectDirectory;
    PSTR ProjectFile;
    PSTR ProjectFileName;
    INT Status;

    ProjectDirectory = NULL;
    ProjectFile = strdup(Context->ProjectFilePath);
    if (ProjectFile == NULL) {
        Status = ENOMEM;
        goto FindSourceRootEnd;
    }

    MingenSplitPath(ProjectFile, &ProjectDirectory, &ProjectFileName);
    ProjectDirectory = MingenGetAbsoluteDirectory(ProjectDirectory);
    if (ProjectDirectory == NULL) {
        Status = errno;
        if (Status == 0) {
            Status = -1;
        }

        goto FindSourceRootEnd;
    }

    //
    // If no source directory was specified in the project file, then the
    // default is that the directory the project file was in is the source root.
    //

    if (Context->SourceRoot == NULL) {
        Context->SourceRoot = ProjectDirectory;
        ProjectDirectory = NULL;
        Status = 0;
        goto FindSourceRootEnd;
    }

    //
    // If the source path specified in the project file is absolute (not
    // recommended), then leave it alone.
    //

    if (MINGEN_IS_ABSOLUTE_PATH(Context->SourceRoot)) {
        Status = 0;
        goto FindSourceRootEnd;
    }

    AppendedPath = MingenAppendPaths(ProjectDirectory, Context->SourceRoot);
    if (AppendedPath == NULL) {
        Status = ENOMEM;
        goto FindSourceRootEnd;
    }

    free(ProjectDirectory);
    ProjectDirectory = MingenGetAbsoluteDirectory(AppendedPath);
    free(AppendedPath);
    if (ProjectDirectory == NULL) {
        Status = errno;
        if (Status == 0) {
            Status = -1;
        }

        goto FindSourceRootEnd;
    }

    free(Context->SourceRoot);
    Context->SourceRoot = ProjectDirectory;
    ProjectDirectory = NULL;
    Status = 0;

FindSourceRootEnd:
    if (ProjectFile != NULL) {
        free(ProjectFile);
    }

    if (ProjectDirectory != NULL) {
        free(ProjectDirectory);
    }

    return Status;
}

PSTR
MingenPathForTree (
    PMINGEN_CONTEXT Context,
    MINGEN_DIRECTORY_TREE Tree
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

    if (Tree == MingenSourceTree) {
        return Context->SourceRoot;

    } else if (Tree == MingenBuildTree) {
        return Context->BuildRoot;
    }

    assert(Tree == MingenAbsolutePath);

    return "";
}

PSTR
MingenSplitExtension (
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

VOID
MingenSplitPath (
    PSTR Path,
    PSTR *DirectoryName,
    PSTR *FileName
    )

/*++

Routine Description:

    This routine splits the directory and the file portion of a path.

Arguments:

    Path - Supplies a pointer to the path to split in line.

    DirectoryName - Supplies an optional pointer where the directory portion
        will be returned. This may be a pointer within the path or a static
        string.

    FileName - Supplies an optional pointer where the file name portion will be
        returned.

Return Value:

    None.

--*/

{

    PSTR Current;
    PSTR Directory;
    PSTR File;

    Current = Path + strlen(Path);
    if (Current == Path) {
        File = Path;
        Directory = ".";
        goto SplitPathEnd;
    }

    //
    // Back up to the first slash. If there wasn't one, the whole thing is a
    // file.
    //

    Current -= 1;
    while ((Current != Path) && (*Current != '/') && (*Current != '\\')) {
        Current -= 1;
    }

    if ((*Current != '/') && (*Current != '\\')) {
        File = Path;
        Directory = ".";
        goto SplitPathEnd;
    }

    File = Current + 1;

    //
    // Get to the first slash.
    //

    while ((Current != Path) && ((*Current == '/') || (*Current == '\\'))) {
        Current -= 1;
    }

    //
    // Either it's not at the beginnin, it's at the beginning and there's a
    // slash, or it's at the beginning and there's no slash. So if there's no
    // slash, then there must be some valid portion of the path from the
    // beginning, so return the path.
    //

    if ((*Current != '/') && (*Current != '\\')) {
        *(Current + 1) = '\0';
        Directory = Path;

    //
    // Otherwise, it was slashes all the way to the beginning, so start from
    // slash. Truncate the path in case the caller needs the directory that way.
    //

    } else {
        *Current = '\0';
        Directory = "/";
    }

SplitPathEnd:
    if (FileName != NULL) {
        *FileName = File;
    }

    if (DirectoryName != NULL) {
        *DirectoryName = Directory;
    }

    return;
}

INT
MingenAddPathToList (
    PMINGEN_PATH_LIST PathList,
    PMINGEN_PATH Path
    )

/*++

Routine Description:

    This routine adds a path to the path list.

Arguments:

    PathList - Supplies a pointer to the path list to add to.

    Path - Supplies a pointer to the path to add.

Return Value:

    0 on success.

    ENOMEM on allocation failure.

--*/

{

    PMINGEN_PATH Destination;
    PVOID NewBuffer;
    UINTN NewCapacity;

    if (PathList->Count >= PathList->Capacity) {

        assert(PathList->Count == PathList->Capacity);

        NewCapacity = PathList->Capacity * 2;
        if (NewCapacity == 0) {
            NewCapacity = 16;
        }

        NewBuffer = realloc(PathList->Array, NewCapacity * sizeof(MINGEN_PATH));
        if (NewBuffer == NULL) {
            return ENOMEM;
        }

        PathList->Array = NewBuffer;
        PathList->Capacity = NewCapacity;
    }

    Destination = &(PathList->Array[PathList->Count]);
    memset(Destination, 0, sizeof(MINGEN_PATH));
    Destination->Root = Path->Root;
    if (Path->Path != NULL) {
        Destination->Path = strdup(Path->Path);
        if (Destination->Path == NULL) {
            return ENOMEM;
        }
    }

    Destination->Target = NULL;
    PathList->Count += 1;
    return 0;
}

VOID
MingenDestroyPathList (
    PMINGEN_PATH_LIST PathList
    )

/*++

Routine Description:

    This routine destroys a path list, freeing all entries.

Arguments:

    PathList - Supplies a pointer to the path list.

Return Value:

    None.

--*/

{

    ULONG Index;
    PMINGEN_PATH Path;

    for (Index = 0; Index < PathList->Count; Index += 1) {
        Path = &(PathList->Array[Index]);

        assert(Path->Target == NULL);

        if (Path->Path != NULL) {
            free(Path->Path);
            Path->Path = NULL;
        }
    }

    if (PathList->Array != NULL) {
        free(PathList->Array);
    }

    PathList->Count = 0;
    PathList->Capacity = 0;
    return;
}

VOID
MingenDeduplicatePathList (
    PMINGEN_PATH_LIST PathList
    )

/*++

Routine Description:

    This routine sorts and deduplicates a path list.

Arguments:

    PathList - Supplies a pointer to the path list.

Return Value:

    None.

--*/

{

    int Compare;
    PMINGEN_PATH Entry;
    UINTN Index;

    //
    // Sort the paths.
    //

    qsort(PathList->Array,
          PathList->Count,
          sizeof(MINGEN_PATH),
          MingenComparePaths);

    Index = 0;
    while (Index + 1 < PathList->Count) {
        Entry = &(PathList->Array[Index]);

        //
        // Compare this entry with the next one. If they're the same, copy
        // the remainder down on top of the next one, and keep checking at this
        // index for even more duplicates.
        //

        Compare = MingenComparePaths(Entry, Entry + 1);
        if (Compare == 0) {
            Entry += 1;
            free(Entry->Path);

            assert(Entry->Target == NULL);

            memmove(Entry,
                    Entry + 1,
                    (PathList->Count - (Index + 2)) * sizeof(MINGEN_PATH));

            PathList->Count -= 1;

        //
        // They are not equal, move to the next one.
        //

        } else {
            Index += 1;
        }
    }

    return;
}

INT
MingenCreateDirectories (
    PMINGEN_CONTEXT Context,
    PMINGEN_PATH_LIST PathList
    )

/*++

Routine Description:

    This routine creates directories, including intermediate directories,
    in the given path list. If creating one directory fails, the routine still
    tries to create the others.

Arguments:

    Context - Supplies a pointer to the application context.

    PathList - Supplies a pointer to the path list.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN Index;
    PMINGEN_PATH Path;
    INT Status;
    INT TotalStatus;
    PSTR TreeRoot;

    TotalStatus = 0;
    for (Index = 0; Index < PathList->Count; Index += 1) {
        Path = &(PathList->Array[Index]);
        TreeRoot = MingenPathForTree(Context, Path->Root);
        Status = chdir(TreeRoot);
        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to cd to %s: %s.\n",
                    TreeRoot,
                    strerror(errno));

        } else {
            Status = MingenCreateDirectory(Path->Path);
        }

        if ((Status != 0) && (TotalStatus == 0)) {
            TotalStatus = Status;
        }
    }

    return TotalStatus;
}

INT
MingenCreateDirectory (
    PSTR Path
    )

/*++

Routine Description:

    This routine creates a directory, including intermediate directories.

Arguments:

    Path - Supplies a pointer to the directory to create.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR CurrentSeparator;
    PSTR PathCopy;
    int Result;

    //
    // Create a copy of the path so it can be modified.
    //

    PathCopy = strdup(Path);
    if (PathCopy == NULL) {
        Result = ENOMEM;
        goto CreateDirectoryEnd;
    }

    CurrentSeparator = PathCopy;

    //
    // Skip the drive letter if it exists.
    //

    if ((isalpha(PathCopy[0]) != 0) &&
        (PathCopy[1] == ':') &&
        ((PathCopy[2] == '/') || (PathCopy[2] == '\\'))) {

        CurrentSeparator += 3;
    }

    //
    // Skip the pointless system call to try to create the root directory.
    //

    while ((*CurrentSeparator == '/') || (*CurrentSeparator == '\\')) {
        CurrentSeparator += 1;
    }

    //
    // Loop creating each component of the directory.
    //

    while (TRUE) {
        errno = 0;
        if (*CurrentSeparator == '\0') {
            break;
        }

        while ((*CurrentSeparator != '/') && (*CurrentSeparator != '\\') &&
               (*CurrentSeparator != '\0')) {

            CurrentSeparator += 1;
        }

        if (*CurrentSeparator == '\0') {
            CurrentSeparator = NULL;
        }

        //
        // If there's a separator, create the intermediate directory.
        //

        if (CurrentSeparator != NULL) {
            *CurrentSeparator = '\0';
            Result = mkdir(PathCopy, 0777);
            if ((Result == -1) && (errno != EEXIST)) {
                perror("Error: Failed to create directory");
                break;
            }

            *CurrentSeparator = '/';
            while (*CurrentSeparator == '/') {
                CurrentSeparator += 1;
            }

        //
        // This is the final component. If creating intermediate directories,
        // don't fail if it already exists.
        //

        } else {
            Result = mkdir(PathCopy, 0777);
            if ((Result != 0) && (errno != EEXIST)) {
                perror("Error: Failed to create directory");
            }

            errno = 0;
            break;
        }
    }

    Result = errno;

CreateDirectoryEnd:
    if (PathCopy != NULL) {
        free(PathCopy);
    }

    return Result;
}

PSTR
MingenGetAbsoluteDirectory (
    PSTR Path
    )

/*++

Routine Description:

    This routine converts the given path into an absolute path by changing to
    that directory.

Arguments:

    Path - Supplies a pointer to the directory path.

Return Value:

    Returns the absolute path of the given directory. It is the caller's
    responsibility to free this memory.

    NULL on allocation failure, or if the directory does not exist.

--*/

{

    PSTR Current;
    PSTR Directory;
    PSTR Original;

    Original = getcwd(NULL, 0);
    if (Original == NULL) {
        return NULL;
    }

    Directory = NULL;
    if (chdir(Path) != 0) {
        fprintf(stderr,
                "Error: Invalid directory %s: %s.\n",
                Path,
                strerror(errno));

        goto GetAbsoluteDirectoryEnd;
    }

    Directory = getcwd(NULL, 0);
    if (Directory == NULL) {
        goto GetAbsoluteDirectoryEnd;
    }

    //
    // Convert backslashes to forward slashes, unless told not to.
    //

    if (getenv("MINGEN_NO_SLASH_CONVERSION") != NULL) {
        goto GetAbsoluteDirectoryEnd;
    }

    Current = Directory;
    while (*Current != '\0') {
        if (*Current == '\\') {
            *Current = '/';
        }

        Current += 1;
    }

GetAbsoluteDirectoryEnd:
    chdir(Original);
    return Directory;
}

//
// --------------------------------------------------------- Internal Functions
//

int
MingenComparePaths (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares two MINGEN_PATH structures and orders them.

Arguments:

    LeftPointer - Supplies a pointer to the left path.

    RightPointer - Supplies a pointer to the right path.

Return Value:

    < 0 if Left < Right.

    0 if they are equal.

    > 0 if Left > Right.

--*/

{

    PMINGEN_PATH Left;
    PMINGEN_PATH Right;

    Left = (PMINGEN_PATH)LeftPointer;
    Right = (PMINGEN_PATH)RightPointer;
    if (Left->Root < Right->Root) {
        return -1;
    }

    if (Left->Root > Right->Root) {
        return 1;
    }

    assert((Left->Target == NULL) && (Right->Target == NULL));

    return strcmp(Left->Path, Right->Path);
}

