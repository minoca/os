/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.h

Abstract:

    This header contains definitions for the initial first stage loader.

Author:

    Evan Green 1-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/soc/omap4.h>
#include <dev/tirom.h>
#include "util.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to generic registers.
//

#define OMAP4_READ8(_Register) \
        *(volatile UINT8 *)(_Register)

#define OMAP4_WRITE8(_Register, _Value) \
        *((volatile UINT8 *)(_Register)) = (_Value)

#define OMAP4_READ16(_Register) \
        *(volatile UINT16 *)(_Register)

#define OMAP4_WRITE16(_Register, _Value) \
        *((volatile UINT16 *)(_Register)) = (_Value)

#define OMAP4_READ32(_Register) \
        *(volatile UINT32 *)(_Register)

#define OMAP4_WRITE32(_Register, _Value) \
        *((volatile UINT32 *)(_Register)) = (_Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the address the boot loader is loaded to on USB.
//

#define OMAP4_USB_BOOT_ADDRESS (0x82000000 - 64)

//
// Define the address the boot loader is loaded to on SD.
//

#define OMAP4_SD_BOOT_ADDRESS (0x82000000 - 64)

//
// Define the name of the firmware file to load.
//

#define PANDA_FIRMWARE_NAME "pandafw"

//
// Define the working space where the CRC32 table can go.
//

#define PANDA_BOARD_CRC_TABLE_ADDRESS 0x81FE0000

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
EfipInitializeSerial (
    VOID
    );

/*++

Routine Description:

    This routine initialize the serial port for the first stage loader.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipInitializeBoardMux (
    VOID
    );

/*++

Routine Description:

    This routine sets up the correct pin muxing for the PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipInitializeDdr (
    VOID
    );

/*++

Routine Description:

    This routine sets up the DDR RAM on the the PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipInitializeGpmc (
    VOID
    );

/*++

Routine Description:

    This routine initializes the General Purpose Memory Controller on the
    PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipScaleVcores (
    VOID
    );

/*++

Routine Description:

    This routine set up the voltages on the board.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipInitializePrcm (
    VOID
    );

/*++

Routine Description:

    This routine initializes the PRCM. It must be done from SRAM or flash.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipConfigureCoreDpllNoLock (
    VOID
    );

/*++

Routine Description:

    This routine configures the core DPLL.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipLockCoreDpllShadow (
    VOID
    );

/*++

Routine Description:

    This routine locks the core DPLL shadow registers.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipSetRegister32 (
    UINT32 Address,
    UINT32 StartBit,
    UINT32 BitCount,
    UINT32 Value
    );

/*++

Routine Description:

    This routine writes certain bits into a register in a read modify write
    fashion.

Arguments:

    Address - Supplies the address of the register.

    StartBit - Supplies the starting bit index of the mask to change.

    BitCount - Supplies the number of bits to change.

    Value - Supplies the value to set. This will be shifted by the start bit
        count.

Return Value:

    None.

--*/

VOID
EfipSpin (
    UINT32 LoopCount
    );

/*++

Routine Description:

    This routine spins the specified number of times. This is based on the CPU
    cycles, not time.

Arguments:

    LoopCount - Supplies the number of times to spin.

Return Value:

    None.

--*/

OMAP4_REVISION
EfipOmap4GetRevision (
    VOID
    );

/*++

Routine Description:

    This routine returns the OMAP4 revision number.

Arguments:

    None.

Return Value:

    Returns the SoC revision value.

--*/

VOID
EfipPandaSetLeds (
    BOOLEAN Led1,
    BOOLEAN Led2
    );

/*++

Routine Description:

    This routine sets the LED state for the PandaBoard.

Arguments:

    Led1 - Supplies a boolean indicating if the D1 LED should be lit.

    Led2 - Supplies a boolean indicating if the D2 LED should be lit.

Return Value:

    None.

--*/

VOID
EfipOmap4GpioWrite (
    UINT32 GpioNumber,
    UINT32 Value
    );

/*++

Routine Description:

    This routine writes to the given GPIO output on an OMAP4.

Arguments:

    GpioNumber - Supplies the GPIO number to write to.

    Value - Supplies 0 to set the GPIO low, or non-zero to set it high.

Return Value:

    None.

--*/

UINT32
EfipOmap4GpioRead (
    UINT32 GpioNumber
    );

/*++

Routine Description:

    This routine reads the current input on the given GPIO on an OMAP4.

Arguments:

    GpioNumber - Supplies the GPIO number to read from.

Return Value:

    0 if the read value is low.

    1 if the read value is high.

--*/

