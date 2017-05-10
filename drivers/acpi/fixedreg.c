/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    fixedreg.c

Abstract:

    This module implements support for accessing ACPI fixed hardware, which
    is a mess because there are so many different ways to both access and
    specify the register locations.

Author:

    Evan Green 21-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "amlos.h"
#include "fixedreg.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the number of seconds to wait for the global lock before taking
// the system down.
//

#define ACPI_GLOBAL_LOCK_TIMEOUT 60

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
AcpipReadFixedRegister (
    ULONG AddressA,
    ULONG AddressB,
    UCHAR AddressLength,
    ULONG GenericAddressAOffset,
    ULONG GenericAddressBOffset,
    PVOID *MappedAddressA,
    PVOID *MappedAddressB,
    PULONG MappedSizeA,
    PULONG MappedSizeB,
    UINTN Offset,
    PULONG Value
    );

KSTATUS
AcpipWriteFixedRegister (
    ULONG AddressA,
    ULONG AddressB,
    UCHAR AddressLength,
    ULONG GenericAddressAOffset,
    ULONG GenericAddressBOffset,
    PVOID *MappedAddressA,
    PVOID *MappedAddressB,
    PULONG MappedSizeA,
    PULONG MappedSizeB,
    UINTN Offset,
    ULONG Value
    );

KSTATUS
AcpipReadGenericAddressFixedRegister (
    PVOID *MappedAddress,
    PULONG MappedSize,
    PGENERIC_ADDRESS GenericAddress,
    UINTN Offset,
    PULONG Value
    );

KSTATUS
AcpipWriteGenericAddressFixedRegister (
    PVOID *MappedAddress,
    PULONG MappedSize,
    PGENERIC_ADDRESS GenericAddress,
    UINTN Offset,
    ULONG Value
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the PM1 control block.
//

PVOID AcpiPm1aControlRegister = NULL;
ULONG AcpiPm1aControlRegisterSize = 0;
PVOID AcpiPm1bControlRegister = NULL;
ULONG AcpiPm1bControlRegisterSize = 0;

PVOID AcpiPm2ControlRegister = NULL;
ULONG AcpiPm2ControlRegisterSize = 0;

PVOID AcpiPm1aEventRegister = NULL;
ULONG AcpiPm1aEventRegisterSize = 0;
PVOID AcpiPm1bEventRegister = NULL;
ULONG AcpiPm1bEventRegisterSize = 0;

//
// Store a pointer to the lock that protects the global lock.
//

PQUEUED_LOCK AcpiGlobalLock = NULL;

//
// Store a pointer to the FACS table.
//

PFACS AcpiFacsTable = NULL;

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipReadPm1ControlRegister (
    PULONG Value
    )

/*++

Routine Description:

    This routine reads the PM1 control register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipReadFixedRegister(Fadt->Pm1aControlBlock,
                                    Fadt->Pm1bControlBlock,
                                    Fadt->Pm1ControlLength,
                                    FIELD_OFFSET(FADT, XPm1aControlBlock),
                                    FIELD_OFFSET(FADT, XPm1bControlBlock),
                                    &AcpiPm1aControlRegister,
                                    &AcpiPm1bControlRegister,
                                    &AcpiPm1aControlRegisterSize,
                                    &AcpiPm1bControlRegisterSize,
                                    0,
                                    Value);

    return Status;
}

KSTATUS
AcpipWritePm1ControlRegister (
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to the PM1 control register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipWriteFixedRegister(Fadt->Pm1aControlBlock,
                                     Fadt->Pm1bControlBlock,
                                     Fadt->Pm1ControlLength,
                                     FIELD_OFFSET(FADT, XPm1aControlBlock),
                                     FIELD_OFFSET(FADT, XPm1bControlBlock),
                                     &AcpiPm1aControlRegister,
                                     &AcpiPm1bControlRegister,
                                     &AcpiPm1aControlRegisterSize,
                                     &AcpiPm1bControlRegisterSize,
                                     0,
                                     Value);

    return Status;
}

KSTATUS
AcpipReadPm2ControlRegister (
    PULONG Value
    )

/*++

Routine Description:

    This routine reads the PM2 control register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipReadFixedRegister(Fadt->Pm2ControlBlock,
                                    0,
                                    Fadt->Pm2ControlLength,
                                    FIELD_OFFSET(FADT, XPm2ControlBlock),
                                    0,
                                    &AcpiPm2ControlRegister,
                                    NULL,
                                    &AcpiPm2ControlRegisterSize,
                                    NULL,
                                    0,
                                    Value);

    return Status;
}

KSTATUS
AcpipWritePm2ControlRegister (
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to the PM2 control register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipWriteFixedRegister(Fadt->Pm2ControlBlock,
                                     0,
                                     Fadt->Pm2ControlLength,
                                     FIELD_OFFSET(FADT, XPm2ControlBlock),
                                     0,
                                     &AcpiPm2ControlRegister,
                                     NULL,
                                     &AcpiPm2ControlRegisterSize,
                                     NULL,
                                     0,
                                     Value);

    return Status;
}

KSTATUS
AcpipReadPm1EventRegister (
    PULONG Value
    )

/*++

Routine Description:

    This routine reads the PM1 event/status register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipReadFixedRegister(Fadt->Pm1aEventBlock,
                                    Fadt->Pm1bEventBlock,
                                    Fadt->Pm1EventLength / 2,
                                    FIELD_OFFSET(FADT, XPm1aEventBlock),
                                    FIELD_OFFSET(FADT, XPm1bEventBlock),
                                    &AcpiPm1aEventRegister,
                                    &AcpiPm1bEventRegister,
                                    &AcpiPm1aEventRegisterSize,
                                    &AcpiPm1bEventRegisterSize,
                                    0,
                                    Value);

    return Status;
}

KSTATUS
AcpipWritePm1EventRegister (
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to the PM1 event register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    Status = AcpipWriteFixedRegister(Fadt->Pm1aEventBlock,
                                     Fadt->Pm1bEventBlock,
                                     Fadt->Pm1EventLength / 2,
                                     FIELD_OFFSET(FADT, XPm1aEventBlock),
                                     FIELD_OFFSET(FADT, XPm1bEventBlock),
                                     &AcpiPm1aEventRegister,
                                     &AcpiPm1bEventRegister,
                                     &AcpiPm1aEventRegisterSize,
                                     &AcpiPm1bEventRegisterSize,
                                     0,
                                     Value);

    return Status;
}

KSTATUS
AcpipReadPm1EnableRegister (
    PULONG Value
    )

/*++

Routine Description:

    This routine reads the PM1 enable register.

Arguments:

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    ULONG HalfLength;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    HalfLength = Fadt->Pm1EventLength / 2;
    Status = AcpipReadFixedRegister(Fadt->Pm1aEventBlock,
                                    Fadt->Pm1bEventBlock,
                                    HalfLength,
                                    FIELD_OFFSET(FADT, XPm1aEventBlock),
                                    FIELD_OFFSET(FADT, XPm1bEventBlock),
                                    &AcpiPm1aEventRegister,
                                    &AcpiPm1bEventRegister,
                                    &AcpiPm1aEventRegisterSize,
                                    &AcpiPm1bEventRegisterSize,
                                    HalfLength,
                                    Value);

    return Status;
}

KSTATUS
AcpipWritePm1EnableRegister (
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to the PM1 enable register.

Arguments:

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    ULONG HalfLength;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    HalfLength = Fadt->Pm1EventLength / 2;
    Status = AcpipWriteFixedRegister(Fadt->Pm1aEventBlock,
                                     Fadt->Pm1bEventBlock,
                                     HalfLength,
                                     FIELD_OFFSET(FADT, XPm1aEventBlock),
                                     FIELD_OFFSET(FADT, XPm1bEventBlock),
                                     &AcpiPm1aEventRegister,
                                     &AcpiPm1bEventRegister,
                                     &AcpiPm1aEventRegisterSize,
                                     &AcpiPm1bEventRegisterSize,
                                     HalfLength,
                                     Value);

    return Status;
}

VOID
AcpipAcquireGlobalLock (
    VOID
    )

/*++

Routine Description:

    This routine acquires the ACPI global lock that coordinates between the
    OSPM and firmware in SMI-land (or in some external controller).

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG CurrentValue;
    ULONG NewValue;
    ULONG OriginalValue;
    KSTATUS Status;
    ULONGLONG Timeout;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (AcpiFacsTable == NULL) {
        return;
    }

    KeAcquireQueuedLock(AcpiGlobalLock);
    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * ACPI_GLOBAL_LOCK_TIMEOUT);

    //
    // Loop trying to acquire the lock until the timeout occurs.
    //

    Status = STATUS_TIMEOUT;
    do {

        //
        // Loop trying to get a clean compare-exchange.
        //

        while (TRUE) {
            OriginalValue = AcpiFacsTable->GlobalLock;

            //
            // Clear the pending bit. If the owner bit is set, set the pending
            // bit.
            //

            NewValue = (OriginalValue | FACS_GLOBAL_LOCK_OWNED) &
                       (~FACS_GLOBAL_LOCK_PENDING);

            if ((OriginalValue & FACS_GLOBAL_LOCK_OWNED) != 0) {
                NewValue |= FACS_GLOBAL_LOCK_PENDING;
            }

            CurrentValue = RtlAtomicCompareExchange32(
                                                  &(AcpiFacsTable->GlobalLock),
                                                  NewValue,
                                                  OriginalValue);

            if (CurrentValue == OriginalValue) {
                break;
            }
        }

        //
        // If the value shoved in there didn't have the pending bit set, then
        // this routine must have just set the owner bit successfully.
        //

        if ((NewValue & FACS_GLOBAL_LOCK_PENDING) == 0) {
            Status = STATUS_SUCCESS;
            break;
        }

        //
        // Stall for a bit to let the firmware do its thing.
        //

        KeDelayExecution(FALSE, FALSE, MICROSECONDS_PER_MILLISECOND);

    } while (KeGetRecentTimeCounter() <= Timeout);

    //
    // It's serious not to be able to acquire the lock.
    //

    if (!KSUCCESS(Status)) {
        AcpipFatalError(ACPI_CRASH_GLOBAL_LOCK_FAILURE,
                        Status,
                        (UINTN)AcpiFacsTable,
                        0);
    }

    return;
}

VOID
AcpipReleaseGlobalLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the ACPI global lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG CurrentValue;
    ULONG NewValue;
    ULONG OriginalValue;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    if (AcpiFacsTable == NULL) {
        return;
    }

    //
    // Loop trying to get a clean compare exchange clearing the owned and
    // pending bits.
    //

    while (TRUE) {
        OriginalValue = AcpiFacsTable->GlobalLock;
        NewValue = OriginalValue &
                   (~(FACS_GLOBAL_LOCK_PENDING | FACS_GLOBAL_LOCK_OWNED));

        CurrentValue = RtlAtomicCompareExchange32(&(AcpiFacsTable->GlobalLock),
                                                  NewValue,
                                                  OriginalValue);

        if (CurrentValue == OriginalValue) {
            break;
        }
    }

    //
    // If the firmware wants control, signal to them that it's their turn.
    //

    if ((OriginalValue & FACS_GLOBAL_LOCK_PENDING) != 0) {
        Status = AcpipReadPm1ControlRegister(&OriginalValue);
        if (KSUCCESS(Status)) {
            OriginalValue |= FADT_PM1_CONTROL_GLOBAL_LOCK_RELEASED;
            AcpipWritePm1ControlRegister(OriginalValue);
        }
    }

    KeReleaseQueuedLock(AcpiGlobalLock);
    return;
}

KSTATUS
AcpipInitializeFixedRegisterSupport (
    VOID
    )

/*++

Routine Description:

    This routine initializes support for accessing fixed registers.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PHYSICAL_ADDRESS FacsPhysicalAddress;
    PFADT Fadt;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt != NULL) {

        //
        // Get the physical address of the FACS table.
        //

        FacsPhysicalAddress = 0;
        if (Fadt->Header.Length >=
            FIELD_OFFSET(FADT, XFirmwareControl) + sizeof(ULONGLONG)) {

            FacsPhysicalAddress = Fadt->XFirmwareControl;
        }

        if (FacsPhysicalAddress == 0) {
            FacsPhysicalAddress = Fadt->FirmwareControlAddress;
        }

        //
        // Map the FACS if it's present. Map it cache-disable as it
        // communicates directly to firmware.
        //

        if (FacsPhysicalAddress != 0) {
            AcpiFacsTable = MmMapPhysicalAddress(FacsPhysicalAddress,
                                                 sizeof(FACS),
                                                 TRUE,
                                                 FALSE,
                                                 TRUE);

            if (AcpiFacsTable == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeFixedRegisterSupportEnd;
            }

            //
            // Also create a global lock to protect the global lock.
            //

            AcpiGlobalLock = KeCreateQueuedLock();
            if (AcpiGlobalLock == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto InitializeFixedRegisterSupportEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

InitializeFixedRegisterSupportEnd:
    return Status;
}

VOID
AcpipUnmapFixedRegisters (
    VOID
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    It unmaps any mappings created to access the fixed ACPI registers.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if (AcpiGlobalLock != NULL) {
        KeDestroyQueuedLock(AcpiGlobalLock);
    }

    //
    // Unmap the fixed hardware.
    //

    if (AcpiPm1aControlRegister != NULL) {
        MmUnmapAddress(AcpiPm1aControlRegister, AcpiPm1aControlRegisterSize);
        AcpiPm1aControlRegister = NULL;
        AcpiPm1aControlRegisterSize = 0;
    }

    if (AcpiPm1bControlRegister != NULL) {
        MmUnmapAddress(AcpiPm1bControlRegister, AcpiPm1bControlRegisterSize);
        AcpiPm1bControlRegister = NULL;
        AcpiPm1bControlRegisterSize = 0;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
AcpipReadFixedRegister (
    ULONG AddressA,
    ULONG AddressB,
    UCHAR AddressLength,
    ULONG GenericAddressAOffset,
    ULONG GenericAddressBOffset,
    PVOID *MappedAddressA,
    PVOID *MappedAddressB,
    PULONG MappedSizeA,
    PULONG MappedSizeB,
    UINTN Offset,
    PULONG Value
    )

/*++

Routine Description:

    This routine reads a fixed register from the FADT.

Arguments:

    AddressA - Supplies the I/O port address of the A (or only) portion of
        the register.

    AddressB - Supplies the I/O port address of the B portion of the register
        if it exists, or 0 if there is only an A portion.

    AddressLength - Supplies the length of the I/O port address in bytes.

    GenericAddressAOffset - Supplies the offset into the FADT where the
        generic address of the A portion resides, or 0 if there is no
        generic address for this register in the FADT.

    GenericAddressBOffset - Supplies the offset into the FADT where the
        generic address of the B portion resides, or 0 if there is no B portion.

    MappedAddressA - Supplies a pointer that on input contains the mapped
        address of the register A for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedAddressB - Supplies a pointer that on input contains the mapped
        address of the register B for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedSizeA - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    MappedSizeB - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    Offset - Supplies the offset in bytes from the register base to read.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    PGENERIC_ADDRESS GenericAddressA;
    PGENERIC_ADDRESS GenericAddressB;
    ULONG MaxOffset;
    KSTATUS Status;
    ULONG ValueA;
    ULONG ValueB;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    ValueA = 0;
    ValueB = 0;

    //
    // Use the extended value if it's there.
    //

    GenericAddressA = (PVOID)Fadt + GenericAddressAOffset;
    GenericAddressB = (PVOID)Fadt + GenericAddressBOffset;
    MaxOffset = GenericAddressAOffset;
    if (GenericAddressBOffset > GenericAddressAOffset) {
        MaxOffset = GenericAddressBOffset;
    }

    MaxOffset += sizeof(GENERIC_ADDRESS);
    if ((Fadt->Header.Length >= MaxOffset) &&
        (GenericAddressA->Address != 0)) {

        Status = AcpipReadGenericAddressFixedRegister(MappedAddressA,
                                                      MappedSizeA,
                                                      GenericAddressA,
                                                      Offset,
                                                      &ValueA);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        if (GenericAddressB->Address != 0) {
            Status = AcpipReadGenericAddressFixedRegister(MappedAddressB,
                                                          MappedSizeB,
                                                          GenericAddressB,
                                                          Offset,
                                                          &ValueB);

            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        *Value = ValueA | ValueB;
        return STATUS_SUCCESS;
    }

    //
    // Use the old fashioned values.
    //

    if (AddressLength == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    ASSERT(AddressA != 0);

    switch (AddressLength) {
    case 1:
        ValueA = HlIoPortInByte(AddressA + Offset);
        if (AddressB != 0) {
            ValueB = HlIoPortInByte(AddressB + Offset);
        }

        break;

    case 2:
        ValueA = HlIoPortInShort(AddressA + Offset);
        if (AddressB != 0) {
            ValueB = HlIoPortInShort(AddressB + Offset);
        }

        break;

    case 4:
        ValueA = HlIoPortInLong(AddressA + Offset);
        if (AddressB != 0) {
            ValueB = HlIoPortInLong(AddressB + Offset);
        }

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    *Value = ValueA | ValueB;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipWriteFixedRegister (
    ULONG AddressA,
    ULONG AddressB,
    UCHAR AddressLength,
    ULONG GenericAddressAOffset,
    ULONG GenericAddressBOffset,
    PVOID *MappedAddressA,
    PVOID *MappedAddressB,
    PULONG MappedSizeA,
    PULONG MappedSizeB,
    UINTN Offset,
    ULONG Value
    )

/*++

Routine Description:

    This routine writes to a fixed register from the FADT.

Arguments:

    AddressA - Supplies the I/O port address of the A (or only) portion of
        the register.

    AddressB - Supplies the I/O port address of the B portion of the register
        if it exists, or 0 if there is only an A portion.

    AddressLength - Supplies the length of the I/O port address in bytes.

    GenericAddressAOffset - Supplies the offset into the FADT where the
        generic address of the A portion resides, or 0 if there is no
        generic address for this register in the FADT.

    GenericAddressBOffset - Supplies the offset into the FADT where the
        generic address of the B portion resides, or 0 if there is no B portion.

    MappedAddressA - Supplies a pointer that on input contains the mapped
        address of the register A for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedAddressB - Supplies a pointer that on input contains the mapped
        address of the register B for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedSizeA - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    MappedSizeB - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    Offset - Supplies the offset in bytes to write to.

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    PFADT Fadt;
    PGENERIC_ADDRESS GenericAddressA;
    PGENERIC_ADDRESS GenericAddressB;
    ULONG MaxOffset;
    KSTATUS Status;

    Fadt = AcpiFadtTable;
    if (Fadt == NULL) {
        return STATUS_NOT_SUPPORTED;
    }

    //
    // Use the extended value if it's there.
    //

    GenericAddressA = (PVOID)Fadt + GenericAddressAOffset;
    GenericAddressB = (PVOID)Fadt + GenericAddressBOffset;
    MaxOffset = GenericAddressAOffset;
    if (GenericAddressBOffset > GenericAddressAOffset) {
        MaxOffset = GenericAddressBOffset;
    }

    MaxOffset += sizeof(GENERIC_ADDRESS);
    if ((Fadt->Header.Length >= MaxOffset) &&
        (GenericAddressA->Address != 0)) {

        Status = AcpipWriteGenericAddressFixedRegister(MappedAddressA,
                                                       MappedSizeA,
                                                       GenericAddressA,
                                                       Offset,
                                                       Value);

        if (!KSUCCESS(Status)) {
            return Status;
        }

        if (GenericAddressB->Address != 0) {
            Status = AcpipWriteGenericAddressFixedRegister(MappedAddressB,
                                                           MappedSizeB,
                                                           GenericAddressB,
                                                           Offset,
                                                           Value);

            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        return STATUS_SUCCESS;
    }

    //
    // Use the old fashioned values.
    //

    if (AddressLength == 0) {
        return STATUS_NOT_SUPPORTED;
    }

    ASSERT(AddressA != 0);

    switch (AddressLength) {
    case 1:
        HlIoPortOutByte(AddressA + Offset, (UCHAR)Value);
        if (AddressB != 0) {
            HlIoPortOutByte(AddressB + Offset, Value);
        }

        break;

    case 2:
        HlIoPortOutShort(AddressA + Offset, (USHORT)Value);
        if (AddressB != 0) {
            HlIoPortOutShort(AddressB + Offset, (USHORT)Value);
        }

        break;

    case 4:
        HlIoPortOutLong(AddressA + Offset, Value);
        if (AddressB != 0) {
            HlIoPortOutLong(AddressB + Offset, Value);
        }

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipReadGenericAddressFixedRegister (
    PVOID *MappedAddress,
    PULONG MappedSize,
    PGENERIC_ADDRESS GenericAddress,
    UINTN Offset,
    PULONG Value
    )

/*++

Routine Description:

    This routine reads a register value from a generic address.

Arguments:

    MappedAddress - Supplies a pointer that on input contains the mapped
        address of the register for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedSize - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    GenericAddress - Supplies a pointer to the generic address detailing the
        location of the register.

    Offset - Supplies an offset in bytes to read from.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    Status code.

--*/

{

    USHORT AccessSize;
    USHORT IoPortAddress;
    ULONG PageSize;

    //
    // Deal with odd offsets if needed.
    //

    ASSERT(GenericAddress->RegisterBitOffset == 0);

    AccessSize = GenericAddress->AccessSize;
    if (AccessSize == 0) {
        if (GenericAddress->Address == 0) {
            return STATUS_NOT_SUPPORTED;
        }

        ASSERT(GenericAddress->RegisterBitWidth != 0);

        AccessSize = GenericAddress->RegisterBitWidth / BITS_PER_BYTE;
    }

    switch (GenericAddress->AddressSpaceId) {
    case AddressSpaceMemory:
        if (*MappedAddress == NULL) {
            PageSize = MmPageSize();

            ASSERT(Offset + AccessSize <= PageSize);

            *MappedAddress = MmMapPhysicalAddress(GenericAddress->Address,
                                                  PageSize,
                                                  TRUE,
                                                  FALSE,
                                                  TRUE);

            if (*MappedAddress == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            *MappedSize = PageSize;
        }

        switch (AccessSize) {
        case 1:
            *Value = HlReadRegister8(*MappedAddress + Offset);
            break;

        case 2:
            *Value = HlReadRegister16(*MappedAddress + Offset);
            break;

        case 4:
            *Value = HlReadRegister32(*MappedAddress + Offset);
            break;

        default:

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        break;

    case AddressSpaceIo:
        IoPortAddress = (USHORT)(GenericAddress->Address);
        switch (AccessSize) {
        case 1:
            *Value = HlIoPortInByte(IoPortAddress + Offset);
            break;

        case 2:
            *Value = HlIoPortInShort(IoPortAddress + Offset);
            break;

        case 4:
            *Value = HlIoPortInLong(IoPortAddress + Offset);
            break;

        default:

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        break;

    //
    // Implement other types if needed.
    //

    default:

        ASSERT(FALSE);

        return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipWriteGenericAddressFixedRegister (
    PVOID *MappedAddress,
    PULONG MappedSize,
    PGENERIC_ADDRESS GenericAddress,
    UINTN Offset,
    ULONG Value
    )

/*++

Routine Description:

    This routine writes a register value to a generic address.

Arguments:

    MappedAddress - Supplies a pointer that on input contains the mapped
        address of the register for memory mapped addresses. If the value of
        this is NULL and the generic address is memory backed, it will be
        mapped in this routine and the virtual address stored here.

    MappedSize - Supplies a pointer that on input contains the size of the
        virtual mapping for this address. If this routine maps the register,
        it will fill in the size here.

    GenericAddress - Supplies a pointer to the generic address detailing the
        location of the register.

    Offset - Supplies the offset in bytes to write to.

    Value - Supplies the value to write.

Return Value:

    Status code.

--*/

{

    USHORT AccessSize;
    USHORT IoPortAddress;
    ULONG PageSize;

    //
    // Deal with odd offsets if needed.
    //

    ASSERT(GenericAddress->RegisterBitOffset == 0);

    AccessSize = GenericAddress->AccessSize;
    if (AccessSize == 0) {

        ASSERT(GenericAddress->RegisterBitWidth != 0);

        AccessSize = GenericAddress->RegisterBitWidth / BITS_PER_BYTE;

    } else {

        ASSERT(GenericAddress->RegisterBitWidth >=
               GenericAddress->AccessSize * BITS_PER_BYTE);
    }

    switch (GenericAddress->AddressSpaceId) {
    case AddressSpaceMemory:
        if (*MappedAddress == NULL) {
            PageSize = MmPageSize();

            ASSERT(Offset + GenericAddress->AccessSize <= PageSize);

            *MappedAddress = MmMapPhysicalAddress(GenericAddress->Address,
                                                  PageSize,
                                                  TRUE,
                                                  FALSE,
                                                  TRUE);

            if (*MappedAddress == NULL) {
                return STATUS_INSUFFICIENT_RESOURCES;
            }

            *MappedSize = PageSize;
        }

        switch (AccessSize) {
        case 1:
            HlWriteRegister8(*MappedAddress + Offset, (UCHAR)Value);
            break;

        case 2:
            HlWriteRegister16(*MappedAddress + Offset, (USHORT)Value);
            break;

        case 4:
            HlWriteRegister32(*MappedAddress + Offset, Value);
            break;

        default:

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        break;

    case AddressSpaceIo:
        IoPortAddress = (USHORT)(GenericAddress->Address);
        switch (AccessSize) {
        case 1:
            HlIoPortOutByte(IoPortAddress + Offset, (UCHAR)Value);
            break;

        case 2:
            HlIoPortOutShort(IoPortAddress + Offset, (USHORT)Value);
            break;

        case 4:
            HlIoPortOutLong(IoPortAddress + Offset, Value);
            break;

        default:

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        break;

    //
    // Implement other types if needed.
    //

    default:

        ASSERT(FALSE);

        return STATUS_NOT_IMPLEMENTED;
    }

    return STATUS_SUCCESS;
}

