/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bdsutil.c

Abstract:

    This module implements support routines for the Boot Device Selection
    module.

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
#include "fileinfo.h"
#include <minoca/uefi/protocol/loadimg.h>
#include <minoca/uefi/protocol/sfilesys.h>

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
EfipBdsConnectAllEfi (
    VOID
    );

EFI_STATUS
EfipBdsDisconnectAllEfi (
    VOID
    );

UINT16
EfipBdsGetHexCodeFromString (
    CHAR16 *HexCodeString
    );

UINTN
EfipBdsStringSize (
    CONST CHAR16 *String,
    UINTN MaxStringSize
    );

UINTN
EfipBdsGetDevicePathSize (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINTN MaxSize
    );

UINT16
EfipBdsGetFreeOptionNumber (
    CHAR16 *VariableName
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipBdsConnectAll (
    VOID
    )

/*++

Routine Description:

    This routine connects all system drivers to controllers first, then
    specially connect the default console. This ensures all system controllers
    are available and the platform default console is connected.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EfipBdsConnectAllDefaultConsoles();
    EfipBdsConnectAllDriversToAllControllers();
    EfipBdsConnectAllDefaultConsoles();
}

VOID
EfipBdsConnectAllDriversToAllControllers (
    VOID
    )

/*++

Routine Description:

    This routine connects all system drivers to controllers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EFI_STATUS Status;

    do {
        EfipBdsConnectAllEfi();
        Status = EfiCoreDispatcher();

    } while (!EFI_ERROR(Status));

    return;
}

VOID
EfipBdsLoadDrivers (
    PLIST_ENTRY DriverList
    )

/*++

Routine Description:

    This routine loads and starts every driver on the given load list.

Arguments:

    DriverList - Supplies a pointer to the head of the list of boot options
        describing the drivers to load.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    CHAR16 *ExitData;
    UINTN ExitDataSize;
    EFI_HANDLE ImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInformation;
    PEFI_BDS_COMMON_OPTION Option;
    BOOLEAN ReconnectAll;
    EFI_STATUS Status;

    ReconnectAll = FALSE;
    CurrentEntry = DriverList->Next;
    while (CurrentEntry != DriverList) {
        Option = LIST_VALUE(CurrentEntry, EFI_BDS_COMMON_OPTION, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        ASSERT(Option->Magic == EFI_BDS_COMMON_OPTION_MAGIC);

        //
        // Skip options not marked active.
        //

        if ((Option->Attribute & LOAD_OPTION_ACTIVE) == 0) {
            continue;
        }

        //
        // If the force reconnect is enabled, then all EFI drivers in the
        // system will be disconnected and reconnected after the last driver
        // load option is processed.
        //

        if ((Option->Attribute & LOAD_OPTION_FORCE_RECONNECT) != 0) {
            ReconnectAll = TRUE;
        }

        //
        // Make sure the driver path is connected.
        //

        EfipBdsConnectDevicePath(Option->DevicePath);

        //
        // Load and start the image that Driver#### describes.
        //

        Status = EfiLoadImage(FALSE,
                              EfiFirmwareImageHandle,
                              Option->DevicePath,
                              NULL,
                              0,
                              &ImageHandle);

        if (!EFI_ERROR(Status)) {
            Status = EfiHandleProtocol(ImageHandle,
                                       &EfiLoadedImageProtocolGuid,
                                       (VOID **)&ImageInformation);

            ASSERT(!EFI_ERROR(Status));

            //
            // Verify that this image is a driver.
            //

            if ((ImageInformation == NULL) ||
                ((ImageInformation->ImageCodeType != EfiBootServicesCode) &&
                 (ImageInformation->ImageCodeType != EfiRuntimeServicesCode))) {

                EfiExit(ImageHandle, EFI_INVALID_PARAMETER, 0, NULL);
                continue;
            }

            if (Option->LoadOptionsSize != 0) {
                ImageInformation->LoadOptionsSize = Option->LoadOptionsSize;
                ImageInformation->LoadOptions = Option->LoadOptions;
            }

            //
            // Enable the watchdog timer for 5 minutes.
            //

            EfiSetWatchdogTimer(EFI_DEFAULT_WATCHDOG_DURATION, 0, 0, NULL);

            //
            // Go launch the driver.
            //

            Status = EfiStartImage(ImageHandle, &ExitDataSize, &ExitData);

            //
            // Clear the watchdog timer, as the image has returned.
            //

            EfiSetWatchdogTimer(0, 0, 0, NULL);
        }
    }

    if (ReconnectAll != FALSE) {
        EfipBdsDisconnectAllEfi();
        EfipBdsConnectAll();
    }

    return;
}

EFI_STATUS
EfipBdsBuildOptionFromVariable (
    PLIST_ENTRY OptionList,
    CHAR16 *VariableName
    )

/*++

Routine Description:

    This routine processes BootOrder or DriverOrder variables.

Arguments:

    OptionList - Supplies a pointer to the head of the list of boot or
        driver options.

    VariableName - Supplies a pointer to the variable name indicating the boot
        order or driver order. Typically this should be BootOrder or
        DriverOrder.

Return Value:

    EFI status code.

--*/

{

    UINTN Index;
    PEFI_BDS_COMMON_OPTION Option;
    CHAR16 OptionName[20];
    UINT16 *OptionOrder;
    UINTN OptionOrderSize;

    EfiCoreSetMemory(OptionName, sizeof(OptionName), 0);

    //
    // Read in the BootOrder or DriverOrder variable.
    //

    OptionOrder = EfipBdsGetVariable(VariableName,
                                     &EfiGlobalVariableGuid,
                                     &OptionOrderSize);

    if (OptionOrder == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    for (Index = 0; Index < (OptionOrderSize / sizeof(UINT16)); Index += 1) {
        if (*VariableName == 'B') {
            EfipBdsCreateHexCodeString(L"Boot",
                                       OptionOrder[Index],
                                       OptionName,
                                       sizeof(OptionName));

        } else {
            EfipBdsCreateHexCodeString(L"Driver",
                                       OptionOrder[Index],
                                       OptionName,
                                       sizeof(OptionName));
        }

        Option = EfipBdsConvertVariableToOption(OptionList, OptionName);
        if (Option != NULL) {
            Option->BootCurrent = OptionOrder[Index];
        }
    }

    EfiCoreFreePool(OptionOrder);
    return EFI_SUCCESS;
}

PEFI_BDS_COMMON_OPTION
EfipBdsConvertVariableToOption (
    PLIST_ENTRY OptionList,
    CHAR16 *VariableName
    )

/*++

Routine Description:

    This routine builds a Boot#### or Driver#### option from the given variable
    name. The new option will also be linked into the given list.

Arguments:

    OptionList - Supplies a pointer to the list to link the option into upon
        success.

    VariableName - Supplies a pointer to the variable name, which is typically
        in the form Boot#### or Driver####.

Return Value:

    Returns a pointer to the option on success.

    NULL on failure.

--*/

{

    UINT32 Attribute;
    UINT8 *CurrentOffset;
    CHAR16 *Description;
    UINTN DescriptionSize;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN DevicePathSize;
    UINT16 FilePathSize;
    VOID *LoadOptions;
    UINT32 LoadOptionsSize;
    PEFI_BDS_COMMON_OPTION Option;
    EFI_STATUS Status;
    UINT8 *Variable;
    UINTN VariableSize;

    Option = NULL;
    Status = EFI_OUT_OF_RESOURCES;

    //
    // Read in the variable.
    //

    Variable = EfipBdsGetVariable(VariableName,
                                  &EfiGlobalVariableGuid,
                                  &VariableSize);

    if (Variable == NULL) {
        return NULL;
    }

    if (EfipBdsValidateOption(Variable, VariableSize) == FALSE) {
        goto BdsVariableToOptionEnd;
    }

    //
    // Pull the members of this variable length structure out of the binary
    // blob. Start with the option attribute.
    //

    CurrentOffset = Variable;
    Attribute = *(UINT32 *)Variable;
    CurrentOffset += sizeof(UINT32);

    //
    // Get the options device path size.
    //

    FilePathSize = *(UINT16 *)CurrentOffset;
    CurrentOffset += sizeof(UINT16);

    //
    // Get the option's description string.
    //

    Description = (CHAR16 *)CurrentOffset;
    DescriptionSize = (EfiCoreStringLength(Description) + 1) * sizeof(CHAR16);
    CurrentOffset += DescriptionSize;

    //
    // Get the option's device path.
    //

    DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)CurrentOffset;
    CurrentOffset += FilePathSize;

    //
    // Get the load option data.
    //

    LoadOptions = CurrentOffset;
    LoadOptionsSize = (UINT32)(VariableSize -
                               ((UINTN)CurrentOffset - (UINTN)Variable));

    Option = EfiCoreAllocateBootPool(sizeof(EFI_BDS_COMMON_OPTION));
    if (Option == NULL) {
        goto BdsVariableToOptionEnd;
    }

    EfiCoreSetMemory(Option, sizeof(EFI_BDS_COMMON_OPTION), 0);
    Option->Magic = EFI_BDS_COMMON_OPTION_MAGIC;
    DevicePathSize = EfiCoreGetDevicePathSize(DevicePath);
    Option->DevicePath = EfiCoreAllocateBootPool(DevicePathSize);
    if (Option->DevicePath == NULL) {
        goto BdsVariableToOptionEnd;
    }

    EfiCoreCopyMemory(Option->DevicePath, DevicePath, DevicePathSize);
    Option->Attribute = Attribute;
    Option->Description = EfiCoreAllocateBootPool(DescriptionSize);
    if (Option->Description == NULL) {
        goto BdsVariableToOptionEnd;
    }

    EfiCoreCopyMemory(Option->Description, Description, DescriptionSize);
    Option->LoadOptions = EfiCoreAllocateBootPool(LoadOptionsSize);
    if (Option->LoadOptions == NULL) {
        goto BdsVariableToOptionEnd;
    }

    EfiCoreCopyMemory(Option->LoadOptions, LoadOptions, LoadOptionsSize);
    Option->LoadOptionsSize = LoadOptionsSize;

    //
    // Get the value from the variable name string if this is a boot option.
    //

    if (*VariableName == L'B') {
        Option->BootCurrent = EfipBdsGetHexCodeFromString(
                                                       &(VariableName[0]) + 4);
    }

    INSERT_BEFORE(&(Option->ListEntry), OptionList);
    Status = EFI_SUCCESS;

BdsVariableToOptionEnd:
    if (Variable != NULL) {
        EfiCoreFreePool(Variable);
    }

    if (EFI_ERROR(Status)) {
        if (Option != NULL) {
            if (Option->DevicePath != NULL) {
                EfiCoreFreePool(Option->DevicePath);
            }

            if (Option->Description != NULL) {
                EfiCoreFreePool(Option->Description);
            }

            if (Option->LoadOptions != NULL) {
                EfiCoreFreePool(Option->LoadOptions);
            }

            EfiCoreFreePool(Option);
            Option = NULL;
        }
    }

    return Option;
}

VOID *
EfipBdsGetVariable (
    CHAR16 *Name,
    EFI_GUID *VendorGuid,
    UINTN *VariableSize
    )

/*++

Routine Description:

    This routine reads the given EFI variable and returns a buffer allocated
    from pool containing its contents.

Arguments:

    Name - Supplies a pointer to the name of the variable to get.

    VendorGuid - Supplies the GUID part of the variable name.

    VariableSize - Supplies a pointer where the size of the variable contents
        will be returned on success.

Return Value:

    Returns a pointer to the variable contents allocated from pool on success.
    The caller is responsible for freeing this memory.

    NULL on failure.

--*/

{

    VOID *Buffer;
    UINTN BufferSize;
    EFI_STATUS Status;

    Buffer = NULL;

    //
    // Call once to find out the size.
    //

    BufferSize = 0;
    Status = EfiGetVariable(Name, VendorGuid, NULL, &BufferSize, Buffer);
    if (Status == EFI_BUFFER_TOO_SMALL) {
        Buffer = EfiCoreAllocateBootPool(BufferSize);
        if (Buffer == NULL) {
            *VariableSize = 0;
            return NULL;
        }

        EfiCoreSetMemory(Buffer, BufferSize, 0);

        //
        // Now read it for real.
        //

        Status = EfiGetVariable(Name, VendorGuid, NULL, &BufferSize, Buffer);
        if (EFI_ERROR(Status)) {
            EfiCoreFreePool(Buffer);
            BufferSize = 0;
            Buffer = NULL;
        }
    }

    ASSERT(((Buffer == NULL) && (BufferSize == 0)) ||
           ((Buffer != NULL) && (BufferSize != 0)));

    *VariableSize = BufferSize;
    return Buffer;
}

EFI_DEVICE_PATH_PROTOCOL *
EfipBdsDeletePartialMatchInstance (
    EFI_DEVICE_PATH_PROTOCOL *MultiInstancePath,
    EFI_DEVICE_PATH_PROTOCOL *SingleInstance
    )

/*++

Routine Description:

    This routine deletes the instance in the given multi-instance device path
    that matches partly with the given instance.

Arguments:

    MultiInstancePath - Supplies a pointer to the multi-instance device path.

    SingleInstance - Supplies a pointer to the single-instance device path.

Return Value:

    Returns the modified multi-instance path on success.

    NULL on failure.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *Instance;
    UINTN InstanceSize;
    EFI_DEVICE_PATH_PROTOCOL *NewDevicePath;
    EFI_DEVICE_PATH_PROTOCOL *OldNewDevicePath;
    UINT SingleSize;
    UINTN Size;

    NewDevicePath = NULL;
    OldNewDevicePath = NULL;
    if ((MultiInstancePath == NULL) || (SingleInstance == NULL)) {
        return MultiInstancePath;
    }

    Instance = EfiCoreGetNextDevicePathInstance(&MultiInstancePath,
                                                &InstanceSize);

    SingleSize = EfiCoreGetDevicePathSize(SingleInstance) -
                 END_DEVICE_PATH_LENGTH;

    InstanceSize -= END_DEVICE_PATH_LENGTH;
    while (Instance != NULL) {
        Size = InstanceSize;
        if (SingleSize < InstanceSize) {
            Size = SingleSize;
        }

        //
        // If the instance doesn't match, append the instance.
        //

        if (EfiCoreCompareMemory(Instance, SingleInstance, Size) != 0) {
            OldNewDevicePath = NewDevicePath;
            NewDevicePath = EfiCoreAppendDevicePathInstance(NewDevicePath,
                                                            Instance);

            if (OldNewDevicePath != NULL) {
                EfiCoreFreePool(OldNewDevicePath);
            }
        }

        EfiCoreFreePool(Instance);
        Instance = EfiCoreGetNextDevicePathInstance(&MultiInstancePath,
                                                    &InstanceSize);

        InstanceSize -= END_DEVICE_PATH_LENGTH;
    }

    return NewDevicePath;
}

BOOLEAN
EfipBdsMatchDevicePaths (
    EFI_DEVICE_PATH_PROTOCOL *MultiInstancePath,
    EFI_DEVICE_PATH_PROTOCOL *SingleInstance
    )

/*++

Routine Description:

    This routine compares a device path structure to that of all nodes of a
    second device path instance.

Arguments:

    MultiInstancePath - Supplies a pointer to the multi-instance device path to
        search through.

    SingleInstance - Supplies a pointer to the single-instance device path to
        search for.

Return Value:

    TRUE if the single instance path exists somewhere in the multi-instance
    path.

    FALSE if th single instance path is not found anywhere in the
    multi-instance path.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL *Instance;
    UINTN Size;

    if ((MultiInstancePath == NULL) || (SingleInstance == NULL)) {
        return FALSE;
    }

    DevicePath = MultiInstancePath;
    Instance = EfiCoreGetNextDevicePathInstance(&DevicePath, &Size);
    while (Instance != NULL) {
        if (EfiCoreCompareMemory(SingleInstance, Instance, Size) == 0) {
            EfiCoreFreePool(Instance);
            return TRUE;
        }

        EfiCoreFreePool(Instance);
        Instance = EfiCoreGetNextDevicePathInstance(&DevicePath, &Size);
    }

    return FALSE;
}

EFI_STATUS
EfipBdsRegisterNewOption (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    CHAR16 *String,
    CHAR16 *VariableName
    )

/*++

Routine Description:

    This routine registers a new Boot#### or Driver#### option base on the
    given variable name. The BootOrder or DriverOrder will also be updated.

Arguments:

    DevicePath - Supplies a pointer to the device path of the option.

    String - Supplies a pointer to a string describing the option.

    VariableName - Supplies a pointer to the string BootOrder or DriverOrder
        to update.

Return Value:

    EFI status code.

--*/

{

    UINT32 Attributes;
    UINT16 BootOrderEntry;
    INTN CompareResult;
    CHAR16 *Description;
    UINTN DescriptionSize;
    UINTN DevicePathSize;
    UINTN Index;
    VOID *Option;
    EFI_DEVICE_PATH_PROTOCOL *OptionDevicePath;
    UINTN OptionDevicePathSize;
    UINT8 *OptionMember;
    CHAR16 OptionName[10];
    UINT16 *OptionOrder;
    UINTN OptionSize;
    UINT16 *OptionVariable;
    UINTN OptionVariableSize;
    UINTN OrderItemCount;
    UINT16 RegisterOptionNumber;
    EFI_STATUS Status;
    UINTN StringSize;
    BOOLEAN UpdateDescription;

    Option = NULL;
    OptionSize = 0;
    OptionDevicePath = NULL;
    OptionVariable = NULL;
    OptionVariableSize = 0;
    Description = NULL;
    OptionOrder = NULL;
    UpdateDescription = FALSE;
    Status = EFI_SUCCESS;
    EfiCoreSetMemory(OptionName, sizeof(OptionName), 0);
    OptionVariable = EfipBdsGetVariable(VariableName,
                                        &EfiGlobalVariableGuid,
                                        &OptionVariableSize);

    ASSERT((OptionVariableSize == 0) || (OptionVariable != NULL));

    for (Index = 0; Index < OptionVariableSize / sizeof(UINT16); Index += 1) {
        if (*VariableName == L'B') {
            EfipBdsCreateHexCodeString(L"Boot",
                                       OptionVariable[Index],
                                       OptionName,
                                       sizeof(OptionName));

        } else {
            EfipBdsCreateHexCodeString(L"Driver",
                                       OptionVariable[Index],
                                       OptionName,
                                       sizeof(OptionName));
        }

        Option = EfipBdsGetVariable(OptionName,
                                    &EfiGlobalVariableGuid,
                                    &OptionSize);

        if (Option == NULL) {
            continue;
        }

        if (EfipBdsValidateOption(Option, OptionSize) == FALSE) {
            continue;
        }

        OptionMember = Option;
        OptionMember += sizeof(UINT32) + sizeof(UINT16);
        Description = (CHAR16 *)OptionMember;
        DescriptionSize = (EfiCoreStringLength(Description) + 1) *
                          sizeof(CHAR16);

        OptionMember += DescriptionSize;
        OptionDevicePath = (EFI_DEVICE_PATH_PROTOCOL *)OptionMember;

        //
        // Check to see if the device path or description changed.
        //

        OptionDevicePathSize = EfiCoreGetDevicePathSize(OptionDevicePath);
        CompareResult = EfiCoreCompareMemory(OptionDevicePath,
                                             DevicePath,
                                             OptionDevicePathSize);

        if (CompareResult == 0) {
            CompareResult = EfiCoreCompareMemory(Description,
                                                 String,
                                                 DescriptionSize);

            //
            // This option already exists, so just return.
            //

            if (CompareResult == 0) {
                EfiCoreFreePool(Option);
                EfiCoreFreePool(OptionVariable);
                return EFI_SUCCESS;

            } else {
                UpdateDescription = TRUE;
                EfiCoreFreePool(Option);
                break;
            }
        }

        EfiCoreFreePool(Option);
    }

    //
    // Create the Boot#### or Driver#### boot option variable.
    //

    StringSize = (EfiCoreStringLength(String) + 1) * sizeof(CHAR16);
    OptionSize = sizeof(UINT32) + sizeof(UINT16) + StringSize;
    DevicePathSize = EfiCoreGetDevicePathSize(DevicePath);
    OptionSize += DevicePathSize;
    Option = EfiCoreAllocateBootPool(OptionSize);
    if (Option == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreSetMemory(Option, OptionSize, 0);
    OptionMember = Option;
    *(UINT32 *)OptionMember = LOAD_OPTION_ACTIVE;
    OptionMember += sizeof(UINT32);
    *(UINT16 *)OptionMember = DevicePathSize;
    OptionMember += sizeof(UINT16);
    EfiCoreCopyMemory(OptionMember, String, StringSize);
    OptionMember += StringSize;
    EfiCoreCopyMemory(OptionMember, DevicePath, DevicePathSize);
    if (UpdateDescription != FALSE) {

        ASSERT(OptionVariable != NULL);

        RegisterOptionNumber = OptionVariable[Index];

    } else {
        RegisterOptionNumber = EfipBdsGetFreeOptionNumber(VariableName);
    }

    if (*VariableName == L'B') {
        EfipBdsCreateHexCodeString(L"Boot",
                                   RegisterOptionNumber,
                                   OptionName,
                                   sizeof(OptionName));

    } else {
        EfipBdsCreateHexCodeString(L"Driver",
                                   RegisterOptionNumber,
                                   OptionName,
                                   sizeof(OptionName));
    }

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS |
                 EFI_VARIABLE_NON_VOLATILE;

    Status = EfiSetVariable(OptionName,
                            &EfiGlobalVariableGuid,
                            Attributes,
                            OptionSize,
                            Option);

    if ((EFI_ERROR(Status)) || (UpdateDescription != FALSE)) {
        EfiCoreFreePool(Option);
        if (OptionVariable != NULL) {
            EfiCoreFreePool(OptionVariable);
        }

        return Status;
    }

    EfiCoreFreePool(Option);

    //
    // Update the option order variable. If there was no option order, set one.
    //

    if (OptionVariableSize == 0) {
        BootOrderEntry = RegisterOptionNumber;
        Status = EfiSetVariable(VariableName,
                                &EfiGlobalVariableGuid,
                                Attributes,
                                sizeof(UINT16),
                                &BootOrderEntry);

        if (OptionVariable != NULL) {
            EfiCoreFreePool(OptionVariable);
        }

        return Status;
    }

    ASSERT(OptionVariable != NULL);

    //
    // Append the new option number to the original option order.
    //

    OrderItemCount = (OptionVariableSize / sizeof(UINT16)) + 1;
    OptionOrder = EfiCoreAllocateBootPool(OrderItemCount * sizeof(UINT16));
    if (OptionOrder == NULL) {
        EfiCoreFreePool(OptionVariable);
        return EFI_OUT_OF_RESOURCES;
    }

    EfiCoreCopyMemory(OptionOrder, OptionVariable, OptionVariableSize);
    OptionOrder[Index] = RegisterOptionNumber;
    Status = EfiSetVariable(VariableName,
                            &EfiGlobalVariableGuid,
                            Attributes,
                            OrderItemCount * sizeof(UINT16),
                            OptionOrder);

    EfiCoreFreePool(OptionVariable);
    EfiCoreFreePool(OptionOrder);
    return Status;
}

EFI_STATUS
EfipBdsGetImageHeader (
    EFI_HANDLE Device,
    CHAR16 *FileName,
    EFI_IMAGE_DOS_HEADER *DosHeader,
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    )

/*++

Routine Description:

    This routine gets the image headers from an image.

Arguments:

    Device - Supplies the simple file system handle.

    FileName - Supplies the path to the file to get the headers for.

    DosHeader - Supplies a pointer where the DOS header will be returned.

    Header - Supplies a pointer (union) where the PE headers will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if the file was not found.

    EFI_LOAD_ERROR if the file is not a valid image.

--*/

{

    UINTN BufferSize;
    UINT64 FileSize;
    EFI_FILE_INFO *Information;
    EFI_FILE_HANDLE Root;
    EFI_STATUS Status;
    EFI_FILE_HANDLE ThisFile;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;

    Root = NULL;
    ThisFile = NULL;
    Status = EfiHandleProtocol(Device,
                               &EfiSimpleFileSystemProtocolGuid,
                               (VOID **)&Volume);

    if (EFI_ERROR(Status)) {
        goto BdsGetImageHeaderEnd;
    }

    Status = Volume->OpenVolume(Volume, &Root);
    if (EFI_ERROR(Status)) {
        Root = NULL;
        goto BdsGetImageHeaderEnd;
    }

    ASSERT(Root != NULL);

    Status = Root->Open(Root, &ThisFile, FileName, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(Status)) {
        goto BdsGetImageHeaderEnd;
    }

    ASSERT(ThisFile != NULL);

    //
    // Get the file information, reallocating the buffer for its needed size.
    //

    BufferSize = SIZE_OF_EFI_FILE_INFO + 200;
    while (TRUE) {
        Information = EfiCoreAllocateBootPool(BufferSize);
        if (Information == NULL) {
            goto BdsGetImageHeaderEnd;
        }

        Status = ThisFile->GetInfo(ThisFile,
                                   &EfiFileInformationGuid,
                                   &BufferSize,
                                   Information);

        if (!EFI_ERROR(Status)) {
            break;
        }

        if (Status != EFI_BUFFER_TOO_SMALL) {
            EfiCoreFreePool(Information);
            goto BdsGetImageHeaderEnd;
        }

        EfiCoreFreePool(Information);
    }

    FileSize = Information->FileSize;
    EfiCoreFreePool(Information);

    //
    // Read the DOS header.
    //

    BufferSize = sizeof(EFI_IMAGE_DOS_HEADER);
    Status = ThisFile->Read(ThisFile, &BufferSize, DosHeader);
    if ((EFI_ERROR(Status)) ||
        (BufferSize < sizeof(EFI_IMAGE_DOS_HEADER)) ||
        (FileSize < DosHeader->e_lfanew) ||
        (DosHeader->e_magic != EFI_IMAGE_DOS_SIGNATURE)) {

        Status = EFI_LOAD_ERROR;
        goto BdsGetImageHeaderEnd;
    }

    //
    // Read the PE header.
    //

    Status = ThisFile->SetPosition(ThisFile, DosHeader->e_lfanew);
    if (EFI_ERROR(Status)) {
        Status = EFI_LOAD_ERROR;
        goto BdsGetImageHeaderEnd;
    }

    BufferSize = sizeof(EFI_IMAGE_OPTIONAL_HEADER_UNION);
    Status = ThisFile->Read(ThisFile, &BufferSize, Header.Pe32);
    if ((EFI_ERROR(Status)) ||
        (BufferSize < sizeof(EFI_IMAGE_OPTIONAL_HEADER_UNION)) ||
        (Header.Pe32->Signature != EFI_IMAGE_NT_SIGNATURE)) {

        Status = EFI_LOAD_ERROR;
        goto BdsGetImageHeaderEnd;
    }

    Status = EFI_SUCCESS;

BdsGetImageHeaderEnd:
    if (ThisFile != NULL) {
        ThisFile->Close(ThisFile);
    }

    if (Root != NULL) {
        Root->Close(Root);
    }

    return Status;
}

EFI_STATUS
EfipBdsConnectDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *Path
    )

/*++

Routine Description:

    This routine creates all handles associated with every device path node.

Arguments:

    Path - Supplies a pointer to the device path to connect.

Return Value:

    EFI Status code.

--*/

{

    EFI_TPL CurrentTpl;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathCopy;
    EFI_HANDLE Handle;
    EFI_DEVICE_PATH_PROTOCOL *Instance;
    EFI_DEVICE_PATH_PROTOCOL *Next;
    EFI_HANDLE PreviousHandle;
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath;
    UINTN Size;
    EFI_STATUS Status;

    if (Path == NULL) {
        return EFI_SUCCESS;
    }

    CurrentTpl = EfiCoreGetCurrentTpl();
    DevicePath = EfiCoreDuplicateDevicePath(Path);
    if (DevicePath == NULL) {
        return EFI_OUT_OF_RESOURCES;
    }

    DevicePathCopy = DevicePath;

    //
    // Loop through every instance in a multi-instance device path. Only
    // console variables contain multi-instance device paths.
    //

    do {
        Instance = EfiCoreGetNextDevicePathInstance(&DevicePath, &Size);
        if (Instance == NULL) {
            EfiCoreFreePool(DevicePathCopy);
            return EFI_OUT_OF_RESOURCES;
        }

        Next = Instance;
        while (EfiCoreIsDevicePathEndType(Next) == FALSE) {
            Next = EfiCoreGetNextDevicePathNode(Next);
        }

        EfiCoreSetDevicePathEndNode(Next);

        //
        // This is the main loop.
        //

        PreviousHandle = NULL;
        do {

            //
            // Find the handle that best matches the device path. This may only
            // be a partial match.
            //

            RemainingDevicePath = Instance;
            Status = EfiLocateDevicePath(&EfiDevicePathProtocolGuid,
                                         &RemainingDevicePath,
                                         &Handle);

            if (!EFI_ERROR(Status)) {
                if (Handle == PreviousHandle) {

                    //
                    // If no forward progress was made try invoking the
                    // dispatcher to load any pending drivers.
                    //

                    if (CurrentTpl == TPL_APPLICATION) {
                        Status = EfiCoreDispatcher();

                    } else {
                        Status = EFI_NOT_FOUND;
                    }
                }

                if (!EFI_ERROR(Status)) {
                    PreviousHandle = Handle;

                    //
                    // Connect all drivers that apply to the handle and
                    // remaining device path. Only go one level deep.
                    //

                    EfiConnectController(Handle,
                                         NULL,
                                         RemainingDevicePath,
                                         FALSE);
                }
            }

        } while ((!EFI_ERROR(Status)) &&
                 (EfiCoreIsDevicePathEnd(RemainingDevicePath) == FALSE));

    } while (DevicePath != NULL);

    if (DevicePathCopy != NULL) {
        EfiCoreFreePool(DevicePathCopy);
    }

    return Status;
}

BOOLEAN
EfipBdsValidateOption (
    UINT8 *Variable,
    UINTN VariableSize
    )

/*++

Routine Description:

    This routine validates the contents of a Boot#### option variable.

Arguments:

    Variable - Supplies a pointer to the variable contents.

    VariableSize - Supplies the size of the variable contents buffer in bytes.

Return Value:

    TRUE if the variable data is correct.

    FALSE if the variable data is not correct.

--*/

{

    UINT8 *CurrentOffset;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINT16 FilePathSize;
    UINTN Size;

    if (VariableSize < sizeof(UINT16) + sizeof(UINT32)) {
        return FALSE;
    }

    //
    // Skip the attributes.
    //

    CurrentOffset = Variable;
    CurrentOffset += sizeof(UINT32);

    //
    // Get the option's device path size.
    //

    FilePathSize = *(UINT16 *)CurrentOffset;
    CurrentOffset += sizeof(UINT16);

    //
    // Get the option's description string size.
    //

    Size = EfipBdsStringSize((CHAR16 *)CurrentOffset,
                             VariableSize - sizeof(UINT16) - sizeof(UINT32));

    CurrentOffset += Size;

    //
    // Get the option's device path.
    //

    DevicePath = (EFI_DEVICE_PATH_PROTOCOL *)CurrentOffset;
    CurrentOffset += FilePathSize;

    //
    // Validate the boot option variable.
    //

    if ((FilePathSize == 0) || (Size == 0)) {
        return FALSE;
    }

    if (Size + FilePathSize + sizeof(UINT16) + sizeof(UINT32) > VariableSize) {
        return FALSE;
    }

    if (EfipBdsGetDevicePathSize(DevicePath, FilePathSize) == 0) {
        return FALSE;
    }

    return TRUE;
}

VOID
EfipBdsCreateHexCodeString (
    CHAR16 *String,
    UINT16 HexInteger,
    CHAR16 *Destination,
    UINTN DestinationSize
    )

/*++

Routine Description:

    This routine appends a four-digit hex code to a string. For example,
    Boot####.

Arguments:

    String - Supplies a pointer to the string to prepend to the destination
        string.

    HexInteger - Supplies the four digit hex integer to append.

    Destination - Supplies a pointer to the destination string buffer.

    DestinationSize - Supplies the size of the destination string in bytes.

Return Value:

    None.

--*/

{

    CHAR16 Digit;
    UINTN DigitIndex;

    //
    // Convert the destination size to be in characters instead of bytes.
    //

    DestinationSize /= sizeof(UINT16);

    //
    // Prepend the given string first.
    //

    if (String != NULL) {
        while ((*String != L'\0') && (DestinationSize > 1)) {
            *Destination = *String;
            Destination += 1;
            DestinationSize -= 1;
            String += 1;
        }
    }

    for (DigitIndex = 0; DigitIndex < 4; DigitIndex += 1) {
        Digit = (HexInteger >> ((3 - DigitIndex) * 4)) & 0x000F;
        if (Digit > 9) {
            Digit = Digit - 0xA + L'A';

        } else {
            Digit += L'0';
        }

        if (DestinationSize > 1) {
            *Destination = Digit;
            Destination += 1;
            DestinationSize -= 1;
        }
    }

    if (DestinationSize > 0) {
        *Destination = L'\0';
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBdsConnectAllEfi (
    VOID
    )

/*++

Routine Description:

    This routine connects all current system handles recursively.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    EFI_STATUS Status;

    HandleBuffer = NULL;
    Status = EfiLocateHandleBuffer(AllHandles,
                                   NULL,
                                   NULL,
                                   &HandleCount,
                                   &HandleBuffer);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index += 1) {
        EfiConnectController(HandleBuffer[Index], NULL, NULL, TRUE);
    }

    if (HandleBuffer != NULL) {
        EfiCoreFreePool(HandleBuffer);
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipBdsDisconnectAllEfi (
    VOID
    )

/*++

Routine Description:

    This routine disconnects all current system handles.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    EFI_HANDLE *HandleBuffer;
    UINTN HandleCount;
    UINTN Index;
    EFI_STATUS Status;

    Status = EfiLocateHandleBuffer(AllHandles,
                                   NULL,
                                   NULL,
                                   &HandleCount,
                                   &HandleBuffer);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    for (Index = 0; Index < HandleCount; Index += 1) {
        EfiDisconnectController(HandleBuffer[Index], NULL, NULL);
    }

    if (HandleBuffer != NULL) {
        EfiCoreFreePool(HandleBuffer);
    }

    return EFI_SUCCESS;
}

UINT16
EfipBdsGetHexCodeFromString (
    CHAR16 *HexCodeString
    )

/*++

Routine Description:

    This routine converts a four digit hex code string to its numerical value.

Arguments:

    HexCodeString - Supplies a pointer to the numerical portion of the string.

Return Value:

    Returns the hex integer representation of the string value.

--*/

{

    UINTN Index;
    UINT16 Value;

    Value = 0;
    for (Index = 0; Index < 4; Index += 1) {
        Value = Value << 4;
        if (*HexCodeString == L'\0') {
            break;
        }

        if ((*HexCodeString >= L'0') && (*HexCodeString <= '9')) {
            Value += *HexCodeString - L'0';

        } else if ((*HexCodeString >= L'A') && (*HexCodeString <= L'F')) {
            Value += *HexCodeString - L'A' + 0xA;

        } else if ((*HexCodeString >= L'a') && (*HexCodeString <= L'f')) {
            Value += *HexCodeString - L'a' + 0xA;
        }
    }

    return Value;
}

UINTN
EfipBdsStringSize (
    CONST CHAR16 *String,
    UINTN MaxStringSize
    )

/*++

Routine Description:

    This routine returns the length of a null-terminated unicode string.

Arguments:

    String - Supplies a pointer to the string to get the length of.

    MaxStringSize - Supplies the size of the buffer containing the string in
        bytes.

Return Value:

    0 if the string is invalid (it is not terminated before the string size).

    Returns the size of the string in bytes.

--*/

{

    UINTN Length;

    ASSERT((String != NULL) && (MaxStringSize != 0));
    ASSERT(((UINTN)String & 0x1) == 0);

    Length = 0;
    while ((*String != L'\0') && (Length < MaxStringSize)) {
        String += 1;
        Length += sizeof(CHAR16);
    }

    if ((*String != L'\0') && (Length >= MaxStringSize)) {
        return 0;
    }

    return Length + sizeof(CHAR16);
}

UINTN
EfipBdsGetDevicePathSize (
    CONST EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINTN MaxSize
    )

/*++

Routine Description:

    This routine returns size of the given device path including the end node,
    limited by the given size.

Arguments:

    DevicePath - Supplies a pointer to the device path.

    MaxSize - Supplies the size of the buffer containing the device path.

Return Value:

    0 if the path is invalid (it is not terminated before the max size).

    Returns the size of the device path in bytes.

--*/

{

    UINTN NodeSize;
    UINTN Size;

    if (DevicePath == NULL) {
        return 0;
    }

    Size = 0;
    while (EfiCoreIsDevicePathEnd(DevicePath) == FALSE) {
        NodeSize = EfiCoreGetDevicePathNodeLength(DevicePath);
        if (NodeSize < END_DEVICE_PATH_LENGTH) {
            return 0;
        }

        Size += NodeSize;
        if (Size > MaxSize) {
            return 0;
        }

        DevicePath = EfiCoreGetNextDevicePathNode(DevicePath);
    }

    Size += EfiCoreGetDevicePathNodeLength(DevicePath);
    if (Size > MaxSize) {
        return 0;
    }

    return Size;
}

UINT16
EfipBdsGetFreeOptionNumber (
    CHAR16 *VariableName
    )

/*++

Routine Description:

    This routine attempts to find an unused Boot#### or Driver#### variable.

Arguments:

    VariableName - Supplies a pointer to the string containing the variable
        name.

Return Value:

    Returns the first available option number.

--*/

{

    UINTN Index;
    UINT16 *OptionBuffer;
    UINTN OptionSize;
    CHAR16 String[10];

    for (Index = 0; Index < 0xFFFF; Index += 1) {
        if (*VariableName == L'B') {
            EfipBdsCreateHexCodeString(L"Boot", Index, String, sizeof(String));

        } else {
            EfipBdsCreateHexCodeString(L"Driver",
                                       Index,
                                       String,
                                       sizeof(String));
        }

        OptionBuffer = EfipBdsGetVariable(String,
                                          &EfiGlobalVariableGuid,
                                          &OptionSize);

        if (OptionBuffer == NULL) {
            break;
        }

        EfiCoreFreePool(OptionBuffer);
    }

    ASSERT(Index <= 0xFFFF);

    return (UINT16)Index;
}

