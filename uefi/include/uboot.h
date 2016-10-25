/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uboot.h

Abstract:

    This header contains definitions for U-Boot image formats.

Author:

    Chris Stevens 1-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// U-Boot header magic numbers.
//

#define UBOOT_MAGIC 0x27051956
#define UBOOT_FIT_MAGIC 0xD00DFEED

//
// U-Boot operating system type definitions.
//

#define UBOOT_OS_INVALID 0
#define UBOOT_OS_LINUX 5

//
// Define the OS name strings.
//

#define UBOOT_OS_STRING_LINUX "linux"

//
// U-Boot architecture type definitions.
//

#define UBOOT_ARCHITECTURE_INVALID 0
#define UBOOT_ARCHITECTURE_ARM 2
#define UBOOT_ARCHITECTURE_X86 3

//
// Define the architecture name strings.
//

#define UBOOT_ARCHITECTURE_STRING_ARM "arm"
#define UBOOT_ARCHITECTURE_STRING_X86 "x86"

//
// U-Boot image type definitions.
//

#define UBOOT_IMAGE_INVALID 0
#define UBOOT_IMAGE_KERNEL 2
#define UBOOT_IMAGE_FLAT_DEVICE_TREE 8
#define UBOOT_IMAGE_KERNEL_NO_LOAD 14

//
// Define the image type strings.
//

#define UBOOT_IMAGE_STRING_KERNEL "kernel"
#define UBOOT_IMAGE_STRING_FLAT_DEVICE_TREE "flat_dt"
#define UBOOT_IMAGE_STRING_KERNEL_NO_LOAD "kernel_noload"

//
// U-Boot compression type definitions.
//

#define UBOOT_COMPRESSION_NONE 0
#define UBOOT_COMPRESSION_GZIP 1
#define UBOOT_COMPRESSION_BZIP2 2

//
// Define the compression type strings.
//

#define UBOOT_COMPRESSION_STRING_NONE "none"
#define UBOOT_COMPRESSION_STRING_GZIP "gzip"
#define UBOOT_COMPRESSION_STRING_BZIP2 "bzip2"

//
// U-Boot image names must not be bigger than 32 characters.
//

#define UBOOT_MAX_NAME 32

//
// The supported version and last compatible version.
//

#define UBOOT_FIT_VERSION 17
#define UBOOT_FIT_LAST_COMPATIBLE_VERSION 16

//
// All U-Boot FIT tags must be aligned on a 4-byte boundary.
//

#define UBOOT_FIT_TAG_ALIGNMENT 4

//
// U-Boot FIT tag definitions.
//

#define UBOOT_FIT_TAG_NODE_START 1
#define UBOOT_FIT_TAG_NODE_END 2
#define UBOOT_FIT_TAG_PROPERTY  3
#define UBOOT_FIT_TAG_NOP 4
#define UBOOT_FIT_TAG_END 9

//
// Define the node strings.
//

#define UBOOT_FIT_NODE_ROOT ""
#define UBOOT_FIT_NODE_IMAGES "images"
#define UBOOT_FIT_NODE_CONFIGURATIONS "configurations"

//
// Define the property strings.
//

#define UBOOT_FIT_PROPERTY_DESCRIPTION "description"
#define UBOOT_FIT_PROPERTY_TIMESTAMP "timestamp"
#define UBOOT_FIT_PROPERTY_DATA "data"
#define UBOOT_FIT_PROPERTY_TYPE "type"
#define UBOOT_FIT_PROPERTY_ARCHITECTURE "arch"
#define UBOOT_FIT_PROPERTY_OS "os"
#define UBOOT_FIT_PROPERTY_COMPRESSION "compression"
#define UBOOT_FIT_PROPERTY_LOAD_ADDRESS "load"
#define UBOOT_FIT_PROPERTY_ENTRY_POINT "entry"
#define UBOOT_FIT_PROPERTY_DEFAULT "default"
#define UBOOT_FIT_PROPERTY_KERNEL "kernel"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the image header U-Boot is expecting at the
    beginning of the image. All data is stored *big endian* in this structure.

Members:

    Magic - Stores the magic number indicating that this is a U-Boot image.
        See UBOOT_MAGIC.

    HeaderCrc32 - Stores the CRC32 checksum of the header structure. This field
        is assumed to be 0.

    CreationTimestamp - Stores the creation date of the image.

    DataSize - Stores the size of the image data.

    DataLoadAddress - Stores the address to load the data to.

    EntryPoint - Stores the initial address to jump to within the image.

    DataCrc32 - Stores the CRC32 checksum of only the data (not this header).

    OperatingSystem - Stores the operating system of the image.

    Architecture - Stores the CPU architecture of the image.

    ImageType - Stores the image type.

    CompressionType - Stores the compression type.

    ImageName - Stores the name of the image.

--*/

typedef struct _UBOOT_HEADER {
    UINT32 Magic;
    UINT32 HeaderCrc32;
    UINT32 CreationTimestamp;
    UINT32 DataSize;
    UINT32 DataLoadAddress;
    UINT32 EntryPoint;
    UINT32 DataCrc32;
    UINT8 OperatingSystem;
    UINT8 Architecture;
    UINT8 ImageType;
    UINT8 CompressionType;
    CHAR8 ImageName[UBOOT_MAX_NAME];
} PACKED UBOOT_HEADER, *PUBOOT_HEADER;

/*++

Structure Description:

    This structure describes the header U-Boot is expecting at the beginning of
    and FIT (Flattended Image Tree). All data is stored in *big endian* in this
    structure.

Members:

    Magic - Stores the magic number indicating that this is a U-Boot FIT image.
        See UBOOT_FIT_MAGIC.

    TotalSize - Stores the total size of the U-Boot image, including this
        header.

    StructuresOffset - Stores the offset to the start of the U-Boot FIT
        structures.

    StringsOffset - Stores the offset to the start of the string dictionary
        used to look up FIT property names.

    MemoryReserveMapOffset - Stores the offset to the memory reserve map.

    Version - Stores the version of this structure.

    LastCompatibaleVersion - Stores the version with which this structure was
        last compatible.

    BootCpuId - Stores the ID of the CPU booting the system.

    StringsSize - Stores the size, in bytes, of the string dictionary.

    StructuresSize - Stores the size, in bytes, of the structures.

--*/

typedef struct _UBOOT_FIT_HEADER {
    UINT32 Magic;
    UINT32 TotalSize;
    UINT32 StructuresOffset;
    UINT32 StringsOffset;
    UINT32 MemoryReserveMapOffset;
    UINT32 Version;
    UINT32 LastCompatibleVersion;
    UINT32 BootCpuId;
    UINT32 StringsSize;
    UINT32 StructuresSize;
} PACKED UBOOT_FIT_HEADER, *PUBOOT_FIT_HEADER;

/*++

Structure Description:

    This structure defines the memory reservation for the FIT U-Boot image.

Members:

    BaseAddress - Stores the base address of the memory reserve map.

    Size - Stores the size, in bytes, of the memory reservation.

--*/

typedef struct _UBOOT_FIT_MEMORY_RESERVE_MAP {
    UINT64 BaseAddress;
    UINT64 Size;
} PACKED UBOOT_FIT_MEMORY_RESERVE_MAP, *PUBOOT_FIT_MEMORY_RESERVE_MAP;

/*++

Structure Description:

    This structure defines a FIT image node. The tag should be equal to
    UBOOT_FIT_TAG_NODE_START and is followed by a NULL-terminated string naming
    the node. The next tag will start on a 4-byte boundary after the name.

Members:

    Tag - Stores the FIT node tag. Should be UBOOT_FIT_TAG_NODE_START.

    Name - Stores a NULL-terminated string, naming the node.

--*/

typedef struct _UBOOT_FIT_NODE {
    UINT32 Tag;
    // CHAR8 Name[ANYSIZE_ARRAY];
} PACKED UBOOT_FIT_NODE, *PUBOOT_FIT_NODE;

/*++

Structure Description:

    This structure defines a UBOOT FIT property. The tag should be equal to
    UBOOT_FIT_TAG_PROPERTY. The next tag will start on a 4-byte boundary after
    the data.

Members:

    Tag - Stores the FIT node tag. This should be UBOOT_FIT_TAG_PROPERTY.

    Size - Stores the size of the property data, in bytes.

    StringOffset - Stores the offset, in bytes, into the strings section of the
        image where the property's name is stored.

    Data - Stores the property's data.

--*/

typedef struct _UBOOT_FIT_PROPERTY {
    UINT32 Tag;
    UINT32 Size;
    UINT32 StringOffset;
    // UINT8 Data[Size];
} PACKED UBOOT_FIT_PROPERTY, *PUBOOT_FIT_PROPERTY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

