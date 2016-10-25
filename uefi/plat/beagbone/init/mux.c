/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mux.c

Abstract:

    This module sets up pin muxing for the BeagleBone.

Author:

    Evan Green 17-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipAm335InitializeMmc0 (
    VOID
    );

VOID
EfipAm335InitializeMmc1 (
    VOID
    );

VOID
EfipAm335InitializeEthernet (
    VOID
    );

VOID
EfipAm335InitializeUsb1 (
    VOID
    );

VOID
EfipBeagleBoneBlackInitializeDdr3Phy (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipInitializeBoardMux (
    VOID
    )

/*++

Routine Description:

    This routine sets up the correct pin muxing for the BeagleBone.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINTN Register;

    EfipAm335InitializeMmc0();
    EfipAm335InitializeMmc1();
    EfipAm335InitializeEthernet();
    EfipAm335InitializeUsb1();

    //
    // Set the mux for CLKOUT1 which acts as the clock for the HDMI framer.
    //

    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_CONF_XDMA_EVENT_INTR0;

    AM3_WRITE32(Register, 3);
    return;
}

VOID
EfipBeagleBoneBlackInitializeLeds (
    VOID
    )

/*++

Routine Description:

    This routine initializes the SoC so that the LEDs can be driven.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    //
    // Enable GPIO1 in CM PER.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_GPIO1_CLOCK_CONTROL);
    Value |= AM335_CM_PER_GPIO1_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_GPIO1_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_GPIO1_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_GPIO1_CLOCK_MODE_MASK) !=
             AM335_CM_PER_GPIO1_CLOCK_ENABLE);

    //
    // Enable the GPIO1 functional clock.
    //

    Value |= AM335_CM_PER_GPIO1_CLOCK_FUNCTIONAL_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_GPIO1_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_GPIO1_CLOCK_CONTROL);

    } while ((Value &
              AM335_CM_PER_GPIO1_CLOCK_FUNCTIONAL_CLOCK_ENABLE) == 0);

    //
    // Wait for the idle state to switch.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_GPIO1_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_GPIO1_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_PER_GPIO1_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the clock activity to settle down.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L4LS_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_L4LS_CLOCK_STATE_ACTIVITY_GPIO1) == 0);

    //
    // Change the pin muxing to select GPIO.
    //

    Value = AM335_PAD_MUXCODE(7);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_GPMC_A(5);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_GPMC_A(6);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_GPMC_A(7);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_GPMC_A(8);
    AM3_WRITE32(Register, Value);

    //
    // Enable the GPIO module.
    //

    Value = AM3_READ32(AM335_GPIO_1_BASE + AM335_GPIO_CONTROL);
    Value &= ~AM335_GPIO_CONTROL_DISABLE_MODULE;
    AM3_WRITE32(AM335_GPIO_1_BASE + AM335_GPIO_CONTROL, Value);

    //
    // Reset the GPIO module.
    //

    Value = AM3_READ32(AM335_GPIO_1_BASE + AM335_GPIO_CONFIGURATION);
    Value &= ~AM335_GPIO_CONFIGURATION_SOFT_RESET;
    AM3_WRITE32(AM335_GPIO_1_BASE + AM335_GPIO_CONFIGURATION, Value);
    do {
        Value = AM3_READ32(AM335_GPIO_1_BASE + AM335_GPIO_SYSTEM_STATUS);

    } while ((Value & AM335_GPIO_CONFIGURATION_RESET_DONE) == 0);

    //
    // Set the direction to be output.
    //

    Value = AM3_READ32(AM335_GPIO_1_BASE + AM335_GPIO_OUTPUT_ENABLE);
    Value &= ~((1 << 21) | (1 << 22) | (1 << 23) | (1 << 24));
    AM3_WRITE32(AM335_GPIO_1_BASE + AM335_GPIO_OUTPUT_ENABLE, Value);
    return;
}

VOID
EfipBeagleBoneBlackSetLeds (
    UINT32 Leds
    )

/*++

Routine Description:

    This routine sets the LEDs to a new value.

Arguments:

    Leds - Supplies the four bits containing whether to set the LEDs high or
        low.

Return Value:

    None.

--*/

{

    UINT32 Value;

    Value = (Leds & 0x0F) << 21;
    AM3_WRITE32(AM335_GPIO_1_BASE + AM335_GPIO_SET_DATA_OUT, Value);
    Value = (~Leds & 0x0F) << 21;
    AM3_WRITE32(AM335_GPIO_1_BASE + AM335_GPIO_CLEAR_DATA_OUT, Value);
    return;
}

VOID
EfipAm335InitializeEmif (
    VOID
    )

/*++

Routine Description:

    This routine performs EMIF initialization in preparation for firing up DDR
    RAM.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Mask;
    UINT32 Register;
    UINT32 Value;

    //
    // Enable the clocks for EMIF.
    //

    Register = AM335_CM_PER_EMIF_FW_CLOCK_CONTROL;
    Value = AM3_CM_PER_READ(Register);
    Value &= ~AM335_CM_PER_EMIF_FW_CLOCK_MODE_MASK;
    Value |= AM335_CM_PER_EMIF_FW_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(Register, Value);
    Register = AM335_CM_PER_EMIF_CLOCK_CONTROL;
    Value = AM3_CM_PER_READ(Register);
    Value &= ~AM335_CM_PER_EMIF_CLOCK_MODE_MASK;
    Value |= AM335_CM_PER_EMIF_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(Register, Value);
    Mask = AM335_CM_PER_L3_CLOCK_STATE_EMIF_ACTIVE |
           AM335_CM_PER_L3_CLOCK_STATE_ACTIVE;

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_STATE_CONTROL);

    } while ((Value & Mask) != Mask);

    return;
}

VOID
EfipBeagleBoneBlackInitializeDdr3 (
    VOID
    )

/*++

Routine Description:

    This routine fires up the DDR3 main memory.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    EfipBeagleBoneBlackInitializeDdr3Phy();
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_DDR_CMD_IO_CONTROL(0);

    AM3_WRITE32(Register, AM335_DDR3_CONTROL_DDR_CMD_IOCTRL_0);
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_DDR_CMD_IO_CONTROL(1);

    AM3_WRITE32(Register, AM335_DDR3_CONTROL_DDR_CMD_IOCTRL_1);
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_DDR_CMD_IO_CONTROL(2);

    AM3_WRITE32(Register, AM335_DDR3_CONTROL_DDR_CMD_IOCTRL_2);
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_DDR_DATA_IO_CONTROL(0);

    AM3_WRITE32(Register, AM335_DDR3_CONTROL_DDR_DATA_IOCTRL_0);
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_DDR_DATA_IO_CONTROL(1);

    AM3_WRITE32(Register, AM335_DDR3_CONTROL_DDR_DATA_IOCTRL_1);

    //
    // Set up the I/O to work with DDR3.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_DDR_IO_CONTROL;
    Value = AM3_READ32(Register);
    Value &= AM335_DDR3_CONTROL_DDR_IO_CTRL;
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_DDR_CKE_CONTROL;
    Value = AM3_READ32(Register);
    Value |= AM335_DDR3_CONTROL_DDR_CKE_CONTROL;
    AM3_WRITE32(Register, Value);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_DDR_PHY_CONTROL_1;
    Value = AM335_DDR3_EMIF_DDR_PHY_CTRL_1;

    //
    // If the device supports it add dynamic power down.
    //

    if ((EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_0) ||
        (EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_1)) {

        Value |= AM335_DDR3_EMIF_DDR_PHY_CTRL_1_DY_PWRDN;
    }

    AM3_WRITE32(Register, Value);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_DDR_PHY_CONTROL_1_SHADOW;
    Value = AM335_DDR3_EMIF_DDR_PHY_CTRL_1_SHDW;
    if ((EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_0) ||
        (EfiAm335DeviceVersion == AM335_SOC_DEVICE_VERSION_2_1)) {

        Value |= AM335_DDR3_EMIF_DDR_PHY_CTRL_1_SHDW_DY_PWRDN;
    }

    AM3_WRITE32(Register, Value);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_DDR_PHY_CONTROL_2;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_DDR_PHY_CTRL_2);

    //
    // Write timing registers one through three.
    //

    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_1;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_1);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_1_SHADOW;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_1_SHDW);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_2;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_2);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_2_SHADOW;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_2_SHDW);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_3;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_3);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_TIM_3_SHADOW;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_TIM_3_SHDW);

    //
    // Write reference control and other configuration.
    //

    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_REF_CONTROL;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_REF_CTRL_VAL1);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_REF_CONTROL_SHADOW;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_REF_CTRL_SHDW_VAL1);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_ZQ_CONFIG;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_ZQ_CONFIG_VAL);
    Register = AM335_EMIF_0_REGISTERS + AM335_EMIF_SDRAM_CONFIG;
    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_CONFIG);
    Register = AM335_SOC_CONTROL_REGISTERS +
               AM335_SOC_CONTROL_SECURE_EMIF_SDRAM_CONFIG;

    AM3_WRITE32(Register, AM335_DDR3_EMIF_SDRAM_CONFIG);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipAm335InitializeMmc0 (
    VOID
    )

/*++

Routine Description:

    This routine sets up the clocking and pin muxing for the MMC/SD 0
    controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    //
    // Set the pad configuration properly so the MMCSD controller owns the pins.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_DAT3;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_DAT2;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_DAT1;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_DAT0;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_CLK;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MMC0_CMD;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_SPI0_CS1;
    Value = (5 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);

    //
    // Enable the clock and wait for it to become enabled.
    //

    Register = AM335_PRCM_REGISTERS + AM335_CM_PER_MMC0_CLOCK_CONTROL;
    Value = AM3_READ32(Register);
    Value |= AM335_CM_PER_MMC0_CLOCK_ENABLE;
    AM3_WRITE32(Register, Value);
    do {
        Value = AM3_READ32(Register);

    } while ((Value & AM335_CM_PER_MMC0_CLOCK_ENABLE) == 0);

    return;
}

VOID
EfipAm335InitializeMmc1 (
    VOID
    )

/*++

Routine Description:

    This routine sets up the clocking and pin muxing for the MMC/SD 0
    controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    //
    // Set the pad configuration properly so the MMC1 controller owns 8 data
    // pins, plus clock and command.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD0;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD1;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD2;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD3;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD4;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD5;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD6;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_AD7;
    Value = (1 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_CSN1;
    Value = (2 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_GPMC_CSN2;
    Value = (2 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_PUDEN_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);

    //
    // Enable the clock and wait for it to become enabled.
    //

    Register = AM335_PRCM_REGISTERS + AM335_CM_PER_MMC1_CLOCK_CONTROL;
    Value = AM3_READ32(Register);
    Value |= AM335_CM_PER_MMC1_CLOCK_ENABLE;
    AM3_WRITE32(Register, Value);
    do {
        Value = AM3_READ32(Register);

    } while ((Value & AM335_CM_PER_MMC1_CLOCK_ENABLE) == 0);

    return;
}

VOID
EfipAm335InitializeEthernet (
    VOID
    )

/*++

Routine Description:

    This routine sets up the clocking and pin muxing for the ethernet
    controller (in MII mode on the BeagleBone Black).

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    //
    // Set up the pin muxing to enable the MII and MDIO lines of the ethernet
    // controller.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_COL;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_CRS;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXERR;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXEN;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXDV;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXD3;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXD2;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXD1;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXD0;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);
    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_TXCLK;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXCLK;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXD3;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXD2;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXD1;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MII1_RXD0;
    Value = (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MDIO_DATA;
    Value = (1 << AM335_SOC_CONF_MUX_RXACTIVE_SHIFT) |
            (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_CONF_MDIO_CLK;
    Value = (1 << AM335_SOC_CONF_MUX_PUTYPESEL_SHIFT) |
            (0 << AM335_SOC_CONF_MUX_MMODE_SHIFT);

    AM3_WRITE32(Register, Value);

    //
    // Select MII internal delay mode.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_GMII_SEL;
    Value = 0;
    AM3_WRITE32(Register, Value);

    //
    // Enable the clocks for the MAC and CPSW and wait for them to become
    // enabled.
    //

    Register = AM335_PRCM_REGISTERS + AM335_CM_PER_CPGMAC0_CLOCK_CONTROL;
    Value |= AM335_CM_PER_CPGMAC0_CLOCK_ENABLE;
    AM3_WRITE32(Register, Value);
    do {
        Value = AM3_READ32(Register) &
                AM335_CM_PER_CPGMAC0_CLOCK_IDLE_STATE_MASK;

    } while (Value != AM335_CM_PER_CPGMAC0_CLOCK_IDLE_STATE_FUNCTIONAL);

    Register = AM335_PRCM_REGISTERS + AM335_CM_PER_CPSW_CLOCK_STATE_CONTROL;
    Value = AM335_CM_PER_CPSW_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_WRITE32(Register, Value);
    do {
        Value = AM3_READ32(Register);

    } while ((Value & AM335_CM_PER_CPSW_CLOCK_STATE_CPSW_125MHZ_GCLK) == 0);

    return;
}

VOID
EfipAm335InitializeUsb1 (
    VOID
    )

/*++

Routine Description:

    This routine sets up the clocking and pin muxing for the USB controller.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_USB_CONTROL1;
    Value = AM3_READ32(Register);
    Value |= AM335_SOC_USB_CONTROL_SESSION_END_DETECT |
             AM335_SOC_USB_CONTROL_VBUS_DETECT;

    Value &= ~(AM335_SOC_USB_CONTROL_CM_PHY_POWER_DOWN |
               AM335_SOC_USB_CONTROL_OTG_PHY_POWER_DOWN);

    AM3_WRITE32(Register, Value);
    return;
}

VOID
EfipBeagleBoneBlackInitializeDdr3Phy (
    VOID
    )

/*++

Routine Description:

    This routine initializes the DDR3 PHY for the BeagleBone Black.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Register;
    UINT32 Value;

    //
    // Enable VTP.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_VTP_CONTROL;
    Value = AM3_READ32(Register);
    Value |= AM335_SOC_CONTROL_VTP_CONTROL_ENABLE;
    AM3_WRITE32(Register, Value);
    Value &= ~AM335_SOC_CONTROL_VTP_CONTROL_CLRZ;
    AM3_WRITE32(Register, Value);
    Value |= AM335_SOC_CONTROL_VTP_CONTROL_CLRZ;
    AM3_WRITE32(Register, Value);
    do {
        Value = AM3_READ32(Register);

    } while ((Value & AM335_SOC_CONTROL_VTP_CONTROL_READY) == 0);

    //
    // Configure the DDR PHY CMD0 register.
    //

    AM3_WRITE32(AM335_DDR_CMD0_SLAVE_RATIO_0, AM335_DDR3_CMD0_SLAVE_RATIO_0);
    AM3_WRITE32(AM335_DDR_CMD0_INVERT_CLKOUT_0,
                AM335_DDR3_CMD0_INVERT_CLKOUT_0);

    //
    // Configure the DDR PHY CMD1 register.
    //

    AM3_WRITE32(AM335_DDR_CMD1_SLAVE_RATIO_0,
                AM335_DDR3_CMD1_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_CMD1_INVERT_CLKOUT_0,
                AM335_DDR3_CMD1_INVERT_CLKOUT_0);

    //
    // Configure the DDR PHY CMD2 register.
    //

    AM3_WRITE32(AM335_DDR_CMD2_SLAVE_RATIO_0,
                AM335_DDR3_CMD2_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_CMD2_INVERT_CLKOUT_0,
                AM335_DDR3_CMD2_INVERT_CLKOUT_0);

    //
    // Perform DATA configuration.
    //

    AM3_WRITE32(AM335_DDR_DATA0_RD_DQS_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_RD_DQS_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_DATA0_WR_DQS_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_WR_DQS_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_DATA0_FIFO_WE_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_FIFO_WE_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_DATA0_WR_DATA_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_WR_DATA_SLAVE_RATIO_0);

    AM3_WRITE32(AM335_DDR_DATA1_RD_DQS_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_RD_DQS_SLAVE_RATIO_1);

    AM3_WRITE32(AM335_DDR_DATA1_WR_DQS_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_WR_DQS_SLAVE_RATIO_1);

    AM3_WRITE32(AM335_DDR_DATA1_FIFO_WE_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_FIFO_WE_SLAVE_RATIO_1);

    AM3_WRITE32(AM335_DDR_DATA1_WR_DATA_SLAVE_RATIO_0,
                AM335_DDR3_DATA0_WR_DATA_SLAVE_RATIO_1);

    return;
}

