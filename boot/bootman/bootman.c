/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootman.c

Abstract:

    This module loads the selected operating system loader into memory and
    jumps to it.

Author:

    Evan Green 21-Feb-2014

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/lib/fat/fat.h>
#include "firmware.h"
#include "bootlib.h"
#include <minoca/lib/basevid.h>
#include "bootman.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_MANAGER_BINARY_NAME_MAX_SIZE 16

#define BOOT_MANAGER_MODULE_BUFFER_SIZE \
    (sizeof(DEBUG_MODULE) + sizeof(BOOT_MANAGER_BINARY_NAME_MAX_SIZE))

#define BOOT_MANAGER_NAME "Minoca Boot Manager"

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BmpLoadBootConfiguration (
    PVOID BootDevice,
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY *SelectedBootEntry
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to enable debugging in the boot manager.
//

BOOL BmDebug = FALSE;

DEBUG_MODULE BmModule;
LIST_ENTRY BmLoadedImageList;

//
// Carve off memory to store the loader module, including its string.
//

UCHAR BmModuleBuffer[BOOT_MANAGER_MODULE_BUFFER_SIZE];

//
// ------------------------------------------------------------------ Functions
//

INT
BmMain (
    PBOOT_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine is the entry point for the boot manager program.

Arguments:

    Parameters - Supplies a pointer to the application parameters.

Return Value:

    On success, this function does not return.

    On failure, this function returns the step number on which it failed. This
    provides an indication as to where in the boot process it failed.

--*/

{

    PCSTR ApplicationName;
    INT ApplicationReturn;
    UINTN BaseDifference;
    BOOT_CONFIGURATION_CONTEXT BootConfigurationContext;
    PBOOT_VOLUME BootDevice;
    PBOOT_ENTRY BootEntry;
    PDEBUG_DEVICE_DESCRIPTION DebugDevice;
    PDEBUG_MODULE DebugModule;
    PLOADED_IMAGE LoaderImage;
    PCSTR LoaderName;
    UINTN LoaderNameSize;
    PBOOT_INITIALIZATION_BLOCK LoaderParameters;
    ULONG LoadFlags;
    ULONG ModuleNameLength;
    PBOOT_VOLUME OsDevice;
    KSTATUS Status;
    ULONG Step;

    BootDevice = NULL;
    DebugDevice = NULL;
    LoaderParameters = NULL;
    Step = 0;

    //
    // Perform just enough firmware initialization to get to the debugger. Not
    // much happens here, as this is all undebuggable.
    //

    Status = FwInitialize(0, Parameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Perform very basic processor initialization, preparing it to take
    // exceptions and use the serial port.
    //

    Step += 1;
    BoInitializeProcessor();
    Step += 1;
    BoHlBootInitialize(&DebugDevice, NULL);
    if (BoFirmwareDebugDevice != NULL) {
        DebugDevice = BoFirmwareDebugDevice;
    }

    Step += 1;
    DebugModule = (PDEBUG_MODULE)BmModuleBuffer;

    //
    // Initialize the debugging subsystem.
    //

    RtlZeroMemory(&BmModuleBuffer, sizeof(BmModuleBuffer));
    ApplicationName = (PVOID)(UINTN)(Parameters->ApplicationName);
    ModuleNameLength = RtlStringLength(ApplicationName) + 1;
    if (ModuleNameLength > BOOT_MANAGER_BINARY_NAME_MAX_SIZE) {
        ModuleNameLength = BOOT_MANAGER_BINARY_NAME_MAX_SIZE;
    }

    DebugModule->StructureSize = sizeof(DEBUG_MODULE) + ModuleNameLength -
                                 (ANYSIZE_ARRAY * sizeof(CHAR));

    RtlStringCopy(DebugModule->BinaryName, ApplicationName, ModuleNameLength);
    DebugModule->LowestAddress =
                          (PVOID)(UINTN)(Parameters->ApplicationLowestAddress);

    DebugModule->Size = Parameters->ApplicationSize;
    BoProductName = BOOT_MANAGER_NAME;
    if (BmDebug != FALSE) {
        Status = KdInitialize(DebugDevice, DebugModule);
        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    //
    // Initialize the firmware layer.
    //

    Step += 1;
    Status = FwInitialize(1, Parameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Mount the boot device.
    //

    Step += 1;
    Status = BoOpenBootVolume(Parameters->DriveNumber,
                              Parameters->PartitionOffset,
                              NULL,
                              &BootDevice);

    if (!KSUCCESS(Status)) {
        FwPrintString(0, 0, "Failed to open boot volume.");
        goto MainEnd;
    }

    //
    // Load the boot configuration information.
    //

    Step += 1;
    Status = BmpLoadBootConfiguration(BootDevice,
                                      &BootConfigurationContext,
                                      &BootEntry);

    if (!KSUCCESS(Status)) {
        FwPrintString(0, 0, "Failed to load Boot Configuration.");
        goto MainEnd;
    }

    //
    // Close the boot volume and open the OS volume. It is possible these are
    // the same.
    //

    Step += 1;
    Status = BoCloseVolume(BootDevice);
    BootDevice = NULL;
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    Step += 1;
    if (BootEntry == NULL) {
        FwPrintString(0, 0, "No boot entry selected.");
        Status = STATUS_NO_DATA_AVAILABLE;
        goto MainEnd;
    }

    Step += 1;
    Status = BoOpenVolume(BootEntry->PartitionId, &OsDevice);
    if (!KSUCCESS(Status)) {
        FwPrintString(0, 0, "Failed to open OS volume.");
        goto MainEnd;
    }

    //
    // Load the loader.
    //

    Step += 1;
    Status = BmpInitializeImageSupport(OsDevice, BootEntry);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderName = BootEntry->LoaderPath;
    LoaderNameSize = RtlStringLength(BootEntry->LoaderPath);
    LoadFlags = IMAGE_LOAD_FLAG_IGNORE_INTERPRETER |
                IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE |
                IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS |
                IMAGE_LOAD_FLAG_BIND_NOW;

    Status = ImLoad(&BmLoadedImageList,
                    BootEntry->LoaderPath,
                    NULL,
                    NULL,
                    NULL,
                    LoadFlags,
                    &LoaderImage,
                    NULL);

    if (!KSUCCESS(Status)) {
        FwPrintString(0, 0, "Failed to load OS loader.");
        goto MainEnd;
    }

    //
    // Initialize the boot parameters.
    //

    Step += 1;
    LoaderParameters = BoAllocateMemory(sizeof(BOOT_INITIALIZATION_BLOCK));
    if (LoaderParameters == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MainEnd;
    }

    Step += 1;
    RtlZeroMemory(LoaderParameters, sizeof(BOOT_INITIALIZATION_BLOCK));
    LoaderParameters->Version = BOOT_INITIALIZATION_BLOCK_VERSION;
    LoaderParameters->BootConfigurationFile =
                                    (UINTN)(BootConfigurationContext.FileData);

    LoaderParameters->BootConfigurationFileSize =
                                         BootConfigurationContext.FileDataSize;

    LoaderParameters->BootEntryId = BootEntry->Id;
    LoaderParameters->BootEntryFlags = BootEntry->Flags;
    LoaderParameters->StackTop = Parameters->StackTop;
    LoaderParameters->StackSize = Parameters->StackSize;
    LoaderParameters->Flags = Parameters->Flags |
                              BOOT_INITIALIZATION_FLAG_SCREEN_CLEAR;

    if (LoaderImage->Format == ImageElf64) {
        LoaderParameters->Flags |= BOOT_INITIALIZATION_FLAG_64BIT;
    }

    BaseDifference = LoaderImage->BaseDifference;

    //
    // Set the file name and base address of the loader.
    //

    ApplicationName = RtlStringFindCharacterRight(LoaderName,
                                                  '/',
                                                  LoaderNameSize);

    if (ApplicationName == NULL) {
        ApplicationName = LoaderName;

    } else {
        ApplicationName += 1;
    }

    LoaderParameters->ApplicationName = (UINTN)ApplicationName;
    LoaderParameters->ApplicationLowestAddress =
                   (UINTN)LoaderImage->PreferredLowestAddress + BaseDifference;

    LoaderParameters->ApplicationSize = LoaderImage->Size;
    LoaderParameters->ApplicationArguments = (UINTN)BootEntry->LoaderArguments;
    Status = BmpFwInitializeBootBlock(LoaderParameters, OsDevice);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    Step += 1;
    Status = BoCloseVolume(OsDevice);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    Step += 1;
    KdDisconnect();

    //
    // Launch the boot application. Hopefully this does not return.
    //

    Step += 1;
    ApplicationReturn = BmpFwTransferToBootApplication(LoaderParameters,
                                                       LoaderImage->EntryPoint);

    Step += 1;

    //
    // The loader prints on the first two lines, so leave those alone.
    //

    FwPrintString(0, 3, "Boot Application returned ");
    FwPrintHexInteger(26, 3, ApplicationReturn);

    //
    // Unload the image.
    //

    ImImageReleaseReference(LoaderImage);
    LoaderImage = NULL;

    //
    // Destroy the initialization block.
    //

    if (LoaderParameters != NULL) {
        if (LoaderParameters->ReservedRegions != (UINTN)NULL) {
            BoFreeMemory((PVOID)(UINTN)(LoaderParameters->ReservedRegions));
        }

        BoFreeMemory(LoaderParameters);
        LoaderParameters = NULL;
    }

    Status = STATUS_SUCCESS;

MainEnd:

    //
    // The loader prints on the first two lines, and the "application returned"
    // message occurs on the third, so start on the fourth.
    //

    FwPrintString(0, 4, "Boot Manager Failed: ");
    FwPrintHexInteger(21, 4, Status);
    FwPrintString(0, 5, "Step: ");
    FwPrintInteger(6, 5, Step);
    FwDestroy();
    return Step;
}

PVOID
BoExpandHeap (
    PMEMORY_HEAP Heap,
    UINTN Size,
    UINTN Tag
    )

/*++

Routine Description:

    This routine is called when the heap wants to expand and get more space.

Arguments:

    Heap - Supplies a pointer to the heap to allocate from.

    Size - Supplies the size of the allocation request, in bytes.

    Tag - Supplies a 32-bit tag to associate with this allocation for debugging
        purposes. These are usually four ASCII characters so as to stand out
        when a poor developer is looking at a raw memory dump. It could also be
        a return address.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

{

    ULONG AllocationSize;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID PhysicalPointer;
    KSTATUS Status;

    PhysicalPointer = NULL;
    if (Size == 0) {
        return NULL;
    }

    PageSize = MmPageSize();

    //
    // Attempt to allocate new pages to satisfy the allocation.
    //

    AllocationSize = ALIGN_RANGE_UP(Size, PageSize);
    Status = FwAllocatePages(&PhysicalAddress,
                             AllocationSize,
                             PageSize,
                             MemoryTypeLoaderTemporary);

    if (!KSUCCESS(Status)) {
        goto ExpandHeapEnd;
    }

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);

    PhysicalPointer = (PVOID)(UINTN)PhysicalAddress;

ExpandHeapEnd:
    return PhysicalPointer;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BmpLoadBootConfiguration (
    PVOID BootDevice,
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY *SelectedBootEntry
    )

/*++

Routine Description:

    This routine loads and read the boot configuration information.

Arguments:

    BootDevice - Supplies the open handle to the boot partition.

    Context - Supplies a pointer where the initialized boot configuration
        context will be returned.

    SelectedBootEntry - Supplies a pointer where a pointer to the selected
        boot entry will be returned on success.

Return Value:

    Status code.

--*/

{

    FILE_PROPERTIES DirectoryProperties;
    PVOID FileData;
    UINTN FileDataSize;
    ULONGLONG ModificationDate;
    KSTATUS Status;

    *SelectedBootEntry = NULL;
    FileData = NULL;
    Status = BoLookupPath(BootDevice,
                          NULL,
                          BOOT_CONFIGURATION_FILE_PATH,
                          &DirectoryProperties);

    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    Status = BoLoadFile(BootDevice,
                        &(DirectoryProperties.FileId),
                        BOOT_CONFIGURATION_FILE_NAME,
                        &FileData,
                        &FileDataSize,
                        &ModificationDate);

    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    //
    // Initialize the boot configuration context.
    //

    RtlZeroMemory(Context, sizeof(BOOT_CONFIGURATION_CONTEXT));
    Context->AllocateFunction = BoAllocateMemory;
    Context->FreeFunction = BoFreeMemory;
    Context->FileData = FileData;
    Context->FileDataSize = FileDataSize;
    Status = BcInitializeContext(Context);
    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    //
    // Read and parse the boot configuration file data.
    //

    Status = BcReadBootConfigurationFile(Context);
    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    //
    // If there's no boot once entry, then fill out the default and return.
    //

    *SelectedBootEntry = Context->GlobalConfiguration.DefaultBootEntry;
    if (Context->GlobalConfiguration.BootOnce == NULL) {
        Status = STATUS_SUCCESS;
        goto LoadBootConfigurationEnd;
    }

    //
    // There is a boot once entry. Save it as the selected boot entry, then
    // work to write out the boot configuration with the boot once field
    // cleared.
    //

    *SelectedBootEntry = Context->GlobalConfiguration.BootOnce;
    Context->GlobalConfiguration.BootOnce = NULL;
    Status = BcWriteBootConfigurationFile(Context);
    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    Status = BoStoreFile(BootDevice,
                         DirectoryProperties.FileId,
                         BOOT_CONFIGURATION_FILE_NAME,
                         sizeof(BOOT_CONFIGURATION_FILE_NAME),
                         Context->FileData,
                         Context->FileDataSize,
                         ModificationDate);

    if (!KSUCCESS(Status)) {
        goto LoadBootConfigurationEnd;
    }

    Status = STATUS_SUCCESS;

LoadBootConfigurationEnd:
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("Failed to load Boot Configuration: %d.\n", Status);
        if (FileData != NULL) {
            BoFreeMemory(FileData);
        }
    }

    return Status;
}

