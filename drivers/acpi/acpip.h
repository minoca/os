/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpip.h

Abstract:

    This header contains internal definitions for the ACPI support library.

Author:

    Evan Green 18-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "acpiobj.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ACPI_IMPLEMENTED_REVISION 5

//
// Define the names of the system bus and processor objects.
//

#define ACPI_SYSTEM_BUS_OBJECT_NAME 0x5F42535F  // '_SB_'
#define ACPI_PROCESSOR_OBJECT_NAME 0x5F52505F // '_PR_'

//
// Define the 4-byte names of the various standard ACPI functions.
//

#define ACPI_METHOD__HID 0x4449485F // '_HID'
#define ACPI_METHOD__ADR 0x5244415F // '_ADR'
#define ACPI_METHOD__PRS 0x5352505F // '_PRS'
#define ACPI_METHOD__CRS 0x5352435F // '_CRS'
#define ACPI_METHOD__SRS 0x5352535F // '_SRS'
#define ACPI_METHOD__PRT 0x5452505F // '_PRT'
#define ACPI_METHOD__STA 0x4154535F // '_STA'
#define ACPI_METHOD__INI 0x494E495F // '_INI'
#define ACPI_METHOD__PIC 0x4349505F // '_PIC'
#define ACPI_METHOD__UID 0x4449555F // '_HID'
#define ACPI_METHOD__CST 0x5453435F // '_CST'
#define ACPI_METHOD__OSC 0x43534F5F // '_OSC'
#define ACPI_METHOD__PDC 0x4344505F // '_PDC'
#define ACPI_METHOD__TTS 0x5354545F // '_TTS'
#define ACPI_METHOD__PTS 0x5354505F // '_PTS'

#define ACPI_OBJECT__S0 0x5F30535F // '_S0_'
#define ACPI_OBJECT__S1 0x5F31535F // '_S1_'
#define ACPI_OBJECT__S2 0x5F32535F // '_S2_'
#define ACPI_OBJECT__S3 0x5F33535F // '_S3_'
#define ACPI_OBJECT__S4 0x5F34535F // '_S4_'
#define ACPI_OBJECT__S5 0x5F35535F // '_S5_'

//
// Define recognized PCI bus object EISA identifiers.
//

#define EISA_ID_PCI_BUS 0x030AD041
#define EISA_ID_PCI_EXPRESS_BUS 0x080AD041

//
// Define an uninitialized bus address value.
//

#define ACPI_INVALID_BUS_ADDRESS (-1ULL)

//
// Define the operating system name. Use Windows because every BIOS recognizes
// it.
//

#define ACPI_OPERATING_SYSTEM_NAME "Microsoft Windows NT"

//
// Define PCI bridge class IDs.
//

#define PCI_BRIDGE_CLASS_ID "PCIBridge"
#define PCI_SUBTRACTIVE_BRIDGE_CLASS_ID "PCIBridgeSubtractive"

//
// Define flags in the ACPI device context.
//

//
// This flag is set if ACPI is the bus driver for the device.
//

#define ACPI_DEVICE_BUS_DRIVER 0x00000001

//
// This flag is set if this device is a PCI bridge. ACPI connects to all PCI
// devices even if they're not in the namespace so it can filter interrupts
// through the _PRT. If the device is a bridge ACPI needs to attach to all of
// its children
//

#define ACPI_DEVICE_PCI_BRIDGE 0x00000002

//
// This flag is set if this device is a processor device.
//

#define ACPI_DEVICE_PROCESSOR 0x00000004

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _ACPI_PROCESSOR_CONTEXT
    ACPI_PROCESSOR_CONTEXT, *PACPI_PROCESSOR_CONTEXT;

/*++

Structure Description:

    This structure stores information about an entry in a PCI routing table.

Members:

    Slot - Stores the PCI slot number of the entry being described.

    InterruptLine - Stores the interrupt line of the entry being described.
        Valid values are 0 through 3 and correspond to lines INTA through INTD.

    RoutingDevice - Stores a pointer to the namespace object of the routing
        device (link node) that this interrupt line routes through. This
        may be NULL if the interrupt line is directly connected to an
        interrupt controller.

    RoutingDeviceResourceIndex - Stores the zero-based resource index into
        the routing device's resources that represents the output routing of the
        slot's interrupt line.

    GlobalSystemInterruptNumber - Stores the global system interrupt number of
        the interrupt line when the line is directly connected to an
        interrupt controller (the routing device is NULL). If the routing
        device is not NULL, this parameter is unused.

--*/

typedef struct _PCI_ROUTING_TABLE_ENTRY {
    USHORT Slot;
    USHORT InterruptLine;
    PACPI_OBJECT RoutingDevice;
    ULONG RoutingDeviceResourceIndex;
    ULONG GlobalSystemInterruptNumber;
} PCI_ROUTING_TABLE_ENTRY, *PPCI_ROUTING_TABLE_ENTRY;

/*++

Structure Description:

    This structure stores a PCI routing table, used to determine the routing of
    interrupt lines on a PCI slot.

Members:

    EntryCount - Stores the number of entries in the PCI routing table.

    Entry - Stores an array of PCI routing table entries.

--*/

typedef struct _PCI_ROUTING_TABLE {
    ULONG EntryCount;
    PPCI_ROUTING_TABLE_ENTRY Entry;
} PCI_ROUTING_TABLE, *PPCI_ROUTING_TABLE;

/*++

Structure Description:

    This structure stores information about a child of an ACPI device.

Members:

    NamespaceObject - Stores a pointer to the namespace object that
        corresponds to this device.

    Device - Stores a pointer to the system device pointer that corresponds
        to this device.

--*/

typedef struct _ACPI_CHILD_DEVICE {
    PACPI_OBJECT NamespaceObject;
    PDEVICE Device;
} ACPI_CHILD_DEVICE, *PACPI_CHILD_DEVICE;

/*++

Structure Description:

    This structure stores information about an ACPI device that has been
    enumerated with the system.

Members:

    ListEntry - Stores pointers to the next and previous device objects in the
        global list.

    NamespaceObject - Stores a pointer to the namespace object that
        corresponds to this device object.

    ParentObject - Stores an pointer to the parent device context, which can
        be used to traverse up even when the device has no object in the ACPI
        namespace.

    OsDevice - Stores a pointer to the operating system device object.

    ChildArray - Stores a pointer to an array of ACPI child structures outlining
        the previously enumerated children.

    ChildCount - Stores the number of elements in the child array.

    HasDependentDevices - Stores a boolean indicating if this device has
        devices that delayed starting because they were waiting for this device
        to come up.

    Flags - Stores a bitfield of flags concerning the device. See ACPI_DEVICE_*
        definitions.

    ResourceBuffer - Stores a pointer to an ACPI buffer object that contains
        the resources as laid out by the CRS function. This same format is used
        in the SRS function to set resources.

    PciRoutingTable - Stores a pointer to the PCI routing table. This pointer
        will only be non-NULL for PCI bus devices, for everything else this
        pointer is unused.

    BusAddress - Stores the bus address of this device (the result of
        evaluating the _ADR method under the device).

    Processor - Stores a pointer to additional context if this device is a
        processor.

--*/

typedef struct _ACPI_DEVICE_CONTEXT ACPI_DEVICE_CONTEXT, *PACPI_DEVICE_CONTEXT;
struct _ACPI_DEVICE_CONTEXT {
    LIST_ENTRY ListEntry;
    PACPI_OBJECT NamespaceObject;
    PACPI_DEVICE_CONTEXT ParentObject;
    PDEVICE OsDevice;
    PACPI_CHILD_DEVICE ChildArray;
    ULONG ChildCount;
    ULONG Flags;
    PACPI_OBJECT ResourceBuffer;
    PPCI_ROUTING_TABLE PciRoutingTable;
    ULONGLONG BusAddress;
    PACPI_PROCESSOR_CONTEXT Processor;
};

/*++

Structure Description:

    This structure stores information about a device dependency.

Members:

    ListEntry - Stores pointers to the next and previous device dependencies
        in the global list.

    DependentDevice - Stores a pointer to the device that was not started
        because a device it depended on (like a link node) was not started.

    Dependency - Stores a pointer to the ACPI device being depended on.

--*/

typedef struct _ACPI_DEVICE_DEPENDENCY {
    LIST_ENTRY ListEntry;
    PDEVICE DependentDevice;
    PACPI_OBJECT Dependency;
} ACPI_DEVICE_DEPENDENCY, *PACPI_DEVICE_DEPENDENCY;

//
// -------------------------------------------------------------------- Globals
//

//
// Remember a pointer to the driver object returned by the system corresponding
// to this driver.
//

extern PDRIVER AcpiDriver;

//
// Store a pointer to the FADT.
//

extern PFADT AcpiFadtTable;

//
// Store a global list of ACPI device objects.
//

extern LIST_ENTRY AcpiDeviceObjectListHead;
extern LIST_ENTRY AcpiDeviceDependencyList;
extern KSPIN_LOCK AcpiDeviceListLock;

//
// Store the global ACPI objects for Zero, One, and Ones.
//

extern ACPI_OBJECT AcpiZero;
extern ACPI_OBJECT AcpiOne;
extern ACPI_OBJECT AcpiOnes32;
extern ACPI_OBJECT AcpiOnes64;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipInitializeAmlInterpreter (
    VOID
    );

/*++

Routine Description:

    This routine initializes the ACPI AML interpreter and global namespace.

Arguments:

    None.

Return Value:

    Status code.

--*/

UCHAR
AcpipChecksumData (
    PVOID Address,
    ULONG Length
    );

/*++

Routine Description:

    This routine sums all of the bytes in a given buffer. In a correctly
    checksummed region, the result should be zero.

Arguments:

    Address - Supplies the address of the region to checksum.

    Length - Supplies the length of the region, in bytes.

Return Value:

    Returns the sum of all the bytes in the region.

--*/

KSTATUS
AcpipEnumerateDeviceChildren (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    );

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

KSTATUS
AcpipQueryResourceRequirements (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    );

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

KSTATUS
AcpipFilterResourceRequirements (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    );

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

KSTATUS
AcpipStartDevice (
    PDEVICE Device,
    PACPI_DEVICE_CONTEXT DeviceObject,
    PIRP Irp
    );

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

VOID
AcpipRemoveDevice (
    PACPI_DEVICE_CONTEXT Device
    );

/*++

Routine Description:

    This routine cleans up and destroys an ACPI device object.

Arguments:

    Device - Supplies a pointer to the device object.

Return Value:

    None.

--*/

KSTATUS
AcpipGetDeviceBusAddress (
    PACPI_OBJECT Device,
    PULONGLONG BusAddress
    );

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

KSTATUS
AcpipGetDeviceStatus (
    PACPI_OBJECT Device,
    PULONG DeviceStatus
    );

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

KSTATUS
AcpipEnableAcpiMode (
    VOID
    );

/*++

Routine Description:

    This routine enables ACPI mode on the given system. This routine only needs
    to be called once on initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
AcpipCreateDeviceDependency (
    PDEVICE DependentDevice,
    PACPI_OBJECT Provider
    );

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

KSTATUS
AcpipInitializeSystemStateTransitions (
    VOID
    );

/*++

Routine Description:

    This routine initializes support for reboot and system power state
    transitions.

Arguments:

    None.

Return Value:

    Status code.

--*/

