/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ktests.h

Abstract:

    This header contains the prototypes for the kernel tests.

Author:

    Evan Green 5-Nov-2013

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

KSTATUS
KTestPoolStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new invocation of the paged and non-paged pool stress
    test.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

KSTATUS
KTestWorkStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new invocation of the work item stress test.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

KSTATUS
KTestThreadStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new invocation of the thread stress test.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

KSTATUS
KTestDescriptorStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new invocation of the memory descriptor stress test.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

KSTATUS
KTestBlockStressStart (
    PKTEST_START_TEST Command,
    PKTEST_ACTIVE_TEST Test
    );

/*++

Routine Description:

    This routine starts a new invocation of the block allocator stress tests.

Arguments:

    Command - Supplies a pointer to the start command.

    Test - Supplies a pointer to the active test structure to initialize.

Return Value:

    Status code.

--*/

