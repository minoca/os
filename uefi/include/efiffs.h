/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efiffs.h

Abstract:

    This header contains definitions for the EFI Firmware File System.

Author:

    Evan Green 6-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "efiimg.h"

//
// --------------------------------------------------------------------- Macros
//

//
// Define a macro to test attribute bits, using the correct bits depending on
// the erase polarity.
//

#define EFI_TEST_FFS_ATTRIBUTES_BIT(_Attributes, _TestAttributes, _Bit)   \
    ((BOOLEAN)((_Attributes & EFI_FVB_ERASE_POLARITY) ?                   \
               (((~_TestAttributes) & _Bit) == _Bit) :                    \
               ((_TestAttributes & _Bit) == _Bit))

//
// This macro determines if the given header is an FFS2 file.
//

#define EFI_IS_FFS_FILE2(_FileHeader)                               \
    (((((EFI_FFS_FILE_HEADER *)(UINTN)_FileHeader)->Attributes) &   \
      FFS_ATTRIB_LARGE_FILE) == FFS_ATTRIB_LARGE_FILE)

//
// These macros return the file size given the FFS file header.
//

#define EFI_FFS_FILE_SIZE(_FileHeader)              \
    (((UINT32)((_FileHeader)->Size[0])) |           \
     (((UINT32)((_FileHeader)->Size[1])) << 8) |    \
     (((UINT32)((_FileHeader)->Size[2])) << 16))

#define EFI_FFS_FILE2_SIZE(_FileHeader) \
    (((EFI_FFS_FILE_HEADER2 *)(UINTN)_FileHeader)->ExtendedSize)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define FFS GUIDs.
//

#define EFI_FIRMWARE_FILE_SYSTEM_GUID                       \
    {                                                       \
        0x7A9354D9, 0x0468, 0x444A,                         \
        {0x81, 0xCE, 0x0B, 0xF6, 0x17, 0xD8, 0x90, 0xDF}    \
    }

#define EFI_FIRMWARE_FILE_SYSTEM2_GUID                      \
    {                                                       \
        0x8C8CE578, 0x8A3D, 0x4F1C,                         \
        {0x99, 0x35, 0x89, 0x61, 0x85, 0xC3, 0x2D, 0xD3}    \
    }

#define EFI_FIRMWARE_FILE_SYSTEM3_GUID                      \
    {                                                       \
        0x5473C07A, 0x3DCB, 0x4DCA,                         \
        {0xBD, 0x6F, 0x1E, 0x96, 0x89, 0xE7, 0x34, 0x9A}    \
    }

#define EFI_FFS_VOLUME_TOP_FILE_GUID                        \
    {                                                       \
        0x1BA0062E, 0xC779, 0x4582,                         \
        {0x85, 0x66, 0x33, 0x6A, 0xE8, 0xF7, 0x8F, 0x9}     \
    }

//
// Define FFS File Attributes.
//

#define FFS_ATTRIB_LARGE_FILE         0x01

#define FFS_ATTRIB_TAIL_PRESENT     0x01
#define FFS_ATTRIB_RECOVERY         0x02
#define FFS_ATTRIB_FIXED            0x04
#define FFS_ATTRIB_DATA_ALIGNMENT   0x38
#define FFS_ATTRIB_CHECKSUM         0x40

//
// Define firmware volume types.
//

#define EFI_FV_FILETYPE_ALL                   0x00
#define EFI_FV_FILETYPE_RAW                   0x01
#define EFI_FV_FILETYPE_FREEFORM              0x02
#define EFI_FV_FILETYPE_SECURITY_CORE         0x03
#define EFI_FV_FILETYPE_PEI_CORE              0x04
#define EFI_FV_FILETYPE_DXE_CORE              0x05
#define EFI_FV_FILETYPE_PEIM                  0x06
#define EFI_FV_FILETYPE_DRIVER                0x07
#define EFI_FV_FILETYPE_COMBINED_PEIM_DRIVER  0x08
#define EFI_FV_FILETYPE_APPLICATION           0x09
#define EFI_FV_FILETYPE_SMM                   0x0A
#define EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE 0x0B
#define EFI_FV_FILETYPE_COMBINED_SMM_DXE      0x0C
#define EFI_FV_FILETYPE_SMM_CORE              0x0D
#define EFI_FV_FILETYPE_OEM_MIN               0xC0
#define EFI_FV_FILETYPE_OEM_MAX               0xDF
#define EFI_FV_FILETYPE_DEBUG_MIN             0xE0
#define EFI_FV_FILETYPE_DEBUG_MAX             0xEF
#define EFI_FV_FILETYPE_FFS_MIN               0xF0
#define EFI_FV_FILETYPE_FFS_MAX               0xFF
#define EFI_FV_FILETYPE_FFS_PAD               0xF0

//
// Define the fixed checksum value used when checksum bit is clear. Defined in
// PI 1.2.
//

#define FFS_FIXED_CHECKSUM  0xAA

//
// Define file state bits.
//

#define EFI_FILE_HEADER_CONSTRUCTION  0x01
#define EFI_FILE_HEADER_VALID         0x02
#define EFI_FILE_DATA_VALID           0x04
#define EFI_FILE_MARKED_FOR_UPDATE    0x08
#define EFI_FILE_DELETED              0x10
#define EFI_FILE_HEADER_INVALID       0x20

#define EFI_FILE_ALL_STATE_BITS (EFI_FILE_HEADER_CONSTRUCTION |     \
                                 EFI_FILE_HEADER_VALID |            \
                                 EFI_FILE_DATA_VALID |              \
                                 EFI_FILE_MARKED_FOR_UPDATE |       \
                                 EFI_FILE_DELETED |                 \
                                 EFI_FILE_HEADER_INVALID)

#define MAX_FFS_SIZE        0x1000000
#define MAX_SECTION_SIZE    0x1000000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef UINT16 EFI_FFS_FILE_TAIL, *PEFI_FFS_FILE_TAIL;
typedef UINT8 EFI_FFS_FILE_ATTRIBUTES, *PEFI_FFS_FILE_ATTRIBUTES;
typedef UINT8 EFI_FFS_FILE_STATE, *PEFI_FFS_FILE_STATE;

typedef union _EFI_FFS_INTEGRITY_CHECK {
    struct {
        UINT8 Header;
        UINT8 File;
    } Checksum;

    UINT16  Checksum16;
} EFI_FFS_INTEGRITY_CHECK, *PEFI_FFS_INTEGRITY_CHECK;

typedef struct _EFI_FFS_FILE_HEADER {
    EFI_GUID Name;
    EFI_FFS_INTEGRITY_CHECK IntegrityCheck;
    EFI_FV_FILETYPE Type;
    EFI_FFS_FILE_ATTRIBUTES Attributes;
    UINT8 Size[3];
    EFI_FFS_FILE_STATE State;
} EFI_FFS_FILE_HEADER, *PEFI_FFS_FILE_HEADER;

typedef struct _EFI_FFS_FILE_HEADER2 {
    EFI_GUID Name;
    EFI_FFS_INTEGRITY_CHECK IntegrityCheck;
    EFI_FV_FILETYPE Type;
    EFI_FFS_FILE_ATTRIBUTES Attributes;
    UINT8 Size[3];
    EFI_FFS_FILE_STATE State;
    UINT32 ExtendedSize;
} EFI_FFS_FILE_HEADER2, *PEFI_FFS_FILE_HEADER2;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
