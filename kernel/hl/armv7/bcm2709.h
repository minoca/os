/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/soc/b2709os.h>

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
// Store a pointer to the BCM2709 ACPI Table.
//

extern PBCM2709_TABLE HlBcm2709Table;

//
// -------------------------------------------------------- Function Prototypes
//

