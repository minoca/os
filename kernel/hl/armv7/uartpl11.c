/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uartpl11.c

Abstract:

    This module implements the kernel serial port interface on a PrimeCell
    PL-011 UART.

Author:

    Evan Green 7-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro performs a 32-bit read from the serial port.
//

#define READ_SERIAL_REGISTER(_Register) \
    HlReadRegister32(HlPl11UartBase + (_Register))

//
// This macro performs a 32-bit write to the serial port.
//

#define WRITE_SERIAL_REGISTER(_Register, _Value) \
    HlWriteRegister32(HlPl11UartBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define UART_CLOCK_FREQUENCY_3MHZ 3000000
#define UART_CLOCK_FREQUENCY_14MHZ 14745600

#define PL11_UART_SIZE 0x1000

//
// Define bits for the PL11 UART Line Control Register.
//

#define PL11_UART_LINE_CONTROL_FIFO_ENABLE       0x10
#define PL11_UART_LINE_CONTROL_WORD_LENGTH_8BITS 0x60

//
// Define bits for the PL11 UART Control Register.
//

#define PL11_UART_CONTROL_UART_ENABLE        0x001
#define PL11_UART_CONTROL_TRANSMITTER_ENABLE 0x100
#define PL11_UART_CONTROL_RECEIVER_ENABLE    0x200

//
// Define the interrupt mask for the UART Interrupt Mask Register.
//

#define PL11_UART_INTERRUPT_MASK 0x7FF

//
// Define bits for the PL11 UART Flags Register.
//

#define PL11_UART_FLAG_CLEAR_TO_SEND       0x001
#define PL11_UART_FLAG_DATA_SET_READY      0x002
#define PL11_UART_FLAG_DATA_CARRIER_DETECT 0x004
#define PL11_UART_FLAG_TRANSMIT_BUSY       0x008
#define PL11_UART_FLAG_RECEIVE_EMPTY       0x010
#define PL11_UART_FLAG_TRANSMIT_FULL       0x020
#define PL11_UART_FLAG_RECEIVE_FULL        0x040
#define PL11_UART_FLAG_TRANSMIT_EMPTY      0x080
#define PL11_UART_FLAG_RING_INDICATOR      0x100

//
// Define bits for the PL11 UART Receive Status register.
//

#define PL11_UART_RECEIVE_STATUS_FRAMING_ERROR 0x0001
#define PL11_UART_RECEIVE_STATUS_PARITY_ERROR  0x0002
#define PL11_UART_RECEIVE_STATUS_BREAK_ERROR   0x0004
#define PL11_UART_RECEIVE_STATUS_OVERRUN_ERROR 0x0008
#define PL11_UART_RECEIVE_STATUS_ERROR_MASK    0x000F
#define PL11_UART_RECEIVE_STATUS_ERROR_CLEAR   0xFF00

//
// Define the bits for the PL11 UART data register.
//

#define PL11_UART_DATA_BYTE_MASK     0x00FF
#define PL11_UART_DATA_FRAMING_ERROR 0x0100
#define PL11_UART_DATA_PARITY_ERROR  0x0200
#define PL11_UART_DATA_BREAK_ERROR   0x0400
#define PL11_UART_DATA_OVERRUN_ERROR 0x0800
#define PL11_UART_DATA_ERROR_MASK    0x0F00

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpPl11Reset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
HlpPl11Transmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
HlpPl11Receive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
HlpPl11GetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
HlpPl11Disconnect (
    PVOID Context
    );

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structures defines a baud rate for the PL011 UART.

Members:

    BaudRate - Stores the baud rate value.

    IntegerDivisor - Stores the integer divisor to program into the PL011.

    FractionalDivisor - Stores the fractional divisor to program into the PL011.

--*/

typedef struct _BAUD_RATE {
    ULONG BaudRate;
    ULONG IntegerDivisor;
    ULONG FractionalDivisor;
} BAUD_RATE, *PBAUD_RATE;

//
// Register set definition for the PL-011. These are offsets in bytes, not
// words.
//

typedef enum _PL011_REGISTER {
    UartDataBuffer          = 0x0,
    UartReceiveStatus       = 0x4,
    UartFlags               = 0x18,
    UartIrDaLowPowerCounter = 0x20,
    UartIntegerBaudRate     = 0x24,
    UartFractionalBaudRate  = 0x28,
    UartLineControl         = 0x2C,
    UartControl             = 0x30,
    UartFifoInterruptLevel  = 0x34,
    UartInterruptMask       = 0x38,
    UartInterruptStatus     = 0x3C,
    UartMaskedInterrupts    = 0x40,
    UartInterruptClear      = 0x44,
    UartDmaControl          = 0x48,
    UartPeripheralId0       = 0xFE0,
    UartPeripheralId1       = 0xFE4,
    UartPeripheralId2       = 0xFE8,
    UartPeripheralId3       = 0xFEC,
    UartPcellId0            = 0xFF0,
    UartPcellId1            = 0xFF4,
    UartPcellId2            = 0xFF8,
    UartPcellId3            = 0xFFC
} PL011_REGISTER, *PPL011_REGISTER;

//
// -------------------------------------------------------------------- Globals
//

//
// Integer and fractional baud rates for an input clock of 14.7456 MHz.
//

BAUD_RATE HlPl11Available14MhzRates[] = {
    {9600, 0x60, 0},
    {19200, 0x30, 0},
    {38400, 0x18, 0},
    {57600, 0x10, 0},
    {115200, 0x8, 0}
};

//
// Integer and fractional baud rates for an input clock of 3 MHz.
//

BAUD_RATE HlPl11Available3MhzRates[] = {
    {9600, 19, 34},
    {19200, 9, 49},
    {38400, 4, 57},
    {57600, 3, 16},
    {115200, 1, 40}
};

//
// Store the virtual address of the mapped UART.
//

PVOID HlPl11UartBase = NULL;

//
// Store the physical address of the UART, initialized to a value that should
// never see the light of day unless UART initialization is forced.
//

PHYSICAL_ADDRESS HlPl11UartPhysicalAddress;

//
// Store the clock frequency of the UART, initialized to a value that should
// never see the light of day unless UART initialization is forced.
//

ULONG HlPl11UartClockFrequency;

//
// Store a boolean indicating whether enumeration of this serial port should be
// forced. Setting this to TRUE causes this module to register a serial port
// even if one is not found in firmware tables. This is useful to temporarily
// enable boot debugging on a system.
//

BOOL HlPl11ForceEnumeration = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpPl11SerialModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the PL-011 Serial module. Its role is to
    detect and report the presence of any PL-011s.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PDEBUG_DEVICE_INFORMATION DebugDevice;
    ULONG DebugDeviceIndex;
    PDEBUG_PORT_TABLE2 DebugTable;
    DEBUG_DEVICE_DESCRIPTION Description;
    BOOL FoundIt;
    PGENERIC_ADDRESS GenericAddress;
    USHORT GenericAddressOffset;
    VOID *OemData;
    USHORT PortSubType;
    USHORT PortType;
    KSTATUS Status;

    //
    // Look for the debug port table.
    //

    FoundIt = FALSE;
    DebugTable = HlGetAcpiTable(DBG2_SIGNATURE, NULL);
    if (DebugTable != NULL) {
        DebugDevice =
            (PDEBUG_DEVICE_INFORMATION)(((PVOID)DebugTable) +
                                        DebugTable->DeviceInformationOffset);

        for (DebugDeviceIndex = 0;
             DebugDeviceIndex < DebugTable->DeviceInformationCount;
             DebugDeviceIndex += 1) {

            PortType = READ_UNALIGNED16(&(DebugDevice->PortType));
            PortSubType = READ_UNALIGNED16(&(DebugDevice->PortSubType));
            if ((PortType == DEBUG_PORT_TYPE_SERIAL) &&
                (PortSubType == DEBUG_PORT_SERIAL_ARM_PL011) &&
                (DebugDevice->GenericAddressCount == 1) &&
                (DebugDevice->OemDataLength == sizeof(ULONG))) {

                GenericAddressOffset = READ_UNALIGNED16(
                                    &(DebugDevice->BaseAddressRegisterOffset));

                GenericAddress = (PGENERIC_ADDRESS)(((PVOID)DebugDevice) +
                                                    GenericAddressOffset);

                HlPl11UartPhysicalAddress = GenericAddress->Address;
                OemData = ((PVOID)DebugDevice + DebugDevice->OemDataOffset);
                HlPl11UartClockFrequency = READ_UNALIGNED32(OemData);
                FoundIt = TRUE;
                break;
            }

            DebugDevice = (PDEBUG_DEVICE_INFORMATION)(((PVOID)DebugDevice) +
                                     READ_UNALIGNED16(&(DebugDevice->Length)));
        }
    }

    //
    // If no serial port was found and enumeration was not forced, then bail.
    //

    if ((FoundIt == FALSE) && (HlPl11ForceEnumeration == FALSE)) {
        Status = STATUS_SUCCESS;
        goto Pl11SerialModuleEntryEnd;
    }

    //
    // Report the physical address space that the UART is occupying.
    //

    HlReportPhysicalAddressUsage(HlPl11UartPhysicalAddress, PL11_UART_SIZE);
    RtlZeroMemory(&Description, sizeof(DEBUG_DEVICE_DESCRIPTION));
    Description.TableVersion = DEBUG_DEVICE_DESCRIPTION_VERSION;
    Description.FunctionTable.Reset = HlpPl11Reset;
    Description.FunctionTable.Transmit = HlpPl11Transmit;
    Description.FunctionTable.Receive = HlpPl11Receive;
    Description.FunctionTable.GetStatus = HlpPl11GetStatus;
    Description.FunctionTable.Disconnect = HlpPl11Disconnect;
    Description.PortType = DEBUG_PORT_TYPE_SERIAL;
    Description.PortSubType = DEBUG_PORT_SERIAL_ARM_PL011;
    Description.Identifier = HlPl11UartPhysicalAddress;
    Status = HlRegisterHardware(HardwareModuleDebugDevice, &Description);
    if (!KSUCCESS(Status)) {
        goto Pl11SerialModuleEntryEnd;
    }

Pl11SerialModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpPl11Reset (
    PVOID Context,
    ULONG BaudRate
    )

/*++

Routine Description:

    This routine initializes and resets a debug device, preparing it to send
    and receive data.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    BaudRate - Supplies the baud rate to set.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The device will not be used if a failure
    status code is returned.

--*/

{

    ULONG BaudRateCount;
    PBAUD_RATE BaudRateData;
    PBAUD_RATE BaudRates;
    ULONG RateIndex;
    KSTATUS Status;
    ULONG UartControlValue;
    ULONG UartLineControlValue;

    BaudRateData = NULL;
    switch (HlPl11UartClockFrequency) {
    case UART_CLOCK_FREQUENCY_3MHZ:
        BaudRates = HlPl11Available3MhzRates;
        BaudRateCount = sizeof(HlPl11Available3MhzRates) /
                        sizeof(HlPl11Available3MhzRates[0]);

        break;

    case UART_CLOCK_FREQUENCY_14MHZ:
        BaudRates = HlPl11Available14MhzRates;
        BaudRateCount = sizeof(HlPl11Available14MhzRates) /
                        sizeof(HlPl11Available14MhzRates[0]);

        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        goto Pl11ResetEnd;
    }

    for (RateIndex = 0; RateIndex < BaudRateCount; RateIndex += 1) {
        BaudRateData = &(BaudRates[RateIndex]);
        if (BaudRateData->BaudRate == BaudRate) {
            break;
        }
    }

    if (RateIndex == BaudRateCount) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto Pl11ResetEnd;
    }

    //
    // Map the controller if it has not yet been done.
    //

    if (HlPl11UartBase == NULL) {
        HlPl11UartBase = HlMapPhysicalAddress(HlPl11UartPhysicalAddress,
                                              PL11_UART_SIZE,
                                              TRUE);

        if (HlPl11UartBase == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Pl11ResetEnd;
        }
    }

    //
    // Program the Control Register. Enable the UART, transmitter, and receiver.
    // Clearing the other bits turns off hardware flow control, disables
    // loop-back mode, and disables IrDA features.
    //

    UartControlValue = PL11_UART_CONTROL_UART_ENABLE |
                       PL11_UART_CONTROL_TRANSMITTER_ENABLE |
                       PL11_UART_CONTROL_RECEIVER_ENABLE;

    WRITE_SERIAL_REGISTER(UartControl, UartControlValue);

    //
    // Mask all interrupts.
    //

    WRITE_SERIAL_REGISTER(UartInterruptMask, PL11_UART_INTERRUPT_MASK);

    //
    // Disable DMA.
    //

    WRITE_SERIAL_REGISTER(UartDmaControl, 0);

    //
    // Set the correct divisor values for the chosen baud rate.
    //

    WRITE_SERIAL_REGISTER(UartIntegerBaudRate, BaudRateData->IntegerDivisor);
    WRITE_SERIAL_REGISTER(UartFractionalBaudRate,
                          BaudRateData->FractionalDivisor);

    //
    // Program the Line Control Register. Setting bit 4 enables the FIFOs.
    // Clearing bit 3 sets 1 stop bit. Clearing bit 1 sets no parity. Clearing
    // bit 0 means not sending a break. The TRM for the PL-011 implies that the
    // ordering of the Integer Baud Rate, Fractional Baud Rate, and Line Control
    // registers is somewhat fixed, so observe that order here.
    //

    UartLineControlValue = PL11_UART_LINE_CONTROL_FIFO_ENABLE |
                           PL11_UART_LINE_CONTROL_WORD_LENGTH_8BITS;

    WRITE_SERIAL_REGISTER(UartLineControl, UartLineControlValue);

    //
    // Write a 0 to the receive status register to clear all errors.
    //

    WRITE_SERIAL_REGISTER(UartReceiveStatus, 0);
    Status = STATUS_SUCCESS;

Pl11ResetEnd:
    return Status;
}

KSTATUS
HlpPl11Transmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    ULONG ByteIndex;
    PUCHAR Bytes;

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            if ((READ_SERIAL_REGISTER(UartReceiveStatus) &
                 PL11_UART_RECEIVE_STATUS_ERROR_MASK) != 0) {

                return STATUS_DEVICE_IO_ERROR;
            }

        } while ((READ_SERIAL_REGISTER(UartFlags) &
                  PL11_UART_FLAG_TRANSMIT_BUSY) != 0);

        //
        // Send the byte and return.
        //

        WRITE_SERIAL_REGISTER(UartDataBuffer, Bytes[ByteIndex]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpPl11Receive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device. If no data is
    available, this routine should return immediately. If only some of the
    requested data is available, this routine should return the data that can
    be obtained now and return.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_DATA_AVAILABLE if there was no data to be read at the current
    time.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    ULONG ByteCount;
    ULONG ByteIndex;
    PUCHAR Bytes;
    ULONG DataRegister;
    KSTATUS Status;

    ByteCount = *Size;
    Bytes = Data;
    Status = STATUS_NO_DATA_AVAILABLE;

    //
    // The receive status register contains the break, framing, and parity
    // error status for the character read prior to the read of the status. The
    // overrun error is set as soon as an overrun occurs. As a result, read the
    // data register rather than the status register; the data register also
    // returns the status bits.
    //

    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        if ((READ_SERIAL_REGISTER(UartFlags) &
             PL11_UART_FLAG_RECEIVE_EMPTY) != 0) {

            break;
        }

        DataRegister = READ_SERIAL_REGISTER(UartDataBuffer);
        if ((DataRegister & PL11_UART_DATA_ERROR_MASK) != 0) {

            //
            // Clear the errors and return.
            //

            WRITE_SERIAL_REGISTER(UartReceiveStatus,
                                  PL11_UART_RECEIVE_STATUS_ERROR_CLEAR);

            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }

        Bytes[ByteIndex] = DataRegister & PL11_UART_DATA_BYTE_MASK;
        Status = STATUS_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

KSTATUS
HlpPl11GetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    Status code.

--*/

{

    ULONG Flags;

    *ReceiveDataAvailable = FALSE;
    Flags = READ_SERIAL_REGISTER(UartFlags);
    if ((Flags & PL11_UART_FLAG_RECEIVE_EMPTY) == 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return STATUS_SUCCESS;
}

VOID
HlpPl11Disconnect (
    PVOID Context
    )

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    Context - Supplies the pointer to the port's context, provided by the
        hardware module upon initialization.

Return Value:

    None.

--*/

{

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

