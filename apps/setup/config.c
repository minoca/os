/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    config.c

Abstract:

    This module implements the interface to the Chalk interpreter used to
    gather the configuration together.

Author:

    Evan Green 21-Oct-2016

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

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

VOID
SetupChalkModuleInitialize (
    PCK_VM Vm
    );

INT
SetupReadBootConfiguration (
    PCK_VM Vm,
    PSETUP_CONFIGURATION Configuration
    );

INT
SetupReadBootEntry (
    PCK_VM Vm,
    PBOOT_ENTRY BootEntry
    );

INT
SetupReadDiskConfiguration (
    PCK_VM Vm,
    PSETUP_DISK_CONFIGURATION Disk
    );

INT
SetupReadPartitionConfiguration (
    PCK_VM Vm,
    PSETUP_PARTITION_CONFIGURATION Partition
    );

PCSTR *
SetupReadStringsList (
    PCK_VM Vm
    );

INT
SetupReadCopy (
    PCK_VM Vm,
    PSETUP_COPY Copy
    );

BOOL
SetupDictGet (
    PCK_VM Vm,
    INTN StackIndex,
    PSTR Key
    );

int
SetupComparePartitionConfigurations (
    const void *LeftPointer,
    const void *RightPointer
    );

VOID
SetupDestroyCopyCommand (
    PSETUP_COPY Copy
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupLoadConfiguration (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine prepares to run the configuration specialization script.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Returns a non-zero value on failure.

--*/

{

    PVOID Buffer;
    ssize_t BytesRead;
    PVOID File;
    BOOL Result;
    ULONGLONG Size;
    INT Status;

    Buffer = NULL;
    File = NULL;

    assert((Context->PlatformName != NULL) && (Context->ArchName != NULL) &&
           (Context->SourceVolume != NULL));

    CkSetContext(Context->ChalkVm, Context);
    Result = CkPreloadForeignModule(Context->ChalkVm,
                                    "msetup",
                                    NULL,
                                    NULL,
                                    SetupChalkModuleInitialize);

    if (Result == FALSE) {
        return -1;
    }

    //
    // Open up the install configuration script in the image.
    //

    File = SetupFileOpen(Context->SourceVolume,
                         SETUP_CONFIGURATION_PATH,
                         O_RDONLY | O_BINARY,
                         0);

    if (File == NULL) {
        fprintf(stderr,
                "msetup: Failed to open configuration %s\n",
                SETUP_CONFIGURATION_PATH);

        Status = -1;
        goto LoadConfigurationEnd;
    }

    Size = 0;
    Status = SetupFileFileStat(File, &Size, NULL, NULL);
    if (Status != 0) {
        goto LoadConfigurationEnd;
    }

    Buffer = malloc(Size);
    if (Buffer == NULL) {
        goto LoadConfigurationEnd;
    }

    BytesRead = SetupFileRead(File, Buffer, Size);
    if (BytesRead != Size) {
        Status = -1;
        goto LoadConfigurationEnd;
    }

    //
    // Execute the script.
    //

    Status = CkInterpret(Context->ChalkVm, NULL, Buffer, Size, 1, FALSE);
    if (Status != CkSuccess) {
        fprintf(stderr,
                "msetup: Failed to execute configuration script: %d\n",
                Status);

        Status = EINVAL;
        goto LoadConfigurationEnd;
    }

LoadConfigurationEnd:
    if (File != NULL) {
        SetupFileClose(File);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    return Status;
}

INT
SetupLoadUserScript (
    PSETUP_CONTEXT Context,
    PCSTR Path
    )

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

{

    PSTR Buffer;
    FILE *File;
    struct stat Stat;
    INT Status;

    Buffer = NULL;
    File = NULL;
    if (stat(Path, &Stat) != 0) {
        return errno;
    }

    File = fopen(Path, "rb");
    if (File == NULL) {
        return errno;
    }

    Buffer = malloc(Stat.st_size + 1);
    if (Buffer == NULL) {
        Status = errno;
        goto LoadUserScriptEnd;
    }

    if (fread(Buffer, 1, Stat.st_size, File) != Stat.st_size) {
        Status = errno;
        goto LoadUserScriptEnd;
    }

    Buffer[Stat.st_size] = '\0';
    Status = CkInterpret(Context->ChalkVm,
                         Path,
                         Buffer,
                         Stat.st_size,
                         1,
                         FALSE);

    if (Status != CkSuccess) {
        fprintf(stderr, "Failed to interpret script %s: %d\n", Path, Status);
        Status = -1;
        goto LoadUserScriptEnd;
    }

    Status = 0;

LoadUserScriptEnd:
    if (File != NULL) {
        fclose(File);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    return Status;
}

INT
SetupLoadUserExpression (
    PSETUP_CONTEXT Context,
    PCSTR Expression
    )

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

{

    INT Status;

    Status = CkInterpret(Context->ChalkVm,
                         NULL,
                         Expression,
                         strlen(Expression),
                         1,
                         FALSE);

    if (Status != CkSuccess) {
        fprintf(stderr,
                "Failed to evaluate expression: %s\nError: %d\n",
                Expression,
                Status);

        return -1;
    }

    return 0;
}

INT
SetupReadConfiguration (
    PCK_VM Vm,
    PSETUP_CONFIGURATION *NewConfiguration
    )

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

{

    PSETUP_CONFIGURATION Configuration;
    INT Status;

    Configuration = malloc(sizeof(SETUP_CONFIGURATION));
    if (Configuration == NULL) {
        Status = ENOMEM;
        goto ReadConfigurationEnd;
    }

    memset(Configuration, 0, sizeof(SETUP_CONFIGURATION));
    if (!CkEnsureStack(Vm, 50)) {
        Status = ENOMEM;
        goto ReadConfigurationEnd;
    }

    CkPushModule(Vm, "__main");
    CkGetVariable(Vm, -1, "Settings");
    if (CkIsNull(Vm, -1)) {
        fprintf(stderr, "Error: No settings found.\n");
        Status = EINVAL;
        goto ReadConfigurationEnd;
    }

    if (SetupDictGet(Vm, -1, "BootConfiguration") != FALSE) {
        Status = SetupReadBootConfiguration(Vm, Configuration);
        if (Status != 0) {
            goto ReadConfigurationEnd;
        }

        CkStackPop(Vm);
    }

    if (!SetupDictGet(Vm, -1, "Disk")) {
        fprintf(stderr, "Error: No disk configuration found.\n");
        Status = EINVAL;
        goto ReadConfigurationEnd;
    }

    Status = SetupReadDiskConfiguration(Vm, &(Configuration->Disk));
    CkStackPop(Vm);
    if (Status != 0) {
        goto ReadConfigurationEnd;
    }

    //
    // Get the driver database.
    //

    if (SetupDictGet(Vm, -1, "DriverDb")) {
        if (SetupDictGet(Vm, -1, "BootDrivers")) {
            Configuration->BootDrivers = SetupReadStringsList(Vm);
            if (Configuration->BootDrivers == NULL) {
                Status = EINVAL;
                goto ReadConfigurationEnd;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "BootDriversPath")) {
            Configuration->BootDriversPath = CkGetString(Vm, -1, NULL);
            CkStackPop(Vm);
        }

        CkStackPop(Vm);
    }

    CkStackPop(Vm);
    CkStackPop(Vm);
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

    ULONG CopyIndex;
    PSETUP_DISK_CONFIGURATION Disk;
    ULONG Index;
    PSETUP_PARTITION_CONFIGURATION Partition;

    Configuration->BootDriversPath = NULL;
    if (Configuration->BootDrivers != NULL) {
        free(Configuration->BootDrivers);
        Configuration->BootDrivers = NULL;
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

VOID
SetupChalkModuleInitialize (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine initializes the msetup module planted in the Chalk interpreter.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None. The return value of the function should be in the first stack slot.

--*/

{

    PSETUP_CONTEXT Context;

    Context = CkGetContext(Vm);

    //
    // Set the global variables in the module in the Chalk environment.
    //

    CkPushString(Vm, Context->ArchName, strlen(Context->ArchName));
    CkSetVariable(Vm, 0, "arch");
    CkPushString(Vm, Context->PlatformName, strlen(Context->PlatformName));
    CkSetVariable(Vm, 0, "plat");
    return;
}

INT
SetupReadBootConfiguration (
    PCK_VM Vm,
    PSETUP_CONFIGURATION Configuration
    )

/*++

Routine Description:

    This routine reads the boot configuration at the top of the Chalk stack
    into the C structures.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    Configuration - Supplies a pointer to the configuration to read into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    PBOOT_ENTRY BootEntry;
    CK_INTEGER Count;
    UINTN Index;
    INT Status;

    //
    // Convert the global configuration first (timeout, etc).
    //

    if (SetupDictGet(Vm, -1, "Timeout")) {
        Configuration->GlobalBootConfiguration.Timeout = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (!SetupDictGet(Vm, -1, "BootEntries")) {
        fprintf(stderr, "Error: No boot entries found.\n");
        Status = EINVAL;
        goto ReadBootConfigurationEnd;
    }

    if (!CkGetLength(Vm, -1, &Count)) {
        Status = EINVAL;
        goto ReadBootConfigurationEnd;
    }

    AllocationSize = sizeof(BOOT_ENTRY) * Count;
    Configuration->BootEntries = malloc(AllocationSize);
    if (Configuration->BootEntries == NULL) {
        Status = ENOMEM;
        goto ReadBootConfigurationEnd;
    }

    memset(Configuration->BootEntries, 0, AllocationSize);
    for (Index = 0; Index < Count; Index += 1) {
        BootEntry = &(Configuration->BootEntries[Index]);
        CkListGet(Vm, -1, Index);
        Status = SetupReadBootEntry(Vm, BootEntry);
        CkStackPop(Vm);
        if (Status != 0) {
            fprintf(stderr, "Error: Failed to read boot entry.\n");
            goto ReadBootConfigurationEnd;
        }
    }

    CkStackPop(Vm);
    Configuration->BootEntryCount = Count;
    if (SetupDictGet(Vm, -1, "DataPath")) {
        Configuration->BootDataPath = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

ReadBootConfigurationEnd:
    return Status;
}

INT
SetupReadBootEntry (
    PCK_VM Vm,
    PBOOT_ENTRY BootEntry
    )

/*++

Routine Description:

    This routine reads a boot entry at the top of the Chalk stack into the C
    structure.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    BootEntry - Supplies a pointer where the boot entry information will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN Size;
    PCSTR String;

    if (SetupDictGet(Vm, -1, "DiskId")) {
        String = CkGetString(Vm, -1, &Size);
        if (Size > sizeof(BootEntry->DiskId)) {
            Size = sizeof(BootEntry->DiskId);
        }

        memcpy(BootEntry->DiskId, String, Size);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "PartitionId")) {
        String = CkGetString(Vm, -1, &Size);
        if (Size > sizeof(BootEntry->PartitionId)) {
            Size = sizeof(BootEntry->PartitionId);
        }

        memcpy(BootEntry->PartitionId, String, Size);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Name")) {
        BootEntry->Name = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "LoaderArguments")) {
        BootEntry->LoaderArguments = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "KernelArguments")) {
        BootEntry->KernelArguments = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "LoaderPath")) {
        BootEntry->LoaderPath = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "KernelPath")) {
        BootEntry->KernelPath = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "SystemPath")) {
        BootEntry->SystemPath = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Flags")) {
        if (!CkIsDict(Vm, -1)) {
            fprintf(stderr, "BootEntry flags should be a dict.\n");
            return EINVAL;
        }

        if (SetupDictGet(Vm, -1, "Debug")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                BootEntry->Flags |= BOOT_ENTRY_FLAG_DEBUG;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "BootDebug")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                BootEntry->Flags |= BOOT_ENTRY_FLAG_BOOT_DEBUG;
            }

            CkStackPop(Vm);
        }

        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "DebugDevice")) {
        BootEntry->DebugDevice = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    return 0;
}

INT
SetupReadDiskConfiguration (
    PCK_VM Vm,
    PSETUP_DISK_CONFIGURATION Disk
    )

/*++

Routine Description:

    This routine reads the disk configuration at the top of the Chalk stack
    into the C structures.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    Disk - Supplies a pointer to the disk configuration to read into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    CK_INTEGER Count;
    UINTN Index;
    INT Status;

    if (!SetupDictGet(Vm, -1, "Format")) {
        fprintf(stderr, "Error: Missing disk format.\n");
        return EINVAL;
    }

    Disk->PartitionFormat = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    if (SetupDictGet(Vm, -1, "Mbr")) {
        Status = SetupReadCopy(Vm, &(Disk->Mbr));
        if (Status != 0) {
            return Status;
        }

        CkStackPop(Vm);
    }

    if (!SetupDictGet(Vm, -1, "Partitions")) {
        fprintf(stderr, "Error: No partition configuration found.\n");
        return EINVAL;
    }

    if ((!CkIsList(Vm, -1)) || (!CkGetLength(Vm, -1, &Count))) {
        fprintf(stderr, "Error: Invalid partition configuration.\n");
        return EINVAL;
    }

    AllocationSize = sizeof(SETUP_PARTITION_CONFIGURATION) * Count;
    Disk->Partitions = malloc(AllocationSize);
    if (Disk->Partitions == NULL) {
        return ENOMEM;
    }

    memset(Disk->Partitions, 0, AllocationSize);
    for (Index = 0; Index < Count; Index += 1) {
        CkListGet(Vm, -1, Index);
        if (!CkIsDict(Vm, -1)) {
            fprintf(stderr,
                    "Error: Partition configuration should be a dictionary.\n");

            return EINVAL;
        }

        Status = SetupReadPartitionConfiguration(Vm,
                                                 &(Disk->Partitions[Index]));

        if (Status != 0) {
            fprintf(stderr,
                    "Error: Failed to read partition %d configuration.\n",
                    (int)Index);

            return Status;
        }

        CkStackPop(Vm);
    }

    Disk->PartitionCount = Count;

    //
    // Sort the partitions by index.
    //

    qsort(Disk->Partitions,
          Count,
          sizeof(SETUP_PARTITION_CONFIGURATION),
          SetupComparePartitionConfigurations);

    CkStackPop(Vm);
    return 0;
}

INT
SetupReadPartitionConfiguration (
    PCK_VM Vm,
    PSETUP_PARTITION_CONFIGURATION Partition
    )

/*++

Routine Description:

    This routine reads the partition configuration at the top of the Chalk
    stack into the C structures.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    Partition - Supplies a pointer to the partition configuration to read into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    CK_INTEGER Count;
    UINTN Index;
    UINTN Size;
    INT Status;
    PCSTR String;

    if (!SetupDictGet(Vm, -1, "Index")) {
        fprintf(stderr, "Error: Partition index is required.\n");
        return EINVAL;
    }

    Partition->Index = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    if (!SetupDictGet(Vm, -1, "Size")) {
        fprintf(stderr, "Error: Partition size is required.\n");
        return EINVAL;
    }

    Partition->Size = CkGetInteger(Vm, -1);
    CkStackPop(Vm);
    if (SetupDictGet(Vm, -1, "Alignment")) {
        Partition->Alignment = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "PartitionType")) {
        String = CkGetString(Vm, -1, &Size);
        if (Size > PARTITION_TYPE_SIZE) {
            Size = PARTITION_TYPE_SIZE;
        }

        memcpy(Partition->PartitionType, String, Size);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "MbrType")) {
        Partition->MbrType = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Attributes")) {
        Partition->Attributes = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Vbr")) {
        Status = SetupReadCopy(Vm, &(Partition->Vbr));
        CkStackPop(Vm);
        if (Status != 0) {
            return Status;
        }
    }

    if (SetupDictGet(Vm, -1, "Flags")) {
        if (SetupDictGet(Vm, -1, "Boot")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                Partition->Flags |= SETUP_PARTITION_FLAG_BOOT;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "System")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                Partition->Flags |= SETUP_PARTITION_FLAG_SYSTEM;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "CompatibilityMode")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                Partition->Flags |= SETUP_PARTITION_FLAG_COMPATIBILITY_MODE;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "WriteVbrLba")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                Partition->Flags |= SETUP_PARTITION_FLAG_WRITE_VBR_LBA;
            }

            CkStackPop(Vm);
        }

        if (SetupDictGet(Vm, -1, "MergeVbr")) {
            if (CkGetInteger(Vm, -1) != FALSE) {
                Partition->Flags |= SETUP_PARTITION_FLAG_MERGE_VBR;
            }

            CkStackPop(Vm);
        }

        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Files")) {
        if ((!CkIsList(Vm, -1)) || (!CkGetLength(Vm, -1, &Count))) {
            fprintf(stderr, "Error: Partition files must be a list.\n");
            return EINVAL;
        }

        AllocationSize = Count * sizeof(SETUP_COPY);
        Partition->CopyCommands = malloc(AllocationSize);
        if (Partition->CopyCommands == NULL) {
            return ENOMEM;
        }

        memset(Partition->CopyCommands, 0, AllocationSize);
        for (Index = 0; Index < Count; Index += 1) {
            CkListGet(Vm, -1, Index);
            Status = SetupReadCopy(Vm, &(Partition->CopyCommands[Index]));
            if (Status != 0) {
                return Status;
            }

            Partition->CopyCommandCount = Index + 1;
            if (SetupDictGet(Vm, -1, "Files")) {
                Partition->CopyCommands[Index].Files = SetupReadStringsList(Vm);
                if (Partition->CopyCommands[Index].Files == NULL) {
                    return EINVAL;
                }

                CkStackPop(Vm);
            }

            CkStackPop(Vm);
        }

        CkStackPop(Vm);
    }

    return 0;
}

PCSTR *
SetupReadStringsList (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine converts a list of strings at the top of the Chalk stack
    into an array of null-terminated C strings.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns a pointer to an array of strings on success. The caller is
    responsible freeing the array itself, but not each string within it, as
    those are owned by Chalk.

--*/

{

    PCSTR *Array;
    CK_INTEGER Count;
    UINTN Index;

    if ((!CkIsList(Vm, -1)) || (!CkGetLength(Vm, -1, &Count))) {
        return NULL;
    }

    Array = malloc(sizeof(PCSTR) * (Count + 1));
    memset(Array, 0, sizeof(PCSTR) * (Count + 1));
    for (Index = 0; Index < Count; Index += 1) {
        CkListGet(Vm, -1, Index);
        Array[Index] = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
        if (Array[Index] == NULL) {
            free(Array);
            return NULL;
        }
    }

    return Array;
}

INT
SetupReadCopy (
    PCK_VM Vm,
    PSETUP_COPY Copy
    )

/*++

Routine Description:

    This routine reads a copy command at the top of the Chalk stack into the C
    structure.

Arguments:

    Vm - Supplies a pointer to the Chalk virtual machine.

    Copy - Supplies a pointer where the filled out copy command will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    if (SetupDictGet(Vm, -1, "Destination")) {
        Copy->Destination = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Offset")) {
        Copy->Offset = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Source")) {
        Copy->Source = CkGetString(Vm, -1, NULL);
        CkStackPop(Vm);

    } else {
        fprintf(stderr, "Error: Source field missing in copy.\n");
        return EINVAL;
    }

    if (SetupDictGet(Vm, -1, "SourceVolume")) {
        Copy->SourceVolume = CkGetInteger(Vm, -1);
        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Update")) {
        if (CkGetInteger(Vm, -1) != FALSE) {
            Copy->Flags |= SETUP_COPY_FLAG_UPDATE;
        }

        CkStackPop(Vm);
    }

    if (SetupDictGet(Vm, -1, "Optional")) {
        if (CkGetInteger(Vm, -1) != FALSE) {
            Copy->Flags |= SETUP_COPY_FLAG_OPTIONAL;
        }

        CkStackPop(Vm);
    }

    return 0;
}

BOOL
SetupDictGet (
    PCK_VM Vm,
    INTN StackIndex,
    PSTR Key
    )

/*++

Routine Description:

    This routine gets the value in a dict associated with a given string key.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    StackIndex - Supplies the stack index of the dictionary.

    Key - Supplies the string key to use.

Return Value:

    TRUE if the value at the key is non-null.

    FALSE if null was returned.

--*/

{

    CkPushString(Vm, Key, strlen(Key));
    if (StackIndex < 0) {
        StackIndex -= 1;
    }

    if (!CkDictGet(Vm, StackIndex)) {
        return FALSE;
    }

    return TRUE;
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

    if (Copy->Files != NULL) {
        free(Copy->Files);
        Copy->Files = NULL;
    }

    return;
}

