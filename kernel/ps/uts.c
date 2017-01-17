/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    uts.c

Abstract:

    This module implements support for handling the UTS realm, which manages
    the system hostname and domain name.

Author:

    Evan Green 16-Jan-2016

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include "psp.h"

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

    This structure stores information about a group of processes that
    interact with their controlling terminal as a unit.

Members:

    ListEntry - Stores pointers to the next and previous process groups in the
        global list.

    ReferenceCount - Stores the number of outstanding references to this
        process group. This structure will be automatically destroyed when the
        reference count hits zero.

    Identifier - Stores the identifier for this process group, which is usually
        the identifier of the process that created the process group.

    ProcessListHead - Stores the head of the list of processes in the group.

    SessionId - Stores the session identifier the process group belongs to.

    OutsideParents - Stores the number of processes with living parents outside
        the process group. When the number of processes in a process group with
        parents outside the process group (but inside the session) drops to
        zero, the process is considered orphaned as there is no one to do job
        control on it.

--*/

struct _UTS_REALM {
    volatile UINTN ReferenceCount;
    CHAR HostName[UTS_NAME_MAX + 1];
    CHAR DomainName[UTS_NAME_MAX + 1];
};

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
PspUtsDestroyRealm (
    PUTS_REALM Realm
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store the root realm.
//

UTS_REALM PsUtsRootRealm;

//
// This is a single global lock that serializes all access to any UTS name.
//

PQUEUED_LOCK PsUtsLock;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
PspInitializeUtsRealm (
    PKPROCESS KernelProcess
    )

/*++

Routine Description:

    This routine initializes the UTS realm space as the kernel process is
    coming online.

Arguments:

    KernelProcess - Supplies a pointer to the kernel process.

Return Value:

    Status code.

--*/

{

    PsUtsLock = KeCreateQueuedLock();
    if (PsUtsLock == NULL) {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    PsUtsRootRealm.ReferenceCount = 1;
    KernelProcess->Realm.Uts = &PsUtsRootRealm;
    return STATUS_SUCCESS;
}

PUTS_REALM
PspCreateUtsRealm (
    PUTS_REALM Source
    )

/*++

Routine Description:

    This routine creates a new UTS realm.

Arguments:

    Source - Supplies a pointer to the realm to copy from.

Return Value:

    Returns a pointer to a new realm with a single reference on success.

    NULL on allocation failure.

--*/

{

    PUTS_REALM NewRealm;

    NewRealm = MmAllocatePagedPool(sizeof(UTS_REALM), PS_UTS_ALLOCATION_TAG);
    if (NewRealm == NULL) {
        return NULL;
    }

    RtlCopyMemory(NewRealm, Source, sizeof(UTS_REALM));
    NewRealm->ReferenceCount = 1;
    RtlMemoryBarrier();
    return NewRealm;
}

KSTATUS
PspGetSetUtsInformation (
    BOOL FromKernelMode,
    PS_INFORMATION_TYPE InformationType,
    PVOID Data,
    PUINTN DataSize,
    BOOL Set
    )

/*++

Routine Description:

    This routine gets or sets process informaiton related to the host or
    domain name.

Arguments:

    FromKernelMode - Supplies a boolean indicating whether or not this request
        (and the buffer associated with it) originates from user mode (FALSE)
        or kernel mode (TRUE).

    InformationType - Supplies the information type.

    Data - Supplies a pointer to the data buffer where the data is either
        returned for a get operation or given for a set operation.

    DataSize - Supplies a pointer that on input contains the size of the
        data buffer. On output, contains the required size of the data buffer.

    Set - Supplies a boolean indicating if this is a get operation (FALSE) or
        a set operation (TRUE).

Return Value:

    Status code.

--*/

{

    UINTN Length;
    PKPROCESS Process;
    KSTATUS Status;
    PUTS_REALM Uts;
    UINTN UtsLength;
    PSTR UtsString;

    Process = PsGetCurrentProcess();
    Uts = Process->Realm.Uts;
    if (InformationType == PsInformationHostName) {
        UtsString = Uts->HostName;

    } else {

        ASSERT(InformationType == PsInformationDomainName);

        UtsString = Uts->DomainName;
    }

    //
    // Even when from user mode, the buffer is expected to be in kernel mode.
    //

    ASSERT(Data >= KERNEL_VA_START);

    KeAcquireQueuedLock(PsUtsLock);

    //
    // Set the host or domain name. The caller must have the system
    // administrator permission to change the host name.
    //

    if (Set != FALSE) {
        Length = *DataSize;
        if (Length > UTS_NAME_MAX) {
            Length = UTS_NAME_MAX;
        }

        *DataSize = Length;
        Status = STATUS_SUCCESS;
        if (FromKernelMode == FALSE) {
            Status = PsCheckPermission(PERMISSION_SYSTEM_ADMINISTRATOR);
        }

        if (KSUCCESS(Status)) {
            RtlCopyMemory(UtsString, Data, Length);
            UtsString[Length] = '\0';
        }

    //
    // Get the host or domain name.
    //

    } else {
        Status = STATUS_SUCCESS;
        UtsLength = RtlStringLength(UtsString) + 1;
        Length = UtsLength;
        if (Length > *DataSize) {
            Length = *DataSize;
            Status = STATUS_BUFFER_TOO_SMALL;
        }

        *DataSize = UtsLength;
        RtlCopyMemory(Data, UtsString, Length);
    }

    KeReleaseQueuedLock(PsUtsLock);
    return Status;
}

VOID
PspUtsRealmAddReference (
    PUTS_REALM Realm
    )

/*++

Routine Description:

    This routine adds a reference to the given UTS realm.

Arguments:

    Realm - Supplies a pointer to the realm.

Return Value:

    None.

--*/

{

    //
    // Save all the heavy atomic operations if this is the root realm, which
    // will never go away because it's used by the kernel.
    //

    if (Realm != &PsUtsRootRealm) {
        RtlAtomicAdd(&(Realm->ReferenceCount), 1);
    }

    return;
}

VOID
PspUtsRealmReleaseReference (
    PUTS_REALM Realm
    )

/*++

Routine Description:

    This routine releases a reference to the given UTS realm. If the reference
    count drops to zero, the realm will be destroyed.

Arguments:

    Realm - Supplies a pointer to the realm.

Return Value:

    None.

--*/

{

    UINTN Previous;

    //
    // Save all the heavy atomic operations if this is the root realm, which
    // will never go away because it's used by the kernel.
    //

    if (Realm != &PsUtsRootRealm) {
        Previous = RtlAtomicAdd(&(Realm->ReferenceCount), (UINTN)-1L);
        if (Previous == 1) {
            PspUtsDestroyRealm(Realm);
        }
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
PspUtsDestroyRealm (
    PUTS_REALM Realm
    )

/*++

Routine Description:

    This routine destroys a UTS realm.

Arguments:

    Realm - Supplies a pointer to the realm.

Return Value:

    None.

--*/

{

    MmFreePagedPool(Realm);
    return;
}

