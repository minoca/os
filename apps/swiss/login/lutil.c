/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lutil.c

Abstract:

    This module implements utility functions for the login commands.

Author:

    Evan Green 10-Mar-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _GNU_SOURCE 1

#include <minoca/lib/types.h>

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <utmp.h>
#include <utmpx.h>

#include "../swlib.h"
#include "lutil.h"

//
// ---------------------------------------------------------------- Definitions
//

#define UPDATE_PASSWORD_WAIT 10
#define PASSWORD_LINE_MAX 2048
#define GROUP_LINE_MAX 4096

#define LIBCRYPT_PATH "/lib/libcrypt.so.1"

#define SALT_ALPHABET \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

#define PASSWORD_ROUNDS_MIN 1000
#define PASSWORD_ROUNDS_MAX 999999999

#ifndef _PATH_WTMPX
#define _PATH_WTMPX _PATH_WTMP
#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
char *
(*PCRYPT_FUNCTION) (
    const char *Key,
    const char *Salt
    );

/*++

Routine Description:

    This routine encrypts a user's password using various encryption/hashing
    standards. The default is DES, which is fairly weak and subject to
    dictionary attacks.

Arguments:

    Key - Supplies the key, a user's plaintext password.

    Salt - Supplies a two character salt to use to perterb the results. If this
        string starts with a $ and a number, alternate hashing algorithms are
        selected. The format is $id$salt$encrypted. ID can be 1 for MD5, 5 for
        SHA-256, or 6 for SHA-512.

Return Value:

    Returns a pointer to the encrypted password (plus ID and salt information
    in cases where an alternate mechanism is used). This is a static buffer,
    which may be overwritten by subsequent calls to crypt.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
SwpPrintShadowLine (
    PSTR Line,
    UINTN LineSize,
    struct spwd *Shadow
    );

VOID
SwpWriteNewUtmpEntry (
    pid_t ProcessId,
    int NewType,
    PSTR TerminalName,
    PSTR UserName,
    PSTR HostName
    );

//
// -------------------------------------------------------------------- Globals
//

extern char **environ;

PASSWD_ALGORITHM SwPasswordAlgorithms[] = {
    {"md5", "$1$"},
    {"sha256", "$5$"},
    {"sha512", "$6$"},
    {NULL, NULL}
};

void *SwLibCrypt = NULL;
PCRYPT_FUNCTION SwCryptFunction = NULL;

PSTR SwDangerousEnvironmentVariables[] = {
    "ENV",
    "BASH_ENV",
    "HOME",
    "IFS",
    "SHELL",
    "LD_LIBRARY_PATH",
    "LD_PRELOAD",
    "LD_TRACE_LOADED_OBJECTS",
    "LD_BIND_NOW",
    "LD_AOUT_LIBRARY_PATH",
    "LD_AOUT_PRELOAD",
    "LD_NOWARN",
    "LD_KEEPDIR",
    NULL
};

const struct spwd SwShadowTemplate = {
    NULL,
    "!",
    0,
    0,
    99999,
    7,
    -1,
    -1,
    -1
};

//
// ------------------------------------------------------------------ Functions
//

INT
SwUpdatePasswordLine (
    struct passwd *User,
    struct spwd *Shadow,
    UPDATE_PASSWORD_OPERATION Operation
    )

/*++

Routine Description:

    This routine adds or updates an entry in the password database.

Arguments:

    User - Supplies a pointer to the user information structure.

    Shadow - Supplies a pointer to the shadow information.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR Gecos;
    PSTR Home;
    CHAR Line[PASSWORD_LINE_MAX];
    PSTR Password;
    int Result;
    PSTR Shell;

    assert((Operation == UpdatePasswordAddLine) ||
           (Operation == UpdatePasswordUpdateLine) ||
           (Operation == UpdatePasswordDeleteLine));

    Password = "x";
    Gecos = "";
    Home = "";
    Shell = "";
    if (User->pw_passwd != NULL) {
        Password = User->pw_passwd;
    }

    if (User->pw_gecos != NULL) {
        Gecos = User->pw_gecos;
    }

    if (User->pw_dir != NULL) {
        Home = User->pw_dir;
    }

    if (User->pw_shell != NULL) {
        Shell = User->pw_shell;
    }

    if ((User->pw_name[0] == '+') || (User->pw_name[0] == '-')) {
        Result = snprintf(Line,
                          PASSWORD_LINE_MAX,
                          "%s:%s:::%s:%s:%s",
                          User->pw_name,
                          Password,
                          Gecos,
                          Home,
                          Shell);

    } else {
        Result = snprintf(Line,
                          PASSWORD_LINE_MAX,
                          "%s:%s:%lu:%lu:%s:%s:%s",
                          User->pw_name,
                          Password,
                          (unsigned long int)(User->pw_uid),
                          (unsigned long int)(User->pw_gid),
                          Gecos,
                          Home,
                          Shell);
    }

    if ((Result >= PASSWORD_LINE_MAX) || (Result < 0)) {
        return ENAMETOOLONG;
    }

    Result = SwUpdatePasswordFile(PASSWD_FILE_PATH,
                                  User->pw_name,
                                  Line,
                                  NULL,
                                  Operation);

    if (Result != 0) {
        return Result;
    }

    //
    // Update the shadow file as well.
    //

    if (Shadow != NULL) {
        if (SwpPrintShadowLine(Line, PASSWORD_LINE_MAX, Shadow) < 0) {
            return ENAMETOOLONG;
        }

        Result = SwUpdatePasswordFile(_PATH_SHADOW,
                                      User->pw_name,
                                      Line,
                                      NULL,
                                      Operation);

        if (Result != 0) {
            return Result;
        }
    }

    return 0;
}

INT
SwUpdateGroupLine (
    struct group *Group,
    UPDATE_PASSWORD_OPERATION Operation
    )

/*++

Routine Description:

    This routine adds or updates an entry in the group database.

Arguments:

    Group - Supplies the group to add or update.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentLine;
    ssize_t CurrentLineSize;
    UINTN Index;
    PSTR Line;
    PSTR Password;
    ssize_t Size;
    INT Status;

    Line = malloc(GROUP_LINE_MAX);
    if (Line == NULL) {
        return ENOMEM;
    }

    Password = "";
    if (Group->gr_passwd != NULL) {
        Password = Group->gr_passwd;
    }

    Size = snprintf(Line,
                    GROUP_LINE_MAX,
                    "%s:%s:%lu:",
                    Group->gr_name,
                    Password,
                    (long unsigned int)(Group->gr_gid));

    if ((Size <= 0) || (Size >= GROUP_LINE_MAX)) {
        Status = EINVAL;
        goto UpdateGroupLineEnd;
    }

    CurrentLine = Line + Size;
    CurrentLineSize = GROUP_LINE_MAX - Size;
    Index = 0;
    if (Group->gr_mem != NULL) {
        while (Group->gr_mem[Index] != NULL) {
            if (Index == 0) {
                Size = snprintf(CurrentLine,
                                CurrentLineSize,
                                "%s",
                                Group->gr_mem[Index]);

            } else {
                Size = snprintf(CurrentLine,
                                CurrentLineSize,
                                ",%s",
                                Group->gr_mem[Index]);
            }

            if ((Size <= 0) || (Size >= CurrentLineSize)) {
                Status = EINVAL;
                goto UpdateGroupLineEnd;
            }

            CurrentLine += Size;
            CurrentLineSize -= Size;
            Index += 1;
        }
    }

    Status = SwUpdatePasswordFile(GROUP_FILE_PATH,
                                  Group->gr_name,
                                  Line,
                                  NULL,
                                  Operation);

UpdateGroupLineEnd:
    if (Line != NULL) {
        free(Line);
    }

    return Status;
}

INT
SwUpdatePasswordFile (
    PSTR FilePath,
    PSTR Name,
    PSTR NewLine,
    PSTR GroupMember,
    UPDATE_PASSWORD_OPERATION Operation
    )

/*++

Routine Description:

    This routine updates a password file, usually either passwd, groups,
    shadow, or gshadow.

Arguments:

    FilePath - Supplies a pointer to the path of the file to update.

    Name - Supplies the name of the member to update.

    NewLine - Supplies the new line to set.

    GroupMember - Supplies the group member to add or remove for group
        operations.

    Operation - Supplies the operation to perform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AppendedPath;
    UINTN AppendedPathSize;
    UINTN ChangedLines;
    PSTR Colon;
    PSTR CurrentMember;
    char *Line;
    size_t LineBufferSize;
    ssize_t LineSize;
    struct flock Lock;
    size_t NameSize;
    FILE *NewFile;
    int NewFileDescriptor;
    PSTR NextComma;
    FILE *OldFile;
    mode_t OldUmask;
    PSTR Separator;
    UINTN Stall;
    struct stat Stat;
    INT Status;

    Line = NULL;
    LineBufferSize = 0;
    OldFile = NULL;
    OldUmask = umask(S_IWGRP | S_IWOTH | S_IROTH);
    NameSize = strlen(Name);
    NewFileDescriptor = -1;
    AppendedPathSize = strlen(FilePath) + 5;
    AppendedPath = malloc(AppendedPathSize);
    if (AppendedPath == NULL) {
        Status = ENOMEM;
        goto UpdatePasswordFileEnd;
    }

    snprintf(AppendedPath, AppendedPathSize, "%s.tmp", FilePath);
    OldFile = fopen(FilePath, "r+");
    if (OldFile == NULL) {
        Status = errno;
        SwPrintError(Status, FilePath, "Cannot open");
        goto UpdatePasswordFileEnd;
    }

    //
    // Try to open the new file, being a bit patient.
    //

    NewFileDescriptor = -1;
    for (Stall = 0; Stall < UPDATE_PASSWORD_WAIT; Stall += 1) {
        NewFileDescriptor = SwOpen(AppendedPath,
                                   O_WRONLY | O_CREAT | O_EXCL,
                                   S_IRUSR | S_IWUSR);

        if (NewFileDescriptor >= 0) {
            break;
        }

        if (errno != EEXIST) {
            Status = errno;
            SwPrintError(Status, AppendedPath, "Could not create");
            goto UpdatePasswordFileEnd;
        }

        sleep(1);
    }

    if (fstat(fileno(OldFile), &Stat) == 0) {
        fchmod(NewFileDescriptor, Stat.st_mode & ALLPERMS);
        Status = fchown(NewFileDescriptor, Stat.st_uid, Stat.st_gid);
        if (Status != 0) {
            Status = errno;
            goto UpdatePasswordFileEnd;
        }
    }

    if (NewFileDescriptor <= 0) {
        Status = errno;
        SwPrintError(Status, AppendedPath, "Could not create");
        goto UpdatePasswordFileEnd;
    }

    NewFile = fdopen(NewFileDescriptor, "w");
    if (NewFile == NULL) {
        Status = errno;
        goto UpdatePasswordFileEnd;
    }

    NewFileDescriptor = -1;

    //
    // Lock the file.
    //

    Lock.l_type = F_WRLCK;
    Lock.l_whence = SEEK_SET;
    Lock.l_start = 0;
    Lock.l_len = 0;
    if (fcntl(fileno(OldFile), F_SETLK, &Lock) < 0) {
        Status = errno;
        SwPrintError(Status, FilePath, "Cannot lock file");
        goto UpdatePasswordFileEnd;
    }

    ChangedLines = 0;
    while (TRUE) {
        LineSize = getline(&Line, &LineBufferSize, OldFile);
        if (LineSize <= 0) {
            break;
        }

        while ((LineSize > 0) && (isspace(Line[LineSize - 1]) != 0)) {
            Line[LineSize - 1] = '\0';
            LineSize -= 1;
        }

        //
        // If the line is uninteresting, spit it to the output and move on.
        //

        if ((strncmp(Line, Name, NameSize) != 0) || (Line[NameSize] != ':')) {
            fprintf(NewFile, "%s\n", Line);
            continue;
        }

        //
        // Add or remove a member from a group.
        //

        if (GroupMember != NULL) {
            if (Operation == UpdatePasswordAddGroupMember) {
                Separator = ",";
                if (Line[LineSize - 1] == ':') {
                    Separator = "";
                }

                fprintf(NewFile, "%s%s%s\n", Line, Separator, GroupMember);
                ChangedLines += 1;

            //
            // Delete a user from a group.
            //

            } else {

                assert(Operation == UpdatePasswordDeleteGroupMember);

                Colon = strrchr(Line, ':');
                if (Colon == NULL) {
                    fprintf(NewFile, "%s\n", Line);
                    continue;
                }

                //
                // Write out everything up to the group list.
                //

                *Colon = '\0';
                fprintf(NewFile, "%s:", Line);

                //
                // Loop writing out the members, unless it's the member of
                // honor.
                //

                Separator = "";
                CurrentMember = Colon + 1;
                while (CurrentMember != NULL) {
                    NextComma = strchr(CurrentMember, ',');
                    if (NextComma != NULL) {
                        *NextComma = '\0';
                        NextComma += 1;
                    }

                    if (strcmp(CurrentMember, GroupMember) != 0) {
                        fprintf(NewFile, "%s%s", Separator, CurrentMember);
                        Separator = ",";

                    } else {
                        ChangedLines += 1;
                    }

                    CurrentMember = NextComma;
                }

                fprintf(NewFile, "\n");
            }

        //
        // There is no group member, this must be a passwd or shadow update.
        //

        } else {
            if ((Operation == UpdatePasswordAddLine) ||
                (Operation == UpdatePasswordUpdateLine)) {

                fprintf(NewFile, "%s\n", NewLine);
                ChangedLines += 1;

            //
            // For deleting an entry, just do nothing, and it won't make it to
            // the output file.
            //

            } else {

                assert(Operation == UpdatePasswordDeleteLine);

                ChangedLines += 1;
            }
        }
    }

    Status = 0;
    if (ChangedLines == 0) {
        if (Operation != UpdatePasswordAddLine) {
            SwPrintError(0, NULL, "Cannot find '%s' in '%s'", Name, FilePath);
            Status = 1;

        } else {
            fprintf(NewFile, "%s\n", NewLine);
            ChangedLines += 1;
        }
    }

    //
    // Unlock the password file.
    //

    Lock.l_type = F_UNLCK;
    fcntl(fileno(OldFile), F_SETLK, &Lock);
    errno = 0;
    fflush(NewFile);
    fsync(fileno(NewFile));
    fclose(NewFile);
    NewFile = NULL;
    if (errno != 0) {
        Status = errno;
        SwPrintError(Status, AppendedPath, "Failed to sync/close");
        goto UpdatePasswordFileEnd;
    }

    if (rename(AppendedPath, FilePath) != 0) {
        Status = errno;
        SwPrintError(Status, FilePath, "Failed to move");
        goto UpdatePasswordFileEnd;
    }

    Status = 0;

UpdatePasswordFileEnd:
    umask(OldUmask);
    if (OldFile != NULL) {
        fclose(OldFile);
    }

    if (Status != 0) {
        if (AppendedPath != NULL) {
            unlink(AppendedPath);
        }
    }

    if (AppendedPath != NULL) {
        free(AppendedPath);
    }

    if (NewFileDescriptor >= 0) {
        close(NewFileDescriptor);
    }

    if (Line != NULL) {
        free(Line);
    }

    return Status;
}

PSTR
SwCreateHashedPassword (
    PSTR Algorithm,
    int RandomSource,
    UINTN Rounds,
    PSTR Password
    )

/*++

Routine Description:

    This routine creates a hashed password, choosing an algorithm and a salt.

Arguments:

    Algorithm - Supplies the algorithm to use. Valid values are something like
        "$1$", "$5$", or "$6$".

    RandomSource - Supplies an optional file descriptor sourcing random data.

    Rounds - Supplies the rounds parameter for SHA256 and SHA512 algorithms.
        Supply 0 to use a default value.

    Password - Supplies the plaintext password to encode.

Return Value:

    Returns a pointer to the hashed password on success. This comes from a
    static buffer and may be overwritten by subsequent calls to this function.

    NULL on failure.

--*/

{

    ssize_t BytesRead;
    char Salt[17];
    int SaltIndex;
    CHAR SaltLine[64];
    int URandom;

    if (RandomSource >= 0) {
        URandom = RandomSource;

    } else {
        URandom = SwOpen(URANDOM_PATH, O_RDONLY, 0);
        if (URandom < 0) {
            SwPrintError(errno, URANDOM_PATH, "Failed to open random source");
            return NULL;
        }
    }

    do {
        BytesRead = read(URandom, Salt, sizeof(Salt));

    } while ((BytesRead < 0) && (errno == EINTR));

    if (URandom != RandomSource) {
        close(URandom);
    }

    if (BytesRead != sizeof(Salt)) {
        SwPrintError(errno, URANDOM_PATH, "Failed to read random source");
        return NULL;
    }

    for (SaltIndex = 0; SaltIndex < sizeof(Salt); SaltIndex += 1) {
        Salt[SaltIndex] = SALT_ALPHABET[(UCHAR)(Salt[SaltIndex]) % 62];
    }

    Salt[sizeof(Salt) - 1] = '\0';

    //
    // Only a couple algorithms support rounds, namely SHA256 and SHA512.
    //

    if ((Rounds != 0) &&
        (strcmp(Algorithm, "$5$") != 0) &&
        (strcmp(Algorithm, "$6$") != 0)) {

        Rounds = 0;
    }

    if (Rounds == 0) {
        snprintf(SaltLine,
                 sizeof(SaltLine),
                 "%s%s",
                 Algorithm,
                 Salt);

    } else {
        if (Rounds < PASSWORD_ROUNDS_MIN) {
            Rounds = PASSWORD_ROUNDS_MIN;

        } else if (Rounds > PASSWORD_ROUNDS_MAX) {
            Rounds = PASSWORD_ROUNDS_MAX;
        }

        snprintf(SaltLine,
                 sizeof(SaltLine),
                 "%srounds=%d$%s",
                 Algorithm,
                 (int)Rounds,
                 Salt);
    }

    return SwCrypt(Password, SaltLine);
}

INT
SwCheckAccount (
    struct passwd *User
    )

/*++

Routine Description:

    This routine validates that the account is enabled and can be logged in via
    password.

Arguments:

    User - Supplies a pointer to the user to get information for.

Return Value:

    0 if account is enabled.

    Non-zero if the account cannot be logged in to via password.

--*/

{

    PSTR HashedPassword;
    struct spwd *Shadow;

    HashedPassword = User->pw_passwd;
    Shadow = getspnam(User->pw_name);
    if ((Shadow == NULL) && (errno != ENOENT)) {
        SwPrintError(errno,
                     User->pw_name,
                     "Error: Could not read password information for user");

        return EACCES;
    }

    if (Shadow != NULL) {
        HashedPassword = Shadow->sp_pwdp;
    }

    if (*HashedPassword == '\0') {
        return 0;
    }

    if (*HashedPassword == '!') {
        SwPrintError(0, NULL, "Account locked");
        return EACCES;
    }

    if ((!isalnum(*HashedPassword)) &&
        (*HashedPassword != '/') &&
        (*HashedPassword != '.') &&
        (*HashedPassword != '_') &&
        (*HashedPassword != '$')) {

        SwPrintError(0, NULL, "Account disabled");
        return EACCES;
    }

    return 0;
}

INT
SwGetAndCheckPassword (
    struct passwd *User,
    PSTR Prompt
    )

/*++

Routine Description:

    This routine asks for and validates the password for the given user.

Arguments:

    User - Supplies a pointer to the user to get information for.

    Prompt - Supplies an optional pointer to the prompt to use. Supply NULL
        to use the prompt "Enter password:".

Return Value:

    0 if the password matched.

    EPERM if the password did not match.

    EACCES if the account is locked or expired.

    Other error codes on other failures.

--*/

{

    BOOL Correct;
    PSTR HashedPassword;
    PSTR Password;
    struct spwd *Shadow;

    HashedPassword = NULL;
    if (User != NULL) {
        HashedPassword = User->pw_passwd;
        Shadow = getspnam(User->pw_name);
        if ((Shadow == NULL) && (errno != ENOENT)) {
            SwPrintError(errno,
                         User->pw_name,
                         "Error: Could not read password information for user");

            return EACCES;
        }

        if (Shadow != NULL) {
            HashedPassword = Shadow->sp_pwdp;
        }

        if (*HashedPassword == '!') {
            SwPrintError(0, NULL, "Account locked");
            return EACCES;
        }

        //
        // If the password is empty just return success, no need to ask really.
        //

        if (*HashedPassword == '\0') {
            return 0;
        }
    }

    if (Prompt == NULL) {
        Prompt = "Enter password: ";
    }

    Password = getpass(Prompt);
    if (Password == NULL) {
        if (errno != 0) {
            return errno;
        }

        return EACCES;
    }

    if (User == NULL) {
        return EPERM;
    }

    Correct = SwCheckPassword(Password, HashedPassword);
    memset(Password, 0, strlen(Password));
    if (Correct != FALSE) {
        return 0;
    }

    return EPERM;
}

BOOL
SwCheckPassword (
    PSTR Password,
    PSTR EncryptedPassword
    )

/*++

Routine Description:

    This routine checks a password against its hash.

Arguments:

    Password - Supplies a pointer to the plaintext password to check.

    EncryptedPassword - Supplies the password hash.

Return Value:

    TRUE if the passwords match.

    FALSE if the passwords do not match.

--*/

{

    PSTR Result;

    Result = SwCrypt(Password, EncryptedPassword);

    assert(Result != EncryptedPassword);

    if (strcmp(Result, EncryptedPassword) == 0) {
        return TRUE;
    }

    return FALSE;
}

PSTR
SwCrypt (
    PSTR Password,
    PSTR Salt
    )

/*++

Routine Description:

    This routine calls the crypt function off in libcrypt.

Arguments:

    Password - Supplies the plaintext password to encode.

    Salt - Supplies the algorithm and salt information.

Return Value:

    Returns a pointer to the hashed password on success. This comes from a
    static buffer and may be overwritten by subsequent calls to this function.

    NULL on failure.

--*/

{

    //
    // Get the address of the crypt function from libcrypt if not found already.
    //

    if (SwCryptFunction == NULL) {
        if (SwLibCrypt == NULL) {
            SwLibCrypt = dlopen(LIBCRYPT_PATH, 0);
            if (SwLibCrypt == NULL) {
                SwPrintError(0,
                             NULL,
                             "Failed to open %s: %s",
                             LIBCRYPT_PATH,
                             dlerror());

                return NULL;
            }
        }

        SwCryptFunction = dlsym(SwLibCrypt, "crypt");
        if (SwCryptFunction == NULL) {
            SwPrintError(0, NULL, "Failed to find crypt in libcrypt.so");
            return NULL;
        }
    }

    if (Password == NULL) {
        return NULL;
    }

    return SwCryptFunction(Password, Salt);
}

BOOL
SwIsValidUserName (
    PSTR Name
    )

/*++

Routine Description:

    This routine validates that the given username doesn't have any invalid
    characters.

Arguments:

    Name - Supplies a pointer to the user name to check.

Return Value:

    TRUE if it is valid.

    FALSE if it is not valid.

--*/

{

    PSTR Start;

    Start = Name;
    if ((*Name == '-') || (*Name == '.') || (*Name == '\0')) {
        return FALSE;
    }

    while (*Name != '\0') {
        if ((*Name != '_') && (!isalnum(*Name)) && (*Name != '.') &&
            (*Name != '-')) {

            return FALSE;
        }

        Name += 1;
    }

    if (*(Name - 1) == '$') {
        return FALSE;
    }

    if (Name - Start >= LOGIN_NAME_MAX) {
        return FALSE;
    }

    return TRUE;
}

INT
SwBecomeUser (
    struct passwd *User
    )

/*++

Routine Description:

    This routine changes the current identity to that of the given user,
    including the real user ID, group ID, and supplementary groups.

Arguments:

    User - Supplies a pointer to the user to become.

Return Value:

    None.

--*/

{

    INT Result;
    INT Status;

    Status = 0;
    Result = initgroups(User->pw_name, User->pw_gid);
    if (Result < 0) {
        Status = errno;
        SwPrintError(Status, User->pw_name, "Failed to init groups for");
        if (Status == EPERM) {
            return Status;
        }
    }

    Result = setgid(User->pw_gid);
    if (Result < 0) {
        Status = errno;
        SwPrintError(Status, User->pw_name, "Failed to set gid for");
    }

    Result = setuid(User->pw_uid);
    if (Result < 0) {
        Status = errno;
        SwPrintError(Status, User->pw_name, "Failed to set uid for");
    }

    return Status;
}

VOID
SwSetupUserEnvironment (
    struct passwd *User,
    PSTR Shell,
    ULONG Flags
    )

/*++

Routine Description:

    This routine changes the current identity to that of the given user,
    including the real user ID, group ID, and supplementary groups.

Arguments:

    User - Supplies a pointer to the user to set up for.

    Shell - Supplies a pointer to the shell to use. If not specified
        the ever-classic /bin/sh will be used.

    Flags - Supplies a bitfield of flags. See SETUP_USER_ENVIRONMENT_*
        definitions.

Return Value:

    None.

--*/

{

    PSTR Path;
    PSTR Terminal;

    if ((Shell == NULL) || (*Shell == '\0')) {
        Shell = USER_FALLBACK_SHELL;
    }

    //
    // Change the current directory to the user's home directory.
    //

    if ((Flags & SETUP_USER_ENVIRONMENT_NO_DIRECTORY) == 0) {
        if ((User->pw_dir != NULL) && (User->pw_dir[0] != '\0')) {
            if (chdir(User->pw_dir) < 0) {
                SwPrintError(errno, User->pw_dir, "Cannot change to directory");
            }
        }
    }

    //
    // If the caller wants to clear the environment, wipe out everything.
    // Preserve TERM.
    //

    if ((Flags & SETUP_USER_ENVIRONMENT_CLEAR_ENVIRONMENT) != 0) {
        Terminal = getenv("TERM");
        environ = NULL;
        if (Terminal != NULL) {
            setenv("TERM", Terminal, 1);
        }

        Path = USER_DEFAULT_PATH;
        if (User->pw_uid == 0) {
            Path = SUPERUSER_DEFAULT_PATH;
        }

        setenv("PATH", Path, 1);
        setenv("USER", User->pw_name, 1);
        setenv("LOGNAME", User->pw_name, 1);
        setenv("HOME", User->pw_dir, 1);
        setenv("SHELL", Shell, 1);

    } else if ((Flags & SETUP_USER_ENVIRONMENT_CHANGE_ENVIRONMENT) != 0) {
        if (User->pw_uid != 0) {
            setenv("USER", User->pw_name, 1);
            setenv("LOGNAME", User->pw_name, 1);
        }

        setenv("HOME", User->pw_dir, 1);
        setenv("SHELL", Shell, 1);
    }

    return;
}

VOID
SwExecuteShell (
    PSTR Shell,
    BOOL LoginShell,
    PSTR Command,
    PSTR *AdditionalArguments
    )

/*++

Routine Description:

    This routine execs this program into a shell program with the given
    arguments.

Arguments:

    Shell - Supplies a pointer to the shell to use. If not specified
        the ever-classic /bin/sh will be used.

    LoginShell - Supplies a boolean indicating if this is a login shell or
        not.

    Command - Supplies the command to run.

    AdditionalArguments - Supplies an array of the additional arguments.

Return Value:

    None. On success, this routine does not return.

--*/

{

    UINTN ArgumentCount;
    PSTR *Arguments;
    UINTN Index;
    PSTR LastSlash;
    PSTR LoginLine;

    LoginLine = NULL;
    ArgumentCount = 0;
    if (AdditionalArguments != NULL) {
        while (AdditionalArguments[ArgumentCount] != NULL) {
            ArgumentCount += 1;
        }
    }

    //
    // Add four extra for the sh, -c, command, and NULL.
    //

    Arguments = malloc(sizeof(PSTR) * (ArgumentCount + 4));
    if (Arguments == NULL) {
        return;
    }

    if ((Shell == NULL) || (*Shell == '\0')) {
        Shell = USER_FALLBACK_SHELL;
    }

    LastSlash = strrchr(Shell, '/');
    if ((LastSlash == NULL) ||
        ((LastSlash == Shell) && (LastSlash[1] == '\0'))) {

        LastSlash = Shell;

    } else {
        LastSlash += 1;
    }

    Arguments[0] = LastSlash;
    if (LoginShell != FALSE) {
        LoginLine = malloc(strlen(LastSlash) + 2);
        if (LoginLine != NULL) {
            *LoginLine = '-';
            strcpy(LoginLine + 1, LastSlash);
            Arguments[0] = LoginLine;
        }
    }

    Index = 1;
    if (Command != NULL) {
        Arguments[Index] = "-c";
        Index += 1;
        Arguments[Index] = Command;
        Index += 1;
    }

    if (AdditionalArguments != NULL) {
        while (*AdditionalArguments != NULL) {
            Arguments[Index] = *AdditionalArguments;
            Index += 1;
            AdditionalArguments += 1;
        }
    }

    Arguments[Index] = NULL;

    assert(Index < ArgumentCount + 4);

    execv(Shell, Arguments);
    SwPrintError(errno, Shell, "Cannot execute");
    free(Arguments);
    if (LoginLine != NULL) {
        free(LoginLine);
    }

    return;
}

VOID
SwSanitizeEnvironment (
    VOID
    )

/*++

Routine Description:

    This routine removes several dangerous environment variables from the
    environment, and resets the PATH.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSTR *Variable;

    Variable = &(SwDangerousEnvironmentVariables[0]);
    while (*Variable != NULL) {
        unsetenv(*Variable);
        Variable += 1;
    }

    if (geteuid() != 0) {
        setenv("PATH", USER_DEFAULT_PATH, 1);

    } else {
        setenv("PATH", SUPERUSER_DEFAULT_PATH, 1);
    }

    return;
}

VOID
SwPrintLoginPrompt (
    VOID
    )

/*++

Routine Description:

    This routine prints the standard login prompt.

Arguments:

    None.

Return Value:

    None.

--*/

{

    struct utsname SystemInformation;

    memset(&SystemInformation, 0, sizeof(SystemInformation));
    uname(&SystemInformation);
    if (SystemInformation.nodename[0] == '\0') {
        printf("login: ");

    } else {
        printf("%s login: ", SystemInformation.nodename);
    }

    fflush(NULL);
    return;
}

VOID
SwUpdateUtmp (
    pid_t ProcessId,
    int NewType,
    PSTR TerminalName,
    PSTR UserName,
    PSTR HostName
    )

/*++

Routine Description:

    This routine updates a utmp entry, and potentially wtmp as well.

Arguments:

    ProcessId - Supplies the current process ID.

    NewType - Supplies the type of entry to add.

    TerminalName - Supplies a pointer to the terminal name.

    UserName - Supplies a pointer to the user name.

    HostName - Supplies a pointer to the host name.

Return Value:

    None.

--*/

{

    struct utmpx Copy;
    struct utmpx *Entry;

    setutxent();
    while (TRUE) {
        Entry = getutxent();
        if (Entry == NULL) {
            break;
        }

        if ((Entry->ut_pid == ProcessId) &&
            ((Entry->ut_type == INIT_PROCESS) ||
             (Entry->ut_type == LOGIN_PROCESS) ||
             (Entry->ut_type == USER_PROCESS) ||
             (Entry->ut_type == DEAD_PROCESS))) {

            if (Entry->ut_type == NewType) {
                memset(Entry->ut_host, 0, sizeof(Entry->ut_host));
            }

            break;
        }
    }

    if (Entry == NULL) {
        SwpWriteNewUtmpEntry(ProcessId,
                             NewType,
                             TerminalName,
                             UserName,
                             HostName);

    } else {
        memcpy(&Copy, Entry, sizeof(struct utmpx));
        Copy.ut_type = NewType;
        if (TerminalName != NULL) {
            strncpy(Copy.ut_line, TerminalName, sizeof(Copy.ut_line));
        }

        if (UserName != NULL) {
            strncpy(Copy.ut_user, UserName, sizeof(Copy.ut_user));
        }

        if (HostName != NULL) {
            strncpy(Copy.ut_host, HostName, sizeof(Copy.ut_host));
        }

        Copy.ut_tv.tv_sec = time(NULL);
        pututxline(&Copy);
        endutxent();
    }

    if ((NewType == USER_PROCESS) || (NewType == DEAD_PROCESS)) {
        if (NewType == DEAD_PROCESS) {
            Copy.ut_user[0] = '\0';
        }

        updwtmpx(_PATH_WTMPX, &Copy);
    }

    return;
}

VOID
SwPrintLoginIssue (
    PSTR IssuePath,
    PSTR TerminalName
    )

/*++

Routine Description:

    This routine prints the login issue file to standard out.

Arguments:

    IssuePath - Supplies an optional path to the issue file. If not specified,
        the default of /etc/issue will be used.

    TerminalName - Supplies the name of this terminal, used for expanding %l
        escapes in the issue file.

Return Value:

    None.

--*/

{

    CHAR Buffer[256];
    INT Character;
    FILE *Issue;
    PSTR String;
    struct utsname SystemInformation;
    time_t Time;

    if (IssuePath == NULL) {
        IssuePath = ISSUE_PATH;
    }

    Issue = fopen(IssuePath, "r");
    if (Issue == NULL) {
        return;
    }

    Time = time(NULL);
    uname(&SystemInformation);
    while (TRUE) {
        Character = fgetc(Issue);
        if (Character == EOF) {
            break;
        }

        String = Buffer;
        Buffer[0] = Character;
        Buffer[1] = '\0';
        if (Character == '\n') {
            Buffer[1] = '\r';
            Buffer[2] = '\0';

        } else if ((Character == '\\') || (Character == '%')) {
            Character = fgetc(Issue);
            if (Character == EOF) {
                break;
            }

            switch (Character) {
            case 's':
                String = SystemInformation.sysname;
                break;

            case 'n':
            case 'h':
                String = SystemInformation.nodename;
                break;

            case 'r':
                String = SystemInformation.release;
                break;

            case 'v':
                String = SystemInformation.version;
                break;

            case 'm':
                String = SystemInformation.machine;
                break;

            case 'D':
            case 'o':
                String = SystemInformation.domainname;
                break;

            case 'd':
                strftime(Buffer,
                         sizeof(Buffer),
                         "%A, %d %B %Y",
                         localtime(&Time));

                break;

            case 't':
                strftime(Buffer, sizeof(Buffer), "%H:%M:%S", localtime(&Time));
                break;

            case 'l':
                String = TerminalName;
                break;

            default:
                Buffer[0] = Character;
                break;
            }
        }

        fputs(String, stdout);
    }

    fclose(Issue);
    fflush(NULL);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
SwpPrintShadowLine (
    PSTR Line,
    UINTN LineSize,
    struct spwd *Shadow
    )

/*++

Routine Description:

    This routine prints a shadow entry to a line.

Arguments:

    Line - Supplies the line where the entry will be returned.

    LineSize - Supplies the size of the line buffer.

    Shadow - Supplies the shadow structure.

Return Value:

    0 on success.

    Non-zero if the line was too small.

--*/

{

    ssize_t Size;

    Size = snprintf(Line, LineSize, "%s:", Shadow->sp_namp);
    if (Size > 0) {
        Line += Size;
        LineSize -= Size;
    }

    if (Shadow->sp_pwdp != NULL) {
        Size = snprintf(Line, LineSize, "%s:", Shadow->sp_pwdp);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_lstchg != (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", (long int)(Shadow->sp_lstchg));
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_min != (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", Shadow->sp_min);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_max != (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", Shadow->sp_max);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_warn != (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", Shadow->sp_warn);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_inact != (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", Shadow->sp_inact);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_expire!= (long int)-1) {
        Size = snprintf(Line, LineSize, "%ld:", Shadow->sp_expire);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }

    } else {
        if (LineSize > 0) {
            *Line = ':';
            Line += 1;
            LineSize -= 1;
        }
    }

    if (Shadow->sp_flag != (unsigned long int)-1) {
        Size = snprintf(Line, LineSize, "%lu", Shadow->sp_flag);
        if (Size > 0) {
            Line += Size;
            LineSize -= Size;
        }
    }

    if (LineSize > 0) {
        *Line = '\0';
        return 0;
    }

    return -1;
}

VOID
SwpWriteNewUtmpEntry (
    pid_t ProcessId,
    int NewType,
    PSTR TerminalName,
    PSTR UserName,
    PSTR HostName
    )

/*++

Routine Description:

    This routine writes a new a utmp entry.

Arguments:

    ProcessId - Supplies the current process ID.

    NewType - Supplies the type of entry to add.

    TerminalName - Supplies a pointer to the terminal name.

    UserName - Supplies a pointer to the user name.

    HostName - Supplies a pointer to the host name.

Return Value:

    None.

--*/

{

    struct utmpx Entry;
    char *Id;
    UINTN Index;
    PSTR TerminalEnd;
    UINTN Width;

    memset(&Entry, 0, sizeof(struct utmpx));
    Entry.ut_pid = ProcessId;
    Entry.ut_type = NewType;
    if (TerminalName != NULL) {
        strncpy(Entry.ut_line, TerminalName, sizeof(Entry.ut_line));
    }

    if (UserName != NULL) {
        strncpy(Entry.ut_user, UserName, sizeof(Entry.ut_user));
    }

    if (HostName != NULL) {
        strncpy(Entry.ut_host, HostName, sizeof(Entry.ut_host));
    }

    Entry.ut_tv.tv_sec = time(NULL);
    Id = Entry.ut_id;
    Width = sizeof(Entry.ut_id);

    assert(Width != 0);

    if (TerminalName != NULL) {

        //
        // Fill the ID with zero characters.
        //

        for (Index = 0; Index < Width; Index += 1) {
            Id[Index] = '0';
        }

        //
        // Add all digits on the end.
        //

        Index = Width - 1;
        TerminalEnd = TerminalName + strlen(TerminalName);
        if (TerminalEnd != TerminalName) {
            TerminalEnd -= 1;
            while (TerminalEnd >= TerminalName) {
                if (!isdigit(*TerminalEnd)) {
                    break;
                }

                Id[Index] = *TerminalEnd;
                if (Index == 0) {
                    break;
                }

                Index -= 1;
                TerminalEnd -= 1;
            }
        }
    }

    setutxent();
    pututxline(&Entry);
    endutxent();
    return;
}

