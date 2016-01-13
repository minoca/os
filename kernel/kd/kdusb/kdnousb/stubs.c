/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#include <minoca/kernel.h>
#include <minoca/kdebug.h>
#include <minoca/kdusb.h>

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
    PHARDWARE_MODULE_KERNEL_SERVICES Services,
    PDEBUG_USB_HOST_DESCRIPTION Host,
    BOOL TestInterface
    )

/*++

Routine Description:

    This routine initializes a USB debug based transport.

Arguments:

    Services - Supplies a pointer to the hardware module services.

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
    PHARDWARE_MODULE_KERNEL_SERVICES Services
    )

/*++

Routine Description:

    This routine is the entry point for a hardware module. Its role is to
    detect the prescense of any of the hardware modules it contains
    implementations for and instantiate them with the kernel.

Arguments:

    Services - Supplies a pointer to the services/APIs made available by the
        kernel to the hardware module. This set of services is extremely
        limited due to the core nature of these hardware modules. Many of the
        normal services rely on these hardware modules to operate properly.

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

