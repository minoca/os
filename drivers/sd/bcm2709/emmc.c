/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    emmc.c

Abstract:

    This module implements eMMC support for BCM2709 SoCs.

Author:

    Chris Stevens 10-Dec-2014

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
#include <minoca/soc/bcm27mbx.h>

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the data necessary to get a clock's rate in Hz.

Members:

    Header - Stores a header that defines the total size of the messages being
        sent to and received from the mailbox.

    ClockRate - Stores a request to get the rate for a particular clock.

    EndTag - Stores the tag to denote the end of the mailbox message.

--*/

typedef struct _BCM2709_EMMC_GET_CLOCK_INFORMATION {
    BCM2709_MAILBOX_HEADER Header;
    BCM2709_MAILBOX_GET_CLOCK_RATE ClockRate;
    ULONG EndTag;
} BCM2709_EMMC_GET_CLOCK_INFORMATION, *PBCM2709_EMMC_GET_CLOCK_INFORMATION;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
Bcm2709EmmcpMailboxSendPropertiesChannelCommand (
    PVOID CommandBuffer,
    ULONG CommandBufferSize,
    PIO_BUFFER *ResultIoBuffer
    );

VOID
Bcm2709EmmcpMailboxSend (
    PVOID Base,
    ULONG Channel,
    PHYSICAL_ADDRESS Data
    );

KSTATUS
Bcm2709EmmcpMailboxReceive (
    PVOID Base,
    ULONG Channel,
    PPHYSICAL_ADDRESS Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define a template for the command to enable the eMMC power.
//

BCM2709_MAILBOX_POWER Bcm2709EmmcPowerCommand = {
    {
        sizeof(BCM2709_MAILBOX_POWER),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_SET_POWER_STATE,
            sizeof(ULONG) + sizeof(ULONG),
            sizeof(ULONG) + sizeof(ULONG)
        },

        BCM2709_MAILBOX_DEVICE_SDHCI,
        BCM2709_MAILBOX_POWER_STATE_ON
    },

    0
};

//
// Define a template for the command to get the eMMC clock rate.
//

BCM2709_EMMC_GET_CLOCK_INFORMATION Bcm2709EmmcGetClockRateCommand = {
    {
        sizeof(BCM2709_EMMC_GET_CLOCK_INFORMATION),
        0
    },

    {
        {
            BCM2709_MAILBOX_TAG_GET_CLOCK_RATE,
            sizeof(ULONG) + sizeof(ULONG),
            sizeof(ULONG)
        },

        BCM2709_MAILBOX_CLOCK_ID_EMMC,
        0
    },

    0
};

PHYSICAL_ADDRESS Bcm2709MailboxPhysicalAddress;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Bcm2709EmmcInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the BCM2709 SoC's Emmc controller.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PBCM2709_TABLE Bcm2709Table;
    KSTATUS Status;

    Bcm2709Table = AcpiFindTable(BCM2709_SIGNATURE, NULL);
    if (Bcm2709Table == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Bcm2709MailboxPhysicalAddress = Bcm2709Table->MailboxPhysicalAddress;
    Status = Bcm2709EmmcpMailboxSendPropertiesChannelCommand(
                                                 &Bcm2709EmmcPowerCommand,
                                                 sizeof(BCM2709_MAILBOX_POWER),
                                                 NULL);

    return Status;
}

KSTATUS
Bcm2709EmmcGetClockFrequency (
    PULONG Frequency
    )

/*++

Routine Description:

    This routine gets the eMMC's clock frequency for the BCM2709 SoC.

Arguments:

    Frequency - Supplies a pointer that receives the eMMC's clock frequency.

Return Value:

    Status code.

--*/

{

    ULONG ExpectedLength;
    PBCM2709_EMMC_GET_CLOCK_INFORMATION GetClockInformation;
    ULONG Length;
    PIO_BUFFER ResultIoBuffer;
    KSTATUS Status;

    *Frequency = 0;
    Status = Bcm2709EmmcpMailboxSendPropertiesChannelCommand(
                                    &Bcm2709EmmcGetClockRateCommand,
                                    sizeof(BCM2709_EMMC_GET_CLOCK_INFORMATION),
                                    &ResultIoBuffer);

    if (!KSUCCESS(Status)) {
        goto EmmcGetClockFrequencyEnd;
    }

    ASSERT(ResultIoBuffer != NULL);
    ASSERT(ResultIoBuffer->FragmentCount == 1);
    ASSERT(ResultIoBuffer->Fragment[0].VirtualAddress != NULL);

    GetClockInformation = ResultIoBuffer->Fragment[0].VirtualAddress;
    Length = GetClockInformation->ClockRate.TagHeader.Length;
    ExpectedLength = sizeof(BCM2709_MAILBOX_GET_CLOCK_RATE) -
                     sizeof(BCM2709_MAILBOX_TAG);

    if (BCM2709_MAILBOX_CHECK_TAG_LENGTH(Length, ExpectedLength) == FALSE) {
        Status = STATUS_DEVICE_IO_ERROR;

    } else {
        *Frequency = GetClockInformation->ClockRate.Rate;
        Status = STATUS_SUCCESS;
    }

    MmFreeIoBuffer(ResultIoBuffer);

EmmcGetClockFrequencyEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
Bcm2709EmmcpMailboxSendPropertiesChannelCommand (
    PVOID CommandBuffer,
    ULONG CommandBufferSize,
    PIO_BUFFER *ResultIoBuffer
    )

/*++

Routine Description:

    This routine sends a command to the mailbox's properties channel and
    returns the result, if requested.

Arguments:

    CommandBuffer - Supplies a pointer to the mailbox command that is to be
        sent to the properties channel.

    CommandBufferSize - Supplies the size of the command buffer.

    ResultIoBuffer - Supplies an optional pointer that receives a pointer to
        the I/O buffer returned by the mailbox. The caller is expected to
        release the I/O buffer.

Return Value:

    Status code.

--*/

{

    PBCM2709_MAILBOX_HEADER Header;
    ULONG IoBufferFlags;
    PVOID MailboxBase;
    ULONG PageSize;
    PHYSICAL_ADDRESS ReceivePhysicalAddress;
    PIO_BUFFER SendIoBuffer;
    PHYSICAL_ADDRESS SendPhysicalAddress;
    KSTATUS Status;

    PageSize = MmPageSize();
    SendIoBuffer = NULL;
    if (Bcm2709MailboxPhysicalAddress == INVALID_PHYSICAL_ADDRESS) {
        return STATUS_NOT_INITIALIZED;
    }

    //
    // Map the maiilbox base.
    //

    MailboxBase = MmMapPhysicalAddress(Bcm2709MailboxPhysicalAddress,
                                       PageSize,
                                       TRUE,
                                       FALSE,
                                       TRUE);

    if (MailboxBase == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    //
    // Allocate and map an aligned I/O buffer for sending the data.
    //

    IoBufferFlags = IO_BUFFER_FLAG_PHYSICALLY_CONTIGUOUS |
                    IO_BUFFER_FLAG_MAP_NON_CACHED;

    SendIoBuffer = MmAllocateNonPagedIoBuffer(0,
                                              MAX_ULONG,
                                              BCM2709_MAILBOX_DATA_ALIGNMENT,
                                              CommandBufferSize,
                                              IoBufferFlags);

    if (SendIoBuffer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto MailboxSendPropertiesChannelCommandEnd;
    }

    ASSERT(SendIoBuffer->FragmentCount == 1);
    ASSERT(SendIoBuffer->Fragment[0].VirtualAddress != NULL);

    //
    // Copy the data from the given command buffer.
    //

    RtlCopyMemory(SendIoBuffer->Fragment[0].VirtualAddress,
                  CommandBuffer,
                  CommandBufferSize);

    //
    // Send the data to the properties channel.
    //

    SendPhysicalAddress = SendIoBuffer->Fragment[0].PhysicalAddress;
    Bcm2709EmmcpMailboxSend(MailboxBase,
                            BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                            SendPhysicalAddress);

    //
    // Wait for a response to make sure the data was written or to get the read
    // data.
    //

    Status = Bcm2709EmmcpMailboxReceive(MailboxBase,
                                        BCM2709_MAILBOX_PROPERTIES_CHANNEL,
                                        &ReceivePhysicalAddress);

    if (!KSUCCESS(Status)) {
        goto MailboxSendPropertiesChannelCommandEnd;
    }

    //
    // The receive should be sending the same buffer back.
    //

    ASSERT(ReceivePhysicalAddress == SendPhysicalAddress);

    Header = SendIoBuffer->Fragment[0].VirtualAddress;
    if (Header->Code != BCM2709_MAILBOX_STATUS_SUCCESS) {
        Status = STATUS_UNSUCCESSFUL;
        goto MailboxSendPropertiesChannelCommandEnd;
    }

    Status = STATUS_SUCCESS;

MailboxSendPropertiesChannelCommandEnd:
    if (KSUCCESS(Status) && (ResultIoBuffer != NULL)) {
        *ResultIoBuffer = SendIoBuffer;
        SendIoBuffer = NULL;
    }

    if (SendIoBuffer != NULL) {
        MmFreeIoBuffer(SendIoBuffer);
    }

    MmUnmapAddress(MailboxBase, PageSize);
    return STATUS_SUCCESS;
}

VOID
Bcm2709EmmcpMailboxSend (
    PVOID Base,
    ULONG Channel,
    PHYSICAL_ADDRESS Data
    )

/*++

Routine Description:

    This routine send the given data to the specified mailbox channel.

Arguments:

    Base - Supplies the base at which the mailbox is mapped.

    Channel - Supplies the mailbox channel to which the data should be sent.

    Data - Supplies a pointer to the data to send to the mailbox.

Return Value:

    None.

--*/

{

    ULONG MailboxData;
    ULONG MailboxStatus;

    //
    // The data should be aligned such that there is room to OR in the channel
    // information.
    //

    ASSERT((ULONG)Data == (UINTN)Data);
    ASSERT(((ULONG)Data & BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) == 0);
    ASSERT((Channel & ~BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) == 0);

    //
    // Wait until there is nothing to read as noted by the read empty flag.
    //

    while (TRUE) {
        MailboxStatus = BCM2709_READ_MAILBOX_REGISTER(Base,
                                                      Bcm2709MailboxStatus);

        if ((MailboxStatus & BCM2709_MAILBOX_STATUS_READ_EMPTY) != 0) {
            break;
        }
    }

    //
    // Wait until there is room to write into the mailbox.
    //

    while (TRUE) {
        MailboxStatus = BCM2709_READ_MAILBOX_REGISTER(Base,
                                                      Bcm2709MailboxStatus);

        if ((MailboxStatus & BCM2709_MAILBOX_STATUS_WRITE_FULL) == 0) {
            break;
        }
    }

    //
    // Add the channel to the supplied data and write the data to the mailbox.
    //

    MailboxData = (ULONG)Data | Channel;
    BCM2709_WRITE_MAILBOX_REGISTER(Base, Bcm2709MailboxWrite, MailboxData);
    return;
}

KSTATUS
Bcm2709EmmcpMailboxReceive (
    PVOID Base,
    ULONG Channel,
    PPHYSICAL_ADDRESS Data
    )

/*++

Routine Description:

    This routine receives data from the given mailbox channel.

Arguments:

    Base - Supplies the base at which the mailbox is mapped.

    Channel - Supplies the mailbox channel from which data is expected.

    Data - Supplies a pointer that receives the data returned by the mailbox.

Return Value:

    Status code.

--*/

{

    ULONG MailboxData;
    ULONG MailboxStatus;
    KSTATUS Status;

    //
    // Wait until there is something to read from the mailbox.
    //

    while (TRUE) {
        MailboxStatus = BCM2709_READ_MAILBOX_REGISTER(Base,
                                                      Bcm2709MailboxStatus);

        if ((MailboxStatus & BCM2709_MAILBOX_STATUS_READ_EMPTY) == 0) {
            break;
        }
    }

    //
    // Read the mailbox and fail if the response is not for the correct channel.
    // There really shouldn't be concurrency issues at this point, but the
    // recourse would be to retry until data from the correct channel is
    // returned.
    //

    MailboxData = BCM2709_READ_MAILBOX_REGISTER(Base, Bcm2709MailboxRead);
    if ((MailboxData & BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) != Channel) {
        Status = STATUS_UNSUCCESSFUL;
        goto Bcm2709MailboxReceiveEnd;
    }

    //
    // Remove the channel information and return the data.
    //

    MailboxData &= ~BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK;
    *Data = (PHYSICAL_ADDRESS)MailboxData;
    Status = STATUS_SUCCESS;

Bcm2709MailboxReceiveEnd:
    return Status;
}

