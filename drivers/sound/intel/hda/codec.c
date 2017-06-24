/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    codec.c

Abstract:

    This module implements codec parsing support for the Intel HD Audio driver.

Author:

    Chris Stevens 14-Apr-2017

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
HdapCreateSoundDevices (
    PHDA_CONTROLLER Controller
    );

VOID
HdapDestroySoundDevices (
    PHDA_CONTROLLER Controller
    );

PSOUND_DEVICE
HdapCreateSoundDevice (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    );

VOID
HdapDestroySoundDevice (
    PSOUND_DEVICE Device
    );

KSTATUS
HdapValidateCodec (
    PHDA_CODEC Codec,
    PBOOL Valid
    );

KSTATUS
HdapCreateAndEnumerateCodec (
    PHDA_CONTROLLER Controller,
    UCHAR Address,
    PHDA_CODEC *Codec
    );

VOID
HdapDestroyCodec (
    PHDA_CODEC Codec
    );

KSTATUS
HdapCreateAndEnumerateFunctionGroup (
    PHDA_CODEC Codec,
    USHORT NodeId,
    PHDA_FUNCTION_GROUP *Group
    );

VOID
HdapDestroyFunctionGroup (
    PHDA_FUNCTION_GROUP Group
    );

KSTATUS
HdapEnumerateWidget (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    );

KSTATUS
HdapEnableWidgets (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    );

KSTATUS
HdapEnumeratePaths (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    );

KSTATUS
HdapFindPaths (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget,
    HDA_PATH_TYPE PathType,
    PULONG Path,
    ULONG PathLength
    );

ULONG
HdapGetPathCount (
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    );

PHDA_PATH
HdapGetPrimaryPath (
    PHDA_DEVICE Device
    );

KSTATUS
HdapCreatePath (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    HDA_PATH_TYPE PathType,
    PULONG PathWidgets,
    ULONG PathLength
    );

VOID
HdapDestroyPath (
    PHDA_PATH Path
    );

KSTATUS
HdapResetFunctionGroup (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    );

BOOL
HdapIsOutputDevice (
    PHDA_WIDGET Widget
    );

BOOL
HdapIsInputDevice (
    PHDA_WIDGET Widget
    );

KSTATUS
HdapCodecGetParameter (
    PHDA_CODEC Codec,
    USHORT NodeId,
    HDA_PARAMETER ParameterId,
    PULONG Parameter
    );

KSTATUS
HdapCodecGetSetVerb (
    PHDA_CODEC Codec,
    USHORT NodeId,
    USHORT Verb,
    USHORT Payload,
    PULONG Response
    );

KSTATUS
HdapCodecCommandBarrier (
    PHDA_CODEC Codec
    );

USHORT
HdapComputeGainMute (
    ULONG AmpCapabilities,
    ULONG Volume
    );

KSTATUS
HdapGetConnectionListIndex (
    PHDA_CODEC Codec,
    PHDA_WIDGET ListWidget,
    PHDA_WIDGET ConnectedWidget,
    PULONG ListIndex
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store human readable names for the paths. These are used for debug output.
//

PCSTR HdaPathTypeNames[HdaPathTypeCount] = {
    "ADC from Input",
    "DAC to Output",
    "Input to Output"
};

SOUND_DEVICE_ROUTE_TYPE HdaDeviceTypeToRouteType[] = {
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
    SoundDeviceRouteUnknown
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
HdapEnumerateCodecs (
    PHDA_CONTROLLER Controller,
    USHORT StateChange
    )

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

{

    UCHAR Address;
    PHDA_CODEC Codec;
    KSTATUS Status;
    BOOL Valid;

    //
    // Get the state change status to see if any new codecs arrived or any
    // existing codecs disappeared.
    //

    Codec = NULL;
    KeAcquireQueuedLock(Controller->ControllerLock);
    for (Address = 0; Address < HDA_MAX_CODEC_COUNT; Address += 1) {

        //
        // If no codec is present, destory any previously allocated codec.
        //

        if ((StateChange & (1 << Address)) == 0) {
            if (Controller->Codec[Address] != NULL) {
                HdapDestroyCodec(Controller->Codec[Address]);
                Controller->Codec[Address] = NULL;
            }

            continue;
        }

        //
        // If a codec is already allocated at this address, make sure it is the
        // same codec as before.
        //

        if (Controller->Codec[Address] != NULL) {
            Status = HdapValidateCodec(Controller->Codec[Address], &Valid);
            if (!KSUCCESS(Status)) {
                goto EnumerateCodecsEnd;
            }

            if (Valid != FALSE) {
                continue;
            }

            HdapDestroyCodec(Controller->Codec[Address]);
            Controller->Codec[Address] = NULL;
        }

        //
        // Allocate and enumerate a new codec at this address.
        //

        Status = HdapCreateAndEnumerateCodec(Controller, Address, &Codec);
        if (!KSUCCESS(Status)) {
            if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
                RtlDebugPrint("HDA: Failed to create codec for controller "
                              "0x%08x at address 0x%02x: %d\n",
                              Controller,
                              Address,
                              Status);
            }

            goto EnumerateCodecsEnd;
        }

        Controller->Codec[Address] = Codec;
        if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
            RtlDebugPrint("HDA: Codec at Address 0x%02x:\n"
                          "\tVendorId: 0x%04x\n"
                          "\tDeviceId: 0x%04x\n"
                          "\tRevision: 0x%08x\n",
                          Codec->Address,
                          Codec->VendorId,
                          Codec->DeviceId,
                          Codec->Revision);
        }
    }

    //
    // Create sound library devices based on the enumerated widgets.
    //

    Status = HdapCreateSoundDevices(Controller);
    if (!KSUCCESS(Status)) {
        goto EnumerateCodecsEnd;
    }

EnumerateCodecsEnd:
    KeReleaseQueuedLock(Controller->ControllerLock);
    return Status;
}

VOID
HdapDestroyCodecs (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine cleans up all of the resources created during codec
    enumeration.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

{

    UCHAR Address;

    KeAcquireQueuedLock(Controller->ControllerLock);
    for (Address = 0; Address < HDA_MAX_CODEC_COUNT; Address += 1) {
        if (Controller->Codec[Address] != NULL) {
            HdapDestroyCodec(Controller->Codec[Address]);
        }
    }

    HdapDestroySoundDevices(Controller);
    KeReleaseQueuedLock(Controller->ControllerLock);
    return;
}

KSTATUS
HdapEnableDevice (
    PHDA_DEVICE Device,
    PHDA_PATH Path,
    USHORT Format
    )

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

{

    ULONG ConnectedIndex;
    PHDA_WIDGET ConnectedWidget;
    ULONG DeviceType;
    PHDA_FUNCTION_GROUP Group;
    ULONG Index;
    ULONG ListLength;
    PHDA_PATH OldPath;
    ULONG SelectorIndex;
    KSTATUS Status;
    ULONG Value;
    PHDA_WIDGET Widget;
    ULONG WidgetType;

    //
    // Set the provided path for future use, but mute and disable the old path
    // first.
    //

    if (Path != Device->Path) {
        Status = HdapSetDeviceVolume(Device, 0);
        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }

        OldPath = Device->Path;
        Status = STATUS_SUCCESS;
        for (Index = 0; Index < OldPath->Length; Index += 1) {
            Widget = &(Device->Group->Widgets[OldPath->Widgets[Index]]);
            if (HDA_GET_WIDGET_TYPE(Widget) != HDA_AUDIO_WIDGET_TYPE_PIN) {
                continue;
            }

            Status = HdapCodecGetSetVerb(Device->Codec,
                                         Widget->NodeId,
                                         HdaVerbSetPinWidgetControl,
                                         0,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }
        }

        Device->Path = Path;
    }

    Path = Device->Path;

    ASSERT(Path != NULL);

    //
    // Enable the input and output pin's appropriately for each widget in the
    // path.
    //

    Group = Device->Group;
    Status = STATUS_SUCCESS;
    for (Index = 0; Index < Path->Length; Index += 1) {
        Widget = &(Group->Widgets[Path->Widgets[Index]]);
        WidgetType = HDA_GET_WIDGET_TYPE(Widget);
        switch (WidgetType) {
        case HDA_AUDIO_WIDGET_TYPE_PIN:
            if (Device->SoundDevice.Type == SoundDeviceInput) {
                Value = HDA_PIN_WIDGET_CONTROL_IN_ENABLE;

            } else {

                ASSERT(Device->SoundDevice.Type == SoundDeviceOutput);

                DeviceType = (Widget->PinConfiguration &
                              HDA_CONFIGURATION_DEFAULT_DEVICE_MASK) >>
                             HDA_CONFIGURATION_DEFAULT_DEVICE_SHIFT;

                Value = HDA_PIN_WIDGET_CONTROL_OUT_ENABLE;
                if (DeviceType == HDA_DEVICE_HP_OUT) {
                    Value |= HDA_PIN_WIDGET_CONTROL_HEAD_PHONE_ENABLE;
                }
            }

            Status = HdapCodecGetSetVerb(Device->Codec,
                                         Widget->NodeId,
                                         HdaVerbSetPinWidgetControl,
                                         Value,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }

            //
            // Pins fall through as they also need to select the correct input.
            //

        case HDA_AUDIO_WIDGET_TYPE_INPUT:
        case HDA_AUDIO_WIDGET_TYPE_SELECTOR:

            //
            // Input and Selector widgets search the next widget in the path
            // for an index.
            //

            if ((WidgetType == HDA_AUDIO_WIDGET_TYPE_INPUT) ||
                (WidgetType == HDA_AUDIO_WIDGET_TYPE_SELECTOR)) {

                ConnectedIndex = Index + 1;

            //
            // If it's a Pin at the end of an output path, search the previous
            // widget for an index. If it's a Pin at the end of an input path
            // or the start of a input/output path, ignore it.
            //

            } else if ((Index != 0) &&
                       ((Path->Type == HdaPathDacToOutput) ||
                        (Path->Type == HdaPathInputToOutput))) {

                ConnectedIndex = Index - 1;

            } else {
                break;
            }

            //
            // If the connect list is of length one, then there is no
            // Connection Select control.
            //

            Status = HdapCodecGetParameter(Device->Codec,
                                           Widget->NodeId,
                                           HdaParameterConnectionListLength,
                                           &ListLength);

            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }

            if (ListLength <= 1) {
                break;
            }

            ASSERT(ConnectedIndex < Path->Length);

            ConnectedWidget = &(Group->Widgets[Path->Widgets[ConnectedIndex]]);
            Status = HdapGetConnectionListIndex(Device->Codec,
                                                Widget,
                                                ConnectedWidget,
                                                &SelectorIndex);

            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }

            Status = HdapCodecGetSetVerb(Device->Codec,
                                         Widget->NodeId,
                                         HdaVerbSetConnectionSelectControl,
                                         SelectorIndex,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto EnableDeviceEnd;
            }

            break;

        default:
            break;
        }
    }

    //
    // Initialize the device's main widget.
    //

    Status = HdapCodecGetSetVerb(Device->Codec,
                                 Device->Widget->NodeId,
                                 HdaVerbSetConverterFormat,
                                 Format,
                                 NULL);

    if (!KSUCCESS(Status)) {
        goto EnableDeviceEnd;
    }

    Value = (Device->StreamNumber << HDA_CONVERTER_CONTROL_STREAM_SHIFT) &
            HDA_CONVERTER_CONTROL_STREAM_MASK;

    Status = HdapCodecGetSetVerb(Device->Codec,
                                 Device->Widget->NodeId,
                                 HdaVerbSetConverterStreamChannel,
                                 Value,
                                 NULL);

    if (!KSUCCESS(Status)) {
        goto EnableDeviceEnd;
    }

    WidgetType = HDA_GET_WIDGET_TYPE(Device->Widget);
    if ((WidgetType == HDA_AUDIO_WIDGET_TYPE_OUTPUT) &&
        (Device->SoundDevice.MaxChannelCount > 2)) {

        Value = (Format & HDA_FORMAT_NUMBER_OF_CHANNELS_MASK) >>
                HDA_FORMAT_NUMBER_OF_CHANNELS_SHIFT;

        Value -= 1;
        Status = HdapCodecGetSetVerb(Device->Codec,
                                     Device->Widget->NodeId,
                                     HdaVerbSetConverterChannelCount,
                                     Value,
                                     NULL);

        if (!KSUCCESS(Status)) {
            goto EnableDeviceEnd;
        }
    }

    //
    // Make sure all of the above command complete before returning.
    //

    Status = HdapCodecCommandBarrier(Device->Codec);
    if (!KSUCCESS(Status)) {
        goto EnableDeviceEnd;
    }

EnableDeviceEnd:
    return Status;
}

KSTATUS
HdapSetDeviceVolume (
    PHDA_DEVICE Device,
    ULONG Volume
    )

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

{

    ULONG AmpIndex;
    ULONG ConnectedIndex;
    PHDA_WIDGET ConnectedWidget;
    PHDA_FUNCTION_GROUP Group;
    ULONG Index;
    USHORT InputAmp;
    USHORT InputLeftGainMute;
    USHORT InputRightGainMute;
    USHORT LeftAmp;
    ULONG LeftVolume;
    USHORT OutputAmp;
    USHORT OutputLeftGainMute;
    USHORT OutputRightGainMute;
    PHDA_PATH Path;
    USHORT RightAmp;
    ULONG RightVolume;
    KSTATUS Status;
    ULONG Value;
    PHDA_WIDGET Widget;
    ULONG WidgetCapabilities;
    ULONG WidgetType;

    Group = Device->Group;

    //
    // The device should have a path.
    //

    Path = Device->Path;

    ASSERT(Path != NULL);

    //
    // The volume encodes multiple channels. Decode them here.
    //

    LeftVolume = (Volume & SOUND_VOLUME_LEFT_CHANNEL_MASK) >>
                 SOUND_VOLUME_LEFT_CHANNEL_SHIFT;

    RightVolume = (Volume & SOUND_VOLUME_RIGHT_CHANNEL_MASK) >>
                  SOUND_VOLUME_RIGHT_CHANNEL_SHIFT;

    RightAmp = HDA_SET_AMPLIFIER_GAIN_PAYLOAD_RIGHT;
    LeftAmp = HDA_SET_AMPLIFIER_GAIN_PAYLOAD_LEFT;
    if (LeftVolume == RightVolume) {
        LeftAmp |= HDA_SET_AMPLIFIER_GAIN_PAYLOAD_RIGHT;
    }

    InputRightGainMute = 0;
    OutputRightGainMute = 0;

    //
    // Set the amplifier gain/mute register for each widget in the path.
    //

    Status = STATUS_SUCCESS;
    for (Index = 0; Index < Path->Length; Index += 1) {
        Widget = &(Group->Widgets[Path->Widgets[Index]]);
        WidgetType = HDA_GET_WIDGET_TYPE(Widget);
        WidgetCapabilities = Widget->WidgetCapabilities;
        OutputAmp = 0;
        if ((WidgetCapabilities & HDA_AUDIO_WIDGET_OUT_AMP_PRESENT) != 0) {
            OutputAmp = HDA_SET_AMPLIFIER_GAIN_PAYLOAD_OUTPUT;
            OutputLeftGainMute = HdapComputeGainMute(Widget->OutputAmplifier,
                                                     LeftVolume);

            if (LeftVolume != RightVolume) {
                OutputRightGainMute = HdapComputeGainMute(
                                                       Widget->OutputAmplifier,
                                                       RightVolume);
            }
        }

        InputAmp = 0;
        if ((WidgetCapabilities & HDA_AUDIO_WIDGET_IN_AMP_PRESENT) != 0) {
            InputAmp = HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INPUT;
            InputLeftGainMute = HdapComputeGainMute(Widget->InputAmplifier,
                                                    LeftVolume);

            if (LeftVolume != RightVolume) {
                InputRightGainMute = HdapComputeGainMute(
                                                        Widget->InputAmplifier,
                                                        RightVolume);
            }
        }

        AmpIndex = 0;
        switch (WidgetType) {

        //
        // An input converter shouldn't have an output amp; make sure it is not
        // programmed.
        //

        case HDA_AUDIO_WIDGET_TYPE_INPUT:
            OutputAmp = 0;
            break;

        //
        // An output converter shouldn't have an input AMP; make sure it is not
        // programmed.
        //

        case HDA_AUDIO_WIDGET_TYPE_OUTPUT:
            InputAmp = 0;
            break;

        //
        // Mixers and selectors may have both input and output amplifiers.
        // Enable both if they are present. For input amplifiers, pick the
        // correct index for this path.
        //

        case HDA_AUDIO_WIDGET_TYPE_SELECTOR:
        case HDA_AUDIO_WIDGET_TYPE_MIXER:
            if (InputAmp == 0) {
                break;
            }

            //
            // If this is an output path, the index is based on the previous
            // widget's offset in the mixer's connection list. If this is an
            // input path, then the index is based on the next widget.
            //

            if (Path->Type == HdaPathAdcFromInput) {
                ConnectedIndex = Index + 1;

            } else {
                ConnectedIndex = Index - 1;
            }

            ASSERT(ConnectedIndex < Path->Length);

            ConnectedWidget = &(Group->Widgets[Path->Widgets[ConnectedIndex]]);
            Status = HdapGetConnectionListIndex(Device->Codec,
                                                Widget,
                                                ConnectedWidget,
                                                &AmpIndex);

            if (!KSUCCESS(Status)) {
                goto SetDeviceVolumeEnd;
            }

            AmpIndex <<= HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INDEX_SHIFT;
            AmpIndex &= HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INDEX_MASK;
            break;

        //
        // On an input path, the last node should be a pin and its input amp
        // should be enabled. On an output path, the last node should be a pin
        // and its output amp should be enabled. On an input/output path, the
        // first pin should also be a pin and its input amp should be enabled.
        //

        case HDA_AUDIO_WIDGET_TYPE_PIN:
            switch (Path->Type) {
            case HdaPathAdcFromInput:

                ASSERT(Index == (Path->Length - 1));

                OutputAmp = 0;
                break;

            case HdaPathDacToOutput:

                ASSERT(Index == (Path->Length - 1));

                InputAmp = 0;
                break;

            case HdaPathInputToOutput:
                if (Index == (Path->Length - 1)) {
                    InputAmp = 0;

                } else {

                    ASSERT(Index == 0);

                    OutputAmp = 0;
                }

                break;

            default:
                break;
            }

            break;

        default:
            break;
        }

        //
        // Now that all of that business is sorted, get on to actually
        // programming the amplifiers.
        //

        if (InputAmp != 0) {
            Value = InputAmp | LeftAmp | AmpIndex | InputLeftGainMute;
            Status = HdapCodecGetSetVerb(Device->Codec,
                                         Widget->NodeId,
                                         HdaVerbSetAmplifierGain,
                                         Value,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto SetDeviceVolumeEnd;
            }

            if ((LeftAmp & HDA_SET_AMPLIFIER_GAIN_PAYLOAD_RIGHT) == 0) {
                Value = InputAmp | RightAmp | AmpIndex | InputRightGainMute;
                Status = HdapCodecGetSetVerb(Device->Codec,
                                             Widget->NodeId,
                                             HdaVerbSetAmplifierGain,
                                             Value,
                                             NULL);

                if (!KSUCCESS(Status)) {
                    goto SetDeviceVolumeEnd;
                }
            }
        }

        if (OutputAmp != 0) {
            Value = OutputAmp | LeftAmp | OutputLeftGainMute;
            Status = HdapCodecGetSetVerb(Device->Codec,
                                         Widget->NodeId,
                                         HdaVerbSetAmplifierGain,
                                         Value,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto SetDeviceVolumeEnd;
            }

            if ((LeftAmp & HDA_SET_AMPLIFIER_GAIN_PAYLOAD_RIGHT) == 0) {
                Value = OutputAmp | RightAmp | OutputRightGainMute;
                Status = HdapCodecGetSetVerb(Device->Codec,
                                             Widget->NodeId,
                                             HdaVerbSetAmplifierGain,
                                             Value,
                                             NULL);

                if (!KSUCCESS(Status)) {
                    goto SetDeviceVolumeEnd;
                }
            }
        }
    }

    //
    // Make sure all of the above command complete before returning.
    //

    Status = HdapCodecCommandBarrier(Device->Codec);
    if (!KSUCCESS(Status)) {
        goto SetDeviceVolumeEnd;
    }

SetDeviceVolumeEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HdapCreateSoundDevices (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine creates the array of sound devices based on the information
    gathered from the codecs.

Arguments:

    Controller - Supplies a pointer to the sound controller information.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PHDA_CODEC Codec;
    ULONG CodecIndex;
    PSOUND_DEVICE Device;
    ULONG DeviceCount;
    ULONG DeviceIndex;
    PSOUND_DEVICE *Devices;
    PHDA_FUNCTION_GROUP Group;
    ULONG GroupIndex;
    PHDA_DEVICE HdaDevice;
    PHDA_PATH Path;
    ULONG PathIndex;
    PSOUND_DEVICE PrimaryInput;
    ULONG PrimaryInputPriority;
    PSOUND_DEVICE PrimaryOutput;
    ULONG PrimaryOutputPriority;
    ULONG Priority;
    ULONG PriorityMask;
    KSTATUS Status;
    PHDA_WIDGET Widget;
    ULONG WidgetIndex;

    //
    // Count the number of devices needed. A device is created for each DAC and
    // ADC widget that is accessible via a path. They are already marked.
    //

    DeviceCount = 0;
    for (CodecIndex = 0; CodecIndex < HDA_MAX_CODEC_COUNT; CodecIndex += 1) {
        Codec = Controller->Codec[CodecIndex];
        if (Codec == NULL) {
            continue;
        }

        for (GroupIndex = 0;
             GroupIndex < Codec->FunctionGroupCount;
             GroupIndex += 1) {

            Group = Codec->FunctionGroups[GroupIndex];
            for (WidgetIndex = 0;
                 WidgetIndex < Group->WidgetCount;
                 WidgetIndex += 1) {

                Widget = &(Group->Widgets[WidgetIndex]);
                if ((Widget->Flags & HDA_WIDGET_FLAG_ACCESSIBLE) != 0) {
                    DeviceCount += 1;
                }
            }
        }
    }

    if (DeviceCount == 0) {
        Status = STATUS_SUCCESS;
        goto CreateDevicesEnd;
    }

    //
    // Allocate the array of devices.
    //

    AllocationSize = DeviceCount * sizeof(PSOUND_DEVICE);
    Devices = MmAllocatePagedPool(AllocationSize, HDA_ALLOCATION_TAG);
    if (Devices == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateDevicesEnd;
    }

    RtlZeroMemory(Devices, AllocationSize);

    //
    // Iterate over the codecs again, creating a sound library device and an
    // HDA device for each DAC and ADC.
    //

    DeviceIndex = 0;
    PriorityMask = HDA_CONFIGURATION_DEFAULT_ASSOCIATION_MASK |
                   HDA_CONFIGURATION_DEFAULT_SEQUENCE_MASK;

    for (CodecIndex = 0; CodecIndex < HDA_MAX_CODEC_COUNT; CodecIndex += 1) {
        Codec = Controller->Codec[CodecIndex];
        if (Codec == NULL) {
            continue;
        }

        for (GroupIndex = 0;
             GroupIndex < Codec->FunctionGroupCount;
             GroupIndex += 1) {

            PrimaryInputPriority = MAX_ULONG;
            PrimaryOutputPriority = MAX_ULONG;
            PrimaryInput = NULL;
            PrimaryOutput = NULL;
            Group = Codec->FunctionGroups[GroupIndex];
            for (WidgetIndex = 0;
                 WidgetIndex < Group->WidgetCount;
                 WidgetIndex += 1) {

                Widget = &(Group->Widgets[WidgetIndex]);
                if ((Widget->Flags & HDA_WIDGET_FLAG_ACCESSIBLE) == 0) {
                    continue;
                }

                Device = HdapCreateSoundDevice(Codec, Group, Widget);
                if (Device == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto CreateDevicesEnd;
                }

                ASSERT(DeviceIndex < DeviceCount);

                Devices[DeviceIndex] = Device;
                DeviceIndex += 1;

                //
                // Determine if this is the primary device for this function
                // group.
                //

                HdaDevice = Device->Context;
                Path = HdaDevice->Path;
                PathIndex = Path->Widgets[Path->Length - 1];
                Widget = &(Group->Widgets[PathIndex]);
                Priority = Widget->PinConfiguration & PriorityMask;
                if (Device->Type == SoundDeviceInput) {
                    if (Priority < PrimaryInputPriority) {
                        PrimaryInputPriority = Priority;
                        PrimaryInput = Device;
                    }

                } else if (Device->Type == SoundDeviceOutput) {
                    if (Priority < PrimaryOutputPriority) {
                        PrimaryOutputPriority = Priority;
                        PrimaryOutput = Device;
                    }
                }
            }

            if (PrimaryInput != NULL) {
                PrimaryInput->Flags |= SOUND_DEVICE_FLAG_PRIMARY;
            }

            if (PrimaryOutput != NULL) {
                PrimaryOutput->Flags |= SOUND_DEVICE_FLAG_PRIMARY;
            }
        }
    }

    ASSERT(Controller->Devices == NULL);

    Controller->Devices = Devices;
    Controller->DeviceCount = DeviceCount;
    Status = STATUS_SUCCESS;

CreateDevicesEnd:
    if (!KSUCCESS(Status)) {
        if (Devices != NULL) {
            for (DeviceIndex = 0; DeviceIndex < DeviceCount; DeviceIndex += 1) {
                if (Devices[DeviceIndex] != NULL) {
                    HdapDestroySoundDevice(Devices[DeviceIndex]);
                }
            }

            MmFreePagedPool(Devices);
        }
    }

    return Status;
}

VOID
HdapDestroySoundDevices (
    PHDA_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys the array of sound devices for the controller.

Arguments:

    Controller - Supplies a pointer to an HDA controller.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < Controller->DeviceCount; Index += 1) {
        HdapDestroySoundDevice(Controller->Devices[Index]);
    }

    MmFreePagedPool(Controller->Devices);
    Controller->DeviceCount = 0;
    Controller->Devices = NULL;
    return;
}

PSOUND_DEVICE
HdapCreateSoundDevice (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    )

/*++

Routine Description:

    This routine creates a sound device to pass to the sound core library. It
    is based on the supplied codec, group, and widget tuple.

Arguments:

    Codec - Supplies a pointer to the codec to which the device is attached.

    Group - Supplies a pointer to the function group to which the device is
        attached.

    Widget - Supplies a pointer to the widget on which the sound device will be
        based.

Return Value:

    Returns a pointer to the new sound device on success or NULL on failure.

--*/

{

    UINTN AllocationSize;
    ULONG Capabilities;
    PLIST_ENTRY CurrentEntry;
    PHDA_PATH CurrentPath;
    ULONG Formats;
    PHDA_DEVICE HdaDevice;
    ULONG MaxChannelCount;
    ULONG MinChannelCount;
    INT RateCount;
    ULONG RateIndex;
    PULONG Rates;
    UINTN RatesSize;
    ULONG RouteCount;
    ULONG RouteIndex;
    PSOUND_DEVICE_ROUTE Routes;
    UINTN RoutesSize;
    PSOUND_DEVICE SoundDevice;
    KSTATUS Status;
    USHORT SupportedIndex;
    USHORT SupportedRates;
    USHORT SupportedSizes;
    ULONG WidgetType;

    SupportedRates = Widget->SupportedRates;
    RateCount = RtlCountSetBits32(SupportedRates);
    RatesSize = RateCount * sizeof(ULONG);
    RouteCount = HdapGetPathCount(Group, Widget);
    RoutesSize = RouteCount * sizeof(SOUND_DEVICE_ROUTE);
    AllocationSize = sizeof(HDA_DEVICE) + RatesSize + RoutesSize;
    HdaDevice = MmAllocateNonPagedPool(AllocationSize, HDA_ALLOCATION_TAG);
    if (HdaDevice == NULL) {
        return NULL;
    }

    //
    // The internal HDA device is actually the start of the allocation. This
    // makes the sound device easy to find when given a pointer to the HDA
    // device.
    //

    RtlZeroMemory(HdaDevice, AllocationSize);
    SoundDevice = &(HdaDevice->SoundDevice);
    HdaDevice->Codec = Codec;
    HdaDevice->Group = Group;
    HdaDevice->Widget = Widget;
    HdaDevice->StreamNumber = HDA_INVALID_STREAM_NUMBER;
    HdaDevice->StreamIndex = HDA_INVALID_STREAM;
    HdaDevice->Path = HdapGetPrimaryPath(HdaDevice);
    if (HdaDevice->Path == NULL) {
        Status = STATUS_INVALID_CONFIGURATION;
        goto CreateSoundDeviceEnd;
    }

    HdaDevice->State = SoundDeviceStateUninitialized;
    SoundDevice->Version = SOUND_DEVICE_VERSION;
    SoundDevice->StructureSize = sizeof(SOUND_DEVICE) + RatesSize + RoutesSize;
    SoundDevice->Context = HdaDevice;
    WidgetType = HDA_GET_WIDGET_TYPE(Widget);
    Capabilities = SOUND_CAPABILITY_MMAP | SOUND_CAPABILITY_MANUAL_ENABLE;
    if (WidgetType == HDA_AUDIO_WIDGET_TYPE_INPUT) {
        SoundDevice->Type = SoundDeviceInput;
        if  ((Widget->WidgetCapabilities & HDA_AUDIO_WIDGET_DIGITAL) != 0) {
            Capabilities |= SOUND_CAPABILITY_INTERFACE_DIGITAL_IN;

        } else {
            Capabilities |= SOUND_CAPABILITY_INTERFACE_ANALOG_IN;
        }

        Capabilities |= SOUND_CAPABILITY_INPUT;

    } else {

        ASSERT(WidgetType == HDA_AUDIO_WIDGET_TYPE_OUTPUT);

        SoundDevice->Type = SoundDeviceOutput;
        if  ((Widget->WidgetCapabilities & HDA_AUDIO_WIDGET_DIGITAL) != 0) {
            Capabilities |= SOUND_CAPABILITY_INTERFACE_DIGITAL_OUT;

        } else {
            Capabilities |= SOUND_CAPABILITY_INTERFACE_ANALOG_OUT;
        }

        Capabilities |= SOUND_CAPABILITY_OUTPUT;
    }

    Formats = 0;
    if ((Widget->SupportedStreamFormats & HDA_STREAM_FORMAT_AC3) != 0) {
        Formats |= SOUND_FORMAT_AC3;
    }

    if ((Widget->SupportedStreamFormats & HDA_STREAM_FORMAT_FLOAT32) != 0) {
        Formats |= SOUND_FORMAT_FLOAT;
    }

    if ((Widget->SupportedStreamFormats & HDA_STREAM_FORMAT_PCM) != 0) {
        SupportedSizes = Widget->SupportedPcmSizes;
        SupportedIndex = 0;
        while (SupportedSizes != 0) {
            if ((SupportedSizes & 0x1) != 0) {
                Formats |= HdaPcmSizeFormats[SupportedIndex];
            }

            SupportedSizes >>= 1;
            SupportedIndex += 1;
        }
    }

    if (Group->Type == HDA_FUNCTION_GROUP_TYPE_MODEM) {
        Capabilities |= SOUND_CAPABILITY_MODEM;
    }

    //
    // Use the maximum channel count as the preferred channel count. If the
    // maximum channel count is greater than or equal to 2 (stereo or better),
    // then the minimum channel must unfortunately be 2 as well. Real Intel HD
    // Audio devices with a maximum channel count of 2 should (and do) support
    // mono sound, but VirtualBox 5.1.22 (and older) has a bug. In
    // hdaAddStreamOut, it forces the channel count to 2, disregarding what had
    // previously been recorded from the write to the stream's format register.
    // This causes the VirtualBox backend to interpret mono audio as stereo
    // audio and it gets played twice as fast.
    //

    MaxChannelCount = HDA_GET_WIDGET_CHANNEL_COUNT(Widget);
    if (MaxChannelCount == 1) {
        MinChannelCount = 1;
        Capabilities |= SOUND_CAPABILITY_CHANNEL_MONO;

    } else {
        MinChannelCount = 2;
        if (MaxChannelCount > 2) {
            Capabilities |= SOUND_CAPABILITY_CHANNEL_MULTI;

        } else {
            Capabilities |= SOUND_CAPABILITY_CHANNEL_STEREO;
        }
    }

    SoundDevice->Capabilities = Capabilities;
    SoundDevice->Formats = Formats;
    SoundDevice->MinChannelCount = MinChannelCount;
    SoundDevice->MaxChannelCount = MaxChannelCount;
    SoundDevice->RateCount = RateCount;
    SoundDevice->RatesOffset = sizeof(SOUND_DEVICE);
    SoundDevice->RouteCount = RouteCount;
    SoundDevice->RoutesOffset = sizeof(SOUND_DEVICE) + RatesSize;
    Rates = (PVOID)SoundDevice + SoundDevice->RatesOffset;
    RateIndex = 0;
    SupportedIndex = 0;
    while (SupportedRates != 0) {
        if ((SupportedRates & 0x1) != 0) {
            Rates[RateIndex] = HdaSampleRates[SupportedIndex].Rate;
            RateIndex += 1;
        }

        SupportedRates >>= 1;
        SupportedIndex += 1;
    }

    ASSERT(RateIndex == RateCount);
    ASSERT(RouteCount != 0);

    //
    // Fill out the route information for the device. The primary path should
    // be stored as the first route.
    //

    Routes = (PVOID)SoundDevice + SoundDevice->RoutesOffset;
    RouteIndex = 0;
    Routes[RouteIndex].Type = HdaDevice->Path->RouteType;
    Routes[RouteIndex].Context = HdaDevice->Path;
    RouteIndex += 1;
    CurrentEntry = Group->PathList[HdaDevice->Path->Type].Next;
    while (CurrentEntry != &(Group->PathList[HdaDevice->Path->Type])) {
        CurrentPath = LIST_VALUE(CurrentEntry, HDA_PATH, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if ((CurrentPath != HdaDevice->Path) &&
            (Widget == &(Group->Widgets[CurrentPath->Widgets[0]]))) {

            ASSERT(RouteIndex < RouteCount);

            Routes[RouteIndex].Type = CurrentPath->RouteType;
            Routes[RouteIndex].Context = CurrentPath;
            RouteIndex += 1;
        }
    }

    Status = STATUS_SUCCESS;

CreateSoundDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (SoundDevice != NULL) {
            HdapDestroySoundDevice(SoundDevice);
            SoundDevice = NULL;
        }
    }

    //
    // The sound device is returned rather than the internal HDA device because
    // initializing the sound core library requires passing an array of sound
    // devices. The controller stores those rather than the HDA devices. It is
    // easy to find one from the other.
    //

    return SoundDevice;
}

VOID
HdapDestroySoundDevice (
    PSOUND_DEVICE SoundDevice
    )

/*++

Routine Description:

    This routine destroys a sound device and all of its resources.

Arguments:

    SoundDevice - Supplies a pointer to the sound device to destroy.

Return Value:

    None.

--*/

{

    PHDA_DEVICE HdaDevice;

    //
    // The start of the allocation is actually the internal HDA device.
    //

    HdaDevice = PARENT_STRUCTURE(SoundDevice, HDA_DEVICE, SoundDevice);
    MmFreeNonPagedPool(HdaDevice);
    return;
}

KSTATUS
HdapValidateCodec (
    PHDA_CODEC Codec,
    PBOOL Valid
    )

/*++

Routine Description:

    This routine determines whether the given codec is still valid at its
    address. If a the device ID, vendor ID, and revision match then it is
    deemed valid.

Arguments:

    Codec - Supplies a pointer to the codec to validate.

    Valid - Supplies a pointer that receives a boolean indicating whether or
        not the codec is still valid at its address.

Return Value:

    Status code.

--*/

{

    USHORT DeviceId;
    ULONG Parameter;
    ULONG Revision;
    KSTATUS Status;
    USHORT VendorId;

    *Valid = FALSE;
    Status = HdapCodecGetParameter(Codec,
                                   HDA_ROOT_NODE_ID,
                                   HdaParameterVendorId,
                                   &Parameter);

    if (!KSUCCESS(Status)) {
        goto ValidateCodecEnd;
    }

    VendorId = (Parameter & HDA_VENDOR_ID_VENDOR_MASK) >>
               HDA_VENDOR_ID_VENDOR_SHIFT;

    DeviceId = (Parameter & HDA_VENDOR_ID_DEVICE_MASK) >>
               HDA_VENDOR_ID_DEVICE_SHIFT;

    if ((Codec->VendorId != VendorId) || (Codec->DeviceId != DeviceId)) {
        goto ValidateCodecEnd;
    }

    Status = HdapCodecGetParameter(Codec,
                                   HDA_ROOT_NODE_ID,
                                   HdaParameterRevisionId,
                                   &Revision);

    if (!KSUCCESS(Status)) {
        goto ValidateCodecEnd;
    }

    if (Codec->Revision != Revision) {
        goto ValidateCodecEnd;
    }

    *Valid = TRUE;

ValidateCodecEnd:
    return Status;
}

KSTATUS
HdapCreateAndEnumerateCodec (
    PHDA_CONTROLLER Controller,
    UCHAR Address,
    PHDA_CODEC *Codec
    )

/*++

Routine Description:

    This routine creates an HDA codec structure and enumerates it.

Arguments:

    Controller - Supplies a pointer to the HD Audio controller to which the
        codec is connected.

    Address - Supplies the address of the codec.

    Codec - Supplies a pointer that receives a pointer to the new codec.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    PHDA_FUNCTION_GROUP Group;
    UCHAR GroupCount;
    UCHAR GroupIndex;
    USHORT GroupNodeId;
    UCHAR GroupNodeStart;
    PHDA_CODEC NewCodec;
    ULONG Parameter;
    KSTATUS Status;

    NewCodec = NULL;

    //
    // Get the number of function groups attached to this codec.
    //

    Status = HdapGetParameter(Controller,
                              Address,
                              HDA_ROOT_NODE_ID,
                              HdaParameterSubordinateNodeCount,
                              &Parameter);

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateCodecEnd;
    }

    GroupNodeStart = (Parameter & HDA_SUBORDINATE_NODE_START_MASK) >>
                     HDA_SUBORDINATE_NODE_START_SHIFT;

    GroupCount = (Parameter & HDA_SUBORDINATE_NODE_COUNT_MASK) >>
                 HDA_SUBORDINATE_NODE_COUNT_SHIFT;

    AllocationSize = sizeof(HDA_CODEC) +
                     (sizeof(PHDA_FUNCTION_GROUP) *
                      (GroupCount - ANYSIZE_ARRAY));

    NewCodec = MmAllocatePagedPool(AllocationSize, HDA_ALLOCATION_TAG);
    if (NewCodec == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndEnumerateCodecEnd;
    }

    RtlZeroMemory(NewCodec, AllocationSize);
    NewCodec->Controller = Controller;
    NewCodec->Address = Address;
    NewCodec->FunctionGroupNodeStart = GroupNodeStart;
    NewCodec->FunctionGroupCount = GroupCount;

    //
    // Get the vendor ID, device ID, and revision to identify the codec.
    //

    Status = HdapCodecGetParameter(NewCodec,
                                   HDA_ROOT_NODE_ID,
                                   HdaParameterVendorId,
                                   &Parameter);

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateCodecEnd;
    }

    NewCodec->VendorId = (Parameter & HDA_VENDOR_ID_VENDOR_MASK) >>
                         HDA_VENDOR_ID_VENDOR_SHIFT;

    NewCodec->DeviceId = (Parameter & HDA_VENDOR_ID_DEVICE_MASK) >>
                         HDA_VENDOR_ID_DEVICE_SHIFT;

    Status = HdapCodecGetParameter(NewCodec,
                                   HDA_ROOT_NODE_ID,
                                   HdaParameterRevisionId,
                                   &(NewCodec->Revision));

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateCodecEnd;
    }

    //
    // Initialize each of the function groups. This driver only uses the audio
    // function groups for now, but may support other groups in the future.
    //

    for (GroupIndex = 0; GroupIndex < GroupCount; GroupIndex += 1) {
        GroupNodeId = (USHORT)GroupNodeStart + GroupIndex;
        Status = HdapCreateAndEnumerateFunctionGroup(NewCodec,
                                                     GroupNodeId,
                                                     &Group);

        if (!KSUCCESS(Status)) {
            goto CreateAndEnumerateCodecEnd;
        }

        Status = HdapResetFunctionGroup(NewCodec, Group);
        if (!KSUCCESS(Status)) {
            goto CreateAndEnumerateCodecEnd;
        }

        NewCodec->FunctionGroups[GroupIndex] = Group;
    }

CreateAndEnumerateCodecEnd:
    if (!KSUCCESS(Status)) {
        if (NewCodec != NULL) {
            HdapDestroyCodec(NewCodec);
            NewCodec = NULL;
        }
    }

    *Codec = NewCodec;
    return Status;
}

VOID
HdapDestroyCodec (
    PHDA_CODEC Codec
    )

/*++

Routine Description:

    This routine destroys a codec and all of its resources.

Arguments:

    Codec - Supplies a pointer to a codec.

Return Value:

    None.

--*/

{

    ULONG Index;

    for (Index = 0; Index < Codec->FunctionGroupCount; Index += 1) {
        if (Codec->FunctionGroups[Index] != NULL) {
            HdapDestroyFunctionGroup(Codec->FunctionGroups[Index]);
        }
    }

    MmFreePagedPool(Codec);
    return;
}

KSTATUS
HdapCreateAndEnumerateFunctionGroup (
    PHDA_CODEC Codec,
    USHORT NodeId,
    PHDA_FUNCTION_GROUP *Group
    )

/*++

Routine Description:

    This routine creates and enumerates a function group.

Arguments:

    Codec - Supplies a pointer to the codec to which the function group
        belongs.

    NodeId - Supplies the ID of the function group node.

    Group - Supplies a pointer that receives a pointer to the newly allocated
        function group.

Return Value:

    Returns a pointer to the newly allocated function group on success, or
    NULL on failure.

--*/

{

    UINTN AllocationSize;
    ULONG Index;
    PHDA_FUNCTION_GROUP NewGroup;
    ULONG Parameter;
    KSTATUS Status;
    PHDA_WIDGET Widget;
    UCHAR WidgetCount;
    UCHAR WidgetIndex;
    USHORT WidgetNodeId;
    UCHAR WidgetNodeStart;

    NewGroup = NULL;

    //
    // Get the number of widgets attached to the function group.
    //

    Status = HdapCodecGetParameter(Codec,
                                   NodeId,
                                   HdaParameterSubordinateNodeCount,
                                   &Parameter);

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateFunctionGroupEnd;
    }

    WidgetNodeStart = (Parameter & HDA_SUBORDINATE_NODE_START_MASK) >>
                      HDA_SUBORDINATE_NODE_START_SHIFT;

    WidgetCount = (Parameter & HDA_SUBORDINATE_NODE_COUNT_MASK) >>
                  HDA_SUBORDINATE_NODE_COUNT_SHIFT;

    AllocationSize = sizeof(HDA_FUNCTION_GROUP) +
                     (sizeof(HDA_WIDGET) * (WidgetCount - ANYSIZE_ARRAY));

    NewGroup = MmAllocatePagedPool(AllocationSize, HDA_ALLOCATION_TAG);
    if (NewGroup == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndEnumerateFunctionGroupEnd;
    }

    RtlZeroMemory(NewGroup, AllocationSize);
    NewGroup->NodeId = NodeId;
    NewGroup->WidgetNodeStart = WidgetNodeStart;
    NewGroup->WidgetCount = WidgetCount;
    for (Index = 0; Index < HdaPathTypeCount; Index += 1) {
        INITIALIZE_LIST_HEAD(&(NewGroup->PathList[Index]));
    }

    //
    // Get the function group type.
    //

    Status = HdapCodecGetParameter(Codec,
                                   NewGroup->NodeId,
                                   HdaParameterFunctionGroupType,
                                   &Parameter);

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateFunctionGroupEnd;
    }

    NewGroup->Type = (Parameter & HDA_FUNCTION_GROUP_TYPE_MASK) >>
                     HDA_FUNCTION_GROUP_TYPE_SHIFT;

    //
    // The function group reset command must be sent twice if extended power
    // states are supported by the function group node or any widget. Check
    // to see if the function group has extended power states.
    //

    Status = HdapCodecGetParameter(Codec,
                                   NewGroup->NodeId,
                                   HdaParameterSupportedPowerStates,
                                   &Parameter);

    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateFunctionGroupEnd;
    }

    if ((Parameter & HDA_SUPPORTED_POWER_STATES_EXTENDED) != 0) {
        NewGroup->Flags |= HDA_FUNCTION_GROUP_FLAG_EXTENDED_POWER_STATES;
    }

    //
    // If this is an audio function group, record the default formats and rates.
    //

    if (NewGroup->Type == HDA_FUNCTION_GROUP_TYPE_AUDIO) {
        Status = HdapCodecGetParameter(Codec,
                                       NewGroup->NodeId,
                                       HdaParameterSupportedStreamFormats,
                                       &(NewGroup->SupportedStreamFormats));

        if (!KSUCCESS(Status)) {
            goto CreateAndEnumerateFunctionGroupEnd;
        }

        Status = HdapCodecGetParameter(Codec,
                                       NewGroup->NodeId,
                                       HdaParameterSupportedPcmSizeRates,
                                       &Parameter);

        if (!KSUCCESS(Status)) {
            goto CreateAndEnumerateFunctionGroupEnd;
        }

        NewGroup->SupportedPcmSizes = (Parameter &
                                       HDA_PCM_SIZE_RATES_SIZE_MASK) >>
                                      HDA_PCM_SIZE_RATES_SIZE_SHIFT;

        NewGroup->SupportedRates = (Parameter &
                                    HDA_PCM_SIZE_RATES_RATE_MASK) >>
                                   HDA_PCM_SIZE_RATES_RATE_SHIFT;
    }

    if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
        RtlDebugPrint("HDA: Created function group:\n"
                      "\tCodec: 0x%08x\n"
                      "\tType: 0x%02x\n"
                      "\tNodeId: 0x%04x\n"
                      "\tWidget Count: 0x%02x\n"
                      "\tFlags 0x%08x\n",
                      Codec,
                      NewGroup->Type,
                      NewGroup->NodeId,
                      NewGroup->WidgetCount,
                      NewGroup->Flags);
    }

    //
    // Enumerate each widget. Record its type and volume control information.
    // These widgets will be used to create paths and the volume needs to be
    // adjusted along the whole path when playing or recording sound.
    //

    for (WidgetIndex = 0; WidgetIndex < WidgetCount; WidgetIndex += 1) {
        WidgetNodeId = (USHORT)WidgetNodeStart + WidgetIndex;
        Widget = &(NewGroup->Widgets[WidgetIndex]);
        Widget->NodeId = WidgetNodeId;
        Status = HdapEnumerateWidget(Codec, NewGroup, Widget);
        if (!KSUCCESS(Status)) {
            goto CreateAndEnumerateFunctionGroupEnd;
        }
    }

    //
    // Find all the input and output paths for the group.
    //

    Status = HdapEnumeratePaths(Codec, NewGroup);
    if (!KSUCCESS(Status)) {
        goto CreateAndEnumerateFunctionGroupEnd;
    }

CreateAndEnumerateFunctionGroupEnd:
    if (!KSUCCESS(Status)) {
        if (NewGroup != NULL) {
            HdapDestroyFunctionGroup(NewGroup);
            NewGroup = NULL;
        }

        if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
            RtlDebugPrint("HDA: Failed to create function group: "
                          "Codec 0x%08x, GroupNode 0x%02x: %d\n",
                          Codec,
                          NodeId,
                          Status);
        }
    }

    *Group = NewGroup;
    return Status;
}

VOID
HdapDestroyFunctionGroup (
    PHDA_FUNCTION_GROUP Group
    )

/*++

Routine Description:

    This routine destroys a function group.

Arguments:

    Group - Supplies a pointer to the function group to destroy.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PHDA_PATH Path;
    HDA_PATH_TYPE PathType;

    for (PathType = 0; PathType < HdaPathTypeCount; PathType += 1) {
        CurrentEntry = Group->PathList[PathType].Next;
        while (CurrentEntry != &(Group->PathList[PathType])) {
            Path = LIST_VALUE(CurrentEntry, HDA_PATH, ListEntry);
            CurrentEntry = CurrentEntry->Next;
            HdapDestroyPath(Path);
        }
    }

    MmFreePagedPool(Group);
    return;
}

KSTATUS
HdapEnumerateWidget (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    )

/*++

Routine Description:

    This routine enumerates a widget, collecting basic information about the
    widget.

Arguments:

    Codec - Supplies a pointer to the codec to which the widget belongs.

    Group - Supplies a pointer to the function group to which the widget
        belongs.

    Widget - Supplies a pointer to the widget to enumerate.

Return Value:

    Status code.

--*/

{

    ULONG Parameter;
    KSTATUS Status;
    HDA_PARAMETER TypeCapabilitiesId;

    Status = HdapCodecGetParameter(Codec,
                                   Widget->NodeId,
                                   HdaParameterAudioWidgetCapabilities,
                                   &(Widget->WidgetCapabilities));

    if (!KSUCCESS(Status)) {
        goto EnumerateWidgetEnd;
    }

    //
    // Get any type-specific capabilities or extra information.
    //

    TypeCapabilitiesId = 0;
    switch (HDA_GET_WIDGET_TYPE(Widget)) {
    case HDA_AUDIO_WIDGET_TYPE_PIN:
        Status = HdapCodecGetSetVerb(Codec,
                                     Widget->NodeId,
                                     HdaVerbGetConfigurationDefault,
                                     0,
                                     &(Widget->PinConfiguration));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

        TypeCapabilitiesId = HdaParameterPinCapabilities;
        break;

    case HDA_AUDIO_WIDGET_TYPE_VOLUME_KNOB:
        TypeCapabilitiesId = HdaParameterVolumeKnobCapabilities;
        break;

    //
    // Get the supported stream formats and sample rates for all input and
    // output audio converters. If the converter node returns 0 for a
    // parameter, then override it with the group's default values.
    //

    case HDA_AUDIO_WIDGET_TYPE_INPUT:
    case HDA_AUDIO_WIDGET_TYPE_OUTPUT:
        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       HdaParameterSupportedStreamFormats,
                                       &(Widget->SupportedStreamFormats));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

        if (Widget->SupportedStreamFormats == 0) {
            Widget->SupportedStreamFormats = Group->SupportedStreamFormats;
        }

        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       HdaParameterSupportedPcmSizeRates,
                                       &Parameter);

        if (Parameter == 0) {
            Widget->SupportedRates = Group->SupportedRates;
            Widget->SupportedPcmSizes = Group->SupportedPcmSizes;

        } else {
            Widget->SupportedRates = (Parameter &
                                      HDA_PCM_SIZE_RATES_RATE_MASK) >>
                                     HDA_PCM_SIZE_RATES_RATE_SHIFT;

            Widget->SupportedPcmSizes = (Parameter &
                                         HDA_PCM_SIZE_RATES_SIZE_MASK) >>
                                        HDA_PCM_SIZE_RATES_SIZE_SHIFT;
        }

        break;

    default:
        break;
    }

    if (TypeCapabilitiesId != 0) {
        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       TypeCapabilitiesId,
                                       &(Widget->TypeCapabilities));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }
    }

    //
    // Get the input and output amplifier gain/mute capabilities. Get them from
    // the widget if it overrides the function groups capabilties. Otherwise
    // get them from the function group.
    //

    if ((Widget->WidgetCapabilities & HDA_AUDIO_WIDGET_AMP_OVERRIDE) != 0) {
        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       HdaParameterInputAmplifierCapabilities,
                                       &(Widget->InputAmplifier));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       HdaParameterOutputAmplifierCapabilities,
                                       &(Widget->OutputAmplifier));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

    } else {
        Status = HdapCodecGetParameter(Codec,
                                       Group->NodeId,
                                       HdaParameterInputAmplifierCapabilities,
                                       &(Widget->InputAmplifier));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

        Status = HdapCodecGetParameter(Codec,
                                       Group->NodeId,
                                       HdaParameterOutputAmplifierCapabilities,
                                       &(Widget->OutputAmplifier));

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }
    }

    //
    // Check this widget if extended power states have not been detected.
    //

    if ((Group->Flags & HDA_FUNCTION_GROUP_FLAG_EXTENDED_POWER_STATES) == 0) {
        Status = HdapCodecGetParameter(Codec,
                                       Widget->NodeId,
                                       HdaParameterSupportedPowerStates,
                                       &Parameter);

        if (!KSUCCESS(Status)) {
            goto EnumerateWidgetEnd;
        }

        if ((Parameter & HDA_SUPPORTED_POWER_STATES_EXTENDED) != 0) {
            Group->Flags |= HDA_FUNCTION_GROUP_FLAG_EXTENDED_POWER_STATES;
        }
    }

    if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
        RtlDebugPrint("HDA: Created widget:\n"
                      "\tCodec: 0x%08x\n"
                      "\tGroup: 0x%08x\n"
                      "\tNodeId: 0x%04x\n"
                      "\tWidget Cap: 0x%08x\n"
                      "\tType Cap: 0x%08x\n"
                      "\tPin Config: 0x%08x\n"
                      "\tInput Amp: 0x%08x\n"
                      "\tOutput Amp: 0x%08x\n"
                      "\tRates 0x%04x\n"
                      "\tPcm Sizes: 0x%04x\n"
                      "\tStream Formats: 0x%08x\n",
                      Codec,
                      Group,
                      Widget->NodeId,
                      Widget->WidgetCapabilities,
                      Widget->TypeCapabilities,
                      Widget->PinConfiguration,
                      Widget->InputAmplifier,
                      Widget->OutputAmplifier,
                      Widget->SupportedRates,
                      Widget->SupportedPcmSizes,
                      Widget->SupportedStreamFormats);
    }

EnumerateWidgetEnd:
    return Status;
}

KSTATUS
HdapEnableWidgets (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    )

/*++

Routine Description:

    This routine enables all of the widgets in a function group.

Arguments:

    Codec - Supplies a pointer to the HDA codec that owns the group.

    Group - Supplies a pointer to the function group whose widgets needs to be
        enabled.

Return Value:

    Status code.

--*/

{

    ULONG Index;
    KSTATUS Status;
    ULONG Value;
    PHDA_WIDGET Widget;

    for (Index = 0; Index < Group->WidgetCount; Index += 1) {
        Widget = &(Group->Widgets[Index]);
        if ((Widget->WidgetCapabilities &
             HDA_AUDIO_WIDGET_POWER_CONTROL) != 0) {

            Status = HdapCodecGetSetVerb(Codec,
                                         Widget->NodeId,
                                         HdaVerbSetPowerState,
                                         HDA_POWER_STATE_D0,
                                         NULL);

            if (!KSUCCESS(Status)) {
                goto EnableWidgetsEnd;
            }
        }

        //
        // If the pin has external amplifier power down support (EAPD),
        // then make sure the amplifier is on. The HDA spec claims the
        // amplifier should be powered on within 85 milliseconds. In practice,
        // it takes much longer (a few seconds). Hopefully it's on by the time
        // the system boots and the user is ready to play sound.
        //

        if ((HDA_GET_WIDGET_TYPE(Widget) == HDA_AUDIO_WIDGET_TYPE_PIN) &&
            ((Widget->TypeCapabilities & HDA_PIN_CAPABILITIES_EAPD) != 0)) {

            Status = HdapCodecGetSetVerb(Codec,
                                         Widget->NodeId,
                                         HdaVerbGetEapdBtlEnable,
                                         0,
                                         &Value);

            if (!KSUCCESS(Status)) {
                goto EnableWidgetsEnd;
            }

            if ((Value & HDA_EAPD_BTL_ENABLE_EAPD) == 0) {
                Value |= HDA_EAPD_BTL_ENABLE_EAPD;
                Status = HdapCodecGetSetVerb(Codec,
                                             Widget->NodeId,
                                             HdaVerbSetEapdBtlEnable,
                                             Value,
                                             NULL);

                if (!KSUCCESS(Status)) {
                    goto EnableWidgetsEnd;
                }
            }
        }
    }

    Status = HdapCodecCommandBarrier(Codec);
    if (!KSUCCESS(Status)) {
        goto EnableWidgetsEnd;
    }

EnableWidgetsEnd:
    return Status;
}

KSTATUS
HdapEnumeratePaths (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    )

/*++

Routine Description:

    This routine enumerates all of the paths supported by the function group.

Arguments:

    Codec - Supplies a pointer to the codec to which the group belongs.

    Group - Supplies a pointer to the function group whose paths are to be
        enumerated.

Return Value:

    Status code.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PHDA_PATH CurrentPath;
    BOOL OutputDevice;
    ULONG Path[HDA_MAX_PATH_LENGTH];
    ULONG PathIndex;
    KSTATUS Status;
    ULONG TypeIndex;
    PHDA_WIDGET Widget;
    UCHAR WidgetIndex;

    //
    // Paths worth saving start at either an output pin widget or an input
    // widget. Enumerate them all.
    //

    for (WidgetIndex = 0; WidgetIndex < Group->WidgetCount; WidgetIndex += 1) {
        Widget = &(Group->Widgets[WidgetIndex]);
        switch (HDA_GET_WIDGET_TYPE(Widget)) {

        //
        // Input (ADC) widgets can be the start of a path.
        //

        case HDA_AUDIO_WIDGET_TYPE_INPUT:

            //
            // Find and create all paths that end at this ADC, startin from
            // input pins.
            //

            Status = HdapFindPaths(Codec,
                                   Group,
                                   Widget,
                                   HdaPathAdcFromInput,
                                   Path,
                                   0);

            if (!KSUCCESS(Status)) {
                goto EnumeratePathsEnd;
            }

            break;

        //
        // An output pin widget attached to an output device with a connected
        // port can be the start of a path search. Once found, the path order
        // will be reversed to start with the DAC.
        //

        case HDA_AUDIO_WIDGET_TYPE_PIN:
            if ((Widget->TypeCapabilities & HDA_PIN_CAPABILITIES_OUTPUT) == 0) {
                break;
            }

            if (HDA_IS_PIN_WIDGET_CONNECTED(Widget) == FALSE) {
                break;
            }

            OutputDevice = HdapIsOutputDevice(Widget);
            if (OutputDevice == FALSE) {
                break;
            }

            //
            // Find and create all of the output paths. If this encounters an
            // input pin, it will create an "input to output path".
            //

            Status = HdapFindPaths(Codec,
                                   Group,
                                   Widget,
                                   HdaPathDacToOutput,
                                   Path,
                                   0);

            if (!KSUCCESS(Status)) {
                goto EnumeratePathsEnd;
            }

            break;

        //
        // Don't do anything for the other widget types.
        //

        default:
            break;
        }
    }

    //
    // Print out the discovered paths.
    //

    if ((HdaDebugFlags & HDA_DEBUG_FLAG_CODEC_ENUMERATION) != 0) {
        for (TypeIndex = 0; TypeIndex < HdaPathTypeCount; TypeIndex += 1) {
            RtlDebugPrint("HDA: %s paths:\n", HdaPathTypeNames[TypeIndex]);
            CurrentEntry = Group->PathList[TypeIndex].Next;
            while (CurrentEntry != &(Group->PathList[TypeIndex])) {
                CurrentPath = LIST_VALUE(CurrentEntry, HDA_PATH, ListEntry);
                CurrentEntry = CurrentEntry->Next;
                for (PathIndex = 0;
                     PathIndex < CurrentPath->Length;
                     PathIndex += 1) {

                    WidgetIndex = CurrentPath->Widgets[PathIndex];
                    RtlDebugPrint("0x%04x ",
                                  Group->Widgets[WidgetIndex].NodeId);
                }

                RtlDebugPrint("\n");
            }

            RtlDebugPrint("\n");
        }
    }

    Status = STATUS_SUCCESS;

EnumeratePathsEnd:
    return Status;
}

KSTATUS
HdapFindPaths (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget,
    HDA_PATH_TYPE PathType,
    PULONG Path,
    ULONG PathLength
    )

/*++

Routine Description:

    This routine finds and creates all the paths of the given type that can be
    found below the given widget. It will add them to the functino group's list
    of paths of that type.

Arguments:

    Codec - Supplies a pointer to the codec that owns the paths.

    Group - Supplies a pointer to the function group that owns that paths.

    Widget - Supplies a pointer to the first widget in the path.

    PathType - Supplies the type of path to find.

    Path - Supplies a pointer to the path up to this point. This is an array
        that can hold up to HDA_MAX_PATH_LENGTH entries.

    PathLength - Supplies the current length of the path.

Return Value:

    Status code.

--*/

{

    BOOL CreatePath;
    ULONG Entries;
    ULONG EntriesCount;
    ULONG EntriesIndex;
    ULONG EntriesThisRound;
    ULONG EntryIndex;
    PHDA_WIDGET EntryWidget;
    BOOL InputDevice;
    BOOL InputPin;
    ULONG ListLength;
    PUSHORT LongEntries;
    ULONG MaxEntriesPerRound;
    USHORT NodeId;
    BOOL RangeEntry;
    USHORT RangeStart;
    PUCHAR ShortEntries;
    KSTATUS Status;
    UCHAR WidgetType;

    //
    // If the current path is already too long, then exit. Nothing was found.
    //

    if (PathLength >= HDA_MAX_PATH_LENGTH) {
        Status = STATUS_SUCCESS;
        goto FindPathsEnd;
    }

    //
    // Otherwise add this widget to the path.
    //

    Path[PathLength] = HDA_GET_WIDGET_GROUP_INDEX(Group, Widget);
    PathLength += 1;

    //
    // Paths of length one aren't allowed, so skip the termination checks if
    // this is the first entry.
    //

    if (PathLength > 1) {

        //
        // Check to see if this is an input pin. Multiple path types can
        // terminate on an input pin.
        //

        InputPin = FALSE;
        WidgetType = HDA_GET_WIDGET_TYPE(Widget);
        if ((WidgetType == HDA_AUDIO_WIDGET_TYPE_PIN) &&
            ((Widget->TypeCapabilities & HDA_PIN_CAPABILITIES_INPUT) != 0)) {

            InputPin = TRUE;
        }

        //
        // If this completes a path, then allocate that new path and add it to
        // the group.
        //

        CreatePath = FALSE;
        switch (PathType) {

        //
        // Both input and "karaoke" paths terminate once an input pin is found.
        //

        case HdaPathInputToOutput:
        case HdaPathAdcFromInput:
            if (InputPin == FALSE) {
                break;
            }

            //
            // If the input pin is not connected, terminate the search without
            // adding any more paths.
            //

            if (HDA_IS_PIN_WIDGET_CONNECTED(Widget) == FALSE) {
                Status = STATUS_SUCCESS;
                goto FindPathsEnd;
            }

            //
            // If the input pin is not attached to an input device, terminate
            // the search.
            //

            InputDevice = HdapIsInputDevice(Widget);
            if (InputDevice == FALSE) {
                Status = STATUS_SUCCESS;
                goto FindPathsEnd;
            }

            CreatePath = TRUE;
            break;

        //
        // Output paths terminate once a DAC is found.
        //

        case HdaPathDacToOutput:

            //
            // Output paths that reach an input pin, get converted to "karaoke"
            // paths - output that comes from an input pin. If that pin is not
            // connected to a port or not attached to an input device, then
            // just terminate the search.
            //

            if (InputPin != FALSE) {
                if (HDA_IS_PIN_WIDGET_CONNECTED(Widget) == FALSE) {
                    Status = STATUS_SUCCESS;
                    goto FindPathsEnd;
                }

                InputDevice = HdapIsInputDevice(Widget);
                if (InputDevice == FALSE) {
                    Status = STATUS_SUCCESS;
                    goto FindPathsEnd;
                }

                PathType = HdaPathInputToOutput;

            } else if (WidgetType != HDA_AUDIO_WIDGET_TYPE_OUTPUT) {
                break;
            }

            CreatePath = TRUE;
            break;

        default:

            ASSERT(FALSE);

            break;
        }

        if (CreatePath != FALSE) {
            Status = HdapCreatePath(Codec, Group, PathType, Path, PathLength);
            goto FindPathsEnd;
        }
    }

    //
    // Recurse on each widget in this widget's connection list.
    //

    Status = HdapCodecGetParameter(Codec,
                                   Widget->NodeId,
                                   HdaParameterConnectionListLength,
                                   &ListLength);

    if (!KSUCCESS(Status)) {
        goto FindPathsEnd;
    }

    MaxEntriesPerRound = sizeof(ULONG) / sizeof(UCHAR);
    if ((ListLength & HDA_CONNECTION_LIST_LENGTH_LONG_FORM) != 0) {
        MaxEntriesPerRound = sizeof(ULONG) / sizeof(USHORT);
    }

    RangeStart = 0;
    EntriesCount = (ListLength & HDA_CONNECTION_LIST_LENGTH_MASK) >>
                   HDA_CONNECTION_LIST_LENGTH_SHIFT;

    EntriesIndex = 0;
    LongEntries = (PUSHORT)&Entries;
    ShortEntries = (PUCHAR)&Entries;
    while (EntriesIndex < EntriesCount) {
        Status = HdapCodecGetSetVerb(Codec,
                                     Widget->NodeId,
                                     HdaVerbGetConnectionListEntry,
                                     EntriesIndex,
                                     &Entries);

        if (!KSUCCESS(Status)) {
            goto FindPathsEnd;
        }

        EntriesThisRound = EntriesCount - EntriesIndex;
        if (EntriesThisRound > MaxEntriesPerRound) {
            EntriesThisRound = MaxEntriesPerRound;
        }

        for (EntryIndex = 0;
             EntryIndex < EntriesThisRound;
             EntryIndex += 1) {

            RangeEntry = FALSE;
            if ((ListLength & HDA_CONNECTION_LIST_LENGTH_LONG_FORM) != 0) {
                NodeId = (LongEntries[EntryIndex] &
                          HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_MASK) >>
                         HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_SHIFT;

                if ((LongEntries[EntryIndex] &
                    HDA_CONNECTION_LIST_LONG_FORM_RANGE) != 0) {

                    RangeEntry = TRUE;
                }

            } else {
                NodeId = (ShortEntries[EntryIndex] &
                          HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_MASK) >>
                         HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_SHIFT;

                if ((ShortEntries[EntryIndex] &
                     HDA_CONNECTION_LIST_SHORT_FORM_RANGE) != 0) {

                    RangeEntry = TRUE;
                }
            }

            //
            // If this is a range entry, then check paths for each widget in
            // the range [RangeStart, NodeId). The current node ID is always
            // checked below.
            //

            if (RangeEntry != FALSE) {

                ASSERT(RangeStart != 0);

                while (RangeStart != NodeId) {
                    EntryWidget = HDA_GET_WIDGET_FROM_ID(Group, RangeStart);
                    Status = HdapFindPaths(Codec,
                                           Group,
                                           EntryWidget,
                                           PathType,
                                           Path,
                                           PathLength);

                    if (!KSUCCESS(Status)) {
                        goto FindPathsEnd;
                    }

                    RangeStart += 1;
                }
            }

            EntryWidget = HDA_GET_WIDGET_FROM_ID(Group, NodeId);
            Status = HdapFindPaths(Codec,
                                   Group,
                                   EntryWidget,
                                   PathType,
                                   Path,
                                   PathLength);

            if (!KSUCCESS(Status)) {
                goto FindPathsEnd;
            }

            //
            // If the next entry is a range entry, then this node was the true
            // start of the range, but it's already been visited. Move the
            // range start forward a node.
            //

            RangeStart = NodeId + 1;
        }

        EntriesIndex += EntriesThisRound;
    }

    Status = STATUS_SUCCESS;

FindPathsEnd:
    return Status;
}

PHDA_PATH
HdapGetPrimaryPath (
    PHDA_DEVICE Device
    )

/*++

Routine Description:

    This routine finds the primary path that the given device should use.

Arguments:

    Device - Supplies a pointer to a device for which a path should be found.

Return Value:

    Returns a pointer to a path on success or NULL on failure.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PHDA_PATH CurrentPath;
    ULONG FirstIndex;
    PHDA_FUNCTION_GROUP Group;
    ULONG LastIndex;
    PHDA_WIDGET LastWidget;
    ULONG MinPriority;
    HDA_PATH_TYPE PathType;
    PHDA_PATH PrimaryPath;
    ULONG Priority;
    ULONG PriorityMask;
    ULONG WidgetType;

    //
    // Find the primary path for each type. This is based on the pin's
    // association value.
    //

    WidgetType = HDA_GET_WIDGET_TYPE(Device->Widget);
    if (WidgetType == HDA_AUDIO_WIDGET_TYPE_INPUT) {
        PathType = HdaPathAdcFromInput;

    } else if (WidgetType == HDA_AUDIO_WIDGET_TYPE_OUTPUT) {
        PathType = HdaPathDacToOutput;

    } else if (WidgetType == HDA_AUDIO_WIDGET_TYPE_PIN) {
        PathType = HdaPathInputToOutput;

    } else {
        return NULL;
    }

    PriorityMask = HDA_CONFIGURATION_DEFAULT_ASSOCIATION_MASK |
                   HDA_CONFIGURATION_DEFAULT_SEQUENCE_MASK;

    Group = Device->Group;
    MinPriority = MAX_ULONG;
    PrimaryPath = NULL;
    CurrentEntry = Group->PathList[PathType].Next;
    while (CurrentEntry != &(Group->PathList[PathType])) {
        CurrentPath = LIST_VALUE(CurrentEntry, HDA_PATH, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FirstIndex = CurrentPath->Widgets[0];
        LastIndex = CurrentPath->Widgets[CurrentPath->Length - 1];

        //
        // The first widget in the path must match the devices main widget.
        //

        if (Device->Widget != &(Group->Widgets[FirstIndex])) {
            continue;
        }

        LastWidget = &(Group->Widgets[LastIndex]);
        Priority = LastWidget->PinConfiguration & PriorityMask;
        if (Priority < MinPriority) {
            MinPriority = Priority;
            PrimaryPath = CurrentPath;
        }
    }

    return PrimaryPath;
}

ULONG
HdapGetPathCount (
    PHDA_FUNCTION_GROUP Group,
    PHDA_WIDGET Widget
    )

/*++

Routine Description:

    This routine returns the number of paths that start from the given widget.

Arguments:

    Group - Supplies a pointer to the function group to which the widget
        belongs.

    Widget - Supplies a pointer to a widget.

Return Value:

    Returns the number of paths starting from the widget.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PHDA_PATH CurrentPath;
    ULONG FirstIndex;
    ULONG PathCount;
    HDA_PATH_TYPE PathType;
    ULONG WidgetType;

    WidgetType = HDA_GET_WIDGET_TYPE(Widget);
    if (WidgetType == HDA_AUDIO_WIDGET_TYPE_INPUT) {
        PathType = HdaPathAdcFromInput;

    } else if (WidgetType == HDA_AUDIO_WIDGET_TYPE_OUTPUT) {
        PathType = HdaPathDacToOutput;

    } else if (WidgetType == HDA_AUDIO_WIDGET_TYPE_PIN) {
        PathType = HdaPathInputToOutput;

    } else {
        return 0;
    }

    PathCount = 0;
    CurrentEntry = Group->PathList[PathType].Next;
    while (CurrentEntry != &(Group->PathList[PathType])) {
        CurrentPath = LIST_VALUE(CurrentEntry, HDA_PATH, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        FirstIndex = CurrentPath->Widgets[0];
        if (Widget == &(Group->Widgets[FirstIndex])) {
            PathCount += 1;
        }
    }

    return PathCount;
}

KSTATUS
HdapCreatePath (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group,
    HDA_PATH_TYPE PathType,
    PULONG PathWidgets,
    ULONG PathLength
    )

/*++

Routine Description:

    This routine creates a path of widgets that comprise a route through the
    codec for either input or output audio.

Arguments:

    Codec - Supplies a pointer to the HD audio codec that owns the group.

    Group - Supplies a pointer to the function group that owns the path.

    PathType - Supplies the type of the path being created.

    PathWidgets - Supplies an array of widget indices that make up the path.

    PathLength - Supplies the length of the array of widget IDs.

Return Value:

    Status code.

--*/

{

    UINTN AllocationSize;
    ULONG DeviceType;
    ULONG DeviceTypeCount;
    ULONG Index;
    ULONG LastIndex;
    PHDA_WIDGET LastWidget;
    PHDA_PATH NewPath;
    KSTATUS Status;
    PHDA_WIDGET Widget;

    AllocationSize = sizeof(HDA_PATH) +
                     (sizeof(ULONG) * (PathLength - ANYSIZE_ARRAY));

    NewPath = MmAllocatePagedPool(AllocationSize, HDA_ALLOCATION_TAG);
    if (NewPath == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreatePathEnd;
    }

    RtlZeroMemory(NewPath, AllocationSize);
    NewPath->Type = PathType;
    NewPath->Length = PathLength;

    //
    // Mark the converter widget accessible, as a path was found connecting it
    // to and input or output pin.
    //

    Widget = NULL;
    if (PathType == HdaPathAdcFromInput) {
        Widget = &(Group->Widgets[PathWidgets[0]]);

    } else if (PathType == HdaPathDacToOutput) {
        Widget = &(Group->Widgets[PathWidgets[PathLength - 1]]);
    }

    if (Widget != NULL) {
        Widget->Flags |= HDA_WIDGET_FLAG_ACCESSIBLE;
    }

    //
    // Copy the supplied array of widgets that make up the path. Reverse the
    // order if necessary.
    //

    if ((PathType == HdaPathDacToOutput) ||
        (PathType == HdaPathInputToOutput)) {

        for (Index = 0; Index < PathLength; Index += 1) {
            NewPath->Widgets[Index] = PathWidgets[PathLength - 1 - Index];
        }

    } else {
        RtlCopyMemory(NewPath->Widgets,
                      PathWidgets,
                      PathLength * sizeof(ULONG));
    }

    //
    // Store the sound core route type of the path.
    //

    LastIndex = NewPath->Widgets[NewPath->Length - 1];
    LastWidget = &(Group->Widgets[LastIndex]);

    ASSERT(HDA_GET_WIDGET_TYPE(LastWidget) == HDA_AUDIO_WIDGET_TYPE_PIN);

    DeviceType = (LastWidget->PinConfiguration &
                  HDA_CONFIGURATION_DEFAULT_DEVICE_MASK) >>
                 HDA_CONFIGURATION_DEFAULT_DEVICE_SHIFT;

    DeviceTypeCount = sizeof(HdaDeviceTypeToRouteType) /
                      sizeof(HdaDeviceTypeToRouteType[0]);

    if (DeviceType >= DeviceTypeCount) {
        NewPath->RouteType = SoundDeviceRouteUnknown;

    } else {
        NewPath->RouteType = HdaDeviceTypeToRouteType[DeviceType];
    }

    //
    // The controller's lock protects the group's path list.
    //

    ASSERT(KeIsQueuedLockHeld(Codec->Controller->ControllerLock) != FALSE);

    INSERT_BEFORE(&(NewPath->ListEntry), &(Group->PathList[PathType]));
    Status = STATUS_SUCCESS;

CreatePathEnd:
    if (!KSUCCESS(Status)) {
        if (NewPath != NULL) {
            HdapDestroyPath(NewPath);
        }
    }

    return Status;
}

VOID
HdapDestroyPath (
    PHDA_PATH Path
    )

/*++

Routine Description:

    This routine destroys a HD audio widget path.

Arguments:

    Path - Supplies a pointer to the path to destroy.

Return Value:

    None.

--*/

{

    MmFreePagedPool(Path);
    return;
}

KSTATUS
HdapResetFunctionGroup (
    PHDA_CODEC Codec,
    PHDA_FUNCTION_GROUP Group
    )

/*++

Routine Description:

    This routine resets a function group.

Arguments:

    Codec - Supplies a pointer to the codec to which the group belongs.

    Group - Supplies a pointer to the function group to reset.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = HdapCodecGetSetVerb(Codec,
                                 Group->NodeId,
                                 HdaVerbExecuteFunctionGroupReset,
                                 0,
                                 NULL);

    if (!KSUCCESS(Status)) {
        goto ResetFunctionGroupEnd;
    }

    if ((Group->Flags & HDA_FUNCTION_GROUP_FLAG_EXTENDED_POWER_STATES) != 0) {
        Status = HdapCodecGetSetVerb(Codec,
                                     Group->NodeId,
                                     HdaVerbExecuteFunctionGroupReset,
                                     0,
                                     NULL);

        if (!KSUCCESS(Status)) {
            goto ResetFunctionGroupEnd;
        }
    }

    //
    // Make sure the reset commands complete.
    //

    Status = HdapCodecCommandBarrier(Codec);
    if (!KSUCCESS(Status)) {
        goto ResetFunctionGroupEnd;
    }

    //
    // Put it in the D0 power state after reset.
    //

    Status = HdapCodecGetSetVerb(Codec,
                                 Group->NodeId,
                                 HdaVerbSetPowerState,
                                 HDA_POWER_STATE_D0,
                                 NULL);

    if (!KSUCCESS(Status)) {
        goto ResetFunctionGroupEnd;
    }

    //
    // Make sure the group is powered on.
    //

    Status = HdapCodecCommandBarrier(Codec);
    if (!KSUCCESS(Status)) {
        goto ResetFunctionGroupEnd;
    }

    //
    // Power on all of the widgets.
    //

    Status = HdapEnableWidgets(Codec, Group);
    if (!KSUCCESS(Status)) {
        goto ResetFunctionGroupEnd;
    }

ResetFunctionGroupEnd:
    return Status;
}

BOOL
HdapIsOutputDevice (
    PHDA_WIDGET Widget
    )

/*++

Routine Description:

    This routine determines if the given output pin widget is attached to an
    output device.

Arguments:

    Widget - Supplies a pointer to the pin widget to test.

Return Value:

    Returns TRUE if the pin is attached to an output device or FALSE otherwise.

--*/

{

    ULONG DeviceType;
    BOOL Output;

    ASSERT(HDA_GET_WIDGET_TYPE(Widget) == HDA_AUDIO_WIDGET_TYPE_PIN);
    ASSERT((Widget->TypeCapabilities & HDA_PIN_CAPABILITIES_OUTPUT) != 0);

    Output = FALSE;
    DeviceType = (Widget->PinConfiguration &
                  HDA_CONFIGURATION_DEFAULT_DEVICE_MASK) >>
                 HDA_CONFIGURATION_DEFAULT_DEVICE_SHIFT;

    switch (DeviceType) {
    case HDA_DEVICE_LINE_OUT:
    case HDA_DEVICE_SPEAKER:
    case HDA_DEVICE_HP_OUT:
    case HDA_DEVICE_CD:
    case HDA_DEVICE_SPDIF_OUT:
    case HDA_DEVICE_DIGITAL_OTHER_OUT:
    case HDA_DEVICE_AUX:
    case HDA_DEVICE_OTHER:
        Output = TRUE;
        break;

    default:
        break;
    }

    return Output;
}

BOOL
HdapIsInputDevice (
    PHDA_WIDGET Widget
    )

/*++

Routine Description:

    This routine determines if the given input pin widget is attached to an
    input device.

Arguments:

    Widget - Supplies a pointer to the pin widget to test.

Return Value:

    Returns TRUE if the pin is attached to an input device or FALSE otherwise.

--*/

{

    ULONG DeviceType;
    BOOL Input;

    ASSERT(HDA_GET_WIDGET_TYPE(Widget) == HDA_AUDIO_WIDGET_TYPE_PIN);
    ASSERT((Widget->TypeCapabilities & HDA_PIN_CAPABILITIES_INPUT) != 0);

    Input = FALSE;
    DeviceType = (Widget->PinConfiguration &
                  HDA_CONFIGURATION_DEFAULT_DEVICE_MASK) >>
                 HDA_CONFIGURATION_DEFAULT_DEVICE_SHIFT;

    switch (DeviceType) {
    case HDA_DEVICE_LINE_IN:
    case HDA_DEVICE_AUX:
    case HDA_DEVICE_MIC_IN:
    case HDA_DEVICE_SPDIF_IN:
    case HDA_DEVICE_DIGITAL_OTHER_IN:
    case HDA_DEVICE_OTHER:
        Input = TRUE;
        break;

    default:
        break;
    }

    return Input;
}

KSTATUS
HdapCodecGetParameter (
    PHDA_CODEC Codec,
    USHORT NodeId,
    HDA_PARAMETER ParameterId,
    PULONG Parameter
    )

/*++

Routine Description:

    This routine gets a codec node's parameter value.

Arguments:

    Codec - Supplies a pointer to the codec whose parameter is being queried.

    NodeId - Supplies the ID of the codec's node being queried.

    ParameterId - Supplies the ID of the parameter being requested.

    Parameter - Supplies a pointer that receives the parameter value.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    Status = HdapCodecGetSetVerb(Codec,
                                 NodeId,
                                 HdaVerbGetParameter,
                                 ParameterId,
                                 Parameter);

    return Status;
}

KSTATUS
HdapCodecGetSetVerb (
    PHDA_CODEC Codec,
    USHORT NodeId,
    USHORT Verb,
    USHORT Payload,
    PULONG Response
    )

/*++

Routine Description:

    This routine gets a verb value for the given codec's node.

Arguments:

    Codec - Supplies a pointer to the codec.

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

    KSTATUS Status;

    Status = HdapGetSetVerb(Codec->Controller,
                            Codec->Address,
                            NodeId,
                            Verb,
                            Payload,
                            Response);

    return Status;
}

KSTATUS
HdapCodecCommandBarrier (
    PHDA_CODEC Codec
    )

/*++

Routine Description:

    This routine synchronizes a batch of commands to make sure they have all
    completed before the driver continues operation.

Arguments:

    Codec - Supplies a pointer to a codec.

Return Value:

    Status code.

--*/

{

    return HdapCommandBarrier(Codec->Controller, Codec->Address);
}

USHORT
HdapComputeGainMute (
    ULONG AmpCapabilities,
    ULONG Volume
    )

/*++

Routine Description:

    This routine computes a gain/mute value based on the given amplifier
    capabilities and volume. The volume will be treated as a valume between 0
    (mute) and 100 (full volume).

Arguments:

    AmpCapabilities - Supplies the amplifier capabilities to use to make the
        conversion.

    Volume - Supplies the single channel volume value that is to be converted
        to gain/mute.

Return Value:

    Returns the gain/mute to set in the amplifier gain/mute register.

--*/

{

    ULONG AdjustedVolume;
    USHORT Gain;
    USHORT GainMute;
    UCHAR Offset;
    UCHAR StepCount;

    GainMute = 0;
    Gain = 0;
    if (Volume == 0) {
        GainMute |= HDA_SET_AMPLIFIER_GAIN_PAYLOAD_MUTE;
        if ((AmpCapabilities & HDA_AMP_CAPABILITIES_MUTE) != 0) {
            goto ComputeGainMuteEnd;
        }
    }

    Offset = (AmpCapabilities & HDA_AMP_CAPABILITIES_OFFSET_MASK) >>
             HDA_AMP_CAPABILITIES_OFFSET_SHIFT;

    StepCount = (AmpCapabilities & HDA_AMP_CAPABILITIES_STEP_COUNT_MASK) >>
                HDA_AMP_CAPABILITIES_STEP_COUNT_SHIFT;

    StepCount += 1;

    //
    // If the step count is fixed, the gain is fixed and will not be changed.
    // The return value should be irrelevant.
    //

    if (StepCount == 1) {
        goto ComputeGainMuteEnd;
    }

    ASSERT(Volume <= SOUND_VOLUME_MAXIMUM);

    //
    // Otherwise take the 0-100 scale and map it to the amplifier's step scale.
    //

    AdjustedVolume = (Volume * StepCount) / SOUND_VOLUME_MAXIMUM;

    ASSERT(AdjustedVolume <= StepCount);

    Gain = Offset - (StepCount - AdjustedVolume);

ComputeGainMuteEnd:
    GainMute |= Gain;
    return GainMute;
}

KSTATUS
HdapGetConnectionListIndex (
    PHDA_CODEC Codec,
    PHDA_WIDGET ListWidget,
    PHDA_WIDGET ConnectedWidget,
    PULONG ListIndex
    )

/*++

Routine Description:

    This routine determines the index of the connected widget in the list
    widget's connection list.

Arguments:

    Codec - Supplies a pointer to the codec to which the widgets belong.

    ListWidget - Supplies a pointer to the widget whose connection list is to
        be searched.

    ConnectedWidget - Supplies a pointer to the widget whose index is to be
        retrieved.

    ListIndex - Supplies a pointer that receives in the index of the connected
        widget.

Return Value:

    Status code.

--*/

{

    ULONG Entries;
    ULONG EntriesCount;
    ULONG EntriesIndex;
    ULONG EntriesThisRound;
    ULONG EntryIndex;
    ULONG ListLength;
    PUSHORT LongEntries;
    ULONG MaxEntriesPerRound;
    USHORT NodeId;
    BOOL RangeEntry;
    USHORT RangeStart;
    PUCHAR ShortEntries;
    KSTATUS Status;

    Status = HdapCodecGetParameter(Codec,
                                   ListWidget->NodeId,
                                   HdaParameterConnectionListLength,
                                   &ListLength);

    if (!KSUCCESS(Status)) {
        goto GetConnectionListIndexEnd;
    }

    MaxEntriesPerRound = sizeof(ULONG) / sizeof(UCHAR);
    if ((ListLength & HDA_CONNECTION_LIST_LENGTH_LONG_FORM) != 0) {
        MaxEntriesPerRound = sizeof(ULONG) / sizeof(USHORT);
    }

    RangeStart = 0;
    EntriesCount = (ListLength & HDA_CONNECTION_LIST_LENGTH_MASK) >>
                   HDA_CONNECTION_LIST_LENGTH_SHIFT;

    EntriesIndex = 0;
    *ListIndex = 0;
    LongEntries = (PUSHORT)&Entries;
    ShortEntries = (PUCHAR)&Entries;
    while (EntriesIndex < EntriesCount) {
        Status = HdapCodecGetSetVerb(Codec,
                                     ListWidget->NodeId,
                                     HdaVerbGetConnectionListEntry,
                                     EntriesIndex,
                                     &Entries);

        if (!KSUCCESS(Status)) {
            goto GetConnectionListIndexEnd;
        }

        EntriesThisRound = EntriesCount - EntriesIndex;
        if (EntriesThisRound > MaxEntriesPerRound) {
            EntriesThisRound = MaxEntriesPerRound;
        }

        for (EntryIndex = 0;
             EntryIndex < EntriesThisRound;
             EntryIndex += 1) {

            RangeEntry = FALSE;
            if ((ListLength & HDA_CONNECTION_LIST_LENGTH_LONG_FORM) != 0) {
                NodeId = (LongEntries[EntryIndex] &
                          HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_MASK) >>
                         HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_SHIFT;

                if ((LongEntries[EntryIndex] &
                    HDA_CONNECTION_LIST_LONG_FORM_RANGE) != 0) {

                    RangeEntry = TRUE;
                }

            } else {
                NodeId = (ShortEntries[EntryIndex] &
                          HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_MASK) >>
                         HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_SHIFT;

                if ((ShortEntries[EntryIndex] &
                     HDA_CONNECTION_LIST_SHORT_FORM_RANGE) != 0) {

                    RangeEntry = TRUE;
                }
            }

            //
            // If this is a range entry, then each widget in the range
            // increments the list index. The current node ID is always
            // checked below, so go from [RangeStart, NodeId).
            //

            if (RangeEntry != FALSE) {

                ASSERT(RangeStart != 0);

                while (RangeStart != NodeId) {
                    if (RangeStart == ConnectedWidget->NodeId) {
                        Status = STATUS_SUCCESS;
                        goto GetConnectionListIndexEnd;
                    }

                    *ListIndex += 1;
                    RangeStart += 1;
                }
            }

            if (NodeId == ConnectedWidget->NodeId) {
                Status = STATUS_SUCCESS;
                goto GetConnectionListIndexEnd;
            }

            *ListIndex += 1;
            RangeStart = NodeId + 1;
        }

        EntriesIndex += EntriesThisRound;
    }

    Status = STATUS_NOT_FOUND;

GetConnectionListIndexEnd:
    return Status;
}

