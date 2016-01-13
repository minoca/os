/*++

Copyright (c) 2015 Minoca Corp. All rights reserved.

Module Name:

    sdrk.h

Abstract:

    This header contains definitions for the SD/MMC Rockchip device library.

Author:

    Chris Stevens 16-Jul-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <dev/sd.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_SD_RK_CONTROLLER *PEFI_SD_RK_CONTROLLER;

/*++

Structure Description:

    This structure defines the initialization parameters passed upon creation
    of a new SD Rockchip controller.

Members:

    ControllerBase - Stores a pointer to the base address of the host
        controller registers.

    Voltages - Stores a bitmask of supported voltages. See SD_VOLTAGE_*
        definitions.

    FundamentalClock - Stores the fundamental clock speed in Hertz.

    HostCapabilities - Stores the host controller capability bits See SD_MODE_*
        definitions.

--*/

typedef struct _EFI_SD_RK_INITIALIZATION_BLOCK {
    VOID *ControllerBase;
    UINT32 Voltages;
    UINT32 FundamentalClock;
    UINT32 HostCapabilities;
} EFI_SD_RK_INITIALIZATION_BLOCK, *PEFI_SD_RK_INITIALIZATION_BLOCK;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

PEFI_SD_RK_CONTROLLER
EfiSdRkCreateController (
    PEFI_SD_RK_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine creates a new SD Rockchip controller object.

Arguments:

    Parameters - Supplies a pointer to the parameters to use when creating the
        controller. This can be stack allocated, as the Rockchip SD device
        won't use this memory after this routine returns.

Return Value:

    Returns a pointer to the controller structure on success.

    NULL on allocation failure or if a required parameter was not filled in.

--*/

VOID
EfiSdRkDestroyController (
    PEFI_SD_RK_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys an SD Rockchip controller object.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

EFI_STATUS
EfiSdRkInitializeController (
    PEFI_SD_RK_CONTROLLER Controller,
    BOOLEAN HardReset,
    BOOLEAN SoftReset
    );

/*++

Routine Description:

    This routine resets and initializes the SD Rockchip host controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize.

    HardReset - Supplies a boolean indicating whether or not to perform a
        hardware reset on the controller.

    SoftReset - Supplies a boolean indicating whether or not to perform a soft
        reset on the controller.

Return Value:

    Status code.

--*/

EFI_STATUS
EfiSdRkBlockIoPolled (
    PEFI_SD_RK_CONTROLLER Controller,
    UINT64 BlockOffset,
    UINTN BlockCount,
    VOID *BufferVirtual,
    BOOLEAN Write
    );

/*++

Routine Description:

    This routine performs a block I/O read or write using the CPU and not
    DMA.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockOffset - Supplies the logical block address of the I/O.

    BlockCount - Supplies the number of blocks to read or write.

    BufferVirtual - Supplies the virtual address of the I/O buffer.

    Write - Supplies a boolean indicating if this is a read operation (FALSE)
        or a write operation.

Return Value:

    Status code.

--*/

EFI_STATUS
EfiSdRkGetMediaParameters (
    PEFI_SD_RK_CONTROLLER Controller,
    UINT64 *BlockCount,
    UINT32 *BlockSize
    );

/*++

Routine Description:

    This routine returns information about the media card.

Arguments:

    Controller - Supplies a pointer to the controller.

    BlockCount - Supplies a pointer where the number of blocks in the user
        area of the medium will be returned.

    BlockSize - Supplies a pointer where the block size of the medium will be
        returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NO_MEDIA if there is no card in the slot.

--*/

