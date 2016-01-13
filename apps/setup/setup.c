/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
#include <minoca/uefi/uefi.h>
#include <minoca/bconflib.h>

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_VERSION_MAJOR 1
#define SETUP_VERSION_MINOR 0

#define EFI_BOOT_MANAGER_PATH "/EFI/MINOCA/BOOTMEFI.EFI"

#define PCAT_BOOT_MANAGER_SOURCE "/minoca/system/bootman"
#define EFI_BOOT_MANAGER_SOURCE "/minoca/system/bootmefi.efi"

#define SETUP_USAGE                                                            \
    "usage: setup [-v] [-d|-p|-f destination]\n"                               \
    "Setup installs Minoca OS to a new destination. Options are:\n"            \
    "  -2, --two-partitions -- Split the disk and create two primary \n"       \
    "      partitions (plus the small boot partition) instead of one.\n"       \
    "  -a, --page-file=size -- Specifies the size in megabytes of the page \n" \
    "      file to create. Specify 0 to skip page file creation. If not \n"    \
    "      supplied, a default value of 1.5x the amount of memory on the \n"   \
    "      system will be used.\n"                                             \
    "  -b, --boot=destination -- Specifies the boot partition to install to.\n"\
    "  -B, --boot-debug -- Enable boot debugging on the target installation.\n"\
    "  -D, --debug -- Enable debugging on the target installation.\n"          \
    "  -d, --disk=destination -- Specifies the install destination as a "      \
    "disk.\n"                                                                  \
    "  -p, --partition=destination -- Specifies the install destination as \n" \
    "      a partition.\n"                                                     \
    "  -f, --directory=destination -- Specifies the install destination as \n" \
    "      a directory.\n"                                                     \
    "  -i, --input=image -- Specifies the location of the installation \n"     \
    "      image. The default is to open install.img in the current directory."\
    "  -l, --platform=name -- Specifies the platform type.\n"                  \
    "  -m, --partition-format=format -- Specifies the partition format for \n" \
    "      installs to a disk. Valid values are GPT and MBR.\n"                \
    "  -S  --boot-short-names -- Specifies that short file names should be\n"  \
    "      allowed when creating the boot partition.\n"                        \
    "  -r, --no-reboot -- Do not reboot after installation is complete.\n"     \
    "  -v, --verbose -- Print files being copied.\n"                           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n\n"   \
    "The destination parameter can take the form of a device ID starting \n"   \
    "with 0x or a path.\n"                                                     \
    "Example: 'setup -v -p 0x26' Installs on a partition with device ID "      \
    "0x26.\n"

#define SETUP_OPTIONS_STRING "2a:b:BDd:hi:l:m:p:f:rSvV"

//
// Define the path to the EFI default application.
//

#define EFI_DEFAULT_APPLICATION_PATH_IA32    "/EFI/BOOT/BOOTIA32.EFI"
#define EFI_DEFAULT_APPLICATION_PATH_X64     "/EFI/BOOT/BOOTX64.EFI"
#define EFI_DEFAULT_APPLICATION_PATH_ARM     "/EFI/BOOT/BOOTARM.EFI"
#define EFI_DEFAULT_APPLICATION_PATH_AARCH64 "/EFI/BOOT/BOOTAA64.EFI"

#if defined (EFI_X86)

#define EFI_DEFAULT_APPLICATION_PATH EFI_DEFAULT_APPLICATION_PATH_IA32

#elif defined (EFI_X64)

#define EFI_DEFAULT_APPLICATION_PATH EFI_DEFAULT_APPLICATION_PATH_X64

#elif defined (EFI_ARM)

#define EFI_DEFAULT_APPLICATION_PATH EFI_DEFAULT_APPLICATION_PATH_ARM

#elif defined (EFI_AARCH64)

#define EFI_DEFAULT_APPLICATION_PATH EFI_DEFAULT_APPLICATION_PATH_AARCH64

#else

#error Unknown Architecture

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SetupInstallToDisk (
    PSETUP_CONTEXT Context
    );

INT
SetupInstallToPartition (
    PSETUP_CONTEXT Context
    );

INT
SetupInstallToDirectory (
    PSETUP_CONTEXT Context
    );

INT
SetupInstallFiles (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume
    );

INT
SetupUpdateBootVolume (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    );

INT
SetupDetermineAutodeployDestination (
    PSETUP_CONTEXT Context
    );

INT
SetupDeterminePageFileSize (
    PSETUP_CONTEXT Context
    );

//
// -------------------------------------------------------------------- Globals
//

struct option SetupLongOptions[] = {
    {"two-partitions", no_argument, 0, '2'},
    {"page-file", required_argument, 0, 'a'},
    {"autodeploy", no_argument, 0, 'A'},
    {"boot", required_argument, 0, 'b'},
    {"boot-debug", no_argument, 0, 'B'},
    {"debug", no_argument, 0, 'D'},
    {"disk", required_argument, 0, 'd'},
    {"directory", required_argument, 0, 'f'},
    {"input", required_argument, 0, 'i'},
    {"partition", required_argument, 0, 'p'},
    {"partition-format", required_argument, 0, 'm'},
    {"platform", required_argument, 0, 'l'},
    {"help", no_argument, 0, 'h'},
    {"boot-short-names", no_argument, 0, 'S'},
    {"no-reboot", no_argument, 0, 'r'},
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
    BOOL AllowShortFileNames;
    PVOID BootVolume;
    SETUP_CONTEXT Context;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    PSETUP_PARTITION_DESCRIPTION Devices;
    PSTR InstallImagePath;
    INT Option;
    BOOL PrintHeader;
    BOOL QuietlyQuit;
    PSETUP_DESTINATION SourcePath;
    INT Status;

    BootVolume = NULL;
    DeviceCount = 0;
    Devices = NULL;
    InstallImagePath = SETUP_DEFAULT_IMAGE_NAME;
    QuietlyQuit = FALSE;
    SourcePath = NULL;
    srand(time(NULL) ^ getpid());
    memset(&Context, 0, sizeof(SETUP_CONTEXT));
    Context.DiskFormat = PartitionFormatGpt;

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
        case '2':
            Context.Flags |= SETUP_FLAG_TWO_PARTITIONS;
            break;

        case 'A':
            Context.Flags |= SETUP_FLAG_AUTO_DEPLOY | SETUP_FLAG_TWO_PARTITIONS;
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

            Context.Flags |= SETUP_FLAG_PAGE_FILE_SPECIFIED;
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

        case 'm':
            if (strcasecmp(optarg, "MBR") == 0) {
                Context.DiskFormat = PartitionFormatMbr;
                Context.Flags |= SETUP_FLAG_MBR;

            } else if (strcasecmp(optarg, "GPT") == 0) {
                Context.DiskFormat = PartitionFormatGpt;
                Context.Flags &= ~SETUP_FLAG_MBR;

            } else {
                fprintf(stderr,
                        "Error: Unrecognized partition format '%s'.\n",
                        optarg);

                Status = EINVAL;
                goto mainEnd;
            }

            break;

        case 'S':
            Context.Flags |= SETUP_FLAG_BOOT_ALLOW_SHORT_FILE_NAMES;
            break;

        case 'r':
            Context.Flags |= SETUP_FLAG_NO_REBOOT;
            break;

        case 'v':
            Context.Flags |= SETUP_FLAG_VERBOSE;
            break;

        case 'V':
            printf("Minoca setup version %d.%d.%d\n"
                   "Built on %s\n"
                   "Copyright (c) 2014 Minoca Corp. All Rights Reserved.\n\n",
                   SETUP_VERSION_MAJOR,
                   SETUP_VERSION_MINOR,
                   REVISION,
                   BUILD_TIME_STRING);

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
    // Detect the platform type if it was not yet done.
    //

    if (Context.Recipe == NULL) {
        Status = SetupDeterminePlatform(&Context);
        if (Status != 0) {
            fprintf(stderr,
                    "Error: Could not detect platform type. Specify -l "
                    "manually.\n");

            goto mainEnd;
        }
    }

    //
    // Add in any flags specified by the recipe.
    //

    Context.Flags |= Context.Recipe->Flags;
    if ((Context.Flags & SETUP_FLAG_MBR) != 0) {
        Context.DiskFormat = PartitionFormatMbr;
    }

    //
    // Open up the source image.
    //

    SourcePath = SetupCreateDestination(SetupDestinationImage,
                                        InstallImagePath,
                                        0);

    if (SourcePath == NULL) {
        Status = ENOMEM;
        goto mainEnd;
    }

    Context.SourceVolume = SetupVolumeOpen(&Context, SourcePath, FALSE, FALSE);
    if (Context.SourceVolume == NULL) {
        fprintf(stderr,
                "Setup failed to open the install source: %s.\n",
                InstallImagePath);

        Status = -1;
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

        //
        // Don't reboot for disk installs, as the user needs to remove the
        // install medium.
        //

        Context.Flags |= SETUP_FLAG_NO_REBOOT;
        printf("\nRemove install media and reboot to continue.\n");

    //
    // There's no disk, open a partition.
    //

    } else if (Context.PartitionPath != NULL) {
        Status = SetupInstallToPartition(&Context);
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
    // If a boot partition was specified, try to update that. First try to open
    // it without formatting. If that doesn't work, try opening and formatting
    // it.
    //

    BootVolume = NULL;
    if (Context.BootPartitionPath != NULL) {
        AllowShortFileNames = FALSE;
        if ((Context.Flags & SETUP_FLAG_BOOT_ALLOW_SHORT_FILE_NAMES) != 0) {
            AllowShortFileNames = TRUE;
        }

        BootVolume = SetupVolumeOpen(&Context,
                                     Context.BootPartitionPath,
                                     FALSE,
                                     AllowShortFileNames);

        if (BootVolume == NULL) {
            BootVolume = SetupVolumeOpen(&Context,
                                         Context.BootPartitionPath,
                                         TRUE,
                                         AllowShortFileNames);
        }

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
    if (Context.SourceVolume != NULL) {
        SetupVolumeClose(&Context, Context.SourceVolume);
        Context.SourceVolume = NULL;
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
        if ((Context.Flags & SETUP_FLAG_NO_REBOOT) == 0) {
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
        if (QuietlyQuit == FALSE) {
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

    printf("Setup completed successfully.\n");
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SetupInstallToDisk (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs the OS onto an open disk.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOL AllowShortFileNames;
    PVOID BootVolume;
    INT Result;

    BootVolume = NULL;

    //
    // Format the disk.
    //

    Result = SetupFormatDisk(Context);
    if (Result != 0) {
        fprintf(stderr, "Failed to format disk.\n");
        goto InstallToDiskEnd;
    }

    //
    // Perform the bulk of the installation, setting up the system.
    //

    Result = SetupInstallToPartition(Context);
    if (Result != 0) {
        goto InstallToDiskEnd;
    }

    //
    // Set up the boot volume.
    //

    AllowShortFileNames = FALSE;
    if ((Context->Flags & SETUP_FLAG_BOOT_ALLOW_SHORT_FILE_NAMES) != 0) {
        AllowShortFileNames = TRUE;
    }

    Context->CurrentPartitionOffset = Context->BootPartitionOffset;
    Context->CurrentPartitionSize = Context->BootPartitionSize;
    BootVolume = SetupVolumeOpen(Context,
                                 Context->DiskPath,
                                 TRUE,
                                 AllowShortFileNames);

    if (BootVolume == NULL) {
        fprintf(stderr, "Error: Failed to open boot volume.\n");
        Result = -1;
        goto InstallToDiskEnd;
    }

    Result = SetupUpdateBootVolume(Context, BootVolume);
    if (Result != 0) {
        fprintf(stderr, "Error: Failed to update boot volume.\n");
        goto InstallToDiskEnd;
    }

    SetupVolumeClose(Context, BootVolume);
    BootVolume = NULL;

    //
    // Write out the MBR and VBR.
    //

    Result = SetupInstallBootSector(Context);
    if (Result != 0) {
        fprintf(stderr, "Error: Failed to write boot sector.\n");
        goto InstallToDiskEnd;
    }

InstallToDiskEnd:
    if (BootVolume != NULL) {
        SetupVolumeClose(Context, BootVolume);
    }

    return Result;
}

INT
SetupInstallToPartition (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs the OS onto an open partition.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_DESTINATION Destination;
    INT Result;
    ULONGLONG SeekResult;
    PVOID Volume;

    Volume = NULL;

    //
    // Open up the partition. If there's already a disk, then set the offset
    // to the install partition offset.
    //

    if (Context->Disk != NULL) {
        Destination = Context->DiskPath;
        Context->CurrentPartitionOffset = Context->InstallPartition.FirstBlock;
        SeekResult = SetupPartitionSeek(Context, Context->Disk, 0);
        if (SeekResult != 0) {
            fprintf(stderr, "Failed to seek to install partition.\n");
            Result = -1;
            goto InstallToPartitionEnd;
        }

    //
    // No device has been opened, so open up the partition directly.
    //

    } else {
        Destination = Context->PartitionPath;
        Context->Disk = SetupPartitionOpen(Context,
                                           Destination,
                                           &(Context->InstallPartition));

        if (Context->Disk == NULL) {
            Result = errno;
            fprintf(stderr, "Failed to open partition: %s.\n", strerror(errno));
            goto InstallToPartitionEnd;
        }

        Context->CurrentPartitionOffset = 0;
    }

    Context->CurrentPartitionSize = Context->InstallPartition.LastBlock + 1 -
                                    Context->InstallPartition.FirstBlock;

    assert(Destination != NULL);

    Volume = SetupVolumeOpen(Context, Destination, TRUE, FALSE);
    if (Volume == NULL) {
        Result = -1;
        goto InstallToPartitionEnd;
    }

    //
    // Compute the page file size if needed.
    //

    if ((Context->Flags & SETUP_FLAG_PAGE_FILE_SPECIFIED) == 0) {
        Result = SetupDeterminePageFileSize(Context);
        if (Result != 0) {
            fprintf(stderr,
                    "Warning: Failed to determine page file size. Page file "
                    "will not be created.\n");
        }
    }

    Result = SetupInstallFiles(Context, Volume);
    if (Result != 0) {
        goto InstallToPartitionEnd;
    }

InstallToPartitionEnd:
    if (Volume != NULL) {
        SetupVolumeClose(Context, Volume);
    }

    //
    // Only close the partition if this routine opened it.
    //

    if ((Context->Disk != NULL) && (Destination != Context->DiskPath)) {
        SetupPartitionClose(Context, Context->Disk);
        Context->Disk = NULL;
    }

    return Result;
}

INT
SetupInstallToDirectory (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine installs the OS onto a directory.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Result;
    PVOID Volume;

    //
    // Compute the page file size if needed.
    //

    if ((Context->Flags & SETUP_FLAG_PAGE_FILE_SPECIFIED) == 0) {
        Result = SetupDeterminePageFileSize(Context);
        if (Result != 0) {
            fprintf(stderr,
                    "Warning: Failed to determine page file size. Page file "
                    "will not be created.\n");
        }
    }

    Volume = SetupVolumeOpen(Context, Context->DirectoryPath, FALSE, FALSE);
    if (Volume == NULL) {
        return -1;
    }

    Result = SetupInstallFiles(Context, Volume);
    SetupVolumeClose(Context, Volume);
    return Result;
}

INT
SetupInstallFiles (
    PSETUP_CONTEXT Context,
    PVOID DestinationVolume
    )

/*++

Routine Description:

    This routine installs to the given volume.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    DestinationVolume - Supplies a pointer to the open destination volume
        handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID PageFile;
    ULONGLONG PageFileSize;
    INT Result;

    PageFile = NULL;

    //
    // Copy the root directory of the image to the destination.
    //

    Result = SetupCopyFile(Context,
                           DestinationVolume,
                           Context->SourceVolume,
                           "/",
                           "/");

    if (Result != 0) {
        goto InstallFilesEnd;
    }

    //
    // Create a page file if needed.
    //

    if (Context->PageFileSize != 0) {
        PageFileSize = Context->PageFileSize * _1MB;

        //
        // Watch out for file system limitations on max file size.
        // TODO: Max file size is file system specific, not hardcoded.
        //

        if (PageFileSize > MAX_ULONG) {
            PageFileSize = MAX_ULONG;
        }

        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Creating %I64dMB page file...", PageFileSize / _1MB);
            fflush(stdout);
        }

        PageFile = SetupFileOpen(DestinationVolume,
                                 SETUP_PAGE_FILE_PATH,
                                 O_RDWR | O_CREAT,
                                 0);

        if (PageFile == NULL) {
            fprintf(stderr, "Warning: Failed to create page file.\n");
            goto InstallFilesEnd;
        }

        Result = SetupFileFileTruncate(PageFile, PageFileSize);
        SetupFileClose(PageFile);
        PageFile = NULL;
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Done\n", Context->PageFileSize);
        }
    }

InstallFilesEnd:
    if (PageFile != NULL) {
        SetupFileClose(PageFile);
    }

    return Result;
}

INT
SetupUpdateBootVolume (
    PSETUP_CONTEXT Context,
    PVOID BootVolume
    )

/*++

Routine Description:

    This routine updates the boot volume, updating the boot manager if the
    existing version is older and adding or updating a boot entry for the
    newly installed drive.

Arguments:

    Context - Supplies a pointer to the applicaton context.

    BootVolume - Supplies a pointer to the open boot volume handle.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    BOOT_CONFIGURATION_CONTEXT BootConfiguration;
    BOOL BootConfigurationInitialized;
    PBOOT_ENTRY BootEntry;
    PVOID Buffer;
    ssize_t BytesComplete;
    int CompareResult;
    PVOID Destination;
    ULONGLONG FileSize;
    UINTN Index;
    PSTR LoaderPath;
    PBOOT_ENTRY NewBootEntry;
    PVOID NewBuffer;
    size_t NewSize;
    mode_t Permissions;
    INT Result;
    KSTATUS Status;

    memset(&BootConfiguration, 0, sizeof(BOOT_CONFIGURATION_CONTEXT));
    BootConfigurationInitialized = FALSE;
    Destination = NULL;
    NewBootEntry = NULL;

    //
    // Make sure the appropriate directories exist.
    //

    Permissions = S_IRUSR | S_IWUSR | S_IXUSR;
    SetupFileCreateDirectory(BootVolume, "/EFI", Permissions);
    SetupFileCreateDirectory(BootVolume, "/EFI/BOOT", Permissions);
    SetupFileCreateDirectory(BootVolume, "/EFI/MINOCA", Permissions);

    //
    // Update the EFI boot manager if needed.
    //

    Result = SetupUpdateFile(Context,
                             BootVolume,
                             Context->SourceVolume,
                             EFI_BOOT_MANAGER_PATH,
                             EFI_BOOT_MANAGER_SOURCE);

    if (Result != 0) {
        fprintf(stderr, "Failed to update %s.\n", EFI_BOOT_MANAGER_PATH);
        goto UpdateBootVolumeEnd;
    }

    //
    // Update the default boot application.
    //

    Result = SetupUpdateFile(Context,
                             BootVolume,
                             Context->SourceVolume,
                             EFI_DEFAULT_APPLICATION_PATH,
                             EFI_BOOT_MANAGER_SOURCE);

    if (Result != 0) {
        fprintf(stderr,
                "Failed to update %s.\n",
                EFI_REMOVABLE_MEDIA_FILE_NAME);

        goto UpdateBootVolumeEnd;
    }

    //
    // The install partition information had better be valid.
    //

    assert(Context->InstallPartition.Version ==
           PARTITION_DEVICE_INFORMATION_VERSION);

    //
    // Initialize the boot configuration library support.
    //

    BootConfiguration.AllocateFunction = (PBOOT_CONFIGURATION_ALLOCATE)malloc;
    BootConfiguration.FreeFunction = (PBOOT_CONFIGURATION_FREE)free;
    Status = BcInitializeContext(&BootConfiguration);
    if (!KSUCCESS(Status)) {
        fprintf(stderr, "BcInitializeContext Error: %x\n", Status);
        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    BootConfigurationInitialized = TRUE;

    //
    // Attempt to open up the boot configuration data.
    //

    Destination = SetupFileOpen(BootVolume,
                                BOOT_CONFIGURATION_ABSOLUTE_PATH,
                                O_RDONLY,
                                0);

    if (Destination != NULL) {
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Reading existing boot configuration.\n");
        }

        //
        // The file exists. Read it in.
        //

        Result = SetupFileFileStat(Destination, &FileSize, NULL, NULL);
        if (Result != 0) {
            goto UpdateBootVolumeEnd;
        }

        Status = STATUS_NOT_FOUND;
        if (FileSize != 0) {
            Buffer = malloc(FileSize);
            if (Buffer == NULL) {
                goto UpdateBootVolumeEnd;
            }

            BytesComplete = SetupFileRead(Destination, Buffer, FileSize);
            if (BytesComplete != FileSize) {
                fprintf(stderr, "Failed to read boot configuration file.\n");
                goto UpdateBootVolumeEnd;
            }

            BootConfiguration.FileData = Buffer;
            BootConfiguration.FileDataSize = FileSize;
            Buffer = NULL;

            //
            // Read in and parse the boot configuration data. If it is
            // invalid, create a brand new default configuration.
            //

            Status = BcReadBootConfigurationFile(&BootConfiguration);
            if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "Failed to read boot configuration data: %x.\n",
                        Status);
            }
        }

        //
        // If the file size is zero or could not be read, create a default
        // configuration.
        //

        if (!KSUCCESS(Status)) {
            Status = BcCreateDefaultBootConfiguration(
                                        &BootConfiguration,
                                        Context->InstallPartition.DiskId,
                                        Context->InstallPartition.PartitionId);

            if (!KSUCCESS(Status)) {
                fprintf(stderr,
                        "Failed to create default boot configuration: "
                        "%x\n",
                        Status);

                Result = -1;
                goto UpdateBootVolumeEnd;
            }
        }

        SetupFileClose(Destination);
        Destination = NULL;

    //
    // There is no boot configuration data. Create a new one.
    //

    } else {
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Creating initial boot configuration.\n");
        }

        Status = BcCreateDefaultBootConfiguration(
                                        &BootConfiguration,
                                        Context->InstallPartition.DiskId,
                                        Context->InstallPartition.PartitionId);

        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "BcCreateDefaultBootConfiguration Error: %x\n",
                    Status);

            Result = -1;
            goto UpdateBootVolumeEnd;
        }
    }

    //
    // Create a new default entry.
    //

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Adding boot configuration entry.\n");
    }

    NewBootEntry = BcCreateDefaultBootEntry(
                            &BootConfiguration,
                            NULL,
                            Context->InstallPartition.DiskId,
                            Context->InstallPartition.PartitionId);

    if (NewBootEntry == NULL) {
        Result = ENOMEM;
        fprintf(stderr, "Failed to create boot entry.\n");
        goto UpdateBootVolumeEnd;
    }

    if ((Context->Flags & SETUP_FLAG_INSTALL_DEBUG) != 0) {
        NewBootEntry->Flags |= BOOT_ENTRY_FLAG_DEBUG;
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Enabled debug mode.\n");
        }
    }

    if ((Context->Flags & SETUP_FLAG_INSTALL_BOOT_DEBUG) != 0) {
        NewBootEntry->Flags |= BOOT_ENTRY_FLAG_BOOT_DEBUG;
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Enabled boot debug mode.\n");
        }
    }

    //
    // Use a custom loader if the platform recipe calls for it.
    //

    if (Context->Recipe->Loader != NULL) {
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Using platform loader path: %s.\n",
                   Context->Recipe->Loader);
        }

        LoaderPath = strdup(Context->Recipe->Loader);
        if (LoaderPath == NULL) {
            Result = ENOMEM;
            goto UpdateBootVolumeEnd;
        }

        free(NewBootEntry->LoaderPath);
        NewBootEntry->LoaderPath = LoaderPath;
    }

    //
    // Look for a boot entry with this partition ID to replace.
    //

    for (Index = 0;
         Index < BootConfiguration.BootEntryCount;
         Index += 1) {

        BootEntry = BootConfiguration.BootEntries[Index];
        CompareResult = memcmp(BootEntry->PartitionId,
                               NewBootEntry->PartitionId,
                               BOOT_PARTITION_ID_SIZE);

        if (CompareResult == 0) {
            if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
                printf("Replacing boot entry %d: %s.\n",
                       BootEntry->Id,
                       BootEntry->Name);
            }

            BootConfiguration.BootEntries[Index] = NewBootEntry;
            BcDestroyBootEntry(&BootConfiguration, BootEntry);
            break;
        }
    }

    //
    // If there was no previous entry pointing at this partition,
    // add it to the end of the list.
    //

    if (Index == BootConfiguration.BootEntryCount) {
        NewSize = (BootConfiguration.BootEntryCount + 1) *
                  sizeof(PBOOT_ENTRY);

        NewBuffer = realloc(BootConfiguration.BootEntries, NewSize);
        if (NewBuffer == NULL) {
            Result = ENOMEM;
            goto UpdateBootVolumeEnd;
        }

        BootConfiguration.BootEntries = NewBuffer;
        BootConfiguration.BootEntries[BootConfiguration.BootEntryCount] =
                                                                  NewBootEntry;

        BootConfiguration.BootEntryCount += 1;
    }

    BootConfiguration.GlobalConfiguration.DefaultBootEntry = NewBootEntry;
    NewBootEntry = NULL;

    //
    // Serialize the boot configuration data.
    //

    Status = BcWriteBootConfigurationFile(&BootConfiguration);
    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "Error: Failed to serialize boot configuration data: %x.\n",
                Status);

        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("Writing boot configuration data.\n");
    }

    //
    // Open and write the data.
    //

    Destination = SetupFileOpen(BootVolume,
                                BOOT_CONFIGURATION_ABSOLUTE_PATH,
                                O_RDWR | O_CREAT | O_TRUNC,
                                Permissions);

    if (Destination == NULL) {
        fprintf(stderr,
                "Error: Failed to open %s for writing.\n",
                BOOT_CONFIGURATION_ABSOLUTE_PATH);

        Result = errno;
        if (Result == 0) {
            Result = -1;
        }

        goto UpdateBootVolumeEnd;
    }

    BytesComplete = SetupFileWrite(Destination,
                                   BootConfiguration.FileData,
                                   BootConfiguration.FileDataSize);

    if (BytesComplete != BootConfiguration.FileDataSize) {
        fprintf(stderr,
                "Error: Failed to write boot configuration data.\n");

        Result = -1;
        goto UpdateBootVolumeEnd;
    }

    SetupFileClose(Destination);
    Destination = NULL;

    //
    // Install any platform specific files into the boot volume.
    //

    Result = SetupInstallPlatformBootFiles(Context, BootVolume);
    if (Result != 0) {
        fprintf(stderr, "Error: Failed to install platform boot files.\n");
        goto UpdateBootVolumeEnd;
    }

UpdateBootVolumeEnd:
    if (BootConfigurationInitialized != FALSE) {
        if (NewBootEntry != NULL) {
            BcDestroyBootEntry(&BootConfiguration, NewBootEntry);
        }

        BcDestroyContext(&BootConfiguration);
    }

    if (Destination != NULL) {
        SetupFileClose(Destination);
    }

    return Result;
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

    PSETUP_PARTITION_DESCRIPTION Device;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    PSETUP_PARTITION_DESCRIPTION Devices;
    INT Status;

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
        // If a viable partition was already found, stop, as there seem to be
        // multiple choices.
        //

        if (Context->PartitionPath != NULL) {
            printf("Setup found multiple viable partitions. Stop.\n");
            SetupPrintDestination(Context->PartitionPath);
            printf("\n");
            SetupPrintDestination(Device->Destination);
            Status = ENODEV;
            goto DetermineAutodeployDestinationEnd;
        }

        Context->PartitionPath = SetupCreateDestination(
                                                Device->Destination->Type,
                                                Device->Destination->Path,
                                                Device->Destination->DeviceId);

        if (Context->PartitionPath == NULL) {
            Status = ENOMEM;
            goto DetermineAutodeployDestinationEnd;
        }
    }

    Status = 0;
    if (Context->PartitionPath == NULL) {
        printf("Setup found no viable partitions to install to.\n");
        Status = ENODEV;
    }

DetermineAutodeployDestinationEnd:
    if (Devices != NULL) {
        SetupDestroyDeviceDescriptions(Devices, DeviceCount);
    }

    return Status;
}

INT
SetupDeterminePageFileSize (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine determines the size of the page file to create.

Arguments:

    Context - Supplies a pointer to the applicaton context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG InstallPartitionSize;
    ULONGLONG PageFileSize;
    INT Result;
    ULONGLONG SystemMemory;

    Result = SetupOsGetSystemMemorySize(&SystemMemory);
    if (Result != 0) {
        return Result;
    }

    PageFileSize = (SystemMemory * SETUP_DEFAULT_PAGE_FILE_NUMERATOR) /
                   SETUP_DEFAULT_PAGE_FILE_DENOMINATOR;

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("System memory %I64dMB, Page File size %I64dMB.\n",
               SystemMemory,
               PageFileSize);
    }

    InstallPartitionSize = (Context->InstallPartition.LastBlock -
                            Context->InstallPartition.FirstBlock) *
                           Context->InstallPartition.BlockSize;

    InstallPartitionSize /= _1MB;
    if ((InstallPartitionSize != 0) &&
        (PageFileSize >
         (InstallPartitionSize / SETUP_MAX_PAGE_FILE_DISK_DIVISOR))) {

        PageFileSize = InstallPartitionSize / SETUP_MAX_PAGE_FILE_DISK_DIVISOR;
        if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
            printf("Clipping page file to %I64dMB, as install partition is "
                   "only %I64dMB.\n",
                   PageFileSize,
                   InstallPartitionSize);
        }
    }

    Context->PageFileSize = PageFileSize;
    return 0;
}

