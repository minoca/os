/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    util.c

Abstract:

    This module implements miscellaneous functionality for the UEFI core.

Author:

    Evan Green 28-Feb-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/kdebug.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
VOID
EfiCoreEmptyCallbackFunction (
    EFI_EVENT Event,
    VOID *Context
    )

/*++

Routine Description:

    This routine does nothing but return. It conforms to the event notification
    function prototype.

Arguments:

    Event - Supplies an unused event.

    Context - Supplies an unused context pointer.

Return Value:

    None.

--*/

{

    return;
}

EFIAPI
VOID
EfiCoreCopyMemory (
    VOID *Destination,
    VOID *Source,
    UINTN Length
    )

/*++

Routine Description:

    This routine copies the contents of one buffer to another.

Arguments:

    Destination - Supplies a pointer to the destination of the copy.

    Source - Supplies a pointer to the source of the copy.

    Length - Supplies the number of bytes to copy.

Return Value:

    None.

--*/

{

    ASSERT((Destination != NULL) && (Source != NULL));

    while (Length != 0) {
        *(INT8 *)Destination = *(INT8 *)Source;
        Length -= 1;
        Destination += 1;
        Source += 1;
    }

    return;
}

EFIAPI
VOID
EfiCoreSetMemory (
    VOID *Buffer,
    UINTN Size,
    UINT8 Value
    )

/*++

Routine Description:

    This routine fills a buffer with a specified value.

Arguments:

    Buffer - Supplies a pointer to the buffer to fill.

    Size - Supplies the size of the buffer in bytes.

    Value - Supplies the value to fill the buffer with.

Return Value:

    None.

--*/

{

    UINT8 *Bytes;
    UINTN Index;

    Bytes = Buffer;
    for (Index = 0; Index < Size; Index += 1) {
        Bytes[Index] = Value;
    }

    return;
}

INTN
EfiCoreCompareMemory (
    VOID *FirstBuffer,
    VOID *SecondBuffer,
    UINTN Length
    )

/*++

Routine Description:

    This routine compares the contents of two buffers for equality.

Arguments:

    FirstBuffer - Supplies a pointer to the first buffer to compare.

    SecondBuffer - Supplies a pointer to the second buffer to compare.

    Length - Supplies the number of bytes to compare.

Return Value:

    0 if the buffers are identical.

    Returns the first mismatched byte as
    First[MismatchIndex] - Second[MismatchIndex].

--*/

{

    INTN Result;

    ASSERT((FirstBuffer != NULL) && (SecondBuffer != NULL));

    while (Length != 0) {
        if (*(INT8 *)FirstBuffer != *(INT8 *)SecondBuffer) {
            Result = (INTN)(UINT8 *)FirstBuffer - (INTN)(UINT8 *)SecondBuffer;
            return Result;
        }

        Length -= 1;
        FirstBuffer += 1;
        SecondBuffer += 1;
    }

    return 0;
}

BOOLEAN
EfiCoreCompareGuids (
    EFI_GUID *FirstGuid,
    EFI_GUID *SecondGuid
    )

/*++

Routine Description:

    This routine compares two GUIDs.

Arguments:

    FirstGuid - Supplies a pointer to the first GUID.

    SecondGuid - Supplies a pointer to the second GUID.

Return Value:

    TRUE if the GUIDs are equal.

    FALSE if the GUIDs are different.

--*/

{

    UINT32 *FirstPointer;
    UINT32 *SecondPointer;

    //
    // Compare GUIDs 32 bits at a time.
    //

    FirstPointer = (UINT32 *)FirstGuid;
    SecondPointer = (UINT32 *)SecondGuid;
    if ((FirstPointer[0] == SecondPointer[0]) &&
        (FirstPointer[1] == SecondPointer[1]) &&
        (FirstPointer[2] == SecondPointer[2]) &&
        (FirstPointer[3] == SecondPointer[3])) {

        return TRUE;
    }

    return FALSE;
}

VOID *
EfiCoreAllocateBootPool (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates pool from boot services data.

Arguments:

    Size - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure.

--*/

{

    VOID *Allocation;
    EFI_STATUS Status;

    Status = EfiAllocatePool(EfiBootServicesData, Size, &Allocation);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    return Allocation;
}

VOID *
EfiCoreAllocateRuntimePool (
    UINTN Size
    )

/*++

Routine Description:

    This routine allocates pool from runtime services data.

Arguments:

    Size - Supplies the size of the allocation in bytes.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure.

--*/

{

    VOID *Allocation;
    EFI_STATUS Status;

    Status = EfiAllocatePool(EfiRuntimeServicesData, Size, &Allocation);
    if (EFI_ERROR(Status)) {
        return NULL;
    }

    return Allocation;
}

INTN
EfiCoreFindHighBitSet64 (
    UINT64 Value
    )

/*++

Routine Description:

    This routine returns the bit position of the highest bit set in a 64-bit
    value.

Arguments:

    Value - Supplies the input value.

Return Value:

    Returns the index of the highest bit set, between 0 and 63. If the value is
    zero, then -1 is returned.

--*/

{

    if (Value == (UINT32)Value) {
        return EfiCoreFindHighBitSet32((UINT32)Value);
    }

    return EfiCoreFindHighBitSet32((UINT32)(Value >> 32)) + 32;
}

INTN
EfiCoreFindHighBitSet32 (
    UINT32 Value
    )

/*++

Routine Description:

    This routine returns the bit position of the highest bit set in a 32-bit
    value.

Arguments:

    Value - Supplies the input value.

Return Value:

    Returns the index of the highest bit set, between 0 and 31. If the value is
    zero, then -1 is returned.

--*/

{

    INTN BitIndex;

    if (Value == 0) {
        return -1;
    }

    for (BitIndex = 31; BitIndex >= 0; BitIndex -= 1) {
        if ((INT32)Value < 0) {
            break;
        }

        Value <<= 1;
    }

    return BitIndex;
}

VOID
EfiCoreCalculateTableCrc32 (
    EFI_TABLE_HEADER *Header
    )

/*++

Routine Description:

    This routine recalculates the CRC32 of a given EFI table.

Arguments:

    Header - Supplies a pointer to the header. The size member will be used to
        determine the size of the entire table.

Return Value:

    None. The CRC is set in the header.

--*/

{

    UINT32 Crc;

    Header->CRC32 = 0;
    Crc = 0;

    //
    // This boot service may be "not yet implemented", in which case CRC comes
    // back staying zero. This will presumably be filled in correctly and
    // reapplied later.
    //

    EfiCalculateCrc32((UINT8 *)Header, Header->HeaderSize, &Crc);
    Header->CRC32 = Crc;
}

EFIAPI
EFI_EVENT
EfiCoreCreateProtocolNotifyEvent (
    EFI_GUID *ProtocolGuid,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    VOID **Registration
    )

/*++

Routine Description:

    This routine creates an event, then registers that event to be notified
    whenever the given protocol appears. Finally, it signals the event so that
    any pre-existing protocols will be found.

Arguments:

    ProtocolGuid - Supplies a pointer to the GUID of the protocol to watch.

    NotifyTpl - Supplies the Task Priority Level of the callback function.

    NotifyFunction - Supplies a pointer to the function to call when a new
        protocol with the given GUID crops up.

    NotifyContext - Supplies a pointer to pass into the notify function.

    Registration - Supplies a pointer where the registration token for the
        event will be returned.

Return Value:

    Returns the notification event that was created.

    NULL on failure.

--*/

{

    EFI_EVENT Event;
    EFI_STATUS Status;

    ASSERT((ProtocolGuid != NULL) && (NotifyFunction != NULL) &&
           (Registration != NULL));

    Status = EfiCreateEvent(EVT_NOTIFY_SIGNAL,
                            NotifyTpl,
                            NotifyFunction,
                            NotifyContext,
                            &Event);

    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        return NULL;
    }

    //
    // Register for protocol notifications on the event just created.
    //

    Status = EfiRegisterProtocolNotify(ProtocolGuid, Event, Registration);
    if (EFI_ERROR(Status)) {

        ASSERT(FALSE);

        EfiCloseEvent(Event);
        return NULL;
    }

    //
    // Kick the event so that pre-existing protocol instances will be
    // discovered.
    //

    EfiSignalEvent(Event);
    return Event;
}

UINTN
EfiCoreStringLength (
    CHAR16 *String
    )

/*++

Routine Description:

    This routine returns the length of the given string, in characters (not
    bytes).

Arguments:

    String - Supplies a pointer to the string.

Return Value:

    Returns the number of characters in the string.

--*/

{

    UINTN Length;

    ASSERT(String != NULL);

    if (String == NULL) {
        return 0;
    }

    Length = 0;
    while (*String != L'\0') {
        String += 1;
        Length += 1;
    }

    return Length;
}

VOID
EfiCoreCopyString (
    CHAR16 *Destination,
    CHAR16 *Source
    )

/*++

Routine Description:

    This routine copies one string over to another buffer.

Arguments:

    Destination - Supplies a pointer to the destination buffer where the
        string will be copied to.

    Source - Supplies a pointer to the string to copy.

Return Value:

    None.

--*/

{

    if ((Destination == NULL) || (Source == NULL)) {

        ASSERT(FALSE);

        return;
    }

    while (*Source != L'\0') {
        *Destination = *Source;
        Destination += 1;
        Source += 1;
    }

    *Destination = L'\0';
    return;
}

EFI_TPL
EfiCoreGetCurrentTpl (
    VOID
    )

/*++

Routine Description:

    This routine returns the current TPL.

Arguments:

    None.

Return Value:

    Returns the current TPL.

--*/

{

    EFI_TPL Tpl;

    Tpl = EfiRaiseTPL(TPL_HIGH_LEVEL);
    EfiRestoreTPL(Tpl);
    return Tpl;
}

VOID
EfiDebugPrint (
    CHAR8 *Format,
    ...
    )

/*++

Routine Description:

    This routine prints to the debugger and console.

Arguments:

    Format - Supplies a pointer to the format string.

    ... - Supplies the remaining arguments to the format string.

Return Value:

    None.

--*/

{

    va_list ArgumentList;
    CHAR Ascii[128];
    ULONG Index;
    USHORT Wide[128];

    //
    // Simply pass the data on to the debugger's print function.
    //

    va_start(ArgumentList, Format);
    KdPrintWithArgumentList(Format, ArgumentList);
    va_end(ArgumentList);
    if (EfiSystemTable->StdErr != NULL) {
        va_start(ArgumentList, Format);
        RtlFormatString(Ascii,
                        sizeof(Ascii) - 1,
                        CharacterEncodingAscii,
                        Format,
                        ArgumentList);

        Index = 0;
        while (Ascii[Index] != '\0') {
            Wide[Index] = Ascii[Index];
            Index += 1;
        }

        Wide[Index] = L'\0';
        va_end(ArgumentList);
        EfiSystemTable->StdErr->OutputString(EfiSystemTable->StdErr, Wide);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

