/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    b2709int.c

Abstract:

    This module implements support for the BCM2709 interrupt controller.

Author:

    Chris Stevens 24-Mar-2014

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Include kernel.h, but be cautious about which APIs are used. Most of the
// system depends on the hardware modules. Limit use to HL, RTL and AR routines.
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>
#include "bcm2709.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the flags for the basic interrupts.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_TIMER            0x00000001
#define BCM2709_INTERRUPT_IRQ_BASIC_MAILBOX          0x00000002
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL0        0x00000004
#define BCM2709_INTERRUPT_IRQ_BASIC_DOORBELL1        0x00000008
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU0_HALTED      0x00000010
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU1_HALTED      0x00000020
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_1 0x00000040
#define BCM2709_INTERRUPT_IRQ_BASIC_ILLEGAL_ACCESS_0 0x00000080

#define BCM2709_INTERRUPT_IRQ_BASIC_MASK             0x000000FF

//
// Define the flags for the GPU interrupts included in the basic pending status
// register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_7            0x00000400
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_9            0x00000800
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_10           0x00001000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_18           0x00002000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_19           0x00004000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_53           0x00008000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_54           0x00010000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_55           0x00020000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_56           0x00040000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_57           0x00080000
#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_62           0x00100000

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK         0x001FFC00

//
// Define the number of bits to shift in order to get to the GPU bits in the
// basic pending register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT 10

//
// Define the number of GPU registers whose pending status is expressed in the
// basic pending status register.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT 11

//
// Define the flags that signify that one of the normal pending status
// registers has a pending interrupt.
//

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1        0x00000100
#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_2        0x00000200

#define BCM2709_INTERRUPT_IRQ_BASIC_PENDING_MASK     0x00000300

//
// Define the masks for GPU interrupt bits that are served by the basic
// interrupt register.
//

#define BCM2709_INTERRUPT_IRQ1_BASIC_MASK 0x000C0680
#define BCM2709_INTERRUPT_IRQ2_BASIC_MASK 0x43E00000

//
// Define the number of GPU interrupt lines on the BCM2709.
//

#define BCM2709_INTERRUPT_GPU_LINE_COUNT 64

//
// Define the bits for the CPU local mailbox interrupt control registers.
//

#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_FIQ_3_ENABLE 0x00000080
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_FIQ_2_ENABLE 0x00000040
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_FIQ_1_ENABLE 0x00000020
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_FIQ_0_ENABLE 0x00000010
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_IRQ_3_ENABLE 0x00000008
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_IRQ_2_ENABLE 0x00000004
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_IRQ_1_ENABLE 0x00000002
#define BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_IRQ_0_ENABLE 0x00000001

//
// Define the status bitmask for the pending IRQ local register.
//

#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_SECURE     0x00000001
#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_NON_SECURE 0x00000002
#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_HYPERVISOR 0x00000004
#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_VIRTUAL    0x00000008
#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_IPI           0x00000010
#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_GPU           0x00000100

#define BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CORE_TIMER_MASK   \
    (BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_SECURE |     \
     BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_NON_SECURE | \
     BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_HYPERVISOR | \
     BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CT_VIRTUAL)

//
// Define the number of software lines.
//

#define BCM2709_INTERRUPT_SOFTWARE_LINE_COUNT 32

//
// Define the base for the software lines.
//

#define BCM2709_INTERRUPT_SOFTWARE_LINE_BASE     \
    (Bcm2709InterruptHardwareLineCount +         \
     BCM2709_INTERRUPT_PER_PROCESSOR_LINE_COUNT)

//
// Define the number of per-processor interrupt lines.
//

#define BCM2709_INTERRUPT_PER_PROCESSOR_LINE_COUNT 32

//
// Define the base for the per-processor interrupt lines.
//

#define BCM2709_INTERRUPT_PER_PROCESSOR_LINE_BASE \
    Bcm2709InterruptHardwareLineCount

//
// Define the total number of interrupt lines.
//

#define BCM2709_INTERRUPT_MAX_LINE_COUNT         \
    (Bcm2709InterruptHardwareLineCount +         \
     BCM2709_INTERRUPT_SOFTWARE_LINE_COUNT +     \
     BCM2709_INTERRUPT_PER_PROCESSOR_LINE_COUNT)

//
// Define the number of soft priorities implemented in the interrrupt
// controller.
//

#define BCM2709_INTERRUPT_PRIORITY_COUNT 16

//
// Define which bits of the MPIDR are valid processor ID bits for the local
// BCM2709 interrupt controller.
//

#define BCM2709_INTERRUPT_PROCESSOR_ID_MASK 0x000000FF

//
// --------------------------------------------------------------------- Macros
//

//
// This macro translates a register and a processor ID into a local register
// address. The processors' local registers are 4 bytes apart.
//

#define GET_LOCAL_ADDRESS(_Register, _ProcessorId) \
    (HlBcm2709LocalBase + ((_Register) + (0x4 * (_ProcessorId))))

//
// This macro translates a register and a processor ID into an IPI register
// address. The processors' IPI registers are 16 bytes apart.
//

#define GET_IPI_ADDRESS(_Register, _ProcessorId) \
    (HlBcm2709LocalBase + ((_Register) + (0x10 * (_ProcessorId))))

//
// This macro reads from the BCM2709 interrupt controller. The parameter should
// be a BCM2709_INTERRUPT_REGISTER value.
//

#define READ_INTERRUPT_REGISTER(_Register) \
    HlReadRegister32(HlBcm2709InterruptController + (_Register))

//
// This macro writes to the BCM2709 interrupt controller. _Register should be a
// BCM2709_INTERRUPT_REGISTER value and _Value should be a ULONG. This Broadcom
// interrupt controller appears to make posted writes. Perform a read of the
// same register to make sure the write sticks.
//

#define WRITE_INTERRUPT_REGISTER(_Register, _Value)                     \
    HlWriteRegister32(HlBcm2709InterruptController + (_Register), _Value); \
    HlReadRegister32(HlBcm2709InterruptController + (_Register));

//
// This macro reads from the BCM2709 local registers. _Register should be a
// BCM2709_LOCAL_REGISTER value and _ProcessorId should be the index of the
// processor whose register information is being requested.
//

#define READ_LOCAL_REGISTER(_Register, _ProcessorId) \
    HlReadRegister32(GET_LOCAL_ADDRESS(_Register, _ProcessorId))

//
// This macro writes to the BCM2709 interrupt controller. _Register should be a
// BCM2709_LOCAL_REGISTER value, _ProcessorId should be the index of the
// processor whose register is being written and _Value should be a ULONG. This
// Broadcom interrupt controller appears to make posted writes. Perform a read
// of the same register to make sure the write sticks.
//

#define WRITE_LOCAL_REGISTER(_Register, _ProcessorId, _Value)           \
    HlWriteRegister32(GET_LOCAL_ADDRESS(_Register, _ProcessorId), _Value); \
    HlReadRegister32(GET_LOCAL_ADDRESS(_Register, _ProcessorId));

//
// This macro reads from the BCM2709 local interrupt registers. _Register
// should be a BCM2836_LOCAL_REGISTER value and _ProcessorId should be the
// index of the processor whose register information is being requested.
//

#define READ_LOCAL_IPI_REGISTER(_Register, _ProcessorId) \
    HlReadRegister32(GET_IPI_ADDRESS(_Register, _ProcessorId))

//
// This macro writes to the BCM2709 local interrupt controller. _Register
// should be a BCM2709_LOCAL_REGISTER value, _ProcessorId should be the index
// of the processor whose register is being written, and _Value should be a
// ULONG. This Broadcom interrupt controller appears to make posted writes.
// Perform a read of the same register to make sure the write sticks.
//

#define WRITE_LOCAL_IPI_REGISTER(_Register, _ProcessorId, _Value)     \
    HlWriteRegister32(GET_IPI_ADDRESS(_Register, _ProcessorId), _Value); \
    HlReadRegister32(GET_IPI_ADDRESS(_Register, _ProcessorId));

//
// ------------------------------------------------------ Data Type Definitions
//

//
// Define the offsets to interrupt controller registers, in bytes.
//

typedef enum _BCM2709_INTERRUPT_REGISTER {
    Bcm2709InterruptIrqPendingBasic = 0x00,
    Bcm2709InterruptIrqPending1     = 0x04,
    Bcm2709InterruptIrqPending2     = 0x08,
    Bcm2709InterruptFiqControl      = 0x0C,
    Bcm2709InterruptIrqEnable1      = 0x10,
    Bcm2709InterruptIrqEnable2      = 0x14,
    Bcm2709InterruptIrqEnableBasic  = 0x18,
    Bcm2709InterruptIrqDisable1     = 0x1C,
    Bcm2709InterruptIrqDisable2     = 0x20,
    Bcm2709InterruptIrqDisableBasic = 0x24,
    Bcm2709InterruptSize            = 0x28
} BCM2709_INTERRUPT_REGISTER, *PBCM2709_INTERRUPT_REGISTER;

//
// Define the offsets to the BCM2709 local registers, in bytes.
//

typedef enum _BCM2836_LOCAL_REGISTER {
    Bcm2709LocalCoreTimerInterruptControl     = 0x40,
    Bcm2709LocalCoreTimerInterruptControlCpu0 = 0x40,
    Bcm2709LocalCoreTimerInterruptControlCpu1 = 0x44,
    Bcm2709LocalCoreTimerInterruptControlCpu2 = 0x48,
    Bcm2709LocalCoreTimerInterruptControlCpu4 = 0x4C,
    Bcm2709LocalMailboxInterruptControl       = 0x50,
    Bcm2709LocalMailboxInterruptControlCpu0   = 0x50,
    Bcm2709LocalMailboxInterruptControlCpu1   = 0x54,
    Bcm2709LocalMailboxInterruptControlCpu2   = 0x58,
    Bcm2709LocalMailboxInterruptControlCpu3   = 0x5C,
    Bcm2709LocalIrqPending                    = 0x60,
    Bcm2709LocalIrqPendingCpu0                = 0x60,
    Bcm2709LocalIrqPendingCpu1                = 0x64,
    Bcm2709LocalIrqPendingCpu2                = 0x68,
    Bcm2709LocalIrqPendingCpu3                = 0x6C,
    Bcm2709LocalFiqPending                    = 0x70,
    Bcm2709LocalFiqPendingCpu0                = 0x70,
    Bcm2709LocalFiqPendingCpu1                = 0x74,
    Bcm2709LocalFiqPendingCpu2                = 0x78,
    Bcm2709LocalFiqPendingCpu3                = 0x7C,
    Bcm2709LocalRequestIpi                    = 0x80,
    Bcm2709LocalRequestIpiCpu0                = 0x80,
    Bcm2709LocalRequestIpiCpu1                = 0x90,
    Bcm2709LocalRequestIpiCpu2                = 0xA0,
    Bcm2709LocalRequestIpiCpu3                = 0xB0,
    Bcm2709LocalIpiPending                    = 0xC0,
    Bcm2709LocalIpiPendingCpu0                = 0xC0,
    Bcm2709LocalIpiPendingCpu1                = 0xD0,
    Bcm2709LocalIpiPendingCpu2                = 0xE0,
    Bcm2709LocalIpiPendingCpu3                = 0xF0,
    Bcm2709LocalInterruptSize                 = 0x100
} BCM2709_LOCAL_REGISTER, *PBCM2709_LOCAL_REGISTER;

//
// Define the interrupt lines for the non GPU interrupts.
//

typedef enum _BCM2709_CPU_INTERRUPT_LINE {
    Bcm2709InterruptArmTimer          = 64,
    Bcm2709InterruptArmMailbox        = 65,
    Bcm2709InterruptArmDoorbell0      = 66,
    Bcm2709InterruptArmDoorbell1      = 67,
    Bcm2709InterruptGpu0Halted        = 68,
    Bcm2709InterruptGpu1Halted        = 69,
    Bcm2709InterruptIllegalAccess1    = 70,
    Bcm2709InterruptIllegalAccess0    = 71,
    Bcm2709InterruptHardwareLineCount = 96
} BCM2709_CPU_INTERRUPT_LINE, *PBCM2709_CPU_INTERRUPT_LINE;

//
// Define the interrupt lines for the per-processor interrupts.
//

typedef enum _BCM2709_PPI_INTERRUPT_LINE {
    Bcm2709PpiCoreTimerSecure     = 96,
    Bcm2709PpiCoreTimerNonSecure  = 97,
    Bcm2709PpiCoreTimerHypervisor = 98,
    Bcm2709PpiCoreTimerVirtual    = 99,
} BCM2709_PPI_INTERRUPT_LINE, *PBCM2709_PPI_INTERRUPT_LINE;

/*++

Structure Description:

    This structure defines an interrupt priority level.

Members:

    IrqMaskBasic - Stores the mask for all basic interrupts that operate at the
        priority level.

    IrqMask1 - Stores the mask for all register 1 interrupts that operate at
        the priority level.

    IrqMask2 - Stores the mask for all register 2 interrupts that operate at
        the priority level.

    IrqMaskPpi - Stores the mask for all PPIs that operate at the priority
        level.

    IrqMaskSgi - Stores the mask for all SGIs that operate at the priority
        level.

--*/

typedef struct _BCM2709_INTERRUPT_MASK {
    ULONG IrqMaskBasic;
    ULONG IrqMask1;
    ULONG IrqMask2;
    ULONG IrqMaskPpi;
    ULONG IrqMaskSgi;
} BCM2709_INTERRUPT_MASK, *PBCM2709_INTERRUPT_MASK;

/*++

Structure Description:

    This structure defines the processor state for the BCM2709 interrupt
    controller.

Members:

    CurrentPriority - Stores the current priority level of the interrupt that
        is being handled.

    PendingIpis - Stores a mask of processor local interrupts that arrived
        while the processor was dispatching another IPI of the same priority or
        greater.

--*/

typedef struct _BCM2709_INTERRUPT_PROCESSOR {
    UCHAR CurrentPriority;
    ULONG PendingIpis;
} BCM2709_INTERRUPT_PROCESSOR, *PBCM2709_INTERRUPT_PROCESSOR;

/*++

Structure Description:

    This structure defines the internal data for an BCM2709 interrupt
    controller.

Members:

    LinePriority - Stores the priority level for each interrupt line.

    Masks - Stores an array of interrupt masks for each priority level.

    EnabledMask - Stores the mask of interrupts enabled at any priority
        level.

    ProcessorCount - Stores the total number of processors goverened by this
        interrupt controller.

    Processor - Stores an array of interrupt data for each processor, including
        its current priority and pending IPIs.

--*/

typedef struct _BCM2709_INTERRUPT_CONTROLLER {
    UCHAR LinePriority[BCM2709_INTERRUPT_MAX_LINE_COUNT];
    BCM2709_INTERRUPT_MASK Masks[BCM2709_INTERRUPT_PRIORITY_COUNT];
    BCM2709_INTERRUPT_MASK EnabledMask;
    ULONG ProcessorCount;
    BCM2709_INTERRUPT_PROCESSOR Processor[ANYSIZE_ARRAY];
} BCM2709_INTERRUPT_CONTROLLER, *PBCM2709_INTERRUPT_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpBcm2709InterruptEnumerateProcessors (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    );

KSTATUS
HlpBcm2709InterruptInitializeLocalUnit (
    PVOID Context,
    PULONG Identifier
    );

KSTATUS
HlpBcm2709InterruptInitializeIoUnit (
    PVOID Context
    );

KSTATUS
HlpBcm2709InterruptSetLocalUnitAddressing (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    );

INTERRUPT_CAUSE
HlpBcm2709InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    );

VOID
HlpBcm2709InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    );

KSTATUS
HlpBcm2709InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    );

KSTATUS
HlpBcm2709InterruptStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    );

KSTATUS
HlpBcm2709InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    );

VOID
HlpBcm2709InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    );

KSTATUS
HlpBcm2709InitializeController (
    PBCM2709_INTERRUPT_CONTROLLER Controller
    );

KSTATUS
HlpBcm2709InterruptDescribeLines (
    PBCM2709_INTERRUPT_CONTROLLER Controller
    );
//
// -------------------------------------------------------------------- Globals
//

//
// Store the virtual address of the mapped interrupt controller.
//

PVOID HlBcm2709InterruptController = NULL;
PVOID HlBcm2709LocalBase = NULL;

//
// Store a pointer to the BCM2709 ACPI Table.
//

PBCM2709_TABLE HlBcm2709Table = NULL;

//
// Store a table that tracks which GPU IRQs are in the basic pending status
// register.
//

ULONG
HlBcm2709InterruptIrqBasicGpuTable[BCM2709_INTERRUPT_IRQ_BASIC_GPU_COUNT] = {
    7,
    9,
    10,
    18,
    19,
    53,
    54,
    55,
    56,
    57,
    62
};

//
// Define the interrupt function table template.
//

INTERRUPT_FUNCTION_TABLE HlBcm2709InterruptFunctionTable = {
    HlpBcm2709InterruptInitializeIoUnit,
    HlpBcm2709InterruptSetLineState,
    HlpBcm2709InterruptMaskLine,
    HlpBcm2709InterruptBegin,
    NULL,
    HlpBcm2709InterruptEndOfInterrupt,
    HlpBcm2709InterruptRequestInterrupt,
    HlpBcm2709InterruptEnumerateProcessors,
    HlpBcm2709InterruptInitializeLocalUnit,
    HlpBcm2709InterruptSetLocalUnitAddressing,
    HlpBcm2709InterruptStartProcessor,
    NULL,
    NULL,
    NULL
};

//
// ------------------------------------------------------------------ Functions
//

VOID
HlpBcm2709InterruptModuleEntry (
    VOID
    )

/*++

Routine Description:

    This routine is the entry point for the APIC hardware module. Its role is to
    detect and report the prescense of an APIC.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG AllocationSize;
    PBCM2709_INTERRUPT_CONTROLLER Context;
    PBCM2709_GENERIC_ENTRY CurrentEntry;
    ULONG Index;
    INTERRUPT_CONTROLLER_DESCRIPTION NewController;
    ULONG ProcessorCount;
    KSTATUS Status;

    HlBcm2709Table = HlGetAcpiTable(BCM2709_SIGNATURE, NULL);
    if (HlBcm2709Table == NULL) {
        goto Bcm2709InterruptModuleEntryEnd;
    }

    //
    // Loop through every entry in the BCM2709 table once to determine the
    // number of processors in the system. If no CPU entries are found, then
    // there is actually just one processor.
    //

    ProcessorCount = 0;
    CurrentEntry = (PBCM2709_GENERIC_ENTRY)(HlBcm2709Table + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)HlBcm2709Table + HlBcm2709Table->Header.Length)) {

        if ((CurrentEntry->Type == Bcm2709EntryTypeCpu) &&
            (CurrentEntry->Length == sizeof(BCM2709_CPU_ENTRY))) {

            ProcessorCount += 1;
        }

        CurrentEntry = (PBCM2709_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                                CurrentEntry->Length);
    }

    //
    // Allocate the interrupt controller context.
    //

    AllocationSize = sizeof(BCM2709_INTERRUPT_CONTROLLER);
    if (ProcessorCount > 1) {
        AllocationSize += sizeof(BCM2709_INTERRUPT_PROCESSOR) *
                          (ProcessorCount - 1);
    }

    Context = HlAllocateMemory(AllocationSize,
                               BCM2709_ALLOCATION_TAG,
                               FALSE,
                               NULL);

    if (Context == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto Bcm2709InterruptModuleEntryEnd;
    }

    RtlZeroMemory(Context, sizeof(BCM2709_INTERRUPT_CONTROLLER));
    for (Index = 0; Index < BCM2709_INTERRUPT_MAX_LINE_COUNT; Index += 1) {
        Context->LinePriority[Index] = BCM2709_INTERRUPT_PRIORITY_COUNT;
    }

    //
    // If there is only 1 processor, then the controller needs to be reported
    // with a processor count of 0, but recall locally that there is at least
    // one processor.
    //

    if (ProcessorCount == 0) {
        Context->ProcessorCount = 1;

    } else {
        Context->ProcessorCount = ProcessorCount;
    }

    //
    // Zero out the controller description.
    //

    RtlZeroMemory(&NewController, sizeof(INTERRUPT_CONTROLLER_DESCRIPTION));
    NewController.TableVersion = INTERRUPT_CONTROLLER_DESCRIPTION_VERSION;
    RtlCopyMemory(&(NewController.FunctionTable),
                  &HlBcm2709InterruptFunctionTable,
                  sizeof(INTERRUPT_FUNCTION_TABLE));

    //
    // If there is only one processor, do not report the multi-processor
    // functions.
    //

    if (ProcessorCount == 0) {
        NewController.FunctionTable.EnumerateProcessors = NULL;
        NewController.FunctionTable.InitializeLocalUnit = NULL;
        NewController.FunctionTable.SetLocalUnitAddressing = NULL;
        NewController.FunctionTable.StartProcessor = NULL;
    }

    NewController.Context = Context;
    NewController.Identifier = 0;
    NewController.ProcessorCount = ProcessorCount;
    NewController.PriorityCount = BCM2709_INTERRUPT_PRIORITY_COUNT;

    //
    // Register the controller with the system.
    //

    Status = HlRegisterHardware(HardwareModuleInterruptController,
                                &NewController);

    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptModuleEntryEnd;
    }

Bcm2709InterruptModuleEntryEnd:
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpBcm2709InterruptEnumerateProcessors (
    PVOID Context,
    PPROCESSOR_DESCRIPTION Descriptions,
    ULONG DescriptionsBufferSize
    )

/*++

Routine Description:

    This routine describes all processors.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Descriptions - Supplies a pointer to a buffer of an array of processor
        descriptions that the hardware module is expected to fill out on
        success. The number of entries in the array is the number of processors
        reported during the interrupt controller registration.

    DescriptionsBufferSize - Supplies the size of the buffer passed. The
        hardware module should fail the routine if the buffer size is smaller
        than expected, but should not fail if the buffer size is larger than
        expected.

Return Value:

    STATUS_SUCCESS on success. The Descriptions buffer will contain
        descriptions of all processors under the jurisdiction of the given
        interrupt controller.

    Other status codes on failure. The contents of the Descriptions buffer is
        undefined.

--*/

{

    PBCM2709_CPU_ENTRY CpuEntry;
    PBCM2709_GENERIC_ENTRY CurrentEntry;
    PPROCESSOR_DESCRIPTION CurrentProcessor;
    ULONG ProcessorCount;
    ULONG ProcessorId;
    KSTATUS Status;

    if (HlBcm2709Table == NULL) {
        Status = STATUS_NOT_INITIALIZED;
        goto Bcm2709InterruptEnumerateProcessorsEnd;
    }

    //
    // Loop through every entry in the BCM2709 table looking for CPU interfaces.
    //

    ProcessorCount = 0;
    CurrentProcessor = Descriptions;
    CurrentEntry = (PBCM2709_GENERIC_ENTRY)(HlBcm2709Table + 1);
    while ((UINTN)CurrentEntry <
           ((UINTN)HlBcm2709Table + HlBcm2709Table->Header.Length)) {

        if ((CurrentEntry->Type == Bcm2709EntryTypeCpu) &&
            (CurrentEntry->Length == sizeof(BCM2709_CPU_ENTRY))) {

            CpuEntry = (PBCM2709_CPU_ENTRY)CurrentEntry;
            ProcessorCount += 1;

            //
            // Fail if the buffer is not big enough for this processor.
            //

            if (sizeof(PROCESSOR_DESCRIPTION) * ProcessorCount >
                DescriptionsBufferSize) {

                Status = STATUS_BUFFER_TOO_SMALL;
                goto Bcm2709InterruptEnumerateProcessorsEnd;
            }

            CurrentProcessor->Version = PROCESSOR_DESCRIPTION_VERSION;
            ProcessorId = CpuEntry->ProcessorId;
            CurrentProcessor->PhysicalId = ProcessorId;
            CurrentProcessor->LogicalFlatId =
                      1 << (ProcessorId & BCM2709_INTERRUPT_PROCESSOR_ID_MASK);

            CurrentProcessor->FirmwareIdentifier = CpuEntry->ProcessorId;
            CurrentProcessor->Flags = 0;
            if ((CpuEntry->Flags & BCM2709_CPU_FLAG_ENABLED) != 0) {
                CurrentProcessor->Flags |= PROCESSOR_DESCRIPTION_FLAG_PRESENT;
            }

            CurrentProcessor->ParkedPhysicalAddress = CpuEntry->ParkedAddress;
            CurrentProcessor += 1;
        }

        CurrentEntry = (PBCM2709_GENERIC_ENTRY)((PUCHAR)CurrentEntry +
                                                CurrentEntry->Length);
    }

    Status = STATUS_SUCCESS;

Bcm2709InterruptEnumerateProcessorsEnd:
    return Status;
}

KSTATUS
HlpBcm2709InterruptInitializeLocalUnit (
    PVOID Context,
    PULONG Identifier
    )

/*++

Routine Description:

    This routine initializes the local unit of an interrupt controller. It is
    always called on the processor of the local unit to initialize.

Arguments:

    Context - Supplies the pointer to the context of the I/O unit that owns
        this processor, provided by the hardware module upon initialization.

    Identifier - Supplies a pointer where this function will return the
        identifier of the processor being initialized.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG ProcessorId;
    KSTATUS Status;

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    if (HlBcm2709InterruptController == NULL) {
        Status = HlpBcm2709InitializeController(Controller);
        if (!KSUCCESS(Status)) {
            goto Bcm2709InterruptInitializeLocalUnitEnd;
        }
    }

    *Identifier = 0;
    if (Controller->ProcessorCount > 1) {
        ProcessorId = ArGetMultiprocessorIdRegister() & ARM_PROCESSOR_ID_MASK;
        *Identifier = ProcessorId;
        ProcessorId &= BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
        WRITE_LOCAL_IPI_REGISTER(Bcm2709LocalIpiPending,
                                 ProcessorId,
                                 0xFFFFFFFF);

        //
        // Make sure the mailbox 0 interrupt is enabled for the core. It will
        // be used by the IPIs.
        //

        WRITE_LOCAL_REGISTER(
                         Bcm2709LocalMailboxInterruptControl,
                         ProcessorId,
                         BCM2709_INTERRUPT_LOCAL_MAILBOX_CONTROL_IRQ_0_ENABLE);
    }

    Status = STATUS_SUCCESS;

Bcm2709InterruptInitializeLocalUnitEnd:
    return Status;
}

KSTATUS
HlpBcm2709InterruptInitializeIoUnit (
    PVOID Context
    )

/*++

Routine Description:

    This routine initializes an interrupt controller. It's responsible for
    masking all interrupt lines on the controller and setting the current
    priority to the lowest (allow all interrupts). Once completed successfully,
    it is expected that interrupts can be enabled at the processor core with
    no interrupts occurring.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG Index;
    KSTATUS Status;

    Controller = Context;
    if (HlBcm2709InterruptController == NULL) {
        Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
        Status = HlpBcm2709InitializeController(Controller);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    //
    // Disable all FIQ and IRQ lines.
    //

    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable1, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable2, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisableBasic, 0xFFFFFFFF);
    WRITE_INTERRUPT_REGISTER(Bcm2709InterruptFiqControl, 0);
    Controller->EnabledMask.IrqMaskBasic = 0;
    Controller->EnabledMask.IrqMask1 = 0;
    Controller->EnabledMask.IrqMask2 = 0;
    Controller->EnabledMask.IrqMaskPpi = 0;
    Controller->EnabledMask.IrqMaskSgi = 0;
    for (Index = 0; Index < Controller->ProcessorCount; Index += 1) {
        Controller->Processor[Index].CurrentPriority = 0;
        Controller->Processor[Index].PendingIpis = 0;
    }

    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
HlpBcm2709InterruptSetLocalUnitAddressing (
    PVOID Context,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine attempts to set the current processor's addressing mode.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Target - Supplies a pointer to the targeting configuration to set for this
        processor.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNSUCCESSFUL if the operation failed.

    STATUS_NOT_SUPPORTED if this configuration is never supported on this
        hardware.

--*/

{

    KSTATUS Status;
    UCHAR ThisProcessorTarget;

    ThisProcessorTarget = ArGetMultiprocessorIdRegister() &
                          ARM_PROCESSOR_ID_MASK;

    switch (Target->Addressing) {
    case InterruptAddressingLogicalClustered:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case InterruptAddressingPhysical:
        if (Target->U.PhysicalId != ThisProcessorTarget) {
            Status = STATUS_UNSUCCESSFUL;
            goto SetLocalUnitAddressingEnd;
        }

        Status = STATUS_SUCCESS;
        break;

    case InterruptAddressingLogicalFlat:
        ThisProcessorTarget &= BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
        if (Target->U.LogicalFlatId != (1 << ThisProcessorTarget)) {
            Status = STATUS_UNSUCCESSFUL;
            goto SetLocalUnitAddressingEnd;
        }

        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        goto SetLocalUnitAddressingEnd;
    }

SetLocalUnitAddressingEnd:
    return Status;
}

INTERRUPT_CAUSE
HlpBcm2709InterruptBegin (
    PVOID Context,
    PINTERRUPT_LINE FiringLine,
    PULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called when an interrupt fires. Its role is to determine
    if an interrupt has fired on the given controller, accept it, and determine
    which line fired if any. This routine will always be called with interrupts
    disabled at the processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    FiringLine - Supplies a pointer where the interrupt hardware module will
        fill in which line fired, if applicable.

    MagicCandy - Supplies a pointer where the interrupt hardware module can
        store 32 bits of private information regarding this interrupt. This
        information will be returned to it when the End Of Interrupt routine
        is called.

Return Value:

    Returns an interrupt cause indicating whether or not an interrupt line,
    spurious interrupt, or no interrupt fired on this controller.

--*/

{

    ULONG Base;
    ULONG BasicMask;
    BOOL CheckGpu;
    PBCM2709_INTERRUPT_CONTROLLER Controller;
    BOOL Disabled;
    ULONG DisableMask;
    BOOL HandleInterrupt;
    ULONG Line;
    PBCM2709_INTERRUPT_MASK Mask;
    ULONG PendingIpi;
    ULONG PendingIrq;
    UCHAR Priority;
    PBCM2709_INTERRUPT_PROCESSOR Processor;
    ULONG ProcessorId;
    BCM2709_INTERRUPT_REGISTER Register;

    CheckGpu = TRUE;
    HandleInterrupt = FALSE;

    //
    // Determine which processor the interrupt arrived on.
    //

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    ProcessorId = 0;
    if (Controller->ProcessorCount > 1) {
        ProcessorId = ArGetMultiprocessorIdRegister() &
                      BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
    }

    Processor = &(Controller->Processor[ProcessorId]);

    //
    // If there are multiple processors available, then check for an IPI first.
    //

    if (Controller->ProcessorCount > 1) {

        //
        // If the IPI flag is set in the pending IRQ mask, attempt to handle
        // the IPI.
        //

        PendingIrq = READ_LOCAL_REGISTER(Bcm2709LocalIrqPending, ProcessorId);
        if ((PendingIrq & BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_IPI) != 0) {
            PendingIpi = READ_LOCAL_IPI_REGISTER(Bcm2709LocalIpiPending,
                                                 ProcessorId);

            if (PendingIpi != 0) {
                Line = RtlCountTrailingZeros32(PendingIpi);
                PendingIpi = 1 << Line;
                WRITE_LOCAL_IPI_REGISTER(Bcm2709LocalIpiPending,
                                         ProcessorId,
                                         PendingIpi);

                //
                // If this IPI is disabled at the current priority, keep it
                // pended. Otherwise handle the IPI.
                //

                Mask = &(Controller->Masks[Processor->CurrentPriority]);
                if ((PendingIpi & Mask->IrqMaskSgi) != 0) {
                    Processor->PendingIpis |= PendingIpi;
                    return InterruptCauseSpuriousInterrupt;
                }

                //
                // If an IPI was present and acknowledged, never do further
                // checks for GPU interrupts.
                //

                CheckGpu = FALSE;
                HandleInterrupt = TRUE;
                Line += BCM2709_INTERRUPT_SOFTWARE_LINE_BASE;
            }

        } else if ((PendingIrq &
                    BCM2709_INTERRUPT_LOCAL_IRQ_PENDING_CORE_TIMER_MASK) != 0) {

            PendingIrq = READ_LOCAL_REGISTER(
                                         Bcm2709LocalCoreTimerInterruptControl,
                                         ProcessorId);

            if (PendingIrq != 0) {
                Line = RtlCountTrailingZeros32(PendingIrq);
                Line += BCM2709_INTERRUPT_PER_PROCESSOR_LINE_BASE;
                CheckGpu = FALSE;
                HandleInterrupt = TRUE;
            }
        }
    }

    //
    // Determine which interrupt fired based on the pending status. Only handle
    // GPU interrupts on processor zero as there is no interrupt stearing.
    //

    if ((ProcessorId == 0) && (CheckGpu != FALSE)) {
        PendingIrq = READ_INTERRUPT_REGISTER(Bcm2709InterruptIrqPendingBasic);
        if (PendingIrq == 0) {
            return InterruptCauseNoInterruptHere;
        }

        //
        // If this is a basic interrupt, then determine which line fired based
        // on the bit set.
        //

        if ((PendingIrq & BCM2709_INTERRUPT_IRQ_BASIC_MASK) != 0) {
            PendingIrq &= BCM2709_INTERRUPT_IRQ_BASIC_MASK;
            Line = RtlCountTrailingZeros32(PendingIrq);
            Line += Bcm2709InterruptArmTimer;

        //
        // If this is a GPU interrupt that gets set in the basic pending status
        // register, then check which bit is set. The pending 1 and 2 bits do
        // not get set for these interrupts.
        //

        } else if ((PendingIrq & BCM2709_INTERRUPT_IRQ_BASIC_GPU_MASK) != 0) {
            PendingIrq = PendingIrq >> BCM2709_INTERRUPT_IRQ_BASIC_GPU_SHIFT;
            Line = RtlCountTrailingZeros32(PendingIrq);
            Line = HlBcm2709InterruptIrqBasicGpuTable[Line];

        } else {
            if ((PendingIrq & BCM2709_INTERRUPT_IRQ_BASIC_PENDING_1) != 0) {
                Register = Bcm2709InterruptIrqPending1;
                BasicMask = BCM2709_INTERRUPT_IRQ1_BASIC_MASK;
                Base = 0;

            } else {
                Register = Bcm2709InterruptIrqPending2;
                BasicMask = BCM2709_INTERRUPT_IRQ2_BASIC_MASK;
                Base = 32;
            }

            //
            // Remove the GPU interrupts that appear in the basic register in
            // order to count the trailing zeros.
            //

            PendingIrq = READ_INTERRUPT_REGISTER(Register);
            PendingIrq &= ~BasicMask;
            Line = RtlCountTrailingZeros32(PendingIrq);
            Line += Base;
        }

        HandleInterrupt = TRUE;
    }

    if (HandleInterrupt == FALSE) {
        return InterruptCauseNoInterruptHere;
    }

    //
    // Processor zero is the only core that receives interrupts that are not
    // IPIs and PPIs. So, if this is processor 0, mask all of the interrupts
    // at or below the firing line's priority level.
    //

    Disabled = FALSE;
    Priority = Controller->LinePriority[Line];
    if (ProcessorId == 0) {
        Mask = &(Controller->Masks[Priority]);
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisableBasic,
                                 Mask->IrqMaskBasic);

        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable1, Mask->IrqMask1);
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqDisable2, Mask->IrqMask2);
        Disabled = TRUE;
    }

    //
    // If there is more than one core, then PPIs may be enabled. Disable all of
    // the PPIs enabled at or below the firing line's priority level. IPIs
    // cannot be disabled in the hardware, so even those are per-processor,
    // they are treated separately.
    //

    if (Controller->ProcessorCount > 1) {
        Mask = &(Controller->Masks[Priority]);
        DisableMask = ~Mask->IrqMaskPpi & Controller->EnabledMask.IrqMaskPpi;
        WRITE_LOCAL_REGISTER(Bcm2709LocalCoreTimerInterruptControl,
                             ProcessorId,
                             DisableMask);

        Disabled = TRUE;
    }

    //
    // Now that the interrupt is disabled, if the firing interrupt's priority
    // is less than the current priority, treat it as spurious. This can happen
    // if another core enables an interrupt line while core zero is running at
    // a higher priority. This spurious interrupt will be re-enabled when core
    // zero lowers its priority. It should fire again at that point.
    //

    if ((Disabled != FALSE) && (Priority < Processor->CurrentPriority)) {
        return InterruptCauseSpuriousInterrupt;
    }

    //
    // Save the previous priority to know where to get back to when this
    // interrupt completes.
    //

    *MagicCandy = Processor->CurrentPriority;
    Processor->CurrentPriority = Priority;

    //
    // Return the interrupt line information.
    //

    FiringLine->Type = InterruptLineControllerSpecified;
    FiringLine->U.Local.Controller = 0;
    FiringLine->U.Local.Line = Line;
    return InterruptCauseLineFired;
}

VOID
HlpBcm2709InterruptEndOfInterrupt (
    PVOID Context,
    ULONG MagicCandy
    )

/*++

Routine Description:

    This routine is called after an interrupt has fired and been serviced. Its
    role is to tell the interrupt controller that processing has completed.
    This routine will always be called with interrupts disabled at the
    processor core.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    MagicCandy - Supplies the magic candy that that the interrupt hardware
        module stored when the interrupt began.

Return Value:

    None.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG EnableMask;
    PBCM2709_INTERRUPT_MASK Mask;
    ULONG PendingIpis;
    UCHAR PreviousPriority;
    PBCM2709_INTERRUPT_PROCESSOR Processor;
    ULONG ProcessorId;

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    ProcessorId = 0;
    if (Controller->ProcessorCount > 1) {
        ProcessorId = ArGetMultiprocessorIdRegister() &
                      BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
    }

    //
    // Set the interrupt masks to correspond to what they were before this
    // interrupt began and raised the priority. Use the enable mask to avoid
    // simply enabling every interrupt ever. Only modify the GPU and CPU
    // interrupt lines on processor 0.
    //

    PreviousPriority = MagicCandy;
    Mask = &(Controller->Masks[PreviousPriority]);
    Controller->Processor[ProcessorId].CurrentPriority = PreviousPriority;
    if (ProcessorId == 0) {
        EnableMask = ~Mask->IrqMaskBasic & Controller->EnabledMask.IrqMaskBasic;
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnableBasic, EnableMask);
        EnableMask = ~Mask->IrqMask1 & Controller->EnabledMask.IrqMask1;
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnable1, EnableMask);
        EnableMask = ~Mask->IrqMask2 & Controller->EnabledMask.IrqMask2;
        WRITE_INTERRUPT_REGISTER(Bcm2709InterruptIrqEnable2, EnableMask);
    }

    //
    // Check the PPI and IPI masks on all cores to see if the lowering of the
    // priority re-enables some per-processor interrupts. If there were any
    // pending IPIs in the re-enabled set, replay those interrupts.
    //

    if (Controller->ProcessorCount > 1) {
        EnableMask = ~Mask->IrqMaskPpi & Controller->EnabledMask.IrqMaskPpi;
        if (EnableMask != 0) {
            WRITE_LOCAL_REGISTER(Bcm2709LocalCoreTimerInterruptControl,
                                 ProcessorId,
                                 EnableMask);
        }

        EnableMask = ~Mask->IrqMaskSgi & Controller->EnabledMask.IrqMaskSgi;
        if (EnableMask != 0) {
            Processor = &(Controller->Processor[ProcessorId]);
            PendingIpis = EnableMask & Processor->PendingIpis;
            if (PendingIpis != 0) {
                Processor->PendingIpis &= ~PendingIpis;
                WRITE_LOCAL_IPI_REGISTER(Bcm2709LocalRequestIpi,
                                         ProcessorId,
                                         PendingIpis);
            }
        }
    }

    return;
}

KSTATUS
HlpBcm2709InterruptRequestInterrupt (
    PVOID Context,
    PINTERRUPT_LINE Line,
    ULONG Vector,
    PINTERRUPT_HARDWARE_TARGET Target
    )

/*++

Routine Description:

    This routine requests a hardware interrupt on the given line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the interrupt line to spark.

    Vector - Supplies the vector to generate the interrupt on (for vectored
        architectures only).

    Target - Supplies a pointer to the set of processors to target.

Return Value:

    STATUS_SUCCESS on success.

    Error code on failure.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG InterruptValue;
    ULONG ProcessorId;
    ULONG ProcessorMask;
    KSTATUS Status;

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;

    //
    // Currently requesting device interrupts is not supported. This support
    // will probably have to be added when deep power management comes online.
    //

    if (Line->U.Local.Line < BCM2709_INTERRUPT_SOFTWARE_LINE_BASE) {
        Status = STATUS_NOT_IMPLEMENTED;
        goto Bcm2709InterruptRequestInterruptEnd;
    }

    Status = STATUS_SUCCESS;
    ProcessorMask = 0;
    switch (Target->Addressing) {
    case InterruptAddressingLogicalClustered:
        Status = STATUS_NOT_SUPPORTED;
        break;

    case InterruptAddressingSelf:
        ProcessorId = ArGetMultiprocessorIdRegister();
        ProcessorId &= BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
        ProcessorMask = 1 << ProcessorId;
        break;

    case InterruptAddressingAll:
        ProcessorMask = (1 << Controller->ProcessorCount) - 1;
        break;

    case InterruptAddressingAllExcludingSelf:
        ProcessorId = ArGetMultiprocessorIdRegister();
        ProcessorId &= BCM2709_INTERRUPT_PROCESSOR_ID_MASK;
        ProcessorMask = (1 << Controller->ProcessorCount) - 1;
        ProcessorMask &= ~(1 << ProcessorId);
        break;

    case InterruptAddressingLogicalFlat:
        ProcessorMask = Target->U.LogicalFlatId;
        break;

    case InterruptAddressingPhysical:
        ProcessorMask =
             1 << (Target->U.PhysicalId & BCM2709_INTERRUPT_PROCESSOR_ID_MASK);

        break;

    default:
        Status = STATUS_INVALID_PARAMETER;
        break;
    }

    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptRequestInterruptEnd;
    }

    InterruptValue =
              1 << (Line->U.Local.Line - BCM2709_INTERRUPT_SOFTWARE_LINE_BASE);

    //
    // Write the command out to the software interrupt register for each
    // processor targeted by the interrupt.
    //

    ProcessorId = 0;
    while (ProcessorMask != 0) {
        if ((ProcessorMask & 0x1) == 0) {
            ProcessorMask >>= 1;
            ProcessorId += 1;
            continue;
        }

        WRITE_LOCAL_IPI_REGISTER(Bcm2709LocalRequestIpi,
                                 ProcessorId,
                                 InterruptValue);

        ProcessorMask >>= 1;
        ProcessorId += 1;
    }

Bcm2709InterruptRequestInterruptEnd:
    return Status;
}

KSTATUS
HlpBcm2709InterruptStartProcessor (
    PVOID Context,
    ULONG Identifier,
    PHYSICAL_ADDRESS JumpAddressPhysical
    )

/*++

Routine Description:

    This routine attempts to start the given processor.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Identifier - Supplies the identifier of the processor to start.

    JumpAddressPhysical - Supplies the physical address of the location that
        new processor should jump to.

Return Value:

    STATUS_SUCCESS if the start command was successfully sent.

    Error code on failure.

--*/

{

    INTERRUPT_LINE Line;
    KSTATUS Status;
    INTERRUPT_HARDWARE_TARGET Target;

    Line.Type = InterruptLineControllerSpecified;
    Line.U.Local.Controller = 0;
    Line.U.Local.Line = BCM2709_INTERRUPT_SOFTWARE_LINE_BASE;
    Target.Addressing = InterruptAddressingPhysical;
    Target.U.PhysicalId = Identifier;
    Status = HlpBcm2709InterruptRequestInterrupt(Context, &Line, 0, &Target);
    return Status;
}

KSTATUS
HlpBcm2709InterruptSetLineState (
    PVOID Context,
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PVOID ResourceData,
    UINTN ResourceDataSize
    )

/*++

Routine Description:

    This routine enables or disables and configures an interrupt line.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to set up. This will always be a
        controller specified line.

    State - Supplies a pointer to the new configuration of the line.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG Index;
    ULONG LineNumber;
    BOOL LocalInterrupt;
    BCM2709_INTERRUPT_MASK Mask;
    BOOL PpiInterrupt;
    UCHAR Priority;
    BCM2709_INTERRUPT_REGISTER Register;
    ULONG RegisterValue;
    ULONG Shift;
    KSTATUS Status;

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    LineNumber = Line->U.Local.Line;
    if ((Line->Type != InterruptLineControllerSpecified) ||
        (Line->U.Local.Controller != 0) ||
        (LineNumber >= BCM2709_INTERRUPT_MAX_LINE_COUNT)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Bcm2709SetLineStateEnd;
    }

    if ((State->Output.Type != InterruptLineControllerSpecified) ||
        (State->Output.U.Local.Controller != INTERRUPT_CPU_IDENTIFIER) ||
        (State->Output.U.Local.Line != INTERRUPT_CPU_IRQ_PIN)) {

        Status = STATUS_INVALID_PARAMETER;
        goto Bcm2709SetLineStateEnd;
    }

    RtlZeroMemory(&Mask, sizeof(BCM2709_INTERRUPT_MASK));
    LocalInterrupt = FALSE;
    PpiInterrupt = FALSE;

    //
    // If the line is a GPU line, then determine which of the two
    // disable/enable registers it belongs to.
    //

    if (LineNumber < BCM2709_INTERRUPT_GPU_LINE_COUNT) {
        Shift = LineNumber;
        if (LineNumber >= 32) {
            Shift -= 32;
        }

        RegisterValue = 1 << Shift;
        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
            if (LineNumber < 32) {
                Register = Bcm2709InterruptIrqDisable1;

            } else {
                Register = Bcm2709InterruptIrqDisable2;
            }

        } else {
            if (LineNumber < 32) {
                Register = Bcm2709InterruptIrqEnable1;

            } else {
                Register = Bcm2709InterruptIrqEnable2;
            }
        }

        //
        // Set the mask in the priority level.
        //

        if (LineNumber < 32) {
            Mask.IrqMask1 |= RegisterValue;

        } else {
            Mask.IrqMask2 |= RegisterValue;
        }

    //
    // If this is an ARM line, then get the correct register and mask.
    //

    } else if (LineNumber < Bcm2709InterruptHardwareLineCount) {
        Shift = LineNumber - BCM2709_INTERRUPT_GPU_LINE_COUNT;
        RegisterValue = 1 << Shift;
        if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
            Register = Bcm2709InterruptIrqDisableBasic;

        } else {
            Register = Bcm2709InterruptIrqEnableBasic;
        }

        //
        // Set the mask in the priority level.
        //

        Mask.IrqMaskBasic |= RegisterValue;

    //
    // If this is a per-processor interrupt, prepare to enable/disable on each
    // core.
    //

    } else if (LineNumber < BCM2709_INTERRUPT_SOFTWARE_LINE_BASE) {
        PpiInterrupt = TRUE;
        Register = Bcm2709LocalCoreTimerInterruptControl;
        Shift = LineNumber - BCM2709_INTERRUPT_PER_PROCESSOR_LINE_BASE;
        Mask.IrqMaskPpi |= 1 << Shift;

    //
    // Otherwise this is a software interrupt.
    //

    } else {
        LocalInterrupt = TRUE;
        Shift = LineNumber - BCM2709_INTERRUPT_SOFTWARE_LINE_BASE;
        Mask.IrqMaskSgi |= 1 << Shift;
    }

    //
    // If the interrupt is about to be enabled, make sure the priority mask is
    // updated first.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0) {
        Controller->EnabledMask.IrqMaskBasic |= Mask.IrqMaskBasic;
        Controller->EnabledMask.IrqMask1 |= Mask.IrqMask1;
        Controller->EnabledMask.IrqMask2 |= Mask.IrqMask2;
        Controller->EnabledMask.IrqMaskPpi |= Mask.IrqMaskPpi;
        Controller->EnabledMask.IrqMaskSgi |= Mask.IrqMaskSgi;
        Priority = State->HardwarePriority;
        Controller->LinePriority[LineNumber] = Priority;

        //
        // This interrupt should be masked for any priority at or above it.
        //

        for (Index = Priority;
             Index < BCM2709_INTERRUPT_PRIORITY_COUNT;
             Index += 1) {

            Controller->Masks[Index].IrqMaskBasic |= Mask.IrqMaskBasic;
            Controller->Masks[Index].IrqMask1 |= Mask.IrqMask1;
            Controller->Masks[Index].IrqMask2 |= Mask.IrqMask2;
            Controller->Masks[Index].IrqMaskPpi |= Mask.IrqMaskPpi;
            Controller->Masks[Index].IrqMaskSgi |= Mask.IrqMaskSgi;
        }
    }

    //
    // Change the state of the interrupt based on the register and the value
    // determined above. There is nothing to do for IPIs, but for regular PPIs
    // the interrupt must be enabled/disabled on each core.
    //

    if (LocalInterrupt == FALSE) {
        if (PpiInterrupt == FALSE) {
            WRITE_INTERRUPT_REGISTER(Register, RegisterValue);

        } else {
            for (Index = 0; Index < Controller->ProcessorCount; Index += 1) {
                RegisterValue = READ_LOCAL_REGISTER(
                                         Bcm2709LocalCoreTimerInterruptControl,
                                         Index);

                if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
                    RegisterValue &= ~Mask.IrqMaskPpi;

                } else {
                    RegisterValue |= Mask.IrqMaskPpi;
                }

                WRITE_LOCAL_REGISTER(Bcm2709LocalCoreTimerInterruptControl,
                                     Index,
                                     RegisterValue);
            }
        }
    }

    //
    // If the interrupt was just disabled, make sure the priority mask is
    // updated after.
    //

    if ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) == 0) {
        Controller->EnabledMask.IrqMaskBasic &= ~Mask.IrqMaskBasic;
        Controller->EnabledMask.IrqMask1 &= ~Mask.IrqMask1;
        Controller->EnabledMask.IrqMask2 &= ~Mask.IrqMask2;
        Controller->EnabledMask.IrqMaskPpi &= ~Mask.IrqMaskPpi;
        Controller->EnabledMask.IrqMaskSgi &= ~Mask.IrqMaskSgi;

        //
        // Remove the mask for this interrupt at any priority.
        //

        for (Index = 0;
             Index < BCM2709_INTERRUPT_PRIORITY_COUNT;
             Index += 1) {

            Controller->Masks[Index].IrqMaskBasic &= ~Mask.IrqMaskBasic;
            Controller->Masks[Index].IrqMask1 &= ~Mask.IrqMask1;
            Controller->Masks[Index].IrqMask2 &= ~Mask.IrqMask2;
            Controller->Masks[Index].IrqMaskPpi &= ~Mask.IrqMaskPpi;
            Controller->Masks[Index].IrqMaskSgi &= ~Mask.IrqMaskSgi;
        }
    }

    Status = STATUS_SUCCESS;

Bcm2709SetLineStateEnd:
    return Status;
}

VOID
HlpBcm2709InterruptMaskLine (
    PVOID Context,
    PINTERRUPT_LINE Line,
    BOOL Enable
    )

/*++

Routine Description:

    This routine masks or unmasks an interrupt line, leaving the rest of the
    line state intact.

Arguments:

    Context - Supplies the pointer to the controller's context, provided by the
        hardware module upon initialization.

    Line - Supplies a pointer to the line to maek or unmask. This will always
        be a controller specified line.

    Enable - Supplies a boolean indicating whether to mask the interrupt,
        preventing interrupts from coming through (FALSE), or enable the line
        and allow interrupts to come through (TRUE).

Return Value:

    None.

--*/

{

    PBCM2709_INTERRUPT_CONTROLLER Controller;
    ULONG Index;
    ULONG LineNumber;
    ULONG Mask;
    BCM2709_INTERRUPT_REGISTER Register;
    ULONG RegisterValue;
    ULONG Shift;

    Controller = (PBCM2709_INTERRUPT_CONTROLLER)Context;
    LineNumber = Line->U.Local.Line;

    //
    // Handle GPU interrupts.
    //

    if (LineNumber < Bcm2709InterruptHardwareLineCount) {

        //
        // If the line is a GPU line, then determine which of the two
        // disable/enable registers it belongs to.
        //

        if (LineNumber < BCM2709_INTERRUPT_GPU_LINE_COUNT) {
            Shift = LineNumber;
            if (LineNumber >= 32) {
                Shift -= 32;
            }

            RegisterValue = 1 << Shift;
            if (Enable == FALSE) {
                if (LineNumber < 32) {
                    Register = Bcm2709InterruptIrqDisable1;

                } else {
                    Register = Bcm2709InterruptIrqDisable2;
                }

            } else {
                if (LineNumber < 32) {
                    Register = Bcm2709InterruptIrqEnable1;

                } else {
                    Register = Bcm2709InterruptIrqEnable2;
                }
            }

        //
        // Otherwise the interrupt belongs to the basic enable and disable
        // registers.
        //

        } else {
            Shift = LineNumber - BCM2709_INTERRUPT_GPU_LINE_COUNT;
            RegisterValue = 1 << Shift;
            if (Enable == FALSE) {
                Register = Bcm2709InterruptIrqDisableBasic;

            } else {
                Register = Bcm2709InterruptIrqEnableBasic;
            }
        }

        WRITE_INTERRUPT_REGISTER(Register, RegisterValue);

    //
    // Handle per-processor interrupts.
    //

    } else if (LineNumber < BCM2709_INTERRUPT_SOFTWARE_LINE_BASE) {
        LineNumber -= BCM2709_INTERRUPT_PER_PROCESSOR_LINE_BASE;
        Mask = 1 << LineNumber;
        for (Index = 0; Index < Controller->ProcessorCount; Index += 1) {
            RegisterValue = READ_LOCAL_REGISTER(
                                         Bcm2709LocalCoreTimerInterruptControl,
                                         Index);

            if (Enable == FALSE) {
                RegisterValue &= ~Mask;

            } else {
                RegisterValue |= Mask;
            }

            WRITE_LOCAL_REGISTER(Bcm2709LocalCoreTimerInterruptControl,
                                 Index,
                                 RegisterValue);
        }
    }

    return;
}

KSTATUS
HlpBcm2709InitializeController (
    PBCM2709_INTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initialized the interrupt controller state for the BCM2709
    interrupt controller.

Arguments:

    Controller - Supplies a pointer to the BCM2709 interrupt controller being
        initialized.

Return Value:

    Status code.

--*/

{

    PVOID InterruptController;
    PVOID LocalBase;
    PHYSICAL_ADDRESS PhysicalAddress;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    if (HlBcm2709InterruptController == NULL) {
        PhysicalAddress = HlBcm2709Table->InterruptControllerPhysicalAddress;
        InterruptController = HlMapPhysicalAddress(PhysicalAddress,
                                                   Bcm2709InterruptSize,
                                                   TRUE);

        if (InterruptController == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto Bcm2709InitializeControllerEnd;
        }

        HlBcm2709InterruptController = InterruptController;
        PhysicalAddress = HlBcm2709Table->CpuLocalPhysicalAddress;
        if ((HlBcm2709LocalBase == NULL) && (PhysicalAddress != 0)) {
            LocalBase = HlMapPhysicalAddress(PhysicalAddress,
                                             Bcm2709LocalInterruptSize,
                                             TRUE);

            if (LocalBase == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto Bcm2709InitializeControllerEnd;
            }

            HlBcm2709LocalBase = LocalBase;
        }

        Status = HlpBcm2709InterruptDescribeLines(Controller);
        if (!KSUCCESS(Status)) {
            goto Bcm2709InitializeControllerEnd;
        }
    }

Bcm2709InitializeControllerEnd:
    return Status;
}

KSTATUS
HlpBcm2709InterruptDescribeLines (
    PBCM2709_INTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine describes all interrupt lines to the system.

Arguments:

    Controller - Supplies a pointer to the BCM2709 interrupt controller whose
        lines are being described.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINES_DESCRIPTION Lines;
    KSTATUS Status;

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;

    //
    // Describe the normal lines on the BCM2709.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.LineStart = 0;
    Lines.LineEnd = Lines.LineStart + Bcm2709InterruptHardwareLineCount;
    Lines.Gsi = HlBcm2709Table->InterruptControllerGsiBase;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

    //
    // Describe the per-processor interrupt lines.
    //

    ASSERT(Lines.LineEnd == BCM2709_INTERRUPT_PER_PROCESSOR_LINE_BASE);

    Lines.Type = InterruptLinesProcessorLocal;
    Lines.LineStart = Lines.LineEnd;
    Lines.LineEnd = Lines.LineStart +
                    BCM2709_INTERRUPT_PER_PROCESSOR_LINE_COUNT;

    Lines.Gsi = Lines.Gsi + Bcm2709InterruptHardwareLineCount;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

    //
    // Describe the SGIs. These are fake and actually tied up to GSI 100 for
    // the ARM local mailbox 0, but that particular mailbox can express 32 bits.
    //

    ASSERT(Lines.LineEnd == BCM2709_INTERRUPT_SOFTWARE_LINE_BASE);

    Lines.Type = InterruptLinesSoftwareOnly;
    Lines.LineStart = Lines.LineEnd;
    Lines.LineEnd = Lines.LineStart + BCM2709_INTERRUPT_SOFTWARE_LINE_COUNT;
    Lines.Gsi = INTERRUPT_LINES_GSI_NONE;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

    //
    // Register the output lines.
    //

    Lines.Type = InterruptLinesOutput;
    Lines.OutputControllerIdentifier = INTERRUPT_CPU_IDENTIFIER;
    Lines.LineStart = INTERRUPT_ARM_MIN_CPU_LINE;
    Lines.LineEnd = INTERRUPT_ARM_MAX_CPU_LINE;
    Status = HlRegisterHardware(HardwareModuleInterruptLines, &Lines);
    if (!KSUCCESS(Status)) {
        goto Bcm2709InterruptDescribeLinesEnd;
    }

Bcm2709InterruptDescribeLinesEnd:
    return Status;
}

