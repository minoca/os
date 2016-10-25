/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tps65217.h

Abstract:

    This header contains definitions for the TPS65217 PMIC.

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
// Define the magic password value.
//

#define TPS65217_PASSWORD_UNLOCK 0x7D

//
// Define interrupt register bits.
//

#define TPS65217_INTERRUPT_USB 0x01
#define TPS65217_INTERRUPT_AC 0x02
#define TPS65217_INTERRUPT_PUSHBUTTON 0x04
#define TPS65217_INTERRUPT_USB_MASK 0x10
#define TPS65217_INTERRUPT_AC_MASK 0x20
#define TPS65217_INTERRUPT_PUSHBUTTON_MASK 0x40
#define TPS65217_INTERRUPT_STATUS_MASK \
    (TPS65217_INTERRUPT_USB | TPS65217_INTERRUPT_AC | \
     TPS65217_INTERRUPT_PUSHBUTTON)

//
// Define status register bits.
//

#define TPS65217_STATUS_PUSHBUTTON 0x01
#define TPS65217_STATUS_USB_POWER 0x04
#define TPS65217_STATUS_AC_POWER 0x08
#define TPS65217_STATUS_OFF 0x80

//
// Define slew control register bits.
//

#define TPS65217_SLEW_CONTROL_DCDC_GO 0x80

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TPS65217_REGISTER {
    Tps65217ChipId = 0x00,
    Tps65217PowerPathControl = 0x01,
    Tps65217Interrupt = 0x02,
    Tps65217ChargerControl0 = 0x03,
    Tps65217ChargerControl1 = 0x04,
    Tps65217ChargerControl2 = 0x05,
    Tps65217ChargerControl3 = 0x06,
    Tps65217WledControl1 = 0x07,
    Tps65217WledControl2 = 0x08,
    Tps65217MuxControl = 0x09,
    Tps65217Status = 0x0A,
    Tps65217Password = 0x0B,
    Tps65217PowerGood = 0x0C,
    Tps65217PowerGoodDelay = 0x0D,
    Tps65217DcDc1Voltage = 0x0E,
    Tps65217DcDc2Voltage = 0x0F,
    Tps65217DcDc3Voltage = 0x10,
    Tps65217SlewControl = 0x11,
    Tps65217Ldo1Voltage = 0x12,
    Tps65217Ldo2Voltage = 0x13,
    Tps65217Ls1Ldo3Voltage = 0x14,
    Tps65217Ls2Ldo4Voltage = 0x15,
    Tps65217Enable = 0x16,
    Tps65217UvloControl = 0x18,
    Tps65217Seq1 = 0x19,
    Tps65217Seq2 = 0x1A,
    Tps65217Seq3 = 0x1B,
    Tps65217Seq4 = 0x1C,
    Tps65217Seq5 = 0x1D,
    Tps65217Seq6 = 0x1E,
    Tps65217RegisterCount
} TPS65217_REGISTER, *PTPS65217_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
