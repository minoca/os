/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pl110.c

Abstract:

    This module implements support for ARM PL110 and PL111 LCD controller.

Author:

    Evan Green 7-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define PL110_PART_NUMBER 0x10
#define PL111_PART_NUMBER 0x11

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the registers for the PL110. This also works for the PL111, except
// notice how the control register is at a different offset.
//

typedef enum _PL110_DISPLAY_REGISTERS {
    Pl110RegisterLcdTiming0             = 0x000,
    Pl110RegisterLcdTiming1             = 0x004,
    Pl110RegisterLcdTiming2             = 0x008,
    Pl110RegisterLcdTiming3             = 0x00C,
    Pl110RegisterUpperPanelFrameBase    = 0x010,
    Pl110RegisterLowerPanelFrameBase    = 0x014,
    Pl111RegisterControl                = 0x018,
    Pl110RegisterControl                = 0x01C,
    Pl110RegisterId                     = 0xFE0
} PL110_DISPLAY_REGISTERS, *PPL110_DISPLAY_REGISTERS;

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
EfipPl110Initialize (
    EFI_PHYSICAL_ADDRESS Controller,
    EFI_PHYSICAL_ADDRESS FrameBufferBase,
    UINT32 FrameBufferWidth,
    UINT32 FrameBufferHeight
    )

/*++

Routine Description:

    This routine initialize the PrimeCell PL110 display controller found in
    the Integrator/CP.

Arguments:

    Controller - Supplies the physical address of the PL110 registers.

    FrameBufferBase - Supplies the base of the frame buffer memory to set.

    FrameBufferWidth - Supplies the desired width.

    FrameBufferHeight - Supplies the desired height.

Return Value:

    EFI status code.

--*/

{

    VOID *ControlRegister;
    VOID *Display;
    UINT8 Identifier;

    Display = (VOID *)(UINTN)Controller;
    Identifier = EfiReadRegister8(Display + Pl110RegisterId);
    if (Identifier == PL111_PART_NUMBER) {
        ControlRegister = Display + Pl111RegisterControl;

    } else {
        ControlRegister = Display + Pl110RegisterControl;
    }

    //
    // Currently only one resolution is supported.
    //

    if ((FrameBufferWidth != 1024) || (FrameBufferHeight != 768)) {
        return EFI_UNSUPPORTED;
    }

    //
    // Set the horizontal timing value.
    //

    EfiWriteRegister32(Display + Pl110RegisterLcdTiming0, 0x3F1F3FFC);

    //
    // Set the vertical timing value.
    //

    EfiWriteRegister32(Display + Pl110RegisterLcdTiming1, 0x080B62FF);

    //
    // Set the other timing value.
    //

    EfiWriteRegister32(Display + Pl110RegisterLcdTiming2, 0x067F3800);

    //
    // Set the frame buffer base.
    //

    EfiWriteRegister32(Display + Pl110RegisterUpperPanelFrameBase,
                       FrameBufferBase);

    //
    // Set to 24 bits per pixel and enable the controller.
    //

    EfiWriteRegister32(ControlRegister, 0x192B);
    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

