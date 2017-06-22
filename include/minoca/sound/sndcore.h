/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sndcore.h

Abstract:

    This header contains definitions for creating and managing new sound
    controllers via the sound core library.

Author:

    Chris Stevens 18-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/sound/sound.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifndef SOUND_API

#define SOUND_API __DLLIMPORT

#endif

#define SOUND_CONTROLLER_INFORMATION_VERSION 0x00000001
#define SOUND_DEVICE_VERSION 0x00000001
#define SOUND_DEVICE_STATE_INFORMATION_VERSION 0x00000001

//
// Define the set of publicly accessible sound device flags.
//

#define SOUND_DEVICE_FLAG_PRIMARY 0x00000001

//
// Define the mask of publicly accessible sound device flags.
//

#define SOUND_DEVICE_FLAG_PUBLIC_MASK SOUND_DEVICE_FLAG_PRIMARY

//
// Define the set of controller wide flags.
//

#define SOUND_CONTROLLER_FLAG_NON_CACHED_DMA_BUFFER  0x00000001
#define SOUND_CONTROLLER_FLAG_NON_PAGED_SOUND_BUFFER 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _SOUND_CONTROLLER SOUND_CONTROLLER, *PSOUND_CONTROLLER;
typedef struct _SOUND_DEVICE_HANDLE SOUND_DEVICE_HANDLE, *PSOUND_DEVICE_HANDLE;

typedef enum _SOUND_DEVICE_INFORMATION_TYPE {
    SoundDeviceInformationState,
    SoundDeviceInformationVolume,
} SOUND_DEVICE_INFORMATION_TYPE, *PSOUND_DEVICE_INFORMATION_TYPE;

typedef enum _SOUND_DEVICE_STATE {
    SoundDeviceStateUninitialized,
    SoundDeviceStateInitialized,
    SoundDeviceStateRunning,
} SOUND_DEVICE_STATE, *PSOUND_DEVICE_STATE;

/*++

Structure Description:

    This structure defines a sound I/O buffer with state shared between the
    sound core library and the sound controller.

Members:

    IoBuffer - Stores a pointer to the raw I/O buffer that stores the data.

    Size - Stores the total size of the buffer, in bytes. This must be a power
        of 2.

    FragmentSize - Stores the size of each sound fragment. This is not to be
        confused with an I/O buffer fragment size. This must be a power of 2.

    FragmentShift - Stores the number of bits to shift to convert from bytes to
        fragments. This means that the fragment size must be a power of 2.

    FragmentCount - Stores the number of fragments in the sound buffer. This is
        not to be confused with the I/O buffer fragment count. This must be a
        power of 2.

    LowThreshold - Stores the low water mark bytes threshold. The buffer's I/O
        state will only be signaled once this many bytes are available to read
        or are free to write into.

    IoState - Stores a pointer to the I/O state to signal when data is
        available to consume.

    CoreOffset - Stores the offset of the next byte to be "consumed" by the
        sound core library. On input, sound core consumes bytes written by the
        controller. On output, sound core consumes empty bytes that have
        been read by the controller. If this offset equals the controller
        offset, then the buffer is empty. Otherwise there are bytes to consume.

    ControllerOffset - Stores the offset of the next byte to be "produced" by
        the sound controller. On input, the sound controller produces bytes by
        wiriting into the buffer. On output, the sound controller produces
        empty bytes by reading from the buffer. The sound controller should not
        worry about the buffer being full, it should just keep reading/writing.
        It's up the sound core to keep up.

    BytesAvailable - Stores the number of bytes available in the buffer to
        read from or write into. Even as the controller offset continues to
        move, this should never be more than the buffer size.

    BytesCompleted - Stores the total number of bytes that have been processed
        by the device since its last reset.

    FragmentsCompleted - Stores the total fragments that had been process by
        the device by the last time the buffer position information was queried.

--*/

typedef struct _SOUND_IO_BUFFER {
    PIO_BUFFER IoBuffer;
    UINTN Size;
    UINTN FragmentSize;
    UINTN FragmentShift;
    UINTN FragmentCount;
    UINTN LowThreshold;
    PIO_OBJECT_STATE IoState;
    volatile UINTN CoreOffset;
    volatile UINTN ControllerOffset;
    volatile UINTN BytesAvailable;
    volatile UINTN BytesCompleted;
    volatile UINTN FragmentsCompleted;
} SOUND_IO_BUFFER, *PSOUND_IO_BUFFER;

/*++

Structure Description:

    This structure defines the state needed to initialize a device in
    preparation for I/O.

Members:

    Buffer - Supplies a pointer to a sound buffer to be shared between the
        sound core library and the sound controller driver.

    Format - Stores the desired stream format. See SOUND_FORMAT_* for
        definitions.

    ChannelCount - Stores the number of channels in the stream.

    SampleRate - Stores the rate of the data samples, in Hz.

    Volume - Stores the device volume. This stores both the left and right
        channel volume. If the device does not support separate channel volume
        control, it should use the left channel volume. See SOUND_VOLUME_* for
        mask definitions.

    RouteContext - Stores an opaque pointer to the sound controller's context
        for the chosen route.

--*/

typedef struct _SOUND_DEVICE_STATE_INITIALIZE {
    PSOUND_IO_BUFFER Buffer;
    ULONG Format;
    ULONG ChannelCount;
    ULONG SampleRate;
    ULONG Volume;
    PVOID RouteContext;
} SOUND_DEVICE_STATE_INITIALIZE, *PSOUND_DEVICE_STATE_INITIALIZE;

/*++

Structure Description:

    This structure defines the information supplied when setting a sound device
    state. The sound core has a lock that serializes state transitions, so the
    sound controller drivers do not need to worry about multiple state changes
    requests racing.

Members:

    Version - Stores the version of the device state information structure. Set
        to SOUND_DEVICE_STATE_INFORMATION_VERSION.

    State - Stores the device state to set.

    Initialize - Stores the information necessary to initialize a device.

--*/

typedef struct _SOUND_DEVICE_STATE_INFORMATION {
    ULONG Version;
    SOUND_DEVICE_STATE State;
    struct {
        SOUND_DEVICE_STATE_INITIALIZE Initialize;
    } U;

} SOUND_DEVICE_STATE_INFORMATION, *PSOUND_DEVICE_STATE_INFORMATION;

typedef enum _SOUND_DEVICE_TYPE {
    SoundDeviceInput,
    SoundDeviceOutput,
    SoundDeviceTypeCount
} SOUND_DEVICE_TYPE, *PSOUND_DEVICE_TYPE;

typedef enum _SOUND_DEVICE_ROUTE_TYPE {
    SoundDeviceRouteUnknown,
    SoundDeviceRouteLineOut,
    SoundDeviceRouteSpeaker,
    SoundDeviceRouteHeadphone,
    SoundDeviceRouteCd,
    SoundDeviceRouteSpdifOut,
    SoundDeviceRouteDigitalOut,
    SoundDeviceRouteModemLineSide,
    SoundDeviceRouteModemHandsetSide,
    SoundDeviceRouteLineIn,
    SoundDeviceRouteAux,
    SoundDeviceRouteMicrophone,
    SoundDeviceRouteTelephony,
    SoundDeviceRouteSpdifIn,
    SoundDeviceRouteDigitalIn,
    SoundDeviceRouteTypeCount,
} SOUND_DEVICE_ROUTE_TYPE, *PSOUND_DEVICE_ROUTE_TYPE;

/*++

Structure Description:

    This structures defines a sound device route. A route represents a path
    the sound core device can take to reach one external audio devices attached
    to the sound controller. For instance, if there may be multiple microphones
    attached to a controller, there should be a route for each.

Members:

    Type - Stores the type of route, dictated by the audio device at the
        external end of the route.

    Context - Stores an opaque pointer to the sound controller's context for
        this route.

--*/

typedef struct _SOUND_DEVICE_ROUTE {
    SOUND_DEVICE_ROUTE_TYPE Type;
    PVOID Context;
} SOUND_DEVICE_ROUTE, *PSOUND_DEVICE_ROUTE;

/*++

Structure Description:

    This structure defines a sound device. A device represents a unique
    interface on the sound controller. For example, a digital audio converter
    (DAC) is an output device. A device may not have a fixed route through the
    controller (e.g. the DAC may be routed to the headphones or a speaker).

Members:

    Version - Stores the value SOUND_DEVICE_VERSION, used to enable future
        expansion of this structure.

    StructureSize - Stores the size of this structure, in bytes. This includes
        any data appended to the end, like the array of rates.

    Context - Stores an opaque pointer to the sound controller's context for
        this device.

    Type - Stores the sound device type.

    Flags - Stores a bitmask of flags. See SOUND_DEVICE_FLAG_* for definitions.

    Capabilities - Stores a bitmask of device capabilities. See
        SOUND_CAPABILITY_* for definitions.

    Formats - Stores a bitmask of supported formats. See SOUND_FORMAT_* for
        definitions.

    MinChannelCount - Stores the minimum number of channels the device supports.

    MaxChannelCount - Stores the maximum number of channels the device supports.

    RateCount - Stores the number of supported rates.

    RatesOffset - Stores an offset from the beginning of this structure to the
        start of the sorted array of supported rates. The rates are stored in
        Hz.

    RouteCount - Stores the number of available routes.

    RoutesOffset - Stores an offset from the beginning of this structure to the
        start of the array of SOUND_DEVICE_ROUTE structures.

--*/

typedef struct _SOUND_DEVICE {
    ULONG Version;
    UINTN StructureSize;
    PVOID Context;
    SOUND_DEVICE_TYPE Type;
    volatile ULONG Flags;
    ULONG Capabilities;
    ULONG Formats;
    ULONG MinChannelCount;
    ULONG MaxChannelCount;
    ULONG RateCount;
    UINTN RatesOffset;
    ULONG RouteCount;
    UINTN RoutesOffset;
} SOUND_DEVICE, *PSOUND_DEVICE;

typedef
KSTATUS
(*PSOUND_ALLOCATE_DMA_BUFFER) (
    PVOID ControllerContext,
    PVOID DeviceContext,
    UINTN FragmentSize,
    UINTN FragmentCount,
    PIO_BUFFER *NewIoBuffer
    );

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

typedef
VOID
(*PSOUND_FREE_DMA_BUFFER) (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER IoBuffer
    );

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

typedef
KSTATUS
(*PSOUND_GET_SET_INFORMATION) (
    PVOID ControllerContext,
    PVOID DeviceContext,
    SOUND_DEVICE_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

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

typedef
KSTATUS
(*PSOUND_COPY_BUFFER_DATA) (
    PVOID ControllerContext,
    PVOID DeviceContext,
    PIO_BUFFER Destination,
    UINTN DestinationOffset,
    PIO_BUFFER Source,
    UINTN SourceOffset,
    UINTN Size
    );

/*++

Routine Description:

    This routine copies sound data from one I/O buffer to another. This gives
    the sound controller an opportunity to do any conversions if its audio
    format does not conform to one of sound core's formats. One of the two
    buffers will be the buffer supplied to the sound controller when the
    device was put in the initialized state. Which one it is depends on the
    direction of the sound.

Arguments:

    ControllerContext - Supplies a pointer to the sound controller's context.

    DeviceContext - Supplies a pointer to the sound controller's device context.

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

/*++

Structure Description:

    This structure stores the set of sound controller functions called by the
    sound library.

Members:

    AllocateDmaBuffer - Stores a pointer to a function that allocates a buffer
        to use for DMA transfers. If not supplied, then the sound core will
        handle buffer allocation and assume DMA is not possible.

    FreeDmaBuffer - Stores a pointer to a function that destroys a DMA buffer.

    GetSetInformation - Stores a pointer to a function that gets and sets
        sound device state.

--*/

typedef struct _SOUND_FUNCTION_TABLE {
    PSOUND_ALLOCATE_DMA_BUFFER AllocateDmaBuffer;
    PSOUND_FREE_DMA_BUFFER FreeDmaBuffer;
    PSOUND_GET_SET_INFORMATION GetSetInformation;
    PSOUND_COPY_BUFFER_DATA CopyBufferData;
} SOUND_FUNCTION_TABLE, *PSOUND_FUNCTION_TABLE;

/*++

Structure Description:

    This structure defines the information provided to the sound library by a
    sound controller.

Members:

    Version - Stores the value SOUND_CONTROLLER_INFORMATION_VERSION, used to
        enable future expansion of this structure.

    Context - Stores an opaque context pointer that is passed to the sound
        controller functions.

    OsDevice - Stores a pointer to the OS device associated with this
        controller.

    Flags - Stores a bitmask of controller wide flags. See
        SOUND_CONTROLLER_FLAG_* for definitions.

    FunctionTable - Stores a pointer to the function table the library uses to
        call back into the controller.

    MinFragmentCount - Stores the minimum number of allowed DMA buffer
        fragments. This must be a power of 2.

    MaxFragmentCount - Stores the maximum number of allowed DMA buffer
        fragments. This must be a power of 2.

    MinFragmentSize - Stores the minimum allowed size of a DMA buffer fragment.
        This must be a power of 2.

    MaxFragmentSize - Stores the maximum allowed size of a DMA buffer fragment.
        This must be a power of 2.

    MaxBufferSize - Stores the maximum allowed DMA buffer size, in bytes. This
        must be a power of 2.

    DeviceCount - Stores the number of sound devices advertised by the
        controller.

    Devices - Stores an array of pointers to sound devices attached to the
        controller.

--*/

typedef struct _SOUND_CONTROLLER_INFORMATION {
    ULONG Version;
    PVOID Context;
    PDEVICE OsDevice;
    ULONG Flags;
    PSOUND_FUNCTION_TABLE FunctionTable;
    UINTN MinFragmentCount;
    UINTN MaxFragmentCount;
    UINTN MinFragmentSize;
    UINTN MaxFragmentSize;
    UINTN MaxBufferSize;
    ULONG DeviceCount;
    PSOUND_DEVICE *Devices;
} SOUND_CONTROLLER_INFORMATION, *PSOUND_CONTROLLER_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

SOUND_API
KSTATUS
SoundCreateController (
    PSOUND_CONTROLLER_INFORMATION Registration,
    PSOUND_CONTROLLER *Controller
    );

/*++

Routine Description:

    This routine creates a sound core controller object.

Arguments:

    Registration - Supplies a pointer to the host registration information.
        This information will be copied, allowing it to be stack allocated by
        the caller.

    Controller - Supplies a pointer where a pointer to the new controller will
        be returned on success.

Return Value:

    Status code.

--*/

SOUND_API
VOID
SoundDestroyController (
    PSOUND_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys a sound controller.

Arguments:

    Controller - Supplies a pointer to the controller to tear down.

Return Value:

    None.

--*/

SOUND_API
KSTATUS
SoundLookupDevice (
    PSOUND_CONTROLLER Controller,
    PSYSTEM_CONTROL_LOOKUP Lookup
    );

/*++

Routine Description:

    This routine looks for a sound device underneath the given controller.

Arguments:

    Controller - Supplies a pointer to the sound core library's controller.

    Lookup - Supplies a pointer to the lookup information.

Return Value:

    Status code.

--*/

SOUND_API
KSTATUS
SoundOpenDevice (
    PSOUND_CONTROLLER Controller,
    PFILE_PROPERTIES FileProperties,
    ULONG AccessFlags,
    ULONG OpenFlags,
    PIO_OBJECT_STATE IoState,
    PSOUND_DEVICE_HANDLE *Handle
    );

/*++

Routine Description:

    This routine opens a sound device. This helps a sound driver coordinate the
    sharing of its resources and may even select which physical device to open.

Arguments:

    Controller - Supplies a pointer to the sound core library's controller.

    FileProperties - Supplies a pointer to the file properties that indicate
        which device is being opened.

    AccessFlags - Supplies a bitmask of access flags. See IO_ACCESS_* for
        definitions.

    OpenFlags - Supplies a bitmask of open flags. See OPEN_FLAG_* for
        definitions.

    IoState - Supplies a pointer I/O state to signal for this device.

    Handle - Supplies a pointer that receives an opaque handle to the opened
        sound device.

Return Value:

    Status code.

--*/

SOUND_API
VOID
SoundCloseDevice (
    PSOUND_DEVICE_HANDLE Handle
    );

/*++

Routine Description:

    This routine closes a sound device, releasing any resources allocated for
    the device.

Arguments:

    Handle - Supplies a pointer to the sound device handle to close.

Return Value:

    None.

--*/

SOUND_API
KSTATUS
SoundPerformIo (
    PSOUND_DEVICE_HANDLE Handle,
    PIO_BUFFER IoBuffer,
    PIO_OFFSET IoOffset,
    UINTN SizeInBytes,
    ULONG IoFlags,
    ULONG TimeoutInMilliseconds,
    BOOL Write,
    PUINTN BytesCompleted
    );

/*++

Routine Description:

    This routine will play or record sound on the given device.

Arguments:

    Handle - Supplies a pointer to the sound device handle to use for I/O.

    IoBuffer - Supplies a pointer to I/O buffer with the data to play or the
        where the recorded data should end up.

    IoOffset - Supplies a pointer to the offset where the I/O should start on
        input. On output, it stores the I/O offset at the end of the I/O. This
        is only relevant to the controller "directory".

    SizeInBytes - Supplies the amount of data to play or record.

    IoFlags - Supplies a bitmask of I/O flags. See IO_FLAG_* for definitions.

    TimeoutInMilliseconds - Supplies the number of milliseconds that the I/O
        operation should be waited on before timing out. Use
        WAIT_TIME_INDEFINITE to wait forever on the I/O.

    Write - Supplies a boolean indicating whether this is a write (TRUE) or
        read (FALSE) request.

    BytesCompleted - Supplies a pointer that receives the number of bytes
        played or recorded.

Return Value:

    Status code.

--*/

SOUND_API
KSTATUS
SoundUserControl (
    PSOUND_DEVICE_HANDLE Handle,
    BOOL FromKernelMode,
    ULONG RequestCode,
    PVOID RequestBuffer,
    UINTN RequestBufferSize
    );

/*++

Routine Description:

    This routine handles user control requests that get or set the state of the
    given sound device.

Arguments:

    Handle - Supplies a pointer to the sound device handle for the request.

    FromKernelMode - Supplies a boolean indicating whether or not the request
        came from kernel mode (TRUE) or user mode (FALSE). If it came from user
        mode, then special MM routines must be used when accessing the request
        buffer.

    RequestCode - Supplies the request code for the user control.

    RequestBuffer - Supplies a pointer to the buffer containing context for the
        user control request. Treat this with suspicion if the request came
        from user mode.

    RequestBufferSize - Supplies the size of the request buffer. If the request
        is from user mode, this must be treated with suspicion.

Return Value:

    Status code.

--*/

SOUND_API
KSTATUS
SoundGetSetDeviceInformation (
    PSOUND_CONTROLLER Controller,
    PUUID Uuid,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    );

/*++

Routine Description:

    This routine gets or sets device information for a sound controller.

Arguments:

    Controller - Supplies a pointer to the sound core library's controller.

    Uuid - Supplies a pointer to the information identifier.

    Data - Supplies a pointer to the data buffer.

    DataSize - Supplies a pointer that on input contains the size of the data
        buffer in bytes. On output, returns the needed size of the data buffer,
        even if the supplied buffer was nonexistant or too small.

    Set - Supplies a boolean indicating whether to get the information (FALSE)
        or set the information (TRUE).

Return Value:

    Status code.

--*/

SOUND_API
VOID
SoundUpdateBufferState (
    PSOUND_IO_BUFFER Buffer,
    SOUND_DEVICE_TYPE Type,
    UINTN Offset
    );

/*++

Routine Description:

    This routine updates the given buffer's state in a lock-less way. It will
    increment the total bytes processes and signal the I/O state if necessary.
    It assumes, however, that the sound controller has some sort of
    synchronization to prevent this routine from being called simultaneously
    for the same buffer.

Arguments:

    Buffer - Supplies a pointer to the sound I/O buffer whose state needs to be
        updated.

    Type - Supplies the type of sound device to which the buffer belongs.

    Offset - Supplies the hardware's updated offset within the I/O buffer.

Return Value:

    None.

--*/

