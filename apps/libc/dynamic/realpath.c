/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    realpath.c

Abstract:

    This module implements support for the realpath function.

Author:

    Evan Green 9-Mar-2015

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

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
char *
realpath (
    const char *Path,
    char *ResolvedPath
    )

/*++

Routine Description:

    This routine returns the canonical path for the given file path. This
    canonical path will include no '.' or '..' components, and will not
    contain symbolic links in any components of the path. All path components
    must exist.

Arguments:

    Path - Supplies a pointer to the path to canonicalize.

    ResolvedPath - Supplies an optional pointer to the buffer to place the
        resolved path in. This must be at least PATH_MAX bytes.

Return Value:

    Returns a pointer to the resolved path on success.

    NULL on failure.

--*/

{

    PSTR AllocatedPath;
    PSTR AppendedLink;
    UINTN ComponentSize;
    PSTR Destination;
    PSTR End;
    UINTN EndLength;
    PSTR Link;
    UINTN LinkCount;
    ssize_t LinkSize;
    PVOID NewBuffer;
    size_t NewSize;
    long PathMax;
    PSTR ResolvedPathEnd;
    PSTR Start;
    struct stat Stat;
    int Status;

    AllocatedPath = NULL;
    AppendedLink = NULL;
    Link = NULL;
    LinkCount = 0;
    if (Path == NULL) {
        errno = EINVAL;
        return NULL;
    }

    if (*Path == '\0') {
        errno = ENOENT;
        return NULL;
    }

    PathMax = PATH_MAX;
    if (ResolvedPath == NULL) {
        AllocatedPath = malloc(PathMax);
        if (AllocatedPath == NULL) {
            return NULL;
        }

        ResolvedPath = AllocatedPath;
    }

    ResolvedPathEnd = ResolvedPath + PathMax;

    //
    // Add the current working directory if this is a relative path.
    //

    if (Path[0] != '/') {
        if (getcwd(ResolvedPath, PathMax) == NULL) {
            Status = errno;
            goto realpathEnd;
        }

        Destination = memchr(ResolvedPath, '\0', PathMax);

    } else {
        ResolvedPath[0] = '/';
        Destination = ResolvedPath + 1;
    }

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
            if (*(Destination - 1) != '/') {
                *Destination = '/';
                Destination += 1;
            }

            //
            // Handle the component being too big, needing reallocation.
            //

            if (Destination + ComponentSize >= ResolvedPathEnd) {

                //
                // If the buffer was handed in, there is no reallocation.
                //

                if (AllocatedPath == NULL) {
                    Status = ENAMETOOLONG;
                    if (Destination > ResolvedPath + 1) {
                        Destination -= 1;
                    }

                    *Destination = '\0';
                    goto realpathEnd;
                }

                NewSize = ResolvedPathEnd - ResolvedPath;
                if (ComponentSize + 1 > PathMax) {
                    NewSize += ComponentSize + 1;

                } else {
                    NewSize += PathMax;
                }

                NewBuffer = realloc(AllocatedPath, NewSize);
                if (NewBuffer == NULL) {
                    Status = errno;
                    goto realpathEnd;
                }

                Destination = NewBuffer + (Destination - ResolvedPath);
                AllocatedPath = NewBuffer;
                ResolvedPath = NewBuffer;
                ResolvedPathEnd = ResolvedPath + NewSize;
            }

            memcpy(Destination, Start, ComponentSize);
            Destination += ComponentSize;
            *Destination = '\0';
            if (lstat(ResolvedPath, &Stat) < 0) {
                Status = errno;
                goto realpathEnd;
            }

            //
            // Follow symbolic links.
            //

            if (S_ISLNK(Stat.st_mode)) {
                if (Link == NULL) {
                    Link = malloc(PathMax);
                    if (Link == NULL) {
                        Status = errno;
                        goto realpathEnd;
                    }
                }

                LinkCount += 1;
                if (LinkCount > MAXSYMLINKS) {
                    Status = ELOOP;
                    goto realpathEnd;
                }

                LinkSize = readlink(ResolvedPath, Link, PathMax - 1);
                if (LinkSize < 0) {
                    Status = errno;
                    goto realpathEnd;
                }

                Link[LinkSize] = '\0';

                //
                // Create another buffer containing the concatenation of the
                // link destination and the rest of the path string.
                //

                if (AppendedLink == NULL) {
                    AppendedLink = malloc(PathMax);
                    if (AppendedLink == NULL) {
                        Status = errno;
                        goto realpathEnd;
                    }
                }

                EndLength = strlen(End);
                if ((EndLength + LinkSize) >= PathMax) {
                    Status = ENAMETOOLONG;
                    goto realpathEnd;
                }

                //
                // The loop may have already been through here, which means
                // the end buffer may already point inside this buffer. Hence
                // the need for the more delicate memmove (and the need to do
                // it before copying the link over the beginning part).
                //

                memmove(&(AppendedLink[LinkSize]), End, EndLength + 1);
                memcpy(AppendedLink, Link, LinkSize);
                Path = AppendedLink;
                End = AppendedLink;

                //
                // If it's an absolute link, start there.
                //

                if (Link[0] == '/') {
                    Destination = ResolvedPath + 1;

                //
                // If it's a relative link, back up a component.
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

            } else if ((!S_ISDIR(Stat.st_mode)) && (*End != '\0')) {
                Status = ENOTDIR;
                goto realpathEnd;
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

realpathEnd:
    if (Link != NULL) {
        free(Link);
    }

    if (AppendedLink != NULL) {
        free(AppendedLink);
    }

    assert((ResolvedPath != NULL) &&
           ((AllocatedPath == NULL) || (ResolvedPath == AllocatedPath)));

    if (Status != 0) {
        errno = Status;
        if (AllocatedPath != NULL) {
            free(AllocatedPath);
            AllocatedPath = NULL;
        }

        ResolvedPath = NULL;
    }

    return ResolvedPath;
}

//
// --------------------------------------------------------- Internal Functions
//

