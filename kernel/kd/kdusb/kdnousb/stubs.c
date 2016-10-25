/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    stubs.c

Abstract:

    This module implements stub routines for KD USB functions.

Author:

    Evan Green 18-Apr-2014

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/kdusb.h>

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
KdUsbInitialize (
    PDEBUG_USB_HOST_DESCRIPTION Host,
    BOOL TestInterface
    )

/*++

Routine Description:

    This routine initializes a USB debug based transport.

Arguments:

    Host - Supplies a pointer to the host controller.

    TestInterface - Supplies a boolean indicating if the interface test should
        be run. This is only true under debugging scenarios where the USB
        debug transport itself is being debugged.

Return Value:

    Status code.

--*/

{

    return STATUS_SUCCESS;
}

VOID
KdEhciModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for a hardware module. Its role is to
    detect the prescense of any of the hardware modules it contains
    implementations for and instantiate them with the kernel.

Arguments:

    None.

Return Value:

    None.

--*/

{

    return;
}

KSTATUS
KdpUsbGetHandoffData (
    PDEBUG_HANDOFF_DATA Data
    )

/*++

Routine Description:

    This routine returns a pointer to the handoff data the USB driver needs to
    operate with a USB debug host controller.

Arguments:

    Data - Supplies a pointer where a pointer to the handoff data is returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_ELIGIBLE_DEVICES if there is no USB debug device.

--*/

{

    return STATUS_NO_ELIGIBLE_DEVICES;
}

//
// --------------------------------------------------------- Internal Functions
//

