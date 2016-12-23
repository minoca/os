/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    clock.c

Abstract:

    This module implements early clock initializatio for the PandaBoard.

Author:

    Evan Green 1-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write to PRM_DEVICE registers.
//

#define OMAP4_WRITE_PRM_DEVICE(_Register, _Value) \
    OMAP4_WRITE32(OMAP4430_PRM_BASE + OMAP4430_PRM_DEVICE_OFFSET + (_Register),\
                  (_Value))

#define OMAP4_READ_PRM_DEVICE(_Register) \
    OMAP4_READ32(OMAP4430_PRM_BASE + OMAP4430_PRM_DEVICE_OFFSET + (_Register))

//
// These macros read and write to the INTRCONN_SOCKET registers.
//

#define OMAP4_WRITE_PRM(_Register, _Value)                                    \
    OMAP4_WRITE32(                                                            \
        OMAP4430_PRM_BASE + OMAP4430_PRM_INTRCONN_SOCKET_OFFSET + (_Register),\
        (_Value))

#define OMAP4_READ_PRM(_Register)   \
    OMAP4_READ32(                   \
        OMAP4430_PRM_BASE + OMAP4430_PRM_INTRCONN_SOCKET_OFFSET + (_Register))

//
// These macros read and write to CKGEN_PRM registers.
//

#define OMAP4_WRITE_PRM_CKGEN(_Register, _Value) \
    OMAP4_WRITE32(OMAP4430_PRM_BASE + OMAP4430_PRM_CKGEN_OFFSET + (_Register),\
                  (_Value))

#define OMAP4_READ_PRM_CKGEN(_Register) \
    OMAP4_READ32(OMAP4430_PRM_BASE + OMAP4430_PRM_CKGEN_OFFSET + (_Register))

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_PRM_IRQSTATUS_MPU_A9 0x0010

#define OMAP4_PRM_CM_SYS_CLKSEL 0x0010

#define OMAP4_PRM_CFG_I2C_MODE 0x00A8
#define OMAP4_PRM_CFG_I2C_CLK 0x00AC
#define OMAP4_PRM_VC_VAL_BYPASS 0x00A0

#define PLL_STOP        1
#define PLL_MN_POWER_BYPASS 4
#define PLL_LOW_POWER_BYPASS    5
#define PLL_FAST_RELOCK_BYPASS  6
#define PLL_LOCK        7

//
// Compile time definition for the core speed in MHz. This can be 1000, 600,
// or 400 if not set.
//

#define CONFIG_MPU_1000 1

#define LDELAY 12000000

//
// Define DEVICE_PRM registers.
//

#define PRM_VC_VAL_BYPASS               0x4a307ba0
#define PRM_VC_CFG_CHANNEL              0x4a307ba4
#define PRM_VC_CFG_I2C_MODE             0x4a307ba8
#define PRM_VC_CFG_I2C_CLK              0x4a307bac

//
// Define PRM_VC_VAL_BYPASS register bit definitions.
//

#define PRM_VC_I2C_CHANNEL_FREQ_KHZ             400
#define PRM_VC_VAL_BYPASS_VALID_BIT             0x1000000
#define PRM_VC_VAL_BYPASS_SLAVEADDR_SHIFT       0
#define PRM_VC_VAL_BYPASS_SLAVEADDR_MASK        0x7F
#define PRM_VC_VAL_BYPASS_REGADDR_SHIFT         8
#define PRM_VC_VAL_BYPASS_REGADDR_MASK          0xFF
#define PRM_VC_VAL_BYPASS_DATA_SHIFT            16
#define PRM_VC_VAL_BYPASS_DATA_MASK             0xFF

//
// TPS62361 PMIC definitions.
//

#define TPS62361_I2C_SLAVE_ADDR         0x60
#define TPS62361_REG_ADDR_SET0          0x0
#define TPS62361_REG_ADDR_SET1          0x1
#define TPS62361_REG_ADDR_SET2          0x2
#define TPS62361_REG_ADDR_SET3          0x3
#define TPS62361_REG_ADDR_CTRL          0x4
#define TPS62361_REG_ADDR_TEMP          0x5
#define TPS62361_REG_ADDR_RMP_CTRL      0x6
#define TPS62361_REG_ADDR_CHIP_ID       0x8
#define TPS62361_REG_ADDR_CHIP_ID_2     0x9
#define TPS62361_BASE_VOLT_MV           500
#define TPS62361_VSEL0_GPIO             7

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DPLL_PARAMETERS {
    UINT32 MValue;
    UINT32 NValue;
    UINT32 M2;
    UINT32 M3;
    UINT32 M4;
    UINT32 M5;
    UINT32 M6;
    UINT32 M7;
} DPLL_PARAMETERS, *PDPLL_PARAMETERS;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipConfigureMpuDpll (
    PDPLL_PARAMETERS Parameters
    );

VOID
EfipConfigureIvaDpll (
    PDPLL_PARAMETERS Parameters
    );

VOID
EfipConfigurePerDpll (
    PDPLL_PARAMETERS Parameters
    );

VOID
EfipConfigureAbeDpll (
    PDPLL_PARAMETERS Parameters
    );

VOID
EfipConfigureUsbDpll (
    PDPLL_PARAMETERS Parameters
    );

VOID
EfipEnableAllClocks (
    VOID
    );

VOID
EfipOmap4ScaleTps62361 (
    UINT32 Register,
    UINT32 Value
    );

BOOLEAN
EfipWaitOnValue (
    UINT32 ReadBitMask,
    UINT32 MatchValue,
    UINT32 ReadAddress,
    UINT32 SpinCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// The values here are computed for an input crystal frequency of 38.4MHz.
// Compile time options exist for 600MHz, 1000MHz, and 400MHz.
//

DPLL_PARAMETERS EfiMpuDpllParameters4430 = {

#ifdef CONFIG_MPU_600

    0x7D, 0x07, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

#elif CONFIG_MPU_1000

    0x69, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

#else

    0x1A, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,

#endif

};

//
// If figured out, this should be set to the same parameters as 600 MHz above,
// but with DCC enabled, which doubles the 600MHz to 1.2GHz.
//

DPLL_PARAMETERS EfiMpuDpllParameters4460 = {
    0x69, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
};

DPLL_PARAMETERS EfiPerDpllParameters = {
    0x14, 0x00, 0x08, 0x06, 0x0C, 0x02, 0x04, 0x05,
};

DPLL_PARAMETERS EfiIvaDpllParameters = {
    0x61, 0x03, 0x00, 0x00, 0x04, 0x07, 0x00, 0x00,
};

DPLL_PARAMETERS EfiCoreDpllDdr400Parameters = {
    0x7D, 0x05, 0x01, 0x05, 0x08, 0x04, 0x06, 0x05,
};

DPLL_PARAMETERS EfiAbeDpllParameters = {
    0x40, 0x18, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00,
};

DPLL_PARAMETERS EfiUsbDpllParameters = {
    0x32, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipScaleVcores (
    VOID
    )

/*++

Routine Description:

    This routine set up the voltages on the board.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Millivolts;
    OMAP4_REVISION Revision;
    UINT32 Value;

    Revision = EfipOmap4GetRevision();

    //
    // Enable all clocks now so that GPIO can be twiddled by the TPS62361
    // initialization.
    //

    EfipEnableAllClocks();

    //
    // For VC bypass only VCOREx_CFG_FORCE is necessary and VCORx_CFG_VOLTAGE
    // changes can be discarded.
    //

    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_CFG_I2C_MODE, 0);
    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_CFG_I2C_CLK, 0x6026);
    if (Revision >= Omap4460RevisionEs10) {
        Millivolts = (1300 - TPS62361_BASE_VOLT_MV) / 10;
        EfipOmap4ScaleTps62361(TPS62361_REG_ADDR_SET1, Millivolts);
    }

    //
    // Set VCORE1 to force VSEL.
    //

    if (Revision >= Omap4460RevisionEs10) {
        OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, 0x305512);

    } else {
        OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, 0x3A5512);
    }

    Value = OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS);
    Value |= 0x1000000;
    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, Value);
    while (TRUE) {
        if ((OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS) & 0x1000000) == 0) {
            break;
        }
    }

    //
    // Clear any interrupts.
    //

    Value = OMAP4_READ_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9);
    OMAP4_WRITE_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9, Value);

    //
    // Set VCORE2 to force VSEL.
    //

    if (Revision >= Omap4460RevisionEs10) {
        OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, 0x305B12);

    } else {
        OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, 0x295B12);
    }

    Value = OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS);
    Value |= 0x1000000;
    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, Value);
    while (TRUE) {
        if ((OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS) & 0x1000000) == 0) {
            break;
        }
    }

    //
    // Clear any interrupts.
    //

    Value = OMAP4_READ_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9);
    OMAP4_WRITE_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9, Value);

    //
    // Set VCORE3 to force VSEL. This is not needed on the 4460.
    //

    if (Revision >= Omap4460RevisionEs10) {
        return;
    }

    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, 0x2A6112);
    Value = OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS);
    Value |= 0x1000000;
    OMAP4_WRITE_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS, Value);
    while (TRUE) {
        if ((OMAP4_READ_PRM_DEVICE(OMAP4_PRM_VC_VAL_BYPASS) & 0x1000000) == 0) {
            break;
        }
    }

    //
    // Clear any interrupts.
    //

    Value = OMAP4_READ_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9);
    OMAP4_WRITE_PRM(OMAP4_PRM_IRQSTATUS_MPU_A9, Value);
    return;
}

VOID
EfipInitializePrcm (
    VOID
    )

/*++

Routine Description:

    This routine initializes the PRCM. It must be done from SRAM or flash.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 ClockIndex;
    PDPLL_PARAMETERS MpuParameters;
    OMAP4_REVISION Revision;

    OMAP4_WRITE_PRM_CKGEN(OMAP4_PRM_CM_SYS_CLKSEL, 0x7);
    ClockIndex = OMAP4_READ_PRM_CKGEN(OMAP4_PRM_CM_SYS_CLKSEL);
    if (ClockIndex == 0) {
        return;
    }

    Revision = EfipOmap4GetRevision();
    if (Revision >= Omap4460RevisionEs10) {
        MpuParameters = &EfiMpuDpllParameters4460;

    } else {
        MpuParameters = &EfiMpuDpllParameters4430;
    }

    EfipConfigureMpuDpll(MpuParameters);
    EfipConfigureIvaDpll(&EfiIvaDpllParameters);
    EfipConfigurePerDpll(&EfiPerDpllParameters);
    EfipConfigureAbeDpll(&EfiAbeDpllParameters);
    EfipConfigureUsbDpll(&EfiUsbDpllParameters);
    return;
}

VOID
EfipConfigureCoreDpllNoLock (
    VOID
    )

/*++

Routine Description:

    This routine configures the core DPLL.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PDPLL_PARAMETERS Parameters;

    Parameters = &EfiCoreDpllDdr400Parameters;

    //
    // Get the sysclk speed from cm_sys_clksel. Set it to 38.4 MHz in case the
    // ROM code was bypassed.
    //

    OMAP4_WRITE32(CM_SYS_CLKSEL, 0x07);

    //
    // Set CORE_CLK to CORE_X2_CLK / 2, L3_CLK to CORE_CLK / 2, and
    // L4_CLK to L3_CLK / 2.
    //

    EfipSetRegister32(CM_CLKSEL_CORE, 0, 32, 0x110);

    //
    // Unlock the CORE dpll.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_CORE, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_CORE, LDELAY);

    //
    // Disable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_CORE, 0, 3, 0x0);

    //
    // Set the values.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_CORE, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_CORE, 0, 6, Parameters->NValue);
    EfipSetRegister32(CM_DIV_M2_DPLL_CORE, 0, 5, Parameters->M2);
    EfipSetRegister32(CM_DIV_M3_DPLL_CORE, 0, 5, Parameters->M3);
    EfipSetRegister32(CM_DIV_M4_DPLL_CORE, 0, 5, Parameters->M4);
    EfipSetRegister32(CM_DIV_M5_DPLL_CORE, 0, 5, Parameters->M5);
    EfipSetRegister32(CM_DIV_M6_DPLL_CORE, 0, 5, Parameters->M6);
    EfipSetRegister32(CM_DIV_M7_DPLL_CORE, 0, 5, Parameters->M7);
    return;
}

VOID
EfipLockCoreDpllShadow (
    VOID
    )

/*++

Routine Description:

    This routine locks the core DPLL shadow registers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PDPLL_PARAMETERS Parameters;
    UINT32 Value;

    Parameters = &EfiCoreDpllDdr400Parameters;
    Value = OMAP4_READ32(CM_MEMIF_CLKSTCTRL);
    Value &= (~3);
    Value |= 2;
    OMAP4_WRITE32(CM_MEMIF_CLKSTCTRL, Value);
    while (TRUE) {
        if ((OMAP4_READ32(CM_MEMIF_EMIF_1_CLKCTRL) & 0x30000) == 0) {
            break;
        }
    }

    while (TRUE) {
        if ((OMAP4_READ32(CM_MEMIF_EMIF_2_CLKCTRL) & 0x30000) == 0) {
            break;
        }
    }

    //
    // Lock the core DPLL using the frequency update method:
    // CM_CLKMODE_DPLL_CORE.
    //

    OMAP4_WRITE32(0x4A004120, 0x0A);

    //
    // CM_SHADOW_FREQ_CONFIG1: DLL_OVERRIDE = 1(hack), DLL_RESET = 1,
    // DPLL_CORE_M2_DIV =1, DPLL_CORE_DPLL_EN = 0x7, FREQ_UPDATE = 1
    //

    OMAP4_WRITE32(0x4A004260, 0x70D | (Parameters->M2 << 11));

    //
    // Wait for the frequency update to clear: CM_SHADOW_FREQ_CONFIG1.
    //

    while (TRUE) {
        if ((OMAP4_READ32(0x4A004260) & 0x1) == 0) {
            break;
        }
    }

    //
    // Wait for the DPLL to lock: CM_IDLEST_DPLL_CORE.
    //

    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_CORE, LDELAY);
    while (TRUE) {
        if ((OMAP4_READ32(CM_MEMIF_EMIF_1_CLKCTRL) & 0x30000) == 0) {
            break;
        }
    }

    while (TRUE) {
        if ((OMAP4_READ32(CM_MEMIF_EMIF_2_CLKCTRL) & 0x30000) == 0) {
            break;
        }
    }

    OMAP4_WRITE32(CM_MEMIF_CLKSTCTRL, Value | 3);
    return;
}

VOID
EfipSetRegister32 (
    UINT32 Address,
    UINT32 StartBit,
    UINT32 BitCount,
    UINT32 Value
    )

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

{

    UINT32 Mask;
    UINT32 Register;

    if (BitCount == 32) {
        Mask = 0;

    } else {
        Mask = 1 << BitCount;
    }

    Mask -= 1;
    Register = OMAP4_READ32(Address) & ~(Mask << StartBit);
    Register |= Value << StartBit;
    OMAP4_WRITE32(Address, Register);
    return;
}

VOID
EfipSpin (
    UINT32 LoopCount
    )

/*++

Routine Description:

    This routine spins the specified number of times. This is based on the CPU
    cycles, not time.

Arguments:

    LoopCount - Supplies the number of times to spin.

Return Value:

    None.

--*/

{

    __asm__ volatile("1:\n"
                     "subs %0, %1, #1\n"
                     "bne 1b" : "=r" (LoopCount) : "0"(LoopCount));

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipConfigureMpuDpll (
    PDPLL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine configures the MPU DPLL.

Arguments:

    Parameters - Supplies the DPLL values to set.

Return Value:

    None.

--*/

{

    BOOLEAN AbeDivideBy8;
    BOOLEAN DutyCycleCorrection;
    BOOLEAN EmifDivideBy4;
    OMAP4_REVISION Revision;

    Revision = EfipOmap4GetRevision();

    //
    // Unlock the MPU DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_MPU, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_MPU, LDELAY);
    if (Revision >= Omap4460RevisionEs10) {

        //
        // DCC would cause the ARM_FCLK to get diverted from CLKOUT_M2 to
        // CLKOUT_M3, doubling its rate and enabling duty cycle correction.
        // This currently hangs the board, so there must be more to it.
        //

        DutyCycleCorrection = FALSE;
        AbeDivideBy8 = DutyCycleCorrection;
        EmifDivideBy4 = DutyCycleCorrection;
        EfipSetRegister32(CM_MPU_MPU_CLKCTRL, 24, 1, EmifDivideBy4);
        EfipSetRegister32(CM_MPU_MPU_CLKCTRL, 25, 1, AbeDivideBy8);
        EfipSetRegister32(CM_CLKSEL_DPLL_MPU, 22, 1, DutyCycleCorrection);
    }

    //
    // Disable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_MPU, 0, 3, 0x0);

    //
    // Set M, N and M2 values.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_MPU, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_MPU, 0, 6, Parameters->NValue);
    EfipSetRegister32(CM_DIV_M2_DPLL_MPU, 0, 5, Parameters->M2);
    EfipSetRegister32(CM_DIV_M2_DPLL_MPU, 8, 1, 0x1);

    //
    // Lock the DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_MPU, 0, 3, PLL_LOCK | 0x10);
    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_MPU, LDELAY);
    return;
}

VOID
EfipConfigureIvaDpll (
    PDPLL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine configures the IVA DPLL.

Arguments:

    Parameters - Supplies the DPLL values to set.

Return Value:

    None.

--*/

{

    //
    // Unlock the IVA DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_IVA, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_IVA, LDELAY);

    //
    // Set the bypass clock to Core x2 / 2.
    // CM_BYPCLK_DPLL_IVA = CORE_X2_CLK / 2
    //

    EfipSetRegister32(CM_BYPCLK_DPLL_IVA, 0, 2, 0x1);

    //
    // Diable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_IVA, 0, 3, 0x0);

    //
    // Set M, N, M4 and M5.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_IVA, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_IVA, 0, 7, Parameters->NValue);
    EfipSetRegister32(CM_DIV_M4_DPLL_IVA, 0, 5, Parameters->M4);
    EfipSetRegister32(CM_DIV_M4_DPLL_IVA, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M5_DPLL_IVA, 0, 5, Parameters->M5);
    EfipSetRegister32(CM_DIV_M5_DPLL_IVA, 8, 1, 0x1);

    //
    // Lock the DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_IVA, 0, 3, PLL_LOCK);
    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_IVA, LDELAY);
    return;
}

VOID
EfipConfigurePerDpll (
    PDPLL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine configures the PER DPLL.

Arguments:

    Parameters - Supplies the DPLL values to set.

Return Value:

    None.

--*/

{

    //
    // Unlock the PER DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_PER, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_PER, LDELAY);

    //
    // Diable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_PER, 0, 3, 0x0);

    //
    // Set all values.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_PER, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_PER, 0, 6, Parameters->NValue);
    EfipSetRegister32(CM_DIV_M2_DPLL_PER, 0, 5, Parameters->M2);
    EfipSetRegister32(CM_DIV_M2_DPLL_PER, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M3_DPLL_PER, 0, 5, Parameters->M3);
    EfipSetRegister32(CM_DIV_M3_DPLL_PER, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M4_DPLL_PER, 0, 5, Parameters->M4);
    EfipSetRegister32(CM_DIV_M4_DPLL_PER, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M5_DPLL_PER, 0, 5, Parameters->M5);
    EfipSetRegister32(CM_DIV_M5_DPLL_PER, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M6_DPLL_PER, 0, 5, Parameters->M6);
    EfipSetRegister32(CM_DIV_M6_DPLL_PER, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M7_DPLL_PER, 0, 5, Parameters->M7);
    EfipSetRegister32(CM_DIV_M7_DPLL_PER, 8, 1, 0x1);

    //
    // Lock the DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_PER, 0, 3, PLL_LOCK);
    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_PER, LDELAY);
    return;
}

VOID
EfipConfigureAbeDpll (
    PDPLL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine configures the ABE DPLL.

Arguments:

    Parameters - Supplies the DPLL values to set.

Return Value:

    None.

--*/

{

    //
    // Select sys_clk as the reference clock for the ABE DPLL.
    //

    EfipSetRegister32(CM_ABE_PLL_REF_CLKSEL, 0, 32, 0x0);

    //
    // Unlock the ABE DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_ABE, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_ABE, LDELAY);

    //
    // Disable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_ABE, 0, 3, 0x0);

    //
    // Set M and N.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_ABE, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_ABE, 0, 6, Parameters->NValue);

    //
    // Force DPLL_CLKOUTHIF to stay enabled for M2 and M3.
    //

    EfipSetRegister32(CM_DIV_M2_DPLL_ABE, 0, 32, 0x500);
    EfipSetRegister32(CM_DIV_M2_DPLL_ABE, 0, 5, Parameters->M2);
    EfipSetRegister32(CM_DIV_M2_DPLL_ABE, 8, 1, 0x1);
    EfipSetRegister32(CM_DIV_M3_DPLL_ABE, 0, 32, 0x100);
    EfipSetRegister32(CM_DIV_M3_DPLL_ABE, 0, 5, Parameters->M3);
    EfipSetRegister32(CM_DIV_M3_DPLL_ABE, 8, 1, 0x1);

    //
    // Lock the DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_ABE, 0, 3, PLL_LOCK);
    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_ABE, LDELAY);
    return;
}

VOID
EfipConfigureUsbDpll (
    PDPLL_PARAMETERS Parameters
    )

/*++

Routine Description:

    This routine configures the ABE DPLL.

Arguments:

    Parameters - Supplies the DPLL values to set.

Return Value:

    None.

--*/

{

    //
    // Select the 60 MHz clock (480 / 8 = 60).
    //

    EfipSetRegister32(CM_CLKSEL_USB_60MHz, 0, 32, 0x1);

    //
    // Unlock the USB DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_USB, 0, 3, PLL_MN_POWER_BYPASS);
    EfipWaitOnValue((1 << 0), 0, CM_IDLEST_DPLL_USB, LDELAY);

    //
    // Disable autoidle.
    //

    EfipSetRegister32(CM_AUTOIDLE_DPLL_USB, 0, 3, 0x0);

    //
    // Set M and N.
    //

    EfipSetRegister32(CM_CLKSEL_DPLL_USB, 8, 11, Parameters->MValue);
    EfipSetRegister32(CM_CLKSEL_DPLL_USB, 0, 6, Parameters->NValue);

    //
    // Force DPLL CLKOUT to stay active.
    //

    EfipSetRegister32(CM_DIV_M2_DPLL_USB, 0, 32, 0x100);
    EfipSetRegister32(CM_DIV_M2_DPLL_USB, 0, 5, Parameters->M2);
    EfipSetRegister32(CM_DIV_M2_DPLL_USB, 8, 1, 0x1);
    EfipSetRegister32(CM_CLKDCOLDO_DPLL_USB, 8, 1, 0x1);

    //
    // Lock the DPLL.
    //

    EfipSetRegister32(CM_CLKMODE_DPLL_USB, 0, 3, PLL_LOCK);
    EfipWaitOnValue((1 << 0), 1, CM_IDLEST_DPLL_USB, LDELAY);

    //
    // Force enable the CLKCOLDO clock.
    //

    EfipSetRegister32(CM_CLKDCOLDO_DPLL_USB, 0, 32, 0x100);
    return;
}

VOID
EfipEnableAllClocks (
    VOID
    )

/*++

Routine Description:

    This routine fires up the OMAP4 clock tree.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Enable L4PER clocks.
    //

    EfipSetRegister32(CM_L4PER_CLKSTCTRL, 0, 32, 0x2);
    EfipSetRegister32(CM_L4PER_DMTIMER10_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER10_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_DMTIMER11_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER11_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_DMTIMER2_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER2_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_DMTIMER3_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER3_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_DMTIMER4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER4_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_DMTIMER9_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_DMTIMER9_CLKCTRL,
                    LDELAY);

    //
    // Enable GPIO clocks.
    //

    EfipSetRegister32(CM_L4PER_GPIO2_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_GPIO2_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_GPIO3_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_GPIO3_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_GPIO4_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_GPIO4_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_GPIO4_CLKCTRL, 8, 1, 0x1);
    EfipSetRegister32(CM_L4PER_GPIO5_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_GPIO5_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_GPIO6_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_GPIO6_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_HDQ1W_CLKCTRL, 0, 32, 0x2);

    //
    // Enable I2C clocks.
    //

    EfipSetRegister32(CM_L4PER_I2C1_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_I2C1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_I2C2_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_I2C2_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_I2C3_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_I2C3_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_I2C4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_I2C4_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_MCBSP4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_MCBSP4_CLKCTRL, LDELAY);

    //
    // Enable MCSPI clocks.
    //

    EfipSetRegister32(CM_L4PER_MCSPI1_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_MCSPI1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_MCSPI2_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_MCSPI2_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_MCSPI3_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_MCSPI3_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_MCSPI4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_MCSPI4_CLKCTRL, LDELAY);

    //
    // Enable MMC clocks.
    //

    EfipSetRegister32(CM_L3INIT_HSMMC1_CLKCTRL, 0, 2, 0x2);
    EfipSetRegister32(CM_L3INIT_HSMMC1_CLKCTRL, 24, 1, 0x1);
    EfipSetRegister32(CM_L3INIT_HSMMC2_CLKCTRL, 0, 2, 0x2);
    EfipSetRegister32(CM_L3INIT_HSMMC2_CLKCTRL, 24, 1, 0x1);
    EfipSetRegister32(CM_L4PER_MMCSD3_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 18) | (1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_MMCSD3_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_MMCSD4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 18) | (1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_MMCSD4_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L4PER_MMCSD5_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L4PER_MMCSD5_CLKCTRL,
                    LDELAY);

    //
    // Enable UART clocks.
    //

    EfipSetRegister32(CM_L4PER_UART1_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_UART1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_UART2_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_UART2_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_UART3_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_UART3_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L4PER_UART4_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L4PER_UART4_CLKCTRL, LDELAY);

    //
    // Enable wakeup clocks.
    //

    EfipSetRegister32(CM_WKUP_GPIO1_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_WKUP_GPIO1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_WKUP_TIMER1_CLKCTRL, 0, 32, 0x01000002);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_WKUP_TIMER1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_WKUP_KEYBOARD_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_WKUP_KEYBOARD_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_SDMA_CLKSTCTRL, 0, 32, 0x0);
    EfipSetRegister32(CM_MEMIF_CLKSTCTRL, 0, 32, 0x3);
    EfipSetRegister32(CM_MEMIF_EMIF_1_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_MEMIF_EMIF_1_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_MEMIF_EMIF_2_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_MEMIF_EMIF_2_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_D2D_CLKSTCTRL, 0, 32, 0x3);
    EfipSetRegister32(CM_L3_2_GPMC_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L3_2_GPMC_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L3INSTR_L3_3_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_L3INSTR_L3_3_CLKCTRL, LDELAY);
    EfipSetRegister32(CM_L3INSTR_L3_INSTR_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L3INSTR_L3_INSTR_CLKCTRL,
                    LDELAY);

    EfipSetRegister32(CM_L3INSTR_OCP_WP1_CLKCTRL, 0, 32, 0x1);
    EfipWaitOnValue((1 << 17) | (1 << 16),
                    0,
                    CM_L3INSTR_OCP_WP1_CLKCTRL,
                    LDELAY);

    //
    // Enable watchdog clocks.
    //

    EfipSetRegister32(CM_WKUP_WDT2_CLKCTRL, 0, 32, 0x2);
    EfipWaitOnValue((1 << 17) | (1 << 16), 0, CM_WKUP_WDT2_CLKCTRL, LDELAY);

    //
    // Select DPLL PER clock as the source for SGX FCLK.
    //

    EfipSetRegister32(CM_SGX_SGX_CLKCTRL, 24, 1, 0x1);

    //
    // Enable clocks for USB fast boot.
    //

    EfipSetRegister32(CM_L3INIT_USBPHY_CLKCTRL, 0, 32, 0x301);
    EfipSetRegister32(CM_L3INIT_HSUSBOTG_CLKCTRL, 0, 32, 0x1);
    return;
}

VOID
EfipOmap4ScaleTps62361 (
    UINT32 Register,
    UINT32 Value
    )

/*++

Routine Description:

    This routine enables the proper scaling on the TPS62361 which controls
    the MPU voltage on PandaBoard ES models.

Arguments:

    Register - Supplies the register to set.

    Value - Supplies the value to set.

Return Value:

    None.

--*/

{

    UINT32 RegisterValue;

    EfipOmap4GpioWrite(TPS62361_VSEL0_GPIO, TRUE);
    RegisterValue = TPS62361_I2C_SLAVE_ADDR |
                    (Register << PRM_VC_VAL_BYPASS_REGADDR_SHIFT) |
                    (Value << PRM_VC_VAL_BYPASS_DATA_SHIFT) |
                    PRM_VC_VAL_BYPASS_VALID_BIT;

    OMAP4_WRITE32(PRM_VC_VAL_BYPASS, RegisterValue);
    do {
        RegisterValue = OMAP4_READ32(PRM_VC_VAL_BYPASS);

    } while ((RegisterValue & PRM_VC_VAL_BYPASS_VALID_BIT) != 0);

    return;
}

BOOLEAN
EfipWaitOnValue (
    UINT32 ReadBitMask,
    UINT32 MatchValue,
    UINT32 ReadAddress,
    UINT32 SpinCount
    )

/*++

Routine Description:

    This routine spins waiting for a value to change.

Arguments:

    ReadBitMask - Supplies the mask of bits to apply when comparing.

    MatchValue - Supplies the value the register should have after applying the
        bit mask.

    ReadAddress - Supplies the address of the register to read.

    SpinCount - Supplies the number of times to spin reading before giving up.

Return Value:

    FALSE if the register did not become the desired value.

    TRUE if the register is the desired value.

--*/

{

    UINT32 Index;
    UINT32 Value;

    Index = 0;
    while (Index <= SpinCount) {
        Index += 1;
        Value = OMAP4_READ32(ReadAddress) & ReadBitMask;
        if (Value == MatchValue) {
            return TRUE;
        }
    }

    return FALSE;
}

