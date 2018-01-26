/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smbios.c

Abstract:

    This module implements SMBIOS tables for the TI BeagleBone Black.

Author:

    Evan Green 7-May-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/smbios.h>
#include "uefifw.h"
#include "bbonefw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define BBONE_EEPROM_ADDRESS 0x50

#define BBONE_SMBIOS_BIOS_VENDOR "Minoca Corp"

#define BBONE_SMBIOS_SYSTEM_MANUFACTURER "Texas Instruments"

#define BBONE_SMBIOS_MODULE_MANUFACTURER "Texas Instruments"

#define BBONE_SMBIOS_PROCESSOR_MANUFACTURER "Texas Instruments"
#define BBONE_SMBIOS_PROCESSOR_PART "AM3358"
#define BBONE_SMBIOS_PROCESSOR_EXTERNAL_CLOCK 24
#define BBONE_SMBIOS_PROCESSOR_MAX_SPEED 1000
#define BBONE_SMBIOS_PROCESSOR_CURRENT_SPEED 1000
#define BBONE_SMBIOS_PROCESSOR_CORE_COUNT 1

#define BBONE_SMBIOS_CACHE_L1_SIZE 32
#define BBONE_SMBIOS_CACHE_L2_SIZE 256

#define BBONE_BLACK_EEPROM_HEADER 0xEE3355AA
#define BBONE_BLACK_BOARD_NAME_SIZE 8
#define BBONE_BLACK_VERSION_SIZE 4
#define BBONE_BLACK_SERIAL_NUMBER_SIZE 12
#define BBONE_BLACK_CONFIGURATION_OPTIONS_SIZE 32
#define BBONE_BLACK_RESERVED_SIZE 6

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes the format of the EEPROM in the BeagleBone Black.

Members:

    Header - Stores the magic header value. Should be set to the value
        BBONE_BLACK_EEPROM_HEADER.

    BoardName - Stores the ASCII name for the board, which might be A335BNLT.

    Version - Stores the hardware version for the board in ASCII.

    SerialNumber - Stores the ASCII serial number for the board.

    Configuration - Stores the configuration data, contents currently unused.

    Reserved - Stores a reserved area.

--*/

#pragma pack(push, 1)

typedef struct _EFI_BBONE_EEPROM {
    UINT32 Header;
    UINT8 BoardName[BBONE_BLACK_BOARD_NAME_SIZE];
    UINT8 Version[BBONE_BLACK_VERSION_SIZE];
    UINT8 SerialNumber[BBONE_BLACK_SERIAL_NUMBER_SIZE];
    UINT8 Configuration[BBONE_BLACK_CONFIGURATION_OPTIONS_SIZE];
    UINT8 Reserved[BBONE_BLACK_RESERVED_SIZE];
} PACKED EFI_BBONE_EEPROM, *PEFI_BBONE_EEPROM;

#pragma pack(pop)

//
// ----------------------------------------------- Internal Function Prototypes
//

EFI_STATUS
EfipBeagleBoneBlackReadEeprom (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the EEPROM data.
//

EFI_BBONE_EEPROM EfiBeagleBoneBlackEeprom;

SMBIOS_BIOS_INFORMATION EfiBeagleBoneSmbiosBiosInformation = {
    {
        SmbiosBiosInformation,
        sizeof(SMBIOS_BIOS_INFORMATION),
        0x0100
    },

    1,
    2,
    0,
    3,
    0,
    SMBIOS_BIOS_CHARACTERISTIC_UNSUPPORTED,
    0,
    0,
    0,
    0,
    0
};

SMBIOS_SYSTEM_INFORMATION EfiBeagleBoneSmbiosSystemInformation = {
    {
        SmbiosSystemInformation,
        sizeof(SMBIOS_SYSTEM_INFORMATION),
        0x0101
    },

    1,
    2,
    3,
    4,
    {0},
    SMBIOS_SYSTEM_WAKEUP_UNKNOWN,
    3,
    2
};

SMBIOS_MODULE_INFORMATION EfiBeagleBoneSmbiosModuleInformation = {
    {
        SmbiosModuleInformation,
        sizeof(SMBIOS_MODULE_INFORMATION),
        0x0102
    },

    1,
    2,
    0,
    0,
    0,
    SMBIOS_MODULE_MOTHERBOARD,
    0,
    0x0104,
    SMBIOS_MODULE_TYPE_MOTHERBOARD,
    0
};

SMBIOS_ENCLOSURE EfiBeagleBoneSmbiosEnclosure = {
    {
        SmbiosSystemEnclosure,
        sizeof(SMBIOS_ENCLOSURE),
        0x0104
    },

    0,
    SMBIOS_ENCLOSURE_TYPE_UNKNOWN,
    0,
    0,
    0,
    SMBIOS_ENCLOSURE_STATE_UNKNOWN,
    SMBIOS_ENCLOSURE_STATE_UNKNOWN,
    SMBIOS_ENCLOSURE_SECURITY_STATE_UNKNOWN,
    0,
    0,
    0,
    0,
    0,
    0
};

SMBIOS_PROCESSOR_INFORMATION EfiBeagleBoneSmbiosProcessorInformation = {
    {
        SmbiosProcessorInformation,
        sizeof(SMBIOS_PROCESSOR_INFORMATION),
        0x0105
    },

    0,
    SMBIOS_PROCESSOR_TYPE_CENTRAL_PROCESSOR,
    0x2,
    1,
    0,
    0,
    0,
    BBONE_SMBIOS_PROCESSOR_EXTERNAL_CLOCK,
    BBONE_SMBIOS_PROCESSOR_MAX_SPEED,
    BBONE_SMBIOS_PROCESSOR_CURRENT_SPEED,
    SMBIOS_PROCESSOR_STATUS_ENABLED,
    0,
    0x0106,
    0x0107,
    0xFFFF,
    2,
    0,
    3,
    BBONE_SMBIOS_PROCESSOR_CORE_COUNT,
    0,
    SMBIOS_PROCESSOR_CHARACTERISTIC_UNKNOWN,
    0
};

SMBIOS_CACHE_INFORMATION EfiBeagleBoneSmbiosL1Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0106
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    BBONE_SMBIOS_CACHE_L1_SIZE,
    BBONE_SMBIOS_CACHE_L1_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_4_WAY_SET
};

SMBIOS_CACHE_INFORMATION EfiBeagleBoneSmbiosL2Cache = {
    {
        SmbiosCacheInformation,
        sizeof(SMBIOS_CACHE_INFORMATION),
        0x0107
    },

    0,
    SMBIOS_CACHE_ENABLED | SMBIOS_CACHE_WRITE_BACK,
    BBONE_SMBIOS_CACHE_L2_SIZE,
    BBONE_SMBIOS_CACHE_L2_SIZE,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    SMBIOS_CACHE_SRAM_UNKNOWN,
    0,
    SMBIOS_CACHE_ERROR_CORRECTION_NONE,
    SMBIOS_CACHE_TYPE_DATA,
    SMBIOS_CACHE_ASSOCIATIVITY_16_WAY_SET
};

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBeagleBoneCreateSmbiosTables (
    VOID
    )

/*++

Routine Description:

    This routine creates the SMBIOS tables.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    CHAR8 ProductName[BBONE_BLACK_BOARD_NAME_SIZE + 1];
    CHAR8 ProductVersion[BBONE_BLACK_VERSION_SIZE + 1];
    CHAR8 SerialNumber[BBONE_BLACK_SERIAL_NUMBER_SIZE + 1];
    EFI_STATUS Status;

    Status = EfipBeagleBoneBlackReadEeprom();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiBeagleBoneSmbiosBiosInformation.BiosMajorRelease = EfiVersionMajor;
    EfiBeagleBoneSmbiosBiosInformation.BiosMinorRelease = EfiVersionMinor;
    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosBiosInformation,
                                   BBONE_SMBIOS_BIOS_VENDOR,
                                   EfiBuildString,
                                   EfiBuildTimeString,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    //
    // Set the serial number as the UUID.
    //

    EfiCopyMem(SerialNumber,
               EfiBeagleBoneBlackEeprom.SerialNumber,
               BBONE_BLACK_SERIAL_NUMBER_SIZE);

    SerialNumber[BBONE_BLACK_SERIAL_NUMBER_SIZE] = '\0';
    EfiCopyMem(EfiBeagleBoneSmbiosSystemInformation.Uuid,
               EfiBeagleBoneBlackEeprom.SerialNumber,
               BBONE_BLACK_SERIAL_NUMBER_SIZE);

    EfiCopyMem(ProductName,
               EfiBeagleBoneBlackEeprom.BoardName,
               BBONE_BLACK_BOARD_NAME_SIZE);

    ProductName[BBONE_BLACK_BOARD_NAME_SIZE] = '\0';
    EfiCopyMem(ProductVersion,
               EfiBeagleBoneBlackEeprom.Version,
               BBONE_BLACK_VERSION_SIZE);

    ProductVersion[BBONE_BLACK_VERSION_SIZE] = '\0';
    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosSystemInformation,
                                   BBONE_SMBIOS_SYSTEM_MANUFACTURER,
                                   ProductName,
                                   ProductVersion,
                                   SerialNumber,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosModuleInformation,
                                   BBONE_SMBIOS_MODULE_MANUFACTURER,
                                   ProductName,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosEnclosure, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosProcessorInformation,
                                   BBONE_SMBIOS_PROCESSOR_MANUFACTURER,
                                   SerialNumber,
                                   BBONE_SMBIOS_PROCESSOR_PART,
                                   NULL);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosL1Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfiSmbiosAddStructure(&EfiBeagleBoneSmbiosL2Cache, NULL);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBeagleBoneBlackReadEeprom (
    VOID
    )

/*++

Routine Description:

    This routine reads and verifies the EEPROM in the BeagleBone Black.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

{

    UINT16 Address;

    EfipAm335I2c0Initialize();
    EfipAm335I2c0SetSlaveAddress(BBONE_EEPROM_ADDRESS);

    //
    // Write the 0 address to the EEPROM to reset its internal address counter.
    //

    Address = 0;
    EfipAm335I2c0Write(-1, 2, (UINT8 *)&Address);

    //
    // Now read the EEPROM structure.
    //

    EfipAm335I2c0Read(-1,
                      sizeof(EFI_BBONE_EEPROM),
                      (UINT8 *)&EfiBeagleBoneBlackEeprom);

    if (EfiBeagleBoneBlackEeprom.Header != BBONE_BLACK_EEPROM_HEADER) {
        return EFI_NOT_FOUND;
    }

    return EFI_SUCCESS;
}

