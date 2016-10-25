/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smp.c

Abstract:

    This module implements support routines for the second core on OMAP4 SoCs.

Author:

    Evan Green 31-Mar-2013

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "pandafw.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_CPU0_PARKED_ADDRESS 0x81FFA000
#define OMAP4_CPU1_PARKED_ADDRESS 0x81FFB000
#define OMAP4_CPU_PARKED_ADDRESS_SIZE 0x1000
#define OMAP4_CPU_TOTAL_PARKED_ADDRESS_SIZE (2 * OMAP4_CPU_PARKED_ADDRESS_SIZE)

#define ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET 0x0800

//
// This SMC command modifies AuxCoreBoot 0.
//

#define OMAP4_SMC_COMMAND_MODIFY_AUX_CORE_BOOT_0 0x104

//
// This SMC command writes to AuxCoreBoot 1.
//

#define OMAP4_SMC_COMMAND_WRITE_AUX_CORE_BOOT_1 0x105

//
// This SMC command writes to the Power Status register in the SCU (Snoop
// Control Unit).
//

#define OMAP4_SMC_COMMAND_SET_SCU_POWER_STATUS 0x108

//
// Define the value to write into AuxCoreBoot 0 to start the CPU.
//

#define OMAP4_AUX_CORE_BOOT_0_START 0x00000200

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Internal assembly routines.
//

UINT32
EfipOmap4SmcCommand (
    UINT32 Argument1,
    UINT32 Argument2,
    UINT32 Command
    );

VOID
EfipOmap4ProcessorStartup (
    VOID
    );

VOID
EfipOmap4SendEvent (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the variables other cores read to boot.
//

volatile UINT32 EfiOmap4ProcessorId;
VOID volatile *EfiOmap4ProcessorJumpAddress;

extern UINT8 EfipOmap4ParkingLoop;
extern UINT8 EfipOmap4ParkingLoopEnd;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipSmpInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes and parks the second core on the OMAP4.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

{

    VOID *Cpu0;
    VOID *Cpu1;
    EFI_PHYSICAL_ADDRESS ParkedAddress;
    EFI_STATUS Status;

    //
    // Allocate the pages for the firmware parked spaces.
    //

    ParkedAddress = OMAP4_CPU0_PARKED_ADDRESS;
    Status = EfiAllocatePages(
                        AllocateAddress,
                        EfiACPIMemoryNVS,
                        EFI_SIZE_TO_PAGES(OMAP4_CPU_TOTAL_PARKED_ADDRESS_SIZE),
                        &ParkedAddress);

    if (EFI_ERROR(Status)) {
        return Status;
    }

    EfiSetMem((VOID *)(UINTN)ParkedAddress,
              OMAP4_CPU_TOTAL_PARKED_ADDRESS_SIZE,
              0);

    Cpu0 = (VOID *)(UINTN)ParkedAddress;
    Cpu1 = Cpu0 + OMAP4_CPU_PARKED_ADDRESS_SIZE;

    //
    // Write -1 to the processor number location.
    //

    *((UINT32 *)Cpu0) = -1;
    *((UINT32 *)Cpu1) = -1;

    //
    // Copy the parking protocol loops into the right places.
    //

    EfiCopyMem(Cpu0 + ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET,
               &EfipOmap4ParkingLoop,
               (UINTN)&EfipOmap4ParkingLoopEnd - (UINTN)&EfipOmap4ParkingLoop);

    EfiCopyMem(Cpu1 + ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET,
               &EfipOmap4ParkingLoop,
               (UINTN)&EfipOmap4ParkingLoopEnd - (UINTN)&EfipOmap4ParkingLoop);

    EfiCoreInvalidateInstructionCacheRange(Cpu0,
                                           OMAP4_CPU_TOTAL_PARKED_ADDRESS_SIZE);

    EfiOmap4ProcessorId = 1;
    EfiOmap4ProcessorJumpAddress = Cpu1 + ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET;

    //
    // Set AuxCoreBoot 1 to the physical address that it should jump to.
    //

    EfipOmap4SmcCommand((UINTN)EfipOmap4ProcessorStartup,
                        0,
                        OMAP4_SMC_COMMAND_WRITE_AUX_CORE_BOOT_1);

    //
    // Set AuxCoreBoot 0 to tell the CPU that it really should jump.
    //

    EfipOmap4SmcCommand(OMAP4_AUX_CORE_BOOT_0_START,
                        ~OMAP4_AUX_CORE_BOOT_0_START,
                        OMAP4_SMC_COMMAND_MODIFY_AUX_CORE_BOOT_0);

    //
    // Send an event to core 1.
    //

    EfipOmap4SendEvent();

    //
    // Wait for the second CPU to come to life. For some reason marking the
    // variable as volatile doesn't seem to prevent optimization.
    //

    while (EfiReadRegister32(&EfiOmap4ProcessorJumpAddress) != (UINTN)NULL) {
        NOTHING;
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

