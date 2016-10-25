/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbhub.h

Abstract:

    This header contains definitions for the USB Hub driver.

Author:

    Evan Green 16-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the USB Hub allocation tag.
//

#define USB_HUB_ALLOCATION_TAG 0x48627355 // 'HbsU'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER UsbHubDriver;

//
// -------------------------------------------------------- Function Prototypes
//

