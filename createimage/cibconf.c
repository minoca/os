/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    cibconf.c

Abstract:

    This module implements support for the Boot Configuration file in
    createimage.

Author:

    Evan Green 20-Feb-2014

Environment:

    Build

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>
#include <minoca/partlib.h>
#include <minoca/bconflib.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "createimage.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the path to the old PC/AT loader.
//

#define PCAT_LOADER_PATH "system/pcat/loader"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
CipBootConfigurationAllocate (
    UINTN Size
    );

VOID
CipBootConfigurationFree (
    PVOID Memory
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
CiCreateBootConfigurationFile (
    PCI_VOLUME BootVolume,
    PCREATEIMAGE_CONTEXT Context
    )

/*++

Routine Description:

    This routine creates the boot configuration file.

Arguments:

    BootVolume - Supplies a pointer to the boot volume, the volume that
        contains the EFI system partition (or the active partition on a legacy
        system).

    Context - Supplies a pointer to the createimage context.

Return Value:

    Status code.

--*/

{

    BOOT_CONFIGURATION_CONTEXT BootContext;
    PBOOT_ENTRY BootEntry;
    UINTN BytesCompleted;
    PCI_HANDLE FileHandle;
    PPARTITION_INFORMATION InstallPartition;
    PSTR LoaderPath;
    BOOL Result;
    KSTATUS Status;

    FileHandle = NULL;
    RtlZeroMemory(&BootContext, sizeof(BOOT_CONFIGURATION_CONTEXT));
    BootContext.AllocateFunction = CipBootConfigurationAllocate;
    BootContext.FreeFunction = CipBootConfigurationFree;
    Status = BcInitializeContext(&BootContext);
    if (!KSUCCESS(Status)) {
        goto CreateBootConfigurationFileEnd;
    }

    InstallPartition = Context->InstallPartition;
    Status = BcCreateDefaultBootConfiguration(
                                      &BootContext,
                                      Context->PartitionContext.DiskIdentifier,
                                      InstallPartition->Identifier);

    if (!KSUCCESS(Status)) {
        printf("createimage: Failed to create default boot configuration: "
               "%x\n",
               Status);

        goto CreateBootConfigurationFileEnd;
    }

    assert(BootContext.BootEntryCount > 0);

    BootEntry = BootContext.GlobalConfiguration.DefaultBootEntry;
    if ((Context->Options & CREATEIMAGE_OPTION_TARGET_DEBUG) != 0) {
        BootEntry->Flags |= BOOT_ENTRY_FLAG_DEBUG;
    }

    BootEntry->DebugDevice = Context->DebugDeviceIndex;
    if (Context->KernelCommandLine != NULL) {
        BootEntry->KernelArguments = strdup(Context->KernelCommandLine);
    }

    //
    // If not in EFI mode, then change the loader to the old PC/AT loader.
    //

    if ((Context->Options & CREATEIMAGE_OPTION_EFI) == 0) {
        LoaderPath = strdup(PCAT_LOADER_PATH);
        if (LoaderPath == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateBootConfigurationFileEnd;
        }

        free(BootEntry->LoaderPath);
        BootEntry->LoaderPath = LoaderPath;
    }

    Status = BcWriteBootConfigurationFile(&BootContext);
    if (!KSUCCESS(Status)) {
        printf("createimage: Failed to create Boot Configuration: %x.\n",
               Status);

        goto CreateBootConfigurationFileEnd;
    }

    Result = CiOpen(BootVolume,
                    BOOT_CONFIGURATION_ABSOLUTE_PATH,
                    TRUE,
                    &FileHandle);

    if (Result == FALSE) {
        printf("createimage: Failed to open Boot Configuration file at %s.\n",
               BOOT_CONFIGURATION_ABSOLUTE_PATH);

        Status = STATUS_UNSUCCESSFUL;
        goto CreateBootConfigurationFileEnd;
    }

    Result = CiWrite(FileHandle,
                     BootContext.FileData,
                     BootContext.FileDataSize,
                     &BytesCompleted);

    if ((Result == FALSE) || (BytesCompleted != BootContext.FileDataSize)) {
        printf("createimage: Failed to write boot configuration data.\n");
        Status = STATUS_UNSUCCESSFUL;
        goto CreateBootConfigurationFileEnd;
    }

    Status = STATUS_SUCCESS;

CreateBootConfigurationFileEnd:
    if (FileHandle != NULL) {
        CiClose(FileHandle);
    }

    BcDestroyContext(&BootContext);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PVOID
CipBootConfigurationAllocate (
    UINTN Size
    )

/*++

Routine Description:

    This routine is called when the partition library needs to allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    return malloc(Size);
}

VOID
CipBootConfigurationFree (
    PVOID Memory
    )

/*++

Routine Description:

    This routine is called when the partition library needs to free allocated
    memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

{

    free(Memory);
    return;
}

