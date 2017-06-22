/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hdahw.c

Abstract:

    This module implements the Intel High Definition Audio interface with the
    hardware.

Author:

    Chris Stevens 3-Apr-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/intrface/pci.h>
#include "hda.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HdapResetController (
    PHDA_CONTROLLER Device
    );

KSTATUS
HdapSendCommand (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    UCHAR NodeId,
    ULONG Payload
    );

KSTATUS
HdapReceiveResponse (
    PHDA_CONTROLLER Device,
    UCHAR CodecAddress,
    PULONG Response
    );

VOID
HdapReapResponses (
    PHDA_CONTROLLER Controller
    );

KSTATUS
HdapSetDeviceState (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device,
    PSOUND_DEVICE_STATE_INFORMATION State
    );

VOID
HdapProcessDeviceStatus (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    );

KSTATUS
HdapAllocateStream (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    );

VOID
HdapFreeStream (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    );

VOID
HdapResetStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex,
    SOUND_DEVICE_TYPE DeviceType
    );

VOID
HdapInitializeStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex,
    PSOUND_IO_BUFFER Buffer,
    USHORT Format,
    UCHAR StreamNumber
    );

VOID
HdapStartStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex
    );

VOID
HdapStopStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex
    );

USHORT
HdapGetStreamFormat (
    ULONG SoundFormat,
    ULONG SampleRate,
    ULONG ChannelCount
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores the array of possible sample rates, in Hz. These are ordered based on
// the bit indices of the Supported PCM Size, Rates parameter.
//

HDA_RATE HdaSampleRates[] = {
    { 8000, HDA_FORMAT_SAMPLE_BASE_RATE_8000 },
    { 11025, HDA_FORMAT_SAMPLE_BASE_RATE_11025},
    { 16000, HDA_FORMAT_SAMPLE_BASE_RATE_16000 },
    { 22050, HDA_FORMAT_SAMPLE_BASE_RATE_22050 },
    { 32000, HDA_FORMAT_SAMPLE_BASE_RATE_32000 },
    { 44100, HDA_FORMAT_SAMPLE_BASE_RATE_44100 },
    { 48000, HDA_FORMAT_SAMPLE_BASE_RATE_48000 },
    { 88200, HDA_FORMAT_SAMPLE_BASE_RATE_88200 },
    { 96000, HDA_FORMAT_SAMPLE_BASE_RATE_96000 },
    { 176400, HDA_FORMAT_SAMPLE_BASE_RATE_176400 },
    { 192000, HDA_FORMAT_SAMPLE_BASE_RATE_192000 },
    { 384000, HDA_FORMAT_SAMPLE_BASE_RATE_384000 },
};

//
// Store the array of possible formats. These are ordered based on the bit
// indices of the Supports PCM Size, Rates parameter.
//

ULONG HdaPcmSizeFormats[] = {
    SOUND_FORMAT_8_BIT_UNSIGNED,
    SOUND_FORMAT_16_BIT_SIGNED_LITTLE_ENDIAN,
    0,
    SOUND_FORMAT_24_BIT_SIGNED_LITTLE_ENDIAN,
    SOUND_FORMAT_32_BIT_SIGNED_LITTLE_ENDIAN
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HdaSoundAllocateDmaBuffer (
    PVOID ControllerContext,
    PVOID DeviceContext,
    UINTN FragmentSize,
    UINTN FragmentCount,
    PIO_BUFFER *NewIoBuffer
    )

/*++

Routine Description:

    This routine allocates an I/O buffer that will be used for DMA by the sound
    device. The sound core will write data to it and read data from it. The
    allocation requirements are based on a fragment size and count. Each
    fragment will be used in a single DMA transfer and may need to be
    physically contiguous depending on the device's capabilities.

Arguments:

    ControllerContext - Supplies a pointer to the sound controller's context.

    DeviceContext - Supplies a pointer to the sound controller's device context.

    FragmentSize - Supplies the size of a fragments, in bytes.

    FragmentCount - Supplies the desired number of fragments.

    NewIoBuffer - Supplies a pointer that receives a pointer to the newly
        allocated buffer.

Return Value:

    Status code.

--*/

{

    PHDA_CONTROLLER Controller;
    ULONG Flags;
    PIO_BUFFER IoBuffer;
    PHYSICAL_ADDRESS MaximumPhysicalAddress;
    ULONG PageSize;
    UINTN Size;
    KSTATUS Status;

    Controller = (PHDA_CONTROLLER)ControllerContext;
    MaximumPhysicalAddress = MAX_ULONG;
    if ((Controller->Flags & HDA_CONTROLLER_FLAG_64_BIT_ADDRESSES) != 0) {
        MaximumPhysicalAddress = MAX_ULONGLONG;
    }

    //
    // Even on x86, these DMA buffers need to be mapped non-cached.
    //

    Flags = IO_BUFFER_FLAG_MAP_NON_CACHED;
    Size = FragmentSize * FragmentCount;

    //
    // If the fragment size is greater than a page size, then the buffer needs
    // to be physically contiguous. If the fragment size is less than a page
    // size, it should be a power of two and divide the page size evenly.
    //

    ASSERT(POWER_OF_2(FragmentSize) != FALSE);

    PageSize = MmPageSize();
    if (FragmentSize > PageSize) {
        Flags |= IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;

    } else {

        ASSERT((PageSize % FragmentSize) == 0);

    }

    ASSERT(Size < MAX_ULONG);

    IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                          MaximumPhysicalAddress,
                                          HDA_DMA_BUFFER_ALIGNMENT,
                                          Size,
                                          Flags);

    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto SoundAllocateDmaBufferEnd;
    }

    Status = STATUS_SUCCESS;

SoundAllocateDmaBufferEnd:
    if (!KSUCCESS(Status)) {
        if (IoBuffer != NULL) {
            MmFreeIoBuffer(IoBuffer);
            IoBuffer = NULL;
        }
    }

    *NewIoBuffer = IoBuffer;
    return Status;
}

VOID
HdaSoundFreeDmaBuffer (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER IoBuffer
    )

/*++

Routine Description:

    This routine destroys a sound DMA I/O buffer.

Arguments:

    ControllerContext - Supplies a pointer to the sound controller's context.

    DeviceContext - Supplies a pointer to the sound controller's device context.

    IoBuffer - Supplies a pointer to the buffer to destroy.

Return Value:

    None.

--*/

{

    MmFreeIoBuffer(IoBuffer);
    return;
}

KSTATUS
HdaSoundGetSetInformation (
    PVOID ControllerContext,
    PVOID DeviceContext,
    SOUND_DEVICE_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets and sets information for a sound device.

Arguments:

    ControllerContext - Supplies a pointer to the sound controller's context.

    DeviceContext - Supplies a pointer to the sound sontroller's device context.

    InformationType - Supplies the type of sound device information to get or
        set.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer. On output, this contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    PHDA_CONTROLLER Controller;
    PHDA_DEVICE HdaDevice;
    KSTATUS Status;
    ULONG Volume;

    Controller = (PHDA_CONTROLLER)ControllerContext;
    HdaDevice = (PHDA_DEVICE)DeviceContext;
    switch (InformationType) {
    case SoundDeviceInformationState:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED;
            goto SoundGetSetInformationEnd;
        }

        if (*DataSize < sizeof(SOUND_DEVICE_STATE_INFORMATION)) {
            *DataSize = sizeof(SOUND_DEVICE_STATE_INFORMATION);
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto SoundGetSetInformationEnd;
        }

        Status = HdapSetDeviceState(Controller, HdaDevice, Data);
        break;

    case SoundDeviceInformationVolume:
        if (Set == FALSE) {
            Status = STATUS_NOT_SUPPORTED;
            goto SoundGetSetInformationEnd;
        }

        if (*DataSize < sizeof(ULONG)) {
            *DataSize = sizeof(ULONG);
            Status = STATUS_DATA_LENGTH_MISMATCH;
            goto SoundGetSetInformationEnd;
        }

        Volume = *(PULONG)Data;
        HdapSetDeviceVolume(HdaDevice, Volume);
        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

SoundGetSetInformationEnd:
    return Status;
}

INTERRUPT_STATUS
HdaInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine implements the HDA interrupt service routine.

Arguments:

    Context - Supplies the context pointer given to the system when the
        interrupt was connected. In this case, this points to the AHCI
        controller.

Return Value:

    Interrupt status.

--*/

{

    PHDA_CONTROLLER Controller;
    PHDA_DEVICE Device;
    UCHAR RirbStatus;
    ULONG SoftwareInterrupts;
    USHORT StateChange;
    ULONG Status;
    ULONG StreamIndex;
    ULONG StreamMask;
    UCHAR StreamStatus;

    Controller = (PHDA_CONTROLLER)Context;
    Status = HDA_READ32(Controller, HdaRegisterInterruptStatus);
    if ((Status & HDA_INTERRUPT_STATUS_GLOBAL) == 0) {
        return InterruptStatusNotClaimed;
    }

    //
    // A little digging is required to clear these interrupts. The controller
    // interrupt was triggered by either the response buffer or the codec
    // status register.
    //

    SoftwareInterrupts = 0;
    if ((Status & HDA_INTERRUPT_STATUS_CONTROLLER) != 0) {
        StateChange = HDA_READ16(Controller, HdaRegisterStateChangeStatus);
        if (StateChange != 0) {
            HDA_WRITE16(Controller, HdaRegisterStateChangeStatus, StateChange);
        }

        //
        // Record that an RIRB interrupt fired in order to handle it at low
        // level and then clear it.
        //

        RirbStatus = HDA_READ8(Controller, HdaRegisterRirbStatus);
        if (RirbStatus != 0) {
            SoftwareInterrupts |= HDA_SOFTWARE_INTERRUPT_RESPONSE_BUFFER;
            HDA_WRITE8(Controller, HdaRegisterRirbStatus, RirbStatus);
        }

    }

    //
    // A stream interrupt is cleared by clearing the bits in the firing stream
    // status register. Save each stream's status to process them at low level.
    //

    StreamMask = (Status & HDA_INTERRUPT_STATUS_STREAM_MASK) >>
                 HDA_INTERRUPT_STATUS_STREAM_SHIFT;

    StreamIndex = 0;
    if (StreamMask != 0) {
        while (StreamMask != 0) {
            if ((StreamMask & 0x1) != 0) {
                StreamStatus = HDA_STREAM_READ8(Controller,
                                                StreamIndex,
                                                HdaStreamRegisterStatus);

                Device = Controller->StreamDevices[StreamIndex];
                if (Device != NULL) {
                    RtlAtomicOr32(&(Device->PendingStatus), StreamStatus);
                }

                HDA_STREAM_WRITE8(Controller,
                                  StreamIndex,
                                  HdaStreamRegisterStatus,
                                  StreamStatus);
            }

            StreamMask >>= 1;
            StreamIndex += 1;
        }

        SoftwareInterrupts |= HDA_SOFTWARE_INTERRUPT_STREAM;
    }

    if (SoftwareInterrupts != 0) {
        RtlAtomicOr32(&(Controller->PendingSoftwareInterrupts),
                      SoftwareInterrupts);
    }

    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
HdaInterruptServiceDpc (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine implements the HDA dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

{

    PHDA_CONTROLLER Controller;
    PHDA_DEVICE Device;
    ULONG Index;
    ULONG Pending;

    Controller = (PHDA_CONTROLLER)Parameter;
    Pending = RtlAtomicAnd32(&(Controller->PendingSoftwareInterrupts),
                             ~HDA_SOFTWARE_INTERRUPT_STREAM);

    if ((Pending & HDA_SOFTWARE_INTERRUPT_STREAM) == 0) {
        return InterruptStatusNotClaimed;
    }

    for (Index = 0; Index < Controller->StreamCount; Index += 1) {
        Device = Controller->StreamDevices[Index];
        if (Device == NULL) {
            continue;
        }

        if (Device->PendingStatus != 0) {
            HdapProcessDeviceStatus(Controller, Device);
        }
    }

    return InterruptStatusClaimed;
}

INTERRUPT_STATUS
HdaInterruptServiceWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine processes interrupts for the HDA controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt Status.

--*/

{

    PHDA_CONTROLLER Controller;
    ULONG Pending;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Controller = (PHDA_CONTROLLER)Parameter;
    Pending = RtlAtomicAnd32(&(Controller->PendingSoftwareInterrupts),
                             HDA_SOFTWARE_INTERRUPT_STREAM);

    if ((Pending & ~HDA_SOFTWARE_INTERRUPT_STREAM) == 0) {
        return InterruptStatusNotClaimed;
    }

    if ((Pending & HDA_SOFTWARE_INTERRUPT_RESPONSE_BUFFER) != 0) {
        HdapReapResponses(Controller);
    }

    return InterruptStatusClaimed;
}

KSTATUS
HdapInitializeDeviceStructures (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine allocates any resources needed to communicate with the HDA
    controller.

Arguments:

    Controller - Supplies a pointer to the controller context.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    ULONG BufferDescriptorListSize;
    ULONG BufferDescriptorListsTotalSize;
    USHORT CorbEntryCount;
    ULONG CorbSize;
    UCHAR Count;
    PIO_BUFFER IoBuffer;
    ULONG IoBufferFlags;
    PHYSICAL_ADDRESS MaxAddress;
    PHYSICAL_ADDRESS PhysicalAddress;
    USHORT RirbEntryCount;
    ULONG RirbSize;
    KSTATUS Status;
    ULONG Value;
    PVOID VirtualAddress;

    Controller->CommandLock = KeCreateQueuedLock();
    if (Controller->CommandLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    Controller->ControllerLock = KeCreateQueuedLock();
    if (Controller->ControllerLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Determine the size capabilities of the command output ring buffer (CORB)
    // and response input ring buffer (RIRB). Prefer the most number of entries.
    //

    Value = HDA_READ8(Controller, HdaRegisterCorbSize);
    if ((Value & HDA_CORB_SIZE_CAPABILITY_256) != 0) {
        CorbEntryCount = 256;

    } else if ((Value & HDA_CORB_SIZE_CAPABILITY_16) != 0) {
        CorbEntryCount = 16;

    } else if ((Value & HDA_CORB_SIZE_CAPABILITY_2) != 0) {
        CorbEntryCount = 2;

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
        goto InitializeDeviceStructuresEnd;
    }

    Value = HDA_READ8(Controller, HdaRegisterRirbSize);
    if ((Value & HDA_RIRB_SIZE_CAPABILITY_256) != 0) {
        RirbEntryCount = 256;

    } else if ((Value & HDA_RIRB_SIZE_CAPABILITY_16) != 0) {
        RirbEntryCount = 16;

    } else if ((Value & HDA_RIRB_SIZE_CAPABILITY_2) != 0) {
        RirbEntryCount = 2;

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
        goto InitializeDeviceStructuresEnd;
    }

    //
    // Record whether or not 64-bit addresses are allowed.
    //

    MaxAddress = MAX_ULONG;
    Value = HDA_READ16(Controller, HdaRegisterGlobalCapabilities);
    if ((Value & HDA_GLOBAL_CAPABILITIES_64_BIT_ADDRESSES_SUPPORTED) != 0) {
        Controller->Flags |= HDA_CONTROLLER_FLAG_64_BIT_ADDRESSES;
        MaxAddress = MAX_ULONGLONG;
    }

    //
    // Determine the number of stream descriptors.
    //

    Value = HDA_READ16(Controller, HdaRegisterGlobalCapabilities);
    Count = (Value & HDA_GLOBAL_CAPABILITIES_OUTPUT_STREAMS_SUPPORTED_MASK) >>
            HDA_GLOBAL_CAPABILITIES_OUTPUT_STREAMS_SUPPORTED_SHIFT;

    Controller->OutputStreamCount = Count;
    Count = (Value & HDA_GLOBAL_CAPABILITIES_INPUT_STREAMS_SUPPORTED_MASK) >>
            HDA_GLOBAL_CAPABILITIES_INPUT_STREAMS_SUPPORTED_SHIFT;

    Controller->InputStreamCount = Count;
    Count = (Value &
             HDA_GLOBAL_CAPABILITIES_BIDIRECTIONAL_STREAMS_SUPPORTED_MASK) >>
            HDA_GLOBAL_CAPABILITIES_BIDIRECTIONAL_STREAMS_SUPPORTED_SHIFT;

    Controller->BidirectionalStreamCount = Count;
    Controller->StreamCount = Controller->OutputStreamCount +
                                  Controller->InputStreamCount +
                                  Controller->BidirectionalStreamCount;

    //
    // Allocate an array of HDA device pointers to record which descriptors are
    // allocated.
    //

    AllocationSize = Controller->StreamCount * sizeof(PHDA_DEVICE);
    Controller->StreamDevices = MmAllocateNonPagedPool(AllocationSize,
                                                       HDA_ALLOCATION_TAG);

    if (Controller->StreamDevices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    RtlZeroMemory((PVOID)Controller->StreamDevices, AllocationSize);

    //
    // Each stream descriptor gets its own 128-byte aligned buffer descriptor
    // list. These could be allocated dynamically, but might as well not slow
    // down the I/O by needing to allocate buffers.
    //

    BufferDescriptorListSize = HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT *
                               sizeof(HDA_BUFFER_DESCRIPTOR_LIST_ENTRY);

    ASSERT(IS_ALIGNED(BufferDescriptorListSize,
                      HDA_BUFFER_DESCRIPTOR_LIST_ALIGNMENT) != FALSE);

    BufferDescriptorListsTotalSize = BufferDescriptorListSize *
                                     Controller->StreamCount;

    //
    // Allocate the buffer descriptor lists, CORB and RIRB. Align each buffer
    // up to the alignment requirement of the subsequent buffer.
    //

    BufferDescriptorListsTotalSize = ALIGN_RANGE_UP(
                                                BufferDescriptorListsTotalSize,
                                                HDA_CORB_ALIGNMENT);

    CorbSize = CorbEntryCount * sizeof(HDA_COMMAND_ENTRY);
    CorbSize = ALIGN_RANGE_UP(CorbSize, HDA_RIRB_ALIGNMENT);
    RirbSize = RirbEntryCount * sizeof(HDA_RESPONSE_ENTRY);
    AllocationSize = BufferDescriptorListsTotalSize + CorbSize + RirbSize;
    IoBufferFlags = IO_BUFFER_FLAG_MAP_NON_CACHED |
                    IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS;

    IoBuffer = MmAllocateNonPagedIoBuffer(0,
                                          MaxAddress,
                                          HDA_CORB_ALIGNMENT,
                                          AllocationSize,
                                          IoBufferFlags);

    if (IoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InitializeDeviceStructuresEnd;
    }

    ASSERT(IoBuffer->FragmentCount == 1);

    //
    // Record each buffer's starting address. They cannot be programmed until
    // the device is taken out of reset. The registers will not accept writes
    // until then.
    //

    Controller->IoBuffer = IoBuffer;
    PhysicalAddress = IoBuffer->Fragment[0].PhysicalAddress;
    VirtualAddress = IoBuffer->Fragment[0].VirtualAddress;
    Controller->BufferDescriptorLists = VirtualAddress;
    Controller->BufferDescriptorListsPhysical = PhysicalAddress;
    VirtualAddress += BufferDescriptorListsTotalSize;
    PhysicalAddress += BufferDescriptorListsTotalSize;
    Controller->CommandBuffer = VirtualAddress;
    Controller->CommandBufferPhysical = PhysicalAddress;
    Controller->CommandBufferEntryCount = CorbEntryCount;
    VirtualAddress += CorbSize;
    PhysicalAddress += CorbSize;
    Controller->ResponseBuffer = VirtualAddress;
    Controller->ResponseBufferPhysical = PhysicalAddress;
    Controller->ResponseBufferEntryCount = RirbEntryCount;
    Status = STATUS_SUCCESS;

InitializeDeviceStructuresEnd:
    if (!KSUCCESS(Status)) {
        HdapDestroyDeviceStructures(Controller);
    }

    return Status;
}

VOID
HdapDestroyDeviceStructures (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys all of the internal allocations made when
    initializing the controller structure.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

{

    HdapDestroyCodecs(Controller);
    if (Controller->CommandLock != NULL) {
        KeDestroyQueuedLock(Controller->CommandLock);
        Controller->CommandLock = NULL;
    }

    if (Controller->ControllerLock != NULL) {
        KeDestroyQueuedLock(Controller->ControllerLock);
        Controller->ControllerLock = NULL;
    }

    if (Controller->StreamDevices != NULL) {
        MmFreeNonPagedPool((PVOID)Controller->StreamDevices);
        Controller->StreamDevices = NULL;
    }

    if (Controller->IoBuffer != NULL) {
        MmFreeIoBuffer(Controller->IoBuffer);
        Controller->IoBuffer = NULL;
    }

    return;
}

KSTATUS
HdapInitializeController (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes an Intel HD Audio controller from the reset state.

Arguments:

    Controller - Supplies a pointer to the controller context.

Return Value:

    Status code.

--*/

{

    UCHAR Control;
    ULONG DescriptorCount;
    ULONG DescriptorListSize;
    ULONG Index;
    ULONG Interrupts;
    PHYSICAL_ADDRESS PhysicalAddress;
    UCHAR SizeEncoding;
    USHORT StateChange;
    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutInterval;
    ULONG Value;

    TimeoutInterval = HlQueryTimeCounterFrequency() * HDA_DEVICE_TIMEOUT;

    //
    // Reset the controller.
    //

    Status = HdapResetController(Controller);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    //
    // Reserve stream number 0, it is not allowed to be used.
    //

    Controller->StreamNumbers = HDA_STREAM_NUMBER_0;

    //
    // Initialize the stream descriptors by setting the buffer descriptor lists
    // for each.
    //

    DescriptorListSize = HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT *
                         sizeof(HDA_BUFFER_DESCRIPTOR_LIST_ENTRY);

    DescriptorCount = Controller->InputStreamCount +
                      Controller->OutputStreamCount +
                      Controller->BidirectionalStreamCount;

    PhysicalAddress = Controller->BufferDescriptorListsPhysical;
    for (Index = 0; Index < DescriptorCount; Index += 1) {
        HDA_STREAM_WRITE32(Controller,
                           Index,
                           HdaStreamRegisterBdlLowerBaseAddress,
                           (ULONG)PhysicalAddress);

        HDA_STREAM_WRITE32(Controller,
                           Index,
                           HdaStreamRegisterBdlUpperBaseAddress,
                           (ULONG)(PhysicalAddress >> 32));

        PhysicalAddress += DescriptorListSize;
    }

    //
    // Stop the command output ring buffer (CORB), then initialize and enable
    // it.
    //

    Value = HDA_READ8(Controller, HdaRegisterCorbControl);
    if ((Value & HDA_CORB_CONTROL_DMA_ENABLE) != 0) {
        Value &= ~HDA_CORB_CONTROL_DMA_ENABLE;
        HDA_WRITE8(Controller, HdaRegisterCorbControl, Value);
        Status = STATUS_TIMEOUT;
        Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
        do {
            Value = HDA_READ8(Controller, HdaRegisterCorbControl);
            if ((Value & HDA_CORB_CONTROL_DMA_ENABLE) == 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (KeGetRecentTimeCounter() < Timeout);

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    if (Controller->CommandBufferEntryCount == 256) {
        SizeEncoding = HDA_CORB_SIZE_256;

    } else if (Controller->CommandBufferEntryCount == 16) {
        SizeEncoding = HDA_CORB_SIZE_16;

    } else if (Controller->CommandBufferEntryCount == 2) {
        SizeEncoding = HDA_CORB_SIZE_2;

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
        goto InitializeControllerEnd;
    }

    SizeEncoding = (SizeEncoding << HDA_CORB_SIZE_SHIFT) & HDA_CORB_SIZE_MASK;
    HDA_WRITE8(Controller, HdaRegisterCorbSize, (UCHAR)SizeEncoding);
    HDA_WRITE32(Controller,
                HdaRegisterCorbLowerBaseAddress,
                (ULONG)Controller->CommandBufferPhysical);

    HDA_WRITE32(Controller,
                HdaRegisterCorbUpperBaseAddress,
                (ULONG)(Controller->CommandBufferPhysical >> 32));

    HDA_WRITE16(Controller,
                HdaRegisterCorbReadPointer,
                HDA_CORB_READ_POINTER_RESET);

    //
    // The reset is complete onces the bit can be read back. Some devices don't
    // operate according to the Intel HD audio specification and never
    // transition the reset bit to high. As a result, don't treat this as
    // fatal.
    //

    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    do {
        Value = HDA_READ16(Controller, HdaRegisterCorbReadPointer);
        if ((Value & HDA_CORB_READ_POINTER_RESET) != 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    //
    // Now clear the reset bit and wait for it to clear.
    //

    HDA_WRITE16(Controller, HdaRegisterCorbReadPointer, 0);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    do {
        Value = HDA_READ16(Controller, HdaRegisterCorbReadPointer);
        if ((Value & HDA_CORB_READ_POINTER_RESET) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

    HDA_WRITE16(Controller, HdaRegisterCorbWritePointer, 0);
    Controller->CommandNextWritePointer = 1;
    HDA_WRITE8(Controller, HdaRegisterCorbControl, HDA_CORB_CONTROL_DMA_ENABLE);

    //
    // Stop the response input ring buffer (RIRB), then initialize and enable
    // it.
    //

    Value = HDA_READ8(Controller, HdaRegisterRirbControl);
    if ((Value & HDA_RIRB_CONTROL_DMA_ENABLE) != 0) {
        Value &= ~HDA_RIRB_CONTROL_DMA_ENABLE;
        HDA_WRITE8(Controller, HdaRegisterRirbControl, Value);
        Status = STATUS_TIMEOUT;
        Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
        do {
            Value = HDA_READ8(Controller, HdaRegisterRirbControl);
            if ((Value & HDA_RIRB_CONTROL_DMA_ENABLE) == 0) {
                Status = STATUS_SUCCESS;
                break;
            }

        } while (KeGetRecentTimeCounter() < Timeout);

        if (!KSUCCESS(Status)) {
            goto InitializeControllerEnd;
        }
    }

    if (Controller->ResponseBufferEntryCount == 256) {
        SizeEncoding = HDA_RIRB_SIZE_256;

    } else if (Controller->ResponseBufferEntryCount == 16) {
        SizeEncoding = HDA_RIRB_SIZE_16;

    } else if (Controller->ResponseBufferEntryCount == 2) {
        SizeEncoding = HDA_RIRB_SIZE_2;

    } else {
        Status = STATUS_INVALID_CONFIGURATION;
        goto InitializeControllerEnd;
    }

    SizeEncoding = (SizeEncoding << HDA_RIRB_SIZE_SHIFT) & HDA_RIRB_SIZE_MASK;
    HDA_WRITE8(Controller, HdaRegisterRirbSize, (UCHAR)SizeEncoding);
    HDA_WRITE32(Controller,
                HdaRegisterRirbLowerBaseAddress,
                (ULONG)Controller->ResponseBufferPhysical);

    HDA_WRITE32(Controller,
                HdaRegisterRirbUpperBaseAddress,
                (ULONG)(Controller->ResponseBufferPhysical >> 32));

    HDA_WRITE16(Controller,
                HdaRegisterRirbWritePointer,
                HDA_RIRB_WRITE_POINTER_RESET);

    Controller->ResponseReadPointer = 0;

    //
    // The Intel HD Audio specification does not clearly describe the response
    // interrupt count, but it dictates how many responses should be received
    // before an RIRB interrupt is generated. Real hardware works even when
    // this is left at 0 (256 responses before an interrupt), emulated hardware
    // does not (e.g. Qemu).
    //

    HDA_WRITE16(Controller,
                HdaRegisterResponseInterruptCount,
                HDA_RESPONSE_INTERRUPT_COUNT_DEFAULT);

    Control = HDA_RIRB_CONTROL_DMA_ENABLE | HDA_RIRB_CONTROL_INTERRUPT_ENABLE;
    HDA_WRITE8(Controller, HdaRegisterRirbControl, Control);

    //
    // Before enabling interrupts, collect and clear the state change status.
    // It is only needed for enumeration and would be noisy otherwise. The
    // state change status should be available as soon as the controller comes
    // out of reset.
    //

    StateChange = HDA_READ16(Controller, HdaRegisterStateChangeStatus);
    HDA_WRITE16(Controller, HdaRegisterStateChangeStatus, StateChange);

    //
    // Enable interrupts. There is no easy way to clear any spurious interrupts
    // that have already appeared, so trust that the reset cleared all the
    // state.
    //

    Interrupts = HDA_INTERRUPT_CONTROL_GLOBAL_ENABLE |
                 HDA_INTERRUPT_CONTROL_CONTROLLER_ENABLE;

    HDA_WRITE32(Controller, HdaRegisterInterruptControl, Interrupts);

    //
    // Scan the codecs to determine their capabilities.
    //

    Status = HdapEnumerateCodecs(Controller, StateChange);
    if (!KSUCCESS(Status)) {
        goto InitializeControllerEnd;
    }

InitializeControllerEnd:
    return Status;
}

KSTATUS
HdapGetParameter (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    USHORT NodeId,
    HDA_PARAMETER ParameterId,
    PULONG Parameter
    )

/*++

Routine Description:

    This routine gets a parameter value for the node at the given codec
    address.

Arguments:

    Controller - Supplies a pointer to an HD Audio controller.

    CodecAddress - Supplies the address of the codec to which the node belongs.

    NodeId - Supplies the ID of the codec's node being queried.

    ParameterId - Supplies the ID of the parameter being requested.

    Parameter - Supplies a pointer that receives the parameter value.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = HdapGetSetVerb(Controller,
                            CodecAddress,
                            NodeId,
                            HdaVerbGetParameter,
                            ParameterId,
                            Parameter);

    return Status;
}

KSTATUS
HdapGetSetVerb (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    USHORT NodeId,
    USHORT Verb,
    USHORT Payload,
    PULONG Response
    )

/*++

Routine Description:

    This routine gets a verb value for the node at the given codec address.

Arguments:

    Controller - Supplies a pointer to an HD Audio controller.

    CodecAddress - Supplies the address of a codec.

    NodeId - Supplies the ID of the codec's node to which the command should be
        sent.

    Verb - Supplies the command verb.

    Payload - Supplies the command payload.

    Response - Supplies an optional pointer that receives the command's
        response.

Return Value:

    Status code.

--*/

{

    ULONG Command;
    KSTATUS Status;

    if (Verb > HDA_MAX_16_BIT_PAYLOAD_VERB) {
        Command = (Verb << 8) | (Payload & 0xFF);

    } else {
        Command = (Verb << 16) | Payload;
    }

    //
    // Send the get parameter command.
    //

    Status = HdapSendCommand(Controller, CodecAddress, NodeId, Command);
    if (!KSUCCESS(Status)) {
        goto GetParameterEnd;
    }

    //
    // If a response is required, then wait around until the interrupt
    // processing collects the response.
    //

    if (Response != NULL) {
        Status = HdapReceiveResponse(Controller, CodecAddress, Response);
        if (!KSUCCESS(Status)) {
            goto GetParameterEnd;
        }
    }

GetParameterEnd:
    return Status;
}

KSTATUS
HdapCommandBarrier (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress
    )

/*++

Routine Description:

    This routine synchronizes a batch of commands to make sure they have all
    completed before the driver continues operation on a codec.

Arguments:

    Controller - Supplies a pointer to the controller that owns the codec.

    CodecAddress - Supplies the address of the codec.

Return Value:

    Status code.

--*/

{

    //
    // Just receive the last response. And toss the value.
    //

    return HdapReceiveResponse(Controller, CodecAddress, NULL);
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HdapResetController (
    PHDA_CONTROLLER Device
    )

/*++

Routine Description:

    This routine resets a HD Audio device.

Arguments:

    Device - Supplies a pointer to the device that needs to be reset.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONGLONG Timeout;
    ULONGLONG TimeoutInterval;
    ULONG Value;

    TimeoutInterval = HlQueryTimeCounterFrequency() * HDA_DEVICE_TIMEOUT;

    //
    // Place the controller into reset.
    //

    HDA_WRITE32(Device, HdaRegisterGlobalControl, 0);
    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    do {
        Value = HDA_READ32(Device, HdaRegisterGlobalControl);
        if ((Value & HDA_GLOBAL_CONTROL_CONTROLLER_RESET) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    //
    // Per section 5.5.1.2 of the HD Audio specification, wait at least 100
    // microseconds for codec PLLs.
    //

    KeDelayExecution(FALSE, FALSE, HDA_CONTROLLER_RESET_DELAY);

    //
    // Take the controller out of reset.
    //

    HDA_WRITE32(Device,
                HdaRegisterGlobalControl,
                HDA_GLOBAL_CONTROL_CONTROLLER_RESET);

    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    do {
        Value = HDA_READ32(Device, HdaRegisterGlobalControl);
        if ((Value & HDA_GLOBAL_CONTROL_CONTROLLER_RESET) != 0) {
            Status = STATUS_SUCCESS;
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    if (!KSUCCESS(Status)) {
        goto ResetControllerEnd;
    }

    //
    // Wait at least 25 frames (> 521 microseconds) for any codecs to perform
    // self-enumeration.
    //

    KeDelayExecution(FALSE, FALSE, HDA_CODEC_ENUMERATION_DELAY);

ResetControllerEnd:
    return Status;
}

KSTATUS
HdapSendCommand (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    UCHAR NodeId,
    ULONG Payload
    )

/*++

Routine Description:

    This routine sends a command to the codec specified by the given address.

Arguments:

    Controller - Supplies a pointer to the HD Audio controller.

    CodecAddress - Supplies the address of the codec to which the command
        should be sent.

    NodeId - Supplies the ID of the node to which the command should be sent.

    Payload - Supplies the verb's payload.

Return Value:

    Status code.

--*/

{

    USHORT NextWritePointer;
    USHORT ReadPointer;
    ULONG Verb;

    //
    // Create the verb out of the given parameters.
    //

    Verb = ((CodecAddress << HDA_COMMAND_VERB_CODEC_ADDRESS_SHIFT) &
            HDA_COMMAND_VERB_CODEC_ADDRESS_MASK) |
           ((NodeId << HDA_COMMAND_VERB_NODE_ID_SHIFT) &
            HDA_COMMAND_VERB_NODE_ID_MASK) |
           ((Payload << HDA_COMMAND_VERB_PAYLOAD_SHIFT) &
            HDA_COMMAND_VERB_PAYLOAD_MASK);

    //
    // If the software write pointer equals the hardware read pointer, then the
    // command buffer is full. Wait until something is read.
    //

    KeAcquireQueuedLock(Controller->CommandLock);
    NextWritePointer = Controller->CommandNextWritePointer;
    do {
        ReadPointer = HDA_READ16(Controller, HdaRegisterCorbReadPointer);

    } while (ReadPointer == NextWritePointer);

    //
    // Write the command into the buffer and make sure that write completes
    // before the write pointer is updated.
    //

    Controller->CommandBuffer[NextWritePointer].Verb = Verb;
    RtlMemoryBarrier();
    HDA_WRITE16(Controller, HdaRegisterCorbWritePointer, NextWritePointer);
    NextWritePointer += 1;
    if (NextWritePointer == Controller->CommandBufferEntryCount) {
        NextWritePointer = 0;
    }

    Controller->CommandNextWritePointer = NextWritePointer;
    Controller->CodecPendingResponseCount[CodecAddress] += 1;
    KeReleaseQueuedLock(Controller->CommandLock);
    return STATUS_SUCCESS;
}

KSTATUS
HdapReceiveResponse (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    PULONG Response
    )

/*++

Routine Description:

    This routine receives the response for the last command sent to the given
    codec. It must wait until there are no more pending commands for the codec.
    It is up to the caller to make sure that no new commands are sent to the
    codec if the controller is waiting on a previous response.

Arguments:

    Controller - Supplies a pointer to the HD Audio controller.

    CodecAddress - Supplies the address of the codec from which to receive the
        response.

    Response - Supplies an optional pointer that receives the latest response
        in the codec.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;
    ULONGLONG Timeout;

    Status = STATUS_TIMEOUT;
    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * HDA_RESPONSE_TIMEOUT);

    //
    // Loop until all pending commands disappear.
    //

    do {
        if (Controller->CodecPendingResponseCount[CodecAddress] == 0) {
            if (Response != NULL) {
                *Response = Controller->CodecLastResponse[CodecAddress];
            }

            Status = STATUS_SUCCESS;
            break;
        }

        KeDelayExecution(FALSE, FALSE, 10 * MICROSECONDS_PER_MILLISECOND);

    } while (KeGetRecentTimeCounter() < Timeout);

    return Status;
}

VOID
HdapReapResponses (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine reaps all of the pending command responses for the
    controller's codecs.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

{

    UCHAR Address;
    PHDA_RESPONSE_ENTRY Entry;
    USHORT ReadPointer;
    USHORT WritePointer;

    KeAcquireQueuedLock(Controller->CommandLock);

    //
    // If the write pointer is not equal to the read pointer, than there
    // are entries to collect.
    //

    ReadPointer = Controller->ResponseReadPointer;
    WritePointer = HDA_READ16(Controller, HdaRegisterRirbWritePointer);
    while (ReadPointer != WritePointer) {
        ReadPointer += 1;
        if (ReadPointer == Controller->ResponseBufferEntryCount) {
            ReadPointer = 0;
        }

        Entry = &(Controller->ResponseBuffer[ReadPointer]);
        if ((Entry->ResponseExtended &
             HDA_RESPONSE_EXTENDED_FLAG_UNSOLICITED) != 0) {

            //
            // TODO: Handle Intel HD Audio unsolicited responses.
            //

            continue;
        }

        //
        // Store the response for the codec.
        //

        Address = (Entry->ResponseExtended &
                   HDA_RESPONSE_EXTENDED_FLAG_CODEC_ADDRESS_MASK) >>
                  HDA_RESPONSE_EXTENDED_FLAG_CODEC_ADDRESS_SHIFT;

        Controller->CodecLastResponse[Address] = Entry->Response;
        RtlMemoryBarrier();
        Controller->CodecPendingResponseCount[Address] -= 1;
    }

    Controller->ResponseReadPointer = ReadPointer;
    KeReleaseQueuedLock(Controller->CommandLock);
    return;
}

KSTATUS
HdapSetDeviceState (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device,
    PSOUND_DEVICE_STATE_INFORMATION State
    )

/*++

Routine Description:

    This routine puts the given widget into the provided state.

Arguments:

    Controller - Supplies a pointer to the controller to which the widget
        belongs.

    Device - Supplies a pointer to the device that should be put into the given
        state.

    State - Supplies a pointer to the new state information.

Return Value:

    Status code.

--*/

{

    USHORT Format;
    PHDA_PATH Path;
    KSTATUS Status;

    if (State->Version < SOUND_DEVICE_STATE_INFORMATION_VERSION) {
        return STATUS_VERSION_MISMATCH;
    }

    //
    // Do nothing if the device is already in the desired state.
    //

    if (State->State == Device->State) {
        return STATUS_SUCCESS;
    }

    Status = STATUS_SUCCESS;
    switch (State->State) {

    //
    // Setting the device to the uninitialized state clears out any resources
    // allocated by initialization.
    //

    case SoundDeviceStateUninitialized:
        if (Device->StreamIndex != HDA_INVALID_STREAM) {
            HdapStopStream(Controller, Device->StreamIndex);
            HdapFreeStream(Controller, Device);
        }

        break;

    //
    // Initializing the device allocates the necessary controller resources to
    // transition to the running state.
    //

    case SoundDeviceStateInitialized:

        //
        // Allocate a stream if necessary. The stream number is what connects
        // the stream descriptor to the device's widget. It is programmed into
        // both ends below.
        //

        if (Device->StreamIndex == HDA_INVALID_STREAM) {
            Status = HdapAllocateStream(Controller, Device);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        //
        // The stream format is set in both the allocated descriptor and the
        // device's main widget (DAC or ADC). It is formed using the format,
        // sample rate, and channel count supplied by the sound core library.
        //

        Format = HdapGetStreamFormat(State->U.Initialize.Format,
                                     State->U.Initialize.SampleRate,
                                     State->U.Initialize.ChannelCount);

        //
        // Prepre the device (i.e. the path of widgets) by making sure its
        // powered on, enabled, and has the proper format and volume set.
        //

        Path = (PHDA_PATH)State->U.Initialize.RouteContext;
        Status = HdapEnableDevice(Device, Path, Format);
        if (!KSUCCESS(Status)) {
            break;
        }

        Status = HdapSetDeviceVolume(Device, State->U.Initialize.Volume);
        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // Reset the stream to clear out any old state and the initialize it
        // for use by this device.
        //

        HdapResetStream(Controller,
                        Device->StreamIndex,
                        Device->SoundDevice.Type);

        HdapInitializeStream(Controller,
                             Device->StreamIndex,
                             State->U.Initialize.Buffer,
                             Format,
                             Device->StreamNumber);

        //
        // Record the FIFO size now that the stream has been initialized. If
        // the FIFO size is dynamic, it should update immediately after the
        // format is changed and remain static until the format changes again.
        //

        Device->StreamFifoSize = HDA_STREAM_READ16(Controller,
                                                   Device->StreamIndex,
                                                   HdaStreamRegisterFifoSize);

        //
        // Save the sound buffer in the device handle.
        //

        Device->Buffer = State->U.Initialize.Buffer;
        break;

    //
    // The running state turns on the DMA engine.
    //

    case SoundDeviceStateRunning:
        HdapStartStream(Controller, Device->StreamIndex);
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

    if (KSUCCESS(Status)) {
        Device->State = State->State;
    }

    return Status;
}

VOID
HdapProcessDeviceStatus (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    )

/*++

Routine Description:

    This routine processes the given device's interrupt status. It's goal is
    to either update the sound core about the controller's position in the
    buffer or to stop the stream if it about to play the same data again.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

    Device - Supplies a pointer to the HDA device whose interrupt status needs
        to be processed.

Return Value:

    None.

--*/

{

    UINTN Offset;
    ULONG Status;

    Status = RtlAtomicExchange32(&(Device->PendingStatus), 0);
    if (Status == 0) {
        return;
    }

    //
    // One or more fragments have been processed, notify sound core.
    //

    if ((Status & HDA_STREAM_STATUS_BUFFER_COMPLETE) != 0) {
        Offset = HDA_STREAM_READ32(Controller,
                                   Device->StreamIndex,
                                   HdaStreamRegisterLinkPositionInBuffer);

        //
        // For output devices, the fragment completion interrupt is fired as
        // soon as the last of the fragment has been loaded into the FIFO.
        // Sound core really wants a report when the fragment is complete and
        // wants the offset to reflect that. Adding the FIFO length was tried,
        // but isn't enough in practice. Sometimes the offset is still short of
        // a fragment boundary by a few bytes. It's unclear when the interrupt
        // actually fires. The audio is already in the FIFO, so consider it
        // played rather than waiting around for the link position to change.
        // If higher precision is needed, then some extra work could be
        // scheduled to report exactly when the link position moves to the next
        // fragment. To make sound core happy, align the offset to the nearest
        // fragment.
        //

        ASSERT(POWER_OF_2(Device->Buffer->FragmentSize) != FALSE);

        if (REMAINDER(Offset, Device->Buffer->FragmentSize) <
            (Device->Buffer->FragmentSize / 2)) {

            Offset = ALIGN_RANGE_DOWN(Offset, Device->Buffer->FragmentSize);

        } else {
            Offset = ALIGN_RANGE_UP(Offset, Device->Buffer->FragmentSize);
        }

        //
        // The buffer size should be a power of 2, so just mask off the size.
        //

        ASSERT(POWER_OF_2(Device->Buffer->Size) != FALSE);

        Offset = REMAINDER(Offset, Device->Buffer->Size);
        SoundUpdateBufferState(Device->Buffer,
                               Device->SoundDevice.Type,
                               Offset);
    }

    if (((Status & HDA_STREAM_STATUS_DESCRIPTOR_ERROR) != 0) ||
        ((Status & HDA_STREAM_STATUS_FIFO_ERROR) != 0)) {

        RtlDebugPrint("HDA: Stream Error: 0x%x08x, 0x%08x\n", Device, Status);
    }

    return;
}

KSTATUS
HdapAllocateStream (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    )

/*++

Routine Description:

    This routine allocates a stream for the given device.

Arguments:

    Controller - Supplies a pointer to a controller that owns the streams.

    Device - Supplies a pointer to an HDA device in need of a stream.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    KSTATUS Status;
    ULONG StreamCount;
    ULONG StreamIndex;
    ULONG StreamNumber;
    ULONG StreamOffset;
    BOOL TriedBidirectional;

    if (Device->SoundDevice.Type == SoundDeviceInput) {
        StreamCount = Controller->InputStreamCount;
        StreamOffset = 0;

    } else {

        ASSERT(Device->SoundDevice.Type == SoundDeviceOutput);

        StreamCount = Controller->OutputStreamCount;
        StreamOffset = Controller->InputStreamCount;
    }

    if ((StreamCount == 0) && (Controller->BidirectionalStreamCount == 0)) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    KeAcquireQueuedLock(Controller->ControllerLock);

    //
    // First attempt to allocate a stream number. These are different than the
    // stream descriptors.
    //

    for (StreamNumber = 0;
         StreamNumber < HDA_STREAM_NUMBER_COUNT;
         StreamNumber += 1) {

        if ((Controller->StreamNumbers & (1 << StreamNumber)) == 0) {
            break;
        }
    }

    if (StreamNumber == HDA_STREAM_NUMBER_COUNT) {
        Status = STATUS_RESOURCE_IN_USE;
        goto AllocateStreamEnd;
    }

    //
    // Now allocate a stream descriptor. Try the unidirectional and the
    // bidirectional streams.
    //

    StreamIndex = HDA_INVALID_STREAM;
    TriedBidirectional = FALSE;
    while (TRUE) {
        for (Index = 0; Index < StreamCount; Index += 1) {
            StreamIndex = StreamOffset + Index;
            if (Controller->StreamDevices[StreamIndex] == NULL) {
                break;
            }
        }

        if ((Index != StreamCount) || (TriedBidirectional != FALSE)) {
            break;
        }

        StreamCount = Controller->BidirectionalStreamCount;
        StreamOffset = Controller->InputStreamCount +
                       Controller->OutputStreamCount;

        TriedBidirectional = TRUE;
    }

    if (Index == StreamCount) {
        Status = STATUS_RESOURCE_IN_USE;
        goto AllocateStreamEnd;
    }

    Controller->StreamNumbers |= (1 << StreamNumber);
    Controller->StreamDevices[StreamIndex] = Device;
    Device->StreamNumber = StreamNumber;
    Device->StreamIndex = StreamIndex;
    Status = STATUS_SUCCESS;

AllocateStreamEnd:
    KeReleaseQueuedLock(Controller->ControllerLock);
    return Status;
}

VOID
HdapFreeStream (
    PHDA_CONTROLLER Controller,
    PHDA_DEVICE Device
    )

/*++

Routine Description:

    This routine releases the stream allocated by the given device.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

    Device - Supplies a pointer to the device that owns the descriptor to
        release.

Return Value:

    None.

--*/

{

    ASSERT(Device->StreamIndex != HDA_INVALID_STREAM);
    ASSERT(Controller->StreamDevices[Device->StreamIndex] != NULL);
    ASSERT(Device->StreamNumber != HDA_INVALID_STREAM_NUMBER);
    ASSERT((Controller->StreamNumbers & (1 << Device->StreamNumber)) != 0);

    KeAcquireQueuedLock(Controller->ControllerLock);
    Controller->StreamDevices[Device->StreamIndex] = NULL;
    Controller->StreamNumbers &= ~(1 << Device->StreamNumber);
    KeReleaseQueuedLock(Controller->ControllerLock);
    Device->StreamNumber = HDA_INVALID_STREAM_NUMBER;
    Device->StreamIndex = HDA_INVALID_STREAM;
    return;
}

VOID
HdapResetStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex,
    SOUND_DEVICE_TYPE Type
    )

/*++

Routine Description:

    This routine resets a stream descriptor.

Arguments:

    Controller - Supplies a pointer to the controller to which the descriptor
        belongs.

    StreamIndex - Supplies the index of the stream to reset.

    Type - Supplies the type of sound device that will be using the stream.
        This only matters if the stream is bidirectional.

Return Value:

    None.

--*/

{

    ULONG BdlLowerAddress;
    ULONG BdlUpperAddress;
    ULONG BidirectionalOffset;
    ULONGLONG Timeout;
    ULONGLONG TimeoutInterval;
    ULONG Value;

    TimeoutInterval = (HlQueryTimeCounterFrequency() * HDA_STREAM_TIMEOUT) /
                      MILLISECONDS_PER_SECOND;

    //
    // When the controller was initialized, the buffer descriptor list was
    // saved in the register. Preserve that over the reset.
    //

    BdlLowerAddress = HDA_STREAM_READ32(Controller,
                                        StreamIndex,
                                        HdaStreamRegisterBdlLowerBaseAddress);

    BdlUpperAddress = HDA_STREAM_READ32(Controller,
                                        StreamIndex,
                                        HdaStreamRegisterBdlUpperBaseAddress);

    //
    // Write the stream reset bit in the descriptor and wait for it to set.
    // Don't fail if it is never read back as one as Qemu does not follow the
    // specification here.
    //

    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterControl,
                       HDA_STREAM_CONTROL_RESET);

    do {
        Value = HDA_STREAM_READ32(Controller,
                                  StreamIndex,
                                  HdaStreamRegisterControl);

        if ((Value & HDA_STREAM_CONTROL_RESET) != 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    //
    // Take the stream out of reset and wait for the reset bit to unset.
    //

    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterControl,
                       0);

    do {
        Value = HDA_STREAM_READ32(Controller,
                                  StreamIndex,
                                  HdaStreamRegisterControl);

        if ((Value & HDA_STREAM_CONTROL_RESET) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    //
    // The bidirectional input/output bit must be set before any other stream
    // descriptor registers are written. If the stream index is less than the
    // bidirectional offset, don't program the register.
    //

    if ((Type == SoundDeviceOutput) &&
        (Controller->BidirectionalStreamCount != 0)) {

        BidirectionalOffset = Controller->InputStreamCount +
                              Controller->OutputStreamCount;

        if (StreamIndex >= BidirectionalOffset) {
            HDA_STREAM_WRITE32(Controller,
                               StreamIndex,
                               HdaStreamRegisterControl,
                               HDA_STREAM_CONTROL_BIDIRECTIONAL_OUTPUT);
        }
    }

    //
    // Always restore the buffer descriptor list.
    //

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterBdlLowerBaseAddress,
                       BdlLowerAddress);

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterBdlUpperBaseAddress,
                       BdlUpperAddress);

    return;
}

VOID
HdapInitializeStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex,
    PSOUND_IO_BUFFER Buffer,
    USHORT Format,
    UCHAR StreamNumber
    )

/*++

Routine Description:

    This routine initializes a stream descriptor and its associated buffer
    descriptor list.

Arguments:

    Controller - Supplies a pointer to the HDA controller.

    StreamIndex - Supplies the index of the stream to initialize.

    Buffer - Supplies a pointer to the sound I/O buffer to program into the
        stream.

    Format - Supplies the format of the data stored in the buffer.

    StreamNumber - Supplies the stream nubmer to use for this stream.

Return Value:

    None.

--*/

{

    ULONG Control;
    PHDA_BUFFER_DESCRIPTOR_LIST_ENTRY Entry;
    UINTN Index;
    PIO_BUFFER_FRAGMENT IoFragment;
    UINTN IoFragmentOffset;

    //
    // Set up the buffer descriptor list.
    //

    IoFragmentOffset = 0;
    IoFragment = &(Buffer->IoBuffer->Fragment[0]);
    Entry = HDA_GET_STREAM_BDL(Controller, StreamIndex);
    for (Index = 0; Index < Buffer->FragmentCount; Index += 1) {
        Entry->Address = IoFragment->PhysicalAddress + IoFragmentOffset;
        Entry->Length = Buffer->FragmentSize;
        Entry->Flags = HDA_BUFFER_DESCRIPTOR_FLAG_INTERRUPT_ON_COMPLETION;
        Entry += 1;
        IoFragmentOffset += Buffer->FragmentSize;
        if (IoFragmentOffset == IoFragment->Size) {
            IoFragment += 1;
            IoFragmentOffset = 0;
        }
    }

    //
    // Initialize the descriptor registers.
    //

    HDA_STREAM_WRITE16(Controller,
                       StreamIndex,
                       HdaStreamRegisterFormat,
                       Format);

    ASSERT(Buffer->FragmentCount <=
           HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT);

    HDA_STREAM_WRITE16(Controller,
                       StreamIndex,
                       HdaStreamRegisterLastValidIndex,
                       Buffer->FragmentCount - 1);

    ASSERT(Buffer->Size <= MAX_ULONG);

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterCyclicBufferLength,
                       Buffer->Size);

    Control = HDA_STREAM_READ32(Controller,
                                StreamIndex,
                                HdaStreamRegisterControl);

    Control |= (StreamNumber << HDA_STREAM_CONTROL_STREAM_NUMBER_SHIFT) &
               HDA_STREAM_CONTROL_STREAM_NUMBER_MASK;

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterControl,
                       Control);

    return;
}

VOID
HdapStartStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex
    )

/*++

Routine Description:

    This routine starts a stream, enabling its DMA engine.

Arguments:

    Controller - Supplies a pointer to the HDA controller.

    StreamIndex - Supplies the index of the stream to start.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG Interrupts;
    ULONG Sync;

    //
    // Protect access to the interrupt and synchronization registers.
    //

    KeAcquireQueuedLock(Controller->ControllerLock);

    //
    // Enable interrupts for this stream descriptor.
    //

    Interrupts = HDA_READ32(Controller, HdaRegisterInterruptControl);
    Interrupts |= HDA_INTERRUPT_CONTROL_GLOBAL_ENABLE;
    Interrupts |= (1 << StreamIndex);
    HDA_WRITE32(Controller, HdaRegisterInterruptControl, Interrupts);

    //
    // Enable the stream descriptors DMA engine.
    //

    Sync = HDA_READ32(Controller, Controller->StreamSynchronizationRegister);
    Sync |= 1 << StreamIndex;
    HDA_WRITE32(Controller, Controller->StreamSynchronizationRegister, Sync);
    RtlMemoryBarrier();
    Control = HDA_STREAM_READ32(Controller,
                                StreamIndex,
                                HdaStreamRegisterControl);

    Control |= HDA_STREAM_CONTROL_TRAFFIC_PRIORITY |
               HDA_STREAM_CONTROL_COMPLETION_INTERRUPT_ENABLE |
               HDA_STREAM_CONTROL_DMA_ENABLE;

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterControl,
                       Control);

    Sync = HDA_READ32(Controller, Controller->StreamSynchronizationRegister);
    Sync &= ~(1 << StreamIndex);
    HDA_WRITE32(Controller, Controller->StreamSynchronizationRegister, Sync);
    KeReleaseQueuedLock(Controller->ControllerLock);
    return;
}

VOID
HdapStopStream (
    PHDA_CONTROLLER Controller,
    ULONG StreamIndex
    )

/*++

Routine Description:

    This routine stops a stream by disabling the DMA enable bit and waiting
    for it to clear.

Arguments:

    Controller - Supplies a pointer to the HDA controller.

    StreamIndex - Supplies the index of the stream to stop.

Return Value:

    None.

--*/

{

    ULONG Control;
    ULONG Interrupts;
    ULONG Sync;
    ULONGLONG Timeout;
    ULONGLONG TimeoutInterval;

    //
    // Protect access to the synchronization and interrupt registers.
    //

    KeAcquireQueuedLock(Controller->ControllerLock);
    Sync = HDA_READ32(Controller, Controller->StreamSynchronizationRegister);
    Sync |= 1 << StreamIndex;
    HDA_WRITE32(Controller, Controller->StreamSynchronizationRegister, Sync);
    Control = HDA_STREAM_READ32(Controller,
                                StreamIndex,
                                HdaStreamRegisterControl);

    Control &= ~(HDA_STREAM_CONTROL_COMPLETION_INTERRUPT_ENABLE |
                 HDA_STREAM_CONTROL_DMA_ENABLE);

    HDA_STREAM_WRITE32(Controller,
                       StreamIndex,
                       HdaStreamRegisterControl,
                       Control);

    //
    // Wait for the run bit to read back as 0. It should happen within 40
    // microseconds according to the spec (Section 4.5.4 Stopping Streams), but
    // add a timeout just to be safe.
    //

    TimeoutInterval = (HlQueryTimeCounterFrequency() * HDA_STREAM_TIMEOUT) /
                      MILLISECONDS_PER_SECOND;

    Timeout = KeGetRecentTimeCounter() + TimeoutInterval;
    do {
        Control = HDA_STREAM_READ32(Controller,
                                    StreamIndex,
                                    HdaStreamRegisterControl);

        if ((Control & HDA_STREAM_CONTROL_DMA_ENABLE) == 0) {
            break;
        }

    } while (KeGetRecentTimeCounter() < Timeout);

    //
    // Clear the format register. VirtualBox 5.1.2 and below do not stop the
    // stream properly unless this is cleared.
    //

    HDA_STREAM_WRITE16(Controller, StreamIndex, HdaStreamRegisterFormat, 0);

    //
    // Disable interrupts for this stream descriptor.
    //

    Interrupts = HDA_READ32(Controller, HdaRegisterInterruptControl);
    Interrupts &= ~(1 << StreamIndex);
    HDA_WRITE32(Controller, HdaRegisterInterruptControl, Interrupts);
    Sync = HDA_READ32(Controller, Controller->StreamSynchronizationRegister);
    Sync &= ~(1 << StreamIndex);
    HDA_WRITE32(Controller, Controller->StreamSynchronizationRegister, Sync);
    KeReleaseQueuedLock(Controller->ControllerLock);
    return;
}

USHORT
HdapGetStreamFormat (
    ULONG SoundFormat,
    ULONG SampleRate,
    ULONG ChannelCount
    )

/*++

Routine Description:

    This routine converts the given parameters into an HDA stream format value
    that encodes the sample rate, bit depth, and channel count. This value is
    created from a sound library format (SOUND_FORMAT_*), a raw sample rate in
    Hz, and a channel count.

Arguments:

    SoundFormat - Supplies the sound library format to encode. See
        SOUND_FORMAT_* for definitions.

    SampleRate - Supplies the raw sample rate, in Hz, to encode.

    ChannelCount - Supplies the channel count to encode.

Return Value:

    Returns an HDA stream format value.

--*/

{

    ULONG BitsPerSample;
    ULONG Count;
    USHORT Format;
    ULONG Index;

    Format = 0;
    Count = sizeof(HdaSampleRates) / sizeof(HdaSampleRates[0]);
    for (Index = 0; Index < Count; Index += 1) {
        if (HdaSampleRates[Index].Rate == SampleRate) {
            Format |= (HdaSampleRates[Index].Format <<
                       HDA_FORMAT_SAMPLE_BASE_RATE_SHIFT) &
                      HDA_FORMAT_SAMPLE_BASE_RATE_MASK;

            break;
        }
    }

    ASSERT(Index != Count);

    if (SoundFormat == SOUND_FORMAT_AC3) {
        Format |= HDA_FORMAT_NON_PCM;
        BitsPerSample = HDA_FORMAT_BITS_PER_SAMPLE_16;

    } else if (SoundFormat == SOUND_FORMAT_FLOAT) {
        Format |= HDA_FORMAT_NON_PCM;
        BitsPerSample = HDA_FORMAT_BITS_PER_SAMPLE_32;

    } else {
        BitsPerSample = 0;
        Count = sizeof(HdaPcmSizeFormats) / sizeof(HdaPcmSizeFormats[0]);
        for (Index = 0; Index < Count; Index += 1) {
            if (SoundFormat == HdaPcmSizeFormats[Index]) {
                BitsPerSample = Index;
                break;
            }
        }

        ASSERT(Index != Count);
    }

    Format |= (BitsPerSample << HDA_FORMAT_BITS_PER_SAMPLE_SHIFT) &
              HDA_FORMAT_BITS_PER_SAMPLE_MASK;

    Format |= ((ChannelCount - 1) << HDA_FORMAT_NUMBER_OF_CHANNELS_SHIFT) &
              HDA_FORMAT_NUMBER_OF_CHANNELS_MASK;

    return Format;
}

