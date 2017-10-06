/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    glob.h

Abstract:

    This header contains definitions for glob functions, which allow expanding
    of a pattern to valid paths.

Author:

    Evan Green 10-Feb-2015

--*/

#ifndef _GLOB_H
#define _GLOB_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <dirent.h>
#include <sys/stat.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define flags that can be passed to the glob function.
//

//
// Set this flag to return on read errors.
//

#define GLOB_ERR 0x00000001

//
// Set this flag to append a slash to each name.
//

#define GLOB_MARK 0x00000002

//
// Set this flag to skip sorting the results.
//

#define GLOB_NOSORT 0x00000004

//
// Set this flag to insert NULL array slots, the number of which is specified
// by the gl_offs member.
//

#define GLOB_DOOFFS 0x00000008

//
// Set this flag to return the pattern itself if nothing matches the pattern.
//

#define GLOB_NOCHECK 0x00000010

//
// Set this flag to append the results to a previous call to glob.
//

#define GLOB_APPEND 0x00000020

//
// Set this flag to indicate that backslashes aren't escape characters.
//

#define GLOB_NOESCAPE 0x00000040

//
// Set this flag to indicate that leading periods can be matched by wildcards.
//

#define GLOB_PERIOD 0x00000080

//
// This flag is set if any wildcard characters were seen.
//

#define GLOB_MAGCHAR 0x00000100

//
// Set this flag to use the alternate function pointers in the glob_t structure.
//

#define GLOB_ALTDIRFUNC 0x00000200

//
// Set this flag to expand brace options.
//

#define GLOB_BRACE 0x00000400

//
// Set this flag to simply return the pattern if there were no wildcards.
//

#define GLOB_NOMAGIC 0x00000800

//
// Set this flag to enable expanding of ~user to their home directory.
//

#define GLOB_TILDE 0x00001000

//
// Set this flag to match only directories.
//

#define GLOB_ONLYDIR 0x00002000

//
// Set this flag to enable the same thing as GLOB_TILDE, but fail if the
// given user name does not exist.
//

#define GLOB_TILDE_CHECK 0x00004000

//
// Set this flag to limit the results to sane values.
//

#define GLOB_LIMIT 0x00008000

//
// Old definition for compatibility.
//

#define GLOB_MAXPATH GLOB_LIMIT
#define GLOB_ABEND GLOB_ABORTED

//
// Define error values returned from glob.
//

//
// Memory allocation failure
//

#define GLOB_NOSPACE 1

//
// Read error
//

#define GLOB_ABORTED 2

//
// No matches were found
//

#define GLOB_NOMATCH 3

//
// Not implemented
//

#define GLOB_NOSYS 4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the glob structure type.

Members:

    gl_pathc - Stores the number of paths matched by the given pattern.

    gl_pathv - Stores the array of matched paths.

    gl_matchc - Stores the count of matches desired.

    gl_offs - Stores the number of null array entries to leave at the beginning
        of the path array.

    gl_flags - Stores the flags governing the glob operation. See GLOB_*
        definitions.

    gl_errfunc - Stores a pointer to the error function passed in to glob.

    gl_closedir - Stores an optional pointer to a function used to close a
        directory.

    gl_readdir - Stores an optional pointer to a function used to read from a
        directory.

    gl_opendir - Stores an optional pointer to a function used to open a
        directory.

    gl_lstat - Stores an optional pointer to a function used to get information
        about a path entry, not following symbolic links.

    gl_stat - Stores an optional pointer to a function used to get information
        about a path entry, following symbolic linkds.

--*/

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_matchc;
    size_t gl_offs;
    int gl_flags;
    int (*gl_errfunc) (const char *, int);
    void (*gl_closedir) (void *);
    struct dirent *(*gl_readdir) (void *);
    void *(*gl_opendir) (const char *);
    int (*gl_lstat) (const char *, struct stat *);
    int (*gl_stat) (const char *, struct stat *);
} glob_t;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
glob (
    const char *Pattern,
    int Flags,
    int (*ErrorFunction) (const char *, int),
    glob_t *Glob
    );

/*++

Routine Description:

    This routine is a pathname generator that will expand a pattern out to all
    matching path names.

Arguments:

    Pattern - Supplies a null terminated string containing the pattern to
        match.

    Flags - Supplies a bitfield of flags governing the operation. See GLOB_*
        definitions.

    ErrorFunction - Supplies an optional pointer to an error function that is
        called if a directory cannot be read. It receives the path that failed,
        and the error number set by the operation. If this routine returns
        non-zero, the GLOB_ERR flag is set in the flags, and this routine stops
        and returns GLOB_ABORTED after setting gl_pathc and gl_pathv to
        reflect the paths already scanned. If the routine returns 0, the error
        is ignored.

    Glob - Supplies a pointer to the state where paths are returned.

Return Value:

    0 on success. The gl_pathc and gl_pathv members will be filled out with the
    number of matches.

    Returns one of the GLOB_* return values on failure.

--*/

LIBC_API
void
globfree (
    glob_t *Glob
    );

/*++

Routine Description:

    This routine frees allocated data inside of a glob state structure.

Arguments:

    Glob - Supplies a pointer to the state to free.

Return Value:

    None.

--*/

#ifdef __cplusplus

}

#endif
#endif

