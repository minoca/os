/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hda.h

Abstract:

    This header contains definitions for Intel High Definition Audio
    controllers.

Author:

    Chris Stevens 3-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/sound/sndcore.h>
#include "hdahw.h"

//
// --------------------------------------------------------------------- Macros
//

//
// This macro returns a widget's type.
//

#define HDA_GET_WIDGET_TYPE(_Widget)                                 \
    (((_Widget)->WidgetCapabilities & HDA_AUDIO_WIDGET_TYPE_MASK) >> \
     HDA_AUDIO_WIDGET_TYPE_SHIFT)

//
// This macro returns the widget's maximum channel count. Combine the extension
// value with the least significant bit and add one.
//

#define HDA_GET_WIDGET_CHANNEL_COUNT(_Widget)            \
    (((((_Widget)->WidgetCapabilities &                  \
        HDA_AUDIO_WIDGET_CHANNEL_COUNT_EXT_MASK) >>      \
       (HDA_AUDIO_WIDGET_CHANNEL_COUNT_EXT_SHIFT - 1)) | \
     ((_Widget)->WidgetCapabilities &                    \
      HDA_AUDIO_WIDGET_CHANNEL_COUNT_LSB)) + 1)

//
// This macro returns a widget's index in the function group's array of widgets.
//

#define HDA_GET_WIDGET_GROUP_INDEX(_Group, _Widget) \
    ((_Widget)->NodeId - (_Group)->WidgetNodeStart)

//
// This macro returns a pointer to the widget, given a node ID.
//

#define HDA_GET_WIDGET_FROM_ID(_Group, _NodeId) \
    &((_Group)->Widgets[(_NodeId) - (_Group)->WidgetNodeStart])

//
// This macro determines whether or not a pin widget has a physical connection.
//

#define HDA_IS_PIN_WIDGET_CONNECTED(_Widget)               \
    (((_Widget)->PinConfiguration &                        \
      HDA_CONFIGURATION_DEFAULT_PORT_CONNECTIVITY_MASK) != \
     (HDA_PORT_CONNECTIVITY_NONE <<                        \
      HDA_CONFIGURATION_DEFAULT_PORT_CONNECTIVITY_SHIFT))

//
// This macro returns the first buffer descriptor list entry for a given stream.
//

#define HDA_GET_STREAM_BDL(_Controller, _StreamIndex)                   \
    ((_Controller)->BufferDescriptorLists +                             \
     ((_StreamIndex) * HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT))

//
// ---------------------------------------------------------------- Definitions
//

#define HDA_ALLOCATION_TAG 0x61644849 // 'adHI'

//
// Define how long to wait for the device to perform an initialization
// operation before timing out, in seconds.
//

#define HDA_DEVICE_TIMEOUT 1

//
// Define how long the stream should wait for resets and disables before timing
// out, in milliseconds.
//

#define HDA_STREAM_TIMEOUT 20

//
// Define how long to wait for a solicited response, in seconds.
//

#define HDA_RESPONSE_TIMEOUT 5

//
// Define the controller flags bits.
//

#define HDA_CONTROLLER_FLAG_64_BIT_ADDRESSES 0x00000001

//
// Define the set of debug flags.
//

#define HDA_DEBUG_FLAG_CODEC_ENUMERATION 0x00000001

//
// Define a set of flags used to determine if MSI/MSI-X interrupt should be
// used.
//

#define HDA_PCI_MSI_FLAG_INTERFACE_REGISTERED 0x00000001
#define HDA_PCI_MSI_FLAG_INTERFACE_AVAILABLE  0x00000002
#define HDA_PCI_MSI_FLAG_RESOURCES_REQUESTED  0x00000004
#define HDA_PCI_MSI_FLAG_RESOURCES_ALLOCATED  0x00000008

//
// Define the minimum number of allowed buffer descriptor list entries.
//

#define HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_MINIMUM 2

//
// Define the default number of entries in a buffer descriptor list. The buffer
// descriptor lists can be between 2 and 256 entries in length, but need to be
// aligned to 128 bytes. As 8 entries can fit in 128 bytes, set that as the
// default.
//

#define HDA_BUFFER_DESCRIPTOR_LIST_ENTRY_COUNT_DEFAULT 8

//
// Define maximum allowed sound buffer fragment size. This must be a power of 2.
//

#define HDA_SOUND_BUFFER_MAX_FRAGMENT_SIZE 0x40000000

//
// Define maximum allowed sound buffer size. This must be a power of 2.
//

#define HDA_SOUND_BUFFER_MAX_SIZE 0x80000000

//
// Define the set of function group flags.
//

#define HDA_FUNCTION_GROUP_FLAG_EXTENDED_POWER_STATES 0x00000001

//
// Define the maximum allowed path length.
//

#define HDA_MAX_PATH_LENGTH 10

//
// Define the widget flags.
//

#define HDA_WIDGET_FLAG_ACCESSIBLE 0x00000001

//
// Define the reserved stream number 0 bitmask.
//

#define HDA_STREAM_NUMBER_0 0x0001

//
// Define the total number of streams, including stream 0.
//

#define HDA_STREAM_NUMBER_COUNT 16

//
// Define the invalid stream value.
//

#define HDA_INVALID_STREAM_NUMBER MAX_UCHAR

//
// Define the invalid descriptor value.
//

#define HDA_INVALID_STREAM MAX_UCHAR

//
// Define the default number of response required to generate an interrupt.
//

#define HDA_RESPONSE_INTERRUPT_COUNT_DEFAULT 1

//
// Define the software interrupt bits for the HD controller.
//

#define HDA_SOFTWARE_INTERRUPT_RESPONSE_BUFFER 0x00000001
#define HDA_SOFTWARE_INTERRUPT_STREAM          0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _HDA_CONTROLLER HDA_CONTROLLER, *PHDA_CONTROLLER;

typedef enum _HDA_PATH_TYPE {
    HdaPathAdcFromInput,
    HdaPathDacToOutput,
    HdaPathInputToOutput,
    HdaPathTypeCount
} HDA_PATH_TYPE, *PHDA_PATH_TYPE;

/*++

Structure Description:

    This structure defines a widget path through a function group.

Members:

    ListEntry - Stores pointers to the next and previous entries in the
        function group's list of paths.

    Type - Stores the path type.

    RouteType - Stores the sound device route type of the path. This really
        describes the connection of the paths last pin widget.

    Length - Stores the length of the widget path.

    Widgets - Stores an array of widget indices. These are indices into the
        function group's array of the widgets; not the raw node IDs of each
        widget.

--*/

typedef struct _HDA_PATH {
    LIST_ENTRY ListEntry;
    HDA_PATH_TYPE Type;
    SOUND_DEVICE_ROUTE_TYPE RouteType;
    ULONG Length;
    ULONG Widgets[ANYSIZE_ARRAY];
} HDA_PATH, *PHDA_PATH;

/*++

Structure Description:

    This structure defines a generic widget.

Members:

    NodeId - Stores the ID of this widget's node.

    Flags - Stores a bitmask of widget flags. See HDA_WIDGET_FLAG_* for
        definitions.

    WidgetCapabilities - Stores the generic audio widget capabilities that
        are supported by all widgets.

    TypeCapabilities - Stores the type-specific capabilities. For example, the
        pin capabilities for pin widgets and the volume knob capabilities for
        volume knobs.

    PinConfiguration - Stores the default configuration for pin widgets.

    InputAmplifier - Stores the capabilities for the input amplifier.

    OutputAmplifier - Stores the capabilities for the output amplifier.

    SupportedRates - Stores the supported data rates for input and output
        audio converter widgets.

    SupportedPcmSizes - Stores the supported PCM sizes for input and output
        audio converter widgets.

    SupportedSreamFormats - Stores the supported stream formats for input and
        output audio converter widgets.

--*/

typedef struct _HDA_WIDGET {
    USHORT NodeId;
    USHORT Flags;
    ULONG WidgetCapabilities;
    ULONG TypeCapabilities;
    ULONG PinConfiguration;
    ULONG InputAmplifier;
    ULONG OutputAmplifier;
    USHORT SupportedRates;
    USHORT SupportedPcmSizes;
    ULONG SupportedStreamFormats;
} HDA_WIDGET, *PHDA_WIDGET;

/*++

Structure Description:

    This structure defines a generic function group.

Members:

    Type - Stores the function group type. See HDA_FUNCTION_GROUP_TYPE_*.

    NodeId - Stores the ID of the function group's node.

    WidgetNodeStart - Stores the ID of the first widget node.

    WidgetCount - Stores the number of widgets attached to the group.

    Flags - Stores a bitmap of codec flags. See HDA_FUNCTION_GROUP_FLAG_* for
        definitions.

    SupportedRates - Stores the supported data rates for an audio function
        group.

    SupportedPcmSizes - Stores the supported PCM sizes for an audio function
        group.

    SupportedSreamFormats - Stores the supported stream formats for an audio
        function group.

    PathList - Stores the list of viable input and output paths for the device.

    Widgets - Stores an array of widgets.

--*/

typedef struct _HDA_FUNCTION_GROUP {
    USHORT NodeId;
    UCHAR Type;
    UCHAR WidgetNodeStart;
    UCHAR WidgetCount;
    ULONG Flags;
    USHORT SupportedRates;
    USHORT SupportedPcmSizes;
    ULONG SupportedStreamFormats;
    LIST_ENTRY PathList[HdaPathTypeCount];
    HDA_WIDGET Widgets[ANYSIZE_ARRAY];
} HDA_FUNCTION_GROUP, *PHDA_FUNCTION_GROUP;

/*++

Structure Description:

    This structure defines an HD Audio codec.

Members:

    Controller - Stores a pointer to the HD Audio controller to which the
        codec is attached.

    VendorId - Stores the vendor ID of the codec manufacturer.

    DeviceId - Stores the device ID of the codec.

    Revision - Stores the codec's revision number.

    PendingResponseCount - Stores the number of pending responses on this
        codec.

    Address - Stores the address of the codec.

    FunctionGroupNodeStart - Stores the ID of the first function group node.

    FunctionGroupCount - Stores the number of function groups.

    FunctionGroups - Stores an array of pointers to function groups.

--*/

typedef struct _HDA_CODEC {
    PHDA_CONTROLLER Controller;
    USHORT VendorId;
    USHORT DeviceId;
    ULONG Revision;
    UCHAR Address;
    UCHAR FunctionGroupNodeStart;
    UCHAR FunctionGroupCount;
    PHDA_FUNCTION_GROUP FunctionGroups[ANYSIZE_ARRAY];
} HDA_CODEC, *PHDA_CODEC;

/*++

Structure Description:

Members:

    Codec - Stores a pointer to the codec to which the device is attached.

    Group - Stores a pointer to the function group to which the device is
        attached.

    Widget - Stores a pointer to the based widget for the device. All paths
        will start at this widget.

    Path - Stores a pointer to the current path for the device.

    Buffer - Stores the sound I/O buffer in use by this device.

    PendingStatus - Stores the pending stream status bits recorded by the ISR.

    State - Stores the current state of the device.

    StreamIndex - Stores the stream index in use by the device.

    StreamNumber - Stores the stream allocation by the device.

    StreamFifoSize - Stores the size of the stream's FIFO, in bytes.

    SoundDevice - Stores the sound core library device information. This must
        be the last element as the sound device structure is variable in size,
        as it stores an array of supported rates at the end.

--*/

typedef struct _HDA_DEVICE {
    PHDA_CODEC Codec;
    PHDA_FUNCTION_GROUP Group;
    PHDA_WIDGET Widget;
    PHDA_PATH Path;
    PSOUND_IO_BUFFER Buffer;
    volatile ULONG PendingStatus;
    SOUND_DEVICE_STATE State;
    UCHAR StreamIndex;
    UCHAR StreamNumber;
    USHORT StreamFifoSize;
    SOUND_DEVICE SoundDevice;
} HDA_DEVICE, *PHDA_DEVICE;

/*++

Structure Description:

    This structure defines the context for a HD Audio device.

Members:

    OsDevice - Stores a pointer to the OS device object.

    SoundController - Stores a pointer to the sound core library's controller.

    InterruptLine - Stores the interrupt line that this controller's interrupt
        comes in on.

    InterruptVector - Stores the interrupt vector that this controller's
        interrupt comes in on.

    InterruptHandle - Stores a pointer to the handle received when the
        interrupt was connected.

    PendingSoftwareInterrupts - Stores a bitmask of bending software
        interrupts. See HDA_SOFTWARE_INTERRUPT_* for definitions.

    ControllerBase - Stores the virtual address of the memory mapping to the
        registers.

    ControllerLock - Stores a pointer to a lock that synchronizes codec
        enumeration, stream allocation, and register access.

    CommandLock - Stores a pointer to a queued lock that protects access to the
        command and response ring buffers.

    Flags - Stores a bitmask of flags describing controller properties. See
        HDA_CONTROLLER_FLAG_* for definitions.

    IoBuffer - Stores a pointer to an I/O buffer that stores the controller's
        ring buffers and buffer descriptor lists.

    CommandBuffer - Stores a pointer to the base virtual address of the command
        output ring buffer (CORB).

    ResponseBuffer - Stores a pointer to the base virtual address of the
        response input ring buffer (RIRB).

    BufferDescriptorLists - Stores a pointer to the base virtual address of the
        buffer descriptor lists. There is one per stream.

    CommandBufferPhysical - Stores the base physical address of the command
        output ring buffer (CORB).

    ResponseBufferPhysical - Stores the base physical address of the response
        input ring buffer (RIRB).

    BufferDescriptorListsPhysical - Stores the base physical address of the
        buffer descriptor lists.

    CommandBufferEntryCount - Stores the number of entries in the command
        output ring buffer. The maximum is 256 entries.

    ResponseBufferEntryCount - Stores the number of entries in the response
        input ring buffer. The maximum is 256 entries.

    CommandNextWritePointer - Stores the software controlled index into the
        command output ring buffer indicating which entry to write to next. The
        hardware stores a write pointer to the last command written.

    ResponseReadPointer - Stores the software controlled index into the
        response ring buffer indicating which response was last read. The
        hardware stores a pointer to the last written response.

    StreamDevices - Stores an array of pointers to HDA devices that are
        assigned to streams. The length is the sum of all input, output, and
        bidirectional stream counts. This buffer serves a means to allocate
        free streams.

    StreamCount - Stores the total number of streams.

    OutputStreamCount - Stores the number of available output streams.

    InputStreamCount - Stores the number of available input streams.

    BidirectionalStreamCount - Stores the number of available bidirectional
        streams.

    StreamNumbers - Stores a bitmask that indicates which of the 16 stream
        numbers are in use. Stream number 0 is reserved.

    StreamSynchronizationRegister - Stores the stream synchronization register
        offset. This depends on the controller type.

    PciMsiFlags - Stores a bitmask of flags indicating whether or not MSI/MSI-X
        interrupts should be used. See HDA_PCI_MSI_FLAG_* for definitions.

    PciMsiInterface - Stores the interface to enable PCI message signaled
        interrupts.

    Codec - Stores an array of pointers to codec structures. If the pointer is
        not NULL, then a codec is present in that address slot.

    CodecLastResponse - Stores an array of the last response that arrived from
        each codec.

    CodecPendingResponseCount - Stores an array that holds the number of
        pending responses outstanding for each codec.

    DeviceCount - Stores the number of sound devices present in the array.

    Devices - Stores an array of pointers to sound devices.

--*/

struct _HDA_CONTROLLER {
    PDEVICE OsDevice;
    PSOUND_CONTROLLER SoundController;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
    volatile ULONG PendingSoftwareInterrupts;
    PVOID ControllerBase;
    PQUEUED_LOCK ControllerLock;
    PQUEUED_LOCK CommandLock;
    ULONG Flags;
    PIO_BUFFER IoBuffer;
    PHDA_COMMAND_ENTRY CommandBuffer;
    PHDA_RESPONSE_ENTRY ResponseBuffer;
    PHDA_BUFFER_DESCRIPTOR_LIST_ENTRY BufferDescriptorLists;
    PHYSICAL_ADDRESS CommandBufferPhysical;
    PHYSICAL_ADDRESS ResponseBufferPhysical;
    PHYSICAL_ADDRESS BufferDescriptorListsPhysical;
    USHORT CommandBufferEntryCount;
    USHORT ResponseBufferEntryCount;
    USHORT CommandNextWritePointer;
    USHORT ResponseReadPointer;
    HDA_DEVICE * volatile *StreamDevices;
    USHORT StreamCount;
    UCHAR OutputStreamCount;
    UCHAR InputStreamCount;
    UCHAR BidirectionalStreamCount;
    USHORT StreamNumbers;
    HDA_REGISTER StreamSynchronizationRegister;
    ULONG PciMsiFlags;
    INTERFACE_PCI_MSI PciMsiInterface;
    PHDA_CODEC Codec[HDA_MAX_CODEC_COUNT];
    ULONG CodecLastResponse[HDA_MAX_CODEC_COUNT];
    volatile ULONG CodecPendingResponseCount[HDA_MAX_CODEC_COUNT];
    ULONG DeviceCount;
    PSOUND_DEVICE *Devices;
};

/*++

Structure Description:

    This structure defines a supported sample rate, linking the value in Hz to
    the value to be programmed into the various format registers.

Members:

    Rate - Stores the sample rate, in Hz.

    Format - Stores the value to set in the format registers.

--*/

typedef struct _HDA_RATE {
    ULONG Rate;
    ULONG Format;
} HDA_RATE, *PHDA_RATE;

//
// -------------------------------------------------------------------- Globals
//

//
// Define the set of enable HD Audio debug flags.
//

extern ULONG HdaDebugFlags;

//
// Define the supported sample rates and PCM bit depths.
//

extern HDA_RATE HdaSampleRates[];
extern ULONG HdaPcmSizeFormats[];

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
HdaSoundAllocateDmaBuffer (
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

VOID
HdaSoundFreeDmaBuffer (
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

KSTATUS
HdaSoundGetSetInformation (
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

INTERRUPT_STATUS
HdaInterruptService (
    PVOID Context
    );

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

INTERRUPT_STATUS
HdaInterruptServiceDpc (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine implements the HDA dispatch level interrupt service.

Arguments:

    Parameter - Supplies the context, in this case the controller structure.

Return Value:

    None.

--*/

INTERRUPT_STATUS
HdaInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine processes interrupts for the HDA controller at low level.

Arguments:

    Parameter - Supplies an optional parameter passed in by the creator of the
        work item.

Return Value:

    Interrupt Status.

--*/

KSTATUS
HdapInitializeDeviceStructures (
    PHDA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine allocates any resources needed to communicate with the HDA
    controller.

Arguments:

    Device - Supplies a pointer to the controller context.

Return Value:

    Status code.

--*/

VOID
HdapDestroyDeviceStructures (
    PHDA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine destroys all of the internal allocations made when
    initializing the controller structure.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

KSTATUS
HdapInitializeController (
    PHDA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine initializes an Intel HD Audio controller from the reset state.

Arguments:

    Device - Supplies a pointer to the controller context.

Return Value:

    Status code.

--*/

KSTATUS
HdapGetParameter (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    USHORT NodeId,
    HDA_PARAMETER ParameterId,
    PULONG Parameter
    );

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

KSTATUS
HdapGetSetVerb (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress,
    USHORT NodeId,
    USHORT Verb,
    USHORT Payload,
    PULONG Response
    );

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

KSTATUS
HdapCommandBarrier (
    PHDA_CONTROLLER Controller,
    UCHAR CodecAddress
    );

/*++

Routine Description:

    This routine synchronizes a batch of commands to make sure they have all
    completed before the driver continues operation on a codec.

Arguments:

    Codec - Supplies a pointer to a codec.

Return Value:

    Status code.

--*/

KSTATUS
HdapEnumerateCodecs (
    PHDA_CONTROLLER Controller,
    USHORT StateChange
    );

/*++

Routine Description:

    This routine enumerates the codecs attached to the given HD Audio
    controller's link.

Arguments:

    Controller - Supplies a pointer to the HD Audio controller.

    StateChange - Supplies the saved state change status register value that
        indicates which codecs needs to be enumerated.

Return Value:

    Status code.

--*/

VOID
HdapDestroyCodecs (
    PHDA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine cleans up all of the resources created during codec
    enumeration.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

KSTATUS
HdapEnableDevice (
    PHDA_DEVICE Device,
    PHDA_PATH Path,
    USHORT Format
    );

/*++

Routine Description:

    This routine enables an HDA device in preparation for it to start playing
    or recording audio.

Arguments:

    Device - Supplies a pointer to the device to enable.

    Path - Supplies a pointer to the HDA path to enable for the device.

    Format - Supplies the HDA stream format to be programmed in the device.

Return Value:

    Status code.

--*/

KSTATUS
HdapSetDeviceVolume (
    PHDA_DEVICE Device,
    ULONG Volume
    );

/*++

Routine Description:

    This routine sets the HDA device's volume by modifying the gain levels for
    each amplifier in the path.

Arguments:

    Device - Supplies a pointer to the device whose volume is to be set.

    Volume - Supplies a bitmask of channel volumes. See SOUND_VOLUME_* for
        definitions.

Return Value:

    Status code.

--*/

