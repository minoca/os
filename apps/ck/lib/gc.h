/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gc.h

Abstract:

    This header contains definitions for memory allocation and garbage
    collection.

Author:

    Evan Green 8-Aug-2016

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

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
CkpPushRoot (
    PCK_VM Vm,
    PCK_OBJECT Object
    );

/*++

Routine Description:

    This routine pushes the given object onto a temporary stack to ensure that
    it will not be garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Object - Supplies a pointer to the object to push.

Return Value:

    None.

--*/

VOID
CkpPopRoot (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine pops the top working object off of the temporary stack used to
    ensure that certain objects are not garbage collected.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

PVOID
CkpReallocate (
    PCK_VM Vm,
    PVOID Memory,
    UINTN OldSize,
    UINTN NewSize
    );

/*++

Routine Description:

    This routine performs a Chalk dynamic memory operation.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Memory - Supplies an optional pointer to the memory to resize or free.

    OldSize - Supplies the optional previous size of the allocation.

    NewSize - Supplies the new size of the allocation. Set this to 0 to free
        the memory.

Return Value:

    Returns a pointer to the newly allocated or reallocated memory on success.

    NULL on allocation failure or for free operations.

--*/