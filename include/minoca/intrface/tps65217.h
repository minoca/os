/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tps65217.h

Abstract:

    This header contains definitions for the TPS65217 PMIC interface.

Author:

    Evan Green 8-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for TPS65217 interface.
//

#define UUID_TPS65217_INTERFACE \
    {{0x5122B554, 0xA3534CD4, 0x870AF1B3, 0xD0AC4C9A}}

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _INTERFACE_TPS65217 INTERFACE_TPS65217, *PINTERFACE_TPS65217;

typedef enum _TPS65217_DCDC_REGULATOR {
    Tps65217DcDcInvalid,
    Tps65217DcDc1,
    Tps65217DcDc2,
    Tps65217DcDc3,
} TPS65217_DCDC_REGULATOR, *PTPS65217_DCDC_REGULATOR;

//
// TPS65217 interface functions
//

typedef
KSTATUS
(*PTPS65217_SET_DCDC_REGULATOR) (
    PINTERFACE_TPS65217 Interface,
    TPS65217_DCDC_REGULATOR Regulator,
    ULONG Millivolts
    );

/*++

Routine Description:

    This routine sets a TPS65217 DC-DC regulator voltage to the given value.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    Regulator - Supplies the regulator number to change.

    Millivolts - Supplies the millivolt value to change to.

Return Value:

    Status code.

--*/

/*++

Structure Description:

    This structure defines the interface for a TPS65217 PMIC.

Members:

    Context - Stores an oqaque token used by the interface functions that
        identifies the device. Users of the interface should not modify this
        value.

    SetDcDcRegulator - Stores a pointer to a function used to change one of the
        DC-DC regulator values.

--*/

struct _INTERFACE_TPS65217 {
    PVOID Context;
    PTPS65217_SET_DCDC_REGULATOR SetDcDcRegulator;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
