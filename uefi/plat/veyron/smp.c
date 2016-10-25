/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smp.c

Abstract:

    This module implements support routines for the second core on RK3288 SoCs.

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

//
// Define the parking page locations. This is completely arbitrary, but has to
// match the values in the MADT.
//

#define RK32_CPU_PARKING_BASE 0x00080000
#define RK32_CPU_COUNT 4
#define RK32_CPU_PARKED_ADDRESS_SIZE 0x1000
#define RK32_CPU_TOTAL_PARKED_ADDRESS_SIZE \
    (RK32_CPU_COUNT * RK32_CPU_PARKED_ADDRESS_SIZE)

#define ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET 0x0800

//
// Define the physical processor ID base. This comes from core 0's MPIDR and
// must match the values in the MADT.
//

#define RK32_PROCESSOR_ID_BASE 0x500

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Internal assembly routines.
//

VOID
EfipRk32SendEvent (
    VOID
    );

VOID
EfipRk32ProcessorStartup (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the variables other cores read to boot.
//

volatile UINT32 EfiRk32ProcessorId;
VOID volatile *EfiRk32ProcessorJumpAddress;

extern UINT8 EfipRk32ParkingLoop;
extern UINT8 EfipRk32ParkingLoopEnd;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipSmpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes and parks the second core on the RK32xx.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    UINT32 Bit;
    UINT32 CoreMask;
    VOID *Cpu;
    UINTN CpuIndex;
    VOID *Cru;
    EFI_PHYSICAL_ADDRESS ParkedAddress;
    VOID *Pmu;
    UINT32 *Sram;
    EFI_STATUS Status;
    UINT32 Value;

    //
    // Allocate the pages for the firmware parked spaces.
    //

    Cru = (VOID *)RK32_CRU_BASE;
    Pmu = (VOID *)RK32_PMU_BASE;
    ParkedAddress = RK32_CPU_PARKING_BASE;
    Status = EfiAllocatePages(
                        AllocateAddress,
                        EfiACPIMemoryNVS,
                        EFI_SIZE_TO_PAGES(RK32_CPU_TOTAL_PARKED_ADDRESS_SIZE),
                        &ParkedAddress);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiSetMem((VOID *)(UINTN)ParkedAddress,
              RK32_CPU_TOTAL_PARKED_ADDRESS_SIZE,
              0);

    Cpu = (VOID *)(UINTN)ParkedAddress;
    for (CpuIndex = 0; CpuIndex < RK32_CPU_COUNT; CpuIndex += 1) {

        //
        // Write -1 to the processor number location.
        //

        Cpu = (VOID *)((UINTN)ParkedAddress +
                       (CpuIndex * RK32_CPU_PARKED_ADDRESS_SIZE));

        EfiWriteRegister32(Cpu, -1);

        //
        // Copy the parking protocol loops into the right places.
        //

        EfiCopyMem(
                 Cpu + ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET,
                 &EfipRk32ParkingLoop,
                 (UINTN)&EfipRk32ParkingLoopEnd - (UINTN)&EfipRk32ParkingLoop);

    }

    EfiCoreInvalidateInstructionCacheRange((VOID *)(UINTN)ParkedAddress,
                                           RK32_CPU_TOTAL_PARKED_ADDRESS_SIZE);

    //
    // Assert reset on cores 1 through 3 before powering them down.
    //

    CoreMask = RK32_CRU_SOFT_RESET0_CORE1 |
               RK32_CRU_SOFT_RESET0_CORE2 |
               RK32_CRU_SOFT_RESET0_CORE3;

    Value = (CoreMask << RK32_CRU_SOFT_RESET0_PROTECT_SHIFT) | CoreMask;
    EfiWriteRegister32(Cru + Rk32CruSoftReset0, Value);

    //
    // Power down the cores.
    //

    CoreMask = RK32_PMU_POWER_DOWN_CONTROL_A17_1 |
               RK32_PMU_POWER_DOWN_CONTROL_A17_2 |
               RK32_PMU_POWER_DOWN_CONTROL_A17_3;

    Value = EfiReadRegister32(Pmu + Rk32PmuPowerDownControl);
    Value |= CoreMask;
    EfiWriteRegister32(Pmu + Rk32PmuPowerDownControl, Value);
    CoreMask = RK32_PMU_POWER_DOWN_STATUS_A17_1 |
               RK32_PMU_POWER_DOWN_STATUS_A17_2 |
               RK32_PMU_POWER_DOWN_STATUS_A17_3;

    while (TRUE) {
        Value = EfiReadRegister32(Pmu + Rk32PmuPowerDownStatus);
        if ((Value & CoreMask) == CoreMask) {
            break;
        }
    }

    //
    // Start up other cores and send them to their parking places.
    //

    for (CpuIndex = 1; CpuIndex < RK32_CPU_COUNT; CpuIndex += 1) {
        Cpu = (VOID *)((UINTN)ParkedAddress +
                       (CpuIndex * RK32_CPU_PARKED_ADDRESS_SIZE));

        EfiRk32ProcessorJumpAddress = Cpu +
                                      ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET;

        EfiRk32ProcessorId = RK32_PROCESSOR_ID_BASE + CpuIndex;

        //
        // Power up the core by clearing the power down control for the core.
        //

        Value = EfiReadRegister32(Pmu + Rk32PmuPowerDownControl);
        Value &= ~(RK32_PMU_POWER_DOWN_CONTROL_A17_0 << CpuIndex);
        EfiWriteRegister32(Pmu + Rk32PmuPowerDownControl, Value);

        //
        // Take the core out of reset. Deasserting reset means writing a 0,
        // and the reset protect tells the register which bits to listen to.
        //

        Bit = RK32_CRU_SOFT_RESET0_CORE0 << CpuIndex;
        Value = Bit << RK32_CRU_SOFT_RESET0_PROTECT_SHIFT;
        EfiWriteRegister32(Cru + Rk32CruSoftReset0, Value);

        //
        // Wait for the status bit to clear.
        //

        do {
            Value = EfiReadRegister32(Pmu + Rk32PmuPowerDownStatus);

        } while ((Value & (RK32_PMU_POWER_DOWN_STATUS_A17_0 << CpuIndex)) != 0);

        //
        // The other cores are sitting in their own parking loop off in SRAM.
        // bring them out of that and into this parking loop.
        //

        Sram = (UINT32 *)RK32_SRAM_BASE;
        Sram[2] = (UINT32)EfipRk32ProcessorStartup;
        Sram[1] = 0xDEADBEAF;

        //
        // Send an event to wake the core up.
        //

        EfipRk32SendEvent();

        //
        // Wait for the CPU to come to life. For some reason marking the
        // variable as volatile doesn't seem to prevent optimization.
        //

        while (EfiReadRegister32(&EfiRk32ProcessorJumpAddress) != (UINTN)NULL) {
            NOTHING;
        }
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

