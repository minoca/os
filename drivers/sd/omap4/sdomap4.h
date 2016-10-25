/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sdomap4.h

Abstract:

    This header contains internal definitions for the OMAP4 SD/MMC controller.

Author:

    Evan Green 16-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/sd/sd.h>
#include <minoca/intrface/disk.h>
#include <minoca/dma/dma.h>
#include <minoca/dma/edma3.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the recognized device IDs.
//

#define SD_OMAP4_DEVICE_ID "TEX4004"
#define SD_AM335_DEVICE_ID "TEX3004"

//
// Define the minimum expected length of the HSMMC block.
//

#define SD_OMAP4_CONTROLLER_LENGTH 0x1000

//
// Define the offset into the HSMMC block where the SD standard registers
// start.
//

#define SD_OMAP4_CONTROLLER_SD_REGISTER_OFFSET 0x200

//
// Define the fundamental frequency of the HSMMC clock. An initial divisor of
// 0x80 (divide by 256) gets a base frequency of 375000, just under the 400kHz
// limit.
//

#define SD_OMAP4_FUNDAMENTAL_CLOCK_SPEED 96000000
#define SD_OMAP4_INITIAL_DIVISOR 0x80

#define SD_OMAP4_SYSCONFIG_REGISTER 0x10
#define SD_OMAP4_SYSSTATUS_REGISTER 0x114
#define SD_OMAP4_CON_REGISTER 0x12C

//
// Sysconfig register definitions
//

#define SD_OMAP4_SYSCONFIG_SOFT_RESET 0x00000001

//
// Sysstatus register definitions
//

#define SD_OMAP4_SYSSTATUS_RESET_DONE 0x00000001

//
// Con (control) register definitions.
//

#define SD_OMAP4_CON_INIT           (1 << 1)
#define SD_OMAP4_CON_8BIT           (1 << 5)
#define SD_OMAP4_CON_DEBOUNCE_MASK  (0x3 << 9)
#define SD_OMAP4_CON_DMA_MASTER     (1 << 20)

//
// Define the OMAP4 SD timeout in seconds.
//

#define SD_OMAP4_TIMEOUT 1

//
// Define the OMAP4 vendor specific interrupt status bits.
//

#define SD_OMAP4_INTERRUPT_STATUS_CARD_ERROR       (1 << 28)
#define SD_OMAP4_INTERRUPT_STATUS_BAD_ACCESS_ERROR (1 << 29)

//
// Define the OMAP4 vendor specific interrupt signal and status enable bits.
//

#define SD_OMAP4_INTERRUPT_ENABLE_ERROR_CARD       (1 << 28)
#define SD_OMAP4_INTERRUPT_ENABLE_ERROR_BAD_ACCESS (1 << 29)

//
// Define the set of flags for the parent SD device.
//

#define SD_OMAP4_DEVICE_FLAG_INTERRUPT_RESOURCES_FOUND 0x00000001

//
// Define the set of flags for the child SD disk.
//

#define SD_OMAP4_CHILD_FLAG_DMA_SUPPORTED 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _SD_OMAP4_DEVICE_TYPE {
    SdOmap4InvalidDeviceType,
    SdOmap4Parent,
    SdOmap4Child
} SD_OMAP4_DEVICE_TYPE, *PSD_OMAP4_DEVICE_TYPE;

typedef enum _SD_TI_SOC {
    SdTiSocInvalid,
    SdTiSocOmap4,
    SdTiSocAm335,
} SD_TI_SOC, *PSD_TI_SOC;

typedef struct _SD_OMAP4_CONTEXT SD_OMAP4_CONTEXT, *PSD_OMAP4_CONTEXT;

/*++

Structure Description:

    This structure describes the SD OMAP4 child device context.

Members:

    Type - Stores the device type, child in this case.

    ReferenceCount - Stores a reference count for the child.

    Parent - Stores a pointer to the parent structure.

    Device - Stores a pointer to the OS device.

    Controller - Stores an pointer to the controller structure.

    ControllerLock - Stores a pointer to a lock used to serialize I/O requests.

    Irp - Stores a pointer to the current IRP being processed.

    Flags - Stores a bitmask of flags for the disk. See SD_OMAP4_CHILD_FLAG_*
        for definitions.

    BlockShift - Stores the cached block size shift of the media.

    BlockCount - Stores the cached block count of the media.

    DiskInterface - Stores the disk interface presented to the system.

    RemainingInterrupts - Stores the count of remaining interrupts expected to
        come in before the transfer is complete.

--*/

typedef struct _SD_OMAP4_CHILD {
    SD_OMAP4_DEVICE_TYPE Type;
    volatile ULONG ReferenceCount;
    PSD_OMAP4_CONTEXT Parent;
    PDEVICE Device;
    PSD_CONTROLLER Controller;
    PQUEUED_LOCK ControllerLock;
    PIRP Irp;
    ULONG Flags;
    ULONG BlockShift;
    ULONGLONG BlockCount;
    DISK_INTERFACE DiskInterface;
    ULONG RemainingInterrupts;
} SD_OMAP4_CHILD, *PSD_OMAP4_CHILD;

/*++

Structure Description:

    This structure describes the SD OMAP4 device context.

Members:

    Type - Stores the device type, parent in this case.

    Controller - Stores an pointer to the controller structure.

    ControllerBase - Stores a pointer to the virtual address of the HSMMC
        registers.

    ControllerPhysical - Stores the physical address of the HSMMC registers.

    InterruptLine - Stores ths interrupt line of the controller.

    InterruptVector - Stores the interrupt vector of the controller.

    Flags - Stores a bitmask of flags for the device. See
        SD_OMAP4_DEVICE_FLAG_* for definitions.

    InterruptHandle - Stores the interrupt connection handle.

    Child - Stores a pointer to the child device context.

    Lock - Stores a pointer to a lock used to serialize I/O requests.

    Soc - Stores the type of system-on-chip this driver is servicing.

    TxDmaResource - Stores a pointer to the transmit DMA resource.

    RxDmaResource - Stores a pointer to the receive DMA resource.

    DmaTransfer - Stores a pointer to the DMA transfer used on I/O.

    EdmaConfiguration - Stores a pointer to the EDMA configuration used for the
        transfer.

    Dma - Stores a pointer to the DMA interface.

--*/

struct _SD_OMAP4_CONTEXT {
    SD_OMAP4_DEVICE_TYPE Type;
    PSD_CONTROLLER Controller;
    PVOID ControllerBase;
    PHYSICAL_ADDRESS ControllerPhysical;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    volatile ULONG Flags;
    HANDLE InterruptHandle;
    PSD_OMAP4_CHILD Child;
    PQUEUED_LOCK Lock;
    SD_TI_SOC Soc;
    PRESOURCE_ALLOCATION TxDmaResource;
    PRESOURCE_ALLOCATION RxDmaResource;
    PDMA_TRANSFER DmaTransfer;
    PEDMA_CONFIGURATION EdmaConfiguration;
    PDMA_INTERFACE Dma;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Omap4Twl6030InitializeMmcPower (
    VOID
    );

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL6030.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
OmapI2cInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the I2C device.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OmapI2cFlushData (
    VOID
    );

/*++

Routine Description:

    This routine flushes extraneous data out of the internal FIFOs.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
OmapI2cWrite (
    UCHAR Chip,
    ULONG Address,
    ULONG AddressLength,
    PVOID Buffer,
    ULONG Length
    );

/*++

Routine Description:

    This routine writes the given buffer out to the given i2c device.

Arguments:

    Chip - Supplies the device to write to.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies the buffer containing the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

KSTATUS
OmapI2cRead (
    UCHAR Chip,
    ULONG Address,
    ULONG AddressLength,
    PVOID Buffer,
    ULONG Length
    );

/*++

Routine Description:

    This routine reads from the given i2c device into the given buffer.

Arguments:

    Chip - Supplies the device to read from.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies a pointer to the buffer where the read data will be
        returned.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

