/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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

#include <minoca/kernel/kernel.h>
#include "version.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the system version information.
//

#define PRODUCT_NAME "Minoca Boot App"

#ifndef VERSION_RELEASE

#define VERSION_RELEASE SystemReleaseDevelopment

#endif

#ifdef DEBUG

#define VERSION_DEBUG SystemBuildDebug

#else

#define VERSION_DEBUG SystemBuildRelease

#endif

#ifndef VERSION_MAJOR

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_REVISION 0

#endif

#ifndef VERSION_SERIAL

#define VERSION_SERIAL 0

#endif

#ifndef VERSION_BUILD_TIME

#define VERSION_BUILD_TIME 0

#endif

#ifndef VERSION_BUILD_STRING

#define VERSION_BUILD_STRING ""

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

ULONG BoEncodedVersion = ENCODE_VERSION_INFORMATION(VERSION_MAJOR,
                                                    VERSION_MINOR,
                                                    VERSION_REVISION,
                                                    VERSION_RELEASE,
                                                    VERSION_DEBUG);

ULONG BoVersionSerial = VERSION_SERIAL;
ULONG BoBuildTime = VERSION_BUILD_TIME;
PSTR BoBuildString = VERSION_BUILD_STRING;
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

