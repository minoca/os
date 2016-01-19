/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    version.c

Abstract:

    This module implements support for returning the loader system version
    information.

Author:

    Evan Green 19-Nov-2013

Environment:

    Boot

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel.h>

//
// --------------------------------------------------------------------- Macros
//

#define ENCODE_VERSION_INFORMATION(_MajorVersion,                              \
                                   _MinorVersion,                              \
                                   _Revision,                                  \
                                   _ReleaseLevel,                              \
                                   _DebugLevel)                                \
                                                                               \
    (((ULONGLONG)(_MajorVersion) << 48) | ((ULONGLONG)(_MinorVersion) << 32) | \
     ((ULONGLONG)(_Revision) << 16) | ((ULONGLONG)(_ReleaseLevel) << 8) |      \
     (ULONGLONG)(_DebugLevel))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the system version information.
//

#define PRODUCT_NAME "Minoca Boot App"

#ifndef SYSTEM_VERSION_RELEASE

#define SYSTEM_VERSION_RELEASE SystemReleaseDevelopment

#endif

#ifdef DEBUG

#define SYSTEM_VERSION_DEBUG SystemBuildChecked

#else

#define SYSTEM_VERSION_DEBUG SystemBuildFree

#endif

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
// Store the version information jammed into a packed format.
//

ULONGLONG BoEncodedVersion = ENCODE_VERSION_INFORMATION(SYSTEM_VERSION_MAJOR,
                                                        SYSTEM_VERSION_MINOR,
                                                        SYSTEM_VERSION_REVISION,
                                                        SYSTEM_VERSION_RELEASE,
                                                        SYSTEM_VERSION_DEBUG);

ULONGLONG BoVersionSerial = REVISION;
ULONGLONG BoBuildTime = BUILD_TIME;
PSTR BoBuildString = BUILD_STRING;
PSTR BoProductName = PRODUCT_NAME;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
KeGetSystemVersion (
    PSYSTEM_VERSION_INFORMATION VersionInformation,
    PVOID Buffer,
    PULONG BufferSize
    )

/*++

Routine Description:

    This routine gets the system version information.

Arguments:

    VersionInformation - Supplies a pointer where the system version
        information will be returned.

    Buffer - Supplies an optional pointer to the buffer to use for the
        product name and build string.

    BufferSize - Supplies an optional pointer that on input contains the size
        of the supplied string buffer in bytes. On output, returns the needed
        size of the build string buffer in bytes including the null terminator
        characters.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_TOO_SMALL if the supplied buffer was not big enough to hold
    both strings.

--*/

{

    ULONG BuildStringSize;
    ULONG ProductNameSize;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    VersionInformation->MajorVersion = DECODE_MAJOR_VERSION(BoEncodedVersion);
    VersionInformation->MinorVersion = DECODE_MINOR_VERSION(BoEncodedVersion);
    VersionInformation->Revision = DECODE_VERSION_REVISION(BoEncodedVersion);
    VersionInformation->SerialVersion = BoVersionSerial;
    VersionInformation->ReleaseLevel = DECODE_VERSION_RELEASE(BoEncodedVersion);
    VersionInformation->DebugLevel = DECODE_VERSION_DEBUG(BoEncodedVersion);
    VersionInformation->BuildTime.Seconds = BoBuildTime;
    VersionInformation->BuildTime.Nanoseconds = 0;
    VersionInformation->ProductName = NULL;
    VersionInformation->BuildString = NULL;
    BuildStringSize = RtlStringLength(BoBuildString);
    if (BuildStringSize != 0) {
        BuildStringSize += 1;
    }

    ProductNameSize = RtlStringLength(BoProductName) + 1;
    if ((BufferSize != NULL) && (Buffer != NULL)) {
        if (*BufferSize < BuildStringSize + ProductNameSize) {
            Status = STATUS_BUFFER_TOO_SMALL;

        } else {
            RtlCopyMemory(Buffer, BoProductName, ProductNameSize);
            VersionInformation->ProductName = Buffer;
            if (BuildStringSize != 0) {
                VersionInformation->BuildString = Buffer + ProductNameSize;
                RtlCopyMemory(VersionInformation->BuildString,
                              BoBuildString,
                              BuildStringSize);
            }
        }
    }

    if (BufferSize != NULL) {
        *BufferSize = BuildStringSize + ProductNameSize;
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

