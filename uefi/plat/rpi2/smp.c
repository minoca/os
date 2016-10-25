/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    smp.c

Abstract:

    This module implements support routines for the application processors on
    BCM2836 SoCs.

Author:

    Chris Stevens 19-Apr-2015

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/b2709os.h>
#include <uefifw.h>
#include "rpi2fw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro sets the jump address for the given CPU.
//

#define BCM2836_SET_CPU_JUMP_ADDRESS(_CpuId, _JumpAddress)                    \
    EfiWriteRegister32(                                                       \
                     (VOID *)BCM2836_CPU_0_MAILBOX_3_SET + ((_CpuId) * 0x10), \
                     (_JumpAddress))

//
// This macros reads and clears a core's jump address to check to see that is
// has come to life.
//

#define BCM2836_READ_CPU_JUMP_ADDRESS(_CpuId)                                 \
    EfiReadRegister32((VOID *)BCM2836_CPU_0_MAILBOX_3_CLEAR +                 \
                     ((_CpuId) * 0x10))

//
// This macro enables IRQs on a particular core.
//

#define BCM2836_CPU_ENABLE_IRQS(_CpuId)                                  \
    EfiWriteRegister32((VOID *)BCM2836_CPU_0_MAILBOX_INTERRUPT_CONTROL + \
                       ((_CpuId) * 0x4),                                 \
                       0x1)

//
// ---------------------------------------------------------------- Definitions
//

#define BCM2836_CPU_COUNT 4
#define BCM2836_CPU_0_PARKED_ADDRESS 0x01FFA000
#define BCM2836_CPU_1_PARKED_ADDRESS 0x01FFB000
#define BCM2836_CPU_2_PARKED_ADDRESS 0x01FFC000
#define BCM2836_CPU_3_PARKED_ADDRESS 0x01FFD000
#define BCM2836_CPU_PARKED_ADDRESS_SIZE 0x1000
#define BCM2836_CPU_TOTAL_PARKED_ADDRESS_SIZE \
    (BCM2836_CPU_COUNT * BCM2836_CPU_PARKED_ADDRESS_SIZE)

#define ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET 0x0800

//
// Define which bits of the MPIDR are valid processor ID bits.
//

#define ARM_PROCESSOR_ID_MASK 0x00FFFFFF

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipBcm2836ProcessorStartup (
    VOID
    );

UINT32
EfipBcm2836GetMultiprocessorIdRegister (
    VOID
    );

VOID
EfipBcm2836SendEvent (
    VOID
    );

EFI_STATUS
EfipBcm2836UpdateAcpi (
    UINT32 ProcessorIdBase
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the variables other cores read to boot.
//

volatile UINT32 EfiBcm2836ProcessorId;
VOID volatile *EfiBcm2836JumpAddress;

extern UINT8 EfipBcm2836ParkingLoop;
extern UINT8 EfipBcm2836ParkingLoopEnd;

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2836SmpInitialize (
    UINT32 Phase
    )

/*++

Routine Description:

    This routine initializes and parks the application processors on the
    BCM2836.

Arguments:

    Phase - Supplies the iteration number this routine is being called on.
        Phase zero occurs very early, just after the debugger comes up.
        Phase one occurs a bit later, after timer, interrupt services, and the
        memory core are initialized.

Return Value:

    EFI status code.

--*/

{

    VOID *Cpu[BCM2836_CPU_COUNT];
    UINT32 IdBase;
    UINT32 Index;
    UINTN Pages;
    EFI_PHYSICAL_ADDRESS ParkedAddress;
    UINTN ParkingLoopSize;
    EFI_STATUS Status;

    //
    // Get the MPIDR of the current core to determine the base CPU ID.
    //

    IdBase = EfipBcm2836GetMultiprocessorIdRegister() & ARM_PROCESSOR_ID_MASK;

    //
    // Phase 0 initializes all of the cores and then parks the non-boot cores.
    // They are currently parked within page zero, but UEFI memory
    // initialization zeroes that page in order to reclaim it. As a result, the
    // cores need to be parked elsewhere before being parked at the final
    // destination.
    //

    if (Phase == 0) {

        //
        // Enable IRQs on all cores.
        //

        for (Index = 0; Index < BCM2836_CPU_COUNT; Index += 1) {
            BCM2836_CPU_ENABLE_IRQS(Index);
        }

        //
        // Park the application cores to the first space.
        //

        for (Index = 1; Index < BCM2836_CPU_COUNT; Index += 1) {
            EfiBcm2836ProcessorId = IdBase + Index;

            //
            // Poke the CPU to fire it up.
            //

            BCM2836_SET_CPU_JUMP_ADDRESS(Index,
                                         (UINTN)EfipBcm2836ProcessorStartup);

            //
            // Wait for the CPU to come to life.
            //

            while (BCM2836_READ_CPU_JUMP_ADDRESS(Index) != (UINTN)NULL) {
                NOTHING;
            }

            //
            // Wait for the processor ID to be cleared. For some reason marking
            // the variable as volatile doesn't seem to prevent optimization.
            //

            while (EfiReadRegister32((UINT32 *)&EfiBcm2836ProcessorId) != 0) {
                NOTHING;
            }
        }

    //
    // Phase 1 moves the application processors to their final parking
    // location in allocated memory. These parking locations are then passed
    // along to higher level systems via ACPI.
    //

    } else if (Phase == 1) {

        //
        // Allocate the pages for the firmware parked spaces.
        //

        Pages = EFI_SIZE_TO_PAGES(BCM2836_CPU_TOTAL_PARKED_ADDRESS_SIZE);
        ParkedAddress = BCM2836_CPU_0_PARKED_ADDRESS;
        Status = EfiAllocatePages(AllocateAddress,
                                  EfiACPIMemoryNVS,
                                  Pages,
                                  &ParkedAddress);

        if (EFI_ERROR(Status)) {
            return Status;
        }

        EfiSetMem((VOID *)(UINTN)ParkedAddress,
                  BCM2836_CPU_TOTAL_PARKED_ADDRESS_SIZE,
                  0);

        //
        // Initialize the parked address for each CPU, write -1 to the
        // processor number location, and copy the parking protocol loop into
        // the place for each CPU.
        //

        ParkingLoopSize = (UINTN)&EfipBcm2836ParkingLoopEnd -
                          (UINTN)&EfipBcm2836ParkingLoop;

        for (Index = 0; Index < BCM2836_CPU_COUNT; Index += 1) {
            Cpu[Index] = (VOID *)((UINTN)ParkedAddress +
                                  (BCM2836_CPU_PARKED_ADDRESS_SIZE * Index));

            *((UINT32 *)Cpu[Index]) = -1;
            EfiCopyMem(Cpu[Index] + ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET,
                       &EfipBcm2836ParkingLoop,
                       ParkingLoopSize);
        }

        EfiCoreInvalidateInstructionCacheRange(
                                        (VOID *)(UINTN)ParkedAddress,
                                        BCM2836_CPU_TOTAL_PARKED_ADDRESS_SIZE);

        //
        // Park each of the application cores.
        //

        for (Index = 1; Index < BCM2836_CPU_COUNT; Index += 1) {
            EfiBcm2836JumpAddress = Cpu[Index] +
                                    ARM_PARKING_PROTOCOL_FIRMWARE_OFFSET;

            EfiBcm2836ProcessorId = IdBase + Index;

            //
            // Send an event to the cores, only the one with the matching ID
            // should proceed.
            //

            EfipBcm2836SendEvent();

            //
            // Make sure the core moves on.
            //

            while (EfiReadRegister32((UINT32 *)&EfiBcm2836JumpAddress) !=
                   (UINT32)NULL) {

                NOTHING;
            }
        }

    } else {
        Status = EfipBcm2836UpdateAcpi(IdBase);
        if (EFI_ERROR(Status)) {
            return Status;
        }
    }

    return EFI_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

EFI_STATUS
EfipBcm2836UpdateAcpi (
    UINT32 ProcessorIdBase
    )

/*++

Routine Description:

    This routine updates the BCM2 ACPI table with the current platform's SMP
    information.

Arguments:

    ProcessorIdBase - Supplies the base ID for the BCM2836's ARM cores.

Return Value:

    EFI status code.

--*/

{

    PBCM2709_CPU_ENTRY CpuEntry;
    PBCM2709_GENERIC_ENTRY CurrentEntry;
    UINT32 ProcessorCount;
    EFI_STATUS Status;
    PBCM2709_TABLE Table;

    Table = EfiGetAcpiTable(BCM2709_SIGNATURE, NULL);
    if (Table == NULL) {
        Status = EFI_NOT_FOUND;
        goto UpdateAcpiEnd;
    }

    //
    // Update the processor ID for each CPU entry in the table. Different
    // BCM2836 devices have different sets of MPIDR values.
    //

    ProcessorCount = 0;
    CurrentEntry = (PBCM2709_GENERIC_ENTRY)(Table + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)Table + Table->Header.Length)) {

        if ((CurrentEntry->Type == Bcm2709EntryTypeCpu) &&
            (CurrentEntry->Length == sizeof(BCM2709_CPU_ENTRY))) {

            CpuEntry = (PBCM2709_CPU_ENTRY)CurrentEntry;
            CpuEntry->ProcessorId = ProcessorIdBase + ProcessorCount;
            ProcessorCount += 1;
            if (ProcessorCount == BCM2836_CPU_COUNT) {
                break;
            }
        }

        CurrentEntry = (PBCM2709_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                                CurrentEntry->Length);
    }

    //
    // Now that the table has been modified, recompute the checksum.
    //

    EfiAcpiChecksumTable(Table,
                         Table->Header.Length,
                         OFFSET_OF(DESCRIPTION_HEADER, Checksum));

    Status = EFI_SUCCESS;

UpdateAcpiEnd:
    return Status;
}

