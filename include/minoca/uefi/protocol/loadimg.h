/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loadimg.h

Abstract:

    This header contains definitions for the UEFI Loaded Image Protocol.

Author:

    Evan Green 8-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_LOADED_IMAGE_PROTOCOL_GUID                          \
    {                                                           \
        0x5B1B31A1, 0x9562, 0x11D2,                             \
        {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}        \
    }

#define EFI_LOADED_IMAGE_DEVICE_PATH_PROTOCOL_GUID              \
    {                                                           \
        0xBC62157E, 0x3E33, 0x4FEC,                             \
        {0x99, 0x20, 0x2D, 0x3B, 0x36, 0xD7, 0x50, 0xDF }       \
    }

//
// Protocol GUID defined in EFI1.1.
//

#define LOADED_IMAGE_PROTOCOL  EFI_LOADED_IMAGE_PROTOCOL_GUID

#define EFI_LOADED_IMAGE_PROTOCOL_REVISION  0x1000

//
// Revision defined in EFI1.1.
//

#define EFI_LOADED_IMAGE_INFORMATION_REVISION EFI_LOADED_IMAGE_PROTOCOL_REVISION

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the Loaded Image Protocol, which can be used on any
    image handle to obtain information about the loaded image.

Members:

    Revision - Stores the revision number of the protocol. Set to
        EFI_LOADED_IMAGE_PROTOCOL_REVISION.

    ParentHandle - Stores the parent image's handle, or NULL if the image was
        loaded directly from the firmware's boot manager.

    SystemTable - Stores a pointer to the EFI System Table.

    DeviceHandle - Stores the device handle that the EFI image was loaded from.

    FilePath - Stores a pointer to the file path portion specific to the device
        handle that the image was loaded from.

    Reserved - Stores a reserved pointer. Ignore this.

    LoadOptionsSize - Stores the size in bytes of the load options.

    LoadOptions - Stores a pointer to the image's binary load options.

    ImageBase - Stores the base address at which the image was loaded.

    ImageSize - Stores the size in bytes of the loaded image.

    ImageCodeType - Stores the memory type that code sections were loaded as.

    ImageDataType - Stores the memory type that the data sections were loaded
        as.

    Unload - Stores a pointer to the unload function.

--*/

typedef struct {
    UINT32 Revision;
    EFI_HANDLE ParentHandle;
    EFI_SYSTEM_TABLE *SystemTable;
    EFI_HANDLE DeviceHandle;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    VOID *Reserved;
    UINT32 LoadOptionsSize;
    VOID *LoadOptions;
    VOID *ImageBase;
    UINT64 ImageSize;
    EFI_MEMORY_TYPE ImageCodeType;
    EFI_MEMORY_TYPE ImageDataType;
    EFI_IMAGE_UNLOAD Unload;
} EFI_LOADED_IMAGE_PROTOCOL;

//
// EFI1.1 backward compatibility definition.
//

typedef EFI_LOADED_IMAGE_PROTOCOL EFI_LOADED_IMAGE;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

