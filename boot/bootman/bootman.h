/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootman.h

Abstract:

    This header contains definitions for the Boot Manager.

Author:

    Evan Green 21-Feb-2014

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

extern LIST_ENTRY BmLoadedImageList;

//
// -------------------------------------------------------- Function Prototypes
//

INT
BmMain (
    PBOOT_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine is the entry point for the boot manager program.

Arguments:

    Parameters - Supplies a pointer to the application parameters.

Return Value:

    On success, this function does not return.

    On failure, this function returns the step number on which it failed. This
    provides an indication as to where in the boot process it failed.

--*/

KSTATUS
BmpInitializeImageSupport (
    PVOID BootDevice,
    PBOOT_ENTRY BootEntry
    );

/*++

Routine Description:

    This routine initializes the image library for use in the boot manager.

Arguments:

    BootDevice - Supplies a pointer to the boot volume token, used for loading
        images from disk.

    BootEntry - Supplies a pointer to the selected boot entry.

Return Value:

    Status code.

--*/

KSTATUS
BmpFwInitializeBootBlock (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_VOLUME OsVolume
    );

/*++

Routine Description:

    This routine initializes the boot initialization block that is passed when
    control is handed off to the next boot application.

Arguments:

    Parameters - Supplies a pointer to the boot initialization block.

    OsVolume - Supplies a pointer to the open volume containing the application
        to be launched.

Return Value:

    Status code.

--*/

INT
BmpFwTransferToBootApplication (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_APPLICATION_ENTRY EntryPoint
    );

/*++

Routine Description:

    This routine transfers control to another boot application.

Arguments:

    Parameters - Supplies a pointer to the initialization block.

    EntryPoint - Supplies tne address of the entry point routine of the new
        application.

Return Value:

    Returns the integer return value from the application. Often does not
    return on success.

--*/
