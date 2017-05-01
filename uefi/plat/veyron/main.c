/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    main.c

Abstract:

    This module implements the entry point for the UEFI firmware running on top
    of the RK3288 Veyron.

Author:

    Evan Green 10-Jul-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "veyronfw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define FIRMWARE_IMAGE_NAME "veyronfw.elf"

//
// The I2C PMU defaults to run at 400KHz.
//

#define RK32_I2C_PMU_FREQUENCY 400000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipVeyronConfigureArmPll (
    VOID
    );

EFI_STATUS
EfipVeyronConfigureI2cClock (
    VOID
    );

VOID
EfipVeyronConfigureMmcClocks (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Variables defined in the linker script that mark the start and end of the
// image.
//

extern INT8 _end;
extern INT8 __executable_start;

//
// Disable the RK3288 watchdog by default. Once it is started, it cannot be
// stopped. So, it is essentially useless unless a keep-alive method is
// implemented.
//

BOOLEAN EfiDisableWatchdog = TRUE;

//
// Define a boolean indicating whether the firmware was loaded via SD or eMMC.
//

BOOLEAN EfiBootedViaSd;

//
// ------------------------------------------------------------------ Functions
//

__USED
VOID
EfiVeyronMain (
    VOID *TopOfStack,
    UINTN StackSize
    )

/*++

Routine Description:

    This routine is the C entry point for the firmware.

Arguments:

    TopOfStack - Supplies the top of the stack that has been set up for the
        loader.

    StackSize - Supplies the total size of the stack set up for the loader, in
        bytes.

Return Value:

    This routine does not return.

--*/

{

    UINTN FirmwareSize;

    //
    // Initialize UEFI enough to get into the debugger.
    //

    FirmwareSize = (UINTN)&_end - (UINTN)&__executable_start;
    EfiCoreMain((VOID *)-1,
                &__executable_start,
                FirmwareSize,
                FIRMWARE_IMAGE_NAME,
                TopOfStack - StackSize,
                StackSize);

    return;
}

EFI_STATUS
EfiPlatformInitialize (
    UINT32 Phase
    )

/*++

Routine Description:

    This routine performs platform-specific firmware initialization.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer and interrupt services are
        initialized. Phase two happens right before boot, after all platform
        devices have been enumerated.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    if (Phase == 0) {
        if (EfiDisableWatchdog != FALSE) {
            EfiPlatformSetWatchdogTimer(0, 0, 0, NULL);
        }

        EfipVeyronConfigureArmPll();
        EfipVeyronConfigureMmcClocks();

        //
        // Initialize the I2C clock so that the clock frequency querying code
        // does not need to be present in the runtime core.
        //

        Status = EfipVeyronConfigureI2cClock();
        if (EFI_ERROR(Status)) {
            return Status;
        }

    } else if (Phase == 1) {
        EfipVeyronUsbInitialize();
        Status = EfipSmpInitialize();
        if (EFI_ERROR(Status)) {
            return Status;
        }

        Status = EfipVeyronCreateSmbiosTables();
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfiPlatformEnumerateDevices (
    VOID
    )

/*++

Routine Description:

    This routine enumerates and connects any builtin devices the platform
    contains.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    EFI_STATUS Status;

    Status = EfipVeyronEnumerateVideo();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipVeyronEnumerateSd();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipVeyronEnumerateSerial();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    Status = EfipEnumerateRamDisks();
    if (EFI_ERROR(Status)) {
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
EfipRk32GetPllClockFrequency (
    RK32_PLL_TYPE PllType,
    UINT32 *Frequency
    )

/*++

Routine Description:

    This routine returns the base PLL clock frequency of the given type.

Arguments:

    PllType - Supplies the type of the PLL clock whose frequency is being
        queried.

    Frequency - Supplies a pointer that receives the PLL clock's frequency in
        Hertz.

Return Value:

    Status code.

--*/

{

    RK32_CRU_REGISTER Configuration0;
    RK32_CRU_REGISTER Configuration1;
    VOID *CruBase;
    UINT32 Mode;
    UINT32 Nf;
    UINT32 No;
    UINT32 Nr;
    UINT32 Value;

    CruBase = (VOID *)RK32_CRU_BASE;
    *Frequency = 0;

    //
    // The CRU mode control register encodes the clock mode for each of the PLL
    // clocks.
    //

    Mode = EfiReadRegister32(CruBase + Rk32CruModeControl);
    switch (PllType) {
    case Rk32PllNew:
        Mode = (Mode & RK32_CRU_MODE_CONTROL_NEW_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_NEW_PLL_MODE_SHIFT;

        Configuration0 = Rk32CruNewPllConfiguration0;
        Configuration1 = Rk32CruNewPllConfiguration1;
        break;

    case Rk32PllCodec:
        Mode = (Mode & RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_CODEC_PLL_MODE_SHIFT;

        Configuration0 = Rk32CruCodecPllConfiguration0;
        Configuration1 = Rk32CruCodecPllConfiguration1;
        break;

    case Rk32PllGeneral:
        Mode = (Mode & RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_GENERAL_PLL_MODE_SHIFT;

        Configuration0 = Rk32CruGeneralPllConfiguration0;
        Configuration1 = Rk32CruGeneralPllConfiguration1;
        break;

    case Rk32PllDdr:
        Mode = (Mode & RK32_CRU_MODE_CONTROL_DDR_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_DDR_PLL_MODE_SHIFT;

        Configuration0 = Rk32CruDdrPllConfiguration0;
        Configuration1 = Rk32CruDdrPllConfiguration1;
        break;

    case Rk32PllArm:
        Mode = (Mode & RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK) >>
                RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_SHIFT;

        Configuration0 = Rk32CruArmPllConfiguration0;
        Configuration1 = Rk32CruArmPllConfiguration1;
        break;

    default:
        return EFI_UNSUPPORTED;
    }

    //
    // Calculate the frequency based on the mode.
    //

    if (Mode == RK32_CRU_MODE_CONTROL_SLOW_MODE) {
        *Frequency = RK32_CRU_PLL_SLOW_MODE_FREQUENCY;

    } else if (Mode == RK32_CRU_MODE_CONTROL_NORMAL_MODE) {

        //
        // Calculate the clock speed based on the formula described in
        // section 3.9 of the RK3288 TRM.
        //

        Value = EfiReadRegister32(CruBase + Configuration0);
        No = (Value & RK32_PLL_CONFIGURATION0_OD_MASK) >>
             RK32_PLL_CONFIGURATION0_OD_SHIFT;

        No += 1;
        Nr = (Value & RK32_PLL_CONFIGURATION0_NR_MASK) >>
             RK32_PLL_CONFIGURATION0_NR_SHIFT;

        Nr += 1;
        Value = EfiReadRegister32(CruBase + Configuration1);
        Nf = (Value & RK32_PLL_CONFIGURATION1_NF_MASK) >>
             RK32_PLL_CONFIGURATION1_NF_SHIFT;

        Nf += 1;
        *Frequency = RK32_CRU_PLL_COMPUTE_CLOCK_FREQUENCY(Nf, Nr, No);

    } else if (Mode == RK32_CRU_MODE_CONTROL_DEEP_SLOW_MODE) {
        *Frequency = RK32_CRU_PLL_DEEP_SLOW_MODE_FREQUENCY;

    } else {
        return EFI_DEVICE_ERROR;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipVeyronConfigureArmPll (
    VOID
    )

/*++

Routine Description:

    This routine configures the ARM PLL, since the 1800MHz set by the firmware
    actually seems to be too fast to run correctly.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Frequency;
    UINT32 Nf;
    UINT32 No;
    UINT32 Nr;
    UINT32 Value;

    //
    // Put the PLL in slow mode to bypass the PLL while it's being configured.
    //

    Value = (RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK << 16) |
            RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_SLOW;

    RK32_WRITE_CRU(Rk32CruModeControl, Value);
    Frequency = VEYRON_ARM_CPU_HERTZ;
    Nr = 1;
    No = 1;
    Nf = (UINT32)((UINT64)Frequency * Nr * No / VEYRON_OSC_HERTZ);

    //
    // Reset the PLL.
    //

    Value = (RK32_PLL_CONFIGURATION3_RESET << 16) |
            RK32_PLL_CONFIGURATION3_RESET;

    RK32_WRITE_CRU(Rk32CruArmPllConfiguration3, Value);

    //
    // Configure the PLL.
    //

    Value = ((RK32_PLL_CONFIGURATION0_NR_MASK |
              RK32_PLL_CONFIGURATION0_OD_MASK) << 16) |
            ((Nr - 1) << RK32_PLL_CONFIGURATION0_NR_SHIFT) | (No - 1);

    RK32_WRITE_CRU(Rk32CruArmPllConfiguration0, Value);
    Value = (RK32_PLL_CONFIGURATION1_NF_MASK << 16) | (Nf - 1);
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration1, Value);
    Value = (RK32_PLL_CONFIGURATION2_BWADJ_MASK << 16) | ((Nf >> 1) - 1);
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration2, Value);
    EfiStall(10);

    //
    // Clear reset.
    //

    Value = RK32_PLL_CONFIGURATION3_RESET << 16;
    RK32_WRITE_CRU(Rk32CruArmPllConfiguration3, Value);

    //
    // Wait for the PLL to lock itself.
    //

    do {
        Value = RK32_READ_GRF(Rk32GrfSocStatus1);

    } while ((Value & RK32_GRF_SOC_STATUS1_ARM_PLL_LOCK) == 0);

    //
    // Enter normal mode on the PLL.
    //

    Value = (RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_MASK << 16) |
            RK32_CRU_MODE_CONTROL_ARM_PLL_MODE_NORMAL;

    RK32_WRITE_CRU(Rk32CruModeControl, Value);
    return;
}

EFI_STATUS
EfipVeyronConfigureI2cClock (
    VOID
    )

/*++

Routine Description:

    This routine configures the I2C clock to 400Khz. This is done outside the
    runtime core to avoid pulling in the clock querying code and divide
    intrinsics.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    UINT32 AclkPllFrequency;
    UINT32 BusAclkDivider;
    UINT32 BusAclkDivider1;
    UINT32 BusAclkFrequency;
    UINT32 BusPclkDivider;
    UINT32 BusPclkFrequency;
    UINT32 Divisor;
    UINT32 DivisorHigh;
    UINT32 DivisorLow;
    VOID *I2cPmuBase;
    VOID *IoMuxRegister;
    RK32_PLL_TYPE PllType;
    VOID *PmuBase;
    VOID *PullRegister;
    EFI_STATUS Status;
    UINT32 Value;

    I2cPmuBase = (VOID *)RK32_I2C_PMU_BASE;
    PmuBase = (VOID *)RK32_PMU_BASE;
    EfiWriteRegister32(PmuBase + Rk32PmuIomuxGpio0B,
                       RK32_PMU_IOMUX_GPIO0B_I2C0_SDA);

    EfiWriteRegister32(PmuBase + Rk32PmuIomuxGpio0C,
                       RK32_PMU_IOMUX_GPIO0C_I2C0_SCL);

    //
    // Initialize the I/O muxing for I2C4 for the touchpad.
    //

    IoMuxRegister = (VOID *)(RK32_GRF_BASE + Rk32GrfGpio7clIomux);
    EfiWriteRegister32(IoMuxRegister, RK32_GRF_GPIO7CL_IOMUX_VALUE);

    //
    // Get the frequency of the bus PCLK. The bus's ACLK must first be
    // calculated.
    //

    PllType = Rk32PllCodec;
    Value = EfiReadRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect1);
    if ((Value & RK32_CRU_CLOCK_SELECT1_GENERAL_PLL) != 0) {
        PllType = Rk32PllGeneral;
    }

    Status = EfipRk32GetPllClockFrequency(PllType, &AclkPllFrequency);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    BusAclkDivider = (Value & RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER_MASK) >>
                     RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER_SHIFT;

    BusAclkDivider += 1;
    BusAclkDivider1 = (Value & RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER1_MASK) >>
                      RK32_CRU_CLOCK_SELECT1_ACLK_DIVIDER1_SHIFT;

    BusAclkDivider1 += 1;
    BusAclkFrequency = AclkPllFrequency / (BusAclkDivider * BusAclkDivider1);

    //
    // Now divide the ACLK by the PCLK's divider to get the PCLK frequency.
    //

    BusPclkDivider = (Value & RK32_CRU_CLOCK_SELECT1_PCLK_DIVIDER_MASK) >>
                     RK32_CRU_CLOCK_SELECT1_PCLK_DIVIDER_SHIFT;

    BusPclkDivider += 1;
    BusPclkFrequency = BusAclkFrequency / BusPclkDivider;

    //
    // Set the clock divisor to run at 400khz.
    //

    Divisor = (BusPclkFrequency + (8 * RK32_I2C_PMU_FREQUENCY - 1)) /
              (8 * RK32_I2C_PMU_FREQUENCY);

    DivisorHigh = ((Divisor * 3) / 7) - 1;
    DivisorLow = Divisor - DivisorHigh - 2;
    Value = (DivisorHigh << RK32_I2C_CLOCK_DIVISOR_HIGH_SHIFT) &
            RK32_I2C_CLOCK_DIVISOR_HIGH_MASK;

    Value |= (DivisorLow << RK32_I2C_CLOCK_DIVISOR_LOW_SHIFT) &
             RK32_I2C_CLOCK_DIVISOR_LOW_MASK;

    EfiWriteRegister32(I2cPmuBase + Rk32I2cClockDivisor, Value);

    //
    // Do all this same magic for I2C4, the touchpad controller. This is
    // the code equivalent of tracing the clock tree diagram with your finger.
    //

    PllType = Rk32PllCodec;
    Value = EfiReadRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect10);
    if ((Value & RK32_CRU_CLOCK_SELECT10_GENERAL_PLL) != 0) {
        PllType = Rk32PllGeneral;
    }

    Status = EfipRk32GetPllClockFrequency(PllType, &AclkPllFrequency);
    if (EFI_ERROR(Status)) {
        return Status;
    }

    BusAclkDivider = (Value & RK32_CRU_CLOCK_SELECT10_ACLK_DIVIDER_MASK) >>
                     RK32_CRU_CLOCK_SELECT10_ACLK_DIVIDER_SHIFT;

    BusAclkDivider += 1;
    BusAclkFrequency = AclkPllFrequency / BusAclkDivider;
    BusPclkDivider = (Value & RK32_CRU_CLOCK_SELECT10_PCLK_DIVIDER_MASK) >>
                     RK32_CRU_CLOCK_SELECT10_PCLK_DIVIDER_SHIFT;

    BusPclkFrequency = BusAclkFrequency / (1 << BusPclkDivider);

    //
    // Set the clock divisor to run at 400kHz.
    //

    Divisor = (BusPclkFrequency + (8 * RK32_I2C_PMU_FREQUENCY - 1)) /
              (8 * RK32_I2C_PMU_FREQUENCY);

    DivisorHigh = ((Divisor * 3) / 7) - 1;
    DivisorLow = Divisor - DivisorHigh - 2;
    Value = (DivisorHigh << RK32_I2C_CLOCK_DIVISOR_HIGH_SHIFT) &
            RK32_I2C_CLOCK_DIVISOR_HIGH_MASK;

    Value |= (DivisorLow << RK32_I2C_CLOCK_DIVISOR_LOW_SHIFT) &
             RK32_I2C_CLOCK_DIVISOR_LOW_MASK;

    EfiWriteRegister32((VOID *)RK32_I2C_TP_BASE + Rk32I2cClockDivisor, Value);

    //
    // Enable the pull-up for the touchpad interrupt line.
    //

    PullRegister = (VOID *)RK32_GRF_BASE + Rk32GrfGpio7aPull;
    EfiWriteRegister32(PullRegister, RK32_GRF_GPIO7A_PULL_VALUE);
    return EFI_SUCCESS;
}

VOID
EfipVeyronConfigureMmcClocks (
    VOID
    )

/*++

Routine Description:

    This routine configures the MMC clock.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    UINT32 Mask;
    UINT32 Mmc;
    UINT32 Value;

    //
    // To figure out if the firmware was loaded from SD or eMMC, check to see
    // which clock was configured. If SD was configured for high speed, assume
    // boot came from there.
    //

    Mmc = EfiReadRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect11);
    if ((Mmc & RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_MASK) !=
        (RK32_CRU_CLOCK_SELECT11_MMC0_24MHZ <<
         RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_SHIFT)) {

        EfiBootedViaSd = TRUE;
    }

    //
    // Set up MMC0 to clock off of the general PLL / 6, which comes out to
    // 99MHz.
    //

    Mmc = (RK32_CRU_CLOCK_SELECT11_MMC0_GENERAL_PLL <<
           RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_SHIFT) |
          (5 << RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_SHIFT);

    Mask = RK32_CRU_CLOCK_SELECT11_MMC0_CLOCK_MASK |
           RK32_CRU_CLOCK_SELECT11_MMC0_DIVIDER_MASK;

    Mmc |= Mask << RK32_CRU_CLOCK_SELECT11_PROTECT_SHIFT;
    EfiWriteRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect11, Mmc);

    //
    // Set up eMMC like the MMC0.
    //

    Mmc = (RK32_CRU_CLOCK_SELECT12_EMMC_GENERAL_PLL <<
           RK32_CRU_CLOCK_SELECT12_EMMC_CLOCK_SHIFT) |
          (5 << RK32_CRU_CLOCK_SELECT12_EMMC_DIVIDER_SHIFT);

    Mask = RK32_CRU_CLOCK_SELECT12_EMMC_CLOCK_MASK |
           RK32_CRU_CLOCK_SELECT12_EMMC_DIVIDER_MASK;

    Mmc |= Mask << RK32_CRU_CLOCK_SELECT12_PROTECT_SHIFT;
    EfiWriteRegister32((VOID *)RK32_CRU_BASE + Rk32CruClockSelect12, Mmc);

    //
    // Reset the SD/MMC.
    //

    Value = RK32_CRU_SOFT_RESET8_MMC0 << RK32_CRU_SOFT_RESET8_PROTECT_SHIFT;
    Value |= RK32_CRU_SOFT_RESET8_MMC0;
    EfiWriteRegister32((VOID *)RK32_CRU_BASE + Rk32CruSoftReset8, Value);
    EfiStall(100);
    Value &= ~RK32_CRU_SOFT_RESET8_MMC0;
    EfiWriteRegister32((VOID *)RK32_CRU_BASE + Rk32CruSoftReset8, Value);

    //
    // Reset the IOMUX to the correct value for SD/MMC.
    //

    Value = RK32_GRF_GPIO6C_IOMUX_VALUE;
    EfiWriteRegister32((VOID *)RK32_GRF_BASE + Rk32GrfGpio6cIomux, Value);
    return;
}

