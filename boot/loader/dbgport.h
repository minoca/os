/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgport.h

Abstract:

    This header contains internal definitions for the debug port functionality.

Author:

    Evan Green 26-Mar-2014

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
BoSetUpKernelDebugTransport (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters
    );

/*++

Routine Description:

    This routine attempts to set up the kernel debugger transport.

Arguments:

    KernelParameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None. Failure here is not fatal so it is not reported.

--*/

VOID
BopDisableLegacyInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine shuts off any legacy interrupts routed to SMIs for boot
    services.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
BopExploreForDebugDevice (
    PDEBUG_PORT_TABLE2 *CreatedTable
    );

/*++

Routine Description:

    This routine performs architecture-specific actions to go hunting for a
    debug device.

Arguments:

    CreatedTable - Supplies a pointer where a pointer to a generated debug
        port table will be returned on success.

Return Value:

    Status code.

--*/

