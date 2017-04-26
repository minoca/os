/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    boot.c

Abstract:

    This module implements support for the first stage load on TI's AM335x

Author:

    Evan Green 17-Dec-2014

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

#define AM335_MEMORY_DEVICE_DATA_BUFFER 0x80000000
#define AM335_MEMORY_DEVICE_DATA_SIZE 2500

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
INT32
(*PAM335_BOOT_ENTRY_POINT) (
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
EfipAm335LoadFromSd (
    UINT8 DeviceType,
    UINT32 *Length
    );

INTN
EfipAm335BootImage (
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

//
// Define the device version of the AM335x.
//

UINT32 EfiAm335DeviceVersion;

TI_ROM_MEM_HANDLE EfiAm335RomMemHandle;

BOOLEAN EfiSkipCrc;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiFirstStageLoader (
    PAM335_BOOT_DATA BootData
    )

/*++

Routine Description:

    This routine implements the main C routine of the first stage loader. Its
    role is to load the primary firmware.

Arguments:

    BootData - Supplies a pointer to the boot data structure created by the
        SoC ROM code.

Return Value:

    None.

--*/

{

    UINT32 Length;
    UINT32 OppIndex;
    INTN Result;

    //
    // Store the device revision, used in a few places.
    //

    EfiAm335DeviceVersion =
        AM3_READ32(AM335_SOC_CONTROL_REGISTERS + AM335_SOC_CONTROL_DEVICE_ID) >>
        AM335_SOC_CONTROL_DEVICE_ID_REVISION_SHIFT;

    EfipAm335InitializeClocks();
    EfipBeagleBoneBlackInitializeLeds();
    EfipBeagleBoneBlackSetLeds(0x1);
    EfipAm335EnableUart();
    EfipInitializeBoardMux();
    EfipAm335ConfigureVddOpVoltage();
    OppIndex = EfipAm335GetMaxOpp();
    EfipAm335SetVdd1Voltage(EfiAm335OppTable[OppIndex].PmicVoltage);
    EfipAm335InitializePlls(OppIndex, AM335_DDR_PLL_M_DDR3);
    EfipAm335InitializeEmif();
    EfipBeagleBoneBlackInitializeDdr3();
    EfipSerialPrintString("\r\nMinoca Firmware Loader\r\nBoot Device: ");
    EfipSerialPrintHexInteger(BootData->BootDevice);
    EfipSerialPrintString("\r\n");
    Result = EfipAm335LoadFromSd(BootData->BootDevice, &Length);
    if (Result != 0) {
        EfipSerialPrintString("Load Error\r\n");

    } else {
        EfipAm335BootImage(BootData->BootDevice,
                           AM335_SD_BOOT_ADDRESS,
                           Length);
    }

    while (TRUE) {
        NOTHING;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INTN
EfipAm335LoadFromSd (
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

    Base = AM335_PUBLIC_API_BASE;
    DeviceData = (VOID *)AM335_MEMORY_DEVICE_DATA_BUFFER;
    EfipInitZeroMemory(DeviceData, AM335_MEMORY_DEVICE_DATA_SIZE);
    Result = EfipTiMemOpen(DeviceType, Base, DeviceData, &EfiAm335RomMemHandle);
    if (Result != 0) {
        return Result;
    }

    Result = EfipTiLoadFirmwareFromFat(&EfiAm335RomMemHandle,
                                       AM335_FIRMWARE_NAME,
                                       (VOID *)AM335_SD_BOOT_ADDRESS,
                                       Length);

    if (Result != 0) {
        return Result;
    }

    return 0;
}

INTN
EfipAm335BootImage (
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
    PAM335_BOOT_ENTRY_POINT EntryPoint;
    UINT32 HeaderDataCrc;
    UINT32 LoadAddress;
    INTN Result;
    PUBOOT_HEADER UbootHeader;

    //
    // Check for the U-Boot header.
    //

    EfipInitializeCrc32((VOID *)BEAGLEBONE_CRC_TABLE_ADDRESS);
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
        EntryPoint = (PAM335_BOOT_ENTRY_POINT)Image;
    }

    //
    // Set the LEDs to 2 to indicate transition out of the first stage loader.
    //

    EfipBeagleBoneBlackSetLeds(0x2);
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

