/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    drvsup.c

Abstract:

    This module implements driver support functions for ACPI.

Author:

    Evan Green 2-Dec-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/acpi.h>
#include "acpip.h"
#include "fixedreg.h"
#include "namespce.h"
#include "amlos.h"
#include "resdesc.h"
#include "proc.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of seconds to wait for the SCI enable bit to flip on.
//

#define ENABLE_ACPI_TIMEOUT 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
AcpipEnableAcpiModeDpc (
    PDPC Dpc
    );

KSTATUS
AcpipGetDeviceHardwareId (
    PACPI_OBJECT Device,
    PSTR *DeviceHardwareId
    );

KSTATUS
AcpipQueryOsDeviceBusAddress (
    PDEVICE Device,
    PULONGLONG BusAddress
    );

BOOL
AcpipIsDeviceIdPciBus (
    ULONG DeviceId
    );

PPCI_ROUTING_TABLE
AcpipCreatePciRoutingTable (
    PACPI_OBJECT PrtPackage
    );

VOID
AcpipDestroyPciRoutingTable (
    PPCI_ROUTING_TABLE RoutingTable
    );

KSTATUS
AcpipTranslateInterruptLine (
    PACPI_DEVICE_CONTEXT Device,
    PULONGLONG InterruptLine,
    PULONGLONG InterruptLineCharacteristics,
    PULONGLONG InterruptLineFlags
    );

KSTATUS
AcpipApplyPciRoutingTable (
    PACPI_DEVICE_CONTEXT Device,
    ULONGLONG BusAddress,
    PPCI_ROUTING_TABLE RoutingTable,
    PULONGLONG Interrupt,
    PULONGLONG InterruptCharacteristics,
    PULONGLONG InterruptFlags
    );

KSTATUS
AcpipCreateOsDevice (
    PACPI_OBJECT NamespaceDevice,
    PDEVICE ParentDevice,
    PACPI_DEVICE_CONTEXT ParentDeviceContext,
    PDEVICE *OsDevice
    );

BOOL
AcpipIsDevicePciBridge (
    PDEVICE Device
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the interface UUID for getting the ACPI bus address of a device.
//

UUID AcpiBusAddressUuid = UUID_ACPI_BUS_ADDRESS;

//
// Store a boolean that helps debug interrupt routing issues.
//

BOOL AcpiDebugInterruptRouting = FALSE;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipEnumerateDeviceChildren (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine enumerates any children of the given ACPI device. It matches
    up any children reported by the bus, and creates any missing devices.

Arguments:

    Device - Supplies a pointer to the device to enumerate.

    DeviceObject - Supplies a pointer to the ACPI information associated with
        the device to enumerate.

    Irp - Supplies a pointer to the query children IRP.

Return Value:

    Status code.

--*/

{

    ULONGLONG AcpiDeviceBusAddress;
    ULONG AllocationSize;
    PDEVICE *AugmentedChildArray;
    ULONG ChildIndex;
    PDEVICE *Children;
    ULONG ExistingChildIndex;
    ULONG NamespaceChildCount;
    PACPI_OBJECT *NamespaceChildren;
    PACPI_DEVICE_CONTEXT NewChild;
    PACPI_CHILD_DEVICE NewChildList;
    ULONG OldChildIndex;
    ULONG OriginalChildCount;
    ULONGLONG OsDeviceBusAddress;
    PDEVICE PciChild;
    ULONG PciChildIndex;
    ULONG PreviousChildCount;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    NewChild = NULL;
    NewChildList = NULL;
    NamespaceChildCount = 0;
    NamespaceChildren = NULL;

    //
    // Get the child devices of this object.
    //

    if (DeviceObject->NamespaceObject != NULL) {
        NamespaceChildren = AcpipEnumerateChildObjects(
                                                DeviceObject->NamespaceObject,
                                                AcpiObjectDevice,
                                                &NamespaceChildCount);
    }

    if (NamespaceChildren != NULL) {

        ASSERT(NamespaceChildCount != 0);

        //
        // Create a new child list.
        //

        AllocationSize = NamespaceChildCount * sizeof(ACPI_CHILD_DEVICE);
        NewChildList = MmAllocatePagedPool(AllocationSize, ACPI_ALLOCATION_TAG);
        if (NewChildList == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateDeviceChildrenEnd;
        }

        RtlZeroMemory(NewChildList, AllocationSize);

        //
        // Loop on by and fill up the new child list.
        //

        for (ChildIndex = 0;
             ChildIndex < NamespaceChildCount;
             ChildIndex += 1) {

            NewChildList[ChildIndex].NamespaceObject =
                                                 NamespaceChildren[ChildIndex];

            ASSERT(NewChildList[ChildIndex].NamespaceObject != NULL);

            NewChildList[ChildIndex].Device = NULL;

            //
            // Go through the old list to see if this object was enumerated
            // previously, and snatch its device pointer if it was.
            //

            for (OldChildIndex = 0;
                 OldChildIndex < DeviceObject->ChildCount;
                 OldChildIndex += 1) {

                if (DeviceObject->ChildArray[OldChildIndex].NamespaceObject ==
                    NamespaceChildren[ChildIndex]) {

                    NewChildList[ChildIndex].Device =
                                DeviceObject->ChildArray[OldChildIndex].Device;

                    DeviceObject->ChildArray[OldChildIndex].NamespaceObject =
                                                                          NULL;

                    break;
                }
            }
        }
    }

    //
    // Free and replace the old list.
    //

    if (DeviceObject->ChildCount != 0) {

        ASSERT(DeviceObject->ChildArray != NULL);

        MmFreePagedPool(DeviceObject->ChildArray);
    }

    DeviceObject->ChildArray = NewChildList;
    DeviceObject->ChildCount = NamespaceChildCount;

    //
    // Look through every child device to ensure there is an actual system
    // device matched up to it.
    //

    OriginalChildCount = Irp->U.QueryChildren.ChildCount;
    for (ChildIndex = 0; ChildIndex < NamespaceChildCount; ChildIndex += 1) {
        if (NewChildList[ChildIndex].Device == NULL) {

            //
            // If there are no children already listed, ACPI must be the bus
            // driver here. Create a device.
            //

            if (Irp->U.QueryChildren.ChildCount == 0) {
                Status = AcpipCreateOsDevice(
                                      NewChildList[ChildIndex].NamespaceObject,
                                      Device,
                                      DeviceObject,
                                      &(NewChildList[ChildIndex].Device));

                if (!KSUCCESS(Status)) {

                    //
                    // If the device failed because it does not have a _UID
                    // method, it was probably trying to augment a real device
                    // that's not there. Count that as success.
                    //

                    if (Status == STATUS_DEVICE_NOT_CONNECTED) {
                        Status = STATUS_SUCCESS;
                    }

                    goto EnumerateDeviceChildrenEnd;
                }

            //
            // On an enumerable bus, ACPI is not the head honcho. Try to match
            // against an already existing device on the bus, and attach to it.
            // If the bus doesn't enuemerate, then neither will ACPI.
            //

            } else {

                //
                // Get the bus address of the namespace object.
                //

                Status = AcpipGetDeviceBusAddress(
                                      NewChildList[ChildIndex].NamespaceObject,
                                      &AcpiDeviceBusAddress);

                if (!KSUCCESS(Status)) {

                    //
                    // If there is no bus address, then ACPI is trying to add
                    // a non-enumerable device onto an enumerable bus. Add that
                    // new device now.
                    //

                    Status = AcpipCreateOsDevice(
                                      NewChildList[ChildIndex].NamespaceObject,
                                      Device,
                                      DeviceObject,
                                      &(NewChildList[ChildIndex].Device));

                    if (!KSUCCESS(Status)) {
                        goto EnumerateDeviceChildrenEnd;
                    }

                    //
                    // Replace the IRP's array with this new one that is
                    // augmented to contain the new device ACPI just enumerated.
                    //

                    PreviousChildCount = Irp->U.QueryChildren.ChildCount;
                    AllocationSize = (PreviousChildCount + 1) * sizeof(PDEVICE);
                    AugmentedChildArray = MmAllocatePagedPool(
                                                          AllocationSize,
                                                          ACPI_ALLOCATION_TAG);

                    if (AugmentedChildArray == NULL) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        goto EnumerateDeviceChildrenEnd;
                    }

                    RtlCopyMemory(AugmentedChildArray,
                                  Irp->U.QueryChildren.Children,
                                  PreviousChildCount * sizeof(PDEVICE));

                    AugmentedChildArray[PreviousChildCount] =
                                               NewChildList[ChildIndex].Device;

                    MmFreePagedPool(Irp->U.QueryChildren.Children);
                    Irp->U.QueryChildren.Children = AugmentedChildArray;
                    Irp->U.QueryChildren.ChildCount = PreviousChildCount + 1;
                    continue;
                }

                Children = Irp->U.QueryChildren.Children;
                for (ExistingChildIndex = 0;
                     ExistingChildIndex < OriginalChildCount;
                     ExistingChildIndex += 1) {

                    //
                    // Get the bus address of the OS device object.
                    //

                    Status = AcpipQueryOsDeviceBusAddress(
                                                  Children[ExistingChildIndex],
                                                  &OsDeviceBusAddress);

                    if (!KSUCCESS(Status)) {
                        goto EnumerateDeviceChildrenEnd;
                    }

                    //
                    // If the bus address numbers are equal, then attach to the
                    // OS device.
                    //

                    if (AcpiDeviceBusAddress == OsDeviceBusAddress) {

                        //
                        // Create a new child object structure.
                        //

                        NewChild = MmAllocatePagedPool(
                                                   sizeof(ACPI_DEVICE_CONTEXT),
                                                   ACPI_ALLOCATION_TAG);

                        if (NewChild == NULL) {
                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            goto EnumerateDeviceChildrenEnd;
                        }

                        RtlZeroMemory(NewChild, sizeof(ACPI_DEVICE_CONTEXT));
                        NewChild->NamespaceObject =
                                      NewChildList[ChildIndex].NamespaceObject;

                        NewChild->ParentObject = DeviceObject;
                        NewChild->OsDevice = Children[ExistingChildIndex];
                        NewChild->BusAddress = ACPI_INVALID_BUS_ADDRESS;
                        if (AcpipIsDevicePciBridge(NewChild->OsDevice) !=
                            FALSE) {

                            NewChild->Flags |= ACPI_DEVICE_PCI_BRIDGE;
                        }

                        KeAcquireSpinLock(&AcpiDeviceListLock);
                        INSERT_AFTER(&(NewChild->ListEntry),
                                     &AcpiDeviceObjectListHead);

                        KeReleaseSpinLock(&AcpiDeviceListLock);
                        Status = IoAttachDriverToDevice(
                                                  AcpiDriver,
                                                  Children[ExistingChildIndex],
                                                  NewChild);

                        if (!KSUCCESS(Status)) {
                            goto EnumerateDeviceChildrenEnd;
                        }

                        NewChild->NamespaceObject->U.Device.OsDevice =
                                                  Children[ExistingChildIndex];

                        NewChild->NamespaceObject->U.Device.DeviceContext =
                                                                      NewChild;

                        NewChild = NULL;
                        break;
                    }
                }
            }
        }
    }

    //
    // If the IRP's child array is NULL, then ACPI must be the bus driver
    // (ie the bus this device is actually on is non-enumerable).
    //

    if ((Irp->U.QueryChildren.Children == NULL) &&
        (NamespaceChildCount != 0)) {

        ASSERT(Irp->U.QueryChildren.ChildCount == 0);

        Children = MmAllocatePagedPool(NamespaceChildCount * sizeof(PDEVICE),
                                       ACPI_ALLOCATION_TAG);

        if (Children == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateDeviceChildrenEnd;
        }

        for (ChildIndex = 0;
             ChildIndex < NamespaceChildCount;
             ChildIndex += 1) {

            ASSERT(NewChildList[ChildIndex].Device != NULL);

            Children[ChildIndex] = NewChildList[ChildIndex].Device;
        }

        Irp->U.QueryChildren.Children = Children;
        Irp->U.QueryChildren.ChildCount = NamespaceChildCount;

    //
    // ACPI is not the bus driver here. Check to see if this is a PCI bus. If
    // it is, then connect to all devices so that the PCI routing table can be
    // evaluated.
    //

    } else if (((DeviceObject->NamespaceObject != NULL) &&
                (DeviceObject->NamespaceObject->U.Device.IsPciBus != FALSE)) ||
               ((DeviceObject->Flags & ACPI_DEVICE_PCI_BRIDGE) != 0)) {

        for (PciChildIndex = 0;
             PciChildIndex < OriginalChildCount;
             PciChildIndex += 1) {

            PciChild = Irp->U.QueryChildren.Children[PciChildIndex];

            //
            // Loop through and attempt to find the ACPI namespace object
            // corresponding to this device.
            //

            for (ChildIndex = 0;
                 ChildIndex < NamespaceChildCount;
                 ChildIndex += 1) {

                if (NamespaceChildren[ChildIndex]->U.Device.OsDevice ==
                    PciChild) {

                    break;
                }
            }

            //
            // If no such device exists, attach to the device.
            //

            if (ChildIndex == NamespaceChildCount) {
                NewChild = MmAllocatePagedPool(sizeof(ACPI_DEVICE_CONTEXT),
                                               ACPI_ALLOCATION_TAG);

                if (NewChild == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto EnumerateDeviceChildrenEnd;
                }

                RtlZeroMemory(NewChild, sizeof(ACPI_DEVICE_CONTEXT));
                NewChild->BusAddress = ACPI_INVALID_BUS_ADDRESS;
                NewChild->ParentObject = DeviceObject;
                NewChild->OsDevice = PciChild;
                if (AcpipIsDevicePciBridge(NewChild->OsDevice) != FALSE) {
                    NewChild->Flags |= ACPI_DEVICE_PCI_BRIDGE;
                }

                KeAcquireSpinLock(&AcpiDeviceListLock);
                INSERT_AFTER(&(NewChild->ListEntry),
                             &AcpiDeviceObjectListHead);

                KeReleaseSpinLock(&AcpiDeviceListLock);
                Status = IoAttachDriverToDevice(AcpiDriver, PciChild, NewChild);
                if (!KSUCCESS(Status)) {
                    goto EnumerateDeviceChildrenEnd;
                }

                NewChild = NULL;
            }
        }
    }

    Status = STATUS_SUCCESS;

EnumerateDeviceChildrenEnd:
    if (!KSUCCESS(Status)) {
        if (NewChild != NULL) {
            LIST_REMOVE(&(NewChild->ListEntry));
            MmFreePagedPool(NewChild);
        }
    }

    if (NamespaceChildren != NULL) {
        AcpipReleaseChildEnumerationArray(NamespaceChildren,
                                          NamespaceChildCount);
    }

    return Status;
}

KSTATUS
AcpipQueryResourceRequirements (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine determines the resource requirements of the given device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceObject - Supplies a pointer to the ACPI information associated with
        the system device.

    Irp - Supplies a pointer to the query resources IRP.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT AcpiDevice;
    PRESOURCE_ALLOCATION_LIST BootAllocations;
    PRESOURCE_CONFIGURATION_LIST CrsConfigurationList;
    PACPI_OBJECT CrsObject;
    PACPI_OBJECT CrsReturnValue;
    ULONG DeviceStatus;
    PRESOURCE_CONFIGURATION_LIST PrsConfigurationList;
    PACPI_OBJECT PrsObject;
    PACPI_OBJECT PrsReturnValue;
    KSTATUS Status;

    AcpiDevice = DeviceObject->NamespaceObject;
    BootAllocations = NULL;
    CrsConfigurationList = NULL;
    CrsReturnValue = NULL;
    PrsConfigurationList = NULL;
    PrsReturnValue = NULL;

    ASSERT((Irp->MajorCode == IrpMajorStateChange) &&
           (Irp->MinorCode == IrpMinorQueryResources));

    //
    // Attempt to find and execute the _PRS (Possible Resource Settings) method.
    //

    PrsObject = AcpipFindNamedObject(AcpiDevice, ACPI_METHOD__PRS);
    if (PrsObject != NULL) {
        Status = AcpiExecuteMethod(PrsObject,
                                   NULL,
                                   0,
                                   AcpiObjectBuffer,
                                   &PrsReturnValue);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }

        if ((PrsReturnValue == NULL) ||
            (PrsReturnValue->Type != AcpiObjectBuffer)) {

            Status = STATUS_UNEXPECTED_TYPE;
            goto QueryResourceRequirementsEnd;
        }

        //
        // Convert the buffer into a configuration list.
        //

        Status = AcpipConvertFromAcpiResourceBuffer(AcpiDevice,
                                                    PrsReturnValue,
                                                    &PrsConfigurationList);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    //
    // Execute _STA (Status) to determine if it's valid to evaluate _CRS. If
    // the device is not enabled and decoding resources, then evaluate _CRS to
    // determine the format that _SRS must take, but don't actually send the
    // system that list, as it's probably not valid.
    //

    Status = AcpipGetDeviceStatus(AcpiDevice, &DeviceStatus);
    if (!KSUCCESS(Status)) {
        goto QueryResourceRequirementsEnd;
    }

    //
    // Attempt to find and execute the _CRS (Current Resource Settings) method.
    //

    CrsObject = AcpipFindNamedObject(AcpiDevice, ACPI_METHOD__CRS);
    if (CrsObject != NULL) {
        Status = AcpiExecuteMethod(CrsObject,
                                   NULL,
                                   0,
                                   AcpiObjectBuffer,
                                   &CrsReturnValue);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }

        if ((CrsReturnValue == NULL) ||
            (CrsReturnValue->Type != AcpiObjectBuffer)) {

            Status = STATUS_UNEXPECTED_TYPE;
            goto QueryResourceRequirementsEnd;
        }

        //
        // Save the result of CRS into the device context.
        //

        DeviceObject->ResourceBuffer = CrsReturnValue;
        AcpipObjectAddReference(CrsReturnValue);
        if ((DeviceStatus & ACPI_DEVICE_STATUS_ENABLED) == 0) {
            goto QueryResourceRequirementsEnd;
        }

        //
        // Convert the buffer into a configuration list.
        //

        Status = AcpipConvertFromAcpiResourceBuffer(AcpiDevice,
                                                    CrsReturnValue,
                                                    &CrsConfigurationList);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }

        //
        // Convert the configuration list into an allocation list.
        //

        Status = AcpipConvertFromRequirementListToAllocationList(
                                                          CrsConfigurationList,
                                                          &BootAllocations);

        if (!KSUCCESS(Status)) {
            goto QueryResourceRequirementsEnd;
        }
    }

    Status = STATUS_SUCCESS;

QueryResourceRequirementsEnd:
    if (CrsReturnValue != NULL) {
        AcpipObjectReleaseReference(CrsReturnValue);
    }

    if (PrsReturnValue != NULL) {
        AcpipObjectReleaseReference(PrsReturnValue);
    }

    if (!KSUCCESS(Status)) {
        if (PrsConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(PrsConfigurationList);
            PrsConfigurationList = NULL;
        }

        if (BootAllocations != NULL) {
            IoDestroyResourceAllocationList(BootAllocations);
            BootAllocations = NULL;
        }

        if (CrsConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(CrsConfigurationList);
            CrsConfigurationList = NULL;
        }

        if (DeviceObject->ResourceBuffer != NULL) {
            AcpipObjectReleaseReference(DeviceObject->ResourceBuffer);
            DeviceObject->ResourceBuffer = NULL;
        }
    }

    //
    // If there is a PRS configuration list, use it for resource requirements.
    // Otherwise, use CRS as a requirements list.
    //

    if (PrsConfigurationList != NULL) {
        Irp->U.QueryResources.ResourceRequirements = PrsConfigurationList;
        if (CrsConfigurationList != NULL) {
            IoDestroyResourceConfigurationList(CrsConfigurationList);
            CrsConfigurationList = NULL;
        }

    } else {
        Irp->U.QueryResources.ResourceRequirements = CrsConfigurationList;
    }

    Irp->U.QueryResources.BootAllocation = BootAllocations;
    return Status;
}

KSTATUS
AcpipFilterResourceRequirements (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine filters resource requirements for the given device. This
    routine is called when ACPI is not the bus driver, but may adjust things
    like interrupt line resources for PCI devices.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceObject - Supplies a pointer to the ACPI information associated with
        the system device.

    Irp - Supplies a pointer to the query resources IRP.

Return Value:

    Status code.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    PRESOURCE_CONFIGURATION_LIST ConfigurationList;
    PRESOURCE_REQUIREMENT Requirement;
    PRESOURCE_REQUIREMENT_LIST RequirementList;
    KSTATUS Status;

    //
    // Loop through every resource configuration list.
    //

    ConfigurationList = Irp->U.QueryResources.ResourceRequirements;
    if (ConfigurationList != NULL) {
        RequirementList = IoGetNextResourceConfiguration(ConfigurationList,
                                                         NULL);

        while (RequirementList != NULL) {

            //
            // Loop through every requirement in the requirement list.
            //

            Requirement = IoGetNextResourceRequirement(RequirementList, NULL);
            while (Requirement != NULL) {

                //
                // If it's an interrupt line requirement, translate that up.
                //

                if (Requirement->Type == ResourceTypeInterruptLine) {
                    Status = AcpipTranslateInterruptLine(
                                               DeviceObject,
                                               &(Requirement->Minimum),
                                               &(Requirement->Characteristics),
                                               &(Requirement->Flags));

                    if (!KSUCCESS(Status)) {
                        goto FilterResourceRequirementsEnd;
                    }

                    Requirement->Maximum = Requirement->Minimum + 1;

                    ASSERT(Requirement->Length == 1);
                }

                //
                // Get the next resource requirement.
                //

                Requirement = IoGetNextResourceRequirement(RequirementList,
                                                           Requirement);
            }

            //
            // Get the next configuration.
            //

            RequirementList = IoGetNextResourceConfiguration(ConfigurationList,
                                                             RequirementList);
        }
    }

    //
    // Loop through every boot allocation as well.
    //

    AllocationList = Irp->U.QueryResources.BootAllocation;
    if (AllocationList != NULL) {
        Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
        while (Allocation != NULL) {

            //
            // If it's an interrupt line allocation, translate that up.
            //

            if (Allocation->Type == ResourceTypeInterruptLine) {
                Status = AcpipTranslateInterruptLine(
                                           DeviceObject,
                                           &(Allocation->Allocation),
                                           &(Allocation->Characteristics),
                                           &(Allocation->Flags));

                if (!KSUCCESS(Status)) {
                    goto FilterResourceRequirementsEnd;
                }

                ASSERT(Allocation->Length == 1);
            }

            //
            // Get the next allocation.
            //

            Allocation = IoGetNextResourceAllocation(AllocationList,
                                                     Allocation);
        }
    }

    Status = STATUS_SUCCESS;

FilterResourceRequirementsEnd:
    return Status;
}

KSTATUS
AcpipStartDevice (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    )

/*++

Routine Description:

    This routine starts an ACPI supported device.

Arguments:

    Device - Supplies a pointer to the device to query.

    DeviceObject - Supplies a pointer to the ACPI information associated with
        the system device.

    Irp - Supplies a pointer to the query resources IRP.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PACPI_DEVICE_DEPENDENCY Dependency;
    BOOL DeviceIsPciBus;
    PACPI_OBJECT PrtObject;
    PACPI_OBJECT PrtReturnValue;
    PACPI_OBJECT SrsObject;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    PrtObject = NULL;
    PrtReturnValue = NULL;
    SrsObject = NULL;
    if (DeviceObject->NamespaceObject == NULL) {
        Status = STATUS_SUCCESS;
        goto StartDeviceEnd;
    }

    if ((DeviceObject->Flags & ACPI_DEVICE_PROCESSOR) != 0) {
        Status = AcpipProcessorStart(DeviceObject);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    ASSERT((DeviceObject->NamespaceObject->Type == AcpiObjectDevice) ||
           (DeviceObject->NamespaceObject->Type == AcpiObjectProcessor));

    //
    // Attempt to find and execute the _SRS (Set Resource Settings) method.
    //

    SrsObject = AcpipFindNamedObject(DeviceObject->NamespaceObject,
                                     ACPI_METHOD__SRS);

    if ((SrsObject != NULL) &&
        (Irp->U.StartDevice.ProcessorLocalResources != NULL)) {

        ASSERT(DeviceObject->ResourceBuffer != NULL);

        //
        // If there is an _SRS method, then convert the processor resources
        // into an ACPI resource buffer.
        //

        Status = AcpipConvertToAcpiResourceBuffer(
                                    Irp->U.StartDevice.ProcessorLocalResources,
                                    DeviceObject->ResourceBuffer);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Status = AcpiExecuteMethod(SrsObject,
                                   &(DeviceObject->ResourceBuffer),
                                   1,
                                   0,
                                   NULL);

        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }
    }

    //
    // Attempt to get and save the device's bus address.
    //

    Status = AcpipGetDeviceBusAddress(DeviceObject->NamespaceObject,
                                      &(DeviceObject->BusAddress));

    if (!KSUCCESS(Status)) {
        DeviceObject->BusAddress = ACPI_INVALID_BUS_ADDRESS;
    }

    //
    // Check to see if this is a PCI device, and perform some additional work
    // if so.
    //

    DeviceIsPciBus = FALSE;
    if ((DeviceObject->NamespaceObject->U.Device.IsPciBus != FALSE) ||
        ((DeviceObject->Flags & ACPI_DEVICE_PCI_BRIDGE) != 0)) {

        DeviceIsPciBus = TRUE;

        //
        // Attempt to find and execute a _PRT (PCI Routing Table) method.
        //

        PrtObject = AcpipFindNamedObject(DeviceObject->NamespaceObject,
                                         ACPI_METHOD__PRT);

        if (PrtObject != NULL) {
            Status = AcpiExecuteMethod(PrtObject,
                                       NULL,
                                       0,
                                       AcpiObjectPackage,
                                       &PrtReturnValue);

            if (!KSUCCESS(Status)) {
                goto StartDeviceEnd;
            }

            //
            // Attempt to create a PCI routing table based on the PRT return
            // value.
            //

            if (DeviceObject->PciRoutingTable != NULL) {
                AcpipDestroyPciRoutingTable(DeviceObject->PciRoutingTable);
            }

            DeviceObject->PciRoutingTable =
                                    AcpipCreatePciRoutingTable(PrtReturnValue);

            if (DeviceObject->PciRoutingTable == NULL) {
                Status = STATUS_UNSUCCESSFUL;
                goto StartDeviceEnd;
            }
        }

        //
        // Acquire the PCI lock to synchronize with other parties doing early
        // PCI configuration space access.
        //

        AcpipAcquirePciLock();
    }

    DeviceObject->NamespaceObject->U.Device.IsDeviceStarted = TRUE;
    if (DeviceIsPciBus != FALSE) {
        AcpipReleasePciLock();
    }

    //
    // If there are any dependent devices, iterate through them to restart any
    // that were dependent on this device.
    //

    KeAcquireSpinLock(&AcpiDeviceListLock);
    CurrentEntry = AcpiDeviceDependencyList.Next;
    while (CurrentEntry != &AcpiDeviceDependencyList) {
        Dependency = LIST_VALUE(CurrentEntry,
                                ACPI_DEVICE_DEPENDENCY,
                                ListEntry);

        CurrentEntry = CurrentEntry->Next;
        if (Dependency->Dependency != DeviceObject->NamespaceObject) {
            continue;
        }

        //
        // Restart the dependent device.
        //

        Status = IoClearDeviceProblem(Dependency->DependentDevice);
        if (KSUCCESS(Status)) {
            LIST_REMOVE(&(Dependency->ListEntry));
            MmFreePagedPool(Dependency);
        }
    }

    KeReleaseSpinLock(&AcpiDeviceListLock);
    Status = STATUS_SUCCESS;

StartDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (DeviceObject->PciRoutingTable != NULL) {
            AcpipDestroyPciRoutingTable(DeviceObject->PciRoutingTable);
            DeviceObject->PciRoutingTable = NULL;
        }
    }

    if (SrsObject != NULL) {
        AcpipObjectReleaseReference(SrsObject);
    }

    if (PrtObject != NULL) {
        AcpipObjectReleaseReference(PrtObject);
    }

    if (PrtReturnValue != NULL) {
        AcpipObjectReleaseReference(PrtReturnValue);
    }

    return Status;
}

VOID
AcpipRemoveDevice (
    PACPI_DEVICE_CONTEXT Device
    )

/*++

Routine Description:

    This routine cleans up and destroys an ACPI device object.

Arguments:

    Device - Supplies a pointer to the device object.

Return Value:

    None.

--*/

{

    ULONG ChildIndex;

    //
    // Pull this structure out of the namespace object.
    //

    if (Device->NamespaceObject != NULL) {

        ASSERT(Device->NamespaceObject->Type == AcpiObjectDevice);

        Device->NamespaceObject->U.Device.OsDevice = NULL;
        Device->NamespaceObject->U.Device.DeviceContext = NULL;
    }

    Device->NamespaceObject = NULL;
    Device->ParentObject = NULL;
    Device->OsDevice = NULL;
    for (ChildIndex = 0; ChildIndex < Device->ChildCount; ChildIndex += 1) {
        if (Device->ChildArray[ChildIndex].NamespaceObject != NULL) {
            AcpipObjectReleaseReference(
                               Device->ChildArray[ChildIndex].NamespaceObject);
        }
    }

    if (Device->ChildArray != NULL) {
        MmFreePagedPool(Device->ChildArray);
        Device->ChildArray = NULL;
    }

    Device->ChildCount = 0;
    if (Device->ResourceBuffer != NULL) {
        AcpipObjectReleaseReference(Device->ResourceBuffer);
        Device->ResourceBuffer = NULL;
    }

    if (Device->PciRoutingTable != NULL) {
        AcpipDestroyPciRoutingTable(Device->PciRoutingTable);
    }

    LIST_REMOVE(&(Device->ListEntry));
    Device->ListEntry.Next = NULL;
    MmFreePagedPool(Device);
    return;
}

KSTATUS
AcpipGetDeviceBusAddress (
    PACPI_OBJECT Device,
    PULONGLONG BusAddress
    )

/*++

Routine Description:

    This routine determines the device ID of a given ACPI device namespace
    object by executing the _ADR method.

Arguments:

    Device - Supplies a pointer to the device namespace object.

    BusAddress - Supplies a pointer where the device's bus address number will
        be returned.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT AddressMethod;
    PACPI_OBJECT AddressMethodReturnValue;
    ULONGLONG ReturnValue;
    KSTATUS Status;

    if (Device->Type != AcpiObjectDevice) {

        ASSERT(Device->Type == AcpiObjectProcessor);

        return STATUS_NOT_FOUND;
    }

    AddressMethodReturnValue = NULL;
    AddressMethod = NULL;
    ReturnValue = (ULONGLONG)-1;

    //
    // Attempt to find the _ADR function.
    //

    AddressMethod = AcpipFindNamedObject(Device, ACPI_METHOD__ADR);
    if (AddressMethod == NULL) {
        Status = STATUS_NOT_FOUND;
        goto GetDeviceHardwareIdEnd;
    }

    //
    // Execute the _ADR function.
    //

    Status = AcpiExecuteMethod(AddressMethod,
                               NULL,
                               0,
                               AcpiObjectInteger,
                               &AddressMethodReturnValue);

    if (!KSUCCESS(Status)) {
        goto GetDeviceHardwareIdEnd;
    }

    if (AddressMethodReturnValue == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto GetDeviceHardwareIdEnd;
    }

    //
    // Pluck out the bus number.
    //

    if (AddressMethodReturnValue->Type == AcpiObjectInteger) {
        ReturnValue = AddressMethodReturnValue->U.Integer.Value;

    } else {
        Status = STATUS_UNEXPECTED_TYPE;
        goto GetDeviceHardwareIdEnd;
    }

    Status = STATUS_SUCCESS;

GetDeviceHardwareIdEnd:
    if (AddressMethodReturnValue != NULL) {
        AcpipObjectReleaseReference(AddressMethodReturnValue);
    }

    *BusAddress = ReturnValue;
    return Status;
}

KSTATUS
AcpipGetDeviceStatus (
    PACPI_OBJECT Device,
    PULONG DeviceStatus
    )

/*++

Routine Description:

    This routine attempts to find and execute the _STA method under a device.
    If no such method exists, the default status value will be returned as
    defined by ACPI.

Arguments:

    Device - Supplies a pointer to the device namespace object to query.

    DeviceStatus - Supplies a pointer where the device status will be returned
        on success.

Return Value:

    Status code. Failure here indicates a serious problem, not just a
    non-functional or non-existant device status.

--*/

{

    KSTATUS Status;
    PACPI_OBJECT StatusObject;
    PACPI_OBJECT StatusReturnValue;

    *DeviceStatus = ACPI_DEFAULT_DEVICE_STATUS;
    Status = STATUS_SUCCESS;
    if (Device->Type != AcpiObjectDevice) {

        ASSERT(Device->Type == AcpiObjectProcessor);

        goto GetDeviceStatusEnd;
    }

    //
    // Attempt to find and execute the status object.
    //

    StatusObject = AcpipFindNamedObject(Device,
                                        ACPI_METHOD__STA);

    if (StatusObject != NULL) {
        Status = AcpiExecuteMethod(StatusObject,
                                   NULL,
                                   0,
                                   AcpiObjectInteger,
                                   &StatusReturnValue);

        if (!KSUCCESS(Status)) {
            goto GetDeviceStatusEnd;
        }

        if (StatusReturnValue != NULL) {
            if (StatusReturnValue->Type == AcpiObjectInteger) {
                *DeviceStatus = (ULONG)StatusReturnValue->U.Integer.Value;
            }

            AcpipObjectReleaseReference(StatusReturnValue);
        }
    }

GetDeviceStatusEnd:
    return Status;
}

KSTATUS
AcpipEnableAcpiMode (
    VOID
    )

/*++

Routine Description:

    This routine enables ACPI mode on the given system. This routine only needs
    to be called once on initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PDPC EnableAcpiModeDpc;
    PFADT FadtTable;
    ULONG Pm1Register;
    KSTATUS Status;
    ULONGLONG Timeout;

    //
    // Find the FADT table.
    //

    FadtTable = AcpiFadtTable;

    ASSERT(FadtTable != NULL);

    //
    // If the SMI command register is unavailable, just return now.
    //

    if (FadtTable->SmiCommandPort == 0) {
        Status = STATUS_SUCCESS;
        goto EnableAcpiModeEnd;
    }

    Status = AcpipReadPm1ControlRegister(&Pm1Register);
    if (!KSUCCESS(Status)) {
        goto EnableAcpiModeEnd;
    }

    //
    // If SCI_EN is already set, then the system is already in ACPI mode and no
    // action is needed.
    //

    if ((Pm1Register & FADT_PM1_CONTROL_SCI_ENABLED) != 0) {
        Status = STATUS_SUCCESS;
        goto EnableAcpiModeEnd;
    }

    //
    // Write the ACPI enable value into the SMI_CMD register. SMI_CMD
    // operations must be issued synchronously from the boot processor. Issue
    // the command as a DPC on processor 0.
    //

    EnableAcpiModeDpc = KeCreateDpc(AcpipEnableAcpiModeDpc, FadtTable);
    if (EnableAcpiModeDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnableAcpiModeEnd;
    }

    KeQueueDpcOnProcessor(EnableAcpiModeDpc, 0);

    //
    // Flush and then destroy the DPC.
    //

    KeFlushDpc(EnableAcpiModeDpc);
    KeDestroyDpc(EnableAcpiModeDpc);

    //
    // Wait for the SCI_EN bit to flip on.
    //

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * ENABLE_ACPI_TIMEOUT);

    Status = STATUS_TIMEOUT;
    do {
        Status = AcpipReadPm1ControlRegister(&Pm1Register);
        if (!KSUCCESS(Status)) {
            goto EnableAcpiModeEnd;
        }

        if ((Pm1Register & FADT_PM1_CONTROL_SCI_ENABLED) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() <= Timeout);

EnableAcpiModeEnd:
    return Status;
}

KSTATUS
AcpipCreateDeviceDependency (
    PDEVICE DependentDevice,
    PACPI_OBJECT Provider
    )

/*++

Routine Description:

    This routine creates a device dependency. ACPI will attempt to restart the
    given device once its dependency has come online.

Arguments:

    DependentDevice - Supplies a pointer to the OS device that is dependent on
        something else.

    Provider - Supplies a pointer to the ACPI object containing the device that
        satisfies the dependency.

Return Value:

    STATUS_TOO_LATE if the device actually did start in the meantime.

    Status code.

--*/

{

    PACPI_DEVICE_DEPENDENCY Dependency;
    KSTATUS Status;

    ASSERT((DependentDevice != NULL) && (Provider != NULL));

    Dependency = MmAllocatePagedPool(sizeof(ACPI_DEVICE_DEPENDENCY),
                                     ACPI_ALLOCATION_TAG);

    if (Dependency == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Dependency, sizeof(ACPI_DEVICE_DEPENDENCY));
    Dependency->DependentDevice = DependentDevice;
    Dependency->Dependency = Provider;

    //
    // Check one more time after acquiring the lock in case the device
    // suddenly came online during the gap.
    //

    KeAcquireSpinLock(&AcpiDeviceListLock);

    //
    // If the routing device suddenly appeared, back out and keep going.
    //

    if (Provider->U.Device.IsDeviceStarted != FALSE) {
        KeReleaseSpinLock(&AcpiDeviceListLock);
        MmFreePagedPool(Dependency);
        Status = STATUS_TOO_LATE;

    //
    // Otherwise, add this dependency entry and fail for now. This device
    // will get restarted when the dependency comes online.
    //

    } else {
        INSERT_BEFORE(&(Dependency->ListEntry),
                      &AcpiDeviceDependencyList);

        KeReleaseSpinLock(&AcpiDeviceListLock);
        Status = STATUS_SUCCESS;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AcpipEnableAcpiModeDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DPC routine that is run in order to enable ACPI
    mode from processor 0.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PFADT FadtTable;

    FadtTable = (PFADT)Dpc->UserData;

    //
    // Write the ACPI enable value into the SMI_CMD register.
    //

    HlIoPortOutByte((USHORT)FadtTable->SmiCommandPort, FadtTable->AcpiEnable);
    return;
}

KSTATUS
AcpipGetDeviceHardwareId (
    PACPI_OBJECT Device,
    PSTR *DeviceHardwareId
    )

/*++

Routine Description:

    This routine determines the device ID of a given ACPI device namespace
    object by executing the _HID method.

Arguments:

    Device - Supplies a pointer to the device namespace object.

    DeviceHardwareId - Supplies a pointer where a pointer to a string
        containing the hardware ID will be returned on success. This string
        will be allocated in paged pool, and it is the responsibility of the
        caller to free it.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_NOT_CONNECTED if the ACPI device did not have a _UID method.

    Other status codes on error.

--*/

{

    ULONG AllocationSize;
    PSTR HardwareId;
    ULONG HardwareIdInteger;
    PACPI_OBJECT HidMethod;
    PACPI_OBJECT HidMethodReturnValue;
    KSTATUS Status;

    HidMethodReturnValue = NULL;
    HardwareId = NULL;

    //
    // If this is actually a processor object, then return the ACPI processor
    // reserved ID.
    //

    if (Device->Type == AcpiObjectProcessor) {
        HardwareId = MmAllocatePagedPool(sizeof(ACPI_PROCESSOR_DEVICE_ID),
                                         ACPI_ALLOCATION_TAG);

        if (HardwareId == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetDeviceHardwareIdEnd;
        }

        RtlStringCopy(HardwareId,
                      ACPI_PROCESSOR_DEVICE_ID,
                      sizeof(ACPI_PROCESSOR_DEVICE_ID));

        Status = STATUS_SUCCESS;
        goto GetDeviceHardwareIdEnd;
    }

    ASSERT(Device->Type == AcpiObjectDevice);

    //
    // Attempt to find the _HID function.
    //

    HidMethod = AcpipFindNamedObject(Device, ACPI_METHOD__HID);
    if (HidMethod == NULL) {
        Status = STATUS_DEVICE_NOT_CONNECTED;
        goto GetDeviceHardwareIdEnd;
    }

    //
    // Execute the _HID function.
    //

    Status = AcpiExecuteMethod(HidMethod, NULL, 0, 0, &HidMethodReturnValue);
    if (!KSUCCESS(Status)) {
        goto GetDeviceHardwareIdEnd;
    }

    if (HidMethodReturnValue == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto GetDeviceHardwareIdEnd;
    }

    //
    // Convert to a device ID string if needed.
    //

    if (HidMethodReturnValue->Type == AcpiObjectInteger) {

        //
        // Convert from an EISA encoded ID to a string.
        //

        HardwareId = MmAllocatePagedPool(EISA_ID_STRING_LENGTH,
                                         ACPI_ALLOCATION_TAG);

        if (HardwareId == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetDeviceHardwareIdEnd;
        }

        HardwareIdInteger = HidMethodReturnValue->U.Integer.Value;
        AcpipConvertEisaIdToString(HardwareIdInteger, HardwareId);

        //
        // Remember if this is a PCI bus.
        //

        Device->U.Device.IsPciBus = AcpipIsDeviceIdPciBus(HardwareIdInteger);

    } else if (HidMethodReturnValue->Type == AcpiObjectString) {

        //
        // Allocate and initialize a copy of the string.
        //

        AllocationSize =
                    RtlStringLength(HidMethodReturnValue->U.String.String) + 1;

        HardwareId = MmAllocatePagedPool(AllocationSize, ACPI_ALLOCATION_TAG);
        if (HardwareId == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto GetDeviceHardwareIdEnd;
        }

        RtlCopyMemory(HardwareId,
                      HidMethodReturnValue->U.String.String,
                      AllocationSize);

    } else {
        Status = STATUS_UNEXPECTED_TYPE;
        goto GetDeviceHardwareIdEnd;
    }

    Status = STATUS_SUCCESS;

GetDeviceHardwareIdEnd:
    if (HidMethodReturnValue != NULL) {
        AcpipObjectReleaseReference(HidMethodReturnValue);
    }

    *DeviceHardwareId = HardwareId;
    return Status;
}

KSTATUS
AcpipQueryOsDeviceBusAddress (
    PDEVICE Device,
    PULONGLONG BusAddress
    )

/*++

Routine Description:

    This routine queries the given system device for the ACPI bus address
    interface, and returns the device's bus number if it supports it.

Arguments:

    Device - Supplies a pointer to the device to query.

    BusAddress - Supplies a pointer where the device's bus address number will
        be returned.

Return Value:

    Status code.

--*/

{

    INTERFACE_ACPI_BUS_ADDRESS BusAddressInterface;
    PIRP QueryInterfaceIrp;
    ULONGLONG ReturnValue;
    KSTATUS Status;

    ReturnValue = (ULONGLONG)-1;

    //
    // Allocate and send an IRP to the bus driver requesting access
    // to the ATA register interface.
    //

    QueryInterfaceIrp = IoCreateIrp(Device, IrpMajorStateChange, 0);
    if (QueryInterfaceIrp == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto QueryOsDeviceBusAddressEnd;
    }

    QueryInterfaceIrp->MinorCode = IrpMinorQueryInterface;
    QueryInterfaceIrp->U.QueryInterface.Interface = &AcpiBusAddressUuid;
    QueryInterfaceIrp->U.QueryInterface.InterfaceBuffer = &BusAddressInterface;
    QueryInterfaceIrp->U.QueryInterface.InterfaceBufferSize =
                                            sizeof(INTERFACE_ACPI_BUS_ADDRESS);

    Status = IoSendSynchronousIrp(QueryInterfaceIrp);
    if (!KSUCCESS(Status)) {
        goto QueryOsDeviceBusAddressEnd;
    }

    Status = IoGetIrpStatus(QueryInterfaceIrp);
    if (!KSUCCESS(Status)) {
        goto QueryOsDeviceBusAddressEnd;
    }

    ReturnValue = BusAddressInterface.BusAddress;
    Status = STATUS_SUCCESS;

QueryOsDeviceBusAddressEnd:
    if (QueryInterfaceIrp != NULL) {
        IoDestroyIrp(QueryInterfaceIrp);
    }

    *BusAddress = ReturnValue;
    return Status;
}

BOOL
AcpipIsDeviceIdPciBus (
    ULONG DeviceId
    )

/*++

Routine Description:

    This routine determines if the given EISA hardware ID integer represents
    a generic PCI or PCI Express bus or bridge.

Arguments:

    DeviceId - Supplies the integer representing the encoded EISA ID.

Return Value:

    TRUE if the ID is a PCI bus or bridge.

    FALSE if the ID is not a PCI object.

--*/

{

    if ((DeviceId == EISA_ID_PCI_EXPRESS_BUS) ||
        (DeviceId == EISA_ID_PCI_BUS)) {

        return TRUE;
    }

    return FALSE;
}

PPCI_ROUTING_TABLE
AcpipCreatePciRoutingTable (
    PACPI_OBJECT PrtPackage
    )

/*++

Routine Description:

    This routine creates a PCI Routing table based on the package that comes out
    of the _PRT object/method.

Arguments:

    PrtPackage - Supplies a pointer to the return value of the _PRT method. This
        object must be a package.

Return Value:

    Returns a pointer to the PCI routing table on success.

    NULL on failure.

--*/

{

    ULONG AllocationSize;
    ULONG EntryCount;
    ULONG EntryIndex;
    PACPI_OBJECT EntryPackage;
    PACPI_OBJECT Line;
    PPCI_ROUTING_TABLE RoutingTable;
    PACPI_OBJECT Slot;
    PACPI_OBJECT Source;
    PACPI_OBJECT SourceIndex;
    KSTATUS Status;

    RoutingTable = NULL;

    //
    // Fail now if this parameter is not a package or has no objects.
    //

    if ((PrtPackage->Type != AcpiObjectPackage) ||
        (PrtPackage->U.Package.ElementCount == 0)) {

        Status = STATUS_INVALID_PARAMETER;
        goto CreatePciRoutingTableEnd;
    }

    //
    // Create the table with enough entries.
    //

    EntryCount = PrtPackage->U.Package.ElementCount;
    AllocationSize = sizeof(PCI_ROUTING_TABLE) +
                     (EntryCount * sizeof(PCI_ROUTING_TABLE_ENTRY));

    RoutingTable = AcpipAllocateMemory(AllocationSize);
    if (RoutingTable == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePciRoutingTableEnd;
    }

    RoutingTable->EntryCount = EntryCount;
    RoutingTable->Entry = (PPCI_ROUTING_TABLE_ENTRY)(RoutingTable + 1);

    //
    // Loop through initializing all the entries.
    //

    for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
        EntryPackage = AcpipGetPackageObject(PrtPackage, EntryIndex, FALSE);
        if ((EntryPackage == NULL) ||
            (EntryPackage->Type != AcpiObjectPackage) ||
            (EntryPackage->U.Package.ElementCount != 4)) {

            Status = STATUS_INVALID_PARAMETER;
            goto CreatePciRoutingTableEnd;
        }

        //
        // Get the slot number.
        //

        Slot = AcpipGetPackageObject(EntryPackage, 0, FALSE);
        if ((Slot == NULL) || (Slot->Type != AcpiObjectInteger)) {
            Status = STATUS_INVALID_PARAMETER;
            goto CreatePciRoutingTableEnd;
        }

        RoutingTable->Entry[EntryIndex].Slot =
                                         (USHORT)(Slot->U.Integer.Value >> 16);

        //
        // Get the line number.
        //

        Line = AcpipGetPackageObject(EntryPackage, 1, FALSE);
        if ((Line == NULL) ||
            (Line->Type != AcpiObjectInteger) ||
            (Line->U.Integer.Value > 4)) {

            Status = STATUS_INVALID_PARAMETER;
            goto CreatePciRoutingTableEnd;
        }

        RoutingTable->Entry[EntryIndex].InterruptLine =
                                               (USHORT)(Line->U.Integer.Value);

        //
        // Get the source device, which can either be a device or an integer
        // (which should be zero).
        //

        Source = AcpipGetPackageObject(EntryPackage, 2, FALSE);
        if ((Source == NULL) ||
            ((Source->Type != AcpiObjectInteger) &&
             (Source->Type != AcpiObjectDevice))) {

            Status = STATUS_INVALID_PARAMETER;
            goto CreatePciRoutingTableEnd;
        }

        //
        // Get the source index before storing the source value.
        //

        SourceIndex = AcpipGetPackageObject(EntryPackage, 3, FALSE);
        if ((SourceIndex == NULL) || (SourceIndex->Type != AcpiObjectInteger)) {
            Status = STATUS_INVALID_PARAMETER;
            goto CreatePciRoutingTableEnd;
        }

        //
        // If the source is a device, then save the source index.
        //

        if (Source->Type == AcpiObjectDevice) {
            RoutingTable->Entry[EntryIndex].RoutingDevice = Source;
            RoutingTable->Entry[EntryIndex].RoutingDeviceResourceIndex =
                                         (ULONG)(SourceIndex->U.Integer.Value);

            RoutingTable->Entry[EntryIndex].GlobalSystemInterruptNumber = 0;

        //
        // The source is not a device, so the source index is actually a
        // Global System Interrupt number.
        //

        } else {
            RoutingTable->Entry[EntryIndex].RoutingDevice = NULL;
            RoutingTable->Entry[EntryIndex].RoutingDeviceResourceIndex = 0;
            RoutingTable->Entry[EntryIndex].GlobalSystemInterruptNumber =
                                         (ULONG)(SourceIndex->U.Integer.Value);
        }
    }

    Status = STATUS_SUCCESS;

CreatePciRoutingTableEnd:
    if (!KSUCCESS(Status)) {
        if (RoutingTable != NULL) {
            AcpipFreeMemory(RoutingTable);
            RoutingTable = NULL;
        }
    }

    return RoutingTable;
}

VOID
AcpipDestroyPciRoutingTable (
    PPCI_ROUTING_TABLE RoutingTable
    )

/*++

Routine Description:

    This routine destroys a PCI routing table.

Arguments:

    RoutingTable - Supplies a pointer to the routing table to release.

Return Value:

    None.

--*/

{

    AcpipFreeMemory(RoutingTable);
    return;
}

KSTATUS
AcpipTranslateInterruptLine (
    PACPI_DEVICE_CONTEXT Device,
    PULONGLONG InterruptLine,
    PULONGLONG InterruptLineCharacteristics,
    PULONGLONG InterruptLineFlags
    )

/*++

Routine Description:

    This routine runs the given interrupt line resource through any PCI
    routing tables.

Arguments:

    Device - Supplies a pointer to the device that owns the interrupt line.

    InterruptLine - Supplies a pointer to the initial local interrupt line on
        input. On output, this will contain the interrupt GSI after running
        it through any PCI routing tables.

    InterruptLineCharacteristics - Supplies a pointer to the interrupt line
        resource characteristics on input. Returns the tranlated interrupt
        line characteristics on output.

    InterruptLineFlags - Supplies a pointer to the interrupt line resource
        flags on input. Returns the tranlated interrupt line characteristics
        on output.

Return Value:

    Status code.

--*/

{

    ULONGLONG BusAddress;
    PACPI_OBJECT Child;
    PACPI_DEVICE_CONTEXT ChildContext;
    PACPI_OBJECT CurrentParent;
    PACPI_DEVICE_CONTEXT CurrentParentContext;
    PACPI_OBJECT NamespaceRoot;
    ULONGLONG PreviousInterrupt;
    ULONG Slot;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (AcpiDebugInterruptRouting != FALSE) {
        RtlDebugPrint("ACPI: Translating interrupt line for %s.\n",
                      IoGetDeviceId(Device->OsDevice));
    }

    CurrentParentContext = Device->ParentObject;
    CurrentParent = CurrentParentContext->NamespaceObject;
    ChildContext = Device;
    NamespaceRoot = AcpipGetSystemBusRoot();
    while (CurrentParent != NamespaceRoot) {

        //
        // Find a device with an ACPI namespace object attached to it.
        //

        while (CurrentParentContext->NamespaceObject == NULL) {
            ChildContext = CurrentParentContext;
            CurrentParentContext = CurrentParentContext->ParentObject;
        }

        CurrentParent = CurrentParentContext->NamespaceObject;
        Child = ChildContext->NamespaceObject;

        //
        // If the current parent is a PCI device, then the child will need to
        // be run through the PCI routing table.
        //

        if ((CurrentParent->Type == AcpiObjectDevice) &&
            ((CurrentParent->U.Device.IsPciBus != FALSE) ||
             ((CurrentParentContext->Flags & ACPI_DEVICE_PCI_BRIDGE) != 0))) {

            //
            // Get the bus address of the child, either using the OS or by
            // executing an ACPI method.
            //

            if (ChildContext->BusAddress == ACPI_INVALID_BUS_ADDRESS) {
                if (Child != NULL) {
                    Status = AcpipGetDeviceBusAddress(Child, &BusAddress);

                } else {
                    Status = AcpipQueryOsDeviceBusAddress(
                                                        ChildContext->OsDevice,
                                                        &BusAddress);
                }

                if (!KSUCCESS(Status)) {
                    goto TranslateInterruptLineEnd;
                }

                //
                // Cache the answer.
                //

                ChildContext->BusAddress = BusAddress;

            } else {
                BusAddress = ChildContext->BusAddress;
            }

            //
            // Run the child through the PCI Routing table.
            //

            if (CurrentParentContext->PciRoutingTable != NULL) {
                Status = AcpipApplyPciRoutingTable(
                                         Device,
                                         BusAddress,
                                         CurrentParentContext->PciRoutingTable,
                                         InterruptLine,
                                         InterruptLineCharacteristics,
                                         InterruptLineFlags);

                if (!KSUCCESS(Status)) {

                    //
                    // If a "not ready" status was returned, then the link node
                    // this device points to is not yet started. Anything else
                    // is a real error.
                    //

                    if (Status != STATUS_NOT_READY) {
                        RtlDebugPrint("ACPI: Failed to apply bus address "
                                      "0x%I64x to PCI routing table 0x%x: %d\n",
                                      BusAddress,
                                      CurrentParentContext->PciRoutingTable,
                                      Status);
                    }

                    goto TranslateInterruptLineEnd;
                }

                //
                // A PCI routing table is the final word.
                //

                break;

            //
            // There is no PCI routing table, but this is a PCI device. If it's
            // a bridge (not a bus), then swizzle the line. The formula for
            // swizzling lines is: ParentLine = (ChildLine + ChildSlot) % 4.
            // The plus and minus ones are there because the lines are
            // based.
            //

            } else {
                if ((CurrentParentContext->Flags &
                     ACPI_DEVICE_PCI_BRIDGE) != 0) {

                    ASSERT(CurrentParent->U.Device.IsPciBus == FALSE);
                    ASSERT((*InterruptLine >= 1) && (*InterruptLine <= 4));

                    PreviousInterrupt = *InterruptLine;
                    Slot = (USHORT)(BusAddress >> 16);
                    *InterruptLine -= 1;
                    *InterruptLine = (*InterruptLine + Slot) % 4;
                    *InterruptLine += 1;
                    if (AcpiDebugInterruptRouting != FALSE) {
                        RtlDebugPrint("Swizzling line %I64d through PCI bridge "
                                      "0x%x, Address 0x%I64x, New line "
                                      "%I64d.\n",
                                      PreviousInterrupt,
                                      CurrentParentContext,
                                      BusAddress,
                                      *InterruptLine);
                    }

                }
            }
        }

        //
        // Set the child to this parent, and get the next parent up.
        //

        ChildContext = CurrentParentContext;
        CurrentParentContext = CurrentParentContext->ParentObject;
    }

    Status = STATUS_SUCCESS;

TranslateInterruptLineEnd:
    return Status;
}

KSTATUS
AcpipApplyPciRoutingTable (
    PACPI_DEVICE_CONTEXT Device,
    ULONGLONG BusAddress,
    PPCI_ROUTING_TABLE RoutingTable,
    PULONGLONG Interrupt,
    PULONGLONG InterruptCharacteristics,
    PULONGLONG InterruptFlags
    )

/*++

Routine Description:

    This routine runs the given interrupt line resource through the given
    PCI routing table.

Arguments:

    Device - Supplies a pointer to the device the translation is being
        performed on behalf of. If the interrupt routing device the device
        routes through is not started, then a dependency is created for this
        device.

    BusAddress - Supplies the PCI bus address of the device whose interrupt
        is being routed.

    RoutingTable - Supplies a pointer to the PCI routing table being applied.

    Interrupt - Supplies a pointer to the interrupt in the PCI device being
        used. The resulting routing will be returned here on success.

    InterruptCharacteristics - Supplies a pointer to the interrupt
        characteristics on input. Contains the translated interrupt
        characteristics on output.

    InterruptFlags - Supplies a pointer to the interrupt resource's flags on
        input. Contains the tranlated interrupt resource flags on output.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_NO_SUCH_DEVICE if the PCI routing table does not have an entry for
    this bus address.

    STATUS_CONVERSION_FAILED if the interrupt routing device did not have
    interrupt resources.

    STATUS_NOT_READY if the interrupt routing device is not started.

--*/

{

    PRESOURCE_ALLOCATION Allocation;
    PPCI_ROUTING_TABLE_ENTRY Entry;
    ULONG EntryIndex;
    USHORT Line;
    ULONG ResourceIndex;
    PRESOURCE_ALLOCATION_LIST Resources;
    PDEVICE RoutingDevice;
    USHORT Slot;
    KSTATUS Status;

    Entry = NULL;
    Slot = (USHORT)(BusAddress >> 16);
    Line = *Interrupt;
    if (AcpiDebugInterruptRouting != FALSE) {
        RtlDebugPrint("Applying BusAddress 0x%I64x Line 0x%x through PRT "
                      "0x%x\n",
                      BusAddress,
                      Line,
                      RoutingTable);
    }

    //
    // The line had better be within INTA through INTD.
    //

    ASSERT((Line >= 1) && (Line <= 4));

    Line -= 1;

    //
    // Find the PCI routing table entry for this slot and interrupt line.
    //

    for (EntryIndex = 0;
         EntryIndex < RoutingTable->EntryCount;
         EntryIndex += 1) {

        if ((RoutingTable->Entry[EntryIndex].Slot == Slot) &&
            (RoutingTable->Entry[EntryIndex].InterruptLine == Line)) {

            Entry = &(RoutingTable->Entry[EntryIndex]);
            break;
        }
    }

    if (Entry == NULL) {
        return STATUS_NO_SUCH_DEVICE;
    }

    //
    // If the routing table is hooked directly up to a Global System Interrupt
    // number, then simply return that.
    //

    if (Entry->RoutingDevice == NULL) {
        *Interrupt = Entry->GlobalSystemInterruptNumber;
        if (AcpiDebugInterruptRouting != FALSE) {
            RtlDebugPrint("Routes to GSI 0x%I64x\n", *Interrupt);
        }

        return STATUS_SUCCESS;
    }

    //
    // Look up the routing device. Fail if it is not started or has no
    // resources.
    //

    ASSERT(Entry->RoutingDevice->Type == AcpiObjectDevice);

    if (Entry->RoutingDevice->U.Device.IsDeviceStarted == FALSE) {
        if (AcpiDebugInterruptRouting != FALSE) {
            RtlDebugPrint("Delaying because routing device 0x%x is not "
                          "started.\n",
                          Entry->RoutingDevice);
        }

        Status = AcpipCreateDeviceDependency(Device->OsDevice,
                                             Entry->RoutingDevice);

        if (Status != STATUS_TOO_LATE) {
            if (KSUCCESS(Status)) {
                Status = STATUS_NOT_READY;
            }

            return Status;
        }
    }

    RoutingDevice = Entry->RoutingDevice->U.Device.OsDevice;

    ASSERT(RoutingDevice != NULL);

    Resources = IoGetProcessorLocalResources(RoutingDevice);
    if (Resources == NULL) {
        return STATUS_CONVERSION_FAILED;
    }

    ResourceIndex = Entry->RoutingDeviceResourceIndex;
    Allocation = IoGetNextResourceAllocation(Resources, NULL);
    while ((Allocation != NULL) && (ResourceIndex != 0)) {
        ResourceIndex -= 1;
        Allocation = IoGetNextResourceAllocation(Resources, Allocation);
    }

    //
    // Fail if the result is not an interrupt line.
    //

    if ((Allocation == NULL) ||
        (Allocation->Type != ResourceTypeInterruptLine)) {

        return STATUS_CONVERSION_FAILED;
    }

    //
    // Return the interrupt number of the given resource.
    //

    *Interrupt = Allocation->Allocation;
    *InterruptCharacteristics = Allocation->Characteristics;
    *InterruptFlags = Allocation->Flags;
    if (AcpiDebugInterruptRouting != FALSE) {
        RtlDebugPrint("Routes to 0x%I64x 0x%I64x 0x%I64x\n",
                      *Interrupt,
                      *InterruptCharacteristics,
                      *InterruptFlags);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateOsDevice (
    PACPI_OBJECT NamespaceDevice,
    PDEVICE ParentDevice,
    PACPI_DEVICE_CONTEXT ParentDeviceContext,
    PDEVICE *OsDevice
    )

/*++

Routine Description:

    This routine creates a device in the operating system corresponding to the
    given namespace device.

Arguments:

    NamespaceDevice - Supplies a pointer to the namespace device.

    ParentDevice - Supplies a pointer to the OS device's parent.

    ParentDeviceContext - Supplies a pointer to the device context of the
        parent.

    OsDevice - Supplies a pointer where a pointer to the OS device will be
        returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_NOT_CONNECTED if the ACPI device did not have a _UID method.

    Other status codes on error.

--*/

{

    PSTR DeviceId;
    PACPI_DEVICE_CONTEXT NewChild;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT((NamespaceDevice->Type == AcpiObjectDevice) ||
           (NamespaceDevice->Type == AcpiObjectProcessor));

    DeviceId = NULL;
    NewChild = NULL;
    *OsDevice = NULL;

    //
    // Get the device ID.
    //

    Status = AcpipGetDeviceHardwareId(NamespaceDevice, &DeviceId);
    if (!KSUCCESS(Status)) {
        goto CreateOsDeviceEnd;
    }

    NewChild = MmAllocatePagedPool(sizeof(ACPI_DEVICE_CONTEXT),
                                   ACPI_ALLOCATION_TAG);

    if (NewChild == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateOsDeviceEnd;
    }

    RtlZeroMemory(NewChild, sizeof(ACPI_DEVICE_CONTEXT));
    NewChild->Flags |= ACPI_DEVICE_BUS_DRIVER;
    if (IoAreDeviceIdsEqual(ACPI_PROCESSOR_DEVICE_ID, DeviceId) != FALSE) {
        NewChild->Flags |= ACPI_DEVICE_PROCESSOR;
    }

    NewChild->NamespaceObject = NamespaceDevice;
    NewChild->BusAddress = ACPI_INVALID_BUS_ADDRESS;
    NewChild->ParentObject = ParentDeviceContext;
    KeAcquireSpinLock(&AcpiDeviceListLock);
    INSERT_AFTER(&(NewChild->ListEntry), &AcpiDeviceObjectListHead);
    KeReleaseSpinLock(&AcpiDeviceListLock);
    Status = IoCreateDevice(AcpiDriver,
                            NewChild,
                            ParentDevice,
                            DeviceId,
                            NULL,
                            NULL,
                            OsDevice);

    if (!KSUCCESS(Status)) {
        goto CreateOsDeviceEnd;
    }

    NewChild->OsDevice = *OsDevice;
    NewChild->NamespaceObject->U.Device.OsDevice = *OsDevice;
    NewChild->NamespaceObject->U.Device.DeviceContext = NewChild;

CreateOsDeviceEnd:
    if (DeviceId != NULL) {
        MmFreePagedPool(DeviceId);
    }

    if (!KSUCCESS(Status)) {
        if (NewChild != NULL) {
            MmFreePagedPool(NewChild);
        }
    }

    return Status;
}

BOOL
AcpipIsDevicePciBridge (
    PDEVICE Device
    )

/*++

Routine Description:

    This routine determines if the given device is a PCI bridge device.

Arguments:

    Device - Supplies a pointer to the OS device to check.

Return Value:

    TRUE if the device is a PCI bridge.

    FALSE otherwise.

--*/

{

    PCSTR ClassId;
    BOOL Match;

    ClassId = IoGetDeviceClassId(Device);
    if (ClassId == NULL) {
        return FALSE;
    }

    Match = RtlAreStringsEqual(ClassId,
                               PCI_SUBTRACTIVE_BRIDGE_CLASS_ID,
                               sizeof(PCI_SUBTRACTIVE_BRIDGE_CLASS_ID));

    if (Match != FALSE) {
        return TRUE;
    }

    Match = RtlAreStringsEqual(ClassId,
                               PCI_BRIDGE_CLASS_ID,
                               sizeof(PCI_BRIDGE_CLASS_ID));

    if (Match != FALSE) {
        return TRUE;
    }

    return FALSE;
}

