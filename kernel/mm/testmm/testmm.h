/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    testmm.h

Abstract:

    This header contains definitions for the memory manager test program.

Author:

    Evan Green 27-Jul-2012

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

ULONG
TestMdls (
    VOID
    );

/*++

Routine Description:

    This routine tests memory descriptor lists.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

BOOL
ValidateMdl (
    PMEMORY_DESCRIPTOR_LIST Mdl
    );

/*++

Routine Description:

    This routine ensures that all entries of an MDL are valid and in order.

Arguments:

    Mdl - Supplies the memory descriptor list to validate.

Return Value:

    TRUE if the MDL is correct.

    FALSE if something was invalid about the MDL.

--*/

ULONG
TestUserVa (
    VOID
    );

/*++

Routine Description:

    This routine tests the user virtual allocator functionality.

Arguments:

    None.

Return Value:

    Returns the number of test failures.

--*/

