/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    eventgrp.h

Abstract:

    This header contains UEFI GUID definitions for well known event groups.

Author:

    Evan Green 4-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define well known event groups.
//

#define EFI_EVENT_GROUP_EXIT_BOOT_SERVICES                  \
    {                                                       \
        0x27ABF055, 0xB1B8, 0x4C26,                         \
        {0x80, 0x48, 0x74, 0x8F, 0x37, 0xBA, 0xA2, 0xDF}    \
    }

#define EFI_EVENT_GROUP_VIRTUAL_ADDRESS_CHANGE              \
    {                                                       \
        0x13FA7698, 0xC831, 0x49C7,                         \
        {0x87, 0xEA, 0x8F, 0x43, 0xFC, 0xC2, 0x51, 0x96}    \
    }

#define EFI_EVENT_GROUP_MEMORY_MAP_CHANGE                   \
    {                                                       \
        0x78BEE926, 0x692F, 0x48FD,                         \
        {0x9E, 0xDB, 0x01, 0x42, 0x2D, 0xF0, 0xF7, 0xAB}    \
    }

#define EFI_EVENT_GROUP_READY_TO_BOOT                       \
    {                                                       \
        0x7CE88FB3, 0x4BD7, 0x4679,                         \
        {0x87, 0xA8, 0xA8, 0xD8, 0xDE, 0xE5, 0x0D, 0x2B}    \
    }

#define EFI_EVENT_GROUP_DXE_DISPATCH_GUID                   \
    {                                                       \
        0x7081E22F, 0xCAC6, 0x4053,                         \
        {0x94, 0x68, 0x67, 0x57, 0x82, 0xCF, 0x88, 0xE5}    \
    }

#define EFI_END_OF_DXE_EVENT_GROUP_GUID                     \
    {                                                       \
        0x02CE967A, 0xDD7E, 0x4FFC,                         \
        {0x9E, 0xE7, 0x81, 0x0C, 0xF0, 0x47, 0x08, 0x80}    \
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
