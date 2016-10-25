/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pmic.c

Abstract:

    This module implements support for the TWL4030 PMIC that usually
    accompanies the TI OMAP4.

Author:

    Evan Green 13-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "twl6030.h"
#include "sdomap4.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_SYSCTRL_PADCONF_CORE_BASE 0x4A100000
#define OMAP4_SYSTEM_CONTROL_PBIASLITE 0x600

#define OMAP4_MMC1_VMODE (1 << 21)
#define OMAP4_MMC1_PBIASLITE_PWRDNZ (1 << 22)
#define OMAP4_MMC1_PWRDNZ (1 << 26)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Omap4Twl6030I2cWrite8 (
    UCHAR ChipNumber,
    UCHAR Register,
    UCHAR Value
    );

KSTATUS
Omap4Twl6030I2cRead8 (
    UCHAR ChipNumber,
    UCHAR Register,
    PUCHAR Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Omap4Twl6030InitializeMmcPower (
    VOID
    )

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL6030.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ULONG PageSize;
    PVOID PbiasLite;
    KSTATUS Status;
    PVOID SystemControlBase;
    ULONG Value;

    PageSize = MmPageSize();

    //
    // Map the system control base.
    //

    SystemControlBase = MmMapPhysicalAddress(OMAP4_SYSCTRL_PADCONF_CORE_BASE,
                                             PageSize,
                                             TRUE,
                                             FALSE,
                                             TRUE);

    if (SystemControlBase == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PbiasLite = SystemControlBase + OMAP4_SYSTEM_CONTROL_PBIASLITE;
    Value = HlReadRegister32(PbiasLite);
    Value &= ~(OMAP4_MMC1_PBIASLITE_PWRDNZ | OMAP4_MMC1_PWRDNZ);
    HlWriteRegister32(PbiasLite, Value);

    //
    // Set VMMC1 to 3.15 Volts.
    //

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, VMMC_CFG_VOLTAGE, 0x15);

    ASSERT(KSUCCESS(Status));

    Status = Omap4Twl6030I2cWrite8(TWL6030_CHIP_PM, VMMC_CFG_STATE, 0x21);

    ASSERT(KSUCCESS(Status));

    if (KSUCCESS(Status)) {
        Value = HlReadRegister32(PbiasLite);
        Value |= OMAP4_MMC1_PBIASLITE_PWRDNZ | OMAP4_MMC1_PWRDNZ |
                 OMAP4_MMC1_VMODE;

        HlWriteRegister32(PbiasLite, Value);
    }

    MmUnmapAddress(SystemControlBase, PageSize);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Omap4Twl6030I2cWrite8 (
    UCHAR ChipNumber,
    UCHAR Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes a register on the TWL6030.

Arguments:

    ChipNumber - Supplies the device address of the TWL4030 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies the register value.

Return Value:

    Status code.

--*/

{

    return OmapI2cWrite(ChipNumber, Register, 1, &Value, 1);
}

KSTATUS
Omap4Twl6030I2cRead8 (
    UCHAR ChipNumber,
    UCHAR Register,
    PUCHAR Value
    )

/*++

Routine Description:

    This routine reads a register on the TWL6030.

Arguments:

    ChipNumber - Supplies the device address of the TWL4030 on the I2C bus.

    Register - Supplies the register number to write.

    Value - Supplies a pointer where the read value will be returned.

Return Value:

    Status code.

--*/

{

    return OmapI2cRead(ChipNumber, Register, 1, Value, 1);
}

