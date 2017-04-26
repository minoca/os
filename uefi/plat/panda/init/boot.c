/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    boot.c

Abstract:

    This module implements support for the first stage loader.

Author:

    Evan Green 1-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <uboot.h>
#include "init.h"
#include "util.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_BOOT_USB  0x45
#define OMAP4_BOOT_MMC1 0x05
#define OMAP4_BOOT_MMC2 0x06

#define OMAP4_BOOT_DEVICE_OFFSET 8

//
// Define the "hello" sent over USB indicating to the app on the other side
// that this code is alive.
//

#define OMAP4_USB_BOOT_RESPONSE 0xAABBCCDD

#define OMAP4_MEMORY_DEVICE_DATA_BUFFER 0x80000000
#define OMAP4_MEMORY_DEVICE_DATA_SIZE 2500

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
INT32
(*POMAP4_BOOT_ENTRY_POINT) (
    UINT32 BootType,
    UINT32 Length
    );

/*++

Routine Description:

    This routine is the entry point for a booted option.

Arguments:

    BootType - Supplies the boot device type.

    Length - Supplies the image length.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

//
// ----------------------------------------------- Internal Function Prototypes
//

INTN
EfipOmap4LoadFromUsb (
    UINT32 *Length
    );

INTN
EfipOmap4LoadFromSd (
    UINT8 DeviceType,
    UINT32 *Length
    );

INTN
EfipOmap4BootImage (
    UINT32 BootDeviceType,
    UINT32 Image,
    UINT32 Length
    );

UINT32
EfipByteSwap32 (
    UINT32 Value
    );

//
// -------------------------------------------------------------------- Globals
//

TI_ROM_USB_HANDLE EfiOmap4RomUsbHandle;
TI_ROM_MEM_HANDLE EfiOmap4RomMemHandle;

UINT32 EfiOmap4UsbBootResponse = OMAP4_USB_BOOT_RESPONSE;

BOOLEAN EfiSkipCrc;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiFirstStageLoader (
    UINT8 *Information
    )

/*++

Routine Description:

    This routine implements the main C routine of the first stage loader. Its
    role is to load the primary firmware.

Arguments:

    Information - Supplies an optional pointer to information handed off by the
        ROM code in the SoC.

Return Value:

    None.

--*/

{

    UINT32 BootDevice;
    UINT32 ImageAddress;
    UINT32 Length;
    INTN Result;

    EfipInitializeBoardMux();
    EfipSpin(100);
    EfipScaleVcores();
    EfipInitializePrcm();
    EfipInitializeDdr();
    EfipInitializeGpmc();
    EfipInitializeSerial();
    EfipSerialPrintString("Minoca Firmware Loader\n");
    if (Information != NULL) {
        BootDevice = Information[OMAP4_BOOT_DEVICE_OFFSET];
        EfipSerialPrintString("ResetReason ");
        EfipSerialPrintHexInteger(Information[0x9]);
        EfipSerialPrintString(".\n");

    } else {
        BootDevice = OMAP4_BOOT_USB;
    }

    ImageAddress = 0;
    switch (BootDevice) {
    case OMAP4_BOOT_USB:
        EfipSerialPrintString("USB Boot\n");
        Result = EfipOmap4LoadFromUsb(&Length);
        ImageAddress = OMAP4_USB_BOOT_ADDRESS;
        break;

    case OMAP4_BOOT_MMC1:
    case OMAP4_BOOT_MMC2:
        EfipSerialPrintString("SD Boot\n");
        Result = EfipOmap4LoadFromSd(BootDevice, &Length);
        ImageAddress = OMAP4_SD_BOOT_ADDRESS;
        break;

    default:
        EfipSerialPrintString("Boot type unknown!\n");
        Result = -1;
        break;
    }

    if (Result != 0) {
        EfipSerialPrintString("Load Error.\n");

    } else {
        Result = EfipOmap4BootImage(BootDevice, ImageAddress, Length);
    }

    EfipSerialPrintString("Result: ");
    EfipSerialPrintHexInteger(Result);
    EfipSerialPrintString(".\nHanging...");
    while (TRUE) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INTN
EfipOmap4LoadFromUsb (
    UINT32 *Length
    )

/*++

Routine Description:

    This routine loads the boot loader over USB.

Arguments:

    Length - Supplies a pointer where the length of the loaded image will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT32 ReadLength;
    INTN Result;

    //
    // The processor must be in ARM mode, otherwise enabling interrupts causes
    // a reset.
    //

    EfiEnableInterrupts();
    Result = EfipOmap4UsbOpen(&EfiOmap4RomUsbHandle);
    if (Result != 0) {
        return Result;
    }

    EfipOmap4UsbWrite(&EfiOmap4RomUsbHandle,
                      &EfiOmap4UsbBootResponse,
                      sizeof(EfiOmap4UsbBootResponse));

    EfipOmap4UsbRead(&EfiOmap4RomUsbHandle, &ReadLength, sizeof(UINT32));
    Result = EfipOmap4UsbRead(&EfiOmap4RomUsbHandle,
                              (VOID *)OMAP4_USB_BOOT_ADDRESS,
                              ReadLength);

    if (Result != 0) {
        return Result;
    }

    EfipOmap4UsbClose(&EfiOmap4RomUsbHandle);
    EfiDisableInterrupts();
    *Length = ReadLength;
    return 0;
}

INTN
EfipOmap4LoadFromSd (
    UINT8 DeviceType,
    UINT32 *Length
    )

/*++

Routine Description:

    This routine loads the boot loader over SD.

Arguments:

    DeviceType - Supplies the device type to boot from.

    Length - Supplies a pointer where the length of the loaded image will be
        returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    UINT32 Base;
    VOID *DeviceData;
    INTN Result;

    if (EfipOmap4GetRevision() >= Omap4460RevisionEs10) {
        Base = OMAP4460_PUBLIC_API_BASE;

    } else {
        Base = OMAP4430_PUBLIC_API_BASE;
    }

    DeviceData = (VOID *)OMAP4_MEMORY_DEVICE_DATA_BUFFER;
    EfipInitZeroMemory(DeviceData, OMAP4_MEMORY_DEVICE_DATA_SIZE);
    Result = EfipTiMemOpen(DeviceType, Base, DeviceData, &EfiOmap4RomMemHandle);
    if (Result != 0) {
        return Result;
    }

    Result = EfipTiLoadFirmwareFromFat(&EfiOmap4RomMemHandle,
                                       PANDA_FIRMWARE_NAME,
                                       (VOID *)OMAP4_SD_BOOT_ADDRESS,
                                       Length);

    if (Result != 0) {
        return Result;
    }

    return 0;
}

INTN
EfipOmap4BootImage (
    UINT32 BootDeviceType,
    UINT32 Image,
    UINT32 Length
    )

/*++

Routine Description:

    This routine boots a loaded image in memory.

Arguments:

    BootDeviceType - Supplies the boot device type.

    Image - Supplies the address of the image.

    Length - Supplies the length of the image in bytes.

Return Value:

    0 on success. Actually, does not return on success.

    Non-zero on failure.

--*/

{

    UINT32 Crc;
    POMAP4_BOOT_ENTRY_POINT EntryPoint;
    UINT32 HeaderDataCrc;
    UINT32 LoadAddress;
    INTN Result;
    PUBOOT_HEADER UbootHeader;

    //
    // Check for the U-Boot header.
    //

    EfipInitializeCrc32((VOID *)PANDA_BOARD_CRC_TABLE_ADDRESS);
    UbootHeader = (PUBOOT_HEADER)(UINTN)Image;
    if (EfipByteSwap32(UbootHeader->Magic) == UBOOT_MAGIC) {
        LoadAddress = EfipByteSwap32(UbootHeader->DataLoadAddress);
        if (LoadAddress != Image + sizeof(UBOOT_HEADER)) {
            EfipSerialPrintString("Warning: U-boot load address ");
            EfipSerialPrintHexInteger(LoadAddress);
            EfipSerialPrintString(" but expected ");
            EfipSerialPrintHexInteger(Image + sizeof(UBOOT_HEADER));
            EfipSerialPrintString("\n");
        }

        EntryPoint = (VOID *)EfipByteSwap32(UbootHeader->EntryPoint);
        EfipSerialPrintString("Launching ");
        EfipSerialPrintString(UbootHeader->ImageName);
        EfipSerialPrintString(".\n");
        EfiSkipCrc = TRUE;
        if (EfiSkipCrc == FALSE) {
            Crc = EfipInitCalculateCrc32((VOID *)Image + sizeof(UBOOT_HEADER),
                                         EfipByteSwap32(UbootHeader->DataSize));

            HeaderDataCrc = EfipByteSwap32(UbootHeader->DataCrc32);
            if (Crc != HeaderDataCrc) {
                EfipSerialPrintString("Error: CRC was ");
                EfipSerialPrintHexInteger(Crc);
                EfipSerialPrintString(", header value was ");
                EfipSerialPrintHexInteger(HeaderDataCrc);
                EfipSerialPrintString(".\n");
                return 0x44;
            }
        }

    } else {
        EntryPoint = (POMAP4_BOOT_ENTRY_POINT)Image;
    }

    //
    // Turn on an LED to indicate progress.
    //

    EfipPandaSetLeds(1, 0);

    //
    // Not a u-boot header, jump to it directly.
    //

    EfipSerialPrintString("Jumping to ");
    EfipSerialPrintHexInteger((UINT32)EntryPoint);
    EfipSerialPrintString("...\n");
    Result = EntryPoint(BootDeviceType, Length);
    EfipSerialPrintString("Returned ");
    EfipSerialPrintHexInteger(Result);
    return Result;
}

UINT32
EfipByteSwap32 (
    UINT32 Value
    )

/*++

Routine Description:

    This routine swaps the endianness of the given value.

Arguments:

    Value - Supplies the value to convert.

Return Value:

    Returns the byte swapped value.

--*/

{

    UINT32 Result;

    Result = (Value & 0x000000FF) << 24;
    Result |= (Value & 0x0000FF00) << 8;
    Result |= (Value & 0x00FF0000) >> 8;
    Result |= ((Value & 0xFF000000) >> 24) & 0x000000FF;
    return Result;
}

