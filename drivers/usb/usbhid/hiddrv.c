/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hiddrv.c

Abstract:

    This module implements the Minoca driver portion of the USB hid driver.

Author:

    Evan Green 15-Mar-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "usbhidp.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the USB HID allocation tag: UHid.
//

#define USB_HID_ALLOCATION_TAG 0x64694855

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
UsbhidpReallocate (
    PVOID Allocation,
    UINTN Size
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the USB HID library.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    UsbhidReallocate = UsbhidpReallocate;
    return STATUS_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
UsbhidpReallocate (
    PVOID Allocation,
    UINTN Size
    )

/*++

Routine Description:

    This routine is called to allocate, reallocate, or free memory.

Arguments:

    Context - Supplies a pointer to the context passed into the parser.

    Allocation - Supplies an optional pointer to an existing allocation to
        either reallocate or free. If NULL, then a new allocation is being
        requested.

    Size - Supplies the size of the allocation request, in bytes. If this is
        non-zero, then an allocation or reallocation is being requested. If
        this is is 0, then the given memory should be freed.

Return Value:

    Returns a pointer to the allocated memory on success.

    NULL on allocation failure or free.

--*/

{

    if (Size == 0) {
        MmFreeNonPagedPool(Allocation);
        return NULL;
    }

    Allocation = MmReallocatePool(PoolTypeNonPaged,
                                  Allocation,
                                  Size,
                                  USB_HID_ALLOCATION_TAG);

    return Allocation;
}

