/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sconf.h

Abstract:

    This header contains definitions for setup configuration structures.

Author:

    Evan Green 20-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/bconf.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define flags for a particular partition configuration.
//

#define SETUP_PARTITION_FLAG_BOOT 0x00000001
#define SETUP_PARTITION_FLAG_SYSTEM 0x00000002
#define SETUP_PARTITION_FLAG_COMPATIBILITY_MODE 0x00000004
#define SETUP_PARTITION_FLAG_WRITE_VBR_LBA 0x00000008
#define SETUP_PARTITION_FLAG_MERGE_VBR 0x00000010

#define SETUP_COPY_FLAG_UPDATE 0x00000001
#define SETUP_COPY_FLAG_OPTIONAL 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a setup copy command.

Members:

    Destination - Stores the destination path. Directories should end in a
        slash.

    Offset - Stores the destination offset in bytes from the beginning of the
        disk or partition. This is used for MBR and VBR copies.

    Source - Stores the source path. Directories should end in a slash.

    SourceVolume - Stores the source volume, usually zero for the primary
        image. Supply -1 to use the host file system.

    Files - Stores a pointer to an array of files to be copied if the source
        and destination are directories.

    Flags - Stores a bitfield of flags governing the copy command. See
        SETUP_COPY_FLAG_* definitions.

--*/

typedef struct _SETUP_COPY {
    PCSTR Destination;
    ULONG Offset;
    PCSTR Source;
    LONG SourceVolume;
    PCSTR *Files;
    ULONG Flags;
} SETUP_COPY, *PSETUP_COPY;

/*++

Structure Description:

    This structure contains the information coming out of a partition
    configuration dictionary.

Members:

    Index - Stores the partition index.

    Alignment - Stores the alignment requirement for the partition.

    Offset - Stores the partition offset in bytes.

    Size - Stores the partition size in bytes. Set to -1 to expand to fill
        the remaining size.

    PartitionType - Stores the GPT partition type identifier.

    MbrType - Stores the MBR partition type.

    Attributes - Stores the partition attributes for GPT partitions.

    Vbr - Stores the file to add as the volume boot record.

    Flags - Stores a bitfield of flags describing this partition. See
        SETUP_PARTITION_FLAG_* definitions.

    CopyCommands - Stores a pointer to an array of copy commands describing
        files to be installed on the partition.

    CopyCommandCount - Stores the number of copy commands in the array.

--*/

typedef struct _SETUP_PARTITION_CONFIGURATION {
    ULONG Index;
    ULONGLONG Alignment;
    ULONGLONG Offset;
    ULONGLONG Size;
    UCHAR PartitionId[PARTITION_IDENTIFIER_SIZE];
    UCHAR PartitionType[PARTITION_TYPE_SIZE];
    UCHAR MbrType;
    ULONGLONG Attributes;
    SETUP_COPY Vbr;
    ULONG Flags;
    PSETUP_COPY CopyCommands;
    ULONG CopyCommandCount;
} SETUP_PARTITION_CONFIGURATION, *PSETUP_PARTITION_CONFIGURATION;

/*++

Structure Description:

    This structure contains the disk formatting information coming out a
    partitioning data dictionary.

Members:

    PartitionFormat - Stores the partitioning scheme for this disk. This is of
        type PARTITION_FORMAT, but is specified with an explicit size because
        it interacts with the interpreter as a 32-bit value.

    PartitionList - Stores the head of the list of partitions.

    Mbr - Stores the optional MBR to put at the head of the disk.

--*/

typedef struct _SETUP_DISK_CONFIGURATION {
    ULONG PartitionFormat;
    SETUP_COPY Mbr;
    PSETUP_PARTITION_CONFIGURATION Partitions;
    ULONG PartitionCount;
} SETUP_DISK_CONFIGURATION, *PSETUP_DISK_CONFIGURATION;

/*++

Structure Description:

    This structure contains the configuration information for a setup
    installation.

Members:

    Disk - Stores the disk and partition configuration, including the list of
        files to copy.

    GlobalBootConfiguration - Stores the global boot configuration.

    BootEntries - Stores the boot entries for the new installation.

    BootEntryCount - Stores the number of valid boot entries.

    BootDrivers - Stores an array of strings describing the boot drivers.

    BootDriversPath - Stores a pointer to the path where the boot drivers file
        should be written out.

    BootDataPath - Stores a pointer to the path on the boot partition where
        the boot configuration data resides.

--*/

struct _SETUP_CONFIGURATION {
    SETUP_DISK_CONFIGURATION Disk;
    BOOT_CONFIGURATION_GLOBAL GlobalBootConfiguration;
    PBOOT_ENTRY BootEntries;
    ULONG BootEntryCount;
    PCSTR *BootDrivers;
    PCSTR BootDriversPath;
    PCSTR BootDataPath;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

INT
SetupLoadConfiguration (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine prepares to run the configuration specialization script.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns a non-zero value on failure.

--*/

INT
SetupLoadUserScript (
    PSETUP_CONTEXT Context,
    PCSTR Path
    );

/*++

Routine Description:

    This routine loads and runs a user customization script in the setup app.

Arguments:

    Context - Supplies a pointer to the application context.

    Path - Supplies a pointer to the path of the custom script to run.

Return Value:

    0 on success.

    Returns a non-zero value on failure.

--*/

INT
SetupLoadUserExpression (
    PSETUP_CONTEXT Context,
    PCSTR Expression
    );

/*++

Routine Description:

    This routine runs a user customization script expression in the setup app.

Arguments:

    Context - Supplies a pointer to the application context.

    Expression - Supplies a pointer to the script fragment to evaluate.

Return Value:

    0 on success.

    Returns a non-zero value on failure.

--*/

INT
SetupReadConfiguration (
    PCK_VM Vm,
    PSETUP_CONFIGURATION *NewConfiguration
    );

/*++

Routine Description:

    This routine reads the configuration into the given setup context after the
    interpreter has finished running.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    NewConfiguration - Supplies a pointer where a pointer to the new
        configuration will be returned on success.

Return Value:

    0 on success.

    EINVAL on configuration errors.

    Other errors on other failures.

--*/

VOID
SetupDestroyConfiguration (
    PSETUP_CONFIGURATION Configuration
    );

/*++

Routine Description:

    This routine destroys a setup configuration.

Arguments:

    Configuration - Supplies a pointer to the configuration to destroy.

Return Value:

    None.

--*/

//
// Setup execution steps
//

INT
SetupInstallToDisk (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine installs the OS onto an open disk.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupInstallToPartition (
    PSETUP_CONTEXT Context,
    PSETUP_PARTITION_CONFIGURATION PartitionConfiguration
    );

/*++

Routine Description:

    This routine perform the required installation steps for a particular
    partition.

Arguments:

    Context - Supplies a pointer to the application context.

    PartitionConfiguration - Supplies a pointer to the partition data to
        install.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupInstallToDirectory (
    PSETUP_CONTEXT Context
    );

/*++

Routine Description:

    This routine installs the OS onto a directory, copying only system
    partition files.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupInstallFiles (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume,
    PSETUP_PARTITION_CONFIGURATION Partition
    );

/*++

Routine Description:

    This routine installs to the given volume.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    DestinationVolume - Supplies a pointer to the open destination volume
        handle.

    Partition - Supplies a pointer to the partition configuration, which
        contains the files to copy.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupUpdateBootVolume (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    );

/*++

Routine Description:

    This routine updates the boot volume, copying the boot files and updating
    the boot entries.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    BootVolume - Supplies a pointer to the open boot volume handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

INT
SetupUpdateBootEntries (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    );

/*++

Routine Description:

    This routine writes out the new boot entries for the installed image.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    BootVolume - Supplies a pointer to the open boot volume handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/
