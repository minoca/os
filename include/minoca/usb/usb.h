/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usb.h

Abstract:

    This header contains definitions for interacting with USB devices.

Author:

    Evan Green 15-Jan-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifndef USB_API

#define USB_API __DLLIMPORT

#endif

//
// Define the maximum size of a USB hub descriptor.
//

#define USB_HUB_DESCRIPTOR_MAX_SIZE 39

//
// Define the maximum descriptor size.
//

#define USB_MAX_DESCRIPTOR_SIZE 0xFF

//
// Define the values in the Setup Packet's RequestType field.
//

#define USB_SETUP_REQUEST_TO_HOST (1 << 7)
#define USB_SETUP_REQUEST_TO_DEVICE (0 << 7)
#define USB_SETUP_REQUEST_STANDARD (0x0 << 5)
#define USB_SETUP_REQUEST_CLASS (0x1 << 5)
#define USB_SETUP_REQUEST_VENDOR (0x2 << 5)
#define USB_SETUP_REQUEST_DEVICE_RECIPIENT 0x0
#define USB_SETUP_REQUEST_INTERFACE_RECIPIENT 0x1
#define USB_SETUP_REQUEST_ENDPOINT_RECIPIENT 0x2
#define USB_SETUP_REQUEST_OTHER_RECIPIENT 0x3

//
// Define the USB standard requests.
//

#define USB_REQUEST_GET_STATUS 0x0
#define USB_REQUEST_CLEAR_FEATURE 0x1
#define USB_REQUEST_SET_FEATURE 0x3
#define USB_REQUEST_SET_ADDRESS 0x5
#define USB_REQUEST_GET_DESCRIPTOR 0x6
#define USB_REQUEST_SET_DESCRIPTOR 0x7
#define USB_REQUEST_GET_CONFIGURATION 0x8
#define USB_REQUEST_SET_CONFIGURATION 0x9
#define USB_REQUEST_GET_INTERFACE 0xA
#define USB_REQUEST_SET_INTERFACE 0xB
#define USB_REQUEST_SYNCH_FRAME 0xC

//
// Define the USB feature selectors.
//

#define USB_FEATURE_ENDPOINT_HALT 0x0
#define USB_FEATURE_DEVICE_REMOTE_WAKEUP 0x1
#define USB_FEATURE_DEVICE_TEST_MODE 0x2

//
// Define standard Device requests.
//

#define USB_DEVICE_REQUEST_GET_STATUS USB_REQUEST_GET_STATUS
#define USB_DEVICE_REQUEST_CLEAR_FEATURE USB_REQUEST_CLEAR_FEATURE
#define USB_DEVICE_REQUEST_SET_FEATURE USB_REQUEST_SET_FEATURE
#define USB_DEVICE_REQUEST_SET_ADDRESS USB_REQUEST_SET_ADDRESS
#define USB_DEVICE_REQUEST_GET_DESCRIPTOR USB_REQUEST_GET_DESCRIPTOR
#define USB_DEVICE_REQUEST_SET_DESCRIPTOR USB_REQUEST_SET_DESCRIPTOR
#define USB_DEVICE_REQUEST_GET_CONFIGURATION USB_REQUEST_GET_CONFIGURATION
#define USB_DEVICE_REQUEST_SET_CONFIGURATION USB_REQUEST_SET_CONFIGURATION

//
// Define the USB device status bits.
//

#define USB_DEVICE_STATUS_SELF_POWERED 0x1
#define USB_DEVICE_STATUS_REMOTE_WAKEUP 0x2

//
// Define standard Interface requests.
//

#define USB_INTERFACE_REQUEST_GET_STATUS USB_REQUEST_GET_STATUS
#define USB_INTERFACE_REQUEST_CLEAR_FEATURE USB_REQUEST_CLEAR_FEATURE
#define USB_INTERFACE_REQUEST_SET_FEATURE USB_REQUEST_SET_FEATURE
#define USB_INTERFACE_GET_INTERFACE USB_REQUEST_GET_INTERFACE
#define USB_INTERFACE_SET_INTERFACE USB_REQUEST_SET_INTERFACE

//
// Define standard Endpoint requests.
//

#define USB_ENDPOINT_REQUEST_GET_STATUS USB_REQUEST_GET_STATUS
#define USB_ENDPOINT_REQUEST_CLEAR_FEATURE USB_REQUEST_CLEAR_FEATURE
#define USB_ENDPOINT_REQUEST_SET_FEATURE USB_REQUEST_SET_FEATURE
#define USB_ENDPOINT_REQUEST_SYNCH_FRAME USB_REQUEST_SYNCH_FRAME

//
// Define the endpoint address bits in a USB endpoint descriptor.
//

#define USB_ENDPOINT_ADDRESS_DIRECTION_IN 0x80
#define USB_ENDPOINT_ADDRESS_MASK 0x0F

//
// Define the attributes bits in a USB endpoint descriptor.
//

#define USB_ENDPOINT_ATTRIBUTES_TYPE_MASK 0x03
#define USB_ENDPOINT_ATTRIBUTES_TYPE_CONTROL 0x00
#define USB_ENDPOINT_ATTRIBUTES_TYPE_ISOCHRONOUS 0x01
#define USB_ENDPOINT_ATTRIBUTES_TYPE_BULK 0x02
#define USB_ENDPOINT_ATTRIBUTES_TYPE_INTERRUPT 0x03

//
// Define the USB endpoint status bits.
//

#define USB_ENDPOINT_STATUS_HALT 0x1

//
// Define USB Hub characteristics flags.
//

#define USB_HUB_CHARACTERISTIC_POWER_SWITCHING_MASK 0x03
#define USB_HUB_CHARACTERISTIC_POWER_GANGED 0x00
#define USB_HUB_CHARACTERISTIC_POWER_INDIVIDUAL 0x01
#define USB_HUB_CHARACTERISTIC_OVER_CURRENT_MASK 0x0C
#define USB_HUB_CHARACTERISTIC_OVER_CURRENT_GLOBAL 0x00
#define USB_HUB_CHARACTERISTIC_OVER_CURRENT_INDIVIDUAL 0x04
#define USB_HUB_CHARACTERISTIC_OVER_CURRENT_NONE 0x08
#define USB_HUB_CHARACTERISTIC_TT_THINK_MASK 0x30
#define USB_HUB_CHARACTERISTIC_TT_THINK_8_FS_TIMES 0x00
#define USB_HUB_CHARACTERISTIC_TT_THINK_16_FS_TIMES 0x10
#define USB_HUB_CHARACTERISTIC_TT_THINK_24_FS_TIMES 0x20
#define USB_HUB_CHARACTERISTIC_TT_THINK_32_FS_TIMES 0x30
#define USB_HUB_CHARACTERISTIC_INDICATORS_SUPPORTED 0x80

//
// Define USB language IDs.
//

#define USB_LANGUAGE_ENGLISH_US 0x0409

//
// USB Hub definitions
//

//
// Define the size of various hub transfers.
//

#define USB_HUB_MAX_CONTROL_TRANSFER_SIZE \
    (USB_HUB_DESCRIPTOR_MAX_SIZE + sizeof(USB_SETUP_PACKET))

#define USB_HUB_MAX_PORT_COUNT 127
#define USB_HUB_MAX_INTERRUPT_SIZE \
    (ALIGN_RANGE_UP(USB_HUB_MAX_PORT_COUNT + 1, BITS_PER_BYTE) / BITS_PER_BYTE)

//
// Define Hub class feature selectors (that go in the Value of the setup
// packet).
//

#define USB_HUB_FEATURE_C_HUB_LOCAL_POWER   0
#define USB_HUB_FEATURE_C_HUB_OVER_CURRENT  1

#define USB_HUB_FEATURE_PORT_CONNECTION     0
#define USB_HUB_FEATURE_PORT_ENABLE         1
#define USB_HUB_FEATURE_PORT_SUSPEND        2
#define USB_HUB_FEATURE_PORT_OVER_CURRENT   3
#define USB_HUB_FEATURE_PORT_RESET          4
#define USB_HUB_FEATURE_PORT_POWER          8
#define USB_HUB_FEATURE_PORT_LOW_SPEED      9
#define USB_HUB_FEATURE_C_PORT_CONNECTION   16
#define USB_HUB_FEATURE_C_PORT_ENABLE       17
#define USB_HUB_FEATURE_C_PORT_SUSPEND      18
#define USB_HUB_FEATURE_C_PORT_OVER_CURRENT 19
#define USB_HUB_FEATURE_C_PORT_RESET        20
#define USB_HUB_FEATURE_PORT_TEST           21
#define USB_HUB_FEATURE_PORT_INDICATOR      22

//
// Define hub status bits.
//

#define USB_HUB_HUB_STATUS_LOCAL_POWER  (1 << 0)
#define USB_HUB_HUB_STATUS_OVER_CURRENT (1 << 1)

#define USB_HUB_HUB_STATUS_CHANGE_SHIFT 16

//
// Define port status bits.
//

#define USB_HUB_PORT_STATUS_DEVICE_CONNECTED    (1 << 0)
#define USB_HUB_PORT_STATUS_ENABLED             (1 << 1)
#define USB_HUB_PORT_STATUS_SUSPENDED           (1 << 2)
#define USB_HUB_PORT_STATUS_OVER_CURRENT        (1 << 3)
#define USB_HUB_PORT_STATUS_RESET               (1 << 4)
#define USB_HUB_PORT_STATUS_POWERED_ON          (1 << 8)
#define USB_HUB_PORT_STATUS_LOW_SPEED           (1 << 9)
#define USB_HUB_PORT_STATUS_HIGH_SPEED          (1 << 10)
#define USB_HUB_PORT_STATUS_TEST                (1 << 11)
#define USB_HUB_PORT_STATUS_SOFTWARE_INDICATORS (1 << 12)

#define USB_HUB_PORT_STATUS_CHANGE_SHIFT 16

//
// Define indicator values.
//

#define USB_HUB_INDICATOR_AUTOMATIC (0 << 8)
#define USB_HUB_INDICATOR_AMBER     (1 << 8)
#define USB_HUB_INDICATOR_GREEN     (2 << 8)
#define USB_HUB_INDICATOR_OFF       (3 << 8)
#define USB_HUB_INDICATOR_MASK      (0xFF << 8)

//
// Define well-known USB device IDs.
//

#define USB_ROOT_HUB_DEVICE_ID "UsbRootHub"
#define USB_COMPOUND_DEVICE_CLASS_ID "UsbCompoundDevice"
#define USB_HID_CLASS_ID "UsbHid"
#define USB_BOOT_KEYBOARD_CLASS_ID "UsbBootKeyboard"
#define USB_BOOT_MOUSE_CLASS_ID "UsbBootMouse"
#define USB_MASS_STORAGE_CLASS_ID "UsbMassStorage"
#define USB_HUB_CLASS_ID "UsbHub"

//
// Define the required subclass and protocol for this device to be a keyboard
// or mouse that follows the boot protocol.
//

#define USB_HID_BOOT_INTERFACE_SUBCLASS 1
#define USB_HID_BOOT_KEYBOARD_PROTOCOL 1
#define USB_HID_BOOT_MOUSE_PROTOCOL 2

//
// Define USB HID standard requests.
//

#define USB_HID_GET_REPORT   0x01
#define USB_HID_GET_IDLE     0x02
#define USB_HID_GET_PROTOCOL 0x03
#define USB_HID_SET_REPORT   0x09
#define USB_HID_SET_IDLE     0x0A
#define USB_HID_SET_PROTOCOL 0x0B

//
// Define USB HID report value fields.
//

#define USB_HID_RERPOT_VALUE_TYPE_MASK    (0xFF << 8)
#define USB_HID_REPORT_VALUE_TYPE_SHIFT   8
#define USB_HID_REPORT_VALUE_TYPE_INPUT   1
#define USB_HID_REPORT_VALUE_TYPE_OUTPUT  2
#define USB_HID_REPORT_VALUE_TYPE_FEATURE 3
#define USB_HID_REPORT_VALUE_ID_MASK      (0xFF << 0)
#define USB_HID_REPORT_VALUE_ID_SHIFT     0

//
// Define the USB HID protocol request values.
//

#define USB_HID_PROTOCOL_VALUE_BOOT   0
#define USB_HID_PROTOCOL_VALUE_REPORT 1

//
// Define USB Transfer flags.
//

//
// Set this flag to continue trying if a transfer comes up short.
//

#define USB_TRANSFER_FLAG_NO_SHORT_TRANSFERS 0x00000001

//
// Set this flag to prevent an interrupt from firing when the packet completes.
// This is usually only used for internal intermediate transfers.
//

#define USB_TRANSFER_FLAG_NO_INTERRUPT_ON_COMPLETION 0x00000002

//
// Set this flag for non-synchronous transfers from a paging USB mass storage
// device.
//

#define USB_TRANSFER_FLAG_PAGING_DEVICE 0x00000004

//
// Set this flag to force a short, zero-length transfer to be sent if the
// payload is a multiple of the max packet size for the endpoint.
//

#define USB_TRANSFER_FLAG_FORCE_SHORT_TRANSFER 0x00000008

//
// Define the maximum size of a USB string descriptor.
//

#define USB_STRING_DESCRIPTOR_MAX_SIZE 0xFF

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _USB_DEVICE_SPEED {
    UsbDeviceSpeedInvalid,
    UsbDeviceSpeedLow,
    UsbDeviceSpeedFull,
    UsbDeviceSpeedHigh,
    UsbDeviceSpeedSuper,
} USB_DEVICE_SPEED, *PUSB_DEVICE_SPEED;

typedef enum _USB_TRANSFER_TYPE {
    UsbTransferTypeInvalid,
    UsbTransferTypeControl,
    UsbTransferTypeInterrupt,
    UsbTransferTypeBulk,
    UsbTransferTypeIsochronous,
    UsbTransferTypeCount
} USB_TRANSFER_TYPE, *PUSB_TRANSFER_TYPE;

typedef enum _USB_TRANSFER_DIRECTION {
    UsbTransferDirectionInvalid,
    UsbTransferDirectionIn,
    UsbTransferDirectionOut,
    UsbTransferBidirectional,
    UsbTransferDirectionCount
} USB_TRANSFER_DIRECTION, *PUSB_TRANSFER_DIRECTION;

typedef enum _USB_DESCRIPTOR_TYPE {
    UsbDescriptorTypeDevice                  = 0x01,
    UsbDescriptorTypeConfiguration           = 0x02,
    UsbDescriptorTypeString                  = 0x03,
    UsbDescriptorTypeInterface               = 0x04,
    UsbDescriptorTypeEndpoint                = 0x05,
    UsbDescriptorTypeDeviceQualifier         = 0x06,
    UsbDescriptorTypeOtherSpeedConfiguration = 0x07,
    UsbDescriptorTypeHid                     = 0x21,
    UsbDescriptorTypeHidReport               = 0x22,
    UsbDescriptorTypeHidPhysical             = 0x23,
    UsbDescriptorTypeHub                     = 0x29,
} USB_DESCRIPTOR_TYPE, *PUSB_DESCRIPTOR_TYPE;

typedef enum _USB_INTERFACE_CLASS {
    UsbInterfaceClassAudio               = 0x01,
    UsbInterfaceClassCdcControl          = 0x02,
    UsbInterfaceClassHid                 = 0x03,
    UsbInterfaceClassPhysical            = 0x05,
    UsbInterfaceClassImage               = 0x06,
    UsbInterfaceClassPrinter             = 0x07,
    UsbInterfaceClassMassStorage         = 0x08,
    UsbInterfaceClassCdcData             = 0x0A,
    UsbInterfaceClassSmartCard           = 0x0B,
    UsbInterfaceClassContentSecurity     = 0x0D,
    UsbInterfaceClassVideo               = 0x0E,
    UsbInterfaceClassPersonalHealthcare  = 0x0F,
    UsbInterfaceClassAudioVideo          = 0x10,
    UsbInterfaceClassDiagnosticDevice    = 0xDC,
    UsbInterfaceClassWireless            = 0xE0,
    UsbInterfaceClassMiscellaneous       = 0xEF,
    UsbInterfaceClassApplicationSpecific = 0xFE,
    UsbInterfaceClassVendor              = 0xFF,
} USB_INTERFACE_CLASS, *PUSB_INTERFACE_CLASS;

typedef enum _USB_DEVICE_CLASS {
    UsbDeviceClassUseInterface           = 0x00,
    UsbDeviceClassCdcControl             = 0x02,
    UsbDeviceClassHid                    = 0x03,
    UsbDeviceClassHub                    = 0x09,
    UsbDeviceClassDiagnosticDevice       = 0xDC,
    UsbDeviceClassMiscellaneous          = 0xEF,
    UsbDeviceClassVendor                 = 0xFF,
} USB_DEVICE_CLASS, *PUSB_DEVICE_CLASS;

typedef enum _USB_ERROR {
    UsbErrorNone,
    UsbErrorTransferNotStarted,
    UsbErrorTransferCancelled,
    UsbErrorTransferAllocatedIncorrectly,
    UsbErrorTransferSubmittedWhileStillActive,
    UsbErrorTransferIncorrectlyFilledOut,
    UsbErrorTransferFailedToSubmit,
    UsbErrorTransferStalled,
    UsbErrorTransferDataBuffer,
    UsbErrorTransferBabbleDetected,
    UsbErrorTransferNakReceived,
    UsbErrorTransferCrcOrTimeoutError,
    UsbErrorTransferBitstuff,
    UsbErrorTransferMissedMicroFrame,
    UsbErrorTransferBufferNotAligned,
    UsbErrorTransferDeviceNotConnected,
    UsbErrorTransferDeviceIo,
    UsbErrorShortPacket,
    UsbErrorCount
} USB_ERROR, *PUSB_ERROR;

typedef struct _USB_TRANSFER USB_TRANSFER, *PUSB_TRANSFER;
typedef struct _USB_DEVICE USB_DEVICE, *PUSB_DEVICE;

/*++

Structure Description:

    This structure defines the format of the USB Device Descriptor, as defined
    by the USB specification.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is a device
        descriptor.

    UsbSpecification - Stores a binary coded decimal number indicating the
        revision of the USB specification this device conforms to.

    Class - Stores the class code that the device conforms to. Most class
        specifications choose to identify at the interface level as opposed to
        here at the device level.

    Subclass - Stores the subclass code that the device conforms to.

    Protocol - Stores the protocol number of the class/subclass that the device
        conforms to.

    MaxPacketSize - Stores the maximum supported size of packets on this
        default endpoint. Valid values are 8, 16, 32, and 64.

    VendorId - Stores the vendor identification number (VID) of the device.

    ProductId - Stores the product identification number (PID) of the device.

    DeviceRevision - Stores a binary coded decimal hardware revision number.

    ManufacturerStringIndex - Stores the index of the Manufacturer String
        Descriptor.

    ProductStringIndex - Stores the index of the Product Name String Descriptor.

    SerialNumberIndex - Stores the index of the Serial Number String Descriptor.

    ConfigurationCount - Stores the number of configurations this device
        supports.

--*/

typedef struct _USB_DEVICE_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    USHORT UsbSpecification;
    UCHAR Class;
    UCHAR Subclass;
    UCHAR Protocol;
    UCHAR MaxPacketSize;
    USHORT VendorId;
    USHORT ProductId;
    USHORT DeviceRevision;
    UCHAR ManufacturerStringIndex;
    UCHAR ProductStringIndex;
    UCHAR SerialNumberStringIndex;
    UCHAR ConfigurationCount;
} PACKED USB_DEVICE_DESCRIPTOR, *PUSB_DEVICE_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of the USB Configuration Descriptor, as
    defined by the USB specification.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is a configuration
        descriptor.

    TotalLength - Stores the total length of all the data returned (which
        includes the interfaces and endpoints).

    InterfaceCount - Stores the number of interfaces in this configuration.

    ConfigurationValue - Stores the index of this configuration.

    StringIndex - Stores the index of the string descriptor describing this
        configuration.

    Attributes - Stores various attributes about this configuration, mostly
        centered around power.

    MaxPower - Stores the maximum power consumption of this configuration, in
        2mA units.

--*/

typedef struct _USB_CONFIGURATION_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    USHORT TotalLength;
    UCHAR InterfaceCount;
    UCHAR ConfigurationValue;
    UCHAR StringIndex;
    UCHAR Attributes;
    UCHAR MaxPower;
} PACKED USB_CONFIGURATION_DESCRIPTOR, *PUSB_CONFIGURATION_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of the USB Interface Descriptor, as
    defined by the USB specification.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is an interface
        descriptor.

    InterfaceNumber - Stores the index of this interface.

    AlternateNumber - Stores the alternate index of this interface.

    EndpointCount - Stores the number of endpoints in this interface, not
        counting endpoint zero.

    Class - Stores the class code of the interface (assigned by the USB
        organization).

    Subclass - Stores the subclass code of the interface (assigned by the USB
        organization).

    Protocol - Stores the protocol code of the interface (assigned by the USB
        organization).

    StringIndex - Stores the index of the string descriptor describing the
        interface.

--*/

typedef struct _USB_INTERFACE_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    UCHAR InterfaceNumber;
    UCHAR AlternateNumber;
    UCHAR EndpointCount;
    UCHAR Class;
    UCHAR Subclass;
    UCHAR Protocol;
    UCHAR StringIndex;
} PACKED USB_INTERFACE_DESCRIPTOR, *PUSB_INTERFACE_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of the USB Endpoint Descriptor, as
    defined by the USB specification.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is an endpoint
        descriptor.

    EndpointAddress - Stores the address and direction of this endpoint.

    Attributes - Stores a bitfield of attributes of the endpoint.

    MaxPacketSize - Stores the maximum packet size this endpoint is capable of
        sending or receiving.

    Interval - Stores the interval for polling data transfer. This value is in
        frame counts, and is ignored for Control endpoints. Isochronous
        endpoints must set this to 1, interrupt endpoints may range from 1
        to 255, and Bulk OUT endpoints range from 0 to 255 to speceified the
        maximum NAK rate.

--*/

typedef struct _USB_ENDPOINT_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    UCHAR EndpointAddress;
    UCHAR Attributes;
    USHORT MaxPacketSize;
    UCHAR Interval;
} PACKED USB_ENDPOINT_DESCRIPTOR, *PUSB_ENDPOINT_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of the USB String Descriptor, as defined
    by the USB specification. The string itself immediately follows this
    descriptor structure.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is a string
        descriptor.

--*/

typedef struct _USB_STRING_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
} PACKED USB_STRING_DESCRIPTOR, *PUSB_STRING_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of the Setup Packet, as defined
    by the USB specification.

Members:

    RequestType - Stores a bitfield indicating the transfer direction, transfer
        type, and recipient.

    Request - Stores the request value.

    Value - Stores the value associated with the request.

    Index - Stores the index associated with the request.

    Length - Stores the number of bytes to transfer if there is a data phase.

--*/

/*++

Structure Description:

    This structure defines the format of the USB Hub Descriptor, as defined by
    the USB specification.

Members:

    Length - Stores the length of the structure.

    DescriptorType - Stores a constant indicating that this is a hub descriptor.

    PortCount - Stores the number of downstream ports in this hub.

    HubCharacteristics - Stores a bitfield of hub characteristics. See
        USB_HUB_CHARACTERISTIC_* definitions.

    PowerDelayIn2ms - Stores the time, in 2ms intervals, from the time the
        power-on sequence begins on a port until the power is good on that
        port. Software uses this value to determine how long to wait before
        accessing a powered-on port.

    HubCurrent - Stores the maximum current requirements of the hub controller
        electronics in mA.

    DeviceRemovable - Stores a variable-length byte array indicating if a port
        has a removable device attached. Within a byte, if no port exists for
        a given location, the field representing the port characteristics
        returns 0. Each bit is set if the corresponding port is non-removable,
        and is clear if the port has a removable device.

--*/

typedef struct _USB_HUB_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    UCHAR PortCount;
    USHORT HubCharacteristics;
    UCHAR PowerUpDelayIn2ms;
    UCHAR HubCurrent;
    UCHAR DeviceRemovable[ANYSIZE_ARRAY];
} PACKED USB_HUB_DESCRIPTOR, *PUSB_HUB_DESCRIPTOR;

/*++

Structure Description:

    This structure defines the format of a report description within a HID
    descriptor.

Members:

    Type - Stores the class specific descriptor type.

    Length - Stores the descriptor length.

--*/

typedef struct _USB_HID_DESCRIPTOR_REPORT {
    UCHAR Type;
    USHORT Length;
} PACKED USB_HID_DESCRIPTOR_REPORT, *PUSB_HID_DESCRIPTOR_REPORT;

/*++

Structure Description:

    This structure defines the format of the USB Human Interface Device
    descriptor. Report descriptors underneath this follow.

Members:

    Length - Stores the length of the structure, including all subordinate
        descriptors.

    DescriptorType - Stores a constant indicating that this is a HID descriptor.

    HidVersion - Stores the BCD HID version.

    CountryCode - Stores an optional country code.

    DescriptorCount - Stores the number of report descriptors that follow.

    Descriptors - Stores the size and types of the descriptors. This will
        always be at least one for the report descriptor.

--*/

typedef struct _USB_HID_DESCRIPTOR {
    UCHAR Length;
    UCHAR DescriptorType;
    USHORT HidVersion;
    UCHAR CountryCode;
    UCHAR DescriptorCount;
    USB_HID_DESCRIPTOR_REPORT Descriptors[ANYSIZE_ARRAY];
} PACKED USB_HID_DESCRIPTOR, *PUSB_HID_DESCRIPTOR;

/*++

Structure Description:

    This structure defines a USB setup packet.

Members:

    RequestType - Stores the properties of the request.

    Request - Stores the particular type of request in the packet.

    Value - Stores request-specific parameters for the device.

    Index - Stores request-specific parameters for the device.

    Length - Stores the length of the data to be transferred.

--*/

typedef struct _USB_SETUP_PACKET {
    UCHAR RequestType;
    UCHAR Request;
    USHORT Value;
    USHORT Index;
    USHORT Length;
} PACKED USB_SETUP_PACKET, *PUSB_SETUP_PACKET;

/*++

Structure Description:

    This structure defines a USB device description.

Members:

    ListEntry - Stores pointers to the next and previous device descriptions
        in the parent's child list.

    Descriptor - Stores the device descriptor.

    ChildListHead - Stores the head of the list of children of this device.

--*/

typedef struct _USB_DEVICE_DESCRIPTION {
    LIST_ENTRY ListEntry;
    USB_DEVICE_DESCRIPTOR Descriptor;
    LIST_ENTRY ChildListHead;
} USB_DEVICE_DESCRIPTION, *PUSB_DEVICE_DESCRIPTION;

/*++

Structure Description:

    This structure defines a USB configuration description.

Members:

    Descriptor - Stores the configuration descriptor.

    Index - Stores the index number of the configuration.

    InterfaceListHead - Stores the head of the list of interfaces in this
        configuration.

--*/

typedef struct _USB_CONFIGURATION_DESCRIPTION {
    USB_CONFIGURATION_DESCRIPTOR Descriptor;
    UCHAR Index;
    LIST_ENTRY InterfaceListHead;
} USB_CONFIGURATION_DESCRIPTION, *PUSB_CONFIGURATION_DESCRIPTION;

/*++

Structure Description:

    This structure defines a USB interface description.

Members:

    ListEntry - Stores pointers to the next and previous interfaces in the
        parent configuration.

    Descriptor - Stores the interface descriptor.

    EndpointListHead - Stores the head of the list of endpoints in this
        interface.

    UnknownListHead - Stores the head of the list of other descriptors present
        in this interface.
--*/

typedef struct _USB_INTERFACE_DESCRIPTION {
    LIST_ENTRY ListEntry;
    USB_INTERFACE_DESCRIPTOR Descriptor;
    LIST_ENTRY EndpointListHead;
    LIST_ENTRY UnknownListHead;
} USB_INTERFACE_DESCRIPTION, *PUSB_INTERFACE_DESCRIPTION;

/*++

Structure Description:

    This structure defines a USB endpoint description.

Members:

    ListEntry - Stores pointers to the next and previous endpoints in the
        parent interface.

    Descriptor - Stores the endpoint descriptor.

--*/

typedef struct _USB_ENDPOINT_DESCRIPTION {
    LIST_ENTRY ListEntry;
    USB_ENDPOINT_DESCRIPTOR Descriptor;
} USB_ENDPOINT_DESCRIPTION, *PUSB_ENDPOINT_DESCRIPTION;

/*++

Structure Description:

    This structure defines an alternate descriptor within the USB interface.

Members:

    ListEntry - Stores pointers to the next and previous descriptors in the
        parent interface.

    Descriptor - Stores a pointer to the descriptor. The length is in the
        descriptor.

--*/

typedef struct _USB_UNKNOWN_DESCRIPTION {
    LIST_ENTRY ListEntry;
    PUCHAR Descriptor;
} USB_UNKNOWN_DESCRIPTION, *PUSB_UNKNOWN_DESCRIPTION;

typedef
VOID
(*PUSB_TRANSFER_CALLBACK) (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine is called when an asynchronous I/O request completes with
    success, failure, or is cancelled.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about a USB transfer request.

Members:

    Direction - Stores the direction of the USB transfer. This must be
        consistent with the endpoint being sent to.

    Status - Stores the completion status of the request.

    Length - Stores the length of the request, in bytes.

    LengthTransferred - Stores the number of bytes that have actually been
        transferred.

    CallbackRoutine - Stores a pointer to a routine that will be called back
        when the transfer completes.

    UserData - Stores an area where the user can store a pointer's worth of
        data, usually used by the callback routine to identify a request.

    Buffer - Stores a pointer to the data buffer.

    BufferPhysicalAddress - Stores the physical address of the data buffer.

    BufferActualLength - Stores the actual length of the buffer, in bytes. The
        buffer must be at least as large as the length, and must be aligned to
        a flushable boundary.

    Flags - Stores a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

    Error - Stores a more detailed and USB specific error code.

--*/

struct _USB_TRANSFER {
    USB_TRANSFER_DIRECTION Direction;
    KSTATUS Status;
    ULONG Length;
    ULONG LengthTransferred;
    PUSB_TRANSFER_CALLBACK CallbackRoutine;
    PVOID UserData;
    PVOID Buffer;
    PHYSICAL_ADDRESS BufferPhysicalAddress;
    ULONG BufferActualLength;
    ULONG Flags;
    USB_ERROR Error;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

USB_API
KSTATUS
UsbDriverAttach (
    PDEVICE Device,
    PDRIVER Driver,
    PHANDLE UsbCoreHandle
    );

/*++

Routine Description:

    This routine attaches a USB driver to a USB device, and returns a USB
    core handle to the device, used for all USB communications. This routine
    must be called at low level.

Arguments:

    Device - Supplies a pointer to the OS device object representation of the
        USB device.

    Driver - Supplies a pointer to the driver that will take ownership of the
        device.

    UsbCoreHandle - Supplies a pointer where the USB Core device handle will
        be returned.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbEnumerateDeviceForInterface (
    HANDLE UsbCoreHandle,
    PUSB_INTERFACE_DESCRIPTION InterfaceDescription,
    PDEVICE *ChildDevice
    );

/*++

Routine Description:

    This routine enumerates a child OS device on the requested device and
    interface combination. With this interface multiple drivers can
    independently operate interfaces of a shared USB device.

Arguments:

    UsbCoreHandle - Supplies the core handle to the device containing the
        interface to share.

    InterfaceDescription - Supplies a pointer to the interface to enumerate a
        device for.

    ChildDevice - Supplies a pointer to an OS device that will come up to
        claim the given interface. This device should be returned in Query
        Children calls sent to the parent device so the device can properly
        enumerate.

Return Value:

    Status code.

--*/

USB_API
PUSB_INTERFACE_DESCRIPTION
UsbGetDesignatedInterface (
    PDEVICE Device,
    HANDLE UsbCoreHandle
    );

/*++

Routine Description:

    This routine returns the interface for which the given pseudo-device was
    enumerated. This routine is used by general class drivers (like Hub or
    Mass Storage) that can interact with an interface without necessarily
    taking responsibility for the entire device.

Arguments:

    Device - Supplies a pointer to the OS device object representation of the
        USB device.

    UsbCoreHandle - Supplies the core handle to the device.

Return Value:

    Returns a pointer to the interface this pseudo-device is supposed to take
    ownership of. If the device only has one interface, then that interface is
    returned.

    NULL if the OS device was not enumerated for any one particular interface.

--*/

USB_API
KSTATUS
UsbGetDeviceSpeed (
    PUSB_DEVICE Device,
    PUSB_DEVICE_SPEED Speed
    );

/*++

Routine Description:

    This routine returns the connected speed of the given USB device.

Arguments:

    Device - Supplies a pointer to the device.

    Speed - Supplies a pointer where the device speed will be returned.

Return Value:

    Status code.

--*/

USB_API
VOID
UsbDetachDevice (
    HANDLE UsbCoreHandle
    );

/*++

Routine Description:

    This routine detaches a USB device from the USB core by marking it as
    disconnected, and cancelling all active transfers belonging to the device.
    It does not close the device.

Arguments:

    UsbCoreHandle - Supplies the core handle to the device that is to be
        removed.

Return Value:

    None.

--*/

USB_API
KSTATUS
UsbReadDeviceString (
    PUSB_DEVICE Device,
    UCHAR StringNumber,
    USHORT Language,
    PUSB_STRING_DESCRIPTOR Buffer
    );

/*++

Routine Description:

    This routine reads a string descriptor from a USB device.

Arguments:

    Device - Supplies a pointer to the device to read from.

    StringNumber - Supplies the string descriptor index of the string to read.

    Language - Supplies the language code.

    Buffer - Supplies a pointer where the string descriptor and data will be
        returned. This buffer must be the size of the maximum string descriptor,
        which is 256 bytes.

Return Value:

    Status code.

--*/

USB_API
HANDLE
UsbDeviceOpen (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine attempts to open a USB device for I/O.

Arguments:

    Device - Supplies a pointer to the device to open.

Return Value:

    Returns a handle to the device upon success.

    INVALID_HANDLE if the device could not be opened.

--*/

USB_API
VOID
UsbDeviceClose (
    HANDLE UsbDeviceHandle
    );

/*++

Routine Description:

    This routine closes an open USB handle.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    None.

--*/

USB_API
PUSB_TRANSFER
UsbAllocateTransfer (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber,
    ULONG MaxTransferSize,
    ULONG Flags
    );

/*++

Routine Description:

    This routine allocates a new USB transfer structure. This routine must be
    used to allocate transfers.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    EndpointNumber - Supplies the endpoint number that the transfer will go to.

    MaxTransferSize - Supplies the maximum length, in bytes, of the transfer.
        Attempts to submit a transfer with lengths longer than this initialized
        length will fail. Longer transfer sizes do require more resources as
        they are split into subpackets, so try to be reasonable.

    Flags - Supplies a bitfield of flags regarding the transaction. See
        USB_TRANSFER_FLAG_* definitions.

Return Value:

    Returns a pointer to the new USB transfer on success.

    NULL when there are insufficient resources to complete the request.

--*/

USB_API
VOID
UsbDestroyTransfer (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine destroys an allocated transfer. This transfer must not be
    actively transferring.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    None.

--*/

USB_API
KSTATUS
UsbSubmitTransfer (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine submits a USB transfer. The routine returns immediately,
    indicating only whether the transfer was submitted successfully. When the
    transfer actually completes, the callback routine will be called.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    STATUS_SUCCESS if the transfer was submitted to the USB host controller's
    queue.

    STATUS_INVALID_PARAMETER if one or more of the transfer fields is not
        properly filled out.

    Failing status codes if the request could not be submitted.

--*/

USB_API
KSTATUS
UsbSubmitSynchronousTransfer (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine submits a USB transfer, and does not return until the transfer
    is completed successfully or with an error. This routine must be called at
    low level.

Arguments:

    Transfer - Supplies a pointer to the transfer to destroy.

Return Value:

    STATUS_SUCCESS if the transfer was submitted to the USB host controller's
    queue.

    STATUS_INVALID_PARAMETER if one or more of the transfer fields is not
        properly filled out.

    Failing status codes if the request could not be submitted.

--*/

USB_API
KSTATUS
UsbSubmitPolledTransfer (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine submits a USB transfer, and does not return until the transfer
    is completed successfully or with an error. This routine is meant to be
    called in critical code paths at high level.

Arguments:

    Transfer - Supplies a pointer to the transfer to submit.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbCancelTransfer (
    PUSB_TRANSFER Transfer,
    BOOL Wait
    );

/*++

Routine Description:

    This routine cancels a USB transfer, waiting for the transfer to enter the
    inactive state before returning. Must be called at low level.

Arguments:

    Transfer - Supplies a pointer to the transfer to cancel.

    Wait - Supplies a boolean indicating that the caller wants to wait for the
        transfer the reach the inactive state. Specify TRUE if unsure.

Return Value:

    Returns STATUS_SUCCESS if the transfer was successfully cancelled.

    Returns STATUS_TOO_LATE if the transfer was not cancelled, but moved to the
    inactive state.

--*/

USB_API
KSTATUS
UsbInitializePagingDeviceTransfers (
    VOID
    );

/*++

Routine Description:

    This routine initializes the USB core to handle special paging device
    transfers that are serviced on their own work queue.

Arguments:

    None.

Return Value:

    Status code.

--*/

USB_API
ULONG
UsbTransferAddReference (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine adds a reference to a USB transfer.

Arguments:

    Transfer - Supplies a pointer to the transfer that is to be referenced.

Return Value:

    Returns the old reference count.

--*/

USB_API
ULONG
UsbTransferReleaseReference (
    PUSB_TRANSFER Transfer
    );

/*++

Routine Description:

    This routine releases a reference on a USB transfer.

Arguments:

    Transfer - Supplies a pointer to the transfer that is to be dereferenced.

Return Value:

    Returns the old reference count.

--*/

USB_API
KSTATUS
UsbGetStatus (
    HANDLE UsbDeviceHandle,
    UCHAR RequestRecipient,
    USHORT Index,
    PUSHORT Data
    );

/*++

Routine Description:

    This routine gets the status from the given device, interface, or endpoint,
    as determined based on the request type and index. This routine must be
    called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestRecipient - Supplies the recipient of this get status request.

    Index - Supplies the index of this get status request. This can be
        zero for devices, an interface number, or an endpoint number.

    Data - Supplies a pointer that receives the status from the request.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbSetFeature (
    HANDLE UsbDeviceHandle,
    UCHAR RequestRecipient,
    USHORT Feature,
    USHORT Index
    );

/*++

Routine Description:

    This routine sets the given feature for a device, interface or endpoint,
    as specified by the request type and index. This routine must be called at
    low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestRecipient - Supplies the recipient of this clear feature request.

    Feature - Supplies the value of this clear feature request.

    Index - Supplies the index of this clear feature request. This can be
        zero for devices, an interface number, or an endpoint number.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbClearFeature (
    HANDLE UsbDeviceHandle,
    UCHAR RequestType,
    USHORT FeatureSelector,
    USHORT Index
    );

/*++

Routine Description:

    This routine sets the configuration to the given configuration value. This
    routine must be called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    RequestType - Supplies the type of this clear feature request.

    FeatureSelector - Supplies the value of this clear feature request.

    Index - Supplies the index of this clear feature request. This can be
        zero for devices, an interface number, or an endpoint number.

Return Value:

    Status code.

--*/

USB_API
ULONG
UsbGetConfigurationCount (
    HANDLE UsbDeviceHandle
    );

/*++

Routine Description:

    This routine gets the number of possible configurations in a given device.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns the number of configurations in the device.

--*/

USB_API
KSTATUS
UsbGetConfiguration (
    HANDLE UsbDeviceHandle,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex,
    PUSB_CONFIGURATION_DESCRIPTION *Configuration
    );

/*++

Routine Description:

    This routine gets a configuration out of the given device. This routine will
    send a blocking request to the device. This routine must be called at low
    level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    ConfigurationNumber - Supplies the index or configuration value of the
        configuration to get.

    NumberIsIndex - Supplies a boolean indicating whether the configuration
        number is an index (TRUE) or a specific configuration value (FALSE).

    Configuration - Supplies a pointer where a pointer to the desired
        configuration will be returned.

Return Value:

    Status code.

--*/

USB_API
PUSB_CONFIGURATION_DESCRIPTION
UsbGetActiveConfiguration (
    HANDLE UsbDeviceHandle
    );

/*++

Routine Description:

    This routine gets the currently active configuration set in the device.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns a pointer to the current configuration.

    NULL if the device is not currently configured.

--*/

USB_API
KSTATUS
UsbSetConfiguration (
    HANDLE UsbDeviceHandle,
    UCHAR ConfigurationNumber,
    BOOL NumberIsIndex
    );

/*++

Routine Description:

    This routine sets the configuration to the given configuration value. This
    routine must be called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    ConfigurationNumber - Supplies the configuration index or value to set.

    NumberIsIndex - Supplies a boolean indicating whether the configuration
        number is an index (TRUE) or a specific configuration value (FALSE).

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbClaimInterface (
    HANDLE UsbDeviceHandle,
    UCHAR InterfaceNumber
    );

/*++

Routine Description:

    This routine claims an interface, preparing it for I/O use. An interface
    can be claimed more than once. This routine must be called at low level.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    InterfaceNumber - Supplies the number of the interface to claim.

Return Value:

    Status code.

--*/

USB_API
VOID
UsbReleaseInterface (
    HANDLE UsbDeviceHandle,
    UCHAR InterfaceNumber
    );

/*++

Routine Description:

    This routine releases an interface that was previously claimed for I/O.
    After this call, the caller that had claimed the interface should not use
    it again without reclaiming it.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    InterfaceNumber - Supplies the number of the interface to release.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbSendControlTransfer (
    HANDLE UsbDeviceHandle,
    USB_TRANSFER_DIRECTION TransferDirection,
    PUSB_SETUP_PACKET SetupPacket,
    PVOID Buffer,
    ULONG BufferLength,
    PULONG LengthTransferred
    );

/*++

Routine Description:

    This routine sends a syncrhonous control transfer to or from the given USB
    device.

Arguments:

    UsbDeviceHandle - Supplies a pointer to the device to talk to.

    TransferDirection - Supplies whether or not the transfer is to the device
        or to the host.

    SetupPacket - Supplies a pointer to the setup packet.

    Buffer - Supplies a pointer to the buffer to be sent or received. This does
        not include the setup packet, this is the optional data portion only.

    BufferLength - Supplies the length of the buffer, not including the setup
        packet.

    LengthTransferred - Supplies a pointer where the number of bytes that were
        actually transfered (not including the setup packet) will be returned.

Return Value:

    Status code.

--*/

USB_API
PVOID
UsbGetDeviceToken (
    PUSB_DEVICE Device
    );

/*++

Routine Description:

    This routine returns the system device token associated with the given USB
    device.

Arguments:

    Device - Supplies a pointer to a USB device.

Return Value:

    Returns a system device token.

--*/

USB_API
BOOL
UsbIsPolledIoSupported (
    HANDLE UsbDeviceHandle
    );

/*++

Routine Description:

    This routine returns a boolean indicating whether or not the given USB
    device's controller supports polled I/O mode. Polled I/O should only be
    used in dire circumstances. That is, during system failure when a crash
    dump file needs to be written over USB Mass Storage at high run level with
    interrupts disabled.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

Return Value:

    Returns a boolean indicating if polled I/O is supported (TRUE) or not
    (FALSE).

--*/

USB_API
KSTATUS
UsbResetEndpoint (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber
    );

/*++

Routine Description:

    This routine resets the given endpoint for the given USB device. This
    includes resetting the data toggle to DATA 0.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    EndpointNumber - Supplies the number of the endpoint to be reset.

Return Value:

    Status code.

--*/

USB_API
KSTATUS
UsbFlushEndpoint (
    HANDLE UsbDeviceHandle,
    UCHAR EndpointNumber,
    PULONG TransferCount
    );

/*++

Routine Description:

    This routine flushes the given endpoint for the given USB device. This
    includes busily waiting for all active transfers to complete. This is only
    meant to be used at high run level when preparing to write a crash dump
    file using USB Mass Storage.

Arguments:

    UsbDeviceHandle - Supplies the handle returned when the device was opened.

    EndpointNumber - Supplies the number of the endpoint to be reset.

    TransferCount - Supplies a pointer that receives the total number of
        transfers that were flushed.

Return Value:

    Status code.

--*/

