/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

