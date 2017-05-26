/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bdsboot.c

Abstract:

    This module implements boot support for the BDS module.

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
#include "efiimg.h"
#include "fv2.h"
#include <minoca/uefi/protocol/blockio.h>
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

VOID
EfipBdsBuildOptionFromHandle (
    EFI_HANDLE Handle,
    CHAR16 *String
    );

VOID
EfipBdsBuildOptionFromShell (
    EFI_HANDLE Handle
    );

UINT32
EfipBdsGetBootTypeFromDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

EFI_STATUS
EfipBdsDeleteOptionFromHandle (
    EFI_HANDLE Handle
    );

EFI_STATUS
EfipBdsDeleteBootOption (
    UINTN OptionNumber,
    UINT16 *BootOrder,
    UINTN *BootOrderSize
    );

EFI_DEVICE_PATH_PROTOCOL *
EfipBdsExpandPartitionDevicePath (
    HARDDRIVE_DEVICE_PATH *HardDriveDevicePath
    );

BOOLEAN
EfipBdsMatchPartitionDevicePathNode (
    EFI_DEVICE_PATH_PROTOCOL *BlockIoDevicePath,
    HARDDRIVE_DEVICE_PATH *HardDriveDevicePath
    );

BOOLEAN
EfipBdsIsBootOptionValidVariable (
    PEFI_BDS_COMMON_OPTION Option
    );

VOID
EfipBdsSignalReadyToBootEvent (
    VOID
    );

EFI_HANDLE
EfipBdsGetBootableHandle (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the file path to the flash shell to be tested for existence. This
// can be overridden by platform-specific code.
//

EFI_GUID EfiDefaultShellFileGuid = EFI_DEFAULT_SHELL_FILE_GUID;

BOOLEAN EfiBootDevicesEnumerated;

EFI_GUID EfiBlockIoProtocolGuid = EFI_BLOCK_IO_PROTOCOL_GUID;
EFI_GUID EfiHdBootDevicePathVariableGuid =
                                         EFI_HD_BOOT_DEVICE_PATH_VARIABLE_GUID;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBdsBootViaBootOption (
    PEFI_BDS_COMMON_OPTION Option,
    EFI_DEVICE_PATH_PROTOCOL *DevicePath,
    UINTN *ExitDataSize,
    CHAR16 **ExitData
    )

/*++

Routine Description:

    This routine attempts to boot the given boot option.

Arguments:

    Option - Supplies a pointer to the option to boot.

    DevicePath - Supplies a pointer to the device path describing where to
        load the boot image or legacy BBS device path.

    ExitDataSize - Supplies a pointer where the exit data size will be returned.

    ExitData - Supplies a pointer where a pointer to the exit data will be
        returned.

Return Value:

    EFI status code.

--*/

{

    UINT32 Attributes;
    EFI_DEVICE_PATH_PROTOCOL *FilePath;
    EFI_HANDLE Handle;
    EFI_HANDLE ImageHandle;
    EFI_LOADED_IMAGE_PROTOCOL *ImageInformation;
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *WorkingDevicePath;

    *ExitDataSize = 0;
    *ExitData = NULL;
    ImageHandle = NULL;
    Status = EFI_SUCCESS;
    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS;

    //
    // If the device path starts with a hard drive path, append it to the
    // front part to create a full device path.
    //

    WorkingDevicePath = NULL;
    if ((EfiCoreGetDevicePathType(DevicePath) == MEDIA_DEVICE_PATH) &&
        (EfiCoreGetDevicePathSubType(DevicePath) == MEDIA_HARDDRIVE_DP)) {

        WorkingDevicePath = EfipBdsExpandPartitionDevicePath(
                                          (HARDDRIVE_DEVICE_PATH *)DevicePath);

        if (WorkingDevicePath != NULL) {
            DevicePath = WorkingDevicePath;
        }
    }

    //
    // Set boot current.
    //

    if (EfipBdsIsBootOptionValidVariable(Option) != FALSE) {
        EfiSetVariable(L"BootCurrent",
                       &EfiGlobalVariableGuid,
                       Attributes,
                       sizeof(UINT16),
                       &(Option->BootCurrent));
    }

    //
    // Signal the EVT_SIGNAL_READY_TO_BOOT event.
    //

    EfipBdsSignalReadyToBootEvent();
    EfiCoreSaveVariablesToFileSystem();

    //
    // Get the image handle for non-USB class or WWID device paths.
    //

    if (ImageHandle == NULL) {

        ASSERT(Option->DevicePath != NULL);

        //
        // Legacy BBS options are not supported in this implementation.
        //

        if ((EfiCoreGetDevicePathType(Option->DevicePath) == BBS_DEVICE_PATH) &&
            (EfiCoreGetDevicePathSubType(Option->DevicePath) == BBS_BBS_DP)) {

            return EFI_UNSUPPORTED;
        }

        Status = EfiLoadImage(TRUE,
                              EfiFirmwareImageHandle,
                              DevicePath,
                              NULL,
                              0,
                              &ImageHandle);

        //
        // If an image wasn't found directly, try as if it is a removable
        // device boot option and load the image according to the default
        // behavior for a removable device.
        //

        if (EFI_ERROR(Status)) {
            Handle = EfipBdsGetBootableHandle(DevicePath);
            if (Handle != NULL) {
                FilePath = EfiCoreCreateFileDevicePath(
                                                Handle,
                                                EFI_REMOVABLE_MEDIA_FILE_NAME);

                if (FilePath != NULL) {
                    Status = EfiLoadImage(TRUE,
                                          EfiFirmwareImageHandle,
                                          FilePath,
                                          NULL,
                                          0,
                                          &ImageHandle);
                }
            }
        }
    }

    //
    // Provide the image with its load options.
    //

    if ((ImageHandle == NULL) || (EFI_ERROR(Status))) {
        goto BdsBootViaBootOptionEnd;
    }

    Status = EfiHandleProtocol(ImageHandle,
                               &EfiLoadedImageProtocolGuid,
                               (VOID **)&ImageInformation);

    ASSERT(!EFI_ERROR(Status));

    if (Option->LoadOptionsSize != 0) {
        ImageInformation->LoadOptionsSize = Option->LoadOptionsSize;
        ImageInformation->LoadOptions = Option->LoadOptions;
    }

    //
    // NULL out the parent handle since this image is loaded directly by the
    // firmware boot manager.
    //

    ImageInformation->ParentHandle = NULL;

    //
    // Set the watchdog timer before launching the boot option.
    //

    EfiSetWatchdogTimer(EFI_DEFAULT_WATCHDOG_DURATION, 0, 0, NULL);
    Status = EfiStartImage(ImageHandle, ExitDataSize, ExitData);
    RtlDebugPrint("EFI Image Returned: 0x%x\r\n", Status);

    //
    // Disable the watchdog timer.
    //

    EfiSetWatchdogTimer(0, 0, 0, NULL);

BdsBootViaBootOptionEnd:

    //
    // Clear the boot current variable.
    //

    EfiSetVariable(L"BootCurrent",
                   &EfiGlobalVariableGuid,
                   Attributes,
                   0,
                   &(Option->BootCurrent));

    return Status;
}

EFI_STATUS
EfipBdsEnumerateAllBootOptions (
    PLIST_ENTRY OptionList
    )

/*++

Routine Description:

    This routine enumerates all possible boot devices in the system, and
    creates boot options for them. There are six types of automatic boot
    options:

    1. Network - Creates boot options on any load file protocol instances.

    2. Shell - Creates boot options for any firmware volumes that contain a
       specific path on them.

    3. Removable Block I/O - Creates a boot option for any removable block I/O
       device.

    4. Fixed Block I/O - Does not create a boot option for fixed drives.

    5. Non-Block I/O Simple File Systems - Creates a boot option for
       \EFI\BOOT\boot{machine}.EFI using the Simple File System Protocol.

    6. File - Does not create, modify, or delete a boot option pointing at a
       file.

Arguments:

    OptionList - Supplies a pointer to the head of the boot option list.

Return Value:

    EFI status code.

--*/

{

    EFI_FV_FILE_ATTRIBUTES Attributes;
    UINT32 AuthenticationStatus;
    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    UINTN BlockIoHandleCount;
    EFI_HANDLE *BlockIoHandles;
    CHAR16 Buffer[40];
    UINT16 CdromNumber;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN DevicePathType;
    EFI_IMAGE_DOS_HEADER DosHeader;
    UINTN FileSystemHandleCount;
    EFI_HANDLE *FileSystemHandles;
    EFI_FIRMWARE_VOLUME2_PROTOCOL *FirmwareVolume;
    UINTN FirmwareVolumeHandleCount;
    EFI_HANDLE *FirmwareVolumeHandles;
    UINT16 FloppyNumber;
    UINT16 HarddriveNumber;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header;
    EFI_IMAGE_OPTIONAL_HEADER_UNION HeaderData;
    UINTN Index;
    UINTN LoadFileHandleCount;
    EFI_HANDLE *LoadFileHandles;
    UINT16 MiscNumber;
    BOOLEAN NeedDelete;
    UINT16 NonBlockNumber;
    UINTN RemovableIndex;
    BOOLEAN RemovableSkip;
    UINT16 ScsiNumber;
    UINTN Size;
    EFI_STATUS Status;
    EFI_FV_FILETYPE Type;
    UINT16 UsbNumber;

    FloppyNumber = 0;
    HarddriveNumber = 0;
    CdromNumber = 0;
    UsbNumber = 0;
    MiscNumber = 0;
    ScsiNumber = 0;

    //
    // If the boot device enumeration happened, just get the boot device from
    // the boot order variable.
    //

    if (EfiBootDevicesEnumerated != FALSE) {
        Status = EfipBdsBuildOptionFromVariable(OptionList, L"BootOrder");
        return Status;
    }

    EfiLocateHandleBuffer(ByProtocol,
                          &EfiBlockIoProtocolGuid,
                          NULL,
                          &BlockIoHandleCount,
                          &BlockIoHandles);

    //
    // Loop twice, once for removable media and once for non-removable media.
    //

    for (RemovableIndex = 0; RemovableIndex < 2; RemovableIndex += 1) {
        RemovableSkip = FALSE;
        if (RemovableIndex != 0) {
            RemovableSkip = TRUE;
        }

        //
        // Loop through every block I/O handle.
        //

        for (Index = 0; Index < BlockIoHandleCount; Index += 1) {
            Status = EfiHandleProtocol(BlockIoHandles[Index],
                                       &EfiBlockIoProtocolGuid,
                                       (VOID **)&BlockIo);

            //
            // Skip invalid or inapplicable handles.
            //

            if ((EFI_ERROR(Status)) ||
                (BlockIo->Media->RemovableMedia == RemovableSkip)) {

                continue;
            }

            DevicePath = EfiCoreGetDevicePathFromHandle(BlockIoHandles[Index]);
            DevicePathType = EfipBdsGetBootTypeFromDevicePath(DevicePath);
            switch (DevicePathType) {
            case BDS_EFI_ACPI_FLOPPY_BOOT:
                if (FloppyNumber == 0) {
                    EfiCoreCopyString(Buffer, L"Floppy");

                } else {
                    EfipBdsCreateHexCodeString(L"Floppy",
                                               FloppyNumber,
                                               Buffer,
                                               sizeof(Buffer));
                }

                EfipBdsBuildOptionFromHandle(BlockIoHandles[Index], Buffer);
                FloppyNumber += 1;
                break;

            case BDS_EFI_MESSAGE_ATAPI_BOOT:
            case BDS_EFI_MESSAGE_SATA_BOOT:
            case BDS_EFI_MEDIA_HD_BOOT:
            case BDS_EFI_MEDIA_CDROM_BOOT:
                if (BlockIo->Media->RemovableMedia != FALSE) {
                    if (CdromNumber == 0) {
                        EfiCoreCopyString(Buffer, L"CD/DVD");

                    } else {
                        EfipBdsCreateHexCodeString(L"CD/DVD",
                                                   CdromNumber,
                                                   Buffer,
                                                   sizeof(Buffer));
                    }

                    CdromNumber += 1;

                } else {
                    if (HarddriveNumber == 0) {
                        EfiCoreCopyString(Buffer, L"HardDrive");

                    } else {
                        EfipBdsCreateHexCodeString(L"HardDrive",
                                                   HarddriveNumber,
                                                   Buffer,
                                                   sizeof(Buffer));
                    }

                    HarddriveNumber += 1;
                }

                EfipBdsBuildOptionFromHandle(BlockIoHandles[Index], Buffer);
                break;

            case BDS_EFI_MESSAGE_USB_DEVICE_BOOT:
                if (UsbNumber == 0) {
                    EfiCoreCopyString(Buffer, L"USB");

                } else {
                    EfipBdsCreateHexCodeString(L"USB",
                                               UsbNumber,
                                               Buffer,
                                               sizeof(Buffer));
                }

                EfipBdsBuildOptionFromHandle(BlockIoHandles[Index], Buffer);
                UsbNumber += 1;
                break;

            case BDS_EFI_MESSAGE_SCSI_BOOT:
                if (ScsiNumber == 0) {
                    EfiCoreCopyString(Buffer, L"SCSI");

                } else {
                    EfipBdsCreateHexCodeString(L"SCSI",
                                               ScsiNumber,
                                               Buffer,
                                               sizeof(Buffer));
                }

                EfipBdsBuildOptionFromHandle(BlockIoHandles[Index], Buffer);
                ScsiNumber += 1;
                break;

            case BDS_EFI_MESSAGE_MISC_BOOT:
                if (MiscNumber == 0) {
                    EfiCoreCopyString(Buffer, L"Misc");

                } else {
                    EfipBdsCreateHexCodeString(L"Misc",
                                               MiscNumber,
                                               Buffer,
                                               sizeof(Buffer));
                }

                EfipBdsBuildOptionFromHandle(BlockIoHandles[Index], Buffer);
                MiscNumber += 1;
                break;

            default:
                break;
            }
        }
    }

    if (BlockIoHandleCount != 0) {
        EfiCoreFreePool(BlockIoHandles);
    }

    //
    // Look for simple file system protocols which do not consume block I/O
    // protocols, and create boot options for each of those.
    //

    NonBlockNumber = 0;
    EfiLocateHandleBuffer(ByProtocol,
                          &EfiSimpleFileSystemProtocolGuid,
                          NULL,
                          &FileSystemHandleCount,
                          &FileSystemHandles);

    for (Index = 0; Index < FileSystemHandleCount; Index += 1) {

        //
        // Skip it if there's a block I/O protocol on here as well.
        //

        Status = EfiHandleProtocol(FileSystemHandles[Index],
                                   &EfiBlockIoProtocolGuid,
                                   (VOID **)&BlockIo);

        if (!EFI_ERROR(Status)) {
            continue;
        }

        //
        // Do that removable media thang: \EFI\BOOT\boot{arch}.EFI.
        //

        Header.Union = &HeaderData;
        NeedDelete = TRUE;
        Status = EfipBdsGetImageHeader(FileSystemHandles[Index],
                                       EFI_REMOVABLE_MEDIA_FILE_NAME,
                                       &DosHeader,
                                       Header);

        if ((!EFI_ERROR(Status)) &&
            (EFI_IMAGE_MACHINE_TYPE_SUPPORTED(
                                           Header.Pe32->FileHeader.Machine)) &&
            (Header.Pe32->OptionalHeader.Subsystem ==
             EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION)) {

            NeedDelete = FALSE;
        }

        //
        // If there was no valid image there, delete the boot option.
        //

        if (NeedDelete != FALSE) {
            EfipBdsDeleteOptionFromHandle(FileSystemHandles[Index]);

        //
        // Create a boot option.
        //

        } else {
            if (NonBlockNumber == 0) {
                EfiCoreCopyString(Buffer, L"NonBlock");

            } else {
                EfipBdsCreateHexCodeString(L"NonBlock",
                                           NonBlockNumber,
                                           Buffer,
                                           sizeof(Buffer));
            }

            EfipBdsBuildOptionFromHandle(FileSystemHandles[Index], Buffer);
            NonBlockNumber += 1;
        }
    }

    if (FileSystemHandleCount != 0) {
        EfiCoreFreePool(FileSystemHandles);
    }

    //
    // Add network/load file entries.
    //

    LoadFileHandleCount = 0;
    EfiLocateHandleBuffer(ByProtocol,
                          &EfiLoadFileProtocolGuid,
                          NULL,
                          &LoadFileHandleCount,
                          &LoadFileHandles);

    for (Index = 0; Index < LoadFileHandleCount; Index += 1) {
        if (Index == 0) {
            EfiCoreCopyString(Buffer, L"Net");

        } else {
            EfipBdsCreateHexCodeString(L"Net",
                                       Index,
                                       Buffer,
                                       sizeof(Buffer));
        }

        EfipBdsBuildOptionFromHandle(LoadFileHandles[Index], Buffer);
    }

    if (LoadFileHandleCount != 0) {
        EfiCoreFreePool(LoadFileHandles);
    }

    //
    // Add the flash shell if there is one.
    //

    FirmwareVolumeHandleCount = 0;
    EfiLocateHandleBuffer(ByProtocol,
                          &EfiLoadFileProtocolGuid,
                          NULL,
                          &FirmwareVolumeHandleCount,
                          &FirmwareVolumeHandles);

    for (Index = 0; Index < FirmwareVolumeHandleCount; Index += 1) {
        Status = EfiHandleProtocol(FirmwareVolumeHandles[Index],
                                   &EfiFirmwareVolume2ProtocolGuid,
                                   (VOID **)&FirmwareVolume);

        if ((EFI_ERROR(Status)) || (FirmwareVolume == NULL)) {
            continue;
        }

        Status = FirmwareVolume->ReadFile(FirmwareVolume,
                                          &EfiDefaultShellFileGuid,
                                          NULL,
                                          &Size,
                                          &Type,
                                          &Attributes,
                                          &AuthenticationStatus);

        if (EFI_ERROR(Status)) {
            continue;
        }

        EfipBdsBuildOptionFromShell(FirmwareVolumeHandles[Index]);
    }

    if (FirmwareVolumeHandleCount != 0) {
        EfiCoreFreePool(FirmwareVolumeHandles);
    }

    Status = EfipBdsBuildOptionFromVariable(OptionList, L"BootOrder");
    EfiBootDevicesEnumerated = TRUE;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipBdsBuildOptionFromHandle (
    EFI_HANDLE Handle,
    CHAR16 *String
    )

/*++

Routine Description:

    This routine builds a boot option off the given handle.

Arguments:

    Handle - Supplies the handle to build the boot option around.

    OptionList - Supplies a pointer to the boot option list. This option will
        be added.

    String - Supplies a pointer to a string describing the option. A copy of
        this string will be made.

Return Value:

    None.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *DevicePath;

    DevicePath = EfiCoreGetDevicePathFromHandle(Handle);
    EfipBdsRegisterNewOption(DevicePath, String, L"BootOrder");
    return;
}

VOID
EfipBdsBuildOptionFromShell (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine builds a boot option off the given handle for the internal
    flash shell.

Arguments:

    Handle - Supplies the handle to build the boot option around.

    OptionList - Supplies a pointer to the boot option list. This option will
        be added.

Return Value:

    None.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    MEDIA_FW_VOL_FILEPATH_DEVICE_PATH ShellNode;

    DevicePath = EfiCoreGetDevicePathFromHandle(Handle);
    EfiCoreInitializeFirmwareVolumeDevicePathNode(&ShellNode,
                                                  &EfiDefaultShellFileGuid);

    DevicePath = EfiCoreAppendDevicePathNode(
                                       DevicePath,
                                       (EFI_DEVICE_PATH_PROTOCOL *)&ShellNode);

    EfipBdsRegisterNewOption(DevicePath, L"EFI Shell", L"BootOrder");
    return;
}
UINT32
EfipBdsGetBootTypeFromDevicePath (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    )

/*++

Routine Description:

    This routine returns a boot type associated with a given device path.

Arguments:

    DevicePath - Supplies a pointer to the device path.

Return Value:

    Returns a BDS_EFI_* definition.

--*/

{

    ACPI_HID_DEVICE_PATH *Acpi;
    UINT32 BootType;
    EFI_DEVICE_PATH_PROTOCOL *CurrentPath;
    EFI_DEVICE_PATH_PROTOCOL *LastDeviceNode;
    UINT8 SubType;

    if (DevicePath == NULL) {
        return BDS_EFI_UNSUPPORTED;
    }

    CurrentPath = DevicePath;
    while (EfiCoreIsDevicePathEndType(CurrentPath) == FALSE) {
        switch (EfiCoreGetDevicePathType(CurrentPath)) {
        case BBS_DEVICE_PATH:
            return BDS_LEGACY_BBS_BOOT;

        case MEDIA_DEVICE_PATH:
            SubType = EfiCoreGetDevicePathSubType(CurrentPath);
            if (SubType == MEDIA_HARDDRIVE_DP) {
                return BDS_EFI_MEDIA_HD_BOOT;

            } else if (SubType == MEDIA_CDROM_DP) {
                return BDS_EFI_MEDIA_CDROM_BOOT;
            }

            break;

        case ACPI_DEVICE_PATH:
            Acpi = (ACPI_HID_DEVICE_PATH *)CurrentPath;
            if (EISA_ID_TO_NUM(Acpi->HID) == 0x0604) {
                return BDS_EFI_ACPI_FLOPPY_BOOT;
            }

            break;

        case MESSAGING_DEVICE_PATH:
            LastDeviceNode = EfiCoreGetNextDevicePathNode(CurrentPath);

            //
            // If the next node type is Device Logical Unit (which specifies
            // the Logical Unit Number (LUN)), skip it.
            //

            if (EfiCoreGetDevicePathSubType(LastDeviceNode) ==
                MSG_DEVICE_LOGICAL_UNIT_DP) {

                LastDeviceNode = EfiCoreGetNextDevicePathNode(LastDeviceNode);
            }

            //
            // The next one should really be the last. Ignore it if it's not.
            //

            if (EfiCoreIsDevicePathEndType(LastDeviceNode) == FALSE) {
                break;
            }

            SubType = EfiCoreGetDevicePathSubType(CurrentPath);
            switch (SubType) {
            case MSG_ATAPI_DP:
                BootType = BDS_EFI_MESSAGE_ATAPI_BOOT;
                break;

            case MSG_USB_DP:
                BootType = BDS_EFI_MESSAGE_USB_DEVICE_BOOT;
                break;

            case MSG_SCSI_DP:
                BootType = BDS_EFI_MESSAGE_SCSI_BOOT;
                break;

            case MSG_SATA_DP:
                BootType = BDS_EFI_MESSAGE_SATA_BOOT;
                break;

            case MSG_MAC_ADDR_DP:
            case MSG_VLAN_DP:
            case MSG_IPv4_DP:
            case MSG_IPv6_DP:
                BootType = BDS_EFI_MESSAGE_MAC_BOOT;
                break;

            default:
                BootType = BDS_EFI_MESSAGE_MISC_BOOT;
                break;
            }

            return BootType;

        default:
            break;
        }

        CurrentPath = EfiCoreGetNextDevicePathNode(CurrentPath);
    }

    return BDS_EFI_UNSUPPORTED;
}

EFI_STATUS
EfipBdsDeleteOptionFromHandle (
    EFI_HANDLE Handle
    )

/*++

Routine Description:

    This routine deletes a boot option associated with the given handle.

Arguments:

    Handle - Supplies the handle associated with the boot option to delete.

Return Value:

    EFI status code.

--*/

{

    UINT32 Attributes;
    UINT16 BootOption[EFI_BOOT_OPTION_MAX_CHAR];
    UINTN BootOptionSize;
    UINT8 *BootOptionVariable;
    UINT16 *BootOrder;
    UINTN BootOrderSize;
    INTN CompareResult;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    UINTN DevicePathSize;
    UINTN Index;
    EFI_DEVICE_PATH_PROTOCOL *OptionDevicePath;
    UINTN OptionDevicePathSize;
    UINT8 *OptionMember;
    EFI_STATUS Status;

    Status = EFI_SUCCESS;
    BootOrder = NULL;
    BootOrderSize = 0;
    BootOrder = EfipBdsGetVariable(L"BootOrder",
                                   &EfiGlobalVariableGuid,
                                   &BootOrderSize);

    if (BootOrder == NULL) {
        return EFI_NOT_FOUND;
    }

    DevicePath = EfiCoreGetDevicePathFromHandle(Handle);
    if (DevicePath == NULL) {
        return EFI_NOT_FOUND;
    }

    DevicePathSize = EfiCoreGetDevicePathSize(DevicePath);

    //
    // Loop over all the boot order variables to find the matching device path.
    //

    Index = 0;
    while (Index < BootOrderSize / sizeof(UINT16)) {
        EfipBdsCreateHexCodeString(L"Boot",
                                   BootOrder[Index],
                                   BootOption,
                                   sizeof(BootOption));

        BootOptionVariable = EfipBdsGetVariable(BootOption,
                                                &EfiGlobalVariableGuid,
                                                &BootOptionSize);

        if (BootOptionVariable == NULL) {
            EfiCoreFreePool(BootOrder);
            return EFI_OUT_OF_RESOURCES;
        }

        if (EfipBdsValidateOption(BootOptionVariable, BootOptionSize) ==
            FALSE) {

            EfipBdsDeleteBootOption(BootOrder[Index],
                                    BootOrder,
                                    &BootOrderSize);

            EfiCoreFreePool(BootOptionVariable);
            Index += 1;
            continue;
        }

        OptionMember = BootOptionVariable;
        OptionMember += sizeof(UINT32) + sizeof(UINT16);
        OptionMember += (EfiCoreStringLength((CHAR16 *)OptionMember) + 1) *
                        sizeof(CHAR16);

        OptionDevicePath = (EFI_DEVICE_PATH_PROTOCOL *)OptionMember;
        OptionDevicePathSize = EfiCoreGetDevicePathSize(OptionDevicePath);

        //
        // Match against the device path.
        //

        if (OptionDevicePathSize == DevicePathSize) {
            CompareResult = EfiCoreCompareMemory(DevicePath,
                                                 OptionDevicePath,
                                                 DevicePathSize);

            if (CompareResult == 0) {
                EfipBdsDeleteBootOption(BootOrder[Index],
                                        BootOrder,
                                        &BootOrderSize);

                EfiCoreFreePool(BootOptionVariable);
                break;
            }
        }

        EfiCoreFreePool(BootOptionVariable);
        Index += 1;
    }

    //
    // Adjust the number of options for the BootOrder variable.
    //

    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS |
                 EFI_VARIABLE_NON_VOLATILE;

    Status = EfiSetVariable(L"BootOrder",
                            &EfiGlobalVariableGuid,
                            Attributes,
                            BootOrderSize,
                            BootOrder);

    EfiCoreFreePool(BootOrder);
    return Status;
}

EFI_STATUS
EfipBdsDeleteBootOption (
    UINTN OptionNumber,
    UINT16 *BootOrder,
    UINTN *BootOrderSize
    )

/*++

Routine Description:

    This routine deletes the boot option from EFI boot variables. The boot
    order array is also updated.

Arguments:

    OptionNumber - Supplies the option number to delete.

    BootOrder - Supplies a pointer to the boot order array.

    BootOrderSize - Supplies a pointer that on input contains the size of the
        boot order array. On output this will be updated if an entry was
        deleted.

Return Value:

    EFI status code.

--*/

{

    CHAR16 BootOption[EFI_BOOT_OPTION_MAX_CHAR];
    UINTN Index;
    EFI_STATUS Status;

    EfipBdsCreateHexCodeString(L"Boot",
                               OptionNumber,
                               BootOption,
                               sizeof(BootOption));

    Status = EfiSetVariable(BootOption,
                            &EfiGlobalVariableGuid,
                            0,
                            0,
                            NULL);

    //
    // Adjust the boot order array.
    //

    for (Index = 0; Index < *BootOrderSize / sizeof(UINT16); Index += 1) {
        if (BootOrder[Index] == OptionNumber) {
            EfiCoreCopyMemory(&(BootOrder[Index]),
                              &(BootOrder[Index + 1]),
                              *BootOrderSize - ((Index + 1) * sizeof(UINT16)));

            *BootOrderSize -= sizeof(UINT16);
            break;
        }
    }

    return Status;
}

EFI_DEVICE_PATH_PROTOCOL *
EfipBdsExpandPartitionDevicePath (
    HARDDRIVE_DEVICE_PATH *HardDriveDevicePath
    )

/*++

Routine Description:

    This routine expands a device path that starts with a hard drive media
    device path node to be a full device path that includes the full hardware
    path to the device. As an optimization the front match (the part pointing
    to the partition node. E.g. ACPI() /PCI()/ATA()/Partition()) is saved in a
    variable so a connect all is not required on every boot. All successful
    device paths which point to partition nodes (the front part) will be saved.

Arguments:

    HardDriveDevicePath - Supplies a pointer to the hard drive device path.

Return Value:

    Returns a pointer to the fully expanded device path.

--*/

{

    UINT32 Attributes;
    EFI_HANDLE *BlockIoBuffer;
    EFI_DEVICE_PATH_PROTOCOL *BlockIoDevicePath;
    UINTN BlockIoHandleCount;
    EFI_DEVICE_PATH_PROTOCOL *CachedDevicePath;
    UINTN CachedDevicePathSize;
    BOOLEAN DeviceExists;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    EFI_DEVICE_PATH_PROTOCOL *FullDevicePath;
    UINTN Index;
    EFI_DEVICE_PATH_PROTOCOL *Instance;
    UINTN InstanceCount;
    BOOLEAN Match;
    BOOLEAN NeedsAdjustment;
    EFI_DEVICE_PATH_PROTOCOL *OldDevicePath;
    UINTN Size;
    EFI_STATUS Status;

    FullDevicePath = NULL;
    Attributes = EFI_VARIABLE_BOOTSERVICE_ACCESS |
                 EFI_VARIABLE_RUNTIME_ACCESS |
                 EFI_VARIABLE_NON_VOLATILE;

    //
    // Check to see if there is a cached variable of the translation.
    //

    CachedDevicePath = EfipBdsGetVariable(EFI_HD_BOOT_DEVICE_PATH_VARIABLE_NAME,
                                          &EfiHdBootDevicePathVariableGuid,
                                          &CachedDevicePathSize);

    if (CachedDevicePath != NULL) {
        OldDevicePath = CachedDevicePath;
        DeviceExists = FALSE;
        NeedsAdjustment = FALSE;
        do {

            //
            // Check every instance of the variable. First, check whether the
            // instance contains the partition node, needed for distinguishing
            // partial partition boot options. Second, check whether or not the
            // instance can be connected.
            //

            Instance = EfiCoreGetNextDevicePathInstance(&OldDevicePath, &Size);
            Match = EfipBdsMatchPartitionDevicePathNode(Instance,
                                                        HardDriveDevicePath);

            if (Match != FALSE) {
                Status = EfipBdsConnectDevicePath(Instance);
                if (!EFI_ERROR(Status)) {
                    DeviceExists = TRUE;
                    break;
                }
            }

            NeedsAdjustment = TRUE;
            EfiCoreFreePool(Instance);

        } while (OldDevicePath != NULL);

        //
        // If a matching device was found, append the file path information
        // from the boot option and return the fully expanded path.
        //

        if (DeviceExists != FALSE) {
            DevicePath = EfiCoreGetNextDevicePathNode(
                              (EFI_DEVICE_PATH_PROTOCOL *)HardDriveDevicePath);

            FullDevicePath = EfiCoreAppendDevicePath(Instance, DevicePath);

            //
            // Adjust the cached variable instance sequence if the matched one
            // is not the first one.
            //

            if (NeedsAdjustment != FALSE) {

                //
                // First, delete the matched instance.
                //

                OldDevicePath = CachedDevicePath;
                CachedDevicePath = EfipBdsDeletePartialMatchInstance(
                                                              CachedDevicePath,
                                                              Instance);

                EfiCoreFreePool(OldDevicePath);

                //
                // Next, append the remaining device path after the matched
                // instance.
                //

                OldDevicePath = CachedDevicePath;
                CachedDevicePath = EfiCoreAppendDevicePathInstance(
                                                             Instance,
                                                             CachedDevicePath);

                EfiCoreFreePool(OldDevicePath);

                //
                // Save the variable for future speedups.
                //

                Status = EfiSetVariable(
                                    EFI_HD_BOOT_DEVICE_PATH_VARIABLE_NAME,
                                    &EfiHdBootDevicePathVariableGuid,
                                    Attributes,
                                    EfiCoreGetDevicePathSize(CachedDevicePath),
                                    CachedDevicePath);
            }

            EfiCoreFreePool(Instance);
            EfiCoreFreePool(CachedDevicePath);
            return FullDevicePath;
        }
    }

    //
    // The device was not found in the cached variable, so it's time to search
    // all devices for a matched partition.
    //

    EfipBdsConnectAllDriversToAllControllers();
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiBlockIoProtocolGuid,
                                   NULL,
                                   &BlockIoHandleCount,
                                   &BlockIoBuffer);

    if ((EFI_ERROR(Status)) || (BlockIoHandleCount == 0) ||
        (BlockIoBuffer == NULL)) {

        return NULL;
    }

    //
    // Loop through everything that supports block I/O.
    //

    for (Index = 0; Index < BlockIoHandleCount; Index += 1) {
        Status = EfiHandleProtocol(BlockIoBuffer[Index],
                                   &EfiDevicePathProtocolGuid,
                                   (VOID **)&BlockIoDevicePath);

        if ((EFI_ERROR(Status)) || (BlockIoDevicePath == NULL)) {
            continue;
        }

        Match = EfipBdsMatchPartitionDevicePathNode(BlockIoDevicePath,
                                                    HardDriveDevicePath);

        if (Match != FALSE) {

            //
            // Find the matched partition device path.
            //

            DevicePath = EfiCoreGetNextDevicePathNode(
                              (EFI_DEVICE_PATH_PROTOCOL *)HardDriveDevicePath);

            FullDevicePath = EfiCoreAppendDevicePath(BlockIoDevicePath,
                                                     DevicePath);

            //
            // Save the matched partition device path in the variable.
            //

            if (CachedDevicePath != NULL) {
                Match = EfipBdsMatchDevicePaths(CachedDevicePath,
                                                BlockIoDevicePath);

                if (Match != FALSE) {
                    OldDevicePath = CachedDevicePath;
                    CachedDevicePath = EfipBdsDeletePartialMatchInstance(
                                                            CachedDevicePath,
                                                            BlockIoDevicePath);

                    EfiCoreFreePool(OldDevicePath);
                }

                if (CachedDevicePath != NULL) {
                    OldDevicePath = CachedDevicePath;
                    CachedDevicePath = EfiCoreAppendDevicePathInstance(
                                                             BlockIoDevicePath,
                                                             CachedDevicePath);

                    EfiCoreFreePool(OldDevicePath);

                } else {
                    CachedDevicePath =
                                 EfiCoreDuplicateDevicePath(BlockIoDevicePath);
                }

                //
                // Limit the device path instance number to avoid growing the
                // variable infinitely.
                //

                InstanceCount = 0;

                ASSERT(CachedDevicePath != NULL);

                //
                // Count the instances.
                //

                OldDevicePath = CachedDevicePath;
                while (EfiCoreIsDevicePathEnd(OldDevicePath) == FALSE) {
                    OldDevicePath = EfiCoreGetNextDevicePathNode(OldDevicePath);

                    //
                    // Parse one instance.
                    //

                    while (EfiCoreIsDevicePathEndType(OldDevicePath) == FALSE) {
                        OldDevicePath =
                                   EfiCoreGetNextDevicePathNode(OldDevicePath);
                    }

                    InstanceCount += 1;
                    if (InstanceCount >= EFI_MAX_HD_DEVICE_PATH_CACHE_SIZE) {
                        EfiCoreSetDevicePathEndNode(OldDevicePath);
                        break;
                    }
                }

            } else {
                CachedDevicePath =
                                 EfiCoreDuplicateDevicePath(BlockIoDevicePath);
            }

            //
            // Save the matching device path variable for speedups on future
            // boots.
            //

            Status = EfiSetVariable(EFI_HD_BOOT_DEVICE_PATH_VARIABLE_NAME,
                                    &EfiHdBootDevicePathVariableGuid,
                                    Attributes,
                                    EfiCoreGetDevicePathSize(CachedDevicePath),
                                    CachedDevicePath);

            break;
        }
    }

    if (CachedDevicePath != NULL) {
        EfiCoreFreePool(CachedDevicePath);
    }

    if (BlockIoBuffer != NULL) {
        EfiCoreFreePool(BlockIoBuffer);
    }

    return FullDevicePath;
}

BOOLEAN
EfipBdsMatchPartitionDevicePathNode (
    EFI_DEVICE_PATH_PROTOCOL *BlockIoDevicePath,
    HARDDRIVE_DEVICE_PATH *HardDriveDevicePath
    )

/*++

Routine Description:

    This routine looks for the given hard drive device path node in the
    block I/O device path.

Arguments:

    BlockIoDevicePath - Supplies a pointer to the block I/O device path node
        to search.

    HardDriveDevicePath - Supplies a pointer to the node to search for.

Return Value:

    TRUE if the hard drive node is in the block I/O node.

    FALSE if the hard drive node is not contained within the block I/O node.

--*/

{

    EFI_DEVICE_PATH_PROTOCOL *BlockIoDriveNode;
    EFI_DEVICE_PATH_PROTOCOL *DevicePath;
    HARDDRIVE_DEVICE_PATH *DriveNode;
    INTN Match;

    if ((BlockIoDevicePath == NULL) || (HardDriveDevicePath == NULL)) {
        return FALSE;
    }

    DevicePath = BlockIoDevicePath;
    BlockIoDriveNode = NULL;

    //
    // Find the partition node.
    //

    while (EfiCoreIsDevicePathEnd(DevicePath) == FALSE) {
        if ((EfiCoreGetDevicePathType(DevicePath) == MEDIA_DEVICE_PATH) &&
            (EfiCoreGetDevicePathSubType(DevicePath) ==
             MEDIA_HARDDRIVE_DP)) {

            BlockIoDriveNode = DevicePath;
            break;
        }

        DevicePath = EfiCoreGetNextDevicePathNode(DevicePath);
    }

    if (BlockIoDriveNode == NULL) {
        return FALSE;
    }

    DriveNode = (HARDDRIVE_DEVICE_PATH *)BlockIoDriveNode;
    Match = FALSE;
    if ((DriveNode->MBRType == HardDriveDevicePath->MBRType) &&
        (DriveNode->SignatureType == HardDriveDevicePath->SignatureType)) {

        switch (DriveNode->SignatureType) {
        case SIGNATURE_TYPE_GUID:
            Match = EfiCoreCompareGuids(
                                   (EFI_GUID *)DriveNode->Signature,
                                   (EFI_GUID *)HardDriveDevicePath->Signature);

            break;

        case SIGNATURE_TYPE_MBR:
            Match = EfiCoreCompareMemory(&(DriveNode->Signature[0]),
                                         &(HardDriveDevicePath->Signature[0]),
                                         sizeof(UINT32));

            if (Match == 0) {
                Match = TRUE;

            } else {
                Match = FALSE;
            }

            break;

        default:
            Match = FALSE;
            break;
        }
    }

    return Match;
}

BOOLEAN
EfipBdsIsBootOptionValidVariable (
    PEFI_BDS_COMMON_OPTION Option
    )

/*++

Routine Description:

    This routine determines if the given EFI boot option is a valid
    non-volatile boot option variable.

Arguments:

    Option - Supplies a pointer to the option to check.

Return Value:

    TRUE if the boot option is a non-volatile variable boot option.

    FALSE if the boot option is some sort of temporary boot selection.

--*/

{

    PEFI_BDS_COMMON_OPTION BootOption;
    INTN CompareResult;
    LIST_ENTRY List;
    CHAR16 OptionName[EFI_BOOT_OPTION_MAX_CHAR];
    BOOLEAN Valid;

    Valid = FALSE;
    INITIALIZE_LIST_HEAD(&List);
    EfipBdsCreateHexCodeString(L"Boot",
                               Option->BootCurrent,
                               OptionName,
                               sizeof(OptionName));

    BootOption = EfipBdsConvertVariableToOption(&List, OptionName);
    if (BootOption == NULL) {
        return FALSE;
    }

    if (Option->BootCurrent == BootOption->BootCurrent) {
        CompareResult = EfiCoreCompareMemory(
                                 Option->DevicePath,
                                 BootOption->DevicePath,
                                 EfiCoreGetDevicePathSize(Option->DevicePath));

        if (CompareResult == 0) {
            Valid = TRUE;
        }
    }

    EfiCoreFreePool(BootOption);
    return Valid;
}

VOID
EfipBdsSignalReadyToBootEvent (
    VOID
    )

/*++

Routine Description:

    This routine creates, signals, and closes a "ready to boot" event group.

Arguments:

    None.

Return Value:

    None.

--*/

{

    EFI_EVENT ReadyToBootEvent;
    EFI_STATUS Status;

    Status = EfiCreateEventEx(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              EfiCoreEmptyCallbackFunction,
                              NULL,
                              &EfiEventReadyToBootGuid,
                              &ReadyToBootEvent);

    if (!EFI_ERROR(Status)) {
        EfiSignalEvent(ReadyToBootEvent);
        EfiCloseEvent(ReadyToBootEvent);
    }

    return;
}

EFI_HANDLE
EfipBdsGetBootableHandle (
    EFI_DEVICE_PATH_PROTOCOL *DevicePath
    )

/*++

Routine Description:

    This routine returns the bootable media handle. It checks to see if the
    device is connected, opens the simple file system interface, and then
    detects a boot file in the media.

Arguments:

    DevicePath - Supplies a pointer to the device path of a bootable device.

Return Value:

    Returns the bootable media handle.

    NULL if the media is not bootable.

--*/

{

    EFI_BLOCK_IO_PROTOCOL *BlockIo;
    VOID *Buffer;
    EFI_DEVICE_PATH_PROTOCOL *DevicePathCopy;
    EFI_IMAGE_DOS_HEADER DosHeader;
    EFI_HANDLE Handle;
    EFI_IMAGE_OPTIONAL_HEADER_UNION Header;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION HeaderPointer;
    UINTN Index;
    EFI_TPL OldTpl;
    EFI_DEVICE_PATH_PROTOCOL *PotentialPath;
    UINTN PotentialPathSize;
    EFI_HANDLE ReturnHandle;
    UINTN SimpleFileSystemHandleCount;
    EFI_HANDLE *SimpleFileSystemHandles;
    UINTN Size;
    EFI_STATUS Status;
    EFI_DEVICE_PATH_PROTOCOL *UpdatedDevicePath;

    DevicePathCopy = NULL;
    UpdatedDevicePath = DevicePath;

    //
    // Raise the TPL to prevent the block I/O instance from getting released
    // due to a USB hot plug event.
    //

    OldTpl = EfiRaiseTPL(TPL_CALLBACK);

    //
    // Find out whether or not the device is connected.
    //

    Status = EfiLocateDevicePath(&EfiBlockIoProtocolGuid,
                                 &UpdatedDevicePath,
                                 &Handle);

    if (EFI_ERROR(Status)) {
        Status = EfiLocateDevicePath(&EfiSimpleFileSystemProtocolGuid,
                                     &UpdatedDevicePath,
                                     &Handle);

        //
        // If the simple file system and block I/O protocols are not present,
        // perhaps it's just because the device is not connected.
        //

        if (EFI_ERROR(Status)) {
            UpdatedDevicePath = DevicePath;
            Status = EfiLocateDevicePath(&EfiDevicePathProtocolGuid,
                                         &UpdatedDevicePath,
                                         &Handle);

            if (!EFI_ERROR(Status)) {
                EfiConnectController(Handle, NULL, NULL, TRUE);
            }
        }

    } else {

        //
        // For a removable device boot option, make sure all children are
        // created.
        //

        EfiConnectController(Handle, NULL, NULL, TRUE);

        //
        // Get the block I/O protocol and check the removable attribute.
        //

        Status = EfiHandleProtocol(Handle,
                                   &EfiBlockIoProtocolGuid,
                                   (VOID **)&BlockIo);

        ASSERT(!EFI_ERROR(Status));

        //
        // Issue a dummy read to check for media change.
        //

        Buffer = EfiCoreAllocateBootPool(BlockIo->Media->BlockSize);
        if (Buffer != NULL) {
            BlockIo->ReadBlocks(BlockIo,
                                BlockIo->Media->MediaId,
                                0,
                                BlockIo->Media->BlockSize,
                                Buffer);

            EfiCoreFreePool(Buffer);
        }
    }

    //
    // Detect the default boot file from removable media.
    //

    DevicePathCopy = EfiCoreDuplicateDevicePath(DevicePath);
    if (DevicePathCopy == NULL) {
        return NULL;
    }

    UpdatedDevicePath = DevicePathCopy;
    Status = EfiLocateDevicePath(&EfiDevicePathProtocolGuid,
                                 &UpdatedDevicePath,
                                 &Handle);

    if (EFI_ERROR(Status)) {
        EfiCoreFreePool(DevicePathCopy);
        return NULL;
    }

    //
    // If the resulting device path points to a USB node and the USB node is
    // a dummy node, only let the device path point to the previous PCI node:
    // ACPI/PCI/USB --> ACPI/PCI.
    //

    if ((EfiCoreGetDevicePathType(UpdatedDevicePath) ==
         MESSAGING_DEVICE_PATH) &&
        (EfiCoreGetDevicePathSubType(UpdatedDevicePath) == MSG_USB_DP)) {

        //
        // Remove the USB node, let the device path point to the PCI node.
        //

        EfiCoreSetDevicePathEndNode(UpdatedDevicePath);
        UpdatedDevicePath = DevicePathCopy;

    } else {
        UpdatedDevicePath = DevicePath;
    }

    //
    // Get the device path size of the boot option.
    //

    Size = EfiCoreGetDevicePathSize(UpdatedDevicePath) - END_DEVICE_PATH_LENGTH;
    ReturnHandle = NULL;
    Status = EfiLocateHandleBuffer(ByProtocol,
                                   &EfiSimpleFileSystemProtocolGuid,
                                   NULL,
                                   &SimpleFileSystemHandleCount,
                                   &SimpleFileSystemHandles);

    for (Index = 0; Index < SimpleFileSystemHandleCount; Index += 1) {
        PotentialPath = EfiCoreGetDevicePathFromHandle(
                                               SimpleFileSystemHandles[Index]);

        PotentialPathSize = EfiCoreGetDevicePathSize(PotentialPath) -
                            END_DEVICE_PATH_LENGTH;

        //
        // Determine whether or not the device path of the boot option is part
        // of the simple file system handle's device path.
        //

        if ((Size <= PotentialPathSize) &&
            (EfiCoreCompareMemory(PotentialPath, UpdatedDevicePath, Size) ==
             0)) {

            HeaderPointer.Union = &Header;
            Status = EfipBdsGetImageHeader(SimpleFileSystemHandles[Index],
                                           EFI_REMOVABLE_MEDIA_FILE_NAME,
                                           &DosHeader,
                                           HeaderPointer);

            if ((!EFI_ERROR(Status)) &&
                (EFI_IMAGE_MACHINE_TYPE_SUPPORTED(
                                    HeaderPointer.Pe32->FileHeader.Machine)) &&
                (HeaderPointer.Pe32->OptionalHeader.Subsystem ==
                 EFI_IMAGE_SUBSYSTEM_EFI_APPLICATION)) {

                ReturnHandle = SimpleFileSystemHandles[Index];
                break;
            }
        }
    }

    EfiCoreFreePool(DevicePathCopy);
    if (SimpleFileSystemHandles != NULL) {
        EfiCoreFreePool(SimpleFileSystemHandles);
    }

    EfiRestoreTPL(OldTpl);
    return ReturnHandle;
}

