/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    procarch.c

Abstract:

    This module contains architecture-specific support for ACPI processor
    management.

Author:

    Evan Green 29-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "../acpip.h"
#include "../proc.h"
#include "../namespce.h"

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

KSTATUS
AcpipArchInitializeProcessorManagement (
    PACPI_OBJECT NamespaceObject
    )

/*++

Routine Description:

    This routine is called to perform architecture-specific initialization for
    ACPI-based processor power management.

Arguments:

    NamespaceObject - Supplies the namespace object of this processor.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
AcpipEnterCState (
    PPM_IDLE_PROCESSOR_STATE Processor,
    ULONG State
    )

/*++

Routine Description:

    This routine prototype represents a function that is called to go into a
    given idle state on the current processor. This routine is called with
    interrupts disabled, and should return with interrupts disabled.

Arguments:

    Processor - Supplies a pointer to the information for the current processor.

    State - Supplies the new state index to change to.

Return Value:

    None. It is assumed when this function returns that the idle state was
    entered and then exited.

--*/

{

    //
    // C-states via ACPI on ARM are not yet implemented.
    //

    ASSERT(FALSE);

    ArWaitForInterrupt();
    ArDisableInterrupts();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

