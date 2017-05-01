/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pci.c

Abstract:

    This module implements the PCI driver.

Author:

    Evan Green 16-Sep-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "pci.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
PciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
PciDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
PciDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
PcipReportChildren (
    PIRP QueryChildrenIrp,
    PPCI_DEVICE PciDevice
    );

VOID
PcipEnumerateChildren (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    );

KSTATUS
PcipQueryResourceRequirements (
    PDEVICE Device,
    PPCI_DEVICE DeviceObject,
    PIRP Irp
    );

KSTATUS
PcipQueryBridgeResourceRequirements (
    PDEVICE Device,
    PPCI_DEVICE DeviceObject,
    PIRP Irp
    );

KSTATUS
PcipSetDeviceResources (
    PPCI_DEVICE DeviceContext,
    PRESOURCE_ALLOCATION_LIST AllocationList
    );

VOID
PcipEnableDevice (
    PPCI_DEVICE DeviceContext
    );

KSTATUS
PcipSetBridgeDeviceResources (
    PPCI_DEVICE DeviceContext,
    PRESOURCE_ALLOCATION_LIST AllocationList
    );

ULONG
PcipFindDevice (
    PPCI_DEVICE ParentBus,
    UCHAR Device,
    UCHAR Function
    );

ULONG
PcipGetNewChildIndex (
    PPCI_DEVICE ParentBus
    );

KSTATUS
PcipQueryInterface (
    PIRP Irp,
    PPCI_DEVICE PciDevice
    );

KSTATUS
PcipInterfaceReadConfigSpace (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    );

KSTATUS
PcipInterfaceWriteConfigSpace (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    );

KSTATUS
PcipInterfaceReadSpecificConfigSpace (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    );

KSTATUS
PcipInterfaceWriteSpecificConfigSpace (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    );

KSTATUS
PcipStartBusDevice (
    PIRP StartIrp,
    PPCI_DEVICE DeviceContext
    );

KSTATUS
PcipCreateFunctionInterfaces (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    );

KSTATUS
PcipCreateBusInterfaces (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    );

PSTR
PcipGetClassId (
    ULONG ClassCode
    );

KSTATUS
PcipGetBusDriverDevice (
    PDEVICE OsDevice,
    PPCI_DEVICE *BusDriverDevice
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER PciDriver = NULL;

//
// Store the UUID of PCI configuration space access.
//

UUID PciConfigSpaceUuid = UUID_PCI_CONFIG_ACCESS;

//
// Store the UUID of specific PCI configuration space access.
//

UUID PciSpecificConfigSpaceUuid = UUID_PCI_CONFIG_ACCESS_SPECIFIC;

//
// Store the UUID of the PCI MSI and MSI-X access.
//

UUID PciMessageSignaledInterruptsUuid = UUID_PCI_MESSAGE_SIGNALED_INTERRUPTS;

//
// Store the UUID of the ACPI bus number interface.
//

UUID PciAcpiBusAddressUuid = UUID_ACPI_BUS_ADDRESS;

//
// Store the UUID of the internal PCI interface for getting the bus driver's
// PCI device structure.
//

UUID PciBusDriverDeviceUuid =
    {{0x73696D6F, 0x74207365, 0x656B206F, 0x61207066}};

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

    This routine is the entry point for the PCI driver. It registers its other
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

    PciDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = PciAddDevice;
    FunctionTable.DispatchStateChange = PciDispatchStateChange;
    FunctionTable.DispatchSystemControl = PciDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
PciAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a PCI device is detected. PCI will attach
    itself to the driver stack.

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

    STATUS_UNKNOWN_DEVICE if the device is not recognized by the driver. This
    usually a sign of misconfiguration (assigning PCI as a driver for something
    it does not own).

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PPCI_DEVICE Device;
    PCI_DEVICE_TYPE DeviceType;
    BOOL Match;
    KSTATUS Status;

    Device = NULL;
    DeviceType = PciDeviceInvalid;
    Status = STATUS_UNKNOWN_DEVICE;

    //
    // The PCI driver is the functional driver for the PCI Root device.
    //

    Match = IoAreDeviceIdsEqual(DeviceId, PCI_BUS_ID);
    if (Match == FALSE) {
        Match = IoAreDeviceIdsEqual(DeviceId, PCI_EXPRESS_BUS_ID);
    }

    if (Match != FALSE) {
        DeviceType = PciDeviceBus;

    } else {
        if (ClassId != NULL) {
            Match = RtlAreStringsEqual(ClassId,
                                       PCI_BRIDGE_CLASS_ID,
                                       sizeof(PCI_BRIDGE_CLASS_ID));
        }

        if ((Match == FALSE) && (ClassId != NULL)) {
            Match = RtlAreStringsEqual(ClassId,
                                       PCI_SUBTRACTIVE_BRIDGE_CLASS_ID,
                                       sizeof(PCI_SUBTRACTIVE_BRIDGE_CLASS_ID));
        }

        if (Match != FALSE) {
            DeviceType = PciDeviceBridge;
        }
    }

    //
    // If the device was not idenfified, then the system was misconfigured to
    // have PCI be the driver of some random device.
    //

    if (Match == FALSE) {
        Status = STATUS_UNKNOWN_DEVICE;
        goto AddDeviceEnd;
    }

    ASSERT(DeviceType != PciDeviceInvalid);

    Device = MmAllocateNonPagedPool(sizeof(PCI_DEVICE), PCI_ALLOCATION_TAG);
    if (Device == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlZeroMemory(Device, sizeof(PCI_DEVICE));
    Device->Type = DeviceType;
    Device->BusNumber = 0;
    if (DeviceType == PciDeviceBus) {
        Device->ReadConfig = PcipRootReadConfig;
        Device->WriteConfig = PcipRootWriteConfig;
    }

    Status = IoAttachDriverToDevice(Driver, DeviceToken, Device);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (Device != NULL) {
            MmFreeNonPagedPool(Device);
        }
    }

    return Status;
}

VOID
PciDispatchStateChange (
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

    PPCI_DEVICE PciDevice;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    PciDevice = DeviceContext;

    //
    // The IRP is on its way down the stack. Do most processing here.
    //

    if (Irp->Direction == IrpDown) {
        Status = STATUS_NOT_SUPPORTED;
        switch (Irp->MinorCode) {

        //
        // If the device is a function (therefore PCI is acting as the bus
        // driver), then return the device's resources.
        //

        case IrpMinorQueryResources:
            if (PciDevice->Type == PciDeviceFunction) {
                if (PciDevice->DeviceIsBridge != FALSE) {
                    Status = PcipQueryBridgeResourceRequirements(Irp->Device,
                                                                 PciDevice,
                                                                 Irp);

                } else {
                    Status = PcipQueryResourceRequirements(Irp->Device,
                                                           PciDevice,
                                                           Irp);
                }

                IoCompleteIrp(PciDriver, Irp, Status);
            }

            break;

        //
        // Assume the device is already started. Expose the interface for
        // interacting with the device's PCI config space.
        //

        case IrpMinorStartDevice:
            if (PciDevice->Type == PciDeviceFunction) {

                //
                // Set the BARs and enable the device.
                //

                if (PciDevice->DeviceIsBridge != FALSE) {
                    Status = PcipSetBridgeDeviceResources(
                                         DeviceContext,
                                         Irp->U.StartDevice.BusLocalResources);

                } else {
                    Status = PcipSetDeviceResources(
                                         DeviceContext,
                                         Irp->U.StartDevice.BusLocalResources);
                }

                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(PciDriver, Irp, Status);
                    break;
                }

                //
                // Enable decoding on the device.
                //

                if (PciDevice->DeviceIsBridge == FALSE) {
                    PcipEnableDevice(DeviceContext);
                }

                //
                // As the bus driver of a function, PCI completes the IRP.
                //

                IoCompleteIrp(PciDriver, Irp, Status);

            } else if ((PciDevice->Type == PciDeviceBus) ||
                       (PciDevice->Type == PciDeviceBridge)) {

                Status = PcipStartBusDevice(Irp, PciDevice);
                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(PciDriver, Irp, Status);
                }

                Status = STATUS_SUCCESS;
            }

            break;

        //
        // Enumerate any children on the bus.
        //

        case IrpMinorQueryChildren:

            //
            // If the driver is acting as a bus driver for a function, there are
            // no children. Complete the IRP.
            //

            if (PciDevice->Type == PciDeviceFunction) {
                IoCompleteIrp(PciDriver, Irp, STATUS_SUCCESS);

            //
            // If PCI is acting as the functional driver, enumerate the
            // children, but don't complete the IRP.
            //

            } else {
                Status = PcipReportChildren(Irp, DeviceContext);
            }

            break;

        //
        // Process interface requests.
        //

        case IrpMinorQueryInterface:
            Status = PcipQueryInterface(Irp, DeviceContext);
            if (Status != STATUS_NO_INTERFACE) {
                IoCompleteIrp(PciDriver, Irp, Status);
            }

            break;

        case IrpMinorIdle:
            if (PciDevice->Type == PciDeviceFunction) {
                IoCompleteIrp(PciDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorSuspend:
            if (PciDevice->Type == PciDeviceFunction) {
                IoCompleteIrp(PciDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorResume:
            if (PciDevice->Type == PciDeviceFunction) {
                IoCompleteIrp(PciDriver, Irp, STATUS_SUCCESS);
            }

            break;

        //
        // If the IRP is unknown, don't touch it.
        //

        default:
            break;
        }

    } else {

        ASSERT(Irp->Direction == IrpUp);
    }

    return;
}

VOID
PciDispatchSystemControl (
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

    if (Irp->Direction == IrpDown) {
        IoCompleteIrp(PciDriver, Irp, STATUS_NOT_SUPPORTED);

    } else {

        ASSERT(Irp->Direction == IrpUp);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
PcipReportChildren (
    PIRP QueryChildrenIrp,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine responds to a Query Children IRP.

Arguments:

    QueryChildrenIrp - Supplies a pointer to the I/O request packet.

    PciDevice - Supplies a pointer to the PCI device context this IRP relates
        to.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if the array could not be allocated for the
        children.

--*/

{

    PDEVICE *Children;

    //
    // If the device is not a bus, it has no children.
    //

    if ((PciDevice->Type != PciDeviceBus) &&
        (PciDevice->Type != PciDeviceBridge)) {

        QueryChildrenIrp->U.QueryChildren.Children = NULL;
        QueryChildrenIrp->U.QueryChildren.ChildCount = 0;
        return STATUS_SUCCESS;
    }

    //
    // Scan the bus and pick up any changes.
    //

    PcipEnumerateChildren(QueryChildrenIrp->Device, PciDevice);
    if (PciDevice->ChildCount == 0) {
        QueryChildrenIrp->U.QueryChildren.Children = NULL;
        QueryChildrenIrp->U.QueryChildren.ChildCount = 0;
        return STATUS_SUCCESS;
    }

    //
    // Allocated paged pool for the array to return.
    //

    Children = MmAllocatePagedPool(sizeof(PDEVICE) * PciDevice->ChildCount,
                                   PCI_ALLOCATION_TAG);

    if (Children == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(Children,
                  PciDevice->Children,
                  PciDevice->ChildCount * sizeof(PDEVICE));

    QueryChildrenIrp->U.QueryChildren.Children = Children;
    QueryChildrenIrp->U.QueryChildren.ChildCount = PciDevice->ChildCount;
    return STATUS_SUCCESS;
}

VOID
PcipEnumerateChildren (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine scans the given PCI bus, enumerating any new children and
    removing any missing ones.

Arguments:

    Device - Supplies a pointer to the device.

    PciDevice - Supplies a pointer to the PCI device context relating to this
        device.

Return Value:

    None.

--*/

{

    PPCI_CHILD Child;
    ULONG ChildIndex;
    ULONG ClassCode;
    PSTR ClassCodeString;
    ULONG DeviceId;
    UCHAR DeviceNumber;
    UCHAR Function;
    ULONG HeaderType;
    ULONG Id;
    UCHAR MaxFunction;
    CHAR NewDeviceId[PCI_DEVICE_ID_SIZE];
    PPCI_DEVICE NewPciDevice;
    KSTATUS Status;
    ULONG VendorId;

    //
    // If the device is not a bus, it has no children.
    //

    if ((PciDevice->Type != PciDeviceBus) &&
        (PciDevice->Type != PciDeviceBridge)) {

        return;
    }

    //
    // Scan through all functions and all devices on this bus.
    //

    for (DeviceNumber = 0; DeviceNumber < MAX_PCI_DEVICE; DeviceNumber += 1) {

        //
        // Read configuration space to get the vendor and device ID.
        //

        Id = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                          DeviceNumber,
                                          0,
                                          PCI_ID_OFFSET,
                                          sizeof(ULONG));

        DeviceId = (Id & PCI_DEVICE_ID_MASK) >> PCI_DEVICE_ID_SHIFT;
        VendorId = Id & PCI_VENDOR_ID_MASK;
        if ((VendorId == 0) || (VendorId == PCI_INVALID_VENDOR_ID)) {
            continue;
        }

        //
        // Determine the total number of functions that need to be scanned for
        // this device by looking at the header type's multi-function flag.
        //

        HeaderType = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                  DeviceNumber,
                                                  0,
                                                  PCI_HEADER_TYPE_OFFSET,
                                                  sizeof(ULONG));

        HeaderType = (HeaderType & PCI_HEADER_TYPE_MASK) >>
                     PCI_HEADER_TYPE_SHIFT;

        if ((HeaderType & PCI_HEADER_TYPE_FLAG_MULTIPLE_FUNCTIONS) != 0) {
            MaxFunction = MAX_PCI_FUNCTION;

        } else {
            MaxFunction = 0;
        }

        for (Function = 0; Function <= MaxFunction; Function += 1) {

            //
            // Read configuration space to get the vendor and device ID if it
            // has not already been read.
            //

            if (Function != 0) {
                Id = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                  DeviceNumber,
                                                  Function,
                                                  PCI_ID_OFFSET,
                                                  sizeof(ULONG));

                DeviceId = (Id & PCI_DEVICE_ID_MASK) >> PCI_DEVICE_ID_SHIFT;
                VendorId = Id & PCI_VENDOR_ID_MASK;
            }

            //
            // Attempt to find a previously enumerated child for this device
            // and function.
            //

            ChildIndex = PcipFindDevice(PciDevice, DeviceNumber, Function);

            //
            // If there was a device here and it seems to have disappeared,
            // free the device and swap it out for the last one.
            //

            if (ChildIndex != MAX_ULONG) {
                Child = PciDevice->ChildrenData[ChildIndex];
                if ((VendorId == Child->VendorId) &&
                    (DeviceId == Child->DeviceId)) {

                    continue;
                }

                //
                // Devices shouldn't just come and go like this. If they really
                // do, then completely remove the old device and add a new
                // different one in its place, rather than this bizarre
                // switcharoo.
                //

                ASSERT(FALSE);
                ASSERT(PciDevice->ChildCount != 0);

                PciDevice->Children[ChildIndex] =
                                PciDevice->Children[PciDevice->ChildCount - 1];

                MmFreePagedPool(PciDevice->ChildrenData[ChildIndex]);
                PciDevice->ChildrenData[ChildIndex] =
                            PciDevice->ChildrenData[PciDevice->ChildCount - 1];

                PciDevice->Children[PciDevice->ChildCount - 1] = NULL;
                PciDevice->ChildrenData[PciDevice->ChildCount - 1] = NULL;
                PciDevice->ChildCount -= 1;

            //
            // There was no child there before.
            //

            } else {

                //
                // If the vendor ID is invalid, skip this function.
                //

                if ((VendorId == 0) || (VendorId == PCI_INVALID_VENDOR_ID)) {
                    continue;
                }

                //
                // There's a child now where there didn't used to be, kick out
                // a new device. Start by getting an index where the child will
                // go in the array. This also allocates the new child structure.
                //

                ChildIndex = PcipGetNewChildIndex(PciDevice);
                if (ChildIndex == MAX_ULONG) {
                    continue;
                }

                //
                // Create the new device ID string. This only needs to be
                // temporary, which is why the buffer is stack allocated.
                //

                RtlPrintToString(NewDeviceId,
                                 PCI_DEVICE_ID_SIZE,
                                 CharacterEncodingDefault,
                                 PCI_DEVICE_ID_FORMAT,
                                 VendorId,
                                 DeviceId);

                //
                // Read the class code and create a string from it.
                //

                ClassCode = (ULONG)PciDevice->ReadConfig(PciDevice->BusNumber,
                                                         DeviceNumber,
                                                         Function,
                                                         PCI_CLASS_CODE_OFFSET,
                                                         sizeof(ULONG));

                ClassCode &= PCI_CLASS_CODE_MASK;
                ClassCodeString = PcipGetClassId(ClassCode);

                //
                // Create the driver context for the new child.
                //

                NewPciDevice = MmAllocateNonPagedPool(sizeof(PCI_DEVICE),
                                                      PCI_ALLOCATION_TAG);

                if (NewPciDevice == NULL) {
                    continue;
                }

                RtlZeroMemory(NewPciDevice, sizeof(PCI_DEVICE));
                NewPciDevice->Type = PciDeviceFunction;
                NewPciDevice->BusNumber = PciDevice->BusNumber;
                NewPciDevice->DeviceNumber = DeviceNumber;
                NewPciDevice->FunctionNumber = Function;
                NewPciDevice->ClassCode = ClassCode;
                if ((ClassCode == PCI_SUBTRACTIVE_BRIDGE_CLASS_CODE) ||
                    (ClassCode == PCI_BRIDGE_CLASS_CODE)) {

                    NewPciDevice->DeviceIsBridge = TRUE;
                }

                NewPciDevice->ReadConfig = PciDevice->ReadConfig;
                NewPciDevice->WriteConfig = PciDevice->WriteConfig;
                Status = PcipGetBusDriverDevice(Device,
                                                &(NewPciDevice->Parent));

                if (!KSUCCESS(Status)) {

                    ASSERT(FALSE);

                    MmFreeNonPagedPool(NewPciDevice);
                }

                //
                // Create the child device and fill out the accounting
                // structures.
                //

                Status = IoCreateDevice(PciDriver,
                                        NewPciDevice,
                                        Device,
                                        NewDeviceId,
                                        ClassCodeString,
                                        NULL,
                                        &(PciDevice->Children[ChildIndex]));

                if (!KSUCCESS(Status)) {
                    MmFreeNonPagedPool(NewPciDevice);
                    continue;
                }

                Child = PciDevice->ChildrenData[ChildIndex];
                Child->DeviceNumber = DeviceNumber;
                Child->Function = Function;
                Child->VendorId = VendorId;
                Child->DeviceId = DeviceId;
                PciDevice->ChildCount += 1;
                PcipCreateFunctionInterfaces(PciDevice->Children[ChildIndex],
                                             NewPciDevice);
            }
        }
    }

    return;
}

KSTATUS
PcipQueryResourceRequirements (
    PDEVICE Device,
    PPCI_DEVICE DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine determines the resource requirements of the given device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceObject - Supplies a pointer to the PCI information associated with
        the system device.

    Irp - Supplies a pointer to the query resources IRP.

Return Value:

    Status code.

--*/

{

    ULONGLONG AddressDecode;
    RESOURCE_ALLOCATION Allocation;
    ULONG BarIndex;
    ULONGLONG BarLength[PCI_BAR_COUNT];
    ULONG BitNumber;
    PRESOURCE_ALLOCATION_LIST BootAllocations;
    UCHAR Bus;
    PRESOURCE_CONFIGURATION_LIST ConfigurationList;
    USHORT ControlRegister;
    UCHAR DeviceNumber;
    UCHAR Function;
    ULONG InterruptPin;
    ULONGLONG Maximum;
    UCHAR Offset;
    PPCI_READ_CONFIG ReadConfig;
    RESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    KSTATUS Status;
    ULONG Value;
    PPCI_WRITE_CONFIG WriteConfig;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Bridges are not handled in this function.
    //

    ASSERT((DeviceObject->Type == PciDeviceFunction) &&
           (DeviceObject->DeviceIsBridge == FALSE));

    BootAllocations = NULL;
    Bus = DeviceObject->BusNumber;
    ConfigurationList = NULL;
    DeviceNumber = DeviceObject->DeviceNumber;
    Function = DeviceObject->FunctionNumber;
    ReadConfig = DeviceObject->ReadConfig;
    RequirementList = NULL;
    WriteConfig = DeviceObject->WriteConfig;

    //
    // If the BARs have not been read yet from boot, see if the BIOS has
    // this device enabled, and read the BARs if so.
    //

    if (DeviceObject->BarsRead == FALSE) {
        DeviceObject->BarsRead = TRUE;

        //
        // If decoding is enabled, read the BARs.
        //

        ControlRegister = (USHORT)ReadConfig(Bus,
                                             DeviceNumber,
                                             Function,
                                             PCI_CONTROL_OFFSET,
                                             sizeof(USHORT));

        DeviceObject->BootControlRegister = ControlRegister;
        if (((ControlRegister & PCI_CONTROL_IO_DECODE_ENABLED) != 0) ||
            ((ControlRegister & PCI_CONTROL_MEMORY_DECODE_ENABLED) != 0)) {

            for (BarIndex = 0; BarIndex < PCI_BAR_COUNT; BarIndex += 1) {
                Offset = PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG));
                Value = (ULONG)ReadConfig(Bus,
                                          DeviceNumber,
                                          Function,
                                          Offset,
                                          sizeof(ULONG));

                DeviceObject->BootConfiguration.U.Bar32[BarIndex] = Value;
            }
        }

        InterruptPin = (USHORT)ReadConfig(Bus,
                                          DeviceNumber,
                                          Function,
                                          PCI_INTERRUPT_LINE_OFFSET,
                                          sizeof(USHORT));

        InterruptPin = (UCHAR)(InterruptPin >> BITS_PER_BYTE);
        DeviceObject->InterruptPin = InterruptPin;
        if (DeviceObject->InterruptPin > 4) {

            ASSERT(FALSE);

            DeviceObject->InterruptPin = 0;
        }

        //
        // Disable all decoding in preparation for the BAR test.
        //

        WriteConfig(Bus,
                    DeviceNumber,
                    Function,
                    PCI_CONTROL_OFFSET,
                    sizeof(USHORT),
                    0);

        //
        // The technique to determine how many resources a device needs is to
        // write all ones to each BAR, and then read them back to see which ones
        // stick, and therefore which addresses the device decodes. Write all
        // ones here.
        //

        for (BarIndex = 0; BarIndex < PCI_BAR_COUNT; BarIndex += 1) {
            Offset = PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG));
            WriteConfig(Bus, DeviceNumber, Function, Offset, sizeof(ULONG), -1);
        }

        //
        // Now read them back.
        //

        for (BarIndex = 0; BarIndex < PCI_BAR_COUNT; BarIndex += 1) {
            Offset = PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG));
            Value = (ULONG)ReadConfig(Bus,
                                      DeviceNumber,
                                      Function,
                                      Offset,
                                      sizeof(ULONG));

            if (Value != 0) {
                DeviceObject->BarCount = BarIndex + 1;
            }

            DeviceObject->AddressDecodeBits.U.Bar32[BarIndex] = Value;
        }

        //
        // For the safest feeling possible, restore the BARs and control
        // register to what it was before.
        //

        for (BarIndex = 0; BarIndex < PCI_BAR_COUNT; BarIndex += 1) {
            Offset = PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG));
            Value = DeviceObject->BootConfiguration.U.Bar32[BarIndex];
            WriteConfig(Bus,
                        DeviceNumber,
                        Function,
                        Offset,
                        sizeof(ULONG),
                        Value);
        }

        WriteConfig(Bus,
                    DeviceNumber,
                    Function,
                    PCI_CONTROL_OFFSET,
                    sizeof(USHORT),
                    ControlRegister);
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));

    //
    // Create a new resource requirement list.
    //

    RequirementList = IoCreateResourceRequirementList();
    if (RequirementList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryResourceRequirementsEnd;
    }

    RtlZeroMemory(BarLength, sizeof(BarLength));

    //
    // Loop through the BARs to determine the resource requirements.
    //

    for (BarIndex = 0; BarIndex < DeviceObject->BarCount; BarIndex += 1) {
        Value = DeviceObject->AddressDecodeBits.U.Bar32[BarIndex];

        //
        // Create an I/O or memory space requirement.
        //

        if ((Value & PCI_BAR_IO_SPACE) != 0) {
            Requirement.Type = ResourceTypeIoPort;
            Requirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;

            //
            // The value has 1 bit set for everything it decodes. Start from the
            // high order bits and find the maximum that it does decode.
            //

            if ((Value & (~PCI_BAR_IO_FLAGS_MASK)) == 0) {
                Maximum = 0;
                AddressDecode = 0;

            } else {
                BitNumber = 31;
                Maximum = 1 << BitNumber;
                while ((Maximum & Value) == 0) {
                    BitNumber -= 1;
                    Maximum = 1 << BitNumber;
                }

                //
                // Back up a smidge, the loop went one too far.
                //

                BitNumber += 1;
                Maximum = 1ULL << BitNumber;

                //
                // To get the needed size, OR in the empty bits on the right
                // (so the whole thing eventually rolls over), then mask off
                // the flags bits so they would be zero. Negate the whole
                // thing, and add 1 to roll over to a power of 2 that
                // represents the required size.
                //

                AddressDecode =
                    (~((Value | ~(Maximum - 1)) & ~PCI_BAR_IO_FLAGS_MASK)) + 1;
            }

            Requirement.Minimum = 0;
            Requirement.Maximum = Maximum;
            Requirement.Length = AddressDecode;
            Requirement.Alignment = AddressDecode;
            Requirement.Characteristics = 0;
            if ((Value & PCI_BAR_MEMORY_PREFETCHABLE) != 0) {
                Requirement.Characteristics |=
                                            MEMORY_CHARACTERISTIC_PREFETCHABLE;
            }

            BarLength[BarIndex] = AddressDecode;

        //
        // Create a memory space requirement.
        //

        } else {
            Requirement.Type = ResourceTypePhysicalAddressSpace;
            Requirement.Flags |= RESOURCE_FLAG_NOT_SHAREABLE;
            AddressDecode = Value & (~PCI_BAR_MEMORY_FLAGS_MASK);
            Requirement.Minimum = 0;

            //
            // Set the minimum and maximum based on the BAR limits.
            //

            switch (Value & PCI_BAR_MEMORY_SIZE_MASK) {
            case PCI_BAR_MEMORY_32_BIT:
                BitNumber = 31;
                break;

            case PCI_BAR_MEMORY_1MB:
                BitNumber = 20;
                break;

            case PCI_BAR_MEMORY_64_BIT:

                ASSERT((BarIndex & 0x1) == 0);

                AddressDecode =
                        DeviceObject->AddressDecodeBits.U.Bar64[BarIndex / 2] &
                        (~PCI_BAR_MEMORY_FLAGS_MASK);

                BitNumber = 63;
                break;

            default:

                ASSERT(FALSE);

                BitNumber = 0;
                break;
            }

            //
            // Just like the I/O bars above, find the unset bits on the left
            // to get the maximum address.
            //

            if ((AddressDecode & (~PCI_BAR_MEMORY_FLAGS_MASK)) == 0) {
                Maximum = 0;
                AddressDecode = 0;

            } else {
                Maximum = 1ULL << BitNumber;
                while ((Maximum & AddressDecode) == 0) {
                    BitNumber -= 1;
                    Maximum = 1ULL << BitNumber;
                }

                //
                // Back up a smidge, the loop went too far.
                //

                BitNumber += 1;
                Maximum = 1ULL << BitNumber;

                //
                // Get the size needed for this BAR. Again this is done by
                // ORing in the unsupported bits on the left masking out the
                // flags, negating the whole thing, and adding one. Remember
                // that the flags were masked out already above.
                //

                if (BitNumber == 64) {
                    AddressDecode = ~AddressDecode + 1;
                    Maximum = MAX_ULONGLONG;

                } else {
                    AddressDecode = (~(AddressDecode | ~(Maximum - 1))) + 1;
                }
            }

            Requirement.Length = AddressDecode;
            Requirement.Alignment = AddressDecode;
            Requirement.Maximum = Maximum;
            Requirement.Characteristics = 0;
            BarLength[BarIndex] = AddressDecode;

            //
            // 64 bit BARs take up two of the regular size BARs, so advance
            // past the second one.
            //

            if ((Value & PCI_BAR_MEMORY_SIZE_MASK) == PCI_BAR_MEMORY_64_BIT) {
                BarIndex += 1;
            }
        }

        //
        // Create and add the requirement to the list.
        //

        Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    //
    // If the interrupt pin is not zero, then request an interrupt line
    // resource as well. By default PCI interrupts are level triggered active
    // low, and shareable.
    //

    InterruptPin = DeviceObject->InterruptPin;
    if (InterruptPin != 0) {
        Requirement.Type = ResourceTypeInterruptLine;
        Requirement.Flags &= ~RESOURCE_FLAG_NOT_SHAREABLE;
        Requirement.Length = 1;
        Requirement.Characteristics = INTERRUPT_LINE_ACTIVE_LOW;
        Requirement.Flags = 0;
        Requirement.Alignment = 1;
        Requirement.Minimum = InterruptPin;
        Requirement.Maximum = InterruptPin + 1;
        Status = IoCreateAndAddResourceRequirement(&Requirement,
                                                   RequirementList,
                                                   NULL);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    //
    // Create the resource configuration list.
    //

    ConfigurationList = IoCreateResourceConfigurationList(RequirementList);
    if (ConfigurationList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryResourceRequirementsEnd;
    }

    RequirementList = NULL;

    //
    // Create the boot configuration.
    //

    BootAllocations = IoCreateResourceAllocationList();
    if (BootAllocations == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryResourceRequirementsEnd;
    }

    RtlZeroMemory(&Allocation, sizeof(RESOURCE_ALLOCATION));
    for (BarIndex = 0; BarIndex < DeviceObject->BarCount; BarIndex += 1) {
        Value = DeviceObject->BootConfiguration.U.Bar32[BarIndex];

        //
        // Create an I/O or memory space allocation.
        //

        if ((Value & PCI_BAR_IO_SPACE) != 0) {
            Allocation.Type = ResourceTypeIoPort;
            Allocation.Allocation = Value & (~PCI_BAR_IO_FLAGS_MASK);
            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Allocation.Length = 0;
            if ((DeviceObject->BootControlRegister &
                 PCI_CONTROL_IO_DECODE_ENABLED) != 0) {

                Allocation.Length = BarLength[BarIndex];
            }

        //
        // Create a memory space allocation.
        //

        } else {
            Allocation.Type = ResourceTypePhysicalAddressSpace;
            Allocation.Allocation = Value & (~PCI_BAR_MEMORY_FLAGS_MASK);
            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Allocation.Length = 0;
            if ((DeviceObject->BootControlRegister &
                 PCI_CONTROL_MEMORY_DECODE_ENABLED) != 0) {

                Allocation.Length = BarLength[BarIndex];
                if ((Value & PCI_BAR_MEMORY_SIZE_MASK) ==
                    PCI_BAR_MEMORY_64_BIT) {

                    ASSERT((BarIndex & 0x1) == 0);

                    Allocation.Allocation =
                        DeviceObject->BootConfiguration.U.Bar64[BarIndex / 2] &
                        (~PCI_BAR_MEMORY_FLAGS_MASK);

                    BarIndex += 1;
                }
            }
        }

        //
        // Create and add the allocation to the list.
        //

        Status = IoCreateAndAddResourceAllocation(&Allocation, BootAllocations);
        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    //
    // Add the interrupt pin to the boot configuration.
    //

    if (InterruptPin != 0) {
        Allocation.Type = ResourceTypeInterruptLine;
        Allocation.Allocation = InterruptPin;
        Allocation.Length = 1;
        Allocation.Flags = 0;
        Allocation.Characteristics = INTERRUPT_LINE_ACTIVE_LOW;
        Status = IoCreateAndAddResourceAllocation(&Allocation, BootAllocations);
        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    Status = STATUS_SUCCESS;

QueryResourceRequirementsEnd:
    if (!KSUCCESS(Status)) {
        if (RequirementList != NULL) {
            IoDestroyResourceRequirementList(RequirementList);
        }

        if (ConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(ConfigurationList);
            ConfigurationList = NULL;
        }

        if (BootAllocations != NULL) {
            IoDestroyResourceAllocationList(BootAllocations);
            BootAllocations = NULL;
        }
    }

    Irp->U.QueryResources.ResourceRequirements = ConfigurationList;
    Irp->U.QueryResources.BootAllocation = BootAllocations;
    return Status;
}

KSTATUS
PcipQueryBridgeResourceRequirements (
    PDEVICE Device,
    PPCI_DEVICE DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine determines the resource requirements of the given PCI bridge.
    For the confused, this routine is called by PCI acting as the bus driver,
    not the function driver.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceObject - Supplies a pointer to the PCI information associated with
        the system device.

    Irp - Supplies a pointer to the query resources IRP.

Return Value:

    Status code.

--*/

{

    RESOURCE_ALLOCATION Allocation;
    BOOL BarsRead;
    PRESOURCE_ALLOCATION_LIST BootAllocations;
    UCHAR Bus;
    PRESOURCE_CONFIGURATION_LIST ConfigurationList;
    UCHAR DeviceNumber;
    UCHAR Function;
    UCHAR InterruptPin;
    ULONG IoDecodeBase;
    ULONG IoDecodeLimit;
    BOOL IoDecodeUpperBaseValid;
    BOOL IoDecodeUpperLimitValid;
    ULONG MemoryDecodeBase;
    ULONG MemoryDecodeLimit;
    ULONGLONG PrefetchableMemoryDecodeBase;
    ULONGLONG PrefetchableMemoryDecodeLimit;
    BOOL PrefetchableMemoryUpperBaseValid;
    BOOL PrefetchableMemoryUpperLimitValid;
    PPCI_READ_CONFIG ReadConfig;
    RESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    UCHAR SecondaryBusNumber;
    KSTATUS Status;
    ULONG Value;
    ULONG ValueHigh;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Only bridges are handled in this function.
    //

    ASSERT((DeviceObject->Type == PciDeviceFunction) &&
           (DeviceObject->DeviceIsBridge != FALSE));

    BarsRead = FALSE;
    BootAllocations = NULL;
    Bus = DeviceObject->BusNumber;
    ConfigurationList = NULL;
    DeviceNumber = DeviceObject->DeviceNumber;
    Function = DeviceObject->FunctionNumber;
    IoDecodeBase = MAX_USHORT;
    IoDecodeLimit = 0;
    MemoryDecodeBase = MAX_ULONG;
    MemoryDecodeLimit = 0;
    PrefetchableMemoryDecodeBase = MAX_ULONGLONG;
    PrefetchableMemoryDecodeLimit = 0;
    ReadConfig = DeviceObject->ReadConfig;
    RequirementList = NULL;
    SecondaryBusNumber = 0xFF;

    //
    // If the BARs have not been read yet from boot, see if the BIOS has
    // this device enabled, and read the BARs if so.
    //

    if (DeviceObject->BarsRead == FALSE) {
        DeviceObject->BarsRead = TRUE;
        BarsRead = TRUE;

        //
        // Read the bus number BAR to see how the BIOS configured it.
        //

        Value = (ULONG)ReadConfig(Bus,
                                  DeviceNumber,
                                  Function,
                                  PCI_BRIDGE_BUS_NUMBERS_OFFSET,
                                  sizeof(ULONG));

        SecondaryBusNumber = (UCHAR)(Value >> PCI_BRIDGE_SECONDARY_BUS_SHIFT);

        //
        // Read the value set by the BIOS for the I/O decode region.
        //

        IoDecodeUpperBaseValid = FALSE;
        IoDecodeUpperLimitValid = FALSE;
        Value = (USHORT)ReadConfig(Bus,
                                   DeviceNumber,
                                   Function,
                                   PCI_BRIDGE_IO_BAR_OFFSET,
                                   sizeof(USHORT));

        if ((Value & PCI_BRIDGE_IO_BASE_DECODE_MASK) ==
            PCI_BRIDGE_IO_BASE_DECODE_32_BIT) {

            IoDecodeUpperBaseValid = TRUE;
        }

        if ((Value & PCI_BRIDGE_IO_LIMIT_DECODE_MASK) ==
            PCI_BRIDGE_IO_LIMIT_DECODE_32_BIT) {

            IoDecodeUpperLimitValid = TRUE;
        }

        IoDecodeBase = (Value & PCI_BRIDGE_IO_BASE_MASK) <<
                       PCI_BRIDGE_IO_BASE_ADDRESS_SHIFT;

        IoDecodeLimit = Value & PCI_BRIDGE_IO_LIMIT_MASK;
        if ((IoDecodeUpperBaseValid != FALSE) ||
            (IoDecodeUpperLimitValid != FALSE)) {

            ValueHigh = (USHORT)ReadConfig(Bus,
                                           DeviceNumber,
                                           Function,
                                           PCI_BRIDGE_IO_HIGH_BAR_OFFSET,
                                           sizeof(ULONG));

            if (IoDecodeUpperBaseValid != FALSE) {
                IoDecodeBase |= (ValueHigh &
                                 PCI_BRIDGE_IO_BASE_HIGH_MASK) <<
                                PCI_BRIDGE_IO_BASE_HIGH_ADDRESS_SHIFT;
            }

            if (IoDecodeUpperLimitValid != FALSE) {
                IoDecodeLimit |= ValueHigh & PCI_BRIDGE_IO_LIMIT_HIGH_MASK;
            }
        }

        //
        // Read the value set by the BIOS for the memory decode region.
        //

        Value = (ULONG)ReadConfig(Bus,
                                  DeviceNumber,
                                  Function,
                                  PCI_BRIDGE_MEMORY_BAR_OFFSET,
                                  sizeof(ULONG));

        MemoryDecodeBase = (Value & PCI_BRIDGE_MEMORY_BASE_MASK) <<
                           PCI_BRIDGE_MEMORY_BASE_ADDRESS_SHIFT;

        MemoryDecodeLimit = Value & PCI_BRIDGE_MEMORY_LIMIT_MASK;

        //
        // Read the prefetchable memory range as well.
        //

        PrefetchableMemoryUpperBaseValid = FALSE;
        PrefetchableMemoryUpperLimitValid = FALSE;
        Value = (ULONG)ReadConfig(Bus,
                                  DeviceNumber,
                                  Function,
                                  PCI_BRIDGE_PREFETCHABLE_MEMORY_BAR_OFFSET,
                                  sizeof(ULONG));

        if ((Value & PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_DECODE_MASK) ==
            PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_DECODE_64_BIT) {

            PrefetchableMemoryUpperBaseValid = TRUE;
        }

        if ((Value & PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_DECODE_MASK) ==
            PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_DECODE_64_BIT) {

            PrefetchableMemoryUpperLimitValid = TRUE;
        }

        PrefetchableMemoryDecodeBase =
                      (Value & PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_MASK) <<
                      PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_ADDRESS_SHIFT;

        PrefetchableMemoryDecodeLimit =
                         Value & PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_MASK;

        if (PrefetchableMemoryUpperBaseValid != FALSE) {
            ValueHigh = (ULONG)ReadConfig(
                           Bus,
                           DeviceNumber,
                           Function,
                           PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_HIGH_OFFSET,
                           sizeof(ULONG));

            PrefetchableMemoryDecodeBase |=
                         (ULONGLONG)ValueHigh <<
                         PCI_BRIDGE_PREFETCHABLE_MEMORY_HIGH_ADDRESS_SHIFT;
        }

        if (PrefetchableMemoryUpperLimitValid != FALSE) {
            ValueHigh = (ULONG)ReadConfig(
                          Bus,
                          DeviceNumber,
                          Function,
                          PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_HIGH_OFFSET,
                          sizeof(ULONG));

            PrefetchableMemoryDecodeLimit |=
                         (ULONGLONG)ValueHigh <<
                         PCI_BRIDGE_PREFETCHABLE_MEMORY_HIGH_ADDRESS_SHIFT;
        }

        InterruptPin = (USHORT)ReadConfig(Bus,
                                          DeviceNumber,
                                          Function,
                                          PCI_INTERRUPT_LINE_OFFSET,
                                          sizeof(USHORT));

        InterruptPin = (UCHAR)(InterruptPin >> BITS_PER_BYTE);
        DeviceObject->InterruptPin = InterruptPin;
        if (DeviceObject->InterruptPin > 4) {

            ASSERT(FALSE);

            DeviceObject->InterruptPin = 0;
        }
    }

    RtlZeroMemory(&Requirement, sizeof(RESOURCE_REQUIREMENT));

    //
    // Create a new resource requirement list and add the bus number
    // requirement.
    //

    RequirementList = IoCreateResourceRequirementList();
    if (RequirementList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryBridgeResourceRequirementsEnd;
    }

    Requirement.Type = ResourceTypeBusNumber;
    Requirement.Minimum = 0;
    Requirement.Maximum = (UCHAR)-1;
    Requirement.Length = 1;
    Requirement.Characteristics = 0;
    Requirement.Alignment = 0;
    Requirement.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto QueryBridgeResourceRequirementsEnd;
    }

    //
    // Add empty requirements for the windows.
    //

    Requirement.Type = ResourceTypeIoPort;
    Requirement.Minimum = 0;
    Requirement.Maximum = MAX_ULONG;
    Requirement.Length = 0;
    Requirement.Characteristics = 0;
    Requirement.Alignment = PCI_BRIDGE_IO_GRANULARITY;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto QueryBridgeResourceRequirementsEnd;
    }

    Requirement.Type = ResourceTypePhysicalAddressSpace;
    Requirement.Minimum = 0;
    Requirement.Maximum = MAX_ULONG;
    Requirement.Length = 0;
    Requirement.Characteristics = 0;
    Requirement.Alignment = PCI_BRIDGE_MEMORY_GRANULARITY;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto QueryBridgeResourceRequirementsEnd;
    }

    //
    // The prefetchable memory window is the same as the MMIO region, but is
    // 64-bit capable.
    //

    Requirement.Maximum = MAX_ULONGLONG;
    Requirement.Characteristics = MEMORY_CHARACTERISTIC_PREFETCHABLE;
    Status = IoCreateAndAddResourceRequirement(&Requirement,
                                               RequirementList,
                                               NULL);

    if (!KSUCCESS(Status)) {
        goto QueryBridgeResourceRequirementsEnd;
    }

    //
    // Create the resource configuration list.
    //

    ConfigurationList = IoCreateResourceConfigurationList(RequirementList);
    if (ConfigurationList == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryBridgeResourceRequirementsEnd;
    }

    RequirementList = NULL;

    //
    // Create the boot configuration.
    //

    BootAllocations = IoCreateResourceAllocationList();
    if (BootAllocations == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryBridgeResourceRequirementsEnd;
    }

    RtlZeroMemory(&Allocation, sizeof(RESOURCE_ALLOCATION));
    if (BarsRead != FALSE) {
        if (SecondaryBusNumber != 0xFF) {
            Allocation.Type = ResourceTypeBusNumber;
            Allocation.Allocation = SecondaryBusNumber;
            Allocation.Length = 1;
            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Status = IoCreateAndAddResourceAllocation(&Allocation,
                                                      BootAllocations);

            if (!KSUCCESS(Status)) {
                goto QueryBridgeResourceRequirementsEnd;
            }

            Allocation.Type = ResourceTypeIoPort;
            Allocation.Allocation = IoDecodeBase;
            if (IoDecodeLimit >= IoDecodeBase) {
                Allocation.Length =
                    (IoDecodeLimit + PCI_BRIDGE_IO_GRANULARITY) - IoDecodeBase;

            } else {
                Allocation.Length = 0;
            }

            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Status = IoCreateAndAddResourceAllocation(&Allocation,
                                                      BootAllocations);

            if (!KSUCCESS(Status)) {
                goto QueryBridgeResourceRequirementsEnd;
            }

            Allocation.Type = ResourceTypePhysicalAddressSpace;
            Allocation.Allocation = MemoryDecodeBase;
            if (MemoryDecodeLimit >= MemoryDecodeBase) {
                Allocation.Length = (MemoryDecodeLimit +
                                     PCI_BRIDGE_MEMORY_GRANULARITY) -
                                    MemoryDecodeBase;

            } else {
                Allocation.Length = 0;
            }

            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Status = IoCreateAndAddResourceAllocation(&Allocation,
                                                      BootAllocations);

            if (!KSUCCESS(Status)) {
                goto QueryBridgeResourceRequirementsEnd;
            }

            Allocation.Type = ResourceTypePhysicalAddressSpace;
            Allocation.Allocation = PrefetchableMemoryDecodeBase;
            if (PrefetchableMemoryDecodeLimit >= PrefetchableMemoryDecodeBase) {
                Allocation.Length = (PrefetchableMemoryDecodeLimit +
                                     PCI_BRIDGE_MEMORY_GRANULARITY) -
                                    PrefetchableMemoryDecodeBase;

            } else {
                Allocation.Length = 0;
            }

            Allocation.Characteristics = MEMORY_CHARACTERISTIC_PREFETCHABLE;
            Allocation.Flags = RESOURCE_FLAG_NOT_SHAREABLE;
            Status = IoCreateAndAddResourceAllocation(&Allocation,
                                                      BootAllocations);

            if (!KSUCCESS(Status)) {
                goto QueryBridgeResourceRequirementsEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

QueryBridgeResourceRequirementsEnd:
    if (!KSUCCESS(Status)) {
        if (RequirementList != NULL) {
            IoDestroyResourceRequirementList(RequirementList);
        }

        if (ConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(ConfigurationList);
            ConfigurationList = NULL;
        }

        if (BootAllocations != NULL) {
            IoDestroyResourceAllocationList(BootAllocations);
            BootAllocations = NULL;
        }
    }

    Irp->U.QueryResources.ResourceRequirements = ConfigurationList;
    Irp->U.QueryResources.BootAllocation = BootAllocations;
    return Status;
}

KSTATUS
PcipSetDeviceResources (
    PPCI_DEVICE DeviceContext,
    PRESOURCE_ALLOCATION_LIST AllocationList
    )

/*++

Routine Description:

    This routine sets the assigned resources in the PCI BARs.

Arguments:

    DeviceContext - Supplies a pointer to the device to set.

    AllocationList - Supplies a pointer to the resource allocation list
        containing the device's resource assignment.

Return Value:

    Status code.

--*/

{

    ULONGLONG AddressDecode;
    PRESOURCE_ALLOCATION Allocation;
    ULONG BarIndex;
    ULONG BarSize;
    UCHAR Bus;
    USHORT ControlRegister;
    UCHAR DeviceNumber;
    UCHAR Function;
    PPCI_MSI_CONTEXT MsiContext;
    UCHAR Offset;
    ULONG PendingArrayIndex;
    ULONG PendingArrayOffset;
    PPCI_READ_CONFIG ReadConfig;
    RESOURCE_TYPE ResourceType;
    KSTATUS Status;
    ULONGLONG Value;
    ULONG VectorTableIndex;
    ULONG VectorTableOffset;
    PPCI_WRITE_CONFIG WriteConfig;

    //
    // This routine only handles functions, not bridges.
    //

    ASSERT((DeviceContext->Type == PciDeviceFunction) &&
           (DeviceContext->DeviceIsBridge == FALSE));

    if (AllocationList == NULL) {
        Status = STATUS_SUCCESS;
        goto SetDeviceResourcesEnd;
    }

    Bus = DeviceContext->BusNumber;
    DeviceNumber = DeviceContext->DeviceNumber;
    Function = DeviceContext->FunctionNumber;
    ReadConfig = DeviceContext->ReadConfig;
    WriteConfig = DeviceContext->WriteConfig;

    //
    // If MSI-X is available on the device then prepare to squirrel away the
    // physical address of the table and pending array.
    //

    MsiContext = DeviceContext->MsiContext;
    if ((MsiContext != NULL) && (MsiContext->MsiXOffset != 0)) {
        PcipGetMsiXBarInformation(DeviceContext,
                                  &VectorTableIndex,
                                  &VectorTableOffset,
                                  &PendingArrayIndex,
                                  &PendingArrayOffset);
    }

    //
    // Read the control register.
    //

    ControlRegister = (USHORT)ReadConfig(Bus,
                                         DeviceNumber,
                                         Function,
                                         PCI_CONTROL_OFFSET,
                                         sizeof(USHORT));

    //
    // Disable all decoding in preparation for setting the BARs.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_CONTROL_OFFSET,
                sizeof(USHORT),
                0);

    //
    // Loop through the BARs and assign resources to each one.
    //

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    for (BarIndex = 0; BarIndex < DeviceContext->BarCount; BarIndex += 1) {
        AddressDecode = DeviceContext->AddressDecodeBits.U.Bar32[BarIndex];

        //
        // Get the resource type for this BAR.
        //

        ResourceType = ResourceTypePhysicalAddressSpace;
        if ((AddressDecode & PCI_BAR_IO_SPACE) != 0) {
            ResourceType = ResourceTypeIoPort;
        }

        //
        // Find the next resource of that type.
        //

        while (Allocation->Type != ResourceType) {
            Allocation =
                       IoGetNextResourceAllocation(AllocationList, Allocation);

            if (Allocation == NULL) {
                Status = STATUS_INVALID_CONFIGURATION;
                goto SetDeviceResourcesEnd;
            }
        }

        //
        // Skip it if it's zero length.
        //

        if (AddressDecode == 0) {

            ASSERT(Allocation->Length == 0);

            Allocation = IoGetNextResourceAllocation(AllocationList,
                                                     Allocation);

            continue;
        }

        //
        // See if this is a 64 bit bar.
        //

        BarSize = sizeof(ULONG);
        if ((ResourceType == ResourceTypePhysicalAddressSpace) &&
            ((AddressDecode & PCI_BAR_MEMORY_SIZE_MASK) ==
             PCI_BAR_MEMORY_64_BIT)) {

            BarSize = sizeof(ULONGLONG);
        }

        Value = Allocation->Allocation;
        if (ResourceType == ResourceTypePhysicalAddressSpace) {

            ASSERT((Value & PCI_BAR_MEMORY_FLAGS_MASK) == 0);

            ControlRegister |= PCI_CONTROL_MEMORY_DECODE_ENABLED;

        } else {

            ASSERT(ResourceType == ResourceTypeIoPort);
            ASSERT((Value & PCI_BAR_IO_FLAGS_MASK) == 0);

            ControlRegister |= PCI_CONTROL_IO_DECODE_ENABLED;
            Value |= PCI_BAR_IO_SPACE;
        }

        //
        // Write out the BAR.
        //

        Offset = PCI_BAR_OFFSET + (BarIndex * sizeof(ULONG));
        WriteConfig(Bus, DeviceNumber, Function, Offset, BarSize, Value);

        //
        // If MSI-X is available then check to see if this is the BAR for
        // either the vector table or pending bit array. They could be in the
        // same BAR.
        //

        if ((MsiContext != NULL) && (MsiContext->MsiXOffset != 0)) {
            if (VectorTableIndex == BarIndex) {

                ASSERT(MsiContext->MsiXTablePhysicalAddress ==
                       INVALID_PHYSICAL_ADDRESS);

                MsiContext->MsiXTablePhysicalAddress = Value +
                                                       VectorTableOffset;
            }

            if (PendingArrayIndex == BarIndex) {

                ASSERT(MsiContext->MsiXPendingArrayPhysicalAddress ==
                       INVALID_PHYSICAL_ADDRESS);

                MsiContext->MsiXPendingArrayPhysicalAddress =
                                                    Value + PendingArrayOffset;
            }
        }

        //
        // Skip over the next BAR if this one was a 64-bit BAR.
        //

        if ((ResourceType == ResourceTypePhysicalAddressSpace) &&
            ((AddressDecode & PCI_BAR_MEMORY_SIZE_MASK) ==
             PCI_BAR_MEMORY_64_BIT)) {

            BarIndex += 1;
        }

        //
        // Move on to the next allocation.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Write out the control register to enable the device.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_CONTROL_OFFSET,
                sizeof(USHORT),
                ControlRegister);

    Status = STATUS_SUCCESS;

SetDeviceResourcesEnd:
    return Status;
}

VOID
PcipEnableDevice (
    PPCI_DEVICE DeviceContext
    )

/*++

Routine Description:

    This routine enables the I/O space, memory space, and Bus master bits in
    the PCI device.

Arguments:

    DeviceContext - Supplies a pointer to the device to set.

Return Value:

    None.

--*/

{

    UCHAR Bus;
    USHORT CommandRegister;
    UCHAR DeviceNumber;
    UCHAR Function;
    PPCI_READ_CONFIG ReadConfig;
    PPCI_WRITE_CONFIG WriteConfig;

    //
    // This routine only handles functions, not bridges.
    //

    ASSERT((DeviceContext->Type == PciDeviceFunction) &&
           (DeviceContext->DeviceIsBridge == FALSE));

    Bus = DeviceContext->BusNumber;
    DeviceNumber = DeviceContext->DeviceNumber;
    Function = DeviceContext->FunctionNumber;
    ReadConfig = DeviceContext->ReadConfig;
    WriteConfig = DeviceContext->WriteConfig;

    //
    // Read the command register, and enable some bits.
    //

    CommandRegister = (USHORT)ReadConfig(Bus,
                                         DeviceNumber,
                                         Function,
                                         PCI_CONTROL_OFFSET,
                                         sizeof(USHORT));

    CommandRegister |= PCI_CONTROL_IO_DECODE_ENABLED |
                       PCI_CONTROL_MEMORY_DECODE_ENABLED |
                       PCI_CONTROL_WRITE_INVALIDATE_ENABLED |
                       PCI_CONTROL_BUS_MASTER_ENABLED;

    //
    // Disable all decoding in preparation for setting the BARs.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_CONTROL_OFFSET,
                sizeof(USHORT),
                CommandRegister);

    return;
}

KSTATUS
PcipSetBridgeDeviceResources (
    PPCI_DEVICE DeviceContext,
    PRESOURCE_ALLOCATION_LIST AllocationList
    )

/*++

Routine Description:

    This routine sets the assigned resource window into the given bridge.

Arguments:

    DeviceContext - Supplies a pointer to the device to set.

    AllocationList - Supplies a pointer to the resource allocation list
        containing the device's resource assignment.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    UCHAR Bus;
    ULONG BusRegister;
    USHORT ControlRegister;
    UCHAR DeviceNumber;
    UCHAR Function;
    ULONG IoPortHigh;
    USHORT IoPortRegister;
    ULONGLONG Limit;
    ULONG MemoryRegister;
    UCHAR OriginalSecondaryBusNumber;
    ULONG PrefetchableMemoryBaseHigh;
    ULONG PrefetchableMemoryLimitHigh;
    ULONG PrefetchableMemoryLow;
    UCHAR PrimaryBusNumber;
    PPCI_READ_CONFIG ReadConfig;
    UCHAR SecondaryBusNumber;
    KSTATUS Status;
    UCHAR SubordinateBusNumber;
    PPCI_WRITE_CONFIG WriteConfig;

    //
    // This routine only handles bridges.
    //

    ASSERT((DeviceContext->Type == PciDeviceFunction) &&
           (DeviceContext->DeviceIsBridge != FALSE));

    if (AllocationList == NULL) {
        Status = STATUS_SUCCESS;
        goto SetBridgeDeviceResourcesEnd;
    }

    //
    // Initialize the locals. Set the window registers up so that the base is
    // higher than the limit, a safe default if no resources were given for that
    // window.
    //

    Bus = DeviceContext->BusNumber;
    DeviceNumber = DeviceContext->DeviceNumber;
    Function = DeviceContext->FunctionNumber;
    IoPortRegister = ((MAX_USHORT >> PCI_BRIDGE_IO_BASE_ADDRESS_SHIFT) &
                      PCI_BRIDGE_IO_BASE_MASK) |
                     (0 & PCI_BRIDGE_IO_LIMIT_MASK);

    IoPortHigh = 0;
    MemoryRegister = ((MAX_ULONG >> PCI_BRIDGE_MEMORY_BASE_ADDRESS_SHIFT) &
                      PCI_BRIDGE_MEMORY_BASE_MASK) |
                     (0 & PCI_BRIDGE_MEMORY_LIMIT_MASK);

    PrimaryBusNumber = Bus;
    PrefetchableMemoryLow =
                        ((MAX_ULONG >> PCI_BRIDGE_MEMORY_BASE_ADDRESS_SHIFT) &
                         PCI_BRIDGE_MEMORY_BASE_MASK) |
                        (0 & PCI_BRIDGE_MEMORY_LIMIT_MASK);

    PrefetchableMemoryBaseHigh = MAX_ULONG;
    PrefetchableMemoryLimitHigh = 0;
    SecondaryBusNumber = Bus;
    ReadConfig = DeviceContext->ReadConfig;
    WriteConfig = DeviceContext->WriteConfig;
    BusRegister = (ULONG)ReadConfig(Bus,
                                    DeviceNumber,
                                    Function,
                                    PCI_BRIDGE_BUS_NUMBERS_OFFSET,
                                    sizeof(ULONG));

    //
    // Save the secondary and subordinate bus numbers that were programmed by
    // the firmware. The final secondary bus number will be retrieved from the
    // allocated resources; they should match. The subordinate bus number is
    // the highest bus number underneath this bridge and all bus numbers
    // beneath a given bridge must be contiguous. A depth-first search would
    // need to be performed before the system enumerates the bridges in order
    // to correctly calculate the subordinate bus numbers. For now, rely on the
    // firmware to have done the work.
    //

    OriginalSecondaryBusNumber = (BusRegister &
                                  PCI_BRIDGE_SECONDARY_BUS_MASK) >>
                                 PCI_BRIDGE_SECONDARY_BUS_SHIFT;

    SubordinateBusNumber = (BusRegister & PCI_BRIDGE_SUBORDINATE_BUS_MASK) >>
                           PCI_BRIDGE_SUBORDINATE_BUS_SHIFT;

    //
    // Read the control register.
    //

    ControlRegister = (USHORT)ReadConfig(Bus,
                                         DeviceNumber,
                                         Function,
                                         PCI_CONTROL_OFFSET,
                                         sizeof(USHORT));

    ControlRegister |= PCI_CONTROL_BUS_MASTER_ENABLED |
                       PCI_CONTROL_SPECIAL_CYCLES_ENABLED |
                       PCI_CONTROL_WRITE_INVALIDATE_ENABLED |
                       PCI_CONTROL_SERR_ENABLED;

    //
    // Disable all decoding in preparation for setting the BARs.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_CONTROL_OFFSET,
                sizeof(USHORT),
                0);

    //
    // Loop over all the given resources, and extract the necessary items.
    // Don't program anything in until everything's retrieved.
    //

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // Skip zero length allocations.
        //

        if (Allocation->Length == 0) {
            Allocation = IoGetNextResourceAllocation(AllocationList,
                                                     Allocation);

            continue;
        }

        //
        // Save the bus number.
        //

        if (Allocation->Type == ResourceTypeBusNumber) {

            ASSERT((UCHAR)Allocation->Allocation == Allocation->Allocation);
            ASSERT(Allocation->Length == 1);

            BusRegister &= PCI_BRIDGE_SECONDARY_LATENCY_TIMER_MASK;
            SecondaryBusNumber = (UCHAR)Allocation->Allocation;

        //
        // Save the I/O port window.
        //

        } else if (Allocation->Type == ResourceTypeIoPort) {
            ControlRegister |= PCI_CONTROL_IO_DECODE_ENABLED;
            Limit = Allocation->Allocation + Allocation->Length -
                    PCI_BRIDGE_IO_GRANULARITY;

            IoPortRegister = (USHORT)(((Allocation->Allocation >>
                                        PCI_BRIDGE_IO_BASE_ADDRESS_SHIFT) &
                                       PCI_BRIDGE_IO_BASE_MASK) |
                                      (Limit & PCI_BRIDGE_IO_LIMIT_MASK));

            IoPortHigh = (ULONG)((Allocation->Allocation >>
                                  PCI_BRIDGE_IO_BASE_HIGH_ADDRESS_SHIFT) &
                                 PCI_BRIDGE_IO_BASE_HIGH_MASK);

            if (IoPortHigh != 0) {
                IoPortRegister |= PCI_BRIDGE_IO_BASE_DECODE_32_BIT;
            }

            if ((ULONG)(Limit & PCI_BRIDGE_IO_LIMIT_HIGH_MASK) != 0) {
                IoPortRegister |= PCI_BRIDGE_IO_LIMIT_DECODE_32_BIT;
            }

            IoPortHigh |= (ULONG)(Limit & PCI_BRIDGE_IO_LIMIT_HIGH_MASK);

        //
        // Save the non-prefetchable (MMIO) memory window.
        //

        } else if ((Allocation->Type == ResourceTypePhysicalAddressSpace) &&
                   ((Allocation->Characteristics &
                     MEMORY_CHARACTERISTIC_PREFETCHABLE) == 0)) {

            ControlRegister |= PCI_CONTROL_MEMORY_DECODE_ENABLED;
            Limit = Allocation->Allocation + Allocation->Length -
                    PCI_BRIDGE_MEMORY_GRANULARITY;

            MemoryRegister = (ULONG)(((Allocation->Allocation >>
                                       PCI_BRIDGE_MEMORY_BASE_ADDRESS_SHIFT) &
                                      PCI_BRIDGE_MEMORY_BASE_MASK) |
                                     (Limit & PCI_BRIDGE_MEMORY_LIMIT_MASK));

        //
        // Save the prefetchable memory window.
        //

        } else if ((Allocation->Type == ResourceTypePhysicalAddressSpace) &&
                   ((Allocation->Characteristics &
                     MEMORY_CHARACTERISTIC_PREFETCHABLE) != 0)) {

            ControlRegister |= PCI_CONTROL_MEMORY_DECODE_ENABLED;
            Limit = Allocation->Allocation + Allocation->Length -
                    PCI_BRIDGE_MEMORY_GRANULARITY;

            PrefetchableMemoryLow =
                 (ULONG)(((Allocation->Allocation >>
                           PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_ADDRESS_SHIFT) &
                          PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_MASK) |
                         (Limit & PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_MASK));

            PrefetchableMemoryBaseHigh =
                    (ULONG)(Allocation->Allocation >>
                            PCI_BRIDGE_PREFETCHABLE_MEMORY_HIGH_ADDRESS_SHIFT);

            if (PrefetchableMemoryBaseHigh != 0) {
                PrefetchableMemoryLow |=
                             PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_DECODE_64_BIT;
            }

            PrefetchableMemoryLimitHigh =
                    (ULONG)(Limit >>
                            PCI_BRIDGE_PREFETCHABLE_MEMORY_HIGH_ADDRESS_SHIFT);

            if (PrefetchableMemoryLimitHigh != 0) {
                PrefetchableMemoryLow |=
                            PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_DECODE_64_BIT;
            }
        }

        //
        // Loop on to the next allocation.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // The secondary bus number that was allocated for this bridge should be
    // equal to the number allocated by the firmware at boot. This dependency
    // is taken to avoid doing a depth-first search to determine the correct
    // subordinate bus number for each bridge.
    //

    ASSERT(SecondaryBusNumber == OriginalSecondaryBusNumber);

    //
    // Set up the bus number register value now that the information has been
    // extracted.
    //

    BusRegister |= PrimaryBusNumber |
                   (SecondaryBusNumber << PCI_BRIDGE_SECONDARY_BUS_SHIFT) |
                   (SubordinateBusNumber << PCI_BRIDGE_SUBORDINATE_BUS_SHIFT);

    //
    // Okay, everything's accounted for. Write the values into the bridge.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_BUS_NUMBERS_OFFSET,
                sizeof(ULONG),
                BusRegister);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_IO_BAR_OFFSET,
                sizeof(USHORT),
                IoPortRegister);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_IO_HIGH_BAR_OFFSET,
                sizeof(ULONG),
                IoPortHigh);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_MEMORY_BAR_OFFSET,
                sizeof(ULONG),
                MemoryRegister);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_PREFETCHABLE_MEMORY_BAR_OFFSET,
                sizeof(ULONG),
                PrefetchableMemoryLow);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_HIGH_OFFSET,
                sizeof(ULONG),
                PrefetchableMemoryBaseHigh);

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_HIGH_OFFSET,
                sizeof(ULONG),
                PrefetchableMemoryLimitHigh);

    //
    // Write out the control register to enable address decoding.
    //

    WriteConfig(Bus,
                DeviceNumber,
                Function,
                PCI_CONTROL_OFFSET,
                sizeof(USHORT),
                ControlRegister);

    Status = STATUS_SUCCESS;

SetBridgeDeviceResourcesEnd:
    return Status;
}

ULONG
PcipFindDevice (
    PPCI_DEVICE ParentBus,
    UCHAR Device,
    UCHAR Function
    )

/*++

Routine Description:

    This routine searches for a PCI device matching the given device and
    function in the child list of another device.

Arguments:

    ParentBus - Supplies a pointer to the PCI device whose children should be
        searched.

    Device - Supplies the PCI device slot number to search for.

    Function - Supplies the function number to search for.

Return Value:

    Returns the index of the child in the device's child array, or MAX_ULONG
    if the device could not be found.

--*/

{

    ULONG ChildIndex;

    for (ChildIndex = 0; ChildIndex < ParentBus->ChildCount; ChildIndex += 1) {
        if ((ParentBus->ChildrenData[ChildIndex]->DeviceNumber == Device) &&
            (ParentBus->ChildrenData[ChildIndex]->Function == Function)) {

            return ChildIndex;
        }
    }

    return MAX_ULONG;
}

ULONG
PcipGetNewChildIndex (
    PPCI_DEVICE ParentBus
    )

/*++

Routine Description:

    This routine allocates space in the list of child devices, and also
    allocates space for the child information.

Arguments:

    ParentBus - Supplies a pointer to the parent bus device where a new device
        is about to be added.

Return Value:

    Returns an index into the child array where the new child device should be
    placed, and where the child device information buffer is stored.

    MAX_ULONG on error.

--*/

{

    ULONG AllocationCount;
    ULONG AllocationSize;
    PDEVICE *NewChildren;
    PPCI_CHILD *NewChildrenData;
    ULONG NewIndex;

    NewChildren = NULL;
    NewChildrenData = NULL;
    NewIndex = MAX_ULONG;

    ASSERT(ParentBus->ChildCount < MAX_PCI_DEVICES);

    if (ParentBus->ChildCount >= MAX_PCI_DEVICES) {
        goto GetNewChildIndexEnd;
    }

    //
    // If there's room in the array, simply use that.
    //

    if (ParentBus->ChildCount < ParentBus->ChildSize) {
        NewIndex = ParentBus->ChildCount;

    //
    // There's no room in the array. Allocate a new array, copy the old
    // contents in, and free the old array.
    //

    } else {
        AllocationCount = ParentBus->ChildSize * 2;
        if (AllocationCount < PCI_INITIAL_CHILD_COUNT) {
            AllocationCount = PCI_INITIAL_CHILD_COUNT;
        }

        if (AllocationCount > MAX_PCI_DEVICES) {
            AllocationCount = MAX_PCI_DEVICES;
        }

        //
        // Allocate the new array.
        //

        AllocationSize = (sizeof(PDEVICE) + sizeof(PPCI_CHILD)) *
                         AllocationCount;

        NewChildren = MmAllocatePagedPool(AllocationSize, PCI_ALLOCATION_TAG);
        if (NewChildren == NULL) {
            goto GetNewChildIndexEnd;
        }

        NewChildrenData = (PPCI_CHILD *)(NewChildren + AllocationCount);
        if (ParentBus->Children != NULL) {

            //
            // Copy the old contents over.
            //

            RtlCopyMemory(NewChildren,
                          ParentBus->Children,
                          sizeof(PDEVICE) * ParentBus->ChildCount);

            RtlCopyMemory(NewChildrenData,
                          ParentBus->ChildrenData,
                          sizeof(PPCI_DEVICE) * ParentBus->ChildCount);

            //
            // Free the old contents and update the pointers.
            //

            MmFreePagedPool(ParentBus->Children);
        }

        ParentBus->Children = NewChildren;
        ParentBus->ChildrenData = NewChildrenData;
        ParentBus->ChildSize = AllocationCount;
        NewIndex = ParentBus->ChildCount;
        NewChildren = NULL;
    }

    //
    // Allocate a new PCI child structure.
    //

    ParentBus->ChildrenData[NewIndex] =
                   MmAllocatePagedPool(sizeof(PCI_CHILD), PCI_ALLOCATION_TAG);

    if (ParentBus->ChildrenData[NewIndex] == NULL) {
        NewIndex = MAX_ULONG;
        goto GetNewChildIndexEnd;
    }

    RtlZeroMemory(ParentBus->ChildrenData[NewIndex], sizeof(PCI_CHILD));

GetNewChildIndexEnd:
    if (NewChildren != NULL) {
        MmFreePagedPool(NewChildren);
    }

    return NewIndex;
}

KSTATUS
PcipQueryInterface (
    PIRP Irp,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine responds to interface requests.

Arguments:

    Irp - Supplies a pointer to the Query Interface IRP.

    PciDevice - Supplies a pointer to the PCI device context relating to this
        device.

Return Value:

    Status code.

--*/

{

    PINTERFACE_ACPI_BUS_ADDRESS BusAddressInterface;
    PINTERFACE_PCI_BUS_DEVICE BusDeviceInterface;
    BOOL Match;
    PINTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    PINTERFACE_SPECIFIC_PCI_CONFIG_ACCESS SpecificPciConfigInterface;
    KSTATUS Status;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryInterface));

    if (Irp->U.QueryInterface.Interface == NULL) {
        return STATUS_INVALID_PARAMETER;
    }

    Status = STATUS_SUCCESS;

    //
    // Handle PCI config access interface requests.
    //

    Match = RtlAreUuidsEqual(&PciConfigSpaceUuid,
                             Irp->U.QueryInterface.Interface);

    if (Match != FALSE) {
        if (Irp->U.QueryInterface.InterfaceBuffer != NULL) {

            //
            // Copy the interface into the buffer, assuming its big enough.
            //

            if (Irp->U.QueryInterface.InterfaceBufferSize !=
                sizeof(INTERFACE_PCI_CONFIG_ACCESS)) {

                Status = STATUS_INCORRECT_BUFFER_SIZE;
                Irp->U.QueryInterface.InterfaceBufferSize =
                                            sizeof(INTERFACE_PCI_CONFIG_ACCESS);

                goto QueryInterfaceEnd;
            }

            PciConfigInterface = Irp->U.QueryInterface.InterfaceBuffer;
            PciConfigInterface->ReadPciConfig = PcipInterfaceReadConfigSpace;
            PciConfigInterface->WritePciConfig = PcipInterfaceWriteConfigSpace;
            PciConfigInterface->DeviceToken = PciDevice;

        //
        // The buffer is NULL, indicating the caller just wanted to know if the
        // interface was out there. Fill out the size and return success.
        //

        } else {
            Irp->U.QueryInterface.InterfaceBufferSize =
                                           sizeof(INTERFACE_PCI_CONFIG_ACCESS);
        }

        goto QueryInterfaceEnd;
    }

    //
    // Handle specific PCI config access interface requests.
    //

    Match = RtlAreUuidsEqual(&PciSpecificConfigSpaceUuid,
                             Irp->U.QueryInterface.Interface);

    if (Match != FALSE) {

        ASSERT((PciDevice->Type == PciDeviceBus) ||
               (PciDevice->Type == PciDeviceBridge));

        if (Irp->U.QueryInterface.InterfaceBuffer != NULL) {

            //
            // Copy the interface into the buffer, assuming its big enough.
            //

            if (Irp->U.QueryInterface.InterfaceBufferSize !=
                sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS)) {

                Status = STATUS_INCORRECT_BUFFER_SIZE;
                Irp->U.QueryInterface.InterfaceBufferSize =
                                  sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS);

                goto QueryInterfaceEnd;
            }

            SpecificPciConfigInterface = Irp->U.QueryInterface.InterfaceBuffer;
            SpecificPciConfigInterface->ReadPciConfig =
                                          PcipInterfaceReadSpecificConfigSpace;

            SpecificPciConfigInterface->WritePciConfig =
                                         PcipInterfaceWriteSpecificConfigSpace;

            SpecificPciConfigInterface->DeviceToken = PciDevice;

        //
        // The buffer is NULL, indicating the caller just wanted to know if the
        // interface was out there. Fill out the size and return success.
        //

        } else {
            Irp->U.QueryInterface.InterfaceBufferSize =
                                  sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS);
        }

        goto QueryInterfaceEnd;
    }

    //
    // Handle ACPI bus address interface requests.
    //

    if (PciDevice->Type == PciDeviceFunction) {
        Match = RtlAreUuidsEqual(&PciAcpiBusAddressUuid,
                                 Irp->U.QueryInterface.Interface);

        if (Match != FALSE) {

            ASSERT(PciDevice->Type == PciDeviceFunction);

            if (Irp->U.QueryInterface.InterfaceBuffer != NULL) {

                //
                // Copy the interface into the buffer, assuming its big enough.
                //

                if (Irp->U.QueryInterface.InterfaceBufferSize !=
                    sizeof(INTERFACE_ACPI_BUS_ADDRESS)) {

                    Status = STATUS_INCORRECT_BUFFER_SIZE;
                    Irp->U.QueryInterface.InterfaceBufferSize =
                                            sizeof(INTERFACE_ACPI_BUS_ADDRESS);

                    goto QueryInterfaceEnd;
                }

                BusAddressInterface = Irp->U.QueryInterface.InterfaceBuffer;
                BusAddressInterface->BusAddress =
                                              (PciDevice->DeviceNumber << 16) |
                                              PciDevice->FunctionNumber;

            //
            // The buffer is NULL, indicating the caller just wanted to know if
            // the interface was out there. Fill out the size and return
            // success.
            //

            } else {
                Irp->U.QueryInterface.InterfaceBufferSize =
                                            sizeof(INTERFACE_ACPI_BUS_ADDRESS);
            }

            goto QueryInterfaceEnd;
        }
    }

    //
    // Handle internal PCI bus driver context requests. The function driver for
    // bridges should not respond to this, leave it for the root bus function
    // driver or a PCI bus driver.
    //

    if ((PciDevice->Type == PciDeviceBus) ||
        (PciDevice->Type == PciDeviceFunction)) {

        Match = RtlAreUuidsEqual(&PciBusDriverDeviceUuid,
                                 Irp->U.QueryInterface.Interface);

        if (Match != FALSE) {
            if (Irp->U.QueryInterface.InterfaceBuffer != NULL) {

                //
                // Copy the interface into the buffer, assuming its big enough.
                //

                if (Irp->U.QueryInterface.InterfaceBufferSize !=
                    sizeof(INTERFACE_PCI_BUS_DEVICE)) {

                    ASSERT(FALSE);

                    Status = STATUS_INCORRECT_BUFFER_SIZE;
                    Irp->U.QueryInterface.InterfaceBufferSize =
                                              sizeof(INTERFACE_PCI_BUS_DEVICE);

                    goto QueryInterfaceEnd;
                }

                BusDeviceInterface = Irp->U.QueryInterface.InterfaceBuffer;
                BusDeviceInterface->BusDevice = PciDevice;

            //
            // The buffer is NULL, indicating the caller just wanted to know if
            // the interface was out there. Fill out the size and return
            // success.
            //

            } else {
                Irp->U.QueryInterface.InterfaceBufferSize =
                                              sizeof(INTERFACE_PCI_BUS_DEVICE);
            }

            goto QueryInterfaceEnd;
        }
    }

    //
    // The interface is not exposed by this PCI device.
    //

    Status = STATUS_NO_INTERFACE;

QueryInterfaceEnd:
    return Status;
}

KSTATUS
PcipInterfaceReadConfigSpace (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads from a device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        read.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies a pointer where the value read from PCI configuration
        space will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONGLONG DataRead;
    PPCI_DEVICE PciDevice;

    if (Offset > 0xFF) {
        return STATUS_NOT_SUPPORTED;
    }

    PciDevice = DeviceToken;
    DataRead = PciDevice->ReadConfig(PciDevice->BusNumber,
                                     PciDevice->DeviceNumber,
                                     PciDevice->FunctionNumber,
                                     Offset,
                                     AccessSize);

    *Value = DataRead;
    return STATUS_SUCCESS;
}

KSTATUS
PcipInterfaceWriteConfigSpace (
    PVOID DeviceToken,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes to a device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        write.

    Value - Supplies the value to write into PCI configuration space.

Return Value:

    Status code.

--*/

{

    PPCI_DEVICE PciDevice;

    if (Offset > 0xFF) {
        return STATUS_NOT_SUPPORTED;
    }

    PciDevice = DeviceToken;
    PciDevice->WriteConfig(PciDevice->BusNumber,
                           PciDevice->DeviceNumber,
                           PciDevice->FunctionNumber,
                           Offset,
                           AccessSize,
                           Value);

    return STATUS_SUCCESS;
}

KSTATUS
PcipInterfaceReadSpecificConfigSpace (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads from a specific device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    BusNumber - Supplies the bus number of the device whose PCI configuration
        space should be read from.

    DeviceNumber - Supplies the device number of the device whose PCI
        configuration space should be read from.

    FunctionNumber - Supplies the function number of the device whose PCI
        configuration space should be read from.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        read.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies a pointer where the value read from PCI configuration
        space will be returned on success.

Return Value:

    Status code.

--*/

{

    ULONGLONG DataRead;
    PPCI_DEVICE PciDevice;

    if (Offset > 0xFF) {
        return STATUS_NOT_SUPPORTED;
    }

    PciDevice = DeviceToken;

    ASSERT((PciDevice->Type == PciDeviceBus) ||
           (PciDevice->Type == PciDeviceBridge));

    DataRead = PciDevice->ReadConfig(BusNumber,
                                     DeviceNumber,
                                     FunctionNumber,
                                     Offset,
                                     AccessSize);

    *Value = DataRead;
    return STATUS_SUCCESS;
}

KSTATUS
PcipInterfaceWriteSpecificConfigSpace (
    PVOID DeviceToken,
    ULONG BusNumber,
    ULONG DeviceNumber,
    ULONG FunctionNumber,
    ULONG Offset,
    ULONG AccessSize,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes to a specific device's PCI configuration space.

Arguments:

    DeviceToken - Supplies the device token supplied when the interface was
        acquired.

    BusNumber - Supplies the bus number of the device whose PCI configuration
        space should be written to.

    DeviceNumber - Supplies the device number of the device whose PCI
        configuration space should be written to.

    FunctionNumber - Supplies the function number of the device whose PCI
        configuration space should be written to.

    Offset - Supplies the offset in bytes into the PCI configuration space to
        write.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write into PCI configuration space.

Return Value:

    Status code.

--*/

{

    PPCI_DEVICE PciDevice;

    if (Offset > 0xFF) {
        return STATUS_NOT_SUPPORTED;
    }

    PciDevice = DeviceToken;

    ASSERT((PciDevice->Type == PciDeviceBus) ||
           (PciDevice->Type == PciDeviceBridge));

    PciDevice->WriteConfig(BusNumber,
                           DeviceNumber,
                           FunctionNumber,
                           Offset,
                           AccessSize,
                           Value);

    return STATUS_SUCCESS;
}

KSTATUS
PcipStartBusDevice (
    PIRP StartIrp,
    PPCI_DEVICE DeviceContext
    )

/*++

Routine Description:

    This routine starts a PCI bus.

Arguments:

    StartIrp - Supplies a pointer to the start IRP.

    DeviceContext - Supplies a pointer to the PCI bus or bridge.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    BOOL BusNumberArbiterCreated;
    BOOL IoPortArbiterCreated;
    BOOL MemoryArbiterCreated;
    PPCI_DEVICE Parent;
    KSTATUS Status;

    ASSERT(StartIrp->MinorCode == IrpMinorStartDevice);
    ASSERT((DeviceContext->Type == PciDeviceBus) ||
           (DeviceContext->Type == PciDeviceBridge));

    //
    // Bridges need to query the interface of the bus driver to get
    // configuration space access.
    //

    if (DeviceContext->ReadConfig == NULL) {

        ASSERT(DeviceContext->Type == PciDeviceBridge);

        Status = PcipGetBusDriverDevice(StartIrp->Device, &Parent);
        if (!KSUCCESS(Status)) {
            goto StartBusDeviceEnd;
        }

        DeviceContext->ReadConfig = Parent->ReadConfig;
        DeviceContext->WriteConfig = Parent->WriteConfig;
    }

    ASSERT((DeviceContext->ReadConfig != NULL) &&
           (DeviceContext->WriteConfig != NULL));

    //
    // Create the "specific PCI Config Space" access interface.
    //

    Status = PcipCreateBusInterfaces(StartIrp->Device, DeviceContext);
    if (!KSUCCESS(Status)) {
        goto StartBusDeviceEnd;
    }

    BusNumberArbiterCreated = FALSE;
    IoPortArbiterCreated = FALSE;
    MemoryArbiterCreated = FALSE;
    Status = STATUS_SUCCESS;

    //
    // Loop through every resource given to the bus/bridge, and expose an
    // arbiter for child devices.
    //

    AllocationList = StartIrp->U.StartDevice.ProcessorLocalResources;
    if (AllocationList == NULL) {
        goto StartBusDeviceEnd;
    }

    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // Only create arbiters for expected types.
        //

        switch (Allocation->Type) {
        case ResourceTypeBusNumber:

            //
            // Create a bus number arbiter if one hasn't been created yet and
            // more than one bus number was doled out. Keep the first bus
            // number for this bus itself.
            //

            if (Allocation->Length > 1) {

                ASSERT(Allocation->Allocation == DeviceContext->BusNumber);

                if (BusNumberArbiterCreated == FALSE) {
                    Status = IoCreateResourceArbiter(StartIrp->Device,
                                                     Allocation->Type);

                    if (!KSUCCESS(Status)) {
                        goto StartBusDeviceEnd;
                    }

                    BusNumberArbiterCreated = TRUE;
                }

                Status = IoAddFreeSpaceToArbiter(StartIrp->Device,
                                                 Allocation->Type,
                                                 Allocation->Allocation + 1,
                                                 Allocation->Length - 1,
                                                 Allocation->Characteristics,
                                                 Allocation,
                                                 0);

            //
            // If only one bus number was handed out, this must be a bridge.
            // Save that bus number for downstream config accesses later.
            //

            } else {

                ASSERT(Allocation->Length == 1);
                ASSERT((UCHAR)Allocation->Allocation == Allocation->Allocation);

                DeviceContext->BusNumber = (UCHAR)(Allocation->Allocation);
            }

            break;

        case ResourceTypePhysicalAddressSpace:

            //
            // Create an address space arbiter if one hasn't been created yet.
            //

            if (MemoryArbiterCreated == FALSE) {
                Status = IoCreateResourceArbiter(StartIrp->Device,
                                                 Allocation->Type);

                if (!KSUCCESS(Status)) {
                    goto StartBusDeviceEnd;
                }

                MemoryArbiterCreated = TRUE;
            }

            Status = IoAddFreeSpaceToArbiter(StartIrp->Device,
                                             Allocation->Type,
                                             Allocation->Allocation,
                                             Allocation->Length,
                                             Allocation->Characteristics,
                                             Allocation,
                                             0);

            break;

        case ResourceTypeIoPort:

            //
            // Create an I/O port arbiter if one hasn't been created yet.
            //

            if (IoPortArbiterCreated == FALSE) {
                Status = IoCreateResourceArbiter(StartIrp->Device,
                                                 Allocation->Type);

                if (!KSUCCESS(Status)) {
                    goto StartBusDeviceEnd;
                }

                IoPortArbiterCreated = TRUE;
            }

            Status = IoAddFreeSpaceToArbiter(StartIrp->Device,
                                             Allocation->Type,
                                             Allocation->Allocation,
                                             Allocation->Length,
                                             Allocation->Characteristics,
                                             Allocation,
                                             0);

            break;

        default:
            break;
        }

        if (!KSUCCESS(Status)) {
            goto StartBusDeviceEnd;
        }

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

StartBusDeviceEnd:
    return Status;
}

KSTATUS
PcipCreateFunctionInterfaces (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine creates the exposed interfaces for a PCI device.

Arguments:

    Device - Supplies a pointer to the device to create interfaces for.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    Status code.

--*/

{

    PINTERFACE_ACPI_BUS_ADDRESS BusAddressInterface;
    PINTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    KSTATUS Status;

    BusAddressInterface = NULL;

    //
    // Create the PCI config access interface.
    //

    PciConfigInterface = MmAllocateNonPagedPool(
                                           sizeof(INTERFACE_PCI_CONFIG_ACCESS),
                                           PCI_ALLOCATION_TAG);

    if (PciConfigInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFunctionInterfacesEnd;
    }

    RtlZeroMemory(PciConfigInterface, sizeof(INTERFACE_PCI_CONFIG_ACCESS));
    PciConfigInterface->ReadPciConfig = PcipInterfaceReadConfigSpace;
    PciConfigInterface->WritePciConfig = PcipInterfaceWriteConfigSpace;
    PciConfigInterface->DeviceToken = PciDevice;
    PciDevice->PciConfigInterface = PciConfigInterface;

    //
    // Create the ACPI bus address interface.
    //

    BusAddressInterface = MmAllocateNonPagedPool(
                                            sizeof(INTERFACE_ACPI_BUS_ADDRESS),
                                            PCI_ALLOCATION_TAG);

    if (BusAddressInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateFunctionInterfacesEnd;
    }

    RtlZeroMemory(BusAddressInterface, sizeof(INTERFACE_ACPI_BUS_ADDRESS));
    BusAddressInterface->BusAddress = (PciDevice->DeviceNumber << 16) |
                                      PciDevice->FunctionNumber;

    PciDevice->AcpiBusAddressInterface = BusAddressInterface;

    //
    // Enumerate the devices to the system.
    //

    Status = IoCreateInterface(&PciConfigSpaceUuid,
                               Device,
                               PciConfigInterface,
                               sizeof(INTERFACE_PCI_CONFIG_ACCESS));

    if (!KSUCCESS(Status)) {

        //
        // Allow this to fail with a duplicate entry if the device is a bridge,
        // as the bridge's functional driver will have already created this
        // interface.
        //

        if ((Status != STATUS_DUPLICATE_ENTRY) ||
            (PciDevice->DeviceIsBridge == FALSE)) {

            goto CreateFunctionInterfacesEnd;
        }
    }

    Status = IoCreateInterface(&PciAcpiBusAddressUuid,
                               Device,
                               BusAddressInterface,
                               sizeof(INTERFACE_ACPI_BUS_ADDRESS));

    if (!KSUCCESS(Status)) {
        IoDestroyInterface(&PciConfigSpaceUuid, Device, PciConfigInterface);
        goto CreateFunctionInterfacesEnd;
    }

    //
    // Attempt to create the MSI/MSI-X context and interface for this function
    // device.
    //

    Status = PcipMsiCreateContextAndInterface(Device, PciDevice);
    if (!KSUCCESS(Status)) {
        IoDestroyInterface(&PciConfigSpaceUuid, Device, PciConfigInterface);
        IoDestroyInterface(&PciAcpiBusAddressUuid, Device, BusAddressInterface);
        goto CreateFunctionInterfacesEnd;
    }

    Status = STATUS_SUCCESS;

CreateFunctionInterfacesEnd:
    if (!KSUCCESS(Status)) {
        if (PciConfigInterface != NULL) {
            MmFreeNonPagedPool(PciConfigInterface);
        }

        if (BusAddressInterface != NULL) {
            MmFreeNonPagedPool(BusAddressInterface);
        }

        PciDevice->PciConfigInterface = NULL;
        PciDevice->AcpiBusAddressInterface = NULL;
        if (PciDevice->MsiContext != NULL) {
            PcipMsiDestroyContextAndInterface(Device, PciDevice);
        }
    }

    return Status;
}

KSTATUS
PcipCreateBusInterfaces (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    )

/*++

Routine Description:

    This routine creates the exposed interfaces for a PCI device.

Arguments:

    Device - Supplies a pointer to the device to create interfaces for.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    Status code.

--*/

{

    PINTERFACE_SPECIFIC_PCI_CONFIG_ACCESS SpecificPciConfigInterface;
    KSTATUS Status;

    //
    // Create the specific PCI config access interface.
    //

    SpecificPciConfigInterface = MmAllocateNonPagedPool(
                                  sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS),
                                  PCI_ALLOCATION_TAG);

    if (SpecificPciConfigInterface == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateInterfacesEnd;
    }

    RtlZeroMemory(SpecificPciConfigInterface,
                  sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS));

    SpecificPciConfigInterface->ReadPciConfig =
                                          PcipInterfaceReadSpecificConfigSpace;

    SpecificPciConfigInterface->WritePciConfig =
                                         PcipInterfaceWriteSpecificConfigSpace;

    SpecificPciConfigInterface->DeviceToken = PciDevice;

    //
    // Expose the interface to the system.
    //

    Status = IoCreateInterface(&PciSpecificConfigSpaceUuid,
                               Device,
                               SpecificPciConfigInterface,
                               sizeof(INTERFACE_SPECIFIC_PCI_CONFIG_ACCESS));

    if (!KSUCCESS(Status)) {
        goto CreateInterfacesEnd;
    }

    PciDevice->SpecificPciConfigInterface = SpecificPciConfigInterface;
    Status = STATUS_SUCCESS;

CreateInterfacesEnd:
    if (!KSUCCESS(Status)) {
        if (SpecificPciConfigInterface != NULL) {
            MmFreeNonPagedPool(SpecificPciConfigInterface);
        }

        PciDevice->SpecificPciConfigInterface = NULL;
    }

    return Status;
}

PSTR
PcipGetClassId (
    ULONG ClassCode
    )

/*++

Routine Description:

    This routine returns the class string for the given PCI class code.

Arguments:

    ClassCode - Supplies the class code.

Return Value:

    Returns a pointer to a string describing the class.

    NULL if no class could be determined.

--*/

{

    UCHAR Class;
    USHORT Subclass;

    Class = PCI_CLASS_CODE(ClassCode);
    Subclass = PCI_SUBCLASS_AND_INTERFACE(ClassCode);
    switch (Class) {

    //
    // Unimplemented or unknown class codes.
    //

    case PCI_CLASS_UNKNOWN:
        if (Subclass == PCI_CLASS_UNKNOWN_VGA) {
            return "VGA";
        }

        break;

    case PCI_CLASS_MASS_STORAGE:
        if ((Subclass & PCI_CLASS_MASS_STORAGE_IDE_MASK) ==
            PCI_CLASS_MASS_STORAGE_IDE) {

            return "IDE";
        }

        if (Subclass == PCI_CLASS_MASS_STORAGE_SATA) {
            return "AHCI";
        }

        break;

    case PCI_CLASS_BRIDGE:
        switch (Subclass) {
        case PCI_CLASS_BRIDGE_ISA:
            return "ISA";

        case PCI_CLASS_BRIDGE_PCI:
            return PCI_BRIDGE_CLASS_ID;

        case PCI_CLASS_BRIDGE_PCI_SUBTRACTIVE:
            return PCI_SUBTRACTIVE_BRIDGE_CLASS_ID;

        default:
            break;
        }

        break;

    case PCI_CLASS_SERIAL_BUS:
        switch (Subclass) {
        case PCI_CLASS_SERIAL_BUS_USB_UHCI:
            return "UHCI";

        case PCI_CLASS_SERIAL_BUS_USB_OHCI:
            return "OHCI";

        case PCI_CLASS_SERIAL_BUS_USB_EHCI:
            return "EHCI";

        default:
            break;
        }

        break;

    case PCI_CLASS_MULTIMEDIA:
        switch (Subclass) {
        case PCI_CLASS_MULTIMEDIA_AUDIO:
            return "Audio";

        default:
            break;
        }

        break;

    case PCI_CLASS_NETWORK:
    case PCI_CLASS_DISPLAY:
    case PCI_CLASS_MEMORY:
        break;

    case PCI_CLASS_SIMPLE_COMMUNICATION:
        switch (Subclass) {
        case PCI_CLASS_SIMPLE_COMMUNICATION_XT_UART:
        case PCI_CLASS_SIMPLE_COMMUNICATION_16450:
        case PCI_CLASS_SIMPLE_COMMUNICATION_16550:
            return "Serial16550";
        }

        break;

    case PCI_CLASS_GENERAL_PERIPHERAL:
        switch (Subclass) {
        case PCI_CLASS_GENERAL_SD_HOST_NO_DMA:
            return "SdHostPio";

        case PCI_CLASS_GENERAL_SD_HOST:
            return "SdHost";

        default:
            break;
        }

        break;

    case PCI_CLASS_INPUT:
    case PCI_CLASS_DOCKING_STATION:
    case PCI_CLASS_PROCESSOR:
    case PCI_CLASS_WIRELESS:
    case PCI_CLASS_INTELLIGENT_IO:
    case PCI_CLASS_SATELLITE_COMMUNICATION:
    case PCI_CLASS_ENCRYPTION:
    case PCI_CLASS_DATA_ACQUISITION:
    case PCI_CLASS_VENDOR:
        break;
    }

    return NULL;
}

KSTATUS
PcipGetBusDriverDevice (
    PDEVICE OsDevice,
    PPCI_DEVICE *BusDriverDevice
    )

/*++

Routine Description:

    This routine returns the bus driver's PCI device structure.

Arguments:

    OsDevice - Supplies a pointer to the OS device.

    BusDriverDevice - Supplies a pointer where a pointer to the bus driver's
        device will be returned on success.

Return Value:

    Status code.

--*/

{

    INTERFACE_PCI_BUS_DEVICE Interface;
    PIRP QueryInterfaceIrp;
    KSTATUS Status;

    *BusDriverDevice = NULL;

    //
    // Allocate and send an IRP to the bus driver requesting access
    // to the PCI config interface.
    //

    QueryInterfaceIrp = IoCreateIrp(OsDevice, IrpMajorStateChange, 0);
    if (QueryInterfaceIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetBusDriverDeviceEnd;
    }

    QueryInterfaceIrp->MinorCode = IrpMinorQueryInterface;
    QueryInterfaceIrp->U.QueryInterface.Interface = &PciBusDriverDeviceUuid;
    QueryInterfaceIrp->U.QueryInterface.InterfaceBuffer = &Interface;
    QueryInterfaceIrp->U.QueryInterface.InterfaceBufferSize =
                                              sizeof(INTERFACE_PCI_BUS_DEVICE);

    Status = IoSendSynchronousIrp(QueryInterfaceIrp);
    if (!KSUCCESS(Status)) {
        goto GetBusDriverDeviceEnd;
    }

    Status = IoGetIrpStatus(QueryInterfaceIrp);
    if (!KSUCCESS(Status)) {
        goto GetBusDriverDeviceEnd;
    }

    *BusDriverDevice = Interface.BusDevice;

GetBusDriverDeviceEnd:
    if (QueryInterfaceIrp != NULL) {
        IoDestroyIrp(QueryInterfaceIrp);
    }

    return Status;
}

