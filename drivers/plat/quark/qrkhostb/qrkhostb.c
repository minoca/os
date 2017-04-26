/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    qrkhostb.c

Abstract:

    This module implements the Intel Quark Host Bridge driver.

Author:

    Evan Green 4-Dec-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/pci.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro encodes the bits of the command register.
//

#define QUARK_SIDEBAND_MESSAGE(_Id, _Command, _Register) \
    (((_Command) << QUARK_SIDEBAND_MCR_SHIFT) | \
     (((_Id) << QUARK_SIDEBAND_PORT_SHIFT) & QUARK_SIDEBAND_PORT_MASK) | \
     (((_Register) << QUARK_SIDEBAND_REGISTER_SHIFT) & \
      QUARK_SIDEBAND_REGISTER_MASK) | \
     QUARK_SIDEBAND_BYTE_ENABLE)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the Quark Host Bridge allocation tag: QrkH.
//

#define QUARK_HOST_BRIDGE_ALLOCATION_TAG 0x486B7251

//
// Define the offsets into PCI configuration space where the command and data
// registers live.
//

#define QUARK_SIDEBAND_COMMAND_OFFSET 0xD0
#define QUARK_SIDEBAND_DATA_OFFSET 0xD4

//
// Define the message data bus IDs.
//

#define QUARK_SIDEBAND_ID_IMR 0x05

//
// Define the sideband command opcodes.
//

#define QUARK_SIDEBAND_OPCODE_READ 0x10
#define QUARK_SIDEBAND_OPCODE_WRITE 0x11

//
// Define the sideband command bit definitions.
//

#define QUARK_SIDEBAND_MCR_SHIFT 24
#define QUARK_SIDEBAND_PORT_SHIFT 16
#define QUARK_SIDEBAND_REGISTER_SHIFT 8
#define QUARK_SIDEBAND_PORT_MASK 0x00FF0000
#define QUARK_SIDEBAND_REGISTER_MASK 0x0000FF00
#define QUARK_SIDEBAND_BYTE_ENABLE 0x000000F0

//
// Define IMR registers.
//

#define QUARK_IMR_IMR0L  0x40
#define QUARK_IMR_IMR0H  0x41
#define QUARK_IMR_IMR0RM 0x42
#define QUARK_IMR_IMR0WM 0x43
#define QUARK_IMR_IMR1L  0x44
#define QUARK_IMR_IMR1H  0x45
#define QUARK_IMR_IMR1RM 0x46
#define QUARK_IMR_IMR1WM 0x47
#define QUARK_IMR_IMR2L  0x48
#define QUARK_IMR_IMR2H  0x49
#define QUARK_IMR_IMR2RM 0x4A
#define QUARK_IMR_IMR2WM 0x4B
#define QUARK_IMR_IMR3L  0x4C
#define QUARK_IMR_IMR3H  0x4D
#define QUARK_IMR_IMR3RM 0x4E
#define QUARK_IMR_IMR3WM 0x4F
#define QUARK_IMR_IMR4L  0x50
#define QUARK_IMR_IMR4H  0x51
#define QUARK_IMR_IMR4RM 0x52
#define QUARK_IMR_IMR4WM 0x53
#define QUARK_IMR_IMR5L  0x54
#define QUARK_IMR_IMR5H  0x55
#define QUARK_IMR_IMR5RM 0x56
#define QUARK_IMR_IMR5WM 0x57
#define QUARK_IMR_IMR6L  0x58
#define QUARK_IMR_IMR6H  0x59
#define QUARK_IMR_IMR6RM 0x5A
#define QUARK_IMR_IMR6WM 0x5B
#define QUARK_IMR_IMR7L  0x5C
#define QUARK_IMR_IMR7H  0x5D
#define QUARK_IMR_IMR7RM 0x5E
#define QUARK_IMR_IMR7WM 0x5F

#define QUARK_IMR_READ_ENABLE_ALL 0xBFFFFFFF
#define QUARK_IMR_WRITE_ENABLE_ALL 0xFFFFFFFF
#define QUARK_IMR_BASE_ADDRESS 0x00000000
#define QUARK_IMR_LOCK 0x80000000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for an Intel Host Bridge device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    PciConfigInterface - Stores the interface to access PCI configuration space.

    PciConfigInterfaceAvailable - Stores a boolean indicating if the PCI
        config interface is actively available.

    RegisteredForPciConfigInterfaces - Stores a boolean indicating whether or
        not the driver has registered for PCI configuration space interface
        notifications on this device or not.

--*/

typedef struct _QUARK_HOST_BRIDGE {
    PDEVICE OsDevice;
    INTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    BOOL PciConfigInterfaceAvailable;
    BOOL RegisteredForPciConfigInterfaces;
} QUARK_HOST_BRIDGE, *PQUARK_HOST_BRIDGE;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
QhbAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
QhbDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
QhbDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
QhbDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
QhbDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
QhbDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
QhbpStartDevice (
    PQUARK_HOST_BRIDGE Device
    );

VOID
QhbpProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

KSTATUS
QhbpDisableAllImrs (
    PQUARK_HOST_BRIDGE Device
    );

KSTATUS
QhbpDebugPrintAllImrs (
    PQUARK_HOST_BRIDGE Device
    );

KSTATUS
QhbpDebugPrintImr (
    PQUARK_HOST_BRIDGE Device,
    ULONG ImrIndex,
    UCHAR RegisterLow,
    UCHAR RegisterHigh,
    UCHAR RegisterReadMask,
    UCHAR RegisterWriteMask
    );

KSTATUS
QhbpRemoveImr (
    PQUARK_HOST_BRIDGE Device,
    UCHAR RegisterLow,
    UCHAR RegisterHigh,
    UCHAR RegisterReadMask,
    UCHAR RegisterWriteMask
    );

KSTATUS
QhbpSidebandReadRegister (
    PQUARK_HOST_BRIDGE Device,
    ULONG Identifier,
    UCHAR Command,
    UCHAR Register,
    PULONG Data
    );

KSTATUS
QhbpSidebandWriteRegister (
    PQUARK_HOST_BRIDGE Device,
    ULONG Identifier,
    UCHAR Command,
    UCHAR Register,
    ULONG Data
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER QhbDriver = NULL;
UUID QhbPciConfigurationInterfaceUuid = UUID_PCI_CONFIG_ACCESS;

//
// Set this to TRUE to print all the IMRs.
//

BOOL QhbDebugImrs = FALSE;

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

    This routine is the entry point for the Quark Host Bridge driver. It
    registers its other dispatch functions, and performs driver-wide
    initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    QhbDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = QhbAddDevice;
    FunctionTable.DispatchStateChange = QhbDispatchStateChange;
    FunctionTable.DispatchOpen = QhbDispatchOpen;
    FunctionTable.DispatchClose = QhbDispatchClose;
    FunctionTable.DispatchIo = QhbDispatchIo;
    FunctionTable.DispatchSystemControl = QhbDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
QhbAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which this driver
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

    PQUARK_HOST_BRIDGE Context;
    KSTATUS Status;

    Context = MmAllocatePagedPool(sizeof(QUARK_HOST_BRIDGE),
                                  QUARK_HOST_BRIDGE_ALLOCATION_TAG);

    if (Context == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Context, sizeof(QUARK_HOST_BRIDGE));
    Context->OsDevice = DeviceToken;
    Status = IoAttachDriverToDevice(Driver, DeviceToken, Context);
    return Status;
}

VOID
QhbDispatchStateChange (
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

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    switch (Irp->MinorCode) {
    case IrpMinorStartDevice:
        if ((KSUCCESS(IoGetIrpStatus(Irp))) && (Irp->Direction == IrpUp)) {
            Status = QhbpStartDevice(DeviceContext);
            IoCompleteIrp(QhbDriver, Irp, Status);
        }

        break;

    default:
        break;
    }

    return;
}

VOID
QhbDispatchOpen (
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

    return;
}

VOID
QhbDispatchClose (
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

    return;
}

VOID
QhbDispatchIo (
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

    return;
}

VOID
QhbDispatchSystemControl (
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

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Do no processing on any IRPs. Let them flow.
    //

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
QhbpStartDevice (
    PQUARK_HOST_BRIDGE Device
    )

/*++

Routine Description:

    This routine attempts to start the Quark Host Bridge device.

Arguments:

    Device - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Start listening for a PCI config interface.
    //

    if (Device->RegisteredForPciConfigInterfaces == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                              &QhbPciConfigurationInterfaceUuid,
                              QhbpProcessPciConfigInterfaceChangeNotification,
                              Device->OsDevice,
                              Device,
                              TRUE);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->RegisteredForPciConfigInterfaces = TRUE;
    }

    //
    // PCI config interfaces better have shown up.
    //

    if (Device->PciConfigInterfaceAvailable == FALSE) {
        Status = STATUS_NO_INTERFACE;
        goto StartDeviceEnd;
    }

    //
    // Disable all IMRs. Some day in the secure boot world this would instead
    // properly cover the kernel and boot drivers.
    //

    Status = QhbpDisableAllImrs(Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

StartDeviceEnd:
    return Status;
}

VOID
QhbpProcessPciConfigInterfaceChangeNotification (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called when a PCI configuration space access interface
    changes in availability.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PQUARK_HOST_BRIDGE ControllerContext;

    ControllerContext = (PQUARK_HOST_BRIDGE)Context;
    if (Arrival != FALSE) {
        if (InterfaceBufferSize >= sizeof(INTERFACE_PCI_CONFIG_ACCESS)) {

            ASSERT(ControllerContext->PciConfigInterfaceAvailable == FALSE);

            RtlCopyMemory(&(ControllerContext->PciConfigInterface),
                          InterfaceBuffer,
                          sizeof(INTERFACE_PCI_CONFIG_ACCESS));

            ControllerContext->PciConfigInterfaceAvailable = TRUE;
        }

    } else {
        ControllerContext->PciConfigInterfaceAvailable = FALSE;
    }

    return;
}

KSTATUS
QhbpDisableAllImrs (
    PQUARK_HOST_BRIDGE Device
    )

/*++

Routine Description:

    This routine removes all unlocked IMR regions.

Arguments:

    Device - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

{

    if (QhbDebugImrs != FALSE) {
        QhbpDebugPrintAllImrs(Device);
    }

    //
    // Remove all IMRs, ignoring failures due to them being locked (its
    // assumed any locked IMRs are protecting firmware regions that the
    // firmware also reserved in the memory map).
    //

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR0L,
                  QUARK_IMR_IMR0H,
                  QUARK_IMR_IMR0RM,
                  QUARK_IMR_IMR0WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR1L,
                  QUARK_IMR_IMR1H,
                  QUARK_IMR_IMR1RM,
                  QUARK_IMR_IMR1WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR2L,
                  QUARK_IMR_IMR2H,
                  QUARK_IMR_IMR2RM,
                  QUARK_IMR_IMR2WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR3L,
                  QUARK_IMR_IMR3H,
                  QUARK_IMR_IMR3RM,
                  QUARK_IMR_IMR3WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR4L,
                  QUARK_IMR_IMR4H,
                  QUARK_IMR_IMR4RM,
                  QUARK_IMR_IMR4WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR5L,
                  QUARK_IMR_IMR5H,
                  QUARK_IMR_IMR5RM,
                  QUARK_IMR_IMR5WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR6L,
                  QUARK_IMR_IMR6H,
                  QUARK_IMR_IMR6RM,
                  QUARK_IMR_IMR6WM);

    QhbpRemoveImr(Device,
                  QUARK_IMR_IMR7L,
                  QUARK_IMR_IMR7H,
                  QUARK_IMR_IMR7RM,
                  QUARK_IMR_IMR7WM);

    if (QhbDebugImrs != FALSE) {
        QhbpDebugPrintAllImrs(Device);
    }

    return STATUS_SUCCESS;
}

KSTATUS
QhbpDebugPrintAllImrs (
    PQUARK_HOST_BRIDGE Device
    )

/*++

Routine Description:

    This routine prints all IMRs to the debug console.

Arguments:

    Device - Supplies a pointer to the device context.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = QhbpDebugPrintImr(Device,
                               0,
                               QUARK_IMR_IMR0L,
                               QUARK_IMR_IMR0H,
                               QUARK_IMR_IMR0RM,
                               QUARK_IMR_IMR0WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               1,
                               QUARK_IMR_IMR1L,
                               QUARK_IMR_IMR1H,
                               QUARK_IMR_IMR1RM,
                               QUARK_IMR_IMR1WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               2,
                               QUARK_IMR_IMR2L,
                               QUARK_IMR_IMR2H,
                               QUARK_IMR_IMR2RM,
                               QUARK_IMR_IMR2WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               3,
                               QUARK_IMR_IMR3L,
                               QUARK_IMR_IMR3H,
                               QUARK_IMR_IMR3RM,
                               QUARK_IMR_IMR3WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               4,
                               QUARK_IMR_IMR4L,
                               QUARK_IMR_IMR4H,
                               QUARK_IMR_IMR4RM,
                               QUARK_IMR_IMR4WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               5,
                               QUARK_IMR_IMR5L,
                               QUARK_IMR_IMR5H,
                               QUARK_IMR_IMR5RM,
                               QUARK_IMR_IMR5WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               6,
                               QUARK_IMR_IMR6L,
                               QUARK_IMR_IMR6H,
                               QUARK_IMR_IMR6RM,
                               QUARK_IMR_IMR6WM);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpDebugPrintImr(Device,
                               7,
                               QUARK_IMR_IMR7L,
                               QUARK_IMR_IMR7H,
                               QUARK_IMR_IMR7RM,
                               QUARK_IMR_IMR7WM);

    return Status;
}

KSTATUS
QhbpDebugPrintImr (
    PQUARK_HOST_BRIDGE Device,
    ULONG ImrIndex,
    UCHAR RegisterLow,
    UCHAR RegisterHigh,
    UCHAR RegisterReadMask,
    UCHAR RegisterWriteMask
    )

/*++

Routine Description:

    This routine prints the IMR contents to the debugger.

Arguments:

    Device - Supplies a pointer to the device context.

    ImrIndex - Supplies the IMR number that gets printed out with the values.

    RegisterLow - Supplies the low address register, IMRxL.

    RegisterHigh - Supplies the high address register, IMRxH.

    RegisterReadMask - Supplies the read mask register, IMRxRM.

    RegisterWriteMask - Supplies the write mask register, IMRxWM.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONG Value;

    Status = QhbpSidebandReadRegister(Device,
                                      QUARK_SIDEBAND_ID_IMR,
                                      QUARK_SIDEBAND_OPCODE_READ,
                                      RegisterLow,
                                      &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlDebugPrint("IMR%dL %08X ", ImrIndex, Value);
    Status = QhbpSidebandReadRegister(Device,
                                      QUARK_SIDEBAND_ID_IMR,
                                      QUARK_SIDEBAND_OPCODE_READ,
                                      RegisterHigh,
                                      &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlDebugPrint("IMR%dH %08X ", ImrIndex, Value);
    Status = QhbpSidebandReadRegister(Device,
                                      QUARK_SIDEBAND_ID_IMR,
                                      QUARK_SIDEBAND_OPCODE_READ,
                                      RegisterReadMask,
                                      &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlDebugPrint("IMR%dRM %08X ", ImrIndex, Value);
    Status = QhbpSidebandReadRegister(Device,
                                      QUARK_SIDEBAND_ID_IMR,
                                      QUARK_SIDEBAND_OPCODE_READ,
                                      RegisterWriteMask,
                                      &Value);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    RtlDebugPrint("IMR%dWM %08X\n", ImrIndex, Value);
    return STATUS_SUCCESS;
}

KSTATUS
QhbpRemoveImr (
    PQUARK_HOST_BRIDGE Device,
    UCHAR RegisterLow,
    UCHAR RegisterHigh,
    UCHAR RegisterReadMask,
    UCHAR RegisterWriteMask
    )

/*++

Routine Description:

    This routine removes an Isolated Memory region.

Arguments:

    Device - Supplies a pointer to the device context.

    RegisterLow - Supplies the low address register, IMRxL.

    RegisterHigh - Supplies the high address register, IMRxH.

    RegisterReadMask - Supplies the read mask register, IMRxRM.

    RegisterWriteMask - Supplies the write mask register, IMRxWM.

Return Value:

    Status code.

--*/

{

    ULONG LowValue;
    KSTATUS Status;

    Status = QhbpSidebandReadRegister(Device,
                                      QUARK_SIDEBAND_ID_IMR,
                                      QUARK_SIDEBAND_OPCODE_READ,
                                      RegisterLow,
                                      &LowValue);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    if ((LowValue & QUARK_IMR_LOCK) != 0) {
        return STATUS_ACCESS_DENIED;
    }

    Status = QhbpSidebandWriteRegister(Device,
                                       QUARK_SIDEBAND_ID_IMR,
                                       QUARK_SIDEBAND_OPCODE_WRITE,
                                       RegisterReadMask,
                                       QUARK_IMR_READ_ENABLE_ALL);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpSidebandWriteRegister(Device,
                                       QUARK_SIDEBAND_ID_IMR,
                                       QUARK_SIDEBAND_OPCODE_WRITE,
                                       RegisterWriteMask,
                                       QUARK_IMR_WRITE_ENABLE_ALL);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpSidebandWriteRegister(Device,
                                       QUARK_SIDEBAND_ID_IMR,
                                       QUARK_SIDEBAND_OPCODE_WRITE,
                                       RegisterHigh,
                                       QUARK_IMR_BASE_ADDRESS);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = QhbpSidebandWriteRegister(Device,
                                       QUARK_SIDEBAND_ID_IMR,
                                       QUARK_SIDEBAND_OPCODE_WRITE,
                                       RegisterLow,
                                       QUARK_IMR_BASE_ADDRESS);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
QhbpSidebandReadRegister (
    PQUARK_HOST_BRIDGE Device,
    ULONG Identifier,
    UCHAR Command,
    UCHAR Register,
    PULONG Data
    )

/*++

Routine Description:

    This routine performs a sideband register read.

Arguments:

    Device - Supplies a pointer to the device context.

    Identifier - Supplies the sideband identifier.

    Command - Supplies the type of read command to perform.

    Register - Supplies the register to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    Status code.

--*/

{

    PVOID DeviceToken;
    ULONG Message;
    ULONGLONG Result;
    KSTATUS Status;

    if (Device->PciConfigInterfaceAvailable == FALSE) {
        return STATUS_NO_INTERFACE;
    }

    Message = QUARK_SIDEBAND_MESSAGE(Identifier, Command, Register);

    //
    // Write the command register and read the data register.
    //

    DeviceToken = Device->PciConfigInterface.DeviceToken;
    Status = Device->PciConfigInterface.WritePciConfig(
                                                 DeviceToken,
                                                 QUARK_SIDEBAND_COMMAND_OFFSET,
                                                 sizeof(ULONG),
                                                 Message);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Device->PciConfigInterface.ReadPciConfig(
                                                    DeviceToken,
                                                    QUARK_SIDEBAND_DATA_OFFSET,
                                                    sizeof(ULONG),
                                                    &Result);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    *Data = Result;
    return STATUS_SUCCESS;
}

KSTATUS
QhbpSidebandWriteRegister (
    PQUARK_HOST_BRIDGE Device,
    ULONG Identifier,
    UCHAR Command,
    UCHAR Register,
    ULONG Data
    )

/*++

Routine Description:

    This routine performs a sideband register write.

Arguments:

    Device - Supplies a pointer to the device context.

    Identifier - Supplies the sideband identifier.

    Command - Supplies the type of command to perform.

    Register - Supplies the register to write.

    Data - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PVOID DeviceToken;
    ULONG Message;
    KSTATUS Status;

    if (Device->PciConfigInterfaceAvailable == FALSE) {
        return STATUS_NO_INTERFACE;
    }

    Message = QUARK_SIDEBAND_MESSAGE(Identifier, Command, Register);

    //
    // Write the data register and then write the command register.
    //

    DeviceToken = Device->PciConfigInterface.DeviceToken;
    Status = Device->PciConfigInterface.WritePciConfig(
                                                    DeviceToken,
                                                    QUARK_SIDEBAND_DATA_OFFSET,
                                                    sizeof(ULONG),
                                                    Data);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Status = Device->PciConfigInterface.WritePciConfig(
                                                 DeviceToken,
                                                 QUARK_SIDEBAND_COMMAND_OFFSET,
                                                 sizeof(ULONG),
                                                 Message);

    return Status;
}

