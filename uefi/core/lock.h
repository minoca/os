/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lock.h

Abstract:

    This header contains definitions for EFI "lock" functions.

Author:

    Evan Green 5-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _EFI_LOCK_STATE {
    EfiLockUninitialized,
    EfiLockReleased,
    EfiLockAcquired
} EFI_LOCK_STATE, *PEFI_LOCK_STATE;

typedef struct _EFI_LOCK {
    EFI_TPL Tpl;
    EFI_TPL OwnerTpl;
    EFI_LOCK_STATE State;
} EFI_LOCK, *PEFI_LOCK;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfiCoreInitializeLock (
    PEFI_LOCK Lock,
    EFI_TPL Tpl
    );

/*++

Routine Description:

    This routine initializes an EFI lock.

Arguments:

    Lock - Supplies a pointer to the lock to initialize.

    Tpl - Supplies the Task Prioriry Level the lock is acquired at.

Return Value:

    None.

--*/

EFI_STATUS
EfiCoreAcquireLockOrFail (
    PEFI_LOCK Lock
    );

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

VOID
EfiCoreAcquireLock (
    PEFI_LOCK Lock
    );

/*++

Routine Description:

    This routine raises to the task priority level of the given lock and
    acquires the lock.

Arguments:

    Lock - Supplies a pointer to the lock to acquire.

Return Value:

    None.

--*/

VOID
EfiCoreReleaseLock (
    PEFI_LOCK Lock
    );

/*++

Routine Description:

    This routine releases ownership of the given lock, and lowers back down
    to the original TPL.

Arguments:

    Lock - Supplies a pointer to the lock to release.

Return Value:

    None.

--*/

BOOLEAN
EfiCoreIsLockHeld (
    PEFI_LOCK Lock
    );

/*++

Routine Description:

    This routine determines if the given lock is held.

Arguments:

    Lock - Supplies a pointer to the lock to query.

Return Value:

    TRUE if the lock is held.

    FALSE if the lock is not held.

--*/

