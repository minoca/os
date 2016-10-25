/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intr.c

Abstract:

    This module implements platform interrupt support for BIOS machines.

Author:

    Evan Green 3-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>

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

EFI_STATUS
EfiPlatformInitializeInterrupts (
    EFI_PLATFORM_BEGIN_INTERRUPT *BeginInterruptFunction,
    EFI_PLATFORM_HANDLE_INTERRUPT *HandleInterruptFunction,
    EFI_PLATFORM_END_INTERRUPT *EndInterruptFunction
    )

/*++

Routine Description:

    This routine initializes support for platform interrupts. Interrupts are
    assumed to be disabled at the processor now. This routine should enable
    interrupts at the procesor core.

Arguments:

    BeginInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called when an interrupt occurs.

    HandleInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to handle a platform-specific interurpt.
        NULL may be returned here.

    EndInterruptFunction - Supplies a pointer where a pointer to a function
        will be returned that is called to complete an interrupt.

Return Value:

    EFI Status code.

--*/

{

    //
    // Because the BIOS sets up 16-bit real mode interrupts, do not enable
    // interrupts here.
    //

    *BeginInterruptFunction = NULL;
    *HandleInterruptFunction = NULL;
    *EndInterruptFunction = NULL;
    return EFI_SUCCESS;
}

VOID
EfiPlatformTerminateInterrupts (
    VOID
    )

/*++

Routine Description:

    This routine terminates interrupt services in preparation for transitioning
    out of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

