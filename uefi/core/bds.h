/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bds.h

Abstract:

    This header contains internal definitions for the Boot Device Selection
    module.

Author:

    Evan Green 17-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "peimage.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_BDS_COMMON_OPTION_MAGIC 0x4F736442 // 'OsdB'

//
// ACPI boot type. For ACPI devices, using sub-types to distinguish devices is
// not allowed, so hardcode their values.
//

#define BDS_EFI_ACPI_FLOPPY_BOOT         0x0201

//
// Message boot type
// If a device path of boot option only points to a message node, the boot
// option is a message boot type.
//

#define BDS_EFI_MESSAGE_ATAPI_BOOT       0x0301
#define BDS_EFI_MESSAGE_SCSI_BOOT        0x0302
#define BDS_EFI_MESSAGE_USB_DEVICE_BOOT  0x0305
#define BDS_EFI_MESSAGE_SATA_BOOT        0x0312
#define BDS_EFI_MESSAGE_MAC_BOOT         0x030b
#define BDS_EFI_MESSAGE_MISC_BOOT        0x03FF

//
// Media boot type. If a device path of boot option contains a media node, the
// boot option is media boot type.
//

#define BDS_EFI_MEDIA_HD_BOOT            0x0401
#define BDS_EFI_MEDIA_CDROM_BOOT         0x0402

//
// BBS boot type. If a device path of boot option contains a BBS node, the boot
// option is BBS boot type.
//

#define BDS_LEGACY_BBS_BOOT              0x0501

#define BDS_EFI_UNSUPPORTED              0xFFFF

#define EFI_BOOT_OPTION_MAX_CHAR 10

//
// This GUID is used for an EFI Variable that stores the front device pathes
// for a partial device path that starts with the HD node.
//

#define EFI_HD_BOOT_DEVICE_PATH_VARIABLE_GUID               \
    {                                                       \
        0xFAB7E9E1, 0x39DD, 0x4F2B,                         \
        {0x84, 0x08, 0xE2, 0x0E, 0x90, 0x6C, 0xB6, 0xDE}    \
    }

#define EFI_HD_BOOT_DEVICE_PATH_VARIABLE_NAME L"HDDP"
#define EFI_MAX_HD_DEVICE_PATH_CACHE_SIZE 12

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_BDS_COMMON_OPTION {
    UINTN Magic;
    LIST_ENTRY ListEntry;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    CHAR16 *OptionName;
    UINTN OptionNumber;
    UINT16 BootCurrent;
    UINT32 Attribute;
    CHAR16 *Description;
    VOID *LoadOptions;
    UINT32 LoadOptionsSize;
    CHAR16 *StatusString;
} EFI_BDS_COMMON_OPTION, *PEFI_BDS_COMMON_OPTION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// BDS boot functions
//

EFI_STATUS
EfipBdsBootViaBootOption (
    PEFI_BDS_COMMON_OPTION Option,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
    );

/*++

Routine Description:

    This routine attempts to boot the given boot option.

Arguments:

    Option - Supplies a pointer to the option to boot.

    DevicePath - Supplies a pointer to the device path describing where to
        load the boot image or legacy BBS device path.

    ExitDataSize - Supplies a pointer where the exit data size will be returned.

    ExitData - Supplies a pointer where a pointer to the exit data will be
        returned.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBdsEnumerateAllBootOptions (
    PLIST_ENTRY OptionList
    );

/*++

Routine Description:

    This routine enumerates all possible boot devices in the system, and
    creates boot options for them. There are six types of automatic boot
    options:

    1. Network - Creates boot options on any load file protocol instances.

    2. Shell - Creates boot options for any firmware volumes that contain a
       specific path on them.

    3. Removable Block I/O - Creates a boot option for any removable block I/O
       device.

    4. Fixed Block I/O - Does not create a boot option for fixed drives.

    5. Non-Block I/O Simple File Systems - Creates a boot option for
       \EFI\BOOT\boot{machine}.EFI using the Simple File System Protocol.

    6. File - Does not create, modify, or delete a boot option pointing at a
       file.

Arguments:

    OptionList - Supplies a pointer to the head of the boot option list.

Return Value:

    EFI status code.

--*/

//
// BDS console functions
//

EFI_STATUS
EfipBdsConnectAllDefaultConsoles (
    VOID
    );

/*++

Routine Description:

    This routine connects the console device based on the console variables.

Arguments:

    None.

Return Value:

    None.

--*/

//
// BDS utility functions
//

VOID
EfipBdsConnectAll (
    VOID
    );

/*++

Routine Description:

    This routine connects all system drivers to controllers first, then
    specially connect the default console. This ensures all system controllers
    are available and the platform default console is connected.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipBdsConnectAllDriversToAllControllers (
    VOID
    );

/*++

Routine Description:

    This routine connects all system drivers to controllers.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipBdsLoadDrivers (
    PLIST_ENTRY DriverList
    );

/*++

Routine Description:

    This routine loads and starts every driver on the given load list.

Arguments:

    DriverList - Supplies a pointer to the head of the list of boot options
        describing the drivers to load.

Return Value:

    None.

--*/

EFI_STATUS
EfipBdsBuildOptionFromVariable (
    PLIST_ENTRY OptionList,
    CHAR16 *VariableName
    );

/*++

Routine Description:

    This routine processes BootOrder or DriverOrder variables.

Arguments:

    OptionList - Supplies a pointer to the head of the list of boot or
        driver options.

    VariableName - Supplies a pointer to the variable name indicating the boot
        order or driver order. Typically this should be BootOrder or
        DriverOrder.

Return Value:

    EFI status code.

--*/

PEFI_BDS_COMMON_OPTION
EfipBdsConvertVariableToOption (
    PLIST_ENTRY OptionList,
    CHAR16 *VariableName
    );

/*++

Routine Description:

    This routine builds a Boot#### or Driver#### option from the given variable
    name. The new option will also be linked into the given list.

Arguments:

    OptionList - Supplies a pointer to the list to link the option into upon
        success.

    VariableName - Supplies a pointer to the variable name, which is typically
        in the form Boot#### or Driver####.

Return Value:

    Returns a pointer to the option on success.

    NULL on failure.

--*/

VOID *
EfipBdsGetVariable (
    CHAR16 *Name,
    EFI_GUID *VendorGuid,
    UINTN *VariableSize
    );

/*++

Routine Description:

    This routine reads the given EFI variable and returns a buffer allocated
    from pool containing its contents.

Arguments:

    Name - Supplies a pointer to the name of the variable to get.

    VendorGuid - Supplies the GUID part of the variable name.

    VariableSize - Supplies a pointer where the size of the variable contents
        will be returned on success.

Return Value:

    Returns a pointer to the variable contents allocated from pool on success.
    The caller is responsible for freeing this memory.

    NULL on failure.

--*/

EFI_DEVICE_PATH_PROTOCOL *
EfipBdsDeletePartialMatchInstance (
    EFI_DEVICE_PATH_PROTOCOL *MultiInstancePath,
    EFI_DEVICE_PATH_PROTOCOL *SingleInstance
    );

/*++

Routine Description:

    This routine deletes the instance in the given multi-instance device path
    that matches partly with the given instance.

Arguments:

    MultiInstancePath - Supplies a pointer to the multi-instance device path.

    SingleInstance - Supplies a pointer to the single-instance device path.

Return Value:

    Returns the modified multi-instance path on success.

    NULL on failure.

--*/

BOOLEAN
EfipBdsMatchDevicePaths (
    EFI_DEVICE_PATH_PROTOCOL *MultiInstancePath,
    EFI_DEVICE_PATH_PROTOCOL *SingleInstance
    );

/*++

Routine Description:

    This routine compares a device path structure to that of all nodes of a
    second device path instance.

Arguments:

    MultiInstancePath - Supplies a pointer to the multi-instance device path to
        search through.

    SingleInstance - Supplies a pointer to the single-instance device path to
        search for.

Return Value:

    TRUE if the single instance path exists somewhere in the multi-instance
    path.

    FALSE if th single instance path is not found anywhere in the
    multi-instance path.

--*/

EFI_STATUS
EfipBdsRegisterNewOption (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CHAR16 *String,
    CHAR16 *VariableName
    );

/*++

Routine Description:

    This routine registers a new Boot#### or Driver#### option base on the
    given variable name. The BootOrder or DriverOrder will also be updated.

Arguments:

    DevicePath - Supplies a pointer to the device path of the option.

    String - Supplies a pointer to a string describing the option.

    VariableName - Supplies a pointer to the string BootOrder or DriverOrder
        to update.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBdsGetImageHeader (
    EFI_HANDLE Device,
    CHAR16 *FileName,
    EFI_IMAGE_DOS_HEADER *DosHeader,
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    );

/*++

Routine Description:

    This routine gets the image headers from an image.

Arguments:

    Device - Supplies the simple file system handle.

    FileName - Supplies the path to the file to get the headers for.

    DosHeader - Supplies a pointer where the DOS header will be returned.

    Header - Supplies a pointer (union) where the PE headers will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the file was not found.

    EFI_LOAD_ERROR if the file is not a valid image.

--*/

EFI_STATUS
EfipBdsConnectDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *Path
    );

/*++

Routine Description:

    This routine creates all handles associated with every device path node.

Arguments:

    Path - Supplies a pointer to the device path to connect.

Return Value:

    EFI Status code.

--*/

BOOLEAN
EfipBdsValidateOption (
    UINT8 *Variable,
    UINTN VariableSize
    );

/*++

Routine Description:

    This routine validates the contents of a Boot#### option variable.

Arguments:

    Variable - Supplies a pointer to the variable contents.

    VariableSize - Supplies the size of the variable contents buffer in bytes.

Return Value:

    TRUE if the variable data is correct.

    FALSE if the variable data is not correct.

--*/

VOID
EfipBdsCreateHexCodeString (
    CHAR16 *String,
    UINT16 HexInteger,
    CHAR16 *Destination,
    UINTN DestinationSize
    );

/*++

Routine Description:

    This routine appends a four-digit hex code to a string. For example,
    Boot####.

Arguments:

    String - Supplies a pointer to the string to prepend to the destination
        string.

    HexInteger - Supplies the four digit hex integer to append.

    Destination - Supplies a pointer to the destination string buffer.

    DestinationSize - Supplies the size of the destination string in bytes.

Return Value:

    None.

--*/

