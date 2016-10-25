/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bdscon.c

Abstract:

    This module implements BDS console support functions.

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

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipBdsConnectConsoleVariable (
    CHAR16 *ConsoleVariableName
    );

EFI_STATUS
EfipBdsUpdateConsoleVariable (
    CHAR16 *VariableName,
    EFI_DEVICE_PATH_PROTOCOL *CustomizedDevicePath,
    EFI_DEVICE_PATH_PROTOCOL *ExclusiveDevicePath
    );

BOOLEAN
EfipBdsUpdateSystemTableConsole (
    CHAR16 *VariableName,
    EFI_GUID *ConsoleGuid,
    EFI_HANDLE *ConsoleHandle,
    VOID **ProtocolInterface
    );

BOOLEAN
EfipBdsIsConsoleVariableNonVolatile (
    CHAR16 *Name
    );

//
// -------------------------------------------------------------------- Globals
//

EFI_GUID EfiSimpleTextInputProtocolGuid = EFI_SIMPLE_TEXT_INPUT_PROTOCOL_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBdsConnectAllDefaultConsoles (
    VOID
    )

/*++

Routine Description:

    This routine connects the console device based on the console variables.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EFI_STATUS Status;
    BOOLEAN SystemTableUpdated;
    BOOLEAN Updated;

    Status = EfipBdsConnectConsoleVariable(L"ConOut");
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfipBdsConnectConsoleVariable(L"ConIn");
    EfipBdsConnectConsoleVariable(L"ErrOut");
    SystemTableUpdated = FALSE;

    //
    // Fill console handles in the system table if no console device is
    // assigned.
    //

    Updated = EfipBdsUpdateSystemTableConsole(
                                            L"ConIn",
                                            &EfiSimpleTextInputProtocolGuid,
                                            &(EfiSystemTable->ConsoleInHandle),
                                            (VOID **)&(EfiSystemTable->ConIn));

    if (Updated != FALSE) {
        SystemTableUpdated = TRUE;
    }

    Updated = EfipBdsUpdateSystemTableConsole(
                                           L"ConOut",
                                           &EfiSimpleTextOutputProtocolGuid,
                                           &(EfiSystemTable->ConsoleOutHandle),
                                           (VOID **)&(EfiSystemTable->ConOut));

    if (Updated != FALSE) {
        SystemTableUpdated = TRUE;
    }

    Updated = EfipBdsUpdateSystemTableConsole(
                                        L"ErrOut",
                                        &EfiSimpleTextOutputProtocolGuid,
                                        &(EfiSystemTable->StandardErrorHandle),
                                        (VOID **)&(EfiSystemTable->StdErr));

    if (Updated != FALSE) {
        SystemTableUpdated = TRUE;
    }

    //
    // Recompute the CRC of the system table if it changed.
    //

    if (SystemTableUpdated != FALSE) {
        EfiSystemTable->Hdr.CRC32 = 0;
        EfiCalculateCrc32((UINT8 *)(&EfiSystemTable->Hdr),
                          EfiSystemTable->Hdr.HeaderSize,
                          &(EfiSystemTable->Hdr.CRC32));
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBdsConnectConsoleVariable (
    CHAR16 *ConsoleVariableName
    )

/*++

Routine Description:

    This routine connects the console device named by the given variable name.
    If the device path is multi-instance any any instance is connected, this
    routine returns success.

Arguments:

    ConsoleVariableName - Supplies a pointer to a string containing the name
        of the variable of the console. Valid values are ConIn, ConOut, and
        ErrOut.

Return Value:

    EFI status code.

--*/

{

    BOOLEAN DeviceExists;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathCopy;
    EFI_DEVICE_PATH_PROTOCOL *Instance;
    EFI_DEVICE_PATH_PROTOCOL *Next;
    UINTN Size;
    EFI_DEVICE_PATH_PROTOCOL *StartDevicePath;
    EFI_STATUS Status;
    UINTN VariableSize;

    Status = EFI_SUCCESS;
    DeviceExists = FALSE;
    StartDevicePath = EfipBdsGetVariable(ConsoleVariableName,
                                         &EfiGlobalVariableGuid,
                                         &VariableSize);

    if (StartDevicePath == NULL) {
        return EFI_UNSUPPORTED;
    }

    //
    // Loop across every instance in the variable.
    //

    DevicePathCopy = StartDevicePath;
    do {
        Instance = EfiCoreGetNextDevicePathInstance(&DevicePathCopy, &Size);
        if (Instance == NULL) {
            EfiCoreFreePool(StartDevicePath);
            return EFI_UNSUPPORTED;
        }

        Next = Instance;
        while (EfiCoreIsDevicePathEndType(Next) == FALSE) {
            Next = EfiCoreGetNextDevicePathNode(Next);
        }

        EfiCoreSetDevicePathEndNode(Next);

        //
        // This would be the place to check for a USB short form device path
        // and connect it directly.
        //

        Status = EfipBdsConnectDevicePath(Instance);
        if (EFI_ERROR(Status)) {
            EfipBdsUpdateConsoleVariable(ConsoleVariableName, NULL, Instance);

        } else {
            DeviceExists = TRUE;
        }

        EfiCoreFreePool(Instance);

    } while (DevicePathCopy != NULL);

    EfiCoreFreePool(StartDevicePath);
    if (DeviceExists == FALSE) {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipBdsUpdateConsoleVariable (
    CHAR16 *VariableName,
    EFI_DEVICE_PATH_PROTOCOL *CustomizedDevicePath,
    EFI_DEVICE_PATH_PROTOCOL *ExclusiveDevicePath
    )

/*++

Routine Description:

    This routine updates the console variable, adding or removing a device
    path from the variable.

Arguments:

    VariableName - Supplies a pointer to a string containing the name of the
        variable of the console. Valid values are ConIn, ConOut, and ErrOut.

    CustomizedDevicePath - Supplies an optional pointer to the device path to
        be added to the variable. This parameter cannot be multi-instance.

    ExclusiveDevicePath - Supplies an optional pointer to the device path to
        remove from the variable. This cannot be multi-instance.

Return Value:

    EFI status code.

--*/

{

    UINT32 Attributes;
    EFI_DEVICE_PATH_PROTOCOL *Console;
    UINTN DevicePathSize;
    EFI_DEVICE_PATH_PROTOCOL *NewDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *OldNewDevicePath;
    EFI_STATUS Status;

    Console = NULL;
    DevicePathSize = 0;
    if (CustomizedDevicePath == ExclusiveDevicePath) {
        return EFI_UNSUPPORTED;
    }

    Console = EfipBdsGetVariable(VariableName,
                                 &EfiGlobalVariableGuid,
                                 &DevicePathSize);

    NewDevicePath = Console;

    //
    // If the exclusive device path is part of the variable, delete it.
    //

    if ((ExclusiveDevicePath != NULL) && (Console != NULL)) {
        NewDevicePath = EfipBdsDeletePartialMatchInstance(Console,
                                                          ExclusiveDevicePath);
    }

    //
    // Try to append the customized device path.
    //

    if (CustomizedDevicePath != NULL) {
        if (EfipBdsMatchDevicePaths(NewDevicePath, CustomizedDevicePath) ==
            FALSE) {

            //
            // If there is a part of the customized path in the new device
            // path, delete it.
            //

            NewDevicePath = EfipBdsDeletePartialMatchInstance(
                                                         NewDevicePath,
                                                         CustomizedDevicePath);

            OldNewDevicePath = NewDevicePath;
            NewDevicePath = EfiCoreAppendDevicePathInstance(
                                                         NewDevicePath,
                                                         CustomizedDevicePath);

            if (OldNewDevicePath != NULL) {
                EfiCoreFreePool(OldNewDevicePath);
            }
        }
    }

    //
    // The attributes for ConInDev, ConOutDev, and ErrOutDev is not
    // non-volatile.
    //

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS;

    if (EfipBdsIsConsoleVariableNonVolatile(VariableName) != FALSE) {
        Attributes |= EFI_VARIABLE_NON_VOLATILE;
    }

    //
    // Finally, update the variable of the default console.
    //

    DevicePathSize = EfiCoreGetDevicePathSize(NewDevicePath);
    Status = EfiSetVariable(VariableName,
                            &EfiGlobalVariableGuid,
                            Attributes,
                            DevicePathSize,
                            NewDevicePath);

    if ((DevicePathSize == 0) || (Status == EFI_NOT_FOUND)) {
        Status = EFI_SUCCESS;
    }

    ASSERT(!EFI_ERROR(Status));

    if (Console != NULL) {
        EfiCoreFreePool(Console);
    }

    if ((Console != NewDevicePath) && (NewDevicePath != NULL)) {
        EfiCoreFreePool(NewDevicePath);
    }

    return Status;
}

BOOLEAN
EfipBdsUpdateSystemTableConsole (
    CHAR16 *VariableName,
    EFI_GUID *ConsoleGuid,
    EFI_HANDLE *ConsoleHandle,
    VOID **ProtocolInterface
    )

/*++

Routine Description:

    This routine fills in the console handle in the system table if there are
    no valid console handles.

Arguments:

    VariableName - Supplies a pointer to the string containing the standard
        console variable name.

    ConsoleGuid - Supplies a pointer to the console protocol GUID.

    ConsoleHandle - Supplies a pointer that on input points to the console
        handle in the system table to be checked. On output, this value may be
        updated.

    ProtocolInterface - Supplies a pointer that on input points to the console
        handle interface in the system table to be checked. On output, this
        value may be updated.

Return Value:

    TRUE if the system tablw as updated.

    FALSE if the table did not change.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *Console;
    UINTN DevicePathSize;
    EFI_DEVICE_PATH_PROTOCOL *FullDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *Instance;
    VOID *Interface;
    BOOLEAN IsTextOut;
    EFI_HANDLE NewHandle;
    EFI_STATUS Status;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOut;

    ASSERT((VariableName != NULL) && (ConsoleHandle != NULL) &&
           (ConsoleGuid != NULL) && (ProtocolInterface != NULL));

    if (*ConsoleHandle != NULL) {
        Status = EfiHandleProtocol(*ConsoleHandle,
                                   ConsoleGuid,
                                   &Interface);

        if ((Status == EFI_SUCCESS) && (Interface == *ProtocolInterface)) {
            return FALSE;
        }
    }

    //
    // Get all possible device paths from the variable.
    //

    Console = EfipBdsGetVariable(VariableName,
                                 &EfiGlobalVariableGuid,
                                 &DevicePathSize);

    if (Console == NULL) {
        return FALSE;
    }

    //
    // Loop over every instance path in the device path.
    //

    FullDevicePath = Console;
    do {
        Instance = EfiCoreGetNextDevicePathInstance(&Console, &DevicePathSize);
        if (Instance == NULL) {
            EfiCoreFreePool(FullDevicePath);

            ASSERT(FALSE);
        }

        //
        // Find the console device handle with the instance.
        //

        Status = EfiLocateDevicePath(ConsoleGuid, &Instance, &NewHandle);
        if (!EFI_ERROR(Status)) {

            //
            // Get the console protocol on this handle.
            //

            Status = EfiHandleProtocol(NewHandle, ConsoleGuid, &Interface);
            if (!EFI_ERROR(Status)) {
                *ConsoleHandle = NewHandle;
                *ProtocolInterface = Interface;

                //
                // If it's a console out device, set the mode if the mode is
                // not valid.
                //

                IsTextOut = EfiCoreCompareGuids(
                                             ConsoleGuid,
                                             &EfiSimpleTextOutputProtocolGuid);

                if (IsTextOut != FALSE) {
                    TextOut = (EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *)Interface;
                    if (TextOut->Mode->Mode == -1) {
                        TextOut->SetMode(TextOut, 0);
                    }
                }

                return TRUE;
            }
        }

    } while (Instance != NULL);

    //
    // No available console device was found.
    //

    return FALSE;
}

BOOLEAN
EfipBdsIsConsoleVariableNonVolatile (
    CHAR16 *Name
    )

/*++

Routine Description:

    This routine returns whether or not a given console variable name should be
    set with the non-volatile flag. If the device ends in Dev, then it returns
    FALSE, otherwise it returns TRUE.

Arguments:

    Name - Supplies a pointer to the string containing the standard console
        variable name.

Return Value:

    TRUE if the variable is non-volatile.

    FALSE if the variable is volatile.

--*/

{

    CHAR16 *Character;

    Character = Name;
    while (*Character != L'\0') {
        Character += 1;
    }

    if (((INTN)((UINTN)Character - (UINTN)Name) / sizeof(CHAR16)) <= 3) {
        return TRUE;
    }

    if ((*(Character - 3) == L'D') && (*(Character - 2) == L'e') &&
        (*(Character - 1) == L'v')) {

        return FALSE;
    }

    return TRUE;
}

