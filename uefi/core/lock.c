/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lock.c

Abstract:

    This module implements "lock" services for the EFI core. Now don't get
    excited, UEFI really is single threaded. This is more of a validation than
    any real synchronization.

Author:

    Evan Green 28-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

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

VOID
EfiCoreInitializeLock (
    PEFI_LOCK Lock,
    EFI_TPL Tpl
    )

/*++

Routine Description:

    This routine initializes an EFI lock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

    Tpl - Supplies the Task Prioriry Level the lock is acquired at.

Return Value:

    None.

--*/

{

    Lock->Tpl = Tpl;
    Lock->OwnerTpl = TPL_APPLICATION;
    Lock->State = EfiLockReleased;
    return;
}

EFI_STATUS
EfiCoreAcquireLockOrFail (
    PEFI_LOCK Lock
    )

/*++

Routine Description:

    This routine attempts to acquire the given lock, and fails if it is already
    held.

Arguments:

    Lock - Supplies a pointer to the lock to try to acquire.

Return Value:

    EFI_SUCCESS if the lock was successfully acquired.

    EFI_ACCESS_DENIED if the lock was already held and could not be acquired.

--*/

{

    ASSERT((Lock != NULL) && (Lock->State != EfiLockUninitialized));

    if (Lock->State == EfiLockAcquired) {
        return EFI_ACCESS_DENIED;
    }

    Lock->OwnerTpl = EfiCoreRaiseTpl(Lock->Tpl);
    Lock->State = EfiLockAcquired;
    return EFI_SUCCESS;
}

VOID
EfiCoreAcquireLock (
    PEFI_LOCK Lock
    )

/*++

Routine Description:

    This routine raises to the task priority level of the given lock and
    acquires the lock.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

{

    ASSERT((Lock != NULL) && (Lock->State == EfiLockReleased));

    Lock->OwnerTpl = EfiCoreRaiseTpl(Lock->Tpl);
    Lock->State = EfiLockAcquired;
    return;
}

VOID
EfiCoreReleaseLock (
    PEFI_LOCK Lock
    )

/*++

Routine Description:

    This routine releases ownership of the given lock, and lowers back down
    to the original TPL.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

{

    EFI_TPL Tpl;

    ASSERT((Lock != NULL) && (Lock->State == EfiLockAcquired));

    Tpl = Lock->OwnerTpl;
    Lock->State = EfiLockReleased;
    EfiCoreRestoreTpl(Tpl);
    return;
}

BOOLEAN
EfiCoreIsLockHeld (
    PEFI_LOCK Lock
    )

/*++

Routine Description:

    This routine determines if the given lock is held.

Arguments:

    Lock - Supplies a pointer to the lock to query.

Return Value:

    TRUE if the lock is held.

    FALSE if the lock is not held.

--*/

{

    ASSERT((Lock != NULL) && (Lock->State != EfiLockUninitialized));

    if (Lock->State == EfiLockAcquired) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

