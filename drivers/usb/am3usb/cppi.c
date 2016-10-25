/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cppi.c

Abstract:

    This module implements TI CPPI 4.1 DMA controller support for USB.

Author:

    Evan Green 18-Sep-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "cppi.h"

//
// --------------------------------------------------------------------- Macros
//

#define CPPI_READ(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define CPPI_WRITE(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// This macro returns a register for a particular DMA port. There are
// 30 in all, indexed 0-29.
//

#define CPPI_PORT(_Register, _Port) \
    (CppiTxControl0 + ((_Port) * 0x20) + ((_Register) - CppiTxControl0))

//
// This macro converts from and instance + endpoint (zero-based) to a port,
// of which there are 30.
//

#define CPPI_ENDPOINT_TO_PORT(_Instance, _Endpoint) \
    ((_Endpoint) + ((_Instance * CPPI_ENDPOINT_COUNT)))

//
// The read and write port macros take a port number, which is concocted from
// the "endpoint to port" macro.
//

#define CPPI_READ_PORT(_Controller, _Register, _Port) \
    CPPI_READ((_Controller), CPPI_PORT((_Register), (_Port)))

#define CPPI_WRITE_PORT(_Controller, _Register, _Port, _Value) \
    CPPI_WRITE((_Controller), CPPI_PORT((_Register), (_Port)), (_Value))

#define CPPI_QUEUE_READ(_Controller, _Register) \
    CPPI_READ((_Controller), CPPI_QUEUE_OFFSET + (_Register))

#define CPPI_QUEUE_WRITE(_Controller, _Register, _Value) \
    CPPI_WRITE((_Controller), CPPI_QUEUE_OFFSET + (_Register), (_Value))

//
// This macro gets a particular queue control register.
//

#define CPPI_QUEUE_CONTROL(_Register, _Queue) \
    (CppiQueue0A + ((_Queue) * 0x10) + ((_Register) - CppiQueue0A))

#define CPPI_SCHEDULER_READ(_Controller, _Register) \
    CPPI_READ((_Controller), CPPI_SCHEDULER_OFFSET + (_Register))

#define CPPI_SCHEDULER_WRITE(_Controller, _Register, _Value) \
    CPPI_WRITE((_Controller), CPPI_SCHEDULER_OFFSET + (_Register), (_Value))

//
// This macro returns the scheduler queue word for a given word index.
//

#define CPPI_SCHEDULER_WORD(_WordIndex) \
    (CppiSchedulerWord + ((_WordIndex) << 2))

//
// Define macros to get queue numbers based on zero-based DMA endpoint numbers
// (which would be USB endpoint minus one).
//

#define CPPI_GET_FREE_QUEUE(_Instance, _Endpoint) \
    (CPPI_QUEUE_FREE + (_Endpoint) + ((_Instance) * (CPPI_ENDPOINT_COUNT + 1)))

//
// There are two TX queues for each endpoint.
//

#define CPPI_GET_TX_QUEUE(_Instance, _Endpoint) \
    (CPPI_QUEUE_TX + ((_Endpoint) + ((_Instance) * CPPI_ENDPOINT_COUNT)) * 2)

//
// There are 16 TX completion queues, followed by 16 RX completion queues per
// instance.
//

#define CPPI_GET_TX_COMPLETION_QUEUE(_Instance, _Endpoint) \
    (CPPI_QUEUE_TX_COMPLETION + (_Endpoint) + \
     ((_Instance) * (CPPI_ENDPOINT_COUNT + 1) * 2))

#define CPPI_GET_RX_COMPLETION_QUEUE(_Instance, _Endpoint) \
    (CPPI_QUEUE_RX_COMPLETION + (_Endpoint) + \
     ((_Instance) * (CPPI_ENDPOINT_COUNT + 1) * 2))

//
// ---------------------------------------------------------------- Definitions
//

#define CPPI_ALLOCATION_TAG 0x69707043

#define CPPI_MAX_DESCRIPTORS 1024

#define CPPI_DESCRIPTOR_SIZE 32
#define CPPI_DESCRIPTOR_ALIGNMENT CPPI_DESCRIPTOR_SIZE
#define CPPI_DESCRIPTOR_REGION_SIZE \
    (CPPI_MAX_DESCRIPTORS * CPPI_DESCRIPTOR_SIZE)

#define CPPI_LINK_REGION_SIZE (CPPI_MAX_DESCRIPTORS * 4)

#define CPPI_ENDPOINT_COUNT 15
#define CPPI_INSTANCE_COUNT 2
#define CPPI_SCHEDULER_ENTRIES 64

//
// Define offsets into the CPPI region where other register bases start.
//

#define CPPI_SCHEDULER_OFFSET 0x1000
#define CPPI_QUEUE_OFFSET 0x2000

//
// Define queue assignments. Here's a map:
// 0-32: Free queues for USB0/1.
// 32-61: USB0 TX EP1-15 (2 queues each).
// 62-91: USB1 TX EP1-15 (2 queues each).
// 93-107: USB0 TX Completion EP1-15.
// 109-123: USB0 RX Completion EP1-15.
// 125-139: USB1 TX Completion EP1-15.
// 141-155: USB1 RX Completion EP1-15.
//

#define CPPI_QUEUE_FREE 0
#define CPPI_TEARDOWN_QUEUE 31
#define CPPI_QUEUE_TX 32
#define CPPI_QUEUE_TX_COMPLETION 93
#define CPPI_QUEUE_RX_COMPLETION 109

//
// Define queue region control register bits.
//

#define CPPI_QUEUE_REGION_CONTROL_REGION_DESCRIPTOR_SIZE_SHIFT 8
#define CPPI_QUEUE_REGION_CONTROL_LINK_RAM_START_SHIFT 16

//
// Define RX control (RXGCR) register bits.
//

#define CPPI_RX_CONTROL_DEFAULT_DESCRIPTOR_HOST (0x1 << 14)
#define CPPI_RX_CONTROL_RX_ERROR_HANDLING 0x01000000
#define CPPI_RX_CONTROL_TEARDOWN 0x40000000
#define CPPI_RX_CONTROL_CHANNEL_ENABLE 0x80000000

//
// Define TX control (TXGCR) register bits.
//

#define CPPI_TX_CONTROL_TEARDOWN 0x40000000
#define CPPI_TX_CONTROL_CHANNEL_ENABLE 0x80000000

//
// Define scheduler control register bits.
//

#define CPPI_SCHEDULER_CONTROL_ENABLE 0x80000000

#define CPPI_SCHEDULE_WORD_READ_MASK 0x80808080

//
// Define packet descriptor control word 0 register bits.
//

#define CPPI_PACKET_DESCRIPTOR_CONTROL (0x10 << 27)
#define CPPI_PACKET_DESCRIPTOR_CONTROL_LENGTH_MASK 0x001FFFFF

//
// Define packet descriptor control word 1 register bits.
//

#define CPPI_PACKET_DESCRIPTOR_TAG_PORT_SHIFT 27

//
// Define packet descriptor control word 2 register bits.
//

#define CPPI_PACKET_DESCRIPTOR_STATUS_ERROR (1 << 31)
#define CPPI_PACKET_DESCRIPTOR_STATUS_TYPE_USB (0x5 << 26)
#define CPPI_PACKET_DESCRIPTOR_STATUS_ZERO_LENGTH (1 << 19)
#define CPPI_PACKET_DESCRIPTOR_STATUS_ON_CHIP (1 << 14)
#define CPPI_PACKET_DESCRIPTOR_STATUS_RETURN_EACH (1 << 15)

//
// Define teardown descriptor control values.
//

#define CPPI_TEARDOWN_CONTROL_TYPE (0x13 << 27)
#define CPPI_TEARDOWN_RX (1 << 16)

#define CPPI_QUEUE_DESCRIPTOR_ADDRESS_MASK 0xFFFFFFE0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _CPPI_REGISTER {
    CppiRevision = 0x000, // DMAREVID
    CppiTearDownFreeQueue = 0x004, // TDFDQ
    CppiDmaEmulationControl = 0x008, // DMAEMU
    CppiTxControl0 = 0x800, // TXGCR0
    CppiRxControl0 = 0x808, // RXGCR0
    CppiRxChannelA0 = 0x80C, // RXHPCRA0
    CppiRxChannelB0 = 0x810, // RXHPCRB0
} CPPI_REGISTER, *PCPPI_REGISTER;

typedef enum _CPPI_SCHEDULER_REGISTER {
    CppiSchedulerControl = 0x000,
    CppiSchedulerWord = 0x800
} CPPI_SCHEDULER_REGISTER, *PCPP_SCHEDULER_REGISTER;

typedef enum _CPPI_QUEUE_REGISTER {
    CppiQueueRevision = 0x0000,
    CppiQueueReset = 0x0008,
    CppiQueueFdbsc0 = 0x0020,
    CppiQueueFdbsc1 = 0x0024,
    CppiQueueFdbsc2 = 0x0028,
    CppiQueueFdbsc3 = 0x002C,
    CppiQueueFdbsc4 = 0x0030,
    CppiQueueFdbsc5 = 0x0034,
    CppiQueueFdbsc6 = 0x0038,
    CppiQueueFdbsc7 = 0x003C,
    CppiQueueLinkRam0Base = 0x0080,
    CppiQueueLinkRam0Size = 0x0084,
    CppiQueueLinkRam1Base = 0x0088,
    CppiQueuePend0 = 0x0090,
    CppiQueuePend1 = 0x0094,
    CppiQueuePend2 = 0x0098,
    CppiQueuePend3 = 0x009C,
    CppiQueuePend4 = 0x00A0,
    CppiQueueMemoryBase0 = 0x1000,
    CppiQueueMemoryControl0 = 0x1004,
    CppiQueueMemoryBase1 = 0x1010,
    CppiQueueMemoryControl1 = 0x1014,
    CppiQueueMemoryBase2 = 0x1020,
    CppiQueueMemoryControl2 = 0x1024,
    CppiQueueMemoryBase3 = 0x1030,
    CppiQueueMemoryControl3 = 0x1034,
    CppiQueueMemoryBase4 = 0x1040,
    CppiQueueMemoryControl4 = 0x1044,
    CppiQueueMemoryBase5 = 0x1050,
    CppiQueueMemoryControl5 = 0x1054,
    CppiQueueMemoryBase6 = 0x1060,
    CppiQueueMemoryControl6 = 0x1064,
    CppiQueueMemoryBase7 = 0x1070,
    CppiQueueMemoryControl7 = 0x1074,
    CppiQueue0A = 0x2000,
    CppiQueue0B = 0x2004,
    CppiQueue0C = 0x2008,
    CppiQueue0D = 0x200C,
    CppiQueue0StatusA = 0x3000,
    CppiQueue0StatusB = 0x3004,
    CppiQueue0StatusC = 0x3008,
} CPPI_QUEUE_REGISTER, *PCPPI_QUEUE_REGISTER;

/*++

Structure Description:

    This structure stores the hardware-mandated format of a CPPI packet
    descriptor.

Members:

    Control - Stores the first control word, including the overall packet
        length and descriptor type.

    Tag - Stores the second control word, containing the source and destination
        tag numbers (port, channel, subchannel).

    Status - Stores the third control word, containing mostly DMA status bits.

    BufferLength - Stores the length of buffer 0. The DMA engine overwrites
        this on reception.

    BufferPointer - Stores the physical address of buffer 0. The DMA engine
        overwrites this on reception.

    NextDescriptor - Stores the physical address of the next descriptor in the
        set. Set to zero if this is the last descriptor.

    OriginalBufferLength - Stores a copy of the buffer length that the DMA
        engine does not overwrite.

    OriginalBufferPointer - Stores a copy of the buffer pointer that the DMA
        engine does not clobber.

--*/

typedef struct _CPPI_PACKET_DESCRIPTOR {
    ULONG Control;
    ULONG Tag;
    ULONG Status;
    ULONG BufferLength;
    ULONG BufferPointer;
    ULONG NextDescriptor;
    ULONG OriginalBufferLength;
    ULONG OriginalBufferPointer;
} CPPI_PACKET_DESCRIPTOR, *PCPPI_PACKET_DESCRIPTOR;

/*++

Structure Description:

    This structure stores the hardware-mandated format of a CPPI buffer
    descriptor, which is a middle or end descriptor of a packet.

Members:

    Reserved - Stores two unused words in the descriptor.

    Status - Stores the third control word, containing mostly DMA return queue
        information and status bits.

    BufferLength - Stores the length of buffer 0. The DMA engine overwrites
        this on reception.

    BufferPointer - Stores the physical address of buffer 0. The DMA engine
        overwrites this on reception.

    NextDescriptor - Stores the physical address of the next descriptor in the
        set. Set to 0 if this is the last descriptor.

    OriginalBufferLength - Stores a copy of the buffer length that the DMA
        engine does not overwrite.

    OriginalBufferPointer - Stores a copy of the buffer pointer that the DMA
        engine does not clobber.

--*/

typedef struct _CPPI_BUFFER_DESCRIPTOR {
    ULONG Reserved[2];
    ULONG Status;
    ULONG BufferLength;
    ULONG BufferPointer;
    ULONG NextDescriptor;
    ULONG OriginalBufferLength;
    ULONG OriginalBufferPointer;
} CPPI_BUFFER_DESCRIPTOR, *PCPPI_BUFFER_DESCRIPTOR;

/*++

Structure Description:

    This structure stores the hardware-mandated format of a CPPI teardown
    descriptor, which is a sentinal descriptor used to cleanly shut a channel
    down.

Members:

    Control - Stores the descriptor type and control information.

    Reserved - Stores seven unused words in the descriptor.

--*/

typedef struct _CPPI_TEARDOWN_DESCRIPTOR {
    ULONG Control;
    ULONG Reserved[7];
} CPPI_TEARDOWN_DESCRIPTOR, *PCPPI_TEARDOWN_DESCRIPTOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CppipSubmitTeardownDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define the basic schedule that gets written into the DMA scheduler.
//

ULONG CppiSchedule[(CPPI_SCHEDULER_ENTRIES / 4) / 4] = {
    0x03020100,
    0x07060504,
    0x0B0A0908,
    0x0F0E0D0C
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
CppiInitializeControllerState (
    PCPPI_DMA_CONTROLLER Controller,
    PVOID ControllerBase
    )

/*++

Routine Description:

    This routine initializes the CPPI DMA controller state structure.

Arguments:

    Controller - Supplies a pointer to the zeroed controller structure.

    ControllerBase - Supplies the virtual address of the base of the CPPI DMA
        register.

Return Value:

    Status code.

--*/

{

    PHYSICAL_ADDRESS DescriptorBase;
    ULONG Flags;
    PVOID SampleBlock;
    KSTATUS Status;

    KeInitializeSpinLock(&(Controller->TeardownLock));
    Controller->ControllerBase = ControllerBase;
    Flags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
            IO_BUFFER_FLAG_MAP_NON_CACHED;

    Controller->LinkRegionIoBuffer = MmAllocateNonPagedIoBuffer(
                                                         0,
                                                         MAX_ULONG,
                                                         0,
                                                         CPPI_LINK_REGION_SIZE,
                                                         Flags);

    if (Controller->LinkRegionIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    ASSERT(Controller->LinkRegionIoBuffer->FragmentCount == 1);

    MmZeroIoBuffer(Controller->LinkRegionIoBuffer, 0, CPPI_LINK_REGION_SIZE);

    //
    // Create a block allocator for buffer, packet, and teardown descriptors.
    // The block allocator cannot expand because the CPPI controller is
    // programmed with the descriptor region boundaries. So pick a decent max.
    //

    Flags = BLOCK_ALLOCATOR_FLAG_NON_PAGED |
            BLOCK_ALLOCATOR_FLAG_NON_CACHED |
            BLOCK_ALLOCATOR_FLAG_PHYSICALLY_CONTIGUOUS |
            BLOCK_ALLOCATOR_FLAG_NO_EXPANSION;

    Controller->BlockAllocator = MmCreateBlockAllocator(
                                                   CPPI_DESCRIPTOR_SIZE,
                                                   CPPI_DESCRIPTOR_ALIGNMENT,
                                                   CPPI_MAX_DESCRIPTORS,
                                                   Flags,
                                                   CPPI_ALLOCATION_TAG);

    if (Controller->BlockAllocator == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeControllerStateEnd;
    }

    SampleBlock = MmAllocateBlock(Controller->BlockAllocator, &DescriptorBase);

    ASSERT(SampleBlock != NULL);

    Controller->DescriptorBase = DescriptorBase;

    ASSERT((ULONG)DescriptorBase == DescriptorBase);

    MmFreeBlock(Controller->BlockAllocator, SampleBlock);
    Status = STATUS_SUCCESS;

InitializeControllerStateEnd:
    if (!KSUCCESS(Status)) {
        CppiDestroyControllerState(Controller);
    }

    return Status;
}

VOID
CppiDestroyControllerState (
    PCPPI_DMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine tears down and frees all resources associated with the given
    CPPI DMA controller. The structure itself is owned by the caller.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

{

    if (Controller->LinkRegionIoBuffer != NULL) {
        MmFreeIoBuffer(Controller->LinkRegionIoBuffer);
        Controller->LinkRegionIoBuffer = NULL;
    }

    if (Controller->BlockAllocator != NULL) {
        MmDestroyBlockAllocator(Controller->BlockAllocator);
        Controller->BlockAllocator = NULL;
    }

    Controller->DescriptorBase = 0;
    return;
}

VOID
CppiRegisterCompletionCallback (
    PCPPI_DMA_CONTROLLER Controller,
    ULONG Instance,
    PCPPI_ENDPOINT_COMPLETION CallbackRoutine,
    PVOID CallbackContext
    )

/*++

Routine Description:

    This routine registers a DMA completion callback with the CPPI DMA
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Instance - Supplies the instance number.

    CallbackRoutine - Supplies the callback routine to call. The callback will
        be called at dispatch level.

    CallbackContext - Supplies a pointer's worth of context that is passed into
        the callback routine.

Return Value:

    None.

--*/

{

    ASSERT(Instance < CPPI_MAX_INSTANCES);
    ASSERT(Controller->CompletionRoutines[Instance] == NULL);

    Controller->CompletionContexts[Instance] = CallbackContext;
    Controller->CompletionRoutines[Instance] = CallbackRoutine;
    return;
}

KSTATUS
CppiResetController (
    PCPPI_DMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine performs hardware initialization of the CPPI DMA controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG Bits;
    ULONG Endpoint;
    ULONG Index;
    ULONG Instance;
    ULONG Port;
    ULONG ScheduleWord;
    ULONG Value;

    //
    // Give the controller its linking region scratch RAM.
    //

    CPPI_QUEUE_WRITE(
                   Controller,
                   CppiQueueLinkRam0Base,
                   Controller->LinkRegionIoBuffer->Fragment[0].PhysicalAddress);

    //
    // Note that this size is only the size of region 0. If this size does not
    // correspond correctly to the descriptor region size, then the controller
    // will go off and use Link RAM 1, which is not programmed. That would be
    // bad
    //

    CPPI_QUEUE_WRITE(Controller,
                     CppiQueueLinkRam0Size,
                     CPPI_LINK_REGION_SIZE);

    CPPI_QUEUE_WRITE(Controller, CppiQueueLinkRam1Base, 0);

    //
    // Tell the controller where its descriptors are coming from.
    //

    CPPI_QUEUE_WRITE(Controller,
                     CppiQueueMemoryBase0,
                     Controller->DescriptorBase);

    //
    // Tell the controller the size of the descriptor region, the size of the
    // descriptor, and the offset into the link RAM to use. Sizes are encoded
    // as 2^(5 + RegisterValue).
    //

    Bits = RtlCountTrailingZeros32(CPPI_DESCRIPTOR_SIZE);

    ASSERT(Bits >= 5);

    Value = (0 << CPPI_QUEUE_REGION_CONTROL_LINK_RAM_START_SHIFT) |
            ((Bits - 5) <<
             CPPI_QUEUE_REGION_CONTROL_REGION_DESCRIPTOR_SIZE_SHIFT);

    Bits = RtlCountTrailingZeros32(CPPI_DESCRIPTOR_REGION_SIZE);

    ASSERT(Bits >= 5);

    Value |= Bits - 5;
    CPPI_QUEUE_WRITE(Controller, CppiQueueMemoryControl0, Value);

    //
    // Configure the queues for all the endpoints.
    //

    for (Instance = 0; Instance < CPPI_INSTANCE_COUNT; Instance += 1) {
        for (Endpoint = 0; Endpoint < CPPI_ENDPOINT_COUNT; Endpoint += 1) {
            Port = CPPI_ENDPOINT_TO_PORT(Instance, Endpoint);

            //
            // Configure the RX channel for each endpoint.
            //

            Value = CPPI_GET_FREE_QUEUE(Instance, Endpoint);
            Value |= Value << 16;
            CPPI_WRITE_PORT(Controller, CppiRxChannelA0, Port, Value);
            CPPI_WRITE_PORT(Controller, CppiRxChannelB0, Port, Value);

            //
            // Configure the RX and TX completion queues for each endpoint.
            //

            Value = CPPI_GET_RX_COMPLETION_QUEUE(Instance, Endpoint) |
                    CPPI_RX_CONTROL_CHANNEL_ENABLE |
                    CPPI_RX_CONTROL_RX_ERROR_HANDLING |
                    CPPI_RX_CONTROL_DEFAULT_DESCRIPTOR_HOST;

            CPPI_WRITE_PORT(Controller, CppiRxControl0, Port, Value);
            Value = CPPI_GET_TX_COMPLETION_QUEUE(Instance, Endpoint) |
                    CPPI_TX_CONTROL_CHANNEL_ENABLE;

            CPPI_WRITE_PORT(Controller, CppiTxControl0, Port, Value);
        }
    }

    //
    // Configure the teardown descriptor queue.
    //

    CPPI_WRITE(Controller, CppiTearDownFreeQueue, CPPI_TEARDOWN_QUEUE);

    //
    // Set up the scheduler: super basic, equal weights.
    //

    for (Index = 0; Index < (CPPI_SCHEDULER_ENTRIES / 4) / 4; Index += 1) {
        ScheduleWord = CppiSchedule[Index];
        CPPI_SCHEDULER_WRITE(Controller,
                             CPPI_SCHEDULER_WORD(Index * 4),
                             ScheduleWord);

        CPPI_SCHEDULER_WRITE(Controller,
                             CPPI_SCHEDULER_WORD((Index * 4) + 1),
                             ScheduleWord | CPPI_SCHEDULE_WORD_READ_MASK);

        ScheduleWord |= 0x10101010;
        CPPI_SCHEDULER_WRITE(Controller,
                             CPPI_SCHEDULER_WORD((Index * 4) + 2),
                             ScheduleWord);

        CPPI_SCHEDULER_WRITE(Controller,
                             CPPI_SCHEDULER_WORD((Index * 4) + 3),
                             ScheduleWord | CPPI_SCHEDULE_WORD_READ_MASK);
    }

    Value = CPPI_SCHEDULER_CONTROL_ENABLE | (CPPI_SCHEDULER_ENTRIES - 1);
    CPPI_SCHEDULER_WRITE(Controller, CppiSchedulerControl, Value);
    return STATUS_SUCCESS;
}

VOID
CppiInterruptServiceDispatch (
    PCPPI_DMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine is called when a CPPI DMA interrupt occurs. It is called at
    dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

{

    ULONG BitIndex;
    PVOID CompletionContext;
    PCPPI_ENDPOINT_COMPLETION CompletionRoutine;
    ULONG DmaEndpoint;
    ULONG Instance;
    ULONG Pend;
    ULONG PendIndex;
    ULONG Queue;
    BOOL Transmit;

    //
    // Loop through all the pend registers that have completion queue status
    // bits.
    //

    for (PendIndex = 2; PendIndex < 5; PendIndex += 1) {
        Pend = CPPI_QUEUE_READ(Controller, CppiQueuePend0 + (PendIndex * 4));
        while (Pend != 0) {
            BitIndex = RtlCountTrailingZeros32(Pend);
            Pend &= ~(1 << BitIndex);

            //
            // Compute the queue number, and figure out who to notify. This if
            // ladder would need to be rearranged if the queue order rearranges
            // significantly.
            //

            Queue = (PendIndex * 32) + BitIndex;

            //
            // Skip this if it's not a completion queue pend bit.
            //

            if (Queue < CPPI_GET_TX_COMPLETION_QUEUE(0, 0)) {
                continue;

            //
            // Handle instance 0 TX completion.
            //

            } else if (Queue <=
                       CPPI_GET_TX_COMPLETION_QUEUE(0, CPPI_ENDPOINT_COUNT)) {

                Instance = 0;
                DmaEndpoint = Queue - CPPI_GET_TX_COMPLETION_QUEUE(0, 0);
                Transmit = TRUE;

            //
            // Handle instance 0 RX completion.
            //

            } else if (Queue <=
                       CPPI_GET_RX_COMPLETION_QUEUE(0, CPPI_ENDPOINT_COUNT)) {

                Instance = 0;
                DmaEndpoint = Queue - CPPI_GET_RX_COMPLETION_QUEUE(0, 0);
                Transmit = FALSE;

            //
            // Handle instance 1 TX completion.
            //

            } else if (Queue <=
                       CPPI_GET_TX_COMPLETION_QUEUE(1, CPPI_ENDPOINT_COUNT)) {

                Instance = 1;
                DmaEndpoint = Queue - CPPI_GET_TX_COMPLETION_QUEUE(1, 0);
                Transmit = TRUE;

            //
            // This must be instance 1 RX completion.
            //

            } else {

                ASSERT(Queue <=
                       CPPI_GET_RX_COMPLETION_QUEUE(1, CPPI_ENDPOINT_COUNT));

                Instance = 1;
                DmaEndpoint = Queue - CPPI_GET_RX_COMPLETION_QUEUE(1, 0);
                Transmit = FALSE;
            }

            //
            // Call the completion routine for the appropriate instance and
            // endpoint number.
            //

            CompletionContext = Controller->CompletionContexts[Instance];
            CompletionRoutine = Controller->CompletionRoutines[Instance];

            ASSERT(CompletionRoutine != NULL);

            CompletionRoutine(CompletionContext, DmaEndpoint, Transmit);
        }
    }

    return;
}

KSTATUS
CppiCreateDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    ULONG Instance,
    PCPPI_DESCRIPTOR_DATA Descriptor
    )

/*++

Routine Description:

    This routine creates a DMA buffer descriptor, and initializes its immutable
    members.

Arguments:

    Controller - Supplies a pointer to the controller.

    Instance - Supplies the USB instance index.

    Descriptor - Supplies a pointer where the allocated descriptor will be
        returned on success.

Return Value:

    Status code.

--*/

{

    PCPPI_PACKET_DESCRIPTOR Packet;
    PHYSICAL_ADDRESS PhysicalAddress;

    Packet = MmAllocateBlock(Controller->BlockAllocator, &PhysicalAddress);
    if (Packet == NULL) {

        //
        // The buffer descriptor pool is a fixed size. Clearly that size is
        // too small.
        //

        ASSERT(FALSE);

        return STATUS_INSUFFICIENT_RESOURCES;
    }

    Descriptor->Descriptor = Packet;
    Descriptor->Physical = PhysicalAddress;
    Descriptor->Instance = Instance;
    Descriptor->Submitted = FALSE;
    return STATUS_SUCCESS;
}

VOID
CppiInitializeDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor,
    ULONG DmaEndpoint,
    BOOL Transmit,
    ULONG BufferPhysical,
    ULONG BufferSize
    )

/*++

Routine Description:

    This routine initializes the mutable context of a DMA descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor data.

    DmaEndpoint - Supplies the *zero* based endpoint number (USB endpoint minus
        one).

    Transmit - Supplies a boolean indicating whether or not this is a transmit
        operation (TRUE) or a receive operation (FALSE).

    BufferPhysical - Supplies the physical address of the data buffer.

    BufferSize - Supplies the size of the data buffer in bytes.

Return Value:

    None.

--*/

{

    PCPPI_PACKET_DESCRIPTOR Packet;
    ULONG PacketStatus;
    ULONG Value;

    ASSERT(DmaEndpoint <= CPPI_ENDPOINT_COUNT);
    ASSERT(Descriptor->Submitted == FALSE);

    Packet = Descriptor->Descriptor;
    PacketStatus = CPPI_PACKET_DESCRIPTOR_STATUS_TYPE_USB |
                   CPPI_PACKET_DESCRIPTOR_STATUS_ON_CHIP;

    if (BufferSize == 0) {
        PacketStatus |= CPPI_PACKET_DESCRIPTOR_STATUS_ZERO_LENGTH;
        BufferSize = 1;
    }

    if (Transmit != FALSE) {
        Packet->Control = CPPI_PACKET_DESCRIPTOR_CONTROL | BufferSize;
        PacketStatus |= CPPI_GET_TX_COMPLETION_QUEUE(Descriptor->Instance,
                                                     DmaEndpoint);

    } else {
        Packet->Control = CPPI_PACKET_DESCRIPTOR_CONTROL;
        PacketStatus |= CPPI_GET_RX_COMPLETION_QUEUE(Descriptor->Instance,
                                                     DmaEndpoint);
    }

    Value = ((DmaEndpoint + 1) << CPPI_PACKET_DESCRIPTOR_TAG_PORT_SHIFT);
    Packet->Tag = Value;
    Packet->Status = PacketStatus;
    Packet->NextDescriptor = 0;
    Packet->BufferLength = BufferSize;
    Packet->BufferPointer = BufferPhysical;
    Packet->OriginalBufferLength = BufferSize | (1 << 31) | (1 << 30);
    Packet->OriginalBufferPointer = BufferPhysical;
    Descriptor->Endpoint = DmaEndpoint;
    Descriptor->Transmit = Transmit;
    return;
}

VOID
CppiDestroyDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Data
    )

/*++

Routine Description:

    This routine frees resources associated with a DMA descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the descriptor information.

Return Value:

    None.

--*/

{

    ASSERT(Data->Submitted == FALSE);

    MmFreeBlock(Controller->BlockAllocator, Data->Descriptor);
    Data->Descriptor = NULL;
    Data->Physical = 0;
    return;
}

VOID
CppiSubmitDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    )

/*++

Routine Description:

    This routine adds a descriptor to the DMA hardware queue in preparation for
    takeoff.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor to add to the hardware
        queue.

Return Value:

    Status code.

--*/

{

    ULONG Queue;
    ULONG Register;
    ULONG Value;

    ASSERT(Descriptor->Submitted == FALSE);

    //
    // The bottom 5 bits encode the length of the descriptor in 4-byte units,
    // starting at 24.
    //

    Value = Descriptor->Physical | ((sizeof(CPPI_PACKET_DESCRIPTOR) - 24) / 4);
    if (Descriptor->Transmit != FALSE) {
        Queue = CPPI_GET_TX_QUEUE(Descriptor->Instance, Descriptor->Endpoint);

    } else {
        Queue = CPPI_GET_FREE_QUEUE(Descriptor->Instance, Descriptor->Endpoint);
    }

    Descriptor->Submitted = TRUE;
    Register = CPPI_QUEUE_CONTROL(CppiQueue0D, Queue);
    CPPI_QUEUE_WRITE(Controller, Register, Value);
    return;
}

VOID
CppiReapCompletedDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor,
    PULONG CompletedSize
    )

/*++

Routine Description:

    This routine checks the descriptor and pulls it out of the completion queue.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor check for completion.

    CompletedSize - Supplies an optional pointer where the number of bytes in
        the packet that have completed is returned.

Return Value:

    None.

--*/

{

    PCPPI_PACKET_DESCRIPTOR Packet;
    ULONG Pend;
    ULONG PoppedQueue;
    ULONG Queue;
    ULONG Register;
    ULONGLONG Timeout;

    ASSERT(Descriptor->Submitted != FALSE);

    Packet = NULL;
    if (Descriptor->Transmit != FALSE) {
        Queue = CPPI_GET_TX_COMPLETION_QUEUE(Descriptor->Instance,
                                             Descriptor->Endpoint);

    } else {
        Queue = CPPI_GET_RX_COMPLETION_QUEUE(Descriptor->Instance,
                                             Descriptor->Endpoint);
    }

    Timeout = 0;
    Register = CppiQueuePend0 + ((Queue / 32) * 4);
    do {
        Pend = CPPI_QUEUE_READ(Controller, Register);
        if (Timeout == 0) {
            Timeout = KeGetRecentTimeCounter() + HlQueryTimeCounterFrequency();

        } else {
            if (KeGetRecentTimeCounter() >= Timeout) {
                RtlDebugPrint("CPPI Timeout.\n");
                break;
            }
        }

    } while ((Pend & (1 << (Queue & 0x1F))) == 0);

    //
    // If the descriptor is pending, pull it off the completion queue.
    //

    if ((Pend & (1 << (Queue & 0x1F))) != 0) {
        Register = CPPI_QUEUE_CONTROL(CppiQueue0D, Queue);
        PoppedQueue = CPPI_QUEUE_READ(Controller, Register);
        if ((PoppedQueue & CPPI_QUEUE_DESCRIPTOR_ADDRESS_MASK) ==
             Descriptor->Physical) {

            Descriptor->Submitted = FALSE;
            Packet = Descriptor->Descriptor;

        } else {

            //
            // That's odd, there was some other descriptor there.
            //

            RtlDebugPrint("CPPI: Reaped unexpected queue 0x%x\n", PoppedQueue);

            ASSERT(FALSE);
        }

    } else {

        //
        // That's unexpected, the caller thinks the transfer completed but it's
        // not ready in the completion queue.
        //

        RtlDebugPrint("CPPI: Descriptor 0x%x not on CompletionQ 0x%x ",
                      Descriptor,
                      CPPI_QUEUE_CONTROL(CppiQueue0D, Queue));

        //
        // Check the submit queue for the purpose of helping debug this issue.
        // This is not a working solution to the problem because
        // 1) It's indicative of a larger problem between DMA and USB, and
        // 2) This may pop off some other descriptor, which is now lost forever.
        //

        Queue = CPPI_GET_TX_QUEUE(Descriptor->Instance, Descriptor->Endpoint);
        Register = CPPI_QUEUE_CONTROL(CppiQueue0D, Queue);
        PoppedQueue = CPPI_QUEUE_READ(Controller, Register);
        RtlDebugPrint("SubmitQ 0x%x\n", Register);
        if ((PoppedQueue & CPPI_QUEUE_DESCRIPTOR_ADDRESS_MASK) ==
             Descriptor->Physical) {

            //
            // If the transfer was still on the submit queue, then there is a
            // descrepancy between the USB core, which thinks the transfer has
            // finished, and DMA, which clearly hasn't started the transfer
            // yet.
            //

            RtlDebugPrint("Found it on submit queue!\n");
            Descriptor->Submitted = FALSE;
        }

        ASSERT(FALSE);
    }

    if (CompletedSize != NULL) {
        if ((Packet != NULL) &&
            ((Packet->Status &
              CPPI_PACKET_DESCRIPTOR_STATUS_ZERO_LENGTH) == 0)) {

            *CompletedSize = Packet->Control &
                             CPPI_PACKET_DESCRIPTOR_CONTROL_LENGTH_MASK;

        } else {
            *CompletedSize = 0;
        }
    }

    return;
}

KSTATUS
CppiTearDownDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    )

/*++

Routine Description:

    This routine performs a teardown operation on the given descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor to tear down.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG Endpoint;
    ULONG Instance;
    RUNLEVEL OldRunLevel;
    ULONG PoppedQueue;
    ULONG Port;
    ULONG Queue;
    ULONG Register;
    KSTATUS Status;
    ULONG SubmitQueue;
    CPPI_DESCRIPTOR_DATA TeardownDescriptor;
    ULONGLONG Timeout;

    ASSERT(Descriptor->Submitted != FALSE);

    Status = CppiCreateDescriptor(Controller,
                                  Descriptor->Instance,
                                  &TeardownDescriptor);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    KeAcquireSpinLock(&(Controller->TeardownLock));
    CppipSubmitTeardownDescriptor(Controller, &TeardownDescriptor);
    Endpoint = Descriptor->Endpoint;
    Instance = Descriptor->Instance;
    Port = CPPI_ENDPOINT_TO_PORT(Instance, Endpoint);
    if (Descriptor->Transmit != FALSE) {

        //
        // The control registers are mostly write-only, so set up the entire
        // desired value again.
        //

        Control = CPPI_GET_TX_COMPLETION_QUEUE(Instance, Endpoint) |
                  CPPI_TX_CONTROL_CHANNEL_ENABLE;

        CPPI_WRITE_PORT(Controller,
                        CppiTxControl0,
                        Port,
                        Control | CPPI_TX_CONTROL_TEARDOWN);

        Queue = CPPI_GET_TX_COMPLETION_QUEUE(Instance, Endpoint);
        SubmitQueue = CPPI_GET_TX_QUEUE(Instance, Endpoint);

    } else {
        Control = CPPI_GET_RX_COMPLETION_QUEUE(Instance, Endpoint) |
                  CPPI_RX_CONTROL_CHANNEL_ENABLE |
                  CPPI_RX_CONTROL_RX_ERROR_HANDLING |
                  CPPI_RX_CONTROL_DEFAULT_DESCRIPTOR_HOST;

        CPPI_WRITE_PORT(Controller,
                        CppiRxControl0,
                        Port,
                        Control | CPPI_RX_CONTROL_TEARDOWN);

        Queue = CPPI_GET_RX_COMPLETION_QUEUE(Instance, Endpoint);
        SubmitQueue = CPPI_GET_FREE_QUEUE(Instance, Endpoint);
    }

    Timeout = HlQueryTimeCounter() + (HlQueryTimeCounterFrequency() * 5);
    while (TRUE) {

        //
        // Also set the teardown bit in the USBOTG control registers.
        //

        Am3UsbRequestTeardown(Controller,
                              Instance,
                              Endpoint,
                              Descriptor->Transmit);

        Register = CPPI_QUEUE_CONTROL(CppiQueue0D, Queue);
        PoppedQueue = CPPI_QUEUE_READ(Controller, Register);
        PoppedQueue &= CPPI_QUEUE_DESCRIPTOR_ADDRESS_MASK;
        if (PoppedQueue == 0) {
            if (HlQueryTimeCounter() > Timeout) {
                Status = STATUS_TIMEOUT;
                RtlDebugPrint("CPPI Failed to tear down: Registers: "
                              "SubmitQ 0x%x CompleteQ 0x%x Port 0x%x "
                              "Control 0x%x\n",
                              CPPI_QUEUE_CONTROL(CppiQueue0D, SubmitQueue),
                              CPPI_QUEUE_CONTROL(CppiQueue0D, Queue),
                              CPPI_PORT(CppiTxControl0, Port),
                              Control);

                ASSERT(FALSE);

                break;
            }

        //
        // First the descriptor should come to the completion queue.
        //

        } else if (PoppedQueue == Descriptor->Physical) {
            Descriptor->Submitted = FALSE;

        } else if (PoppedQueue == TeardownDescriptor.Physical) {
            TeardownDescriptor.Submitted = FALSE;
            Status = STATUS_SUCCESS;
            break;

        //
        // Something wacky jumped through the completion queue.
        //

        } else {

            ASSERT(FALSE);

            Status = STATUS_DEVICE_IO_ERROR;
            break;
        }
    }

    CppiDestroyDescriptor(Controller, &TeardownDescriptor);

    //
    // If the teardown descriptor came through but the original never did, try
    // to pop it from the submit queue.
    //

    if ((KSUCCESS(Status)) && (Descriptor->Submitted != FALSE)) {
        Register = CPPI_QUEUE_CONTROL(CppiQueue0D, SubmitQueue);
        PoppedQueue = CPPI_QUEUE_READ(Controller, Register);
        PoppedQueue &= CPPI_QUEUE_DESCRIPTOR_ADDRESS_MASK;
        if (PoppedQueue == Descriptor->Physical) {
            Descriptor->Submitted = FALSE;

        } else {

            //
            // The descriptor was neither in the completion queue or the submit
            // queue, something's not right.
            //

            ASSERT(FALSE);

            Status = STATUS_DEVICE_IO_ERROR;
        }
    }

    //
    // Put the port back together.
    //

    if (Descriptor->Transmit != FALSE) {
        CPPI_WRITE_PORT(Controller, CppiTxControl0, Port, Control);

    } else {
        CPPI_WRITE_PORT(Controller, CppiRxControl0, Port, Control);
    }

    KeReleaseSpinLock(&(Controller->TeardownLock));
    KeLowerRunLevel(OldRunLevel);
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CppipSubmitTeardownDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    )

/*++

Routine Description:

    This routine initializes and submits a teardown descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor to add to the hardware
        teardown queue.

Return Value:

    None.

--*/

{

    ULONG Register;
    PCPPI_TEARDOWN_DESCRIPTOR Teardown;
    ULONG Value;

    ASSERT(Descriptor->Submitted == FALSE);

    Teardown = Descriptor->Descriptor;
    Teardown->Control = CPPI_TEARDOWN_CONTROL_TYPE;
    Teardown->Reserved[0] = 0;
    Teardown->Reserved[1] = 0;
    Teardown->Reserved[2] = 0;
    Teardown->Reserved[3] = 0;
    Teardown->Reserved[4] = 0;
    Teardown->Reserved[5] = 0;
    Teardown->Reserved[6] = 0;
    Value = Descriptor->Physical |
            ((sizeof(CPPI_TEARDOWN_DESCRIPTOR) - 24) / 4);

    Descriptor->Submitted = TRUE;
    Register = CPPI_QUEUE_CONTROL(CppiQueue0D, CPPI_TEARDOWN_QUEUE);
    CPPI_QUEUE_WRITE(Controller, Register, Value);
    return;
}

