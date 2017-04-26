/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ser16550.c

Abstract:

    This module implements a kernel driver for 16550-like UARTs.

Author:

    Evan Green 21-Nov-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/kernel/kdebug.h>

//
// --------------------------------------------------------------------- Macros
//

//
// Macros to read from and write to 16550 registers.
//

#define SER16550_READ8(_Device, _Register) \
    (_Device)->Read8((_Device), (_Register))

#define SER16550_WRITE8(_Device, _Register, _Value) \
    (_Device)->Write8((_Device), (_Register), (_Value))

//
// This macro returns the offset of a given register from its base.
//

#define SER16550_REGISTER_OFFSET(_Device, _Register) \
    ((_Device)->RegisterOffset + ((_Register) << (_Device)->RegisterShift))

//
// This macro evaluates to non-zero if the given Oxford device ID has two
// UARTs. This matches against the Mode[2:0] bits being 101 and the UART_EN bit
// being set.
//

#define SER16550_OXFORD_DUAL_UARTS(_DeviceId) \
    (((_DeviceId) & 0x0078) == 0x0058)

//
// ---------------------------------------------------------------- Definitions
//

#define SER16550_ALLOCATION_TAG 0x36317253
#define SER16550_DEFAULT_BUFFER_SIZE 2048

#define SER16550_DEFAULT_BASE_BAUD 115200

#define SER16550A_FIFO_SIZE 16

#define SER16550_MAX_FIFO 256

#define SERIAL_PORT_DEVICE_ID_FORMAT "Serial%d"
#define SERIAL_PORT_DEVICE_ID_SIZE 50

//
// Standard 16x50 register definitions.
//

#define SER16550_LINE_CONTROL_5_DATA_BITS 0x00
#define SER16550_LINE_CONTROL_6_DATA_BITS 0x01
#define SER16550_LINE_CONTROL_7_DATA_BITS 0x02
#define SER16550_LINE_CONTROL_8_DATA_BITS 0x03
#define SER16550_LINE_CONTROL_2_STOP_BITS 0x04
#define SER16550_LINE_CONTROL_PARITY_ENABLE 0x08
#define SER16550_LINE_CONTROL_EVEN_PARITY 0x10
#define SER16550_LINE_CONTROL_SET_PARITY 0x20
#define SER16550_LINE_CONTROL_SET_BREAK 0x40
#define SER16550_LINE_CONTROL_DIVISOR_LATCH 0x80

#define SER16550_FIFO_CONTROL_ENABLE 0x01
#define SER16550_FIFO_CONTROL_CLEAR_RECEIVE 0x02
#define SER16550_FIFO_CONTROL_CLEAR_TRANSMIT 0x04
#define SER16550_FIFO_CONTROL_MULTI_DMA 0x08
#define SER16550_FIFO_CONTROL_64_BYTE_FIFO 0x20
#define SER16550_FIFO_CONTROL_RX_TRIGGER_1 (0 << 6)
#define SER16550_FIFO_CONTROL_RX_TRIGGER_4 (1 << 6)
#define SER16550_FIFO_CONTROL_RX_TRIGGER_8 (2 << 6)
#define SER16550_FIFO_CONTROL_RX_TRIGGER_14 (3 << 6)

#define SER16550_MODEM_CONTROL_DTR 0x01
#define SER16550_MODEM_CONTROL_RTS 0x02
#define SER16550_MODEM_CONTROL_OP1 0x04
#define SER16550_MODEM_CONTROL_ENABLE_INTERRUPT 0x08
#define SER16550_MODEM_CONTROL_LOOPBACK 0x10
#define SER16550_MODEM_CONTROL_ENABLE_FLOW_CONTROL 0x20

#define SER16550_INTERRUPT_ENABLE_RX_DATA 0x01
#define SER16550_INTERRUPT_ENABLE_TX_EMPTY 0x02
#define SER16550_INTERRUPT_ENABLE_RX_STATUS 0x04
#define SER16550_INTERRUPT_ENABLE_MODEM_STATUS 0x08

#define SER16550_INTERRUPT_STATUS_NONE_PENDING 0x01
#define SER16550_INTERRUPT_STATUS_RX_DATA_ERROR 0x06
#define SER16550_INTERRUPT_STATUS_RX_DATA_READY 0x04
#define SER16550_INTERRUPT_STATUS_RX_TIMEOUT 0x0C
#define SER16550_INTERRUPT_STATUS_TX_EMPTY 0x02
#define SER16550_INTERRUPT_STATUS_MODEM_STATUS 0x00
#define SER16550_INTERRUPT_STATUS_MASK 0x0E

#define SER16550_LINE_STATUS_RX_READY 0x01
#define SER16550_LINE_STATUS_OVERRUN_ERROR 0x02
#define SER16550_LINE_STATUS_PARITY_ERROR 0x04
#define SER16550_LINE_STATUS_FRAMING_ERROR 0x08
#define SER16550_LINE_STATUS_BREAK 0x10
#define SER16550_LINE_STATUS_TX_HOLDING_EMPTY 0x20
#define SER16550_LINE_STATUS_TX_EMPTY 0x40
#define SER16550_LINE_STATUS_FIFO_ERROR 0x80

#define SER16550_LINE_STATUS_ERROR_MASK \
    (SER16550_LINE_STATUS_OVERRUN_ERROR | SER16550_LINE_STATUS_PARITY_ERROR | \
     SER16550_LINE_STATUS_FRAMING_ERROR | SER16550_LINE_STATUS_FIFO_ERROR)

#define SER16550_LINE_STATUS_RX_MASK \
    (SER16550_LINE_STATUS_RX_READY | SER16550_LINE_STATUS_BREAK)

//
// Known vendors and devices.
//

#define SER16550_PCI_DEVICE_ID_FORMAT "VEN_%x&DEV_%x"

#define SER16550_VENDOR_INTEL 0x8086
#define SER16550_INTEL_QUARK 0x0936

#define SER16550_VENDOR_OXFORD 0x1415
#define SER16550_OXFORD_UART_OFFSET 0x1000
#define SER16550_OXFORD_UART_STRIDE 0x200
#define SER16550_OXFORD_BASE_BAUD 3916800

//
// Intel Quark UART information.
//

#define SER16550_INTEL_QUARK_UART_BASE_BAUD 2764800
#define SER16550_INTEL_QUARK_UART_REGISTER_SHIFT 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SER16550_OBJECT_TYPE {
    Ser16550ObjectInvalid,
    Ser16550ObjectParent,
    Ser16550ObjectChild
} SER16550_OBJECT_TYPE, *PSER16550_OBJECT_TYPE;

typedef enum _SER16550_VARIANT {
    Ser16550VariantInvalid,
    Ser16550VariantGeneric,
    Ser16550VariantQuark,
    Ser16550VariantOxford,
} SER16550_VARIANT, *PSER16550_VARIANT;

typedef enum _SER16550_REGISTER {
    Ser16550Data            = 0,
    Ser16550DivisorLow      = 0,
    Ser16550InterruptEnable = 1,
    Ser16550DivisorHigh     = 1,
    Ser16550InterruptStatus = 2,
    Ser16550FifoControl     = 2,
    Ser16550LineControl     = 3,
    Ser16550ModemControl    = 4,
    Ser16550LineStatus      = 5,
    Ser16550ModemStatus     = 6,
    Ser16550Scratch         = 7
} SER16550_REGISTER, *PSER16550_REGISTER;

typedef struct _SER16550_CHILD SER16550_CHILD, *PSER16550_CHILD;

typedef
UCHAR
(*PSER16550_READ8) (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register
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
(*PSER16550_WRITE8) (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register,
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

    This structure stores the common header for a 16550 object.

Members:

    Type - Stores the serial object type.

    ReferenceCount - Stores the reference count on the object.

--*/

typedef struct _SER16550_OBJECT {
    SER16550_OBJECT_TYPE Type;
    ULONG ReferenceCount;
} SER16550_OBJECT, *PSER16550_OBJECT;

/*++

Structure Description:

    This structure stores information about a 16550 parent context.

Members:

    Header - Store the common 16550 object header.

    Device - Stores the OS device this device belongs to.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptResourcesFound - Stores a boolean indicating whether or not the
        interrupt line and interrupt vector fields are valid.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    InterruptLock - Stores the spin lock synchronizing access to the pending
        status bits.

    Variant - Stores the variant type of 16550 controller.

    VendorId - Stores the PCI vendor ID of the device, or 0 if this was not a
        PCI device.

    DeviceId - Stores the PCI device ID of thd device.

    ChildObjects - Stores an array of child objects.

    ChildDevices - Stores an array of child devices.

    ChildCount - Stores the count of the number of children.

    RegisterShift - Stores the number of bits to shift left to get from a 16550
        register to an actual device register. By default this is applied to
        all children.

    BaseBaud - Stores the baud rate when the divisor is set to 1.

    Removed - Stores a boolean indicating that the device has been removed.

--*/

typedef struct _SER16550_PARENT {
    SER16550_OBJECT Header;
    PDEVICE Device;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    BOOL InterruptResourcesFound;
    HANDLE InterruptHandle;
    KSPIN_LOCK InterruptLock;
    SER16550_VARIANT Variant;
    USHORT VendorId;
    USHORT DeviceId;
    PSER16550_CHILD ChildObjects;
    PDEVICE *ChildDevices;
    UINTN ChildCount;
    ULONG RegisterShift;
    ULONG BaseBaud;
    BOOL Removed;
} SER16550_PARENT, *PSER16550_PARENT;

/*++

Structure Description:

    This structure stores information about a 16550 port.

Members:

    Header - Store the common 16550 object header.

    Parent - Stores a pointer to the parent structure.

    Read8 - Stores a pointer to a function used to perform an 8-bit register
        read.

    Write8 - Stores a pointer to a function used to perform an 8-bit register
        write.

    Index - Stores the index of this child into the parent.

    PhysicalAddress - Stores the base physical address of the register region,
        if the UART is memory mapped.

    MappedAddress - Stores the virtual address of the register region, if the
        UART is memory mapped.

    MappedSize - Stores the size of the mapping.

    IoPortAddress - Stores the I/O port address of the registers if the region
        is I/O port based.

    Terminal - Stores a pointer to the terminal device.

    TransmitBuffer - Stores the ring buffer of bytes waiting to be sent.

    TransmitStart - Stores the index of the next byte the hardware will send
        out.

    TransmitEnd - Stores the index of the next byte to be added to the buffer.

    TransmitSize - Stores the size of the transmit buffer in bytes.

    TransmitFifoSize - Stores the size of the hardware transmit FIFO in bytes.

    ReceiveBuffer - Stores the buffer of received bytes.

    ReceiveStart - Stores the index of the next byte software should read.

    ReceiveEnd - Stores the index of the next byte the hardware will add.

    ReceiveSize - Stores the size of the receive buffer in bytes.

    ControlFlags - Stores the currently set control flags. See
        TERMINAL_CONTROL_* definitions.

    BaudRate - Stores the currently set baud rate. This may be zero if the
        device is not configured.

    RegisterOffset - Stores the offset in bytes from the begining of the
        register region to the 16550ish registers.

    RegisterShift - Stores the number of bits to shift left to get from a 16550
        register to an actual device register.

    TransmitLock - Stores a pointer to the lock serializing access to the
        transmit buffer.

    ReceiveLock - Stores a pointer to the lock serializing access to the
        receive buffer.

    TransmitReady - Stores an event that is signaled when the UART can accept
        more outgoing data.

    ReceiveReady - Stores an event that is signaled when this receive buffer
        has data.

    InterruptWorkPending - Stores a boolean indicating that the interrupt
        worker needs to process this child.

    ShouldUnmap - Stores a boolean indicating that this child owns the mapping
        and should unmap it to clean up.

    InterruptEnable - Stores a shadow copy of the interrupt enable register.

--*/

struct _SER16550_CHILD {
    SER16550_OBJECT Header;
    PSER16550_PARENT Parent;
    PSER16550_READ8 Read8;
    PSER16550_WRITE8 Write8;
    UINTN Index;
    PRESOURCE_ALLOCATION Resource;
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID MappedAddress;
    UINTN MappedSize;
    USHORT IoPortAddress;
    PIO_HANDLE Terminal;
    PUCHAR TransmitBuffer;
    UINTN TransmitStart;
    UINTN TransmitEnd;
    UINTN TransmitSize;
    UINTN TransmitFifoSize;
    PUCHAR ReceiveBuffer;
    UINTN ReceiveStart;
    UINTN ReceiveEnd;
    UINTN ReceiveSize;
    LONG ControlFlags;
    ULONG BaudRate;
    ULONG RegisterOffset;
    ULONG RegisterShift;
    PQUEUED_LOCK TransmitLock;
    PQUEUED_LOCK ReceiveLock;
    PKEVENT TransmitReady;
    PKEVENT ReceiveReady;
    BOOL InterruptWorkPending;
    BOOL ShouldUnmap;
    UCHAR InterruptEnable;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Ser16550AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Ser16550DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Ser16550DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Ser16550DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Ser16550DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Ser16550DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Ser16550DispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

INTERRUPT_STATUS
Ser16550InterruptService (
    PVOID Context
    );

INTERRUPT_STATUS
Ser16550InterruptServiceWorker (
    PVOID Context
    );

KSTATUS
Ser16550pParentProcessResourceRequirements (
    PIRP Irp,
    PSER16550_PARENT Device
    );

KSTATUS
Ser16550pParentStartDevice (
    PIRP Irp,
    PSER16550_PARENT Device
    );

KSTATUS
Ser16550pInitializeChildrenGeneric (
    PSER16550_PARENT Parent,
    PRESOURCE_ALLOCATION_LIST AllocationList
    );

KSTATUS
Ser16550pInitializeChildrenOxford (
    PSER16550_PARENT Parent,
    PRESOURCE_ALLOCATION_LIST AllocationList
    );

KSTATUS
Ser16550pInitializeChild (
    PSER16550_PARENT Parent,
    UINTN Index
    );

VOID
Ser16550pParentEnumerateChildren (
    PIRP Irp,
    PSER16550_PARENT Device
    );

VOID
Ser16550pChildStartDevice (
    PIRP Irp,
    PSER16550_CHILD Device
    );

VOID
Ser16550pChildDispatchSystemControl (
    PIRP Irp,
    PSER16550_CHILD Device
    );

VOID
Ser16550pChildDispatchUserControl (
    PIRP Irp,
    PSER16550_CHILD Device
    );

VOID
Ser16550pStartTransmit (
    PSER16550_CHILD Device
    );

VOID
Ser16550pStopTransmit (
    PSER16550_CHILD Device
    );

KSTATUS
Ser16550pConfigureDevice (
    PSER16550_CHILD Device,
    ULONG TerminalControlFlags,
    ULONG BaudRate
    );

ULONG
Ser16550pGetFifoSize (
    PSER16550_CHILD Device
    );

VOID
Ser16550pAddReference (
    PSER16550_OBJECT Object
    );

VOID
Ser16550pReleaseReference (
    PSER16550_OBJECT Object
    );

VOID
Ser16550pDestroyDevice (
    PSER16550_OBJECT Object
    );

UCHAR
Ser16550pReadIo8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register
    );

VOID
Ser16550pWriteIo8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register,
    UCHAR Value
    );

UCHAR
Ser16550pReadMemory8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register
    );

VOID
Ser16550pWriteMemory8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register,
    UCHAR Value
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Ser16550Driver = NULL;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the 16550 driver. It registers its
    other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    Ser16550Driver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Ser16550AddDevice;
    FunctionTable.DispatchStateChange = Ser16550DispatchStateChange;
    FunctionTable.DispatchOpen = Ser16550DispatchOpen;
    FunctionTable.DispatchClose = Ser16550DispatchClose;
    FunctionTable.DispatchIo = Ser16550DispatchIo;
    FunctionTable.DispatchSystemControl = Ser16550DispatchSystemControl;
    FunctionTable.DispatchUserControl = Ser16550DispatchUserControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Ser16550AddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the 16550 device
    acts as the function driver. The driver will attach itself to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    ULONG ItemsScanned;
    PSER16550_PARENT Parent;
    ULONG PciDeviceId;
    ULONG PciVendorId;
    KSTATUS Status;

    Parent = MmAllocateNonPagedPool(sizeof(SER16550_PARENT),
                                    SER16550_ALLOCATION_TAG);

    if (Parent == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Parent, sizeof(SER16550_PARENT));
    Parent->Header.Type = Ser16550ObjectParent;
    Parent->Device = DeviceToken;
    Parent->BaseBaud = SER16550_DEFAULT_BASE_BAUD;
    Parent->Variant = Ser16550VariantGeneric;
    Parent->InterruptHandle = INVALID_HANDLE;
    KeInitializeSpinLock(&(Parent->InterruptLock));

    //
    // Detect variants by PCI vendor and device ID.
    //

    Status = RtlStringScan(DeviceId,
                           -1,
                           SER16550_PCI_DEVICE_ID_FORMAT,
                           sizeof(SER16550_PCI_DEVICE_ID_FORMAT),
                           CharacterEncodingDefault,
                           &ItemsScanned,
                           &PciVendorId,
                           &PciDeviceId);

    if ((KSUCCESS(Status)) && (ItemsScanned == 2)) {
        Parent->VendorId = PciVendorId;
        Parent->DeviceId = PciDeviceId;
        switch (PciVendorId) {
        case SER16550_VENDOR_INTEL:
            if (PciDeviceId == SER16550_INTEL_QUARK) {
                Parent->Variant = Ser16550VariantQuark;
                Parent->RegisterShift =
                                      SER16550_INTEL_QUARK_UART_REGISTER_SHIFT;

                Parent->BaseBaud = SER16550_INTEL_QUARK_UART_BASE_BAUD;
            }

            break;

        case SER16550_VENDOR_OXFORD:
            Parent->Variant = Ser16550VariantOxford;
            break;

        default:
            break;
        }
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Parent);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Parent != NULL) {
            MmFreePagedPool(Parent);
        }
    }

    return Status;
}

VOID
Ser16550DispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSER16550_CHILD Child;
    PSER16550_OBJECT Object;
    PSER16550_PARENT Parent;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if ((Irp->Direction == IrpUp) && (!KSUCCESS(IoGetIrpStatus(Irp)))) {
        return;
    }

    Object = DeviceContext;
    switch (Object->Type) {

    //
    // In this case the driver is the functional driver for the controller.
    //

    case Ser16550ObjectParent:
        Parent = PARENT_STRUCTURE(Object, SER16550_PARENT, Header);
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:

            //
            // On the way up, filter the resource requirements to add interrupt
            // vectors to any lines.
            //

            if (Irp->Direction == IrpUp) {
                Status = Ser16550pParentProcessResourceRequirements(Irp,
                                                                    Parent);

                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(Ser16550Driver, Irp, Status);
                }
            }

            break;

        case IrpMinorStartDevice:

            //
            // Attempt to fire the thing up if the bus has already started it.
            //

            if (Irp->Direction == IrpUp) {
                Status = Ser16550pParentStartDevice(Irp, Parent);
                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(Ser16550Driver, Irp, Status);
                }
            }

            break;

        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                Ser16550pParentEnumerateChildren(Irp, Parent);
            }

            break;

        case IrpMinorRemoveDevice:
            Ser16550pReleaseReference(Object);
            IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }

        break;

    //
    // In this case the object is the bus driver for an individual port.
    //

    case Ser16550ObjectChild:
        Child = PARENT_STRUCTURE(Object, SER16550_CHILD, Header);
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorStartDevice:
            Ser16550pChildStartDevice(Irp, Child);
            break;

        case IrpMinorQueryChildren:
            IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorRemoveDevice:

            ASSERT(Child->Parent->Removed != FALSE);

            KeAcquireQueuedLock(Child->ReceiveLock);
            KeAcquireQueuedLock(Child->TransmitLock);
            if (Child->Terminal != NULL) {
                IoTerminalSetDevice(Child->Terminal, NULL);
                IoClose(Child->Terminal);
                Child->Terminal = NULL;
            }

            if (Child->TransmitReady != NULL) {
                KeSignalEvent(Child->TransmitReady, SignalOptionSignalAll);
            }

            if (Child->ReceiveReady != NULL) {
                KeSignalEvent(Child->ReceiveReady, SignalOptionSignalAll);
            }

            KeReleaseQueuedLock(Child->ReceiveLock);
            KeReleaseQueuedLock(Child->TransmitLock);
            Ser16550pReleaseReference(Object);
            IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
Ser16550DispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSER16550_OBJECT Object;

    Object = DeviceContext;
    if (Object->Type == Ser16550ObjectChild) {
        Ser16550pAddReference(Object);
        Irp->U.Open.DeviceContext =
                              PARENT_STRUCTURE(Object, SER16550_CHILD, Header);

        IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
    }

    return;
}

VOID
Ser16550DispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSER16550_CHILD Child;

    Child = Irp->U.Close.DeviceContext;

    ASSERT(Child->Header.Type == Ser16550ObjectChild);

    Ser16550pReleaseReference(&(Child->Header));
    IoCompleteIrp(Ser16550Driver, Irp, STATUS_SUCCESS);
    return;
}

VOID
Ser16550DispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    UINTN BytesRemaining;
    PSER16550_CHILD Child;
    UINTN CopySize;
    UINTN CopyStart;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    PQUEUED_LOCK Lock;
    UINTN Next;
    RUNLEVEL OldRunLevel;
    PSER16550_PARENT Parent;
    UINTN ReceiveEnd;
    UINTN ReceiveStart;
    KSTATUS Status;
    UINTN TransmitEnd;
    UINTN TransmitStart;

    ASSERT(Irp->Direction == IrpDown);

    Child = Irp->U.ReadWrite.DeviceContext;
    IoBuffer = Irp->U.ReadWrite.IoBuffer;
    IoBufferOffset = 0;
    BytesRemaining = Irp->U.ReadWrite.IoSizeInBytes;
    Parent = Child->Parent;

    ASSERT(Child->Header.Type == Ser16550ObjectChild);

    if (Irp->MinorCode == IrpMinorIoRead) {
        Lock = Child->ReceiveLock;

    } else {

        ASSERT(Irp->MinorCode == IrpMinorIoWrite);

        Lock = Child->TransmitLock;
    }

    KeAcquireQueuedLock(Lock);
    if (Irp->MinorCode == IrpMinorIoWrite) {
        while (BytesRemaining != 0) {
            if (Parent->Removed != FALSE) {
                Status = STATUS_DEVICE_NOT_CONNECTED;
                goto DispatchIoEnd;
            }

            TransmitEnd = Child->TransmitEnd;
            TransmitStart = Child->TransmitStart;
            if (TransmitEnd >= TransmitStart) {
                CopySize = Child->TransmitSize - TransmitEnd;

            } else {
                CopySize = TransmitStart - TransmitEnd;
            }

            //
            // If the transmit buffer is full, then wait until a byte can be
            // added.
            //

            if (CopySize == 0) {

                //
                // If the transmitter isn't working at all, kick off the
                // process.
                //

                OldRunLevel = IoRaiseToInterruptRunLevel(
                                                  Parent->InterruptHandle);

                KeAcquireSpinLock(&(Parent->InterruptLock));
                Ser16550pStartTransmit(Child);
                KeReleaseSpinLock(&(Parent->InterruptLock));
                KeLowerRunLevel(OldRunLevel);
                KeSignalEvent(Child->TransmitReady, SignalOptionUnsignal);
                KeReleaseQueuedLock(Lock);
                KeWaitForEvent(Child->TransmitReady,
                               FALSE,
                               WAIT_TIME_INDEFINITE);

                KeAcquireQueuedLock(Lock);
                continue;
            }

            if (CopySize > BytesRemaining) {
                CopySize = BytesRemaining;
            }

            CopyStart = TransmitEnd;
            Next = CopyStart + CopySize;

            ASSERT(Next <= Child->TransmitSize);

            if (Next == Child->TransmitSize) {
                Next = 0;
            }

            if (Next == TransmitStart) {
                CopySize -= 1;
                if (Next == 0) {
                    Next = Child->TransmitSize - 1;

                } else {
                    Next -= 1;
                }
            }

            Status = MmCopyIoBufferData(IoBuffer,
                                        &(Child->TransmitBuffer[CopyStart]),
                                        IoBufferOffset,
                                        CopySize,
                                        FALSE);

            if (!KSUCCESS(Status)) {
                goto DispatchIoEnd;
            }

            IoBufferOffset += CopySize;
            BytesRemaining -= CopySize;
            Child->TransmitEnd = Next;
        }

        //
        // Kick off the transfer if needed.
        //

        OldRunLevel = IoRaiseToInterruptRunLevel(Parent->InterruptHandle);
        KeAcquireSpinLock(&(Parent->InterruptLock));
        Ser16550pStartTransmit(Child);
        KeReleaseSpinLock(&(Parent->InterruptLock));
        KeLowerRunLevel(OldRunLevel);

    } else {

        ASSERT(Irp->MinorCode == IrpMinorIoRead);

        while (BytesRemaining != 0) {
            if (Child->Parent->Removed != FALSE) {
                Status = STATUS_DEVICE_NOT_CONNECTED;
                goto DispatchIoEnd;
            }

            ReceiveEnd = Child->ReceiveEnd;
            ReceiveStart = Child->ReceiveStart;
            if (ReceiveEnd > ReceiveStart) {
                CopySize = ReceiveEnd - ReceiveStart;

            } else {
                CopySize = Child->ReceiveSize - ReceiveStart;
            }

            //
            // Handle an empty receive buffer.
            //

            if (CopySize == 0) {

                //
                // If some bytes were read in, then return them now.
                //

                if (BytesRemaining != Irp->U.ReadWrite.IoSizeInBytes) {
                    break;
                }

                //
                // Block waiting for more bytes to come in.
                //

                KeSignalEvent(Child->ReceiveReady, SignalOptionUnsignal);
                KeReleaseQueuedLock(Lock);
                KeWaitForEvent(Child->ReceiveReady,
                               FALSE,
                               WAIT_TIME_INDEFINITE);

                KeAcquireQueuedLock(Lock);
                continue;
            }

            CopyStart = ReceiveStart;
            if (CopySize > BytesRemaining) {
                CopySize = BytesRemaining;
            }

            ASSERT((CopySize < Child->ReceiveSize) &&
                   (CopyStart + CopySize <= Child->ReceiveSize));

            Status = MmCopyIoBufferData(IoBuffer,
                                        &(Child->ReceiveBuffer[CopyStart]),
                                        IoBufferOffset,
                                        CopySize,
                                        TRUE);

            if (!KSUCCESS(Status)) {
                goto DispatchIoEnd;
            }

            IoBufferOffset += CopySize;
            BytesRemaining -= CopySize;
            CopyStart += CopySize;
            if (CopyStart == Child->ReceiveSize) {
                CopyStart = 0;
            }

            Child->ReceiveStart = CopyStart;
        }
    }

    Status = STATUS_SUCCESS;

DispatchIoEnd:
    KeReleaseQueuedLock(Lock);
    Irp->U.ReadWrite.IoBytesCompleted = Irp->U.ReadWrite.IoSizeInBytes -
                                        BytesRemaining;

    IoCompleteIrp(Ser16550Driver, Irp, Status);
    return;
}

VOID
Ser16550DispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSER16550_CHILD Child;

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    Child = DeviceContext;
    switch (Child->Header.Type) {
    case Ser16550ObjectParent:
        break;

    case Ser16550ObjectChild:
        Ser16550pChildDispatchSystemControl(Irp, Child);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
Ser16550DispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles User Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSER16550_CHILD Child;

    ASSERT(Irp->MajorCode == IrpMajorUserControl);

    Child = DeviceContext;
    switch (Child->Header.Type) {
    case Ser16550ObjectParent:
        break;

    case Ser16550ObjectChild:
        Ser16550pChildDispatchUserControl(Irp, Child);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

INTERRUPT_STATUS
Ser16550InterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the EHCI interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the EHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    UCHAR Byte;
    PSER16550_CHILD Child;
    UINTN ChildIndex;
    ULONG Count;
    BOOL DidSomething;
    UCHAR InterruptRegister;
    INTERRUPT_STATUS InterruptStatus;
    UCHAR LineStatus;
    UINTN Next;
    PSER16550_PARENT Parent;

    InterruptStatus = InterruptStatusNotClaimed;
    Parent = Context;

    //
    // Loop over every serial port this interrupt services.
    //

    for (ChildIndex = 0; ChildIndex < Parent->ChildCount; ChildIndex += 1) {
        Child = &(Parent->ChildObjects[ChildIndex]);

        //
        // Skip invalid children.
        //

        if (Child->Header.Type != Ser16550ObjectChild) {
            continue;
        }

        //
        // Quickly exit if the UART is not interrupting.
        //

        InterruptRegister = SER16550_READ8(Child, Ser16550InterruptStatus);
        if ((InterruptRegister & SER16550_INTERRUPT_STATUS_NONE_PENDING) != 0) {
            continue;
        }

        InterruptStatus = InterruptStatusClaimed;
        do {
            DidSomething = FALSE;
            LineStatus = SER16550_READ8(Child, Ser16550LineStatus);
            if ((LineStatus & SER16550_LINE_STATUS_ERROR_MASK) != 0) {
                DidSomething = TRUE;

                //
                // TODO: Actually handle 16550 line status errors.
                //

                if ((LineStatus & SER16550_LINE_STATUS_OVERRUN_ERROR) != 0) {
                    RtlDebugPrint("16550: Overrun Error.\n");

                } else if ((LineStatus &
                            SER16550_LINE_STATUS_PARITY_ERROR) != 0) {

                    RtlDebugPrint("16550: Parity Error.\n");

                } else if ((LineStatus &
                            SER16550_LINE_STATUS_FRAMING_ERROR) != 0) {

                    RtlDebugPrint("16550: Framing Error.\n");

                } else if ((LineStatus &
                            SER16550_LINE_STATUS_FIFO_ERROR) != 0) {

                    RtlDebugPrint("16550: Fifo Error.\n");
                }
            }

            //
            // Transmit more stuff if possible.
            //

            if ((LineStatus & SER16550_LINE_STATUS_TX_HOLDING_EMPTY) != 0) {
                KeAcquireSpinLock(&(Child->Parent->InterruptLock));
                for (Count = 0; Count < Child->TransmitFifoSize; Count += 1) {
                    if (Child->TransmitStart == Child->TransmitEnd) {
                        Ser16550pStopTransmit(Child);
                        break;

                    } else {
                        Byte = Child->TransmitBuffer[Child->TransmitStart];
                        SER16550_WRITE8(Child, Ser16550Data, Byte);
                        Next = Child->TransmitStart + 1;
                        if (Next == Child->TransmitSize) {
                            Next = 0;
                        }

                        Child->TransmitStart = Next;
                        DidSomething = TRUE;
                    }
                }

                KeReleaseSpinLock(&(Child->Parent->InterruptLock));
                Child->InterruptWorkPending = TRUE;
            }

            //
            // Receive a byte data if possible.
            //

            if ((LineStatus & SER16550_LINE_STATUS_RX_MASK) != 0) {
                DidSomething = TRUE;

                //
                // TODO: Actually handle a 16550 break.
                //

                if ((LineStatus & SER16550_LINE_STATUS_BREAK) != 0) {
                    RtlDebugPrint("16550: Break\n");

                } else if ((LineStatus & SER16550_LINE_STATUS_RX_READY) != 0) {
                    Byte = SER16550_READ8(Child, Ser16550Data);
                    Next = Child->ReceiveEnd + 1;
                    if (Next == Child->ReceiveSize) {
                        Next = 0;
                    }

                    if (Next == Child->ReceiveStart) {
                        RtlDebugPrint("Uart RX Overflow\n");

                    } else {
                        Child->ReceiveBuffer[Child->ReceiveEnd] = Byte;
                        Child->ReceiveEnd = Next;
                    }
                }
            }

        } while (DidSomething != FALSE);
    }

    return InterruptStatus;
}

INTERRUPT_STATUS
Ser16550InterruptServiceWorker (
    PVOID Context
    )

/*++

Routine Description:

    This routine processes interrupts for the 16550 UART at low level.

Arguments:

    Context - Supplies a context pointer, which in this case points to the
        parent controller.

Return Value:

    Interrupt status.

--*/

{

    UINTN BytesCompleted;
    PSER16550_CHILD Child;
    UINTN ChildIndex;
    INTERRUPT_STATUS InterruptStatus;
    PIO_BUFFER IoBuffer;
    UINTN Next;
    PSER16550_PARENT Parent;
    UINTN ReceiveEnd;
    UINTN ReceiveStart;
    UINTN Size;
    KSTATUS Status;

    Parent = Context;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    InterruptStatus = InterruptStatusNotClaimed;

    //
    // Loop over every serial port this interrupt services.
    //

    for (ChildIndex = 0; ChildIndex < Parent->ChildCount; ChildIndex += 1) {
        Child = &(Parent->ChildObjects[ChildIndex]);
        if (Child->InterruptWorkPending == FALSE) {
            continue;
        }

        Child->InterruptWorkPending = FALSE;
        InterruptStatus = InterruptStatusClaimed;

        //
        // Signal the transmit ready event if some data was processed by now.
        //

        KeAcquireQueuedLock(Child->TransmitLock);
        Next = Child->TransmitEnd + 1;
        if (Next == Child->TransmitSize) {
            Next = 0;
        }

        if (Next != Child->TransmitStart) {
            KeSignalEvent(Child->TransmitReady, SignalOptionSignalAll);
        }

        KeReleaseQueuedLock(Child->TransmitLock);

        //
        // If there's a terminal, feed the terminal. Otherwise, maintain the
        // event.
        //

        KeAcquireQueuedLock(Child->ReceiveLock);
        ReceiveEnd = Child->ReceiveEnd;
        ReceiveStart = Child->ReceiveStart;
        while (ReceiveStart != ReceiveEnd) {
            if (ReceiveEnd >= ReceiveStart) {
                Size = ReceiveEnd - ReceiveStart;

            } else {
                Size = Child->ReceiveSize - ReceiveStart;
            }

            Status = MmCreateIoBuffer(Child->ReceiveBuffer + ReceiveStart,
                                      Size,
                                      IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                                      &IoBuffer);

            BytesCompleted = 0;
            if (KSUCCESS(Status)) {
                if (Child->Terminal != NULL) {
                    Status = IoWrite(Child->Terminal,
                                     IoBuffer,
                                     Size,
                                     0,
                                     0,
                                     &BytesCompleted);

                } else {
                    KeSignalEvent(Child->ReceiveReady, SignalOptionSignalAll);
                }

                MmFreeIoBuffer(IoBuffer);
                if (!KSUCCESS(Status)) {
                    RtlDebugPrint("Ser16550: Failed terminal write: %d\n",
                                  Status);
                }
            }

            ASSERT(ReceiveStart + BytesCompleted <= Child->ReceiveSize);

            ReceiveStart += BytesCompleted;
            if (ReceiveStart == Child->ReceiveSize) {
                ReceiveStart = 0;
            }

            Child->ReceiveStart = ReceiveStart;
            if (BytesCompleted == 0) {
                break;
            }

            ReceiveEnd = Child->ReceiveEnd;
            ReceiveStart = Child->ReceiveStart;
        }

        KeReleaseQueuedLock(Child->ReceiveLock);
    }

    return InterruptStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Ser16550pParentProcessResourceRequirements (
    PIRP Irp,
    PSER16550_PARENT Device
    )

/*++

Routine Description:

    This routine filters through the resource requirements presented by the
    bus for a 16550 parent device. It adds an interrupt vector requirement for
    any interrupt line requested.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this parent device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_CONFIGURATION_LIST Requirements;
    KSTATUS Status;
    RESOURCE_REQUIREMENT VectorRequirement;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Initialize a nice interrupt vector requirement in preparation.
    //

    RtlZeroMemory(&VectorRequirement, sizeof(RESOURCE_REQUIREMENT));
    VectorRequirement.Type = ResourceTypeInterruptVector;
    VectorRequirement.Minimum = 0;
    VectorRequirement.Maximum = -1;
    VectorRequirement.Length = 1;
    Requirements = Irp->U.QueryResources.ResourceRequirements;
    Status = IoCreateAndAddInterruptVectorsForLines(Requirements,
                                                    &VectorRequirement);

    if (!KSUCCESS(Status)) {
        goto ParentProcessResourceRequirementsEnd;
    }

ParentProcessResourceRequirementsEnd:
    return Status;
}

KSTATUS
Ser16550pParentStartDevice (
    PIRP Irp,
    PSER16550_PARENT Device
    )

/*++

Routine Description:

    This routine starts up the 16550 controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 16550 device.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    UINTN ChildCount;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PRESOURCE_ALLOCATION LineAllocation;
    KSTATUS Status;

    ChildCount = 0;

    //
    // Loop through the allocated resources to get the interrupt and count the
    // number of BARs.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // If the resource is an interrupt vector, then it should have an
        // owning interrupt line allocation.
        //

        if (Allocation->Type == ResourceTypeInterruptVector) {

            //
            // Currently only one interrupt resource is expected.
            //

            ASSERT(Device->InterruptResourcesFound == FALSE);
            ASSERT(Allocation->OwningAllocation != NULL);

            //
            // Save the line and vector number.
            //

            LineAllocation = Allocation->OwningAllocation;
            Device->InterruptLine = LineAllocation->Allocation;
            Device->InterruptVector = Allocation->Allocation;
            Device->InterruptResourcesFound = TRUE;

        } else if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            ChildCount += 1;

        } else if (Allocation->Type == ResourceTypeIoPort) {
            ChildCount += 1;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Some variants override the child count.
    //

    switch (Device->Variant) {
    case Ser16550VariantQuark:
        ChildCount = 1;
        break;

    case Ser16550VariantOxford:

        //
        // Oxford devices just have a single UART, except for a couple
        // (specified by the mode bits encoded in the device ID) that have two.
        //

        ASSERT(Device->VendorId == SER16550_VENDOR_OXFORD);

        ChildCount = 1;
        if (SER16550_OXFORD_DUAL_UARTS(Device->DeviceId) != FALSE) {
            ChildCount = 2;
        }

        break;

    default:
        break;
    }

    if (ChildCount == 0) {
        Status = STATUS_NOT_CONFIGURED;
        goto ParentStartDeviceEnd;
    }

    //
    // Allocate the child arrays.
    //

    if (ChildCount != Device->ChildCount) {

        ASSERT(Device->ChildCount == 0);

        Device->ChildDevices = MmAllocatePagedPool(ChildCount * sizeof(PDEVICE),
                                                   SER16550_ALLOCATION_TAG);

        if (Device->ChildDevices == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ParentStartDeviceEnd;
        }

        RtlZeroMemory(Device->ChildDevices, ChildCount * sizeof(PDEVICE));
        Device->ChildObjects = MmAllocatePagedPool(
                                           ChildCount * sizeof(SER16550_CHILD),
                                           SER16550_ALLOCATION_TAG);

        if (Device->ChildObjects == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto ParentStartDeviceEnd;
        }

        RtlZeroMemory(Device->ChildObjects,
                      ChildCount * sizeof(SER16550_CHILD));

        Device->ChildCount = ChildCount;
    }

    //
    // Initialize the child devices with their correct resources.
    //

    switch (Device->Variant) {
    case Ser16550VariantOxford:
        Status = Ser16550pInitializeChildrenOxford(Device, AllocationList);
        break;

    case Ser16550VariantQuark:
    case Ser16550VariantGeneric:
        Status = Ser16550pInitializeChildrenGeneric(Device, AllocationList);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_CONFIGURATION;
    }

    if (!KSUCCESS(Status)) {
        goto ParentStartDeviceEnd;
    }

    //
    // Attempt to connect the interrupt.
    //

    if ((Device->InterruptResourcesFound != FALSE) &&
        (Device->InterruptHandle == INVALID_HANDLE)) {

        RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
        Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
        Connect.Device = Irp->Device;
        Connect.LineNumber = Device->InterruptLine;
        Connect.Vector = Device->InterruptVector;
        Connect.InterruptServiceRoutine = Ser16550InterruptService;
        Connect.LowLevelServiceRoutine = Ser16550InterruptServiceWorker;
        Connect.Context = Device;
        Connect.Interrupt = &(Device->InterruptHandle);
        Status = IoConnectInterrupt(&Connect);
        if (!KSUCCESS(Status)) {
            goto ParentStartDeviceEnd;
        }
    }

    Status = STATUS_SUCCESS;

ParentStartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device->InterruptHandle != INVALID_HANDLE) {
            IoDisconnectInterrupt(Device->InterruptHandle);
            Device->InterruptHandle = INVALID_HANDLE;
        }
    }

    return Status;
}

KSTATUS
Ser16550pInitializeChildrenGeneric (
    PSER16550_PARENT Parent,
    PRESOURCE_ALLOCATION_LIST AllocationList
    )

/*++

Routine Description:

    This routine initializes the child device structures for a standard 16550
    UART device. In a standard device, each BAR is assumed to correspond to
    a UART, up to the number of children.

Arguments:

    Parent - Supplies a pointer to the parent.

    AllocationList - Supplies a pointer to the resource allocation list.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    UINTN ChildIndex;
    PSER16550_CHILD ChildObject;
    UINTN ReleaseCount;
    KSTATUS Status;

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    ChildIndex = 0;
    while (ChildIndex < Parent->ChildCount) {

        ASSERT(Allocation != NULL);

        //
        // For each BAR found, initialize a child device.
        //

        if ((Allocation->Type == ResourceTypePhysicalAddressSpace) ||
            (Allocation->Type == ResourceTypeIoPort)) {

            ChildObject = &(Parent->ChildObjects[ChildIndex]);
            Status = Ser16550pInitializeChild(Parent, ChildIndex);
            if (!KSUCCESS(Status)) {
                goto InitializeChildrenGenericEnd;
            }

            if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
                ChildObject->MappedSize = Allocation->Length;
                ChildObject->PhysicalAddress = Allocation->Allocation;
                ChildObject->MappedAddress = MmMapPhysicalAddress(
                                                      Allocation->Allocation,
                                                      Allocation->Length,
                                                      TRUE,
                                                      FALSE,
                                                      TRUE);

                if (ChildObject->MappedAddress == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto InitializeChildrenGenericEnd;
                }

                ChildObject->Read8 = Ser16550pReadMemory8;
                ChildObject->Write8 = Ser16550pWriteMemory8;
                ChildObject->ShouldUnmap = TRUE;

            } else {
                ChildObject->IoPortAddress = (USHORT)(Allocation->Allocation);
                ChildObject->Read8 = Ser16550pReadIo8;
                ChildObject->Write8 = Ser16550pWriteIo8;
            }

            ChildIndex += 1;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    Status = STATUS_SUCCESS;

InitializeChildrenGenericEnd:
    if (!KSUCCESS(Status)) {

        ASSERT(ChildIndex != Parent->ChildCount);

        ReleaseCount = ChildIndex;
        for (ChildIndex = 0; ChildIndex <= ReleaseCount; ChildIndex += 1) {

            ASSERT(Parent->ChildObjects[ChildIndex].Header.ReferenceCount == 1);

            Ser16550pReleaseReference(
                                   &(Parent->ChildObjects[ChildIndex].Header));
        }
    }

    return Status;
}

KSTATUS
Ser16550pInitializeChildrenOxford (
    PSER16550_PARENT Parent,
    PRESOURCE_ALLOCATION_LIST AllocationList
    )

/*++

Routine Description:

    This routine initializes the child device structures for an Oxford UART.

Arguments:

    Parent - Supplies a pointer to the parent.

    AllocationList - Supplies a pointer to the resource allocation list.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    UINTN ChildIndex;
    PSER16550_CHILD ChildObject;
    UINTN ReleaseCount;
    KSTATUS Status;

    //
    // Find the first BAR, which is where the single or dual UART resides.
    //

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {
        if ((Allocation->Type == ResourceTypePhysicalAddressSpace) ||
            (Allocation->Type == ResourceTypeIoPort)) {

            break;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    if (Allocation == NULL) {
        return STATUS_NOT_CONFIGURED;
    }

    //
    // If the BAR is an I/O port, then this is a legacy compatible controller.
    //

    if (Allocation->Type == ResourceTypeIoPort) {
        ChildIndex = 0;
        ChildObject = &(Parent->ChildObjects[0]);
        Status = Ser16550pInitializeChild(Parent, 0);
        if (!KSUCCESS(Status)) {
            goto InitializeChildrenGenericEnd;
        }

        ChildObject->IoPortAddress = (USHORT)(Allocation->Allocation);
        ChildObject->Read8 = Ser16550pReadIo8;
        ChildObject->Write8 = Ser16550pWriteIo8;

    //
    // This is a native UART.
    //

    } else {
        Parent->BaseBaud = SER16550_OXFORD_BASE_BAUD;
        for (ChildIndex = 0; ChildIndex < Parent->ChildCount; ChildIndex += 1) {
            ChildObject = &(Parent->ChildObjects[ChildIndex]);
            Status = Ser16550pInitializeChild(Parent, ChildIndex);
            if (!KSUCCESS(Status)) {
                goto InitializeChildrenGenericEnd;
            }

            ChildObject->MappedSize = Allocation->Length;
            ChildObject->PhysicalAddress = Allocation->Allocation;
            if (ChildIndex == 0) {
                ChildObject->MappedAddress = MmMapPhysicalAddress(
                                                      Allocation->Allocation,
                                                      Allocation->Length,
                                                      TRUE,
                                                      FALSE,
                                                      TRUE);

                if (ChildObject->MappedAddress == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto InitializeChildrenGenericEnd;
                }

                ChildObject->ShouldUnmap = TRUE;

            } else {
                ChildObject->MappedAddress =
                                         Parent->ChildObjects[0].MappedAddress;
            }

            ChildObject->RegisterOffset = SER16550_OXFORD_UART_OFFSET +
                                          (ChildIndex *
                                           SER16550_OXFORD_UART_STRIDE);

            ChildObject->Read8 = Ser16550pReadMemory8;
            ChildObject->Write8 = Ser16550pWriteMemory8;
        }
    }

    Status = STATUS_SUCCESS;

InitializeChildrenGenericEnd:
    if (!KSUCCESS(Status)) {

        ASSERT(ChildIndex != Parent->ChildCount);

        ReleaseCount = ChildIndex;
        for (ChildIndex = 0; ChildIndex <= ReleaseCount; ChildIndex += 1) {

            ASSERT(Parent->ChildObjects[ChildIndex].Header.ReferenceCount == 1);

            Ser16550pReleaseReference(
                                   &(Parent->ChildObjects[ChildIndex].Header));
        }
    }

    return Status;
}

KSTATUS
Ser16550pInitializeChild (
    PSER16550_PARENT Parent,
    UINTN Index
    )

/*++

Routine Description:

    This routine creates the resources associated with a 16550 UART child.

Arguments:

    Parent - Supplies a pointer to the parent.

    Index - Supplies the child index within the parent.

Return Value:

    Status code.

--*/

{

    PSER16550_CHILD ChildObject;
    KSTATUS Status;

    ChildObject = &(Parent->ChildObjects[Index]);
    ChildObject->Header.Type = Ser16550ObjectChild;
    ChildObject->Header.ReferenceCount = 1;
    ChildObject->Parent = Parent;
    ChildObject->Index = Index;
    ChildObject->RegisterOffset = 0;
    ChildObject->RegisterShift = Parent->RegisterShift;
    if (ChildObject->TransmitBuffer == NULL) {
        ChildObject->TransmitBuffer = MmAllocateNonPagedPool(
                                          SER16550_DEFAULT_BUFFER_SIZE,
                                          SER16550_ALLOCATION_TAG);

        if (ChildObject->TransmitBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }

        ChildObject->TransmitSize = SER16550_DEFAULT_BUFFER_SIZE;
        ChildObject->TransmitStart = 0;
        ChildObject->TransmitEnd = 0;
    }

    if (ChildObject->ReceiveBuffer == NULL) {
        ChildObject->ReceiveBuffer = MmAllocateNonPagedPool(
                                          SER16550_DEFAULT_BUFFER_SIZE,
                                          SER16550_ALLOCATION_TAG);

        if (ChildObject->ReceiveBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }

        ChildObject->ReceiveSize = SER16550_DEFAULT_BUFFER_SIZE;
        ChildObject->ReceiveStart = 0;
        ChildObject->ReceiveEnd = 0;
    }

    if (ChildObject->TransmitLock == NULL) {
        ChildObject->TransmitLock = KeCreateQueuedLock();
        if (ChildObject->TransmitLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }
    }

    if (ChildObject->ReceiveLock == NULL) {
        ChildObject->ReceiveLock = KeCreateQueuedLock();
        if (ChildObject->ReceiveLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }
    }

    if (ChildObject->TransmitReady == NULL) {
        ChildObject->TransmitReady = KeCreateEvent(NULL);
        if (ChildObject->TransmitReady == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }

        KeSignalEvent(ChildObject->TransmitReady,
                      SignalOptionSignalAll);
    }

    if (ChildObject->ReceiveReady == NULL) {
        ChildObject->ReceiveReady = KeCreateEvent(NULL);
        if (ChildObject->ReceiveReady == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeChildEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializeChildEnd:
    return Status;
}

VOID
Ser16550pParentEnumerateChildren (
    PIRP Irp,
    PSER16550_PARENT Device
    )

/*++

Routine Description:

    This routine enumerates all ports in the serial controller.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 16550 device.

Return Value:

    None.

--*/

{

    UINTN ChildIndex;
    CHAR DeviceId[SERIAL_PORT_DEVICE_ID_SIZE];
    KSTATUS Status;

    //
    // Create child devices for each child.
    //

    for (ChildIndex = 0; ChildIndex < Device->ChildCount; ChildIndex += 1) {

        ASSERT(Device->ChildObjects[ChildIndex].Header.Type ==
               Ser16550ObjectChild);

        if (Device->ChildDevices[ChildIndex] == NULL) {
            RtlPrintToString(DeviceId,
                             SERIAL_PORT_DEVICE_ID_SIZE,
                             CharacterEncodingDefault,
                             SERIAL_PORT_DEVICE_ID_FORMAT,
                             ChildIndex);

            Status = IoCreateDevice(Ser16550Driver,
                                    &(Device->ChildObjects[ChildIndex]),
                                    Device->Device,
                                    DeviceId,
                                    CHARACTER_CLASS_ID,
                                    NULL,
                                    &(Device->ChildDevices[ChildIndex]));

            if (!KSUCCESS(Status)) {
                goto ParentEnumerateChildrenEnd;
            }
        }
    }

    Status = IoMergeChildArrays(Irp,
                                Device->ChildDevices,
                                Device->ChildCount,
                                SER16550_ALLOCATION_TAG);

    if (!KSUCCESS(Status)) {
        goto ParentEnumerateChildrenEnd;
    }

ParentEnumerateChildrenEnd:
    IoCompleteIrp(Ser16550Driver, Irp, Status);
    return;
}

VOID
Ser16550pChildStartDevice (
    PIRP Irp,
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine starts an individual 16550serial port.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 16550 device.

Return Value:

    None.

--*/

{

    PIO_HANDLE DeviceHandle;
    PDEBUG_HANDOFF_DATA HandoffData;
    KSTATUS Status;

    DeviceHandle = NULL;

    //
    // Determine if this UART is being used by the kernel debug transport, and
    // fail to start up if it is (as the kernel debugger owns it).
    //

    Status = KdGetDeviceInformation(&HandoffData);
    if ((KSUCCESS(Status)) &&
        (HandoffData != NULL) &&
        (HandoffData->PortType == DEBUG_PORT_TYPE_SERIAL) &&
        ((HandoffData->PortSubType == DEBUG_PORT_SERIAL_16550) ||
         (HandoffData->PortSubType == DEBUG_PORT_SERIAL_16550_COMPATIBLE))) {

        if (HandoffData->Identifier == Device->PhysicalAddress) {
            Status = STATUS_RESOURCE_IN_USE;
            goto ChildStartDeviceEnd;
        }
    }

    //
    // Create the terminal object.
    //

    if (Device->Terminal == NULL) {
        Status = IoCreateTerminal(TRUE,
                                  NULL,
                                  NULL,
                                  NULL,
                                  0,
                                  NULL,
                                  0,
                                  IO_ACCESS_READ | IO_ACCESS_WRITE,
                                  OPEN_FLAG_NO_CONTROLLING_TERMINAL,
                                  TERMINAL_DEFAULT_PERMISSIONS,
                                  TERMINAL_DEFAULT_PERMISSIONS,
                                  &(Device->Terminal));

        if (!KSUCCESS(Status)) {
            goto ChildStartDeviceEnd;
        }

        //
        // Open a handle to this very device.
        //

        Status = IoOpenDevice(Irp->Device,
                              IO_ACCESS_READ | IO_ACCESS_WRITE,
                              0,
                              &DeviceHandle,
                              NULL,
                              NULL,
                              NULL);

        if (!KSUCCESS(Status)) {
            goto ChildStartDeviceEnd;
        }

        //
        // Associate the hardware device with the terminal. The terminal now
        // owns the handle.
        //

        Status = IoTerminalSetDevice(Device->Terminal, DeviceHandle);
        if (!KSUCCESS(Status)) {
            goto ChildStartDeviceEnd;
        }

        DeviceHandle = NULL;
    }

    Status = STATUS_SUCCESS;

ChildStartDeviceEnd:
    if (DeviceHandle != NULL) {
        IoClose(DeviceHandle);
    }

    if (!KSUCCESS(Status)) {
        if (Device->Terminal != NULL) {
            IoClose(Device->Terminal);
            Device->Terminal = NULL;
        }
    }

    IoCompleteIrp(Ser16550Driver, Irp, Status);
    return;
}

VOID
Ser16550pChildDispatchSystemControl (
    PIRP Irp,
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine handles system control IRPs for the 16550 child device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 16550 device.

Return Value:

    None. The IRP will be completed appropriately.

--*/

{

    PVOID Context;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a character device.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectCharacterDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = 0;
            Properties->BlockCount = 0;
            Properties->Size = 0;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(Ser16550Driver, Irp, Status);
        break;

    //
    // Fail if the properties being written are different.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        Properties = FileOperation->FileProperties;
        PropertiesFileSize = Properties->Size;
        if ((Properties->FileId != 0) ||
            (Properties->Type != IoObjectCharacterDevice) ||
            (Properties->HardLinkCount != 1) ||
            (Properties->BlockSize != 0) ||
            (Properties->BlockCount != 0) ||
            (PropertiesFileSize != 0)) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(Ser16550Driver, Irp, Status);
        break;

    //
    // Do not support hard disk device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(Ser16550Driver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    //
    // Send all pending output data.
    // TODO: Wait for all pending output data to be complete.
    //

    case IrpMinorSystemControlSynchronize:
        Status = STATUS_SUCCESS;
        IoCompleteIrp(Ser16550Driver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:
        break;
    }

    return;
}

VOID
Ser16550pChildDispatchUserControl (
    PIRP Irp,
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine handles user control IRPs for the 16550 child device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this 16550 device.

Return Value:

    None. The IRP will be completed appropriately.

--*/

{

    TERMINAL_USER_CONTROL_CODE ControlCode;
    KSTATUS Status;
    TERMINAL_SETTINGS TerminalSettings;
    TERMINAL_SETTINGS_OLD TerminalSettingsOld;

    Status = STATUS_NOT_HANDLED;
    ControlCode = Irp->MinorCode;
    switch (ControlCode) {
    case TerminalControlSetAttributesFlush:

        //
        // Flush the input.
        //

        Device->ReceiveStart = Device->ReceiveEnd;

        //
        // Fall through.
        //

    case TerminalControlSetAttributesDrain:

        //
        // TODO: Flush the output.
        //

        //
        // Fall through.
        //

    case TerminalControlSetAttributes:
        if (Irp->U.UserControl.FromKernelMode != FALSE) {
            RtlCopyMemory(&TerminalSettings,
                          Irp->U.UserControl.UserBuffer,
                          sizeof(TERMINAL_SETTINGS));

            Status = STATUS_SUCCESS;

        } else {
            Status = MmCopyFromUserMode(&TerminalSettings,
                                        Irp->U.UserControl.UserBuffer,
                                        sizeof(TERMINAL_SETTINGS));

            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if (TerminalSettings.InputSpeed != TerminalSettings.OutputSpeed) {
            Status = STATUS_NOT_SUPPORTED;
            break;
        }

        if ((TerminalSettings.ControlFlags != Device->ControlFlags) ||
            (TerminalSettings.OutputSpeed != Device->BaudRate)) {

            Status = Ser16550pConfigureDevice(Device,
                                              TerminalSettings.ControlFlags,
                                              TerminalSettings.OutputSpeed);

            if (!KSUCCESS(Status)) {
                break;
            }

            Device->ControlFlags = TerminalSettings.ControlFlags;
            Device->BaudRate = TerminalSettings.OutputSpeed;
        }

        break;

    case TerminalControlSetAttributesFlushOld:

        //
        // Flush the input.
        //

        Device->ReceiveStart = Device->ReceiveEnd;

        //
        // Fall through.
        //

    case TerminalControlSetAttributesDrainOld:

        //
        // TODO: Flush the output.
        //

        //
        // Fall through.
        //

    case TerminalControlSetAttributesOld:
        if (Irp->U.UserControl.FromKernelMode != FALSE) {
            RtlCopyMemory(&TerminalSettingsOld,
                          Irp->U.UserControl.UserBuffer,
                          sizeof(TERMINAL_SETTINGS_OLD));

            Status = STATUS_SUCCESS;

        } else {
            Status = MmCopyFromUserMode(&TerminalSettingsOld,
                                        Irp->U.UserControl.UserBuffer,
                                        sizeof(TERMINAL_SETTINGS_OLD));

            if (!KSUCCESS(Status)) {
                break;
            }
        }

        if (TerminalSettingsOld.ControlFlags != Device->ControlFlags) {
            Status = Ser16550pConfigureDevice(Device,
                                              TerminalSettingsOld.ControlFlags,
                                              Device->BaudRate);

            if (!KSUCCESS(Status)) {
                break;
            }

            Device->ControlFlags = TerminalSettingsOld.ControlFlags;
        }

        break;

    case TerminalControlSendBreak:

        //
        // TODO: Send a serial break.
        //

        break;

    case TerminalControlFlowControl:

        //
        // TODO: Handle serial flow control.
        //

        break;

    case TerminalControlFlush:

        //
        // TODO: Handle serial flush.
        //

        break;

    case TerminalControlGetModemStatus:

        //
        // TODO: Get serial modem status.
        //

        break;

    case TerminalControlOrModemStatus:

        //
        // TODO: Get serial control/modem status.
        //

        break;

    case TerminalControlClearModemStatus:

        //
        // TODO: Clear serial modem status.
        //

        break;

    case TerminalControlSetModemStatus:

        //
        // TODO: Set serial modem status.
        //

        break;

    case TerminalControlGetSoftCarrier:
    case TerminalControlSetSoftCarrier:

        //
        // TODO: Get/set serial soft carrier status.
        //

        break;

    case TerminalControlSendBreakPosix:
    case TerminalControlStartBreak:
    case TerminalControlStopBreak:

        //
        // TODO: Send a serial break.
        //

        Status = STATUS_SUCCESS;
        break;

    case TerminalControlGetAttributes:
    case TerminalControlGetAttributesOld:
    case TerminalControlSetExclusive:
    case TerminalControlClearExclusive:
    case TerminalControlGetOutputQueueSize:
    case TerminalControlGetInputQueueSize:
    case TerminalControlInsertInInputQueue:
    case TerminalControlGetWindowSize:
    case TerminalControlSetWindowSize:
    case TerminalControlRedirectLocalConsole:
    case TerminalControlSetPacketMode:
    case TerminalControlGiveUpControllingTerminal:
    case TerminalControlSetControllingTerminal:
    case TerminalControlGetProcessGroup:
    case TerminalControlSetProcessGroup:
    case TerminalControlGetCurrentSessionId:
    default:
        break;
    }

    if (Status != STATUS_NOT_HANDLED) {
        IoCompleteIrp(Ser16550Driver, Irp, Status);
    }

    return;
}

VOID
Ser16550pStartTransmit (
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine starts transmission on the 16550 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    if ((Device->InterruptEnable & SER16550_INTERRUPT_ENABLE_TX_EMPTY) == 0) {
        Device->InterruptEnable |= SER16550_INTERRUPT_ENABLE_TX_EMPTY;
        SER16550_WRITE8(Device,
                        Ser16550InterruptEnable,
                        Device->InterruptEnable);
    }

    return;
}

VOID
Ser16550pStopTransmit (
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine stops transmission on the 16550 device.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    None.

--*/

{

    if ((Device->InterruptEnable & SER16550_INTERRUPT_ENABLE_TX_EMPTY) != 0) {
        Device->InterruptEnable &= ~SER16550_INTERRUPT_ENABLE_TX_EMPTY;
        SER16550_WRITE8(Device,
                        Ser16550InterruptEnable,
                        Device->InterruptEnable);
    }

    return;
}

KSTATUS
Ser16550pConfigureDevice (
    PSER16550_CHILD Device,
    ULONG TerminalControlFlags,
    ULONG BaudRate
    )

/*++

Routine Description:

    This routine configures the serial device, including baud rate, data bits,
    stop bits, and parity.

Arguments:

    Device - Supplies a pointer to the device.

    TerminalControlFlags - Supplies the bitfield of terminal control flags
        governing parity, data width, and stop bits. See TERMINAL_CONTROL_*
        definitions.

    BaudRate - Supplies the requested baud rate.

Return Value:

    Status code.

--*/

{

    ULONG CurrentBaud;
    ULONG Divisor;
    UCHAR Value;

    //
    // Compute the appropriate divisor.
    //

    Divisor = 1;
    if (BaudRate > Device->Parent->BaseBaud) {
        return STATUS_NOT_SUPPORTED;
    }

    Divisor = 1;
    while (TRUE) {
        CurrentBaud = Device->Parent->BaseBaud / Divisor;
        if ((CurrentBaud <= BaudRate) || (CurrentBaud == 0)) {
            break;
        }

        Divisor += 1;
    }

    if ((CurrentBaud == 0) || (Divisor > MAX_USHORT)) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Disable all interrupts.
    //

    Device->InterruptEnable = 0;
    SER16550_WRITE8(Device, Ser16550InterruptEnable, Device->InterruptEnable);

    //
    // Set the divisor latch enable bit to get at the divisor registers.
    //

    Value = SER16550_LINE_CONTROL_DIVISOR_LATCH;
    SER16550_WRITE8(Device, Ser16550LineControl, Value);

    //
    // Write the computed divisor value.
    //

    SER16550_WRITE8(Device, Ser16550DivisorLow, (UCHAR)(Divisor & 0x00FF));
    SER16550_WRITE8(Device,
                    Ser16550DivisorHigh,
                    (UCHAR)((Divisor >> 8) & 0x00FF));

    //
    // Enable the FIFOs.
    //

    SER16550_WRITE8(Device, Ser16550LineControl, 0);
    Value = SER16550_FIFO_CONTROL_ENABLE;
    SER16550_WRITE8(Device, Ser16550FifoControl, Value);

    //
    // Figure out the appropriate line control register value.
    //

    Value = 0;
    switch (TerminalControlFlags & TERMINAL_CONTROL_CHARACTER_SIZE_MASK) {
    case TERMINAL_CONTROL_5_BITS_PER_CHARACTER:
        Value |= SER16550_LINE_CONTROL_5_DATA_BITS;
        break;

    case TERMINAL_CONTROL_6_BITS_PER_CHARACTER:
        Value |= SER16550_LINE_CONTROL_6_DATA_BITS;
        break;

    case TERMINAL_CONTROL_7_BITS_PER_CHARACTER:
        Value |= SER16550_LINE_CONTROL_7_DATA_BITS;
        break;

    case TERMINAL_CONTROL_8_BITS_PER_CHARACTER:
        Value |= SER16550_LINE_CONTROL_8_DATA_BITS;
        break;

    default:
        return STATUS_NOT_SUPPORTED;
    }

    if ((TerminalControlFlags & TERMINAL_CONTROL_2_STOP_BITS) != 0) {
        Value |= SER16550_LINE_CONTROL_2_STOP_BITS;
    }

    if ((TerminalControlFlags & TERMINAL_CONTROL_ENABLE_PARITY) != 0) {
        Value |= SER16550_LINE_CONTROL_PARITY_ENABLE |
                 SER16550_LINE_CONTROL_SET_PARITY;

        if ((TerminalControlFlags & TERMINAL_CONTROL_ODD_PARITY) == 0) {
            Value |= SER16550_LINE_CONTROL_EVEN_PARITY;
        }
    }

    //
    // Write the line control, which also flips the divisor registers back to
    // their normal registers.
    //

    SER16550_WRITE8(Device, Ser16550LineControl, Value);

    //
    // Initialize the modem control register, which includes flow control
    // (currently disabled).
    //

    Value = 0;
    SER16550_WRITE8(Device, Ser16550ModemControl, Value);

    //
    // Initialize the FIFO size.
    //

    Device->TransmitFifoSize = Ser16550pGetFifoSize(Device);

    //
    // Enable interrupts.
    //

    Device->InterruptEnable = SER16550_INTERRUPT_ENABLE_RX_DATA |
                              SER16550_INTERRUPT_ENABLE_RX_STATUS;

    SER16550_WRITE8(Device, Ser16550InterruptEnable, Device->InterruptEnable);
    return STATUS_SUCCESS;
}

ULONG
Ser16550pGetFifoSize (
    PSER16550_CHILD Device
    )

/*++

Routine Description:

    This routine determines the size of the serial port FIFO.

Arguments:

    Device - Supplies a pointer to the device.

Return Value:

    Returns the FIFO size.

--*/

{

    UCHAR DivisorHigh;
    UCHAR DivisorLow;
    ULONG Index;
    UCHAR LineControl;
    UCHAR ModemControl;
    UCHAR Value;

    LineControl = SER16550_READ8(Device, Ser16550LineControl);
    SER16550_WRITE8(Device, Ser16550LineControl, 0);
    ModemControl = SER16550_READ8(Device, Ser16550ModemControl);
    Value = SER16550_FIFO_CONTROL_ENABLE |
            SER16550_FIFO_CONTROL_CLEAR_TRANSMIT |
            SER16550_FIFO_CONTROL_CLEAR_RECEIVE;

    SER16550_WRITE8(Device, Ser16550FifoControl, Value);
    SER16550_WRITE8(Device,
                    Ser16550ModemControl,
                    SER16550_MODEM_CONTROL_LOOPBACK);

    Value = SER16550_LINE_CONTROL_DIVISOR_LATCH;
    SER16550_WRITE8(Device, Ser16550LineControl, Value);
    DivisorLow = SER16550_READ8(Device, Ser16550DivisorLow);
    DivisorHigh = SER16550_READ8(Device, Ser16550DivisorHigh);
    SER16550_WRITE8(Device, Ser16550DivisorLow, 1);
    SER16550_WRITE8(Device, Ser16550DivisorHigh, 0);
    Value = SER16550_LINE_CONTROL_8_DATA_BITS;
    SER16550_WRITE8(Device, Ser16550LineControl, Value);
    for (Index = 0; Index < SER16550_MAX_FIFO; Index += 1) {
        SER16550_WRITE8(Device, Ser16550Data, Index);
    }

    KeDelayExecution(FALSE, FALSE, 10 * MICROSECONDS_PER_MILLISECOND);
    for (Index = 0; Index < SER16550_MAX_FIFO; Index += 1) {
        if ((SER16550_READ8(Device, Ser16550LineStatus) &
             SER16550_LINE_STATUS_RX_READY) == 0) {

            break;
        }

        SER16550_READ8(Device, Ser16550Data);
    }

    SER16550_WRITE8(Device, Ser16550ModemControl, ModemControl);
    Value = SER16550_LINE_CONTROL_DIVISOR_LATCH;
    SER16550_WRITE8(Device, Ser16550LineControl, Value);
    SER16550_WRITE8(Device, Ser16550DivisorLow, DivisorLow);
    SER16550_WRITE8(Device, Ser16550DivisorHigh, DivisorHigh);
    SER16550_WRITE8(Device, Ser16550LineControl, LineControl);
    return Index;
}

VOID
Ser16550pAddReference (
    PSER16550_OBJECT Object
    )

/*++

Routine Description:

    This routine adds a reference on the given 16550 context.

Arguments:

    Object - Supplies a pointer to the 16550 object.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Object->ReferenceCount), 1);

    ASSERT(OldReferenceCount < 0x10000000);

    return;
}

VOID
Ser16550pReleaseReference (
    PSER16550_OBJECT Object
    )

/*++

Routine Description:

    This routine releases a reference on a 16550 object.

Arguments:

    Object - Supplies a pointer to the 16550 object.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Object->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        Ser16550pDestroyDevice(Object);
    }

    return;
}

VOID
Ser16550pDestroyDevice (
    PSER16550_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys a 16550 object.

Arguments:

    Object - Supplies a pointer to the 16550 object.

Return Value:

    None.

--*/

{

    PSER16550_CHILD Child;
    PSER16550_PARENT Parent;

    switch (Object->Type) {
    case Ser16550ObjectParent:
        Parent = PARENT_STRUCTURE(Object, SER16550_PARENT, Header);
        if (Parent->ChildDevices != NULL) {
            MmFreePagedPool(Parent->ChildDevices);
            Parent->ChildDevices = NULL;
        }

        if (Parent->ChildObjects != NULL) {
            MmFreePagedPool(Parent->ChildObjects);
            Parent->ChildObjects = NULL;
        }

        Parent->Header.Type = Ser16550ObjectInvalid;
        MmFreeNonPagedPool(Parent);
        break;

    case Ser16550ObjectChild:
        Child = PARENT_STRUCTURE(Object, SER16550_CHILD, Header);
        Child->Header.Type = Ser16550ObjectInvalid;
        if (Child->Terminal != NULL) {
            IoTerminalSetDevice(Child->Terminal, NULL);
            IoClose(Child->Terminal);
            Child->Terminal = NULL;
        }

        if ((Child->MappedAddress != NULL) && (Child->ShouldUnmap != FALSE)) {

            ASSERT(Child->MappedSize != 0);

            MmUnmapAddress(Child->MappedAddress, Child->MappedSize);
            Child->MappedAddress = NULL;
            Child->MappedSize = 0;
        }

        if (Child->TransmitBuffer != NULL) {
            MmFreeNonPagedPool(Child->TransmitBuffer);
            Child->TransmitBuffer = NULL;
            Child->TransmitSize = 0;
            Child->TransmitStart = 0;
            Child->TransmitEnd = 0;
        }

        if (Child->ReceiveBuffer != NULL) {
            MmFreeNonPagedPool(Child->ReceiveBuffer);
            Child->ReceiveBuffer = NULL;
            Child->ReceiveSize = 0;
            Child->ReceiveStart = 0;
            Child->ReceiveEnd = 0;
        }

        if (Child->TransmitLock != NULL) {
            KeDestroyQueuedLock(Child->TransmitLock);
            Child->TransmitLock = NULL;
        }

        if (Child->ReceiveLock != NULL) {
            KeDestroyQueuedLock(Child->ReceiveLock);
            Child->ReceiveLock = NULL;
        }

        if (Child->TransmitReady == NULL) {
            KeDestroyEvent(Child->TransmitReady);
            Child->TransmitReady = NULL;
        }

        if (Child->ReceiveReady == NULL) {
            KeDestroyEvent(Child->ReceiveReady);
            Child->ReceiveReady = NULL;
        }

        break;

    default:

        ASSERT(FALSE);

        return;
    }

    return;
}

UCHAR
Ser16550pReadIo8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads an I/O port based 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    USHORT Port;

    Port = Device->IoPortAddress + SER16550_REGISTER_OFFSET(Device, Register);
    return HlIoPortInByte(Port);
}

VOID
Ser16550pWriteIo8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register,
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

    Port = Device->IoPortAddress + SER16550_REGISTER_OFFSET(Device, Register);
    HlIoPortOutByte(Port, Value);
    return;
}

UCHAR
Ser16550pReadMemory8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register
    )

/*++

Routine Description:

    This routine reads a memory-based 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to read.

Return Value:

    Returns the value at the register.

--*/

{

    PVOID Address;

    Address = Device->MappedAddress +
              SER16550_REGISTER_OFFSET(Device, Register);

    return HlReadRegister8(Address);
}

VOID
Ser16550pWriteMemory8 (
    PSER16550_CHILD Device,
    SER16550_REGISTER Register,
    UCHAR Value
    )

/*++

Routine Description:

    This routine writes to a memory-based 16550 register.

Arguments:

    Device - Supplies a pointer to the device context.

    Register - Supplies the register to write to.

    Value - Supplies the value to write.

Return Value:

    None.

--*/

{

    PVOID Address;

    Address = Device->MappedAddress +
              SER16550_REGISTER_OFFSET(Device, Register);

    HlWriteRegister8(Address, Value);
    return;
}

