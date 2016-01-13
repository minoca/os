/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    libgen.h

Abstract:

    This header contains definitions for splitting paths.

Author:

    Evan Green 16-Jul-2013

--*/

#ifndef _LIBGEN_H
#define _LIBGEN_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
char *
basename (
    char *Path
    );

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

LIBC_API
char *
dirname (
    char *Path
    );

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

#ifdef __cplusplus

}

#endif
#endif
