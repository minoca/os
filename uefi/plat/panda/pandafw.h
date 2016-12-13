/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pandafw.h

Abstract:

    This header contains internal definitions for the UEFI PandaBoard firmware.

Author:

    Evan Green 3-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/soc/omap4.h>

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read from and write to a GPIO block.
//

#define READ_GPIO1_REGISTER(_Register) \
    EfiReadRegister32((UINT8 *)EfiOmap4Gpio1Address + (_Register))

#define WRITE_GPIO1_REGISTER(_Register, _Value) \
    EfiWriteRegister32((UINT8 *)EfiOmap4Gpio1Address + (_Register), (_Value))

#define READ_GPIO2_REGISTER(_Register) \
    EfiReadRegister32((UINT8 *)EfiOmap4Gpio2Address + (_Register))

#define WRITE_GPIO2_REGISTER(_Register, _Value) \
    EfiWriteRegister32((UINT8 *)EfiOmap4Gpio2Address + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the PandaBoard RAM area.
//

#define PANDA_RAM_START 0x80000000
#define PANDA_RAM_SIZE (1024 * 1024 * 1024 - 4096)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// Declare variables that need to be virtualized for runtime services.
//

extern VOID *EfiOmap4I2cBase;
extern VOID *EfiOmap4PrmDeviceBase;

//
// Store a pointer to the GPIO register blocks.
//

extern VOID *EfiOmap4Gpio1Address;
extern VOID *EfiOmap4Gpio2Address;

//
// Store a boolean used for debugging that disables the watchdog timer.
//

extern BOOLEAN EfiDisableWatchdog;

//
// -------------------------------------------------------- Function Prototypes
//

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
EfipSmpInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes and parks the second core on the OMAP4.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfipOmap4UsbInitialize (
    VOID
    );

/*++

Routine Description:

    This routine performs any board-specific high speed USB initialization.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipOmapI2cInitialize (
    VOID
    );

/*++

Routine Description:

    This routine initializes the I2C device.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfipOmapI2cFlushData (
    VOID
    );

/*++

Routine Description:

    This routine flushes extraneous data out of the internal FIFOs.

Arguments:

    None.

Return Value:

    None.

--*/

EFI_STATUS
EfipOmapI2cWrite (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    );

/*++

Routine Description:

    This routine writes the given buffer out to the given i2c device.

Arguments:

    Chip - Supplies the device to write to.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies the buffer containing the data to write.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipOmapI2cRead (
    UINT8 Chip,
    UINT32 Address,
    UINT32 AddressLength,
    VOID *Buffer,
    UINT32 Length
    );

/*++

Routine Description:

    This routine reads from the given i2c device into the given buffer.

Arguments:

    Chip - Supplies the device to read from.

    Address - Supplies the address.

    AddressLength - Supplies the width of the address. Valid values are zero
        through two.

    Buffer - Supplies a pointer to the buffer where the read data will be
        returned.

    Length - Supplies the length of the buffer in bytes.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030InitializeMmcPower (
    VOID
    );

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL6030.

Arguments:

    None.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030InitializeRtc (
    VOID
    );

/*++

Routine Description:

    This routine enables the MMC power rails controlled by the TWL4030.

Arguments:

    None.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030ReadRtc (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine reads the current time from the TWL6030.

Arguments:

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030ReadRtcWakeupTime (
    BOOLEAN *Enabled,
    BOOLEAN *Pending,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine reads the wake alarm time from the TWL6030.

Arguments:

    Enabled - Supplies a pointer where a boolean will be returned indicating if
        the wake time interrupt is enabled.

    Pending - Supplies a pointer where a boolean will be returned indicating if
        the wake alarm interrupt is pending and requires service.

    Time - Supplies a pointer where the time is returned on success.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030WriteRtc (
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine writes the current time to the TWL6030.

Arguments:

    Time - Supplies a pointer to the new time to set.

Return Value:

    Status code.

--*/

EFI_STATUS
Omap4Twl6030WriteRtcWakeupTime (
    BOOLEAN Enable,
    EFI_TIME *Time
    );

/*++

Routine Description:

    This routine writes the alarm time to the TWL6030.

Arguments:

    Enable - Supplies a boolean enabling or disabling the wakeup timer.

    Time - Supplies an optional pointer to the time to set. This parameter is
        only optional if the enable parameter is FALSE.

Return Value:

    Status code.

--*/

EFI_STATUS
EfipPandaEnumerateSd (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the SD card on the PandaBoard.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipPandaEnumerateVideo (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the display on the PandaBoard.

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
EfipPandaEnumerateSerial (
    VOID
    );

/*++

Routine Description:

    This routine enumerates the serial port on the PandaBoard.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

EFI_STATUS
EfipPandaCreateSmbiosTables (
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

//
// Runtime functions
//

EFIAPI
EFI_STATUS
EfipOmap4GetTime (
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
EfipOmap4SetTime (
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
EfipOmap4GetWakeupTime (
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
EfipOmap4SetWakeupTime (
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

EFIAPI
VOID
EfipOmap4ResetSystem (
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
