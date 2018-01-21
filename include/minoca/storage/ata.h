/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ata.h

Abstract:

    This header contains definitions for the AT Attachment storage de-facto
    standard.

Author:

    Evan Green 15-Nov-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

#define ATA_SECTOR_SIZE 512

//
// Define ATA supported command set bits.
//

#define ATA_SUPPORTED_COMMAND_LBA48 (1 << 26)

//
// Define values that come out of the LBA1 and LBA2 registers when ATAPI or
// SATA devices are interrogated using an ATA IDENTIFY command.
//

#define ATA_PATAPI_LBA1 0x14
#define ATA_PATAPI_LBA2 0xEB
#define ATA_SATAPI_LBA1 0x69
#define ATA_SATAPI_LBA2 0x96
#define ATA_SATA_LBA1 0x3C
#define ATA_SATA_LBA2 0xC3

//
// Define the maximum LBA for the LBA28 command set.
//

#define ATA_MAX_LBA28 0x0FFFFFFFULL

#define ATA_MAX_LBA28_SECTOR_COUNT 0x100
#define ATA_MAX_LBA48_SECTOR_COUNT 0x10000

//
// Define ATA drive select register bits.
//

#define ATA_DRIVE_SELECT_LBA 0x40
#define ATA_DRIVE_SELECT_MASTER 0xA0
#define ATA_DRIVE_SELECT_SLAVE 0xB0

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ATA_COMMAND {
    AtaCommandReadPio28         = 0x20,
    AtaCommandReadPio48         = 0x24,
    AtaCommandReadDma48         = 0x25,
    AtaCommandWritePio28        = 0x30,
    AtaCommandWritePio48        = 0x34,
    AtaCommandWriteDma48        = 0x35,
    AtaCommandPacket            = 0xA0,
    AtaCommandIdentifyPacket    = 0xA1,
    AtaCommandReadDma28         = 0xC8,
    AtaCommandWriteDma28        = 0xCA,
    AtaCommandCacheFlush28      = 0xE7,
    AtaCommandCacheFlush48      = 0xEA,
    AtaCommandIdentify          = 0xEC,
} ATA_COMMAND, *PATA_COMMAND;

/*++

Structure Description:

    This structure defines the result of an IDENTIFY DEVICE command sent to a
    drive.

Members:

    Configuration - Stores configuration information about the device such as
        whether it's a removable device and whether it's an ATA device.

    SerialNumber - Stores a 20 byte ASCII string representing the device's
        serial number.

    FirmwareRevision - Stores an 8 byte ASCII string representing the device's
        firmware revision.

    ModelNumber - Stores a 40 byte ASCII string representing the device model.

    MaxMultipleSectorTransfer - Stores the maximum number of sectors that can
        be transferred per interrupt on READ/WRITE MULTIPLE commands.

    Capabilities - Stores device capability bits such as whether LBA is
        supported, IORDY is supporetd, DMA is supported, etc.

    ValidFields - Stores bits indicating whether the words in fields 64-70 and
        word 88 are valid.

    CurrentMaxSectorTransfer - Stores the current setting for the number of
        sectors that can be transferred per interrupts on a READ/WRITE MULTIPLE
        command.

    TotalSectors - Stores the total number of user addressable sectors. If the
        LBA48 command set is supported, use that value instead of this one.

    MultiwordDmaSettings - Stores which Multiword DMA modes are supported, and
        which mode is selected.

    PioModesSupported - Stores which Polled I/O modes are supported on this
        device.

    MinMultiwordTransferCycles - Stores the minimum Multiword DMA transfer
        cycle time in nanoseconds.

    RecommendedMultiwordTransferCycles - Stores the manufacturer's
        recommended Multiword DMA transfer cycle time in nanoseconds.

    MinPioTransferCyclesNoFlow - Stores the minimum PIO transfer cycle time
        without flow control, in nanoseconds.

    MinPioTransferCyclesWithFlow - Stores the minimum PIO transfer cycle time
        with IORDY flow control, in nanoseconds.

    QueueDepth - Stores the maximum queue depth minus one.

    MajorVersion - Stores the major version of the ATA/ATAPI protocol
        supported.

    MinorVersion - Stores the device minor version.

    CommandSetSupported - Stores the command/feature sets that are supported on
        this device.

    FeatureSetSupported - Stores the command/feature extensions that are
        supported on this device.

    CommandSetEnabled - Stores a bitmask showing which command/features sets
        are currently enabled.

    CommandSetDefault - Stores the default features enabled.

    UltraDmaSettings - Stores which Ultra DMA modes are supported and currently
        enabled on this device.

    SecurityEraseTime - Stores the time required for security erase unit
        completion.

    EnhancedSecurityEraseTime - Stores the time required for enhanced security
        erase completion.

    CurrentPowerManagementValue - Stores the current power managment value.

    PasswordRevisionCode - Stores the Master Password Revision Code.

    ResetResult - Stores various statistics about how the drives behaved during
        a hardware reset.

    AcousticManagement - Stores the recommended and current acoustic
        management value.

    TotalSectorsLba48 - Stores the one beyond the maximum valid block number if
        the LBA48 command set is supported.

    RemovableMediaStatus - Stores whether or not the removable media status
        notification feature set is supported.

    SecurityStatus - Stores the current security state of the drive.

    PowerMode1 - Stores whether or not the CFA power mode 1 is supported or
        required for some commands.

    MediaSerialNumber - Stores the current media serial number.

    Checksum - Stores the two's complement of the sum of all bytes in words
        0-254 and the byte in bits 0-7 of word 255, if bits 0-7 of word 255
        contains the value 0xA5. Each byte shall be added with unsigned
        arithmetic, and overflow shall be ignored. The sum of all 512 bytes is
        zero when the checksum is correct.

--*/

#pragma pack(push, 1)

typedef struct _ATA_IDENTIFY_PACKET {
    USHORT Configuration;
    USHORT Reserved1[9];
    CHAR SerialNumber[20];
    USHORT Reserved2[3];
    CHAR FirmwareRevision[8];
    CHAR ModelNumber[40];
    USHORT MaxMultipleSectorTransfer;
    USHORT Reserved3;
    ULONG Capabilities;
    USHORT Reserved4[2];
    USHORT ValidFields;
    USHORT Reserved5[5];
    USHORT CurrentMaxSectorTransfer;
    ULONG TotalSectors;
    USHORT Reserved6;
    USHORT MultiwordDmaSettings;
    USHORT PioModesSupported;
    USHORT MinMultiwordTransferCycles;
    USHORT RecommendedMultiwordTransferCycles;
    USHORT MinPioTransferCyclesNoFlow;
    USHORT MinPioTransferCyclesWithFlow;
    USHORT Reserved7[6];
    USHORT QueueDepth;
    USHORT Reserved8[4];
    USHORT MajorVersion;
    USHORT MinorVersion;
    ULONG CommandSetSupported;
    USHORT FeatureSetSupported;
    ULONG CommandSetEnabled;
    USHORT CommandSetDefault;
    USHORT UltraDmaSettings;
    USHORT SecurityEraseTime;
    USHORT EnhancedSecurityEraseTime;
    USHORT CurrentPowerManagementValue;
    USHORT PasswordRevisionCode;
    USHORT ResetResult;
    USHORT AcousticManagement;
    USHORT Reserved9[5];
    ULONGLONG TotalSectorsLba48;
    USHORT Reserved10[23];
    USHORT RemovableMediaStatus;
    USHORT SecurityStatus;
    USHORT Reserved11[31];
    USHORT PowerMode1;
    USHORT Reserved12[15];
    USHORT MediaSerialNumber[30];
    USHORT Reserved13[49];
    USHORT Checksum;
} PACKED ATA_IDENTIFY_PACKET, *PATA_IDENTIFY_PACKET;

#pragma pack(pop)

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
