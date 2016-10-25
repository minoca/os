/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    mailbox.c

Abstract:

    This module implements mailbox support for the TI AM33xx SoCs.

Author:

    Evan Green 1-Oct-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include <minoca/fw/acpitabs.h>
#include <minoca/soc/am335x.h>
#include "mailbox.h"

//
// --------------------------------------------------------------------- Macros
//

#define AM3_READ_MAILBOX(_Controller, _Register) \
    HlReadRegister32((_Controller)->ControllerBase + (_Register))

#define AM3_WRITE_MAILBOX(_Controller, _Register, _Value) \
    HlWriteRegister32((_Controller)->ControllerBase + (_Register), (_Value))

#define AM3_MAILBOX_MESSAGE(_Index) (Am3MailboxMessage0 + ((_Index) * 4))
#define AM3_MAILBOX_FIFO_STATUS(_Index) (Am3MailboxFifoStatus0 + ((_Index) * 4))
#define AM3_MAILBOX_MESSAGE_STATUS(_Index) \
    (Am3MailboxMessageStatus0 + ((_Index) * 4))

//
// There are four possible users of the mailbox:
// 0 - MPU subsystem
// 1 - PRU_ICSS PRU0
// 2 - PRU_ICSS PRU1
// 3 - WakeM3
//
// Each user has their own interrupt.
//

#define AM3_MAILBOX_INTERRUPT_STATUS(_User) \
    (Am3MailboxInterruptStatusClear0 + ((_User) * 0x10))

#define AM3_MAILBOX_INTERRUPT_ENABLE(_User) \
    (Am3MailboxInterruptEnableSet0 + ((_User) * 0x10))

#define AM3_MAILBOX_INTERRUPT_DISABLE(_User) \
    (Am3MailboxInterruptEnableClear0 + ((_User) * 0x10))

//
// This macro returns a bitmask of the given interrupt for the given mailbox
// index.
//

#define AM3_MAILBOX_INTERRUPT(_Mask, _Index) ((_Mask) << ((_Index) * 2))

//
// ---------------------------------------------------------------- Definitions
//

#define AM3_MAILBOX_INTERRUPT_MESSAGE 0x00000001
#define AM3_MAILBOX_INTERRUPT_NOT_FULL 0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _AM3_MAILBOX_REGISTER {
    Am3MailboxRevision = 0x000,
    Am3MailboxSysConfig = 0x010,
    Am3MailboxMessage0 = 0x040,
    Am3MailboxFifoStatus0 = 0x080,
    Am3MailboxMessageStatus0 = 0x0C0,
    Am3MailboxInterruptStatusRaw0 = 0x100,
    Am3MailboxInterruptStatusClear0 = 0x104,
    Am3MailboxInterruptEnableSet0 = 0x108,
    Am3MailboxInterruptEnableClear0 = 0x10C,
} AM3_MAILBOX_REGISTER, *PAM3_MAILBOX_REGISTER;

typedef enum _AM3_MAILBOX_USER {
    Am3MailboxUserMpu = 0,
    Am3MailboxUserPru0 = 1,
    Am3MailboxUserPru1 = 2,
    Am3MailboxUserWakeM3 = 3
} AM3_MAILBOX_USER, *PAM3_MAILBOX_USER;

//
// ----------------------------------------------- Internal Function Prototypes
//

INTERRUPT_STATUS
Am3MailboxInterruptService (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
Am3MailboxInitialize (
    PAM3_MAILBOX Mailbox,
    PIRP Irp,
    PRESOURCE_ALLOCATION ControllerPhysical,
    ULONGLONG InterruptLine,
    ULONGLONG InterruptVector
    )

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

{

    UINTN AlignmentOffset;
    IO_CONNECT_INTERRUPT_PARAMETERS Connect;
    PHYSICAL_ADDRESS EndAddress;
    ULONG PageSize;
    PHYSICAL_ADDRESS PhysicalAddress;
    ULONG Register;
    UINTN Size;
    KSTATUS Status;
    ULONG Value;

    ASSERT(Mailbox->ControllerBase == NULL);

    Mailbox->InterruptHandle = INVALID_HANDLE;
    Mailbox->InterruptLine = InterruptLine;
    Mailbox->InterruptVector = InterruptVector;

    //
    // Map the registers.
    //

    if (Mailbox->ControllerBase == NULL) {

        //
        // Page align the mapping request.
        //

        PageSize = MmPageSize();
        PhysicalAddress = ControllerPhysical->Allocation;
        EndAddress = PhysicalAddress + ControllerPhysical->Length;
        PhysicalAddress = ALIGN_RANGE_DOWN(PhysicalAddress, PageSize);
        AlignmentOffset = ControllerPhysical->Allocation - PhysicalAddress;
        EndAddress = ALIGN_RANGE_UP(EndAddress, PageSize);
        Size = (ULONG)(EndAddress - PhysicalAddress);

        //
        // If the size is not a the constant, then the failure code at the
        // bottom needs to be fancier.
        //

        ASSERT(Size == AM335_MAILBOX_SIZE);

        Mailbox->ControllerBase = MmMapPhysicalAddress(PhysicalAddress,
                                                       Size,
                                                       TRUE,
                                                       FALSE,
                                                       TRUE);

        if (Mailbox->ControllerBase == NULL) {
            Status = STATUS_NO_MEMORY;
            goto MailboxInitializeEnd;
        }

        Mailbox->ControllerBase += AlignmentOffset;
    }

    ASSERT(Mailbox->ControllerBase != NULL);

    //
    // Connect the mailbox interrupt.
    //

    ASSERT(Mailbox->InterruptHandle == INVALID_HANDLE);

    RtlZeroMemory(&Connect, sizeof(IO_CONNECT_INTERRUPT_PARAMETERS));
    Connect.Version = IO_CONNECT_INTERRUPT_PARAMETERS_VERSION;
    Connect.Device = Irp->Device;
    Connect.LineNumber = Mailbox->InterruptLine;
    Connect.Vector = Mailbox->InterruptVector;
    Connect.InterruptServiceRoutine = Am3MailboxInterruptService;
    Connect.Context = Mailbox;
    Connect.Interrupt = &(Mailbox->InterruptHandle);
    Status = IoConnectInterrupt(&Connect);
    if (!KSUCCESS(Status)) {
        goto MailboxInitializeEnd;
    }

    //
    // Enable interrupts towards the Cortex M3 for the mailbox dedicated to it.
    //

    Register = AM3_MAILBOX_INTERRUPT_ENABLE(Am3MailboxUserWakeM3);
    Value = AM3_MAILBOX_INTERRUPT(AM3_MAILBOX_INTERRUPT_MESSAGE,
                                  AM335_WAKEM3_MAILBOX);

    AM3_WRITE_MAILBOX(Mailbox, Register, Value);

MailboxInitializeEnd:
    if (!KSUCCESS(Status)) {
        Am3MailboxDestroy(Mailbox);
    }

    return Status;
}

VOID
Am3MailboxDestroy (
    PAM3_MAILBOX Mailbox
    )

/*++

Routine Description:

    This routine tears down a mailbox controller.

Arguments:

    Mailbox - Supplies a pointer to the initialized controller.

Return Value:

    None.

--*/

{

    if (Mailbox->InterruptHandle != INVALID_HANDLE) {
        IoDisconnectInterrupt(Mailbox->InterruptHandle);
        Mailbox->InterruptHandle = INVALID_HANDLE;
    }

    if (Mailbox->ControllerBase != NULL) {
        MmUnmapAddress(Mailbox->ControllerBase, AM335_MAILBOX_SIZE);
        Mailbox->ControllerBase = NULL;
    }

    return;
}

VOID
Am3MailboxSend (
    PAM3_MAILBOX Mailbox,
    ULONG Index,
    ULONG Message
    )

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

{

    ULONG Register;

    Register = AM3_MAILBOX_MESSAGE(Index);
    AM3_WRITE_MAILBOX(Mailbox, Register, Message);
    return;
}

VOID
Am3MailboxFlush (
    PAM3_MAILBOX Mailbox,
    ULONG Index
    )

/*++

Routine Description:

    This routine reads all messages back out of the mailbox and discards them.

Arguments:

    Mailbox - Supplies a pointer to the controller.

    Index - Supplies the mailbox number to write to. Valid values are 0-7.

Return Value:

    None.

--*/

{

    ULONG InterruptStatus;
    ULONG InterruptStatusRegister;
    ULONG MessageRegister;
    ULONG MessageStatusRegister;

    MessageRegister = AM3_MAILBOX_MESSAGE(Index);
    MessageStatusRegister = AM3_MAILBOX_MESSAGE_STATUS(Index);
    while (AM3_READ_MAILBOX(Mailbox, MessageStatusRegister) != 0) {
        AM3_READ_MAILBOX(Mailbox, MessageRegister);
    }

    //
    // Remove any interrupts from the Cortex M3 status as well.
    //

    if (Index == AM335_WAKEM3_MAILBOX) {
        InterruptStatusRegister =
                            AM3_MAILBOX_INTERRUPT_STATUS(Am3MailboxUserWakeM3);

        InterruptStatus = AM3_READ_MAILBOX(Mailbox, InterruptStatusRegister);
        if (InterruptStatus != 0) {
            AM3_WRITE_MAILBOX(Mailbox,
                              InterruptStatusRegister,
                              InterruptStatus);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

INTERRUPT_STATUS
Am3MailboxInterruptService (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents an interrupt service routine.

Arguments:

    Context - Supplies the context supplied when this interrupt was initially
        connected.

Return Value:

    Returns an interrupt status indicating if this ISR is claiming the
    interrupt, not claiming the interrupt, or needs the interrupt to be
    masked temporarily.

--*/

{

    //
    // Complete official mailbox support is not yet implemented. The only user
    // is currently the sleep code, which has interrupts disabled the whole
    // time.
    //

    ASSERT(FALSE);

    return InterruptStatusNotClaimed;
}

