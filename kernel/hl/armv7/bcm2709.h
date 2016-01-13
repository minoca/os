/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    bcm2709.h

Abstract:

    This header contains definitions for the BCM 2709 hardware modules.

Author:

    Chris Stevens 27-Dec-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/dev/bcm2709.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BCM2709_ALLOCATION_TAG 0x324D4342 // '2MCB'

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the provided hardware layer services.
//

extern PHARDWARE_MODULE_KERNEL_SERVICES HlBcm2709KernelServices;

//
// Store a pointer to the BCM2709 ACPI Table.
//

extern PBCM2709_TABLE HlBcm2709Table;

//
// -------------------------------------------------------- Function Prototypes
//

