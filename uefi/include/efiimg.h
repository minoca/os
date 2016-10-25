/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efiimg.h

Abstract:

    This header contains definitions for the EFI image format.

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
// ---------------------------------------------------------------- Definitions
//

//
// Define EFI file types.
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
#define EFI_FV_FILETYPE_FIRMWARE_VOLUME_IMAGE 0x0B

#define EFI_SECTION_ALL 0x00

//
// Define EFI encapsulation section types.
//

#define EFI_SECTION_COMPRESSION   0x01
#define EFI_SECTION_GUID_DEFINED  0x02

//
// Define EFI leaf section types.
//

#define EFI_SECTION_FIRST_LEAF_SECTION_TYPE 0x10

#define EFI_SECTION_PE32                    0x10
#define EFI_SECTION_PIC                     0x11
#define EFI_SECTION_TE                      0x12
#define EFI_SECTION_DXE_DEPEX               0x13
#define EFI_SECTION_VERSION                 0x14
#define EFI_SECTION_USER_INTERFACE          0x15
#define EFI_SECTION_COMPATIBILITY16         0x16
#define EFI_SECTION_FIRMWARE_VOLUME_IMAGE   0x17
#define EFI_SECTION_FREEFORM_SUBTYPE_GUID   0x18
#define EFI_SECTION_RAW                     0x19
#define EFI_SECTION_PEI_DEPEX               0x1B

#define EFI_SECTION_LAST_LEAF_SECTION_TYPE  0x1B
#define EFI_SECTION_LAST_SECTION_TYPE       0x1B

//
// Define compression type values.
//

#define EFI_NOT_COMPRESSED          0x00
#define EFI_STANDARD_COMPRESSION    0x01
#define EFI_CUSTOMIZED_COMPRESSION  0x02

//
// Define GUIDed section attributes.
//

#define EFI_GUIDED_SECTION_PROCESSING_REQUIRED  0x01
#define EFI_GUIDED_SECTION_AUTH_STATUS_VALID    0x02

//
// Define authentication status bits.
//

#define EFI_AGGREGATE_AUTH_STATUS_PLATFORM_OVERRIDE 0x000001
#define EFI_AGGREGATE_AUTH_STATUS_IMAGE_SIGNED      0x000002
#define EFI_AGGREGATE_AUTH_STATUS_NOT_TESTED        0x000004
#define EFI_AGGREGATE_AUTH_STATUS_TEST_FAILED       0x000008
#define EFI_AGGREGATE_AUTH_STATUS_ALL               0x00000f

#define EFI_LOCAL_AUTH_STATUS_PLATFORM_OVERRIDE     0x010000
#define EFI_LOCAL_AUTH_STATUS_IMAGE_SIGNED          0x020000
#define EFI_LOCAL_AUTH_STATUS_NOT_TESTED            0x040000
#define EFI_LOCAL_AUTH_STATUS_TEST_FAILED           0x080000
#define EFI_LOCAL_AUTH_STATUS_ALL                   0x0f0000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef UINT8 EFI_FV_FILETYPE, *PEFI_FV_FILETYPE;
typedef UINT8 EFI_SECTION_TYPE, *PEFI_SECTION_TYPE;

typedef union _EFI_COMMON_SECTION_HEADER {
    struct {
        UINT8 Size[3];
        UINT8 Type;
    } Elements;

    UINT32 AsUint32;
} PACKED EFI_COMMON_SECTION_HEADER, *PEFI_COMMON_SECTION_HEADER;

typedef union _EFI_COMMON_SECTION_HEADER2 {
    struct {
        UINT8 Size[3];
        EFI_SECTION_TYPE Type;
        UINT32 ExtendedSize;
    } Elements;

    UINT32 AsUint32;
} PACKED EFI_COMMON_SECTION_HEADER2, *PEFI_COMMON_SECTION_HEADER2;

typedef struct _EFI_COMPRESSION_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
    UINT32 UncompressedLength;
    UINT8 CompressionType;
} PACKED EFI_COMPRESSION_SECTION, *PEFI_COMPRESSION_SECTION;

typedef struct _EFI_GUID_DEFINED_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
    EFI_GUID SectionDefinitionGuid;
    UINT16 DataOffset;
    UINT16 Attributes;
} PACKED EFI_GUID_DEFINED_SECTION, *PEFI_GUID_DEFINED_SECTION;

typedef struct _EFI_GUID_DEFINED_SECTION2 {
    EFI_COMMON_SECTION_HEADER2 CommonHeader;
    EFI_GUID SectionDefinitionGuid;
    UINT16 DataOffset;
    UINT16 Attributes;
} PACKED EFI_GUID_DEFINED_SECTION2, *PEFI_GUID_DEFINED_SECTION2;

typedef struct _EFI_PE32_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_PE32_SECTION, *PEFI_PE32_SECTION;

typedef struct _EFI_PIC_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_PIC_SECTION, *PEFI_PIC_SECTION;

typedef struct _EFI_PEIM_HEADER_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_PEIM_HEADER_SECTION, *PEFI_PEIM_HEADER_SECTION;

typedef struct _EFI_DEPEX_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_DEPEX_SECTION, *PEFI_DEPEX_SECTION;

typedef struct _EFI_VERSION_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
    UINT16 BuildNumber;
    INT16 VersionString[1];
} PACKED EFI_VERSION_SECTION, *PEFI_VERSION_SECTION;

typedef struct _EFI_USER_INTERFACE_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
    INT16 FileNameString[1];
} PACKED EFI_USER_INTERFACE_SECTION, *PEFI_USER_INTERFACE_SECTION;

typedef struct _EFI_CODE16_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_CODE16_SECTION, *PEFI_CODE16_SECTION;

typedef struct _EFI_FIRMWARE_VOLUME_IMAGE_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_FIRMWARE_VOLUME_IMAGE_SECTION, *PEFI_FIRMWARE_VOLUME_IMAGE_SECTION;

typedef struct _EFI_FREEFORM_SUBTYPE_GUID_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
    EFI_GUID SubTypeGuid;
} PACKED EFI_FREEFORM_SUBTYPE_GUID_SECTION, *PEFI_FREEFORM_SUBTYPE_GUID_SECTION;

typedef struct _EFI_RAW_SECTION {
    EFI_COMMON_SECTION_HEADER CommonHeader;
} PACKED EFI_RAW_SECTION, *PEFI_RAW_SECTION;

typedef union _EFI_FILE_SECTION_POINTER {
    EFI_COMMON_SECTION_HEADER *CommonHeader;
    EFI_COMPRESSION_SECTION *CompressionSection;
    EFI_GUID_DEFINED_SECTION *GuidDefinedSection;
    EFI_PE32_SECTION *Pe32Section;
    EFI_PIC_SECTION *PicSection;
    EFI_PEIM_HEADER_SECTION *PeimHeaderSection;
    EFI_DEPEX_SECTION *DependencySection;
    EFI_VERSION_SECTION *VersionSection;
    EFI_USER_INTERFACE_SECTION *UISection;
    EFI_CODE16_SECTION *Code16Section;
    EFI_FIRMWARE_VOLUME_IMAGE_SECTION *FVImageSection;
    EFI_FREEFORM_SUBTYPE_GUID_SECTION *FreeformSubtypeSection;
    EFI_RAW_SECTION *RawSection;
} EFI_FILE_SECTION_POINTER, *PEFI_FILE_SECTION_POINTER;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
