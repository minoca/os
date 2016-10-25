/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    gpio.c

Abstract:

    This module implements platform GPIO support for the BCM2709 SoC family.

Author:

    Chris Stevens 3-May-2016

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from a BCM2709 GPIO register.
//

#define READ_GPIO_REGISTER(_Register) \
    EfiReadRegister32(BCM2709_GPIO_BASE + (_Register))

//
// This macro writes to a BCM2709 GPIO register.
//

#define WRITE_GPIO_REGISTER(_Register, _Value) \
    EfiWriteRegister32(BCM2709_GPIO_BASE + (_Register), (_Value))

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

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709GpioFunctionSelect (
    UINT32 Pin,
    UINT32 Mode
    )

/*++

Routine Description:

    This routine sets the given mode for the pin's function select.

Arguments:

    Pin - Supplies the GPIO pin whose function select to modify.

    Mode - Supplies the function select mode to set.

Return Value:

    EFI status code.

--*/

{

    UINT32 Register;
    UINT32 Shift;
    UINT32 Value;

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    if ((Pin > BCM2709_GPIO_PIN_MAX) ||
        (Mode > BCM2709_GPIO_FUNCTION_SELECT_MASK)) {

        return EFI_INVALID_PARAMETER;
    }

    Register = (Pin / BCM2709_GPIO_FUNCTION_SELECT_PIN_COUNT) *
               BCM2709_GPIO_FUNCTION_SELECT_REGISTER_BYTE_WIDTH;

    Shift = (Pin % BCM2709_GPIO_FUNCTION_SELECT_PIN_COUNT) *
            BCM2709_GPIO_FUNCTION_SELECT_PIN_BIT_WIDTH;

    Value = READ_GPIO_REGISTER(Register);
    Value &= ~(BCM2709_GPIO_FUNCTION_SELECT_MASK << Shift);
    WRITE_GPIO_REGISTER(Register, Value);
    Value |= (Mode << Shift);
    WRITE_GPIO_REGISTER(Register, Value);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

