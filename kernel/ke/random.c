/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    random.c

Abstract:

    This module implements kernel-wide entropy management.

Author:

    Evan Green 14-Jan-2015

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/intrface/random.h>
#include "kep.h"

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
KepPseudoRandomInterfaceCallback (
    PVOID Context,
    PDEVICE Device,
    PVOID InterfaceBuffer,
    ULONG InterfaceBufferSize,
    BOOL Arrival
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Set this to TRUE to disable entropy gathering in the kernel. This is only
// polled once during boot.
//

BOOL KeDisableEntropyGathering = FALSE;

UUID KePseudoRandomInterfaceUuid = UUID_PSEUDO_RANDOM_SOURCE_INTERFACE;
PINTERFACE_PSEUDO_RANDOM_SOURCE KePseudoRandomInterface;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
KSTATUS
KeGetRandomBytes (
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine returns pseudo-random bytes from the system's random source.

Arguments:

    Buffer - Supplies a pointer where the random bytes will be returned on
        success.

    Size - Supplies the number of bytes of random data to get.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_SUCH_DEVICE if no pseudo-random interface is present.

--*/

{

    PINTERFACE_PSEUDO_RANDOM_SOURCE Interface;

    Interface = KePseudoRandomInterface;
    if (Interface == NULL) {
        return STATUS_NO_SUCH_DEVICE;
    }

    Interface->GetBytes(Interface, Buffer, Size);
    return STATUS_SUCCESS;
}

KSTATUS
KepInitializeEntropy (
    VOID
    )

/*++

Routine Description:

    This routine initializes the kernel's entropy support. It signs up for
    a pseudo-random generator source.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    if (KeDisableEntropyGathering == FALSE) {
        Status = IoRegisterForInterfaceNotifications(
                                              &KePseudoRandomInterfaceUuid,
                                              KepPseudoRandomInterfaceCallback,
                                              NULL,
                                              NULL,
                                              TRUE);

        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

VOID
KepAddTimePointEntropy (
    VOID
    )

/*++

Routine Description:

    This routine adds entropy in the form of a timestamp to the pseudo random
    interface, if one exists.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PINTERFACE_PSEUDO_RANDOM_SOURCE Interface;

    Interface = KePseudoRandomInterface;
    if (Interface != NULL) {
        Interface->AddTimePointEntropy(Interface);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KepPseudoRandomInterfaceCallback (
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

    ASSERT(InterfaceBufferSize == sizeof(INTERFACE_PSEUDO_RANDOM_SOURCE));

    if (Arrival != FALSE) {
        if (KePseudoRandomInterface == NULL) {
            KePseudoRandomInterface = InterfaceBuffer;
        }

    } else {
        if (InterfaceBuffer == KePseudoRandomInterface) {

            //
            // Pseudo-random interfaces aren't really expected to disappear.
            // This operation is not entirely safe, as there is no
            // synchronization with other processors that might be about to
            // use the interface.
            //

            ASSERT(FALSE);

            KePseudoRandomInterface = NULL;
        }
    }

    return;
}

