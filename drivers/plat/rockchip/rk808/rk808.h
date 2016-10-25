/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk808.h

Abstract:

    This header contains hardware definitions for the RK808 Power Management IC.

Author:

    Evan Green 4-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define RK808_LDO_ON_VSEL(_Ldo) (Rk808Ldo1OnVsel + (((_Ldo) - 1) * 2))
#define RK808_LDO_SLP_VSEL(_Ldo) (Rk808Ldo1SlpVsel + (((_Ldo) - 1) * 2))

//
// ---------------------------------------------------------------- Definitions
//

#define RK808_LDO_COUNT 8

//
// RTC status bits.
//

#define RK808_RTC_STATUS_RUNNING          0x02
#define RK808_RTC_STATUS_1_SECOND_EVENT   0x04
#define RK808_RTC_STATUS_1_MINUTE_EVENT   0x08
#define RK808_RTC_STATUS_1_HOUR_EVENT     0x10
#define RK808_RTC_STATUS_1_DAY_EVENT      0x20
#define RK808_RTC_STATUS_ALARM            0x40
#define RK808_RTC_STATUS_RESET            0x80

//
// RTC control bits.
//

#define RK808_RTC_CONTROL_STOP                0x01
#define RK808_RTC_CONTROL_ROUND_30S           0x02
#define RK808_RTC_CONTROL_AUTO_COMPENSATION   0x04
#define RK808_RTC_CONTROL_12_HOUR_MODE        0x08
#define RK808_RTC_CONTROL_TEST_MODE           0x10
#define RK808_RTC_CONTROL_SET_32_COUNTER      0x20
#define RK808_RTC_CONTROL_GET_TIME            0x40
#define RK808_RTC_CONTROL_READ_SHADOWED       0x80

//
// RTC interrupt bits.
//

#define RK808_RTC_INTERRUPT_PERIODIC_MASK     0x03
#define RK808_RTC_INTERRUPT_EVERY_SECOND      0x00
#define RK808_RTC_INTERRUPT_EVERY_MINUTE      0x01
#define RK808_RTC_INTERRUPT_EVERY_HOUR        0x02
#define RK808_RTC_INTERRUPT_EVERY_DAY         0x03
#define RK808_RTC_INTERRUPT_PERIODIC          0x04
#define RK808_RTC_INTERRUPT_ALARM             0x08
#define RK808_RTC_INTERRUPT_MASK_DURING_SLEEP 0x10

//
// Clock 32k out register bits.
//

#define RK808_CLOCK_32K_OUT2_ENABLE 0x01

//
// Battery voltage monitor register bits.
//

//
// The low voltage is from 2.8 - 3.5V in steps of 100mV.
//

#define RK808_BATTERY_LOW_VOLTAGE_THRESHOLD_MASK 0x07
#define RK808_BATTERY_LOW_VOLTAGE_STATUS 0x08
#define RK808_BATTERY_LOW_ACTION_INTERRUPT 0x10
#define RK808_BATTERY_UNDERVOLTAGE_LOCKOUT 0x20
#define RK808_BATTERY_CHARGER_IN_EVENT 0x40
#define RK808_BATTERY_CHARGER_OUT_EVENT 0x80

//
// Thermal control register bits.
//

#define RK808_THERMAL_SHUTDOWN 0x01
#define RK808_THERMAL_HOT_DIE_WARNING 0x02
#define RK808_THERMAL_HOT_THRESHOLD_MASK 0x0C
#define RK808_THERMAL_HOT_THRESHOLD_85C (0x0 << 2)
#define RK808_THERMAL_HOT_THRESHOLD_95C (0x1 << 2)
#define RK808_THERMAL_HOT_THRESHOLD_105C (0x2 << 2)
#define RK808_THERMAL_HOT_THRESHOLD_115C (0x3 << 2)
#define RK808_THERMAL_SHUTDOWN_140C (0x0 << 4)
#define RK808_THERMAL_SHUTDOWN_170C (0x1 << 4)

//
// DCDC converter enable register bits.
//

#define RK808_DCDC_BUCK1_ENABLE 0x01
#define RK808_DCDC_BUCK2_ENABLE 0x02
#define RK808_DCDC_BUCK3_ENABLE 0x04
#define RK808_DCDC_BUCK4_ENABLE 0x08
#define RK808_DCDC_SWITCH1_ENABLE 0x20
#define RK808_DCDC_SWITCH2_ENABLE 0x40

//
// Buck n Configuration register bits.
//

#define RK808_BUCK_ILMIN_MASK 0x7
#define RK808_BUCK_ILMIN_50MA 0x1
#define RK808_BUCK_ILMIN_100MA 0x1
#define RK808_BUCK_ILMIN_150MA 0x2
#define RK808_BUCK_ILMIN_200MA 0x3
#define RK808_BUCK_ILMIN_250MA 0x4
#define RK808_BUCK_ILMIN_300MA 0x5
#define RK808_BUCK_ILMIN_350MA 0x6
#define RK808_BUCK_ILMIN_400MA 0x7
#define RK808_BUCK_RATE_MASK (0x3 << 3)
#define RK808_BUCK_RATE_2MV_US (0x0 << 3)
#define RK808_BUCK_RATE_4MV_US (0x1 << 3)
#define RK808_BUCK_RATE_6MV_US (0x2 << 3)
#define RK808_BUCK_RATE_10MV_US (0x3 << 3)
#define RK808_BUCK_PHASE_INVERTED 0x80

//
// Device control register bits.
//

#define RK808_DEVICE_OFF 0x01
#define RK808_DEVICE_SLEEP 0x02
#define RK808_DEVICE_OFF_RESET 0x08
#define RK808_DEVICE_OFF_BUTTON_HOLD_MASK (0x3 << 4)
#define RK808_DEVICE_OFF_BUTTON_HOLD_6S (0x0 << 4)
#define RK808_DEVICE_OFF_BUTTON_HOLD_8S (0x1 << 4)
#define RK808_DEVICE_OFF_BUTTON_HOLD_10S (0x2 << 4)
#define RK808_DEVICE_OFF_BUTTON_HOLD_12S (0x3 << 4)
#define RK808_DEVICE_CONTROL_SHUTDOWN (1 << 3)

//
// Interrupt status/mask 1 register bits. Status bits are write 1 to clear.
// Masked interrupts do not fire if the mask bit is 1.
//

#define RK808_INTERRUPT1_VOUT_LOW 0x01
#define RK808_INTERRUPT1_BATTERY_LOW 0x02
#define RK808_INTERRUPT1_POWER_ON 0x04
#define RK808_INTERRUPT1_POWER_ON_LONG_PRESS 0x08
#define RK808_INTERRUPT1_HOT_DIE 0x10
#define RK808_INTERRUPT1_RTC_ALARM 0x20
#define RK808_INTERRUPT1_RTC_PERIOD 0x40

//
// Interrupt status/mask 2 register bits.
//

#define RK808_INTERRUPT2_PLUG_IN 0x01
#define RK808_INTERRUPT2_PLUG_OUT 0x02

//
// I/O polarity register bits.
//

#define RK808_POLARITY_INT_ACTIVE_HIGH 0x01
#define RK808_POLARITY_DVS1_ACTIVE_HIGH 0x02
#define RK808_POLARITY_DVS2_ACTIVE_HIGH 0x04

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _RK808_REGISTER {
    Rk808Seconds = 0x00,
    Rk808Minutes = 0x01,
    Rk808Hours = 0x02,
    Rk808Days = 0x03,
    Rk808Months = 0x04,
    Rk808Years = 0x05,
    Rk808Weeks = 0x06,
    Rk808AlarmSeconds = 0x08,
    Rk808AlarmMinutes = 0x09,
    Rk808AlarmHours = 0x0A,
    Rk808AlarmDays = 0x0B,
    Rk808AlarmMonths = 0x0C,
    Rk808AlarmYears = 0x0D,
    Rk808RtcControl = 0x10,
    Rk808RtcStatus = 0x11,
    Rk808RtcInterrupt = 0x12,
    Rk808CompensationLsb = 0x13,
    Rk808CompensationMsb = 0x14,
    Rk808Clock32kOut = 0x20,
    Rk808BatteryVoltageMonitor = 0x21,
    Rk808Thermal = 0x22,
    Rk808DcDcEnable = 0x23,
    Rk808LdoEnable = 0x24,
    Rk808SleepSetOff1 = 0x25,
    Rk808SleepSetOff2 = 0x26,
    Rk808DcDcUvStatus = 0x27,
    Rk808DcDcUvAct = 0x28,
    Rk808LdoUvStatus = 0x29,
    Rk808LdoUvAct = 0x2A,
    Rk808DcDcPg = 0x2B,
    Rk808LdoPg = 0x2C,
    Rk808VoutMonTdb = 0x2D,
    Rk808Buck1Config = 0x2E,
    Rk808Buck1OnVsel = 0x2F,
    Rk808Buck1SlpVsel = 0x30,
    Rk808Buck1DvsVsel = 0x31,
    Rk808Buck2Config = 0x32,
    Rk808Buck2OnVsel = 0x33,
    Rk808Buck2SlpVsel = 0x34,
    Rk808Buck2DvsVsel = 0x35,
    Rk808Buck3Config = 0x36,
    Rk808Buck4Config = 0x37,
    Rk808Buck4OnVsel = 0x38,
    Rk808Buck4SlpVsel = 0x39,
    Rk808Ldo1OnVsel = 0x3B,
    Rk808Ldo1SlpVsel = 0x3C,
    Rk808Ldo2OnVsel = 0x3D,
    Rk808Ldo2SlpVsel = 0x3E,
    Rk808Ldo3OnVsel = 0x3F,
    Rk808Ldo3SlpVsel = 0x40,
    Rk808Ldo4OnVsel = 0x41,
    Rk808Ldo4SlpVsel = 0x42,
    Rk808Ldo5OnVsel = 0x43,
    Rk808Ldo5SlpVsel = 0x44,
    Rk808Ldo6OnVsel = 0x45,
    Rk808Ldo6SlpVsel = 0x46,
    Rk808Ldo7OnVsel = 0x47,
    Rk808Ldo7SlpVsel = 0x48,
    Rk808Ldo8OnVsel = 0x49,
    Rk808Ldo8SlpVsel = 0x4A,
    Rk808DeviceControl = 0x4B,
    Rk808InterruptStatus1 = 0x4C,
    Rk808InterruptMask1 = 0x4D,
    Rk808InterruptStatus2 = 0x4E,
    Rk808InterruptMask2 = 0x4F,
    Rk808IoPol = 0x50,
    Rk808DcDcIlMax = 0x90,
} RK808_REGISTER, *PRK808_REGISTER;

/*++

Structure Description:

    This structure stores the possible range for an LDO in the RK808.

Members:

    Min - Stores the minimum millivolt value that the LDO can output, in
        millivolts.

    Max - Stores the maximum millivolt value that the LDO can output, in
        millivolts.

    Step - Stores the granularity of millivolt output between the minimum and
        the maximum, in millivolts.

--*/

typedef struct _RK808_LDO_RANGE {
    USHORT Min;
    USHORT Max;
    USHORT Step;
} RK808_LDO_RANGE, *PRK808_LDO_RANGE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
