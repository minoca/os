/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elani2c.h

Abstract:

    This header contains definitions for the Elan i2C touchpad driver.

Author:

    Evan Green 28-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define various packet lengths.
//

#define ELAN_I2C_INFO_LENGTH 2
#define ELAN_I2C_DEVICE_DESCRIPTOR_LENGTH 30
#define ELAN_I2C_REPORT_DESCRIPTOR_LENGTH 158

#define ELAN_I2C_MAX_PACKET_SIZE 256

#define ELAN_I2C_PRESSURE_ADJUSTED 0x10

#define ELAN_I2C_PRESSURE_OFFSET 25

#define ELAN_I2C_REPORT_SIZE 34

//
// Define mode register bits.
//

#define ELAN_I2C_ENABLE_ABSOLUTE 0x0001

//
// Define offsets within the report.
//

#define ELAN_I2C_REPORT_ID_OFFSET 2
#define ELAN_I2C_REPORT_TOUCH_OFFSET 3
#define ELAN_I2C_REPORT_FINGER_DATA_OFFSET 4
#define ELAN_I2C_REPORT_FINGER_DATA_LENGTH 5
#define ELAN_I2C_REPORT_HOVER_OFFSET 30

//
// Define offsets within each replicated finger data area.
//

#define ELAN_I2C_FINGER_XY_HIGH_OFFSET 0
#define ELAN_I2C_FINGER_X_OFFSET 1
#define ELAN_I2C_FINGER_Y_OFFSET 2

#define ELAN_I2C_REPORT_ID 0x5D
#define ELAN_I2C_FINGER_COUNT 5

#define ELAN_I2C_REPORT_TOUCH_LEFT_BUTTON 0x01
#define ELAN_I2C_REPORT_TOUCH_FINGER 0x08

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ELAN_I2C_COMMAND {
    ElanI2cCommandDeviceDescriptor = 0x0001,
    ElanI2cCommandReportDescriptor = 0x0002,
    ElanI2cCommandStandby = 0x0005,
    ElanI2cCommandReset = 0x0100,
    ElanI2cCommandUniqueId = 0x0101,
    ElanI2cCommandFirmwareVersion = 0x0102,
    ElanI2cCommandSampleVersion = 0x0103,
    ElanI2cCommandTraceCount = 0x0105,
    ElanI2cCommandMaxXAxis = 0x0106,
    ElanI2cCommandMaxYAxis = 0x0107,
    ElanI2cCommandResolution = 0x0108,
    ElanI2cCommandPressureFormat = 0x010A,
    ElanI2cCommandIapVersion = 0x0110,
    ElanI2cCommandSetMode = 0x0300,
    ElanI2cCommandFirmwareChecksum = 0x030F,
    ElanI2cCommandWake = 0x0800,
    ElanI2cCommandSleep = 0x0801,
} ELAN_I2C_COMMAND, *PELAN_I2C_COMMAND;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
