/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpidrv.c

Abstract:

    This module implements the ACPI driver functions.

Author:

    Evan Green 29-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "fixedreg.h"
#include "namespce.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the identifier of the root ACPI device.
//

#define ACPI_ROOT_DEVICE_ID "ACPI"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AcpiUnload (
    PVOID Driver
    );

KSTATUS
AcpiAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
AcpiDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AcpiDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AcpiDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AcpiDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
AcpiDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Remember a pointer to the driver object returned by the system corresponding
// to this driver.
//

PDRIVER AcpiDriver = NULL;

//
// Store a pointer to the FADT.
//

PFADT AcpiFadtTable = NULL;

//
// Store a global list of ACPI device objects.
//

LIST_ENTRY AcpiDeviceObjectListHead;
LIST_ENTRY AcpiDeviceDependencyList;
KSPIN_LOCK AcpiDeviceListLock;

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

    This routine is the entry point for the ACPI driver. It registers its other
    dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    AcpiDriver = Driver;
    INITIALIZE_LIST_HEAD(&AcpiDeviceObjectListHead);
    INITIALIZE_LIST_HEAD(&AcpiDeviceDependencyList);
    KeInitializeSpinLock(&AcpiDeviceListLock);
    AcpiFadtTable = AcpiFindTable(FADT_SIGNATURE, NULL);
    if (AcpiFadtTable == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto DriverEntryEnd;
    }

    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = AcpiUnload;
    FunctionTable.AddDevice = AcpiAddDevice;
    FunctionTable.DispatchStateChange = AcpiDispatchStateChange;
    FunctionTable.DispatchOpen = AcpiDispatchOpen;
    FunctionTable.DispatchClose = AcpiDispatchClose;
    FunctionTable.DispatchIo = AcpiDispatchIo;
    FunctionTable.DispatchSystemControl = AcpiDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    Status = AcpipInitializeFixedRegisterSupport();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Take control of the system from the BIOS by enabling ACPI mode.
    //

    Status = AcpipEnableAcpiMode();
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ACPI: Failed to enable ACPI mode: %d.\n", Status);
        goto DriverEntryEnd;
    }

    //
    // Fire up the AML interpreter.
    //

    Status = AcpipInitializeAmlInterpreter();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Add shutdown, reboot, and system state transition support.
    //

    Status = AcpipInitializeSystemStateTransitions();
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ACPI: Warning: InitSystemStateTransitions: %d\n",
                      Status);

        Status = STATUS_SUCCESS;
    }

DriverEntryEnd:
    if (!KSUCCESS(Status)) {
        AcpipUnmapFixedRegisters();
    }

    return Status;
}

VOID
AcpiUnload (
    PVOID Driver
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

{

    AcpipUnmapFixedRegisters();
    return;
}

KSTATUS
AcpiAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when the ACPI root device is enumerated. It will
    attach the driver to the device.

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

    PACPI_DEVICE_CONTEXT Device;
    BOOL Match;
    KSTATUS Status;
    PACPI_OBJECT SystemBusObject;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Device = NULL;
    Status = STATUS_UNKNOWN_DEVICE;

    //
    // ACPI is the functional driver for the root object.
    //

    Match = IoAreDeviceIdsEqual(DeviceId, ACPI_ROOT_DEVICE_ID);
    if (Match != FALSE) {
        Device = MmAllocatePagedPool(sizeof(ACPI_DEVICE_CONTEXT),
                                     ACPI_ALLOCATION_TAG);

        if (Device == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AddDeviceEnd;
        }

        RtlZeroMemory(Device, sizeof(ACPI_DEVICE_CONTEXT));
        Device->BusAddress = ACPI_INVALID_BUS_ADDRESS;
        SystemBusObject = AcpipFindNamedObject(AcpipGetNamespaceRoot(),
                                               ACPI_SYSTEM_BUS_OBJECT_NAME);

        ASSERT(SystemBusObject != NULL);

        Device->NamespaceObject = SystemBusObject;
        Device->Flags |= ACPI_DEVICE_BUS_DRIVER;
        Status = IoAttachDriverToDevice(Driver, DeviceToken, Device);
        if (!KSUCCESS(Status)) {
            goto AddDeviceEnd;
        }

        Device->NamespaceObject->U.Device.OsDevice = DeviceToken;
        Device->NamespaceObject->U.Device.DeviceContext = Device;
        KeAcquireSpinLock(&AcpiDeviceListLock);
        INSERT_AFTER(&(Device->ListEntry), &AcpiDeviceObjectListHead);
        KeReleaseSpinLock(&AcpiDeviceListLock);
        goto AddDeviceEnd;
    }

    //
    // ACPI does not recognize this device.
    //

    Status = STATUS_UNKNOWN_DEVICE;

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            MmFreePagedPool(Device);
        }
    }

    return Status;
}

VOID
AcpiDispatchStateChange (
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

    PACPI_DEVICE_CONTEXT Device;
    KSTATUS Status;

    Device = (PACPI_DEVICE_CONTEXT)DeviceContext;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpUp) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            if ((Device->Flags & ACPI_DEVICE_BUS_DRIVER) != 0) {
                Status = AcpipQueryResourceRequirements(Irp->Device,
                                                        Device,
                                                        Irp);

                if (!KSUCCESS(Status)) {
                    if (Status != STATUS_NOT_READY) {
                        RtlDebugPrint("ACPI: Failed to get device resources."
                                      "Device 0x%08x, Status: %d\n",
                                      Irp->Device,
                                      Status);
                    }
                }

                IoCompleteIrp(AcpiDriver, Irp, Status);

            //
            // If ACPI is not the head honcho, then play a supporting role
            // of translating any resources the bus driver requested.
            //

            } else {
                Status = AcpipFilterResourceRequirements(Irp->Device,
                                                         Device,
                                                         Irp);

                //
                // Fail an IRP that was going to succeed if this fails.
                //

                if ((!KSUCCESS(Status)) && (KSUCCESS(IoGetIrpStatus(Irp)))) {
                    IoCompleteIrp(AcpiDriver, Irp, Status);
                }
            }

            break;

        case IrpMinorStartDevice:
            Status = AcpipStartDevice(Irp->Device, Device, Irp);
            if ((!KSUCCESS(Status)) ||
                ((Device->Flags & ACPI_DEVICE_BUS_DRIVER) != 0)) {

                IoCompleteIrp(AcpiDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryChildren:
            Status = AcpipEnumerateDeviceChildren(Irp->Device, Device, Irp);
            if (!KSUCCESS(Status)) {
                RtlDebugPrint("ACPI: Failed to enumerate device children. "
                              "Device 0x%08x, Status: %d\n",
                              Irp->Device,
                              Status);

                IoCompleteIrp(AcpiDriver, Irp, Status);

            //
            // If it was successful and ACPI is the bus driver, complete the
            // IRP.
            //

            } else if ((Device->Flags & ACPI_DEVICE_BUS_DRIVER) != 0) {
                IoCompleteIrp(AcpiDriver, Irp, Status);
            }

            break;

        case IrpMinorRemoveDevice:
            AcpipRemoveDevice(Device);
            if ((Device->Flags & ACPI_DEVICE_BUS_DRIVER) != 0) {
                IoCompleteIrp(AcpiDriver, Irp, STATUS_SUCCESS);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
AcpiDispatchOpen (
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
AcpiDispatchClose (
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
AcpiDispatchIo (
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
AcpiDispatchSystemControl (
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

    return;
}

