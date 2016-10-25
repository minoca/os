/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devpathu.c

Abstract:

    This module implements device path utilities for the UEFI core.

Author:

    Evan Green 5-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"

//
// ---------------------------------------------------------------- Definitions
//

#define DEVICE_PATH_MAX_NODE_COUNT 255

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiDevicePathProtocolGuid = EFI_DEVICE_PATH_PROTOCOL_GUID;

EFI_DEVICE_PATH_PROTOCOL EfiEndDevicePath = {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    END_DEVICE_PATH_LENGTH
};

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreDuplicateDevicePath (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath
    )

/*++

Routine Description:

    This routine creates a duplicate of the specified path.

Arguments:

    DevicePath - Supplies a pointer to the device path instance.

Return Value:

    Returns a pointer to the duplicate device path on success.

    NULL on allocation failure or if the input device path was null.

--*/

{

    VOID *Copy;
    UINTN Size;

    Size = EfiCoreGetDevicePathSize(DevicePath);
    if (Size == 0) {
        return NULL;
    }

    Copy = EfiCoreAllocateBootPool(Size);
    if (Copy == NULL) {
        return NULL;
    }

    EfiCoreCopyMemory(Copy, (VOID *)DevicePath, Size);
    return Copy;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePath (
    CONST EFI_DEVICE_PATH_PROTOCOL *First,
    CONST EFI_DEVICE_PATH_PROTOCOL *Second
    )

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

{

    UINTN FinalSize;
    UINTN FirstSize;
    EFI_DEVICE_PATH_PROTOCOL *NewDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *SecondLeg;
    UINTN SecondSize;

    if (First == NULL) {
        if (Second != NULL) {
            return EfiCoreDuplicateDevicePath(Second);

        } else {
            return EfiCoreDuplicateDevicePath(&EfiEndDevicePath);
        }
    }

    if (Second == NULL) {
        return EfiCoreDuplicateDevicePath(First);
    }

    if ((EfiCoreIsDevicePathValid(First, 0) == FALSE) ||
        (EfiCoreIsDevicePathValid(Second, 0) == FALSE)) {

        return NULL;
    }

    FirstSize = EfiCoreGetDevicePathSize(First);
    SecondSize = EfiCoreGetDevicePathSize(Second);
    FinalSize = FirstSize + SecondSize - END_DEVICE_PATH_LENGTH;
    NewDevicePath = EfiCoreAllocateBootPool(FinalSize);
    if (NewDevicePath == NULL) {
        return NULL;
    }

    EfiCoreCopyMemory(NewDevicePath, (VOID *)First, FirstSize);

    //
    // Copy over the first end node.
    //

    SecondLeg = (EFI_DEVICE_PATH_PROTOCOL *)((CHAR8 *)NewDevicePath +
                                             (FirstSize -
                                              END_DEVICE_PATH_LENGTH));

    EfiCoreCopyMemory(SecondLeg, (VOID *)Second, SecondSize);
    return NewDevicePath;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePathInstance (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePathInstance
    )

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

{

    EFI_DEVICE_PATH_PROTOCOL *End;
    UINTN InstanceSize;
    EFI_DEVICE_PATH_PROTOCOL *NewDevicePath;
    UINTN SourceSize;

    if (DevicePath == NULL) {
        return EfiCoreDuplicateDevicePath(DevicePathInstance);
    }

    if (DevicePathInstance == NULL) {
        return NULL;
    }

    if ((EfiCoreIsDevicePathValid(DevicePath, 0) == FALSE) ||
        (EfiCoreIsDevicePathValid(DevicePathInstance, 0) == FALSE)) {

        return NULL;
    }

    SourceSize = EfiCoreGetDevicePathSize(DevicePath);
    InstanceSize = EfiCoreGetDevicePathSize(DevicePathInstance);
    NewDevicePath = EfiCoreAllocateBootPool(SourceSize + InstanceSize);
    if (NewDevicePath != NULL) {
        EfiCoreCopyMemory(NewDevicePath, (VOID *)DevicePath, SourceSize);
        End = NewDevicePath;
        while (EfiCoreIsDevicePathEnd(End) == FALSE) {
            End = EfiCoreGetNextDevicePathNode(End);
        }

        End->SubType = END_INSTANCE_DEVICE_PATH_SUBTYPE;
        End = EfiCoreGetNextDevicePathNode(End);
        EfiCoreCopyMemory(End, (VOID *)DevicePathInstance, InstanceSize);
    }

    return NewDevicePath;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreAppendDevicePathNode (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CONST EFI_DEVICE_PATH_PROTOCOL *Node
    )

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

{

    EFI_DEVICE_PATH_PROTOCOL *NewDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *NextNode;
    UINTN NodeLength;
    EFI_DEVICE_PATH_PROTOCOL *NodePath;

    if (Node == NULL) {
        if (DevicePath != NULL) {
            return EfiCoreDuplicateDevicePath(DevicePath);

        } else {
            return EfiCoreDuplicateDevicePath(&EfiEndDevicePath);
        }
    }

    NodeLength = EfiCoreGetDevicePathNodeLength(Node);

    //
    // Create a copy of the node and add an end node to make it a complete
    // device path.
    //

    NodePath = EfiCoreAllocateBootPool(NodeLength + END_DEVICE_PATH_LENGTH);
    if (NodePath == NULL) {
        return NULL;
    }

    EfiCoreCopyMemory(NodePath, (VOID *)Node, NodeLength);
    NextNode = EfiCoreGetNextDevicePathNode(NodePath);
    EfiCoreSetDevicePathEndNode(NextNode);

    //
    // Append the two (now complete) paths.
    //

    NewDevicePath = EfiCoreAppendDevicePath(DevicePath, NodePath);
    EfiCoreFreePool(NodePath);
    return NewDevicePath;
}

EFIAPI
UINTN
EfiCoreGetDevicePathSize (
    CONST VOID *DevicePath
    )

/*++

Routine Description:

    This routine returns the length of the given device path in bytes.

Arguments:

    DevicePath - Supplies a pointer to the device path.

Return Value:

    Returns the device path size in bytes.

    0 if the device path is NULL or invalid.

--*/

{

    UINTN Size;
    CONST EFI_DEVICE_PATH_PROTOCOL *Start;

    if (DevicePath == NULL) {
        return 0;
    }

    if (EfiCoreIsDevicePathValid(DevicePath, 0) == FALSE) {
        return 0;
    }

    //
    // Search for the end of the device path.
    //

    Start = DevicePath;
    while (EfiCoreIsDevicePathEnd(DevicePath) == FALSE) {
        DevicePath = EfiCoreGetNextDevicePathNode(DevicePath);
    }

    //
    // Compute the size and add the end device path entry.
    //

    Size = ((UINTN)DevicePath - (UINTN)Start) +
           EfiCoreGetDevicePathNodeLength(DevicePath);

    return Size;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetNextDevicePathNode (
    CONST VOID *Node
    )

/*++

Routine Description:

    This routine returns a pointer to the next node in the device.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns a pointer to the device path node that follows this node.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *Next;

    Next = (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)Node +
                                        EfiCoreGetDevicePathNodeLength(Node));

    return Next;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetNextDevicePathInstance (
    EFI_DEVICE_PATH_PROTOCOL **DevicePath,
    UINTN *Size
    )

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

{

    EFI_DEVICE_PATH_PROTOCOL *Path;
    EFI_DEVICE_PATH_PROTOCOL *ReturnValue;
    UINT8 SubType;

    ASSERT(Size != NULL);

    if ((DevicePath == NULL) || (*DevicePath == NULL)) {
        *Size = 0;
        return NULL;
    }

    if (EfiCoreIsDevicePathValid(*DevicePath, 0) == FALSE) {
        return NULL;
    }

    //
    // Find the end of the device path instance.
    //

    Path = *DevicePath;
    while (EfiCoreIsDevicePathEndType(Path) == FALSE) {
        Path = EfiCoreGetNextDevicePathNode(Path);
    }

    //
    // Compute the size of the device path instance.
    //

    *Size = ((UINTN)Path - (UINTN)(*DevicePath)) -
            sizeof(EFI_DEVICE_PATH_PROTOCOL);

    //
    // Make a copy and return the device path instance.
    //

    SubType = Path->SubType;
    Path->SubType = END_ENTIRE_DEVICE_PATH_SUBTYPE;
    ReturnValue = EfiCoreDuplicateDevicePath(*DevicePath);
    Path->SubType = SubType;

    //
    // If the device path is an end of entire device path, then another
    // instance does not follow, so null out the device path parameter.
    //

    if (EfiCoreGetDevicePathSubType(Path) == END_ENTIRE_DEVICE_PATH_SUBTYPE) {
        *DevicePath = NULL;

    } else {
        *DevicePath = EfiCoreGetNextDevicePathNode(Path);
    }

    return ReturnValue;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreGetDevicePathFromHandle (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine returns the device path protocol instance on the given handle.

Arguments:

    Handle - Supplies the handle to get the device path on.

Return Value:

    Returns a pointer to the device path protocol on the handle.

    NULL on failure.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *Path;
    EFI_STATUS Status;

    Status = EfiHandleProtocol(Handle,
                               &EfiDevicePathProtocolGuid,
                               (VOID **)&Path);

    if (EFI_ERROR(Status)) {
        Path = NULL;
    }

    return Path;
}

EFIAPI
BOOLEAN
EfiCoreIsDevicePathValid (
    CONST VOID *DevicePath,
    UINTN MaxSize
    )

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

{

    UINTN Count;
    UINTN NodeLength;
    UINTN Size;

    ASSERT(DevicePath != NULL);

    Count = 0;
    Size = 0;
    while (EfiCoreIsDevicePathEnd(DevicePath) == FALSE) {
        NodeLength = EfiCoreGetDevicePathNodeLength(DevicePath);
        if (NodeLength < sizeof(EFI_DEVICE_PATH_PROTOCOL)) {
            return FALSE;
        }

        if (MaxSize > 0) {
            Size += NodeLength;
            if (Size + END_DEVICE_PATH_LENGTH > MaxSize) {
                return FALSE;
            }
        }

        Count += 1;
        if ((DEVICE_PATH_MAX_NODE_COUNT != 0) &&
            (Count >= DEVICE_PATH_MAX_NODE_COUNT)) {

            return FALSE;
        }

        DevicePath = EfiCoreGetNextDevicePathNode(DevicePath);
    }

    if (EfiCoreGetDevicePathNodeLength(DevicePath) != END_DEVICE_PATH_LENGTH) {
        return FALSE;
    }

    return TRUE;
}

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEnd (
    CONST VOID *Node
    )

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

{

    if ((EfiCoreIsDevicePathEndType(Node) != FALSE) &&
        (EfiCoreGetDevicePathSubType(Node) == END_ENTIRE_DEVICE_PATH_SUBTYPE)) {

        return TRUE;
    }

    return FALSE;
}

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEndInstance (
    CONST VOID *Node
    )

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

{

    if ((EfiCoreIsDevicePathEndType(Node) != FALSE) &&
        (EfiCoreGetDevicePathSubType(Node) ==
         END_INSTANCE_DEVICE_PATH_SUBTYPE)) {

        return TRUE;
    }

    return FALSE;
}

EFIAPI
BOOLEAN
EfiCoreIsDevicePathEndType (
    CONST VOID *Node
    )

/*++

Routine Description:

    This routine determines if a device path node is the end device path type.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    TRUE if this node's type is the end type.

    FALSE if this node's type is not the end type.

--*/

{

    if (EfiCoreGetDevicePathType(Node) == END_DEVICE_PATH_TYPE) {
        return TRUE;
    }

    return FALSE;
}

EFIAPI
UINT8
EfiCoreGetDevicePathType (
    CONST VOID *Node
    )

/*++

Routine Description:

    This routine returns the device path type for the given node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the node's type.

--*/

{

    ASSERT(Node != NULL);

    return ((EFI_DEVICE_PATH_PROTOCOL *)Node)->Type;
}

EFIAPI
UINT8
EfiCoreGetDevicePathSubType (
    CONST VOID *Node
    )

/*++

Routine Description:

    This routine returns the device path sub-type for the given node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the node's sub-type.

--*/

{

    ASSERT(Node != NULL);

    return ((EFI_DEVICE_PATH_PROTOCOL *)Node)->SubType;
}

EFIAPI
UINTN
EfiCoreGetDevicePathNodeLength (
    CONST VOID *Node
    )

/*++

Routine Description:

    This routine returns the length of the given device path node.

Arguments:

    Node - Supplies a pointer to the device path node.

Return Value:

    Returns the device path node length.

--*/

{

    ASSERT(Node != NULL);

    return ((EFI_DEVICE_PATH_PROTOCOL *)Node)->Length;
}

EFIAPI
VOID
EfiCoreInitializeFirmwareVolumeDevicePathNode (
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *FirmwareFile,
    EFI_GUID *NameGuid
    )

/*++

Routine Description:

    This routine initializes a firmware volume file path.

Arguments:

    FirmwareFile - Supplies a pointer to the device path node to initialize.

    NameGuid - Supplies a pointer to the name of the file.

Return Value:

    None.

--*/

{

    ASSERT((FirmwareFile != NULL) && (NameGuid != NULL));

    FirmwareFile->Header.Type = MEDIA_DEVICE_PATH;
    FirmwareFile->Header.SubType = MEDIA_PIWG_FW_FILE_DP;
    EfiCoreSetDevicePathNodeLength(&(FirmwareFile->Header),
                                   sizeof(MEDIA_FW_VOL_FILEPATH_DEVICE_PATH));

    EfiCoreCopyMemory(&(FirmwareFile->FvFileName), NameGuid, sizeof(EFI_GUID));
    return;
}

EFIAPI
EFI_DEVICE_PATH_PROTOCOL *
EfiCoreCreateFileDevicePath (
    EFI_HANDLE Device,
    CONST CHAR16 *FileName
    )

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

{

    UINTN AllocationSize;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL *End;
    EFI_DEVICE_PATH_PROTOCOL *FileDevicePath;
    FILEPATH_DEVICE_PATH *FilePath;
    UINTN Size;

    DevicePath = NULL;
    Size = (EfiCoreStringLength((VOID *)FileName) + 1) * sizeof(CHAR16);
    AllocationSize = Size + SIZE_OF_FILEPATH_DEVICE_PATH +
                     END_DEVICE_PATH_LENGTH;

    FileDevicePath = EfiCoreAllocateBootPool(AllocationSize);
    if (FileDevicePath != NULL) {
        FilePath = (FILEPATH_DEVICE_PATH *)FileDevicePath;
        FilePath->Header.Type = MEDIA_DEVICE_PATH;
        FilePath->Header.SubType = MEDIA_FILEPATH_DP;
        EfiCoreCopyMemory(&(FilePath->PathName), (VOID *)FileName, Size);
        EfiCoreSetDevicePathNodeLength(&(FilePath->Header),
                                       Size + SIZE_OF_FILEPATH_DEVICE_PATH);

        End = EfiCoreGetNextDevicePathNode(&(FilePath->Header));
        EfiCoreSetDevicePathEndNode(End);
        if (Device != NULL) {
            DevicePath = EfiCoreGetDevicePathFromHandle(Device);
        }

        DevicePath = EfiCoreAppendDevicePath(DevicePath, FileDevicePath);
        EfiCoreFreePool(FileDevicePath);
    }

    return DevicePath;
}

EFIAPI
EFI_GUID *
EfiCoreGetNameGuidFromFirmwareVolumeDevicePathNode (
    CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *DevicePathNode
    )

/*++

Routine Description:

    This routine returns the file name GUID out of a firmware volume file
    path node.

Arguments:

    DevicePathNode - Supplies a pointer to the device path node.

Return Value:

    Returns a pointer to the file name GUID.

--*/

{

    ASSERT(DevicePathNode != NULL);

    if ((EfiCoreGetDevicePathType(&(DevicePathNode->Header)) ==
         MEDIA_DEVICE_PATH) &&
        (EfiCoreGetDevicePathSubType(&(DevicePathNode->Header)) ==
         MEDIA_PIWG_FW_FILE_DP)) {

        return (EFI_GUID *)(&(DevicePathNode->FvFileName));
    }

    return NULL;
}

EFIAPI
UINT16
EfiCoreSetDevicePathNodeLength (
    VOID *Node,
    UINTN Length
    )

/*++

Routine Description:

    This routine sets a device path node length.

Arguments:

    Node - Supplies a pointer to the device path header.

    Length - Supplies the length to set.

Return Value:

    Returns the length value.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    ASSERT((Node != 0) && (Length >= sizeof(EFI_DEVICE_PATH_PROTOCOL)) &&
           (Length < 64 * 1024));

    DevicePath = Node;
    DevicePath->Length = Length;
    return DevicePath->Length;
}

EFIAPI
VOID
EfiCoreSetDevicePathEndNode (
    VOID *Node
    )

/*++

Routine Description:

    This routine sets the given device path node as an end of the entire
    device path.

Arguments:

    Node - Supplies a pointer to the device path header.

Return Value:

    None.

--*/

{

    ASSERT(Node != NULL);

    EfiCoreCopyMemory(Node, &EfiEndDevicePath, sizeof(EfiEndDevicePath));
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

