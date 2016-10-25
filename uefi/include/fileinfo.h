/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fileinfo.h

Abstract:

    This header contains definitions for EFI File information.

Author:

    Evan Green 13-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// The file name field of the EFI_FILE_INFO data structure is variable length.
// Whenever code needs to know the size of the EFI_FILE_INFO data structure, it
// needs to be the size of the data structure without the file name field.
// The following macro computes this size correctly no matter how big the file
// name array is declared. This is required to make the EFI_FILE_INFO data
// structure ANSI compilant.
//

#define SIZE_OF_EFI_FILE_INFO OFFSET_OF(EFI_FILE_INFO, FileName)

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FILE_INFO_ID                                    \
    {                                                       \
        0x9576E92, 0x6D3F, 0x11D2,                          \
        {0x8E, 0x39, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}    \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines EFI file information.

Members:

    Size - Stores the size of the EFI_FILE_INFO structure, including the
        null-terminated file name string.

    FileSize - Stores the size of the file in bytes.

    PhysicalSize - Stores the amount of physical space the file consumes on the
        file system volume.

    CreateTime - Stores the time the file was created.

    LastAccessTime - Stores the time when the file was last accessed.

    ModificationTime - Stores the time when the file's contents were last
        modified.

    Attribute - Stores the attribute bits for the file.

    FileName - Stores the null-terminated name of the file.

--*/

typedef struct {
    UINT64 Size;
    UINT64 FileSize;
    UINT64 PhysicalSize;
    EFI_TIME CreateTime;
    EFI_TIME LastAccessTime;
    EFI_TIME ModificationTime;
    UINT64 Attribute;
    CHAR16 FileName[1];
} EFI_FILE_INFO;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
