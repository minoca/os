/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hlp.h

Abstract:

    This header contains internal definitions for the hardware layer library.

Author:

    Evan Green 28-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait for a system reset to take effect in
// microseconds before moving on.
//

#define RESET_SYSTEM_STALL (100 * MICROSECONDS_PER_MILLISECOND)

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
HlpInitializeRebootModules (
    VOID
    );

/*++

Routine Description:

    This routine initializes the reboot modules support.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
HlpRebootModuleRegisterHardware (
    PREBOOT_MODULE_DESCRIPTION Description
    );

/*++

Routine Description:

    This routine is called to register a new reboot module with the system.

Arguments:

    Description - Supplies a pointer to a structure describing the new
        reboot module.

Return Value:

    Status code.

--*/

KSTATUS
HlpArchResetSystem (
    SYSTEM_RESET_TYPE ResetType
    );

/*++

Routine Description:

    This routine resets the system.

Arguments:

    ResetType - Supplies the desired reset type. If the desired reset type is
        not supported, a cold reset will be attempted.

Return Value:

    Does not return on success, the system resets.

    STATUS_NOT_SUPPORTED if the system cannot be reset.

    STATUS_UNSUCCESSFUL if the system did not reset.

--*/

