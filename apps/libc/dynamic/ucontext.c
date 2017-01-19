/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ucontext.c

Abstract:

    This module implements architecture independent functions related to
    manipulating ucontext structures.

Author:

    Evan Green 8-Sep-2016

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <ucontext.h>

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
swapcontext (
    ucontext_t *OldContext,
    ucontext_t *Context
    )

/*++

Routine Description:

    This routine saves the current context, and sets the given new context
    with a backlink to the original context.

Arguments:

    OldContext - Supplies a pointer where the currently running context will
        be saved on success.

    Context - Supplies a pointer to the new context to apply. A link to the
        context running before this call will be saved in this context.

Return Value:

    0 on success.

    -1 on failure, and errno will be set contain more information.

--*/

{

    int Status;

    if ((OldContext == NULL) || (Context == NULL)) {
        errno = EINVAL;
        return -1;
    }

    OldContext->uc_flags &= ~SIGNAL_CONTEXT_FLAG_SWAPPED;
    Status = getcontext(OldContext);

    //
    // Everything below this comment actually runs twice. The first time it is
    // run the swapped flag is just recently cleared. In that case go run the
    // new context. When the new context returns (via makecontext's function
    // returning), it will return right here. The swapped flag will have been
    // set so this routine doesn't go set the same context again.
    //

    if ((Status == 0) &&
        ((OldContext->uc_flags & SIGNAL_CONTEXT_FLAG_SWAPPED) == 0)) {

        OldContext->uc_flags |= SIGNAL_CONTEXT_FLAG_SWAPPED;
        Status = setcontext(Context);
    }

    return Status;
}

__NO_RETURN
VOID
ClpContextEnd (
    ucontext_t *Context
    )

/*++

Routine Description:

    This routine is called after the function entered via makecontext +
    setcontext returns. It sets the next context, or exits the process if there
    is no next context.

Arguments:

    Context - Supplies a pointer to the context.

Return Value:

    This routine does not return. It either sets a new context or exits the
    process.

--*/

{

    if (Context->uc_link == NULL) {
        exit(0);
    }

    setcontext((const ucontext_t *)Context->uc_link);
    abort();
}

//
// --------------------------------------------------------- Internal Functions
//

