/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oswin32.c

Abstract:

    This module implements Windows support for the OS module.

Author:

    Evan Green 1-Feb-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _WIN32_WINNT 0x0601

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>
#include <unistd.h>
#include <utime.h>

#include "oswin32.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of times to retry an unlink.
//

#define UNLINK_RETRY_COUNT 20
#define UNLINK_RETRY_DELAY 50

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

SID_IDENTIFIER_AUTHORITY CkNtAuthority = {SECURITY_NT_AUTHORITY};

//
// ------------------------------------------------------------------ Functions
//

int
uname (
    struct utsname *Name
    )

/*++

Routine Description:

    This routine returns the system name and version.

Arguments:

    Name - Supplies a pointer to a name structure to fill out.

Return Value:

    Returns a non-negative value on success.

    -1 on error, and errno will be set to indicate the error.

--*/

{

    SYSTEM_INFO SystemInfo;
    OSVERSIONINFOEX VersionInfo;

    memset(&VersionInfo, 0, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    memset(&SystemInfo, 0, sizeof(SystemInfo));
    GetSystemInfo(&SystemInfo);
    GetVersionEx((LPOSVERSIONINFO)&VersionInfo);
    if (gethostname(Name->nodename, sizeof(Name->nodename)) != 0) {
        Name->nodename[0] = '\0';
    }

    strncpy(Name->sysname, "Windows", sizeof(Name->sysname));
    snprintf(Name->release,
             sizeof(Name->release),
             "%ld.%ld",
             VersionInfo.dwMajorVersion,
             VersionInfo.dwMinorVersion);

    snprintf(Name->version,
             sizeof(Name->version),
             "%ld %s",
             VersionInfo.dwBuildNumber,
             VersionInfo.szCSDVersion);

    switch (SystemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        strcpy(Name->machine, "x86_64");
        break;

    case PROCESSOR_ARCHITECTURE_ARM:
        strcpy(Name->machine, "armv7");
        break;

    case PROCESSOR_ARCHITECTURE_IA64:
        strcpy(Name->machine, "ia64");
        break;

    case PROCESSOR_ARCHITECTURE_INTEL:
    default:
        strcpy(Name->machine, "i686");
        break;
    }

    if (getdomainname(Name->domainname, sizeof(Name->domainname)) != 0) {
        Name->domainname[0] = '\0';
    }

    return 0;
}

int
getdomainname (
    char *Name,
    size_t NameLength
    )

/*++

Routine Description:

    This routine returns the network domain name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    DWORD NodeNameSize;

    NodeNameSize = NameLength;
    if (!GetComputerNameEx(ComputerNameDnsDomain, Name, &NodeNameSize)) {
        return -1;
    }

    return 0;
}

int
utimes (
    const char *Path,
    const struct timeval Times[2]
    )

/*++

Routine Description:

    This routine sets the access and modification times of the given file. The
    effective user ID of the process must match the owner of the file, or the
    process must have appropriate privileges.

Arguments:

    Path - Supplies a pointer to the path of the file to change times for.

    Times - Supplies an optional array of time value structures containing the
        access and modification times to set.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    struct stat Stat;
    struct utimbuf TimeBuffer;

    //
    // This function doesn't work for directories on Windows. Just pretend it
    // does.
    //

    if ((stat(Path, &Stat) == 0) && (S_ISDIR(Stat.st_mode))) {
        return 0;
    }

    if (Times == NULL) {
        return utime(Path, NULL);
    }

    TimeBuffer.actime = Times[0].tv_sec;
    TimeBuffer.modtime = Times[1].tv_sec;
    return utime(Path, &TimeBuffer);
}

long
sysconf (
    int Variable
    )

/*++

Routine Description:

    This routine gets the system value for the given variable index. These
    variables are not expected to change within a single invocation of a
    process, and therefore need only be queried once per process.

Arguments:

    Variable - Supplies the variable to get. See _SC_* definitions.

Return Value:

    Returns the value for that variable.

    -1 if the variable has no limit. The errno variable will be left unchanged.

    -1 if the variable was invalid, and errno will be set to EINVAL.

--*/

{

    SYSTEM_INFO SystemInfo;
    long Value;

    Value = -1;
    switch (Variable) {
    case _SC_NPROCESSORS_ONLN:
        GetSystemInfo(&SystemInfo);
        Value = SystemInfo.dwNumberOfProcessors;
        break;

    default:
        fprintf(stderr, "Unknown sysconf variable %d\n", Variable);

        assert(FALSE);

        break;
    }

    return Value;
}

int
CkpWin32Unlink (
    const char *Path
    )

/*++

Routine Description:

    This routine attempts to unlink a path. This is the Windows version, so it
    will try a few times and only fail if it really cannot get access after
    some time.

Arguments:

    Path - Supplies a pointer to the path of the file to unlink.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    INT Result;
    struct stat Stat;
    INT Try;

    for (Try = 0; Try < UNLINK_RETRY_COUNT; Try += 1) {

        //
        // Use the underscore version since the regular one is still #defined.
        //

        Result = _unlink(Path);
        if (Result != -1) {
            break;
        }

        //
        // Just do a quick check: unlink is never going to work without the
        // proper permissions.
        //

        if (Try == 0) {
            if (stat(Path, &Stat) != 0) {
                break;
            }

            if ((Stat.st_mode & S_IWUSR) == 0) {
                break;
            }
        }

        Sleep(UNLINK_RETRY_DELAY);
    }

    return Result;
}

int
CkpWin32Rmdir (
    const char *Path
    )

/*++

Routine Description:

    This routine attempts to remove a directory. This is the Windows version,
    so it will try a few times and only fail if it really cannot get access
    after some time.

Arguments:

    Path - Supplies a pointer to the path of the file to unlink.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to contain more information.

--*/

{

    DIR *Directory;
    struct dirent *DirectoryEntry;
    INT Result;
    INT Try;

    //
    // See if the directory is empty. If it isn't, then don't spend all this
    // time spinning waiting for something to happen that won't.
    //

    Directory = opendir(Path);
    if (Directory == NULL) {
        return -1;
    }

    while (TRUE) {
        errno = 0;
        DirectoryEntry = readdir(Directory);
        if (DirectoryEntry == NULL) {
            break;
        }

        if ((strcmp(DirectoryEntry->d_name, ".") == 0) ||
            (strcmp(DirectoryEntry->d_name, "..") == 0)) {

            continue;
        }

        closedir(Directory);
        errno = ENOTEMPTY;
        return -1;
    }

    closedir(Directory);
    for (Try = 0; Try < UNLINK_RETRY_COUNT * 2; Try += 1) {

        //
        // Use the underscore version since the regular one is still #defined.
        //

        Result = _rmdir(Path);
        if ((Result != -1) || (errno != ENOTEMPTY)) {
            break;
        }

        Sleep(UNLINK_RETRY_DELAY);
    }

    return Result;
}

int
geteuid (
    void
    )

/*++

Routine Description:

    This routine returns the effective user ID in Windows. If the process is
    privileged, this routine returns 0. Otherwise, this routine returns 1000.

Arguments:

    None.

Return Value:

    0 if the process is privileged.

    1000 if the process is a regular user.

--*/

{

    PSID AdministratorsGroup;
    BOOL IsAdministrator;
    BOOL Result;

    IsAdministrator = FALSE;

    //
    // Return 0 if the current user is an admin.
    //

    Result = AllocateAndInitializeSid(&CkNtAuthority,
                                      2,
                                      SECURITY_BUILTIN_DOMAIN_RID,
                                      DOMAIN_ALIAS_RID_ADMINS,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      0,
                                      &AdministratorsGroup);

    if (Result != FALSE) {
        if (CheckTokenMembership(NULL,
                                 AdministratorsGroup,
                                 &IsAdministrator) == FALSE) {

            IsAdministrator = FALSE;
        }

        FreeSid(AdministratorsGroup);
    }

    if (IsAdministrator != FALSE) {
        return 0;
    }

    return 1000;
}

//
// --------------------------------------------------------- Internal Functions
//

