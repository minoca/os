/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mux.c

Abstract:

    This module implements board pin muxing setup for the PandaBoard.

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
// This macro writes to a padconf register.
//

#define MV(_Offset, _Value) \
    OMAP4_WRITE16(OMAP4430_CTRL_PADCONF_CORE_BASE + (_Offset), _Value)

//
// This macro writes to a wakeup register.
//

#define MVW(_Offset, _Value) \
    OMAP4_WRITE16(OMAP4430_WAKEUP_CONTROL_BASE + (_Offset), _Value)

//
// This macro prepends the padconf text.
//

#define PADCONF(_Register) (CONTROL_PADCONF_##_Register)

//
// This macro prepends the wakeup text.
//

#define WAKEUP(_Register) (CONTROL_WKUP_##_Register)

//
// This macro reads from and writes to EMIF registers.
//

#define EMIF_READ(_Register) OMAP4_READ32(_Register)
#define EMIF_WRITE(_Value, _Register) OMAP4_WRITE32(_Register, _Value)

#define CS1_MR(_MrValue) ((_MrValue) | 0x80000000)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define pin muxing values.
//

#ifdef CONFIG_OFF_PADCONF

//
// Off mode pull down
//

#define OFF_PD          (1 << 12)

//
// Off mode pull up
//

#define OFF_PU          (3 << 12)

//
// Off mode mux low out
//

#define OFF_OUT_PTD     (0 << 10)

//
// Off mode mux high out
//

#define OFF_OUT_PTU     (2 << 10)

//
// Off mode in
//

#define OFF_IN          (1 << 10)

//
// Off mode out
//

#define OFF_OUT         (0 << 10)

//
// Off mode enable
//

#define OFF_EN          (1 << 9)

#else

#define OFF_PD          (0 << 12)
#define OFF_PU          (0 << 12)
#define OFF_OUT_PTD     (0 << 10)
#define OFF_OUT_PTU     (0 << 10)
#define OFF_IN          (0 << 10)
#define OFF_OUT         (0 << 10)
#define OFF_EN          (0 << 9)

#endif

//
// Input enable
//

#define IEN             (1 << 8)

//
// Input disable
//

#define IDIS            (0 << 8)

//
// Pull type up
//

#define PTU             (3 << 3)

//
// Pull type down
//

#define PTD             (1 << 3)

//
// Pull type selection active
//

#define EN              (1 << 3)

//
// Pull type selection inactive
//

#define DIS             (0 << 3)

//
// Pin muxing modes (pin-dependent)
//

#define M0              0
#define M1              1
#define M2              2
#define M3              3
#define M4              4
#define M5              5
#define M6              6
#define M7              7

#ifdef CONFIG_OFF_PADCONF

#define OFF_IN_PD       (OFF_PD | OFF_IN | OFF_EN)
#define OFF_IN_PU       (OFF_PU | OFF_IN | OFF_EN)
#define OFF_OUT_PD      (OFF_OUT_PTD | OFF_OUT | OFF_EN)
#define OFF_OUT_PU      (OFF_OUT_PTU | OFF_OUT | OFF_EN)

#else

#define OFF_IN_PD       0
#define OFF_IN_PU       0
#define OFF_OUT_PD      0
#define OFF_OUT_PU      0

#endif

//
// Define pad configuration registers
//

#define CONTROL_PADCONF_CORE_REVISION       0x0000
#define CONTROL_PADCONF_CORE_HWINFO         0x0004
#define CONTROL_PADCONF_CORE_SYSCONFIG      0x0010
#define CONTROL_PADCONF_GPMC_AD0            0x0040
#define CONTROL_PADCONF_GPMC_AD1            0x0042
#define CONTROL_PADCONF_GPMC_AD2            0x0044
#define CONTROL_PADCONF_GPMC_AD3            0x0046
#define CONTROL_PADCONF_GPMC_AD4            0x0048
#define CONTROL_PADCONF_GPMC_AD5            0x004A
#define CONTROL_PADCONF_GPMC_AD6            0x004C
#define CONTROL_PADCONF_GPMC_AD7            0x004E
#define CONTROL_PADCONF_GPMC_AD8            0x0050
#define CONTROL_PADCONF_GPMC_AD9            0x0052
#define CONTROL_PADCONF_GPMC_AD10           0x0054
#define CONTROL_PADCONF_GPMC_AD11           0x0056
#define CONTROL_PADCONF_GPMC_AD12           0x0058
#define CONTROL_PADCONF_GPMC_AD13           0x005A
#define CONTROL_PADCONF_GPMC_AD14           0x005C
#define CONTROL_PADCONF_GPMC_AD15           0x005E
#define CONTROL_PADCONF_GPMC_A16            0x0060
#define CONTROL_PADCONF_GPMC_A17            0x0062
#define CONTROL_PADCONF_GPMC_A18            0x0064
#define CONTROL_PADCONF_GPMC_A19            0x0066
#define CONTROL_PADCONF_GPMC_A20            0x0068
#define CONTROL_PADCONF_GPMC_A21            0x006A
#define CONTROL_PADCONF_GPMC_A22            0x006C
#define CONTROL_PADCONF_GPMC_A23            0x006E
#define CONTROL_PADCONF_GPMC_A24            0x0070
#define CONTROL_PADCONF_GPMC_A25            0x0072
#define CONTROL_PADCONF_GPMC_NCS0           0x0074
#define CONTROL_PADCONF_GPMC_NCS1           0x0076
#define CONTROL_PADCONF_GPMC_NCS2           0x0078
#define CONTROL_PADCONF_GPMC_NCS3           0x007A
#define CONTROL_PADCONF_GPMC_NWP            0x007C
#define CONTROL_PADCONF_GPMC_CLK            0x007E
#define CONTROL_PADCONF_GPMC_NADV_ALE       0x0080
#define CONTROL_PADCONF_GPMC_NOE            0x0082
#define CONTROL_PADCONF_GPMC_NWE            0x0084
#define CONTROL_PADCONF_GPMC_NBE0_CLE       0x0086
#define CONTROL_PADCONF_GPMC_NBE1           0x0088
#define CONTROL_PADCONF_GPMC_WAIT0          0x008A
#define CONTROL_PADCONF_GPMC_WAIT1          0x008C
#define CONTROL_PADCONF_C2C_DATA11          0x008E
#define CONTROL_PADCONF_C2C_DATA12          0x0090
#define CONTROL_PADCONF_C2C_DATA13          0x0092
#define CONTROL_PADCONF_C2C_DATA14          0x0094
#define CONTROL_PADCONF_C2C_DATA15          0x0096
#define CONTROL_PADCONF_HDMI_HPD            0x0098
#define CONTROL_PADCONF_HDMI_CEC            0x009A
#define CONTROL_PADCONF_HDMI_DDC_SCL        0x009C
#define CONTROL_PADCONF_HDMI_DDC_SDA        0x009E
#define CONTROL_PADCONF_CSI21_DX0           0x00A0
#define CONTROL_PADCONF_CSI21_DY0           0x00A2
#define CONTROL_PADCONF_CSI21_DX1           0x00A4
#define CONTROL_PADCONF_CSI21_DY1           0x00A6
#define CONTROL_PADCONF_CSI21_DX2           0x00A8
#define CONTROL_PADCONF_CSI21_DY2           0x00AA
#define CONTROL_PADCONF_CSI21_DX3           0x00AC
#define CONTROL_PADCONF_CSI21_DY3           0x00AE
#define CONTROL_PADCONF_CSI21_DX4           0x00B0
#define CONTROL_PADCONF_CSI21_DY4           0x00B2
#define CONTROL_PADCONF_CSI22_DX0           0x00B4
#define CONTROL_PADCONF_CSI22_DY0           0x00B6
#define CONTROL_PADCONF_CSI22_DX1           0x00B8
#define CONTROL_PADCONF_CSI22_DY1           0x00BA
#define CONTROL_PADCONF_CAM_SHUTTER         0x00BC
#define CONTROL_PADCONF_CAM_STROBE          0x00BE
#define CONTROL_PADCONF_CAM_GLOBALRESET     0x00C0
#define CONTROL_PADCONF_USBB1_ULPITLL_CLK   0x00C2
#define CONTROL_PADCONF_USBB1_ULPITLL_STP   0x00C4
#define CONTROL_PADCONF_USBB1_ULPITLL_DIR   0x00C6
#define CONTROL_PADCONF_USBB1_ULPITLL_NXT   0x00C8
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT0  0x00CA
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT1  0x00CC
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT2  0x00CE
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT3  0x00D0
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT4  0x00D2
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT5  0x00D4
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT6  0x00D6
#define CONTROL_PADCONF_USBB1_ULPITLL_DAT7  0x00D8
#define CONTROL_PADCONF_USBB1_HSIC_DATA     0x00DA
#define CONTROL_PADCONF_USBB1_HSIC_STROBE   0x00DC
#define CONTROL_PADCONF_USBC1_ICUSB_DP      0x00DE
#define CONTROL_PADCONF_USBC1_ICUSB_DM      0x00E0
#define CONTROL_PADCONF_SDMMC1_CLK          0x00E2
#define CONTROL_PADCONF_SDMMC1_CMD          0x00E4
#define CONTROL_PADCONF_SDMMC1_DAT0         0x00E6
#define CONTROL_PADCONF_SDMMC1_DAT1         0x00E8
#define CONTROL_PADCONF_SDMMC1_DAT2         0x00EA
#define CONTROL_PADCONF_SDMMC1_DAT3         0x00EC
#define CONTROL_PADCONF_SDMMC1_DAT4         0x00EE
#define CONTROL_PADCONF_SDMMC1_DAT5         0x00F0
#define CONTROL_PADCONF_SDMMC1_DAT6         0x00F2
#define CONTROL_PADCONF_SDMMC1_DAT7         0x00F4
#define CONTROL_PADCONF_ABE_MCBSP2_CLKX     0x00F6
#define CONTROL_PADCONF_ABE_MCBSP2_DR       0x00F8
#define CONTROL_PADCONF_ABE_MCBSP2_DX       0x00FA
#define CONTROL_PADCONF_ABE_MCBSP2_FSX      0x00FC
#define CONTROL_PADCONF_ABE_MCBSP1_CLKX     0x00FE
#define CONTROL_PADCONF_ABE_MCBSP1_DR       0x0100
#define CONTROL_PADCONF_ABE_MCBSP1_DX       0x0102
#define CONTROL_PADCONF_ABE_MCBSP1_FSX      0x0104
#define CONTROL_PADCONF_ABE_PDM_UL_DATA     0x0106
#define CONTROL_PADCONF_ABE_PDM_DL_DATA     0x0108
#define CONTROL_PADCONF_ABE_PDM_FRAME       0x010A
#define CONTROL_PADCONF_ABE_PDM_LB_CLK      0x010C
#define CONTROL_PADCONF_ABE_CLKS            0x010E
#define CONTROL_PADCONF_ABE_DMIC_CLK1       0x0110
#define CONTROL_PADCONF_ABE_DMIC_DIN1       0x0112
#define CONTROL_PADCONF_ABE_DMIC_DIN2       0x0114
#define CONTROL_PADCONF_ABE_DMIC_DIN3       0x0116
#define CONTROL_PADCONF_UART2_CTS           0x0118
#define CONTROL_PADCONF_UART2_RTS           0x011A
#define CONTROL_PADCONF_UART2_RX            0x011C
#define CONTROL_PADCONF_UART2_TX            0x011E
#define CONTROL_PADCONF_HDQ_SIO             0x0120
#define CONTROL_PADCONF_I2C1_SCL            0x0122
#define CONTROL_PADCONF_I2C1_SDA            0x0124
#define CONTROL_PADCONF_I2C2_SCL            0x0126
#define CONTROL_PADCONF_I2C2_SDA            0x0128
#define CONTROL_PADCONF_I2C3_SCL            0x012A
#define CONTROL_PADCONF_I2C3_SDA            0x012C
#define CONTROL_PADCONF_I2C4_SCL            0x012E
#define CONTROL_PADCONF_I2C4_SDA            0x0130
#define CONTROL_PADCONF_MCSPI1_CLK          0x0132
#define CONTROL_PADCONF_MCSPI1_SOMI         0x0134
#define CONTROL_PADCONF_MCSPI1_SIMO         0x0136
#define CONTROL_PADCONF_MCSPI1_CS0          0x0138
#define CONTROL_PADCONF_MCSPI1_CS1          0x013A
#define CONTROL_PADCONF_MCSPI1_CS2          0x013C
#define CONTROL_PADCONF_MCSPI1_CS3          0x013E
#define CONTROL_PADCONF_UART3_CTS_RCTX      0x0140
#define CONTROL_PADCONF_UART3_RTS_SD        0x0142
#define CONTROL_PADCONF_UART3_RX_IRRX       0x0144
#define CONTROL_PADCONF_UART3_TX_IRTX       0x0146
#define CONTROL_PADCONF_SDMMC5_CLK          0x0148
#define CONTROL_PADCONF_SDMMC5_CMD          0x014A
#define CONTROL_PADCONF_SDMMC5_DAT0         0x014C
#define CONTROL_PADCONF_SDMMC5_DAT1         0x014E
#define CONTROL_PADCONF_SDMMC5_DAT2         0x0150
#define CONTROL_PADCONF_SDMMC5_DAT3         0x0152
#define CONTROL_PADCONF_MCSPI4_CLK          0x0154
#define CONTROL_PADCONF_MCSPI4_SIMO         0x0156
#define CONTROL_PADCONF_MCSPI4_SOMI         0x0158
#define CONTROL_PADCONF_MCSPI4_CS0          0x015A
#define CONTROL_PADCONF_UART4_RX            0x015C
#define CONTROL_PADCONF_UART4_TX            0x015E
#define CONTROL_PADCONF_USBB2_ULPITLL_CLK   0x0160
#define CONTROL_PADCONF_USBB2_ULPITLL_STP   0x0162
#define CONTROL_PADCONF_USBB2_ULPITLL_DIR   0x0164
#define CONTROL_PADCONF_USBB2_ULPITLL_NXT   0x0166
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT0  0x0168
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT1  0x016A
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT2  0x016C
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT3  0x016E
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT4  0x0170
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT5  0x0172
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT6  0x0174
#define CONTROL_PADCONF_USBB2_ULPITLL_DAT7  0x0176
#define CONTROL_PADCONF_USBB2_HSIC_DATA     0x0178
#define CONTROL_PADCONF_USBB2_HSIC_STROBE   0x017A
#define CONTROL_PADCONF_UNIPRO_TX0          0x017C
#define CONTROL_PADCONF_UNIPRO_TY0          0x017E
#define CONTROL_PADCONF_UNIPRO_TX1          0x0180
#define CONTROL_PADCONF_UNIPRO_TY1          0x0182
#define CONTROL_PADCONF_UNIPRO_TX2          0x0184
#define CONTROL_PADCONF_UNIPRO_TY2          0x0186
#define CONTROL_PADCONF_UNIPRO_RX0          0x0188
#define CONTROL_PADCONF_UNIPRO_RY0          0x018A
#define CONTROL_PADCONF_UNIPRO_RX1          0x018C
#define CONTROL_PADCONF_UNIPRO_RY1          0x018E
#define CONTROL_PADCONF_UNIPRO_RX2          0x0190
#define CONTROL_PADCONF_UNIPRO_RY2          0x0192
#define CONTROL_PADCONF_USBA0_OTG_CE        0x0194
#define CONTROL_PADCONF_USBA0_OTG_DP        0x0196
#define CONTROL_PADCONF_USBA0_OTG_DM        0x0198
#define CONTROL_PADCONF_FREF_CLK1_OUT       0x019A
#define CONTROL_PADCONF_FREF_CLK2_OUT       0x019C
#define CONTROL_PADCONF_SYS_NIRQ1           0x019E
#define CONTROL_PADCONF_SYS_NIRQ2           0x01A0
#define CONTROL_PADCONF_SYS_BOOT0           0x01A2
#define CONTROL_PADCONF_SYS_BOOT1           0x01A4
#define CONTROL_PADCONF_SYS_BOOT2           0x01A6
#define CONTROL_PADCONF_SYS_BOOT3           0x01A8
#define CONTROL_PADCONF_SYS_BOOT4           0x01AA
#define CONTROL_PADCONF_SYS_BOOT5           0x01AC
#define CONTROL_PADCONF_DPM_EMU0            0x01AE
#define CONTROL_PADCONF_DPM_EMU1            0x01B0
#define CONTROL_PADCONF_DPM_EMU2            0x01B2
#define CONTROL_PADCONF_DPM_EMU3            0x01B4
#define CONTROL_PADCONF_DPM_EMU4            0x01B6
#define CONTROL_PADCONF_DPM_EMU5            0x01B8
#define CONTROL_PADCONF_DPM_EMU6            0x01BA
#define CONTROL_PADCONF_DPM_EMU7            0x01BC
#define CONTROL_PADCONF_DPM_EMU8            0x01BE
#define CONTROL_PADCONF_DPM_EMU9            0x01C0
#define CONTROL_PADCONF_DPM_EMU10           0x01C2
#define CONTROL_PADCONF_DPM_EMU11           0x01C4
#define CONTROL_PADCONF_DPM_EMU12           0x01C6
#define CONTROL_PADCONF_DPM_EMU13           0x01C8
#define CONTROL_PADCONF_DPM_EMU14           0x01CA
#define CONTROL_PADCONF_DPM_EMU15           0x01CC
#define CONTROL_PADCONF_DPM_EMU16           0x01CE
#define CONTROL_PADCONF_DPM_EMU17           0x01D0
#define CONTROL_PADCONF_DPM_EMU18           0x01D2
#define CONTROL_PADCONF_DPM_EMU19           0x01D4
#define CONTROL_PADCONF_WAKEUPEVENT_1       0x01DC
#define CONTROL_PADCONF_WAKEUPEVENT_2       0x01E0
#define CONTROL_PADCONF_WAKEUPEVENT_3       0x01E4
#define CONTROL_PADCONF_WAKEUPEVENT_4       0x01E8
#define CONTROL_PADCONF_WAKEUPEVENT_5       0x01EC
#define CONTROL_PADCONF_WAKEUPEVENT_6       0x01F0

#define CONTROL_PADCONF_GLOBAL              0x05A2
#define CONTROL_PADCONF_MODE                0x05A4
#define CONTROL_SMART1IO_PADCONF_0          0x05A8
#define CONTROL_SMART1IO_PADCONF_1          0x05AC
#define CONTROL_SMART2IO_PADCONF_0          0x05B0
#define CONTROL_SMART2IO_PADCONF_1          0x05B4
#define CONTROL_SMART3IO_PADCONF_0          0x05B8
#define CONTROL_SMART3IO_PADCONF_1          0x05BC
#define CONTROL_SMART3IO_PADCONF_2          0x05C0
#define CONTROL_USBB_HSIC                   0x05C4
#define CONTROL_SLIMBUS                     0x05C8
#define CONTROL_PBIASLITE                   0x0600
#define CONTROL_I2C_0                       0x0604
#define CONTROL_CAMERA_RX                   0x0608
#define CONTROL_AVDAC                       0x060C
#define CONTROL_HDMI_TX_PHY                 0x0610
#define CONTROL_MMC2                        0x0614
#define CONTROL_DSIPHY                      0x0618
#define CONTROL_MCBSPLP                     0x061C
#define CONTROL_USB2PHYCORE                 0x0620
#define CONTROL_I2C_1                       0x0624
#define CONTROL_MMC1                        0x0628
#define CONTROL_HSI                         0x062C
#define CONTROL_USB                         0x0630
#define CONTROL_HDQ                         0x0634
#define CONTROL_LPDDR2IO1_0                 0x0638
#define CONTROL_LPDDR2IO1_1                 0x063C
#define CONTROL_LPDDR2IO1_2                 0x0640
#define CONTROL_LPDDR2IO1_3                 0x0644
#define CONTROL_LPDDR2IO2_0                 0x0648
#define CONTROL_LPDDR2IO2_1                 0x064C
#define CONTROL_LPDDR2IO2_2                 0x0650
#define CONTROL_LPDDR2IO2_3                 0x0654
#define CONTROL_BUS_HOLD                    0x0658
#define CONTROL_C2C                         0x065C
#define CONTROL_CORE_CONTROL_SPARE_RW       0x0660
#define CONTROL_CORE_CONTROL_SPARE_R        0x0664
#define CONTROL_CORE_CONTROL_SPARE_R_C0     0x0668
#define CONTROL_EFUSE_1                     0x0700
#define CONTROL_EFUSE_2                     0x0704
#define CONTROL_EFUSE_3                     0x0708
#define CONTROL_EFUSE_4                     0x070C

#define CONTROL_PADCONF_WKUP_REVISION       0x0000
#define CONTROL_PADCONF_WKUP_HWINFO         0x0004
#define CONTROL_PADCONF_WKUP_SYSCONFIG      0x0010
#define CONTROL_WKUP_PAD0_SIM_IO            0x0040
#define CONTROL_WKUP_PAD1_SIM_CLK           0x0042
#define CONTROL_WKUP_PAD0_SIM_RESET         0x0044
#define CONTROL_WKUP_PAD1_SIM_CD            0x0046
#define CONTROL_WKUP_PAD0_SIM_PWRCTRL       0x0048
#define CONTROL_WKUP_PAD1_SR_SCL            0x004A
#define CONTROL_WKUP_PAD0_SR_SDA            0x004C
#define CONTROL_WKUP_PAD1_FREF_XTAL_IN      0x004E
#define CONTROL_WKUP_PAD0_FREF_SLICER_IN    0x0050
#define CONTROL_WKUP_PAD1_FREF_CLK_IOREQ    0x0052
#define CONTROL_WKUP_PAD0_FREF_CLK0_OUT     0x0054
#define CONTROL_WKUP_PAD1_FREF_CLK3_REQ     0x0056
#define CONTROL_WKUP_PAD0_FREF_CLK3_OUT     0x0058
#define CONTROL_WKUP_PAD1_FREF_CLK4_REQ     0x005A
#define CONTROL_WKUP_PAD0_FREF_CLK4_OUT     0x005C
#define CONTROL_WKUP_PAD1_SYS_32K           0x005E
#define CONTROL_WKUP_PAD0_SYS_NRESPWRON     0x0060
#define CONTROL_WKUP_PAD1_SYS_NRESWARM      0x0062
#define CONTROL_WKUP_PAD0_SYS_PWR_REQ       0x0064
#define CONTROL_WKUP_PAD1_SYS_PWRON_RESET   0x0066
#define CONTROL_WKUP_PAD0_SYS_BOOT6         0x0068
#define CONTROL_WKUP_PAD1_SYS_BOOT7         0x006A
#define CONTROL_WKUP_PAD0_JTAG_NTRST        0x006C
#define CONTROL_WKUP_PAD1_JTAG_TCK          0x006D
#define CONTROL_WKUP_PAD0_JTAG_RTCK         0x0070
#define CONTROL_WKUP_PAD1_JTAG_TMS_TMSC     0x0072
#define CONTROL_WKUP_PAD0_JTAG_TDI          0x0074
#define CONTROL_WKUP_PAD1_JTAG_TDO          0x0076
#define CONTROL_PADCONF_WAKEUPEVENT_0       0x007C
#define CONTROL_SMART1NOPMIO_PADCONF_0      0x05A0
#define CONTROL_SMART1NOPMIO_PADCONF_1      0x05A4
#define CONTROL_XTAL_OSCILLATOR             0x05AC
#define CONTROL_CONTROL_I2C_2               0x0604
#define CONTROL_CONTROL_JTAG                0x0608
#define CONTROL_CONTROL_SYS                 0x060C
#define CONTROL_WKUP_CONTROL_SPARE_RW       0x0614
#define CONTROL_WKUP_CONTROL_SPARE_R        0x0618
#define CONTROL_WKUP_CONTROL_SPARE_R_C0     0x061C

//
// EMIF and DMM registers
//

#define EMIF1_BASE          0x4C000000
#define EMIF2_BASE          0x4D000000
#define DMM_BASE            0x4E000000
#define MA_BASE             0x482AF000

//
// EMIF registers
//

#define EMIF_MOD_ID_REV             0x0000
#define EMIF_STATUS                 0x0004
#define EMIF_SDRAM_CONFIG           0x0008
#define EMIF_LPDDR2_NVM_CONFIG      0x000C
#define EMIF_SDRAM_REF_CTRL         0x0010
#define EMIF_SDRAM_REF_CTRL_SHDW    0x0014
#define EMIF_SDRAM_TIM_1            0x0018
#define EMIF_SDRAM_TIM_1_SHDW       0x001C
#define EMIF_SDRAM_TIM_2            0x0020
#define EMIF_SDRAM_TIM_2_SHDW       0x0024
#define EMIF_SDRAM_TIM_3            0x0028
#define EMIF_SDRAM_TIM_3_SHDW       0x002C
#define EMIF_LPDDR2_NVM_TIM         0x0030
#define EMIF_LPDDR2_NVM_TIM_SHDW    0x0034
#define EMIF_PWR_MGMT_CTRL          0x0038
#define EMIF_PWR_MGMT_CTRL_SHDW     0x003C
#define EMIF_LPDDR2_MODE_REG_DATA   0x0040
#define EMIF_LPDDR2_MODE_REG_CFG    0x0050
#define EMIF_L3_CONFIG              0x0054
#define EMIF_L3_CFG_VAL_1           0x0058
#define EMIF_L3_CFG_VAL_2           0x005C
#define IODFT_TLGC                  0x0060
#define EMIF_PERF_CNT_1             0x0080
#define EMIF_PERF_CNT_2             0x0084
#define EMIF_PERF_CNT_CFG           0x0088
#define EMIF_PERF_CNT_SEL           0x008C
#define EMIF_PERF_CNT_TIM           0x0090
#define EMIF_READ_IDLE_CTRL         0x0098
#define EMIF_READ_IDLE_CTRL_SHDW    0x009c
#define EMIF_ZQ_CONFIG              0x00C8
#define EMIF_DDR_PHY_CTRL_1         0x00E4
#define EMIF_DDR_PHY_CTRL_1_SHDW    0x00E8
#define EMIF_DDR_PHY_CTRL_2         0x00EC

#define DMM_LISA_MAP_0              0x0040
#define DMM_LISA_MAP_1              0x0044
#define DMM_LISA_MAP_2              0x0048
#define DMM_LISA_MAP_3              0x004C

//
// Elpida 2x2Gbit values
//

#define SDRAM_CONFIG_INIT           0x80800EB1
#define DDR_PHY_CTRL_1_INIT         0x849FFFF5
#define READ_IDLE_CTRL              0x000501FF
#define PWR_MGMT_CTRL               0x4000000f
#define PWR_MGMT_CTRL_OPP100        0x4000000f
#define ZQ_CONFIG                   0x500B3215

#define MR0_ADDR                    0
#define MR1_ADDR                    1
#define MR2_ADDR                    2
#define MR4_ADDR                    4
#define MR10_ADDR                   10
#define MR16_ADDR                   16
#define REF_EN                      0x40000000

#define MR10_ZQINIT                 0xFF

//
// GPMC definitions
//

#define GPMC_BASE OMAP4430_GPMC_BASE

#define GPMC_CONFIG_CS0     0x60
#define GPMC_CONFIG_WIDTH   0x30

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _OMAP4_DDR_CONFIGURATION {
    UINT32 Timing1;
    UINT32 Timing2;
    UINT32 Timing3;
    UINT32 PhyControl1;
    UINT32 RefControl;
    UINT32 ConfigInit;
    UINT32 ConfigFinal;
    UINT32 ZqConfig;
    UINT8 Mr1;
    UINT8 Mr2;
} OMAP4_DDR_CONFIGURATION, *POMAP4_DDR_CONFIGURATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipOmap4InitializeDdrRam (
    POMAP4_DDR_CONFIGURATION Emif1Registers,
    POMAP4_DDR_CONFIGURATION Emif2Registers
    );

VOID
EfipOmap4ConfigureEmif (
    UINT32 Base,
    POMAP4_DDR_CONFIGURATION EmifParameters
    );

VOID
EfipResetEmifPhy (
    UINT32 Base
    );

BOOLEAN
EfipPandaEsIsRevisionB3 (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

OMAP4_DDR_CONFIGURATION EfiElpida2G400Mhz2CsConfiguration = {
    0x10EB0662,
    0x20370DD2,
    0x00B1C33F,
    0x849FF408,
    0x00000618,
    0x80000EB9,
    0x80001AB9,
    0xD00B3215,
    0x83,
    0x04
};

//
// Memory timings are different for PandaBoard ES Revision B3.
//

OMAP4_DDR_CONFIGURATION EfiElpida2G400Mhz1CsConfiguration = {
    0x10EB0662,
    0x20370DD2,
    0x00B1C33F,
    0x049FF418,
    0x00000618,
    0x80800EB2,
    0x80801AB2,
    0x500B3215,
    0x83,
    0x04
};

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipInitializeBoardMux (
    VOID
    )

/*++

Routine Description:

    This routine sets up the correct pin muxing for the PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // Set sdmmc2_dat0 through 7.
    //

    MV(PADCONF(GPMC_AD0), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD1), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD2), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD3), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD4), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD5), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD6), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_AD7), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set gpio_32 through 41.
    //

    MV(PADCONF(GPMC_AD8), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M3));
    MV(PADCONF(GPMC_AD9), (PTU | IEN | M3));
    MV(PADCONF(GPMC_AD10), (PTU | IEN | M3));
    MV(PADCONF(GPMC_AD11), (PTU | IEN | M3));
    MV(PADCONF(GPMC_AD12), (PTU | IEN | M3));
    MV(PADCONF(GPMC_AD13), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M3));
    MV(PADCONF(GPMC_AD14), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M3));
    MV(PADCONF(GPMC_AD15), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M3));
    MV(PADCONF(GPMC_A16), (M3));
    MV(PADCONF(GPMC_A17), (PTD | M3));

    //
    // Set kpd_row6 and 7.
    //

    MV(PADCONF(GPMC_A18), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(GPMC_A19), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set gpio_44 through 46.
    //

    MV(PADCONF(GPMC_A20), (IEN | M3));
    MV(PADCONF(GPMC_A21), (M3));
    MV(PADCONF(GPMC_A22), (M3));

    //
    // Set kpd_col7.
    //

    MV(PADCONF(GPMC_A23), (OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set gpio_48 through 56.
    //

    MV(PADCONF(GPMC_A24), (PTD | M3));
    MV(PADCONF(GPMC_A25), (PTD | M3));
    MV(PADCONF(GPMC_NCS0), (M3));
    MV(PADCONF(GPMC_NCS1), (IEN | M3));
    MV(PADCONF(GPMC_NCS2), (IEN | M3));
    MV(PADCONF(GPMC_NCS3), (IEN | M3));
    MV(PADCONF(GPMC_NWP), (M3));
    MV(PADCONF(GPMC_CLK), (PTD | M3));
    MV(PADCONF(GPMC_NADV_ALE), (M3));

    //
    // Set sdmmc2_clk and sdmmc2_cmd.
    //

    MV(PADCONF(GPMC_NOE), (PTU | IEN | OFF_EN | OFF_OUT_PTD | M1));
    MV(PADCONF(GPMC_NWE), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set gpio_59 through 62.
    //

    MV(PADCONF(GPMC_NBE0_CLE), (M3));
    MV(PADCONF(GPMC_NBE1), (PTD | M3));
    MV(PADCONF(GPMC_WAIT0), (PTU | IEN | M3));
    MV(PADCONF(GPMC_WAIT1), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M3));

    //
    // Set gpio_100 through 102.
    //

    MV(PADCONF(C2C_DATA11), (PTD | M3));
    MV(PADCONF(C2C_DATA12), (PTU | IEN | M3));
    MV(PADCONF(C2C_DATA13), (PTD | M3));

    //
    // Set dsi2_te0.
    //

    MV(PADCONF(C2C_DATA14), ( M1));

    //
    // Set gpio_104.
    //

    MV(PADCONF(C2C_DATA15), (PTD | M3));

    //
    // Set hdmi_hpd and cec.
    //

    MV(PADCONF(HDMI_HPD), (M0));
    MV(PADCONF(HDMI_CEC), (M0));

    //
    // Set hdmi_ddc_scl and sca.
    //

    MV(PADCONF(HDMI_DDC_SCL), (PTU | M0));
    MV(PADCONF(HDMI_DDC_SDA), (PTU | IEN | M0));

    //
    // Set csi21_dxN and csi21_dyN where N is 0 through 4
    // (ie dx0, dy0, dx1, dy1, etc).
    //

    MV(PADCONF(CSI21_DX0), (IEN | M0));
    MV(PADCONF(CSI21_DY0), (IEN | M0));
    MV(PADCONF(CSI21_DX1), (IEN | M0));
    MV(PADCONF(CSI21_DY1), (IEN | M0));
    MV(PADCONF(CSI21_DX2), (IEN | M0));
    MV(PADCONF(CSI21_DY2), (IEN | M0));
    MV(PADCONF(CSI21_DX3), (PTD | M7));
    MV(PADCONF(CSI21_DY3), (PTD | M7));
    MV(PADCONF(CSI21_DX4), (PTD | OFF_EN | OFF_PD | OFF_IN | M7));
    MV(PADCONF(CSI21_DY4), (PTD | OFF_EN | OFF_PD | OFF_IN | M7));

    //
    // Set csi22_dx0, dy0, dx1, and dy1.
    //

    MV(PADCONF(CSI22_DX0), (IEN | M0));
    MV(PADCONF(CSI22_DY0), (IEN | M0));
    MV(PADCONF(CSI22_DX1), (IEN | M0));
    MV(PADCONF(CSI22_DY1), (IEN | M0));

    //
    // Set cam_shutter and cam_strobe.
    //

    MV(PADCONF(CAM_SHUTTER), (OFF_EN | OFF_PD | OFF_OUT_PTD | M0));
    MV(PADCONF(CAM_STROBE), (OFF_EN | OFF_PD | OFF_OUT_PTD | M0));

    //
    // Set gpio_83.
    //

    MV(PADCONF(CAM_GLOBALRESET), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M3));

    //
    // Set usbb1_ulpiphy_clk, stp, dir, nxt, and dat0 through 7.
    //

    MV(PADCONF(USBB1_ULPITLL_CLK), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_STP), (OFF_EN | OFF_OUT_PTD | M4));
    MV(PADCONF(USBB1_ULPITLL_DIR), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_NXT), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT0), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT1), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT2), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT3), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT4), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT5), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT6), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));
    MV(PADCONF(USBB1_ULPITLL_DAT7), (IEN | OFF_EN | OFF_PD | OFF_IN | M4));

    //
    // Set usbb1_hsic_data and strobe
    //

    MV(PADCONF(USBB1_HSIC_DATA), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(USBB1_HSIC_STROBE), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));

    //
    // Set usbc1_icusb_dp and dm.
    //

    MV(PADCONF(USBC1_ICUSB_DP), (IEN | M0));
    MV(PADCONF(USBC1_ICUSB_DM), (IEN | M0));

    //
    // Set sdmmc1_clk, cmd, and dat0 through 7.
    //

    MV(PADCONF(SDMMC1_CLK), (PTU | OFF_EN | OFF_OUT_PTD | M0));
    MV(PADCONF(SDMMC1_CMD), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT0), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT1), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT2), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT3), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT4), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT5), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT6), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC1_DAT7), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));

    //
    // Set the ABE McBSP clocks. The first one is GPIO 110, the LED on the
    // PandaBoard ES.
    //

    MV(PADCONF(ABE_MCBSP2_CLKX), (PTU | OFF_EN | OFF_OUT_PTU | M3));
    MV(PADCONF(ABE_MCBSP2_DR), (IEN | OFF_EN | OFF_OUT_PTD | M0));
    MV(PADCONF(ABE_MCBSP2_DX), (OFF_EN | OFF_OUT_PTD | M0));
    MV(PADCONF(ABE_MCBSP2_FSX), (PTU | IEN | M3));
    MV(PADCONF(ABE_MCBSP1_CLKX), (IEN | M1));
    MV(PADCONF(ABE_MCBSP1_DR), (IEN | M1));
    MV(PADCONF(ABE_MCBSP1_DX), (OFF_EN | OFF_OUT_PTD | M0));
    MV(PADCONF(ABE_MCBSP1_FSX), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_PDM_UL_DATA), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_PDM_DL_DATA), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_PDM_FRAME), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_PDM_LB_CLK), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_CLKS), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(ABE_DMIC_CLK1), (M0));
    MV(PADCONF(ABE_DMIC_DIN1), (IEN | M0));
    MV(PADCONF(ABE_DMIC_DIN2),   (PTU | IEN | M3));
    MV(PADCONF(ABE_DMIC_DIN3), (IEN | M0));

    //
    // Set UART2 muxing.
    //

    MV(PADCONF(UART2_CTS), (PTU | IEN | M0));
    MV(PADCONF(UART2_RTS), (M0));
    MV(PADCONF(UART2_RX), (PTU | IEN | M0));
    MV(PADCONF(UART2_TX), (M0));

    //
    // Set gpio_127.
    //

    MV(PADCONF(HDQ_SIO), (M3));

    //
    // Set i2c 1 through 4.
    //

    MV(PADCONF(I2C1_SCL), (PTU | IEN | M0));
    MV(PADCONF(I2C1_SDA), (PTU | IEN | M0));
    MV(PADCONF(I2C2_SCL), (PTU | IEN | M0));
    MV(PADCONF(I2C2_SDA), (PTU | IEN | M0));
    MV(PADCONF(I2C3_SCL), (PTU | IEN | M0));
    MV(PADCONF(I2C3_SDA), (PTU | IEN | M0));
    MV(PADCONF(I2C4_SCL), (PTU | IEN | M0));
    MV(PADCONF(I2C4_SDA), (PTU | IEN | M0));

    //
    // Set up the McSPI.
    //

    MV(PADCONF(MCSPI1_CLK), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI1_SOMI), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI1_SIMO), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI1_CS0), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI1_CS1), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M3));
    MV(PADCONF(MCSPI1_CS2), (PTU | OFF_EN | OFF_OUT_PTU | M3));
    MV(PADCONF(MCSPI1_CS3), (PTU | IEN | M3));

    //
    // Set up UART3.
    //

    MV(PADCONF(UART3_CTS_RCTX), (PTU | IEN | M0));
    MV(PADCONF(UART3_RTS_SD), (M0));
    MV(PADCONF(UART3_RX_IRRX), (IEN | M0));
    MV(PADCONF(UART3_TX_IRTX), (M0));

    //
    // Set up SDMMC 5.
    //

    MV(PADCONF(SDMMC5_CLK), (PTU | IEN | OFF_EN | OFF_OUT_PTD | M0));
    MV(PADCONF(SDMMC5_CMD), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC5_DAT0), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC5_DAT1), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC5_DAT2), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(SDMMC5_DAT3), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M0));

    //
    // Set up McSPI 4.
    //

    MV(PADCONF(MCSPI4_CLK), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI4_SIMO), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI4_SOMI), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(MCSPI4_CS0), (PTD | IEN | OFF_EN | OFF_PD | OFF_IN | M0));

    //
    // Set up UART 4.
    //

    MV(PADCONF(UART4_RX), (IEN | M0));
    MV(PADCONF(UART4_TX), (M0));

    //
    // Set up gpio_157.
    //

    MV(PADCONF(USBB2_ULPITLL_CLK), (IEN | M3));

    //
    // Set up dispc2_data23 through 11 (descending).
    //

    MV(PADCONF(USBB2_ULPITLL_STP), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DIR), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_NXT), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT0), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT1), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT2), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT3), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT4), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT5), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT6), (IEN | M5));
    MV(PADCONF(USBB2_ULPITLL_DAT7), (IEN | M5));

    //
    // Set up gpio_169 through 171.
    //

    MV(PADCONF(USBB2_HSIC_DATA), (PTD | OFF_EN | OFF_OUT_PTU | M3));
    MV(PADCONF(USBB2_HSIC_STROBE), (PTD | OFF_EN | OFF_OUT_PTU | M3));
    MV(PADCONF(UNIPRO_TX0), (PTD | IEN | M3));

    //
    // Set up kpd_col1 through 3.
    //

    MV(PADCONF(UNIPRO_TY0), (OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_TX1), (OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_TY1), (OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set up gpio_0 and 1.
    //

    MV(PADCONF(UNIPRO_TX2), (PTU | IEN | M3));
    MV(PADCONF(UNIPRO_TY2), (PTU | IEN | M3));

    //
    // Set up kpd_row0 through 5.
    //

    MV(PADCONF(UNIPRO_RX0), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_RY0), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_RX1), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_RY1), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_RX2), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));
    MV(PADCONF(UNIPRO_RY2), (PTU | IEN | OFF_EN | OFF_PD | OFF_IN | M1));

    //
    // Set up USBA0 OTG.
    //

    MV(PADCONF(USBA0_OTG_CE), (PTD | OFF_EN | OFF_PD | OFF_OUT_PTD | M0));
    MV(PADCONF(USBA0_OTG_DP), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));
    MV(PADCONF(USBA0_OTG_DM), (IEN | OFF_EN | OFF_PD | OFF_IN | M0));

    //
    // Set up fref_clk1_out.
    //

    MV(PADCONF(FREF_CLK1_OUT), (M0));

    //
    // Set up gpio_182.
    //

    MV(PADCONF(FREF_CLK2_OUT), (PTU | IEN | M3));

    //
    // Set up sys_nirq1 and 2.
    //

    MV(PADCONF(SYS_NIRQ1), (PTU | IEN | M0));
    MV(PADCONF(SYS_NIRQ2), (PTU | IEN | M0));
    MV(PADCONF(SYS_BOOT0), (PTU | IEN | M3));

    //
    // Set up gpio_185 through 189.
    //

    MV(PADCONF(SYS_BOOT1), (M3));
    MV(PADCONF(SYS_BOOT2), (PTD | IEN | M3));
    MV(PADCONF(SYS_BOOT3), (M3));
    MV(PADCONF(SYS_BOOT4), (M3));
    MV(PADCONF(SYS_BOOT5), (PTD | IEN | M3));

    //
    // Set up DPM EMU 0 through 2.
    //

    MV(PADCONF(DPM_EMU0), (IEN | M0));
    MV(PADCONF(DPM_EMU1), (IEN | M0));
    MV(PADCONF(DPM_EMU2), (IEN | M0));

    //
    // Set up dispc2_data 10, 9, 16, 17, hsync, pclk, vsync, de, and data8
    // through 0.
    //

    MV(PADCONF(DPM_EMU3), (IEN | M5));
    MV(PADCONF(DPM_EMU4), (IEN | M5));
    MV(PADCONF(DPM_EMU5), (IEN | M5));
    MV(PADCONF(DPM_EMU6), (IEN | M5));
    MV(PADCONF(DPM_EMU7), (IEN | M5));
    MV(PADCONF(DPM_EMU8), (IEN | M5));
    MV(PADCONF(DPM_EMU9), (IEN | M5));
    MV(PADCONF(DPM_EMU10), (IEN | M5));
    MV(PADCONF(DPM_EMU11), (IEN | M5));
    MV(PADCONF(DPM_EMU12), (IEN | M5));
    MV(PADCONF(DPM_EMU13), (IEN | M5));
    MV(PADCONF(DPM_EMU14), (IEN | M5));
    MV(PADCONF(DPM_EMU15), (IEN | M5));

    //
    // Configure GPIO 27.
    //

    MV(PADCONF(DPM_EMU16), (M3));
    MV(PADCONF(DPM_EMU17), (IEN | M5));
    MV(PADCONF(DPM_EMU18), (IEN | M5));
    MV(PADCONF(DPM_EMU19), (IEN | M5));

    //
    // Set up sim_io, clk, reset, cd, and pwrctrl.
    //

    MVW(WAKEUP(PAD0_SIM_IO), (IEN | M0));
    MVW(WAKEUP(PAD1_SIM_CLK), (M0));
    MVW(WAKEUP(PAD0_SIM_RESET), (M0));
    MVW(WAKEUP(PAD1_SIM_CD), (PTU | IEN | M0));
    MVW(WAKEUP(PAD0_SIM_PWRCTRL), (M0));

    //
    // Set up sr_scl and sda.
    //

    MVW(WAKEUP(PAD1_SR_SCL), (PTU | IEN | M0));
    MVW(WAKEUP(PAD0_SR_SDA), (PTU | IEN | M0));

    //
    // Set up the cryscal.
    //

    MVW(WAKEUP(PAD1_FREF_XTAL_IN), (M0));

    //
    // Set up fref_slicer_in and fref_clk_ioreq.
    //

    MVW(WAKEUP(PAD0_FREF_SLICER_IN), (M0));
    MVW(WAKEUP(PAD1_FREF_CLK_IOREQ), (M0));

    //
    // Set up sys_drm_msecure.
    //

    MVW(WAKEUP(PAD0_FREF_CLK0_OUT), (M2));

    //
    // Set up gpio_wk30.
    //

    MVW(WAKEUP(PAD1_FREF_CLK3_REQ), (M3));

    //
    // Set up fref_clk3_out.
    //

    MVW(WAKEUP(PAD0_FREF_CLK3_OUT), (M0));
    MVW(WAKEUP(PAD1_FREF_CLK4_REQ), (PTU | IEN | M0));
    MVW(WAKEUP(PAD0_FREF_CLK4_OUT), (M0));

    //
    // Set up sys_32k, nrespwron, nreswarm, and pwr_req.
    //

    MVW(WAKEUP(PAD1_SYS_32K), (IEN | M0));
    MVW(WAKEUP(PAD0_SYS_NRESPWRON), (M0));
    MVW(WAKEUP(PAD1_SYS_NRESWARM), (M0));
    MVW(WAKEUP(PAD0_SYS_PWR_REQ), (PTU | M0));

    //
    // Set up gpio_wk29, 9, 10, 30, 7, and 8.
    //

    MVW(WAKEUP(PAD1_SYS_PWRON_RESET), (M3));
    MVW(WAKEUP(PAD0_SYS_BOOT6), (IEN | M3));
    MVW(WAKEUP(PAD1_SYS_BOOT7), (IEN | M3));
    MVW(WAKEUP(PAD1_FREF_CLK3_REQ),     (M3));
    MVW(WAKEUP(PAD1_FREF_CLK4_REQ),     (M3));
    MVW(WAKEUP(PAD0_FREF_CLK4_OUT),     (M3));
    return;
}

VOID
EfipInitializeDdr (
    VOID
    )

/*++

Routine Description:

    This routine sets up the DDR RAM on the the PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

{

    POMAP4_DDR_CONFIGURATION DdrConfiguration;

    //
    // Set up 1GB, 128B interleaved.
    //

    OMAP4_WRITE32(DMM_BASE + DMM_LISA_MAP_0, 0x80640300);
    OMAP4_WRITE32(DMM_BASE + DMM_LISA_MAP_2, 0);
    OMAP4_WRITE32(DMM_BASE + DMM_LISA_MAP_3, 0xFF020100);
    if (EfipOmap4GetRevision() > Omap4460RevisionEs10) {
        OMAP4_WRITE32(MA_BASE + DMM_LISA_MAP_0, 0x80640300);
        EfiElpida2G400Mhz2CsConfiguration.PhyControl1 = 0x449FF408;
    }

    DdrConfiguration = &EfiElpida2G400Mhz2CsConfiguration;
    if (EfipPandaEsIsRevisionB3() != FALSE) {
        DdrConfiguration = &EfiElpida2G400Mhz1CsConfiguration;
    }

    EfipOmap4InitializeDdrRam(DdrConfiguration, DdrConfiguration);
    return;
}

VOID
EfipInitializeGpmc (
    VOID
    )

/*++

Routine Description:

    This routine initializes the General Purpose Memory Controller on the
    PandaBoard.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Base;
    UINTN Index;

    Base = GPMC_BASE + GPMC_CONFIG_CS0;
    for (Index = 0; Index < 8; Index += 1) {
        EfipSetRegister32(Base + GPMC_CONFIG_WIDTH + (0x30 * Index),
                          6,
                          1,
                          0);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipOmap4InitializeDdrRam (
    POMAP4_DDR_CONFIGURATION Emif1Registers,
    POMAP4_DDR_CONFIGURATION Emif2Registers
    )

/*++

Routine Description:

    This routine sets up the DDR RAM on the the PandaBoard.

Arguments:

    Emif1Registers - Supplies a pointer to the EMIF1 configuration.

    Emif2Registers - Supplies a pointer to the EMIF2 configuration.

Return Value:

    None.

--*/

{

    //
    // Configure the core DPLL but don't lock it.
    //

    EfipConfigureCoreDpllNoLock();

    //
    // No idle.
    //

    OMAP4_WRITE32(EMIF1_BASE + EMIF_PWR_MGMT_CTRL, 0);
    OMAP4_WRITE32(EMIF2_BASE + EMIF_PWR_MGMT_CTRL, 0);

    //
    // Configure EMIF1 and 2.
    //

    EfipOmap4ConfigureEmif(EMIF1_BASE, Emif1Registers);
    EfipOmap4ConfigureEmif(EMIF2_BASE, Emif2Registers);

    //
    // Lock core using shadow CM_SHADOW_FREQ_CONFIG1.
    //

    EfipLockCoreDpllShadow();

    //
    // Set DLL override to zero.
    //

    OMAP4_WRITE32(CM_DLL_CTRL, 0);
    EfipSpin(200);

    //
    // Wait for the DDR to become ready.
    //

    while (((OMAP4_READ32(EMIF1_BASE + EMIF_STATUS) & 0x04) != 0x04) ||
           ((OMAP4_READ32(EMIF2_BASE + EMIF_STATUS) & 0x04) != 0x04)) {

        EfipSpin(1);
    }

    EfipSetRegister32(CM_MEMIF_EMIF_1_CLKCTRL, 0, 32, 0x1);
    EfipSetRegister32(CM_MEMIF_EMIF_2_CLKCTRL, 0, 32, 0x1);

    //
    // Put the Core Subsystem PD to the ON state.
    //

    OMAP4_WRITE32(EMIF1_BASE + EMIF_PWR_MGMT_CTRL, 0x80000000);
    OMAP4_WRITE32(EMIF2_BASE + EMIF_PWR_MGMT_CTRL, 0x80000000);

    //
    // DMM : DMM_LISA_MAP_0(Section_0)
    // [31:24] SYS_ADDR         0x80
    // [22:20] SYS_SIZE     0x7 - 2Gb
    // [19:18] SDRC_INTLDMM     0x1 - 128 byte
    // [17:16] SDRC_ADDRSPC     0x0
    // [9:8] SDRC_MAP       0x3
    // [7:0] SDRC_ADDR      0X0
    //

    OMAP4_WRITE32(EMIF1_BASE + EMIF_L3_CONFIG, 0x0A300000);
    OMAP4_WRITE32(EMIF2_BASE + EMIF_L3_CONFIG, 0x0A300000);
    EfipResetEmifPhy(EMIF1_BASE);
    EfipResetEmifPhy(EMIF2_BASE);
    OMAP4_WRITE32(0x80000000, 0);
    OMAP4_WRITE32(0x80000080, 0);
    return;
}

VOID
EfipOmap4ConfigureEmif (
    UINT32 Base,
    POMAP4_DDR_CONFIGURATION EmifParameters
    )

/*++

Routine Description:

    This routine configures an EMIF device.

Arguments:

    Base - Supplies the base address of the EMIF device.

    EmifParameters - Supplies a pointer to the EMIF configuration.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // set SDRAM CONFIG register
    // EMIF_SDRAM_CONFIG[31:29] REG_SDRAM_TYPE = 4 for LPDDR2-S4
    // EMIF_SDRAM_CONFIG[28:27] REG_IBANK_POS = 0
    // EMIF_SDRAM_CONFIG[13:10] REG_CL = 3
    // EMIF_SDRAM_CONFIG[6:4] REG_IBANK = 3 - 8 banks
    // EMIF_SDRAM_CONFIG[3] REG_EBANK = 0 - CS0
    // EMIF_SDRAM_CONFIG[2:0] REG_PAGESIZE = 2  - 512- 9 column
    // JDEC specs - S4-2Gb --8 banks -- R0-R13, C0-c8
    //

    EMIF_WRITE(EMIF_READ(Base + EMIF_LPDDR2_NVM_CONFIG) & 0xBFFFFFFF,
               Base + EMIF_LPDDR2_NVM_CONFIG);

    EMIF_WRITE(EmifParameters->ConfigInit, Base + EMIF_SDRAM_CONFIG);
    EMIF_WRITE(DDR_PHY_CTRL_1_INIT, Base + EMIF_DDR_PHY_CTRL_1);
    EMIF_WRITE(EmifParameters->PhyControl1, Base + EMIF_DDR_PHY_CTRL_1_SHDW);
    EMIF_WRITE(READ_IDLE_CTRL, Base + EMIF_READ_IDLE_CTRL);
    EMIF_WRITE(READ_IDLE_CTRL, Base + EMIF_READ_IDLE_CTRL_SHDW);
    EMIF_WRITE(EmifParameters->Timing1, Base + EMIF_SDRAM_TIM_1);
    EMIF_WRITE(EmifParameters->Timing1, Base + EMIF_SDRAM_TIM_1_SHDW);
    EMIF_WRITE(EmifParameters->Timing2, Base + EMIF_SDRAM_TIM_2);
    EMIF_WRITE(EmifParameters->Timing2, Base + EMIF_SDRAM_TIM_2_SHDW);
    EMIF_WRITE(EmifParameters->Timing3, Base + EMIF_SDRAM_TIM_3);
    EMIF_WRITE(EmifParameters->Timing3, Base + EMIF_SDRAM_TIM_3_SHDW);
    EMIF_WRITE(EmifParameters->ZqConfig, Base + EMIF_ZQ_CONFIG);

    //
    // poll MR0 register (DAI bit)
    // REG_CS[31] = 0 -- Mode register command to CS0
    // REG_REFRESH_EN[30] = 1 -- Refresh enable after MRW
    // REG_ADDRESS[7:0] = 00 -- Refresh enable after MRW
    //

    EMIF_WRITE(MR0_ADDR, Base + EMIF_LPDDR2_MODE_REG_CFG);
    do {
        Value = EMIF_READ(Base + EMIF_LPDDR2_MODE_REG_DATA);

    } while ((Value & 0x1) != 0);

    EMIF_WRITE(CS1_MR(MR0_ADDR), Base + EMIF_LPDDR2_MODE_REG_CFG);
    do {
        Value = EMIF_READ(Base + EMIF_LPDDR2_MODE_REG_DATA);

    } while ((Value & 0x1) != 0);

    //
    // Set MR10.
    //

    EMIF_WRITE(MR10_ADDR, Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(MR10_ZQINIT, Base + EMIF_LPDDR2_MODE_REG_DATA);
    EMIF_WRITE(CS1_MR(MR10_ADDR), Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(MR10_ZQINIT, Base + EMIF_LPDDR2_MODE_REG_DATA);

    //
    // Wait for tZQINIT, about 1us.
    //

    EfipSpin(10);

    //
    // Set MR1.
    //

    EMIF_WRITE(MR1_ADDR, Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(EmifParameters->Mr1, Base + EMIF_LPDDR2_MODE_REG_DATA);
    EMIF_WRITE(CS1_MR(MR1_ADDR), Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(EmifParameters->Mr1, Base + EMIF_LPDDR2_MODE_REG_DATA);

    //
    // Set MR2, RL=6 for OPP100.
    //

    EMIF_WRITE(MR2_ADDR, Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(EmifParameters->Mr2, Base + EMIF_LPDDR2_MODE_REG_DATA);
    EMIF_WRITE(CS1_MR(MR2_ADDR), Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(EmifParameters->Mr2, Base + EMIF_LPDDR2_MODE_REG_DATA);

    //
    // Set SDRAM config register with final RL-WL value.
    //

    EMIF_WRITE(EmifParameters->ConfigFinal, Base + EMIF_SDRAM_CONFIG);
    EMIF_WRITE(EmifParameters->PhyControl1, Base + EMIF_DDR_PHY_CTRL_1);

    //
    // EMIF_SDRAM_REF_CTRL
    // refresh rate = DDR_CLK / reg_refresh_rate
    // 3.9 uS = (400MHz) / reg_refresh_rate
    //

    EMIF_WRITE(EmifParameters->RefControl, Base + EMIF_SDRAM_REF_CTRL);
    EMIF_WRITE(EmifParameters->RefControl, Base + EMIF_SDRAM_REF_CTRL_SHDW);

    //
    // Set MR16.
    //

    EMIF_WRITE(MR16_ADDR | REF_EN, Base + EMIF_LPDDR2_MODE_REG_CFG);
    EMIF_WRITE(0, Base + EMIF_LPDDR2_MODE_REG_DATA);
    EMIF_WRITE(CS1_MR(MR16_ADDR | REF_EN),
               Base + EMIF_LPDDR2_MODE_REG_CFG);

    EMIF_WRITE(0, Base + EMIF_LPDDR2_MODE_REG_DATA);
    return;
}

VOID
EfipResetEmifPhy (
    UINT32 Base
    )

/*++

Routine Description:

    This routine resets an EMIF PHY.

Arguments:

    Base - Supplies the EMIF base address.

Return Value:

    None.

--*/

{

    UINT32 Value;

    Value = OMAP4_READ32(Base + IODFT_TLGC) | (1 << 10);
    OMAP4_WRITE32(Base + IODFT_TLGC, Value);
    return;
}

BOOLEAN
EfipPandaEsIsRevisionB3 (
    VOID
    )

/*++

Routine Description:

    This routine determines if this is board revision 3, which uses Elpida
    RAM with different timings.

Arguments:

    None.

Return Value:

    TRUE if this is the PandaBoard ES revision 3.

    FALSE if this is not the PandaBoard ES revision 3.

--*/

{

    OMAP4_REVISION ProcessorRevision;
    UINT32 Result;

    ProcessorRevision = EfipOmap4GetRevision();
    if ((ProcessorRevision >= Omap4460RevisionEs10) &&
        (ProcessorRevision <= Omap4460RevisionEs11)) {

        Result = EfipOmap4GpioRead(171);
        if (Result != 0) {
            return TRUE;
        }
    }

    return FALSE;
}

