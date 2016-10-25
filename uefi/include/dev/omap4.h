/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omap4.h

Abstract:

    This header contains definitions for the TI OMAP4 UEFI device library.

Author:

    Evan Green 3-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the OMAP4 SD vendor specific interrupt status bits.
//

#define SD_OMAP4_INTERRUPT_STATUS_CARD_ERROR       (1 << 28)
#define SD_OMAP4_INTERRUPT_STATUS_BAD_ACCESS_ERROR (1 << 29)

//
// Define the OMAP4 SD vendor specific interrupt signal and status enable bits.
//

#define SD_OMAP4_INTERRUPT_ENABLE_ERROR_CARD       (1 << 28)
#define SD_OMAP4_INTERRUPT_ENABLE_ERROR_BAD_ACCESS (1 << 29)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfipOmap4InitializePowerAndClocks (
    VOID
    );

/*++

Routine Description:

    This routine initializes the PRCM and turns on clocks and power domains
    needed by the system.

Arguments:

    None.

Return Value:

    Status code.

--*/

