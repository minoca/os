/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rk808.h

Abstract:

    This header contains definitions for the RK808 PMIC interface.

Author:

    Evan Green 4-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for the RK808 interface.
//

#define UUID_RK808_INTERFACE \
    {{0x6B869CE0, 0xF67F4985, 0x9CB8BB08, 0xDD5CEACC}}

//
// Define LDO configuration flags.
//

//
// Set this flag if the LDO is enabled.
//

#define RK808_LDO_ENABLED 0x00000001

//
// Set this flag if the LDO is off in sleep mode. If not set, then the LDO
// will be enabled to its sleep voltage when the PMIC transitions to the sleep
// state.
//

#define RK808_LDO_OFF_IN_SLEEP 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _INTERFACE_RK808 INTERFACE_RK808, *PINTERFACE_RK808;

/*++

Structure Description:

    This structure defines the configuration for a Low Dropout Regulator in the
    RK808.

Members:

    ActiveVoltage - Stores the active output voltage for the LDO in millivolts.
        If 0, then the active voltage will not be modified.

    SleepVoltage - Stores the sleep mode output voltage for the LDO in
        millivolts. If 0, then the sleep mode voltage will not be modified.

    Flags - Stores flags governing the behavior of the LDO. See RK808_LDO_*
        definitions.

--*/

typedef struct _RK808_LDO_CONFIGURATION {
    USHORT ActiveVoltage;
    USHORT SleepVoltage;
    ULONG Flags;
} RK808_LDO_CONFIGURATION, *PRK808_LDO_CONFIGURATION;

//
// RK808 interface functions
//

typedef
KSTATUS
(*PRK808_SET_LDO) (
    PINTERFACE_RK808 Interface,
    UCHAR Ldo,
    PRK808_LDO_CONFIGURATION Configuration
    );

/*++

Routine Description:

    This routine configures an RK808 LDO.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    LDO - Supplies the LDO number to change. Valid values are between 1 and 8.

    Configuration - Supplies a pointer to the new configuration to set.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the interface for an RK808 PMIC.

Members:

    Context - Stores an oqaque token used by the interface functions that
        identifies the device. Users of the interface should not modify this
        value.

    SetLdo - Stores a pointer to a function used to manage an LDO.

--*/

struct _INTERFACE_RK808 {
    PVOID Context;
    PRK808_SET_LDO SetLdo;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
