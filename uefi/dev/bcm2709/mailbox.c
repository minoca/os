/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mailbox.c

Abstract:

    This module implements support for the Broadcom 2709 Mailbox.

Author:

    Chris Stevens 31-Dec-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/bcm2709.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macro reads from the BCM2709 mailbox. The parameter should
// be a BCM2709_MAILBOX_REGISTER value.
//

#define READ_MAILBOX_REGISTER(_Register) \
    EfiReadRegister32(BCM2709_MAILBOX_BASE + (_Register))

//
// This macro writes to the BCM2709 mailbox. _Register should be a
// BCM2709_MAILBOX_REGISTER value and _Value should be a UINT32.
//

#define WRITE_MAILBOX_REGISTER(_Register, _Value) \
    EfiWriteRegister32(BCM2709_MAILBOX_BASE + (_Register), (_Value))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
EfipBcm2709MailboxSend (
    UINT32 Channel,
    VOID *Data
    );

EFI_STATUS
EfipBcm2709MailboxReceive (
    UINT32 Channel,
    VOID **Data
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFI_STATUS
EfipBcm2709MailboxSendCommand (
    UINT32 Channel,
    VOID *Command,
    UINT32 CommandSize,
    BOOLEAN Set
    )

/*++

Routine Description:

    This routine sends the given command to the given channel of the BCM2709's
    mailbox. If it is a GET request, then the data will be returned in the
    supplied command buffer.

Arguments:

    Channel - Supplies the mailbox channel that is to receive the command.

    Command - Supplies the command to send.

    CommandSize - Supplies the size of the command to send.

    Set - Supplies a boolean indicating whether or not the command is a SET
            (TRUE) or GET (FALSE) request. SET-GET requests should be
            considered GETs.

Return Value:

    Status code.

--*/

{

    VOID *AlignedBuffer;
    UINTN AllocationSize;
    VOID *Buffer;
    PBCM2709_MAILBOX_HEADER Header;
    VOID *ReceiveBuffer;
    EFI_STATUS Status;

    //
    // The BCM2709 device library must be initialized.
    //

    if (EfiBcm2709Initialized == FALSE) {
        return EFI_NOT_READY;
    }

    //
    // Allocate an aligned buffer for the command, if necessary.
    //

    Buffer = NULL;
    if (ALIGN_POINTER(Command, BCM2709_MAILBOX_DATA_ALIGNMENT) != Command) {
        AllocationSize = CommandSize + BCM2709_MAILBOX_DATA_ALIGNMENT;
        Status = EfiAllocatePool(EfiBootServicesData, AllocationSize, &Buffer);
        if (EFI_ERROR(Status)) {
            goto Bcm2709MailboxSendCommandEnd;
        }

        AlignedBuffer = ALIGN_POINTER(Buffer, BCM2709_MAILBOX_DATA_ALIGNMENT);

        //
        // Copy the data from the USB power template.
        //

        EfiCopyMem(AlignedBuffer, Command, CommandSize);

    } else {
        AlignedBuffer = Command;
    }

    //
    // Send the aligned command to the given channel.
    //

    EfipBcm2709MailboxSend(Channel, AlignedBuffer);

    //
    // Wait for a response to make sure the data was written or to get the read
    // data.
    //

    Status = EfipBcm2709MailboxReceive(Channel, &ReceiveBuffer);
    if (EFI_ERROR(Status)) {
        goto Bcm2709MailboxSendCommandEnd;
    }

    //
    // Check to make sure the transmission was successful.
    //

    Header = (PBCM2709_MAILBOX_HEADER)ReceiveBuffer;
    if (Header->Code != BCM2709_MAILBOX_STATUS_SUCCESS) {
        Status = EFI_DEVICE_ERROR;
        goto Bcm2709MailboxSendCommandEnd;
    }

    //
    // Copy the result back to the original command buffer in case information
    // was returned.
    //

    if ((Set == FALSE) && (ReceiveBuffer != Command)) {
        EfiCopyMem(Command, ReceiveBuffer, CommandSize);
    }

Bcm2709MailboxSendCommandEnd:
    if (Buffer != NULL) {
        EfiFreePool(Buffer);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
EfipBcm2709MailboxSend (
    UINT32 Channel,
    VOID *Data
    )

/*++

Routine Description:

    This routine sends the given data to the specified mailbox channel.

Arguments:

    Channel - Supplies the mailbox channel to which the data should be sent.

    Data - Supplies the data buffer to send to the mailbox.

Return Value:

    None.

--*/

{

    UINT32 MailboxData;
    UINT32 MailboxStatus;

    //
    // The data should be aligned such that there is room to OR in the channel
    // information.
    //
    // ASSERT((UINT32)Data == (UINTN)Data);
    // ASSERT(((UINT32)Data & BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) == 0);
    // ASSERT((Channel & ~BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) == 0);
    //

    //
    // Wait until there is nothing to read as noted by the read empty flag.
    //

    while (TRUE) {
        MailboxStatus = READ_MAILBOX_REGISTER(Bcm2709MailboxStatus);
        if ((MailboxStatus & BCM2709_MAILBOX_STATUS_READ_EMPTY) != 0) {
            break;
        }
    }

    //
    // Wait until there is room to write into the mailbox.
    //

    while (TRUE) {
        MailboxStatus = READ_MAILBOX_REGISTER(Bcm2709MailboxStatus);
        if ((MailboxStatus & BCM2709_MAILBOX_STATUS_WRITE_FULL) == 0) {
            break;
        }
    }

    //
    // Add the channel to the supplied data and write the data to the mailbox.
    //

    MailboxData = (UINT32)Data | Channel;
    WRITE_MAILBOX_REGISTER(Bcm2709MailboxWrite, MailboxData);
    return;
}

EFI_STATUS
EfipBcm2709MailboxReceive (
    UINT32 Channel,
    VOID **Data
    )

/*++

Routine Description:

    This routine receives data from the given mailbox channel.

Arguments:

    Channel - Supplies the mailbox channel from which data is expected.

    Data - Supplies a pointer that receives the data buffer returned by the
        mailbox.

Return Value:

    Status code.

--*/

{

    UINT32 MailboxData;
    UINT32 MailboxStatus;
    EFI_STATUS Status;

    //
    // Wait until there is something to read from the mailbox.
    //

    while (TRUE) {
        MailboxStatus = READ_MAILBOX_REGISTER(Bcm2709MailboxStatus);
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

    MailboxData = READ_MAILBOX_REGISTER(Bcm2709MailboxRead);
    if ((MailboxData & BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK) != Channel) {
        Status = EFI_NOT_READY;
        goto Bcm2709MailboxReceiveEnd;
    }

    //
    // Remove the channel information and return the data.
    //

    MailboxData &= ~BCM2709_MAILBOX_READ_WRITE_CHANNEL_MASK;
    *Data = (VOID *)MailboxData;
    Status = EFI_SUCCESS;

Bcm2709MailboxReceiveEnd:
    return Status;
}

