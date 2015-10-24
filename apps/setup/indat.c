/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    indat.c

Abstract:

    This module implements data related functionality for interfacing between
    the interpreter and the rest of the setup app.

Author:

    Evan Green 19-Oct-2015

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "setup.h"
#include "sconf.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupReadCopyCommands (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_PARTITION_CONFIGURATION Partition,
    PSETUP_OBJECT PartitionObject
    );

VOID
SetupDestroyCopyCommand (
    PSETUP_COPY Copy
    );

int
SetupComparePartitionConfigurations (
    const void *LeftPointer,
    const void *RightPointer
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define copy command members.
//

SETUP_C_STRUCTURE_MEMBER SetupCopyMembers[] = {
    {
        SetupCString,
        "Destination",
        FIELD_OFFSET(SETUP_COPY, Destination),
        FALSE,
        {0}
    },

    {
        SetupCUint32,
        "Offset",
        FIELD_OFFSET(SETUP_COPY, Offset),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "Source",
        FIELD_OFFSET(SETUP_COPY, Source),
        TRUE,
        {0}
    },

    {
        SetupCInt32,
        "SourceVolume",
        FIELD_OFFSET(SETUP_COPY, SourceVolume),
        FALSE,
        {0}
    },

    {
        SetupCFlag32,
        "Update",
        FIELD_OFFSET(SETUP_COPY, Flags),
        FALSE,
        {SETUP_COPY_FLAG_UPDATE}
    },

    {0}
};

//
// Define disk configuration members.
//

SETUP_C_STRUCTURE_MEMBER SetupDiskConfigurationMembers[] = {
    {
        SetupCUint32,
        "Format",
        FIELD_OFFSET(SETUP_DISK_CONFIGURATION, PartitionFormat),
        TRUE,
        {0}
    },

    {
        SetupCSubStructure,
        "Mbr",
        FIELD_OFFSET(SETUP_DISK_CONFIGURATION, Mbr),
        FALSE,
        {(UINTN)SetupCopyMembers}
    },

    {0}
};

//
// Define partition entry flags.
//

SETUP_C_STRUCTURE_MEMBER SetupPartitionFlagsMembers[] = {
    {
        SetupCFlag32,
        "Boot",
        0,
        FALSE,
        {SETUP_PARTITION_FLAG_BOOT}
    },

    {
        SetupCFlag32,
        "System",
        FALSE,
        0,
        {SETUP_PARTITION_FLAG_SYSTEM}
    },

    {
        SetupCFlag32,
        "CompatibilityMode",
        FALSE,
        0,
        {SETUP_PARTITION_FLAG_COMPATIBILITY_MODE}
    },

    {
        SetupCFlag32,
        "WriteVbrLba",
        FALSE,
        0,
        {SETUP_PARTITION_FLAG_WRITE_VBR_LBA}
    },

    {0}
};

//
// Define partition configuration members.
//

SETUP_C_STRUCTURE_MEMBER SetupPartitionConfigurationMembers[] = {
    {
        SetupCUint32,
        "Index",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Index),
        TRUE,
        {0}
    },

    {
        SetupCUint64,
        "Alignment",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Alignment),
        FALSE,
        {0}
    },

    {
        SetupCUint64,
        "Size",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Size),
        TRUE,
        {0}
    },

    {
        SetupCByteArray,
        "PartitionType",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, PartitionType),
        FALSE,
        {PARTITION_TYPE_SIZE}
    },

    {
        SetupCUint8,
        "MbrType",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, MbrType),
        FALSE,
        {0}
    },

    {
        SetupCUint64,
        "Attributes",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Attributes),
        FALSE,
        {0}
    },

    {
        SetupCSubStructure,
        "Vbr",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Vbr),
        FALSE,
        {(UINTN)SetupCopyMembers}
    },

    {
        SetupCSubStructure,
        "Flags",
        FIELD_OFFSET(SETUP_PARTITION_CONFIGURATION, Flags),
        FALSE,
        {(UINTN)SetupPartitionFlagsMembers}
    },

    {0}
};

//
// Define boot entry flags.
//

SETUP_C_STRUCTURE_MEMBER SetupBootEntryFlagsMembers[] = {
    {
        SetupCFlag32,
        "Debug",
        0,
        FALSE,
        {BOOT_ENTRY_FLAG_DEBUG}
    },

    {
        SetupCFlag32,
        "BootDebug",
        FALSE,
        0,
        {BOOT_ENTRY_FLAG_BOOT_DEBUG}
    },

    {0}
};

//
// Define boot entry fields.
//

SETUP_C_STRUCTURE_MEMBER SetupBootEntryMembers[] = {
    {
        SetupCByteArray,
        "DiskId",
        FIELD_OFFSET(BOOT_ENTRY, DiskId),
        FALSE,
        {BOOT_DISK_ID_SIZE}
    },

    {
        SetupCByteArray,
        "PartitionId",
        FIELD_OFFSET(BOOT_ENTRY, PartitionId),
        FALSE,
        {BOOT_DISK_ID_SIZE}
    },

    {
        SetupCString,
        "Name",
        FIELD_OFFSET(BOOT_ENTRY, Name),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "LoaderArguments",
        FIELD_OFFSET(BOOT_ENTRY, LoaderArguments),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "KernelArguments",
        FIELD_OFFSET(BOOT_ENTRY, KernelArguments),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "LoaderPath",
        FIELD_OFFSET(BOOT_ENTRY, LoaderPath),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "KernelPath",
        FIELD_OFFSET(BOOT_ENTRY, KernelPath),
        FALSE,
        {0}
    },

    {
        SetupCString,
        "SystemPath",
        FIELD_OFFSET(BOOT_ENTRY, SystemPath),
        FALSE,
        {0}
    },

    {
        SetupCSubStructure,
        "Flags",
        FIELD_OFFSET(BOOT_ENTRY, Flags),
        FALSE,
        {(UINTN)SetupBootEntryFlagsMembers}
    },

    {
        SetupCUint32,
        "DebugDevice",
        FIELD_OFFSET(BOOT_ENTRY, DebugDevice),
        FALSE,
        {0}
    },

    {0}
};

//
// Define global boot configuration fields.
//

SETUP_C_STRUCTURE_MEMBER SetupBootConfigurationMembers[] = {
    {
        SetupCUint32,
        "Timeout",
        FIELD_OFFSET(BOOT_CONFIGURATION_GLOBAL, Timeout),
        FALSE,
        {BOOT_DISK_ID_SIZE}
    },

    {0}
};

//
// ------------------------------------------------------------------ Functions
//

INT
SetupReadConfiguration (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_CONFIGURATION *NewConfiguration
    )

/*++

Routine Description:

    This routine reads the configuration into the given setup context after the
    interpreter has finished running.

Arguments:

    Interpreter - Supplies a pointer to the setup interpreter.

    NewConfiguration - Supplies a pointer where a pointer to the new
        configuration will be returned on success.

Return Value:

    0 on success.

    EINVAL on configuration errors.

    Other errors on other failures.

--*/

{

    UINTN AllocationSize;
    PSETUP_OBJECT BootEntries;
    PSETUP_OBJECT BootEntry;
    PSETUP_CONFIGURATION Configuration;
    ULONG Count;
    PSETUP_OBJECT DriverDb;
    ULONG Index;
    PSETUP_OBJECT Partition;
    PSETUP_OBJECT Settings;
    INT Status;
    PSETUP_OBJECT Value;

    Configuration = malloc(sizeof(SETUP_CONFIGURATION));
    if (Configuration == NULL) {
        Status = ENOMEM;
        goto ReadConfigurationEnd;
    }

    memset(Configuration, 0, sizeof(SETUP_CONFIGURATION));
    Settings = SetupDictLookupCStringKey(Interpreter->Global.Dict,
                                         "Settings");

    if (Settings == NULL) {
        fprintf(stderr, "Error: No settings found.\n");
        Status = EINVAL;
        goto ReadConfigurationEnd;
    }

    Value = SetupDictLookupCStringKey(Settings, "BootConfiguration");
    if (Value != NULL) {

        //
        // Convert the global configuration first (timeout, etc).
        //

        Status = SetupConvertDictToStructure(
                                    Interpreter,
                                    Value,
                                    SetupBootConfigurationMembers,
                                    &(Configuration->GlobalBootConfiguration));

        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to parse global boot configuration.\n");

            goto ReadConfigurationEnd;
        }

        //
        // Parse out the array of boot entry dictionaries.
        //

        BootEntries = SetupDictLookupCStringKey(Value, "BootEntries");
        if ((BootEntries == NULL) ||
            (BootEntries->Header.Type != SetupObjectList)) {

            fprintf(stderr, "Error: No boot entries found.\n");
            Status = EINVAL;
            goto ReadConfigurationEnd;
        }

        Count = BootEntries->List.Count;
        AllocationSize = sizeof(BOOT_ENTRY) * Count;
        Configuration->BootEntries = malloc(AllocationSize);
        if (Configuration->BootEntries == NULL) {
            Status = ENOMEM;
            goto ReadConfigurationEnd;
        }

        memset(Configuration->BootEntries, 0, AllocationSize);
        Count = 0;
        for (Index = 0; Index < BootEntries->List.Count; Index += 1) {
            BootEntry = BootEntries->List.Array[Index];
            if (BootEntry == NULL) {
                continue;
            }

            if (BootEntry->Header.Type == SetupObjectReference) {
                BootEntry = BootEntry->Reference.Value;
            }

            if (BootEntry->Header.Type != SetupObjectDict) {
                continue;
            }

            Status = SetupConvertDictToStructure(
                                         Interpreter,
                                         BootEntry,
                                         SetupBootEntryMembers,
                                         &(Configuration->BootEntries[Count]));

            if (Status != 0) {
                fprintf(stderr, "Error: Failed to parse boot entry.\n");
                goto ReadConfigurationEnd;
            }

            Count += 1;
            Configuration->BootEntryCount = Count;
        }

        Configuration->BootEntryCount = Count;

        //
        // Grab the boot configuration path.
        //

        Value = SetupDictLookupCStringKey(Value, "DataPath");
        if ((Value != NULL) && (Value->Header.Type == SetupObjectString)) {
            Configuration->BootDataPath = Value->String.String;
        }
    }

    Value = SetupDictLookupCStringKey(Settings, "Disk");
    if (Value == NULL) {
        fprintf(stderr, "Error: No disk configuration found.\n");
        Status = EINVAL;
        goto ReadConfigurationEnd;

    } else {
        Status = SetupConvertDictToStructure(Interpreter,
                                             Value,
                                             SetupDiskConfigurationMembers,
                                             &(Configuration->Disk));

        if (Status != 0) {
            fprintf(stderr, "Error: Failed to parse disk configuration.\n");
            goto ReadConfigurationEnd;
        }
    }

    Value = SetupDictLookupCStringKey(Value, "Partitions");
    if ((Value == NULL) || (Value->Header.Type != SetupObjectList)) {
        fprintf(stderr, "Error: No partition configuration found.\n");
        Status = EINVAL;
        goto ReadConfigurationEnd;
    }

    AllocationSize = sizeof(SETUP_PARTITION_CONFIGURATION) * Value->List.Count;
    Configuration->Disk.Partitions = malloc(AllocationSize);
    if (Configuration->Disk.Partitions == NULL) {
        Status = ENOMEM;
        goto ReadConfigurationEnd;
    }

    memset(Configuration->Disk.Partitions, 0, AllocationSize);
    Count = 0;
    for (Index = 0; Index < Value->List.Count; Index += 1) {
        Partition = Value->List.Array[Index];
        if (Partition == NULL) {
            continue;
        }

        if (Partition->Header.Type == SetupObjectReference) {
            Partition = Partition->Reference.Value;
        }

        if (Partition->Header.Type != SetupObjectDict) {
            continue;
        }

        Status = SetupConvertDictToStructure(
                                     Interpreter,
                                     Partition,
                                     SetupPartitionConfigurationMembers,
                                     &(Configuration->Disk.Partitions[Count]));

        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to parse partition configuration.\n");

            goto ReadConfigurationEnd;
        }

        Configuration->Disk.PartitionCount = Count + 1;

        //
        // Parse copy commands.
        //

        Status = SetupReadCopyCommands(Interpreter,
                                       &(Configuration->Disk.Partitions[Count]),
                                       Partition);

        if (Status != 0) {
            goto ReadConfigurationEnd;
        }

        Count += 1;
    }

    //
    // Sort the partitions by index.
    //

    qsort(Configuration->Disk.Partitions,
          Count,
          sizeof(SETUP_PARTITION_CONFIGURATION),
          SetupComparePartitionConfigurations);

    //
    // Get the driver database.
    //

    DriverDb = SetupDictLookupCStringKey(Settings, "DriverDb");
    if ((DriverDb != NULL) && (DriverDb->Header.Type == SetupObjectDict)) {
        Value = SetupDictLookupCStringKey(DriverDb, "BootDrivers");
        if ((Value != NULL) && (Value->Header.Type == SetupObjectList)) {
            Status = SetupReadStringsList(Interpreter,
                                          Value,
                                          &(Configuration->BootDrivers));

            if (Status != 0) {
                goto ReadConfigurationEnd;
            }
        }

        Value = SetupDictLookupCStringKey(DriverDb, "BootDriversPath");
        if ((Value != NULL) && (Value->Header.Type == SetupObjectString)) {
            Configuration->BootDriversPath = Value->String.String;
        }
    }

    Status = 0;

ReadConfigurationEnd:
    if (Status != 0) {
        if (Configuration != NULL) {
            SetupDestroyConfiguration(Configuration);
            Configuration = NULL;
        }
    }

    *NewConfiguration = Configuration;
    return Status;
}

VOID
SetupDestroyConfiguration (
    PSETUP_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine destroys a setup configuration.

Arguments:

    Configuration - Supplies a pointer to the configuration to destroy.

Return Value:

    None.

--*/

{

    PBOOT_ENTRY BootEntry;
    ULONG CopyIndex;
    PSETUP_DISK_CONFIGURATION Disk;
    ULONG Index;
    PSETUP_PARTITION_CONFIGURATION Partition;

    //
    // The boot drivers path is borrowed directly from a string object, which
    // still owns it.
    //

    Configuration->BootDriversPath = NULL;
    if (Configuration->BootDrivers != NULL) {
        free(Configuration->BootDrivers);
        Configuration->BootDrivers = NULL;
    }

    for (Index = 0; Index < Configuration->BootEntryCount; Index += 1) {
        BootEntry = &(Configuration->BootEntries[Index]);
        if (BootEntry->Name != NULL) {
            free(BootEntry->Name);
            BootEntry->Name = NULL;
        }

        if (BootEntry->LoaderArguments != NULL) {
            free(BootEntry->LoaderArguments);
            BootEntry->LoaderArguments = NULL;
        }

        if (BootEntry->KernelArguments != NULL) {
            free(BootEntry->KernelArguments);
            BootEntry->KernelArguments = NULL;
        }

        if (BootEntry->LoaderPath != NULL) {
            free(BootEntry->LoaderPath);
            BootEntry->LoaderPath = NULL;
        }

        if (BootEntry->KernelPath != NULL) {
            free(BootEntry->KernelPath);
            BootEntry->KernelPath = NULL;
        }

        if (BootEntry->SystemPath != NULL) {
            free(BootEntry->SystemPath);
            BootEntry->SystemPath = NULL;
        }
    }

    if (Configuration->BootEntries != NULL) {
        free(Configuration->BootEntries);
        Configuration->BootEntries = NULL;
    }

    Configuration->BootEntryCount = 0;
    Disk = &(Configuration->Disk);
    SetupDestroyCopyCommand(&(Disk->Mbr));
    for (Index = 0; Index < Disk->PartitionCount; Index += 1) {
        Partition = &(Disk->Partitions[Index]);
        SetupDestroyCopyCommand(&(Partition->Vbr));
        for (CopyIndex = 0;
             CopyIndex < Partition->CopyCommandCount;
             CopyIndex += 1) {

            SetupDestroyCopyCommand(&(Partition->CopyCommands[CopyIndex]));
        }

        if (Partition->CopyCommands != NULL) {
            free(Partition->CopyCommands);
            Partition->CopyCommands = NULL;
        }

        Partition->CopyCommandCount = 0;
    }

    free(Configuration);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupReadCopyCommands (
    PSETUP_INTERPRETER Interpreter,
    PSETUP_PARTITION_CONFIGURATION Partition,
    PSETUP_OBJECT PartitionObject
    )

/*++

Routine Description:

    This routine reads the copy commands out of the given partition object.

Arguments:

    Interpreter - Supplies a pointer to the interpreter.

    Partition - Supplies a pointer to the partition configuration.

    PartitionObject - Supplies a pointer to the partition dictionary.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    PSETUP_OBJECT Command;
    UINTN Count;
    PSETUP_OBJECT FileList;
    PSETUP_OBJECT Files;
    ULONG Index;
    INT Status;

    assert(PartitionObject->Header.Type == SetupObjectDict);

    Files = SetupDictLookupCStringKey(PartitionObject, "Files");
    if ((Files == NULL) || (Files->Header.Type != SetupObjectList)) {
        return 0;
    }

    Count = Files->List.Count;
    AllocationSize = Count * sizeof(SETUP_COPY);
    Partition->CopyCommands = malloc(AllocationSize);
    if (Partition->CopyCommands == NULL) {
        return ENOMEM;
    }

    memset(Partition->CopyCommands, 0, AllocationSize);
    Count = 0;
    for (Index = 0; Index < Files->List.Count; Index += 1) {
        Command = Files->List.Array[Index];
        if (Command == NULL) {
            continue;
        }

        if (Command->Header.Type == SetupObjectReference) {
            Command = Command->Reference.Value;
        }

        if (Command->Header.Type != SetupObjectDict) {
            continue;
        }

        Status = SetupConvertDictToStructure(Interpreter,
                                             Command,
                                             SetupCopyMembers,
                                             &(Partition->CopyCommands[Count]));

        if (Status != 0) {
            fprintf(stderr, "Error: Invalid copy command.\n");
            return Status;
        }

        FileList = SetupDictLookupCStringKey(Command, "Files");
        if (FileList != NULL) {
            Status = SetupReadStringsList(
                                      Interpreter,
                                      FileList,
                                      &(Partition->CopyCommands[Count].Files));

            if (Status != 0) {
                fprintf(stderr, "Error: Invalid copy command files.\n");
                return Status;
            }
        }

        Count += 1;
        Partition->CopyCommandCount = Count;
    }

    return 0;
}

VOID
SetupDestroyCopyCommand (
    PSETUP_COPY Copy
    )

/*++

Routine Description:

    This routine destroys the inner contents of a setup copy command.

Arguments:

    Copy - Supplies a pointer to the copy command to destroy.

Return Value:

    None.

--*/

{

    if (Copy->Destination != NULL) {
        free(Copy->Destination);
        Copy->Destination = NULL;
    }

    if (Copy->Source != NULL) {
        free(Copy->Source);
        Copy->Source = NULL;
    }

    if (Copy->Files != NULL) {
        free(Copy->Files);
        Copy->Files = NULL;
    }

    return;
}

int
SetupComparePartitionConfigurations (
    const void *LeftPointer,
    const void *RightPointer
    )

/*++

Routine Description:

    This routine compares two partition configurations by index for the qsort
    function.

Arguments:

    LeftPointer - Supplies a pointer to the left side of the comparison.

    RightPointer - Supplies a pointer to the right side of the comparison.

Return Value:

    < 0 if the partitions are in ascending order.

    0 if the partitions are equal.

    > 0 if the partitions are in descending order.

--*/

{

    PSETUP_PARTITION_CONFIGURATION Left;
    PSETUP_PARTITION_CONFIGURATION Right;

    Left = (PSETUP_PARTITION_CONFIGURATION)LeftPointer;
    Right = (PSETUP_PARTITION_CONFIGURATION)RightPointer;
    if (Left->Index < Right->Index) {
        return -1;

    } else if (Left->Index > Right->Index) {
        return 1;
    }

    return 0;
}

