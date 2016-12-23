/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
// Define the SYSBOOT pin connected to the boot button on the BeagleBone.
//

#define BEAGLE_BONE_PERIPHERAL_SYSBOOT 0x04

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
// Store the boot device type.
//

UINT32 EfiBootDeviceCode;

//
// Define the base of the AM335 PRM Device registers.
//

extern VOID *EfiAm335PrmDeviceBase;

//
// Define the pointer to the RTC base, which will get virtualized when going
// to runtime.
//

extern VOID *EfiAm335RtcBase;

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
EfipBeagleBoneEnumerateStorage (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the SD card and eMMC on the BeagleBone.

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

VOID
EfipBeagleBoneBlackInitializeRtc (
    VOID
    );

/*++

Routine Description:

    This routine fires up the RTC in the AM335x for the BeagleBone Black, if it
    is not already running.

Arguments:

    None.

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

EFIAPI
EFI_STATUS
EfipAm335GetTime (
    EFI_TIME *Time,
    EFI_TIME_CAPABILITIES *Capabilities
    );

/*++

Routine Description:

    This routine returns the current time and dat information, and
    timekeeping capabilities of the hardware platform.

Arguments:

    Time - Supplies a pointer where the current time will be returned.

    Capabilities - Supplies an optional pointer where the capabilities will be
        returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the time parameter was NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

EFIAPI
EFI_STATUS
EfipAm335SetTime (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current local time and date information.

Arguments:

    Time - Supplies a pointer to the time to set.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

EFIAPI
EFI_STATUS
EfipAm335GetWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine gets the current wake alarm setting.

Arguments:

    Enabled - Supplies a pointer that receives a boolean indicating if the
        alarm is currently enabled or disabled.

    Pending - Supplies a pointer that receives a boolean indicating if the
        alarm signal is pending and requires acknowledgement.

    Time - Supplies a pointer that receives the current wake time.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/

EFIAPI
EFI_STATUS
EfipAm335SetWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine sets the current wake alarm setting.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if a time field is out of range.

    EFI_DEVICE_ERROR if there was a hardware error accessing the device.

    EFI_UNSUPPORTED if the wakeup timer is not supported on this platform.

--*/
