/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    reboot.c

Abstract:

    This module implements reset support on a standard PC.

Author:

    Evan Green 26-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/acpitabs.h>
#include <uefifw.h>
#include <minoca/uefi/guid/acpi.h>
#include "../biosfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_PCAT_8042_CONTROL_PORT 0x64
#define EFI_PCAT_8042_RESET_VALUE 0xFE
#define EFI_PCAT_8042_INPUT_BUFFER_FULL 0x02

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PFADT
EfipPcatGetFadt (
    VOID
    );

PRSDP
EfipPcatGetRsdpFromEfiSystemTable (
    VOID
    );

BOOLEAN
EfipPcatCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define these GUIDs in the runtime driver.
//

EFI_GUID EfiAcpiTable1Guid = EFI_ACPI_10_TABLE_GUID;
EFI_GUID EfiAcpiTableGuid = EFI_ACPI_20_TABLE_GUID;

//
// Define the ACPI reset mechanism parameters.
//

BOOLEAN EfiAcpiResetValid;
BOOLEAN EfiAcpiResetIoPort;
UINTN EfiAcpiResetAddress;
UINT8 EfiAcpiResetValue;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID
EfipPcatResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    )

/*++

Routine Description:

    This routine resets the entire platform.

Arguments:

    ResetType - Supplies the type of reset to perform.

    ResetStatus - Supplies the status code for this reset.

    DataSize - Supplies the size of the reset data.

    ResetData - Supplies an optional pointer for reset types of cold, warm, or
        shutdown to a null-terminated string, optionally followed by additional
        binary data.

Return Value:

    None. This routine does not return.

--*/

{

    volatile UINT8 *ResetRegister;
    UINT8 Value;

    //
    // Use the ACPI reset mechanism if there is one.
    //

    if (EfiAcpiResetValid != FALSE) {
        if (EfiAcpiResetIoPort != FALSE) {
            EfiIoPortOut8((UINT16)EfiAcpiResetAddress, EfiAcpiResetValue);

        } else {
            ResetRegister = (volatile UINT8 *)EfiAcpiResetAddress;
            *ResetRegister = EfiAcpiResetValue;
        }

        if (EfiBootServices != NULL) {
            EfiStall(100000);

        } else {
            while (TRUE) {
                NOTHING;
            }
        }
    }

    //
    // Either there was no ACPI reset mechanism or it didn't work. Try the
    // keyboard controller.
    //

    do {
        Value = EfiIoPortIn8(EFI_PCAT_8042_CONTROL_PORT);

    } while ((Value & EFI_PCAT_8042_INPUT_BUFFER_FULL) != 0);

    EfiIoPortOut8(EFI_PCAT_8042_CONTROL_PORT, EFI_PCAT_8042_RESET_VALUE);

    //
    // Just wait for that promised reset to kick in.
    //

    while (TRUE) {
        if (EfiBootServices != NULL) {
            EfiStall(1);
        }
    }

    return;
}

VOID
EfipPcatInitializeReset (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for reset system. This routine must run
    with boot services.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PFADT Fadt;

    if (EfiIsAtRuntime() != FALSE) {
        return;
    }

    Fadt = EfipPcatGetFadt();
    if (Fadt == NULL) {
        return;
    }

    if ((Fadt->ResetRegister.RegisterBitWidth != 0) &&
        (Fadt->ResetRegister.Address != 0) &&
        ((Fadt->ResetRegister.AddressSpaceId == AddressSpaceMemory) ||
         (Fadt->ResetRegister.AddressSpaceId == AddressSpaceIo))) {

        if (Fadt->ResetRegister.AddressSpaceId == AddressSpaceIo) {
            EfiAcpiResetIoPort = TRUE;
        }

        EfiAcpiResetAddress = Fadt->ResetRegister.Address;
        EfiAcpiResetValue = Fadt->ResetValue;
        EfiAcpiResetValid = TRUE;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

PFADT
EfipPcatGetFadt (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the FADT in the configuration table.

Arguments:

    None.

Return Value:

    Returns a pointer to the FADT on success.

    NULL on failure.

--*/

{

    PRSDP Rsdp;
    PRSDT Rsdt;
    PULONG RsdtTableEntry;
    PDESCRIPTION_HEADER Table;
    UINTN TableCount;
    UINTN TableIndex;

    Rsdp = EfipPcatGetRsdpFromEfiSystemTable();
    if (Rsdp == NULL) {
        return NULL;
    }

    Rsdt = (PRSDT)(Rsdp->RsdtAddress);
    if (Rsdt == NULL) {
        return NULL;
    }

    TableCount = (Rsdt->Header.Length - sizeof(DESCRIPTION_HEADER)) /
                 sizeof(UINT32);

    RsdtTableEntry = (PULONG)&(Rsdt->Entries);
    for (TableIndex = 0; TableIndex < TableCount; TableIndex += 1) {
        Table = (PDESCRIPTION_HEADER)(UINTN)(RsdtTableEntry[TableIndex]);
        if (Table == NULL) {
            continue;
        }

        if (Table->Signature == FADT_SIGNATURE) {
            return (PFADT)Table;
        }
    }

    return NULL;
}

PRSDP
EfipPcatGetRsdpFromEfiSystemTable (
    VOID
    )

/*++

Routine Description:

    This routine attempts to find the RSDP in the EFI system table.

Arguments:

    None.

Return Value:

    Returns a pointer to the RSDP on success.

    NULL on failure.

--*/

{

    BOOLEAN Match;
    EFI_CONFIGURATION_TABLE *Table;
    UINTN TableCount;
    UINTN TableIndex;

    TableCount = EfiSystemTable->NumberOfTableEntries;
    for (TableIndex = 0; TableIndex < TableCount; TableIndex += 1) {
        Table = &(EfiSystemTable->ConfigurationTable[TableIndex]);
        Match = EfipPcatCompareGuids(&(Table->VendorGuid), &EfiAcpiTableGuid);
        if (Match != FALSE) {
            return Table->VendorTable;
        }

        Match = EfipPcatCompareGuids(&(Table->VendorGuid), &EfiAcpiTable1Guid);
        if (Match != FALSE) {
            return Table->VendorTable;
        }
    }

    return NULL;
}

BOOLEAN
EfipPcatCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    )

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

{

    UINT32 *FirstPointer;
    UINT32 *SecondPointer;

    //
    // Compare GUIDs 32 bits at a time.
    //

    FirstPointer = (UINT32 *)FirstGuid;
    SecondPointer = (UINT32 *)SecondGuid;
    if ((FirstPointer[0] == SecondPointer[0]) &&
        (FirstPointer[1] == SecondPointer[1]) &&
        (FirstPointer[2] == SecondPointer[2]) &&
        (FirstPointer[3] == SecondPointer[3])) {

        return TRUE;
    }

    return FALSE;
}

