/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devpathu.h

Abstract:

    This header contains definitions for the UEFI Device Path Utilities
    Protocol.

Author:

    Evan Green 5-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_DEVICE_PATH_UTILITIES_PROTOCOL_GUID             \
    {                                                       \
        0x379BE4E, 0xD706, 0x437D,                          \
        {0xB0, 0x37, 0xED, 0xB8, 0x2F, 0xB7, 0x72, 0xA4}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
UINTN
(EFIAPI *EFI_DEVICE_PATH_UTILS_GET_DEVICE_PATH_SIZE) (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine returns the size of the device path in bytes.

Arguments:

    DevicePath - Supplies a pointer to the device path instance.

Return Value:

    Returns the size of the device path in bytes including the end of path tag.

    0 if the device path is NULL.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_DUP_DEVICE_PATH) (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine creates a duplicate of the specified path.

Arguments:

    DevicePath - Supplies a pointer to the device path instance.

Return Value:

    Returns a pointer to the duplicate device path on success.

    NULL on allocation failure or if the input device path was null.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_APPEND_PATH) (
    CONST EFI_DEVICE_PATH_PROTOCOL *First,
    CONST EFI_DEVICE_PATH_PROTOCOL *Second
    );

/*++

Routine Description:

    This routine creates a new path by appending the second device path to the
    first. If the first source is NULL and the second is not, then a duplicate
    of the second is returned. If the first is not NULL and the second is, a
    duplicate of the first is made. If both are NULL, then a copy of an
    end-of-device-path is returned.

Arguments:

    First - Supplies an optional pointer to the first device path instance.

    Second - Supplies an optional pointer to the second device path instance.

Return Value:

    Returns a pointer to the duplicate appended device path on success.

    NULL on allocation failure.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_APPEND_NODE) (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *DeviceNode
    );

/*++

Routine Description:

    This routine creates a new path by appending the device node to the path.
    If the path is NULL and the node is not, then a duplicate of the node is
    returned. If the path is not NULL and the node is, a duplicate of the path
    is made with an end tag appended. If both are NULL, then a copy of an
    end-of-device-path is returned.

Arguments:

    DevicePath - Supplies an optional pointer to the device path instance.

    DeviceNode - Supplies an optional pointer to the device node instance.

Return Value:

    Returns a pointer to the duplicate appended device path on success.

    NULL on allocation failure.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_APPEND_INSTANCE) (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePathInstance
    );

/*++

Routine Description:

    This routine creates a new path by appending the device path instance to a
    device path.

Arguments:

    DevicePath - Supplies an optional pointer to the device path.

    DevicePathInstance - Supplies a pointer to the device path instance.

Return Value:

    Returns a pointer to the duplicate appended device path on success.

    NULL on allocation failure or if the device path instance was NULL.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_GET_NEXT_INSTANCE) (
    EFI_DEVICE_PATH_PROTOCOL **DevicePathInstance,
    UINTN *DevicePathInstanceSize
    );

/*++

Routine Description:

    This routine creates a copy of the current device path instance and returns
    a pointer to the next device path instance.

Arguments:

    DevicePathInstance - Supplies a pointer that on input contains the
        pointer to the current device path instance. On output, this contains
        the pointer to the next device path instance, or NULL if there are no
        more device path instances on the device path.

    DevicePathInstanceSize - Supplies a pointer where the size of the
        returned device path instance in bytes will be returned.

Return Value:

    Returns a pointer to the duplicate device path on success.

    NULL on allocation failure or if the device path instance was NULL.

--*/

typedef
EFI_DEVICE_PATH_PROTOCOL *
(EFIAPI *EFI_DEVICE_PATH_UTILS_CREATE_NODE) (
    UINT8 NodeType,
    UINT8 NodeSubType,
    UINT16 NodeLength
    );

/*++

Routine Description:

    This routine creates a device node.

Arguments:

    NodeType - Suppiles the device node type.

    NodeSubType - Supplies the node subtype.

    NodeLength - Supplies the length of the device node.

Return Value:

    Returns a pointer to the newly allocated node on success.

    NULL if the node length is less than the size of the header or on
    allocation failure.

--*/

typedef
BOOLEAN
(EFIAPI *EFI_DEVICE_PATH_UTILS_IS_MULTI_INSTANCE) (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

/*++

Routine Description:

    This routine returns a boolean indicating whether or not the given device
    path is a multi-instance device path.

Arguments:

    DevicePath - Supplies a pointer to the device path to query.

Return Value:

    TRUE if the devie path has more than one instance.

    FALSE if the device path is empty or contains only a single instance.

--*/

/*++

Structure Description:

    This structure describes the Device Path Utilities Protocol, which is used
    to create and manipulate device paths and device nodes.

Members:

    GetDevicePathSize - Stores a pointer to a function used to determine the
        size in bytes of a device path.

    DuplicateDevicePath - Stores a pointer to a function used to copy a device
        path.

    AppendDevicePath - Stores a pointer to a function used to append one
        device path to another.

    AppendDeviceNode - Stores a pointer to a function used to append a device
        node to a device path.

    AppendDevicePathInstance - Stores a pointer to a function used to append
        a device path instance to a device path.

    GetNextDevicePathInstance - Stores a pointer to a function used to get
        the next device path instance.

    IsDevicePathMultiInstance - Stores a pointer to a function used to
        determine if a device path contains multiple instances.

    CreateDeviceNode - Stores a pointer to a function used to create a new
        device node.

--*/

typedef struct {
    EFI_DEVICE_PATH_UTILS_GET_DEVICE_PATH_SIZE GetDevicePathSize;
    EFI_DEVICE_PATH_UTILS_DUP_DEVICE_PATH DuplicateDevicePath;
    EFI_DEVICE_PATH_UTILS_APPEND_PATH AppendDevicePath;
    EFI_DEVICE_PATH_UTILS_APPEND_NODE AppendDeviceNode;
    EFI_DEVICE_PATH_UTILS_APPEND_INSTANCE AppendDevicePathInstance;
    EFI_DEVICE_PATH_UTILS_GET_NEXT_INSTANCE GetNextDevicePathInstance;
    EFI_DEVICE_PATH_UTILS_IS_MULTI_INSTANCE IsDevicePathMultiInstance;
    EFI_DEVICE_PATH_UTILS_CREATE_NODE CreateDeviceNode;
} EFI_DEVICE_PATH_UTILITIES_PROTOCOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
