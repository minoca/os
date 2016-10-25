/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testhook.c

Abstract:

    This module implements test hooks for the I/O subsystem.

Author:

    Chris Stevens 10-Jun-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "iop.h"

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
// Stores a bitmask for I/O subsystem test hooks.
//

volatile ULONG IoTestHooks = 0;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
IoSetTestHook (
    ULONG TestHookMask
    )

/*++

Routine Description:

    This routine sets the provided test hook mask in the test hook bitmask.

Arguments:

    TestHookMask - Supplies the test hook mask that is to be added to the test
        hook bitmask.

Return Value:

    None.

--*/

{

    RtlAtomicOr32(&IoTestHooks, TestHookMask);
    return;
}

KERNEL_API
VOID
IoClearTestHook (
    ULONG TestHookMask
    )

/*++

Routine Description:

    This routine unsets the provided test hook mask from the test hook bitmask.

Arguments:

    TestHookMask - Supplies the test hook mast hat is to be removed from the
        test hook bitmask.

Return Value:

    None.

--*/

{

    RtlAtomicAnd32(&IoTestHooks, ~TestHookMask);
    return;
}

BOOL
IopIsTestHookSet (
    ULONG TestHookMask
    )

/*++

Routine Description:

    This routine checks to see if the given test hook field is currently set in
    the test hook bitmask. This clears the bit if it is set.

Arguments:

    TestHookMask - Supplies the test hook field this routine will check the
        test hook bitmask against.

Return Value:

    Returns TRUE if the test hook is set, or FALSE otherwise.

--*/

{

    ULONG OldTestHooks;

    //
    // If the test hook is present in the test hooks field, then remove it and
    // return true.
    //

    OldTestHooks = RtlAtomicAnd32(&IoTestHooks, ~TestHookMask);
    if ((OldTestHooks & TestHookMask) != 0) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

