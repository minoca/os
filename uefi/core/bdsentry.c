/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bdsentry.c

Abstract:

    This module implements the high level Boot Device Selection code.

Author:

    Evan Green 17-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "bds.h"
#include <minoca/uefi/guid/globlvar.h>
#include <minoca/uefi/guid/coninct.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_FIRMWARE_REVISION 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipBdsBootDeviceSelect (
    VOID
    );

VOID
EfipBdsFormalizeEfiGlobalVariables (
    VOID
    );

VOID
EfipBdsFormalizeConsoleVariable (
    CHAR16 *VariableName
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the firmware vendor, whose contents are not assumed to be in
// runtime data. The platform code can override this variable.
//

CHAR16 *EfiFirmwareVendor = L"Minoca Corp";
UINT32 EfiFirmwareRevision = EFI_FIRMWARE_REVISION;

//
// Define the default timeout value.
//

UINT16 EfiBootTimeout = 0xFFFF;

UINT16 *EfiBootNext;

EFI_GUID EfiGlobalVariableGuid = EFI_GLOBAL_VARIABLE_GUID;
EFI_GUID EfiConnectConInEventGuid = CONNECT_CONIN_EVENT_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID
EfiBdsEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point into the boot device selection phase of
    the firmware. It attempts to find an OS loader and launch it.

Arguments:

    None.

Return Value:

    None. This routine does not return.

--*/

{

    UINT32 Attributes;
    UINTN BootNextSize;
    LIST_ENTRY BootOptionList;
    UINTN BootTimeout;
    LIST_ENTRY DriverOptionList;
    CHAR16 *FirmwareVendor;
    UINTN FirmwareVendorSize;
    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    UINTN OldHandleCount;
    EFI_STATUS Status;

    INITIALIZE_LIST_HEAD(&DriverOptionList);
    INITIALIZE_LIST_HEAD(&BootOptionList);

    //
    // Copy the firmware vendor into runtime pool and set the firmware revision.
    // This then requires recomputing the CRC of the system table.
    //

    if (EfiFirmwareVendor != NULL) {
        FirmwareVendorSize = EfiCoreStringLength(EfiFirmwareVendor);
        FirmwareVendorSize = (FirmwareVendorSize + 1) * sizeof(CHAR16);
        FirmwareVendor = EfiCoreAllocateRuntimePool(FirmwareVendorSize);
        if (FirmwareVendor != NULL) {
            EfiCoreCopyMemory(FirmwareVendor,
                              EfiFirmwareVendor,
                              FirmwareVendorSize);

            EfiSystemTable->FirmwareVendor = FirmwareVendor;
        }
    }

    EfiSystemTable->FirmwareRevision = EfiFirmwareRevision;
    EfiSystemTable->Hdr.CRC32 = 0;
    EfiCalculateCrc32(EfiSystemTable,
                      sizeof(EFI_SYSTEM_TABLE),
                      &(EfiSystemTable->Hdr.CRC32));

    //
    // Connect all controllers.
    //

    HandleCount = 0;
    while (TRUE) {
        OldHandleCount = HandleCount;
        Status = EfiLocateHandleBuffer(AllHandles,
                                       NULL,
                                       NULL,
                                       &HandleCount,
                                       &HandleBuffer);

        if (EFI_ERROR(Status)) {
            break;
        }

        if (HandleCount == OldHandleCount) {
            break;
        }

        for (Index = 0; Index < HandleCount; Index += 1) {
            EfiConnectController(HandleBuffer[Index], NULL, NULL, TRUE);
        }

        EfiFreePool(HandleBuffer);
    }

    EfiCoreLoadVariablesFromFileSystem();
    EfipBdsFormalizeEfiGlobalVariables();
    EfipBdsConnectAllDefaultConsoles();
    BootTimeout = EfiBootTimeout;
    if (BootTimeout != 0xFFFF) {
        Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                     EFI_VARIABLE_RUNTIME_ACCESS |
                     EFI_VARIABLE_NON_VOLATILE;

        Status = EfiSetVariable(L"Timeout",
                                &EfiGlobalVariableGuid,
                                Attributes,
                                sizeof(UINT16),
                                &BootTimeout);

        ASSERT(!EFI_ERROR(Status));
    }

    //
    // Set up the device list based on EFI 1.1 variables. Process Driver####
    // and load the drivers in the option list.
    //

    EfipBdsBuildOptionFromVariable(&DriverOptionList, L"DriverOrder");
    if (!LIST_EMPTY(&DriverOptionList)) {
        EfipBdsLoadDrivers(&DriverOptionList);
    }

    //
    // Look for a boot next option.
    //

    EfiBootNext = EfipBdsGetVariable(L"BootNext",
                                     &EfiGlobalVariableGuid,
                                     &BootNextSize);

    EfipBdsBootDeviceSelect();

    //
    // Execution should never reach here.
    //

    ASSERT(FALSE);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipBdsBootDeviceSelect (
    VOID
    )

/*++

Routine Description:

    This routine validates the global variables set in EFI for the BDS phase.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Attributes;
    LIST_ENTRY BootList;
    PLIST_ENTRY BootNextEntry;
    BOOLEAN BootNextExists;
    PEFI_BDS_COMMON_OPTION BootOption;
    CHAR16 Buffer[20];
    EFI_EVENT ConnectInputEvent;
    PLIST_ENTRY CurrentEntry;
    CHAR16 *ExitData;
    UINTN ExitDataSize;
    EFI_STATUS Status;
    BOOLEAN TriedEverything;

    BootNextEntry = NULL;
    BootNextExists = FALSE;
    CurrentEntry = NULL;
    ConnectInputEvent = NULL;
    INITIALIZE_LIST_HEAD(&BootList);
    TriedEverything = FALSE;
    EfiCoreSetMemory(Buffer, sizeof(Buffer), 0);

    //
    // Create an event to fire when console input is connected.
    //

    Status = EfiCreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              EfiCoreEmptyCallbackFunction,
                              NULL,
                              &EfiConnectConInEventGuid,
                              &ConnectInputEvent);

    if (EFI_ERROR(Status)) {
        ConnectInputEvent = NULL;
    }

    if (EfiBootNext != NULL) {
        BootNextExists = TRUE;

        //
        // Clear the variable so that it only tries to boot once.
        //

        Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                     EFI_VARIABLE_RUNTIME_ACCESS |
                     EFI_VARIABLE_NON_VOLATILE;

        EfiSetVariable(L"BootNext",
                       &EfiGlobalVariableGuid,
                       Attributes,
                       0,
                       NULL);

        //
        // Add the boot next option.
        //

        EfipBdsCreateHexCodeString(L"Boot",
                                   *EfiBootNext,
                                   Buffer,
                                   sizeof(Buffer));

        BootOption = EfipBdsConvertVariableToOption(&BootList, Buffer);
        if (BootOption == NULL) {
            return;
        }

        BootOption->BootCurrent = *EfiBootNext;
    }

    //
    // Parse the boot order to get boot options.
    //

    EfipBdsBuildOptionFromVariable(&BootList, L"BootOrder");

    //
    // If nothing was enumerated, get desperate.
    //

    if (LIST_EMPTY(&BootList)) {
        EfipBdsEnumerateAllBootOptions(&BootList);
        TriedEverything = TRUE;
    }

    CurrentEntry = BootList.Next;
    if (CurrentEntry == NULL) {

        ASSERT(FALSE);

        return;
    }

    //
    // Loop forever.
    //

    while (TRUE) {

        //
        // Handle reaching the end of the list.
        //

        if (CurrentEntry == &BootList) {
            if (TriedEverything == FALSE) {
                EfipBdsEnumerateAllBootOptions(&BootList);
                TriedEverything = TRUE;
                CurrentEntry = BootList.Next;
                continue;
            }

            if (ConnectInputEvent != NULL) {
                EfiSignalEvent(ConnectInputEvent);
            }

            if (EfiSystemTable->StdErr != NULL) {
                EfiSystemTable->StdErr->OutputString(
                                                EfiSystemTable->StdErr,
                                                L"Found nothing to boot.\r\n");
            }

            //
            // Hmm... eventually do something more intelligent here.
            //

            RtlDebugPrint("Nothing to boot, hanging...\r\n");
            while (TRUE) {
                NOTHING;
            }

            INITIALIZE_LIST_HEAD(&BootList);
            EfipBdsBuildOptionFromVariable(&BootList, L"BootOrder");
            CurrentEntry = BootList.Next;
            continue;
        }

        //
        // Grab the boot option.
        //

        BootOption = LIST_VALUE(CurrentEntry, EFI_BDS_COMMON_OPTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(BootOption->Magic == EFI_BDS_COMMON_OPTION_MAGIC);

        //
        // Skip anything not marked active.
        //

        if ((BootOption->Attribute & LOAD_OPTION_ACTIVE) == 0) {
            continue;
        }

        //
        // Make sure the device path is connected, except for BBS paths.
        //

        if (EfiCoreGetDevicePathType(BootOption->DevicePath) !=
            BBS_DEVICE_PATH) {

            EfipBdsConnectDevicePath(BootOption->DevicePath);
        }

        Status = EfipBdsBootViaBootOption(BootOption,
                                          BootOption->DevicePath,
                                          &ExitDataSize,
                                          &ExitData);

        if (Status != EFI_SUCCESS) {

            //
            // Potentially do something if the boot entry failed. For now,
            // nothing.
            //

        } else {
            if (ConnectInputEvent != NULL) {
                EfiSignalEvent(ConnectInputEvent);
            }

            //
            // This is where the boot menu would be presented, which might
            // change the boot list. Re-enumerate that now even though there
            // is no boot menu.
            //

            if (BootNextExists != FALSE) {
                BootNextEntry = BootList.Next;
            }

            INITIALIZE_LIST_HEAD(&BootList);
            if (BootNextEntry != NULL) {
                INSERT_BEFORE(BootNextEntry, &BootList);
            }

            EfipBdsBuildOptionFromVariable(&BootList, L"BootOrder");
            CurrentEntry = BootList.Next;
        }
    }

    //
    // Execution should never get here.
    //

    ASSERT(FALSE);

    return;
}

VOID
EfipBdsFormalizeEfiGlobalVariables (
    VOID
    )

/*++

Routine Description:

    This routine validates the global variables set in EFI for the BDS phase.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfipBdsFormalizeConsoleVariable(L"ConIn");
    EfipBdsFormalizeConsoleVariable(L"ConOut");
    EfipBdsFormalizeConsoleVariable(L"ErrOut");
    return;
}

VOID
EfipBdsFormalizeConsoleVariable (
    CHAR16 *VariableName
    )

/*++

Routine Description:

    This routine validates that one of the console variables is a valid
    device path.

Arguments:

    VariableName - Supplies a pointer to the variable name. This is expected to
        be ConIn, ConOut, or ErrOut.

Return Value:

    None.

--*/

{

    UINT32 Attributes;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN DevicePathSize;
    EFI_GUID *Guid;
    UINTN HandleCount;
    EFI_HANDLE *Handles;
    EFI_STATUS Status;
    UINTN VariableSize;

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS |
                 EFI_VARIABLE_NON_VOLATILE;

    DevicePath = EfipBdsGetVariable(VariableName,
                                    &EfiGlobalVariableGuid,
                                    &VariableSize);

    //
    // If the device path is not set, try to find one.
    //

    if (DevicePath == NULL) {

        //
        // For ConIn, get the input protocol, otherwise get the simple text
        // output protocol.
        //

        if ((VariableName[0] != L'\0') && (VariableName[1] != L'\0') &&
            (VariableName[2] != L'\0') && (VariableName[3] == L'I')) {

            Guid = &EfiSimpleTextInputProtocolGuid;

        } else {
            Guid = &EfiSimpleTextOutputProtocolGuid;
        }

        HandleCount = 0;
        Handles = NULL;
        Status = EfiLocateHandleBuffer(ByProtocol,
                                       Guid,
                                       NULL,
                                       &HandleCount,
                                       &Handles);

        if ((!EFI_ERROR(Status)) && (HandleCount != 0)) {
            DevicePath = EfiCoreGetDevicePathFromHandle(Handles[0]);
            if (DevicePath != NULL) {
                DevicePathSize = EfiCoreGetDevicePathSize(DevicePath);
                EfiSetVariable(VariableName,
                               &EfiGlobalVariableGuid,
                               Attributes,
                               DevicePathSize,
                               DevicePath);
            }

            EfiFreePool(Handles);
        }
    }

    //
    // If the device path is invalid, delete it.
    //

    if ((DevicePath != NULL) &&
        (EfiCoreIsDevicePathValid(DevicePath, VariableSize) == FALSE)) {

        RtlDebugPrint("Deleting invalid console variable.\n");
        Status = EfiSetVariable(VariableName,
                                &EfiGlobalVariableGuid,
                                Attributes,
                                0,
                                NULL);

        ASSERT(!EFI_ERROR(Status));

        DevicePath = NULL;
    }

    return;
}

