/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    archsupc.c

Abstract:

    This module implements ARMv7 processor architecture features.

Author:

    Chris Stevens 19-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "firmware.h"
#include "bootlib.h"
#include "paging.h"
#include "loader.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the warmup stall duration if using firmware services.
//

#define ARM_FIRMWARE_WARMUP_STALL_DURATION \
    (ARM_FIRMWARE_MEASURING_STALL_DURATION >> 2)

//
// Define the total stall duration when using firmware services.
//

#define ARM_FIRMWARE_MEASURING_STALL_DURATION 125000
#define ARM_FIRMWARE_MEASURING_STALL_FACTOR 8

//
// Define the minimum realistic frequency one can expect from a machine. If
// the measurement appears to be below this then either the cycle counter is
// not ticking or the stall returned immediately. This tick count corresponds
// to about 50MHz. Anything below that and it's assumed to be wrong. Remember
// that the cycle counter is initialized to tick every 64th instruction.
//

#define ARM_FIRMWARE_MINIMUM_TICK_DELTA \
    ((50000000 / 64) / ARM_FIRMWARE_MEASURING_STALL_FACTOR)

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// Architecture assembly routine being borrowed from other parts of the loader.
//

VOID
BoCpuid (
    PARM_CPUID Features
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
BoArchMapNeededHardwareRegions (
    VOID
    )

/*++

Routine Description:

    This routine maps architecture-specific pieces of hardware needed for very
    early kernel initialization.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    ARM_CPUID CpuInformation;
    PVOID LoaderVectors;
    KSTATUS Status;
    ULONG SystemControl;
    PHYSICAL_ADDRESS VectorsPhysical;
    PVOID VectorsVirtual;

    SystemControl = ArGetSystemControlRegister();
    BoCpuid(&CpuInformation);

    //
    // If VBAR is not supported, then the fixed vector address will need to be
    // mapped.
    //

    if ((CpuInformation.ProcessorFeatures[1] &
         CPUID_PROCESSOR1_SECURITY_EXTENSION_MASK) ==
        CPUID_PROCESSOR1_SECURITY_EXTENSION_UNSUPPORTED) {

        //
        // Allocate a page for it.
        //

        Status = FwAllocatePages(&VectorsPhysical,
                                 PAGE_SIZE,
                                 PAGE_SIZE,
                                 MemoryTypeLoaderPermanent);

        if (!KSUCCESS(Status)) {
            goto ArchMapNeededHardwareRegionsEnd;
        }

        //
        // Map the page to the high vectors.
        //

        VectorsVirtual = (PVOID)EXCEPTION_VECTOR_ADDRESS;
        Status = BoMapPhysicalAddress(&VectorsVirtual,
                                      VectorsPhysical,
                                      PAGE_SIZE,
                                      MAP_FLAG_GLOBAL | MAP_FLAG_EXECUTE,
                                      MemoryTypeReserved);

        //
        // Copy the current exception vectors over to allow debugging to
        // continue after paging has been enabled in the loader.
        //

        if ((SystemControl & MMU_HIGH_EXCEPTION_VECTORS) != 0) {
            RtlCopyMemory((PVOID)(UINTN)VectorsPhysical,
                          (PVOID)EXCEPTION_VECTOR_ADDRESS,
                          sizeof(ARM_INTERRUPT_TABLE));

        } else {
            RtlCopyMemory((PVOID)(UINTN)VectorsPhysical,
                          (PVOID)EXCEPTION_VECTOR_LOW_ADDRESS,
                          sizeof(ARM_INTERRUPT_TABLE));

            //
            // Also temporarily map the loader vectors.
            //

            LoaderVectors = (PVOID)(UINTN)EXCEPTION_VECTOR_LOW_ADDRESS;
            Status = BoMapPhysicalAddress(&LoaderVectors,
                                          EXCEPTION_VECTOR_LOW_ADDRESS,
                                          PAGE_SIZE,
                                          MAP_FLAG_EXECUTE,
                                          MemoryTypeLoaderTemporary);

            if (!KSUCCESS(Status)) {
                goto ArchMapNeededHardwareRegionsEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

ArchMapNeededHardwareRegionsEnd:
    return Status;
}

VOID
BoArchMeasureCycleCounter (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine attempts to measure the processor cycle counter.

Arguments:

    Parameters - Supplies a pointer to the kernel initialization block.

Return Value:

    None. The cycle counter frequency (or zero on failure) will be placed in
    the parameter block.

--*/

{

    ULONG Begin;
    ULONG Control;
    ULONG Delta;
    ULONG Enable;
    ULONG End;
    ULONGLONG Frequency;
    KSTATUS Status;

    Frequency = 0;

    //
    // Carefully enable the cycle count register. It's possible the firmware
    // is already using it. If that's the case don't change the divide by 64
    // bit.
    //

    Control = ArGetPerformanceControlRegister();

    //
    // If the firmware has not enabled the cycle counter, then enable it and
    // set the divisor.
    //

    if ((Control & PERF_CONTROL_ENABLE) == 0) {
        Control |= PERF_CONTROL_ENABLE | PERF_CONTROL_CYCLE_COUNT_DIVIDE_64;
        ArSetPerformanceControlRegister(Control);
        ArSetPerformanceCounterEnableRegister(PERF_MONITOR_CYCLE_COUNTER);

    //
    // The firmware has enabled the performance counters. Look to see if the
    // cycle counter is enabled.
    //

    } else {
        Enable = ArGetPerformanceCounterEnableRegister();

        //
        // If the firmware is using the performance counters but not the cycle
        // counter, try to enable it.
        //

        if ((Enable & PERF_MONITOR_CYCLE_COUNTER) == 0) {
            Control |= PERF_CONTROL_CYCLE_COUNT_DIVIDE_64;
            ArSetPerformanceControlRegister(Control);
            Enable |= PERF_MONITOR_CYCLE_COUNTER;
            ArSetPerformanceCounterEnableRegister(Enable);
        }
    }

    //
    // Read the enable register to see if it's fired up.
    //

    Enable = ArGetPerformanceCounterEnableRegister();
    if ((Enable & PERF_MONITOR_CYCLE_COUNTER) == 0) {
        goto ArchMeasureCycleCounterEnd;
    }

    //
    // Get the tubes warm with a practice read.
    //

    ArGetCycleCountRegister();
    Status = FwStall(ARM_FIRMWARE_WARMUP_STALL_DURATION);
    if (!KSUCCESS(Status)) {
        goto ArchMeasureCycleCounterEnd;
    }

    //
    // Perform the real stall.
    //

    Begin = ArGetCycleCountRegister();
    Status = FwStall(ARM_FIRMWARE_MEASURING_STALL_DURATION);
    if (!KSUCCESS(Status)) {
        goto ArchMeasureCycleCounterEnd;
    }

    End = ArGetCycleCountRegister();
    Delta = End - Begin;

    //
    // If the divide by 64 bit is not enabled, then the delta was 64 times as
    // fast as it will be when the cycle counter is initialized for real.
    // Adjust that here.
    //

    Control = ArGetPerformanceControlRegister();
    if ((Control & PERF_CONTROL_CYCLE_COUNT_DIVIDE_64) == 0) {
        Delta /= 64;
    }

    //
    // If the tick count is too small, then the firmware probably returned
    // immediately without actually stalling. Throw away the result.
    //

    if (Delta < ARM_FIRMWARE_MINIMUM_TICK_DELTA) {
        Status = STATUS_NOT_SUPPORTED;
        goto ArchMeasureCycleCounterEnd;
    }

    Frequency = (ULONGLONG)Delta * ARM_FIRMWARE_MEASURING_STALL_FACTOR;
    Status = STATUS_SUCCESS;

ArchMeasureCycleCounterEnd:
    Parameters->CycleCounterFrequency = Frequency;
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

