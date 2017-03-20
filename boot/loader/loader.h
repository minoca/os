/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loader.h

Abstract:

    This header contains private definitions for the boot loader.

Author:

    Evan Green 30-Jul-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define default paths for the unusual case where there is no boot entry.
//

#define DEFAULT_SYSTEM_ROOT_PATH "minoca"
#define DEFAULT_DRIVERS_DIRECTORY_PATH "drivers"
#define DEFAULT_KERNEL_BINARY_PATH "system/kernel"

//
// Define hard-coded paths underneath the system root or configuration
// directory.
//

#define CONFIGURATION_DIRECTORY_PATH "config"
#define BOOT_DRIVER_FILE "bootdrv.set"
#define DEVICE_TO_DRIVER_FILE "dev2drv.set"
#define DEVICE_MAP_FILE "devmap.set"
#define FIRMWARE_TABLES_FILE "fwtables.dat"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern MEMORY_DESCRIPTOR_LIST BoMemoryMap;
extern LIST_ENTRY BoLoadedImageList;

//
// -------------------------------------------------------- Function Prototypes
//

INT
BoMain (
    PBOOT_INITIALIZATION_BLOCK Parameters
    );

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

KSTATUS
BoLoadAndMapFile (
    PBOOT_VOLUME Volume,
    PFILE_ID Directory,
    PSTR FileName,
    PVOID *FilePhysical,
    PVOID *FileVirtual,
    PUINTN FileSize,
    MEMORY_TYPE VirtualType
    );

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

PVOID
BoGetAcpiTable (
    ULONG Signature,
    PVOID PreviousTable
    );

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

KSTATUS
BoAddFirmwareTable (
    PKERNEL_INITIALIZATION_BLOCK KernelParameters,
    PVOID Table
    );

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

KSTATUS
BoFwMapKnownRegions (
    ULONG Phase,
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine maps known ARM regions of memory.

Arguments:

    Phase - Supplies the phase number, as this routine is called twice: once
        before any other mappings have been established (0), and once near the
        end of the loader (1).

    Parameters - Supplies a pointer to the kernel's initialization parameters.

Return Value:

    Status code.

--*/

KSTATUS
BoFwPrepareForKernelLaunch (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine coordinates with the firmware to end boot services and
    prepare for the operating system to take over. Translation is still
    disabled (or identity mapped) at this point.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    Status code.

--*/

VOID
BoTransferToKernelAsm (
    PVOID Parameters,
    PVOID EntryPoint,
    PVOID StackAddress
    );

/*++

Routine Description:

    This routine transfers control of execution to the kernel.

Arguments:

    Parameters - Supplies the parameter block to pass on to the kernel for its
        initialization.

    EntryPoint - Supplies the entry point of the kernel. The function will end
        with a jump to here.

    StackAddress - Supplies the top of the kernel stack. The stack pointer will
        get set to this value just before the kernel is launched.

Return Value:

    None. This function does not return.

--*/

KSTATUS
BoInitializeImageSupport (
    PBOOT_VOLUME BootDevice,
    PBOOT_ENTRY BootEntry
    );

/*++

Routine Description:

    This routine initializes the image library for use in the boot
    environment.

Arguments:

    BootDevice - Supplies a pointer to the boot volume token, used for loading
        images from disk.

    BootEntry - Supplies a pointer to the boot entry being launched.

Return Value:

    Status code.

--*/

PLIST_ENTRY
BoHlGetPhysicalMemoryUsageListHead (
    VOID
    );

/*++

Routine Description:

    This routine returns the head of the list of regions of physical address
    space in use by the hardware layer.

Arguments:

    None.

Return Value:

    Returns a pointer to a list head pointing to a list of
    HL_PHYSICAL_ADDRESS_USAGE structures. Note that the first entry (the value
    returned) is not an entry itself but just the list head. The first valid
    entry comes from ReturnValue->Next.

--*/

KSTATUS
BoArchMapNeededHardwareRegions (
    VOID
    );

/*++

Routine Description:

    This routine maps architecture-specific pieces of hardware needed for very
    early kernel initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

VOID
BoArchMeasureCycleCounter (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    );

/*++

Routine Description:

    This routine attempts to measure the processor cycle counter.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None. The cycle counter frequency (or zero on failure) will be placed in
    the parameter block.

--*/
