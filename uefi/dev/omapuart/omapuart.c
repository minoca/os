/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    omapuart.c

Abstract:

    This module implements the firmware serial port interface for the UART
    in the Texas Instruments OMAP3 and OMAP4.

Author:

    Evan Green 16-Aug-2012

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "uefifw.h"
#include "dev/omapuart.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro performs a 32-bit read from the serial port.
//

#define READ_SERIAL_REGISTER(_Context, _Register) \
    EfiReadRegister32((_Context)->UartBase + _Register)

//
// This macro performs a 32-bit write to the serial port.
//

#define WRITE_SERIAL_REGISTER(_Context, _Register, _Value) \
    EfiWriteRegister32((_Context)->UartBase + _Register, _Value)

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP_UART_SLEEP_MODE_BIT 0x00000010
#define OMAP_UART_WRITE_CONFIGURATION_BIT 0x00000010

//
// Line Status Register bits.
//

#define OMAP_UART_LINE_ERRORS   0x0000009E
#define OMAP_UART_TRANSMIT_DONE 0x00000020
#define OMAP_UART_RECEIVE_READY 0x00000001

//
// Operational mode sets the UART to run with a character length of 8 bits
// (bits 1:0 = 11), 1 stop bit (bit 2 = 0), and no parity (bit 3 = 0)
//

#define OMAP_UART_OPERATIONAL_MODE  0x00000003
#define OMAP_UART_CONFIGURATION_A   0x00000080
#define OMAP_UART_CONFIGURATION_B   0x000000BF
#define OMAP_UART_MODE1_DISABLED    0x00000007
#define OMAP_UART_MODE1_OPERATIONAL 0x00000000
#define OMAP_UART_MODE2_OPERATIONAL 0x00000000

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _UART_REGISTERS {
    UartDivisorLow                = 0x0,
    UartReceiveData               = 0x0,
    UartTransmitData              = 0x0,
    UartDivisorHigh               = 0x4,
    UartInterruptEnable           = 0x4,
    UartFifoControl               = 0x8,
    UartEnhancedFeatures          = 0x8,
    UartInterruptIdentification   = 0x8,
    UartLineControl               = 0xC,
    UartModemControl              = 0x10,
    UartXOn1Character             = 0x10,
    UartLineStatus                = 0x14,
    UartXOn2Character             = 0x14,
    UartTransmissionControl       = 0x18,
    UartModemStatus               = 0x18,
    UartXOff1Character            = 0x18,
    UartXOff2Character            = 0x1C,
    UartScratchpad                = 0x1C,
    UartTriggerLevel              = 0x1C,
    UartMode1                     = 0x20,
    UartMode2                     = 0x24,
    UartTransmitFrameLengthLow    = 0x28,
    UartFifoLineStatus            = 0x28,
    UartResume                    = 0x2C,
    UartTrasmitFrameLengthHigh    = 0x2C,
    UartReceiveFrameLengthLow     = 0x30,
    UartFifoStatusLow             = 0x30,
    UartFifoStatusHigh            = 0x34,
    UartReceiveFrameLengthHigh    = 0x34,
    UartAutobaudStatus            = 0x38,
    UartBofControl                = 0x38,
    UartAuxiliaryControl          = 0x3C,
    UartSupplementaryControl      = 0x40,
    UartSupplementaryStatus       = 0x44,
    UartBofLength                 = 0x48,
    UartSystemConfiguration       = 0x54,
    UartSystemStatus              = 0x58,
    UartWakeEnable                = 0x5C,
    UartCarrierFrequencyPrescaler = 0x60
} UART_REGISTERS, *PUART_REGISTERS;

/*++

Structure Description:

    This structure defines a baud rate for the UEFI platforms.

Members:

    BaudRate - Stores the baud rate.

    BaudRateRegister - Stores the divisors for the baud rate.

--*/

typedef struct _BAUD_RATE {
    UINT32 BaudRate;
    UINT16 BaudRateRegister;
} BAUD_RATE, *PBAUD_RATE;

//
// -------------------------------------------------------------------- Globals
//

//
// Integer and fractional baud rates for the UART.
//

BAUD_RATE EfiOmapUartBaudRates[] = {
    {9600, 0x138},
    {19200, 0x9C},
    {38400, 0x4E},
    {57600, 0x34},
    {115200, 0x1A}
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipUartOmapComputeDivisor (
    UINTN BaudRate,
    UINT16 *Divisor
    )

/*++

Routine Description:

    This routine computes the divisor for the given baud rate.

Arguments:

    BaudRate - Supplies the desired baud rate.

    Divisor - Supplies a pointer where the divisor will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_UNSUPPORTED if the baud rate cannot be achieved.

--*/

{

    UINTN RateCount;
    UINTN RateIndex;

    RateCount = sizeof(EfiOmapUartBaudRates) / sizeof(EfiOmapUartBaudRates[0]);
    for (RateIndex = 0; RateIndex < RateCount; RateIndex += 1) {
        if (EfiOmapUartBaudRates[RateIndex].BaudRate == BaudRate) {
            *Divisor = EfiOmapUartBaudRates[RateIndex].BaudRateRegister;
            return EFI_SUCCESS;
        }
    }

    return EFI_UNSUPPORTED;
}

EFI_STATUS
EfipUartOmapInitialize (
    POMAP_UART_CONTEXT Context
    )

/*++

Routine Description:

    This routine initializes the OMAP UART controller.

Arguments:

    Context - Supplies the pointer to the port's context. The caller should
        have initialized some of these members.

Return Value:

    EFI status code.

--*/

{

    INT8 EnhancedRegister;

    EnhancedRegister = 0;
    if ((Context->UartBase == NULL) || (Context->BaudRateRegister == 0)) {
        return EFI_INVALID_PARAMETER;
    }

    //
    // Set mode to disable UART.
    //

    WRITE_SERIAL_REGISTER(Context, UartMode1, OMAP_UART_MODE1_DISABLED);

    //
    // Switch to configuration mode B, and set the Enhanced Mode bit to allow
    // writes to the Interrupt Enable and FIFO Control registers. Setting the
    // Enhanced Features register also disables auto RTC/CTS, disables
    // special character detection, and disables software flow control.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_CONFIGURATION_B);
    EnhancedRegister = READ_SERIAL_REGISTER(Context, UartEnhancedFeatures);
    WRITE_SERIAL_REGISTER(Context,
                          UartEnhancedFeatures,
                          EnhancedRegister | OMAP_UART_WRITE_CONFIGURATION_BIT);

    //
    // Switch to configuration mode A and set the Modem Control Register to
    // basically disable all modem functionality.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_CONFIGURATION_A);
    WRITE_SERIAL_REGISTER(Context, UartModemControl, 0);

    //
    // Switch back to operational mode to get to the Interrupt Enable Register.
    // Program the interrupt enable to 0, which masks all interrupts and
    // disables sleep mode. The baud rate divisors cannot be programmed unless
    // sleep mode is disabled.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_OPERATIONAL_MODE);
    WRITE_SERIAL_REGISTER(Context, UartInterruptEnable, 0);

    //
    // Switch to Configuration Mode B again to set the divisors. Set them to 0
    // for now to disable clocking, so that the FIFO control register can be
    // programmed.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_CONFIGURATION_B);
    WRITE_SERIAL_REGISTER(Context, UartDivisorHigh, 0);
    WRITE_SERIAL_REGISTER(Context, UartDivisorLow, 0);
    WRITE_SERIAL_REGISTER(Context, UartEnhancedFeatures, EnhancedRegister);

    //
    // Switch to Configuration Mode A and program the FIFO control register to
    // enable and clear the FIFOs.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_CONFIGURATION_A);
    WRITE_SERIAL_REGISTER(Context, UartFifoControl, 0x7);

    //
    // Set Supplementary Control to 0 to disable DMA. Set System Configuration
    // to 0 to turn off all power saving features, and set Wake Enable to 0
    // to disable wake on interrupt capabilities.
    //

    WRITE_SERIAL_REGISTER(Context, UartSupplementaryControl, 0);
    WRITE_SERIAL_REGISTER(Context, UartSystemConfiguration, 0);
    WRITE_SERIAL_REGISTER(Context, UartWakeEnable, 0);

    //
    // Program the real divisor values to restart the baud rate clock.
    //

    WRITE_SERIAL_REGISTER(Context,
                          UartDivisorHigh,
                          (UINT8)(Context->BaudRateRegister >> 8));

    WRITE_SERIAL_REGISTER(Context,
                          UartDivisorLow,
                          (UINT8)(Context->BaudRateRegister));

    //
    // Set Mode2 to 0 for normal UART operation (without pulse shaping), and
    // set Mode1 to 0 to enable the UART in normal UART mode (no IrDA or other
    // crazy modes).
    //

    WRITE_SERIAL_REGISTER(Context, UartMode2, OMAP_UART_MODE2_OPERATIONAL);
    WRITE_SERIAL_REGISTER(Context, UartMode1, OMAP_UART_MODE1_OPERATIONAL);

    //
    // Switch back to operational mode, which also configures the UART for the
    // 8-N-1 configuration, and return success.
    //

    WRITE_SERIAL_REGISTER(Context, UartLineControl, OMAP_UART_OPERATIONAL_MODE);
    return EFI_SUCCESS;
}

EFI_STATUS
EfipUartOmapTransmit (
    POMAP_UART_CONTEXT Context,
    VOID *Data,
    UINTN Size
    )

/*++

Routine Description:

    This routine writes data out the serial port. This routine should busily
    spin if the previously sent byte has not finished transmitting.

Arguments:

    Context - Supplies the pointer to the port context.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT32 ByteIndex;
    UINT8 *Bytes;
    UINT32 StatusRegister;

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            StatusRegister = READ_SERIAL_REGISTER(Context, UartLineStatus);
            if ((StatusRegister & OMAP_UART_LINE_ERRORS) != 0) {
                return EFI_DEVICE_ERROR;
            }

        } while ((StatusRegister & OMAP_UART_TRANSMIT_DONE) == 0);

        //
        // Send the byte and return.
        //

        WRITE_SERIAL_REGISTER(Context, UartTransmitData, Bytes[ByteIndex]);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipUartOmapReceive (
    POMAP_UART_CONTEXT Context,
    VOID *Data,
    UINTN *Size
    )

/*++

Routine Description:

    This routine reads bytes from the serial port.

Arguments:

    Context - Supplies the pointer to the port context.

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if there was no data to be read at the current time.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT32 ByteCount;
    UINT32 ByteIndex;
    UINT8 *Bytes;
    EFI_STATUS Status;
    UINT32 StatusRegister;

    ByteCount = *Size;
    Bytes = Data;
    Status = EFI_NOT_READY;
    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        StatusRegister = READ_SERIAL_REGISTER(Context, UartLineStatus);
        if ((StatusRegister & OMAP_UART_LINE_ERRORS) != 0) {
            Status = EFI_DEVICE_ERROR;
            break;
        }

        if ((StatusRegister & OMAP_UART_RECEIVE_READY) == 0) {
            break;
        }

        Bytes[ByteIndex] = READ_SERIAL_REGISTER(Context, UartReceiveData);
        Status = EFI_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

EFI_STATUS
EfipUartOmapGetStatus (
    POMAP_UART_CONTEXT Context,
    BOOLEAN *ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    Context - Supplies a pointer to the serial port context.

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    EFI_SUCCESS on success.

    EFI_DEVICE_ERROR if a device error occurred.

--*/

{

    UINT32 StatusRegister;

    *ReceiveDataAvailable = FALSE;
    StatusRegister = READ_SERIAL_REGISTER(Context, UartLineStatus);
    if ((StatusRegister & OMAP_UART_RECEIVE_READY) != 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

