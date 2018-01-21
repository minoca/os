/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bconfp.h

Abstract:

    This header contains internal definitions for the Boot Configuration
    Library. Consumers outside the library itself should not include this
    header.

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
// Define the magic value that goes at the beginning of the boot configuration
// file.
//

#define BOOT_CONFIGURATION_HEADER_MAGIC 0x666E4342 // 'fnCB'

//
// Define the current version of the boot configuration file. Future revisions
// must be backwards compatible.
//

#define BOOT_CONFIGURATION_VERSION 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the global header at the top of the boot
    configuration file.

Members:

    Magic - Stores a constant magic value. Set this to
        BOOT_CONFIGURATION_HEADER_MAGIC.

    Version - Stores the version of the file. Future revisions are backwards
        compatible. Set this to BOOT_CONFIGURATION_VERSION.

    Key - Stores a value that is changed each time any part of the
        configuration file is changed.

    TotalSize - Stores the total size of the boot configuration data, including
        the header, all entries, and the string table.

    Crc32 - Stores the CRC32 of the entire file. This field is set to zero
        during the computation.

    EntriesOffset - Stores the offset in bytes from the beginning of this
        header (the beginning of the file) to the first boot entry.

    EntrySize - Stores the size of a single boot entry.

    EntryCount - Stores the number of entries in the array.

    StringsOffset - Stores the offset in bytes from the beginning of this
        header (the beginning of the file) to the string table.

    StringsSize - Stores the number of bytes in the string table.

    DefaultEntry - Stores the ID of the default boot entry. Set to -1 if there
        is no default boot entry.

    BootOnce - Stores the ID of the boot entry to boot from on the next boot.
        The boot loader clears this value once the entry is selected. Set to -1
        to indicate none.

    Timeout - Stores the boot menu timeout, in milliseconds. Set to -1 to
        never time out, forcing the user to make a choice. Set to 0 to pick the
        default (or boot once) option without waiting for the user.

--*/

#pragma pack(push, 1)

typedef struct _BOOT_CONFIGURATION_HEADER {
    ULONG Magic;
    ULONG Version;
    ULONG Key;
    ULONG TotalSize;
    ULONG Crc32;
    ULONG EntriesOffset;
    ULONG EntrySize;
    ULONG EntryCount;
    ULONG StringsOffset;
    ULONG StringsSize;
    ULONG DefaultEntry;
    ULONG BootOnce;
    ULONG Timeout;
} PACKED BOOT_CONFIGURATION_HEADER, *PBOOT_CONFIGURATION_HEADER;

/*++

Structure Description:

    This structure stores the structure of a boot entry on the disk.

Members:

    Id - Stores a unique identifier for this boot entry.

    Name - Stores the offset from the beginning of the string table to the name
        of this boot entry.

    DiskId - Stores the identifier of the disk this boot entry lives on.

    PartitionId - Stores the identifier of the partition this boot entry lives
        on.

    LoaderArguments - Stores the offset from the beginning of the string table
        to a string containing the command line arguments to pass to the loader.

    KernelArguments - Stores the offset from the beginning of the string table
        to a string containing the command line arguments to pass to the kernel.

    LoaderPath - Stores the offset from the beginning of the string table to
        the string containing the path to the loader. This is relative to the
        system path.

    KernelPath - Stores the offset from the beginning of the string table to
        the string containing the path to the kernel. This is relative to the
        system path.

    SystemPath - Stores the offset from the beginning of the string table to
        the string containing the system directory path. This is the base
        directory of the OS installation.

    Flags - Stores a bitfield of flags for the boot entry. See
        BOOT_ENTRY_FLAG_* definitions.

    DebugDevice - Stores the zero-based index of the debug device to use. This
        is an index into the array of successfully enumerated debug interfaces.

--*/

typedef struct _BOOT_CONFIGURATION_ENTRY {
    ULONG Id;
    ULONG Name;
    UCHAR DiskId[BOOT_DISK_ID_SIZE];
    UCHAR PartitionId[BOOT_PARTITION_ID_SIZE];
    ULONG LoaderArguments;
    ULONG KernelArguments;
    ULONG LoaderPath;
    ULONG KernelPath;
    ULONG SystemPath;
    ULONGLONG Flags;
    ULONG DebugDevice;
} PACKED BOOT_CONFIGURATION_ENTRY, *PBOOT_CONFIGURATION_ENTRY;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
