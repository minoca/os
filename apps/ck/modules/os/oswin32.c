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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <windows.h>
#include <utime.h>

#include "oswin32.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

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

//
// --------------------------------------------------------- Internal Functions
//

