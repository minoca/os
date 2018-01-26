/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devpath.h

Abstract:

    This header contains definitions for the UEFI Device Path Protocol.

Author:

    Evan Green 7-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// The EISA macros convert into and out of the compressed EISA format. The
// lower 16 bits contain a three character compressed ASCII EISA ID, 5 bits per
// letter. The upper 16 bits contain the binary number.
//

#define EISA_ID(_Name, _Number) ((UINT32)((_Name) | (_Number) << 16))
#define EISA_PNP_ID(_PnpId) (EISA_ID(PNP_EISA_ID_CONST, (_PnpId)))
#define EFI_PNP_ID(_PnpId) (EISA_ID(PNP_EISA_ID_CONST, (_PnpId)))
#define EISA_ID_TO_NUM(_Id) ((_Id) >> 16)

#define ACPI_DISPLAY_ADR(_DeviceIdScheme,                       \
                         _HeadId,                               \
                         _NonVgaOutput,                         \
                         _BiosCanDetect,                        \
                         _VendorInfo,                           \
                         _Type,                                 \
                         _Port,                                 \
                         _Index)                                \
                                                                \
          ((UINT32)( (((_DeviceIdScheme) & 0x1) << 31) |        \
                      (((_HeadId) & 0x7) << 18) |               \
                      (((_NonVgaOutput) & 0x1) << 17) |         \
                      (((_BiosCanDetect) & 0x1) << 16) |        \
                      (((_VendorInfo) & 0xF) << 12) |           \
                      (((_Type) & 0xF) << 8) |                  \
                      (((_Port) & 0xF) << 4) |                  \
                       ((_Index) & 0xF) ))

//
// ---------------------------------------------------------------- Definitions
//

//
// Device Path protocol
//

#define EFI_DEVICE_PATH_PROTOCOL_GUID                                          \
    {                                                                          \
        0x9576E91, 0x6D3F, 0x11D2,                                             \
        {0x8E, 0x39, 0x0, 0xA0, 0xC9, 0x69, 0x72, 0x3B}                        \
    }

#define EFI_PC_ANSI_GUID                                                       \
    {                                                                          \
        0xE0C14753, 0xF9BE, 0x11D2,                                            \
        {0x9A, 0x0C, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}                       \
    }

#define EFI_VT_100_GUID                                                        \
    {                                                                          \
        0xDFA66065, 0xB419, 0x11D3,                                            \
        {0x9A, 0x2D, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}                       \
    }

#define EFI_VT_100_PLUS_GUID                                                   \
    {                                                                          \
        0x7BAEC70B, 0x57E0, 0x4C76,                                            \
        {0x8E, 0x87, 0x2F, 0x9E, 0x28, 0x08, 0x83, 0x43}                       \
    }

#define EFI_VT_UTF8_GUID                                                       \
    {                                                                          \
        0xAD15A0D6, 0x8BEC, 0x4ACF,                                            \
        {0xA0, 0x73, 0xD0, 0x1D, 0xE7, 0x7E, 0x2D, 0x88}                       \
    }

#define DEVICE_PATH_MESSAGING_UART_FLOW_CONTROL                                \
    {                                                                          \
        0x37499A9D, 0x542F, 0x4C89,                                            \
        {0xA0, 0x26, 0x35, 0xDA, 0x14, 0x20, 0x94, 0xE4}                       \
    }

#define EFI_SAS_DEVICE_PATH_GUID                                               \
    {                                                                          \
        0xD487DDB4, 0x008B, 0x11D9,                                            \
        {0xAF, 0xDC, 0x00, 0x10, 0x83, 0xFF, 0xCA, 0x4D}                       \
    }

//
// Device Path guid definition for backward compatibility with EFI 1.1.
//

#define DEVICE_PATH_PROTOCOL  EFI_DEVICE_PATH_PROTOCOL_GUID

//
// Hardware Device Paths.
//

#define HARDWARE_DEVICE_PATH      0x01

//
// PCI Device Path SubType.
//

#define HW_PCI_DP                 0x01

//
// PCCARD Device Path SubType.
//

#define HW_PCCARD_DP              0x02

//
// Memory Mapped Device Path SubType.
//

#define HW_MEMMAP_DP              0x03

//
// Hardware Vendor Device Path SubType.
//

#define HW_VENDOR_DP              0x04

//
// Controller Device Path SubType.
//

#define HW_CONTROLLER_DP          0x05

//
// ACPI Device Paths.
//

#define ACPI_DEVICE_PATH          0x02

//
// ACPI Device Path SubType.
//

#define ACPI_DP                   0x01

//
// Expanded ACPI Device Path SubType.
//

#define ACPI_EXTENDED_DP          0x02

//
// Define constants used for EISA ID conversion.
//

#define PNP_EISA_ID_CONST         0x41D0
#define PNP_EISA_ID_MASK          0xFFFF

//
// ACPI _ADR Device Path SubType.
//

#define ACPI_ADR_DP               0x03
#define ACPI_ADR_DISPLAY_TYPE_OTHER             0
#define ACPI_ADR_DISPLAY_TYPE_VGA               1
#define ACPI_ADR_DISPLAY_TYPE_TV                2
#define ACPI_ADR_DISPLAY_TYPE_EXTERNAL_DIGITAL  3
#define ACPI_ADR_DISPLAY_TYPE_INTERNAL_DIGITAL  4

//
// Messaging Device Paths. This Device Path is used to describe the connection
// of devices outside the resource domain of the system. This Device Path can
// describe physical messaging information like SCSI ID, or abstract
// information like networking protocol IP addresses.
//

#define MESSAGING_DEVICE_PATH     0x03

//
// ATAPI Device Path SubType
//

#define MSG_ATAPI_DP              0x01

//
// SCSI Device Path SubType.
//

#define MSG_SCSI_DP               0x02

//
// Fibre Channel SubType.
//

#define MSG_FIBRECHANNEL_DP       0x03

//
// Fibre Channel Ex SubType.
//

#define MSG_FIBRECHANNELEX_DP     0x15

//
// 1394 Device Path SubType
//

#define MSG_1394_DP               0x04

//
// USB Device Path SubType.
//

#define MSG_USB_DP                0x05

//
// USB Class Device Path SubType.
//

#define MSG_USB_CLASS_DP          0x0F

//
// USB WWID Device Path SubType.
//

#define MSG_USB_WWID_DP           0x10

//
// Device Logical Unit SubType.
//

#define MSG_DEVICE_LOGICAL_UNIT_DP 0x11

//
// SATA Device Path SubType.
//

#define MSG_SATA_DP               0x12

//
// Flag for if the device is directly connected to the HBA.
//

#define SATA_HBA_DIRECT_CONNECT_FLAG 0x8000

//
// I2O Device Path SubType.
//

#define MSG_I2O_DP                0x06

//
// MAC Address Device Path SubType.
//

#define MSG_MAC_ADDR_DP           0x0B

//
// IPv4 Device Path SubType
//

#define MSG_IPv4_DP               0x0C

//
// IPv6 Device Path SubType.
//

#define MSG_IPv6_DP               0x0D

//
// InfiniBand Device Path SubType.
//

#define MSG_INFINIBAND_DP         0x09

#define INFINIBAND_RESOURCE_FLAG_IOC_SERVICE                0x01
#define INFINIBAND_RESOURCE_FLAG_EXTENDED_BOOT_ENVIRONMENT  0x02
#define INFINIBAND_RESOURCE_FLAG_CONSOLE_PROTOCOL           0x04
#define INFINIBAND_RESOURCE_FLAG_STORAGE_PROTOCOL           0x08
#define INFINIBAND_RESOURCE_FLAG_NETWORK_PROTOCOL           0x10

//
// UART Device Path SubType.
//

#define MSG_UART_DP               0x0E

//
// Use VENDOR_DEVICE_PATH struct
//

#define MSG_VENDOR_DP             0x0A

#define DEVICE_PATH_MESSAGING_PC_ANSI     EFI_PC_ANSI_GUID
#define DEVICE_PATH_MESSAGING_VT_100      EFI_VT_100_GUID
#define DEVICE_PATH_MESSAGING_VT_100_PLUS EFI_VT_100_PLUS_GUID
#define DEVICE_PATH_MESSAGING_VT_UTF8     EFI_VT_UTF8_GUID

//
// Define UART flow control bits.
//

#define UART_FLOW_CONTROL_HARDWARE         0x00000001
#define UART_FLOW_CONTROL_XON_XOFF         0x00000010

#define DEVICE_PATH_MESSAGING_SAS          EFI_SAS_DEVICE_PATH_GUID

//
// Serial Attached SCSI (SAS) Ex Device Path SubType
//

#define MSG_SASEX_DP              0x16

//
// NvmExpress Namespace Device Path SubType.
//

#define MSG_NVME_NAMESPACE_DP     0x17

//
// iSCSI Device Path SubType
//

#define MSG_ISCSI_DP              0x13

#define ISCSI_LOGIN_OPTION_NO_HEADER_DIGEST             0x0000
#define ISCSI_LOGIN_OPTION_HEADER_DIGEST_USING_CRC32C   0x0002
#define ISCSI_LOGIN_OPTION_NO_DATA_DIGEST               0x0000
#define ISCSI_LOGIN_OPTION_DATA_DIGEST_USING_CRC32C     0x0008
#define ISCSI_LOGIN_OPTION_AUTHMETHOD_CHAP              0x0000
#define ISCSI_LOGIN_OPTION_AUTHMETHOD_NON               0x1000
#define ISCSI_LOGIN_OPTION_CHAP_BI                      0x0000
#define ISCSI_LOGIN_OPTION_CHAP_UNI                     0x2000

//
// VLAN Device Path SubType.
//

#define MSG_VLAN_DP               0x14

//
// Media Device Path
//

#define MEDIA_DEVICE_PATH         0x04

//
// Hard Drive Media Device Path SubType.
//

#define MEDIA_HARDDRIVE_DP        0x01

#define MBR_TYPE_PCAT             0x01
#define MBR_TYPE_EFI_PARTITION_TABLE_HEADER 0x02

#define NO_DISK_SIGNATURE         0x00
#define SIGNATURE_TYPE_MBR        0x01
#define SIGNATURE_TYPE_GUID       0x02

//
// CD-ROM Media Device Path SubType.
//

#define MEDIA_CDROM_DP            0x02

//
// Define the media vendor device path subtype.
//

#define MEDIA_VENDOR_DP           0x03

//
// File Path Media Device Path SubType
//

#define MEDIA_FILEPATH_DP         0x04

#define SIZE_OF_FILEPATH_DEVICE_PATH  OFFSET_OF(FILEPATH_DEVICE_PATH, PathName)

//
// Media Protocol Device Path SubType.
//

#define MEDIA_PROTOCOL_DP         0x05

//
// PIWG Firmware File SubType.
//

#define MEDIA_PIWG_FW_FILE_DP     0x06

//
// PIWG Firmware Volume Device Path SubType.
//

#define MEDIA_PIWG_FW_VOL_DP      0x07

//
// Media relative offset range device path.
//

#define MEDIA_RELATIVE_OFFSET_RANGE_DP 0x08

//
// BIOS Boot Specification Device Path.
//

#define BBS_DEVICE_PATH           0x05

//
// BIOS Boot Specification Device Path SubType.
//

#define BBS_BBS_DP                0x01

//
// DeviceType definitions - from BBS specification
//

#define BBS_TYPE_FLOPPY           0x01
#define BBS_TYPE_HARDDRIVE        0x02
#define BBS_TYPE_CDROM            0x03
#define BBS_TYPE_PCMCIA           0x04
#define BBS_TYPE_USB              0x05
#define BBS_TYPE_EMBEDDED_NETWORK 0x06
#define BBS_TYPE_BEV              0x80
#define BBS_TYPE_UNKNOWN          0xFF

//
// Define other device path types.
//

#define END_DEVICE_PATH_TYPE                 0x7F
#define END_ENTIRE_DEVICE_PATH_SUBTYPE       0xFF
#define END_INSTANCE_DEVICE_PATH_SUBTYPE     0x01
#define END_DEVICE_PATH_LENGTH               (sizeof(EFI_DEVICE_PATH_PROTOCOL))

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the common header of the EFI Device Path protocol.
    This protocol can be used on any device handle to obtain generic
    path/location information concerning the physical device or logical device.
    If the handle does not logically map to a physical device, the handle may
    not necessarily support the device path protocol. The device path
    describes the location of the device the handle is for. The size of the
    Device Path can be determined from the structures that make up the Device
    Path.

Members:

    Type - Stores the device path type. Valid values are:
        0x01 - Hardware Device Path
        0x02 - ACPI Device Path
        0x03 - Messaging Device Path
        0x04 - Media Device Path
        0x05 - BIOS Boot Specification Device Path
        0x7F - End of Hardware Device Path

    SubType - Stores the subtype, which varies by type. Some values are:
        0xFF - End of entire device path
        0x01 - End this instance of a device path and start a new device path

    Length - Stores the specific Device Path data. The type and subtype define
        the type of this data. The size of the data is included in the length.

--*/

#pragma pack(push, 1)

typedef struct {
    UINT8 Type;
    UINT8 SubType;
    UINT16 Length;
} PACKED EFI_DEVICE_PATH_PROTOCOL;

//
// Device Path protocol definition for backward compatibility with EFI 1.1.
//

typedef EFI_DEVICE_PATH_PROTOCOL  EFI_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a PCI device path.

Members:

    Header - Stores the common device path header.

    Function - Stores the function number of the PCI device.

    Device - Stores the device number of the PCI device.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 Function;
    UINT8 Device;
} PACKED PCI_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a PC Card device path.

Members:

    Header - Stores the common device path header.

    FunctionNumber - Stores the function number of the device. Zero is the
        first function.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 FunctionNumber;
} PACKED PCCARD_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a Memory Mapped Device path.

Members:

    Header - Stores the common device path header.

    MemoryType - Stores the type of memory. See EFI_MEMORY_TYPE definitions.

    StartingAddress - Stores the starting memory address.

    EndingAddress - Stores the ending memory address.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 MemoryType;
    EFI_PHYSICAL_ADDRESS StartingAddress;
    EFI_PHYSICAL_ADDRESS EndingAddress;
} PACKED MEMMAP_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a "vendor" device path. The Vendor Device Path
    allows the creation of vendor-defined Device Paths. A vendor must allocate
    a Vendor GUID for a Device Path. The Vendor GUID can then be used to define
    the contents on the n bytes that follow in the Vendor Device Path node.

Members:

    Header - Stores the common device path header.

    Guid - Stores the vendor-defined GUID that defines the data that follows.
        The remainder of the structure data continues below this member.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID Guid;
} PACKED VENDOR_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a controller device path.

Members:

    Header - Stores the common device path header.

    ControllerNumber - Stores the controller number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 ControllerNumber;
} PACKED CONTROLLER_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an ACPI HID device path.

Members:

    Header - Stores the common device path header.

    HID - Stores the device's PnP hardware ID in a numeric 32-bit compressed
        EISA-type ID. This value must match the corresponding _HID in the ACPI
        namespace.

    UID - Stores the unique ID that is required by ACPI if two devices have the
        same _HID. This value must also match the corresponding _UID/_HID pair
        in the ACPI namespace. Only the 32-bit numeric value type of _UID is
        supported. Thus, strings must not be used for the _UID in the ACPI
        namespace.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 HID;
    UINT32 UID;
} PACKED ACPI_HID_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an ACPI HID device path. At the end of this
    structure, there are optional variable length _HIDSTR, _UIDSTR, and _CIDSTR
    values.

Members:

    Header - Stores the common device path header.

    HID - Stores the device's PnP hardware ID in a numeric 32-bit compressed
        EISA-type ID. This value must match the corresponding _HID in the ACPI
        namespace.

    UID - Stores the unique ID that is required by ACPI if two devices have the
        same _HID. This value must also match the corresponding _UID/_HID pair
        in the ACPI namespace. Only the 32-bit numeric value type of _UID is
        supported. Thus, strings must not be used for the _UID in the ACPI
        namespace.

    CID - Stores the device's compatible PnP hardware ID stored in a numeric
        32-bit compressed EISA-type ID. This value must match at least one of
        the compatible device IDs returned by the corresponding _CID in the
        ACPI name space.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 HID;
    UINT32 UID;
    UINT32 CID;
} PACKED ACPI_EXTENDED_HID_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an ACPI _ADR device path. The _ADR device path is
    used to contain video output device attributes to support the Graphics
    Output Protocol. The device path can contain multiple _ADR entries if
    multiple video output devices are displaying the same output.

Members:

    Header - Stores the common device path header.

    ADR - Stores the _ADR value. For video output devices the value of this
        field comes from Table B-2 of the ACPI 3.0 specification. At least one
        _ADR value is required. Additional ADR members may be immediately after
        this one.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 ADR;
} PACKED ACPI_ADR_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an ATAPI device path.

Members:

    Header - Stores the common device path header.

    PrimarySecondary - Stores zero if this is the primary device, or one if
        this is the secondary device.

    SlaveMaster - Stores zero if this is the master, or one if this is the
        slave.

    Lun - Stores the Logical Unit Number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 PrimarySecondary;
    UINT8 SlaveMaster;
    UINT16 Lun;
} PACKED ATAPI_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a SCSI device path.

Members:

    Header - Stores the common device path header.

    Pun - Stores the target ID on the SCSI bus.

    Lun - Stores the Logical Unit Number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 Pun;
    UINT16 Lun;
} PACKED SCSI_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a FibreChannel device path.

Members:

    Header - Stores the common device path header.

    Reserved - Stores a value reserved for the future.

    WWN - Stores the World Wide Number.

    Lun - Stores the Logical Unit Number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Reserved;
    UINT64 WWN;
    UINT64 Lun;
} PACKED FIBRECHANNEL_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a fancier FibreChannel device path.

Members:

    Header - Stores the common device path header.

    Reserved - Stores a value reserved for the future.

    WWN - Stores the 8-byte End Device Port Name.

    Lun - Stores the 8-byte Logical Unit Number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Reserved;
    UINT8 WWN[8];
    UINT8 Lun[8];
} PACKED FIBRECHANNELEX_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a 1394 device path.

Members:

    Header - Stores the common device path header.

    Reserved - Stores a value reserved for the future.

    Guid - Stores the 1394 Global Unique ID.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Reserved;
    UINT64 Guid;
} PACKED F1394_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a USB device path.

Members:

    Header - Stores the common device path header.

    ParentPortNumber - Stores the USB Parent port number.

    InterfaceNumber - Stores the USB Interface number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 ParentPortNumber;
    UINT8 InterfaceNumber;
} PACKED USB_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a USB class device path.

Members:

    Header - Stores the common device path header.

    VendorId - Stores the vendor ID assigned by USB-IF. A value of 0xFFFF will
        match any Vendor ID.

    ProductId - Stores the product ID. A value of 0xFFFF will match any product
        ID.

    DeviceClass - Stores the class code assigned by USB-IF. A value of 0xFF
        will match any class code.

    DeviceSubClass - Stores the subclass code assigned by the USB-IF. A value
        of 0xFF will match any subclass code.

    DeviceProtocol - Stores the protocol code assigned by the USB-IF. A value
        of 0xFF will match any protocol code.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 VendorId;
    UINT16 ProductId;
    UINT8 DeviceClass;
    UINT8 DeviceSubClass;
    UINT8 DeviceProtocol;
} PACKED USB_CLASS_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a USB device path by its serial number. Immediately
    after this structure is a variable length of CHAR16s containing the last 64
    or fewer UTF-16 characters of the USB serial number. The length of the
    string is determined by the length field minus the offset of the serial
    number field (10).

Members:

    Header - Stores the common device path header.

    InterfaceNumber - Stores the USB Interface number.

    VendorId - Stores the vendor ID assigned by USB-IF.

    ProductId - Stores the product ID.

    SerialNumber - Storse the last 64 or fewer UTF-16 characters of the USB
        serial number. The length of the string is determined by the length
        field minus the offset of the serial number field (10).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 InterfaceNumber;
    UINT16 VendorId;
    UINT16 ProductId;
    // CHAR16 SerialNumber[...];
} PACKED USB_WWID_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a logical unit device path.

Members:

    Header - Stores the common device path header.

    Lun - Stores the logical unit number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 Lun;
} PACKED DEVICE_LOGICAL_UNIT_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a SATA device path.

Members:

    Header - Stores the common device path header.

    HBAPortNumber - Stores the HBA port number that facilitates the connection
        to the device or a port multiplier. The value 0xFFFF is reserved.

    PortMultiplierPortNumber - Stores the Port multiplier port number that
        facilitates the connection to the device. Bit 15 should be set if the
        device is directly connected to the HBA.

    Lun - Stores the logical unit number.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 HBAPortNumber;
    UINT16 PortMultiplierPortNumber;
    UINT16 Lun;
} PACKED SATA_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an I2O device path.

Members:

    Header - Stores the common device path header.

    Tid - Stores the Target ID (TID) for a device.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Tid;
} PACKED I2O_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an MAC Address device path.

Members:

    Header - Stores the common device path header.

    MacAddress - Stores the MAC address for a network interface, padded with
        zeros.

    IfType - Stores the network interface type (ie. 802.3, FDDI).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_MAC_ADDRESS MacAddress;
    UINT8 IfType;
} PACKED MAC_ADDR_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an IPv4 Address device path.

Members:

    Header - Stores the common device path header.

    LocalIpAddress - Stores the local IP address.

    RemoteIpAddress - Stores the remote IP address.

    LocalPort - Stores the local port number.

    RemotePort - Stores the remote port number.

    Protocol - Stores the network protocol (ie. UDP, TCP).

    StaticIpAddress - Stores a boolean indicating whether the source IP
        address was assigned through DHCP (FALSE) or is statically bound (TRUE).

    GatewayIpAddress - Stores the gateway IP address.

    SubnetMask - Stores the subnet mask.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_IPv4_ADDRESS LocalIpAddress;
    EFI_IPv4_ADDRESS RemoteIpAddress;
    UINT16 LocalPort;
    UINT16 RemotePort;
    UINT16 Protocol;
    BOOLEAN StaticIpAddress;
    EFI_IPv4_ADDRESS GatewayIpAddress;
    EFI_IPv4_ADDRESS SubnetMask;
} PACKED IPv4_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an IPv6 Address device path.

Members:

    Header - Stores the common device path header.

    LocalIpAddress - Stores the local IP address.

    RemoteIpAddress - Stores the remote IP address.

    LocalPort - Stores the local port number.

    RemotePort - Stores the remote port number.

    Protocol - Stores the network protocol (ie. UDP, TCP).

    IpAddressOrigin - Stores a value indicating whether the source IP
        address was assigned through DHCP (0), is statically bound (1), or
        is assigned through IPv6 stateful configuration (2).

    PrefixLength - Stores the prefix length.

    GatewayIpAddress - Stores the gateway IP address.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_IPv6_ADDRESS LocalIpAddress;
    EFI_IPv6_ADDRESS RemoteIpAddress;
    UINT16 LocalPort;
    UINT16 RemotePort;
    UINT16 Protocol;
    UINT8 IpAddressOrigin;
    UINT8 PrefixLength;
    EFI_IPv6_ADDRESS GatewayIpAddress;
} PACKED IPv6_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an InfiniBand device path.

Members:

    Header - Stores the common device path header.

    ResourceFlags - Stores a bitfield of flags to help identify and manage the
        InfiniBand device path elements. Valid bits are:
        Bit 0 - IOC/Service (0b = IOC, 1b = Service).
        Bit 1 - Extend Boot Environment.
        Bit 2 - Console Protocol.
        Bit 3 - Storage Protocol.
        Bit 4 - Network Protocol.
        All other bits are reserved.

    PortGid - Stores the 128 bit Global identifier for the remote fabric port.

    ServiceId - Stores the 64-bit unique identifier to the remote IOC or
        server process. Interpretation of the field is specified by the
        resource flags (bit zero).

    TargetPortId - Stores the 64-bit persistent ID of the remote IOC port.

    DeviceId - Stores the 64-bit persistent ID of the remote device.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 ResourceFlags;
    UINT8 PortGid[16];
    UINT64 ServiceId;
    UINT64 TargetPortId;
    UINT64 DeviceId;
} PACKED INFINIBAND_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a UART device path.

Members:

    Header - Stores the common device path header.

    Reserved - Stores a reserved value.

    BaudRate - Stores the baud rate setting for the UART. A value of 0 means
        the device's default baud rate will be used.

    DataBits - Storse the number of data bits for the UART. A value of 0 means
        the device's default number of data bits will be used.

    Parity - Storse the parity setting for the UART device. Valid values are:
        0x00 - Default Parity.
        0x01 - No Parity.
        0x02 - Even Parity.
        0x03 - Odd Parity.
        0x04 - Mark Parity.
        0x05 - Space Parity.

    StopBits - Stores the number of stop bits for the UART device. Valid values
        are:
        0x00 - Default Stop Bits.
        0x01 - 1 Stop Bit.
        0x02 - 1.5 Stop Bits.
        0x03 - 2 Stop Bits.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Reserved;
    UINT64 BaudRate;
    UINT8 DataBits;
    UINT8 Parity;
    UINT8 StopBits;
} PACKED UART_DEVICE_PATH;

typedef VENDOR_DEVICE_PATH VENDOR_DEFINED_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a UART flow control device path.

Members:

    Header - Stores the common device path header.

    Guid - Stores the DEVICE_PATH_MESSAGING_UART_FLOW_CONTROL GUID.

    FlowControlMap - Stores the bitmap of supported flow control types. Valid
        values are:
        Bit 0 set indicates hardware flow control.
        Bit 1 set indicates Xon/Xoff flow control.
        All other bits are reserved and are clear.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID Guid;
    UINT32 FlowControlMap;
} PACKED UART_FLOW_CONTROL_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a Serial Attached SCSI (SAS) device path.

Members:

    Header - Stores the common device path header.

    Guid - Stores the DEVICE_PATH_MESSAGING_SAS GUID.

    Reserved - Stores a value reserved for future use.

    SasAddress - Stores the SAS address for the Serial Attached SCSI target.

    Lun - Stores the SAS Logical Unit Number.

    DeviceTopology - Stores more information about the device and its
        interconnect.

    RelativeTargetPort - Stores the Relative Target Port (RTP).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID Guid;
    UINT32 Reserved;
    UINT64 SasAddress;
    UINT64 Lun;
    UINT16 DeviceTopology;
    UINT16 RelativeTargetPort;
} PACKED SAS_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a fancier Serial Attached SCSI (SAS) device path.

Members:

    Header - Stores the common device path header.

    SasAddress - Stores the 8-byte SAS address for the Serial Attached SCSI
        target.

    Lun - Stores the 8-byte SAS Logical Unit Number.

    DeviceTopology - Stores more information about the device and its
        interconnect.

    RelativeTargetPort - Stores the Relative Target Port (RTP).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT8 SasAddress[8];
    UINT8 Lun[8];
    UINT16 DeviceTopology;
    UINT16 RelativeTargetPort;
} PACKED SASEX_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a NvmExpress Namespace device path.

Members:

    Header - Stores the common device path header.

    NamespaceId - Stores the namespace identifier.

    NamespaceUuid - Stores the 64-bit namespace ID.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 NamespaceId;
    UINT64 NamespaceUuid;
} PACKED NVME_NAMESPACE_DEVICE_PATH;

/*++

Structure Description:

    This structure defines an iSCSI device path. After this structure is the
    iSCSI target name.

Members:

    Header - Stores the common device path header.

    NetworkProtocol - Stores the network protocol. 0 for TCP, 1 and beyond is
        reserved.

    LoginOption - Stores the iSCSI Login Options.

    Lun - Stores the iSCSI Logical Unit Number.

    TargetPortalGroupTag - Stores the iSCSI Target Portal group tag the
        initiator intends to establish a session with.

    TargetName - Stores the iSCSI target name. The length of the name is
        determined by subtracting the offset of this field from the length.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 NetworkProtocol;
    UINT16 LoginOption;
    UINT64 Lun;
    UINT16 TargetPortalGroupTag;
    // CHAR8 iSCSI Target Name.
} PACKED ISCSI_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a vLAN device path.

Members:

    Header - Stores the common device path header.

    VlanId - Stores the VLAN identifier (0-4094).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 VlanId;
} PACKED VLAN_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a hard drive media device path, which is used to
    represent a partition on a hard drive.

Members:

    Header - Stores the common device path header.

    PartitionNumber - Stores the entry in a partition table, starting with
        entry 1. Partition zero represents the entire device. Valid numbers for
        an MBR partition are 1 to 4, inclusive. Valid numers for a GPT
        partition are 1 to NumberOfPartitionEntries, inclusive.

    PartitionStart - Stores the starting LBA of the partition.

    PartitionSize - Stores the size of the partition in logical block units.

    Signature - Stores the partition signature. If the signature type is 0,
        this is filled with 16 zeros. If the signature type is 1, the MBR
        signature is stored in the first 4 bytes of this field, and all 12
        other bytes are filled with zeros. If the signature type is 2, this
        field contains a 16 byte signature.

    MBRType - Stores the partition format. Valid values are 1 for MBR style,
        and 2 for GUID Partition Table.

    SignatureType - Stores the type of disk signature. Valid values are:
        0x00 - No Disk Signature.
        0x01 - 32-bit signature from address 0x1b8 of the type 0x01 MBR.
        0x02 - GUID signature.
        All other values are reserved.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 PartitionNumber;
    UINT64 PartitionStart;
    UINT64 PartitionSize;
    UINT8 Signature[16];
    UINT8 MBRType;
    UINT8 SignatureType;
} PACKED HARDDRIVE_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a CD-ROM Media Device Path, which is used to
    define a system partition on a CD-ROM.

Members:

    Header - Stores the common device path header.

    BootEntry - Stores the boot entry number from the boot catalog. The
        initial/default entry is defined as zero.

    PartitionStart - Stores the starting RBA of the partition on the medium.
        CD-ROMs use Relative logical Block Addressing.

    PartitionSize - Stores the size of the partition in units of blocks (aka
        sectors).

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 BootEntry;
    UINT64 PartitionStart;
    UINT64 PartitionSize;
} PACKED CDROM_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a file path.

Members:

    Header - Stores the common device path header.

    PathName - Stores a NULL-terminated path string including directory and
        file names.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    CHAR16 PathName[1];
} PACKED FILEPATH_DEVICE_PATH;

/*++

Structure Description:

    This structure defines a Media Protocol device path. The Media Protocol
    Device Path is used to denote the protocol that is being used in a device
    path at the location of the path specified. Many protocols are inherent to
    the style of device path.

Members:

    Header - Stores the common device path header.

    Protocol - Stores the GUID of the protocol in use.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID Protocol;
} PACKED MEDIA_PROTOCOL_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the firmware volume file path device path. This
    device path is used by systems implementing the UEFI PI Specification 1.0
    to describe a firmware file.

Members:

    Header - Stores the common device path header.

    FvFileName - Stores the GUID of the file.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID FvFileName;
} PACKED MEDIA_FW_VOL_FILEPATH_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the firmware volume device path. This device path is
    used by systems implementing the UEFI PI Specification 1.0 to describe a
    firmware volume.

Members:

    Header - Stores the common device path header.

    FvName - Stores the GUID of the firmware volume.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    EFI_GUID FvName;
} PACKED MEDIA_FW_VOL_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the Media Relative Offset Range device path. This
    device path type is used to describe the offset range of media relative.

Members:

    Header - Stores the common device path header.

    Reserved - Stores a reserved value.

    StartingOffset - Stores the start offset.

    EndingOffset - Stores the end offset.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT32 Reserved;
    UINT64 StartingOffset;
    UINT64 EndingOffset;
} PACKED MEDIA_RELATIVE_OFFSET_RANGE_DEVICE_PATH;

/*++

Structure Description:

    This structure defines the BBS BBS device path. This Device Path is used to
    describe the booting of non-EFI-aware operating systems.

Members:

    Header - Stores the common device path header.

    DeviceType - Stores the device type as defined by the BIOS Boot
        Specification.

    StatusFlag - Stores the status flags as defined by the Bios Boot
        Specification.

    String - Stores the null-terminated ASCII string that describes the boot
        device to a user.

--*/

typedef struct {
    EFI_DEVICE_PATH_PROTOCOL Header;
    UINT16 DeviceType;
    UINT16 StatusFlag;
    CHAR8 String[1];
} PACKED BBS_BBS_DEVICE_PATH;

//
// Union of all possible Device Paths and pointers to Device Paths.
//

typedef union {
    EFI_DEVICE_PATH_PROTOCOL DevPath;
    PCI_DEVICE_PATH Pci;
    PCCARD_DEVICE_PATH PcCard;
    MEMMAP_DEVICE_PATH MemMap;
    VENDOR_DEVICE_PATH Vendor;
    CONTROLLER_DEVICE_PATH Controller;
    ACPI_HID_DEVICE_PATH Acpi;
    ACPI_EXTENDED_HID_DEVICE_PATH ExtendedAcpi;
    ACPI_ADR_DEVICE_PATH AcpiAdr;
    ATAPI_DEVICE_PATH Atapi;
    SCSI_DEVICE_PATH Scsi;
    ISCSI_DEVICE_PATH Iscsi;
    FIBRECHANNEL_DEVICE_PATH FibreChannel;
    FIBRECHANNELEX_DEVICE_PATH FibreChannelEx;
    F1394_DEVICE_PATH F1394;
    USB_DEVICE_PATH Usb;
    SATA_DEVICE_PATH Sata;
    USB_CLASS_DEVICE_PATH UsbClass;
    USB_WWID_DEVICE_PATH UsbWwid;
    DEVICE_LOGICAL_UNIT_DEVICE_PATH LogicUnit;
    I2O_DEVICE_PATH I2O;
    MAC_ADDR_DEVICE_PATH MacAddr;
    IPv4_DEVICE_PATH Ipv4;
    IPv6_DEVICE_PATH Ipv6;
    VLAN_DEVICE_PATH Vlan;
    INFINIBAND_DEVICE_PATH InfiniBand;
    UART_DEVICE_PATH Uart;
    UART_FLOW_CONTROL_DEVICE_PATH UartFlowControl;
    SAS_DEVICE_PATH Sas;
    SASEX_DEVICE_PATH SasEx;
    NVME_NAMESPACE_DEVICE_PATH NvmeNamespace;
    HARDDRIVE_DEVICE_PATH HardDrive;
    CDROM_DEVICE_PATH CD;
    FILEPATH_DEVICE_PATH FilePath;
    MEDIA_PROTOCOL_DEVICE_PATH MediaProtocol;
    MEDIA_FW_VOL_DEVICE_PATH FirmwareVolume;
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH FirmwareFile;
    MEDIA_RELATIVE_OFFSET_RANGE_DEVICE_PATH Offset;
    BBS_BBS_DEVICE_PATH Bbs;
} PACKED EFI_DEV_PATH;

typedef union {
    EFI_DEVICE_PATH_PROTOCOL *DevPath;
    PCI_DEVICE_PATH *Pci;
    PCCARD_DEVICE_PATH *PcCard;
    MEMMAP_DEVICE_PATH *MemMap;
    VENDOR_DEVICE_PATH *Vendor;
    CONTROLLER_DEVICE_PATH *Controller;
    ACPI_HID_DEVICE_PATH *Acpi;
    ACPI_EXTENDED_HID_DEVICE_PATH *ExtendedAcpi;
    ACPI_ADR_DEVICE_PATH *AcpiAdr;
    ATAPI_DEVICE_PATH *Atapi;
    SCSI_DEVICE_PATH *Scsi;
    ISCSI_DEVICE_PATH *Iscsi;
    FIBRECHANNEL_DEVICE_PATH *FibreChannel;
    FIBRECHANNELEX_DEVICE_PATH *FibreChannelEx;
    F1394_DEVICE_PATH *F1394;
    USB_DEVICE_PATH *Usb;
    SATA_DEVICE_PATH *Sata;
    USB_CLASS_DEVICE_PATH *UsbClass;
    USB_WWID_DEVICE_PATH *UsbWwid;
    DEVICE_LOGICAL_UNIT_DEVICE_PATH *LogicUnit;
    I2O_DEVICE_PATH *I2O;
    MAC_ADDR_DEVICE_PATH *MacAddr;
    IPv4_DEVICE_PATH *Ipv4;
    IPv6_DEVICE_PATH *Ipv6;
    VLAN_DEVICE_PATH *Vlan;
    INFINIBAND_DEVICE_PATH *InfiniBand;
    UART_DEVICE_PATH *Uart;
    UART_FLOW_CONTROL_DEVICE_PATH *UartFlowControl;
    SAS_DEVICE_PATH *Sas;
    SASEX_DEVICE_PATH *SasEx;
    NVME_NAMESPACE_DEVICE_PATH *NvmeNamespace;
    HARDDRIVE_DEVICE_PATH *HardDrive;
    CDROM_DEVICE_PATH *CD;
    FILEPATH_DEVICE_PATH *FilePath;
    MEDIA_PROTOCOL_DEVICE_PATH *MediaProtocol;
    MEDIA_FW_VOL_DEVICE_PATH *FirmwareVolume;
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FirmwareFile;
    MEDIA_RELATIVE_OFFSET_RANGE_DEVICE_PATH *Offset;
    BBS_BBS_DEVICE_PATH *Bbs;
    UINT8 *Raw;
} PACKED EFI_DEV_PATH_PTR;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

