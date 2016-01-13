/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

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

    CHAR BuildTime[50];
    KSTATUS Status;
    time_t Time;
    struct tm TimeStructure;
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
    snprintf(Name->release,
             sizeof(Name->release),
             "%d.%d.%d.%lld-%s-%s",
             Version.MajorVersion,
             Version.MinorVersion,
             Version.Revision,
             Version.SerialVersion,
             RtlGetReleaseLevelString(Version.ReleaseLevel),
             RtlGetBuildDebugLevelString(Version.DebugLevel));

    Time = ClpConvertSystemTimeToUnixTime(&(Version.BuildTime));
    localtime_r(&Time, &TimeStructure);
    strftime(BuildTime,
             sizeof(BuildTime),
             "%a %b %d, %Y %I:%M %p",
             &TimeStructure);

    if (Version.BuildString == NULL) {
        Version.BuildString = "";
    }

    snprintf(Name->version,
             sizeof(Name->version),
             "%s %s",
             BuildTime,
             Version.BuildString);

#if defined(__i386)

    //
    // Determine whether this is a Pentium Pro machine (everything after 1995)
    // or a Pentium (including Intel Quark).
    //

    if ((OsGetProcessorFeatures() & X86_FEATURE_I686) != 0) {
        strcpy(Name->machine, "i686");

    } else {
        strcpy(Name->machine, "i586");
    }

#elif defined(__amd64)

    strcpy(Name->machine, "x86_64");

#elif defined(__arm__)

    strcpy(Name->machine, "arm");

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

    This routine returns the standard host name for the current machine.

Arguments:

    Name - Supplies a pointer where the null-terminated name will be returned
        on success.

    NameLength - Supplies the length of the name buffer in bytes.

Return Value:

    0 on success.

    -1 on failure, and errno will be set to indicate the error.

--*/

{

    int BytesConverted;
    size_t DomainLength;
    size_t NodeLength;
    int Result;
    struct utsname UtsName;

    Result = uname(&UtsName);
    if (Result != 0) {
        return Result;
    }

    NodeLength = strlen(UtsName.nodename);
    DomainLength = strlen(UtsName.domainname);
    if (DomainLength != 0) {
        BytesConverted = snprintf(Name,
                                  NameLength,
                                  "%s.%s",
                                  UtsName.nodename,
                                  UtsName.domainname);

        if (BytesConverted < 0) {
            Result = BytesConverted;

        } else {
            if (NameLength < (NodeLength + DomainLength + 2)) {
                errno = ENAMETOOLONG;
                Result = -1;
            }
        }

    } else {
        strncpy(Name, UtsName.nodename, NameLength);
        if (NameLength < (NodeLength + 1)) {
            errno = ENAMETOOLONG;
            Result = -1;
            Name[NameLength] = '\0';
        }
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

