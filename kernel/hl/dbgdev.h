/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgdev.h

Abstract:

    This header contains definitions for the hardware layer debug device
    support.

Author:

    Evan Green 8-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kdusb.h>

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
HlpInitializeDebugDevices (
    ULONG DebugDeviceIndex,
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice
    );

/*++

Routine Description:

    This routine initializes the hardware layer's debug device support.
    This routine is called on the boot processor before the debugger is online.

Arguments:

    DebugDeviceIndex - Supplies the index of successfully enumerated debug
        interfaces to return.

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

Return Value:

    Kernel status code.

--*/

VOID
HlpTestUsbDebugInterface (
    VOID
    );

/*++

Routine Description:

    This routine runs the interface test on a USB debug interface if debugging
    the USB transport itself.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
HlpDebugDeviceRegisterHardware (
    PDEBUG_DEVICE_DESCRIPTION Description
    );

/*++

Routine Description:

    This routine is called to register a new debug device with the system.

Arguments:

    Description - Supplies a pointer to a structure describing the new debug
        device.

Return Value:

    Status code.

--*/

KSTATUS
HlpDebugUsbHostRegisterHardware (
    PDEBUG_USB_HOST_DESCRIPTION Description
    );

/*++

Routine Description:

    This routine is called to register a new debug USB host controller with the
    system.

Arguments:

    Description - Supplies a pointer to a structure describing the new debug
        device.

Return Value:

    Status code.

--*/

KSTATUS
HlpArchInitializeDebugDevices (
    VOID
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the serial
        subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

