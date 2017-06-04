/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loader.c

Abstract:

    This module loads the kernel into memory, performs the initialization steps
    necessary to start the kernel, and then transfers execution to it.

Author:

    Evan Green 29-Jul-2012

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/fw/smbios.h>
#include <minoca/lib/fat/fat.h>
#include "firmware.h"
#include "bootlib.h"
#include "dbgport.h"
#include <minoca/lib/basevid.h>
#include "paging.h"
#include "loader.h"

//
// ---------------------------------------------------------------- Definitions
//

#define LOADER_BINARY_NAME_MAX_SIZE 16
#define LOADER_MODULE_BUFFER_SIZE \
    (sizeof(DEBUG_MODULE) + sizeof(LOADER_BINARY_NAME_MAX_SIZE))

#define LOADER_NAME "Minoca Boot Loader"

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
BopMapAcpiTables (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PBOOT_VOLUME BootDevice,
    FILE_ID ConfigurationDirectory,
    PFIRMWARE_TABLE_DIRECTORY *FirmwareTables
    );

VOID
BopAcpiMemoryIteratorRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

KSTATUS
BopLoadDrivers (
    PLOADER_BUFFER BootDriverFile
    );

KSTATUS
BopMapNeededHardwareRegions (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
BopReadBootConfiguration (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_CONFIGURATION_CONTEXT *NewContext,
    PBOOT_ENTRY *BootEntry
    );

KSTATUS
BopGetConfigurationDirectory (
    PBOOT_VOLUME BootDevice,
    PFILE_ID DirectoryFileId
    );

VOID
BopSetBootTime (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

KSTATUS
BopAddSystemMemoryResource (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    UINTN Size,
    SYSTEM_MEMORY_RESOURCE_TYPE Type,
    ULONG MapFlags
    );

KSTATUS
BopAddMmInitMemory (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

VOID
BopMmInitMemoryMapIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    );

KSTATUS
BopAllocateKernelBuffer (
    UINTN Size,
    ULONG MapFlags,
    PPHYSICAL_ADDRESS PhysicalAddress,
    PLOADER_BUFFER BufferOut
    );

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the iteration context when mapping regions of the
    memory map marked for ACPI tables.

Members:

    RsdtTableEntry - Stores the array of table pointers in the RSDT.

    RsdtTableCount - Stores the number of entries in the RSDT table.

    TableEntry - Stores the array of pointers to kernel addresses of ACPI
        tables.

    BootTableEntry - Stores the array of pointers to boot addresses of ACPI
        tables.

    TableDirectory - Stores a pointer to the firmware table directory.

    DsdtTable - Stores a pointer to the DSDT table.

--*/

typedef struct _LOADER_ACPI_MEMORY_ITERATOR {
    PULONG RsdtTableEntry;
    UINTN RsdtTableCount;
    PVOID *TableEntry;
    PVOID *BootTableEntry;
    PFIRMWARE_TABLE_DIRECTORY TableDirectory;
    PVOID DsdtTable;
    KSTATUS Status;
} LOADER_ACPI_MEMORY_ITERATOR, *PLOADER_ACPI_MEMORY_ITERATOR;

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to force-enable debugging the boot loader.
//

BOOL BoForceDebug = FALSE;

DEBUG_MODULE BoLoaderModule;
extern MEMORY_DESCRIPTOR_LIST BoVirtualMap;

//
// Store a pointer to the firmware tables.
//

PFIRMWARE_TABLE_DIRECTORY BoFirmwareTables;
LIST_ENTRY BoLoadedImageList;

//
// Carve off memory to store the loader module, including its string.
//

UCHAR BoLoaderModuleBuffer[LOADER_MODULE_BUFFER_SIZE];

//
// Piggyback off of the image support's system directory file ID.
//

extern FILE_ID BoSystemDirectoryId;

//
// ------------------------------------------------------------------ Functions
//

__USED
INT
BoMain (
    PBOOT_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine is the entry point for the boot loader program.

Arguments:

    Parameters - Supplies a pointer to the application initialization
        parameters.

Return Value:

    On success, this function does not return.

    On failure, this function returns the step number on which it failed. This
    provides an indication as to where in the boot process it failed.

--*/

{

    UINTN AlignedLoaderSize;
    PVOID AlignedLoaderStart;
    PBOOT_CONFIGURATION_CONTEXT BootConfiguration;
    PBOOT_VOLUME BootDevice;
    LOADER_BUFFER BootDriversFile;
    PBOOT_ENTRY BootEntry;
    FILE_ID ConfigurationDirectory;
    PDEBUG_DEVICE_DESCRIPTION DebugDevice;
    PLOADED_IMAGE KernelImage;
    PDEBUG_MODULE KernelModule;
    PKERNEL_INITIALIZATION_BLOCK KernelParameters;
    PCSTR KernelPath;
    PHYSICAL_ADDRESS KernelStackPhysical;
    PDEBUG_MODULE LoaderModule;
    ULONG LoaderModuleNameLength;
    ULONG LoaderStep;
    ULONG LoadFlags;
    PHYSICAL_ADDRESS PageDirectoryPhysical;
    UINTN PageOffset;
    UINTN PageSize;
    UINTN RoundedStackMaximum;
    UINTN RoundedStackMinimum;
    UINTN StackBottom;
    PVOID StackEnd;
    BOOL StackOutsideImage;
    KSTATUS Status;

    BootConfiguration = NULL;
    BootDevice = NULL;
    BootEntry = NULL;
    DebugDevice = NULL;
    PageSize = MmPageSize();
    StackOutsideImage = FALSE;

    //
    // Perform very early firmware initialization before the processor
    // initialization clobbers any processor state.
    //

    LoaderStep = 0;
    Status = FwInitialize(0, Parameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Perform very basic processor initialization, preparing it to take
    // exceptions and use the serial port.
    //

    BoInitializeProcessor();
    LoaderStep += 1;
    BoHlBootInitialize(&DebugDevice, BoGetAcpiTable);
    if (BoFirmwareDebugDevice != NULL) {
        DebugDevice = BoFirmwareDebugDevice;
    }

    LoaderStep += 1;
    LoaderModule = (PDEBUG_MODULE)BoLoaderModuleBuffer;

    //
    // Initialize the debugging subsystem.
    //

    RtlZeroMemory(&BoLoaderModuleBuffer, sizeof(BoLoaderModuleBuffer));
    LoaderModuleNameLength =
              RtlStringLength((PVOID)(UINTN)(Parameters->ApplicationName)) + 1;

    if (LoaderModuleNameLength > LOADER_BINARY_NAME_MAX_SIZE) {
        LoaderModuleNameLength = LOADER_BINARY_NAME_MAX_SIZE;
    }

    LoaderModule->StructureSize = sizeof(DEBUG_MODULE) +
                                  LoaderModuleNameLength -
                                  (ANYSIZE_ARRAY * sizeof(CHAR));

    RtlStringCopy(LoaderModule->BinaryName,
                  (PVOID)(UINTN)(Parameters->ApplicationName),
                  LoaderModuleNameLength);

    LoaderModule->LowestAddress =
                          (PVOID)(UINTN)(Parameters->ApplicationLowestAddress);

    LoaderModule->Size = Parameters->ApplicationSize;
    BoProductName = LOADER_NAME;
    if ((BoForceDebug != FALSE) ||
        (Parameters->BootEntryFlags & BOOT_ENTRY_FLAG_BOOT_DEBUG) != 0) {

        Status = KdInitialize(DebugDevice, LoaderModule);
        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    //
    // Initialize the firmware layer.
    //

    LoaderStep += 1;
    Status = FwInitialize(1, Parameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;

    //
    // Initialize paging structures.
    //

    Status = BoInitializePagingStructures(&PageDirectoryPhysical);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    Status = BoArchMapNeededHardwareRegions();
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    Status = BoFwMapKnownRegions(0, NULL);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;

    //
    // Identity map the loader and its stack into kernel address space.
    //

    AlignedLoaderStart = (PVOID)(UINTN)ALIGN_RANGE_DOWN(
                                          Parameters->ApplicationLowestAddress,
                                          PageSize);

    PageOffset = Parameters->ApplicationLowestAddress -
                 (UINTN)AlignedLoaderStart;

    AlignedLoaderSize = ALIGN_RANGE_UP(Parameters->ApplicationSize + PageOffset,
                                       PageSize);

    Status = BoMapPhysicalAddress(&AlignedLoaderStart,
                                  (PHYSICAL_ADDRESS)(UINTN)AlignedLoaderStart,
                                  AlignedLoaderSize,
                                  MAP_FLAG_EXECUTE,
                                  MemoryTypeLoaderTemporary);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;

    //
    // Parse the boot configuration file.
    //

    if (Parameters->BootConfigurationFileSize != 0) {
        Status = BopReadBootConfiguration(Parameters,
                                          &BootConfiguration,
                                          &BootEntry);

        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    LoaderStep += 1;

    //
    // Determine if the stack is inside the loader image itself (like in a big
    // global).
    //

    StackBottom = Parameters->StackTop - Parameters->StackSize;
    StackOutsideImage = TRUE;
    if ((StackBottom >= Parameters->ApplicationLowestAddress) &&
        (Parameters->StackTop <
         Parameters->ApplicationLowestAddress + Parameters->ApplicationSize)) {

        StackOutsideImage = FALSE;
    }

    if (StackOutsideImage != FALSE) {
        RoundedStackMinimum = ALIGN_RANGE_DOWN(StackBottom, PageSize);
        RoundedStackMaximum = ALIGN_RANGE_UP(Parameters->StackTop, PageSize);
        Status = BoMapPhysicalAddress((PVOID)&RoundedStackMinimum,
                                      RoundedStackMinimum,
                                      RoundedStackMaximum - RoundedStackMinimum,
                                      0,
                                      MemoryTypeLoaderTemporary);

        if (!KSUCCESS(Status)) {
            goto MainEnd;
        }
    }

    //
    // Create and initialize the kernel initializaton block.
    //

    LoaderStep += 1;
    KernelParameters = BoAllocateMemory(sizeof(KERNEL_INITIALIZATION_BLOCK));
    if (KernelParameters == NULL) {
        Status = STATUS_NO_MEMORY;
        goto MainEnd;
    }

    //
    // Initialize the parameter block.
    //

    RtlZeroMemory(KernelParameters, sizeof(KERNEL_INITIALIZATION_BLOCK));
    INITIALIZE_LIST_HEAD(&(KernelParameters->SystemResourceListHead));
    KernelParameters->Version = KERNEL_INITIALIZATION_BLOCK_VERSION;
    KernelParameters->Size = sizeof(KERNEL_INITIALIZATION_BLOCK);
    KernelParameters->MemoryMap = &BoMemoryMap;
    KernelParameters->VirtualMap = &BoVirtualMap;
    KernelParameters->BootEntry = BootEntry;

    //
    // Map the initial page table staging area. It doesn't matter where this
    // gets mapped to, the only important thing here is that a page table get
    // allocated and initialized, and that page table get mapped itself.
    //

    LoaderStep += 1;
    Status = BoCreatePageTableStage(PageDirectoryPhysical,
                                    &(KernelParameters->PageTableStage));

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Mount the boot device.
    //

    LoaderStep += 1;
    Status = BoOpenBootVolume(Parameters->DriveNumber,
                              Parameters->PartitionOffset,
                              BootEntry,
                              &BootDevice);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Initialize support to load images.
    //

    LoaderStep += 1;
    Status = BoInitializeImageSupport(BootDevice, BootEntry);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Open up the configuration directory, which is currently the root
    // directory.
    //

    LoaderStep += 1;
    Status = BopGetConfigurationDirectory(BootDevice, &ConfigurationDirectory);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Load the kernel.
    //

    LoaderStep += 1;
    KernelPath = DEFAULT_KERNEL_BINARY_PATH;
    if (BootEntry != NULL) {
        KernelPath = BootEntry->KernelPath;
    }

    LoadFlags = IMAGE_LOAD_FLAG_IGNORE_INTERPRETER |
                IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE |
                IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS |
                IMAGE_LOAD_FLAG_BIND_NOW;

    Status = ImLoad(&BoLoadedImageList,
                    KernelPath,
                    NULL,
                    NULL,
                    NULL,
                    LoadFlags,
                    &KernelImage,
                    NULL);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    KernelModule = KernelImage->DebuggerModule;
    if (KernelModule == NULL) {
        goto MainEnd;
    }

    KernelParameters->KernelModule = KernelModule;

    //
    // Allocate and map a stack for the kernel.
    //

    LoaderStep += 1;

    ASSERT((DEFAULT_KERNEL_STACK_SIZE & (PageSize - 1)) == 0);

    Status = FwAllocatePages(&KernelStackPhysical,
                             DEFAULT_KERNEL_STACK_SIZE,
                             PageSize,
                             MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    KernelParameters->KernelStack.Buffer = (PVOID)-1;
    KernelParameters->KernelStack.Size = DEFAULT_KERNEL_STACK_SIZE;
    Status = BoMapPhysicalAddress(&(KernelParameters->KernelStack.Buffer),
                                  KernelStackPhysical,
                                  KernelParameters->KernelStack.Size,
                                  MAP_FLAG_GLOBAL,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Map the page directory and self-map the page tables.
    //

    KernelParameters->PageDirectory = (PVOID)-1;
    Status = BoMapPagingStructures(PageDirectoryPhysical,
                                   &(KernelParameters->PageDirectory),
                                   &(KernelParameters->PageTables));

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Map ACPI Tables.
    //

    LoaderStep += 1;
    Status = BopMapAcpiTables(&BoMemoryMap,
                              BootDevice,
                              ConfigurationDirectory,
                              &(KernelParameters->FirmwareTables));

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Load the boot driver list, device to driver database, and boot device
    // map into memory.
    //

    LoaderStep += 1;
    Status = BoLoadFile(BootDevice,
                        &ConfigurationDirectory,
                        BOOT_DRIVER_FILE,
                        &(BootDriversFile.Buffer),
                        &(BootDriversFile.Size),
                        NULL);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    Status = BoLoadFile(BootDevice,
                        &ConfigurationDirectory,
                        DEVICE_TO_DRIVER_FILE,
                        &(KernelParameters->DeviceToDriverFile.Buffer),
                        &(KernelParameters->DeviceToDriverFile.Size),
                        NULL);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;
    Status = BoLoadFile(BootDevice,
                        &ConfigurationDirectory,
                        DEVICE_MAP_FILE,
                        &(KernelParameters->DeviceMapFile.Buffer),
                        &(KernelParameters->DeviceMapFile.Size),
                        NULL);

    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Load all boot drivers.
    //

    LoaderStep += 1;
    Status = BopLoadDrivers(&BootDriversFile);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Dismount the boot volume.
    //

    LoaderStep += 1;
    Status = BoCloseVolume(BootDevice);
    BootDevice = NULL;
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Map any hardware regions.
    //

    LoaderStep += 1;
    Status = BopMapNeededHardwareRegions(KernelParameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Attempt to measure the cycle counter frequency so the kernel has an
    // early stall source.
    //

    BoArchMeasureCycleCounter(KernelParameters);

    //
    // Set up any resources needed for the kernel debug transport.
    //

    LoaderStep += 1;
    BoSetUpKernelDebugTransport(KernelParameters);

    //
    // Corral the loaded image information and stick in the parameter block.
    //

    LoaderStep += 1;
    MOVE_LIST(&BoLoadedImageList, &(KernelParameters->ImageList));
    INITIALIZE_LIST_HEAD(&BoLoadedImageList);
    KernelParameters->LoaderModule = LoaderModule;

    //
    // Allocate some memory for the kernel memory manager to bootstrap with.
    //

    Status = BopAddMmInitMemory(KernelParameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    LoaderStep += 1;

    //
    // Get the boot time as close as possible to the actual kernel launch time
    // while still in boot services.
    //

    BopSetBootTime(KernelParameters);

    //
    // Exit boot services. If the firmware is providing the debug device, then
    // shut down the debugger before exiting boot services.
    //

    LoaderStep += 1;
    if (DebugDevice == BoFirmwareDebugDevice) {
        KdDisconnect();
    }

    LoaderStep += 1;
    Status = BoFwPrepareForKernelLaunch(KernelParameters);
    if (!KSUCCESS(Status)) {
        goto MainEnd;
    }

    //
    // Prevent excessive SMI activity during early kernel init by disabling
    // legacy interrupts.
    //

    LoaderStep += 1;
    BopDisableLegacyInterrupts();

    //
    // Turn on paging.
    //

    LoaderStep += 1;
    BoEnablePaging();
    LoaderStep += 1;
    MmMdPrintMdl(&BoMemoryMap);
    RtlDebugPrint("Virtual Memory Map\n");
    MmMdPrintMdl(&BoVirtualMap);

    //
    // Stop the debugger.
    //

    LoaderStep += 1;
    if (DebugDevice != BoFirmwareDebugDevice) {
        KdDisconnect();
    }

    LoaderStep += 1;

    //
    // Transfer execution to the kernel. This should not return.
    //

    StackEnd = KernelParameters->KernelStack.Buffer +
               KernelParameters->KernelStack.Size;

    BoTransferToKernelAsm(KernelParameters, KernelModule->EntryPoint, StackEnd);
    LoaderStep += 1;
    Status = STATUS_SUCCESS;

MainEnd:
    RtlDebugPrint("Loader Failed: Step 0x%x, Status %d\n", LoaderStep, Status);
    FwPrintString(0, 0, "Loader Failed: ");
    FwPrintHexInteger(15, 0, Status);
    FwPrintString(0, 1, "Step: ");
    FwPrintInteger(6, 1, LoaderStep);
    FwDestroy();
    return LoaderStep;
}

KSTATUS
BoLoadAndMapFile (
    PBOOT_VOLUME Volume,
    PFILE_ID Directory,
    PSTR FileName,
    PVOID *FilePhysical,
    PVOID *FileVirtual,
    PUINTN FileSize,
    MEMORY_TYPE VirtualType
    )

/*++

Routine Description:

    This routine loads a file into memory and maps it into the kernel's
    virtual address space.

Arguments:

    Volume - Supplies a pointer to the mounted volume to read the file from.

    Directory - Supplies an optional pointer to the ID of the directory to
        start path traversal from. If NULL, the root of the volume will be used.

    FileName - Supplies the name of the file to load.

    FilePhysical - Supplies a pointer where the file buffer's physical address
        will be returned. This routine will allocate the buffer to hold the
        file. If this parameter and the virtual parameter are NULL, the status
        code will reflect whether or not the file could be opened, but the file
        contents will not be loaded.

    FileVirtual - Supplies a pointer where the file buffer's virtual address
        will be returned. If this parameter and the physical parameter are
        NULL, the status code will reflect whether or not the file could be
        opened, but the file contents will not be loaded.

    FileSize - Supplies a pointer where the size of the file in bytes will be
        returned.

    VirtualType - Supplies the memory type to use for the virtual allocation.
        The physical allocation type will be loader permanent.

Return Value:

    Status code.

--*/

{

    UINTN AlignedSize;
    PVOID FinalPages;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    PVOID PhysicalBuffer;
    UINTN Size;
    KSTATUS Status;
    PVOID VirtualBuffer;

    FinalPages = NULL;
    PageSize = MmPageSize();
    PhysicalBuffer = NULL;
    Size = 0;
    VirtualBuffer = NULL;
    Status = BoLoadFile(Volume,
                        Directory,
                        FileName,
                        &PhysicalBuffer,
                        &Size,
                        NULL);

    if (!KSUCCESS(Status)) {
        goto LoadAndMapFileEnd;
    }

    //
    // If no PA and no VA is requested, go to the end.
    //

    if (FileVirtual == NULL) {
        goto LoadAndMapFileEnd;
    }

    //
    // Allocate loader permanent pages.
    //

    AlignedSize = ALIGN_RANGE_UP(Size, PageSize);
    Status = FwAllocatePages(&PhysicalAddress,
                             AlignedSize,
                             PageSize,
                             MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto LoadAndMapFileEnd;
    }

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);

    FinalPages = (PVOID)(UINTN)PhysicalAddress;
    RtlCopyMemory(FinalPages, PhysicalBuffer, Size);

    //
    // Map the address.
    //

    VirtualBuffer = (PVOID)-1;
    Status = BoMapPhysicalAddress(&VirtualBuffer,
                                  (PHYSICAL_ADDRESS)(UINTN)FinalPages,
                                  AlignedSize,
                                  MAP_FLAG_GLOBAL,
                                  VirtualType);

    if (!KSUCCESS(Status)) {
        goto LoadAndMapFileEnd;
    }

    ASSERT(VirtualBuffer >= KERNEL_VA_START);

    Status = STATUS_SUCCESS;

LoadAndMapFileEnd:
    if (PhysicalBuffer != NULL) {
        BoFreeMemory(PhysicalBuffer);
    }

    if (FileSize != NULL) {
        *FileSize = Size;
    }

    if (FilePhysical != NULL) {
        *FilePhysical = FinalPages;
    }

    if (FileVirtual != NULL) {
        *FileVirtual = VirtualBuffer;
    }

    return Status;
}

PVOID
BoGetAcpiTable (
    ULONG Signature,
    PVOID PreviousTable
    )

/*++

Routine Description:

    This routine attempts to find an ACPI description table with the given
    signature. This routine does not validate the checksum of the table.

Arguments:

    Signature - Supplies the signature of the desired table.

    PreviousTable - Supplies an optional pointer to the table to start the
        search from.

Return Value:

    Returns a pointer to the beginning of the header to the table if the table
    was found, or NULL if the table could not be located.

--*/

{

    PDESCRIPTION_HEADER Table;
    PVOID *TableEntry;
    LONG TableIndex;

    //
    // Return NULL if someone is asking for firmware tables before they're
    // set up.
    //

    if (BoFirmwareTables == NULL) {
        return NULL;
    }

    //
    // Search the list of pointers, but do it backwards. This runs on the
    // assumption that if there are two tables in the firmware, the later one
    // is the better one. It also allows the test tables to override existing
    // firmware tables.
    //

    TableEntry = (PVOID *)(BoFirmwareTables + 1);
    for (TableIndex = BoFirmwareTables->TableCount - 1;
         TableIndex >= 0;
         TableIndex -= 1) {

        Table = (PDESCRIPTION_HEADER)(TableEntry[TableIndex]);

        //
        // If the caller searched with a previous table, skip anything up to
        // and including that table.
        //

        if (PreviousTable != NULL) {
            if (Table == PreviousTable) {
                PreviousTable = NULL;
            }

            continue;
        }

        if (Table->Signature == Signature) {
            return Table;
        }
    }

    return NULL;
}

KSTATUS
BoAddFirmwareTable (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters,
    PVOID Table
    )

/*++

Routine Description:

    This routine adds a firmware configuration table to the loader's list of
    tables.

Arguments:

    KernelParameters - Supplies a pointer to the kernel initialization
        parameters.

    Table - Supplies a pointer to the table to add.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

    STATUS_TOO_EARLY if firmware tables are not yet available.

--*/

{

    UINTN AllocationSize;
    PVOID NewAllocation;
    ULONG NewCount;
    PVOID *Tables;

    if (BoFirmwareTables == NULL) {
        return STATUS_TOO_EARLY;
    }

    //
    // Reallocate the loader's array.
    //

    NewCount = BoFirmwareTables->TableCount + 1;
    AllocationSize = sizeof(FIRMWARE_TABLE_DIRECTORY) +
                     (NewCount * sizeof(PVOID));

    NewAllocation = BoAllocateMemory(AllocationSize);
    if (NewAllocation == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(NewAllocation,
                  BoFirmwareTables,
                  AllocationSize - sizeof(PVOID));

    BoFreeMemory(BoFirmwareTables);
    BoFirmwareTables = NewAllocation;
    Tables = (PVOID *)(BoFirmwareTables + 1);
    Tables[NewCount - 1] = Table;
    BoFirmwareTables->TableCount = NewCount;

    //
    // Reallocate the kernel's array.
    //

    ASSERT(KernelParameters->FirmwareTables->TableCount + 1 ==
           BoFirmwareTables->TableCount);

    NewAllocation = BoAllocateMemory(AllocationSize);
    if (NewAllocation == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlCopyMemory(NewAllocation,
                  KernelParameters->FirmwareTables,
                  AllocationSize - sizeof(PVOID));

    BoFreeMemory(KernelParameters->FirmwareTables);
    KernelParameters->FirmwareTables = NewAllocation;
    Tables = (PVOID *)(KernelParameters->FirmwareTables + 1);
    Tables[NewCount - 1] = Table;
    KernelParameters->FirmwareTables->TableCount = NewCount;
    return STATUS_SUCCESS;
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
    PVOID VirtualAddress;

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

    //
    // Identity map those pages into kernel address space.
    //

    ASSERT((UINTN)PhysicalAddress == PhysicalAddress);

    VirtualAddress = (PVOID)(UINTN)PhysicalAddress;
    Status = BoMapPhysicalAddress(&VirtualAddress,
                                  PhysicalAddress,
                                  AllocationSize,
                                  0,
                                  MemoryTypeLoaderTemporary);

    if (!KSUCCESS(Status)) {
        goto ExpandHeapEnd;
    }

    PhysicalPointer = (PVOID)(UINTN)PhysicalAddress;

ExpandHeapEnd:
    return PhysicalPointer;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
BopMapAcpiTables (
    PMEMORY_DESCRIPTOR_LIST MemoryMap,
    PBOOT_VOLUME BootDevice,
    FILE_ID ConfigurationDirectory,
    PFIRMWARE_TABLE_DIRECTORY *FirmwareTables
    )

/*++

Routine Description:

    This routine maps any areas mentioned by the BIOS as ACPI tables into
    kernel address space.

Arguments:

    MemoryMap - Supplies a pointer to the physical memory map.

    BootDevice - Supplies a pointer to the boot device.

    ConfigurationDirectory - Supplies the file ID of the configuration
        directory.

    FirmwareTables - Supplies a pointer to the firwmare table list.

Return Value:

    Status code.

--*/

{

    LOADER_ACPI_MEMORY_ITERATOR AcpiContext;
    ULONG AllocationSize;
    PVOID *BootTableEntry;
    PDESCRIPTION_HEADER DsdtTable;
    PFADT FadtTable;
    PDESCRIPTION_HEADER Header;
    PVOID NewTable;
    PRSDP RsdpTable;
    PRSDT RsdtTable;
    ULONG RsdtTableCount;
    PULONG RsdtTableEntry;
    PSMBIOS_ENTRY_POINT SmbiosTable;
    KSTATUS Status;
    ULONG TableCount;
    PFIRMWARE_TABLE_DIRECTORY TableDirectory;
    PVOID *TableEntry;
    ULONG TableIndex;
    ULONG TestTablesExaminedBytes;
    PVOID TestTablesPhysical;
    UINTN TestTablesSize;
    PVOID TestTablesVirtual;

    DsdtTable = NULL;
    FadtTable = NULL;
    RsdtTable = NULL;
    RsdtTableCount = 0;
    RsdtTableEntry = NULL;
    TableCount = 0;
    TableDirectory = NULL;
    TestTablesPhysical = NULL;

    //
    // Locate the RSDP.
    //

    RsdpTable = FwFindRsdp();
    if (RsdpTable != NULL) {

        //
        // Use the RSDP to locate the RSDT, and count the number of tables in
        // the RSDT.
        //

        RsdtTable = (PRSDT)(UINTN)RsdpTable->RsdtAddress;
        RsdtTableCount = (RsdtTable->Header.Length -
                          sizeof(DESCRIPTION_HEADER)) / sizeof(ULONG);

        if (RsdtTableCount == 0) {
            Status = STATUS_INVALID_DIRECTORY;
            goto MapAcpiTablesEnd;
        }

        RsdtTableEntry = (PULONG)&(RsdtTable->Entries);

        //
        // Add one slot for the DSDT, whose pointer is buried in the FADT table.
        //

        TableCount = RsdtTableCount + 1;

        //
        // Attempt to find the FADT and use that to locate the DSDT physical
        // address.
        //

        for (TableIndex = 0; TableIndex < RsdtTableCount; TableIndex += 1) {
            FadtTable = (PFADT)(UINTN)RsdtTableEntry[TableIndex];
            if (FadtTable->Header.Signature == FADT_SIGNATURE) {
                DsdtTable = (PDESCRIPTION_HEADER)(UINTN)FadtTable->DsdtAddress;
                if ((DsdtTable != NULL) &&
                    (DsdtTable->Signature == DSDT_SIGNATURE)) {

                    break;

                } else {
                    DsdtTable = NULL;
                }
            }
        }
    }

    //
    // Attempt to load the test firmware file.
    //

    Status = BoLoadAndMapFile(BootDevice,
                              &ConfigurationDirectory,
                              FIRMWARE_TABLES_FILE,
                              &TestTablesPhysical,
                              &TestTablesVirtual,
                              &TestTablesSize,
                              MemoryTypeLoaderTemporary);

    //
    // Failure is expected here. If it actually succeeded, count the number
    // of tables in the blob. Tables are expected to be contiguous and
    // properly checksummed.
    //

    if (KSUCCESS(Status)) {
        TestTablesExaminedBytes = 0;
        Header = (PDESCRIPTION_HEADER)TestTablesPhysical;
        while (TestTablesExaminedBytes + sizeof(DESCRIPTION_HEADER) <=
               TestTablesSize) {

            TableCount += 1;
            TestTablesExaminedBytes += Header->Length;
            Header = (PDESCRIPTION_HEADER)((PUCHAR)Header + Header->Length);
        }

    } else {
        TestTablesPhysical = NULL;
        TestTablesSize = 0;
    }

    //
    // If there are no tables at all, fail.
    //

    if (TableCount == 0) {
        RtlDebugPrint("Error: No firmware tables found!\n");
        Status = STATUS_NOT_SUPPORTED;
        goto MapAcpiTablesEnd;
    }

    //
    // Add one for the SMBIOS table.
    //

    SmbiosTable = FwFindSmbiosTable();
    if (SmbiosTable != NULL) {
        TableCount += 1;
    }

    //
    // Allocate the firmware table directory.
    //

    AllocationSize = sizeof(FIRMWARE_TABLE_DIRECTORY) +
                     (TableCount * sizeof(PVOID));

    TableDirectory = BoAllocateMemory(AllocationSize);
    if (TableDirectory == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MapAcpiTablesEnd;
    }

    RtlZeroMemory(TableDirectory, AllocationSize);

    //
    // Allocate the loader's version of the same thing.
    //

    BoFirmwareTables = BoAllocateMemory(AllocationSize);
    if (BoFirmwareTables == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MapAcpiTablesEnd;
    }

    RtlZeroMemory(BoFirmwareTables, AllocationSize);

    //
    // Map every descriptor in the memory map marked as an ACPI table.
    //

    TableEntry = (PVOID *)(TableDirectory + 1);
    BootTableEntry = (PVOID *)(BoFirmwareTables + 1);
    RtlZeroMemory(&AcpiContext, sizeof(LOADER_ACPI_MEMORY_ITERATOR));
    AcpiContext.RsdtTableEntry = RsdtTableEntry;
    AcpiContext.RsdtTableCount = RsdtTableCount;
    AcpiContext.TableEntry = TableEntry;
    AcpiContext.BootTableEntry = BootTableEntry;
    AcpiContext.TableDirectory = TableDirectory;
    AcpiContext.DsdtTable = DsdtTable;
    AcpiContext.Status = STATUS_SUCCESS;
    MmMdIterate(MemoryMap, BopAcpiMemoryIteratorRoutine, &AcpiContext);
    if (!KSUCCESS(AcpiContext.Status)) {
        goto MapAcpiTablesEnd;
    }

    //
    // If there are test tables, add them to the list.
    //

    if (TestTablesSize != 0) {

        //
        // Loop through the tables in the file.
        //

        TestTablesExaminedBytes = 0;
        Header = (PDESCRIPTION_HEADER)TestTablesPhysical;
        while (TestTablesExaminedBytes + sizeof(DESCRIPTION_HEADER) <=
               TestTablesSize) {

            TableEntry[TableDirectory->TableCount] =
                      (PVOID)(TestTablesVirtual + ((UINTN)Header -
                                                   (UINTN)TestTablesPhysical));

            TableDirectory->TableCount += 1;
            TestTablesExaminedBytes += Header->Length;
            BootTableEntry[BoFirmwareTables->TableCount] = Header;
            BoFirmwareTables->TableCount += 1;
            Header = (PDESCRIPTION_HEADER)((PUCHAR)Header + Header->Length);
        }
    }

    //
    // If there's an SMBIOS table, then copy it to a single buffer and tack
    // that on as well.
    //

    if (SmbiosTable != NULL) {
        AllocationSize = sizeof(SMBIOS_ENTRY_POINT) +
                         SmbiosTable->StructureTableLength;

        NewTable = BoAllocateMemory(AllocationSize);
        if (NewTable == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto MapAcpiTablesEnd;
        }

        RtlCopyMemory(NewTable, SmbiosTable, sizeof(SMBIOS_ENTRY_POINT));
        RtlCopyMemory(NewTable + sizeof(SMBIOS_ENTRY_POINT),
                      (PVOID)(UINTN)(SmbiosTable->StructureTableAddress),
                      SmbiosTable->StructureTableLength);

        TableEntry[TableDirectory->TableCount] = NewTable;
        TableDirectory->TableCount += 1;
        BootTableEntry[BoFirmwareTables->TableCount] = NewTable;
        BoFirmwareTables->TableCount += 1;
    }

    Status = STATUS_SUCCESS;

MapAcpiTablesEnd:
    if (!KSUCCESS(Status)) {
        if (TableDirectory != NULL) {
            BoFreeMemory(TableDirectory);
            TableDirectory = NULL;
        }
    }

    *FirmwareTables = TableDirectory;
    return Status;
}

VOID
BopAcpiMemoryIteratorRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the memory descriptor
    list.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies an optional opaque pointer of context that was provided
        when the iteration was requested.

Return Value:

    None.

--*/

{

    PLOADER_ACPI_MEMORY_ITERATOR AcpiContext;
    KSTATUS Status;
    PFIRMWARE_TABLE_DIRECTORY TableDirectory;
    UINTN TableIndex;
    PVOID VirtualAddress;

    AcpiContext = Context;
    TableDirectory = AcpiContext->TableDirectory;
    if ((Descriptor->Type == MemoryTypeAcpiTables) ||
        (Descriptor->Type == MemoryTypeAcpiNvStorage) ||
        (Descriptor->Type == MemoryTypeFirmwarePermanent)) {

        VirtualAddress = (PVOID)-1;

        //
        // Loop through each table in the RSDT. If its pointer corresponds
        // to the range just mapped, copy the virtual address equivalent
        // into the next slot of the firmware table.
        //

        for (TableIndex = 0;
             TableIndex < AcpiContext->RsdtTableCount;
             TableIndex += 1) {

            if ((AcpiContext->RsdtTableEntry[TableIndex] >=
                 Descriptor->BaseAddress) &&
                (AcpiContext->RsdtTableEntry[TableIndex] <
                 Descriptor->BaseAddress + Descriptor->Size)) {

                //
                // If the descriptor has not yet been mapped, map that
                // sucker now.
                //

                if (VirtualAddress == (PVOID)-1) {
                    Status = BoMapPhysicalAddress(&VirtualAddress,
                                                  Descriptor->BaseAddress,
                                                  Descriptor->Size,
                                                  MAP_FLAG_READ_ONLY,
                                                  Descriptor->Type);

                    if (!KSUCCESS(Status)) {
                        AcpiContext->Status = Status;
                        return;
                    }
                }

                AcpiContext->TableEntry[TableDirectory->TableCount] =
                    (PVOID)(VirtualAddress +
                            (AcpiContext->RsdtTableEntry[TableIndex] -
                             Descriptor->BaseAddress));

                TableDirectory->TableCount += 1;
                AcpiContext->BootTableEntry[BoFirmwareTables->TableCount] =
                       (PVOID)(UINTN)(AcpiContext->RsdtTableEntry[TableIndex]);

                BoFirmwareTables->TableCount += 1;
            }
        }

        //
        // Check to see if the DSDT is in this region.
        //

        if ((AcpiContext->DsdtTable != NULL) &&
            ((UINTN)(AcpiContext->DsdtTable) >= Descriptor->BaseAddress) &&
            ((UINTN)(AcpiContext->DsdtTable) <
             Descriptor->BaseAddress + Descriptor->Size)) {

            //
            // Again, map it if it has not been mapped yet.
            //

            if (VirtualAddress == (PVOID)-1) {
                Status = BoMapPhysicalAddress(&VirtualAddress,
                                              Descriptor->BaseAddress,
                                              Descriptor->Size,
                                              MAP_FLAG_READ_ONLY,
                                              Descriptor->Type);

                if (!KSUCCESS(Status)) {
                    AcpiContext->Status = Status;
                    return;
                }
            }

            AcpiContext->TableEntry[TableDirectory->TableCount] =
                    (PVOID)(VirtualAddress + ((UINTN)(AcpiContext->DsdtTable) -
                                              Descriptor->BaseAddress));

            TableDirectory->TableCount += 1;
            AcpiContext->BootTableEntry[BoFirmwareTables->TableCount] =
                                        (PVOID)(UINTN)(AcpiContext->DsdtTable);

            BoFirmwareTables->TableCount += 1;
            AcpiContext->DsdtTable = NULL;
        }
    }

    return;
}

KSTATUS
BopLoadDrivers (
    PLOADER_BUFFER BootDriverFile
    )

/*++

Routine Description:

    This routine loads all boot drivers.

Arguments:

    BootDriverFile - Supplies a pointer to the buffer specifying a
        newline-delimited list of drivers that should be loaded at boot.

Return Value:

    Status code.

--*/

{

    PSTR DriverName;
    ULONG LoadFlags;
    PSTR NewLine;
    KSTATUS Status;
    PSTR StringEnd;
    ULONG StringLength;

    StringEnd = (PSTR)(BootDriverFile->Buffer) + BootDriverFile->Size;
    DriverName = (PSTR)(BootDriverFile->Buffer);
    StringLength = BootDriverFile->Size;
    while (TRUE) {

        //
        // Find the next newline character. Assume that the file itself is
        // NULL terminated.
        //

        NewLine = RtlStringFindCharacter(DriverName, '\n', StringLength);
        if (NewLine != NULL) {

            //
            // If it's just one or two away, this is a blank line. Skip it. If
            // this was the last line, stop.
            //

            if ((UINTN)NewLine - (UINTN)DriverName < 2) {
                if (NewLine + 1 == StringEnd) {
                    break;
                }

                StringLength -= (UINTN)((NewLine + 1) - DriverName);
                DriverName = NewLine + 1;
                continue;
            }

            //
            // Terminate the string. Watch out for CRs immediately before this.
            //

            if (*(NewLine - 1) == '\r') {
                *(NewLine - 1) = '\0';

            } else {
                *NewLine = '\0';
            }
        }

        RtlDebugPrint("Driver: %s\n", DriverName);

        //
        // Load the driver.
        //

        LoadFlags = IMAGE_LOAD_FLAG_IGNORE_INTERPRETER |
                    IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS |
                    IMAGE_LOAD_FLAG_BIND_NOW |
                    IMAGE_LOAD_FLAG_GLOBAL;

        Status = ImLoad(&BoLoadedImageList,
                        DriverName,
                        NULL,
                        NULL,
                        NULL,
                        LoadFlags,
                        NULL,
                        NULL);

        if (!KSUCCESS(Status)) {
            goto LoadDriversEnd;
        }

        //
        // If this was the last string, stop now.
        //

        if ((NewLine == NULL) || (NewLine == StringEnd) ||
            (NewLine + 1 == StringEnd)) {

            break;
        }

        //
        // Advance the string to the next driver.
        //

        StringLength -= (ULONG)((NewLine + 1) - DriverName);
        DriverName = NewLine + 1;
    }

    Status = STATUS_SUCCESS;

LoadDriversEnd:
    if (!KSUCCESS(Status) && DriverName != NULL) {
        RtlDebugPrint("Error: Failed to load driver %s (Status %d).\n",
                      DriverName,
                      Status);

        FwPrintString(0, 2, "Failed to load driver ");
        FwPrintString(22, 2, DriverName);
    }

    return Status;
}

KSTATUS
BopMapNeededHardwareRegions (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine maps pieces of hardware needed for very early kernel
    initialization.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization parameters.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PLIST_ENTRY ListHead;
    KSTATUS Status;
    PHL_PHYSICAL_ADDRESS_USAGE Usage;
    PVOID VirtualAddress;

    //
    // Loop through each of the mapped hardware module physical address usage
    // structures.
    //

    ListHead = BoHlGetPhysicalMemoryUsageListHead();
    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Usage = LIST_VALUE(CurrentEntry, HL_PHYSICAL_ADDRESS_USAGE, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Identity map the space to a kernel address.
        //

        VirtualAddress = (PVOID)(UINTN)Usage->PhysicalAddress;
        Status = BoMapPhysicalAddress(&VirtualAddress,
                                      Usage->PhysicalAddress,
                                      Usage->Size,
                                      MAP_FLAG_CACHE_DISABLE,
                                      MemoryTypeLoaderTemporary);

        if (!KSUCCESS(Status)) {
            goto MapNeededHardwareRegionsEnd;
        }

        ASSERT((UINTN)VirtualAddress == Usage->PhysicalAddress);
    }

    //
    // Create a memory resource for the hardware module to use during very
    // early initialization (including initialization of the debug device).
    //

    Status = BopAddSystemMemoryResource(Parameters,
                                        HARDWARE_MODULE_INITIAL_ALLOCATION_SIZE,
                                        SystemMemoryResourceHardwareModule,
                                        MAP_FLAG_GLOBAL);

    if (!KSUCCESS(Status)) {
        goto MapNeededHardwareRegionsEnd;
    }

    //
    // Also create a device memory resource.
    //

    Status = BopAddSystemMemoryResource(
                                Parameters,
                                HARDWARE_MODULE_INITIAL_DEVICE_ALLOCATION_SIZE,
                                SystemMemoryResourceHardwareModuleDevice,
                                MAP_FLAG_GLOBAL);

    if (!KSUCCESS(Status)) {
        goto MapNeededHardwareRegionsEnd;
    }

    //
    // Map any regions needed by firmware.
    //

    Status = BoFwMapKnownRegions(1, Parameters);
    if (!KSUCCESS(Status)) {
        goto MapNeededHardwareRegionsEnd;
    }

MapNeededHardwareRegionsEnd:
    return Status;
}

KSTATUS
BopReadBootConfiguration (
    PBOOT_INITIALIZATION_BLOCK Parameters,
    PBOOT_CONFIGURATION_CONTEXT *NewContext,
    PBOOT_ENTRY *BootEntry
    )

/*++

Routine Description:

    This routine allocates and initializes a boot configuration context with
    the boot configuration from the given file data.

Arguments:

    Parameters - Supplies a pointer to the application initialization
        information.

    NewContext - Supplies a pointer where a pointer to the boot configuration
        information will be returned. It is the caller's responsibility to
        destroy this context and free the returned pointer on success.

    BootEntry - Supplies a pointer where a pointer to the selected boot entry
        will be returned on success.

Return Value:

    Status code.

--*/

{

    PBOOT_CONFIGURATION_CONTEXT BootConfiguration;
    PBOOT_ENTRY Entry;
    ULONG EntryIndex;
    BOOL Initialized;
    PBOOT_ENTRY SelectedEntry;
    KSTATUS Status;

    Initialized = FALSE;
    SelectedEntry = NULL;
    BootConfiguration = BoAllocateMemory(sizeof(BOOT_CONFIGURATION_CONTEXT));
    if (BootConfiguration == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto ReadBootConfigurationEnd;
    }

    RtlZeroMemory(BootConfiguration, sizeof(BOOT_CONFIGURATION_CONTEXT));
    BootConfiguration->AllocateFunction = BoAllocateMemory;
    BootConfiguration->FreeFunction = BoFreeMemory;
    BootConfiguration->FileData =
                             (PVOID)(UINTN)(Parameters->BootConfigurationFile);

    BootConfiguration->FileDataSize = Parameters->BootConfigurationFileSize;
    Status = BcInitializeContext(BootConfiguration);
    if (!KSUCCESS(Status)) {
        goto ReadBootConfigurationEnd;
    }

    Initialized = TRUE;
    Status = BcReadBootConfigurationFile(BootConfiguration);
    if (!KSUCCESS(Status)) {
        goto ReadBootConfigurationEnd;
    }

    //
    // Find the selected boot entry.
    //

    for (EntryIndex = 0;
         EntryIndex < BootConfiguration->BootEntryCount;
         EntryIndex += 1) {

        Entry = BootConfiguration->BootEntries[EntryIndex];
        if (Entry->Id == Parameters->BootEntryId) {
            SelectedEntry = Entry;
            break;
        }
    }

    if (SelectedEntry == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto ReadBootConfigurationEnd;
    }

ReadBootConfigurationEnd:
    if (!KSUCCESS(Status)) {
        if (BootConfiguration != NULL) {
            if (Initialized != FALSE) {
                BcDestroyContext(BootConfiguration);
            }

            BoFreeMemory(BootConfiguration);
            BootConfiguration = NULL;
        }
    }

    *NewContext = BootConfiguration;
    *BootEntry = SelectedEntry;
    return Status;
}

KSTATUS
BopGetConfigurationDirectory (
    PBOOT_VOLUME BootDevice,
    PFILE_ID DirectoryFileId
    )

/*++

Routine Description:

    This routine gets the file ID for the boot configuration directory.

Arguments:

    BootDevice - Supplies a pointer to the boot device volume.

    DirectoryFileId - Supplies a pointer where the file ID of the directory
        will be returned on success.

Return Value:

    Status code.

--*/

{

    FILE_PROPERTIES Properties;
    KSTATUS Status;

    Status = BoLookupPath(BootDevice,
                          &BoSystemDirectoryId,
                          CONFIGURATION_DIRECTORY_PATH,
                          &Properties);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    *DirectoryFileId = Properties.FileId;
    return STATUS_SUCCESS;
}

VOID
BopSetBootTime (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine attempts to retrieve the current time from the system and
    set it in the kernel initialization block.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None, as failure here is not fatal.

--*/

{

    KSTATUS Status;

    Status = FwGetCurrentTime(&(Parameters->BootTime));
    if (!KSUCCESS(Status)) {
        RtlZeroMemory(&(Parameters->BootTime), sizeof(SYSTEM_TIME));
        return;
    }

    return;
}

KSTATUS
BopAddSystemMemoryResource (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    UINTN Size,
    SYSTEM_MEMORY_RESOURCE_TYPE Type,
    ULONG MapFlags
    )

/*++

Routine Description:

    This routine adds a system memory resource to the list of system resources
    in the kernel initialization block.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

    Size - Supplies the requested size of the resource.

    Type - Supplies the type of memory resource to create.

    MapFlags - Supplies the flags to map the memory with.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    LOADER_BUFFER Buffer;
    PSYSTEM_RESOURCE_MEMORY MemoryResource;
    KSTATUS Status;

    MemoryResource = BoAllocateMemory(sizeof(SYSTEM_RESOURCE_MEMORY));
    if (MemoryResource == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddSystemMemoryResourceEnd;
    }

    RtlZeroMemory(MemoryResource, sizeof(SYSTEM_RESOURCE_MEMORY));
    MemoryResource->Header.Type = SystemResourceMemory;
    MemoryResource->MemoryType = Type;
    MemoryResource->Header.Size = HARDWARE_MODULE_INITIAL_ALLOCATION_SIZE;
    Status = BopAllocateKernelBuffer(Size,
                                     MapFlags,
                                     &(MemoryResource->Header.PhysicalAddress),
                                     &Buffer);

    if (!KSUCCESS(Status)) {
        goto AddSystemMemoryResourceEnd;
    }

    MemoryResource->Header.VirtualAddress = Buffer.Buffer;
    INSERT_BEFORE(&(MemoryResource->Header.ListEntry),
                  &(Parameters->SystemResourceListHead));

AddSystemMemoryResourceEnd:
    if (!KSUCCESS(Status)) {
        if (MemoryResource != NULL) {
            BoFreeMemory(MemoryResource);
        }
    }

    return Status;
}

KSTATUS
BopAddMmInitMemory (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine allocates and maps the memory that the memory manager uses to
    bootstrap itself.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    UINTN AllocationSize;
    UINTN DescriptorCount;
    ULONG FirmwarePermanentCount;
    ULONG PageShift;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    //
    // Determine how many descriptors the final virtual memory map is going to
    // need. This value is the current virtual map, plus any runtime services
    // regions in the physical map (that will get virtualized later).
    //

    DescriptorCount = BoVirtualMap.DescriptorCount +
                      FREE_SYSTEM_DESCRIPTORS_REQUIRED_FOR_REFILL;

    FirmwarePermanentCount = 0;
    MmMdIterate(&BoMemoryMap,
                BopMmInitMemoryMapIterationRoutine,
                &FirmwarePermanentCount);

    DescriptorCount += FirmwarePermanentCount;
    PageShift = MmPageShift();
    PageSize = MmPageSize();

    //
    // The memory manager needs space for all the virtual descriptors.
    //

    AllocationSize = DescriptorCount * sizeof(MEMORY_DESCRIPTOR);

    //
    // It also needs a word for each physical page, plus an extra page for the
    // physical memory segments.
    // Note: if the loader continues to be 32-bit for a 64-bit kernel, then
    // this ULONG calculation is off.
    //

    AllocationSize += sizeof(ULONG) * (BoMemoryMap.TotalSpace >> PageShift);
    AllocationSize += PageSize;
    AllocationSize = ALIGN_RANGE_UP(AllocationSize, PageSize);
    Status = BopAllocateKernelBuffer(AllocationSize,
                                     MAP_FLAG_GLOBAL,
                                     &PhysicalAddress,
                                     &(Parameters->MmInitMemory));

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Parameters->MmInitMemory.Size = AllocationSize;
    return Status;
}

VOID
BopMmInitMemoryMapIterationRoutine (
    PMEMORY_DESCRIPTOR_LIST DescriptorList,
    PMEMORY_DESCRIPTOR Descriptor,
    PVOID Context
    )

/*++

Routine Description:

    This routine is called once for each descriptor in the physical memory map.

Arguments:

    DescriptorList - Supplies a pointer to the descriptor list being iterated
        over.

    Descriptor - Supplies a pointer to the current descriptor.

    Context - Supplies a context pointer, which in this case just points to a
        count of how many firmware permanent descriptors were seen.

Return Value:

    None.

--*/

{

    PULONG Count;

    Count = Context;
    if (Descriptor->Type == MemoryTypeFirmwarePermanent) {
        *Count += 1;
    }

    return;
}

KSTATUS
BopAllocateKernelBuffer (
    UINTN Size,
    ULONG MapFlags,
    PPHYSICAL_ADDRESS PhysicalAddress,
    PLOADER_BUFFER BufferOut
    )

/*++

Routine Description:

    This routine allocates and maps a memory of region for the kernel.

Arguments:

    Size - Supplies the requested size of the resource.

    MapFlags - Supplies the flags to map the memory with.

    PhysicalAddress - Supplies a pointer where the physical address of the
        allocation will be returned.

    BufferOut - Supplies a pointer where the buffer virtual address and size
        will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES on allocation failure.

--*/

{

    KSTATUS Status;

    Status = FwAllocatePages(PhysicalAddress,
                             Size,
                             0,
                             MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto AllocateKernelBufferEnd;
    }

    BufferOut->Buffer = (PVOID)-1;
    Status = BoMapPhysicalAddress(&(BufferOut->Buffer),
                                  *PhysicalAddress,
                                  Size,
                                  MapFlags,
                                  MemoryTypeLoaderPermanent);

    if (!KSUCCESS(Status)) {
        goto AllocateKernelBufferEnd;
    }

AllocateKernelBufferEnd:
    return Status;
}

