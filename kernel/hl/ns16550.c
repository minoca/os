/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ns16550.c

Abstract:

    This module implements the kernel serial port interface on a 16550 standard
    UART.

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
#include <minoca/kernel/ioport.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Macros to read from and write to 16550 registers.
//

#define NS16550_READ8(_Device, _Register) \
    (_Device)->Read8((_Device), (_Register))

#define NS16550_WRITE8(_Device, _Register, _Value) \
    (_Device)->Write8((_Device), (_Register), (_Value))

//
// This macro returns the offset of a given register from its base.
//

#define NS16550_REGISTER_OFFSET(_Device, _Register) \
    ((_Device)->RegisterOffset + ((_Register) << (_Device)->RegisterShift))

//
// ---------------------------------------------------------------- Definitions
//

#define NS16550_ALLOCATION_TAG 0x3631534E

//
// If forced, define the port number to assume the serial port is at.
//

#define NS16550_DEFAULT_IO_PORT_BASE 0x3F8
#define NS16550_DEFAULT_BASE_BAUD 115200

//
// Define the bits for the PC UART Line Status register.
//

#define NS16550_LINE_STATUS_DATA_READY     0x01
#define NS16550_LINE_STATUS_TRANSMIT_EMPTY 0x20
#define NS16550_LINE_STATUS_ERRORS         0x8E

//
// Define the possible register shift values.
//

#define NS16550_1_BYTE_REGISTER_SHIFT 0
#define NS16550_2_BYTE_REGISTER_SHIFT 1
#define NS16550_4_BYTE_REGISTER_SHIFT 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _NS16550_REGISTER {
    Ns16550Data            = 0,
    Ns16550DivisorLow      = 0,
    Ns16550InterruptEnable = 1,
    Ns16550DivisorHigh     = 1,
    Ns16550InterruptStatus = 2,
    Ns16550FifoControl     = 2,
    Ns16550LineControl     = 3,
    Ns16550ModemControl    = 4,
    Ns16550LineStatus      = 5,
    Ns16550ModemStatus     = 6,
    Ns16550Scratch         = 7
} NS16550_REGISTER, *PNS16550_REGISTER;

typedef struct _NS16550 NS16550, *PNS16550;

typedef
UCHAR
(*PNS16550_READ8) (
    PNS16550 Device,
    NS16550_REGISTER Register
    );

/*++

Routine Description:

    This routine reads a 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

typedef
VOID
(*PNS16550_WRITE8) (
    PNS16550 Device,
    NS16550_REGISTER Register,
    UCHAR Value
    );

/*++

Routine Description:

    This routine writes to a 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the context for a 16550 UART.

Members:

    MemoryBase - Stores a pointer to the virtual address of the registers, if
        the registers are memory mapped. This contains NULL for I/O port
        implementations.

    IoBase - Stores the I/O port base of the registers if they are accessed via
        I/O ports.

    RegisterOffset - Stores the offset in bytes from the start of the register
        base to the 16550 registers.

    RegisterShift - Stores the amount to shift the register number by to get
        the real register.

    BaseBaud - Stores the base baud rate for a divisor value of 1.

    Flags - Stores the bitmask of flags. See DEBUG_PORT_16550_OEM_FLAG_* for
        definitions.

    PhysicalMemoryBase - Stores the physical address

    RegionSize - Stores the size of the region.

    Read8 - Stores a pointer to a function used to read from the registers.

    Write8 - Stores a pointer to a functino used to write to the registers.

--*/

struct _NS16550 {
    PVOID MemoryBase;
    USHORT IoBase;
    UINTN RegisterOffset;
    ULONG RegisterShift;
    ULONG BaseBaud;
    ULONG Flags;
    PHYSICAL_ADDRESS PhysicalMemoryBase;
    UINTN RegionSize;
    PNS16550_READ8 Read8;
    PNS16550_WRITE8 Write8;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpNs16550RegisterDevice (
    USHORT IoPortBase,
    PHYSICAL_ADDRESS PhysicalBase,
    ULONG Size,
    PDEBUG_PORT_16550_OEM_DATA OemData
    );

KSTATUS
HlpNs16550Reset (
    PVOID Context,
    ULONG BaudRate
    );

KSTATUS
HlpNs16550Transmit (
    PVOID Context,
    PVOID Data,
    ULONG Size
    );

KSTATUS
HlpNs16550Receive (
    PVOID Context,
    PVOID Data,
    PULONG Size
    );

KSTATUS
HlpNs16550GetStatus (
    PVOID Context,
    PBOOL ReceiveDataAvailable
    );

VOID
HlpNs16550Disconnect (
    PVOID Context
    );

UCHAR
HlpNs16550ReadIo8 (
    PNS16550 Device,
    NS16550_REGISTER Register
    );

VOID
HlpNs16550WriteIo8 (
    PNS16550 Device,
    NS16550_REGISTER Register,
    UCHAR Value
    );

UCHAR
HlpNs16550ReadMemory8 (
    PNS16550 Device,
    NS16550_REGISTER Register
    );

VOID
HlpNs16550WriteMemory8 (
    PNS16550 Device,
    NS16550_REGISTER Register,
    UCHAR Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a boolean that can be set to force the UART to enumerate.
//

BOOL HlNs16550ForceEnumeration = FALSE;

//
// Define a boolean that be set to force the UART to never enumerate.
//

BOOL HlNs16550ForceNoEnumeration = FALSE;

NS16550 HlNs16550Default = {
    NULL,
    NS16550_DEFAULT_IO_PORT_BASE,
    0,
    0,
    NS16550_DEFAULT_BASE_BAUD,
    0,
    0,
    0,
    HlpNs16550ReadIo8,
    HlpNs16550WriteIo8
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpNs16550SerialModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the PC Serial module. Its role is to
    detect and report the presence of a PC Serial port module.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PDEBUG_PORT_TABLE2 DebugTable;
    PDEBUG_DEVICE_INFORMATION Device;
    ULONG DeviceIndex;
    BOOL FoundDevice;
    PGENERIC_ADDRESS GenericAddress;
    ULONG IoPortBase;
    PDEBUG_PORT_16550_OEM_DATA OemData;
    PHYSICAL_ADDRESS PhysicalBase;
    UINTN RegionSize;
    PULONG SizePointer;
    KSTATUS Status;

    if (HlNs16550ForceNoEnumeration != FALSE) {
        return;
    }

    FoundDevice = FALSE;
    DebugTable = HlGetAcpiTable(DBG2_SIGNATURE, NULL);
    if (DebugTable != NULL) {

        //
        // Loop through looking for 16550 debug devices.
        //

        Device =
              (PDEBUG_DEVICE_INFORMATION)((PVOID)DebugTable +
                                          DebugTable->DeviceInformationOffset);

        SizePointer = (PULONG)((PVOID)Device + Device->AddressSizeOffset);
        for (DeviceIndex = 0;
             DeviceIndex < DebugTable->DeviceInformationCount;
             DeviceIndex += 1) {

            IoPortBase = 0;
            PhysicalBase = 0;
            OemData = NULL;
            RegionSize = 0;
            if ((Device->PortType == DEBUG_PORT_TYPE_SERIAL) &&
                ((Device->PortSubType == DEBUG_PORT_SERIAL_16550) ||
                 (Device->PortSubType == DEBUG_PORT_SERIAL_16550_COMPATIBLE))) {

                if ((Device->OemDataOffset != 0) &&
                    (Device->OemDataLength >=
                     sizeof(DEBUG_PORT_16550_OEM_DATA))) {

                    OemData = (PDEBUG_PORT_16550_OEM_DATA)((PUCHAR)Device +
                                                        Device->OemDataOffset);
                }

                if (Device->GenericAddressCount >= 1) {
                    GenericAddress =
                         (PGENERIC_ADDRESS)((PVOID)Device +
                                            Device->BaseAddressRegisterOffset);

                    if (GenericAddress->AddressSpaceId == AddressSpaceMemory) {
                        PhysicalBase = GenericAddress->Address;
                        RegionSize = *SizePointer;
                        FoundDevice = TRUE;
                        IoPortBase = 0;

                    } else if (GenericAddress->AddressSpaceId ==
                               AddressSpaceIo) {

                        IoPortBase = GenericAddress->Address;
                        RegionSize = *SizePointer;
                        FoundDevice = TRUE;
                    }

                    Status = HlpNs16550RegisterDevice(IoPortBase,
                                                      PhysicalBase,
                                                      RegionSize,
                                                      OemData);

                    if (!KSUCCESS(Status)) {
                        goto Ns16550ModuleEntryEnd;
                    }

                    FoundDevice = TRUE;
                }
            }

            Device = (PDEBUG_DEVICE_INFORMATION)((PVOID)Device +
                                          READ_UNALIGNED16(&(Device->Length)));
        }
    }

    //
    // If no device was found and enumeration was not forced, return.
    //

    if ((FoundDevice != FALSE) || (HlNs16550ForceEnumeration == FALSE)) {
        return;
    }

    //
    // Enumerate a forced serial device.
    //

    Status = HlpNs16550RegisterDevice(0, 0, 0, NULL);
    if (!KSUCCESS(Status)) {
        goto Ns16550ModuleEntryEnd;
    }

Ns16550ModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpNs16550RegisterDevice (
    USHORT IoPortBase,
    PHYSICAL_ADDRESS PhysicalBase,
    ULONG Size,
    PDEBUG_PORT_16550_OEM_DATA OemData
    )

/*++

Routine Description:

    This routine registers a 16550 UART debug device.

Arguments:

    IoPortBase - Supplies the I/O port base of the UART, if it is based in
        I/O port space.

    PhysicalBase - Supplies the physical base address of the UART if it is
        memory mapped, or 0 if the device is in I/O port space.

    Size - Supplies the size of the region, in bytes.

    OemData - Supplies an optional pointer to the OEM data.

Return Value:

    Status code.

--*/

{

    DEBUG_DEVICE_DESCRIPTION DebugDevice;
    PNS16550 DeviceContext;
    KSTATUS Status;

    //
    // Allocate the context and fill it in.
    //

    if ((IoPortBase == 0) && (PhysicalBase == 0) && (Size == 0) &&
        (OemData == NULL)) {

        DeviceContext = &HlNs16550Default;

    } else {
        DeviceContext = HlAllocateMemory(sizeof(NS16550),
                                         NS16550_ALLOCATION_TAG,
                                         FALSE,
                                         NULL);

        if (DeviceContext == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Ns16550RegisterDeviceEnd;
        }

        RtlZeroMemory(DeviceContext, sizeof(NS16550));
        DeviceContext->PhysicalMemoryBase = PhysicalBase;
        DeviceContext->IoBase = IoPortBase;
        DeviceContext->BaseBaud = NS16550_DEFAULT_BASE_BAUD;
        DeviceContext->RegionSize = Size;

        //
        // Use the OEM data if it's valid.
        //

        if ((OemData != NULL) &&
            (OemData->Signature == DEBUG_PORT_16550_OEM_DATA_SIGNATURE)) {

            DeviceContext->RegisterOffset = OemData->RegisterOffset;
            DeviceContext->RegisterShift = OemData->RegisterShift;
            DeviceContext->BaseBaud = OemData->BaseBaud;
            DeviceContext->Flags = OemData->Flags;
        }
    }

    //
    // Register the serial port.
    //

    RtlZeroMemory(&DebugDevice, sizeof(DEBUG_DEVICE_DESCRIPTION));
    DebugDevice.TableVersion = DEBUG_DEVICE_DESCRIPTION_VERSION;
    DebugDevice.Context = DeviceContext;
    DebugDevice.FunctionTable.Reset = HlpNs16550Reset;
    DebugDevice.FunctionTable.Transmit = HlpNs16550Transmit;
    DebugDevice.FunctionTable.Receive = HlpNs16550Receive;
    DebugDevice.FunctionTable.GetStatus = HlpNs16550GetStatus;
    DebugDevice.FunctionTable.Disconnect = HlpNs16550Disconnect;
    DebugDevice.PortType = DEBUG_PORT_TYPE_SERIAL;
    DebugDevice.PortSubType = DEBUG_PORT_SERIAL_16550_COMPATIBLE;
    if (PhysicalBase != 0) {
        DebugDevice.Identifier = PhysicalBase;

    } else {
        DebugDevice.Identifier = IoPortBase;
    }

    Status = HlRegisterHardware(HardwareModuleDebugDevice, &DebugDevice);
    if (!KSUCCESS(Status)) {
        goto Ns16550RegisterDeviceEnd;
    }

Ns16550RegisterDeviceEnd:
    return Status;
}

KSTATUS
HlpNs16550Reset (
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

    ULONG CurrentBaud;
    PNS16550 Device;
    ULONG Divisor;
    UCHAR Value;

    Device = Context;
    if (Device == NULL) {
        Device = &HlNs16550Default;
    }

    //
    // Compute the baud rate divisor.
    //

    if (BaudRate > Device->BaseBaud) {
        return STATUS_NOT_SUPPORTED;
    }

    Divisor = 1;
    while (TRUE) {
        CurrentBaud = Device->BaseBaud / Divisor;
        if ((CurrentBaud <= BaudRate) || (CurrentBaud == 0)) {
            break;
        }

        Divisor += 1;
    }

    if ((CurrentBaud == 0) || (Divisor > MAX_USHORT)) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Map the registers if needed.
    //

    if ((Device->PhysicalMemoryBase != 0) &&
        (Device->MemoryBase == NULL)) {

        Device->MemoryBase = HlMapPhysicalAddress(Device->PhysicalMemoryBase,
                                                  Device->RegionSize,
                                                  TRUE);

        if (Device->MemoryBase == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    //
    // Determine the correct register access function.
    //

    if (Device->MemoryBase != NULL) {
        Device->Read8 = HlpNs16550ReadMemory8;
        Device->Write8 = HlpNs16550WriteMemory8;

    } else {
        Device->Read8 = HlpNs16550ReadIo8;
        Device->Write8 = HlpNs16550WriteIo8;
    }

    //
    // Begin programming the 16550 controller. The topmost bit in the line
    // control register turns the DLAB (Data Latch Address Byte) on. This
    // changes the meanings of the registers, allowing us to program the baud
    // rate divisor values.
    //

    Value = NS16550_READ8(Device, Ns16550LineControl);
    Value |= 0x80;
    NS16550_WRITE8(Device, Ns16550LineControl, Value);

    //
    // Set the divisor bytes. This programs the baud rate generator.
    //

    NS16550_WRITE8(Device, Ns16550DivisorLow, (UCHAR)(Divisor & 0x00FF));
    NS16550_WRITE8(Device,
                   Ns16550DivisorHigh,
                   (UCHAR)((Divisor >> 8) & 0x00FF));

    //
    // Now program the FIFO queue configuration. It is assumed that the FIFOs
    // are operational, which is not true on certain machines with very old
    // UARTs. Setting bit 0 enables the FIFO. Setting bits 1 and 2 clears both
    // FIFOs. Clearing bit 3 disables DMA mode. The top 4 bits vary depending
    // on the version. Setting bit 5 enables the 64 byte FIFO, which is only
    // available on 16750s. Bit 4 is reserved. Otherwise bits 4 and 5 are
    // either reserved or dictate the transmit FIFO's empty trigger. Bits 6 and
    // 7 set the receive FIFO's trigger, where setting both bits means that
    // "2 less than full", which for the default 16 byte FIFO means 14 bytes
    // are in the buffer.
    //

    Value = 0xC7;
    if ((Device->Flags &
         DEBUG_PORT_16550_OEM_FLAG_TRANSMIT_TRIGGER_2_CHARACTERS) != 0) {

        Value |= 0x10;

    } else if ((Device->Flags & DEBUG_PORT_16550_OEM_FLAG_64_BYTE_FIFO) != 0) {
        Value |= 0x20;
    }

    NS16550_WRITE8(Device, Ns16550FifoControl, Value);

    //
    // Now program the Line Control register again. Setting bits 0 and 1 sets
    // 8 data bits. Clearing bit 2 sets one stop bit. Clearing bit 3 sets no
    // parity. Additionally, clearing bit 7 turns the DLAB latch off, changing
    // the meaning of the registers back and allowing other control registers to
    // be accessed.
    //

    NS16550_WRITE8(Device, Ns16550LineControl, 0x03);

    //
    // Setting the Modem Control register to zero disables all hardware flow
    // control.
    //

    NS16550_WRITE8(Device, Ns16550ModemControl, 0x00);
    return STATUS_SUCCESS;
}

KSTATUS
HlpNs16550Transmit (
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
    PNS16550 Device;
    UCHAR StatusRegister;

    Device = Context;
    if (Device == NULL) {
        Device = &HlNs16550Default;
    }

    Bytes = Data;
    for (ByteIndex = 0; ByteIndex < Size; ByteIndex += 1) {

        //
        // Spin waiting for the buffer to become ready to send. If an error is
        // detected, bail out and report to the caller.
        //

        do {
            StatusRegister = NS16550_READ8(Device, Ns16550LineStatus);
            if ((StatusRegister & NS16550_LINE_STATUS_ERRORS) != 0) {
                return STATUS_DEVICE_IO_ERROR;
            }

        } while ((StatusRegister & NS16550_LINE_STATUS_TRANSMIT_EMPTY) == 0);

        //
        // Send the byte and return.
        //

        NS16550_WRITE8(Device, Ns16550Data, Bytes[ByteIndex]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
HlpNs16550Receive (
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
    PNS16550 Device;
    KSTATUS Status;
    UCHAR StatusRegister;

    Device = Context;
    if (Device == NULL) {
        Device = &HlNs16550Default;
    }

    ByteCount = *Size;
    Bytes = Data;
    Status = STATUS_NO_DATA_AVAILABLE;
    for (ByteIndex = 0; ByteIndex < ByteCount; ByteIndex += 1) {
        StatusRegister = NS16550_READ8(Device, Ns16550LineStatus);
        if ((StatusRegister & NS16550_LINE_STATUS_ERRORS) != 0) {
            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }

        if ((StatusRegister & NS16550_LINE_STATUS_DATA_READY) == 0) {
            break;
        }

        Bytes[ByteIndex] = NS16550_READ8(Device, Ns16550Data);
        Status = STATUS_SUCCESS;
    }

    *Size = ByteIndex;
    return Status;
}

KSTATUS
HlpNs16550GetStatus (
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

    PNS16550 Device;
    BYTE StatusRegister;

    Device = Context;
    if (Device == NULL) {
        Device = &HlNs16550Default;
    }

    *ReceiveDataAvailable = FALSE;
    StatusRegister = NS16550_READ8(Device, Ns16550LineStatus);
    if ((StatusRegister & NS16550_LINE_STATUS_DATA_READY) != 0) {
        *ReceiveDataAvailable = TRUE;
    }

    return STATUS_SUCCESS;
}

VOID
HlpNs16550Disconnect (
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

UCHAR
HlpNs16550ReadIo8 (
    PNS16550 Device,
    NS16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads a 16550 register from an I/O port.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    USHORT Port;

    Port = Device->IoBase + NS16550_REGISTER_OFFSET(Device, Register);
    return HlIoPortInByte(Port);
}

VOID
HlpNs16550WriteIo8 (
    PNS16550 Device,
    NS16550_REGISTER Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes to an I/O port based 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    USHORT Port;

    Port = Device->IoBase + NS16550_REGISTER_OFFSET(Device, Register);
    HlIoPortOutByte(Port, Value);
    return;
}

UCHAR
HlpNs16550ReadMemory8 (
    PNS16550 Device,
    NS16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads a 16550 register from a memory mapped register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    PVOID Address;
    UCHAR Value;

    Address = Device->MemoryBase + NS16550_REGISTER_OFFSET(Device, Register);
    switch (Device->RegisterShift) {
    case NS16550_1_BYTE_REGISTER_SHIFT:
        Value = HlReadRegister8(Address);
        break;

    case NS16550_2_BYTE_REGISTER_SHIFT:
    case NS16550_4_BYTE_REGISTER_SHIFT:
    default:
        Value = HlReadRegister32(Address);
        break;
    }

    return Value;
}

VOID
HlpNs16550WriteMemory8 (
    PNS16550 Device,
    NS16550_REGISTER Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes to a memory mapped 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    PVOID Address;

    Address = Device->MemoryBase + NS16550_REGISTER_OFFSET(Device, Register);
    switch (Device->RegisterShift) {
    case NS16550_1_BYTE_REGISTER_SHIFT:
        HlWriteRegister8(Address, Value);
        break;

    case NS16550_2_BYTE_REGISTER_SHIFT:
    case NS16550_4_BYTE_REGISTER_SHIFT:
    default:
        HlWriteRegister32(Address, (ULONG)Value);
        break;
    }

    return;
}

