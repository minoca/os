/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usb.c

Abstract:

    This module fires up the RK32xx Veyron's High Speed USB controller.

Author:

    Chris Stevens 10-Aug-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "veyronfw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to the GPIO registers.
//

#define RK32_READ_GPIO(_GpioBase, _Register) \
    EfiReadRegister32((VOID *)(_GpioBase) + (_Register))

#define RK32_WRITE_GPIO(_GpioBase, _Register, _Value) \
    EfiWriteRegister32((VOID *)(_GpioBase) + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the GPIO 0 bit values.
//

#define RK32_GPIO0_USB_HOST1_POWER_ENABLE (1 << 3)
#define RK32_GPIO0_USB_OTG_POWER_ENABLE   (1 << 4)

//
// Define the GPIO 7 bit values.
//

#define RK32_GPIO7_USB_5V (1 << 5)

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipVeyronUsbInitialize (
    VOID
    )

/*++

Routine Description:

    This routine performs any board-specific high speed USB initialization.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Enable USB Host 1 power.
    //

    Value = RK32_READ_GPIO(RK32_GPIO0_BASE, Rk32GpioPortADirection);
    Value |= RK32_GPIO0_USB_HOST1_POWER_ENABLE;
    RK32_WRITE_GPIO(RK32_GPIO0_BASE, Rk32GpioPortADirection, Value);
    Value = RK32_READ_GPIO(RK32_GPIO0_BASE, Rk32GpioPortAData);
    Value |= RK32_GPIO0_USB_HOST1_POWER_ENABLE;
    RK32_WRITE_GPIO(RK32_GPIO0_BASE, Rk32GpioPortAData, Value);

    //
    // Enable USB OTG power.
    //

    Value = RK32_READ_GPIO(RK32_GPIO0_BASE, Rk32GpioPortADirection);
    Value |= RK32_GPIO0_USB_OTG_POWER_ENABLE;
    RK32_WRITE_GPIO(RK32_GPIO0_BASE, Rk32GpioPortADirection, Value);
    Value = RK32_READ_GPIO(RK32_GPIO0_BASE, Rk32GpioPortAData);
    Value |= RK32_GPIO0_USB_OTG_POWER_ENABLE;
    RK32_WRITE_GPIO(RK32_GPIO0_BASE, Rk32GpioPortAData, Value);

    //
    // Set USB to 5V.
    //

    Value = RK32_READ_GPIO(RK32_GPIO7_BASE, Rk32GpioPortADirection);
    Value |= RK32_GPIO7_USB_5V;
    RK32_WRITE_GPIO(RK32_GPIO7_BASE, Rk32GpioPortADirection, Value);
    Value = RK32_READ_GPIO(RK32_GPIO7_BASE, Rk32GpioPortAData);
    Value |= RK32_GPIO7_USB_5V;
    RK32_WRITE_GPIO(RK32_GPIO7_BASE, Rk32GpioPortAData, Value);
    return;
}

