/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ramdisk.h

Abstract:

    This header contains definitions for the Minoca-specific UEFI Ram Disk
    protocol.

Author:

    Evan Green 3-Apr-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_RAM_DISK_PROTOCOL_GUID                          \
    {                                                       \
        0x5B1349F8, 0x3FE0, 0x46D7,                         \
        {0xB0, 0x6F, 0xF1, 0xDB, 0x38, 0x95, 0x72, 0xD8}    \
    }

#define EFI_RAM_DISK_PROTOCOL_REVISION 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the RAM Disk protocol structure.

Members:

    Revision - Stores the protocol revision number. All future revisions are
        backwards compatible.

    Base - Stores the base of the RAM disk.

    Length - Stores the size of the RAM disk in bytes.

--*/

typedef struct _EFI_RAM_DISK_PROTOCOL {
    UINT64 Revision;
    EFI_PHYSICAL_ADDRESS Base;
    UINT64 Length;
} EFI_RAM_DISK_PROTOCOL, *PEFI_RAM_DISK_PROTOCOL;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
