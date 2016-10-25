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

    Evan Green 18-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"
#include "util.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM335_WRITE_UART(_Register, _Value) \
    AM3_WRITE32(AM335_UART_0_BASE + (_Register), (_Value))

#define AM335_READ_UART(_Register) \
    AM3_READ32(AM335_UART_0_BASE + (_Register))

//
// ---------------------------------------------------------------- Definitions
//

#define STAGE1_SERIAL_CLOCK_HZ 48000000
#define STAGE1_SERIAL_BAUD_RATE 115200

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
EfipAm335EnableUart (
    VOID
    )

/*++

Routine Description:

    This routine performs rudimentary initialization so that UART0 can be used
    as a debug console.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Divisor;
    UINT32 Register;
    UINT32 Value;

    //
    // Set the pad configuration for UART0.
    //

    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_UART_RXD(0);
    Value = AM335_SOC_CONTROL_UART0_RXD_PULLUP |
            AM335_SOC_CONTROL_UART0_RXD_RX_ACTIVE;

    AM3_WRITE32(Register, Value);
    Register = AM335_SOC_CONTROL_REGISTERS + AM335_PAD_UART_TXD(0);
    Value = AM335_SOC_CONTROL_UART0_TXD_PULLUP;
    AM3_WRITE32(Register, Value);

    //
    // Reset the UART module.
    //

    Value = AM335_READ_UART(AM335_UART_SYSTEM_CONTROL);
    Value |= AM335_UART_SYSTEM_CONTROL_RESET;
    AM335_WRITE_UART(AM335_UART_SYSTEM_CONTROL, Value);
    do {
        Value = AM335_READ_UART(AM335_UART_SYSTEM_STATUS);

    } while ((Value & AM335_UART_SYSTEM_STATUS_RESET_DONE) == 0);

    //
    // Configure the UART.
    //

    Divisor = STAGE1_SERIAL_CLOCK_HZ / 16 / STAGE1_SERIAL_BAUD_RATE;
    AM335_WRITE_UART(AM335_UART_IER, 0x00);
    AM335_WRITE_UART(AM335_UART_MDR1, 0x07);
    AM335_WRITE_UART(AM335_UART_LCR, 0x83);
    AM335_WRITE_UART(AM335_UART_DLL, (UINT8)Divisor);
    AM335_WRITE_UART(AM335_UART_DLM,  (UINT8)(Divisor >> 8));
    AM335_WRITE_UART(AM335_UART_LCR, 0x03);
    AM335_WRITE_UART(AM335_UART_MCR, 0x03);
    AM335_WRITE_UART(AM335_UART_FCR, 0x07);
    AM335_WRITE_UART(AM335_UART_MDR1, 0x00);
    return;
}

VOID
EfipSerialPrintBuffer32 (
    CHAR8 *Title,
    VOID *Buffer,
    UINT32 Size
    )

/*++

Routine Description:

    This routine prints a buffer of 32-bit hex integers.

Arguments:

    Title - Supplies an optional pointer to a string to title the buffer.

    Buffer - Supplies the buffer to print.

    Size - Supplies the size of the buffer. This is assumed to be divisible by
        4.

Return Value:

    None.

--*/

{

    UINT32 *Buffer32;
    UINTN Index;

    if (Title != NULL) {
        EfipSerialPrintString(Title);
    }

    Buffer32 = Buffer;
    for (Index = 0; Index < (Size >> 2); Index += 1) {
        if ((Index & 0x7) == 0) {
            EfipSerialPrintString("\r\n");
        }

        EfipSerialPrintHexInteger(Buffer32[Index]);
        EfipSerialPrintString(" ");
    }

    EfipSerialPrintString("\r\n");
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
        if ((AM335_READ_UART(AM335_UART_LSR) & 0x20) != 0) {
            break;
        }
    }

    AM335_WRITE_UART(AM335_UART_THR, Character);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

