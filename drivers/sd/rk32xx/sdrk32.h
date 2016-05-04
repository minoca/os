/*++

Copyright (c) 2015 Minoca Corp. All rights reserved.

Module Name:

    sdrk32.h

Abstract:

    This header contains internal definitions for the RK32xx SD/MMC controller.

Author:

    Chris Stevens 29-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/sd/sd.h>
#include <minoca/sd/sddwc.h>
#include <minoca/intrface/disk.h>
#include <minoca/intrface/rk808.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the UUID of the SD RK32xx vendor resource.
//

#define SD_RK32_VENDOR_RESOURCE_UUID \
    {{0x9439320C, 0xC6FA11E5, 0x9912BA0B, 0x0483C18E}}

//
// Define the minimum expected length of the HSMMC block.
//

#define SD_RK32_CONTROLLER_LENGTH 0x1000

//
// Define the offset into the HSMMC block where the SD standard registers
// start.
//

#define SD_RK32_CONTROLLER_SD_REGISTER_OFFSET 0x0

//
// Define the RK32 SD timeout in seconds.
//

#define SD_RK32_TIMEOUT 1

//
// Define the maximum number of retries the controller will attempt on an I/O
// transfer.
//

#define SD_RK32_MAX_IO_RETRIES 5

//
// Define the block size used by the RK32 SD.
//

#define SD_RK32_BLOCK_SIZE 512

//
// Define the size of the DMA descriptor table.
//

#define SD_RK32_DMA_DESCRIPTOR_COUNT 0x100
#define SD_RK32_DMA_DESCRIPTOR_TABLE_SIZE \
    (SD_RK32_DMA_DESCRIPTOR_COUNT * sizeof(SD_DWC_DMA_DESCRIPTOR))

//
// Define the set of flags for the parent SD device.
//

#define SD_RK32_DEVICE_FLAG_INSERTION_PENDING         0x00000001
#define SD_RK32_DEVICE_FLAG_REMOVAL_PENDING           0x00000002

//
// Define the set of flags for the child SD disk.
//

#define SD_RK32_CHILD_FLAG_MEDIA_PRESENT 0x00000001
#define SD_RK32_CHILD_FLAG_DMA_SUPPORTED 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_RK32_DEVICE_TYPE {
    SdRk32InvalidDeviceType,
    SdRk32Parent,
    SdRk32Child
} SD_RK32_DEVICE_TYPE, *PSD_RK32_DEVICE_TYPE;

typedef struct _SD_RK32_CONTEXT SD_RK32_CONTEXT, *PSD_RK32_CONTEXT;

/*++

Structure Description:

    This structure describes the SD RK32xx child device context.

Members:

    Type - Stores the device type, child in this case.

    ReferenceCount - Stores a reference count for the child.

    Parent - Stores a pointer to the parent structure.

    Device - Stores a pointer to the OS device.

    Controller - Stores an pointer to the controller structure.

    ControllerLock - Stores a pointer to a lock used to serialize I/O requests.

    Irp - Stores a pointer to the current IRP being processed.

    Retries - Stores the retry count for the current I/O operation.

    Flags - Stores a bitmask of flags for the disk. See SD_RK32_CHILD_FLAG_*
        for definitions.

    BlockShift - Stores the cached block size shift of the media.

    BlockCount - Stores the cached block count of the media.

    DiskInterface - Stores the disk interface presented to the system.

--*/

typedef struct _SD_RK32_CHILD {
    SD_RK32_DEVICE_TYPE Type;
    volatile ULONG ReferenceCount;
    PSD_RK32_CONTEXT Parent;
    PDEVICE Device;
    PSD_CONTROLLER Controller;
    PQUEUED_LOCK ControllerLock;
    PIRP Irp;
    ULONG Retries;
    ULONG Flags;
    ULONG BlockShift;
    ULONGLONG BlockCount;
    DISK_INTERFACE DiskInterface;
} SD_RK32_CHILD, *PSD_RK32_CHILD;

/*++

Structure Description:

    This structure describes the SD RK32xx device context.

Members:

    Type - Stores the device type, parent in this case.

    Controller - Stores an pointer to the controller structure.

    ControllerBase - Stores a pointer to the virtual address of the HSMMC
        registers.

    PhysicalAddress - Stores the physical address of the controller.

    FundamentalClock - Stores the frequency of the SD/MMC fundamental clock, in
        Hertz.

    InterruptLine - Stores ths interrupt line of the controller.

    InterruptVector - Stores the interrupt vector of the controller.

    InterruptHandle - Stores the interrupt connection handle.

    Flags - Stores a bitmask of flags for the device. See
        SD_RK32_DEVICE_FLAG_* for definitions.

    CardInterruptLine - Stores ths card detect interrupt line.

    CardInterruptVector - Stores the card detect interrupt vector.

    CardInterruptHandle - Stores the card detect interrupt connection handle.

    Child - Stores a pointer to the child device context.

    Lock - Stores a pointer to a lock used to serialize I/O requests.

    Device - Stores a pointer to the OS device.

    InVoltageSwitch - Stores a boolean indicating whether or not the controller
        is currently undergoing a voltage switch transition.

    Ldo - Stores the number of the LDO to configure when switching voltages.

    Rk808 - Stores a pointer to the RK808 interface used to switch voltages.

    DpcLock - Stores a spin lock acquired in the dispatch level interrupt
        handler to serialize interrupt processing.

--*/

struct _SD_RK32_CONTEXT {
    SD_RK32_DEVICE_TYPE Type;
    PSD_CONTROLLER Controller;
    PVOID ControllerBase;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG FundamentalClock;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    ULONGLONG CardInterruptLine;
    ULONGLONG CardInterruptVector;
    HANDLE CardInterruptHandle;
    volatile ULONG Flags;
    PSD_RK32_CHILD Child;
    PQUEUED_LOCK Lock;
    PDEVICE OsDevice;
    BOOL InVoltageSwitch;
    UCHAR Ldo;
    PINTERFACE_RK808 Rk808;
    KSPIN_LOCK DpcLock;
};

/*++

Structure Description:

    This structure describes the SD RK32xx vendor specific resource data passed
    in through the firmware.

Members:

    SubType - Stores the subtype, currently zero.

    Uuid - Stores the UUID constant identifying this resource type. This will
        be SD_RK32_VENDOR_RESOURCE_UUID.

    FundamentalClock - Stores the fundamental clock frequency of the controller.

    Ldo - Stores the LDO number of the LDO on an RK808 PMIC that controls the
        SD bus voltage lines. If 0, then no LDO control is needed.

--*/

typedef struct _SD_RK32_VENDOR_RESOURCE {
    UCHAR SubType;
    UUID Uuid;
    ULONG FundamentalClock;
    ULONG Ldo;
} PACKED SD_RK32_VENDOR_RESOURCE, *PSD_RK32_VENDOR_RESOURCE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

