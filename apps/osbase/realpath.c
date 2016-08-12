/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    realpath.c

Abstract:

    This module determines the real path of the given path, removing all
    dot, dot dot, and symbolic link components.

Author:

    Evan Green 11-Aug-2016

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osbasep.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OS_REAL_PATH_ALLOCATION_TAG 0x7052734F

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

OS_API
KSTATUS
OsGetRealPath (
    PCSTR Path,
    PSTR *RealPath
    )

/*++

Routine Description:

    This routine returns the canonical path for the given file path. This
    canonical path will include no '.' or '..' components, and will not
    contain symbolic links in any components of the path. All path components
    must exist.

Arguments:

    Path - Supplies a pointer to the path to convert.

    RealPath - Supplies a pointer where the final real path will be returned.
        The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PSTR AppendedLink;
    UINTN ComponentSize;
    PSTR CurrentDirectory;
    UINTN CurrentDirectorySize;
    PSTR Destination;
    PSTR End;
    UINTN EndLength;
    FILE_PROPERTIES FileProperties;
    PSTR Link;
    UINTN LinkCount;
    UINTN LinkSize;
    PVOID NewBuffer;
    UINTN NewSize;
    UINTN PathSize;
    PSTR ResolvedPath;
    PSTR ResolvedPathEnd;
    PSTR Start;
    KSTATUS Status;

    AppendedLink = NULL;
    Link = NULL;
    LinkCount = 0;
    LinkSize = 0;
    *RealPath = NULL;
    ResolvedPath = NULL;
    if (Path == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    if (*Path == '\0') {
        return STATUS_PATH_NOT_FOUND;
    }

    //
    // Add the current working directory if this is a relative path.
    //

    PathSize = RtlStringLength(Path);
    if (Path[0] != '/') {
        Status = OsGetCurrentDirectory(FALSE,
                                       &CurrentDirectory,
                                       &CurrentDirectorySize);

        if (!KSUCCESS(Status)) {
            goto GetRealPathEnd;
        }

        CurrentDirectorySize = RtlStringLength(CurrentDirectory);
        NewSize = CurrentDirectorySize + PathSize + 2;
        ResolvedPath = OsHeapAllocate(NewSize, OS_REAL_PATH_ALLOCATION_TAG);
        if (ResolvedPath == NULL) {
            OsHeapFree(CurrentDirectory);
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetRealPathEnd;
        }

        RtlCopyMemory(ResolvedPath, CurrentDirectory, CurrentDirectorySize);
        ResolvedPath[CurrentDirectorySize] = '/';
        Destination = ResolvedPath + CurrentDirectorySize + 1;
        OsHeapFree(CurrentDirectory);

    } else {
        NewSize = PathSize + 1;
        ResolvedPath = OsHeapAllocate(NewSize, OS_REAL_PATH_ALLOCATION_TAG);
        if (ResolvedPath == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetRealPathEnd;
        }

        ResolvedPath[0] = '/';
        Destination = ResolvedPath + 1;
    }

    ResolvedPathEnd = ResolvedPath + NewSize;
    Start = (PSTR)Path;
    while (*Start != '\0') {

        //
        // Skip leading separators.
        //

        while (*Start == '/') {
            Start += 1;
        }

        End = Start;
        while ((*End != '\0') && (*End != '/')) {
            End += 1;
        }

        ComponentSize = End - Start;
        if (ComponentSize == 0) {
            break;
        }

        //
        // For dot dot, back up to the previous component.
        //

        if ((ComponentSize == 2) && (Start[0] == '.') && (Start[1] == '.')) {
            if (Destination > ResolvedPath + 1) {
                Destination -= 1;
                while (*(Destination - 1) != '/') {
                    Destination -= 1;
                }
            }

        //
        // If it's just a dot, do nothing. Otherwise, it's a component.
        //

        } else if (!((ComponentSize == 1) && (Start[0] == '.'))) {

            //
            // Handle the component being too big, needing reallocation.
            //

            if (Destination + ComponentSize + 2 >= ResolvedPathEnd) {
                NewSize = (ResolvedPathEnd - ResolvedPath) + ComponentSize + 2;
                NewBuffer = OsHeapReallocate(ResolvedPath,
                                             NewSize,
                                             OS_REAL_PATH_ALLOCATION_TAG);

                if (NewBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto GetRealPathEnd;
                }

                Destination = NewBuffer + (Destination - ResolvedPath);
                ResolvedPath = NewBuffer;
                ResolvedPathEnd = ResolvedPath + NewSize;
            }

            if (*(Destination - 1) != '/') {
                *Destination = '/';
                Destination += 1;
            }

            RtlCopyMemory(Destination, Start, ComponentSize);
            Destination += ComponentSize;
            *Destination = '\0';
            Status = OsGetFileInformation(INVALID_HANDLE,
                                          ResolvedPath,
                                          Destination - ResolvedPath + 1,
                                          FALSE,
                                          &FileProperties);

            if (!KSUCCESS(Status)) {
                goto GetRealPathEnd;
            }

            //
            // Follow symbolic links.
            //

            if (FileProperties.Type == IoObjectSymbolicLink) {
                Status = OsReadSymbolicLink(INVALID_HANDLE,
                                            ResolvedPath,
                                            Destination - ResolvedPath + 1,
                                            Link,
                                            LinkSize,
                                            &LinkSize);

                if (Status == STATUS_BUFFER_TOO_SMALL) {
                    NewBuffer = OsHeapReallocate(Link,
                                                 LinkSize + 1,
                                                 OS_REAL_PATH_ALLOCATION_TAG);

                    if (NewBuffer == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto GetRealPathEnd;
                    }

                    Link = NewBuffer;
                    Status = OsReadSymbolicLink(INVALID_HANDLE,
                                                ResolvedPath,
                                                Destination - ResolvedPath + 1,
                                                Link,
                                                LinkSize,
                                                &LinkSize);

                    if (!KSUCCESS(Status)) {
                        goto GetRealPathEnd;
                    }

                } else if (!KSUCCESS(Status)) {
                    goto GetRealPathEnd;
                }

                Link[LinkSize] = '\0';
                LinkCount += 1;
                if (LinkCount > MAX_SYMBOLIC_LINK_RECURSION) {
                    Status = STATUS_SYMBOLIC_LINK_LOOP;
                    goto GetRealPathEnd;
                }

                //
                // Create another buffer containing the concatenation of the
                // link destination and the rest of the path string.
                //

                EndLength = RtlStringLength(End);
                NewBuffer = OsHeapAllocate(LinkSize + EndLength + 1,
                                           OS_REAL_PATH_ALLOCATION_TAG);

                if (NewBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto GetRealPathEnd;
                }

                RtlCopyMemory(NewBuffer, Link, LinkSize);
                RtlCopyMemory(NewBuffer + LinkSize, End, EndLength + 1);
                if (AppendedLink != NULL) {
                    OsHeapFree(AppendedLink);
                }

                AppendedLink = NewBuffer;
                Path = AppendedLink;
                End = AppendedLink;

                //
                // If it's an absolute link, reset the destination.
                //

                if (Link[0] == '/') {
                    Destination = ResolvedPath + 1;

                //
                // If it's a relative link, back the destination up a component.
                //

                } else {
                    if (Destination > ResolvedPath + 1) {
                        Destination -= 1;
                        while (*(Destination - 1) != '/') {
                            Destination -= 1;
                        }
                    }
                }

            //
            // Fail if it's not the final component and it's not a directory.
            // This also catches paths that end in a slash, enforcing that they
            // must also be directories.
            //

            } else if ((FileProperties.Type != IoObjectRegularDirectory) &&
                       (FileProperties.Type != IoObjectObjectDirectory) &&
                       (*End != '\0')) {

                Status = STATUS_NOT_A_DIRECTORY;
                goto GetRealPathEnd;
            }
        }

        //
        // Move to the next component.
        //

        Start = End;
    }

    //
    // Remove a trailing slash.
    //

    if ((Destination > ResolvedPath + 1) && (*(Destination - 1) == '/')) {
        Destination -= 1;
    }

    *Destination = '\0';
    Status = 0;

GetRealPathEnd:
    if (Link != NULL) {
        OsHeapFree(Link);
    }

    if (AppendedLink != NULL) {
        OsHeapFree(AppendedLink);
    }

    if (!KSUCCESS(Status)) {
        if (ResolvedPath != NULL) {
            OsHeapFree(ResolvedPath);
            ResolvedPath = NULL;
        }
    }

    *RealPath = ResolvedPath;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

