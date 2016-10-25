/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpio.c

Abstract:

    This module handles reads and writes to the GPIO pins on the PandaBoard
    first stage loader.

Author:

    Evan Green 24-Feb-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"
#include "util.h"

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

UINTN EfiGpioBaseAddresses[6] = {
    OMAP4430_GPIO1_BASE,
    OMAP4430_GPIO2_BASE,
    OMAP4430_GPIO3_BASE,
    OMAP4430_GPIO4_BASE,
    OMAP4430_GPIO5_BASE,
    OMAP4430_GPIO6_BASE
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipPandaSetLeds (
    BOOLEAN Led1,
    BOOLEAN Led2
    )

/*++

Routine Description:

    This routine sets the LED state for the PandaBoard.

Arguments:

    Led1 - Supplies a boolean indicating if the D1 LED should be lit.

    Led2 - Supplies a boolean indicating if the D2 LED should be lit.

Return Value:

    None.

--*/

{

    if (EfipOmap4GetRevision() >= Omap4460RevisionEs10) {
        EfipOmap4GpioWrite(110, Led1);

    } else {
        EfipOmap4GpioWrite(7, Led1);
    }

    EfipOmap4GpioWrite(8, Led2);
    return;
}

VOID
EfipOmap4GpioWrite (
    UINT32 GpioNumber,
    UINT32 Value
    )

/*++

Routine Description:

    This routine writes to the given GPIO output on an OMAP4.

Arguments:

    GpioNumber - Supplies the GPIO number to write to.

    Value - Supplies 0 to set the GPIO low, or non-zero to set it high.

Return Value:

    None.

--*/

{

    UINT32 Base;
    UINT32 Bit;
    UINT32 Register;

    Base = EfiGpioBaseAddresses[GpioNumber / 32];
    Bit = 1 << (GpioNumber % 32);

    //
    // Ensure the module is enabled.
    //

    OMAP4_WRITE32(Base + OmapGpioControl, 0);

    //
    // Enable output for this GPIO.
    //

    Register = OMAP4_READ32(Base + OmapGpioOutputEnable) & (~Bit);
    OMAP4_WRITE32(Base + OmapGpioOutputEnable, Register);
    if (Value != 0) {
        OMAP4_WRITE32(Base + OmapGpioOutputSet, Bit);

    } else {
        OMAP4_WRITE32(Base + OmapGpioOutputClear, Bit);
    }

    return;
}

UINT32
EfipOmap4GpioRead (
    UINT32 GpioNumber
    )

/*++

Routine Description:

    This routine reads the current input on the given GPIO on an OMAP4.

Arguments:

    GpioNumber - Supplies the GPIO number to read from.

Return Value:

    0 if the read value is low.

    1 if the read value is high.

--*/

{

    UINT32 Base;
    UINT32 Bit;
    UINT32 Value;

    Base = EfiGpioBaseAddresses[GpioNumber / 32];
    Bit = 1 << (GpioNumber % 32);

    //
    // Ensure the module is enabled.
    //

    OMAP4_WRITE32(Base + OmapGpioControl, 0);
    Value = OMAP4_READ32(Base + OmapGpioDataIn);
    if ((Value & Bit) != 0) {
        return 1;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

