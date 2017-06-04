/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    main.c

Abstract:

    This module implements the PC/AT main function. This is called by the file
    system loader code.

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "firmware.h"
#include "bootlib.h"
#include "bootman.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of builtin reserved regions. One is for the image and
// one is for the stack.
//

#define BOOT_MANAGER_RESERVED_REGION_COUNT 2

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Variables defined in the linker script that mark the start and end of the
// image.
//

extern UCHAR _end;
extern UCHAR __executable_start;

//
// Store a fake boot initialization block, since the boot manager is launched
// by the firmware.
//

BOOT_INITIALIZATION_BLOCK BmBootBlock;

//
// Store regions for the boot manager image and stack.
//

BOOT_RESERVED_REGION BmBootRegions[BOOT_MANAGER_RESERVED_REGION_COUNT];

//
// ------------------------------------------------------------------ Functions
//

__USED
INT
BmPcatApplicationMain (
    PVOID TopOfStack,
    ULONG StackSize,
    ULONGLONG PartitionOffset,
    ULONG BootDriveNumber
    )

/*++

Routine Description:

    This routine is the entry point for the PC/AT Boot Manager.

Arguments:

    StartOfLoader - Supplies the address where the loader begins in memory.

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

    PartitionOffset - Supplies the offset, in sectors, from the start of the
        disk where this boot partition resides.

    BootDriveNumber - Supplies the drive number for the device that booted
        the system.

Return Value:

    Returns the step number that failed.

--*/

{

    ULONG PageSize;

    PageSize = MmPageSize();
    RtlZeroMemory(&BmBootBlock, sizeof(BOOT_INITIALIZATION_BLOCK));
    BmBootBlock.Version = BOOT_INITIALIZATION_BLOCK_VERSION;
    BmBootBlock.StackTop = (UINTN)TopOfStack;
    BmBootBlock.StackSize = StackSize;
    BmBootBlock.PartitionOffset = PartitionOffset;
    BmBootBlock.DriveNumber = BootDriveNumber;
    BmBootBlock.ApplicationName = (UINTN)"bootman";
    BmBootBlock.ApplicationLowestAddress = (UINTN)&__executable_start;
    BmBootBlock.ApplicationSize = (UINTN)&_end - (UINTN)&__executable_start;
    BmBootBlock.ApplicationArguments = (UINTN)"";

    //
    // Initialize the reserved regions for the image itself and the stack.
    //

    BmBootRegions[0].Address =
                      ALIGN_RANGE_DOWN((UINTN)(&__executable_start), PageSize);

    BmBootRegions[0].Size =
                      ALIGN_RANGE_UP((UINTN)&_end - (UINTN)&__executable_start,
                                     PageSize);

    BmBootRegions[0].Flags = 0;
    BmBootRegions[1].Address = ALIGN_RANGE_DOWN((UINTN)TopOfStack - StackSize,
                                                PageSize);

    BmBootRegions[1].Size = ALIGN_RANGE_UP((UINTN)TopOfStack, PageSize) -
                            BmBootRegions[1].Address;

    BmBootRegions[1].Flags = 0;
    BmBootBlock.ReservedRegions = (UINTN)BmBootRegions;
    BmBootBlock.ReservedRegionCount = BOOT_MANAGER_RESERVED_REGION_COUNT;

    //
    // Call the main application.
    //

    return BmMain(&BmBootBlock);
}

//
// --------------------------------------------------------- Internal Functions
//
