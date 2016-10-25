/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bconf.h

Abstract:

    This header contains definitions for the Boot Configuration Library.

Author:

    Evan Green 20-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the size of the disk and partition identifiers in a boot entry.
//

#define BOOT_DISK_ID_SIZE 16
#define BOOT_PARTITION_ID_SIZE 16

//
// Define boot entry flags.
//

//
// Set this flag to enable kernel debugging in the entry.
//

#define BOOT_ENTRY_FLAG_DEBUG 0x0000000000000001ULL

//
// Set this flag to enable boot debugging in the entry (debugging of the OS
// loader.
//

#define BOOT_ENTRY_FLAG_BOOT_DEBUG 0x0000000000000002ULL

//
// Define the name of the boot configuration file.
//

#define BOOT_CONFIGURATION_FILE_NAME "bootconf"

//
// Define the path of the boot configuration file.
//

#define BOOT_CONFIGURATION_FILE_PATH "/EFI/MINOCA/"

//
// Define the absolute path to the boot configuration file.
//

#define BOOT_CONFIGURATION_ABSOLUTE_PATH \
    BOOT_CONFIGURATION_FILE_PATH BOOT_CONFIGURATION_FILE_NAME

//
// Define the default boot entry name.
//

#define BOOT_DEFAULT_NAME "Minoca OS"

//
// Define the default boot entry timeout value, in milliseconds.
//

#define BOOT_DEFAULT_TIMEOUT 0

//
// Define the default loader path, relative to the system root.
//

#define BOOT_DEFAULT_LOADER_PATH "system/loadefi"

//
// Define the default kernel path, relative to the system root.
//

#define BOOT_DEFAULT_KERNEL_PATH "system/kernel"

//
// Define the default system root, relative to the root file system on the
// specified partition.
//

#define BOOT_DEFAULT_SYSTEM_PATH "minoca"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define functions called by the partition library.
//

typedef
PVOID
(*PBOOT_CONFIGURATION_ALLOCATE) (
    UINTN Size
    );

/*++

Routine Description:

    This routine is called when the boot configuration library needs to
    allocate memory.

Arguments:

    Size - Supplies the size of the allocation request, in bytes.

Return Value:

    Returns a pointer to the allocation if successful, or NULL if the
    allocation failed.

--*/

typedef
VOID
(*PBOOT_CONFIGURATION_FREE) (
    PVOID Memory
    );

/*++

Routine Description:

    This routine is called when the boot configuration library needs to free
    allocated memory.

Arguments:

    Memory - Supplies the allocation returned by the allocation routine.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about a bootable entry in the
    configuration.

Members:

    Id - Stores the ID of this boot entry. The boot configuration library
        numbers boot entries when they're written, so this value may change
        across a write of the boot configuration file.

    DiskId - Stores the identifier of the disk this boot entry lives on.

    PartitionId - Stores the identifier of the partition this boot entry lives
        on.

    Name - Stores a pointer to a string containing the descriptive name for
        this boot entry.

    LoaderArguments - Stores a pointer to a string containing the arguments to
        pass to the loader.

    KernelArguments - Stores a pointer to a string containing the arguments to
        pass to the kernel.

    LoaderPath - Stores a pointer to a string containing the path to the loader,
        relative to the system path.

    KernelPath - Stores a pointer to a string containing the path to the kernel,
        relative to the system path.

    SystemPath - Stores a pointer to a string containing the OS root directory.

    Flags - Stores a bitfield of flags. See BOOT_ENTRY_FLAG_* definitions.

    DebugDevice - Stores the zero-based index of the debug device to use. This
        is an index into the array of successfully enumerated debug interfaces.

--*/

typedef struct _BOOT_ENTRY {
    ULONG Id;
    UCHAR DiskId[BOOT_DISK_ID_SIZE];
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE];
    PCSTR Name;
    PCSTR LoaderArguments;
    PCSTR KernelArguments;
    PCSTR LoaderPath;
    PCSTR KernelPath;
    PCSTR SystemPath;
    ULONGLONG Flags;
    ULONG DebugDevice;
} BOOT_ENTRY, *PBOOT_ENTRY;

/*++

Structure Description:

    This structure stores information about global boot configuration.

Members:

    Key - Stores the previous configuration key. This value is incremented
        before being written to the file.

    DefaultBootEntry - Stores a pointer to the default boot entry.

    BootOnce - Stores a pointer to a boot entry to run once on the next boot.
        On subsequent boots the default switches back to the default boot entry.

    Timeout - Stores the boot menu timeout, in milliseconds. Set this to 0 to
        pick the default entry automatically. Set this to -1 to never time out
        and force the user to choose.

--*/

typedef struct _BOOT_CONFIGURATION_GLOBAL {
    ULONG Key;
    PBOOT_ENTRY DefaultBootEntry;
    PBOOT_ENTRY BootOnce;
    ULONG Timeout;
} BOOT_CONFIGURATION_GLOBAL, *PBOOT_CONFIGURATION_GLOBAL;

/*++

Structure Description:

    This structure stores information about a boot configuration context.

Members:

    AllocateFunction - Stores a pointer to a function the library uses to
        allocate memory.

    FreeFunction - Stores a pointer to a function the library uses to free
        previously allocated memory.

    FileData - Stores a pointer to the raw file data. This memory must be
        initialized by the consumer of the library. The library will use the
        free routine to free it upon changing the boot configuration file.

    FileDataSize - Stores the size of the raw file data in bytes. This value
        must be initialized by the consumer of the library. The library will
        update this value if new configuration data is written out.

    BootEntries - Stores an array of pointers to boot entries.

    BootEntryCount - Stores the number of entries in the boot entry array.

--*/

typedef struct _BOOT_CONFIGURATION_CONTEXT {
    PBOOT_CONFIGURATION_ALLOCATE AllocateFunction;
    PBOOT_CONFIGURATION_FREE FreeFunction;
    PVOID FileData;
    ULONG FileDataSize;
    BOOT_CONFIGURATION_GLOBAL GlobalConfiguration;
    PBOOT_ENTRY *BootEntries;
    ULONG BootEntryCount;
} BOOT_CONFIGURATION_CONTEXT, *PBOOT_CONFIGURATION_CONTEXT;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
BcInitializeContext (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

/*++

Routine Description:

    This routine initializes the given boot configuration context.

Arguments:

    Context - Supplies a pointer to the context to initialize. The caller must
        have filled in the allocate and free functions, optionally filled in
        the file data, and zeroed the rest of the structure.

Return Value:

    Status code.

--*/

VOID
BcDestroyContext (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

/*++

Routine Description:

    This routine destroys the given boot configuration context. It will free
    all resources contained in the structure, including the file data.

Arguments:

    Context - Supplies a pointer to the context to destroy.

Return Value:

    None.

--*/

VOID
BcDestroyBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY Entry
    );

/*++

Routine Description:

    This routine destroys the given boot entry, freeing all its resources.

Arguments:

    Context - Supplies a pointer to the context containing the entry.

    Entry - Supplies a pointer to the entry to destroy.

Return Value:

    None.

--*/

KSTATUS
BcReadBootConfigurationFile (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

/*++

Routine Description:

    This routine parses the boot configuration file out into boot entries that
    can be manipulated by consumers of this library.

Arguments:

    Context - Supplies a pointer to the context. The file data and file data
        size must have been filled in by the caller.

Return Value:

    Status code.

--*/

KSTATUS
BcWriteBootConfigurationFile (
    PBOOT_CONFIGURATION_CONTEXT Context
    );

/*++

Routine Description:

    This routine writes the boot entries into the file buffer.

Arguments:

    Context - Supplies a pointer to the context. If there is existing file
        data it will be freed, and new file data will be allocated.

Return Value:

    Status code.

--*/

KSTATUS
BcCreateDefaultBootConfiguration (
    PBOOT_CONFIGURATION_CONTEXT Context,
    UCHAR DiskId[BOOT_DISK_ID_SIZE],
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE]
    );

/*++

Routine Description:

    This routine sets up the boot configuration data, creating a single default
    entry.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    DiskId - Supplies the disk ID of the boot entry.

    PartitionId - Supplies the partition ID of the boot entry.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

PBOOT_ENTRY
BcCreateDefaultBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PSTR Name,
    UCHAR DiskId[BOOT_DISK_ID_SIZE],
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE]
    );

/*++

Routine Description:

    This routine creates a new boot entry with the default values.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    Name - Supplies an optional pointer to a string containing the name of the
        entry. A copy of this string will be made. If no name is supplied, a
        default name will be used.

    DiskId - Supplies the disk ID of the boot entry.

    PartitionId - Supplies the partition ID of the boot entry.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

PBOOT_ENTRY
BcCopyBootEntry (
    PBOOT_CONFIGURATION_CONTEXT Context,
    PBOOT_ENTRY Source
    );

/*++

Routine Description:

    This routine creates a new boot entry based on an existing one.

Arguments:

    Context - Supplies a pointer to the boot configuration context.

    Source - Supplies a pointer to the boot entry to copy.

Return Value:

    Returns a pointer to the new boot entry on success.

    NULL on allocation failure.

--*/

