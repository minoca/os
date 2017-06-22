/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    pwma.c

Abstract:

    This module implements the Broadcom 27xx PWM Audio driver.

Author:

    Chris Stevens 2-May-2017

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/kernel/acpi.h>
#include <minoca/soc/b2709os.h>
#include <minoca/soc/bcm2709.h>
#include <minoca/dma/dma.h>
#include <minoca/dma/dmab2709.h>
#include <minoca/sound/sndcore.h>

//
// --------------------------------------------------------------------- Macros
//

#define BCM27_READ_PWMA(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define BCM27_WRITE_PWMA(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

#define BCM27_PWMA_ALLOCATION_TAG 0x416D7750 // 'AmwP'

//
// Define the minimum number of channels supported by PWM audio.
//

#define BCM27_PWMA_MIN_CHANNEL_COUNT 1

//
// Define the maximum number of channels supported by PWM audio.
//

#define BCM27_PWMA_MAX_CHANNEL_COUNT 2

//
// Define the number of sample rates supported by PWM audio.
//

#define BCM27_PWMA_SAMPLE_RATE_COUNT 2

//
// Define the number of routes.
//

#define BCM27_PWMA_ROUTE_COUNT 1

//
// Define the minimum and maximum fragment counts supported by PWM audio. The
// minimum is 2 and the maximum is 256, which happens to be the maximum number
// of control blocks supported by the DMA controller.
//

#define BCM27_PWMA_FRAGMENT_COUNT_MIN 2
#define BCM27_PWMA_FRAGMENT_COUNT_MAX 256

//
// Define the minimum fragment size, in bytes. This must be a power of 2.
//

#define BCM27_PWMA_FRAGMENT_SIZE_MIN 256

//
// Define the maximum fragment size, in bytes. This must be a power of 2.
//

#define BCM27_PWMA_FRAGMENT_SIZE_MAX 0x40000000

//
// Define the maximum buffer size, in bytes. This must be a power of 2.
//

#define BCM27_PWMA_BUFFER_SIZE_MAX 0x80000000

//
// Define the mask and value for the upper byte of the physical addresses that
// must be supplied to the DMA controller.
//

#define BCM27_PWMA_DEVICE_ADDRESS_MASK  0xFF000000
#define BCM27_PWMA_DEVICE_ADDRESS_VALUE 0x7E000000

//
// Define the default PWM panic and data request thresholds. The default is 7
// bits (for a byte). Set them to 15, as this device uses 16-bit audio.
//

#define BCM27_PWMA_PANIC_DEFAULT 15
#define BCM27_PWMA_DATA_REQUEST_DEFAULT 15

//
// Define the maximum volume and its shift.
//

#define BCM27_PWMA_MAX_VOLUME 128
#define BCM27_PWMA_MAX_VOLUME_SHIFT 7

//
// Define the PCM sample size, in bytes.
//

#define BCM27_PWMA_PCM_SAMPLE_SIZE (16 / BITS_PER_BYTE)

//
// Define the PWM sample size, in bytes.
//

#define BCM27_PWMA_PWM_SAMPLE_SIZE (32 / BITS_PER_BYTE)

//
// Define the conversion ratio between PCM and PWM samples.
//

#define BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES \
    (BCM27_PWMA_PWM_SAMPLE_SIZE / BCM27_PWMA_PCM_SAMPLE_SIZE)

//
// Define the values used to convert PCM to PWM. The conversion takes the
// signed 16-bit PCM value and adds (max signed value + 1) to make everything
// unsigned. It then converts to within the PWM range by multiplying by the
// range and dividing by the max unsigned 16-bit value (plus 1 to make the
// divide a shift).
//

#define BCM27_PWMA_CONVERSION_VALUE 0x8000
#define BCM27_PWMA_CONVERSION_SHIFT 16

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a PWM audio device. This is useful for statically
    allocating a audio device template.

Members:

    SoundDevice - Stores the sound device information.

    SampelRates - Stores the supported sample rates.

    Route - Stores an array of available routes.

--*/

typedef struct _BCM27_PWMA_DEVICE {
    SOUND_DEVICE SoundDevice;
    ULONG SampleRates[BCM27_PWMA_SAMPLE_RATE_COUNT];
    SOUND_DEVICE_ROUTE Routes[BCM27_PWMA_ROUTE_COUNT];
} PACKED BCM27_PWMA_DEVICE, *PBCM27_PWMA_DEVICE;

/*++

Structure Description:

    This structure defines a Broadcom 27xx PWM Audio stream.

Members:

    Public - Stores the public device information passed to the sound core.

    State - Stores the state of the stream.

    Buffer - Stores a pointer to the sound core buffer for the stream.

    BufferPosition - Stores a pointer to the current position within the sound
        buffer.

    Range - Stores the current range programmed into the device. This is used
        to convert the PCM data into a value between 1 and the range.

    RangeShift - Stores the number of bits to shift to get the maximum range
        value, assuming it is a power of 2. If it is not, this member is not
        valid.

    ChannelCount - Stores the current channel count of the device.

    Volume - Stores the device volume. This stores both the left and right
       channel volume. If the device does not support separate channel volume
       control, it should use the left channel volume. The stored values are
       between 0 and 128 to make software volume conversion a little easier.

--*/

typedef struct _BCM27_PWMA_DEVICE_INTERNAL {
    BCM27_PWMA_DEVICE Public;
    SOUND_DEVICE_STATE State;
    PSOUND_IO_BUFFER Buffer;
    UINTN BufferPosition;
    ULONG Range;
    ULONG RangeShift;
    ULONG ChannelCount;
    UCHAR Volume[BCM27_PWMA_MAX_CHANNEL_COUNT];
} BCM27_PWMA_DEVICE_INTERNAL, *PBCM27_PWMA_DEVICE_INTERNAL;

/*++

Structure Description:

    This structure defines the context for a Broadcom 27xx PWM Audio device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    SoundController - Stores a pointer to the sound core library's controller.

    ControllerBase - Stores the virtual address of the memory mapping to the
        PWM registers.

    DmaResource - Stores a pointer to the DMA resource allocated for this
        controller.

    Dma - Stores a pointer to the DMA interface.

    DmaTransfer - Stores a pointer to the DMA transfer used for I/O.

    DmaMinAddress - Stores the lowest physical address (inclusive) that the DMA
        controller can access.

    DmaMaxAddress - Stores the highest physical address (inclusive) that the
        DMA controller can access.

    Device - Stores information on the controller's one audio device.

    PwmClockFrequency - Stores the current PWM clock frequency, in Hz.

--*/

typedef struct _BCM27_PWMA_CONTROLLER {
    PDEVICE OsDevice;
    PSOUND_CONTROLLER SoundController;
    PVOID ControllerBase;
    PHYSICAL_ADDRESS ControllerBasePhysicalAddress;
    PRESOURCE_ALLOCATION DmaResource;
    PDMA_INTERFACE Dma;
    PDMA_TRANSFER DmaTransfer;
    PHYSICAL_ADDRESS DmaMinAddress;
    PHYSICAL_ADDRESS DmaMaxAddress;
    BCM27_PWMA_DEVICE_INTERNAL Device;
    ULONG PwmClockFrequency;
} BCM27_PWMA_CONTROLLER, *PBCM27_PWMA_CONTROLLER;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Bcm27PwmaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
Bcm27PwmaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27PwmaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27PwmaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27PwmaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27PwmaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
Bcm27PwmaDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

KSTATUS
Bcm27PwmaSoundAllocateDmaBuffer (
    PVOID ControllerContext,
    PVOID DeviceContext,
    UINTN FragmentSize,
    UINTN FragmentCount,
    PIO_BUFFER *NewIoBuffer
    );

VOID
Bcm27PwmaSoundFreeDmaBuffer (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER IoBuffer
    );

KSTATUS
Bcm27PwmaSoundGetSetInformation (
    PVOID ControllerContext,
    PVOID DeviceContext,
    SOUND_DEVICE_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

KSTATUS
Bcm27PwmaSoundCopyBufferData (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN Size
    );

KSTATUS
Bcm27PwmapStartController (
    PIRP Irp,
    PBCM27_PWMA_CONTROLLER Controller
    );

KSTATUS
Bcm27PwmapInitializeDma (
    PBCM27_PWMA_CONTROLLER Controller
    );

VOID
Bcm27PwmapDmaInterfaceCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

VOID
Bcm27PwmapSystemDmaCompletion (
    PDMA_TRANSFER Transfer
    );

KSTATUS
Bcm27PwmapSetDeviceState (
    PBCM27_PWMA_CONTROLLER Controller,
    PBCM27_PWMA_DEVICE_INTERNAL Device,
    PSOUND_DEVICE_STATE_INFORMATION State
    );

VOID
Bcm27PwmapSetVolume (
    PBCM27_PWMA_DEVICE_INTERNAL Device,
    ULONG Volume
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER Bcm27PwmaDriver = NULL;
UUID Bcm27PwmaDmaUuid = UUID_DMA_INTERFACE;
UUID Bcm27PwmaDmaBcm2709Uuid = UUID_DMA_BCM2709_CONTROLLER;

//
// Store the sound core interface function table.
//

SOUND_FUNCTION_TABLE Bcm27PwmaSoundFunctionTable = {
    Bcm27PwmaSoundAllocateDmaBuffer,
    Bcm27PwmaSoundFreeDmaBuffer,
    Bcm27PwmaSoundGetSetInformation,
    Bcm27PwmaSoundCopyBufferData,
};

//
// Define the PWM audio device template. As the PCM sound data needs to encoded
// into a PWM format, this device cannot be mmaped.
//

BCM27_PWMA_DEVICE Bcm27PwmAudioDeviceTemplate = {
    {
        SOUND_DEVICE_VERSION,
        sizeof(BCM27_PWMA_DEVICE),
        NULL,
        SoundDeviceOutput,
        SOUND_DEVICE_FLAG_PRIMARY,
        (SOUND_CAPABILITY_CHANNEL_STEREO |
         SOUND_CAPABILITY_OUTPUT |
         SOUND_CAPABILITY_INTERFACE_ANALOG_OUT),
        SOUND_FORMAT_16_BIT_SIGNED_LITTLE_ENDIAN,
        BCM27_PWMA_MIN_CHANNEL_COUNT,
        BCM27_PWMA_MAX_CHANNEL_COUNT,
        BCM27_PWMA_SAMPLE_RATE_COUNT,
        FIELD_OFFSET(BCM27_PWMA_DEVICE, SampleRates),
        BCM27_PWMA_ROUTE_COUNT,
        FIELD_OFFSET(BCM27_PWMA_DEVICE, Routes),
    },

    {
        44100, 48000
    },

    {
        {
            SoundDeviceRouteHeadphone,
            NULL
        },
    },
};

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the Broadcom 2709 PWM Audio driver. It
    registers its other dispatch functions, and performs driver-wide
    initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    Bcm27PwmaDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = Bcm27PwmaAddDevice;
    FunctionTable.DispatchStateChange = Bcm27PwmaDispatchStateChange;
    FunctionTable.DispatchOpen = Bcm27PwmaDispatchOpen;
    FunctionTable.DispatchClose = Bcm27PwmaDispatchClose;
    FunctionTable.DispatchIo = Bcm27PwmaDispatchIo;
    FunctionTable.DispatchSystemControl = Bcm27PwmaDispatchSystemControl;
    FunctionTable.DispatchUserControl = Bcm27PwmaDispatchUserControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    return Status;
}

KSTATUS
Bcm27PwmaAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which this driver
    acts as the function driver. The driver will attach itself to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PBCM27_PWMA_CONTROLLER Controller;

    Controller = MmAllocateNonPagedPool(sizeof(BCM27_PWMA_CONTROLLER),
                                        BCM27_PWMA_ALLOCATION_TAG);

    if (Controller == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    RtlZeroMemory(Controller, sizeof(BCM27_PWMA_CONTROLLER));
    Controller->OsDevice = DeviceToken;
    return IoAttachDriverToDevice(Driver, DeviceToken, Controller);
}

VOID
Bcm27PwmaDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpUp) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:
            IoCompleteIrp(Bcm27PwmaDriver, Irp, STATUS_SUCCESS);
            break;

        case IrpMinorStartDevice:
            Status = Bcm27PwmapStartController(Irp, DeviceContext);
            if (!KSUCCESS(Status)) {
                IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
            }

            break;

        default:
            break;
        }
    }

    return;
}

VOID
Bcm27PwmaDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PBCM27_PWMA_CONTROLLER Controller;
    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;

    Controller = (PBCM27_PWMA_CONTROLLER)DeviceContext;
    Status = SoundOpenDevice(Controller->SoundController,
                             Irp->U.Open.FileProperties,
                             Irp->U.Open.DesiredAccess,
                             Irp->U.Open.OpenFlags,
                             Irp->U.Open.IoState,
                             &SoundHandle);

    if (!KSUCCESS(Status)) {
        goto DispatchOpenEnd;
    }

    Irp->U.Open.DeviceContext = SoundHandle;

DispatchOpenEnd:
    IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
    return;
}

VOID
Bcm27PwmaDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSOUND_DEVICE_HANDLE SoundHandle;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.Close.DeviceContext;
    SoundCloseDevice(SoundHandle);
    IoCompleteIrp(Bcm27PwmaDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
Bcm27PwmaDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    IO_OFFSET IoOffset;
    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;
    BOOL Write;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.ReadWrite.DeviceContext;
    Write = FALSE;
    if (Irp->MinorCode == IrpMinorIoWrite) {
        Write = TRUE;
    }

    IoOffset = Irp->U.ReadWrite.IoOffset;
    Status = SoundPerformIo(SoundHandle,
                            Irp->U.ReadWrite.IoBuffer,
                            &IoOffset,
                            Irp->U.ReadWrite.IoSizeInBytes,
                            Irp->U.ReadWrite.IoFlags,
                            Irp->U.ReadWrite.TimeoutInMilliseconds,
                            Write,
                            &(Irp->U.ReadWrite.IoBytesCompleted));

    Irp->U.ReadWrite.NewIoOffset = IoOffset;
    if (!KSUCCESS(Status)) {
        goto DispatchIoEnd;
    }

DispatchIoEnd:
    IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
    return;
}

VOID
Bcm27PwmaDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PVOID Context;
    PBCM27_PWMA_CONTROLLER Controller;
    PSYSTEM_CONTROL_DEVICE_INFORMATION DeviceInformationRequest;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    KSTATUS Status;

    Controller = (PBCM27_PWMA_CONTROLLER)DeviceContext;
    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = SoundLookupDevice(Controller->SoundController, Lookup);
        IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
        break;

    //
    // Succeed for the basics.
    //

    case IrpMinorSystemControlWriteFileProperties:
    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(Bcm27PwmaDriver, Irp, STATUS_SUCCESS);
        break;

    case IrpMinorSystemControlDeviceInformation:
        DeviceInformationRequest = Irp->U.SystemControl.SystemContext;
        Status = SoundGetSetDeviceInformation(
                                         Controller->SoundController,
                                         &(DeviceInformationRequest->Uuid),
                                         DeviceInformationRequest->Data,
                                         &(DeviceInformationRequest->DataSize),
                                         DeviceInformationRequest->Set);

        IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

    }

    return;
}

VOID
Bcm27PwmaDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles User Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PSOUND_DEVICE_HANDLE SoundHandle;
    KSTATUS Status;

    SoundHandle = (PSOUND_DEVICE_HANDLE)Irp->U.UserControl.DeviceContext;
    Status = SoundUserControl(SoundHandle,
                              Irp->U.UserControl.FromKernelMode,
                              (ULONG)Irp->MinorCode,
                              Irp->U.UserControl.UserBuffer,
                              Irp->U.UserControl.UserBufferSize);

    if (!KSUCCESS(Status)) {
        goto DispatchUserControlEnd;
    }

DispatchUserControlEnd:
    IoCompleteIrp(Bcm27PwmaDriver, Irp, Status);
    return;
}

KSTATUS
Bcm27PwmaSoundAllocateDmaBuffer (
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

    PBCM27_PWMA_CONTROLLER Controller;
    ULONG Flags;
    PIO_BUFFER IoBuffer;
    ULONG PageSize;
    UINTN Size;
    KSTATUS Status;

    Controller = (PBCM27_PWMA_CONTROLLER)ControllerContext;
    Flags = 0;

    //
    // Double the fragment size, because the the DMA controller has a minimum
    // write length of 32-bits. The 2D stride functionality does not work for
    // 16-bit transfers. The 16-bit PCM values supplied by sound core will be
    // converted into 32-bit values by the PWM audio driver. Thus, twice as
    // much space is needed for each fragment.
    //

    FragmentSize *= BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES;
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

    IoBuffer = MmAllocateNonPagedIoBuffer(Controller->DmaMinAddress,
                                          Controller->DmaMaxAddress,
                                          0,
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
Bcm27PwmaSoundFreeDmaBuffer (
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
Bcm27PwmaSoundGetSetInformation (
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

    PBCM27_PWMA_CONTROLLER Controller;
    PBCM27_PWMA_DEVICE_INTERNAL Device;
    KSTATUS Status;
    ULONG Volume;

    Controller = (PBCM27_PWMA_CONTROLLER)ControllerContext;
    Device = (PBCM27_PWMA_DEVICE_INTERNAL)DeviceContext;
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

        Status = Bcm27PwmapSetDeviceState(Controller, Device, Data);
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
        Bcm27PwmapSetVolume(Device, Volume);
        Status = STATUS_SUCCESS;
        break;

    default:
        Status = STATUS_NOT_SUPPORTED;
        break;
    }

SoundGetSetInformationEnd:
    return Status;
}

KSTATUS
Bcm27PwmaSoundCopyBufferData (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN Size
    )

/*++

Routine Description:

    This routine copies sound data from one I/O buffer to another. This gives
    the sound controller an opportunity to do any conversions if its audio
    format does not conform to one of sound core's formats. One of the two
    buffers will be the buffer supplied to the sound controller when the
    device was put in the initialized state. Which one it is depends on the
    direction of the device.

Arguments:

    ControllerContext - Supplies a pointer to the sound controller's context.

    DeviceContext - Supplies a pointer to the sound sontroller's device context.

    Destination - Supplies a pointer to the destination I/O buffer that is to
        be copied into.

    DestinationOffset - Supplies the offset into the destination I/O buffer
        where the copy should begin.

    Source - Supplies a pointer to the source I/O buffer whose contexts will be
        copied to the destination.

    SourceOffset - Supplies the offset into the source I/O buffer where the
        copy should begin.

    Size - Supplies the size of the copy, in bytes.

Return Value:

    Status code.

--*/

{

    UINTN BytesRemaining;
    ULONG Channel;
    PBCM27_PWMA_DEVICE_INTERNAL Device;
    PVOID FlushAddress;
    UINTN FlushAlignment;
    UINTN FlushSize;
    PIO_BUFFER_FRAGMENT Fragment;
    UINTN FragmentIndex;
    UINTN FragmentOffset;
    PUSHORT PcmAddress;
    UINTN PcmBytesThisRound;
    PIO_BUFFER_FRAGMENT PcmFragment;
    UINTN PcmOffset;
    PULONG PwmAddress;
    UINTN PwmBytesThisRound;
    PIO_BUFFER_FRAGMENT PwmFragment;
    UINTN PwmOffset;
    UINTN SampleIndex;
    ULONG SampleUlong;
    USHORT SampleUshort;
    KSTATUS Status;
    UCHAR Volume[BCM27_PWMA_MAX_CHANNEL_COUNT];

    Device = (PBCM27_PWMA_DEVICE_INTERNAL)DeviceContext;

    //
    // PWM Audio only supports output devices.
    //

    ASSERT(Device->Public.SoundDevice.Type == SoundDeviceOutput);

    //
    // The destination buffer should be the same as the DMA buffer.
    //

    ASSERT(Destination == Device->Buffer->IoBuffer);

    //
    // The size and offset better be aligned to a 16-bit sample boundary. The
    // buffer's internal offset better be 0 as well. In order to convert
    // between 16-bit PCM data and 32-bit PWM data, this routine manipulates
    // the buffer offset itself and would be broken by an internal offset.
    //

    ASSERT(IS_ALIGNED(Size, BCM27_PWMA_PCM_SAMPLE_SIZE) != FALSE);
    ASSERT(IS_ALIGNED(DestinationOffset, BCM27_PWMA_PCM_SAMPLE_SIZE) != FALSE);
    ASSERT(MmGetIoBufferCurrentOffset(Destination) == 0);
    ASSERT(Device->Public.SoundDevice.Formats ==
           SOUND_FORMAT_16_BIT_SIGNED_LITTLE_ENDIAN);

    //
    // The source is an unknown. It is likely from user mode. Copy all of the
    // contents into the destination to move it into memory that can be freely
    // manipulated. With the destination offset, sound core assumes the
    // destination values are the same size as the source values. The source
    // values are  16-bit PCM values and need to be converted to 32-bit PWM
    // values. Convert the offset and write the 16-bit source values to cover
    // the first half of the 32-bit PWM value region.
    //

    DestinationOffset *= BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES;
    Status = MmCopyIoBuffer(Destination,
                            DestinationOffset,
                            Source,
                            SourceOffset,
                            Size);

    if (!KSUCCESS(Status)) {
        goto SoundCopyBufferDataEnd;
    }

    //
    // The destination offset is at the start of the 32-bit PWM value region,
    // which now holds the 16-bit PCM values. The conversion will iterate
    // backwards through the PCM values, as to not overwrite them. Determine
    // the byte offset after the last of the PCM and PWM values.
    //

    PcmOffset = DestinationOffset + Size;
    PwmOffset = DestinationOffset + (Size * BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES);

    //
    // Determine which fragment the PCM offset belongs to and its offset within
    // that fragment.
    //

    Fragment = NULL;
    FragmentOffset = 0;
    for (FragmentIndex = 0;
         FragmentIndex < Destination->FragmentCount;
         FragmentIndex += 1) {

        Fragment = &(Destination->Fragment[FragmentIndex]);
        if ((FragmentOffset + Fragment->Size) >= PcmOffset) {
            FragmentOffset = PcmOffset - FragmentOffset;
            break;
        }

        FragmentOffset += Fragment->Size;
    }

    PcmFragment = Fragment;
    PcmOffset = FragmentOffset;

    ASSERT(Fragment != NULL);
    ASSERT(FragmentIndex != Destination->FragmentCount);

    //
    // Do the same for the PWM offset.
    //

    Fragment = NULL;
    FragmentOffset = 0;
    for (FragmentIndex = 0;
         FragmentIndex < Destination->FragmentCount;
         FragmentIndex += 1) {

        Fragment = &(Destination->Fragment[FragmentIndex]);
        if ((FragmentOffset + Fragment->Size) >= PwmOffset) {
            FragmentOffset = PwmOffset - FragmentOffset;
            break;
        }

        FragmentOffset += Fragment->Size;
    }

    PwmOffset = FragmentOffset;
    PwmFragment = Fragment;

    ASSERT(Fragment != NULL);
    ASSERT(FragmentIndex != Destination->FragmentCount);

    //
    // Get the left and right channel volumes. If there is only one channel,
    // fill both entries in the array with the left channel's volume. This way
    // the conversion code works the same for mono and stereo.
    //

    Volume[0] = Device->Volume[0];
    if (Device->ChannelCount == 1) {
        Volume[1] = Volume[0];

    } else {
        Volume[1] = Device->Volume[1];
    }

    //
    // Even samples are the left channel; odd are the right channel.
    //

    Channel = (PcmOffset / BCM27_PWMA_PCM_SAMPLE_SIZE) %
              BCM27_PWMA_MAX_CHANNEL_COUNT;

    //
    // Iterate over the buffer converting from PCM format to PWM format. Linear
    // PCM describes the amplitude of the sample and PWM describes the number
    // of bits set over the programmed range (an approximation of amplitude).
    // Incorporate the volume as well, as there is no hardware volume control
    // for the PWM audio controller. This iterates backwards through the 16-bit
    // PCM samples as to not overwrite them with the larger 32-bit PWM samples.
    //

    FlushAlignment = MmGetIoBufferAlignment();
    BytesRemaining = Size;
    while (BytesRemaining != 0) {
        PcmAddress = PcmFragment->VirtualAddress + PcmOffset;
        PwmAddress = PwmFragment->VirtualAddress + PwmOffset;
        PcmBytesThisRound = PcmOffset;
        if (PcmBytesThisRound > BytesRemaining) {
            PcmBytesThisRound = BytesRemaining;
        }

        PwmBytesThisRound = PcmBytesThisRound *
                            BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES;

        //
        // The samples are signed values between [-32,768, 32,767]. Convert
        // these to unsigned values between [0, 65,535] by adding 32,768.
        //

        if (POWER_OF_2(Device->Range) != FALSE) {
            for (SampleIndex = 0;
                 SampleIndex < (PcmBytesThisRound / BCM27_PWMA_PCM_SAMPLE_SIZE);
                 SampleIndex += 1) {

                PwmAddress -= 1;
                PcmAddress -= 1;
                SampleUshort = *PcmAddress + BCM27_PWMA_CONVERSION_VALUE;
                SampleUlong = SampleUshort >>
                              (BCM27_PWMA_CONVERSION_SHIFT -
                               Device->RangeShift);

                SampleUlong *= Volume[Channel];
                SampleUlong >>= BCM27_PWMA_MAX_VOLUME_SHIFT;

                ASSERT(*PwmAddress <= Device->Range);

                *PwmAddress = SampleUlong;
                Channel = (Channel + 1) % BCM27_PWMA_MAX_CHANNEL_COUNT;
            }

        } else {
            for (SampleIndex = 0;
                 SampleIndex < (PcmBytesThisRound / BCM27_PWMA_PCM_SAMPLE_SIZE);
                 SampleIndex += 1) {

                PwmAddress -= 1;
                PcmAddress -= 1;
                SampleUshort = *PcmAddress + BCM27_PWMA_CONVERSION_VALUE;
                SampleUlong = SampleUshort * Device->Range;
                SampleUlong >>= BCM27_PWMA_CONVERSION_SHIFT;
                SampleUlong *= Volume[Channel];
                SampleUlong >>= BCM27_PWMA_MAX_VOLUME_SHIFT;

                ASSERT(SampleUlong <= Device->Range);

                *PwmAddress = SampleUlong;
                Channel = (Channel + 1) % BCM27_PWMA_MAX_CHANNEL_COUNT;
            }
        }

        //
        // Flush the converted region.
        //

        FlushAddress = ALIGN_POINTER_DOWN(PwmAddress, FlushAlignment);
        FlushSize = PwmBytesThisRound +
                    REMAINDER((UINTN)PwmAddress, FlushAlignment);

        FlushSize = ALIGN_RANGE_UP(FlushSize, FlushAlignment);
        MmFlushBufferForDataOut(FlushAddress, FlushSize);
        BytesRemaining -= PcmBytesThisRound;
        PcmOffset -= PcmBytesThisRound;
        PwmOffset -= PwmBytesThisRound;
        if (PcmOffset == 0) {
            PcmFragment -= 1;
            PcmOffset = PcmFragment->Size;
        }

        if (PwmOffset == 0) {
            PwmFragment -= 1;
            PwmOffset = PwmFragment->Size;
        }
    }

    Status = STATUS_SUCCESS;

SoundCopyBufferDataEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Bcm27PwmapStartController (
    PIRP Irp,
    PBCM27_PWMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine starts the Intel HD Audio controller.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Controller - Supplies a pointer to the controller information.

Return Value:

    Status code.

--*/

{

    ULONG AlignmentOffset;
    PRESOURCE_ALLOCATION Allocation;
    PRESOURCE_ALLOCATION_LIST AllocationList;
    PBCM2709_TABLE Bcm2709Table;
    PRESOURCE_ALLOCATION ControllerBase;
    PHYSICAL_ADDRESS EndAddress;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    SOUND_CONTROLLER_INFORMATION Registration;
    UINTN Size;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;

    ControllerBase = NULL;
    Size = 0;

    //
    // Loop through the allocated resources to get the controller base and the
    // DMA resources.
    //

    AllocationList = Irp->U.StartDevice.ProcessorLocalResources;
    Allocation = IoGetNextResourceAllocation(AllocationList, NULL);
    while (Allocation != NULL) {

        //
        // Look for the first physical address reservation, the registers.
        //

        if (Allocation->Type == ResourceTypePhysicalAddressSpace) {
            if (ControllerBase == NULL) {
                ControllerBase = Allocation;
            }

        } else if (Allocation->Type == ResourceTypeDmaChannel) {
            Controller->DmaResource = Allocation;
        }

        //
        // Get the next allocation in the list.
        //

        Allocation = IoGetNextResourceAllocation(AllocationList, Allocation);
    }

    //
    // Fail to start if the controller base was not found.
    //

    if (ControllerBase == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto StartControllerEnd;
    }

    //
    // Map the controller.
    //

    if (Controller->ControllerBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerBase->Allocation;
        EndAddress = PhysicalAddress + ControllerBase->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerBase->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (UINTN)(EndAddress - PhysicalAddress);
        Controller->ControllerBase = MmMapPhysicalAddress(PhysicalAddress,
                                                          Size,
                                                          TRUE,
                                                          FALSE,
                                                          TRUE);

        if (Controller->ControllerBase == NULL) {
            Status = STATUS_NO_MEMORY;
            goto StartControllerEnd;
        }

        Controller->ControllerBasePhysicalAddress = ControllerBase->Allocation;
        Controller->ControllerBase += AlignmentOffset;
    }

    ASSERT(Controller->ControllerBase != NULL);

    //
    // The PWM clock rate is stored in the Broadcom 2709 ACPI table.
    //

    Bcm2709Table = AcpiFindTable(BCM2709_SIGNATURE, NULL);
    if (Bcm2709Table == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto StartControllerEnd;
    }

    Controller->PwmClockFrequency = Bcm2709Table->PwmClockFrequency;

    //
    // Initialize the DMA engine for the PWM transfers.
    //

    Status = Bcm27PwmapInitializeDma(Controller);
    if (!KSUCCESS(Status)) {
        goto StartControllerEnd;
    }

    //
    // Register with the sound core library.
    //

    if (Controller->SoundController == NULL) {
        RtlCopyMemory(&(Controller->Device.Public),
                      &Bcm27PwmAudioDeviceTemplate,
                      sizeof(BCM27_PWMA_DEVICE));

        Controller->Device.Public.SoundDevice.Context = &(Controller->Device);
        Controller->Device.State = SoundDeviceStateUninitialized;
        RtlZeroMemory(&Registration, sizeof(SOUND_CONTROLLER_INFORMATION));
        Registration.Version = SOUND_CONTROLLER_INFORMATION_VERSION;
        Registration.Context = Controller;
        Registration.OsDevice = Controller->OsDevice;
        Registration.Flags = SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER;
        Registration.FunctionTable = &Bcm27PwmaSoundFunctionTable;
        Registration.MinFragmentCount = BCM27_PWMA_FRAGMENT_COUNT_MIN;
        Registration.MaxFragmentCount = BCM27_PWMA_FRAGMENT_COUNT_MAX;
        Registration.MinFragmentSize = BCM27_PWMA_FRAGMENT_SIZE_MIN;
        Registration.MaxFragmentSize = BCM27_PWMA_FRAGMENT_SIZE_MAX;
        Registration.MaxBufferSize = BCM27_PWMA_BUFFER_SIZE_MAX;
        Registration.DeviceCount = 1;
        SoundDevice = &(Controller->Device.Public.SoundDevice);
        Registration.Devices = &SoundDevice;
        Status = SoundCreateController(&Registration,
                                       &(Controller->SoundController));

        if (!KSUCCESS(Status)) {
            goto StartControllerEnd;
        }
    }

StartControllerEnd:
    if (!KSUCCESS(Status)) {
        if (Controller->ControllerBase != NULL) {
            MmUnmapAddress(Controller->ControllerBase, Size);
            Controller->ControllerBase = NULL;
        }

        if (Controller->SoundController != NULL) {
            SoundDestroyController(Controller->SoundController);
            Controller->SoundController = NULL;
        }
    }

    return Status;
}

KSTATUS
Bcm27PwmapInitializeDma (
    PBCM27_PWMA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine attempts to wire up the Broadcom 2709 DMA controller to the
    PWM controller.

Arguments:

    Controller - Supplies a pointer to the PWM audio controller.

Return Value:

    Status code.

--*/

{

    BOOL Equal;
    DMA_INFORMATION Information;
    PRESOURCE_ALLOCATION Resource;
    KSTATUS Status;
    PDMA_TRANSFER Transfer;

    Resource = Controller->DmaResource;

    ASSERT(Resource != NULL);

    Status = IoRegisterForInterfaceNotifications(&Bcm27PwmaDmaUuid,
                                                 Bcm27PwmapDmaInterfaceCallback,
                                                 Resource->Provider,
                                                 Controller,
                                                 TRUE);

    if (!KSUCCESS(Status)) {
        goto InitializeDmaEnd;
    }

    if (Controller->Dma == NULL) {
        Status = STATUS_NOT_SUPPORTED;
        goto InitializeDmaEnd;
    }

    RtlZeroMemory(&Information, sizeof(DMA_INFORMATION));
    Information.Version = DMA_INFORMATION_VERSION;
    Status = Controller->Dma->GetInformation(Controller->Dma, &Information);
    if (!KSUCCESS(Status)) {
        goto InitializeDmaEnd;
    }

    Equal = RtlAreUuidsEqual(&(Information.ControllerUuid),
                             &Bcm27PwmaDmaBcm2709Uuid);

    if (Equal == FALSE) {
        Status = STATUS_NOT_SUPPORTED;
        goto InitializeDmaEnd;
    }

    //
    // PWM audio needs to run in a continuous loop. The DMA controller must
    // support this.
    //

    if ((Information.Capabilities & DMA_CAPABILITY_CONTINUOUS_MODE) == 0) {
        Status = STATUS_NOT_SUPPORTED;
        goto InitializeDmaEnd;
    }

    Controller->DmaMinAddress = Information.MinAddress;
    Controller->DmaMaxAddress = Information.MaxAddress;
    if (Controller->DmaTransfer == NULL) {
        Status = Controller->Dma->AllocateTransfer(Controller->Dma, &Transfer);
        if (!KSUCCESS(Status)) {
            goto InitializeDmaEnd;
        }

        //
        // Fill in some of the fields that will never change transfer to
        // transfer.
        //

        Controller->DmaTransfer = Transfer;
        Transfer->Allocation = Resource;
        Transfer->Configuration = NULL;
        Transfer->ConfigurationSize = 0;
        Transfer->CompletionCallback = Bcm27PwmapSystemDmaCompletion;
        Transfer->Direction = DmaTransferToDevice;
        Transfer->Width = 32;
        Transfer->Flags = DMA_TRANSFER_CONTINUOUS;
        Transfer->UserContext = &(Controller->Device);
        Transfer->Device.Address = Controller->ControllerBasePhysicalAddress +
                                   Bcm2709PwmFifo;

        Transfer->Device.Address &= ~BCM27_PWMA_DEVICE_ADDRESS_MASK;
        Transfer->Device.Address |= BCM27_PWMA_DEVICE_ADDRESS_VALUE;
    }

InitializeDmaEnd:
    if (!KSUCCESS(Status)) {
        if (Controller->DmaTransfer != NULL) {
            Controller->Dma->FreeTransfer(Controller->Dma,
                                          Controller->DmaTransfer);

            Controller->DmaTransfer = NULL;
        }

        IoUnregisterForInterfaceNotifications(&Bcm27PwmaDmaUuid,
                                              Bcm27PwmapDmaInterfaceCallback,
                                              Resource->Provider,
                                              Controller);
    }

    return Status;
}

VOID
Bcm27PwmapDmaInterfaceCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    )

/*++

Routine Description:

    This routine is called to notify listeners that an interface has arrived
    or departed.

Arguments:

    Context - Supplies the caller's context pointer, supplied when the caller
        requested interface notifications.

    Device - Supplies a pointer to the device exposing or deleting the
        interface.

    InterfaceBuffer - Supplies a pointer to the interface buffer of the
        interface.

    InterfaceBufferSize - Supplies the buffer size.

    Arrival - Supplies TRUE if a new interface is arriving, or FALSE if an
        interface is departing.

Return Value:

    None.

--*/

{

    PBCM27_PWMA_CONTROLLER Controller;

    Controller = Context;

    ASSERT(InterfaceBufferSize >= sizeof(DMA_INTERFACE));
    ASSERT((Controller->Dma == NULL) || (Controller->Dma == InterfaceBuffer));

    if (Arrival != FALSE) {
        Controller->Dma = InterfaceBuffer;

    } else {
        Controller->Dma = NULL;
    }

    return;
}

VOID
Bcm27PwmapSystemDmaCompletion (
    PDMA_TRANSFER Transfer
    )

/*++

Routine Description:

    This routine is called when a transfer set has completed or errored out.

Arguments:

    Transfer - Supplies a pointer to the transfer that completed.

Return Value:

    None.

--*/

{

    PBCM27_PWMA_DEVICE_INTERNAL Device;

    //
    // Another fragment completed. Update the internal count and then update
    // sound core with the offset.
    //

    Device = Transfer->UserContext;
    Device->BufferPosition += Device->Buffer->FragmentSize;

    //
    // The buffer size should be a power of 2, so just mask off the size.
    //

    ASSERT(POWER_OF_2(Device->Buffer->Size) != FALSE);

    Device->BufferPosition = REMAINDER(Device->BufferPosition,
                                       Device->Buffer->Size);

    SoundUpdateBufferState(Device->Buffer,
                           SoundDeviceOutput,
                           Device->BufferPosition);

    return;
}

KSTATUS
Bcm27PwmapSetDeviceState (
    PBCM27_PWMA_CONTROLLER Controller,
    PBCM27_PWMA_DEVICE_INTERNAL Device,
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

    ULONG ChannelControl;
    ULONG Control;
    ULONG DmaConfig;
    PDMA_TRANSFER DmaTransfer;
    ULONG Index;
    ULONG Range;
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
    // Setting the device to the uninitialized state resets all resources and
    // stops the DMA.
    //

    case SoundDeviceStateUninitialized:
        Status = Controller->Dma->Cancel(Controller->Dma,
                                         Controller->DmaTransfer);

        if (!KSUCCESS(Status)) {
            break;
        }

        BCM27_WRITE_PWMA(Controller, Bcm2709PwmControl, 0);
        BCM27_WRITE_PWMA(Controller, Bcm2709PwmDmaConfig, 0);
        Device->BufferPosition = 0;
        Device->Buffer = NULL;
        break;

    //
    // Initializing the device prepares the device for DMA.
    //

    case SoundDeviceStateInitialized:

        //
        // This controller only supports output with signed 16-bit data.
        //

        ASSERT(Device->Public.SoundDevice.Type == SoundDeviceOutput);
        ASSERT(State->U.Initialize.Format ==
               SOUND_FORMAT_16_BIT_SIGNED_LITTLE_ENDIAN);

        ASSERT(State->U.Initialize.RouteContext == NULL);
        ASSERT(State->U.Initialize.ChannelCount <=
               Device->Public.SoundDevice.MaxChannelCount);

        for (Index = 0;
             Index < Device->Public.SoundDevice.RateCount;
             Index += 1) {

            if (State->U.Initialize.SampleRate ==
                Device->Public.SampleRates[Index]) {

                break;
            }
        }

        if (Index == Device->Public.SoundDevice.RateCount) {
            Status = STATUS_INVALID_PARAMETER;
            break;
        }

        Device->ChannelCount = State->U.Initialize.ChannelCount;

        //
        // The provided volume is expressed as a fraction of 100. As PWM audio
        // needs to handle volume in software, convert this to a fraction of
        // 128 to save a divide for each sample.
        //

        Bcm27PwmapSetVolume(Device, State->U.Initialize.Volume);

        //
        // Initialize the DMA transfer. Some of the transfer data is constant
        // and set during initialization.
        //

        Device->Buffer = State->U.Initialize.Buffer;
        DmaTransfer = Controller->DmaTransfer;
        DmaTransfer->Memory = Device->Buffer->IoBuffer;
        DmaTransfer->Size = Device->Buffer->Size *
                            BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES;

        DmaTransfer->Completed = 0;
        DmaTransfer->InterruptPeriod = Device->Buffer->FragmentSize *
                                       BCM27_PWMA_PWM_BYTES_PER_PCM_BYTES;

        //
        // Determine the PWM range value. It's units are clock ticks per audio
        // sample and it is used to set the sample rate. When both channels are
        // enabled, the PWM DMA transfers the second channel without waiting
        // for the first to complete. As a result, the number of channels does
        // not need to be factored into the range calculation. It is as simple
        // as dividing the clock rate (ticks/sec) by the sample rate
        // (samples/sec) in order to arrive at the range (ticks/sample).
        //

        Range = Controller->PwmClockFrequency / State->U.Initialize.SampleRate;
        Device->Range = Range;
        if (POWER_OF_2(Range) != FALSE) {
            Device->RangeShift = RtlCountTrailingZeros32(Range);
        }

        //
        // Program the PWM registers to prepare for the DMA transfer.
        //

        BCM27_WRITE_PWMA(Controller, Bcm2709PwmChannel1Range, Range);
        if (Device->ChannelCount == 2) {
            BCM27_WRITE_PWMA(Controller, Bcm2709PwmChannel2Range, Range);
        }

        DmaConfig = (BCM27_PWMA_PANIC_DEFAULT <<
                     BCM2709_PWM_DMA_CONFIG_PANIC_SHIFT) &
                    BCM2709_PWM_DMA_CONFIG_PANIC_MASK;

        DmaConfig |= (BCM27_PWMA_DATA_REQUEST_DEFAULT <<
                      BCM2709_PWM_DMA_CONFIG_DATA_REQUEST_SHIFT) &
                     BCM2709_PWM_DMA_CONFIG_DATA_REQUEST_MASK;

        DmaConfig |= BCM2709_PWM_DMA_CONFIG_ENABLE;
        BCM27_WRITE_PWMA(Controller, Bcm2709PwmDmaConfig, DmaConfig);
        break;

    //
    // The running state sets the DMA transfers in motion.
    //

    case SoundDeviceStateRunning:
        ChannelControl = BCM2709_PWM_CONTROL_CHANNEL_ENABLE |
                         BCM2709_PWM_CONTROL_CHANNEL_USE_FIFO;

        Control = BCM2709_PWM_CONTROL_CLEAR_FIFO;
        Control |= (ChannelControl << BCM2709_PWM_CONTROL_CHANNEL_1_SHIFT) &
                   BCM2709_PWM_CONTROL_CHANNEL_1_MASK;

        if (Device->ChannelCount == 2) {
            Control |= (ChannelControl << BCM2709_PWM_CONTROL_CHANNEL_2_SHIFT) &
                       BCM2709_PWM_CONTROL_CHANNEL_2_MASK;
        }

        BCM27_WRITE_PWMA(Controller, Bcm2709PwmControl, Control);
        Status = Controller->Dma->Submit(Controller->Dma,
                                         Controller->DmaTransfer);

        if (!KSUCCESS(Status)) {
            break;
        }

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
Bcm27PwmapSetVolume (
    PBCM27_PWMA_DEVICE_INTERNAL Device,
    ULONG Volume
    )

/*++

Routine Description:

    This routine sets the given PWMA device's volume. It takes a sound core
    encoded volume - left and right channel values between 0 and 100) and
    converts them to left and right channel values between 0 and 128. Having
    a power of 2 maximum saves a divide when applying the volume in realtime
    to each of the samples.

Arguments:

    Device - Supplies a pointer to the PWMA device for which to set the volume.

    Volume - Supplies the sound core encoded volume. See SOUND_VOLUME_* for
        definitions.

Return Value:

    None.

--*/

{

    ULONG LeftVolume;
    ULONG RightVolume;

    LeftVolume = (Volume & SOUND_VOLUME_LEFT_CHANNEL_MASK) >>
                 SOUND_VOLUME_LEFT_CHANNEL_SHIFT;

    LeftVolume <<= BCM27_PWMA_MAX_VOLUME_SHIFT;
    LeftVolume /= SOUND_VOLUME_MAXIMUM;
    Device->Volume[0] = (UCHAR)LeftVolume;
    RightVolume = (Volume & SOUND_VOLUME_RIGHT_CHANNEL_MASK) >>
                  SOUND_VOLUME_RIGHT_CHANNEL_SHIFT;

    RightVolume <<= BCM27_PWMA_MAX_VOLUME_SHIFT;
    RightVolume /= SOUND_VOLUME_MAXIMUM;
    Device->Volume[1] = (UCHAR)RightVolume;
    return;
}

