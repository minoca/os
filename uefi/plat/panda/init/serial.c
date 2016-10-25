/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    serial.c

Abstract:

    This module implements basic serial support for the first stage loader.

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

#define OMAP4_WRITE_UART(_Register, _Value) \
    OMAP4_WRITE8(OMAP4430_UART3_BASE + (_Register), (_Value))

#define OMAP4_READ_UART(_Register) \
    OMAP4_READ8(OMAP4430_UART3_BASE + (_Register))

//
// ---------------------------------------------------------------- Definitions
//

#define STAGE1_SERIAL_CLOCK_HZ 48000000
#define STAGE1_SERIAL_BAUD_RATE 115200

#define OMAP4_UART_RBR     0x00
#define OMAP4_UART_THR     0x00
#define OMAP4_UART_DLL     0x00
#define OMAP4_UART_IER     0x04
#define OMAP4_UART_DLM     0x04
#define OMAP4_UART_FCR     0x08
#define OMAP4_UART_IIR     0x08
#define OMAP4_UART_LCR     0x0C
#define OMAP4_UART_MCR     0x10
#define OMAP4_UART_LSR     0x14
#define OMAP4_UART_MSR     0x18
#define OMAP4_UART_SCR     0x1C
#define OMAP4_UART_MDR1    0x20

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipInitializeSerial (
    VOID
    )

/*++

Routine Description:

    This routine initialize the serial port for the first stage loader.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Divisor;

    Divisor = STAGE1_SERIAL_CLOCK_HZ / 16 / STAGE1_SERIAL_BAUD_RATE;
    OMAP4_WRITE_UART(OMAP4_UART_IER, 0x00);
    OMAP4_WRITE_UART(OMAP4_UART_MDR1, 0x07);
    OMAP4_WRITE_UART(OMAP4_UART_LCR, 0x83);
    OMAP4_WRITE_UART(OMAP4_UART_DLL, (UINT8)Divisor);
    OMAP4_WRITE_UART(OMAP4_UART_DLM,  (UINT8)(Divisor >> 8));
    OMAP4_WRITE_UART(OMAP4_UART_LCR, 0x03);
    OMAP4_WRITE_UART(OMAP4_UART_MCR, 0x03);
    OMAP4_WRITE_UART(OMAP4_UART_FCR, 0x07);
    OMAP4_WRITE_UART(OMAP4_UART_MDR1, 0x00);
    return;
}

VOID
EfipSerialPrintString (
    CHAR8 *String
    )

/*++

Routine Description:

    This routine prints a string to the serial console.

Arguments:

    String - Supplies a pointer to the string to send.

Return Value:

    None.

--*/

{

    while (*String != '\0') {
        if (*String == '\n') {
            EfipSerialPutCharacter('\r');
        }

        EfipSerialPutCharacter(*String);
        String += 1;
    }

    return;
}

VOID
EfipSerialPrintHexInteger (
    UINT32 Value
    )

/*++

Routine Description:

    This routine prints a hex integer to the console.

Arguments:

    Value - Supplies the value to print.

Return Value:

    None.

--*/

{

    UINT32 Digit;
    UINTN Index;

    for (Index = 0; Index < 8; Index += 1) {
        Digit = ((Value & 0xF0000000) >> 28) & 0x0000000F;
        if (Digit >= 0xA) {
            EfipSerialPutCharacter(Digit - 0xA + 'A');

        } else {
            EfipSerialPutCharacter(Digit + '0');
        }

        Value = Value << 4;
    }

    return;
}

VOID
EfipSerialPutCharacter (
    CHAR8 Character
    )

/*++

Routine Description:

    This routine prints a character to the serial console.

Arguments:

    Character - Supplies the character to send.

Return Value:

    None.

--*/

{

    while (TRUE) {
        if ((OMAP4_READ_UART(OMAP4_UART_LSR) & 0x20) != 0) {
            break;
        }
    }

    OMAP4_WRITE_UART(OMAP4_UART_THR, Character);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

