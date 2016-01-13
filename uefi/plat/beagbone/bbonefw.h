/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

Module Name:

    bbonefw.h

Abstract:

    This header contains definitions for the BeagleBone Black UEFI
    implementation.

Author:

    Evan Green 19-Dec-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the BeagleBone RAM area.
//

#define BEAGLE_BONE_BLACK_RAM_START 0x80000000
#define BEAGLE_BONE_BLACK_RAM_SIZE (1024 * 1024 * 512)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Store a boolean used for debugging that disables the watchdog timer.
//

extern BOOLEAN EfiDisableWatchdog;

//
// Define the base of the AM335 PRM Device registers.
//

extern VOID *EfiAm335PrmDeviceBase;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfipBeagleBoneBlackSetLeds (
    UINT32 Leds
    );

/*++

Routine Description:

    This routine sets the LEDs to a new value.

Arguments:

    Leds - Supplies the four bits containing whether to set the LEDs high or
        low.

Return Value:

    None.

--*/

VOID
EfipAm335InitializePowerAndClocks (
    VOID
    );

/*++

Routine Description:

    This routine initializes power and clocks for the UEFI firmware on the TI
    AM335x SoC.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfipPlatformSetInterruptLineState (
    UINT32 LineNumber,
    BOOLEAN Enabled,
    BOOLEAN EdgeTriggered
    );

/*++

Routine Description:

    This routine enables or disables an interrupt line.

Arguments:

    LineNumber - Supplies the line number to enable or disable.

    Enabled - Supplies a boolean indicating if the line should be enabled or
        disabled.

    EdgeTriggered - Supplies a boolean indicating if the interrupt is edge
        triggered (TRUE) or level triggered (FALSE).

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipBeagleBoneEnumerateSd (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the SD card on the BeagleBone.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBeagleBoneBlackEnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the display on the BeagleBone Black.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipBeagleBoneEnumerateSerial (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the serial port on the BeagleBone Black.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipEnumerateRamDisks (
    VOID
    );

/*++

Routine Description:

    This routine enumerates any RAM disks embedded in the firmware.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

EFI_STATUS
EfipBeagleBoneCreateSmbiosTables (
    VOID
    );

/*++

Routine Description:

    This routine creates the SMBIOS tables.

Arguments:

    None.

Return Value:

    EFI Status code.

--*/

VOID
EfipAm335I2c0Initialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the I2c bus.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipAm335I2c0SetSlaveAddress (
    UINT8 SlaveAddress
    );

/*++

Routine Description:

    This routine sets which address on the I2C bus to talk to.

Arguments:

    SlaveAddress - Supplies the slave address to communicate with.

Return Value:

    None.

--*/

VOID
EfipAm335I2c0Read (
    UINT32 Register,
    UINT32 Size,
    UINT8 *Data
    );

/*++

Routine Description:

    This routine performs a read from the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to read from. Supply -1 to skip
        transmitting a register number.

    Size - Supplies the number of data bytes to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    None.

--*/

VOID
EfipAm335I2c0Write (
    UINT32 Register,
    UINT32 Size,
    UINT8 *Data
    );

/*++

Routine Description:

    This routine performs a write to the I2C bus. This routine assumes the
    slave address has already been set.

Arguments:

    Register - Supplies the register to write to. Supply -1 to skip transmitting
        a register number.

    Size - Supplies the number of data bytes to write (not including the
        register byte itself).

    Data - Supplies a pointer to the data to write.

Return Value:

    None.

--*/

EFIAPI
VOID
EfipAm335ResetSystem (
    EFI_RESET_TYPE ResetType,
    EFI_STATUS ResetStatus,
    UINTN DataSize,
    VOID *ResetData
    );

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

