/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    setup.c

Abstract:

    This module implements the main setup executable.

Author:

    Evan Green 10-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "setup.h"
#include "sconf.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_VERSION_MAJOR 1
#define SETUP_VERSION_MINOR 0

#define SETUP_USAGE                                                            \
    "usage: msetup [-v] [-d|-p|-f destination]\n"                              \
    "Setup installs Minoca OS to a new destination. Options are:\n"            \
    "  -a, --page-file=size -- Specifies the size in megabytes of the page \n" \
    "      file to create. Specify 0 to skip page file creation. If not \n"    \
    "      supplied, a default value of 1.5x the amount of memory on the \n"   \
    "      system will be used.\n"                                             \
    "  -b, --boot=destination -- Specifies the boot partition to install to.\n"\
    "  -B, --boot-debug -- Enable boot debugging on the target installation.\n"\
    "  -D, --debug -- Enable debugging on the target installation.\n"          \
    "  -d, --disk=destination -- Specifies the install destination as a "      \
    "disk.\n"                                                                  \
    "  -f, --directory=destination -- Specifies the install destination as \n" \
    "      a directory.\n"                                                     \
    "  -G, --disk-size=size -- Specifies the disk size if the install \n"      \
    "      destination is a disk. Suffixes M, G, and T are permitted.\n"       \
    "  -i, --input=image -- Specifies the location of the installation \n"     \
    "      image. The default is to open install.img in the current "          \
    "directory.\n"                                                             \
    "      If the specified image is a file, it will be opened as an image. \n"\
    "      The input can also be a directory.\n"                               \
    "  -l, --platform=name -- Specifies the platform type.\n"                  \
    "  -p, --partition=destination -- Specifies the install destination as \n" \
    "      a partition.\n"                                                     \
    "  -q, --quiet -- Print nothing but errors.\n"                             \
    "  -r, --reboot -- Reboot after installation is complete.\n"               \
    "  -s, --script=file -- Load a script file.\n"                             \
    "  -v, --verbose -- Print files being copied.\n"                           \
    "  -x, --extra-partition=size -- Add an extra partition. Supply -1 to \n"  \
    "      split the remaining space with the system partition. This can be \n"\
    "      specified multiple times.\n"                                        \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"   \
    "The destination parameter can take the form of a device ID starting \n"   \
    "with 0x or a path.\n"                                                     \
    "Example: 'msetup -v -p 0x26' Installs on a partition with device ID "     \
    "0x26.\n"

#define SETUP_OPTIONS_STRING "Aa:b:BDd:G:hi:l:p:f:qrs:vx:V"

#define SETUP_ADD_PARTITION_SCRIPT_FORMAT \
    "Partitions.append({" \
    "\"Index\": %d," \
    "\"Size\": %lld," \
    "\"PartitionType\": PARTITION_TYPE_MINOCA," \
    "\"MbrType\": PARTITION_ID_MINOCA," \
    "\"Attributes\": 0," \
    "\"Alignment\": 4 * KILOBYTE});"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupAddExtraPartition (
    PSETUP_CONTEXT Context,
    ULONG Index,
    LONGLONG Size
    );

INT
SetupDetermineAutodeployDestination (
    PSETUP_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SetupLongOptions[] = {
    {"autodeploy", no_argument, 0, 'A'},
    {"page-file", required_argument, 0, 'a'},
    {"boot-debug", no_argument, 0, 'B'},
    {"boot", required_argument, 0, 'b'},
    {"debug", no_argument, 0, 'D'},
    {"disk", required_argument, 0, 'd'},
    {"directory", required_argument, 0, 'f'},
    {"disk-size", required_argument, 0, 'G'},
    {"help", no_argument, 0, 'h'},
    {"input", required_argument, 0, 'i'},
    {"platform", required_argument, 0, 'l'},
    {"partition", required_argument, 0, 'p'},
    {"quiet", no_argument, 0, 'q'},
    {"reboot", no_argument, 0, 'r'},
    {"script", required_argument, 0, 's'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

int
main (
    int ArgumentCount,
    char **Arguments
    )

/*++

Routine Description:

    This routine implements the setup user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    PVOID BootVolume;
    CK_CONFIGURATION ChalkConfiguration;
    SETUP_CONTEXT Context;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    PSETUP_PARTITION_DESCRIPTION Devices;
    ULONG ExtraPartitionIndex;
    LONGLONG ExtraPartitionSize;
    PSETUP_DESTINATION HostFileSystemPath;
    PSTR HostPathString;
    PSTR InstallImagePath;
    INT Option;
    PSTR PlatformName;
    BOOL PrintHeader;
    BOOL QuietlyQuit;
    PSETUP_DESTINATION SourcePath;
    struct stat Stat;
    INT Status;

    BootVolume = NULL;
    DeviceCount = 0;
    Devices = NULL;
    ExtraPartitionIndex = 100;
    HostPathString = "";
    InstallImagePath = SETUP_DEFAULT_IMAGE_NAME;
    PlatformName = NULL;
    QuietlyQuit = FALSE;
    SourcePath = NULL;
    srand(time(NULL) ^ getpid());
    memset(&Context, 0, sizeof(SETUP_CONTEXT));
    Context.PageFileSize = -1ULL;
    CkInitializeConfiguration(&ChalkConfiguration);
    Context.ChalkVm = CkCreateVm(&ChalkConfiguration);
    if (Context.ChalkVm == NULL) {
        Status = ENOMEM;
        goto mainEnd;
    }

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SETUP_OPTIONS_STRING,
                             SetupLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = -1;
            goto mainEnd;
        }

        switch (Option) {
        case 'A':
            Context.Flags |= SETUP_FLAG_AUTO_DEPLOY;
            break;

        case 'a':
            Context.PageFileSize = strtoul(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr,
                        "Error: Invalid page file megabytes: '%s'.\n",
                        optarg);

                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'b':
            Context.BootPartitionPath = SetupParseDestination(
                                                     SetupDestinationPartition,
                                                     optarg);

            break;

        case 'B':
            Context.Flags |= SETUP_FLAG_INSTALL_BOOT_DEBUG;
            break;

        case 'D':
            Context.Flags |= SETUP_FLAG_INSTALL_DEBUG;
            break;

        case 'd':
            Context.DiskPath = SetupParseDestination(SetupDestinationDisk,
                                                     optarg);

            break;

        case 'f':
            Context.DirectoryPath = SetupParseDestination(
                                                     SetupDestinationDirectory,
                                                     optarg);

            break;

        case 'G':
            Context.DiskSize = strtoull(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid disk size '%s'.\n", optarg);
                Status = EINVAL;
                goto mainEnd;
            }

            switch (toupper(*AfterScan)) {
            case '\0':
                break;

            case 'K':
                Context.DiskSize *= _1KB;
                break;

            case 'M':
                Context.DiskSize *= _1MB;
                break;

            case 'G':
                Context.DiskSize *= _1GB;
                break;

            case 'T':
                Context.DiskSize *= _1TB;
                break;

            default:
                fprintf(stderr, "Error: Invalid suffix %s.\n", AfterScan);
                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'i':
            InstallImagePath = optarg;
            break;

        case 'p':
            Context.PartitionPath = SetupParseDestination(
                                                     SetupDestinationPartition,
                                                     optarg);

            break;

        case 'l':
            if (SetupParsePlatformString(&Context, optarg) == FALSE) {
                fprintf(stderr, "Error: Invalid platform '%s'.\n", optarg);
                SetupPrintPlatformList();
                Status = EINVAL;
                goto mainEnd;
            }

            break;

        //
        // User scripts are handled later.
        //

        case 's':
            break;

        case 'q':
            Context.Flags |= SETUP_FLAG_QUIET;
            break;

        case 'r':
            Context.Flags |= SETUP_FLAG_REBOOT;
            break;

        case 'v':
            Context.Flags |= SETUP_FLAG_VERBOSE;
            break;

        //
        // Extra partitions are handled later.
        //

        case 'x':
            break;

        case 'V':
            printf("Minoca setup version %d.%d\n"
                   "Copyright (c) 2014-2016 Minoca Corp. "
                   "All Rights Reserved.\n\n",
                   SETUP_VERSION_MAJOR,
                   SETUP_VERSION_MINOR);

            return 1;

        case 'h':
            printf(SETUP_USAGE);
            SetupPrintPlatformList();
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto mainEnd;
        }
    }

    //
    // If autodeploy was specified, figure out what partition to install to.
    //

    if ((Context.Flags & SETUP_FLAG_AUTO_DEPLOY) != 0) {
        Status = SetupDetermineAutodeployDestination(&Context);
        if (Status != 0) {
            fprintf(stderr,
                    "Setup failed to determine autodeploy destination.\n");

            goto mainEnd;
        }

    //
    // If no destination was specified, list the possible destinations.
    //

    } else if ((Context.DiskPath == NULL) && (Context.PartitionPath == NULL) &&
               (Context.DirectoryPath == NULL)) {

        Status = SetupOsGetPlatformName(&PlatformName, NULL);
        if (Status != 0) {
            printf("Unable to detect platform name.\n");

        } else {
            printf("Platform: %s\n", PlatformName);
        }

        printf("No destination was specified. Please select one from the "
               "following list.\n");

        //
        // Enumerate all eligible setup devices.
        //

        Status = SetupOsEnumerateDevices(&Devices, &DeviceCount);
        if (Status != 0) {
            fprintf(stderr, "Error: Failed to enumerate devices.\n");
            goto mainEnd;
        }

        //
        // Print a description of the eligible devices.
        //

        if (DeviceCount == 0) {
            printf("Setup found no devices to install to.\n");

        } else {
            if (DeviceCount == 1) {
                printf("Setup found 1 device.\n");

            } else {
                printf("Setup found %d devices:\n", DeviceCount);
            }

            PrintHeader = TRUE;
            for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
                SetupPrintDeviceDescription(&(Devices[DeviceIndex]),
                                            PrintHeader);

                PrintHeader = FALSE;
            }

            printf("\n");
        }

        QuietlyQuit = TRUE;
        Status = -1;
        goto mainEnd;
    }

    //
    // If the install source is a directory, then open that directory as the
    // host file system path.
    //

    Status = stat(InstallImagePath, &Stat);
    if (Status != 0) {
        Status = errno;
        fprintf(stderr,
                "Error: Cannot open input %s: %s\n",
                InstallImagePath,
                strerror(Status));

        goto mainEnd;
    }

    if (S_ISDIR(Stat.st_mode)) {
        HostPathString = InstallImagePath;
    }

    //
    // Create a volume handle to the host file system.
    //

    HostFileSystemPath = SetupCreateDestination(SetupDestinationDirectory,
                                                HostPathString,
                                                0);

    if (HostFileSystemPath == NULL) {
        Status = ENOMEM;
        goto mainEnd;
    }

    Context.HostFileSystem = SetupVolumeOpen(&Context,
                                             HostFileSystemPath,
                                             SetupVolumeFormatNever,
                                             FALSE);

    SetupDestroyDestination(HostFileSystemPath);
    if (Context.HostFileSystem == NULL) {
        fprintf(stderr, "Failed to open local file system.\n");
        Status = errno;
        if (Status == 0) {
            Status = -1;
        }

        goto mainEnd;
    }

    //
    // Open up the source image.
    //

    if (S_ISDIR(Stat.st_mode)) {
        Context.SourceVolume = Context.HostFileSystem;

    } else {
        SourcePath = SetupCreateDestination(SetupDestinationImage,
                                            InstallImagePath,
                                            0);

        if (SourcePath == NULL) {
            Status = ENOMEM;
            goto mainEnd;
        }

        Context.SourceVolume = SetupVolumeOpen(&Context,
                                               SourcePath,
                                               SetupVolumeFormatNever,
                                               FALSE);

        if (Context.SourceVolume == NULL) {
            fprintf(stderr,
                    "Setup failed to open the install source: %s.\n",
                    InstallImagePath);

            Status = -1;
            goto mainEnd;
        }
    }

    //
    // Detect the platform type if it was not yet done.
    //

    if (Context.PlatformName == NULL) {
        Status = SetupDeterminePlatform(&Context);
        if (Status != 0) {
            fprintf(stderr,
                    "Error: Could not detect platform type. Specify -l "
                    "manually.\n");

            goto mainEnd;
        }
    }

    //
    // Read in and run the configuration script.
    //

    Status = SetupLoadConfiguration(&Context);
    if (Status != 0) {
        goto mainEnd;
    }

    //
    // Go through the options again now that the configuration has been loaded
    // so it can potentially be tweaked by the user.
    //

    optind = 1;
    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             SETUP_OPTIONS_STRING,
                             SetupLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        assert((Option != '?') && (Option != ':'));

        switch (Option) {
        case 'A':
            Status = SetupAddExtraPartition(&Context,
                                            ExtraPartitionIndex,
                                            -1LL);

            if (Status != 0) {
                fprintf(stderr, "Error: Failed to add extra partition.\n");
                goto mainEnd;
            }

            ExtraPartitionIndex += 1;
            break;

        case 's':
            Status = SetupLoadUserScript(&Context, optarg);
            if (Status != 0) {
                fprintf(stderr,
                        "Error: Failed to load script %s: %s.\n",
                        optarg,
                        strerror(Status));

                goto mainEnd;
            }

            break;

        case 'x':
            ExtraPartitionSize = strtoll(optarg, &AfterScan, 0);
            if (AfterScan == optarg) {
                fprintf(stderr, "Error: Invalid Size %s.\n", optarg);
                Status = EINVAL;
                goto mainEnd;
            }

            Status = SetupAddExtraPartition(&Context,
                                            ExtraPartitionIndex,
                                            ExtraPartitionSize);

            if (Status != 0) {
                fprintf(stderr, "Error: Failed to add extra partition.\n");
                goto mainEnd;
            }

            ExtraPartitionIndex += 1;
            break;

        //
        // Ignore everything else, assume it was handled in the first loop.
        //

        default:
            break;
        }
    }

    Status = SetupReadConfiguration(Context.ChalkVm, &(Context.Configuration));
    if (Status != 0) {
        perror("Failed to read configuration");
        goto mainEnd;
    }

    //
    // Install to a disk.
    //

    if (Context.DiskPath != NULL) {
        Status = SetupInstallToDisk(&Context);
        if (Status != 0) {
            goto mainEnd;
        }

        if (((Context.Flags & SETUP_FLAG_REBOOT) == 0) &&
            ((Context.Flags & SETUP_FLAG_QUIET) == 0)) {

            if ((Context.Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("\n");
            }

            printf("Remove install media and reboot to continue.\n");
        }

    //
    // There's no disk, open a partition.
    //

    } else if (Context.PartitionPath != NULL) {
        Status = SetupInstallToPartition(&Context, NULL);
        if (Status != 0) {
            goto mainEnd;
        }

    } else if (Context.DirectoryPath != NULL) {
        Status = SetupInstallToDirectory(&Context);
        if (Status != 0) {
            goto mainEnd;
        }

    } else {
        fprintf(stderr, "Error: no installation path specified.\n");
        Status = EINVAL;
        goto mainEnd;
    }

    //
    // If a boot partition was specified, try to update that.
    //

    BootVolume = NULL;
    if (Context.BootPartitionPath != NULL) {
        BootVolume = SetupVolumeOpen(&Context,
                                     Context.BootPartitionPath,
                                     SetupVolumeFormatIfIncompatible,
                                     TRUE);

        if (BootVolume == NULL) {
            fprintf(stderr, "Setup failed to open the boot volume.\n");
            SetupPrintDestination(Context.BootPartitionPath);
            Status = -1;
            goto mainEnd;
        }

    //
    // Update the boot partition for a partition or directory install.
    //

    } else if ((Context.PartitionPath != NULL) ||
               (Context.DirectoryPath != NULL)) {

        BootVolume = SetupOsOpenBootVolume(&Context);
        if (BootVolume == NULL) {
            Status = -1;
            fprintf(stderr, "Error: Failed to open boot volume.\n");
            goto mainEnd;
        }
    }

    if (BootVolume != NULL) {
        Status = SetupUpdateBootVolume(&Context, BootVolume);
        if (Status != 0) {
            goto mainEnd;
        }
    }

mainEnd:
    if (Context.Configuration != NULL) {
        SetupDestroyConfiguration(Context.Configuration);
        Context.Configuration = NULL;
    }

    if (Context.ChalkVm != NULL) {
        CkDestroyVm(Context.ChalkVm);
    }

    if (Context.SourceVolume != NULL) {
        if (Context.SourceVolume != Context.HostFileSystem) {
            SetupVolumeClose(&Context, Context.SourceVolume);
        }

        Context.SourceVolume = NULL;
    }

    if (Context.HostFileSystem != NULL) {
        SetupVolumeClose(&Context, Context.HostFileSystem);
        Context.HostFileSystem = NULL;
    }

    if (PlatformName != NULL) {
        free(PlatformName);
    }

    if (SourcePath != NULL) {
        SetupDestroyDestination(SourcePath);
    }

    if (BootVolume != NULL) {
        SetupVolumeClose(&Context, BootVolume);
    }

    if (Context.Disk != NULL) {
        SetupClose(Context.Disk);
    }

    if (Context.DiskPath != NULL) {
        SetupDestroyDestination(Context.DiskPath);
    }

    if (Context.PartitionPath != NULL) {
        SetupDestroyDestination(Context.PartitionPath);
    }

    if (Context.DirectoryPath != NULL) {
        SetupDestroyDestination(Context.DirectoryPath);
    }

    if (Context.BootPartitionPath != NULL) {
        SetupDestroyDestination(Context.BootPartitionPath);
    }

    if (Devices != NULL) {
        SetupDestroyDeviceDescriptions(Devices, DeviceCount);
    }

    if (Status == 0) {
        if ((Context.Flags & SETUP_FLAG_REBOOT) != 0) {
            if ((Context.Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Rebooting system...\n");
            }

            Status = SetupOsReboot();
            if (Status != 0) {
                fprintf(stderr,
                        "Reboot failed: %d: %s.\n",
                        Status,
                        strerror(Status));
            }
        }
    }

    if (Status != 0) {
        if ((QuietlyQuit == FALSE) &&
            ((Context.Flags & SETUP_FLAG_QUIET) == 0)) {

            if (Status > 0) {
                printf("Setup exiting with status %d: %s\n",
                       Status,
                       strerror(Status));

            } else {
                printf("Setup failed.\n");
            }
        }

        return 1;
    }

    if ((Context.Flags & SETUP_FLAG_QUIET) == 0) {
        printf("Setup completed successfully.\n");
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupAddExtraPartition (
    PSETUP_CONTEXT Context,
    ULONG Index,
    LONGLONG Size
    )

/*++

Routine Description:

    This routine adds an extra partition to the configuration.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    Index - Supplies the partition index to assign.

    Size - Supplies the size of the partition to add.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    CHAR ScriptBuffer[512];
    INT ScriptSize;

    ScriptSize = snprintf(ScriptBuffer,
                          sizeof(ScriptBuffer),
                          SETUP_ADD_PARTITION_SCRIPT_FORMAT,
                          Index,
                          Size);

    assert((ScriptSize > 0) && (ScriptSize < sizeof(ScriptBuffer)));

    return SetupLoadUserExpression(Context, ScriptBuffer);
}

INT
SetupDetermineAutodeployDestination (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine attempts to automatically find the partition to install to.
    It does this by finding a partition on the boot disk that is not the boot
    partition or the system partition. This option should generally only be
    used by test automation, not real users, which is why it is undocumented.

Arguments:

    Context - Supplies a pointer to the applicaton context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Compare;
    PSETUP_PARTITION_DESCRIPTION Device;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    PSETUP_PARTITION_DESCRIPTION Devices;
    BOOL MultipleNonSystem;
    PSETUP_PARTITION_DESCRIPTION SecondBest;
    PSETUP_PARTITION_DESCRIPTION SelectedPartition;
    INT Status;
    PSETUP_PARTITION_DESCRIPTION SystemDisk;

    Devices = NULL;
    if ((Context->DiskPath != NULL) || (Context->PartitionPath != NULL) ||
        (Context->DirectoryPath != NULL)) {

        fprintf(stderr,
                "setup: Autodeploy option is incompatible with a specified "
                "install path.\n");

        Status = EINVAL;
        goto DetermineAutodeployDestinationEnd;
    }

    //
    // Enumerate all eligible setup devices.
    //

    Status = SetupOsEnumerateDevices(&Devices, &DeviceCount);
    if (Status != 0) {
        fprintf(stderr, "Error: Failed to enumerate devices.\n");
        goto DetermineAutodeployDestinationEnd;
    }

    //
    // Print a description of the eligible devices.
    //

    if (DeviceCount == 0) {
        fprintf(stderr, "Setup found no devices to install to.\n");
        Status = ENODEV;
        goto DetermineAutodeployDestinationEnd;
    }

    //
    // First find the system disk, or at least try to.
    //

    SystemDisk = NULL;
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        Device = &(Devices[DeviceIndex]);
        if ((Device->Destination->Type == SetupDestinationDisk) &&
            ((Device->Flags & SETUP_DEVICE_FLAG_SYSTEM) != 0)) {

            SystemDisk = Device;
            break;
        }
    }

    MultipleNonSystem = FALSE;
    SecondBest = NULL;
    SelectedPartition = NULL;
    for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
        Device = &(Devices[DeviceIndex]);

        //
        // Skip anything that's not a Minoca partition.
        //

        if ((Device->Destination->Type != SetupDestinationPartition) ||
            (Device->Partition.PartitionType != PartitionTypeMinoca)) {

            continue;
        }

        //
        // Skip the partition containing the currently running OS.
        //

        if ((Device->Flags & SETUP_DEVICE_FLAG_SYSTEM) != 0) {
            continue;
        }

        //
        // See if this partition is on the system disk. If it is, definitely
        // pick it or fail if there are more than one. If it isn't, mark it
        // as a possibility, remember if there are more than one, and keep
        // looking.
        //

        Compare = -1;
        if (SystemDisk != NULL) {
            Compare = memcmp(Device->Partition.DiskId,
                             SystemDisk->Partition.DiskId,
                             DISK_IDENTIFIER_SIZE);
        }

        if (Compare == 0) {
            if (SelectedPartition == NULL) {
                SelectedPartition = Device;

            } else {
                Status = ENODEV;
                printf("Error: Setup found multiple viable partitions.\n");
                goto DetermineAutodeployDestinationEnd;
            }

        } else {
            if (SecondBest == NULL) {
                SecondBest = Device;

            } else {
                MultipleNonSystem = TRUE;
            }
        }
    }

    //
    // If there was nothing on the system disk but something elsewhere, use it.
    // Fail if there were multiple viable non-system partitions.
    //

    if ((SelectedPartition == NULL) && (SecondBest != NULL)) {
        if (MultipleNonSystem != FALSE) {
            printf("Error: Setup found multiple viable partitions.\n");
            Status = ENODEV;
            goto DetermineAutodeployDestinationEnd;
        }

        SelectedPartition = SecondBest;
    }

    if (SelectedPartition == NULL) {
        printf("Setup found no viable partitions to install to.\n");
        Status = ENODEV;
        goto DetermineAutodeployDestinationEnd;
    }

    Context->PartitionPath = SetupCreateDestination(
                                     SelectedPartition->Destination->Type,
                                     SelectedPartition->Destination->Path,
                                     SelectedPartition->Destination->DeviceId);

    if (Context->PartitionPath == NULL) {
        Status = ENOMEM;
        goto DetermineAutodeployDestinationEnd;
    }

    Status = 0;

DetermineAutodeployDestinationEnd:
    if (Devices != NULL) {
        SetupDestroyDeviceDescriptions(Devices, DeviceCount);
    }

    return Status;
}

