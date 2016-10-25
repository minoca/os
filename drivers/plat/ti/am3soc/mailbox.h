/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mailbox.h

Abstract:

    This header contains definitions for the AM335x mailbox facilities.

Author:

    Evan Green 1-Oct-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the mailbox number reserved for the Cortex M3.
//

#define AM335_WAKEM3_MAILBOX 0

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the context for the AM33xx mailbox controller.

Members:

    ControllerBase - Stores the virtual address of the controller registers.

    InterruptLine - Stores the interrupt line the controller is connected to.

    InterruptVector - Stores the interrupt vector the controller interrupts on.

    InterruptHandle - Stores the interrupt connection handle.

--*/

typedef struct _AM3_MAILBOX {
    PVOID ControllerBase;
    ULONGLONG InterruptLine;
    ULONGLONG InterruptVector;
    HANDLE InterruptHandle;
} AM3_MAILBOX, *PAM3_MAILBOX;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
Am3MailboxInitialize (
    PAM3_MAILBOX Mailbox,
    PIRP Irp,
    PRESOURCE_ALLOCATION ControllerPhysical,
    ULONGLONG InterruptLine,
    ULONGLONG InterruptVector
    );

/*++

Routine Description:

    This routine initializes support for the mailbox.

Arguments:

    Irp - Supplies a pointer to the start IRP.

    Mailbox - Supplies a pointer to the controller, which is assumed to have
        been zeroed.

    ControllerPhysical - Supplies a pointer to the physical resource allocation
        of the mailbox controller.

    InterruptLine - Supplies the interrupt line the mailbox is connected on.

    InterruptVector - Supplies the interrupt vector the mailbox should use.

Return Value:

    Status code.

--*/

VOID
Am3MailboxDestroy (
    PAM3_MAILBOX Mailbox
    );

/*++

Routine Description:

    This routine tears down a mailbox controller.

Arguments:

    Mailbox - Supplies a pointer to the initialized controller.

Return Value:

    None.

--*/

VOID
Am3MailboxSend (
    PAM3_MAILBOX Mailbox,
    ULONG Index,
    ULONG Message
    );

/*++

Routine Description:

    This routine writes a new message to the AM3 mailbox.

Arguments:

    Mailbox - Supplies a pointer to the controller.

    Index - Supplies the mailbox number to write to. Valid values are 0-7.

    Message - Supplies the value to write.

Return Value:

    None.

--*/

VOID
Am3MailboxFlush (
    PAM3_MAILBOX Mailbox,
    ULONG Index
    );

/*++

Routine Description:

    This routine reads all messages back out of the mailbox and discards them.

Arguments:

    Mailbox - Supplies a pointer to the controller.

    Index - Supplies the mailbox number to write to. Valid values are 0-7.

Return Value:

    None.

--*/
