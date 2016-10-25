/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    acpi.h

Abstract:

    This header contains UEFI GUID definitions for ACPI.

Author:

    Evan Green 12-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_ACPI_10_TABLE_GUID                                  \
    {                                                           \
        0xEB9D2D30, 0x2D88, 0x11D3,                             \
        {0x9A, 0x16, 0x00, 0x90, 0x27, 0x3F, 0xC1, 0x4D}        \
    }

//
// ACPI 2.0 or newer tables should use this definition.
//

#define EFI_ACPI_20_TABLE_GUID                                  \
    {                                                           \
        0x8868E871, 0xE4F1, 0x11D3,                             \
        {0xBC, 0x22, 0x00, 0x80, 0xC7, 0x3C, 0x88, 0x81}        \
    }

//
// Firmware volume ACPI table storage file GUID
//

#define EFI_ACPI_TABLE_STORAGE_FILE_GUID                        \
    {                                                           \
        0x7E374E25, 0x8E01, 0x4FEE,                             \
        {0x87, 0xF2, 0x39, 0x0C, 0x23, 0xC6, 0x06, 0xCD}        \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
