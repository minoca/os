/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    main.c

Abstract:

    This module implements the EFI main function. This is called by the boot
    library.

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/protocol/loadimg.h>
#include "firmware.h"
#include "bootlib.h"
#include "bootman.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
BmpEfiGetLoadedImageProtocol (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_LOADED_IMAGE_PROTOCOL **LoadedImage
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a fake boot initialization block, since the boot manager is launched
// by the firmware.
//

BOOT_INITIALIZATION_BLOCK BmBootBlock;

EFI_GUID BmLoadedImageProtocolGuid = EFI_LOADED_IMAGE_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
EFI_STATUS
BmEfiApplicationMain (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    )

/*++

Routine Description:

    This routine is the entry point for the EFI Boot Application.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

{

    EFI_LOADED_IMAGE_PROTOCOL *LoadedImage;

    if (SystemTable->ConOut != NULL) {
        SystemTable->ConOut->OutputString(SystemTable->ConOut,
                                          L"Minoca Boot Manager\r\n");
    }

    RtlZeroMemory(&BmBootBlock, sizeof(BOOT_INITIALIZATION_BLOCK));
    BmBootBlock.Version = BOOT_INITIALIZATION_BLOCK_VERSION;
    BmBootBlock.EfiImageHandle = &ImageHandle;
    BmBootBlock.EfiSystemTable = SystemTable;
    BmBootBlock.ApplicationName = "bootmefi.efi";
    BmpEfiGetLoadedImageProtocol(ImageHandle,
                                 SystemTable,
                                 &LoadedImage);

    BmBootBlock.ApplicationArguments = "";
    if (LoadedImage != NULL) {
        BmBootBlock.ApplicationBaseAddress = LoadedImage->ImageBase;
        BmBootBlock.ApplicationSize = LoadedImage->ImageSize;
        if (LoadedImage->LoadOptionsSize != 0) {
            BmBootBlock.ApplicationArguments = LoadedImage->LoadOptions;
        }
    }

    BmBootBlock.ApplicationLowestAddress = BmBootBlock.ApplicationBaseAddress;
    BmMain(&BmBootBlock);
    return EFI_LOAD_ERROR;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
BmpEfiGetLoadedImageProtocol (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable,
    EFI_LOADED_IMAGE_PROTOCOL **LoadedImage
    )

/*++

Routine Description:

    This routine attempts to get the image base address using the loaded image
    protocol.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

    LoadedImage - Supplies a pointer where the loaded image protocol instance
        will be returned on success.

Return Value:

    None.

--*/

{

    EFI_STATUS Status;

    //
    // Call the firmware directly. Normally a save/restore state is needed
    // around this call, but because this is run so early the application
    // state hasn't even been set up yet.
    //

    *LoadedImage = NULL;
    Status = SystemTable->BootServices->HandleProtocol(
                                                    ImageHandle,
                                                    &BmLoadedImageProtocolGuid,
                                                    (VOID **)LoadedImage);

    if (EFI_ERROR(Status)) {
        *LoadedImage = NULL;
    }

    return;
}

