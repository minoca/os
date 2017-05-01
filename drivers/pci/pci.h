/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pci.h

Abstract:

    This header contains definitions for the PCI (Peripheral Component
    Interconnect) driver.

Author:

    Evan Green 17-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/acpi.h>
#include <minoca/intrface/pci.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns the PCI class codes from the 4-byte register.
//

#define PCI_CLASS_CODE(_Register) (((_Register) >> 24) & 0x000000FF)
#define PCI_SUBCLASS_AND_INTERFACE(_Register) (((_Register) >> 8) & 0x0000FFFF)

//
// ---------------------------------------------------------------- Definitions
//

#define PCI_ALLOCATION_TAG 0x21696350 // '!icP'

#define PCI_BUS_ID "PNP0A03"
#define PCI_EXPRESS_BUS_ID "PNP0A08"
#define PCI_BRIDGE_CLASS_ID "PCIBridge"
#define PCI_SUBTRACTIVE_BRIDGE_CLASS_ID "PCIBridgeSubtractive"
#define PCI_DEVICE_ID_FORMAT "VEN_%04X&DEV_%04X"
#define PCI_DEVICE_ID_SIZE 18

#define MAX_PCI_FUNCTION 7
#define MAX_PCI_DEVICE 32
#define MAX_PCI_DEVICES ((MAX_PCI_FUNCTION + 1) * (MAX_PCI_DEVICE + 1))
#define PCI_INVALID_VENDOR_ID 0xFFFF

#define PCI_INITIAL_CHILD_COUNT 10

#define PCI_ROOT_CONFIG_ADDRESS 0xCF8
#define PCI_ROOT_CONFIG_DATA 0xCFC

#define PCI_BRIDGE_CLASS_CODE 0x06040000
#define PCI_SUBTRACTIVE_BRIDGE_CLASS_CODE 0x06040100

//
// PCI Configuration Space definitions.
//

#define PCI_ID_OFFSET 0x00
#define PCI_VENDOR_ID_MASK 0x0000FFFF
#define PCI_DEVICE_ID_SHIFT 16
#define PCI_DEVICE_ID_MASK 0xFFFF0000
#define PCI_CONTROL_OFFSET 0x04
#define PCI_STATUS_OFFSET 0x04
#define PCI_STATUS_MASK 0xFFFF0000
#define PCI_STATUS_SHIFT 16
#define PCI_REVISION_ID_OFFSET 0x8
#define PCI_REVISION_ID_MASK 0x000000FF
#define PCI_CLASS_CODE_OFFSET 0x08
#define PCI_CLASS_CODE_MASK 0xFFFFFF00
#define PCI_HEADER_TYPE_OFFSET 0x0C
#define PCI_HEADER_TYPE_MASK 0x00FF0000
#define PCI_HEADER_TYPE_SHIFT 16
#define PCI_BAR_OFFSET 0x10
#define PCI_BAR_COUNT 6
#define PCI_DEFAULT_CAPABILITIES_POINTER_OFFSET 0x34
#define PCI_ALTERNATE_CAPABILITIES_POINTER_OFFSET 0x14
#define PCI_INTERRUPT_LINE_OFFSET 0x3C

#define PCI_BRIDGE_BUS_NUMBERS_OFFSET 0x18
#define PCI_BRIDGE_IO_BAR_OFFSET 0x1C
#define PCI_BRIDGE_MEMORY_BAR_OFFSET 0x20
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BAR_OFFSET 0x24
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_HIGH_OFFSET 0x28
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_HIGH_OFFSET 0x2C
#define PCI_BRIDGE_IO_HIGH_BAR_OFFSET 0x30

#define PCI_BRIDGE_BUS_MASK 0x000000FF
#define PCI_BRIDGE_SECONDARY_BUS_SHIFT 8
#define PCI_BRIDGE_SECONDARY_BUS_MASK 0x0000FF00
#define PCI_BRIDGE_SUBORDINATE_BUS_SHIFT 16
#define PCI_BRIDGE_SUBORDINATE_BUS_MASK 0x00FF0000
#define PCI_BRIDGE_SECONDARY_LATENCY_TIMER_MASK 0xFF000000
#define PCI_BRIDGE_IO_BASE_DECODE_MASK 0x000F
#define PCI_BRIDGE_IO_BASE_DECODE_32_BIT 0x0001
#define PCI_BRIDGE_IO_BASE_MASK 0x00F0
#define PCI_BRIDGE_IO_LIMIT_DECODE_MASK 0x0F00
#define PCI_BRIDGE_IO_LIMIT_DECODE_32_BIT 0x0100
#define PCI_BRIDGE_IO_LIMIT_MASK 0xF000
#define PCI_BRIDGE_IO_BASE_ADDRESS_SHIFT 8
#define PCI_BRIDGE_IO_BASE_HIGH_MASK 0x0000FFFF
#define PCI_BRIDGE_IO_LIMIT_HIGH_MASK 0xFFFF0000
#define PCI_BRIDGE_IO_BASE_HIGH_ADDRESS_SHIFT 16
#define PCI_BRIDGE_MEMORY_BASE_MASK 0x0000FFF0
#define PCI_BRIDGE_MEMORY_BASE_ADDRESS_SHIFT 16
#define PCI_BRIDGE_MEMORY_LIMIT_MASK 0xFFF00000
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_DECODE_MASK 0x0000000F
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_DECODE_64_BIT 0x00000001
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_MASK 0x0000FFF0
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_BASE_ADDRESS_SHIFT 16
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_MASK 0xFFF00000
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_DECODE_MASK 0x000F0000
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_LIMIT_DECODE_64_BIT 0x00010000
#define PCI_BRIDGE_PREFETCHABLE_MEMORY_HIGH_ADDRESS_SHIFT 32
#define PCI_BRIDGE_IO_GRANULARITY 4096
#define PCI_BRIDGE_MEMORY_GRANULARITY (1024 * 1024)

//
// PCI Control register definitions.
//

#define PCI_CONTROL_IO_DECODE_ENABLED             0x0001
#define PCI_CONTROL_MEMORY_DECODE_ENABLED         0x0002
#define PCI_CONTROL_BUS_MASTER_ENABLED            0x0004
#define PCI_CONTROL_SPECIAL_CYCLES_ENABLED        0x0008
#define PCI_CONTROL_WRITE_INVALIDATE_ENABLED      0x0010
#define PCI_CONTROL_VGA_PALETTE_SNOOP_ENABLED     0x0020
#define PCI_CONTROL_PARITY_ERROR_RESPONSE_ENABLED 0x0040
#define PCI_CONTROL_STEPPING_CONTROL_ENABLED      0x0080
#define PCI_CONTROL_SERR_ENABLED                  0x0100
#define PCI_CONTROL_FAST_BACK_TO_BACK_ENABLED     0x0200
#define PCI_CONTROL_INTERRUPT_DISABLE             0x0400

//
// PCI Status register definitions.
//

#define PCI_STATUS_INTERRUPT_ASSERTED        0x0008
#define PCI_STATUS_CAPABILITIES_LIST         0x0010
#define PCI_STATUS_66MHZ_CAPABLE             0x0020
#define PCI_STATUS_FAST_BACK_TO_BACK_CAPABLE 0x0080
#define PCI_STATUS_MASTER_DATA_PARITY_ERROR  0x0100
#define PCI_STATUS_DEVSEL_TIMING_MASK        0x0600
#define PCI_STATUS_DEVSEL_TIMING_SHIFT       9
#define PCI_STATUS_DEVSEL_FAST               0x0
#define PCI_STATUS_DEVSEL_MEDIUM             0x1
#define PCI_STATUS_DEVSEL_SLOW               0x2
#define PCI_STATUS_TARGET_ABORT_SIGNALED     0x0800
#define PCI_STATUS_TARGET_ABORT_RECEIVED     0x1000
#define PCI_STATUS_MASTER_ABORT_RECEIVED     0x2000
#define PCI_STATUS_SYSTEM_ERROR_SIGNALED     0x4000
#define PCI_STATUS_PARITY_ERROR_DETECTED     0x8000

//
// PCI Base Address Register bit definitions.
//

#define PCI_BAR_MEMORY_FLAGS_MASK 0x0000000FULL
#define PCI_BAR_IO_FLAGS_MASK 0x00000003
#define PCI_BAR_IO_SPACE 0x00000001
#define PCI_BAR_MEMORY_SIZE_MASK 0x0000006
#define PCI_BAR_MEMORY_32_BIT 0x00000000
#define PCI_BAR_MEMORY_1MB 0x00000002
#define PCI_BAR_MEMORY_64_BIT 0x00000004
#define PCI_BAR_MEMORY_PREFETCHABLE 0x00000008

//
// PCI classes.
//

#define PCI_CLASS_UNKNOWN                   0x00
#define PCI_CLASS_MASS_STORAGE              0x01
#define PCI_CLASS_NETWORK                   0x02
#define PCI_CLASS_DISPLAY                   0x03
#define PCI_CLASS_MULTIMEDIA                0x04
#define PCI_CLASS_MEMORY                    0x05
#define PCI_CLASS_BRIDGE                    0x06
#define PCI_CLASS_SIMPLE_COMMUNICATION      0x07
#define PCI_CLASS_GENERAL_PERIPHERAL        0x08
#define PCI_CLASS_INPUT                     0x09
#define PCI_CLASS_DOCKING_STATION           0x0A
#define PCI_CLASS_PROCESSOR                 0x0B
#define PCI_CLASS_SERIAL_BUS                0x0C
#define PCI_CLASS_WIRELESS                  0x0D
#define PCI_CLASS_INTELLIGENT_IO            0x0E
#define PCI_CLASS_SATELLITE_COMMUNICATION   0x0F
#define PCI_CLASS_ENCRYPTION                0x10
#define PCI_CLASS_DATA_ACQUISITION          0x11
#define PCI_CLASS_VENDOR                    0xFF

//
// PCI subclasses (and interfaces).
//

#define PCI_CLASS_UNKNOWN_NON_VGA 0x0000
#define PCI_CLASS_UNKNOWN_VGA 0x0100

#define PCI_CLASS_MASS_STORAGE_IDE_MASK 0xFF00
#define PCI_CLASS_MASS_STORAGE_IDE 0x0100
#define PCI_CLASS_MASS_STORAGE_SATA 0x0601

#define PCI_CLASS_MULTIMEDIA_AUDIO 0x0300

#define PCI_CLASS_BRIDGE_ISA 0x0100
#define PCI_CLASS_BRIDGE_PCI 0x0400
#define PCI_CLASS_BRIDGE_PCI_SUBTRACTIVE 0x0401

#define PCI_CLASS_SIMPLE_COMMUNICATION_XT_UART 0x0000
#define PCI_CLASS_SIMPLE_COMMUNICATION_16450 0x0001
#define PCI_CLASS_SIMPLE_COMMUNICATION_16550 0x0002
#define PCI_CLASS_SIMPLE_COMMUNICATION_PARALLEL 0x0100
#define PCI_CLASS_SIMPLE_COMMUNICATION_BIDIRECTIONAL_PARALLEL 0x0101
#define PCI_CLASS_SIMPLE_COMMUNICATION_ECP_PARALLEL 0x0102
#define PCI_CLASS_SIMPLE_COMMUNICATION_OTHER 0x8000

#define PCI_CLASS_SERIAL_BUS_USB_UHCI 0x0300
#define PCI_CLASS_SERIAL_BUS_USB_OHCI 0x0310
#define PCI_CLASS_SERIAL_BUS_USB_EHCI 0x0320

#define PCI_CLASS_GENERAL_SD_HOST_NO_DMA 0x0500
#define PCI_CLASS_GENERAL_SD_HOST        0x0501

//
// Header type definitions.
//

#define PCI_HEADER_TYPE_STANDARD          0x00
#define PCI_HEADER_TYPE_PCI_TO_PCI_BRIDGE 0x01
#define PCI_HEADER_TYPE_CARDBUS_BRIDGE    0x02
#define PCI_HEADER_TYPE_VALUE_MASK        0x7F

//
// Header type flags.
//

#define PCI_HEADER_TYPE_FLAG_MULTIPLE_FUNCTIONS 0x80

//
// Define the PCI capability pointer mask. The bottom two bits are reserved.
//

#define PCI_CAPABILITY_POINTER_MASK 0xFC

//
// PCI capability list definitions.
//

#define PCI_CAPABILITY_LIST_ID_MASK            0x00FF
#define PCI_CAPABILITY_LIST_ID_SHIFT           0
#define PCI_CAPABILITY_LIST_NEXT_POINTER_MASK  0xFF00
#define PCI_CAPABILITY_LIST_NEXT_POINTER_SHIFT 8

//
// PCI capability definitions.
//

#define PCI_CAPABILITY_POWER_MANAGEMENT_INTERFACE   0x01
#define PCI_CAPABILITY_ACCELERATED_GRAPHICS_PORT    0x02
#define PCI_CAPABILITY_VITAL_PRODUCT_DATA           0x03
#define PCI_CAPABILITY_SLOT_IDENTIFICATION          0x04
#define PCI_CAPABILITY_MSI                          0x05
#define PCI_CAPABILITY_COMPACT_PCI_HOT_SWAP         0x06
#define PCI_CAPABILITY_PCI_X                        0x07
#define PCI_CAPABILITY_HYPER_TRANSPORT              0x08
#define PCI_CAPABILITY_VENDOR_SPECIFIC              0x09
#define PCI_CAPABILITY_DEBUG_PORT                   0x0A
#define PCI_CAPABILITY_COMPACT_PCI_CONTROL          0x0B
#define PCI_CAPABILITY_HOT_PLUG                     0x0C
#define PCI_CAPABILITY_BRIDGE_SUBSYSTEM_VENDOR_ID   0x0D
#define PCI_CAPABILITY_ACCELERATED_GRAPHICS_PORT_8X 0x0E
#define PCI_CAPABILITY_SECURE_DEVICE                0x0F
#define PCI_CAPABILITY_PCI_EXPRESS                  0x10
#define PCI_CAPABILITY_MSI_X                        0x11

//
// Define the PCI MSI flags.
//

#define PCI_MSI_FLAG_64_BIT_CAPABLE 0x00000001
#define PCI_MSI_FLAG_MASKABLE       0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
ULONGLONG
(*PPCI_READ_CONFIG) (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    );

/*++

Routine Description:

    This routine reads from PCI Configuration Space.

Arguments:

    Bus - Supplies the bus number to read from.

    Device - Supplies the device number to read from. Valid values are 0 to 31.

    Function - Supplies the PCI function to read from. Valid values are 0 to 7.

    Register - Supplies the configuration register to read from.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

Return Value:

    Returns the value read from the bus, or 0xFFFFFFFF on error.

--*/

typedef
VOID
(*PPCI_WRITE_CONFIG) (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine writes to PCI Configuration Space.

Arguments:

    Bus - Supplies the bus number to write to.

    Device - Supplies the device number to write to. Valid values are 0 to 31.

    Function - Supplies the PCI function to write to. Valid values are 0 to 7.

    Register - Supplies the configuration register to write to.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write to the register.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a PCI child device.

Members:

    DeviceNumber - Stores the device/slot number on the parent bus.

    Function - Stores the function number on the bus.

    VendorId - Stores the Vendor ID of the device.

    DeviceId - Stores the Device ID.

--*/

typedef struct _PCI_CHILD {
    UCHAR DeviceNumber;
    UCHAR Function;
    USHORT VendorId;
    USHORT DeviceId;
} PCI_CHILD, *PPCI_CHILD;

//
// This enum is a touch confusing, in that when the device type is function,
// then PCI acts as the bus driver. When the device type is bus or bridge, PCI
// is acting as the functional driver.
//

typedef enum _PCI_DEVICE_TYPE {
    PciDeviceInvalid,
    PciDeviceBus,
    PciDeviceBridge,
    PciDeviceFunction,
} PCI_DEVICE_TYPE, *PPCI_DEVICE_TYPE;

/*++

Structure Description:

    This structure defines the set of PCI Base Address Registers, also known as
    BARs.

Members:

    U - Stores the union of six 32 bit BARs or three 64 bit BARs.

--*/

typedef struct _PCI_BASE_ADDRESS_REGISTER_SET {
    union {
        ULONG Bar32[PCI_BAR_COUNT];
        ULONGLONG Bar64[PCI_BAR_COUNT / 2];
    } U;

} PCI_BASE_ADDRESS_REGISTER_SET, *PPCI_BASE_ADDRESS_REGISTER_SET;

/*++

Structure Description:

    This structure defines a PCI device's MSI/MSI-X context.

Members:

    MsiOffset - Stores the offset into configuration space of the MSI
        capability. A value of 0 indicates that it does not exist.

    MsiXOffset - Stores the offset into configuration space of the MSI-X
        capability. A value of 0 indicates that it does not exist.

    MsiFlags - Stores a bitmask of PCI MSI flags. See PCI_MSI_FLAG_* for
        definitions.

    MsiVectorCount - Stores the number of MSI vectors currently in use.

    MsiMaxVectorCount - Stores the maximum number of MSI vectors supported by
        the device.

    MsiXVectorCount - Stores the number of MSI-X vectors currently in use.

    MsiXMaxVectorCount - Stores the maximum number of MSI-X vectors supported
        by the device.

    MsiXTable - Stores a pointer to the mapped MSI-X vector table.

    MsiXPendingArray - Stores a pointer to the mapped MSI-X pending bit array.

    MsiXTablePhysicalAddress - Stores the physical address of the MSI-X vector
        table.

    MsiXPendingArrayPhysicalAddress - Stores the physical address of the MSI-X
        pending bit array.

    Interface - Stores a pointer to the MSI/MSI-X interface.

--*/

typedef struct _PCI_MSI_CONTEXT {
    UCHAR MsiOffset;
    UCHAR MsiXOffset;
    ULONG MsiFlags;
    ULONGLONG MsiVectorCount;
    ULONGLONG MsiMaxVectorCount;
    ULONGLONG MsiXVectorCount;
    ULONGLONG MsiXMaxVectorCount;
    volatile PVOID MsiXTable;
    volatile PVOID MsiXPendingArray;
    PHYSICAL_ADDRESS MsiXTablePhysicalAddress;
    PHYSICAL_ADDRESS MsiXPendingArrayPhysicalAddress;
    PINTERFACE_PCI_MSI Interface;
} PCI_MSI_CONTEXT, *PPCI_MSI_CONTEXT;

typedef struct _PCI_DEVICE PCI_DEVICE, *PPCI_DEVICE;

/*++

Structure Description:

    This structure defines a PCI device.

Members:

    Type - Stores which genre of PCI device this is.

    BusNumber - Stores the number of this device's PCI bus.

    DeviceNumber - Stores the slot number of this device.

    FunctionNumber - Stores the function number of this device.

    InterruptPin - Stores the interrupt pin that the function uses. This value
        comes from read-only configuration space.

    DeviceIsBridge - Stores a boolean indicating whether or not the device is a
        PCI bridge.

    ClassCode - Stores the class code of the device.

    Parent - Stores a pointer to the parent PCI device if there is one.

    Children - Stores an array allocated in paged pool of the device's children,
        if this device is a root bus or PCI bridge.

    ChildrenData - Stores an array allocated in paged pool of information about
        the device's children. This array parallels the Children array.

    ChildCount - Stores the number of children in the array.

    ChildSize - Stores the size of the children array, in elements.

    ReadConfig - Stores a pointer to a function used to read from configuration
        space for buses.

    WriteConfig - Stores a pointer to a function used to write to configuration
        space for buses.

    BarsRead - Stores a boolean indicating if the device's BARs have been read
        yet.

    BootConfiguration - Stores the state of the BARs as configured by the BIOS.

    BootControlRegister - Stores the value of the control register when the
        system was booted.

    AddressDecodeBits - Stores the values of the BARs after writing all ones
        to them and reading them back to see which ones stick.

    BarCount - Stores one higher than the index of the highest valid BAR.

    PciConfigInterface - Stores a pointer to the config interface.

    AcpiBusAddressInterface - Stores a pointer to the ACPI interface.

    SpecificPciConfigInterface - Stores a pointer to the specific PCI config
        space access interface.

    MsiContext - Stores a pointer to the MSI/MSI-X context if the device
        supports message based interrupts.

--*/

struct _PCI_DEVICE {
    PCI_DEVICE_TYPE Type;
    UCHAR BusNumber;
    UCHAR DeviceNumber;
    UCHAR FunctionNumber;
    UCHAR InterruptPin;
    BOOL DeviceIsBridge;
    ULONG ClassCode;
    PPCI_DEVICE Parent;
    PDEVICE *Children;
    PPCI_CHILD *ChildrenData;
    ULONG ChildCount;
    ULONG ChildSize;
    PPCI_READ_CONFIG ReadConfig;
    PPCI_WRITE_CONFIG WriteConfig;
    BOOL BarsRead;
    PCI_BASE_ADDRESS_REGISTER_SET BootConfiguration;
    USHORT BootControlRegister;
    PCI_BASE_ADDRESS_REGISTER_SET AddressDecodeBits;
    ULONG BarCount;
    PINTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    PINTERFACE_ACPI_BUS_ADDRESS AcpiBusAddressInterface;
    PINTERFACE_SPECIFIC_PCI_CONFIG_ACCESS SpecificPciConfigInterface;
    PPCI_MSI_CONTEXT MsiContext;
};

/*++

Structure Description:

    This structure defines the interface for returning a device's PCI bus
    driver's device structure. This is used internally by PCI to create a
    complete device tree.

Members:

    BusDevice - Stores a pointer to the bus driver context.

--*/

typedef struct _INTERFACE_PCI_BUS_DEVICE {
    PVOID BusDevice;
} INTERFACE_PCI_BUS_DEVICE, *PINTERFACE_PCI_BUS_DEVICE;

//
// -------------------------------------------------------------------- Globals
//

extern PDRIVER PciDriver;

//
// Store the UUID of the PCI MSI and MSI-X access.
//

extern UUID PciMessageSignaledInterruptsUuid;

//
// -------------------------------------------------------- Function Prototypes
//

ULONGLONG
PcipRootReadConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize
    );

/*++

Routine Description:

    This routine reads from PCI Configuration Space on the root PCI bus.

Arguments:

    Bus - Supplies the bus number to read from.

    Device - Supplies the device number to read from. Valid values are 0 to 31.

    Function - Supplies the PCI function to read from. Valid values are 0 to 7.

    Register - Supplies the configuration register to read from.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

Return Value:

    Returns the value read from the bus, or 0xFFFFFFFF on error.

--*/

VOID
PcipRootWriteConfig (
    UCHAR Bus,
    UCHAR Device,
    UCHAR Function,
    ULONG Register,
    ULONG AccessSize,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine writes to PCI Configuration Space on the PCI root bus.

Arguments:

    Bus - Supplies the bus number to write to.

    Device - Supplies the device number to write to. Valid values are 0 to 31.

    Function - Supplies the PCI function to write to. Valid values are 0 to 7.

    Register - Supplies the configuration register to write to.

    AccessSize - Supplies the size of the access to make. Valid values are 1,
        2, 4, and 8.

    Value - Supplies the value to write to the register.

Return Value:

    None.

--*/

KSTATUS
PcipMsiCreateContextAndInterface (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    );

/*++

Routine Description:

    This routine initializes the MSI/MSI-X context and interface for the given
    PCI device.

Arguments:

    Device - Supplies a pointer to the device in need of an MSI/MSI-X context
        and interface.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    Status code.

--*/

VOID
PcipMsiDestroyContextAndInterface (
    PDEVICE Device,
    PPCI_DEVICE PciDevice
    );

/*++

Routine Description:

    This routine destroys the given PCI device's MSI context and interface if
    they exist.

Arguments:

    Device - Supplies a pointer to the device whose MSI/MSI-x context and
        interface is to be destroyed.

    PciDevice - Supplies a pointer to the PCI device context.

Return Value:

    None.

--*/

VOID
PcipGetMsiXBarInformation (
    PPCI_DEVICE PciDevice,
    PULONG TableBarIndex,
    PULONG TableOffset,
    PULONG PendingArrayBarIndex,
    PULONG PendingArrayOffset
    );

/*++

Routine Description:

    This routine gets the BAR information for the MSI-X table and pending bit
    array out of PCI configuration space.

Arguments:

    PciDevice - Supplies a pointer to the PCI device context.

    TableBarIndex - Supplies a pointer that receives the index of the BAR
        within which the MSI-X table resides.

    TableOffset - Supplies a pointer that receives the offset within the BAR
        of the MSI-X table.

    PendingArrayBarIndex - Supplies a pointer that receives the index of the
        BAR within which the MSI-X pending bit array resides.

    PendingArrayOffset - Supplies a pointer that receives the offset within the
        BAR of the MSI-X pending bit array.

Return Value:

    None.

--*/

