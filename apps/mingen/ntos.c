/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ntos.c

Abstract:

    This module implements OS-specific support for mingen on Windows.

Author:

    Evan Green 17-Mar-2016

Environment:

    Win32

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <windows.h>

#include <errno.h>
#include <stdio.h>
#include <string.h>

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

INT
MingenOsUname (
    CHAR Flavor,
    PSTR Buffer,
    ULONG Size
    )

/*++

Routine Description:

    This routine implements the OS-specific uname function.

Arguments:

    Flavor - Supplies the flavor of uname to get. Valid values are s, n, r, v,
        and m.

    Buffer - Supplies a buffer where the string will be returned on success.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    CHAR Source[256];
    DWORD SourceSize;
    int Status;
    SYSTEM_INFO SystemInfo;
    OSVERSIONINFOEX VersionInfo;

    Status = 0;
    memset(&VersionInfo, 0, sizeof(VersionInfo));
    VersionInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    switch (Flavor) {
    case 's':
        strcpy(Source, "Windows");
        break;

    case 'n':
        SourceSize = sizeof(Source);
        Status = GetComputerName(Source, &SourceSize);
        if (Status == FALSE) {
            return ENOSYS;
        }

        break;

    case 'r':
        Status = GetVersionEx((LPOSVERSIONINFO)&VersionInfo);
        if (Status == FALSE) {
            return ENOSYS;
        }

        snprintf(Source,
                 sizeof(Source),
                 "%d.%d",
                 VersionInfo.dwMajorVersion,
                 VersionInfo.dwMinorVersion);

        break;

    case 'v':
        Status = GetVersionEx((LPOSVERSIONINFO)&VersionInfo);
        if (Status == FALSE) {
            return ENOSYS;
        }

        snprintf(Source,
                 sizeof(Source),
                 "%d %s",
                 VersionInfo.dwBuildNumber,
                 VersionInfo.szCSDVersion);

        break;

    case 'm':
        memset(&SystemInfo, 0, sizeof(SystemInfo));
        GetSystemInfo(&SystemInfo);
        switch (SystemInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64:
            strcpy(Source, "x86-64");
            break;

        case PROCESSOR_ARCHITECTURE_ARM:
            strcpy(Source, "armv7");
            break;

        case PROCESSOR_ARCHITECTURE_IA64:
            strcpy(Source, "ia64");
            break;

        case PROCESSOR_ARCHITECTURE_INTEL:
        default:
            strcpy(Source, "i686");
            break;
        }

        break;

    default:
        Status = EINVAL;
        return Status;
    }

    if ((Source != NULL) && (Size != 0)) {
        strncpy(Buffer, Source, Size);
        Buffer[Size - 1] = '\0';
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

