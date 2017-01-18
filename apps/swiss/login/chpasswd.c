/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    chpasswd.c

Abstract:

    This module implements the chpasswd command, which allows passwords to be
    changed in bulk.

Author:

    Evan Green 12-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define CHPASSWD_VERSION_MAJOR 1
#define CHPASSWD_VERSION_MINOR 0

#define CHPASSWD_USAGE                                                         \
    "usage: chpasswd [options]\n"                                              \
    "The chpasswd utility changes user passwords in bulk by reading from \n"   \
    "standard in lines in the form of user:newpassword. Options are:\n"        \
    "  -c, --crypt-method=method -- Use the specified method to encrypt \n"    \
    "      passwords. Valid values are md5, sha256, and sha512.\n"             \
    "  -e, --encrypted -- Specifies that incoming passwords are already "      \
    "encrypted.\n"                                                             \
    "  -S --stdout -- Report encrypted passwords to stdout instead of \n"      \
    "     updating the password file.\n"                                       \
    "  -m, --md5 -- Use the MD5 hashing algorithm.\n"                          \
    "  -R, --root=dir -- Chroot into the given directory before operating.\n"  \
    "  -s, --sha-rounds=rounds -- Use the specified number of rounds to \n"    \
    "      encrypt the passwords. 0 uses the default.\n"                       \
    "  --help -- Displays this help text and exits.\n"                         \
    "  --version -- Displays the application version and exits.\n"

#define CHPASSWD_OPTIONS_STRING "c:eSmR:sHV"

//
// Define application options.
//

//
// Set this option if the passwords are already encrypted.
//

#define CHPASSWD_OPTION_PRE_ENCRYPTED 0x00000001

//
// Set this option to write the results to stdout.
//

#define CHPASSWD_OPTION_WRITE_STDOUT 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

struct option ChpasswdLongOptions[] = {
    {"crypt-method", required_argument, 0, 'c'},
    {"encrypted", no_argument, 0, 'e'},
    {"stdout", required_argument, 0, 'S'},
    {"md5", no_argument, 0, 'm'},
    {"root", required_argument, 0, 'R'},
    {"sha-rounds", required_argument, 0, 's'},
    {"help", no_argument, 0, 'H'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

//
// ------------------------------------------------------------------ Functions
//

INT
ChpasswdMain (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine is the main entry point for the chpasswd utility, which
    allows passwords to be changed in bulk.

Arguments:

    ArgumentCount - Supplies the number of command line arguments the program
        was invoked with.

    Arguments - Supplies a tokenized array of command line arguments.

Return Value:

    Returns an integer exit code. 0 for success, nonzero otherwise.

--*/

{

    PSTR AfterScan;
    PSTR Algorithm;
    PPASSWD_ALGORITHM AlgorithmEntry;
    ULONG ArgumentIndex;
    char *Line;
    size_t LineBufferSize;
    ULONGLONG LineNumber;
    ssize_t LineSize;
    PSTR NewPassword;
    INT Option;
    ULONG Options;
    PSTR Password;
    int RandomSource;
    PSTR RootDirectory;
    ULONG Rounds;
    struct spwd *Shadow;
    int Status;
    int TotalStatus;
    struct passwd *User;
    PSTR UserName;

    Algorithm = PASSWD_DEFAULT_ALGORITHM;
    Line = NULL;
    LineBufferSize = 0;
    NewPassword = NULL;
    Options = 0;
    Password = NULL;
    RandomSource = -1;
    RootDirectory = NULL;
    Rounds = 0;
    TotalStatus = 0;
    UserName = NULL;
    openlog("chpasswd", 0, LOG_AUTH);

    //
    // Process the control arguments.
    //

    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             CHPASSWD_OPTIONS_STRING,
                             ChpasswdLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto MainEnd;
        }

        switch (Option) {
        case 'm':
            optarg = "md5";

            //
            // Fall through.
            //

        case 'c':
            if (strcasecmp(optarg, "des") == 0) {
                SwPrintError(0, NULL, "The DES algorithm has been deprecated");
                Status = 1;
                goto MainEnd;
            }

            AlgorithmEntry = &(SwPasswordAlgorithms[0]);
            while (AlgorithmEntry->Name != NULL) {
                if (strcasecmp(optarg, AlgorithmEntry->Name) == 0) {
                    Algorithm = AlgorithmEntry->Id;
                    break;
                }

                AlgorithmEntry += 1;
            }

            if (AlgorithmEntry->Name == NULL) {
                SwPrintError(0, optarg, "Unknown algorithm");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'e':
            Options |= CHPASSWD_OPTION_PRE_ENCRYPTED;;
            break;

        case 'S':
            Options |= CHPASSWD_OPTION_WRITE_STDOUT;
            break;

        case 'R':
            RootDirectory = optarg;
            break;

        case 's':
            Rounds = strtoul(optarg, &AfterScan, 10);
            if (AfterScan == optarg) {
                SwPrintError(0, optarg, "Invalid rounds");
                Status = 1;
                goto MainEnd;
            }

            break;

        case 'V':
            SwPrintVersion(CHPASSWD_VERSION_MAJOR, CHPASSWD_VERSION_MINOR);
            return 1;

        case 'H':
            printf(CHPASSWD_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto MainEnd;
        }
    }

    ArgumentIndex = optind;
    if (ArgumentIndex > ArgumentCount) {
        ArgumentIndex = ArgumentCount;
    }

    if (ArgumentIndex != ArgumentCount) {
        SwPrintError(0, NULL, "Unexpected arguments");
        Status = 1;
        goto MainEnd;
    }

    if (getuid() != 0) {
        SwPrintError(0, NULL, "You must be root to do this");
        Status = 1;
        goto MainEnd;
    }

    //
    // Try to open /dev/urandom before chrooting in case the root doesn't have
    // it. Don't freak out if this fails, as maybe the root does have it.
    //

    RandomSource = SwOpen(URANDOM_PATH, O_RDONLY, 0);

    //
    // Chroot if requested. Warm up crypt first in case libcrypt isn't in the
    // chrooted environment.
    //

    if (RootDirectory != NULL) {
        SwCrypt(NULL, NULL);
        Status = chroot(RootDirectory);
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chroot");
            goto MainEnd;
        }

        Status = chdir("/");
        if (Status != 0) {
            Status = errno;
            SwPrintError(Status, RootDirectory, "Failed to chdir");
            goto MainEnd;
        }
    }

    //
    // Loop reading lines and changing passwords.
    //

    LineNumber = 1;
    while (TRUE) {
        if (Line != NULL) {
            SECURITY_ZERO(Line, strlen(Line));
        }

        if ((NewPassword != Password) && (NewPassword != NULL)) {
            SECURITY_ZERO(NewPassword, strlen(NewPassword));
            NewPassword = NULL;
        }

        LineSize = getline(&Line, &LineBufferSize, stdin);
        if (LineSize < 0) {
            break;
        }

        while ((LineSize != 0) && (isspace(Line[LineSize - 1]))) {
            LineSize -= 1;
        }

        UserName = Line;
        UserName[LineSize] = '\0';

        //
        // Get past whitespace.
        //

        while (isspace(*UserName)) {
            UserName += 1;
        }

        //
        // Skip any commented lines.
        //

        if ((*UserName == '\0') || (*UserName == '#')) {
            LineNumber += 1;
            continue;
        }

        Password = strchr(UserName, ':');
        if (Password == NULL) {
            SwPrintError(0, NULL, "Line %I64d missing password", LineNumber);
            TotalStatus = 1;
            LineNumber += 1;
            continue;
        }

        if (Password == UserName) {
            SwPrintError(0, NULL, "Line %I64d missing username", LineNumber);
            TotalStatus = 1;
            LineNumber += 1;
            continue;
        }

        *Password = '\0';
        Password += 1;
        User = getpwnam(UserName);
        if (User == NULL) {
            SwPrintError(0,
                         NULL,
                         "User %s not found (line %I64d)",
                         UserName,
                         LineNumber);

            TotalStatus = 1;
            LineNumber += 1;
            continue;
        }

        errno = 0;
        Shadow = getspnam(UserName);
        if ((Shadow == NULL) && (errno != ENOENT)) {
            if ((errno == EPERM) || (errno == EACCES)) {
                SwPrintError(errno, NULL, "Cannot access the password file");
                Status = 1;
                goto MainEnd;
            }

            SwPrintError(0,
                         NULL,
                         "Shadow entry not found for user %s on line %I64d",
                         UserName,
                         LineNumber);

            TotalStatus = 1;
            LineNumber += 1;
            continue;
        }

        //
        // Get the new password, either it's pre-encrypted, or a hashed
        // password is created.
        //

        if ((Options & CHPASSWD_OPTION_PRE_ENCRYPTED) != 0) {
            NewPassword = Password;
            if (strchr(NewPassword, ':') != NULL) {
                SwPrintError(0,
                             NULL,
                             "Supposedly encrypted password has a colon");

                TotalStatus = 1;
                LineNumber += 1;
                continue;
            }

        } else {
            NewPassword = SwCreateHashedPassword(Algorithm,
                                                 RandomSource,
                                                 Rounds,
                                                 Password);
        }

        //
        // Update shadow if it exists or just plain passwd if not.
        //

        if (Shadow != NULL) {
            Shadow->sp_pwdp = NewPassword;
            Shadow->sp_lstchg = time(NULL) / (3600 * 24);
            User->pw_passwd = PASSWORD_SHADOWED;

        } else {
            User->pw_passwd = NewPassword;
        }

        //
        // If writing to stdout, just print.
        //

        if ((Options & CHPASSWD_OPTION_WRITE_STDOUT) != 0) {
            printf("%s:%s\n", UserName, NewPassword);

        //
        // Actually change the password.
        //

        } else {
            Status = SwUpdatePasswordLine(User,
                                          Shadow,
                                          UpdatePasswordUpdateLine);

            if (Status != 0) {
                syslog(LOG_ERR,
                       "Failed to change password for user %s on line %lld",
                       UserName,
                       LineNumber);

                SwPrintError(0,
                             NULL,
                             "Failed to change password for user %s on line "
                             "%I64d",
                             User->pw_name,
                             LineNumber);

                TotalStatus = Status;

            } else {
                syslog(LOG_NOTICE, "Changed password for user %s", UserName);
            }
        }

        LineNumber += 1;
    }

    Status = 0;

MainEnd:
    if (RandomSource >= 0) {
        close(RandomSource);
    }

    if (Line != NULL) {
        SECURITY_ZERO(Line, strlen(Line));
        free(Line);
    }

    if (NewPassword != NULL) {
        SECURITY_ZERO(NewPassword, strlen(NewPassword));
    }

    closelog();
    if ((TotalStatus == 0) && (Status != 0)) {
        TotalStatus = Status;
    }

    return TotalStatus;
}

//
// --------------------------------------------------------- Internal Functions
//

