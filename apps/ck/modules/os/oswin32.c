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
#include <windows.h>

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
CkpWin32GetSystemName (
    PSYSTEM_NAME Name
    )

/*++

Routine Description:

    This routine returns the name and version of the system.

Arguments:

    Name - Supplies a pointer where the name information will be returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    DWORD NodeNameSize;
    SYSTEM_INFO SystemInfo;
    OSVERSIONINFOEX VersionInfo;

    memset(&VersionInfo, 0, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    memset(&SystemInfo, 0, sizeof(SystemInfo));
    GetSystemInfo(&SystemInfo);
    GetVersionEx((LPOSVERSIONINFO)&VersionInfo);
    NodeNameSize = sizeof(Name->NodeName);
    GetComputerName(Name->NodeName, &NodeNameSize);
    strncpy(Name->SystemName, "Windows", sizeof(Name->SystemName));
    snprintf(Name->Release,
             sizeof(Name->Release),
             "%ld.%ld",
             VersionInfo.dwMajorVersion,
             VersionInfo.dwMinorVersion);

    snprintf(Name->Version,
             sizeof(Name->Version),
             "%ld %s",
             VersionInfo.dwBuildNumber,
             VersionInfo.szCSDVersion);

    switch (SystemInfo.wProcessorArchitecture) {
    case PROCESSOR_ARCHITECTURE_AMD64:
        strcpy(Name->Machine, "x86_64");
        break;

    case PROCESSOR_ARCHITECTURE_ARM:
        strcpy(Name->Machine, "armv7");
        break;

    case PROCESSOR_ARCHITECTURE_IA64:
        strcpy(Name->Machine, "ia64");
        break;

    case PROCESSOR_ARCHITECTURE_INTEL:
    default:
        strcpy(Name->Machine, "i686");
        break;
    }

    NodeNameSize = sizeof(Name->DomainName);
    GetComputerNameEx(ComputerNameDnsDomain, Name->DomainName, &NodeNameSize);
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

