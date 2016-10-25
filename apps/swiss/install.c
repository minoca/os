/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    install.c

Abstract:

    This module implements the install utility.

Author:

    Evan Green 19-Apr-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "swlib.h"

//
// ---------------------------------------------------------------- Definitions
//

#define INSTALL_VERSION_MAJOR 1
#define INSTALL_VERSION_MINOR 0

#define INSTALL_USAGE                                                          \
    "usage: install [options] [sources...] [destination]\n"                    \
    "       install [options] -t directory [sources...]\n"                     \
    "       install [options] -d directories...\n"                             \
    "The install utility installs files to their specified destinations.\n"    \
    "Options are:\n"                                                           \
    "  --backup=control -- Create a backup file. Values for control are:\n"    \
    "      none, off -- Never make backups.\n"                                 \
    "      numbered, t -- Make numbered backups\n"                             \
    "      existing, nil -- Numbered if numbered backups exist, simple "       \
    "otherwise.\n"                                                             \
    "      simple, never -- Always make simple backups.\n"                     \
    "  -b -- Like backup, but does not accept an argument.\n"                  \
    "  -c -- Ignored.\n"                                                       \
    "  -C, --compare -- If the target already exists and is the same, do \n"   \
    "      not change the file. Same for the mode.\n"                          \
    "  -d, --directory -- Treat all arguments as directory names. Create \n"   \
    "      all components of the specified directories.\n"                     \
    "  -D -- Create all leading directory components of the destination.\n"    \
    "  -g, --group=group -- Set the group ownership.\n"                        \
    "  -m, --mode=mode -- Set the permissions (as in chmod), instead of "      \
    "0755.\n"                                                                  \
    "  -o, --owner=uid -- Set the file owner.\n"                               \
    "  -p, --preserve-timestamps -- Preserve file access/modification times.\n"\
    "  -s, --strip -- Strip symbol tables.\n"                                  \
    "  --strip-program=program -- Set the program used to strip binaries.\n"   \
    "  -S, --suffix=suffix -- Specify the backup suffix, ~ by default.\n"      \
    "  -t, --target-directory=dir -- Specifies the target directory to \n"     \
    "      install to.\n"                                                      \
    "  -T, --no-target-directory -- Treat the destination as a normal file.\n" \
    "  -v, --verbose -- Print the name of each directory created.\n"           \
    "  --help -- Show this help text and exit.\n"                              \
    "  --version -- Print the application version information and exit.\n"

#define INSTALL_OPTIONS_STRING "bcCdDg:m:o:psS:t:TvVh"

//
// Define the default destination file mode.
//

#define INSTALL_DEFAULT_MODE \
    (S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

//
// Define the default strip program name.
//

#define INSTALL_DEFAULT_STRIP_PROGRAM "strip"

//
// Define the default backup suffix.
//

#define INSTALL_DEFAULT_SUFFIX "~"

//
// Define numeric backup parameters.
//

#define INSTALL_NUMERIC_BACKUP_SUFFIX_SIZE 7
#define INSTALL_MAX_NUMERIC_BACKUP 99999

//
// Define application options.
//

//
// Set this option to compare and not touch destination files that match the
// source.
//

#define INSTALL_OPTION_COMPARE 0x00000001

//
// Set this option to treat all arguments as directories.
//

#define INSTALL_OPTION_DIRECTORY 0x00000002

//
// Set this option to create all leading directory components of the
// destination.
//

#define INSTALL_OPTION_MAKE_COMPONENTS 0x00000004

//
// Set this option to preserve timestamps.
//

#define INSTALL_OPTION_PRESERVE_TIMESTAMPS 0x00000008

//
// Set this option to strip symbol tables.
//

#define INSTALL_OPTION_STRIP 0x00000010

//
// Set this option to treat the destination as a normal file.
//

#define INSTALL_OPTION_DESTINATION_FILE 0x00000020

//
// Set this option to print each installed file.
//

#define INSTALL_OPTION_VERBOSE 0x00000040

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _INSTALL_BACKUP_OPTION {
    InstallBackupInvalid,
    InstallBackupNone,
    InstallBackupNumbered,
    InstallBackupNumberedIfExisting,
    InstallBackupSimple,
} INSTALL_BACKUP_OPTION, *PINSTALL_BACKUP_OPTION;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
InstallCompareFiles (
    PSTR Source,
    PSTR Destination
    );

INT
InstallBackupFile (
    INSTALL_BACKUP_OPTION Option,
    ULONG CopyOptions,
    PSTR FilePath,
    PSTR Suffix
    );

//
// -------------------------------------------------------------------- Globals
//

struct option InstallLongOptions[] = {
    {"backup", required_argument, 0, 'B'},
    {"compare", no_argument, 0, 'C'},
    {"directory", no_argument, 0, 'd'},
    {"group", required_argument, 0, 'g'},
    {"mode", required_argument, 0, 'm'},
    {"owner", required_argument, 0, 'o'},
    {"preserve-timestamps", no_argument, 0, 'p'},
    {"strip-program", required_argument, 0, 'P'},
    {"strip", no_argument, 0, 's'},
    {"suffix", required_argument, 0, 'S'},
    {"target-directory", required_argument, 0, 't'},
    {"no-target-directory", no_argument, 0, 'T'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"verbose", no_argument, 0, 'v'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
InstallMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the install utility.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AppendedPath;
    ULONG AppendedPathSize;
    PSTR Argument;
    ULONG ArgumentIndex;
    INSTALL_BACKUP_OPTION BackupOption;
    PSTR BaseName;
    ULONG CopyOptions;
    PSTR Destination;
    struct stat DestinationStat;
    PSTR DirectoryPart;
    gid_t Group;
    mode_t Mode;
    INT Option;
    ULONG Options;
    uid_t Owner;
    BOOL OwnerSet;
    struct stat SourceStat;
    int Status;
    PSTR StringCopy;
    PSTR StripArguments[4];
    PSTR StripProgram;
    int StripReturn;
    PSTR Suffix;
    PSTR Target;
    BOOL TargetIsDirectory;
    int TotalStatus;
    BOOL Verbose;

    AppendedPath = NULL;
    BackupOption = InstallBackupNone;
    Destination = NULL;
    CopyOptions = COPY_OPTION_FOLLOW_LINKS;
    Group = SwGetRealGroupId();
    Mode = INSTALL_DEFAULT_MODE;
    Options = 0;
    Owner = SwGetRealUserId();
    OwnerSet = FALSE;
    StripProgram = INSTALL_DEFAULT_STRIP_PROGRAM;
    Suffix = INSTALL_DEFAULT_SUFFIX;
    Target = NULL;
    TargetIsDirectory = FALSE;
    TotalStatus = 0;
    Verbose = FALSE;

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             INSTALL_OPTIONS_STRING,
                             InstallLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'B':
            if ((strcmp(optarg, "none") == 0) ||
                (strcmp(optarg, "off") == 0)) {

                BackupOption = InstallBackupNone;

            } else if ((strcmp(optarg, "numbered") == 0) ||
                       (strcmp(optarg, "t") == 0)) {

                BackupOption = InstallBackupNumbered;

            } else if ((strcmp(optarg, "existing") == 0) ||
                       (strcmp(optarg, "nil") == 0)) {

                BackupOption = InstallBackupNumberedIfExisting;

            } else if ((strcmp(optarg, "simple") == 0) ||
                       (strcmp(optarg, "never") == 0)) {

                BackupOption = InstallBackupSimple;

            } else {
                SwPrintError(0, optarg, "Invalid backup control");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'b':
            BackupOption = InstallBackupSimple;
            break;

        case 'c':
            break;

        case 'C':
            Options |= INSTALL_OPTION_COMPARE;
            break;

        case 'd':
            Options |= INSTALL_OPTION_DIRECTORY;
            break;

        case 'D':
            Options |= INSTALL_OPTION_MAKE_COMPONENTS;
            break;

        case 'g':
            Status = SwGetGroupIdFromName(optarg, &Group);
            if (Status != 0) {
                SwPrintError(0, optarg, "Invalid group");
                goto MainEnd;
            }

            OwnerSet = TRUE;
            break;

        case 'm':
            if (SwParseFilePermissionsString(optarg, FALSE, &Mode) == FALSE) {
                SwPrintError(0, optarg, "Invalid mode string");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'o':
            Status = SwGetUserIdFromName(optarg, &Owner);
            if (Status != 0) {
                SwPrintError(0, optarg, "Invalid user");
                goto MainEnd;
            }

            OwnerSet = TRUE;
            break;

        case 'p':
            Options |= INSTALL_OPTION_PRESERVE_TIMESTAMPS;
            CopyOptions |= COPY_OPTION_PRESERVE_PERMISSIONS;
            break;

        case 'P':
            StripProgram = optarg;
            break;

        case 's':
            Options |= INSTALL_OPTION_STRIP;
            break;

        case 'S':
            Suffix = optarg;
            break;

        case 't':
            Target = optarg;
            TargetIsDirectory = TRUE;
            break;

        case 'T':
            Options |= INSTALL_OPTION_DESTINATION_FILE;
            break;

        case 'v':
            Options |= INSTALL_OPTION_VERBOSE;
            CopyOptions |= COPY_OPTION_VERBOSE;
            Verbose = TRUE;
            break;

        case 'V':
            SwPrintVersion(INSTALL_VERSION_MAJOR, INSTALL_VERSION_MINOR);
            return 1;

        case 'h':
            printf(INSTALL_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Arguments expected");
        Status = 1;
        goto MainEnd;
    }

    //
    // If no target was specified and it's not a directory operation, then
    // the last argument is the destination.
    //

    if ((Target == NULL) &&
        ((Options & INSTALL_OPTION_DIRECTORY) == 0)) {

        Target = Arguments[ArgumentCount - 1];
        ArgumentCount -= 1;
        if (((Options & INSTALL_OPTION_DESTINATION_FILE) == 0) &&
            (SwOsStat(Target, TRUE, &DestinationStat) == 0) &&
            (S_ISDIR(DestinationStat.st_mode))) {

            TargetIsDirectory = TRUE;
        }
    }

    if (ArgumentIndex >= ArgumentCount) {
        SwPrintError(0, NULL, "Arguments expected");
        Status = 1;
        goto MainEnd;
    }

    //
    // If there's more than one source and the target is not a directory, that's
    // a problem.
    //

    if ((ArgumentIndex + 1 < ArgumentCount) && (TargetIsDirectory == FALSE) &&
        ((Options & INSTALL_OPTION_DIRECTORY) == 0)) {

        Status = ENOTDIR;
        SwPrintError(TotalStatus, Target, "Extra operand");
        goto MainEnd;
    }

    //
    // Loop through the arguments again and perform the moves.
    //

    Status = 0;
    while (ArgumentIndex < ArgumentCount) {
        if (AppendedPath != NULL) {
            free(AppendedPath);
            AppendedPath = NULL;
        }

        Argument = Arguments[ArgumentIndex];
        ArgumentIndex += 1;

        //
        // Create the directory if this is a directory operation.
        //

        if ((Options & INSTALL_OPTION_DIRECTORY) != 0) {
            Status = SwCreateDirectoryCommand(Argument, TRUE, Verbose, Mode);
            if (Status != 0) {
                SwPrintError(Status, Argument, "Failed to create directory");
                TotalStatus = Status;
            }

            Destination = Argument;

        } else {

            //
            // Create the intermediate components if needed.
            //

            if ((Options & INSTALL_OPTION_MAKE_COMPONENTS) != 0) {
                Destination = strdup(Target);
                if (Destination == NULL) {
                    TotalStatus = ENOMEM;
                    continue;
                }

                DirectoryPart = dirname(Destination);
                Status = SwCreateDirectoryCommand(DirectoryPart,
                                                  TRUE,
                                                  Verbose,
                                                  INSTALL_DEFAULT_MODE);

                if (Status != 0) {
                    SwPrintError(Status,
                                 DirectoryPart,
                                 "Failed to create directory");

                    free(Destination);
                    TotalStatus = Status;
                    continue;
                }

                free(Destination);
            }

            //
            // If the target is a file, just use it directly. If it's a
            // directory, append the file to the directory to get the complete
            // path.
            //

            Destination = Target;
            if (TargetIsDirectory != FALSE) {
                StringCopy = strdup(Argument);
                if (StringCopy == NULL) {
                    TotalStatus = ENOMEM;
                    continue;
                }

                BaseName = basename(StringCopy);
                Status = SwAppendPath(Target,
                                      strlen(Target) + 1,
                                      BaseName,
                                      strlen(BaseName) + 1,
                                      &AppendedPath,
                                      &AppendedPathSize);

                free(StringCopy);
                if (Status == FALSE) {
                    TotalStatus = ENOMEM;
                    continue;
                }

                Destination = AppendedPath;
            }

            //
            // Perform a comparison if requested, and leave the file alone if
            // it's all the same.
            //

            if ((Options & INSTALL_OPTION_COMPARE) != 0) {
                Status = SwStat(Argument, FALSE, &SourceStat);
                if (Status != 0) {
                    SwPrintError(Status, Argument, "Unable to stat");
                    TotalStatus = Status;
                    continue;
                }

                Status = SwStat(Destination, FALSE, &DestinationStat);

                //
                // If the destination exists, has the same size as the source,
                // has the same mode as the source, and has the same user and
                // group (or those aren't set), then compare the file contents.
                // If those are the same, do nothing.
                //

                if ((Status == 0) &&
                    (SourceStat.st_size == DestinationStat.st_size) &&
                    ((DestinationStat.st_mode & ALLPERMS) == Mode) &&
                    ((OwnerSet == FALSE) ||
                     ((DestinationStat.st_uid == Owner) &&
                      (DestinationStat.st_gid == Group)))) {

                    Status = InstallCompareFiles(Argument, Destination);
                    if (Status == 0) {
                        continue;
                    }
                }

                Status = 0;
            }

            //
            // Back up the file if requested.
            //

            if (BackupOption != InstallBackupNone) {
                Status = InstallBackupFile(BackupOption,
                                           CopyOptions,
                                           Destination,
                                           Suffix);

                if (Status != 0) {
                    SwPrintError(Status, Destination, "Failed to back up");
                    TotalStatus = Status;
                    continue;
                }
            }

            //
            // Execute the copy.
            //

            Status = SwCopy(CopyOptions, Argument, Destination);
            if (Status != 0) {
                SwPrintError(Status, Argument, "Failed to install");
                TotalStatus = Status;
                continue;
            }

            //
            // Strip the binary if requested as well.
            //

            if ((Options & INSTALL_OPTION_STRIP) != 0) {
                StripArguments[0] = StripProgram;
                StripArguments[1] = "-p";
                StripArguments[2] = Destination;
                StripArguments[3] = NULL;
                Status = SwRunCommand(StripProgram,
                                      StripArguments,
                                      3,
                                      0,
                                      &StripReturn);

                if (Status != 0) {
                    SwPrintError(Status,
                                 StripProgram,
                                 "Failed to launch strip");

                    TotalStatus = Status;

                } else if (StripReturn != 0) {
                    SwPrintError(0,
                                 StripProgram,
                                 "strip returned %d for '%s'",
                                 StripReturn,
                                 Destination);

                    TotalStatus = StripReturn;
                }
            }
        }

        //
        // Set the file permissions. If the compare option is set, see if the
        // mode is already appropriate.
        //

        Status = chmod(Destination, Mode);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, Destination, "Failed to change mode");
            TotalStatus = Status;
        }

        //
        // Set the file owner.
        //

        if (OwnerSet != FALSE) {
            Status = SwChangeFileOwner(Destination, FALSE, Owner, Group);
            if (Status != 0) {
                SwPrintError(Status, Destination, "Cannot change owner");
                TotalStatus = Status;
            }
        }
    }

MainEnd:
    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
InstallCompareFiles (
    PSTR Source,
    PSTR Destination
    )

/*++

Routine Description:

    This routine compares file contents.

Arguments:

    Source - Supplies the path to the source.

    Destination - Supplies the path to the destination.

Return Value:

    0 if the files are identical.

    -1 if the files differ.

    Returns an error code on failure.

--*/

{

    int DestinationCharacter;
    FILE *DestinationFile;
    int SourceCharacter;
    FILE *SourceFile;
    INT Status;

    SourceFile = NULL;
    DestinationFile = fopen(Destination, "rb");
    if (DestinationFile == NULL) {
        Status = errno;
        goto CompareFilesEnd;
    }

    SourceFile = fopen(Source, "rb");
    if (SourceFile == NULL) {
        Status = errno;
        goto CompareFilesEnd;
    }

    Status = 0;
    while (TRUE) {
        DestinationCharacter = fgetc(DestinationFile);
        SourceCharacter = fgetc(SourceFile);
        if (DestinationCharacter != SourceCharacter) {
            Status = -1;
            goto CompareFilesEnd;
        }

        if (DestinationCharacter == EOF) {
            break;
        }
    }

CompareFilesEnd:
    if (DestinationFile != NULL) {
        fclose(DestinationFile);
    }

    if (SourceFile != NULL) {
        fclose(SourceFile);
    }

    return Status;
}

INT
InstallBackupFile (
    INSTALL_BACKUP_OPTION Option,
    ULONG CopyOptions,
    PSTR FilePath,
    PSTR Suffix
    )

/*++

Routine Description:

    This routine creates a backup of the given file.

Arguments:

    Option - Supplies the backup style.

    CopyOptions - Supplies the options to pass to the copy operation.

    FilePath - Supplies the path to back up.

    Suffix - Supplies the backup suffix.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

{

    PSTR BackupPath;
    UINTN BackupPathSize;
    ULONG Index;
    struct stat Stat;
    INT Status;

    BackupPathSize = strlen(Suffix);
    if (BackupPathSize < INSTALL_NUMERIC_BACKUP_SUFFIX_SIZE) {
        BackupPathSize = INSTALL_NUMERIC_BACKUP_SUFFIX_SIZE;
    }

    BackupPathSize += strlen(FilePath) + 1;
    BackupPath = malloc(BackupPathSize);
    if (BackupPath == NULL) {
        return ENOMEM;
    }

    //
    // If auto is enabled, determine if a numbered backup exists.
    //

    if (Option == InstallBackupNumberedIfExisting) {
        snprintf(BackupPath, BackupPathSize, "%s~%d~", FilePath, 1);
        if (stat(BackupPath, &Stat) == 0) {
            Option = InstallBackupNumbered;

        } else {
            Option = InstallBackupSimple;
        }
    }

    if (Option == InstallBackupNumbered) {
        for (Index = 1; Index < INSTALL_MAX_NUMERIC_BACKUP; Index += 1) {
            snprintf(BackupPath, BackupPathSize, "%s~%u~", FilePath, Index);
            if (stat(BackupPath, &Stat) != 0) {
                break;
            }
        }

        errno = 0;

    } else if (Option == InstallBackupSimple) {
        snprintf(BackupPath, BackupPathSize, "%s%s", FilePath, Suffix);

    } else {

        assert(FALSE);

        Status = EINVAL;
    }

    CopyOptions |= COPY_OPTION_PRESERVE_PERMISSIONS;
    Status = SwCopy(CopyOptions, FilePath, BackupPath);
    if (Status != 0) {
        SwPrintError(Status, FilePath, "Failed to back up to '%s'", BackupPath);
    }

    return Status;
}

