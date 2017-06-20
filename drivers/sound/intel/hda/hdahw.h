/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    hdahw.h

Abstract:

    This header contains hardware definitions for Intel High Definition Audio
    controllers.

Author:

    Chris Stevens 3-Apr-2017

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// These macros read and write to general HD Audio controller registers.
//

#define HDA_READ32(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define HDA_WRITE32(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

#define HDA_READ16(_Controller, _Register) \
    HlReadRegister16((_Controller)->ControllerBase + (_Register))

#define HDA_WRITE16(_Controller, _Register, _Value) \
    HlWriteRegister16((_Controller)->ControllerBase + (_Register), (_Value))

#define HDA_READ8(_Controller, _Register) \
    HlReadRegister8((_Controller)->ControllerBase + (_Register))

#define HDA_WRITE8(_Controller, _Register, _Value) \
    HlWriteRegister8((_Controller)->ControllerBase + (_Register), (_Value))

//
// These macros read and write to stream descriptor registers.
//

#define HDA_STREAM_REGISTER(_Index, _Register) \
    (HdaRegisterStreamDescriptorBase +         \
     (HDA_STREAM_DESCRIPTOR_SIZE * (_Index)) + \
     (_Register))

#define HDA_STREAM_READ32(_Controller, _Index, _Register) \
    HDA_READ32(_Controller, HDA_STREAM_REGISTER(_Index, _Register))

#define HDA_STREAM_WRITE32(_Controller, _Index, _Register, _Value) \
    HDA_WRITE32(_Controller, HDA_STREAM_REGISTER(_Index, _Register), (_Value))

#define HDA_STREAM_READ16(_Controller, _Index, _Register) \
    HDA_READ16(_Controller, HDA_STREAM_REGISTER(_Index, _Register))

#define HDA_STREAM_WRITE16(_Controller, _Index, _Register, _Value) \
    HDA_WRITE16(_Controller, HDA_STREAM_REGISTER(_Index, _Register), (_Value))

#define HDA_STREAM_READ8(_Controller, _Index, _Register) \
    HDA_READ8(_Controller, HDA_STREAM_REGISTER(_Index, _Register))

#define HDA_STREAM_WRITE8(_Controller, _Index, _Register, _Value) \
    HDA_WRITE8(_Controller, HDA_STREAM_REGISTER(_Index, _Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the root node ID, guaranteed to be present for all codecs.
//

#define HDA_ROOT_NODE_ID 0

//
// Define the delay necessary after issuing a controller reset.
//

#define HDA_CONTROLLER_RESET_DELAY 100

//
// Define the delay necessary to allow codecs to self-enumerate, in
// microseconds.
//

#define HDA_CODEC_ENUMERATION_DELAY 521

//
// Define the maximum number of codecs that may be attached to an HD Audio
// device.
//

#define HDA_MAX_CODEC_COUNT 15

//
// Define the maximum verb value that can take a 16-bit payload. All others
// take and 8-bit payload.
//

#define HDA_MAX_16_BIT_PAYLOAD_VERB 0xF

//
// Define the size of a stream descriptor, in bytes.
//

#define HDA_STREAM_DESCRIPTOR_SIZE 0x20

//
// Define the alignment of the DMA buffers.
//

#define HDA_DMA_BUFFER_ALIGNMENT 128

//
// Define the bits for the global capabilities register.
//

#define HDA_GLOBAL_CAPABILITIES_OUTPUT_STREAMS_SUPPORTED_MASK         0xF000
#define HDA_GLOBAL_CAPABILITIES_OUTPUT_STREAMS_SUPPORTED_SHIFT        12
#define HDA_GLOBAL_CAPABILITIES_INPUT_STREAMS_SUPPORTED_MASK          0x0F00
#define HDA_GLOBAL_CAPABILITIES_INPUT_STREAMS_SUPPORTED_SHIFT         8
#define HDA_GLOBAL_CAPABILITIES_BIDIRECTIONAL_STREAMS_SUPPORTED_MASK  0x00F8
#define HDA_GLOBAL_CAPABILITIES_BIDIRECTIONAL_STREAMS_SUPPORTED_SHIFT 3
#define HDA_GLOBAL_CAPABILITIES_SERIAL_DATA_OUT_SIGNALS_MASK          0x0006
#define HDA_GLOBAL_CAPABILITIES_SERIAL_DATA_OUT_SIGNALS_SHIFT         1
#define HDA_GLOBAL_CAPABILITIES_64_BIT_ADDRESSES_SUPPORTED            0x0001

//
// Define the bits for the global control register.
//

#define HDA_GLOBAL_CONTROL_ACCEPT_UNSOLICITED_RESPONSE_ENABLE 0x00000100
#define HDA_GLOBAL_CONTROL_FLUSH_CONTROL                      0x00000002
#define HDA_GLOBAL_CONTROL_CONTROLLER_RESET                   0x00000001

//
// Define the bits for the global status register.
//

#define HDA_GLOBAL_STATUS_FLUSH_STATUS 0x0002

//
// Define the bits for the interrupt control register.
//

#define HDA_INTERRUPT_CONTROL_GLOBAL_ENABLE       0x80000000
#define HDA_INTERRUPT_CONTROL_CONTROLLER_ENABLE   0x40000000
#define HDA_INTERRUPT_CONTROL_STREAM_ENABLE_MASK  0x3FFFFFFF
#define HDA_INTERRUPT_CONTROL_STREAM_ENABLE_SHIFT 0

//
// Define the bits for the interrupt status register.
//

#define HDA_INTERRUPT_STATUS_GLOBAL       0x80000000
#define HDA_INTERRUPT_STATUS_CONTROLLER   0x40000000
#define HDA_INTERRUPT_STATUS_STREAM_MASK  0x3FFFFFFF
#define HDA_INTERRUPT_STATUS_STREAM_SHIFT 0

//
// Define the alignment of the command output ring buffer (CORB), in bytes.
//

#define HDA_CORB_ALIGNMENT 128

//
// Define the bits for the CORB write pointer register.
//

#define HDA_CORB_WRITE_POINTER_MASK  0x00FF
#define HDA_CORB_WRITE_POINTER_SHIFT 0

#define HDA_CORB_WRITE_POINTER_MAX 0xFF

//
// Define the bits for the CORB read pointer register.
//

#define HDA_CORB_READ_POINTER_RESET 0x8000
#define HDA_CORB_READ_POINTER_MASK  0x00FF
#define HDA_CORB_READ_POINTER_SHIFT 0

#define HDA_CORB_READ_POINTER_MAX 0xFF

//
// Define the bits for the CORB control register.
//

#define HDA_CORB_CONTROL_DMA_ENABLE                    0x02
#define HDA_CORB_CONTROL_MEMORY_ERROR_INTERRUPT_ENABLE 0x01

//
// Define the bits for the CORB status register.
//

#define HDA_CORB_STATUS_MEMORY_ERROR_INDICATION 0x01

//
// Define the bits of the CORB size register.
//

#define HDA_CORB_SIZE_CAPABILITY_256 0x40
#define HDA_CORB_SIZE_CAPABILITY_16  0x20
#define HDA_CORB_SIZE_CAPABILITY_2   0x10
#define HDA_CORB_SIZE_MASK           0x03
#define HDA_CORB_SIZE_SHIFT          0
#define HDA_CORB_SIZE_256            0x2
#define HDA_CORB_SIZE_16             0x1
#define HDA_CORB_SIZE_2              0x0

//
// Define the alignment of the response input ring buffer (RIRB), in bytes.
//

#define HDA_RIRB_ALIGNMENT 128

//
// Define the bits for the RIRB write pointer register.
//

#define HDA_RIRB_WRITE_POINTER_RESET 0x8000
#define HDA_RIRB_WRITE_POINTER_MASK  0x00FF
#define HDA_RIRB_WRITE_POINTER_SHIFT 0

#define HDA_RIRB_WRITE_POINTER_MAX 0xFF

//
// Define the response interrupt count register bits.
//

#define HDA_RESPONSE_INTERRUPT_COUNT_MASK  0x00FF
#define HDA_RESPONSE_INTERRUPT_COUNT_SHIFT 0

//
// Define the RIRB control register bits.
//

#define HDA_RIRB_CONTROL_OVERRUN_INTERRUPT_ENABLE 0x04
#define HDA_RIRB_CONTROL_DMA_ENABLE               0x02
#define HDA_RIRB_CONTROL_INTERRUPT_ENABLE         0x01

//
// Define the RIRB status register bits.
//

#define HDA_RIRB_STATUS_OVERRUN_INTERRUPT 0x04
#define HDA_RIRB_STATUS_INTERRUPT         0x01

//
// Define the bits of the RIRB size register.
//

#define HDA_RIRB_SIZE_CAPABILITY_256 0x40
#define HDA_RIRB_SIZE_CAPABILITY_16  0x20
#define HDA_RIRB_SIZE_CAPABILITY_2   0x10
#define HDA_RIRB_SIZE_MASK           0x03
#define HDA_RIRB_SIZE_SHIFT          0
#define HDA_RIRB_SIZE_256            0x2
#define HDA_RIRB_SIZE_16             0x1
#define HDA_RIRB_SIZE_2              0x0

//
// Define the DMA position buffer address alignment.
//

#define HDA_DMA_POSITION_ALIGNMENT 128

//
// Define the bits of the DMA position lower base address register.
//

#define HDA_DMA_POSITION_BUFFER_LOWER_BASE_ENABLE 0x00000001

//
// Define the bits for the stream descriptor control registers.
//

#define HDA_STREAM_CONTROL_STREAM_NUMBER_MASK                0xF00000
#define HDA_STREAM_CONTROL_STREAM_NUMBER_SHIFT               20
#define HDA_STREAM_CONTROL_BIDIRECTIONAL_OUTPUT              0x080000
#define HDA_STREAM_CONTROL_TRAFFIC_PRIORITY                  0x040000
#define HDA_STREAM_CONTROL_STRIPE_CONTROL_MASK               0x030000
#define HDA_STREAM_CONTROL_STRIPE_CONTROL_SHIFT              16
#define HDA_STREAM_CONTROL_STRIPE_CONTROL_1                  0x0
#define HDA_STREAM_CONTROL_STRIPE_CONTROL_2                  0x1
#define HDA_STREAM_CONTROL_STRIPE_CONTROL_4                  0x2
#define HDA_STREAM_CONTROL_DESCRIPTOR_ERROR_INTERRUPT_ENABLE 0x000010
#define HDA_STREAM_CONTROL_FIFO_ERROR_INTERRUPT_ENABLE       0x000008
#define HDA_STREAM_CONTROL_COMPLETION_INTERRUPT_ENABLE       0x000004
#define HDA_STREAM_CONTROL_DMA_ENABLE                        0x000002
#define HDA_STREAM_CONTROL_RESET                             0x000001

//
// Define the bits for the stream descriptor status registers.
//

#define HDA_STREAM_STATUS_FIFO_READY       0x20
#define HDA_STREAM_STATUS_DESCRIPTOR_ERROR 0x10
#define HDA_STREAM_STATUS_FIFO_ERROR       0x08
#define HDA_STREAM_STATUS_BUFFER_COMPLETE  0x04

//
// Define the bits for the stream descriptor last valid index registers.
//

#define HDA_STREAM_LAST_VALID_INDEX_MASK  0x00FF
#define HDA_STREAM_LAST_VALID_INDEX_SHIFT 0

//
// Define the bits for the stream format. This is programmed in both the
// stream descriptor and the converter stream channel verb.
//

#define HDA_FORMAT_NON_PCM                         0x8000
#define HDA_FORMAT_SAMPLE_BASE_RATE_MASK           0x7F00
#define HDA_FORMAT_SAMPLE_BASE_RATE_SHIFT          8
#define HDA_FORMAT_SAMPLE_BASE_RATE_8000           0x05
#define HDA_FORMAT_SAMPLE_BASE_RATE_11025          0x03
#define HDA_FORMAT_SAMPLE_BASE_RATE_16000          0x02
#define HDA_FORMAT_SAMPLE_BASE_RATE_22050          0x41
#define HDA_FORMAT_SAMPLE_BASE_RATE_24000          0x01
#define HDA_FORMAT_SAMPLE_BASE_RATE_32000          0x0A
#define HDA_FORMAT_SAMPLE_BASE_RATE_44100          0x40
#define HDA_FORMAT_SAMPLE_BASE_RATE_48000          0x00
#define HDA_FORMAT_SAMPLE_BASE_RATE_88200          0x48
#define HDA_FORMAT_SAMPLE_BASE_RATE_96000          0x08
#define HDA_FORMAT_SAMPLE_BASE_RATE_176400         0x58
#define HDA_FORMAT_SAMPLE_BASE_RATE_192000         0x18
#define HDA_FORMAT_SAMPLE_BASE_RATE_384000         0x38
#define HDA_FORMAT_SAMPLE_BASE_RATE_44KHZ          0x4000
#define HDA_FORMAT_SAMPLE_BASE_RATE_48KHZ          0x0000
#define HDA_FORMAT_SAMPLE_BASE_RATE_MULTIPLE_MASK  0x3800
#define HDA_FORMAT_SAMPLE_BASE_RATE_MULTIPLE_SHIFT 11
#define HDA_FORMAT_SAMPLE_BASE_RATE_MULTIPLE_X2    0x1
#define HDA_FORMAT_SAMPLE_BASE_RATE_MULTIPLE_X3    0x2
#define HDA_FORMAT_SAMPLE_BASE_RATE_MULTIPLE_X4    0x3
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_MASK   0x0700
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_SHIFT  8
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_1      0x0
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_2      0x1
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_3      0x2
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_4      0x3
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_5      0x4
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_6      0x5
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_7      0x6
#define HDA_FORMAT_SAMPLE_BASE_RATE_DIVISOR_8      0x7
#define HDA_FORMAT_BITS_PER_SAMPLE_MASK            0x0070
#define HDA_FORMAT_BITS_PER_SAMPLE_SHIFT           4
#define HDA_FORMAT_BITS_PER_SAMPLE_8               0x0
#define HDA_FORMAT_BITS_PER_SAMPLE_16              0x1
#define HDA_FORMAT_BITS_PER_SAMPLE_20              0x2
#define HDA_FORMAT_BITS_PER_SAMPLE_24              0x3
#define HDA_FORMAT_BITS_PER_SAMPLE_32              0x4
#define HDA_FORMAT_NUMBER_OF_CHANNELS_MASK         0x000F
#define HDA_FORMAT_NUMBER_OF_CHANNELS_SHIFT        0

//
// Define the buffer descriptor list alignment.
//

#define HDA_BUFFER_DESCRIPTOR_LIST_ALIGNMENT 128

//
// Define the bits for the immediate command status register.
//

#define HDA_IMMEDIATE_COMMAND_STATUS_RESPONSE_RESULT_ADDRESS_MASK  0x00F0
#define HDA_IMMEDIATE_COMMAND_STATUS_RESPONSE_RESULT_ADDRESS_SHIFT 4
#define HDA_IMMEDIATE_COMMAND_STATUS_RESPONSE_RESULT_UNSOLICITED   0x0008
#define HDA_IMMEDIATE_COMMAND_STATUS_VERSION_EXTENDED              0x0004
#define HDA_IMMEDIATE_COMMAND_STATUS_RESULT_VALID                  0x0002
#define HDA_IMMEDIATE_COMMAND_STATUS_BUSY                          0x0001

//
// Define the bits for the buffer descriptor list entry flags.
//

#define HDA_BUFFER_DESCRIPTOR_FLAG_INTERRUPT_ON_COMPLETION 0x00000001

//
// Define the broadcast codec address.
//

#define HDA_CODEC_BROADCAST_ADDRESS 0xF

//
// Define the command verb bits.
//

#define HDA_COMMAND_VERB_CODEC_ADDRESS_MASK  0xF0000000
#define HDA_COMMAND_VERB_CODEC_ADDRESS_SHIFT 28
#define HDA_COMMAND_VERB_INDIRECT_NODE_ID    0x08000000
#define HDA_COMMAND_VERB_NODE_ID_MASK        0x0FF00000
#define HDA_COMMAND_VERB_NODE_ID_SHIFT       20
#define HDA_COMMAND_VERB_PAYLOAD_MASK        0x000FFFFF
#define HDA_COMMAND_VERB_PAYLOAD_SHIFT       0

//
// Define the response extended flags.
//

#define HDA_RESPONSE_EXTENDED_FLAG_UNSOLICITED         0x00000010
#define HDA_RESPONSE_EXTENDED_FLAG_CODEC_ADDRESS_MASK  0x0000000F
#define HDA_RESPONSE_EXTENDED_FLAG_CODEC_ADDRESS_SHIFT 0

//
// Define the bits for the get amplifier gain/mute payload.
//

#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_OUTPUT      0x8000
#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_INPUT       0x0000
#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_LEFT        0x2000
#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_RIGHT       0x0000
#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_INDEX_MASK  0x000F
#define HDA_GET_AMPLIFIER_GAIN_PAYLOAD_INDEX_SHIFT 0

//
// Define the bits for the get amplifier gain/mute response.
//

#define HDA_GET_AMPLIFIER_GAIN_RESPONSE_MUTE       0x00000080
#define HDA_GET_AMPLIFIER_GAIN_RESPONSE_GAIN_MASK  0x0000007F
#define HDA_GET_AMPLIFIER_GAIN_RESPONSE_GAIN_SHIFT 0

//
// Define the bits for the set amplifier/gain payload.
//

#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_OUTPUT      0x8000
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INPUT       0x4000
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_LEFT        0x2000
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_RIGHT       0x1000
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INDEX_MASK  0x0F00
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_INDEX_SHIFT 8
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_MUTE        0x0080
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_GAIN_MASK   0x007F
#define HDA_SET_AMPLIFIER_GAIN_PAYLOAD_GAIN_SHIFT  0

//
// Define the bits for the S/PDIF IEC Control (SIC) payload and response. This
// is for the digital converter control verbs.
//

#define HDA_SIC_KEEP_ALIVE_ENABLE     0x00800000
#define HDA_SIC_IEC_CODING_TYPE_MASK  0x000F0000
#define HDA_SIC_IEC_CODING_TYPE_SHIFT 16
#define HDA_SIC_CATEGORY_CODE_MASK    0x00007F00
#define HDA_SIC_CATEGORY_CODE_SHIFT   8
#define HDA_SIC_GENERATION_LEVEL      0x00000080
#define HDA_SIC_PROFESSIONAL          0x00000040
#define HDA_SIC_NON_PCM               0x00000020
#define HDA_SIC_NO_COPYRIGHT          0x00000010
#define HDA_SIC_PREEMPHASIS           0x00000008
#define HDA_SIC_VALIDITY_CONFIG       0x00000004
#define HDA_SIC_VALIDITY              0x00000002
#define HDA_SIC_DIGITAL_ENABLE        0x00000001

//
// Define the bits for the get power state response.
//

#define HDA_POWER_STATE_RESPONSE_SETTINGS_RESET 0x00000400
#define HDA_POWER_STATE_RESPONSE_CLOCK_STOP_OK  0x00000200
#define HDA_POWER_STATE_RESPONSE_ERROR          0x00000100
#define HDA_POWER_STATE_RESPONSE_ACTUAL_MASK    0x000000F0
#define HDA_POWER_STATE_RESPONSE_ACTUAL_SHIFT   4
#define HDA_POWER_STATE_RESPONSE_SETTING_MASK   0x0000000F
#define HDA_POWER_STATE_RESPONSE_SETTING_SHIFT  0

//
// Define the bits for the set power state payload.
//

#define HDA_POWER_STATE_PAYLOAD_SETTING_MASK  0x0F
#define HDA_POWER_STATE_PAYLOAD_SETTING_SHIFT 0
#define HDA_POWER_STATE_D0                    0x00
#define HDA_POWER_STATE_D1                    0x01
#define HDA_POWER_STATE_D2                    0x02
#define HDA_POWER_STATE_D3                    0x03
#define HDA_POWER_STATE_D3_COLD               0x04

//
// Define the bits for the converter control bits.
//

#define HDA_CONVERTER_CONTROL_STREAM_MASK   0x000000F0
#define HDA_CONVERTER_CONTROL_STREAM_SHIFT  4
#define HDA_CONVERTER_CONTROL_CHANNEL_MASK  0x0000000F
#define HDA_CONVERTER_CONTROL_CHANNEL_SHIFT 0

//
// Define the pin widget control payload and response format.
//

#define HDA_PIN_WIDGET_CONTROL_HEAD_PHONE_ENABLE                    0x80
#define HDA_PIN_WIDGET_CONTROL_OUT_ENABLE                           0x40
#define HDA_PIN_WIDGET_CONTROL_IN_ENABLE                            0x20
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_MASK        0x07
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_SHIFT       0
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_HI_Z        0x0
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_50_PERCENT  0x1
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_GROUND      0x2
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_80_PERCENT  0x4
#define HDA_PIN_WIDGET_CONTROL_VOLTAGE_REFERENCE_ENABLE_100_PERCENT 0x5
#define HDA_PIN_WIDGET_ENCODED_PACKET_TYPE_MASK                     0x03
#define HDA_PIN_WIDGET_ENCODED_PACKET_TYPE_SHIFT                    0
#define HDA_PIN_WIDGET_ENCODED_PACKET_TYPE_NATIVE                   0x0
#define HDA_PIN_WIDGET_ENCODED_PACKET_TYPE_HIGH_BIT_RATE            0x3

//
// Define the bits for the unsolicited response control payload and response.
//

#define HDA_UNSOLICITED_RESPONSE_CONTROL_ENABLE    0x80
#define HDA_UNSOLICITED_RESPONSE_CONTROL_TAG_MASK  0x3F
#define HDA_UNSOLICITED_RESPONSE_CONTROL_TAG_SHIFT 0

//
// Define the bits for an unsolicited response.
//

#define HDA_UNSOLICITED_RESPONSE_TAG_MASK        0xFC000000
#define HDA_UNSOLICITED_RESPONSE_TAG_SHIFT       26
#define HDA_UNSOLICITED_RESPONSE_SUB_TAG_MASK    0x03E00000
#define HDA_UNSOLICITED_RESPONSE_SUB_TAG_SHIFT   21
#define HDA_UNSOLICITED_RESPONSE_ELD_VALID       0x00000002
#define HDA_UNSOLICITED_RESPONSE_PRESENCE_DETECT 0x00000001

//
// Define the bits for the get pin sense response.
//

#define HDA_PIN_SENSE_RESPONSE_PRESENCE_DETECT        0x80000000
#define HDA_PIN_SENSE_RESPONSE_ANALOG_IMPEDANCE_MASK  0x7FFFFFFF
#define HDA_PIN_SENSE_REPSONSE_ANALOG_IMEPDANCE_SHIFT 0
#define HDA_PIN_SENSE_RESPONSE_DIGITAL_ELD_VALID      0x40000000

//
// Define the bits for the pin sense execute payload.
//

#define HDA_PIN_SENSE_EXECUTE_ANALOG_RIGHT_CHANNEL 0x01

//
// Define the bits for the EAPD/BTL enable payload and response.
//

#define HDA_EAPD_BTL_ENABLE_LEFT_RIGHT_SWAP 0x04
#define HDA_EAPD_BTL_ENABLE_EAPD            0x02
#define HDA_EAPD_BTL_ENABLE_BTL             0x01

//
// Define the bits for the volume knob payload and response.
//

#define HDA_VOLUME_KNOB_DIRECT       0x80
#define HDA_VOLUME_KNOB_VOLUME_MASK  0x7F
#define HDA_VOLUME_KNOB_VOLUME_SHIFT 0

//
// Define the bits and mask     values for the configuration default register.
//

#define HDA_CONFIGURATION_DEFAULT_PORT_CONNECTIVITY_MASK  0xC0000000
#define HDA_CONFIGURATION_DEFAULT_PORT_CONNECTIVITY_SHIFT 30
#define HDA_CONFIGURATION_DEFAULT_LOCATION_MASK           0x3F000000
#define HDA_CONFIGURATION_DEFAULT_LOCATION_SHIFT          24
#define HDA_CONFIGURATION_DEFAULT_DEVICE_MASK             0x00F00000
#define HDA_CONFIGURATION_DEFAULT_DEVICE_SHIFT            20
#define HDA_CONFIGURATION_DEFAULT_CONNECTION_TYPE_MASK    0x000F0000
#define HDA_CONFIGURATION_DEFAULT_CONNECTION_TYPE_SHIFT   16
#define HDA_CONFIGURATION_DEFAULT_COLOR_MASK              0x0000F000
#define HDA_CONFIGURATION_DEFAULT_COLOR_SHIFT             12
#define HDA_CONFIGURATION_DEFAULT_MISC_MASK               0x00000F00
#define HDA_CONFIGURATION_DEFAULT_MISC_SHIFT              8
#define HDA_CONFIGURATION_DEFAULT_ASSOCIATION_MASK        0x000000F0
#define HDA_CONFIGURATION_DEFAULT_ASSOCIATION_SHIFT       4
#define HDA_CONFIGURATION_DEFAULT_SEQUENCE_MASK           0x0000000F
#define HDA_CONFIGURATION_DEFAULT_SEQUENCE_SHIFT          0

#define HDA_PORT_CONNECTIVITY_JACK           0x0
#define HDA_PORT_CONNECTIVITY_NONE           0x1
#define HDA_PORT_CONNECTIVITY_FIXED_FUNCTION 0x2
#define HDA_PORT_CONNECTIVITY_BOTH           0x3

#define HDA_LOCATION_GROSS_EXTERNAL     0x00
#define HDA_LOCATION_GROSS_INTERNAL     0x10
#define HDA_LOCATION_GROSS_SEPARATE     0x20
#define HDA_LOCATION_GROSS_OTHER        0x30
#define HDA_LOCATION_GEOMETRIC_NONE     0x00
#define HDA_LOCATION_GEOMETRIC_REAR     0x01
#define HDA_LOCATION_GEOMETRIC_FRONT    0x02
#define HDA_LOCATION_GEOMETRIC_LEFT     0x03
#define HDA_LOCATION_GEOMETRIC_TOP      0x05
#define HDA_LOCATION_GEOMETRIC_BOTTOM   0x06
#define HDA_LOCATION_REAR_PANEL         0x07
#define HDA_LOCATION_RISER              0x17
#define HDA_LOCATION_MOBILE_LID_INSIDE  0x37
#define HDA_LOCATION_DRIVE_BAY          0x08
#define HDA_LOCATION_DIGITAL_DISPLAY    0x18
#define HDA_LOCATION_MOBILE_LID_OUTSIDE 0x38
#define HDA_LOCATION_ATAPI              0x19

#define HDA_DEVICE_LINE_OUT           0x0
#define HDA_DEVICE_SPEAKER            0x1
#define HDA_DEVICE_HP_OUT             0x2
#define HDA_DEVICE_CD                 0x3
#define HDA_DEVICE_SPDIF_OUT          0x4
#define HDA_DEVICE_DIGITAL_OTHER_OUT  0x5
#define HDA_DEVICE_MODEM_LINE_SIDE    0x6
#define HDA_DEVICE_MODEM_HANDSET_SIDE 0x7
#define HDA_DEVICE_LINE_IN            0x8
#define HDA_DEVICE_AUX                0x9
#define HDA_DEVICE_MIC_IN             0xA
#define HDA_DEVICE_TELEPHONY          0xB
#define HDA_DEVICE_SPDIF_IN           0xC
#define HDA_DEVICE_DIGITAL_OTHER_IN   0xD
#define HDA_DEVICE_OTHER              0xF

#define HDA_CONNECTION_TYPE_UNKNOWN             0x0
#define HDA_CONNECTION_TYPE_1_8_STEREO_MONO     0x1
#define HDA_CONNECTION_TYPE_1_4_STEREO_MONO     0x2
#define HDA_CONNECTION_TYPE_ATAPI_INTERNAL      0x3
#define HDA_CONNECTION_TYPE_RCA                 0x4
#define HDA_CONNECTION_TYPE_OPTICAL             0x5
#define HDA_CONNECTION_TYPE_OTHER_DIGITAL       0x6
#define HDA_CONNECTION_TYPE_OTHER_ANALOG        0x7
#define HDA_CONNECTION_TYPE_MULTICHANNEL_ANALOG 0x8
#define HDA_CONNECTION_TYPE_XLR_PROFESSION      0x9
#define HDA_CONNECTION_TYPE_RJ11_MODEM          0xA
#define HDA_CONNECTION_TYPE_COMBINATION         0xB
#define HDA_CONNECTION_TYPE_OTHER               0xF

#define HDA_COLOR_UNKNOWN 0x0
#define HDA_COLOR_BLACK   0x1
#define HDA_COLOR_GREY    0x2
#define HDA_COLOR_BLUE    0x3
#define HDA_COLOR_GREEN   0x4
#define HDA_COLOR_RED     0x5
#define HDA_COLOR_ORANGE  0x6
#define HDA_COLOR_YELLOW  0x7
#define HDA_COLOR_PURPLE  0x8
#define HDA_COLOR_PINK    0x9
#define HDA_COLOR_WHITE   0xE
#define HDA_COLOR_OTHER   0xF

#define HDA_MISC_JACK_DETECT_OVERRIDE 0x1

//
// Define the bits for the stripe control register.
//

#define HDA_STRIPE_CONTROL_CAPABILITY_MASK  0x00700000
#define HDA_STRIPE_CONTROL_CAPABILITY_SHIFT 20
#define HDA_STRIPE_CONTROL_MASK             0x00000003
#define HDA_STRIPE_CONTROL_SHIFT            0

//
// Define the bits for the EDID-Like Data (ELD) data response.
//

#define HDA_ELD_VALID      0x80000000
#define HDA_ELD_BYTE_MASK  0x000000FF
#define HDA_ELD_BYTE_SHIFT 0

//
// Define the bits for the Data Island Packet (DIP) size payload.
//

#define HDA_DIP_SIZE_PAYLOAD_ELD_SIZE           0x08
#define HDA_DIP_SIZE_PAYLOAD_PACKET_INDEX_MASK  0x07
#define HDA_DIP_SIZE_PAYLOAD_PACKET_INDEX_SHIFT 0

//
// Define the bits for the Data Island Packet (DIP) size response.
//

#define HDA_DIP_SIZE_RESPONSE_SIZE_MASK  0x000000FF
#define HDA_DIP_SIZE_RESPONSE_SIZE_SHIFT 0

//
// Define the bits for the vendor and device ID parameter.
//

#define HDA_VENDOR_ID_VENDOR_MASK  0xFFFF0000
#define HDA_VENDOR_ID_VENDOR_SHIFT 16
#define HDA_VENDOR_ID_DEVICE_MASK  0x0000FFFF
#define HDA_VENDOR_ID_DEVICE_SHIFT 0

//
// Define the bits for the subordinate node parameter.
//

#define HDA_SUBORDINATE_NODE_START_MASK  0x00FF0000
#define HDA_SUBORDINATE_NODE_START_SHIFT 16
#define HDA_SUBORDINATE_NODE_COUNT_MASK  0x000000FF
#define HDA_SUBORDINATE_NODE_COUNT_SHIFT 0

//
// Define the bits for function group type parameter.
//

#define HDA_FUNCTION_GROUP_UNSOLICITED_RESPONSE_CAPABLE 0x00000100
#define HDA_FUNCTION_GROUP_TYPE_MASK                    0x000000FF
#define HDA_FUNCTION_GROUP_TYPE_SHIFT                   0
#define HDA_FUNCTION_GROUP_TYPE_AUDIO                   0x01
#define HDA_FUNCTION_GROUP_TYPE_MODEM                   0x02

//
// Define the bits for the audio widget capabilities parameter.
//

#define HDA_AUDIO_WIDGET_TYPE_MASK               0x00F00000
#define HDA_AUDIO_WIDGET_TYPE_SHIFT              20
#define HDA_AUDIO_WIDGET_TYPE_OUTPUT             0x0
#define HDA_AUDIO_WIDGET_TYPE_INPUT              0x1
#define HDA_AUDIO_WIDGET_TYPE_MIXER              0x2
#define HDA_AUDIO_WIDGET_TYPE_SELECTOR           0x3
#define HDA_AUDIO_WIDGET_TYPE_PIN                0x4
#define HDA_AUDIO_WIDGET_TYPE_POWER              0x5
#define HDA_AUDIO_WIDGET_TYPE_VOLUME_KNOB        0x6
#define HDA_AUDIO_WIDGET_TYPE_BEEP_GENERATOR     0x7
#define HDA_AUDIO_WIDGET_TYPE_VENDOR_DEFINED     0xF
#define HDA_AUDIO_WIDGET_DELAY_MASK              0x000F0000
#define HDA_AUDIO_WIDGET_DELAY_SHIFT             16
#define HDA_AUDIO_WIDGET_CHANNEL_COUNT_EXT_MASK  0x0000E000
#define HDA_AUDIO_WIDGET_CHANNEL_COUNT_EXT_SHIFT 13
#define HDA_AUDIO_WIDGET_CONTENT_PROTECTION      0x00001000
#define HDA_AUDIO_WIDGET_LEFT_RIGHT_SWAP         0x00000800
#define HDA_AUDIO_WIDGET_POWER_CONTROL           0x00000400
#define HDA_AUDIO_WIDGET_DIGITAL                 0x00000200
#define HDA_AUDIO_WIDGET_CONNECTION_LIST         0x00000100
#define HDA_AUDIO_WIDGET_UNSOLICITED_CAPABLE     0x00000080
#define HDA_AUDIO_WIDGET_PROCESSING_CONTROLS     0x00000040
#define HDA_AUDIO_WIDGET_STRIPE                  0x00000020
#define HDA_AUDIO_WIDGET_FORMAT_OVERRIDE         0x00000010
#define HDA_AUDIO_WIDGET_AMP_OVERRIDE            0x00000008
#define HDA_AUDIO_WIDGET_OUT_AMP_PRESENT         0x00000004
#define HDA_AUDIO_WIDGET_IN_AMP_PRESENT          0x00000002
#define HDA_AUDIO_WIDGET_CHANNEL_COUNT_LSB       0x00000001

//
// Define the bits for the pin capabilities parameter.
//

#define HDA_PIN_CAPABILITIES_HIGH_BIT_RATE      0x08000000
#define HDA_PIN_CAPABILITIES_DISPLAY_PORT       0x01000000
#define HDA_PIN_CAPABILITIES_EAPD               0x00010000
#define HDA_PIN_CAPABILITIES_VREF_CONTROL_MASK  0x0000FF00
#define HDA_PIN_CAPABILITIES_VREF_CONTROL_SHIFT 8
#define HDA_PIN_CAPABILITIES_HDMI               0x00000080
#define HDA_PIN_CAPABILITIES_BALANCED_IO_PINS   0x00000040
#define HDA_PIN_CAPABILITIES_INPUT              0x00000020
#define HDA_PIN_CAPABILITIES_OUTPUT             0x00000010
#define HDA_PIN_CAPABILITIES_HEADPHONE_CAPABLE  0x00000008
#define HDA_PIN_CAPABILITIES_PRESENCE_DETECT    0x00000004
#define HDA_PIN_CAPABILITIES_TRIGGER_REQUIRED   0x00000002
#define HDA_PIN_CAPABILITIES_IMPEDENCE_SENSE    0x00000001

//
// Define the bits for the supported PCM sizes and rates.
//

#define HDA_PCM_SIZE_RATES_SIZE_MASK  0xFFFF0000
#define HDA_PCM_SIZE_RATES_SIZE_SHIFT 16
#define HDA_PCM_SIZE_RATES_RATE_MASK  0x0000FFFF
#define HDA_PCM_SIZE_RATES_RATE_SHIFT 0

#define HDA_PCM_SIZE_32_BIT 0x0010
#define HDA_PCM_SIZE_24_BIT 0x0008
#define HDA_PCM_SIZE_20_BIT 0x0004
#define HDA_PCM_SIZE_16_BIT 0x0002
#define HDA_PCM_SIZE_8_BIT  0x0001

#define HDA_SAMPLE_RATE_384_KHZ 0x0800
#define HDA_SAMPLE_RATE_192_KHZ 0x0400
#define HDA_SAMPLE_RATE_176_KHZ 0x0200
#define HDA_SAMPLE_RATE_96_KHZ  0x0100
#define HDA_SAMPLE_RATE_88_KHZ  0x0080
#define HDA_SAMPLE_RATE_48_KHZ  0x0040
#define HDA_SAMPLE_RATE_44_KHZ  0x0020
#define HDA_SAMPLE_RATE_32_KHZ  0x0010
#define HDA_SAMPLE_RATE_22_KHZ  0x0008
#define HDA_SAMPLE_RATE_16_KHZ  0x0004
#define HDA_SAMPLE_RATE_11_KHZ  0x0002
#define HDA_SAMPLE_RATE_8_KHZ   0x0001

//
// Define the bits for the supported stream format parameter.
//

#define HDA_STREAM_FORMAT_AC3     0x00000004
#define HDA_STREAM_FORMAT_FLOAT32 0x00000002
#define HDA_STREAM_FORMAT_PCM     0x00000001

//
// Define the bits for the input and output amplifier capabilities parameters.
//

#define HDA_AMP_CAPABILITIES_MUTE             0x80000000
#define HDA_AMP_CAPABILITIES_STEP_SIZE_MASK   0x007F0000
#define HDA_AMP_CAPABILITIES_STEP_SIZE_SHIFT  16
#define HDA_AMP_CAPABILITIES_STEP_COUNT_MASK  0x00007F00
#define HDA_AMP_CAPABILITIES_STEP_COUNT_SHIFT 8
#define HDA_AMP_CAPABILITIES_OFFSET_MASK      0x0000007F
#define HDA_AMP_CAPABILITIES_OFFSET_SHIFT     0

//
// Define the bits for the connection list length parameter.
//

#define HDA_CONNECTION_LIST_LENGTH_LONG_FORM 0x00000080
#define HDA_CONNECTION_LIST_LENGTH_MASK      0x0000007F
#define HDA_CONNECTION_LIST_LENGTH_SHIFT     0

#define HDA_CONNECTION_LIST_LONG_FORM_RANGE         0x8000
#define HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_MASK  0x7FFF
#define HDA_CONNECTION_LIST_LONG_FORM_NODE_ID_SHIFT 0

#define HDA_CONNECTION_LIST_SHORT_FORM_RANGE         0x80
#define HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_MASK  0x7F
#define HDA_CONNECTION_LIST_SHORT_FORM_NODE_ID_SHIFT 0

//
// Define the bits for the supported power states parameter.
//

#define HDA_SUPPORTED_POWER_STATES_EXTENDED   0x80000000
#define HDA_SUPPORTED_POWER_STATES_CLOCK_STOP 0x40000000
#define HDA_SUPPORTED_POWER_STATES_S3_D3_COLD 0x20000000
#define HDA_SUPPORTED_POWER_STATES_D3_COLD    0x00000010
#define HDA_SUPPORTED_POWER_STATES_D3         0x00000008
#define HDA_SUPPORTED_POWER_STATES_D2         0x00000004
#define HDA_SUPPORTED_POWER_STATES_D1         0x00000002
#define HDA_SUPPORTED_POWER_STATES_D0         0x00000001

//
// Define the bits for the volume knob parameter.
//

#define HDA_VOLUME_KNOB_DELTA            0x00000080
#define HDA_VOLUME_KNOB_STEP_COUNT_MASK  0x0000007F
#define HDA_VOLUME_KNOB_STEP_COUNT_SHIFT 0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _HDA_REGISTER {
    HdaRegisterGlobalCapabilities = 0x00,
    HdaRegisterMinorVersion = 0x02,
    HdaRegisterMajorVersion = 0x03,
    HdaRegisterOutputPayloadCapability = 0x04,
    HdaRegisterInputPayloadCapability = 0x06,
    HdaRegisterGlobalControl = 0x08,
    HdaRegisterWakeEnable = 0x0C,
    HdaRegisterStateChangeStatus = 0x0E,
    HdaRegisterGlobalStatus = 0x10,
    HdaRegisterOutputStreamPayloadCapability = 0x18,
    HdaRegisterInputStreamPayloadCapability = 0x1A,
    HdaRegisterInterruptControl = 0x20,
    HdaRegisterInterruptStatus = 0x24,
    HdaRegisterWallClockCounter = 0x30,
    HdaRegisterLegacyStreamSynchronization = 0x34,
    HdaRegisterStreamSynchronization = 0x38,
    HdaRegisterCorbLowerBaseAddress = 0x40,
    HdaRegisterCorbUpperBaseAddress = 0x44,
    HdaRegisterCorbWritePointer = 0x48,
    HdaRegisterCorbReadPointer = 0x4A,
    HdaRegisterCorbControl = 0x4C,
    HdaRegisterCorbStatus = 0x4D,
    HdaRegisterCorbSize = 0x4E,
    HdaRegisterRirbLowerBaseAddress = 0x50,
    HdaRegisterRirbUpperBaseAddress = 0x54,
    HdaRegisterRirbWritePointer = 0x58,
    HdaRegisterResponseInterruptCount = 0x5A,
    HdaRegisterRirbControl = 0x5C,
    HdaRegisterRirbStatus = 0x5D,
    HdaRegisterRirbSize = 0x5E,
    HdaRegisterImmediateCommandOutputInterface = 0x60,
    HdaRegisterImmediateCommandInputInterface = 0x64,
    HdaRegisterImmediateCommandStatus = 0x68,
    HdaRegisterDmaPositionBufferLowerBase = 0x70,
    HdaRegisterDmaPositionBufferUpperBase = 0x74,
    HdaRegisterStreamDescriptorBase = 0x80,
    HdaRegisterWallClockCounterAlias = 0x2030,
} HDA_REGISTER, *PHDA_REGISTER;

typedef enum _HDA_STREAM_REGISTER {
    HdaStreamRegisterControl = 0x00,
    HdaStreamRegisterStatus = 0x03,
    HdaStreamRegisterLinkPositionInBuffer = 0x04,
    HdaStreamRegisterCyclicBufferLength = 0x08,
    HdaStreamRegisterLastValidIndex = 0x0C,
    HdaStreamRegisterFifoSize = 0x10,
    HdaStreamRegisterFormat = 0x12,
    HdaStreamRegisterBdlLowerBaseAddress = 0x18,
    HdaStreamRegisterBdlUpperBaseAddress = 0x1C,
} HDA_STREAM_REGISTER, *PHDA_STREAM_REGISTER;

typedef enum _HDA_VERB {
    HdaVerbSetConverterFormat = 0x2,
    HdaVerbSetAmplifierGain = 0x3,
    HdaVerbSetProcessingCoefficient = 0x4,
    HdaVerbSetCoefficientIndex = 0x5,
    HdaVerbGetConverterFormat = 0xA,
    HdaVerbGetAmplifierGain = 0xB,
    HdaVerbGetProcessingCoefficient = 0xC,
    HdaVerbGetCoefficientIndex = 0xD,
    HdaVerbSetConnectionSelectControl = 0x701,
    HdaVerbSetProcessingState = 0x703,
    HdaVerbSetInputConverterSdiSelect = 0x704,
    HdaVerbSetPowerState = 0x705,
    HdaVerbSetConverterStreamChannel = 0x706,
    HdaVerbSetPinWidgetControl = 0x707,
    HdaVerbSetUnsolicitedResponseControl = 0x708,
    HdaVerbExecutePinSense = 0x709,
    HdaVerbSetEapdBtlEnable = 0x70C,
    HdaVerbSetBeepGeneration = 0x70A,
    HdaVerbSetSic1 = 0x70D,
    HdaVerbSetSic2 = 0x70E,
    HdaVerbSetVolumeKnob = 0x70F,
    HdaVerbSetGpiData = 0x710,
    HdaVerbSetGpiWake = 0x711,
    HdaVerbSetGpiUnsolicited = 0x712,
    HdaVerbSetGpiSticky = 0x713,
    HdaVerbSetGpoData = 0x714,
    HdaVerbSetGpioData = 0x715,
    HdaVerbSetGpioEnable = 0x716,
    HdaVerbSetGpioDirection = 0x717,
    HdaVerbSetGpioWake = 0x718,
    HdaVerbSetGpioUnsolicited = 0x719,
    HdaVerbSetGpioSticky = 0x71A,
    HdaVerbSetConfigurationDefault1 = 0x71C,
    HdaVerbsetConfigurationDefault2 = 0x71D,
    HdaVerbSetConfigurationDefault3 = 0x71E,
    HdaVerbSetConfigurationDefault4 = 0x71F,
    HdaVerbSetImplementationId1 = 0x720,
    HdaVerbSetImplementationId2 = 0x721,
    HdaVerbSetImplementationId3 = 0x722,
    HdaVerbSetImplementationId4 = 0x723,
    HdaVerbSetStripeControl = 0x724,
    HdaVerbSetConverterChannelCount = 0x72D,
    HdaVerbSetDipIndex = 0x730,
    HdaVerbSetDipData = 0x731,
    HdaVerbSetDipTransmitControl = 0x732,
    HdaVerbSetContentProtectionControl = 0x733,
    HdaVerbSetAspChannelMapping = 0x734,
    HdaVerbSetSic3 = 0x73E,
    HdaVerbSetSic4 = 0x73F,
    HdaVerbExecuteFunctionGroupReset = 0x7FF,
    HdaVerbGetParameter = 0xF00,
    HdaVerbGetConnectionSelectControl = 0xF01,
    HdaVerbGetConnectionListEntry = 0xF02,
    HdaVerbGetProcessingState = 0xF03,
    HdaVerbGetInputConverterSdiSelect = 0xF04,
    HdaVerbGetPowerState = 0xF05,
    HdaVerbGetConverterStreamChannel = 0xF06,
    HdaVerbGetPinWidgetControl = 0xF07,
    HdaVerbGetUnsolicitedResponseControl = 0xF08,
    HdaVerbGetPinSense = 0xF09,
    HdaVerbGetBeepGeneration = 0xF0A,
    HdaVerbGetEapdBtlEnable = 0xF0C,
    HdaVerbGetSic = 0xF0D,
    HdaVerbGetVolumeKnob = 0xF0F,
    HdaVerbGetGpiData = 0xF10,
    HdaVerbGetGpiWake = 0xF11,
    HdaVerbGetGpiUnsolicited = 0xF12,
    HdaVerbGetGpiSticky = 0xF13,
    HdaVerbGetGpoData = 0xF14,
    HdaVerbGetGpioData = 0xF15,
    HdaVerbGetGpioEnable = 0xF16,
    HdaVerbGetGpioDirection = 0xF17,
    HdaVerbGetGpioWake = 0xF18,
    HdaVerbGetGpioUnsolicited = 0xF19,
    HdaVerbGetGpioSticky = 0xF1A,
    HdaVerbGetConfigurationDefault = 0xF1C,
    HdaVerbGetImplementationId = 0xF20,
    HdaVerbGetStripeControl = 0xF24,
    HdaVerbGetConverterChannelCount = 0xF2D,
    HdaVerbGetDipSize = 0xF2E,
    HdaVerbGetEld = 0xF2F,
    HdaVerbGetDipIndex = 0xF30,
    HdaVerbGetDipData = 0xF31,
    HdaVerbGetDipTransmitControl = 0xF32,
    HdaVerbGetContentProtectionControl = 0xF33,
    HdaVerbGetAspChannelMapping = 0xF34,
} HDA_VERB, *PHDA_VERB;

typedef enum _HDA_PARAMETER {
    HdaParameterVendorId = 0x00,
    HdaParameterRevisionId = 0x02,
    HdaParameterSubordinateNodeCount = 0x04,
    HdaParameterFunctionGroupType = 0x05,
    HdaParameterAudioFunctionGroupCapabilities = 0x08,
    HdaParameterAudioWidgetCapabilities = 0x09,
    HdaParameterSupportedPcmSizeRates = 0x0A,
    HdaParameterSupportedStreamFormats = 0x0B,
    HdaParameterPinCapabilities = 0x0C,
    HdaParameterInputAmplifierCapabilities = 0x0D,
    HdaParameterConnectionListLength = 0x0E,
    HdaParameterSupportedPowerStates = 0x0F,
    HdaParameterProcessingCapabilities = 0x10,
    HdaParameterGpioCount = 0x011,
    HdaParameterOutputAmplifierCapabilities = 0x12,
    HdaParameterVolumeKnobCapabilities = 0x13,
} HDA_PARAMETER, *PHDA_PARAMETER;

/*++

Structure Description:

    This structure defines a buffer descriptor list entry.

Members:

    Address - Stores the 64-bit address of the buffer. This must be 128-byte
        aligned.

    Length - Stores the length of the buffer in bytes. The buffer must be at
        least 4 bytes in length.

    Flags - Stores a bitmask of descriptor flags. See
        HDA_BUFFER_DESCRIPTOR_FLAG_* for definitions.

--*/

typedef struct _HDA_BUFFER_DESCRIPTOR_LIST_ENTRY {
    ULONGLONG Address;
    ULONG Length;
    ULONG Flags;
} PACKED HDA_BUFFER_DESCRIPTOR_LIST_ENTRY, *PHDA_BUFFER_DESCRIPTOR_LIST_ENTRY;

/*++

Structure Description:

    This structure defines a command output ring buffer entry.

Members:

    Verb - Stores the codec command verb.

--*/

typedef struct _HDA_COMMAND_ENTRY {
    ULONG Verb;
} PACKED HDA_COMMAND_ENTRY, *PHDA_COMMAND_ENTRY;

/*++

Structure Description:

    This structure defines a response input ring buffer entry.

Members:

    Response - Stores the codec's raw response.

    ResponseExtended - Stores the controller's information about the response,
        including which codec it came from and whether or not it was solicited.
        See HDA_RESPONSE_EXTENDED_FLAG_* for details.

--*/

typedef struct _HDA_RESPONSE_ENTRY {
    ULONG Response;
    ULONG ResponseExtended;
} PACKED HDA_RESPONSE_ENTRY, *PHDA_RESPONSE_ENTRY;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
