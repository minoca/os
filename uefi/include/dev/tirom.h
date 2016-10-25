/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    tirom.h

Abstract:

    This header contains definitions for the TI ROM API.

Author:

    Evan Green 1-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

#define TI_ROM_API(_Value) ((void *)(*((UINT32 *)(_Value))))

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4460_PUBLIC_API_BASE 0x30400
#define OMAP4430_PUBLIC_API_BASE 0x28400
#define AM335_PUBLIC_API_BASE    0x20400

#define PUBLIC_GET_DRIVER_MEM_OFFSET 0x04
#define PUBLIC_GET_DRIVER_PER_OFFSET 0x08
#define PUBLIC_GET_DEVICE_MEM_OFFSET 0x80
#define PUBLIC_GET_DEVICE_PER_OFFSET 0x84

#define OMAP4_ROM_DEVICE_NULL     0x40
#define OMAP4_ROM_DEVICE_UART1    0x41
#define OMAP4_ROM_DEVICE_UART2    0x42
#define OMAP4_ROM_DEVICE_UART3    0x43
#define OMAP4_ROM_DEVICE_UART4    0x44
#define OMAP4_ROM_DEVICE_USB      0x45
#define OMAP4_ROM_DEVICE_USBEXT   0x46

#define AM335_ROM_DEVICE_NULL           0x00
#define AM335_ROM_DEVICE_XIP_MUX1       0x01
#define AM335_ROM_DEVICE_XIPWAIT_MUX1   0x02
#define AM335_ROM_DEVICE_XIP_MUX2       0x03
#define AM335_ROM_DEVICE_XIPWAIT_MUX2   0x04
#define AM335_ROM_DEVICE_NAND           0x05
#define AM335_ROM_DEVICE_NAND_I2C       0x06
#define AM335_ROM_DEVICE_MMCSD0         0x08
#define AM335_ROM_DEVICE_MMCSD1         0x09
#define AM335_ROM_DEVICE_SPI            0x15
#define AM335_ROM_DEVICE_UART0          0x41
#define AM335_ROM_DEVICE_USB            0x44
#define AM335_ROM_DEVICE_MAC0           0x46

#define TI_ROM_USB_MAX_IO_SIZE 65536

#define TI_ROM_TRANSFER_MODE_CPU     0
#define TI_ROM_TRANSFER_MODE_DMA     1

#define TI_ROM_STATUS_SUCCESS        0
#define TI_ROM_STATUS_FAILED         1
#define TI_ROM_STATUS_TIMEOUT        2
#define TI_ROM_STATUS_BAD_PARAM      3
#define TI_ROM_STATUS_WAITING        4
#define TI_ROM_STATUS_NO_MEMORY      5
#define TI_ROM_STATUS_INVALID_PTR    6

#define TI_ROM_MMCSD_TYPE_MMC 1
#define TI_ROM_MMCSD_TYPE_SD  2

#define TI_ROM_MMCSD_MODE_RAW 1
#define TI_ROM_MMCSD_MODE_FAT 2

#define TI_ROM_MMCSD_ADDRESSING_BYTE 1
#define TI_ROM_MMCSD_ADDRESSING_SECTOR 2

#define TI_ROM_MMCSD_PARTITION_COUNT 8

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _TI_ROM_PER_HANDLE
    TI_ROM_PER_HANDLE, *PTI_ROM_PER_HANDLE;

/*++

Structure Description:

    This structure defines the format of information passed from the TI ROM
    code to the first stage loader.

Members:

    Reserved - Stores a reserved value, contents unknown.

    MemoryDeviceDescriptor - Stores a pointer to the memory device descriptor
        that has been used during the memory booting process.

    BootDevice - Stores the code of the device that was booted from. See
        AM335_ROM_DEVICE_* definitions.

    ResetReason - Stores the current reset reason bit mask.

    Reserved2 - Stores another reserved field.

--*/

typedef struct _AM335_BOOT_DATA {
    UINT32 Reserved;
    UINT32 MemoryDeviceDescriptor;
    UINT8 BootDevice;
    UINT8 ResetReason;
    UINT8 Reserved2;
} PACKED AM335_BOOT_DATA, *PAM335_BOOT_DATA;

typedef
INT32
(*PTI_ROM_PER_CALLBACK) (
    PTI_ROM_PER_HANDLE Handle
    );

/*++

Routine Description:

    This routine is called by the ROM when I/O completes.

Arguments:

    Handle - Supplies the handle with the I/O.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines the TI ROM peripheral driver handle.

Members:

    IoConfiguration - Stores a pointer to the I/O configuration object.

    Callback - Stores a pointer to the callback function to call when I/O is
        complete.

    Data - Stores the address of the data to send or receive.

    Length - Stores the length of the data in bytes.

    Options - Stores a pointer to the boot options.

    TransferMode - Stores the transfer mode.

    DeviceType - Stores the device type.

    Status - Stores the return status of the I/O.

    HsTocMask - Stores a reserved value.

    GpTocMask - Stores another reserved value.

    ConfigTimeout - Stores another reserved value.

--*/

struct _TI_ROM_PER_HANDLE {
    VOID *IoConfiguration;
    PTI_ROM_PER_CALLBACK Callback;
    VOID *Data;
    UINT32 Length;
    UINT16 *Options;
    UINT32 TransferMode;
    UINT32 DeviceType;
    volatile UINT32 Status;
    UINT16 HsTocMask;
    UINT16 GpTocMask;
    UINT32 ConfigTimeout;
};

typedef
INT32
(*PTI_ROM_PER_INITIALIZE) (
    PTI_ROM_PER_HANDLE Handle
    );

/*++

Routine Description:

    This routine initializes a peripheral device.

Arguments:

    Handle - Supplies a pointer to the device handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_PER_READ) (
    PTI_ROM_PER_HANDLE Handle
    );

/*++

Routine Description:

    This routine performs a peripheral read.

Arguments:

    Handle - Supplies a pointer to the device handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_PER_WRITE) (
    PTI_ROM_PER_HANDLE Handle
    );

/*++

Routine Description:

    This routine performs a peripheral write.

Arguments:

    Handle - Supplies a pointer to the device handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_PER_CLOSE) (
    PTI_ROM_PER_HANDLE Handle
    );

/*++

Routine Description:

    This routine closes a peripheral handle.

Arguments:

    Handle - Supplies a pointer to the device handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_PER_CONFIGURE) (
    PTI_ROM_PER_HANDLE Handle,
    VOID *Data
    );

/*++

Routine Description:

    This routine configures a peripheral device.

Arguments:

    Handle - Supplies a pointer to the device handle.

    Data - Supplies a pointer to the configuration data.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines the TI ROM peripheral driver interface.

Members:

    Initialize - Stores a pointer to a function used to initialize a peripheral
        device handle.

    Read - Stores a pointer to a function used to read from the device.

    Write - Stores a pointer to a function used to write to the device.

    Close - Stores a pointer to a function used to close the handle.

    Configure - Stores a pointer to a function used to configure the handle.

--*/

typedef struct _TI_ROM_PER_DRIVER {
    PTI_ROM_PER_INITIALIZE Initialize;
    PTI_ROM_PER_READ Read;
    PTI_ROM_PER_WRITE Write;
    PTI_ROM_PER_CLOSE Close;
    PTI_ROM_PER_CONFIGURE Configure;
} TI_ROM_PER_DRIVER, *PTI_ROM_PER_DRIVER;

/*++

Structure Description:

    This structure defines the TI ROM USB configuration type.

Members:

    Type - Stores the configuration type identifier.

    Value - Stores the value for the configuration type.

--*/

typedef struct _TI_ROM_USB_CONFIGURATION {
    UINT32 Type;
    UINT32 Value;
} TI_ROM_USB_CONFIGURATION, *PTI_ROM_USB_CONFIGURATION;

typedef
INT32
(*PTI_ROM_GET_PER_DRIVER) (
    PTI_ROM_PER_DRIVER *Driver,
    UINT32 DeviceType
    );

/*++

Routine Description:

    This routine gets a peripheral driver interface from the ROM.

Arguments:

    Driver - Supplies a pointer where a pointer to the interface will be
        returned on success.

    DeviceType - Supplies the type of device to get the interface for.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_GET_PER_DEVICE) (
    PTI_ROM_PER_HANDLE *Handle
    );

/*++

Routine Description:

    This routine closes a peripheral handle.

Arguments:

    Handle - Supplies a pointer where a pointer to the device handle will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines the TI ROM memory device.

Members:

    Initialized - Stores a value indicating the initialization state of the
        structure.

    DeviceType - Stores the current device type.

    TrialsCount - Stores the number of booting trials.

    XipDevice - Stores a boolean indicating that this is a XIP device.

    SearchSize - Stores the size of blocks to search for an image.

    SearchBase - Stores the device base address.

    HsTocMask - Stores the mask of TOC items to search (HS device only).

    GpTocMask - Stores the mask of TOC items to search on GP devices.

    DeviceData - Stores a device dependent sub-structure.

    BootOptions - Stores a pointer to boot options.

--*/

typedef struct _TI_ROM_MEM_DEVICE {
    UINT32 Initialized;
    UINT8 DeviceType;
    UINT8 TrialsCount;
    UINT32 XipDevice;
    UINT16 SearchSize;
    UINT32 BaseAddress;
    UINT16 HsTocMask;
    UINT16 GpTocMask;
    VOID *DeviceData;
    UINT16 *BootOptions;
} TI_ROM_MEM_DEVICE, *PTI_ROM_MEM_DEVICE;

/*++

Structure Description:

    This structure defines the TI ROM Read Descriptor.

Members:

    SectorCount - Stores the starting sector to read from.

    SectorCount - Stores the number of sectors to read.

    Destination - Stores the destination buffer to read the data into.

--*/

typedef struct _TI_ROM_MEM_READ_DESCRIPTOR {
    UINT32 SectorStart;
    UINT32 SectorCount;
    VOID *Destination;
} TI_ROM_MEM_READ_DESCRIPTOR, *PTI_ROM_MEM_READ_DESCRIPTOR;

typedef
INT32
(*PTI_ROM_MEM_INITIALIZE) (
    PTI_ROM_MEM_DEVICE Device
    );

/*++

Routine Description:

    This routine initializes a connection to the ROM memory device.

Arguments:

    Device - Supplies a pointer where the device information will be returned.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_MEM_READ) (
    PTI_ROM_MEM_DEVICE Device,
    PTI_ROM_MEM_READ_DESCRIPTOR Descriptor
    );

/*++

Routine Description:

    This routine reads from the ROM memory device.

Arguments:

    Device - Supplies a pointer to the memory device.

    Descriptor - Supplies a pointer to the read request information.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

typedef
INT32
(*PTI_ROM_MEM_CONFIGURE) (
    PTI_ROM_MEM_DEVICE Device,
    VOID *Configuration
    );

/*++

Routine Description:

    This routine configures a ROM memory device.

Arguments:

    Device - Supplies a pointer to the memory device.

    Configuration - Supplies a pointer to the configuration to set.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines the TI ROM peripheral driver interface.

Members:

    Initialize - Stores a pointer to a function used to initialize a memory
        device handle.

    Read - Stores a pointer to a function used to read from the device.

    Configure - Stores a pointer to a function used to configure the device.

--*/

typedef struct _TI_ROM_MEM_DRIVER {
    PTI_ROM_MEM_INITIALIZE Initialize;
    PTI_ROM_MEM_READ Read;
    PTI_ROM_MEM_CONFIGURE Configure;
} TI_ROM_MEM_DRIVER, *PTI_ROM_MEM_DRIVER;

typedef struct _TI_ROM_MMCSD_DEVICE_DATA {
    UINT32 ModuleId;
    UINT32 Type;
    UINT32 Mode;
    UINT32 Copy;
    UINT32 SpecificationVersion;
    UINT32 AddressingMode;
    UINT32 SupportedBusWidth;
    UINT32 Size;
    UINT32 Rca;
    UINT32 PartitionSize[TI_ROM_MMCSD_PARTITION_COUNT];
    UINT32 PartitionBoot[TI_ROM_MMCSD_PARTITION_COUNT];
    UINT8 Partition;
} TI_ROM_MMCSD_DEVICE_DATA, *PTI_ROM_MMCSD_DEVICE_DATA;

typedef
INT32
(*PTI_ROM_GET_MEM_DRIVER) (
    PTI_ROM_MEM_DRIVER *Driver,
    UINT32 DeviceType
    );

/*++

Routine Description:

    This routine gets a memory driver interface from the ROM.

Arguments:

    Driver - Supplies a pointer where a pointer to the interface will be
        returned on success.

    DeviceType - Supplies the type of device to get the interface for.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

/*++

Structure Description:

    This structure defines a connection to the ROM USB peripheral interface.

Members:

    ReadHandle - Stores the handle used for reading.

    WriteHandle - Stores the handle used for writing.

    Driver - Stores a pointer to the driver interface.

--*/

typedef struct _TI_ROM_USB_HANDLE {
    TI_ROM_PER_HANDLE ReadHandle;
    TI_ROM_PER_HANDLE WriteHandle;
    PTI_ROM_PER_DRIVER Driver;
} TI_ROM_USB_HANDLE, *PTI_ROM_USB_HANDLE;

/*++

Structure Description:

    This structure defines a connection to the ROM memory device (like an SD
    card).

Members:

    Device - Stores the device information.

    Driver - Stores a pointer to the driver interface.

--*/

typedef struct _TI_ROM_MEM_HANDLE {
    TI_ROM_MEM_DEVICE Device;
    PTI_ROM_MEM_DRIVER Driver;
} TI_ROM_MEM_HANDLE, *PTI_ROM_MEM_HANDLE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INTN
EfipTiMemOpen (
    UINT8 DeviceType,
    UINT32 ApiBase,
    VOID *DeviceData,
    PTI_ROM_MEM_HANDLE Handle
    );

/*++

Routine Description:

    This routine opens a connection to the ROM API for the memory device on
    OMAP4 and AM335x SoCs.

Arguments:

    DeviceType - Supplies the device type to open.

    ApiBase - Supplies the base address of the public API area.

    DeviceData - Supplies the device data buffer.

    Handle - Supplies a pointer where the connection state will be returned
        on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INTN
EfipTiMemRead (
    PTI_ROM_MEM_HANDLE Handle,
    UINT32 Sector,
    UINTN SectorCount,
    VOID *Data
    );

/*++

Routine Description:

    This routine reads from the memory device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Sector - Supplies the sector to read from.

    SectorCount - Supplies the number of sectors to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INTN
EfipOmap4UsbOpen (
    PTI_ROM_USB_HANDLE Handle
    );

/*++

Routine Description:

    This routine opens a connection to the ROM API for the USB device.

Arguments:

    Handle - Supplies a pointer where the connection state will be returned
        on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INTN
EfipOmap4UsbRead (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    );

/*++

Routine Description:

    This routine reads from the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer where the data will be returned on success.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INTN
EfipOmap4UsbWrite (
    PTI_ROM_USB_HANDLE Handle,
    VOID *Data,
    UINTN Length
    );

/*++

Routine Description:

    This routine writes to the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Data - Supplies a pointer to the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

VOID
EfipOmap4UsbClose (
    PTI_ROM_USB_HANDLE Handle
    );

/*++

Routine Description:

    This routine closes an open handle to the USB device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

Return Value:

    None.

--*/

INTN
EfipTiLoadFirmwareFromFat (
    PTI_ROM_MEM_HANDLE Handle,
    CHAR8 *FileName,
    VOID *LoadAddress,
    UINT32 *Length
    );

/*++

Routine Description:

    This routine loads the firmware from a FAT file system.

Arguments:

    Handle - Supplies a pointer to the connection to the block device.

    FileName - Supplies a pointer to the null terminated name of the file.

    LoadAddress - Supplies the address where the image should be loaded.

    Length - Supplies a pointer where the length will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

