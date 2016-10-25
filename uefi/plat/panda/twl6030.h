/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    twl6030.h

Abstract:

    This header contains definitions for the TWL6030 Power Management IC.

Author:

    Evan Green 13-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// I2C chip addresses
//

#define TWL6030_CHIP_PM         0x48

#define TWL6030_CHIP_USB        0x49
#define TWL6030_CHIP_ADC        0x49
#define TWL6030_CHIP_CHARGER    0x49
#define TWL6030_CHIP_PWM        0x49

//
// Slave Address 0x48
//

#define TWL6030_RTC_SECONDS             0x00
#define TWL6030_RTC_MINUTES             0x01
#define TWL6030_RTC_HOURS               0x02
#define TWL6030_RTC_DAYS                0x03
#define TWL6030_RTC_MONTHS              0x04
#define TWL6030_RTC_YEARS               0x05
#define TWL6030_RTC_WEEKS               0x06
#define TWL6030_RTC_ALARM_SECONDS       0x08
#define TWL6030_RTC_ALARM_MINUTES       0x09
#define TWL6030_RTC_ALARM_HOURS         0x0A
#define TWL6030_RTC_ALARM_DAYS          0x0B
#define TWL6030_RTC_ALARM_MONTHS        0x0C
#define TWL6030_RTC_ALARM_YEARS         0x0D
#define TWL6030_RTC_CONTROL             0x10
#define TWL6030_RTC_STATUS              0x11
#define TWL6030_RTC_INTERRUPTS          0x12
#define TWL6030_RTC_COMPENSATION_LOW    0x13
#define TWL6030_RTC_COMPENSATION_HIGH   0x14
#define TWL6030_RTC_RESET_STATUS        0x16

//
// RTC status bits
//

#define TWL6030_RTC_STATUS_RUNNING          0x02
#define TWL6030_RTC_STATUS_1_SECOND_EVENT   0x04
#define TWL6030_RTC_STATUS_1_MINUTE_EVENT   0x08
#define TWL6030_RTC_STATUS_1_HOUR_EVENT     0x10
#define TWL6030_RTC_STATUS_1_DAY_EVENT      0x20
#define TWL6030_RTC_STATUS_ALARM            0x40
#define TWL6030_RTC_STATUS_RESET            0x80

//
// RTC control bits
//

#define TWL6030_RTC_CONTROL_RUN                 0x01
#define TWL6030_RTC_CONTROL_ROUND_30_SECONDS    0x02
#define TWL6030_RTC_CONTROL_AUTO_COMPENSATION   0x04
#define TWL6030_RTC_CONTROL_12_HOUR_MODE        0x08
#define TWL6030_RTC_CONTROL_SET_32_COUNTER      0x20
#define TWL6030_RTC_CONTROL_GET_TIME            0x40
#define TWL6030_RTC_CONTROL_READ_SHADOWED       0x80

//
// RTC interrupt bits
//

#define TWL6030_RTC_INTERRUPT_PERIODIC_MASK     0x03
#define TWL6030_RTC_INTERRUPT_EVERY_SECOND      0x00
#define TWL6030_RTC_INTERRUPT_EVERY_MINUTE      0x01
#define TWL6030_RTC_INTERRUPT_EVERY_HOUR        0x02
#define TWL6030_RTC_INTERRUPT_EVERY_DAY         0x03
#define TWL6030_RTC_INTERRUPT_PERIODIC          0x04
#define TWL6030_RTC_INTERRUPT_ALARM             0x08
#define TWL6030_RTC_INTERRUPT_MASK_DURING_SLEEP 0x10

#define VMMC_CFG_STATE      0x9A
#define VMMC_CFG_VOLTAGE    0x9B
#define VUSB_CFG_STATE      0xA2

#define VMMC_VOLTAGE_3V0    0x15

#define MISC1           0xE4
#define VAC_MEAS        (1 << 2)
#define VBAT_MEAS       (1 << 1)
#define BB_MEAS         (1 << 0)

#define MISC2           0xE5

//
// Slave Address 0x49
//

//
// Battery Charger registers
//

#define CONTROLLER_INT_MASK     0xE0
#define CONTROLLER_CTRL1        0xE1
#define CONTROLLER_WDG          0xE2
#define CONTROLLER_STAT1        0xE3
#define CHARGERUSB_INT_STATUS   0xE4
#define CHARGERUSB_INT_MASK     0xE5
#define CHARGERUSB_STATUS_INT1  0xE6
#define CHARGERUSB_STATUS_INT2  0xE7
#define CHARGERUSB_CTRL1        0xE8
#define CHARGERUSB_CTRL2        0xE9
#define CHARGERUSB_CTRL3        0xEA
#define CHARGERUSB_STAT1        0xEB
#define CHARGERUSB_VOREG        0xEC
#define CHARGERUSB_VICHRG       0xED
#define CHARGERUSB_CINLIMIT     0xEE
#define CHARGERUSB_CTRLLIMIT1   0xEF

#define CHARGERUSB_VICHRG_500       0x4
#define CHARGERUSB_VICHRG_1500      0xE

#define CHARGERUSB_CIN_LIMIT_100    0x1
#define CHARGERUSB_CIN_LIMIT_300    0x5
#define CHARGERUSB_CIN_LIMIT_500    0x9
#define CHARGERUSB_CIN_LIMIT_NONE   0xF

//
// Controller interrupt mask
//

#define MVAC_FAULT      (1 << 6)
#define MAC_EOC         (1 << 5)
#define MBAT_REMOVED    (1 << 4)
#define MFAULT_WDG      (1 << 3)
#define MBAT_TEMP       (1 << 2)
#define MVBUS_DET       (1 << 1)
#define MVAC_DET        (1 << 0)

//
// USB charger interrupt mask
//

#define MASK_MCURRENT_TERM          (1 << 3)
#define MASK_MCHARGERUSB_STAT       (1 << 2)
#define MASK_MCHARGERUSB_THMREG     (1 << 1)
#define MASK_MCHARGERUSB_FAULT      (1 << 0)

//
// USB charger regulator
//

#define CHARGERUSB_VOREG_3P52       0x01
#define CHARGERUSB_VOREG_4P0        0x19
#define CHARGERUSB_VOREG_4P2        0x23
#define CHARGERUSB_VOREG_4P76       0x3F

//
// USB charger control 1
//

#define SUSPEND_BOOT    (1 << 7)
#define OPA_MODE        (1 << 6)
#define HZ_MODE         (1 << 5)
#define TERM            (1 << 4)

//
// USB charger control 2
//

#define CHARGERUSB_CTRL2_VITERM_50  (0 << 5)
#define CHARGERUSB_CTRL2_VITERM_100 (1 << 5)
#define CHARGERUSB_CTRL2_VITERM_150 (2 << 5)
#define CHARGERUSB_CTRL2_VITERM_400 (7 << 5)

//
// Control 1
//

#define CONTROLLER_CTRL1_EN_CHARGER     (1 << 4)
#define CONTROLLER_CTRL1_SEL_CHARGER    (1 << 3)

//
// Status 1
//

#define CHRG_EXTCHRG_STATZ  (1 << 7)
#define CHRG_DET_N          (1 << 5)
#define VAC_DET             (1 << 3)
#define VBUS_DET            (1 << 2)

#define FG_REG_10   0xCA
#define FG_REG_11   0xCB

#define TOGGLE1     0x90
#define FGS         (1 << 5)
#define FGR         (1 << 4)
#define GPADCS      (1 << 1)
#define GPADCR      (1 << 0)

#define CTRL_P2          0x34
#define CTRL_P2_SP2     (1 << 2)
#define CTRL_P2_EOCP2   (1 << 1)
#define CTRL_P2_BUSY    (1 << 0)

#define GPCH0_LSB   0x57
#define GPCH0_MSB   0x58

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
