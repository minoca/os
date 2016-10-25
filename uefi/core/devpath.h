/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devpath.h

Abstract:

    This header contains definitions for the UEFI core device path support.

Author:

    Evan Green 5-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreDuplicateDevicePath (
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

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePath (
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

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePathInstance (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePathInstance
    );

/*++

Routine Description:

    This routine creates a new path by appending the second device path
    instance to the first. The end-of-device-path device node is moved after
    the end of the appended device path instance and a new
    end-of-device-path-instance node is inserted between. If DevicePath is
    NULL, then a copy if DevicePathInstance is returned. If the device path
    instance is NULL, then NULL is returned. If the device path or device path
    instance is invalid, then NULL is returned. If there is not enough memory
    to allocate space for the new device path, then NULL is returned. The
    memory is allocated from EFI boot services memory. It is the responsibility
    of the caller to free the memory allocated.

Arguments:

    DevicePath - Supplies an optional pointer to the device path.

    DevicePathInstance - Supplies a pointer to the device path instance to
        append.

Return Value:

    Returns a pointer to the appended device path on success. The caller is
    responsible for freeing this newly allocated memory.

    NULL on allocation failure.

--*/

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePathNode (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *Node
    );

/*++

Routine Description:

    This routine creates a new device path by appending a copy of the given
    device path node to a copy of the given device path in an allocated buffer.
    The end-of-device-path device node is moved after the end of the appended
    device node. If the node is NULL then a copy of the device path is returned.
    If the device path is NULL then a copy of the node, followed by an
    end-of-device path device node is returned. If both are NULL then a copy of
    an end-of-device-path device node is returned. If there is not enough
    memory to allocate space for the new device path, then NULL is returned.
    The memory is allocated from EFI boot services memory. It is the
    responsibility of the caller to free the memory allocated.

Arguments:

    DevicePath - Supplies an optional pointer to the device path.

    Node - Supplies an optional pointer to the device path node to append.

Return Value:

    Returns a pointer to the appended device path on success. The caller is
    responsible for freeing this newly allocated memory.

    NULL on allocation failure.

--*/

EFIAPI
UINTN
EfiCoreGetDevicePathSize (
    CONST VOID *DevicePath
    );

/*++

Routine Description:

    This routine returns the length of the given device path in bytes.

Arguments:

    DevicePath - Supplies a pointer to the device path.

Return Value:

    Returns the device path size in bytes.

    0 if the device path is NULL or invalid.

--*/

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetNextDevicePathNode (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine returns a pointer to the next node in the device.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns a pointer to the device path node that follows this node.

--*/

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetNextDevicePathInstance (
    EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    UINTN *Size
    );

/*++

Routine Description:

    This routine creates a copy of the current device path instance and returns
    a pointer to the next device path instance.

Arguments:

    DevicePath - Supplies a pointer that on input contains a pointer to the
        device path instance to copy. On output, this will point to the next
        device path instance on success or NULL on failure.

    Size - Supplies a pointer that on input contains the size of the device
        path. On output this value will be updated to contain the remaining
        size.

Return Value:

    Returns a pointer to the current device path instance (duplicate).

--*/

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetDevicePathFromHandle (
    EFI_HANDLE Handle
    );

/*++

Routine Description:

    This routine returns the device path protocol instance on the given handle.

Arguments:

    Handle - Supplies the handle to get the device path on.

Return Value:

    Returns a pointer to the device path protocol on the handle.

    NULL on failure.

--*/

EFIAPI
BOOLEAN
EfiCoreIsDevicePathValid (
    CONST VOID *DevicePath,
    UINTN MaxSize
    );

/*++

Routine Description:

    This routine determines if a device path is valid.

Arguments:

    DevicePath - Supplies a pointer to the device path to query.

    MaxSize - Supplies the maximum size of the device path structure.

Return Value:

    TRUE if the device path is valid.

    FALSE if the length of any node is less than the header size, or the length
    exceeds the given maximum size (if not zero).

--*/

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEnd (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine determines if a device path node is an end node of an
    entire device path.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    TRUE if this node is the end of the entire device path.

    FALSE if this node is not the end of the entire device path.

--*/

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEndInstance (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine determines if a device path node is an end node of a
    device path instance.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    TRUE if this node is the end of the device path instance.

    FALSE if this node is not the end of the device path instance.

--*/

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEndType (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine determines if a device path node is the end device path type.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    TRUE if this node's type is the end type.

    FALSE if this node's type is not the end type.

--*/

EFIAPI
UINT8
EfiCoreGetDevicePathType (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine returns the device path type for the given node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the node's type.

--*/

EFIAPI
UINT8
EfiCoreGetDevicePathSubType (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine returns the device path sub-type for the given node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the node's sub-type.

--*/

EFIAPI
UINTN
EfiCoreGetDevicePathNodeLength (
    CONST VOID *Node
    );

/*++

Routine Description:

    This routine returns the length of the given device path node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the device path node length.

--*/

EFIAPI
VOID
EfiCoreInitializeFirmwareVolumeDevicePathNode (
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FirmwareFile,
    EFI_GUID *NameGuid
    );

/*++

Routine Description:

    This routine initializes a firmware volume file path.

Arguments:

    FirmwareFile - Supplies a pointer to the device path node to initialize.

    NameGuid - Supplies a pointer to the name of the file.

Return Value:

    None.

--*/

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreCreateFileDevicePath (
    EFI_HANDLE Device,
    CONST CHAR16 *FileName
    );

/*++

Routine Description:

    This routine creates a device path for a file and appends it to an existing
    device path. If the given device is a valid handle that contains a device
    path protocol, then a device path for the file specified by the given file
    name is allocated and appended to the device path associated with the
    given handle. The allocated device path is returned. If the device is NULL
    or the device is a handle that does not support the device path protocol,
    then a device path containing a single device path node for the file
    specified by the file name is allocated and returned. The memory for the
    new device path is allocated from EFI boot services memory. It is the
    responsibility of the caller to free the memory allocated.

Arguments:

    Device - Supplies an optional device handle.

    FileName - Supplies a pointer to a null-terminated file path string.

Return Value:

    Returns a pointer to the created device path.

--*/

EFIAPI
EFI_GUID *
EfiCoreGetNameGuidFromFirmwareVolumeDevicePathNode (
    CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *DevicePathNode
    );

/*++

Routine Description:

    This routine returns the file name GUID out of a firmware volume file
    path node.

Arguments:

    DevicePathNode - Supplies a pointer to the device path node.

Return Value:

    Returns a pointer to the file name GUID.

--*/

EFIAPI
UINT16
EfiCoreSetDevicePathNodeLength (
    VOID *Node,
    UINTN Length
    );

/*++

Routine Description:

    This routine sets a device path node length.

Arguments:

    Node - Supplies a pointer to the device path header.

    Length - Supplies the length to set.

Return Value:

    Returns the length value.

--*/

EFIAPI
VOID
EfiCoreSetDevicePathEndNode (
    VOID *Node
    );

/*++

Routine Description:

    This routine sets the given device path node as an end of the entire
    device path.

Arguments:

    Node - Supplies a pointer to the device path header.

Return Value:

    None.

--*/

