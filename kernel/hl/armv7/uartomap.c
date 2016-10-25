/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uartomap.c

Abstract:

    This module implements the kernel serial port interface for the UART
    in the Texas Instruments OMAP3 and OMAP4.

Author:

    Evan Green 16-Aug-2012

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
    HlReadRegister32(HlOmapUartBase + (_Register))

//
// This macro performs a 32-bit write to the serial port.
//

#define WRITE_SERIAL_REGISTER(_Register, _Value) \
    HlWriteRegister32(HlOmapUartBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default UART physical address used only when enumeration is
// forced.
//

#define OMAP4_UART3_BASE 0x48020000
#define OMAP_UART_SIZE 0x1000

#define NUMBER_OF_BAUD_RATES \
    (sizeof(HlpOmapAvailableRates) / sizeof(HlpOmapAvailableRates[0]))

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

KSTATUS
HlpOmapSerialReset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
HlpOmapSerialTransmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
HlpOmapSerialReceive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
HlpOmapSerialGetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
HlpOmapSerialDisconnect (
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

    DivisorHigh - Stores the high divisor to program into the OMAP UART.

    DivisorLow - Stores the low divisor to program into the OMAP UART.

--*/

typedef struct _BAUD_RATE {
    ULONG BaudRate;
    UCHAR DivisorHigh;
    UCHAR DivisorLow;
} BAUD_RATE, *PBAUD_RATE;

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

//
// -------------------------------------------------------------------- Globals
//

//
// Integer and fractional baud rates for an input clock of 14.7456 MHz.
//

BAUD_RATE HlpOmapAvailableRates[] = {
    {9600, 1, 0x38},
    {19200, 0, 0x9C},
    {38400, 0, 0x4E},
    {57600, 0, 0x34},
    {115200, 0, 0x1A}
};

//
// Store the virtual address of the UART.
//

PVOID HlOmapUartBase = NULL;

//
// Store the physical address of the UART. For OMAP4 set this to 0x48020000,
// and for the Beaglebone Black it's 0x44E09000.
//

PHYSICAL_ADDRESS HlOmapUartPhysicalAddress;

//
// Store a boolean indicating whether enumeration of this serial port should be
// forced. Setting this to TRUE causes this module to register a serial port
// even if one is not found in firmware tables. This is useful to temporarily
// enable boot debugging on a system.
//

BOOL HlOmapUartForceEnumeration = FALSE;

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpOmapSerialModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the OMAP3/OMAP4 Serial module. Its role
    is to detect and report the presence of any UARTs.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PDEBUG_DEVICE_INFORMATION DebugDevice;
    ULONG DebugDeviceIndex;
    PDEBUG_PORT_TABLE2 DebugPortTable;
    DEBUG_DEVICE_DESCRIPTION Description;
    BOOL FoundIt;
    PGENERIC_ADDRESS GenericAddress;
    USHORT GenericAddressOffset;
    USHORT PortSubType;
    USHORT PortType;
    KSTATUS Status;

    FoundIt = FALSE;
    DebugPortTable = HlGetAcpiTable(DBG2_SIGNATURE, NULL);
    if (DebugPortTable != NULL) {
        DebugDevice =
            (PDEBUG_DEVICE_INFORMATION)(((PVOID)DebugPortTable) +
                                      DebugPortTable->DeviceInformationOffset);

        for (DebugDeviceIndex = 0;
             DebugDeviceIndex < DebugPortTable->DeviceInformationCount;
             DebugDeviceIndex += 1) {

            PortType = READ_UNALIGNED16(&(DebugDevice->PortType));
            PortSubType = READ_UNALIGNED16(&(DebugDevice->PortSubType));
            if ((PortType == DEBUG_PORT_TYPE_SERIAL) &&
                (PortSubType == DEBUG_PORT_SERIAL_ARM_OMAP4) &&
                (DebugDevice->GenericAddressCount == 1)) {

                GenericAddressOffset = READ_UNALIGNED16(
                                    &(DebugDevice->BaseAddressRegisterOffset));

                GenericAddress = (PGENERIC_ADDRESS)(((PVOID)DebugDevice) +
                                                    GenericAddressOffset);

                HlOmapUartPhysicalAddress = GenericAddress->Address;
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

    if ((FoundIt == FALSE) && (HlOmapUartForceEnumeration == FALSE)) {
        Status = STATUS_SUCCESS;
        goto OmapSerialModuleEntryEnd;
    }

    //
    // Report the physical address space occupied by the UART.
    //

    HlReportPhysicalAddressUsage(HlOmapUartPhysicalAddress, OMAP_UART_SIZE);
    RtlZeroMemory(&Description, sizeof(DEBUG_DEVICE_DESCRIPTION));
    Description.TableVersion = DEBUG_DEVICE_DESCRIPTION_VERSION;
    Description.FunctionTable.Reset = HlpOmapSerialReset;
    Description.FunctionTable.Transmit = HlpOmapSerialTransmit;
    Description.FunctionTable.Receive = HlpOmapSerialReceive;
    Description.FunctionTable.GetStatus = HlpOmapSerialGetStatus;
    Description.FunctionTable.Disconnect = HlpOmapSerialDisconnect;
    Description.PortType = DEBUG_PORT_TYPE_SERIAL;
    Description.PortSubType = DEBUG_PORT_SERIAL_ARM_OMAP4;
    Description.Identifier = HlOmapUartPhysicalAddress;
    Status = HlRegisterHardware(HardwareModuleDebugDevice, &Description);
    if (!KSUCCESS(Status)) {
        goto OmapSerialModuleEntryEnd;
    }

OmapSerialModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpOmapSerialReset (
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

    PBAUD_RATE BaudRateData;
    UCHAR EnhancedRegister;
    ULONG RateIndex;
    KSTATUS Status;

    EnhancedRegister = 0;
    BaudRateData = NULL;
    for (RateIndex = 0; RateIndex < NUMBER_OF_BAUD_RATES; RateIndex += 1) {
        BaudRateData = &(HlpOmapAvailableRates[RateIndex]);
        if (BaudRateData->BaudRate == BaudRate) {
            break;
        }
    }

    if (RateIndex == NUMBER_OF_BAUD_RATES) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto OmapSerialResetEnd;
    }

    //
    // Map the controller if it has not yet been done.
    //

    if (HlOmapUartBase == NULL) {
        HlOmapUartBase = HlMapPhysicalAddress(HlOmapUartPhysicalAddress,
                                              OMAP_UART_SIZE,
                                              TRUE);

        if (HlOmapUartBase == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto OmapSerialResetEnd;
        }
    }

    //
    // Set mode to disable UART.
    //

    WRITE_SERIAL_REGISTER(UartMode1, OMAP_UART_MODE1_DISABLED);

    //
    // Switch to configuration mode B, and set the Enhanced Mode bit to allow
    // writes to the Interrupt Enable and FIFO Control registers. Setting the
    // Enhanced Features register also disables auto RTC/CTS, disables
    // special character detection, and disables software flow control.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_CONFIGURATION_B);
    EnhancedRegister = READ_SERIAL_REGISTER(UartEnhancedFeatures);
    WRITE_SERIAL_REGISTER(UartEnhancedFeatures,
                          EnhancedRegister | OMAP_UART_WRITE_CONFIGURATION_BIT);

    //
    // Switch to configuration mode A and set the Modem Control Register to
    // basically disable all modem functionality.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_CONFIGURATION_A);
    WRITE_SERIAL_REGISTER(UartModemControl, 0);

    //
    // Switch back to operational mode to get to the Interrupt Enable Register.
    // Program the interrupt enable to 0, which masks all interrupts and
    // disables sleep mode. The baud rate divisors cannot be programmed unless
    // sleep mode is disabled.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_OPERATIONAL_MODE);
    WRITE_SERIAL_REGISTER(UartInterruptEnable, 0);

    //
    // Switch to Configuration Mode B again to set the divisors. Set them to 0
    // for now to disable clocking, so that the FIFO control register can be
    // programmed.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_CONFIGURATION_B);
    WRITE_SERIAL_REGISTER(UartDivisorHigh, 0);
    WRITE_SERIAL_REGISTER(UartDivisorLow, 0);
    WRITE_SERIAL_REGISTER(UartEnhancedFeatures, EnhancedRegister);

    //
    // Switch to Configuration Mode A and program the FIFO control register to
    // enable and clear the FIFOs.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_CONFIGURATION_A);
    WRITE_SERIAL_REGISTER(UartFifoControl, 0x7);

    //
    // Set Supplementary Control to 0 to disable DMA. Set System Configuration
    // to 0 to turn off all power saving features, and set Wake Enable to 0
    // to disable wake on interrupt capabilities.
    //

    WRITE_SERIAL_REGISTER(UartSupplementaryControl, 0);
    WRITE_SERIAL_REGISTER(UartSystemConfiguration, 0);
    WRITE_SERIAL_REGISTER(UartWakeEnable, 0);

    //
    // Program the real divisor values to restart the baud rate clock.
    //

    WRITE_SERIAL_REGISTER(UartDivisorHigh, BaudRateData->DivisorHigh);
    WRITE_SERIAL_REGISTER(UartDivisorLow, BaudRateData->DivisorLow);

    //
    // Set Mode2 to 0 for normal UART operation (without pulse shaping), and
    // set Mode1 to 0 to enable the UART in normal UART mode (no IrDA or other
    // crazy modes).
    //

    WRITE_SERIAL_REGISTER(UartMode2, OMAP_UART_MODE2_OPERATIONAL);
    WRITE_SERIAL_REGISTER(UartMode1, OMAP_UART_MODE1_OPERATIONAL);

    //
    // Switch back to operational mode, which also configures the UART for the
    // 8-N-1 configuration, and return success.
    //

    WRITE_SERIAL_REGISTER(UartLineControl, OMAP_UART_OPERATIONAL_MODE);
    Status = STATUS_SUCCESS;

OmapSerialResetEnd:
    return Status;
}

KSTATUS
HlpOmapSerialTransmit (
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
    ULONG StatusRegister;

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            StatusRegister = READ_SERIAL_REGISTER(UartLineStatus);
            if ((StatusRegister & OMAP_UART_LINE_ERRORS) != 0) {
                return STATUS_DEVICE_IO_ERROR;
            }

        } while ((StatusRegister & OMAP_UART_TRANSMIT_DONE) == 0);

        //
        // Send the byte and return.
        //

        WRITE_SERIAL_REGISTER(UartTransmitData, Bytes[ByteIndex]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpOmapSerialReceive (
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
    KSTATUS Status;
    ULONG StatusRegister;

    ByteCount = *Size;
    Bytes = Data;
    Status = STATUS_NO_DATA_AVAILABLE;
    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        StatusRegister = READ_SERIAL_REGISTER(UartLineStatus);
        if ((StatusRegister & OMAP_UART_LINE_ERRORS) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }

        if ((StatusRegister & OMAP_UART_RECEIVE_READY) == 0) {
            break;
        }

        Bytes[ByteIndex] = READ_SERIAL_REGISTER(UartReceiveData);
        Status = STATUS_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

KSTATUS
HlpOmapSerialGetStatus (
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

    ULONG StatusRegister;

    *ReceiveDataAvailable = FALSE;
    StatusRegister = READ_SERIAL_REGISTER(UartLineStatus);
    if ((StatusRegister & OMAP_UART_RECEIVE_READY) != 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return STATUS_SUCCESS;
}

VOID
HlpOmapSerialDisconnect (
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

