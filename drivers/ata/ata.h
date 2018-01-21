/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ata.h

Abstract:

    This header contains definitions for the AT Attachment (ATA) driver.

Author:

    Evan Green 4-Jun-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define ATA_ALLOCATION_TAG 0x21617441 // '!atA'

#define ATA_CABLE_COUNT 2
#define ATA_CHILD_COUNT (2 * ATA_CABLE_COUNT)

//
// Define the timeout in seconds before an ATA command expires.
//

#define ATA_TIMEOUT 60

//
// Define the timeout in microseconds before a device selection fails.
//

#define ATA_SELECT_TIMEOUT (60 * MICROSECONDS_PER_MILLISECOND)

//
// Define the amount of time in microseconds to wait for the selected device to
// set the status appropriately.
//

#define ATA_SELECT_STALL MICROSECONDS_PER_MILLISECOND

//
// Define the known locations of the ATA controller if the PCI BARs did not
// seem to specify them.
//

#define ATA_LEGACY_PRIMARY_IO_BASE          0x1F0
#define ATA_LEGACY_PRIMARY_CONTROL_BASE     0x3F6
#define ATA_LEGACY_SECONDARY_IO_BASE        0x170
#define ATA_LEGACY_SECONDARY_CONTROL_BASE   0x376

#define ATA_LEGACY_IO_SIZE                  8
#define ATA_LEGACY_CONTROL_SIZE             4

//
// Define the legacy interrupts assigned to the disk controller.
//

#define ATA_LEGACY_PRIMARY_INTERRUPT 14
#define ATA_LEGACY_SECONDARY_INTERRUPT 15
#define ATA_LEGACY_INTERRUPT_CHARACTERISTICS INTERRUPT_LINE_EDGE_TRIGGERED
#define ATA_LEGACY_VECTOR_CHARACTERISTICS INTERRUPT_VECTOR_EDGE_TRIGGERED

//
// Define the total size of the PRDT for all four disks.
//

#define ATA_PRDT_TOTAL_SIZE 0x1000
#define ATA_PRDT_DISK_SIZE (ATA_PRDT_TOTAL_SIZE / ATA_CABLE_COUNT)

//
// Define the boundary that DMA PRDT entries must not cross.
//

#define ATA_DMA_BOUNDARY 0x10000

//
// Define the flag set in the PRDT entry for the last descriptor.
//

#define ATA_DMA_LAST_DESCRIPTOR 0x8000

//
// Define conversions between the ATA register enum and the actual base
// register segments.
//

#define ATA_HIGH_ADDRESSING_OFFSET (AtaRegisterSectorCountHigh - 2)
#define ATA_HIGH_REGISTER_COUNT 4

#define ATA_CONTROL_REGISTER_OFFSET \
    (ATA_HIGH_ADDRESSING_OFFSET + ATA_HIGH_ADDRESSING_OFFSET)

#define ATA_BUS_MASTER_REGISTER_OFFSET AtaRegisterBusMasterCommand

#define ATA_BUS_MASTER_TABLE_REGISTER 0x4

//
// ATA Status register definitions.
//

#define ATA_STATUS_ERROR           0x01
#define ATA_STATUS_INDEX           0x02
#define ATA_STATUS_CORRECTED_ERROR 0x04
#define ATA_STATUS_DATA_REQUEST    0x08
#define ATA_STATUS_SEEK_COMPLETE   0x10
#define ATA_STATUS_FAULT           0x20
#define ATA_STATUS_DRIVE_READY     0x40
#define ATA_STATUS_BUSY            0x80

#define ATA_STATUS_BUSY_MASK (ATA_STATUS_BUSY | ATA_STATUS_DATA_REQUEST)
#define ATA_STATUS_ERROR_MASK (ATA_STATUS_ERROR | ATA_STATUS_FAULT)

//
// Define ATA control register bits.
//

#define ATA_CONTROL_HIGH_ORDER 0x80
#define ATA_CONTROL_SOFTWARE_RESET 0x04
#define ATA_CONTROL_INTERRUPT_DISABLE 0x02

//
// IDE Bus Master Status Register definitions.
//

//
// Set if the drive is active.
//

#define IDE_STATUS_ACTIVE 0x01

//
// Set if the last operation had an error.
//

#define IDE_STATUS_ERROR  0x02

//
// Set if the INTRQ signal is asserted.
//

#define IDE_STATUS_INTERRUPT 0x04

//
// Set if drive 0 can do DMA.
//

#define IDE_STATUS_DRIVE0_DMA 0x20

//
// Set if drive 1 can do DMA.
//

#define IDE_STATUS_DRIVE1_DMA 0x40

//
// Set if only simplex operations are supported.
//

#define IDE_STATUS_SIMPLEX_ONLY 0x80

//
// Define the IDE programming interface register offset and bits.
//

#define IDE_INTERFACE_OFFSET                        8
#define IDE_INTERFACE_SIZE                          sizeof(USHORT)
#define IDE_INTERFACE_PRIMARY_NATIVE_SUPPORTED      0x0800
#define IDE_INTERFACE_PRIMARY_NATIVE_ENABLED        0x0400
#define IDE_INTERFACE_SECONDARY_NATIVE_SUPPORTED    0x0200
#define IDE_INTERFACE_SECONDARY_NATIVE_ENABLED      0x0100

//
// Define the bus master command bits.
//

#define ATA_BUS_MASTER_COMMAND_DMA_ENABLE 0x01
#define ATA_BUS_MASTER_COMMAND_DMA_READ 0x08

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ATA_CONTEXT_TYPE {
    AtaContextInvalid,
    AtaControllerContext,
    AtaChildContext
} ATA_CONTEXT_TYPE, *PATA_CONTEXT_TYPE;

typedef struct _ATA_CONTROLLER ATA_CONTROLLER, *PATA_CONTROLLER;
typedef struct _ATA_CHILD ATA_CHILD, *PATA_CHILD;

//
// Define the ATA registers. Values >= 7 go the control base.
//

typedef enum _ATA_REGISTER {
    AtaRegisterData                     = 0x0,
    AtaRegisterError                    = 0x1,
    AtaRegisterFeatures                 = 0x1,
    AtaRegisterSectorCountLow           = 0x2,
    AtaRegisterLba0                     = 0x3,
    AtaRegisterLba1                     = 0x4,
    AtaRegisterLba2                     = 0x5,
    AtaRegisterDeviceSelect             = 0x6,
    AtaRegisterCommand                  = 0x7,
    AtaRegisterStatus                   = 0x7,
    AtaRegisterSectorCountHigh          = 0x8,
    AtaRegisterLba3                     = 0x9,
    AtaRegisterLba4                     = 0xA,
    AtaRegisterLba5                     = 0xB,
    AtaRegisterControl                  = 0xC,
    AtaRegisterAlternateStatus          = 0xC,
    AtaRegisterDeviceAddress            = 0xD,
    AtaRegisterBusMasterCommand         = 0xE,
    AtaRegisterBusMasterStatus          = 0x10,
    AtaRegisterBusMasterTableAddress    = 0x12,
} ATA_REGISTER, *PATA_REGISTER;

/*++

Structure Description:

    This structure defines the Physical Region Descriptor Table format, which
    tells the ATA bus mastering controller where the memory is to DMA to.

Members:

    PhysicalAddress - Stores the physical address to DMA to. This buffer
        cannot cross a 64k boundary.

    Size - Stores the size of the region in bytes. 0 is 64k.

    Flags - Stores flags, which should all be zero except the most significant
        bit, which indicates that this is the last entry in the PRDT.

--*/

#pragma pack(push, 1)

typedef struct _ATA_PRDT {
    ULONG PhysicalAddress;
    USHORT Size;
    USHORT Flags;
} PACKED ATA_PRDT, *PATA_PRDT;

#pragma pack(pop)

/*++

Structure Description:

    This structure defines register bases for one ATA channel.

Members:

    IoBase - Supplies the base I/O port of the I/O registers used for issuing
        ATA commands.

    ControlBase - Supplies the base I/O port of the control registers.

    BusMasterBase - Supplies the base I/O port of the bus master registers.

    InterruptDisable - Stores the bit to OR in to the control mask to indicate
        if interrupts are disabled or not.

    SelectedDevice - Stores the currently selected device.

    Lock - Stores a pointer to the lock used to synchronize this channel.

    Irp - Stores a pointer to the I/O IRP actively running on the channel.

    IoSize - Stores the size of this I/O operation.

    OwningChild - Stores a pointer to the child that has the channel locked.

    Prdt - Stores a pointer to the array of Physical Region Descriptor Table
        entries.

    PrdtPhysicalAddress - Stores the physical address of the PRDT.

--*/

typedef struct _ATA_CHANNEL {
    USHORT IoBase;
    USHORT ControlBase;
    USHORT BusMasterBase;
    UCHAR InterruptDisable;
    UCHAR SelectedDevice;
    PQUEUED_LOCK Lock;
    PIRP Irp;
    UINTN IoSize;
    PATA_CHILD OwningChild;
    PATA_PRDT Prdt;
    PHYSICAL_ADDRESS PrdtPhysicalAddress;
} ATA_CHANNEL, *PATA_CHANNEL;

/*++

Structure Description:

    This structure defines state associated with an ATA child device (the
    bus driver's context for a disk itself).

Members:

    Type - Stores the context type.

    Controller - Stores a pointer back up to the controller.

    Channel - Stores a pointer to the channel context.

    OsDevice - Stores a pointer to the OS device.

    Slave - Stores whether or not this device is the slave device or the master
        device.

    DmaSupported - Stores a boolean indicating whether or not this device
        supports DMA transfers.

    Lba48Supported - Stores a boolean indicating whether or not LBA48 is
        supported.

    TotalSectors - Stores the total number of sectors in the device.

    DiskInterface - Stores the disk interface.

--*/

struct _ATA_CHILD {
    ATA_CONTEXT_TYPE Type;
    PATA_CONTROLLER Controller;
    PATA_CHANNEL Channel;
    PDEVICE OsDevice;
    UCHAR Slave;
    BOOL DmaSupported;
    BOOL Lba48Supported;
    ULONGLONG TotalSectors;
    DISK_INTERFACE DiskInterface;
};

/*++

Structure Description:

    This structure defines state associated with an ATA controller.

Members:

    Type - Stores the context type.

    PrimaryInterruptLine - Stores the interrupt line that this controller's
        primary (or only) interrupt comes in on.

    SecondaryInterruptLine - Stores the secondary interrupt line for the
        controller.

    PrimaryInterruptVector - Stores the interrupt vector that this controller's
        primary interrupt comes in on.

    SecondaryInterruptVector - Stores the secondary interrupt vector.

    PrimaryInterruptFound - Stores a boolean indicating whether or not the
        primary interrupt line and interrupt vector fields are valid.

    SecondaryInterruptFound - Stores a boolean indicating whether or not
        secondary interrupt resources were found.

    SkipFirstInterrupt - Stores a boolean indicating whether or not to skip the
        first interrupt line resource found in favor of legacy ones later.

    PrimaryInterruptHandle - Stores a pointer to the handle received when the
        primary interrupt was connected. If the device only has one interrupt,
        it will be this one.

    SecondaryInterruptHandle - Stores a pointer to the handle received when
        the secondary interrupt was connected. This is only used in
        "compatibility" mode.

    DpcLock - Stores a spinlock used to serialize DPC execution.

    Channel - Stores the array (size two) of ATA channels.

    PciConfigInterface - Stores the interface to access PCI configuration space.

    PciConfigInterfaceAvailable - Stores a boolean indicating if the PCI
        config interface is actively available.

    RegisteredForPciConfigInterfaces - Stores a boolean indicating whether or
        not the driver has regsistered for PCI Configuration Space interface
        access.

    Interface - Stores the programming interface of the controller.

    PrdtIoBuffer - Stores a pointer to the I/O buffer containing the
        physical region descriptor table.

    PendingStatus - Stores the pending bus master status register bits. The
        secondary controller's bits are shifted left by 8.

--*/

struct _ATA_CONTROLLER {
    ATA_CONTEXT_TYPE Type;
    ULONGLONG PrimaryInterruptLine;
    ULONGLONG SecondaryInterruptLine;
    ULONGLONG PrimaryInterruptVector;
    ULONGLONG SecondaryInterruptVector;
    BOOL PrimaryInterruptFound;
    BOOL SecondaryInterruptFound;
    BOOL SkipFirstInterrupt;
    HANDLE PrimaryInterruptHandle;
    HANDLE SecondaryInterruptHandle;
    KSPIN_LOCK DpcLock;
    ATA_CHANNEL Channel[ATA_CABLE_COUNT];
    PDEVICE ChildDevices[ATA_CHILD_COUNT];
    ATA_CHILD ChildContexts[ATA_CHILD_COUNT];
    INTERFACE_PCI_CONFIG_ACCESS PciConfigInterface;
    BOOL PciConfigInterfaceAvailable;
    BOOL RegisteredForPciConfigInterfaces;
    USHORT Interface;
    PIO_BUFFER PrdtIoBuffer;
    volatile ULONG PendingStatusBits;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

