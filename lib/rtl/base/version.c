/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    version.c

Abstract:

    This module implements support for working with the system version
    information.

Author:

    Evan Green 19-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>

//
// --------------------------------------------------------------------- Macros
//

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
// Store the string versions of the release levels.
//

PSTR RtlReleaseLevelStrings[SystemReleaseLevelCount] = {
    "INVALID",
    "development",
    "prealpha",
    "alpha",
    "beta",
    "candidate",
    "final"
};

PSTR RtlBuildDebugLevelStrings[SystemBuildDebugLevelCount] = {
    "INVALID",
    "debug",
    "release"
};

//
// ------------------------------------------------------------------ Functions
//

RTL_API
ULONG
RtlGetSystemVersionString (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    SYSTEM_VERSION_STRING_VERBOSITY Level,
    PCHAR Buffer,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine gets the system string.

Arguments:

    VersionInformation - Supplies a pointer to the initialized version
        information to convert to a string.

    Level - Supplies the level of detail to print.

    Buffer - Supplies a pointer to the buffer that receives the version
        information.

    BufferSize - Supplies the size of the supplied buffer in bytes.

Return Value:

    Returns the size of the string as written to the buffer, including the
    null terminator.

--*/

{

    ULONG BytesWritten;
    PSTR DebugString;
    BOOL PrintDebugLevel;
    BOOL PrintReleaseLevel;
    BOOL PrintRevision;
    BOOL PrintSerial;
    PSTR ReleaseString;
    ULONG TotalBytesWritten;

    PrintRevision = TRUE;
    PrintSerial = TRUE;
    PrintReleaseLevel = TRUE;
    PrintDebugLevel = TRUE;
    ReleaseString = RtlGetReleaseLevelString(VersionInformation->ReleaseLevel);
    DebugString = RtlGetBuildDebugLevelString(VersionInformation->DebugLevel);
    switch (Level) {
    case SystemVersionStringMajorMinorOnly:
        PrintRevision = FALSE;
        PrintSerial = TRUE;
        break;

    default:
        break;
    }

    //
    // Skip the release and build level strings for final releases unless it's
    // the complete build string.
    //

    if ((Level != SystemVersionStringComplete) &&
        (VersionInformation->ReleaseLevel == SystemReleaseFinal)) {

        PrintReleaseLevel = FALSE;
    }

    if ((Level != SystemVersionStringComplete) &&
        (VersionInformation->DebugLevel == SystemBuildRelease)) {

        PrintDebugLevel = FALSE;
    }

    TotalBytesWritten = 0;
    if (VersionInformation->ProductName != NULL) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        "%s %d.%d",
                                        VersionInformation->ProductName,
                                        VersionInformation->MajorVersion,
                                        VersionInformation->MinorVersion);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }

    } else {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        "%d.%d",
                                        VersionInformation->ProductName,
                                        VersionInformation->MajorVersion,
                                        VersionInformation->MinorVersion);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    //
    // Print the more detailed aspects.
    //

    if (PrintRevision != FALSE) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        ".%d",
                                        VersionInformation->Revision);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    if (PrintSerial != FALSE) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        ".%I64d",
                                        VersionInformation->SerialVersion);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    if (PrintReleaseLevel != FALSE) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        " %s",
                                        ReleaseString);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    if (PrintDebugLevel != FALSE) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        " %s",
                                        DebugString);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    if (VersionInformation->BuildString != NULL) {
        BytesWritten = RtlPrintToString(Buffer,
                                        BufferSize,
                                        CharacterEncodingDefault,
                                        " %s",
                                        VersionInformation->BuildString);

        ASSERT(BytesWritten <= BufferSize);

        if (BytesWritten != 0) {
            Buffer += BytesWritten - 1;
            BufferSize -= BytesWritten - 1;
            TotalBytesWritten += BytesWritten - 1;
        }
    }

    return TotalBytesWritten;
}

RTL_API
PSTR
RtlGetReleaseLevelString (
    SYSTEM_RELEASE_LEVEL Level
    )

/*++

Routine Description:

    This routine returns a string corresponding with the given release level.

Arguments:

    Level - Supplies the release level.

Return Value:

    Returns a pointer to a static the string describing the given release
    level. The caller should not attempt to modify or free this memory.

--*/

{

    if (Level > SystemReleaseLevelCount) {
        Level = SystemReleaseInvalid;
    }

    return RtlReleaseLevelStrings[Level];
}

RTL_API
PSTR
RtlGetBuildDebugLevelString (
    SYSTEM_BUILD_DEBUG_LEVEL Level
    )

/*++

Routine Description:

    This routine returns a string corresponding with the given build debug
    level.

Arguments:

    Level - Supplies the build debug level.

Return Value:

    Returns a pointer to a static the string describing the given build debug
    level. The caller should not attempt to modify or free this memory.

--*/

{

    if (Level > SystemBuildDebugLevelCount) {
        Level = SystemBuildInvalid;
    }

    return RtlBuildDebugLevelStrings[Level];
}

//
// --------------------------------------------------------- Internal Functions
//

