/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    efiboot.c

Abstract:

    This module implements the efiboot utility, which is a usermode program
    that allows for manipulation of EFI boot entries.

Author:

    Evan Green 9-Dec-2014

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <minoca/lib/minocaos.h>
#include <minoca/lib/mlibc.h>
#include <minoca/uefi/uefi.h>
#include <minoca/uefi/guid/globlvar.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EFIBOOT_VERSION_MAJOR 1
#define EFIBOOT_VERSION_MINOR 0

#define EFIBOOT_USAGE \
    "usage: efiboot [Options] \n" \
    "The efiboot utility can be used to manipulate EFI boot options via \n" \
    "kernel UEFI environment variable access. With no options, displays the \n"\
    "current information. Options are:\n" \
    "  -o, --bootorder=xxxx,yyyy,zzzz -- Sets the boot order. Values \n" \
    "      should be in hexadecimal.\n" \
    "  -V, --version -- Prints application version information and exits.\n" \

#define EFIBOOT_OPTIONS "ohV"

#define EFIBOOT_DEFAULT_VARIABLE_SIZE 4096

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
EfibootIsEfiSystem (
    VOID
    );

INT
EfibootPrintConfiguration (
    VOID
    );

INT
EfibootConvertBootOrderStringToBinary (
    PSTR BootOrderString,
    PVOID *BootOrder,
    PUINTN BootOrderSize
    );

VOID
EfibootPrintBootOrderVariable (
    PVOID VariableData,
    INT VariableDataSize
    );

KSTATUS
EfibootGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID **Data
    );

KSTATUS
EfibootGetSetVariable (
    BOOL Set,
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    );

//
// -------------------------------------------------------------------- Globals
//

struct option EfibootLongOptions[] = {
    {"bootorder", required_argument, 0, 'o'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {NULL, 0, 0, 0},
};

EFI_GUID EfibootGlobalVariableGuid = EFI_GLOBAL_VARIABLE_GUID;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the main entry point for the EFIboot application.

Arguments:

    ArgumentCount - Supplies the number of arguments on the command line.

    Arguments - Supplies an array of pointers to strings representing the
        arguments.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT32 Attributes;
    PVOID BootOrder;
    UINTN BootOrderSize;
    INT Option;
    BOOL OptionSpecified;
    INT Result;
    KSTATUS Status;

    BootOrder = NULL;
    BootOrderSize = 0;

    //
    // Process the control arguments.
    //

    OptionSpecified = FALSE;
    while (TRUE) {
        Option = getopt_long(ArgumentCount,
                             Arguments,
                             EFIBOOT_OPTIONS,
                             EfibootLongOptions,
                             NULL);

        if (Option == -1) {
            break;
        }

        if ((Option == '?') || (Option == ':')) {
            Status = 1;
            goto mainEnd;
        }

        switch (Option) {
        case 'o':
            Result = EfibootConvertBootOrderStringToBinary(optarg,
                                                           &BootOrder,
                                                           &BootOrderSize);

            if (Result != 0) {
                goto mainEnd;
            }

            OptionSpecified = TRUE;
            break;

        case 'V':
            printf("efiboot version %d.%d.\n",
                   EFIBOOT_VERSION_MAJOR,
                   EFIBOOT_VERSION_MINOR);

            return 1;

        case 'h':
            printf(EFIBOOT_USAGE);
            return 1;

        default:

            assert(FALSE);

            Status = 1;
            goto mainEnd;
        }
    }

    //
    // Skip everything if this isn't even a UEFI system.
    //

    if (EfibootIsEfiSystem() == FALSE) {
        fprintf(stderr, "efiboot: Error: This is not a UEFI system.\n");
        Result = EINVAL;
        goto mainEnd;
    }

    if (OptionSpecified == FALSE) {
        Result = EfibootPrintConfiguration();
        goto mainEnd;
    }

    //
    // Set the boot order variable if requested.
    //

    Attributes = EFI_VARIABLE_NON_VOLATILE |
                 EFI_VARIABLE_RUNTIME_ACCESS |
                 EFI_VARIABLE_BOOTSERVICE_ACCESS;

    Status = EfibootGetSetVariable(TRUE,
                                   L"BootOrder",
                                   &EfibootGlobalVariableGuid,
                                   &Attributes,
                                   &BootOrderSize,
                                   BootOrder);

    if (!KSUCCESS(Status)) {
        Result = ClConvertKstatusToErrorNumber(Status);
        fprintf(stderr,
                "efiboot: Error: Failed to set BootOrder: %d: %s.\n",
                Status,
                strerror(Result));

        goto mainEnd;
    }

mainEnd:
    if (BootOrder != NULL) {
        free(BootOrder);
    }

    if (Result != 0) {
        fprintf(stderr, "efiboot: Exiting with status: %s\n", strerror(Result));
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
EfibootIsEfiSystem (
    VOID
    )

/*++

Routine Description:

    This routine determines if the current system is UEFI-based.

Arguments:

    None.

Return Value:

    TRUE if the system is UEFI based.

    FALSE if the system is not UEFI based or on failure.

--*/

{

    ULONG FirmwareType;
    UINTN FirmwareTypeSize;
    KSTATUS Status;

    FirmwareTypeSize = sizeof(ULONG);
    Status = OsGetSetSystemInformation(SystemInformationKe,
                                       KeInformationFirmwareType,
                                       &FirmwareType,
                                       &FirmwareTypeSize,
                                       FALSE);

    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "efiboot: Failed to determine if firmware type is EFI: %d.\n",
                Status);

        return FALSE;
    }

    if (FirmwareType == SystemFirmwareEfi) {
        return TRUE;
    }

    return FALSE;
}

INT
EfibootPrintConfiguration (
    VOID
    )

/*++

Routine Description:

    This routine prints the current EFI configuration.

Arguments:

    None.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PVOID BootOrder;
    UINTN BootOrderSize;
    KSTATUS Status;

    BootOrder = NULL;
    Status = EfibootGetVariable(L"BootOrder",
                                &EfibootGlobalVariableGuid,
                                NULL,
                                &BootOrderSize,
                                &BootOrder);

    if (Status != STATUS_NOT_FOUND) {
        if (!KSUCCESS(Status)) {
            fprintf(stderr,
                    "efiboot: Error: Failed to get BootOrder: %d\n",
                    Status);

        } else {
            printf("BootOrder: ");
            EfibootPrintBootOrderVariable(BootOrder, BootOrderSize);
            printf("\n");
        }
    }

    if (BootOrder != NULL) {
        free(BootOrder);
    }

    return ClConvertKstatusToErrorNumber(Status);
}

INT
EfibootConvertBootOrderStringToBinary (
    PSTR BootOrderString,
    PVOID *BootOrder,
    PUINTN BootOrderSize
    )

/*++

Routine Description:

    This routine converts a boot order string specified by the user to a boot
    order EFI variable.

Arguments:

    BootOrderString - Supplies a pointer to the human readable boot order
        string, in the form XXXX,YYYY,ZZZZ,... where XXXX are hexadecimal
        numbers.

    BootOrder - Supplies a pointer where the boot order binary data will be
        returned on success. The caller is responsible for freeing this memory.

    BootOrderSize - Supplies a pointer where the size of the boot order data
        will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Count;
    PSTR CurrentString;
    INT Index;
    INT Integer;
    INT ItemsScanned;
    INT Result;
    UINT16 *Variable;

    if ((BootOrderString == NULL) || (*BootOrderString == '\0')) {
        *BootOrder = NULL;
        *BootOrderSize = 0;
        return 0;
    }

    //
    // Count the commas to determine how many boot entries there are.
    //

    Count = 1;
    CurrentString = BootOrderString;
    while (TRUE) {
        CurrentString = strchr(CurrentString, ',');
        if (CurrentString == NULL) {
            break;
        }

        Count += 1;
        CurrentString += 1;
    }

    Variable = malloc(Count * sizeof(UINT16));
    if (Variable == NULL) {
        Result = ENOMEM;
        goto ConvertBootOrderStringToBinaryEnd;
    }

    //
    // Scan a string in the form NNNN,NNNN,..., where NNNN is a 4 digit
    // hexadecimal value.
    //

    Index = 0;
    CurrentString = BootOrderString;
    while (TRUE) {

        assert(Index < Count);

        ItemsScanned = sscanf(CurrentString, "%x", &Integer);
        if (ItemsScanned != 1) {
            fprintf(stderr,
                    "efiboot: Invalid boot entry number starting at '%s'.\n"
                    "boot entries should be 4 digit hex numbers, like "
                    "0001,001E,0000\n",
                    CurrentString);

            Result = EINVAL;
            goto ConvertBootOrderStringToBinaryEnd;
        }

        Variable[Index] = Integer;
        Index += 1;
        while (isxdigit(*CurrentString)) {
            CurrentString += 1;
        }

        if (*CurrentString == '\0') {
            break;
        }

        if (*CurrentString != ',') {
            fprintf(stderr,
                    "efiboot: Expected comma, got '%s'.\n", CurrentString);

            Result = EINVAL;
            goto ConvertBootOrderStringToBinaryEnd;
        }

        CurrentString += 1;
    }

    Count = Index;
    Result = 0;

ConvertBootOrderStringToBinaryEnd:
    if (Result != 0) {
        if (Variable != NULL) {
            free(Variable);
            Variable = NULL;
            Count = 0;
        }

    }

    *BootOrder = Variable;
    *BootOrderSize = Count * sizeof(UINT16);
    return Result;
}

VOID
EfibootPrintBootOrderVariable (
    PVOID VariableData,
    INT VariableDataSize
    )

/*++

Routine Description:

    This routine prints the contents of the given boot order variable to
    standard out.

Arguments:

    VariableData - Supplies a pointer to the binary boot variable data.

    VariableDataSize - Supplies the size of the variable data in bytes.

Return Value:

    None.

--*/

{

    INT Count;
    UINT16 *Entry;
    INT Index;

    if ((VariableDataSize % sizeof(UINT16)) != 0) {
        fprintf(stderr,
                "efiboot: Warning: BootOrder variable size was %d, not a "
                "multiple of 2!\n",
                VariableDataSize);
    }

    Count = VariableDataSize / sizeof(UINT16);
    Entry = VariableData;
    for (Index = 0; Index < Count; Index += 1) {
        printf("%04X", Entry[Index]);
        if (Index != Count - 1) {
            printf(",");
        }
    }

    return;
}

KSTATUS
EfibootGetVariable (
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID **Data
    )

/*++

Routine Description:

    This routine gets an EFI firmware variable. The caller must be a system
    administrator.

Arguments:

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies an optional pointer to the attributes.

    DataSize - Supplies a pointer where the size of the data will be returned.

    Data - Supplies a pointer where the allocated data will be returned on
        success. The caller is responsible for freeing this memory.

Return Value:

    None.

--*/

{

    UINT32 LocalAttributes;
    VOID *LocalData;
    UINTN LocalDataSize;
    KSTATUS Status;

    LocalDataSize = EFIBOOT_DEFAULT_VARIABLE_SIZE;
    LocalData = malloc(LocalDataSize);
    if (LocalData == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(LocalData, 0, LocalDataSize);
    Status = EfibootGetSetVariable(FALSE,
                                   VariableName,
                                   VendorGuid,
                                   &LocalAttributes,
                                   &LocalDataSize,
                                   LocalData);

    if (!KSUCCESS(Status)) {
        free(LocalData);
        LocalData = NULL;
        LocalDataSize = 0;
        LocalAttributes = 0;
    }

    if (Attributes != NULL) {
        *Attributes = LocalAttributes;
    }

    *DataSize = LocalDataSize;
    *Data = LocalData;
    return Status;
}

KSTATUS
EfibootGetSetVariable (
    BOOL Set,
    CHAR16 *VariableName,
    EFI_GUID *VendorGuid,
    UINT32 *Attributes,
    UINTN *DataSize,
    VOID *Data
    )

/*++

Routine Description:

    This routine gets or sets an EFI firmware variable. The caller must be a
    system administrator.

Arguments:

    Set - Supplies a boolean indicating whether to get the variable (FALSE) or
        set the variable (TRUE).

    VariableName - Supplies a pointer to a null-terminated string containing
        the name of the vendor's variable.

    VendorGuid - Supplies a pointer to the unique GUID for the vendor.

    Attributes - Supplies a pointer to the attributes.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, the actual size of the data will be returned.

    Data - Supplies a pointer to the variable data buffer.

Return Value:

    None.

--*/

{

    UINTN AllocationSize;
    PHL_EFI_VARIABLE_INFORMATION Information;
    VOID *InformationData;
    CHAR16 *InformationVariable;
    KSTATUS Status;
    UINTN VariableNameSize;

    VariableNameSize = 0;
    while (VariableName[VariableNameSize] != L'\0') {
        VariableNameSize += 1;
    }

    VariableNameSize += 1;
    VariableNameSize *= sizeof(CHAR16);
    AllocationSize = sizeof(HL_EFI_VARIABLE_INFORMATION) +
                     VariableNameSize +
                     *DataSize;

    Information = malloc(AllocationSize);
    if (Information == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    memset(Information, 0, AllocationSize);
    Information->VariableNameSize = VariableNameSize;
    memcpy(&(Information->VendorGuid), VendorGuid, sizeof(EFI_GUID));
    Information->Attributes = *Attributes;
    Information->DataSize = *DataSize;
    InformationVariable = (CHAR16 *)(Information + 1);
    InformationData = ((PUCHAR)InformationVariable) + VariableNameSize;
    memcpy(InformationVariable, VariableName, VariableNameSize);
    if (Set != FALSE) {
        memcpy(InformationData, Data, *DataSize);
    }

    Status = OsGetSetSystemInformation(SystemInformationHl,
                                       HlInformationEfiVariable,
                                       Information,
                                       &AllocationSize,
                                       Set);

    if (!KSUCCESS(Status)) {
        goto GetSetVariableEnd;
    }

    *Attributes = Information->Attributes;
    *DataSize = Information->DataSize;
    if (Set == FALSE) {
        memcpy(Data, InformationData, Information->DataSize);
    }

GetSetVariableEnd:
    if (Information != NULL) {
        free(Information);
    }

    return Status;
}

