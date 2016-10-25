/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    clock.c

Abstract:

    This module implements clock management for the TI AM335x first stage
    loader.

Author:

    Evan Green 18-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

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
EfipAm335InitializeMpuPll (
    UINTN Multiplier
    );

VOID
EfipAm335InitializeCorePll (
    VOID
    );

VOID
EfipAm335InitializePerPll (
    VOID
    );

VOID
EfipAm335InitializeDdrPll (
    UINTN Multiplier
    );

VOID
EfipAm335InitializeDisplayPll (
    VOID
    );

VOID
EfipAm335InitializeInterfaceClocks (
    VOID
    );

VOID
EfipAm335InitializePowerDomainTransition (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

VOID
EfipAm335InitializeClocks (
    VOID
    )

/*++

Routine Description:

    This routine initializes functional clocks for needed modules and domains.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Enable the L3 clock.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L3_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L3_CLOCK_ENABLE);

    //
    // Enable the L3 instr clock.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L3_INSTR_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_INSTR_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L3_INSTR_CLOCK_ENABLE);

    //
    // Force a software wakeup of the L3 clock.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L3_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_CLOCK_STATE_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_L3_CLOCK_STATE_TRANSITION_MASK) !=
             AM335_CM_PER_L3_CLOCK_STATE_SOFTWARE_WAKEUP);

    //
    // Force a software wakeup of the OCPWP L3 clock.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_OCPWP_L3_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_OCPWP_L3_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_OCPWP_L3_CLOCK_STATE_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_OCPWP_L3_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_OCPWP_L3_CLOCK_STATE_TRANSITION_MASK) !=
             AM335_CM_PER_OCPWP_L3_CLOCK_STATE_SOFTWARE_WAKEUP);

    //
    // Force a software wakeup of the L3S clock.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L3S_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_L3S_CLOCK_STATE_TRANSITION_MASK) !=
             AM335_CM_PER_L3S_CLOCK_STATE_SOFTWARE_WAKEUP);

    //
    // Wait for the idle state in the L3 clock control.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_PER_L3_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the idle state in the L3 instr clock control.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_INSTR_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_PER_L3_INSTR_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the L3 clock to become active.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_L3_CLOCK_STATE_ACTIVE) == 0);

    //
    // Wait for the OCPWP L3 clock to become active.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_OCPWP_L3_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_OCPWP_L3_CLOCK_STATE_ACTIVE) == 0);

    //
    // Wait for the L3S clock to become active.
    //

    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_PER_L3S_CLOCK_STATE_ACTIVE) == 0);

    //
    // Enable the wakeup region.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CONTROL_CLOCK_CONTROL);
    Value |= AM335_CM_WAKEUP_CONTROL_CLOCK_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CONTROL_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CONTROL_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CONTROL_CLOCK_MODE_MASK) !=
             AM335_CM_WAKEUP_CONTROL_CLOCK_ENABLE);

    //
    // Force a software wakeup of the CM wakeup clock.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_WAKEUP_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CLOCK_STATE_TRANSITION_MASK) !=
             AM335_CM_WAKEUP_CLOCK_STATE_SOFTWARE_WAKEUP);

    //
    // Force a software wakeup of the CM L3 Always On clock.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_TRANSITION_MASK) !=
             AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_SOFTWARE_WAKEUP);

    //
    // Enable UART0.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_UART0_CLOCK_CONTROL);
    Value |= AM335_CM_WAKEUP_UART0_CONTROL_CLOCK_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_UART0_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_UART0_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_UART0_CLOCK_MODE_MASK) !=
             AM335_CM_WAKEUP_UART0_CONTROL_CLOCK_ENABLE);

    //
    // Enable I2C0.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_I2C0_CLOCK_CONTROL);
    Value |= AM335_CM_WAKEUP_I2C0_CONTROL_CLOCK_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_I2C0_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_I2C0_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_I2C0_CLOCK_MODE_MASK) !=
             AM335_CM_WAKEUP_I2C0_CONTROL_CLOCK_ENABLE);

    //
    // Wait for the idle state in the CM wakeup control clock.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CONTROL_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CONTROL_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_WAKEUP_CONTROL_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the L3 Always On clock to become active.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_L3_AON_CLOCK_STATE_ACTIVE) == 0);

    //
    // Wait for the L4 wakeup clock transition to become idle.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L4WKUP_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_L4WKUP_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_WAKEUP_L4WKUP_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the L4 wakeup clock to become active.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CLOCK_STATE_L4WAKEUP_ACTIVE) == 0);

    //
    // Wait for the L4 wakeup always on clock to become active.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(
                               AM335_CM_WAKEUP_L4WKUP_AON_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_L4WKUP_AON_CLOCK_STATE_ACTIVE) == 0);

    //
    // Wait for the UART0 clock to become enabled.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CLOCK_STATE_UART0_ACTIVE) == 0);

    //
    // Wait for the I2C0 clock to become enabled.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_CLOCK_STATE_I2C0_ACTIVE) == 0);

    //
    // Wait for the UART clock to become idle.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_UART0_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_UART0_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_WAKEUP_UART0_CLOCK_IDLE_STATE_FUNCTIONAL);

    //
    // Wait for the I2C clock to become idle.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_I2C0_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_I2C0_CLOCK_IDLE_STATE_MASK) !=
             AM335_CM_WAKEUP_I2C0_CLOCK_IDLE_STATE_FUNCTIONAL);

    return;
}

VOID
EfipAm335InitializePlls (
    UINT32 OppIndex,
    UINT32 DdrFrequencyMultiplier
    )

/*++

Routine Description:

    This routine initializes the PLLs for the AM335x.

Arguments:

    OppIndex - Supplies the index into the operating conditions table that the
        PLLs should be configured for.

    DdrFrequencyMultiplier - Supplies the multiplier value to initialize the
        DDR PLL with (depends on whether DDR2 or DDR3 is in use).

Return Value:

    None.

--*/

{

    EfipAm335InitializeMpuPll(EfiAm335OppTable[OppIndex].PllMultiplier);
    EfipAm335InitializeCorePll();
    EfipAm335InitializePerPll();
    EfipAm335InitializeDdrPll(DdrFrequencyMultiplier);
    EfipAm335InitializeInterfaceClocks();
    EfipAm335InitializeDisplayPll();
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipAm335InitializeMpuPll (
    UINTN Multiplier
    )

/*++

Routine Description:

    This routine initializes the MPU PLL.

Arguments:

    Multiplier - Supplies the multiplier value for the PLL.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE_MN_BYPASS;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_MN_BYPASS) == 0);

    //
    // Clear the multiplier and divisor fields.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU);
    Value &= ~(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_MASK |
               AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_MASK);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU, Value);

    //
    // Set the new multiplier and divisor.
    //

    Value |= (Multiplier << AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_MULT_SHIFT) |
             (AM335_MPU_PLL_N <<
              AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_MPU, Value);
    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M2_DPLL_MPU);
    Value &= ~(AM335_CM_WAKEUP_DIV_M2_DPLL_MPU_CLOCK_OUT_MASK);
    Value |= AM335_MPU_PLL_M2;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M2_DPLL_MPU, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_MPU, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_MPU_CLOCK) == 0);

    return;
}

VOID
EfipAm335InitializeCorePll (
    VOID
    )

/*++

Routine Description:

    This routine initializes the Core PLL.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE_ENABLE_MN_BYPASS;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE_MN_BYPASS) == 0);

    //
    // Set the multiplier and divisor.
    //

    Value = (AM335_CORE_PLL_M <<
             AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_CORE_MULT_SHIFT) |
            (AM335_CORE_PLL_N <<
             AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_CORE_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_CORE, Value);

    //
    // Configure the high speed divisors. Start with the M4 divisor.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M4_DPLL_CORE);
    Value &= ~AM335_CM_WAKEUP_DIV_M4_DPLL_CORE_HSDIVIDER_CLOCK_OUT1_DIV_MASK;
    Value |= (AM335_CORE_PLL_HSDIVIDER_M4 <<
              AM335_CM_WAKEUP_DIV_M4_DPLL_CORE_HSDIVIDER_CLOCK_OUT1_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M4_DPLL_CORE, Value);

    //
    // Set the M5 divisor.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M5_DPLL_CORE);
    Value &= ~AM335_CM_WAKEUP_DIV_M5_DPLL_CORE_HSDIVIDER_CLOCK_OUT2_DIV_MASK;
    Value |= (AM335_CORE_PLL_HSDIVIDER_M5 <<
              AM335_CM_WAKEUP_DIV_M5_DPLL_CORE_HSDIVIDER_CLOCK_OUT2_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M5_DPLL_CORE, Value);

    //
    // Set the M6 divisor.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M6_DPLL_CORE);
    Value &= ~AM335_CM_WAKEUP_DIV_M6_DPLL_CORE_HSDIVIDER_CLOCK_OUT3_DIV_MASK;
    Value |= (AM335_CORE_PLL_HSDIVIDER_M6 <<
              AM335_CM_WAKEUP_DIV_M6_DPLL_CORE_HSDIVIDER_CLOCK_OUT3_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M6_DPLL_CORE, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_CORE, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_CORE_CLOCK) == 0);

    return;
}

VOID
EfipAm335InitializePerPll (
    VOID
    )

/*++

Routine Description:

    This routine initializes the Peripheral PLL.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER_ENABLE_MN_BYPASS;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER_MN_BYPASS) == 0);

    //
    // Set the multiplier and divisor.
    //

    Value = (AM335_PER_PLL_M <<
             AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_PER_MULT_SHIFT) |
            (AM335_PER_PLL_N <<
             AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_PER_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_PER, Value);

    //
    // Set the M2 divisor.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M2_DPLL_PER);
    Value &= ~AM335_CM_WAKEUP_DIV_M2_DPLL_PER_CLOCK_OUT_DIV_MASK;
    Value |= (AM335_PER_PLL_M2 <<
              AM335_CM_WAKEUP_DIV_M2_DPLL_PER_CLOCK_OUT_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M2_DPLL_PER, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_PER, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_PER_CLOCK) == 0);

    return;
}

VOID
EfipAm335InitializeDdrPll (
    UINTN Multiplier
    )

/*++

Routine Description:

    This routine initializes the DDR PLL.

Arguments:

    Multiplier - Supplies the multiplier value for the PLL.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR_ENABLE_MN_BYPASS;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR_MN_BYPASS) == 0);

    //
    // Clear the multiplier and divisor fields.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR);
    Value &= ~(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR_MULT_MASK |
               AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR_DIV_MASK);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR, Value);

    //
    // Set the new multiplier and divisor.
    //

    Value |= (Multiplier << AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR_MULT_SHIFT) |
             (AM335_DDR_PLL_N <<
              AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DDR, Value);
    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M2_DPLL_DDR);
    Value &= ~(AM335_CM_WAKEUP_DIV_M2_DPLL_DDR_CLOCK_OUT_MASK);
    Value |= AM335_DDR_PLL_M2;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M2_DPLL_DDR, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DDR, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DDR_CLOCK) == 0);

    return;
}

VOID
EfipAm335InitializeDisplayPll (
    VOID
    )

/*++

Routine Description:

    This routine initializes the Display PLL.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Put the PLL in bypass mode.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP) &
            ~AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP_ENABLE;

    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP_ENABLE_MN_BYPASS;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP, Value);

    //
    // Wait for the PLL to go into bypass mode.
    //

    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP_MN_BYPASS) == 0);

    //
    // Clear the multiplier and divisor fields.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP);
    Value &= ~(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_MULT_MASK |
               AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_DIV_MASK);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP, Value);

    //
    // Set the new multiplier and divisor.
    //

    Value |= (AM335_DISP_PLL_M <<
              AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_MULT_SHIFT) |
             (AM335_DISP_PLL_N <<
              AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP_DIV_SHIFT);

    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_SELECT_DPLL_DISP, Value);
    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_DIV_M2_DPLL_DISP);
    Value &= ~(AM335_CM_WAKEUP_DIV_M2_DPLL_DISP_CLOCK_OUT_MASK);
    Value |= AM335_DISP_PLL_M2;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_DIV_M2_DPLL_DISP, Value);

    //
    // Enable and lock the PLL.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP);
    Value |= AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_MODE_DPLL_DISP, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP);

    } while ((Value & AM335_CM_WAKEUP_IDLE_STATUS_DPLL_DISP_CLOCK) == 0);

    return;
}

VOID
EfipAm335InitializeInterfaceClocks (
    VOID
    )

/*++

Routine Description:

    This routine fires up the needed interface clocks around the SoC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    //
    // Some of these interfaces have already been initialized getting the UART
    // and LEDs running, but it's nice to have these all in once place.
    //

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L3_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L3_CLOCK_ENABLE);

    Value = AM3_CM_PER_READ(AM335_CM_PER_L4LS_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L4LS_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L4LS_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L4LS_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L4LS_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L4LS_CLOCK_ENABLE);

    Value = AM3_CM_PER_READ(AM335_CM_PER_L4FW_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L4FW_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L4FW_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L4FW_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L4FW_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L4FW_CLOCK_ENABLE);

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L4WKUP_CLOCK_CONTROL);
    Value |= AM335_CM_WAKEUP_L4FW_CLOCK_ENABLE;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_L4WKUP_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_L4WKUP_CLOCK_CONTROL);

    } while ((Value & AM335_CM_WAKEUP_L4FW_CLOCK_MODE_MASK) !=
             AM335_CM_WAKEUP_L4FW_CLOCK_ENABLE);

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L3_INSTR_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L3_INSTR_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L3_INSTR_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L3_INSTR_CLOCK_ENABLE);

    Value = AM3_CM_PER_READ(AM335_CM_PER_L4HS_CLOCK_CONTROL);
    Value |= AM335_CM_PER_L4HS_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_L4HS_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_L4HS_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_L4HS_CLOCK_MODE_MASK) !=
             AM335_CM_PER_L4FW_CLOCK_ENABLE);

    //
    // Enable USB clocks.
    //

    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_DCO_LDO_DPLL_PER);
    Value |= AM335_CM_WAKEUP_DCO_LDO_PER_DPLL_GATE_CONTROL;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_DCO_LDO_DPLL_PER, Value);
    Value = AM3_CM_PER_READ(AM335_CM_PER_USB0_CLOCK_CONTROL);
    Value |= AM335_CM_PER_USB0_CLOCK_ENABLE;
    AM3_CM_PER_WRITE(AM335_CM_PER_USB0_CLOCK_CONTROL, Value);
    do {
        Value = AM3_CM_PER_READ(AM335_CM_PER_USB0_CLOCK_CONTROL);

    } while ((Value & AM335_CM_PER_USB0_CLOCK_MODE_MASK) !=
             AM335_CM_PER_USB0_CLOCK_ENABLE);

    return;
}

VOID
EfipAm335InitializePowerDomainTransition (
    VOID
    )

/*++

Routine Description:

    This routine performs clock wakeups for needed modules.

Arguments:

    None.

Return Value:

    None.

--*/

{

    UINT32 Value;

    Value = AM3_CM_PER_READ(AM335_CM_PER_L3_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L3_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3_CLOCK_STATE_CONTROL, Value);
    Value = AM3_CM_PER_READ(AM335_CM_PER_L4LS_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L4LS_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L4LS_CLOCK_STATE_CONTROL, Value);
    Value = AM3_CM_WAKEUP_READ(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_WAKEUP_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_WAKEUP_WRITE(AM335_CM_WAKEUP_CLOCK_STATE_CONTROL, Value);
    Value = AM3_CM_PER_READ(AM335_CM_PER_L4FW_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L4FW_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L4FW_CLOCK_STATE_CONTROL, Value);
    Value = AM3_CM_PER_READ(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL);
    Value |= AM335_CM_PER_L3S_CLOCK_STATE_SOFTWARE_WAKEUP;
    AM3_CM_PER_WRITE(AM335_CM_PER_L3S_CLOCK_STATE_CONTROL, Value);
    return;
}

