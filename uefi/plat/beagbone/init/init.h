/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    init.h

Abstract:

    This header contains definitions for the TI AM335x first stage boot loader.

Author:

    Evan Green 17-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/soc/am335x.h>
#include <dev/tirom.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to generic registers.
//

#define AM3_READ8(_Register) \
        *(volatile UINT8 *)(_Register)

#define AM3_WRITE8(_Register, _Value) \
        *((volatile UINT8 *)(_Register)) = (_Value)

#define AM3_READ16(_Register) \
        *(volatile UINT16 *)(_Register)

#define AM3_WRITE16(_Register, _Value) \
        *((volatile UINT16 *)(_Register)) = (_Value)

#define AM3_READ32(_Register) \
        *(volatile UINT32 *)(_Register)

#define AM3_WRITE32(_Register, _Value) \
        *((volatile UINT32 *)(_Register)) = (_Value)

//
// Define macros for accessing peripheral base registers.
//

#define AM3_CM_PER_READ(_Register) \
    AM3_READ32(AM335_CM_PER_REGISTERS + _Register)

#define AM3_CM_PER_WRITE(_Register, _Value) \
    AM3_WRITE32(AM335_CM_PER_REGISTERS + _Register, _Value)

#define AM3_CM_WAKEUP_READ(_Register) \
    AM3_READ32(AM335_CM_WAKEUP_REGISTERS + _Register)

#define AM3_CM_WAKEUP_WRITE(_Register, _Value) \
    AM3_WRITE32(AM335_CM_WAKEUP_REGISTERS + _Register, _Value)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the address the boot loader is loaded to on SD.
//

#define AM335_SD_BOOT_ADDRESS (0x82000000 - 64)

//
// Define the working space where the CRC32 table can go.
//

#define BEAGLEBONE_CRC_TABLE_ADDRESS 0x81FE0000

//
// Define the name of the firmware file to load.
//

#define AM335_FIRMWARE_NAME "bbonefw"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the various potential operating conditions of the
    AM335x.

Members:

    PllMultiplier - Stores the PLL multiplier used to create the desired
        frequency.

    PmicVoltage - Stores the PMIC voltage value used to get the desired voltage.

--*/

typedef struct _AM335_OPP_TABLE_ENTRY {
    UINT32 PllMultiplier;
    UINT32 PmicVoltage;
} AM335_OPP_TABLE_ENTRY, *PAM335_OPP_TABLE_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the device version of the AM335x.
//

extern UINT32 EfiAm335DeviceVersion;

//
// Define the operating conditions table.
//

extern AM335_OPP_TABLE_ENTRY EfiAm335OppTable[];

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfipAm335InitializeClocks (
    VOID
    );

/*++

Routine Description:

    This routine initializes functional clocks for needed modules and domains.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipAm335InitializePlls (
    UINT32 OppIndex,
    UINT32 DdrFrequencyMultiplier
    );

/*++

Routine Description:

    This routine initializes the PLLs for the AM335x.

Arguments:

    OppIndex - Supplies the index into the operating conditions table that the
        PLLs should be configured for.

    DdrFrequencyMultiplier - Supplies the multiplier value to initialize the
        DDR PLL with (depends on whether DDR2 or DDR3 is in use).

Return Value:

    None.

--*/

VOID
EfipAm335ConfigureVddOpVoltage (
    VOID
    );

/*++

Routine Description:

    This routine configures the Vdd op voltage for the AM335x, assuming a
    TPS65217 PMIC hanging off of I2C bus 0.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipAm335SetVdd1Voltage (
    UINT32 PmicVoltage
    );

/*++

Routine Description:

    This routine configures the Vdd1 voltage for the given operating condition.

Arguments:

    PmicVoltage - Supplies the selected PMIC voltage.

Return Value:

    None.

--*/

UINT32
EfipAm335GetMaxOpp (
    VOID
    );

/*++

Routine Description:

    This routine determines the maximum operating conditions for this SoC.

Arguments:

    None.

Return Value:

    Returns the index into the opp table that this SoC can support. See
    AM335_EFUSE_OPP* definitions.

--*/

VOID
EfipInitializeBoardMux (
    VOID
    );

/*++

Routine Description:

    This routine sets up the correct pin muxing for the BeagleBone.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipBeagleBoneBlackInitializeLeds (
    VOID
    );

/*++

Routine Description:

    This routine initializes the SoC so that the LEDs can be driven.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipBeagleBoneBlackSetLeds (
    UINT32 Leds
    );

/*++

Routine Description:

    This routine sets the LEDs to a new value.

Arguments:

    Leds - Supplies the four bits containing whether to set the LEDs high or
        low.

Return Value:

    None.

--*/

VOID
EfipAm335InitializeEmif (
    VOID
    );

/*++

Routine Description:

    This routine performs EMIF initialization in preparation for firing up DDR
    RAM.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipBeagleBoneBlackInitializeDdr3 (
    VOID
    );

/*++

Routine Description:

    This routine fires up the DDR3 main memory.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipAm335EnableUart (
    VOID
    );

/*++

Routine Description:

    This routine performs rudimentary initialization so that UART0 can be used
    as a debug console.

Arguments:

    None.

Return Value:

    None.

--*/

