/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    crashdmp.h

Abstract:

    This header contains definitions for the crash dump file.

Author:

    Chris Stevens 26-Aug-2014

--*/

//
// ---------------------------------------------------------------- Definitions
//

#define CRASH_DUMP_SIGNATURE 0x504D4443 // 'PMDC'

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CRASH_DUMP_TYPE {
    CrashDumpMinimal,
    CrashDumpTypeMax
} CRASH_DUMP_TYPE, *PCRASH_DUMP_TYPE;

/*++

Structure Description:

    This structure defines the header for a crash dump file.

Members:

    Signature - Stores the crash dump signature: CRASH_DUMP_SIGNATURE.

    Type - Stores the type of crash dump file.

    DumpSize - Stores the total size of the crash dump data, including the
        header.

    HeaderChecksum - Stores the one's compliment checksum of the header.

    MajorVersion - Stores the major version number for the OS.

    MinorVersion - Stores the minor version number for the OS.

    Revision - Stores the sub-minor version number for the OS.

    SerialVersion - Stores the globally increasing system version number. This
        value will always be greater than any previous builds.

    ReleaseLevel - Stores the release level of the build.

    DebugLevel - Stores the debug compilation level of the build.

    ProductNameOffset - Stores the offset from the beginning of the file to a
        string containing the name of the product. 0 indicates that it is not
        present.

    BuildStringOffset - Stores the offset from the beginning of the file to a
        string containing an identifier string for this build. 0 indicates that
        it is not present.

    BuildTime - Stores the system build time.

    CrashCode - Stores the reason for the system crash.

    Parameter1 - Stores the first parameter supplied to the crash routine.

    Parameter2 - Stores the second parameter supplied to the crash routine.

    Parameter3 - Stores the third parameter supplied to the crash routine.

    Parameter4 - Stores the fourth parameter supplied to the crash routine.

--*/

#pragma pack(push, 1)

typedef struct _CRASH_DUMP_HEADER {
    ULONG Signature;
    CRASH_DUMP_TYPE Type;
    ULONGLONG DumpSize;
    USHORT HeaderChecksum;
    USHORT MajorVersion;
    USHORT MinorVersion;
    USHORT Revision;
    ULONGLONG SerialVersion;
    SYSTEM_RELEASE_LEVEL ReleaseLevel;
    SYSTEM_BUILD_DEBUG_LEVEL DebugLevel;
    ULONGLONG ProductNameOffset;
    ULONGLONG BuildStringOffset;
    SYSTEM_TIME BuildTime;
    ULONG CrashCode;
    ULONGLONG Parameter1;
    ULONGLONG Parameter2;
    ULONGLONG Parameter3;
    ULONGLONG Parameter4;
} PACKED CRASH_DUMP_HEADER, *PCRASH_DUMP_HEADER;

#pragma pack(pop)

//
// -------------------------------------------------------- Function Prototypes
//

