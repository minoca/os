/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uname.c

Abstract:

    This module implements support for getting the system name.

Author:

    Evan Green 17-Jul-2013

Environment:

    User Mode C Library

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "libcp.h"
#include <errno.h>
#include <string.h>
#include <sys/utsname.h>
#include <time.h>

//
// ---------------------------------------------------------------- Definitions
//

#define UNAME_SYSTEM_NAME "Minoca"

#define UNAME_NODE_NAME "minoca"
#define UNAME_DOMAIN_NAME ""

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

LIBC_API
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

    CHAR EndTag[32];
    KSTATUS Status;
    SYSTEM_VERSION_INFORMATION Version;

    Status = OsGetSystemVersion(&Version, TRUE);
    if (!KSUCCESS(Status)) {
        errno = ClConvertKstatusToErrorNumber(Status);
        return -1;
    }

    //
    // TODO: Implement getting the host and domain name.
    //

    strcpy(Name->sysname, UNAME_SYSTEM_NAME);
    strcpy(Name->nodename, UNAME_NODE_NAME);
    if ((Version.ReleaseLevel == SystemReleaseFinal) &&
        (Version.DebugLevel == SystemBuildRelease)) {

        EndTag[0] = '\0';

    } else if (Version.ReleaseLevel == SystemReleaseFinal) {
        snprintf(EndTag,
                 sizeof(EndTag),
                 "-%s",
                 RtlGetBuildDebugLevelString(Version.DebugLevel));

    } else if (Version.DebugLevel == SystemBuildRelease) {
        snprintf(EndTag,
                 sizeof(EndTag),
                 "-%s",
                 RtlGetReleaseLevelString(Version.ReleaseLevel));

    } else {
        snprintf(EndTag,
                 sizeof(EndTag),
                 "-%s-%s",
                 RtlGetReleaseLevelString(Version.ReleaseLevel),
                 RtlGetBuildDebugLevelString(Version.DebugLevel));
    }

    snprintf(Name->release,
             sizeof(Name->release),
             "%d.%d.%d.%lld%s",
             Version.MajorVersion,
             Version.MinorVersion,
             Version.Revision,
             Version.SerialVersion,
             EndTag);

    if (Version.BuildString == NULL) {
        Version.BuildString = "";
    }

    strncpy(Name->version, Version.BuildString, sizeof(Name->version));
    Name->version[sizeof(Name->version) - 1] = '\0';

#if defined(__i386)

    //
    // Determine whether this is a Pentium Pro machine (everything after 1995)
    // or a Pentium (including Intel Quark).
    //

    if (OsTestProcessorFeature(OsX86I686) != FALSE) {
        strcpy(Name->machine, "i686");

    } else {
        strcpy(Name->machine, "i586");
    }

#elif defined(__amd64)

    strcpy(Name->machine, "x86_64");

#elif defined(__arm__)

    if (OsTestProcessorFeature(OsArmArmv7) != FALSE) {
        strcpy(Name->machine, "armv7");

    } else {
        strcpy(Name->machine, "armv6");
    }

#else

#error Unknown Architecture

#endif

    strcpy(Name->domainname, UNAME_DOMAIN_NAME);
    return 0;
}

LIBC_API
int
gethostname (
    char *Name,
    size_t NameLength
    )

/*++

Routine Description:

    This routine returns the network host name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    size_t NodeLength;
    int Result;
    struct utsname UtsName;

    Result = uname(&UtsName);
    if (Result != 0) {
        return Result;
    }

    NodeLength = strlen(UtsName.nodename);
    strncpy(Name, UtsName.nodename, NameLength);
    if (NameLength < (NodeLength + 1)) {
        errno = ENAMETOOLONG;
        Result = -1;
        Name[NameLength] = '\0';
    }

    return Result;
}

LIBC_API
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

    size_t DomainLength;
    int Result;
    struct utsname UtsName;

    Result = uname(&UtsName);
    if (Result != 0) {
        return Result;
    }

    DomainLength = strlen(UtsName.domainname);
    strncpy(Name, UtsName.domainname, NameLength);
    if (NameLength < (DomainLength + 1)) {
        errno = ENAMETOOLONG;
        Result = -1;
        Name[NameLength] = '\0';
    }

    return Result;
}

PSTR
ClpGetFqdn (
    VOID
    )

/*++

Routine Description:

    This routine returns a null terminated string containing the fully
    qualified domain name of the machine.

Arguments:

    None.

Return Value:

    Returns a null terminated string containing nodename.domainname on success.
    The caller is responsible for freeing this string.

    NULL on allocation failure.

--*/

{

    PSTR FullName;
    ULONG HostNameLength;

    FullName = malloc((HOST_NAME_MAX * 2) + 3);
    if (FullName == NULL) {
        return NULL;
    }

    FullName[HOST_NAME_MAX] = '\0';
    if (gethostname(FullName, HOST_NAME_MAX) != 0) {
        FullName[0] = '\0';
    }

    HostNameLength = strlen(FullName);
    FullName[HostNameLength + HOST_NAME_MAX] = '\0';
    if (getdomainname(FullName + HostNameLength + 1, HOST_NAME_MAX) == 0) {
        FullName[HostNameLength] = '.';

    } else {
        FullName[HostNameLength] = '\0';
    }

    return FullName;
}

//
// --------------------------------------------------------- Internal Functions
//

