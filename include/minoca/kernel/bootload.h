/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bootload.h

Abstract:

    This header contains definitions for the boot loader shared between the
    loader and the kernel, as well as system initialization functions.

Author:

    Evan Green 30-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kdebug.h>
#include <minoca/kernel/sysres.h>

//
// ---------------------------------------------------------------- Definitions
//

#define BOOT_INITIALIZATION_BLOCK_VERSION 4

#define KERNEL_INITIALIZATION_BLOCK_VERSION 4

//
// Define boot initialization flags.
//

#define BOOT_INITIALIZATION_FLAG_SCREEN_CLEAR 0x00000001
#define BOOT_INITIALIZATION_FLAG_64BIT 0x00000002

//
// Define the initial size of the memory allocation to hand to the hardware
// module support.
//

#define HARDWARE_MODULE_INITIAL_ALLOCATION_SIZE 0x4000
#define HARDWARE_MODULE_INITIAL_DEVICE_ALLOCATION_SIZE 0x4000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a region of reserved memory that may or may not
    already be marked in the firmware memory map. The boot manager uses these
    descriptors to stake out its own memory in the loader on legacy PC/AT
    systems.

Members:

    Address - Stores the base address of the reserved region.

    Size - Stores the size of the reserved region in bytes.

    Flags - Stores flags describing the region.

--*/

typedef struct _BOOT_RESERVED_REGION {
    ULONGLONG Address;
    ULONGLONG Size;
    ULONGLONG Flags;
} BOOT_RESERVED_REGION, *PBOOT_RESERVED_REGION;

/*++

Structure Description:

    This structure stores the information passed between the boot manager and
    OS loader or other boot application. Future versions of this structure must
    be backwards compatible as newer boot managers may pass control over to
    older OS loaders. Pointers here are saved as 64-bit values because this
    structure may be passed from a 32-bit boot manager to a 64-bit OS loader.

Members:

    Version - Stores the version number of the loader initialization block.
        Set to BOOT_INITIALIZATION_BLOCK_VERSION.

    BootConfigurationFileSize - Stores the size of the boot configuration file
        buffer in bytes.

    BootConfigurationFile - Stores a pointer to a buffer containing the
        contents of the boot configuration file.

    BootEntryFlags - Stores the flags associated with this boot entry. See
        BOOT_ENTRY_FLAG_* definitions.

    BootEntryId - Stores the identifier of the selected boot entry.

    ReservedRegionCount - Stores the number of reserved region structures in
        the array.

    ReservedRegions - Stores a pointer to an array of reserved regions of
        memeory that may or may not be in the firmware memory map. This array
        is of type BOOT_RESERVED_REGION.

    StackTop - Stores a pointer to the top of the stack.

    StackSize - Stores the size of the boot stack region, in bytes.

    EfiImageHandle - Stores a pointer to the EFI image handle used to launch
        the boot application that launched this boot application. Note the
        type here is an EFI_HANDLE *, not an EFI_HANDLE.

    EfiSystemTable - Stores a pointer to the EFI system table as passed to the
        original EFI boot application. The type here is an EFI_SYSTEM_TABLE *.

    PartitionOffset - Stores the offset in blocks from the beginning of the
        disk to the OS partition if the firmware doesn't support partitions
        natively.

    ApplicationName - Stores a pointer to a string containing the file name of
        the application being launched.

    ApplicationLowestAddress - Stores the lowest address of the boot
        application image.

    ApplicationSize - Stores the size of the loaded boot application image in
        bytes.

    ApplicationArguments - Stores a pointer to a null terminated string
        containing the command-line-esque arguments to the application.

    PageDirectory - Stores the address of the top level page table in use.

    DriveNumber - Stores the drive number of the OS partition for legacy PC/AT
        systems.

    Flags - Stores flags describing the environment state. See
        BOOT_INITIALIZATION_FLAG_* definitions.

--*/

typedef struct _BOOT_INITIALIZATION_BLOCK {
    ULONG Version;
    ULONG BootConfigurationFileSize;
    ULONGLONG BootConfigurationFile;
    ULONGLONG BootEntryFlags;
    ULONG BootEntryId;
    ULONG ReservedRegionCount;
    ULONGLONG ReservedRegions;
    ULONGLONG StackTop;
    ULONGLONG StackSize;
    ULONGLONG EfiImageHandle;
    ULONGLONG EfiSystemTable;
    ULONGLONG PartitionOffset;
    ULONGLONG ApplicationName;
    ULONGLONG ApplicationLowestAddress;
    ULONGLONG ApplicationSize;
    ULONGLONG ApplicationArguments;
    ULONGLONG PageDirectory;
    ULONG DriveNumber;
    ULONG Flags;
} BOOT_INITIALIZATION_BLOCK, *PBOOT_INITIALIZATION_BLOCK;

typedef
INT
(*PBOOT_APPLICATION_ENTRY) (
    PBOOT_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine is the entry point into a boot application.

Arguments:

    Parameters - Supplies a pointer to the initialization information.

Return Value:

    0 or does not return on success.

    Returns a non-zero value on failure.

--*/

/*++

Structure Description:

    This structure stores pointers to all of the static tables provided by the
    firmware. An array of virtual addresses is expected to immediately follow
    this structure.

Members:

    TableCount - Supplies the number of tables in the following array.

--*/

typedef struct _FIRMWARE_TABLE_DIRECTORY {
    ULONG TableCount;
} FIRMWARE_TABLE_DIRECTORY, *PFIRMWARE_TABLE_DIRECTORY;

/*++

Structure Description:

    This structure stores information about a buffer provided by the loader to
    the kernel.

Members:

    Buffer - Stores a pointer to the data buffer.

    Size - Stores the size of the buffer, in bytes.

--*/

typedef struct _LOADER_BUFFER {
    PVOID Buffer;
    UINTN Size;
} LOADER_BUFFER, *PLOADER_BUFFER;

/*++

Structure Description:

    This structure stores information needed by the kernel to initialize. It
    is provided by the loader when the kernel is launched.

Members:

    Version - Stores the version number of the loader block. This is used to
        detect version mismatch between the loader and the kernel.

    Size - Stores the total size of the initialization block structure, in
        bytes. This field can also be used to detect mismatch or corruption
        between the loader and the kernel.

    FirmwareTables - Stores a pointer to the directory of static tables
        provided by the platform firmware.

    MemoryMap - Stores a pointer to the memory map of the machine, including
        any regions defined by the firmware, and regions allocated by the
        loader.

    VirtualMap - Stores a pointer to the virtual memory map created for the
        kernel.

    PageDirectory - Stores a pointer to the top level paging structure.

    PageTables - Stores a pointer to the page tables.

    PageTableStage - Stores a pointer to the initial page table staging area.
        The mapping for this virtual does *not* correspond to any valid memory,
        but a page table has been set up for this VA to prevent infinite loops.

    MmInitMemory - Stores a buffer of memory that the memory manager can use
        to initialize itself. This memory is mapped as loader permanent.

    ImageList - Stores the head of the list of images loaded by the kernel.
        Entries on this list are of type LOADED_IMAGE.

    KernelModule - Stores a pointer to the module information for the kernel
        itself. This data should also be in the loaded modules list.

    LoaderModule - Stores a pointer to the module information for the OS
        loader. This data should also be in the loaded modules list.

    KernelStack - Stores the kernel stack buffer that processor 0 should use.

    DeviceToDriverFile - Stores the location of the file containing the mapping
        between devices and drivers.

    DeviceMapFile - Stores the location of the file containing a list of
        unenumerable devices that exist on the system.

    SystemResourceListHead - Stores the list of system resources provided to
        the kernel by the loader. All system resources begin with a
        SYSTEM_RESOURCE_HEADER.

    BootEntry - Stores a pointer to the boot entry that was launched.

    BootTime - Stores the boot time of the system.

    FirmwareType - Stores the system firmware type.

    EfiRuntimeServices - Stores a pointer to the EFI runtime services table.
        This is only valid on EFI based systems.

    CycleCounterFrequency - Stores an estimate of the frequency of the cycle
        counter, used for very early stall services. On some architectures or
        platforms this may be 0.

--*/

typedef struct _KERNEL_INITIALIZATION_BLOCK {
    ULONG Version;
    ULONG Size;
    PFIRMWARE_TABLE_DIRECTORY FirmwareTables;
    PMEMORY_DESCRIPTOR_LIST MemoryMap;
    PMEMORY_DESCRIPTOR_LIST VirtualMap;
    PVOID PageDirectory;
    PVOID PageTables;
    PVOID PageTableStage;
    LOADER_BUFFER MmInitMemory;
    LIST_ENTRY ImageList;
    PDEBUG_MODULE KernelModule;
    PDEBUG_MODULE LoaderModule;
    LOADER_BUFFER KernelStack;
    LOADER_BUFFER DeviceToDriverFile;
    LOADER_BUFFER DeviceMapFile;
    LIST_ENTRY SystemResourceListHead;
    PVOID BootEntry;
    SYSTEM_TIME BootTime;
    SYSTEM_FIRMWARE_TYPE FirmwareType;
    PVOID EfiRuntimeServices;
    ULONGLONG CycleCounterFrequency;
} KERNEL_INITIALIZATION_BLOCK, *PKERNEL_INITIALIZATION_BLOCK;

/*++

Structure Description:

    This structure stores information needed by an application processor to
    initialize.

Members:

    StackBase - Stores the base of the stack that the initialization is
        running on.

    StackSize - Stores the size of the stack that the initialization is
        running on.

    StackPointer - Stores the stack pointer to set.

    Started - Stores a boolean set by the processor when it has successfully
        run through the initial assembly stub.

    ProcessorNumber - Stores the number of the processor.

    ProcessorStructures - Stores the processor structures buffer used for
        early architecture specific initialization.

    SwapPage - Stores a pointer to the virtual address reservation the
        processor should use for quick dispatch level mappings.

--*/

struct _PROCESSOR_START_BLOCK {
    PVOID StackBase;
    ULONG StackSize;
    PVOID StackPointer;
    ULONG Started;
    ULONG ProcessorNumber;
    PVOID ProcessorStructures;
    PVOID SwapPage;
} PACKED;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

VOID
AcpiInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine pre-initializes ACPI to the extent that the debugger requires
    it. This routine is *undebuggable* as it is called before debug services
    are online.

Arguments:

    Parameters - Supplies the kernel parameter block coming from the loader.

Return Value:

    None.

--*/

KSTATUS
AcpiInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine initializes ACPI.

Arguments:

    Parameters - Supplies the kernel parameter block coming from the loader.

Return Value:

    Status code.

--*/

KSTATUS
MmInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    PPROCESSOR_START_BLOCK StartBlock,
    ULONG Phase
    );

/*++

Routine Description:

    This routine initializes the kernel Memory Manager.

Arguments:

    Parameters - Supplies a pointer to the initialization block from the loader.

    StartBlock - Supplies a pointer to the processor start block if this is an
        application processor.

    Phase - Supplies the phase of initialization. Valid values are 0 through 4.

Return Value:

    Status code.

--*/

KSTATUS
MmPrepareForProcessorLaunch (
    PPROCESSOR_START_BLOCK StartBlock
    );

/*++

Routine Description:

    This routine initializes a processor start block in preparation for
    launching a new processor.

Arguments:

    StartBlock - Supplies a pointer to the start block that will be passed to
        the new core.

Return Value:

    Status code.

--*/

VOID
MmDestroyProcessorStartBlock (
    PPROCESSOR_START_BLOCK StartBlock
    );

/*++

Routine Description:

    This routine destroys structures initialized by MM in preparation for a
    (now failed) processor launch.

Arguments:

    StartBlock - Supplies a pointer to the start block.

Return Value:

    None.

--*/

KSTATUS
KeInitialize (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine initializes the Kernel Executive subsystem.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 through 3.

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

PPROCESSOR_START_BLOCK
KePrepareForProcessorLaunch (
    VOID
    );

/*++

Routine Description:

    This routine prepares the kernel's internal structures for a new processor
    coming online.

Arguments:

    None.

Return Value:

    Returns a pointer to an allocated and filled out processor start block
    structure. At this point the kernel will be ready for this processor to
    come online at any time.

    NULL on failure.

--*/

VOID
KeFreeProcessorStartBlock (
    PPROCESSOR_START_BLOCK StartBlock,
    BOOL FreeResourcesInside
    );

/*++

Routine Description:

    This routine frees a processor start block structure.

Arguments:

    StartBlock - Supplies a pointer to the start block structure to free.

    FreeResourcesInside - Supplies a boolean indicating whether or not to free
        the resources contained inside the start block.

Return Value:

    None.

--*/

KSTATUS
PsInitialize (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    PVOID IdleThreadStackBase,
    ULONG IdleThreadStackSize
    );

/*++

Routine Description:

    This routine initializes the process and thread subsystem.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 and 1.

    Parameters - Supplies an optional pointer to the kernel initialization
        block. It's only required for processor 0.

    IdleThreadStackBase - Supplies the base of the stack for the one thread
        currently running.

    IdleThreadStackSize - Supplies the size of the stack for the one thread
        currently running.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory for the kernel process or thread
        could not be allocated.

--*/

KSTATUS
IoInitialize (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine initializes the I/O subsystem.

Arguments:

    Phase - Supplies the initialization phase.

    Parameters - Supplies a pointer to the kernel's initialization information.

Return Value:

    Status code.

--*/

VOID
HlInitializePreDebugger (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Processor,
    PDEBUG_DEVICE_DESCRIPTION *DebugDevice
    );

/*++

Routine Description:

    This routine implements extremely early hardware layer initialization. This
    routine is *undebuggable*, as it is called before the debugger is brought
    online.

Arguments:

    Parameters - Supplies an optional pointer to the kernel initialization
        parameters. This parameter may be NULL.

    Processor - Supplies the processor index of this processor.

    DebugDevice - Supplies a pointer where a pointer to the debug device
        description will be returned on success.

Return Value:

    None.

--*/

KSTATUS
HlInitialize (
    PKERNEL_INITIALIZATION_BLOCK Parameters,
    ULONG Phase
    );

/*++

Routine Description:

    This routine initializes the core system hardware. During phase 0, on
    application processors, this routine enters at low run level and exits at
    dispatch run level.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

    Phase - Supplies the initialization phase.

Return Value:

    Status code.

--*/

