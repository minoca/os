/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    misc.c

Abstract:

    This module implements miscellaneous OS support functions for the setup
    application.

Author:

    Evan Green 16-Apr-2014

Environment:

    User

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "../setup.h"
#include <minoca/lib/mlibc.h>
#include <minoca/fw/smbios.h>

//
// ---------------------------------------------------------------- Definitions
//

#define SMBIOS_DEFAULT_ALLOCATION_SIZE 0x1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PSTR
SetupOsGetSmbiosString (
    PSMBIOS_HEADER Header,
    ULONG StringNumber
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
SetupOsReboot (
    VOID
    )

/*++

Routine Description:

    This routine reboots the machine.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    KSTATUS Status;

    fflush(NULL);
    Status = OsResetSystem(SystemResetWarm);
    if (!KSUCCESS(Status)) {
        return ClConvertKstatusToErrorNumber(Status);
    }

    return 0;
}

INT
SetupOsGetPlatformName (
    PSTR *Name,
    PSETUP_RECIPE_ID Fallback
    )

/*++

Routine Description:

    This routine gets the platform name.

Arguments:

    Name - Supplies a pointer where a pointer to an allocated string containing
        the SMBIOS system information product name will be returned if
        available. The caller is responsible for freeing this memory when done.

    Fallback - Supplies a fallback platform to use if the given platform
        string was not returned or did not match a known platform.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR CurrentString;
    PVOID End;
    ULONG FirmwareType;
    PSTR PlatformName;
    UINTN Size;
    PSMBIOS_ENTRY_POINT SmbiosEntryPoint;
    PSTR SmbiosProductName;
    KSTATUS Status;
    PSMBIOS_SYSTEM_INFORMATION SystemInformation;

    PlatformName = NULL;
    SmbiosEntryPoint = NULL;

    //
    // First figure out the fallback based on the firmware type.
    //

    if (Fallback != NULL) {
        Size = sizeof(FirmwareType);
        Status = OsGetSetSystemInformation(SystemInformationKe,
                                           KeInformationFirmwareType,
                                           &FirmwareType,
                                           &Size,
                                           FALSE);

        if (!KSUCCESS(Status)) {
            goto OsGetPlatformNameEnd;
        }

        if (FirmwareType == SystemFirmwareEfi) {
            if (sizeof(PVOID) == 8) {
                *Fallback = SetupRecipePc64Efi;

            } else {
                *Fallback = SetupRecipePc32Efi;
            }

        } else if (FirmwareType == SystemFirmwarePcat) {
            if (sizeof(PVOID) == 8) {
                *Fallback = SetupRecipePc64;

            } else {
                *Fallback = SetupRecipePc32;
            }

        } else {
            *Fallback = SetupRecipeNone;
        }
    }

    //
    // Get the SMBIOS tables.
    //

    Size = SMBIOS_DEFAULT_ALLOCATION_SIZE;
    SmbiosEntryPoint = malloc(Size);
    if (SmbiosEntryPoint == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OsGetPlatformNameEnd;
    }

    SmbiosEntryPoint->AnchorString = SMBIOS_ANCHOR_STRING_VALUE;
    Status = OsGetSetSystemInformation(SystemInformationKe,
                                       KeInformationFirmwareTable,
                                       SmbiosEntryPoint,
                                       &Size,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        if (Status == STATUS_NOT_FOUND) {
            Status = STATUS_SUCCESS;
            goto OsGetPlatformNameEnd;
        }

        if (Status == STATUS_BUFFER_TOO_SMALL) {
            free(SmbiosEntryPoint);
            SmbiosEntryPoint = malloc(Size);
            if (SmbiosEntryPoint == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto OsGetPlatformNameEnd;
            }

            SmbiosEntryPoint->AnchorString = SMBIOS_ANCHOR_STRING_VALUE;
            Status = OsGetSetSystemInformation(SystemInformationKe,
                                               KeInformationFirmwareTable,
                                               SmbiosEntryPoint,
                                               &Size,
                                               FALSE);

            if (!KSUCCESS(Status)) {
                goto OsGetPlatformNameEnd;
            }
        }

        goto OsGetPlatformNameEnd;
    }

    //
    // Search the SMBIOS tables for the system information structure.
    //

    SystemInformation = (PSMBIOS_SYSTEM_INFORMATION)(SmbiosEntryPoint + 1);
    End = (PVOID)SystemInformation + SmbiosEntryPoint->StructureTableLength;
    while ((PVOID)SystemInformation < End) {

        //
        // If this is not the right structure, advance past the structure and
        // all the strings.
        //

        if (SystemInformation->Header.Type != SmbiosSystemInformation) {
            CurrentString = ((PVOID)SystemInformation) +
                            SystemInformation->Header.Length;

            while (((PVOID)CurrentString < End) &&
                   ((*CurrentString != '\0') ||
                    (*(CurrentString + 1) != '\0'))) {

                CurrentString += 1;
            }

            SystemInformation = (PSMBIOS_SYSTEM_INFORMATION)(CurrentString + 2);
            continue;
        }

        break;
    }

    if ((PVOID)SystemInformation >= End) {
        Status = STATUS_SUCCESS;
        goto OsGetPlatformNameEnd;
    }

    //
    // Get the product name string.
    //

    SmbiosProductName = SetupOsGetSmbiosString(&(SystemInformation->Header),
                                               SystemInformation->ProductName);

    if (SmbiosProductName == NULL) {
        Status = STATUS_SUCCESS;
        goto OsGetPlatformNameEnd;
    }

    PlatformName = strdup(SmbiosProductName);
    if (PlatformName == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto OsGetPlatformNameEnd;
    }

    Status = STATUS_SUCCESS;

OsGetPlatformNameEnd:
    if (SmbiosEntryPoint != NULL) {
        free(SmbiosEntryPoint);
    }

    if (!KSUCCESS(Status)) {
        if (PlatformName != NULL) {
            free(PlatformName);
            PlatformName = NULL;
        }
    }

    *Name = PlatformName;
    return ClConvertKstatusToErrorNumber(Status);
}

INT
SetupOsGetSystemMemorySize (
    PULONGLONG Megabytes
    )

/*++

Routine Description:

    This routine returns the number of megabytes of memory installed on the
    currently running system.

Arguments:

    Megabytes - Supplies a pointer to where the system memory capacity in
        megabytes will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    ULONGLONG MemoryBytes;
    MM_STATISTICS Statistics;
    UINTN StatisticsSize;
    KSTATUS Status;

    Statistics.Version = MM_STATISTICS_VERSION;
    StatisticsSize = sizeof(MM_STATISTICS);
    Status = OsGetSetSystemInformation(SystemInformationMm,
                                       MmInformationSystemMemory,
                                       &Statistics,
                                       &StatisticsSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        return ClConvertKstatusToErrorNumber(Status);
    }

    assert(Statistics.PageSize != 0);

    MemoryBytes = Statistics.PhysicalPages * Statistics.PageSize;
    *Megabytes = ALIGN_RANGE_UP(MemoryBytes, _1MB) / _1MB;
    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

PSTR
SetupOsGetSmbiosString (
    PSMBIOS_HEADER Header,
    ULONG StringNumber
    )

/*++

Routine Description:

    This routine gets the desired string from an SMBIOS structure.

Arguments:

    Header - Supplies a pointer to the header of the table.

    StringNumber - Supplies the one-based string index of the string to get.

Return Value:

    NULL if the string number is zero or invalid.

--*/

{

    PSTR CurrentString;
    ULONG StringIndex;
    ULONG StringLength;

    if (StringNumber == 0) {
        return NULL;
    }

    CurrentString = ((PVOID)Header) + Header->Length;
    for (StringIndex = 0; StringIndex < StringNumber - 1; StringIndex += 1) {
        StringLength = 0;
        while (*CurrentString != '\0') {
            CurrentString += 1;
            StringLength += 1;
        }

        //
        // If a double null was found, then this string number is invalid.
        //

        if (StringLength == 0) {
            return NULL;
        }

        CurrentString += 1;
    }

    return CurrentString;
}

