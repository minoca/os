/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbmass.c

Abstract:

    This module implements support for the USB Mass Storage driver.

Author:

    Evan Green 27-Jan-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/disk.h>
#include <minoca/usb/usb.h>

//
// --------------------------------------------------------------------- Macros
//

#define CONVERT_BIG_ENDIAN_TO_CPU32(_Value) \
    ((((_Value) << 24) & 0xFF000000) |      \
     (((_Value) << 8) & 0x00FF0000) |       \
     (((_Value) >> 8) & 0x0000FF00) |       \
     (((_Value) >> 24) & 0x000000FF))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used throughout the mass storage driver.
//

#define USB_MASS_ALLOCATION_TAG 0x4D627355 // 'MbsU'

//
// Define the interface protocol numbers used by Mass Storage.
//

#define USB_MASS_BULK_ONLY_PROTOCOL 0x50

//
// Define the class-specific mass storage request codes.
//

#define USB_MASS_REQUEST_GET_MAX_LUN  0xFE
#define USB_MASS_REQUEST_RESET_DEVICE 0xFF

//
// Define the maximum size of the buffer used for command headers and data
// transfers.
//

#define USB_MASS_COMMAND_BUFFER_SIZE 0x200
#define USB_MASS_MAX_DATA_TRANSFER (64 * 1024)

//
// Define the limit of how many times the status transfer can be sent when the
// IN endpoint is stalling.
//

#define USB_MASS_STATUS_TRANSFER_ATTEMPT_LIMIT 2

//
// Define the number of times to retry an I/O request before giving up on the
// IRP.
//

#define USB_MASS_IO_REQUEST_RETRY_COUNT 3

//
// Define the SCSI command block and command status signatures.
//

#define SCSI_COMMAND_BLOCK_SIGNATURE  0x43425355
#define SCSI_COMMAND_STATUS_SIGNATURE 0x53425355

//
// Define SCSI result status codes returned in the command status wrapper.
//

#define SCSI_STATUS_SUCCESS     0x00
#define SCSI_STATUS_FAILED      0x01
#define SCSI_STATUS_PHASE_ERROR 0x02

//
// Define the number of bits the LUN is shifted in most SCSI commands.
//

#define SCSI_COMMAND_LUN_SHIFT 5

//
// Define the flags in the command block wrapper.
//

#define SCSI_COMMAND_BLOCK_FLAG_DATA_IN 0x80

//
// Define SCSI commands.
//

#define SCSI_COMMAND_TEST_UNIT_READY        0x00
#define SCSI_COMMAND_REQUEST_SENSE          0x03
#define SCSI_COMMAND_INQUIRY                0x12
#define SCSI_COMMAND_MODE_SENSE_6           0x1A
#define SCSI_COMMAND_READ_FORMAT_CAPACITIES 0x23
#define SCSI_COMMAND_READ_CAPACITY          0x25
#define SCSI_COMMAND_READ_10                0x28
#define SCSI_COMMAND_WRITE_10               0x2A

//
// Define command sizes.
//

#define SCSI_COMMAND_TEST_UNIT_READY_SIZE             12
#define SCSI_COMMAND_REQUEST_SENSE_SIZE               12
#define SCSI_COMMAND_INQUIRY_SIZE                     12
#define SCSI_COMMAND_MODE_SENSE_6_SIZE                6
#define SCSI_COMMAND_READ_FORMAT_CAPACITIES_SIZE      10
#define SCSI_COMMAND_READ_CAPACITY_SIZE               10
#define SCSI_COMMAND_READ_10_SIZE                     12
#define SCSI_COMMAND_WRITE_10_SIZE                    12

//
// Define command data sizes.
//

#define SCSI_COMMAND_REQUEST_SENSE_DATA_SIZE 18
#define SCSI_COMMAND_READ_FORMAT_CAPACITIES_DATA_SIZE 0xFC
#define SCSI_COMMAND_MODE_SENSE_6_DATA_SIZE 0xC0

//
// Define USB Mass storage driver errors that can be reported back to the
// system.
//

#define USB_MASS_ERROR_FAILED_RESET_RECOVERY 0x00000001

//
// Set this flag if the USB mass storage device has claimed an interface.
//

#define USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED 0x00000001

//
// Set this flag if the USB mass storage device owns the paging disk and has
// prepared the USB core for handling paging.
//

#define USB_MASS_STORAGE_FLAG_PAGING_ENABLED 0x00000002

//
// Define the number of times a command is repeated.
//

#define USB_MASS_RETRY_COUNT 3

//
// Define the number of seconds to wait to get the capacities information.
//

#define USB_MASS_READ_CAPACITY_TIMEOUT 5

//
// Define the number of seconds to wait for the unit to become ready.
//

#define USB_MASS_UNIT_READY_TIMEOUT 30

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USB_MASS_STORAGE_TYPE {
    UsbMassStorageInvalid,
    UsbMassStorageDevice,
    UsbMassStorageLogicalDisk
} USB_MASS_STORAGE_TYPE, *PUSB_MASS_STORAGE_TYPE;

/*++

Structure Description:

    This structure defines the set of buffers and transfers required to send
    USB mass storage requests.

Members:

    CommandBuffer - Stores a pointer to an I/O buffer used as scratch space
        for status and command transfers and small data transfers.

    StatusTransfer - Stores a pointer to the IN USB transfer used for SCSI
        command status results.

    CommandTransfer - Stores a pointer to the OUT USB transfer used for SCSI
        commands.

    DataInTransfer - Stores a pointer to the USB transfer used when a
        command needs to read additional data from the device.

    DataOutTransfer - Stores a pointer to the USB transfer used to write data
        out to the disk.

--*/

typedef struct _USB_MASS_STORAGE_TRANSFERS {
    PIO_BUFFER CommandBuffer;
    PUSB_TRANSFER StatusTransfer;
    PUSB_TRANSFER CommandTransfer;
    PUSB_TRANSFER DataInTransfer;
    PUSB_TRANSFER DataOutTransfer;
} USB_MASS_STORAGE_TRANSFERS, *PUSB_MASS_STORAGE_TRANSFERS;

/*++

Structure Description:

    This structure stores the state necessary to complete polled I/O to a USB
    mass storage device. It is meant to be used at high run level during
    critical code paths (e.g. system failure).

Members:

    IoTransfers - Stores a pointer to the set of transfers used to complete
        I/O requests in polled mode.

    ControlTransfer - Stores a pointer to a control transfer that can be used
        in polled mode.

    ResetRequired - Stores a boolean indicating if a reset is required on all
        endpoints before executing polled transfers.

--*/

typedef struct _USB_MASS_STORAGE_POLLED_IO_STATE {
    USB_MASS_STORAGE_TRANSFERS IoTransfers;
    PUSB_TRANSFER ControlTransfer;
    BOOL ResetRequired;
} USB_MASS_STORAGE_POLLED_IO_STATE, *PUSB_MASS_STORAGE_POLLED_IO_STATE;

/*++

Structure Description:

    This structure stores context about a USB Mass storage device.

Members:

    Type - Stores a tag used to differentiate devices from disks.

    ReferenceCount - Stores a reference count for the device.

    UsbCoreHandle - Stores the handle to the device as identified by the USB
        core library.

    Lock - Stores a pointer to a lock that synchronizes the LUNs' access to the
        device, and serializes transfers.

    LogicalDiskList - Stores the list of logical disks on this device.

    PolledIoState - Stores a pointer to an optional I/O state used for polled
        I/O communications with the USB mass storage device during critical
        code paths.

    LunCount - Stores the maximum number of LUNs on this device.

    InEndpoint - Stores the endpoint number for the bulk IN endpoint.

    OutEndpoint - Stores the endpointer number for the bulk OUT endpoint.

    InterfaceNumber - Stores the USB Mass Storage interface number that this
        driver instance is attached to.

    Flags - Stores a bitmask of flags for this device.
        See USB_MASS_STORAGE_FLAG_* for definitions.

--*/

typedef struct _USB_MASS_STORAGE_DEVICE {
    USB_MASS_STORAGE_TYPE Type;
    volatile ULONG ReferenceCount;
    HANDLE UsbCoreHandle;
    PQUEUED_LOCK Lock;
    LIST_ENTRY LogicalDiskList;
    volatile PUSB_MASS_STORAGE_POLLED_IO_STATE PolledIoState;
    UCHAR LunCount;
    UCHAR InEndpoint;
    UCHAR OutEndpoint;
    UCHAR InterfaceNumber;
    ULONG Flags;
} USB_MASS_STORAGE_DEVICE, *PUSB_MASS_STORAGE_DEVICE;

/*++

Structure Description:

    This structure stores context about a USB Mass storage logical disk.

Members:

    Type - Stores a tag used to differentiate devices from disks.

    ReferenceCount - Stores a reference count for the disk.

    ListEntry - Stores pointers to the next and previous logical disks in the
        device.

    OsDevice - Stores a pointer to the OS device.

    Transfers - Stores the default set of transfers used to communicate with
        the USB mass storage device.

    LunNumber - Stores this logical disk's LUN number (a SCSI term).

    Device - Stores a pointer back to the device that this logical disk lives
        on.

    IoRequestAttempts - Stores the number of attempts that have been made
        to complete the current I/O request.

    StatusTransferAttempts - Stores the number of attempts that have been made
        to receive the status transfer.

    Event - Stores a pointer to an event to wait for in the case of
        synchronous commands.

    Irp - Stores a pointer to the IRP that the disk is currently serving.
        Whether this is NULL or non-NULL also serves to tell the callback
        routine whether to signal the IRP or the event.

    BlockCount - Stores the maximum number of blocks in the device.

    BlockShift - Stores the number of bits to shift to convert from bytes to
        blocks. This means the block size must be a power of two.

    CurrentFragment - Stores the current fragment number in a long transfer.

    CurrentFragmentOffset - Stores the offset (in bytes) into the current
        fragment in a long transfer.

    CurrentBytesTransferred - Stores the number of bytes that have been
        tranferred on behalf of the current I/O IRP.

    Connected - Stores a boolean indicating the disk's connection status.

    DiskInterface - Stores the disk interface published for this disk.

--*/

typedef struct _USB_DISK {
    USB_MASS_STORAGE_TYPE Type;
    volatile ULONG ReferenceCount;
    LIST_ENTRY ListEntry;
    PDEVICE OsDevice;
    UCHAR LunNumber;
    PUSB_MASS_STORAGE_DEVICE Device;
    USB_MASS_STORAGE_TRANSFERS Transfers;
    ULONG IoRequestAttempts;
    ULONG StatusTransferAttempts;
    PKEVENT Event;
    PIRP Irp;
    ULONG BlockCount;
    ULONG BlockShift;
    UINTN CurrentFragment;
    UINTN CurrentFragmentOffset;
    UINTN CurrentBytesTransferred;
    BOOL Connected;
    DISK_INTERFACE DiskInterface;
} USB_DISK, *PUSB_DISK;

/*++

Structure Description:

    This structure defines a SCSI Command Block Wrapper (CBW), which contains
    the command format used to communicate with disks.

Members:

    Signature - Stores a magic constant value. Use SCSI_COMMAND_BLOCK_SIGNATURE.

    Tag - Stores a unique value used to identify this command among others. The
        tag value in the ending command status word will be set to this value to
        signify which command is being acknowledged. The hardware does not
        interpret this value other than to copy it into the CSW at the end.

    DataTransferLength - Store the number of bytes the host expects to transfer
        on the Bulk-In or Bulk-Out endpoint (as indicated by the direction bit)
        during the execution of this command. If this field is zero, the device
        and host shall transfer no data between the CBW and CSW, and the device
        shall ignore the value of the Direction bit in the flags.

    Flags - Stores pretty much just one flag, the direction of the transfer
        (in or out).

    LunNumber - Stores the Logical Unit Number (disk index) to which the
        command block is being sent.

    CommandLength - Stores the valid length of the Command portion in bytes. The
        only legcal values are 1 through 16.

--*/

typedef struct _SCSI_COMMAND_BLOCK {
    ULONG Signature;
    ULONG Tag;
    ULONG DataTransferLength;
    UCHAR Flags;
    UCHAR LunNumber;
    UCHAR CommandLength;
    UCHAR Command[16];
} PACKED SCSI_COMMAND_BLOCK, *PSCSI_COMMAND_BLOCK;

/*++

Structure Description:

    This structure defines a SCSI Command Status Wrapper (CSW), which is sent
    by the disk to contain the ending status of the command just sent.

Members:

    Signature - Stores a magic constant value. Use
        SCSI_COMMAND_STATUS_SIGNATURE.

    Tag - Stores the unique tag value supplied by the host when the command
        was issued. This allows the host to match up statuses with their
        corresponding commands

    DataResidue - Stores a value for Data-Out transfers that represents the
        difference between the amount of data expected as stated in the
        data transfer length and the actual amount of data processed by the
        device. For Data-In the device shall report the difference between the
        amount of data expected as stated in the data transfer length field of
        the command and the actual amount of relevant data sent by the device.
        This shall not exceed the value sent in the transfer length.

    Status - Stores the status code representing the result of the procedure.
        See SCSI_STATUS_* definitions.

--*/

typedef struct _SCSI_COMMAND_STATUS {
    ULONG Signature;
    ULONG Tag;
    ULONG DataResidue;
    UCHAR Status;
} PACKED SCSI_COMMAND_STATUS, *PSCSI_COMMAND_STATUS;

/*++

Structure Description:

    This structure defines tHe result returned from the device of an INQUIRY
    command for page 0.

Members:

    PeripheralDeviceType - Stores the device currently connected to the logical
        unit.

    RemovableFlag - Stores a flag indicating if the device is removable (0x80
        if removable).

    VersionInformation - Stores the ISO, ECMA, and ANSI versions.

    ResponseDataFormat - Stores the response data format.

    AdditionalLength - Stores 31, the number of additional bytes in the
        information.

    Reserved - Stores reserved fields that should be set to 0.

    VendorInformation - Stores the vendor ID string.

    ProductInformation - Stores the product ID string.

    ProductRevision - Stores the product revision string, which should be in an
        "n.nn" type format.

    VendorData - Stores the first byte of the vendor-specific data. Disks like
        to transmit at least 36 bytes, and get fussy when they can't.

--*/

typedef struct _SCSI_INQUIRY_PAGE0 {
    UCHAR PeripheralDeviceType;
    UCHAR RemovableFlag;
    UCHAR VersionInformation;
    UCHAR ResponseDataFormat;
    UCHAR AdditionalLength;
    UCHAR Reserved[2];
    UCHAR VendorInformation[8];
    UCHAR ProductInformation[16];
    UCHAR ProductRevision[4];
    UCHAR VendorData;
} PACKED SCSI_INQUIRY_PAGE0, *PSCSI_INQUIRY_PAGE0;

/*++

Structure Description:

    This structure defines tje result returned from the device of a READ
    FORMAT CAPACITIES command.

Members:

    Reserved - Stores three unused bytes.

    CapacityListLength - Stores the size of the remaining structure (not
        counting this byte). This value should be at least 8 to contain the
        rest of this structure.

    BlockCount - Stores the number of blocks on the device.

    DescriptorCode - Stores whether this descriptor defines the maximum
        formattable capacity for this cartridge, the current media capacity, or
        the maximum formattable capacity for any cartridge.

    BlockLength - Stores the length of one block, in bytes. This value is
        commonly 512.

--*/

typedef struct _SCSI_FORMAT_CAPACITIES {
    UCHAR Reserved[3];
    UCHAR CapacityListLength;
    ULONG BlockCount;
    UCHAR DescriptorCode;
    ULONG BlockLength;
} PACKED SCSI_FORMAT_CAPACITIES, *PSCSI_FORMAT_CAPACITIES;

/*++

Structure Description:

    This structure defines tje result returned from the device of a READ
    CAPACITY command.

Members:

    LastValidBlockAddress - Stores the last valid logical block address for
        media access commands.

    BlockLength - Stores the length of one block, in bytes. This value is
        commonly 512.

--*/

typedef struct _SCSI_CAPACITY {
    ULONG LastValidBlockAddress;
    ULONG BlockLength;
} PACKED SCSI_CAPACITY, *PSCSI_CAPACITY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
UsbMassAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
UsbMassDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMassDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMassDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMassDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
UsbMassDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
UsbMasspStartDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspEnumerateChildren (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    );

KSTATUS
UsbMasspEnablePaging (
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspRemoveDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspDestroyDevice (
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspDeviceAddReference (
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspDeviceReleaseReference (
    PUSB_MASS_STORAGE_DEVICE Device
    );

KSTATUS
UsbMasspSetUpUsbDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    );

KSTATUS
UsbMasspGetLunCount (
    PUSB_MASS_STORAGE_DEVICE Device,
    PUCHAR LunCount
    );

KSTATUS
UsbMasspCreateLogicalDisks (
    PUSB_MASS_STORAGE_DEVICE Device,
    ULONG DiskCount
    );

VOID
UsbMasspDestroyLogicalDisks (
    PUSB_MASS_STORAGE_DEVICE Device
    );

PUSB_MASS_STORAGE_POLLED_IO_STATE
UsbMasspCreatePolledIoState (
    PUSB_MASS_STORAGE_DEVICE Device
    );

VOID
UsbMasspDestroyPolledIoState (
    PUSB_MASS_STORAGE_POLLED_IO_STATE PolledIoState
    );

KSTATUS
UsbMasspCreateTransfers (
    PUSB_MASS_STORAGE_DEVICE Device,
    PUSB_MASS_STORAGE_TRANSFERS Transfers,
    PVOID UserData,
    PUSB_TRANSFER_CALLBACK CallbackRoutine
    );

VOID
UsbMasspDestroyTransfers (
    PUSB_MASS_STORAGE_TRANSFERS Transfers
    );

KSTATUS
UsbMasspStartDisk (
    PUSB_DISK Disk
    );

VOID
UsbMasspRemoveDisk (
    PUSB_DISK Disk
    );

VOID
UsbMasspDestroyDisk (
    PUSB_DISK Disk
    );

VOID
UsbMasspDiskAddReference (
    PUSB_DISK Disk
    );

VOID
UsbMasspDiskReleaseReference (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspSendInquiry (
    PUSB_DISK Disk,
    UCHAR Page,
    PVOID *ResultBuffer,
    PULONG ResultBufferSize
    );

KSTATUS
UsbMasspTestUnitReady (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspRequestSense (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspModeSense (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspReadFormatCapacities (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspReadCapacity (
    PUSB_DISK Disk
    );

PVOID
UsbMasspSetupCommand (
    PUSB_DISK Disk,
    ULONG Tag,
    ULONG DataLength,
    UCHAR CommandLength,
    BOOL DataIn,
    BOOL PolledIo,
    PVOID TransferVirtualAddress,
    PHYSICAL_ADDRESS TransferPhysicalAddress
    );

KSTATUS
UsbMasspSendCommand (
    PUSB_DISK Disk
    );

VOID
UsbMasspTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    );

KSTATUS
UsbMasspEvaluateCommandStatus (
    PUSB_DISK Disk,
    BOOL PolledIo,
    BOOL DisableRecovery,
    PULONG BytesTransferred
    );

KSTATUS
UsbMasspSendNextIoRequest (
    PUSB_DISK Disk
    );

KSTATUS
UsbMasspResetRecovery (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    );

KSTATUS
UsbMasspReset (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    );

KSTATUS
UsbMasspClearHalts (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    );

KSTATUS
UsbMasspClearEndpoint (
    PUSB_MASS_STORAGE_DEVICE Device,
    UCHAR Endpoint,
    BOOL PolledIo
    );

KSTATUS
UsbMasspBlockIoInitialize (
    PVOID DiskToken
    );

KSTATUS
UsbMasspBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
UsbMasspBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    );

KSTATUS
UsbMasspPerformPolledIo (
    PIRP_READ_WRITE IrpReadWrite,
    PUSB_DISK Disk,
    BOOL Write
    );

KSTATUS
UsbMasspSendPolledIoCommand (
    PUSB_DISK Disk,
    PULONG BytesCompleted
    );

KSTATUS
UsbMasspResetForPolledIo (
    PUSB_MASS_STORAGE_DEVICE Device
    );

KSTATUS
UsbMasspSendPolledIoControlTransfer (
    PUSB_MASS_STORAGE_DEVICE Device,
    USB_TRANSFER_DIRECTION TransferDirection,
    PUSB_SETUP_PACKET SetupPacket
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER UsbMassDriver = NULL;
UUID UsbMassDiskInterfaceUuid = UUID_DISK_INTERFACE;

DISK_INTERFACE UsbMassDiskInterfaceTemplate = {
    DISK_INTERFACE_VERSION,
    NULL,
    0,
    0,
    UsbMasspBlockIoInitialize,
    NULL,
    UsbMasspBlockIoRead,
    UsbMasspBlockIoWrite
};

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

    This routine is the entry point for the USB Mass Storage driver. It
    registers the other dispatch functions, and performs driver-wide
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

    UsbMassDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = UsbMassAddDevice;
    FunctionTable.DispatchStateChange = UsbMassDispatchStateChange;
    FunctionTable.DispatchOpen = UsbMassDispatchOpen;
    FunctionTable.DispatchClose = UsbMassDispatchClose;
    FunctionTable.DispatchIo = UsbMassDispatchIo;
    FunctionTable.DispatchSystemControl = UsbMassDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
UsbMassAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the USB Mass
    Storage driver acts as the function driver. The driver will attach itself
    to the stack.

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

    PUSB_MASS_STORAGE_DEVICE NewDevice;
    KSTATUS Status;

    //
    // Create the device context and attach to the device.
    //

    NewDevice = MmAllocateNonPagedPool(sizeof(USB_MASS_STORAGE_DEVICE),
                                       USB_MASS_ALLOCATION_TAG);

    if (NewDevice == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(NewDevice, sizeof(USB_MASS_STORAGE_DEVICE));
    NewDevice->Type = UsbMassStorageDevice;
    NewDevice->ReferenceCount = 1;
    NewDevice->UsbCoreHandle = INVALID_HANDLE;
    INITIALIZE_LIST_HEAD(&(NewDevice->LogicalDiskList));
    NewDevice->Lock = KeCreateQueuedLock();
    if (NewDevice->Lock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    //
    // Attempt to attach to the USB core.
    //

    Status = UsbDriverAttach(DeviceToken,
                             UsbMassDriver,
                             &(NewDevice->UsbCoreHandle));

    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    ASSERT(NewDevice->UsbCoreHandle != INVALID_HANDLE);

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NewDevice);

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewDevice != NULL) {

            //
            // Release the reference, closing the USB core handle and
            // destroying the device.
            //

            UsbMasspDeviceReleaseReference(NewDevice);
        }
    }

    return Status;
}

VOID
UsbMassDispatchStateChange (
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

    PUSB_MASS_STORAGE_DEVICE Device;
    PUSB_DISK Disk;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PUSB_MASS_STORAGE_DEVICE)DeviceContext;
    if (Device->Type == UsbMassStorageDevice) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(UsbMassDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorStartDevice:

            //
            // Attempt to fire the thing up if the bus has already started it.
            //

            if (Irp->Direction == IrpUp) {
                Status = UsbMasspStartDevice(Irp, Device);
                if (!KSUCCESS(Status)) {
                    IoCompleteIrp(UsbMassDriver, Irp, Status);
                }
            }

            break;

        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                UsbMasspEnumerateChildren(Irp, Device);
            }

            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                UsbMasspRemoveDevice(Irp, Device);
            }

            break;

        //
        // For all other IRPs, do nothing.
        //

        default:
            break;
        }

    } else {
        Disk = (PUSB_DISK)DeviceContext;

        ASSERT(Disk->Type == UsbMassStorageLogicalDisk);

        switch (Irp->MinorCode) {
        case IrpMinorStartDevice:
            if (Irp->Direction == IrpUp) {
                Status = UsbMasspStartDisk(Disk);
                IoCompleteIrp(UsbMassDriver, Irp, Status);
            }

            break;

        case IrpMinorQueryResources:
        case IrpMinorQueryChildren:
            if (Irp->Direction == IrpUp) {
                IoCompleteIrp(UsbMassDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorRemoveDevice:
            if (Irp->Direction == IrpUp) {
                UsbMasspRemoveDisk(Disk);
                IoCompleteIrp(UsbMassDriver, Irp, STATUS_SUCCESS);
            }

            break;

        default:
            break;
        }

    }

    return;
}

VOID
UsbMassDispatchOpen (
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

    PUSB_DISK Disk;
    KSTATUS Status;

    Disk = (PUSB_DISK)DeviceContext;
    if (Disk->Type != UsbMassStorageLogicalDisk) {
        return;
    }

    ASSERT(Disk->Connected != FALSE);

    //
    // If this is an open for the paging device then enable paging on this
    // device.
    //

    if ((Irp->U.Open.OpenFlags & OPEN_FLAG_PAGING_DEVICE) != 0) {
        Status = UsbMasspEnablePaging(Disk->Device);
        if (!KSUCCESS(Status)) {
            goto DispatchOpenEnd;
        }
    }

    UsbMasspDiskAddReference(Disk);
    Irp->U.Open.DeviceContext = NULL;
    Status = STATUS_SUCCESS;

DispatchOpenEnd:
    IoCompleteIrp(UsbMassDriver, Irp, Status);
    return;
}

VOID
UsbMassDispatchClose (
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

    PUSB_DISK Disk;

    Disk = (PUSB_DISK)DeviceContext;
    if (Disk->Type != UsbMassStorageLogicalDisk) {
        return;
    }

    UsbMasspDiskReleaseReference(Disk);
    IoCompleteIrp(UsbMassDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
UsbMassDispatchIo (
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

    BOOL CompleteIrp;
    PUSB_DISK Disk;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    ULONG IrpReadWriteFlags;
    BOOL LockHeld;
    BOOL ReadWriteIrpPrepared;
    KSTATUS Status;

    CompleteIrp = TRUE;
    Disk = (PUSB_DISK)DeviceContext;
    LockHeld = FALSE;
    ReadWriteIrpPrepared = FALSE;

    ASSERT(Disk->Type == UsbMassStorageLogicalDisk);

    //
    // Set the read/write flags for preparation. As USB mass storage does not
    // do DMA directly, nor does it do polled I/O, don't set either flag.
    //

    IrpReadWriteFlags = 0;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    //
    // If the IRP is on the way up, then clean up after the DMA as this IRP is
    // still sitting in the channel. An IRP going up is already complete.
    //

    if (Irp->Direction != IrpDown) {
        CompleteIrp = FALSE;

        ASSERT(Irp == Disk->Irp);

        Disk->Irp = NULL;
        KeReleaseQueuedLock(Disk->Device->Lock);
        Status = IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        if (!KSUCCESS(Status)) {
            IoUpdateIrpStatus(Irp, Status);
        }

    } else {

        ASSERT(Irp->U.ReadWrite.IoBuffer != NULL);

        //
        // Before acquiring the device's lock and starting the transfer,
        // prepare the I/O context for USB Mass Storage (i.e. it must use
        // physical addresses that are less than 4GB and be cache aligned).
        //

        Status = IoPrepareReadWriteIrp(&(Irp->U.ReadWrite),
                                       1 << Disk->BlockShift,
                                       0,
                                       MAX_ULONG,
                                       IrpReadWriteFlags);

        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        ReadWriteIrpPrepared = TRUE;
        IoBuffer = Irp->U.ReadWrite.IoBuffer;

        //
        // Map the I/O buffer.
        //
        // TODO: Make sure USB Mass does not need the I/O buffer mapped.
        //

        Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);
        if (!KSUCCESS(Status)) {
            goto DispatchIoEnd;
        }

        //
        // Find the starting fragment based on the current offset.
        //

        IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer);
        FragmentIndex = 0;
        FragmentOffset = 0;
        while (IoBufferOffset != 0) {

            ASSERT(FragmentIndex < IoBuffer->FragmentCount);

            Fragment = &(IoBuffer->Fragment[FragmentIndex]);
            if (IoBufferOffset < Fragment->Size) {
                FragmentOffset = IoBufferOffset;
                break;
            }

            IoBufferOffset -= Fragment->Size;
            FragmentIndex += 1;
        }

        //
        // Lock the disk to serialize all I/O access to the device.
        //

        KeAcquireQueuedLock(Disk->Device->Lock);
        LockHeld = TRUE;
        if (Disk->Connected == FALSE) {
            Status = STATUS_DEVICE_NOT_CONNECTED;
            goto DispatchIoEnd;
        }

        //
        // Otherwise start the I/O on a connected device.
        //

        Disk->CurrentFragment = FragmentIndex;
        Disk->CurrentFragmentOffset = FragmentOffset;
        Disk->CurrentBytesTransferred = 0;
        Disk->Irp = Irp;

        ASSERT(Irp->U.ReadWrite.IoSizeInBytes != 0);
        ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoSizeInBytes,
                          (1 << Disk->BlockShift)));

        ASSERT(IS_ALIGNED(Irp->U.ReadWrite.IoOffset, (1 << Disk->BlockShift)));

        //
        // Pend the IRP first so that the request can't complete in between
        // submitting it and marking it pended.
        //

        CompleteIrp = FALSE;
        IoPendIrp(UsbMassDriver, Irp);

        //
        // Fire the first I/O request off to the disk. If this fails, expect to
        // get called on the way up, as the IRP has already been pended. Thus,
        // act like the lock is not held and the context was not prepared.
        //

        Disk->IoRequestAttempts = 0;
        Status = UsbMasspSendNextIoRequest(Disk);
        if (!KSUCCESS(Status)) {
            CompleteIrp = TRUE;
            LockHeld = FALSE;
            ReadWriteIrpPrepared = FALSE;
            goto DispatchIoEnd;
        }
    }

DispatchIoEnd:
    if (CompleteIrp != FALSE) {
        if (LockHeld != FALSE) {
            KeReleaseQueuedLock(Disk->Device->Lock);
        }

        if (ReadWriteIrpPrepared != FALSE) {
            IoCompleteReadWriteIrp(&(Irp->U.ReadWrite), IrpReadWriteFlags);
        }

        IoCompleteIrp(UsbMassDriver, Irp, Status);
    }

    return;
}

VOID
UsbMassDispatchSystemControl (
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

    PVOID Context;
    PUSB_DISK Disk;
    PSYSTEM_CONTROL_FILE_OPERATION FileOperation;
    ULONGLONG FileSize;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    ULONGLONG PropertiesFileSize;
    KSTATUS Status;

    //
    // Do nothing for non-logical disks.
    //

    Disk = (PUSB_DISK)DeviceContext;
    if (Disk->Type != UsbMassStorageLogicalDisk) {
        return;
    }

    //
    // System control IRPs should only be arriving if the disk is connected.
    //

    ASSERT(Disk->Connected != FALSE);

    //
    // Handle the IRP for logical disks.
    //

    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectBlockDevice;
            Properties->HardLinkCount = 1;

            ASSERT(((1 << Disk->BlockShift) != 0) && (Disk->BlockCount != 0));

            Properties->BlockSize = 1 << Disk->BlockShift;
            Properties->BlockCount = Disk->BlockCount;
            FileSize = (ULONGLONG)Disk->BlockCount <<
                       (ULONGLONG)Disk->BlockShift;

            Properties->Size = FileSize;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(UsbMassDriver, Irp, Status);
        break;

    //
    // Writes to the disk's properties are not allowed. Fail if the data
    // has changed.
    //

    case IrpMinorSystemControlWriteFileProperties:
        FileOperation = (PSYSTEM_CONTROL_FILE_OPERATION)Context;
        Properties = FileOperation->FileProperties;
        PropertiesFileSize = Properties->Size;
        FileSize = (ULONGLONG)Disk->BlockCount << (ULONGLONG)Disk->BlockShift;
        if ((Properties->FileId != 0) ||
            (Properties->Type != IoObjectBlockDevice) ||
            (Properties->HardLinkCount != 1) ||
            (Properties->BlockSize != (1 << Disk->BlockShift)) ||
            (Properties->BlockCount != Disk->BlockCount) ||
            (PropertiesFileSize != FileSize)) {

            Status = STATUS_NOT_SUPPORTED;

        } else {
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(UsbMassDriver, Irp, Status);
        break;

    //
    // Do not support USB mass storage device truncation.
    //

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(UsbMassDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    //
    // Gather and return device information.
    //

    case IrpMinorSystemControlDeviceInformation:
        break;

    case IrpMinorSystemControlSynchronize:
        IoCompleteIrp(UsbMassDriver, Irp, STATUS_SUCCESS);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

KSTATUS
UsbMasspStartDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine starts up the USB Mass Storage device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this mass storage device.

Return Value:

    Status code.

--*/

{

    UCHAR LunCount;
    KSTATUS Status;

    ASSERT(Device->Type == UsbMassStorageDevice);

    //
    // Claim the interface.
    //

    Status = UsbMasspSetUpUsbDevice(Irp, Device);
    if (!KSUCCESS(Status)) {
        goto StartDeviceEnd;
    }

    if (Device->LunCount == 0) {
        Status = UsbMasspGetLunCount(Device, &LunCount);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        //
        // Fire up all those little disks.
        //

        Status = UsbMasspCreateLogicalDisks(Device, LunCount);
        if (!KSUCCESS(Status)) {
            goto StartDeviceEnd;
        }

        Device->LunCount = LunCount;
    }

StartDeviceEnd:
    return Status;
}

KSTATUS
UsbMasspEnablePaging (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine enables paging on the given USB mass storage device. It
    converts all transfers for all disks to paging device transfers, which USB
    core will handle separately from other non-critical transfers.

Arguments:

    Device - Supplies a pointer to a USB mass storage device.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PUSB_DISK Disk;
    BOOL LockHeld;
    KSTATUS Status;
    PUSB_MASS_STORAGE_TRANSFERS Transfers;

    LockHeld = FALSE;

    //
    // If the device has already been enabled for paging, then work here is
    // done.
    //

    if ((Device->Flags & USB_MASS_STORAGE_FLAG_PAGING_ENABLED) != 0) {
        return STATUS_SUCCESS;
    }

    //
    // Notify USB core that a paging device has arrived and that it would like
    // its transfers to be serviced on their own work queue.
    //

    Status = UsbInitializePagingDeviceTransfers();
    if (!KSUCCESS(Status)) {
        goto InitializePagingEnd;
    }

    //
    // Now acquire the device's lock to synchronize with transfer submissions
    // and try to convert this device's transfer to be paging transfers.
    //

    KeAcquireQueuedLock(Device->Lock);
    LockHeld = TRUE;
    if ((Device->Flags & USB_MASS_STORAGE_FLAG_PAGING_ENABLED) != 0) {
        Status = STATUS_SUCCESS;
        goto InitializePagingEnd;
    }

    //
    // Iterate over all transfers for all the disks, converting them to be
    // paging device transfers. Because all disks share the same device lock,
    // all disks need to start using the paging path, even if a disk is not
    // involved in paging.
    //

    CurrentEntry = Device->LogicalDiskList.Next;
    while (CurrentEntry != &(Device->LogicalDiskList)) {
        Disk = LIST_VALUE(CurrentEntry, USB_DISK, ListEntry);
        Transfers = &(Disk->Transfers);
        Transfers->CommandTransfer->Flags |= USB_TRANSFER_FLAG_PAGING_DEVICE;
        Transfers->StatusTransfer->Flags |= USB_TRANSFER_FLAG_PAGING_DEVICE;
        Transfers->DataInTransfer->Flags |= USB_TRANSFER_FLAG_PAGING_DEVICE;
        Transfers->DataOutTransfer->Flags |= USB_TRANSFER_FLAG_PAGING_DEVICE;
        CurrentEntry = CurrentEntry->Next;
    }

    Device->Flags |= USB_MASS_STORAGE_FLAG_PAGING_ENABLED;
    KeReleaseQueuedLock(Device->Lock);
    LockHeld = FALSE;
    Status = STATUS_SUCCESS;

InitializePagingEnd:
    if (LockHeld != FALSE) {
        KeReleaseQueuedLock(Device->Lock);
    }

    return Status;
}

VOID
UsbMasspEnumerateChildren (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine enumerates the USB Mass Storage device's children.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this mass storage device.

Return Value:

    None.

--*/

{

    ULONG ChildCount;
    ULONG ChildIndex;
    PDEVICE *Children;
    PLIST_ENTRY CurrentEntry;
    PUSB_DISK Disk;
    KSTATUS Status;

    ASSERT(Device->Type == UsbMassStorageDevice);

    ChildCount = Device->LunCount;
    Children = NULL;
    if (Device->LunCount == 0) {
        Status = STATUS_SUCCESS;
        goto EnumerateChildrenEnd;
    }

    Children = MmAllocatePagedPool(sizeof(PDEVICE) * ChildCount,
                                   USB_MASS_ALLOCATION_TAG);

    if (Children == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateChildrenEnd;
    }

    RtlZeroMemory(Children, sizeof(PDEVICE) * ChildCount);

    //
    // Loop through and add every disk.
    //

    ChildIndex = 0;
    CurrentEntry = Device->LogicalDiskList.Next;
    while (CurrentEntry != &(Device->LogicalDiskList)) {
        Disk = LIST_VALUE(CurrentEntry, USB_DISK, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Disk->OsDevice == NULL) {
            Status = IoCreateDevice(UsbMassDriver,
                                    Disk,
                                    Irp->Device,
                                    "UsbDisk",
                                    DISK_CLASS_ID,
                                    NULL,
                                    &(Disk->OsDevice));

            if (!KSUCCESS(Status)) {
                goto EnumerateChildrenEnd;
            }
        }

        if (Disk->OsDevice != NULL) {
            Disk->Connected = TRUE;
            Children[ChildIndex] = Disk->OsDevice;
            ChildIndex += 1;
        }
    }

    ChildCount = ChildIndex;
    Status = STATUS_SUCCESS;

EnumerateChildrenEnd:
    if (!KSUCCESS(Status)) {
        if (Children != NULL) {
            MmFreePagedPool(Children);
            Children = NULL;
            ChildCount = 0;
        }
    }

    ASSERT((Irp->U.QueryChildren.Children == NULL) &&
           (Irp->U.QueryChildren.ChildCount == 0));

    Irp->U.QueryChildren.Children = Children;
    Irp->U.QueryChildren.ChildCount = ChildCount;
    IoCompleteIrp(UsbMassDriver, Irp, Status);
    return;
}

VOID
UsbMasspRemoveDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine removes the USB Mass Storage device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this mass storage device.

Return Value:

    None.

--*/

{

    ASSERT(Device->Type == UsbMassStorageDevice);

    //
    // Detach the device from USB core. This marks it as disconnected and
    // cancels all transfers associated with the device.
    //
    // N.B. Since all the logical disks have already received a remove IRP,
    //      the transfers should be inactive already.
    //

    UsbDetachDevice(Device->UsbCoreHandle);

    //
    // The logical disk list for this device should be empty if the device
    // successfully completed enumeration. That is, all the logical disk
    // should have received a removal IRP that this driver handled, acting as
    // the bus driver. If it is not empty, then the device never made it to
    // enumeration, or one logical disk failed to enumerate, and the disks need
    // to be cleaned up.
    //

    UsbMasspDestroyLogicalDisks(Device);

    //
    // Release the interface used for the USB mass storage device.
    //

    if ((Device->Flags & USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED) != 0) {
        UsbReleaseInterface(Device->UsbCoreHandle, Device->InterfaceNumber);
        Device->Flags &= ~USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED;
    }

    //
    // Release the reference taken during device add. Logical disks may still
    // have references on the device and USB core.
    //

    UsbMasspDeviceReleaseReference(Device);
    return;
}

VOID
UsbMasspDestroyDevice (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine destroys a USB mass storage device.

Arguments:

    Device - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    //
    // The device should have already received the removal IRP or have never
    // made it off the ground before it gets destroyed, so assert that there
    // are no references, its logical disk list is empty and the interface is
    // not claimed.
    //

    ASSERT(Device->ReferenceCount == 0);
    ASSERT(LIST_EMPTY(&(Device->LogicalDiskList)) != FALSE);
    ASSERT((Device->Flags & USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED) == 0);

    //
    // Destroy the polled I/O state if it exists.
    //

    if (Device->PolledIoState != NULL) {
        UsbMasspDestroyPolledIoState(Device->PolledIoState);
    }

    //
    // Release the USB core handle. The USB core device does not get dropped
    // until all of its transfers are destroyed. As a result, this handle
    // remains open until the last logical disk reference is dropped, making it
    // pointless to put this close in the device removal routine.
    //

    UsbDeviceClose(Device->UsbCoreHandle);

    //
    // Destroy the lock if necessary.
    //

    if (Device->Lock != NULL) {

        ASSERT(KeIsQueuedLockHeld(Device->Lock) == FALSE);

        KeDestroyQueuedLock(Device->Lock);
    }

    //
    // Release the device itself.
    //

    MmFreeNonPagedPool(Device);
    return;
}

VOID
UsbMasspDeviceAddReference (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine adds a reference to USB mass storage device.

Arguments:

    Device - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
UsbMasspDeviceReleaseReference (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine releases a reference from the USB mass storage device.

Arguments:

    Device - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Device->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        UsbMasspDestroyDevice(Device);
    }

    return;
}

KSTATUS
UsbMasspSetUpUsbDevice (
    PIRP Irp,
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine claims the mass storage interface for the given device.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    Device - Supplies a pointer to this mass storage device.

Return Value:

    Status code.

--*/

{

    PUSB_CONFIGURATION_DESCRIPTION Configuration;
    PLIST_ENTRY CurrentEntry;
    USB_TRANSFER_DIRECTION Direction;
    PUSB_ENDPOINT_DESCRIPTION Endpoint;
    UCHAR EndpointType;
    BOOL InEndpointFound;
    PUSB_INTERFACE_DESCRIPTION Interface;
    BOOL OutEndpointFound;
    KSTATUS Status;

    ASSERT(Device->Type == UsbMassStorageDevice);

    if ((Device->Flags & USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED) != 0) {
        Status = STATUS_SUCCESS;
        goto SetUpUsbDeviceEnd;
    }

    //
    // If the configuration isn't yet set, set the first one.
    //

    Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);
    if (Configuration == NULL) {
        Status = UsbSetConfiguration(Device->UsbCoreHandle, 0, TRUE);
        if (!KSUCCESS(Status)) {
            goto SetUpUsbDeviceEnd;
        }

        Configuration = UsbGetActiveConfiguration(Device->UsbCoreHandle);

        ASSERT(Configuration != NULL);

    }

    //
    // Get and verify the interface.
    //

    Interface = UsbGetDesignatedInterface(Irp->Device, Device->UsbCoreHandle);
    if (Interface == NULL) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    if (Interface->Descriptor.Class != UsbInterfaceClassMassStorage) {
        Status = STATUS_NO_INTERFACE;
        goto SetUpUsbDeviceEnd;
    }

    if (Interface->Descriptor.Protocol != USB_MASS_BULK_ONLY_PROTOCOL) {
        RtlDebugPrint("USB Mass Storage Error: Unsupported protocol 0x%x. Only "
                      "the Bulk-Only protocol (0x50) is supported.\n");

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Locate the IN and OUT bulk endpoints.
    //

    InEndpointFound = FALSE;
    OutEndpointFound = FALSE;
    CurrentEntry = Interface->EndpointListHead.Next;
    while (CurrentEntry != &(Interface->EndpointListHead)) {
        Endpoint = LIST_VALUE(CurrentEntry,
                              USB_ENDPOINT_DESCRIPTION,
                              ListEntry);

        CurrentEntry = CurrentEntry->Next;

        //
        // Deconstruct the components of the endpoint descriptor.
        //

        EndpointType = Endpoint->Descriptor.Attributes &
                       USB_ENDPOINT_ATTRIBUTES_TYPE_MASK;

        if ((Endpoint->Descriptor.EndpointAddress &
             USB_ENDPOINT_ADDRESS_DIRECTION_IN) != 0) {

            Direction = UsbTransferDirectionIn;

        } else {
            Direction = UsbTransferDirectionOut;
        }

        //
        // Look to match the endpoint up to one of the required ones.
        //

        if (EndpointType == USB_ENDPOINT_ATTRIBUTES_TYPE_BULK) {
            if ((InEndpointFound == FALSE) &&
                (Direction == UsbTransferDirectionIn)) {

                InEndpointFound = TRUE;
                Device->InEndpoint = Endpoint->Descriptor.EndpointAddress;

            } else if ((OutEndpointFound == FALSE) &&
                       (Direction == UsbTransferDirectionOut)) {

                OutEndpointFound = TRUE;
                Device->OutEndpoint = Endpoint->Descriptor.EndpointAddress;
            }
        }

        if ((InEndpointFound != FALSE) && (OutEndpointFound != FALSE)) {
            break;
        }
    }

    if ((InEndpointFound == FALSE) || (OutEndpointFound == FALSE)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto SetUpUsbDeviceEnd;
    }

    //
    // Everything's all ready, claim the interface.
    //

    Status = UsbClaimInterface(Device->UsbCoreHandle,
                               Interface->Descriptor.InterfaceNumber);

    if (!KSUCCESS(Status)) {
        goto SetUpUsbDeviceEnd;
    }

    Device->InterfaceNumber = Interface->Descriptor.InterfaceNumber;
    Device->Flags |= USB_MASS_STORAGE_FLAG_INTERFACE_CLAIMED;
    Status = STATUS_SUCCESS;

SetUpUsbDeviceEnd:
    return Status;
}

KSTATUS
UsbMasspGetLunCount (
    PUSB_MASS_STORAGE_DEVICE Device,
    PUCHAR LunCount
    )

/*++

Routine Description:

    This routine returns the maximum number of logical disks contained in this
    mass storage device.

Arguments:

    Device - Supplies a pointer to this mass storage device.

    LunCount - Supplies a pointer where the number of devices will be
        returned on success.

Return Value:

    Status code.

--*/

{

    ULONG Alignment;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    ULONG MaxTransferLength;
    PUSB_SETUP_PACKET Setup;
    KSTATUS Status;
    PUSB_TRANSFER Transfer;
    PVOID TransferBuffer;
    ULONG TransferLength;

    Transfer = NULL;

    //
    // Create the I/O buffer that will be used for the transfer.
    //

    Alignment = MmGetIoBufferAlignment();
    TransferLength = sizeof(USB_SETUP_PACKET) + sizeof(UCHAR);
    MaxTransferLength = ALIGN_RANGE_UP(TransferLength, Alignment);
    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                          MAX_ULONG,
                                          Alignment,
                                          MaxTransferLength,
                                          IoBufferFlags);

    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetMaxLunEnd;
    }

    ASSERT(IoBuffer->FragmentCount == 1);

    TransferBuffer = IoBuffer->Fragment[0].VirtualAddress;
    Setup = (PUSB_SETUP_PACKET)TransferBuffer;
    Setup->RequestType = USB_SETUP_REQUEST_TO_HOST |
                         USB_SETUP_REQUEST_CLASS |
                         USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    Setup->Request = USB_MASS_REQUEST_GET_MAX_LUN;
    Setup->Value = 0;
    Setup->Index = Device->InterfaceNumber;
    Setup->Length = sizeof(UCHAR);

    //
    // Create a USB transfer.
    //

    Transfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                   0,
                                   MaxTransferLength,
                                   0);

    if (Transfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto GetMaxLunEnd;
    }

    Transfer->Direction = UsbTransferDirectionIn;
    Transfer->Length = TransferLength;
    Transfer->Buffer = IoBuffer->Fragment[0].VirtualAddress;
    Transfer->BufferPhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;
    Transfer->BufferActualLength = IoBuffer->Fragment[0].Size;

    //
    // Submit the transfer and wait for it to complete. The spec says that
    // devices that don't support multiple LUNs may stall the transfer.
    //

    Status = UsbSubmitSynchronousTransfer(Transfer);
    if ((Status == STATUS_DEVICE_IO_ERROR) &&
        (Transfer->Error == UsbErrorTransferStalled)) {

        //
        // Clear the halt condition of endpoint zero. One might think that this
        // is not possible, but indeed it is.
        //

        Status = UsbMasspClearEndpoint(Device, 0, FALSE);
        if (!KSUCCESS(Status)) {
            goto GetMaxLunEnd;
        }

        *LunCount = 1;
        goto GetMaxLunEnd;
    }

    if (!KSUCCESS(Status)) {
        goto GetMaxLunEnd;
    }

    ASSERT(KSUCCESS(Transfer->Status));

    if (Transfer->LengthTransferred != TransferLength) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto GetMaxLunEnd;
    }

    *LunCount = *((PUCHAR)Transfer->Buffer +
                  sizeof(USB_SETUP_PACKET));

    //
    // Add 1 since the data value was a "max index", but the caller wants a
    // count.
    //

    *LunCount += 1;
    Status = STATUS_SUCCESS;

GetMaxLunEnd:
    if (Transfer != NULL) {
        UsbDestroyTransfer(Transfer);
    }

    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

KSTATUS
UsbMasspCreateLogicalDisks (
    PUSB_MASS_STORAGE_DEVICE Device,
    ULONG DiskCount
    )

/*++

Routine Description:

    This routine creates a number of logical disks to live under the given
    mass storage device. These disks will be added to the device.

Arguments:

    Device - Supplies a pointer to this mass storage device.

    DiskCount - Supplies the number of logical disks to create.

Return Value:

    Status code.

--*/

{

    PUSB_DISK Disk;
    ULONG DiskIndex;
    KSTATUS Status;

    ASSERT(LIST_EMPTY(&(Device->LogicalDiskList)) != FALSE);

    Disk = NULL;
    for (DiskIndex = 0; DiskIndex < DiskCount; DiskIndex += 1) {
        Disk = MmAllocateNonPagedPool(sizeof(USB_DISK),
                                      USB_MASS_ALLOCATION_TAG);

        if (Disk == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateLogicalDisksEnd;
        }

        RtlZeroMemory(Disk, sizeof(USB_DISK));
        Disk->Type = UsbMassStorageLogicalDisk;
        Disk->ReferenceCount = 1;
        Disk->LunNumber = DiskIndex;
        Disk->Device = Device;
        UsbMasspDeviceAddReference(Device);

        //
        // Create the event for synchronous transfers.
        //

        Disk->Event = KeCreateEvent(NULL);
        if (Disk->Event == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateLogicalDisksEnd;
        }

        //
        // Create the set of default transfers for this disk.
        //

        Status = UsbMasspCreateTransfers(Device,
                                         &(Disk->Transfers),
                                         Disk,
                                         UsbMasspTransferCompletionCallback);

        if (!KSUCCESS(Status)) {
            goto CreateLogicalDisksEnd;
        }

        ASSERT(Disk->Connected == FALSE);

        //
        // Add the new disk to the list.
        //

        INSERT_BEFORE(&(Disk->ListEntry), &(Device->LogicalDiskList));
        Disk = NULL;
    }

    Status = STATUS_SUCCESS;

CreateLogicalDisksEnd:
    if (!KSUCCESS(Status)) {
        if (Disk != NULL) {
            UsbMasspDiskReleaseReference(Disk);
        }

        UsbMasspDestroyLogicalDisks(Device);
    }

    return Status;
}

VOID
UsbMasspDestroyLogicalDisks (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine destroys all logical disks associated with the given mass
    storage device.

Arguments:

    Device - Supplies a pointer to this mass storage device.

Return Value:

    None.

--*/

{

    PUSB_DISK Disk;

    while (LIST_EMPTY(&(Device->LogicalDiskList)) == FALSE) {
        Disk = LIST_VALUE(Device->LogicalDiskList.Next, USB_DISK, ListEntry);
        LIST_REMOVE(&(Disk->ListEntry));

        //
        // The mass storage driver should only need to call this on disks that
        // never completed enumeration.
        //

        ASSERT(Disk->OsDevice == NULL);
        ASSERT(Disk->Connected == FALSE);
        ASSERT(Disk->ReferenceCount == 1);

        UsbMasspDiskReleaseReference(Disk);
    }

    return;
}

PUSB_MASS_STORAGE_POLLED_IO_STATE
UsbMasspCreatePolledIoState (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine creates polled I/O state for the given USB mass storage device.

Arguments:

    Device - Supplies a pointer to the USB device that is to be the target of
        the polled I/O.

Return Value:

    Returns a pointer to a polled I/O state for the device.

--*/

{

    ULONG AllocationSize;
    PUSB_TRANSFER ControlTransfer;
    PUSB_MASS_STORAGE_POLLED_IO_STATE PolledIoState;
    KSTATUS Status;

    AllocationSize = sizeof(USB_MASS_STORAGE_POLLED_IO_STATE);
    PolledIoState = MmAllocateNonPagedPool(AllocationSize,
                                           USB_MASS_ALLOCATION_TAG);

    if (PolledIoState == NULL) {
        return NULL;
    }

    RtlZeroMemory(PolledIoState, AllocationSize);

    //
    // Create the I/O transfers for the newly minted polled I/O state. Since
    // these transfers will be used with polled I/O, they lack a callback
    // routine and user data.
    //

    Status = UsbMasspCreateTransfers(Device,
                                     &(PolledIoState->IoTransfers),
                                     NULL,
                                     NULL);

    if (!KSUCCESS(Status)) {
        goto CreatePolledIoStateEnd;
    }

    //
    // Allocate a control transfer that will be used to perform reset recovery.
    // It only ever needs to send a setup packet.
    //

    ControlTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                          0,
                                          sizeof(USB_SETUP_PACKET),
                                          0);

    if (ControlTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePolledIoStateEnd;
    }

    PolledIoState->ControlTransfer = ControlTransfer;

    //
    // Before polled I/O is used for the first time, assumably in a very
    // critical scenario (e.g. crash dump), then mass storage endpoints will
    // need to be reset.
    //

    PolledIoState->ResetRequired = TRUE;

CreatePolledIoStateEnd:
    if (!KSUCCESS(Status)) {
        if (PolledIoState != NULL) {
            UsbMasspDestroyPolledIoState(PolledIoState);
            PolledIoState = NULL;
        }
    }

    return PolledIoState;
}

VOID
UsbMasspDestroyPolledIoState (
    PUSB_MASS_STORAGE_POLLED_IO_STATE PolledIoState
    )

/*++

Routine Description:

    This routine destroys the given polled I/O state.

Arguments:

    PolledIoState - Supplies a pointer to the polled I/O state to destroy.

Return Value:

    None.

--*/

{

    ASSERT(PolledIoState != NULL);

    UsbMasspDestroyTransfers(&(PolledIoState->IoTransfers));
    if (PolledIoState->ControlTransfer != NULL) {
        UsbDestroyTransfer(PolledIoState->ControlTransfer);
    }

    MmFreeNonPagedPool(PolledIoState);
    return;
}

KSTATUS
UsbMasspCreateTransfers (
    PUSB_MASS_STORAGE_DEVICE Device,
    PUSB_MASS_STORAGE_TRANSFERS Transfers,
    PVOID UserData,
    PUSB_TRANSFER_CALLBACK CallbackRoutine
    )

/*++

Routine Description:

    This routine initializes a set of USB disk transfers by creating the
    command, status, and data transfers as well as any necessary buffers.

Arguments:

    Device - Supplies a pointer to the USB device to which the transfers
        belong.

    Transfers - Supplies a pointer to the USB disk transfers structure to be
        initialized.

    UserData - Supplies an optional pointer to the private user data with which
        the transfers will be initialized.

    CallbackRoutine - Supplies an optional pointer to the USB transfer
        callback routine with which the transfers will be initialized.

Return Value:

    Status code.

--*/

{

    ULONG Alignment;
    PIO_BUFFER CommandBuffer;
    PUSB_TRANSFER CommandTransfer;
    PUSB_TRANSFER DataOutTransfer;
    ULONG IoBufferFlags;
    ULONG MaxCommandBlockSize;
    ULONG MaxCommandBufferSize;
    ULONG MaxCommandStatusSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;
    PUSB_TRANSFER StatusTransfer;

    //
    // Create the I/O buffer used for commands.
    //

    Alignment = MmGetIoBufferAlignment();
    MaxCommandBufferSize = ALIGN_RANGE_UP(USB_MASS_COMMAND_BUFFER_SIZE,
                                          Alignment);

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;
    Transfers->CommandBuffer = MmAllocateNonPagedIoBuffer(0,
                                                          MAX_ULONG,
                                                          Alignment,
                                                          MaxCommandBufferSize,
                                                          IoBufferFlags);

    if (Transfers->CommandBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDiskTransfersEnd;
    }

    ASSERT(Transfers->CommandBuffer->FragmentCount == 1);

    //
    // Create a USB transfer to the get the Command Status Wrapper at the
    // end of a transfer.
    //

    StatusTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                         Device->InEndpoint,
                                         sizeof(SCSI_COMMAND_STATUS),
                                         0);

    if (StatusTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDiskTransfersEnd;
    }

    StatusTransfer->Direction = UsbTransferDirectionIn;
    StatusTransfer->Length = sizeof(SCSI_COMMAND_STATUS);
    StatusTransfer->CallbackRoutine = CallbackRoutine;
    StatusTransfer->UserData = UserData;
    Transfers->StatusTransfer = StatusTransfer;

    //
    // The buffer's virtual and physical address is calculated for each
    // request, but there should always be exactly the same amount of
    // memory used for the status transfer.
    //

    MaxCommandStatusSize = ALIGN_RANGE_UP(sizeof(SCSI_COMMAND_STATUS),
                                          Alignment);

    Transfers->StatusTransfer->BufferActualLength = MaxCommandStatusSize;

    //
    // Create the command transfer for sending the Command Block Wrapper.
    //

    CommandTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                          Device->OutEndpoint,
                                          sizeof(SCSI_COMMAND_BLOCK),
                                          0);

    if (CommandTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDiskTransfersEnd;
    }

    CommandTransfer->Direction = UsbTransferDirectionOut;
    CommandTransfer->Length = sizeof(SCSI_COMMAND_BLOCK);
    CommandBuffer = Transfers->CommandBuffer;
    CommandTransfer->Buffer = CommandBuffer->Fragment[0].VirtualAddress;
    PhysicalAddress = CommandBuffer->Fragment[0].PhysicalAddress;
    CommandTransfer->BufferPhysicalAddress = PhysicalAddress;
    MaxCommandBlockSize = ALIGN_RANGE_UP(sizeof(SCSI_COMMAND_BLOCK), Alignment);
    CommandTransfer->BufferActualLength = MaxCommandBlockSize;
    CommandTransfer->CallbackRoutine = CallbackRoutine;
    CommandTransfer->UserData = UserData;
    Transfers->CommandTransfer = CommandTransfer;

    //
    // Create the data in transfer for receiving data from an incoming
    // command.
    //

    Transfers->DataInTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                                    Device->InEndpoint,
                                                    USB_MASS_MAX_DATA_TRANSFER,
                                                    0);

    if (Transfers->DataInTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDiskTransfersEnd;
    }

    Transfers->DataInTransfer->Direction = UsbTransferDirectionIn;
    Transfers->DataInTransfer->CallbackRoutine = CallbackRoutine;
    Transfers->DataInTransfer->UserData = UserData;

    //
    // Create the data out transfer for sending data to the disk.
    //

    DataOutTransfer = UsbAllocateTransfer(Device->UsbCoreHandle,
                                          Device->OutEndpoint,
                                          USB_MASS_MAX_DATA_TRANSFER,
                                          0);

    if (DataOutTransfer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDiskTransfersEnd;
    }

    DataOutTransfer->Direction = UsbTransferDirectionOut;
    DataOutTransfer->CallbackRoutine = CallbackRoutine;
    DataOutTransfer->UserData = UserData;
    Transfers->DataOutTransfer = DataOutTransfer;
    Status = STATUS_SUCCESS;

CreateDiskTransfersEnd:
    return Status;
}

VOID
UsbMasspDestroyTransfers (
    PUSB_MASS_STORAGE_TRANSFERS Transfers
    )

/*++

Routine Description:

    This routine destroys a USB logical disk's transfers. It does not destroy
    the structure intself.

Arguments:

    Transfers - Supplies a pointer to a USB disk tranfers structure.

Return Value:

    None.

--*/

{

    if (Transfers->DataOutTransfer != NULL) {
        UsbDestroyTransfer(Transfers->DataOutTransfer);
    }

    if (Transfers->DataInTransfer != NULL) {
        UsbDestroyTransfer(Transfers->DataInTransfer);
    }

    if (Transfers->CommandTransfer != NULL) {
        UsbDestroyTransfer(Transfers->CommandTransfer);
    }

    if (Transfers->StatusTransfer != NULL) {
        UsbDestroyTransfer(Transfers->StatusTransfer);
    }

    if (Transfers->CommandBuffer != NULL) {
        MmFreeIoBuffer(Transfers->CommandBuffer);
    }

    return;
}

KSTATUS
UsbMasspStartDisk (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine attempts to fire up a USB logical disk.

Arguments:

    Disk - Supplies a pointer to the disk.

Return Value:

    None.

--*/

{

    ULONG BufferSize;
    PSCSI_INQUIRY_PAGE0 Page0;
    BOOL PolledIoSupported;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONG Try;

    KeAcquireQueuedLock(Disk->Device->Lock);

    //
    // Send the INQUIRY for page 0 as a friendly "hello!".
    //

    BufferSize = sizeof(SCSI_INQUIRY_PAGE0);
    Status = UsbMasspSendInquiry(Disk, 0, (PVOID)&Page0, &BufferSize);
    if (!KSUCCESS(Status)) {
        goto StartDiskEnd;
    }

    //
    // Get the block device parameters of the disk.
    //

    for (Try = 0; Try < USB_MASS_RETRY_COUNT; Try += 1) {
        Status = UsbMasspReadFormatCapacities(Disk);
        if (KSUCCESS(Status)) {
            break;
        }

        Status = UsbMasspRequestSense(Disk);
        if (!KSUCCESS(Status)) {
            goto StartDiskEnd;
        }
    }

    //
    // Ignore any errors from the read format capacities command and just try
    // to read the capacity.
    //

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * USB_MASS_READ_CAPACITY_TIMEOUT);

    do {
        Status = UsbMasspReadCapacity(Disk);
        if (KSUCCESS(Status)) {
            break;
        }

        if (!KSUCCESS(UsbMasspRequestSense(Disk))) {
            goto StartDiskEnd;
        }

        KeDelayExecution(FALSE, FALSE, 10 * MICROSECONDS_PER_MILLISECOND);
        Status = STATUS_TIMEOUT;

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("USB Mass: Failed to read capacity: %d\n", Status);
        goto StartDiskEnd;
    }

    //
    // Wait for the unit to become ready.
    //

    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * USB_MASS_UNIT_READY_TIMEOUT);

    do {
        Status = UsbMasspTestUnitReady(Disk);
        if (KSUCCESS(Status)) {
            break;
        }

        Status = UsbMasspRequestSense(Disk);
        if (!KSUCCESS(Status)) {
            goto StartDiskEnd;
        }

        Status = STATUS_TIMEOUT;

    } while (KeGetRecentTimeCounter() <= Timeout);

    if (!KSUCCESS(Status)) {
        goto StartDiskEnd;
    }

    //
    // Determine if polled I/O is supported, and create the disk interface if
    // so.
    //

    if (Disk->DiskInterface.DiskToken == NULL) {
        PolledIoSupported = UsbIsPolledIoSupported(Disk->Device->UsbCoreHandle);
        if (PolledIoSupported != FALSE) {
            RtlCopyMemory(&(Disk->DiskInterface),
                          &UsbMassDiskInterfaceTemplate,
                          sizeof(DISK_INTERFACE));

            Disk->DiskInterface.DiskToken = Disk;
            Disk->DiskInterface.BlockSize = 1 << Disk->BlockShift;
            Disk->DiskInterface.BlockCount = Disk->BlockCount;
            Status = IoCreateInterface(&UsbMassDiskInterfaceUuid,
                                       Disk->OsDevice,
                                       &(Disk->DiskInterface),
                                       sizeof(DISK_INTERFACE));

            if (!KSUCCESS(Status)) {
                Disk->DiskInterface.DiskToken = NULL;
                goto StartDiskEnd;
            }
        }
    }

StartDiskEnd:
    KeReleaseQueuedLock(Disk->Device->Lock);
    return Status;
}

VOID
UsbMasspRemoveDisk (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine attempts to remove a USB logical disk.

Arguments:

    Disk - Supplies a pointer to the disk.

Return Value:

    None.

--*/

{

    PUSB_MASS_STORAGE_DEVICE Device;

    //
    // Tear down the disk interface if it was brought up.
    //

    if (Disk->DiskInterface.DiskToken != NULL) {
        IoDestroyInterface(&UsbMassDiskInterfaceUuid,
                           Disk->OsDevice,
                           &(Disk->DiskInterface));

        Disk->DiskInterface.DiskToken = NULL;
    }

    //
    // Acquire the lock. Once the lock is held, the device will be no longer
    // be in the middle of any transfers. This guarantees any pending IRPs will
    // finish before the device is torn down.
    //

    Device = Disk->Device;
    KeAcquireQueuedLock(Device->Lock);

    //
    // Assert that there is no active IRP.
    //

    ASSERT(Disk->Irp == NULL);

    //
    // Mark the disk as removed to prevent further IO.
    //

    Disk->Connected = FALSE;

    //
    // Remove the disk from the parents device list while holding the lock.
    //

    LIST_REMOVE(&(Disk->ListEntry));
    KeReleaseQueuedLock(Device->Lock);

    //
    // Release the reference on the disk taken during creation. The disk will
    // be destroyed once all the open handles are closed.
    //

    UsbMasspDiskReleaseReference(Disk);
    return;
}

VOID
UsbMasspDestroyDisk (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine destroys a logical disk.

Arguments:

    Disk - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    //
    // The reference count should be zero.
    //

    ASSERT(Disk->ReferenceCount == 0);

    //
    // Destroy all structures that were created.
    //

    UsbMasspDestroyTransfers(&(Disk->Transfers));
    if (Disk->Event != NULL) {
        KeDestroyEvent(Disk->Event);
    }

    //
    // Release the reference taken on the parent during disk creation.
    //

    UsbMasspDeviceReleaseReference(Disk->Device);

    //
    // Destroy the device structure.
    //

    MmFreeNonPagedPool(Disk);
    return;
}

VOID
UsbMasspDiskAddReference (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine adds a reference to USB mass storage logical disk.

Arguments:

    Disk - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Disk->ReferenceCount), 1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    return;
}

VOID
UsbMasspDiskReleaseReference (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine releases a reference from the USB mass storage logical disk.

Arguments:

    Disk - Supplies a pointer to the USB mass storage device.

Return Value:

    None.

--*/

{

    ULONG OldReferenceCount;

    OldReferenceCount = RtlAtomicAdd32(&(Disk->ReferenceCount), (ULONG)-1);

    ASSERT((OldReferenceCount != 0) && (OldReferenceCount < 0x10000000));

    if (OldReferenceCount == 1) {
        UsbMasspDestroyDisk(Disk);
    }

    return;
}

KSTATUS
UsbMasspSendInquiry (
    PUSB_DISK Disk,
    UCHAR Page,
    PVOID *ResultBuffer,
    PULONG ResultBufferSize
    )

/*++

Routine Description:

    This routine sends an inquiry to the USB disk device. This routine assumes
    the mass storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to inquire to.

    Page - Supplies the information page number to retrieve.

    ResultBuffer - Supplies a pointer where the buffer containing the
        information will be returned on success. This buffer will be pointing
        into the disk's command buffer.

    ResultBufferSize - Supplies a pointer pointing to an integer that on input
        contains the expected size of the buffer. On output, contains the number
        of bytes transferred.

Return Value:

    Status code.

--*/

{

    ULONG BytesTransferred;
    PUCHAR InquiryCommand;
    KSTATUS Status;

    ASSERT(Disk->Irp == NULL);

    BytesTransferred = 0;
    *ResultBuffer = NULL;

    //
    // Set up the standard portion of the command block wrapper.
    //

    InquiryCommand = UsbMasspSetupCommand(Disk,
                                          0,
                                          *ResultBufferSize,
                                          SCSI_COMMAND_INQUIRY_SIZE,
                                          TRUE,
                                          FALSE,
                                          NULL,
                                          0);

    //
    // Set up the command portion for an inquiry command.
    //

    *InquiryCommand = SCSI_COMMAND_INQUIRY;

    ASSERT(Disk->LunNumber <= 7);

    *(InquiryCommand + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(InquiryCommand + 4) = *ResultBufferSize;
    Disk->Transfers.DataInTransfer->Length = *ResultBufferSize;

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto SendInquiryEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        goto SendInquiryEnd;
    }

    if (BytesTransferred > *ResultBufferSize) {
        Status = STATUS_BUFFER_TOO_SMALL;
        goto SendInquiryEnd;
    }

    *ResultBuffer = Disk->Transfers.DataInTransfer->Buffer;

SendInquiryEnd:
    *ResultBufferSize = BytesTransferred;
    return Status;
}

KSTATUS
UsbMasspTestUnitReady (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine sends a "test unit ready" command to the USB disk. This
    routine assumes the mass storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to send the command to.

Return Value:

    STATUS_SUCCESS if the device is ready.

    STATUS_NOT_READY if the device is not ready.

    Other error codes on failure.

--*/

{

    ULONG BytesTransferred;
    KSTATUS Status;
    PUCHAR TestUnitReadyCommand;

    ASSERT(Disk->Irp == NULL);

    BytesTransferred = 0;

    //
    // Set up the standard portion of the command block wrapper.
    //

    TestUnitReadyCommand = UsbMasspSetupCommand(
                                             Disk,
                                             0,
                                             0,
                                             SCSI_COMMAND_TEST_UNIT_READY_SIZE,
                                             TRUE,
                                             FALSE,
                                             NULL,
                                             0);

    //
    // Set up the command portion for an inquiry command.
    //

    *TestUnitReadyCommand = SCSI_COMMAND_TEST_UNIT_READY;

    ASSERT(Disk->LunNumber <= 7);

    *(TestUnitReadyCommand + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(TestUnitReadyCommand + 4) = 0;
    Disk->Transfers.DataInTransfer->Length = 0;

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto TestUnitReadyEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        Status = STATUS_NOT_READY;
        goto TestUnitReadyEnd;
    }

TestUnitReadyEnd:
    return Status;
}

KSTATUS
UsbMasspRequestSense (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine sends a "request sense data" command to the USB disk. This
    routine assumes the mass storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to inquire to.

Return Value:

    STATUS_SUCCESS if the device is ready.

    STATUS_NOT_READY if the device is not ready.

    Other error codes on failure.

--*/

{

    ULONG BytesTransferred;
    PUCHAR RequestSenseCommand;
    KSTATUS Status;

    ASSERT(Disk->Irp == NULL);

    BytesTransferred = 0;

    //
    // Set up the standard portion of the command block wrapper.
    //

    RequestSenseCommand = UsbMasspSetupCommand(
                                          Disk,
                                          0,
                                          SCSI_COMMAND_REQUEST_SENSE_DATA_SIZE,
                                          SCSI_COMMAND_REQUEST_SENSE_SIZE,
                                          TRUE,
                                          FALSE,
                                          NULL,
                                          0);

    //
    // Set up the command portion for an inquiry command.
    //

    *RequestSenseCommand = SCSI_COMMAND_REQUEST_SENSE;

    ASSERT(Disk->LunNumber <= 7);

    *(RequestSenseCommand + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(RequestSenseCommand + 4) = SCSI_COMMAND_REQUEST_SENSE_DATA_SIZE;
    Disk->Transfers.DataInTransfer->Length =
                                          SCSI_COMMAND_REQUEST_SENSE_DATA_SIZE;

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto RequestSenseEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        goto RequestSenseEnd;
    }

RequestSenseEnd:
    return Status;
}

KSTATUS
UsbMasspModeSense (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine sends a "mode sense" command to the USB disk. This routine
    assumes the mass storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to inquire to.

Return Value:

    STATUS_SUCCESS if the device is ready.

    STATUS_NOT_READY if the device is not ready.

    Other error codes on failure.

--*/

{

    ULONG BytesTransferred;
    PUCHAR RequestSenseCommand;
    KSTATUS Status;

    ASSERT(Disk->Irp == NULL);

    BytesTransferred = 0;

    //
    // Set up the standard portion of the command block wrapper.
    //

    RequestSenseCommand = UsbMasspSetupCommand(
                                            Disk,
                                            0,
                                            SCSI_COMMAND_MODE_SENSE_6_DATA_SIZE,
                                            SCSI_COMMAND_MODE_SENSE_6_SIZE,
                                            TRUE,
                                            FALSE,
                                            NULL,
                                            0);

    //
    // Set up the command portion for an inquiry command.
    //

    *RequestSenseCommand = SCSI_COMMAND_MODE_SENSE_6;

    ASSERT(Disk->LunNumber <= 7);

    *(RequestSenseCommand + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(RequestSenseCommand + 2) = 0x1C;
    *(RequestSenseCommand + 4) = SCSI_COMMAND_MODE_SENSE_6_DATA_SIZE;
    Disk->Transfers.DataInTransfer->Length =
                                           SCSI_COMMAND_MODE_SENSE_6_DATA_SIZE;

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto ModeSenseEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        goto ModeSenseEnd;
    }

ModeSenseEnd:
    return Status;
}

KSTATUS
UsbMasspReadFormatCapacities (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine reads the capacity into the device using the "read format
    capacities" command. The results will be written into the disk structure on
    success. This routine assumes the mass storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to query.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    ULONG BytesTransferred;
    PSCSI_FORMAT_CAPACITIES Capacities;
    PUCHAR Command;
    PUSB_TRANSFER DataInTransfer;
    KSTATUS Status;

    ASSERT(Disk->Irp == NULL);

    Command = UsbMasspSetupCommand(
                                 Disk,
                                 0,
                                 SCSI_COMMAND_READ_FORMAT_CAPACITIES_DATA_SIZE,
                                 SCSI_COMMAND_READ_FORMAT_CAPACITIES_SIZE,
                                 TRUE,
                                 FALSE,
                                 NULL,
                                 0);

    //
    // Set up the command portion for a read format capacities command.
    //

    *Command = SCSI_COMMAND_READ_FORMAT_CAPACITIES;

    ASSERT(Disk->LunNumber <= 7);

    *(Command + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(Command + 8) = SCSI_COMMAND_READ_FORMAT_CAPACITIES_DATA_SIZE;
    DataInTransfer = Disk->Transfers.DataInTransfer;
    DataInTransfer->Length = SCSI_COMMAND_READ_FORMAT_CAPACITIES_DATA_SIZE;

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto ReadFormatCapacitiesEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        goto ReadFormatCapacitiesEnd;
    }

    if (BytesTransferred < sizeof(SCSI_FORMAT_CAPACITIES)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadFormatCapacitiesEnd;
    }

    Capacities = (PSCSI_FORMAT_CAPACITIES)DataInTransfer->Buffer;
    Disk->BlockCount = CONVERT_BIG_ENDIAN_TO_CPU32(Capacities->BlockCount) + 1;
    BlockSize = CONVERT_BIG_ENDIAN_TO_CPU32(Capacities->BlockLength);
    if ((Disk->BlockCount == 0) || (BlockSize == 0)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto ReadFormatCapacitiesEnd;
    }

    if (POWER_OF_2(BlockSize) == FALSE) {
        RtlDebugPrint("USB MASS: Invalid block size 0x%08x for device 0x%08x\n",
                      BlockSize,
                      Disk->OsDevice);

        Status = STATUS_INVALID_CONFIGURATION;
        goto ReadFormatCapacitiesEnd;
    }

    Disk->BlockShift = RtlCountTrailingZeros32(BlockSize);

ReadFormatCapacitiesEnd:
    return Status;
}

KSTATUS
UsbMasspReadCapacity (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine reads the capacity into the device. The results will be
    written into the disk structure on success. This routine assumes the mass
    storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk to query.

Return Value:

    Status code.

--*/

{

    ULONG BlockSize;
    ULONG BytesTransferred;
    PSCSI_CAPACITY Capacity;
    PUCHAR Command;
    KSTATUS Status;

    ASSERT(Disk->Irp == NULL);

    Command = UsbMasspSetupCommand(Disk,
                                   0,
                                   sizeof(SCSI_CAPACITY),
                                   SCSI_COMMAND_READ_CAPACITY_SIZE,
                                   TRUE,
                                   FALSE,
                                   NULL,
                                   0);

    //
    // Set up the command portion for a read capacity command.
    //

    *Command = SCSI_COMMAND_READ_CAPACITY;

    ASSERT(Disk->LunNumber <= 7);

    *(Command + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    Disk->Transfers.DataInTransfer->Length = sizeof(SCSI_CAPACITY);

    //
    // Send the command.
    //

    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto ReadCapacityEnd;
    }

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    if (!KSUCCESS(Status)) {
        goto ReadCapacityEnd;
    }

    if (BytesTransferred < sizeof(SCSI_CAPACITY)) {
        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto ReadCapacityEnd;
    }

    Capacity = (PSCSI_CAPACITY)Disk->Transfers.DataInTransfer->Buffer;
    Disk->BlockCount =
              CONVERT_BIG_ENDIAN_TO_CPU32(Capacity->LastValidBlockAddress) + 1;

    BlockSize = CONVERT_BIG_ENDIAN_TO_CPU32(Capacity->BlockLength);
    if ((Disk->BlockCount == 0) || (BlockSize == 0)) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto ReadCapacityEnd;
    }

    if (POWER_OF_2(BlockSize) == FALSE) {
        RtlDebugPrint("USB MASS: Invalid block size 0x%08x for device 0x%08x\n",
                      BlockSize,
                      Disk->OsDevice);

        Status = STATUS_INVALID_CONFIGURATION;
        goto ReadCapacityEnd;
    }

    Disk->BlockShift = RtlCountTrailingZeros32(BlockSize);

ReadCapacityEnd:
    return Status;
}

PVOID
UsbMasspSetupCommand (
    PUSB_DISK Disk,
    ULONG Tag,
    ULONG DataLength,
    UCHAR CommandLength,
    BOOL DataIn,
    BOOL PolledIo,
    PVOID TransferVirtualAddress,
    PHYSICAL_ADDRESS TransferPhysicalAddress
    )

/*++

Routine Description:

    This routine prepares to send a command to a disk by setting up the command
    block wrapper and command status wrapper. This routine assumes the mass
    storage device lock is already held.

Arguments:

    Disk - Supplies a pointer to the disk that the command will be sent to.

    Tag - Supplies an optional tag value to use, which can be useful in matching
        up responses to requests.

    DataLength - Supplies the length of the non-command portion of the transfer.

    CommandLength - Supplies the length of the remaining free-form command
        part of the Command Block Wrapper.

    DataIn - Supplies a boolean indicating if the command is coming into the
        host (TRUE) or going out to the device (FALSE).

    PolledIo - Supplies a boolean indicating if the command is using the polled
        I/O transfers (TRUE) or the default transfers (FALSE).

    TransferVirtualAddress - Supplies a pointer to an optional buffer to use
        for the data transfer.

    TransferPhysicalAddress - Supplies the base physical address of the
        transfer buffer. This parameter is optional but is required if the
        transfer buffer is supplied.

Return Value:

    Returns a pointer to the first free-form command byte in the Command Block
    Wrapper.

--*/

{

    ULONG AlignedDataLength;
    ULONG BufferAlignment;
    PSCSI_COMMAND_BLOCK Command;
    PUSB_TRANSFER CommandTransfer;
    PUSB_TRANSFER StatusTransfer;
    PUSB_MASS_STORAGE_TRANSFERS Transfers;

    if (PolledIo != FALSE) {

        ASSERT(Disk->Device->PolledIoState != NULL);

        Transfers = &(Disk->Device->PolledIoState->IoTransfers);

    } else {
        Transfers = &(Disk->Transfers);
    }

    CommandTransfer = Transfers->CommandTransfer;
    StatusTransfer = Transfers->StatusTransfer;

    //
    // Set up the Command Block Wrapper (CBW).
    //

    Command = CommandTransfer->Buffer;
    RtlZeroMemory(Command, sizeof(SCSI_COMMAND_BLOCK));
    Command->Signature = SCSI_COMMAND_BLOCK_SIGNATURE;
    Command->Tag = Tag;
    Command->DataTransferLength = DataLength;
    if (DataIn != FALSE) {
        Command->Flags = SCSI_COMMAND_BLOCK_FLAG_DATA_IN;

    } else {
        Command->Flags = 0;
    }

    Command->LunNumber = Disk->LunNumber;

    ASSERT(CommandLength <= 0x10);

    Command->CommandLength = CommandLength;

    //
    // If no transfer buffer is supplied, then the transfer will use the
    // command buffer. Set the status buffer after the data.
    //

    BufferAlignment = MmGetIoBufferAlignment();
    if (TransferVirtualAddress == NULL) {
        AlignedDataLength = ALIGN_RANGE_UP(DataLength, BufferAlignment);

    //
    // If a transfer buffer is supplied, the status buffer can start right
    // after the command.
    //

    } else {
        AlignedDataLength = 0;
    }

    ASSERT((CommandTransfer->BufferActualLength +
            StatusTransfer->BufferActualLength +
            AlignedDataLength) <=
            ALIGN_RANGE_UP(USB_MASS_COMMAND_BUFFER_SIZE, BufferAlignment));

    ASSERT(IS_ALIGNED((UINTN)CommandTransfer->Buffer, BufferAlignment));
    ASSERT(IS_ALIGNED(CommandTransfer->BufferPhysicalAddress, BufferAlignment));

    //
    // Set the location and zero out the CSW.
    //

    StatusTransfer->Buffer = CommandTransfer->Buffer +
                             CommandTransfer->BufferActualLength +
                             AlignedDataLength;

    StatusTransfer->BufferPhysicalAddress =
                                       CommandTransfer->BufferPhysicalAddress +
                                       CommandTransfer->BufferActualLength +
                                       AlignedDataLength;

    RtlZeroMemory(StatusTransfer->Buffer, sizeof(SCSI_COMMAND_STATUS));

    ASSERT(IS_ALIGNED((UINTN)StatusTransfer->Buffer, BufferAlignment));
    ASSERT(IS_ALIGNED(StatusTransfer->BufferPhysicalAddress, BufferAlignment));

    //
    // Set up the data in transfer to point immediately after the command block
    // or to the supplied buffer.
    //

    if (TransferVirtualAddress == NULL) {
        DataLength = AlignedDataLength;
        TransferVirtualAddress = CommandTransfer->Buffer +
                                 CommandTransfer->BufferActualLength;

        TransferPhysicalAddress = CommandTransfer->BufferPhysicalAddress +
                                  CommandTransfer->BufferActualLength;
    }

    ASSERT(TransferPhysicalAddress != INVALID_PHYSICAL_ADDRESS);

    Transfers->DataInTransfer->Length = 0;
    Transfers->DataInTransfer->Buffer = TransferVirtualAddress;
    Transfers->DataInTransfer->BufferPhysicalAddress = TransferPhysicalAddress;
    Transfers->DataInTransfer->BufferActualLength = DataLength;
    Transfers->DataOutTransfer->Length = 0;
    Transfers->DataOutTransfer->Buffer = TransferVirtualAddress;
    Transfers->DataOutTransfer->BufferPhysicalAddress = TransferPhysicalAddress;
    Transfers->DataOutTransfer->BufferActualLength = DataLength;
    return &(Command->Command);
}

KSTATUS
UsbMasspSendCommand (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine sends the command that's primed up in the command buffer of
    the given USB disk. This routine assumes the mass storage device lock is
    already held.

Arguments:

    Disk - Supplies a pointer to the disk that the command will be sent to.

    Irp - Supplies a pointer to an I/O IRP to continue when the transfer
        completes. If this is NULL, the transfer will be completed
        synchronously.

Return Value:

    Status code. This status code may represent a failure to send, communicate,
    or the disk failing the operation in the CSW. If the Command Status Wrapper
    returns a phase error, this routine will reset the disk.

--*/

{

    KSTATUS Status;

    ASSERT(KeIsQueuedLockHeld(Disk->Device->Lock) != FALSE);

    if (Disk->Irp == NULL) {
        KeSignalEvent(Disk->Event, SignalOptionUnsignal);
    }

    Disk->StatusTransferAttempts = 0;

    //
    // Send the Command Block Wrapper.
    //

    Status = UsbSubmitTransfer(Disk->Transfers.CommandTransfer);
    if (!KSUCCESS(Status)) {
        goto SendCommandEnd;
    }

    //
    // If there's an IRP, return immediately.
    //

    if (Disk->Irp != NULL) {
        Status = STATUS_SUCCESS;
        goto SendCommandEnd;
    }

    //
    // This is a synchronous transfer. Mark that all transfers have been
    // submitted, and wait for them to come back.
    //

    KeWaitForEvent(Disk->Event, FALSE, WAIT_TIME_INDEFINITE);
    Status = STATUS_SUCCESS;

SendCommandEnd:
    return Status;
}

VOID
UsbMasspTransferCompletionCallback (
    PUSB_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when a USB transfer completes for mass storage.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    ULONG BytesTransferred;
    BOOL CompleteIrp;
    PUSB_DISK Disk;
    UCHAR Endpoint;
    PIRP Irp;
    KSTATUS Status;
    BOOL SubmitStatusTransfer;
    PUSB_MASS_STORAGE_TRANSFERS Transfers;
    BOOL TransferSent;

    CompleteIrp = FALSE;
    Disk = (PUSB_DISK)Transfer->UserData;
    Irp = Disk->Irp;
    SubmitStatusTransfer = FALSE;
    Transfers = &(Disk->Transfers);
    TransferSent = FALSE;

    ASSERT((Disk != NULL) && (Disk->Type == UsbMassStorageLogicalDisk));
    ASSERT(KeIsQueuedLockHeld(Disk->Device->Lock) != FALSE);

    //
    // Handle stall failures according to the transfer type. All other failures
    // should just roll through until the command status transfer is returned.
    //

    if ((Transfer != Transfers->CommandTransfer) &&
        !KSUCCESS(Transfer->Status) &&
        (Transfer->Error == UsbErrorTransferStalled)) {

        //
        // Pick the correct endpoint to clear. The status and data IN transfers
        // clear the IN endpoint. The data OUT transfer clears the out endpoint.
        //

        Endpoint = Disk->Device->InEndpoint;
        if (Transfer == Transfers->DataOutTransfer) {
            Endpoint = Disk->Device->OutEndpoint;
        }

        UsbMasspClearEndpoint(Disk->Device, Endpoint, FALSE);

        //
        // Attempt to receive another command status wrapper if allowed.
        //

        if ((Transfer == Transfers->StatusTransfer) &&
            (Disk->StatusTransferAttempts <
             USB_MASS_STATUS_TRANSFER_ATTEMPT_LIMIT)) {

            SubmitStatusTransfer = TRUE;
        }
    }

    //
    // If this is a successful command transfer completing, then fire off the
    // next transfer in the set. If the command transfer fails, this I/O
    // request is toast.
    //

    if (Transfer == Transfers->CommandTransfer) {
        if (KSUCCESS(Transfer->Status)) {

            //
            // If there's data, submit the appropriate data transfer.
            //

            Disk->Transfers.DataInTransfer->Error = UsbErrorNone;
            Disk->Transfers.DataOutTransfer->Error = UsbErrorNone;
            if (Disk->Transfers.DataInTransfer->Length != 0) {

                ASSERT(Disk->Transfers.DataOutTransfer->Length == 0);

                TransferSent = TRUE;
                Status = UsbSubmitTransfer(Disk->Transfers.DataInTransfer);
                if (!KSUCCESS(Status)) {
                    TransferSent = FALSE;
                }

            } else if (Disk->Transfers.DataOutTransfer->Length != 0) {
                TransferSent = TRUE;
                Status = UsbSubmitTransfer(Disk->Transfers.DataOutTransfer);
                if (!KSUCCESS(Status)) {
                    TransferSent = FALSE;
                }

            //
            // Otherwise submit the transfer for the status word. If there is
            // data then the status transfer will be submitted when the data
            // portion is done.
            //

            } else {

                ASSERT((Disk->Transfers.DataInTransfer->Length == 0) &&
                       (Disk->Transfers.DataOutTransfer->Length == 0));

                SubmitStatusTransfer = TRUE;
            }
        }

    //
    // If the data IN or data OUT portion completed, submit the status transfer.
    // The status transfer needs to be received even if the data transfer
    // failed (or was cancelled). If the submission fails, it will be handled
    // below with a reset. If a device I/O error occurred during the data
    // portion, just skip the status transfer; the endpoint will go through
    // reset recovery.
    //

    } else if ((Transfer != Transfers->StatusTransfer) &&
               (Transfer->Error != UsbErrorTransferDeviceIo)) {

        ASSERT((Transfer == Transfers->DataInTransfer) ||
               (Transfer == Transfers->DataOutTransfer));

        SubmitStatusTransfer = TRUE;
    }

    //
    // If the status transfer needs to be submitted or resubmitted, fire it off.
    //

    if (SubmitStatusTransfer != FALSE) {
        TransferSent = TRUE;
        Disk->StatusTransferAttempts += 1;
        Status = UsbSubmitTransfer(Disk->Transfers.StatusTransfer);
        if (!KSUCCESS(Status)) {
            Disk->StatusTransferAttempts -= 1;
            TransferSent = FALSE;
        }
    }

    //
    // Do not do any processing if another transfer was sent.
    //

    if (TransferSent != FALSE) {
        return;
    }

    //
    // If the IRP is NULL, this must have been a synchronous transfer. If so,
    // signal the event and let it handle the processing.
    //

    if (Irp == NULL) {
        KeSignalEvent(Disk->Event, SignalOptionSignalAll);
        return;
    }

    //
    // Evaluate the result of the transfer and continue the IRP.
    //

    Status = UsbMasspEvaluateCommandStatus(Disk,
                                           FALSE,
                                           FALSE,
                                           &BytesTransferred);

    Disk->CurrentFragmentOffset += BytesTransferred;
    Disk->CurrentBytesTransferred += BytesTransferred;

    ASSERT(Disk->CurrentBytesTransferred <= Irp->U.ReadWrite.IoSizeInBytes);

    //
    // If the command succeeded and all the bytes have been transferred, then
    // complete the IRP.
    //

    if (KSUCCESS(Status)) {
        if (Disk->CurrentBytesTransferred == Irp->U.ReadWrite.IoSizeInBytes) {
            CompleteIrp = TRUE;
            goto TransferCompletionCallbackEnd;
        }

        Disk->IoRequestAttempts = 0;

    //
    // If it failed, prep to try the command again, unless it has been
    // attempted too many times.
    //

    } else {
        Disk->IoRequestAttempts += 1;
        if (Disk->IoRequestAttempts > USB_MASS_IO_REQUEST_RETRY_COUNT) {
            CompleteIrp = TRUE;
            goto TransferCompletionCallbackEnd;
        }
    }

    //
    // Request the next batch of stuff (it could also be retry of the same
    // batch). If this fails, complete the IRP. Do not attempt any retries, as
    // failure here indicates a more serious failure (e.g. the command transfer
    // failed to even be submitted).
    //

    Status = UsbMasspSendNextIoRequest(Disk);
    if (!KSUCCESS(Status)) {
        CompleteIrp = TRUE;
        goto TransferCompletionCallbackEnd;
    }

TransferCompletionCallbackEnd:
    if (CompleteIrp != FALSE) {

        ASSERT(Disk->Irp != NULL);

        Irp->U.ReadWrite.IoBytesCompleted = Disk->CurrentBytesTransferred;
        Irp->U.ReadWrite.NewIoOffset = Irp->U.ReadWrite.IoOffset +
                                       Irp->U.ReadWrite.IoBytesCompleted;

        IoCompleteIrp(UsbMassDriver, Irp, Status);
    }

    return;
}

KSTATUS
UsbMasspEvaluateCommandStatus (
    PUSB_DISK Disk,
    BOOL PolledIo,
    BOOL DisableRecovery,
    PULONG BytesTransferred
    )

/*++

Routine Description:

    This routine evaluates a Command Status Wrapper. This follows the USB Mass
    Storage Class Bulk-Only Transport specification sections 6.5, 6.6, and 6.7.

Arguments:

    Disk - Supplies a pointer to the disk that just completed a command.

    PolledIo - Supplies a boolean indicating if the polled I/O transfers should
        be evaluated (TRUE) or the default transfers (FALSE).

    DisableRecovery - Supplies a boolean indicating if reset recovery is
        disabled (TRUE) or enabled (FALSE).

    BytesTransferred - Supplies a pointer where the number of completed bytes
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if the SCSI status was unsuccessful or a phase error.

    STATUS_CHECKSUM_MISMATCH if the signature or tag values don't match.

    Other errors if the USB transfers themselves failed.

--*/

{

    PSCSI_COMMAND_BLOCK CommandBlock;
    PSCSI_COMMAND_STATUS CommandStatus;
    KSTATUS Status;
    PUSB_TRANSFER StatusTransfer;
    PUSB_MASS_STORAGE_TRANSFERS Transfers;

    *BytesTransferred = 0;
    if (PolledIo != FALSE) {

        ASSERT(Disk->Device->PolledIoState != NULL);

        Transfers = &(Disk->Device->PolledIoState->IoTransfers);

    } else {
        Transfers = &(Disk->Transfers);
    }

    //
    // If the command transfer failed, there is no guarantee about any of the
    // subsequent transfers. Just reset the device and exit.
    //

    if (!KSUCCESS(Transfers->CommandTransfer->Status)) {
        Status = Transfers->CommandTransfer->Status;
        goto EvaluateCommandStatusEnd;
    }

    ASSERT(Transfers->CommandTransfer->LengthTransferred ==
           Transfers->CommandTransfer->Length);

    if ((Transfers->DataInTransfer->Error != UsbErrorNone) ||
        (Transfers->DataOutTransfer->Error != UsbErrorNone)) {

        Status = STATUS_DEVICE_IO_ERROR;
        goto EvaluateCommandStatusEnd;
    }

    //
    // First check to see if the command status transfer itself was successful.
    // If not, reset the device and return. The device will not receive another
    // command transfer until it sends a CSW or a reset occurred. Without a
    // successful status transfer, there is no guarantee the CSW was sent.
    //

    if (!KSUCCESS(Transfers->StatusTransfer->Status)) {
        Status = Transfers->StatusTransfer->Status;
        goto EvaluateCommandStatusEnd;
    }

    //
    // Check to see if the command status transfer is valid. The length
    // transferred has to match the length, the signature needs to match and
    // the tag needs to match that of the command block transfer.
    //

    StatusTransfer = Transfers->StatusTransfer;
    CommandBlock = (PSCSI_COMMAND_BLOCK)Transfers->CommandTransfer->Buffer;
    CommandStatus = (PSCSI_COMMAND_STATUS)StatusTransfer->Buffer;
    if ((StatusTransfer->LengthTransferred != StatusTransfer->Length) ||
        (CommandStatus->Signature != SCSI_COMMAND_STATUS_SIGNATURE) ||
        (CommandStatus->Tag != CommandBlock->Tag)) {

        RtlDebugPrint("USBMASS: CSW Signature and tag were 0x%x 0x%x. "
                      "Possible USB or cache coherency issues.\n",
                      CommandStatus->Signature,
                      CommandStatus->Tag);

        Status = STATUS_DEVICE_IO_ERROR;
        goto EvaluateCommandStatusEnd;
    }

    //
    // Check to see if the status is meaningful. A meaningful status is
    // indicated in two ways. The first is when the status is either success or
    // failure and the residue is less than or equal the transfer length.
    //

    if (((CommandStatus->Status == SCSI_STATUS_SUCCESS) ||
         (CommandStatus->Status == SCSI_STATUS_FAILED)) &&
        (CommandStatus->DataResidue <= CommandBlock->DataTransferLength)) {

        *BytesTransferred = CommandBlock->DataTransferLength -
                            CommandStatus->DataResidue;

        Status = STATUS_SUCCESS;
        goto EvaluateCommandStatusEnd;
    }

    //
    // The second is when the status indicates a phase error. A reset recovery
    // is required and the residue data is ignored.
    //

    if (CommandStatus->Status == SCSI_STATUS_PHASE_ERROR) {
        Status = STATUS_DEVICE_IO_ERROR;
        goto EvaluateCommandStatusEnd;
    }

    //
    // The status is valid, but not meaningful. Section 6.5 of the USB mass
    // storage specification (bulk-only) indicates that a host "may" perform
    // a reset recovery in this case, but is not required. Well, why not? All
    // the other failures in this routine are doing it.
    //

    Status = STATUS_DEVICE_IO_ERROR;

EvaluateCommandStatusEnd:
    if (!KSUCCESS(Status)) {
        if (DisableRecovery == FALSE) {
            UsbMasspResetRecovery(Disk->Device, PolledIo);
        }
    }

    return Status;
}

KSTATUS
UsbMasspSendNextIoRequest (
    PUSB_DISK Disk
    )

/*++

Routine Description:

    This routine starts transmission of the next chunk of I/O in a data
    transfer request.

Arguments:

    Disk - Supplies a pointer to the disk to transfer to or from.

Return Value:

    Status code.

--*/

{

    ULONGLONG Block;
    UINTN BlockCount;
    UINTN BytesToTransfer;
    UCHAR Command;
    PUCHAR CommandBuffer;
    BOOL CommandIn;
    UCHAR CommandLength;
    PIO_BUFFER IoBuffer;
    PIRP Irp;
    PHYSICAL_ADDRESS PhysicalAddress;
    UINTN RequestSize;
    KSTATUS Status;
    PUSB_TRANSFER UsbDataTransfer;
    PVOID VirtualAddress;

    Irp = Disk->Irp;
    IoBuffer = Irp->U.ReadWrite.IoBuffer;

    ASSERT(Irp != NULL);
    ASSERT(IoBuffer != NULL);
    ASSERT(Disk->CurrentBytesTransferred < Irp->U.ReadWrite.IoSizeInBytes);
    ASSERT(Disk->CurrentFragment < IoBuffer->FragmentCount);
    ASSERT(Disk->CurrentFragmentOffset <=
           IoBuffer->Fragment[Disk->CurrentFragment].Size);

    //
    // Advance to the next fragment if the end of the previous one was reached.
    //

    if (Disk->CurrentFragmentOffset ==
        IoBuffer->Fragment[Disk->CurrentFragment].Size) {

        Disk->CurrentFragment += 1;
        Disk->CurrentFragmentOffset = 0;

        //
        // End if this was the last fragment.
        //

        if (Disk->CurrentFragment == IoBuffer->FragmentCount) {

            ASSERT(Disk->CurrentBytesTransferred ==
                   Irp->U.ReadWrite.IoSizeInBytes);

            Status = STATUS_SUCCESS;
            goto SendNextIoRequestEnd;
        }
    }

    //
    // Transfer the rest of the fragment, but cap it to the max of what the
    // allocated USB transfer can do and on how many bytes have already been
    // transferred and/or need to be transferred.
    //

    RequestSize = IoBuffer->Fragment[Disk->CurrentFragment].Size -
                  Disk->CurrentFragmentOffset;

    BytesToTransfer = Irp->U.ReadWrite.IoSizeInBytes -
                      Disk->CurrentBytesTransferred;

    if (BytesToTransfer < RequestSize) {
        RequestSize = BytesToTransfer;
    }

    if (RequestSize > USB_MASS_MAX_DATA_TRANSFER) {
        RequestSize = USB_MASS_MAX_DATA_TRANSFER;
    }

    ASSERT(RequestSize != 0);
    ASSERT(IS_ALIGNED(RequestSize, MmGetIoBufferAlignment()) != FALSE);

    PhysicalAddress =
                    IoBuffer->Fragment[Disk->CurrentFragment].PhysicalAddress +
                    Disk->CurrentFragmentOffset;

    VirtualAddress = IoBuffer->Fragment[Disk->CurrentFragment].VirtualAddress +
                     Disk->CurrentFragmentOffset;

    //
    // Compute the block offset and size.
    //

    Block = Irp->U.ReadWrite.IoOffset + Disk->CurrentBytesTransferred;

    ASSERT(IS_ALIGNED(Block, (1 << Disk->BlockShift)) != FALSE);

    Block >>= Disk->BlockShift;

    ASSERT(IS_ALIGNED(RequestSize, (1 << Disk->BlockShift)) != FALSE);
    ASSERT(Block == (ULONG)Block);

    BlockCount = RequestSize >> Disk->BlockShift;

    ASSERT(BlockCount == (USHORT)BlockCount);
    ASSERT(RequestSize == (ULONG)RequestSize);

    //
    // Watch for doing I/O off the end of the device.
    //

    if ((Block >= Disk->BlockCount) ||
        (Block + BlockCount > Disk->BlockCount)) {

        Status = STATUS_OUT_OF_BOUNDS;
        goto SendNextIoRequestEnd;
    }

    //
    // Set up the transfer.
    //

    if (Irp->MinorCode == IrpMinorIoRead) {
        Command = SCSI_COMMAND_READ_10;
        CommandLength = SCSI_COMMAND_READ_10_SIZE;
        CommandIn = TRUE;
        UsbDataTransfer = Disk->Transfers.DataInTransfer;

    } else {

        ASSERT(Irp->MinorCode == IrpMinorIoWrite);

        Command = SCSI_COMMAND_WRITE_10;
        CommandLength = SCSI_COMMAND_WRITE_10_SIZE;
        CommandIn = FALSE;
        UsbDataTransfer = Disk->Transfers.DataOutTransfer;
    }

    CommandBuffer = UsbMasspSetupCommand(Disk,
                                         Command,
                                         (ULONG)RequestSize,
                                         CommandLength,
                                         CommandIn,
                                         FALSE,
                                         VirtualAddress,
                                         PhysicalAddress);

    *CommandBuffer = Command;
    *(CommandBuffer + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
    *(CommandBuffer + 2) = (UCHAR)(Block >> 24);
    *(CommandBuffer + 3) = (UCHAR)(Block >> 16);
    *(CommandBuffer + 4) = (UCHAR)(Block >> 8);
    *(CommandBuffer + 5) = (UCHAR)Block;
    *(CommandBuffer + 7) = (UCHAR)(BlockCount >> 8);
    *(CommandBuffer + 8) = (UCHAR)BlockCount;
    UsbDataTransfer->Length = (ULONG)RequestSize;
    Status = UsbMasspSendCommand(Disk);
    if (!KSUCCESS(Status)) {
        goto SendNextIoRequestEnd;
    }

    Status = STATUS_SUCCESS;

SendNextIoRequestEnd:
    return Status;
}

KSTATUS
UsbMasspResetRecovery (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    )

/*++

Routine Description:

    This routine issues a reset recovery to the mass storage bulk-only device.
    Reset recovery consists of a bulk-only mass storage reset, clearing the
    HALT feature in the IN endpoint, and then clearing the HALT feature in the
    OUT endpoint.

Arguments:

    Device - Supplies a pointer to the USB mass storage device that needs to
        be reset.

    PolledIo - Supplies a boolean indicating if polled I/O should be used to
        perform the reset recovery (TRUE) or not (FALSE). The default is FALSE.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    //
    // Proceed according to Section 5.3.4 of the USB Mass Storage
    // Specification. First, reset the mass storage device and then clear the
    // halts on both the IN and OUT endpoints.
    //

    Status = UsbMasspReset(Device, PolledIo);
    if (!KSUCCESS(Status)) {
        goto ResetRecoveryEnd;
    }

    Status = UsbMasspClearHalts(Device, PolledIo);
    if (!KSUCCESS(Status)) {
        goto ResetRecoveryEnd;
    }

ResetRecoveryEnd:

    //
    // If reset recovery fails, notify the system so that action can be taken.
    //

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("USB MASS: Failed reset recovery on device 0x%08x!\n",
                      Device);

        if (PolledIo == FALSE) {
            IoSetDeviceDriverError(UsbGetDeviceToken(Device->UsbCoreHandle),
                                   UsbMassDriver,
                                   Status,
                                   USB_MASS_ERROR_FAILED_RESET_RECOVERY);
        }
    }

    return Status;
}

KSTATUS
UsbMasspReset (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    )

/*++

Routine Description:

    This routine resets the given mass storage device. It assumes that the
    device lock is held or polled I/O mode is requested.

Arguments:

    Device - Supplies a pointer to this mass storage device.

    PolledIo - Supplies a boolean indicating if the reset should be performed
        using polled I/O.

Return Value:

    Status code.

--*/

{

    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    ASSERT((PolledIo != FALSE) || (KeIsQueuedLockHeld(Device->Lock) != FALSE));

    RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
    SetupPacket.RequestType = USB_SETUP_REQUEST_TO_DEVICE |
                              USB_SETUP_REQUEST_CLASS |
                              USB_SETUP_REQUEST_INTERFACE_RECIPIENT;

    SetupPacket.Request = USB_MASS_REQUEST_RESET_DEVICE;
    SetupPacket.Value = 0;
    SetupPacket.Index = Device->InterfaceNumber;
    SetupPacket.Length = 0;

    //
    // If polled I/O is requested, then use a USB mass specific send command.
    //

    if (PolledIo != FALSE) {
        Status = UsbMasspSendPolledIoControlTransfer(Device,
                                                     UsbTransferDirectionOut,
                                                     &SetupPacket);

        if (!KSUCCESS(Status)) {
            goto ResetEnd;
        }

    //
    // Otherwise, submit the transfer with the default command and wait for it
    // to complete.
    //

    } else {
        Status = UsbSendControlTransfer(Device->UsbCoreHandle,
                                        UsbTransferDirectionOut,
                                        &SetupPacket,
                                        NULL,
                                        0,
                                        NULL);

        if (!KSUCCESS(Status)) {
            goto ResetEnd;
        }
    }

ResetEnd:
    return Status;
}

KSTATUS
UsbMasspClearHalts (
    PUSB_MASS_STORAGE_DEVICE Device,
    BOOL PolledIo
    )

/*++

Routine Description:

    This routine clears the HALT feature on the bulk in and out endpoints. This
    routine assumes the device lock is held or polled I/O mode is requested.

Arguments:

    Device - Supplies a pointer to the USB mass storage device.

    PolledIo - Supplies a boolean indicating if the halt bits should be cleared
        using polled I/O.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = UsbMasspClearEndpoint(Device, Device->InEndpoint, PolledIo);
    if (!KSUCCESS(Status)) {
        goto ClearHaltsEnd;
    }

    Status = UsbMasspClearEndpoint(Device, Device->OutEndpoint, PolledIo);
    if (!KSUCCESS(Status)) {
        goto ClearHaltsEnd;
    }

ClearHaltsEnd:
    return Status;
}

KSTATUS
UsbMasspClearEndpoint (
    PUSB_MASS_STORAGE_DEVICE Device,
    UCHAR Endpoint,
    BOOL PolledIo
    )

/*++

Routine Description:

    This routine clears the HALT feature on an endpoint. It assumes that the
    device lock is held or polled I/O mode is requested.

Arguments:

    Device - Supplies a pointer to the device whose IN endpoint needs to be
        cleared.

    Endpoint - Supplies the endpoint that needs to be cleared of the HALT
        feature. It should either be IN or OUT.

    PolledIo - Supplies a boolean indicating if the endpoint should be cleared
        using polled I/O.

Return Value:

    Status code.

--*/

{

    USB_SETUP_PACKET SetupPacket;
    KSTATUS Status;

    ASSERT((PolledIo != FALSE) || (KeIsQueuedLockHeld(Device->Lock) != FALSE) ||
           (Device->LunCount == 0));

    if (PolledIo != FALSE) {
        RtlZeroMemory(&SetupPacket, sizeof(USB_SETUP_PACKET));
        SetupPacket.RequestType = USB_SETUP_REQUEST_ENDPOINT_RECIPIENT |
                                  USB_SETUP_REQUEST_TO_DEVICE;

        SetupPacket.Request = USB_REQUEST_CLEAR_FEATURE;
        SetupPacket.Value = USB_FEATURE_ENDPOINT_HALT;
        SetupPacket.Index = Endpoint;
        SetupPacket.Length = 0;
        Status = UsbMasspSendPolledIoControlTransfer(Device,
                                                     UsbTransferDirectionOut,
                                                     &SetupPacket);

        if (!KSUCCESS(Status)) {
            goto ClearEndpointEnd;
        }

        //
        // The endpoint needs to be reset. The USB core conveniently does this
        // automatically in the clear feature routine. But do it manually here.
        //

        Status = UsbResetEndpoint(Device->UsbCoreHandle, Endpoint);
        if (!KSUCCESS(Status)) {
            goto ClearEndpointEnd;
        }

    //
    // Otherwise, attempt to clear the HALT feature from the endpoint using the
    // built-in clear feature routine.
    //

    } else {
        Status = UsbClearFeature(Device->UsbCoreHandle,
                                 USB_SETUP_REQUEST_ENDPOINT_RECIPIENT,
                                 USB_FEATURE_ENDPOINT_HALT,
                                 Endpoint);

        if (!KSUCCESS(Status)) {
            goto ClearEndpointEnd;
        }
    }

ClearEndpointEnd:
    return Status;
}

KSTATUS
UsbMasspBlockIoInitialize (
    PVOID DiskToken
    )

/*++

Routine Description:

    This routine must be called before using the block read and write routines
    in order to allow the disk to prepare for block I/O. This must be called at
    low level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

Return Value:

    Status code.

--*/

{

    PUSB_MASS_STORAGE_DEVICE Device;
    PUSB_DISK Disk;
    PVOID OriginalState;
    PUSB_MASS_STORAGE_POLLED_IO_STATE PolledIoState;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Disk = (PUSB_DISK)DiskToken;
    Device = Disk->Device;

    //
    // If the device's polled I/O state is already present, then block I/O is
    // ready to go.
    //

    if (Device->PolledIoState != NULL) {
        return STATUS_SUCCESS;
    }

    PolledIoState = UsbMasspCreatePolledIoState(Device);
    if (PolledIoState == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto BlockIoInitializeEnd;
    }

    //
    // Try to put this new polled I/O state into the device structure.
    //

    OriginalState= (PVOID)RtlAtomicCompareExchange(
                                              (PUINTN)&(Device->PolledIoState),
                                              (UINTN)PolledIoState,
                                              (UINTN)NULL);

    if (OriginalState == NULL) {
        PolledIoState = NULL;
    }

    Status = STATUS_SUCCESS;

BlockIoInitializeEnd:
    if (PolledIoState != NULL) {
        UsbMasspDestroyPolledIoState(PolledIoState);
    }

    return Status;
}

KSTATUS
UsbMasspBlockIoRead (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine reads the block contents from the disk into the given I/O
    buffer using polled I/O. It does so without acquiring any locks or
    allocating any resources, as this routine is used for crash dump support
    when the system is in a very fragile state. This routine must be called at
    high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer where the data will be read.

    BlockAddress - Supplies the block index to read (for physical disk, this is
        the LBA).

    BlockCount - Supplies the number of blocks to read.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks read.

Return Value:

    Status code.

--*/

{

    PUSB_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PUSB_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;
    Status = UsbMasspPerformPolledIo(&IrpReadWrite, Disk, FALSE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
UsbMasspBlockIoWrite (
    PVOID DiskToken,
    PIO_BUFFER IoBuffer,
    ULONGLONG BlockAddress,
    UINTN BlockCount,
    PUINTN BlocksCompleted
    )

/*++

Routine Description:

    This routine writes the contents of the given I/O buffer to the disk using
    polled I/O. It does so without acquiring any locks or allocating any
    resources, as this routine is used for crash dump support when the system
    is in a very fragile state. This routine must be called at high level.

Arguments:

    DiskToken - Supplies an opaque token for the disk. The appropriate token is
        retrieved by querying the disk device information.

    IoBuffer - Supplies a pointer to the I/O buffer containing the data to
        write.

    BlockAddress - Supplies the block index to write to (for physical disk,
        this is the LBA).

    BlockCount - Supplies the number of blocks to write.

    BlocksCompleted - Supplies a pointer that receives the total number of
        blocks written.

Return Value:

    Status code.

--*/

{

    PUSB_DISK Disk;
    IRP_READ_WRITE IrpReadWrite;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    Disk = (PUSB_DISK)DiskToken;
    IrpReadWrite.IoBuffer = IoBuffer;
    IrpReadWrite.IoOffset = BlockAddress << Disk->BlockShift;
    IrpReadWrite.IoSizeInBytes = BlockCount << Disk->BlockShift;
    Status = UsbMasspPerformPolledIo(&IrpReadWrite, Disk, TRUE);
    *BlocksCompleted = IrpReadWrite.IoBytesCompleted >> Disk->BlockShift;
    return Status;
}

KSTATUS
UsbMasspPerformPolledIo (
    PIRP_READ_WRITE IrpReadWrite,
    PUSB_DISK Disk,
    BOOL Write
    )

/*++

Routine Description:

    This routine performs polled I/O data transfers to the given USB disk.

Arguments:

    IrpReadWrite - Supplies a pointer to the IRP's read/write context.

    Disk - Supplies a pointer to a USB disk.

    Write - Supplies a boolean indicating if this is a read (FALSE) or write
        (TRUE) operation.

Return Value:

    Status code.

--*/

{

    UINTN BlockCount;
    ULONGLONG BlockOffset;
    ULONG BytesCompleted;
    UINTN BytesRemaining;
    UINTN BytesThisRound;
    UCHAR Command;
    PUCHAR CommandBuffer;
    BOOL CommandIn;
    UCHAR CommandLength;
    KSTATUS CompletionStatus;
    PUSB_MASS_STORAGE_DEVICE Device;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PIO_BUFFER IoBuffer;
    UINTN IoBufferOffset;
    ULONG IrpReadWriteFlags;
    PHYSICAL_ADDRESS PhysicalAddress;
    BOOL ReadWriteIrpPrepared;
    KSTATUS Status;
    PUSB_TRANSFER UsbDataTransfer;
    PVOID VirtualAddress;

    ASSERT(KeGetRunLevel() == RunLevelHigh);
    ASSERT(IrpReadWrite->IoBuffer != NULL);

    IrpReadWrite->IoBytesCompleted = 0;
    Device = Disk->Device;
    ReadWriteIrpPrepared = FALSE;

    //
    // The polled I/O transfers better be initialized.
    //

    if (Device->PolledIoState == NULL) {

        ASSERT(Device->PolledIoState != NULL);

        Status = STATUS_NOT_INITIALIZED;
        goto PerformPolledIoEnd;
    }

    //
    // Perform a one-time reset of the I/O endpoints to prepare for the polled
    // I/O. This is necessary because there may be a CBW in flight and the
    // device won't like it if another CBW is sent before it has a chance to
    // finish with the CSW.
    //

    if (Device->PolledIoState->ResetRequired != FALSE) {
        Status = UsbMasspResetForPolledIo(Device);
        if (!KSUCCESS(Status)) {
            goto PerformPolledIoEnd;
        }

        Device->PolledIoState->ResetRequired = FALSE;
    }

    //
    // Prepare for the I/O. This is not polled I/O in the normal sense, as
    // USB transfers are still handling the work. So do not note it as polled.
    //

    IrpReadWriteFlags = 0;
    if (Write != FALSE) {
        IrpReadWriteFlags |= IRP_READ_WRITE_FLAG_WRITE;
    }

    Status = IoPrepareReadWriteIrp(IrpReadWrite,
                                   1 << Disk->BlockShift,
                                   0,
                                   MAX_ULONG,
                                   IrpReadWriteFlags);

    ASSERT(KSUCCESS(Status));

    ReadWriteIrpPrepared = TRUE;
    IoBuffer = IrpReadWrite->IoBuffer;
    Status = MmMapIoBuffer(IoBuffer, FALSE, FALSE, FALSE);

    ASSERT(KSUCCESS(Status));

    //
    // Find the starting fragment based on the current offset.
    //

    IoBufferOffset = MmGetIoBufferCurrentOffset(IoBuffer);
    FragmentIndex = 0;
    FragmentOffset = 0;
    while (IoBufferOffset != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = &(IoBuffer->Fragment[FragmentIndex]);
        if (IoBufferOffset < Fragment->Size) {
            FragmentOffset = IoBufferOffset;
            break;
        }

        IoBufferOffset -= Fragment->Size;
        FragmentIndex += 1;
    }

    //
    // Set up the transfer command.
    //

    if (Write == FALSE) {
        Command = SCSI_COMMAND_READ_10;
        CommandLength = SCSI_COMMAND_READ_10_SIZE;
        CommandIn = TRUE;
        UsbDataTransfer = Device->PolledIoState->IoTransfers.DataInTransfer;

    } else {
        Command = SCSI_COMMAND_WRITE_10;
        CommandLength = SCSI_COMMAND_WRITE_10_SIZE;
        CommandIn = FALSE;
        UsbDataTransfer = Device->PolledIoState->IoTransfers.DataOutTransfer;
    }

    //
    // Loop reading in or writing out each fragment in the I/O buffer.
    //

    BytesRemaining = IrpReadWrite->IoSizeInBytes;

    ASSERT(IS_ALIGNED(BytesRemaining, 1 << Disk->BlockShift) != FALSE);
    ASSERT(IS_ALIGNED(IrpReadWrite->IoOffset, 1 << Disk->BlockShift) != FALSE);

    BlockOffset = IrpReadWrite->IoOffset >> Disk->BlockShift;
    while (BytesRemaining != 0) {

        ASSERT(FragmentIndex < IoBuffer->FragmentCount);

        Fragment = (PIO_BUFFER_FRAGMENT)&(IoBuffer->Fragment[FragmentIndex]);
        VirtualAddress = Fragment->VirtualAddress + FragmentOffset;
        PhysicalAddress = Fragment->PhysicalAddress + FragmentOffset;
        BytesThisRound = Fragment->Size - FragmentOffset;
        if (BytesRemaining < BytesThisRound) {
            BytesThisRound = BytesRemaining;
        }

        //
        // Transfer the rest of the fragment, but cap it to the max of what the
        // allocated USB transfer can do and on how many bytes have already been
        // transferred and/or need to be transferred.
        //

        if (BytesThisRound > USB_MASS_MAX_DATA_TRANSFER) {
            BytesThisRound = USB_MASS_MAX_DATA_TRANSFER;
        }

        ASSERT(BytesThisRound != 0);
        ASSERT(IS_ALIGNED(BytesThisRound, 1 << Disk->BlockShift) != FALSE);

        BlockCount = BytesThisRound >> Disk->BlockShift;

        ASSERT(BlockCount == (USHORT)BlockCount);
        ASSERT(BytesThisRound == (ULONG)BytesThisRound);

        //
        // Watch for doing I/O off the end of the device.
        //

        if ((BlockOffset >= Disk->BlockCount) ||
            ((BlockOffset + BlockCount) > Disk->BlockCount)) {

            Status = STATUS_OUT_OF_BOUNDS;
            goto PerformPolledIoEnd;
        }

        CommandBuffer = UsbMasspSetupCommand(Disk,
                                             Command,
                                             (ULONG)BytesThisRound,
                                             CommandLength,
                                             CommandIn,
                                             TRUE,
                                             VirtualAddress,
                                             PhysicalAddress);

        *CommandBuffer = Command;
        *(CommandBuffer + 1) = Disk->LunNumber << SCSI_COMMAND_LUN_SHIFT;
        *(CommandBuffer + 2) = (UCHAR)(BlockOffset >> 24);
        *(CommandBuffer + 3) = (UCHAR)(BlockOffset >> 16);
        *(CommandBuffer + 4) = (UCHAR)(BlockOffset >> 8);
        *(CommandBuffer + 5) = (UCHAR)BlockOffset;
        *(CommandBuffer + 7) = (UCHAR)(BlockCount >> 8);
        *(CommandBuffer + 8) = (UCHAR)BlockCount;
        UsbDataTransfer->Length = (ULONG)BytesThisRound;

        //
        // Send the command using polled I/O.
        //

        Status = UsbMasspSendPolledIoCommand(Disk, &BytesCompleted);
        if (!KSUCCESS(Status)) {
            goto PerformPolledIoEnd;
        }

        if ((BytesCompleted >> Disk->BlockShift) != BlockCount) {
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto PerformPolledIoEnd;
        }

        FragmentOffset += BytesCompleted;
        if (FragmentOffset == Fragment->Size) {
            FragmentIndex += 1;
            FragmentOffset = 0;
        }

        BlockOffset += BlockCount;
        BytesRemaining -= BytesCompleted;
        IrpReadWrite->IoBytesCompleted += BytesCompleted;
    }

    Status = STATUS_SUCCESS;

PerformPolledIoEnd:
    if (ReadWriteIrpPrepared != FALSE) {
        CompletionStatus = IoCompleteReadWriteIrp(IrpReadWrite,
                                                  IrpReadWriteFlags);

        if (!KSUCCESS(CompletionStatus) && KSUCCESS(Status)) {
            Status = CompletionStatus;
        }
    }

    IrpReadWrite->NewIoOffset = IrpReadWrite->IoOffset +
                                IrpReadWrite->IoBytesCompleted;

    return Status;
}

KSTATUS
UsbMasspSendPolledIoCommand (
    PUSB_DISK Disk,
    PULONG BytesCompleted
    )

/*++

Routine Description:

    This routine sends the current polled I/O command to the USB mass storage
    device.

Arguments:

    Disk - Supplies a pointer to a USB disk.

    BytesCompleted - Supplies a pointer where the number of completed bytes
        will be returned.

Return Value:

    Status code.

--*/

{

    PUSB_TRANSFER DataTransfer;
    KSTATUS Status;
    PUSB_MASS_STORAGE_TRANSFERS Transfers;

    ASSERT(Disk->Device->PolledIoState != NULL);

    Transfers = &(Disk->Device->PolledIoState->IoTransfers);

    //
    // Submit the command transfer.
    //

    Status = UsbSubmitPolledTransfer(Transfers->CommandTransfer);
    if (!KSUCCESS(Status)) {
        goto PerformPolledIoEnd;
    }

    //
    // Submit the data transfer if there is any data. Ignore failures here as
    // the command status transfer is expected given that command block
    // transfer succeeded.
    //

    DataTransfer = NULL;
    if (Transfers->DataInTransfer->Length != 0) {

        ASSERT(Transfers->DataOutTransfer->Length == 0);

        DataTransfer = Transfers->DataInTransfer;

    } else if (Transfers->DataOutTransfer->Length != 0) {
        DataTransfer = Transfers->DataOutTransfer;
    }

    if (DataTransfer != NULL) {
        UsbSubmitPolledTransfer(DataTransfer);
    }

    //
    // Always submit the command status transfer. Ignore status here too.
    //

    UsbSubmitPolledTransfer(Transfers->StatusTransfer);

PerformPolledIoEnd:

    //
    // Now analyze the status from the transfer to see if it worked.
    //

    Status = UsbMasspEvaluateCommandStatus(Disk, TRUE, TRUE, BytesCompleted);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("USBMASS: Polled I/O failed %d.\n", Status);
    }

    return Status;
}

KSTATUS
UsbMasspResetForPolledIo (
    PUSB_MASS_STORAGE_DEVICE Device
    )

/*++

Routine Description:

    This routine resets the USB Mass storage device to which the given disk is
    attached in preparation for polled I/O operations. This includes halting
    any in-flight transfers and performing reset recovery.

Arguments:

    Device - Supplies a pointer to the USB device to be reset.

Return Value:

    Status code.

--*/

{

    ULONG ControlTransfers;
    ULONG DataInTransfers;
    ULONG DataOutTransfers;
    KSTATUS Status;

    Status = UsbFlushEndpoint(Device->UsbCoreHandle, 0, &ControlTransfers);
    if (!KSUCCESS(Status)) {
        goto ResetForPolledIoEnd;
    }

    Status = UsbFlushEndpoint(Device->UsbCoreHandle,
                              Device->InEndpoint,
                              &DataInTransfers);

    if (!KSUCCESS(Status)) {
        goto ResetForPolledIoEnd;
    }

    Status = UsbFlushEndpoint(Device->UsbCoreHandle,
                              Device->OutEndpoint,
                              &DataOutTransfers);

    if (!KSUCCESS(Status)) {
        goto ResetForPolledIoEnd;
    }

    Status = UsbMasspResetRecovery(Device, TRUE);
    if (!KSUCCESS(Status)) {
        goto ResetForPolledIoEnd;
    }

ResetForPolledIoEnd:
    return Status;
}

KSTATUS
UsbMasspSendPolledIoControlTransfer (
    PUSB_MASS_STORAGE_DEVICE Device,
    USB_TRANSFER_DIRECTION TransferDirection,
    PUSB_SETUP_PACKET SetupPacket
    )

/*++

Routine Description:

    This routine sends a control transfer to the given USB mass storage device
    using polled I/O.

Arguments:

    Device - Supplies a pointer to the USB mass storage device to talk to.

    TransferDirection - Supplies whether or not the transfer is to the device
        or to the host.

    SetupPacket - Supplies a pointer to the setup packet.

Return Value:

    Stats code.

--*/

{

    PIO_BUFFER IoBuffer;
    KSTATUS Status;
    PUSB_TRANSFER Transfer;
    PVOID TransferBuffer;

    //
    // This routine is only meant to be used at high run level.
    //

    ASSERT(KeGetRunLevel() == RunLevelHigh);

    //
    // The polled I/O device state must be present.
    //

    if (Device->PolledIoState == NULL) {

        ASSERT(Device->PolledIoState != NULL);

        return STATUS_NOT_INITIALIZED;
    }

    Transfer = Device->PolledIoState->ControlTransfer;

    ASSERT(Transfer != NULL);
    ASSERT(TransferDirection != UsbTransferDirectionInvalid);

    //
    // Borrow the polled I/O state's command I/O buffer. It should not be in
    // use right now.
    //

    IoBuffer = Device->PolledIoState->IoTransfers.CommandBuffer;

    ASSERT(IoBuffer->FragmentCount == 1);

    TransferBuffer = IoBuffer->Fragment[0].VirtualAddress;
    RtlCopyMemory(TransferBuffer, SetupPacket, sizeof(USB_SETUP_PACKET));

    //
    // Initialize the USB transfer.
    //

    Transfer->Direction = TransferDirection;
    Transfer->Length = sizeof(USB_SETUP_PACKET);
    Transfer->Buffer = TransferBuffer;
    Transfer->BufferPhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;
    Transfer->BufferActualLength = IoBuffer->Fragment[0].Size;

    //
    // Submit the transfer via polled I/O and wait for it to complete.
    //

    Status = UsbSubmitPolledTransfer(Transfer);
    if (!KSUCCESS(Status)) {
        goto SendPolledIoControlTransferEnd;
    }

    ASSERT(KSUCCESS(Transfer->Status));

    //
    // Copy the results into the caller's buffer.
    //

    ASSERT(Transfer->LengthTransferred >= sizeof(USB_SETUP_PACKET));

    Status = STATUS_SUCCESS;

SendPolledIoControlTransferEnd:
    return Status;
}

