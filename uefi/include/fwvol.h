/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fwvol.h

Abstract:

    This header contains definitions for UEFI Firmware Volumes.

Author:

    Evan Green 6-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro determines if the given alleged section header is a section
// header version 2.
//

#define EFI_IS_SECTION2(_SectionHeader)                                        \
    (EFI_SECTION_SIZE(_SectionHeader) == 0x00FFFFFF)

#define EFI_SECTION_SIZE(_SectionHeader)                                       \
    (((EFI_COMMON_SECTION_HEADER *)(UINTN)_SectionHeader)->AsUint32 &          \
     0x00FFFFFF)

#define EFI_SECTION2_SIZE(_SectionHeader)                                      \
    (((EFI_COMMON_SECTION_HEADER2 *)                                           \
      (UINTN)_SectionHeader)->Elements.ExtendedSize)

//
// ---------------------------------------------------------------- Definitions
//

//
// Firmware Volume Block Attributes bit definitions.
// They are the shared between Framework and PI1.0.
//

#define EFI_FVB_READ_DISABLED_CAP     0x00000001
#define EFI_FVB_READ_ENABLED_CAP      0x00000002
#define EFI_FVB_READ_STATUS           0x00000004

#define EFI_FVB_WRITE_DISABLED_CAP    0x00000008
#define EFI_FVB_WRITE_ENABLED_CAP     0x00000010
#define EFI_FVB_WRITE_STATUS          0x00000020

#define EFI_FVB_LOCK_CAP              0x00000040
#define EFI_FVB_LOCK_STATUS           0x00000080

#define EFI_FVB_STICKY_WRITE          0x00000200
#define EFI_FVB_MEMORY_MAPPED         0x00000400
#define EFI_FVB_ERASE_POLARITY        0x00000800

#define EFI_FVB2_READ_LOCK_CAP        0x00001000
#define EFI_FVB2_READ_LOCK_STATUS     0x00002000

#define EFI_FVB2_WRITE_LOCK_CAP       0x00004000
#define EFI_FVB2_WRITE_LOCK_STATUS    0x00008000

#define EFI_FVB2_WEAK_ALIGNMENT       0x80000000
#define EFI_FVB2_ALIGNMENT            0x001F0000
#define EFI_FVB2_ALIGNMENT_1          0x00000000
#define EFI_FVB2_ALIGNMENT_2          0x00010000
#define EFI_FVB2_ALIGNMENT_4          0x00020000
#define EFI_FVB2_ALIGNMENT_8          0x00030000
#define EFI_FVB2_ALIGNMENT_16         0x00040000
#define EFI_FVB2_ALIGNMENT_32         0x00050000
#define EFI_FVB2_ALIGNMENT_64         0x00060000
#define EFI_FVB2_ALIGNMENT_128        0x00070000
#define EFI_FVB2_ALIGNMENT_256        0x00080000
#define EFI_FVB2_ALIGNMENT_512        0x00090000
#define EFI_FVB2_ALIGNMENT_1K         0x000A0000
#define EFI_FVB2_ALIGNMENT_2K         0x000B0000
#define EFI_FVB2_ALIGNMENT_4K         0x000C0000
#define EFI_FVB2_ALIGNMENT_8K         0x000D0000
#define EFI_FVB2_ALIGNMENT_16K        0x000E0000
#define EFI_FVB2_ALIGNMENT_32K        0x000F0000
#define EFI_FVB2_ALIGNMENT_64K        0x00100000
#define EFI_FVB2_ALIGNMENT_128K       0x00110000
#define EFI_FVB2_ALIGNMENT_256K       0x00120000
#define EFI_FVB2_ALIGNMENT_512K       0x00130000
#define EFI_FVB2_ALIGNMENT_1M         0x00140000
#define EFI_FVB2_ALIGNMENT_2M         0x00150000
#define EFI_FVB2_ALIGNMENT_4M         0x00160000
#define EFI_FVB2_ALIGNMENT_8M         0x00170000
#define EFI_FVB2_ALIGNMENT_16M        0x00180000
#define EFI_FVB2_ALIGNMENT_32M        0x00190000
#define EFI_FVB2_ALIGNMENT_64M        0x001A0000
#define EFI_FVB2_ALIGNMENT_128M       0x001B0000
#define EFI_FVB2_ALIGNMENT_256M       0x001C0000
#define EFI_FVB2_ALIGNMENT_512M       0x001D0000
#define EFI_FVB2_ALIGNMENT_1G         0x001E0000
#define EFI_FVB2_ALIGNMENT_2G         0x001F0000

#define EFI_FVB_CAPABILITIES (EFI_FVB_READ_DISABLED_CAP |   \
                              EFI_FVB_READ_ENABLED_CAP |    \
                              EFI_FVB_WRITE_DISABLED_CAP |  \
                              EFI_FVB_WRITE_ENABLED_CAP |   \
                              EFI_FVB_LOCK_CAP |            \
                              EFI_FVB2_READ_LOCK_CAP |      \
                              EFI_FVB2_WRITE_LOCK_CAP)

#define EFI_FVB_STATUS (EFI_FVB_READ_STATUS | EFI_FVB_WRITE_STATUS |         \
                        EFI_FVB_LOCK_STATUS | EFI_FVB2_READ_LOCK_STATUS |    \
                        EFI_FVB2_WRITE_LOCK_STATUS)

#define EFI_FV_EXT_TYPE_END   0x00
#define EFI_FV_EXT_TYPE_OEM_TYPE  0x01

//
// Firmware Volume Header Revision definition
//

#define EFI_FVH_REVISION  0x01

//
// PI1.0 define Firmware Volume Header Revision to 2
//

#define EFI_FVH_PI_REVISION  0x02

//
// Firmware Volume Header Signature definition
//

#define EFI_FVH_SIGNATURE 0x4856465F // 'HVF_'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef UINT32 EFI_FVB_ATTRIBUTES, *PEFI_FVB_ATTRIBUTES;

typedef struct _EFI_FIRMWARE_VOLUME_EXT_ENTRY {
    UINT16 ExtEntrySize;
    UINT16 ExtEntryType;
} PACKED EFI_FIRMWARE_VOLUME_EXT_ENTRY, *PEFI_FIRMWARE_VOLUME_EXT_ENTRY;

typedef struct _EFI_FIRMWARE_VOLUME_EXT_HEADER_OEM_TYPE {
    EFI_FIRMWARE_VOLUME_EXT_ENTRY Hdr;
    UINT32 TypeMask;
    EFI_GUID Types[1];
} PACKED EFI_FIRMWARE_VOLUME_EXT_HEADER_OEM_TYPE,
                                    *PEFI_FIRMWARE_VOLUME_EXT_HEADER_OEM_TYPE;

typedef struct _EFI_FIRMWARE_VOLUME_EXT_HEADER {
    EFI_GUID FvName;
    UINT32 ExtHeaderSize;
} PACKED EFI_FIRMWARE_VOLUME_EXT_HEADER, *PEFI_FIRMWARE_VOLUME_EXT_HEADER;

//
// Firmware Volume Header Block Map Entry definition
//

typedef struct {
    UINT32 BlockCount;
    UINT32 BlockLength;
} PACKED EFI_FV_BLOCK_MAP_ENTRY;

//
// Firmware Volume Header definition
//
typedef struct {
    UINT8 ZeroVector[16];
    EFI_GUID FileSystemGuid;
    UINT64 Length;
    UINT32 Signature;
    EFI_FVB_ATTRIBUTES Attributes;
    UINT16 HeaderLength;
    UINT16 Checksum;
    UINT16 ExtHeaderOffset;
    UINT8 Reserved[1];
    UINT8 Revision;
    EFI_FV_BLOCK_MAP_ENTRY BlockMap[1];
} PACKED EFI_FIRMWARE_VOLUME_HEADER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
