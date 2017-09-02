/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    oprgn.c

Abstract:

    This module implements support for ACPI Operation Regions.

Author:

    Evan Green 17-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpiobj.h"
#include "amlos.h"
#include "namespce.h"
#include "oprgn.h"
#include "fixedreg.h"

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
AcpipShiftBufferIntoFieldPosition (
    PVOID Buffer,
    ULONGLONG BitOffset,
    ULONGLONG BitLength,
    ULONG AccessSize
    );

VOID
AcpipWriteFieldBitsIntoBuffer (
    PVOID FieldBuffer,
    ULONGLONG BitOffset,
    ULONGLONG BitLength,
    ULONG AccessSize,
    PVOID ResultBuffer,
    ULONGLONG ResultBufferSize
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipCreateOperationRegion (
    PAML_EXECUTION_CONTEXT Context,
    PSTR Name,
    ACPI_OPERATION_REGION_SPACE Space,
    ULONGLONG Offset,
    ULONGLONG Length
    )

/*++

Routine Description:

    This routine creates an ACPI Operation Region object.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    Name - Supplies a pointer to the name of the Operation Region object.

    Space - Supplies the address space type of the region.

    Offset - Supplies the byte offset into the address space of the beginning
        of the Operation Region.

    Length - Supplies the byte length of the Operation Region.

Return Value:

    Status code.

--*/

{

    PACPI_OPERATION_REGION_FUNCTION_TABLE FunctionTable;
    PACPI_OBJECT Object;
    PVOID OsContext;
    PVOID OsMutex;
    KSTATUS Status;

    FunctionTable = NULL;
    Object = NULL;
    OsContext = NULL;
    OsMutex = NULL;

    //
    // Get a pointer to the operation region function table.
    //

    if (Space >= OperationRegionCount) {
        Status = STATUS_INVALID_PARAMETER;
        goto CreateOperationRegionEnd;
    }

    //
    // Create the namespace object.
    //

    Object = AcpipCreateNamespaceObject(Context,
                                        AcpiObjectOperationRegion,
                                        Name,
                                        NULL,
                                        0);

    if (Object == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateOperationRegionEnd;
    }

    AcpipObjectReleaseReference(Object);

    //
    // Create the mutex.
    //

    OsMutex = AcpipCreateMutex(0);
    if (OsMutex == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateOperationRegionEnd;
    }

    FunctionTable = AcpiOperationRegionFunctionTable[Space];

    //
    // Create the operation region with the OS.
    //

    Status = FunctionTable->Create(Object, Offset, Length, &OsContext);
    if (!KSUCCESS(Status)) {
        goto CreateOperationRegionEnd;
    }

    //
    // Initialize and return the operation region.
    //

    Object->U.OperationRegion.Space = Space;
    Object->U.OperationRegion.OsContext = OsContext;
    Object->U.OperationRegion.Offset = Offset;
    Object->U.OperationRegion.Length = Length;
    Object->U.OperationRegion.FunctionTable = FunctionTable;
    Object->U.OperationRegion.OsMutex = OsMutex;
    Status = STATUS_SUCCESS;

CreateOperationRegionEnd:
    if (!KSUCCESS(Status)) {

        //
        // Destroy the operation region if created.
        //

        if (OsContext != NULL) {
            FunctionTable->Destroy(OsContext);
        }

        //
        // Destroy the mutex.
        //

        if (OsMutex != NULL) {
            AcpipDestroyMutex(OsMutex);
        }

        if (Object != NULL) {
            AcpipObjectReleaseReference(Object);
        }
    }

    return Status;
}

VOID
AcpipDestroyOperationRegion (
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine destroys an ACPI Operation Region object. This routine should
    not be called directly, but will be called from the namespace object
    destruction routine.

Arguments:

    Object - Supplies a pointer to the operation region to destroy.

Return Value:

    None.

--*/

{

    PACPI_OPERATION_REGION_FUNCTION_TABLE FunctionTable;

    ASSERT(Object->Type == AcpiObjectOperationRegion);

    if (Object->U.OperationRegion.OsContext != NULL) {
        FunctionTable = Object->U.OperationRegion.FunctionTable;

        ASSERT(FunctionTable != NULL);

        FunctionTable->Destroy(Object->U.OperationRegion.OsContext);
        Object->U.OperationRegion.OsContext = NULL;
    }

    if (Object->U.OperationRegion.OsMutex != NULL) {
        AcpipDestroyMutex(Object->U.OperationRegion.OsMutex);
        Object->U.OperationRegion.OsMutex = NULL;
    }

    Object->U.OperationRegion.FunctionTable = NULL;
    return;
}

KSTATUS
AcpipReadFromField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT FieldObject,
    PACPI_OBJECT *ResultObject
    )

/*++

Routine Description:

    This routine reads from an Operation Region field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    FieldObject - Supplies a pointer to the field object to read from.

    ResultObject - Supplies a pointer where a pointer to the result object will
        be returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

{

    ULONG AccessSize;
    PACPI_OBJECT AlternateOperationRegion;
    BOOL AlternateOperationRegionMutexAcquired;
    PACPI_OBJECT BankRegister;
    ULONGLONG BufferSize;
    PUCHAR CurrentBuffer;
    ULONGLONG CurrentOffset;
    PACPI_OBJECT DataRegister;
    PACPI_OBJECT DataResult;
    ULONGLONG EndBitOffset;
    ULONGLONG EndByteOffset;
    PACPI_FIELD_UNIT_OBJECT FieldUnit;
    BOOL GlobalLockAcquired;
    PACPI_OBJECT IndexRegister;
    PACPI_OBJECT IndexValue;
    BOOL IntegerWidthIs32;
    PACPI_OPERATION_REGION_OBJECT OperationRegion;
    BOOL OperationRegionMutexHeld;
    PACPI_OBJECT OperationRegionObject;
    PACPI_OBJECT Result;
    PUCHAR ResultBuffer;
    ULONGLONG StartBitOffset;
    ULONGLONG StartByteOffset;
    KSTATUS Status;

    *ResultObject = NULL;
    AlternateOperationRegion = NULL;
    AlternateOperationRegionMutexAcquired = FALSE;
    BankRegister = NULL;
    GlobalLockAcquired = FALSE;
    IndexValue = NULL;
    OperationRegion = NULL;
    OperationRegionMutexHeld = FALSE;
    Result = NULL;
    FieldUnit = &(FieldObject->U.FieldUnit);

    ASSERT(FieldObject->Type == AcpiObjectFieldUnit);

    OperationRegionObject = FieldUnit->OperationRegion;
    if (OperationRegionObject != NULL) {

        ASSERT(OperationRegionObject->Type == AcpiObjectOperationRegion);

        OperationRegion = &(OperationRegionObject->U.OperationRegion);
    }

    switch (FieldUnit->Access) {
    case AcpiFieldAccessAny:
    case AcpiFieldAccessBuffer:
    case AcpiFieldAccessByte:
        AccessSize = BITS_PER_BYTE;
        break;

    case AcpiFieldAccessWord:
        AccessSize = 2 * BITS_PER_BYTE;
        break;

    case AcpiFieldAccessDoubleWord:
        AccessSize = 4 * BITS_PER_BYTE;
        break;

    case AcpiFieldAccessQuadWord:
        AccessSize = 8 * BITS_PER_BYTE;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto ReadFromFieldEnd;
    }

    //
    // Calculate the size of the buffer needed by rounding the offset down
    // to the access size, and the length up.
    //

    StartBitOffset = ALIGN_RANGE_DOWN(FieldUnit->BitOffset, AccessSize);
    EndBitOffset = ALIGN_RANGE_UP(FieldUnit->BitOffset + FieldUnit->BitLength,
                                  AccessSize);

    BufferSize = (EndBitOffset - StartBitOffset) / BITS_PER_BYTE;
    StartByteOffset = StartBitOffset / BITS_PER_BYTE;
    EndByteOffset = EndBitOffset / BITS_PER_BYTE;

    //
    // Allocate the result buffer. Use an integer if the result is small
    // enough to fit in one, or create a buffer if not.
    //

    IntegerWidthIs32 = FALSE;
    if (Context->CurrentMethod != NULL) {
        IntegerWidthIs32 = Context->CurrentMethod->IntegerWidthIs32;
    }

    if ((BufferSize <= sizeof(ULONG)) ||
        ((IntegerWidthIs32 == FALSE) && (BufferSize <= sizeof(ULONGLONG)))) {

        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectInteger,
                                            NULL,
                                            NULL,
                                            0);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto ReadFromFieldEnd;
        }

        Result->U.Integer.Value = 0;
        ResultBuffer = (PUCHAR)&(Result->U.Integer.Value);

    } else {
        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectBuffer,
                                            NULL,
                                            NULL,
                                            BufferSize);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto ReadFromFieldEnd;
        }

        ResultBuffer = Result->U.Buffer.Buffer;
        RtlZeroMemory(ResultBuffer, BufferSize);
    }

    //
    // Validate that the reads are safe.
    //

    if (OperationRegion != NULL) {
        if ((StartByteOffset > OperationRegion->Length) ||
            (EndByteOffset > OperationRegion->Length) ||
            (EndByteOffset <= StartByteOffset)) {

            Status = STATUS_INVALID_PARAMETER;
            goto ReadFromFieldEnd;
        }
    }

    //
    // If the field is banked, acquire the mutex for the Operation Region that
    // the bank register points at, and write the bank value to the bank
    // register.
    //

    BankRegister = FieldUnit->BankRegister;
    IndexRegister = FieldUnit->IndexRegister;
    DataRegister = FieldUnit->DataRegister;
    if (BankRegister != NULL) {

        ASSERT(BankRegister->Type == AcpiObjectFieldUnit);

        AlternateOperationRegion = BankRegister->U.FieldUnit.OperationRegion;

        ASSERT(AlternateOperationRegion->Type == AcpiObjectOperationRegion);

        AcpipAcquireMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);

        AlternateOperationRegionMutexAcquired = TRUE;

        //
        // Store the bank value into the bank register.
        //

        Status = AcpipPerformStoreOperation(Context,
                                            FieldUnit->BankValue,
                                            BankRegister);

        if (!KSUCCESS(Status)) {
            goto ReadFromFieldEnd;
        }

    //
    // If the field is Indexed, acquire the mutex for the Operation Region
    // that the Index register points at.
    //

    } else if (IndexRegister != NULL) {

        ASSERT(IndexRegister->Type == AcpiObjectFieldUnit);

        AlternateOperationRegion = IndexRegister->U.FieldUnit.OperationRegion;

        ASSERT(AlternateOperationRegion->Type == AcpiObjectOperationRegion);

        AcpipAcquireMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);

        AlternateOperationRegionMutexAcquired = TRUE;

        //
        // Also create the index value variable at this time.
        //

        IndexValue = AcpipCreateNamespaceObject(Context,
                                                AcpiObjectInteger,
                                                NULL,
                                                NULL,
                                                0);

        if (IndexValue == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto ReadFromFieldEnd;
        }
    }

    //
    // Acquire the mutex and global lock if needed.
    //

    if (OperationRegion != NULL) {
        AcpipAcquireMutex(Context,
                          OperationRegion->OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);

        OperationRegionMutexHeld = TRUE;
    }

    if (FieldUnit->AcquireGlobalLock != FALSE) {
        AcpipAcquireGlobalLock();
        GlobalLockAcquired = TRUE;
    }

    //
    // Perform the reads.
    //

    Status = STATUS_SUCCESS;
    CurrentBuffer = ResultBuffer;
    for (CurrentOffset = StartByteOffset;
         CurrentOffset < EndByteOffset;
         CurrentOffset += (AccessSize / BITS_PER_BYTE)) {

        //
        // For indexed fields, write the index value, then read from the data
        // register.
        //

        if (IndexRegister != NULL) {
            IndexValue->U.Integer.Value = CurrentOffset;
            Status = AcpipWriteToField(Context, IndexRegister, IndexValue);
            if (!KSUCCESS(Status)) {
                break;
            }

            Status = AcpipReadFromField(Context, DataRegister, &DataResult);
            if (!KSUCCESS(Status)) {
                break;
            }

            //
            // Copy the result from the read into the destination buffer.
            //

            if (DataResult->Type == AcpiObjectInteger) {
                RtlCopyMemory(CurrentBuffer,
                              &(DataResult->U.Integer.Value),
                              AccessSize);

            } else if (DataResult->Type == AcpiObjectBuffer) {
                RtlCopyMemory(CurrentBuffer,
                              DataResult->U.Buffer.Buffer,
                              AccessSize);

            } else {
                AcpipObjectReleaseReference(DataResult);
                Status = STATUS_INVALID_PARAMETER;
                goto ReadFromFieldEnd;
            }

            AcpipObjectReleaseReference(DataResult);

        //
        // Perform a normal region read.
        //

        } else {
            Status = OperationRegion->FunctionTable->Read(
                                                    OperationRegion->OsContext,
                                                    CurrentOffset,
                                                    AccessSize,
                                                    CurrentBuffer);

            if (!KSUCCESS(Status)) {

                //
                // Allow region access that occur before they're supposed to.
                //

                if (Status == STATUS_TOO_EARLY) {
                    RtlZeroMemory(CurrentBuffer, AccessSize / BITS_PER_BYTE);
                    Status = STATUS_SUCCESS;

                } else {
                    break;
                }
            }
        }

        CurrentBuffer += AccessSize / BITS_PER_BYTE;
    }

    if (GlobalLockAcquired != FALSE) {
        AcpipReleaseGlobalLock();
        GlobalLockAcquired = FALSE;
    }

    if (OperationRegionMutexHeld != FALSE) {
        AcpipReleaseMutex(Context, OperationRegion->OsMutex);
        OperationRegionMutexHeld = FALSE;
    }

    //
    // If something in the loop failed, bail now.
    //

    if (!KSUCCESS(Status)) {
        goto ReadFromFieldEnd;
    }

    //
    // Shift the buffer, which was read naturally aligned, into the position
    // dictated by the field.
    //

    AcpipShiftBufferIntoFieldPosition(ResultBuffer,
                                      FieldUnit->BitOffset,
                                      FieldUnit->BitLength,
                                      AccessSize);

ReadFromFieldEnd:
    if (GlobalLockAcquired != FALSE) {
        AcpipReleaseGlobalLock();
    }

    if (OperationRegionMutexHeld != FALSE) {
        AcpipReleaseMutex(Context, OperationRegion->OsMutex);
    }

    //
    // Release the alternate mutex if acquired.
    //

    if (AlternateOperationRegionMutexAcquired != FALSE) {
        AcpipReleaseMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex);
    }

    if (IndexValue != NULL) {
        AcpipObjectReleaseReference(IndexValue);
    }

    if (!KSUCCESS(Status)) {
        if (Result != NULL) {
            AcpipObjectReleaseReference(Result);
            Result = NULL;
        }
    }

    *ResultObject = Result;
    return Status;
}

KSTATUS
AcpipWriteToField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT FieldObject,
    PACPI_OBJECT ValueToWrite
    )

/*++

Routine Description:

    This routine writes to an Operation Region field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    FieldObject - Supplies a pointer to the field object to write to.

    ValueToWrite - Supplies a pointer to an Integer or Buffer object containing
        the value to write into the field.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

{

    ULONG AccessSize;
    PACPI_OBJECT AlternateOperationRegion;
    BOOL AlternateOperationRegionMutexAcquired;
    PACPI_OBJECT BankRegister;
    ULONGLONG BufferSize;
    PUCHAR CurrentBuffer;
    ULONGLONG CurrentOffset;
    PACPI_OBJECT DataRegister;
    PACPI_OBJECT DataResult;
    ULONGLONG EndBitOffset;
    ULONGLONG EndByteOffset;
    ULONGLONG FieldByteLength;
    PACPI_FIELD_UNIT_OBJECT FieldUnit;
    PACPI_OBJECT IndexRegister;
    PACPI_OBJECT IndexValue;
    BOOL IntegerWidthIs32;
    BOOL LocksHeld;
    PACPI_OPERATION_REGION_OBJECT OperationRegion;
    PACPI_OBJECT OperationRegionObject;
    PACPI_OBJECT Result;
    PUCHAR ResultBuffer;
    PUCHAR SourceBuffer;
    ULONGLONG SourceBufferSize;
    ULONGLONG StartBitOffset;
    ULONGLONG StartByteOffset;
    KSTATUS Status;

    AlternateOperationRegion = NULL;
    AlternateOperationRegionMutexAcquired = FALSE;
    BankRegister = NULL;
    DataRegister = NULL;
    DataResult = NULL;
    IndexRegister = NULL;
    IndexValue = NULL;
    LocksHeld = FALSE;
    OperationRegion = NULL;
    Result = NULL;
    FieldUnit = &(FieldObject->U.FieldUnit);

    ASSERT(FieldObject->Type == AcpiObjectFieldUnit);

    OperationRegionObject = FieldUnit->OperationRegion;
    if (OperationRegionObject != NULL) {

        ASSERT(OperationRegionObject->Type == AcpiObjectOperationRegion);

        OperationRegion = &(OperationRegionObject->U.OperationRegion);
    }

    switch (FieldUnit->Access) {
    case AcpiFieldAccessAny:
    case AcpiFieldAccessBuffer:
    case AcpiFieldAccessByte:
        AccessSize = BITS_PER_BYTE;
        break;

    case AcpiFieldAccessWord:
        AccessSize = 2 * BITS_PER_BYTE;
        break;

    case AcpiFieldAccessDoubleWord:
        AccessSize = 4 * BITS_PER_BYTE;
        break;

    case AcpiFieldAccessQuadWord:
        AccessSize = 8 * BITS_PER_BYTE;
        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_INVALID_PARAMETER;
        goto WriteToFieldEnd;
    }

    //
    // Calculate the size of the buffer needed by rounding the offset down
    // to the access size, and the length up.
    //

    StartBitOffset = ALIGN_RANGE_DOWN(FieldUnit->BitOffset, AccessSize);
    EndBitOffset = ALIGN_RANGE_UP(FieldUnit->BitOffset + FieldUnit->BitLength,
                                  AccessSize);

    BufferSize = (EndBitOffset - StartBitOffset) / BITS_PER_BYTE;
    StartByteOffset = StartBitOffset / BITS_PER_BYTE;
    EndByteOffset = EndBitOffset / BITS_PER_BYTE;

    //
    // Allocate the result buffer. Use an integer if the result is small
    // enough to fit in one, or create a buffer if not.
    //

    IntegerWidthIs32 = FALSE;
    if (Context->CurrentMethod != NULL) {
        IntegerWidthIs32 = Context->CurrentMethod->IntegerWidthIs32;
    }

    if ((BufferSize <= sizeof(ULONG)) ||
        ((IntegerWidthIs32 == FALSE) && (BufferSize <= sizeof(ULONGLONG)))) {

        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectInteger,
                                            NULL,
                                            NULL,
                                            0);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto WriteToFieldEnd;
        }

        ResultBuffer = (PUCHAR)&(Result->U.Integer.Value);

    } else {
        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectBuffer,
                                            NULL,
                                            NULL,
                                            BufferSize);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto WriteToFieldEnd;
        }

        ResultBuffer = Result->U.Buffer.Buffer;
    }

    //
    // Validate that the accesses are safe.
    //

    if (OperationRegion != NULL) {
        if ((StartByteOffset >= OperationRegion->Length) ||
            (EndByteOffset > OperationRegion->Length) ||
            (EndByteOffset <= StartByteOffset)) {

            Status = STATUS_INVALID_PARAMETER;
            goto WriteToFieldEnd;
        }
    }

    //
    // Determine the source buffer based on the source object.
    //

    if (ValueToWrite->Type == AcpiObjectInteger) {
        SourceBuffer = (PUCHAR)&(ValueToWrite->U.Integer.Value);
        SourceBufferSize = sizeof(ULONGLONG);
        if (IntegerWidthIs32 != FALSE) {
            SourceBufferSize = sizeof(ULONG);
        }

    } else if (ValueToWrite->Type == AcpiObjectBuffer) {
        SourceBuffer = ValueToWrite->U.Buffer.Buffer;
        SourceBufferSize = ValueToWrite->U.Buffer.Length;

    } else {

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto WriteToFieldEnd;
    }

    //
    // The source buffer needs to be at least the size of the field.
    //

    FieldByteLength = ALIGN_RANGE_UP(FieldUnit->BitLength, BITS_PER_BYTE) /
                      BITS_PER_BYTE;

    if (SourceBufferSize < FieldByteLength) {
        Status = STATUS_BUFFER_OVERRUN;
        goto WriteToFieldEnd;
    }

    //
    // If the field is banked, acquire the mutex for the Operation Region that
    // the bank register points at, and write the bank value to the bank
    // register.
    //

    BankRegister = FieldUnit->BankRegister;
    IndexRegister = FieldUnit->IndexRegister;
    DataRegister = FieldUnit->DataRegister;
    if (BankRegister != NULL) {

        ASSERT(BankRegister->Type == AcpiObjectFieldUnit);

        AlternateOperationRegion = BankRegister->U.FieldUnit.OperationRegion;

        ASSERT(AlternateOperationRegion->Type == AcpiObjectOperationRegion);

        AcpipAcquireMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);

        AlternateOperationRegionMutexAcquired = TRUE;

        //
        // Store the bank value into the bank register.
        //

        Status = AcpipPerformStoreOperation(Context,
                                            FieldUnit->BankValue,
                                            BankRegister);

        if (!KSUCCESS(Status)) {
            goto WriteToFieldEnd;
        }

    //
    // If the field is Indexed, acquire the mutex for the Operation Region
    // that the Index register points at.
    //

    } else if (IndexRegister != NULL) {

        ASSERT(IndexRegister->Type == AcpiObjectFieldUnit);

        AlternateOperationRegion = IndexRegister->U.FieldUnit.OperationRegion;

        ASSERT(AlternateOperationRegion->Type == AcpiObjectOperationRegion);

        AcpipAcquireMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);

        AlternateOperationRegionMutexAcquired = TRUE;

        //
        // Also create the index value variable at this time.
        //

        IndexValue = AcpipCreateNamespaceObject(Context,
                                                AcpiObjectInteger,
                                                NULL,
                                                NULL,
                                                0);

        if (IndexValue == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto WriteToFieldEnd;
        }
    }

    //
    // Acquire the mutex and global lock if needed. Do this now because if the
    // rule is preserve, then the register should be read/modified/written
    // atomically.
    //

    if (OperationRegion != NULL) {
        AcpipAcquireMutex(Context,
                          OperationRegion->OsMutex,
                          ACPI_MUTEX_WAIT_INDEFINITELY);
    }

    if (FieldUnit->AcquireGlobalLock != FALSE) {
        AcpipAcquireGlobalLock();
    }

    LocksHeld = TRUE;

    //
    // Fill up the buffer with an initial value depending on the update rule.
    // If the field is already aligned, then there's no need for the read
    // (and it can in fact be harmful if it has side effects).
    //

    if ((StartBitOffset != FieldUnit->BitOffset) ||
        (EndBitOffset != FieldUnit->BitOffset + FieldUnit->BitLength)) {

        CurrentBuffer = ResultBuffer;
        if (FieldUnit->UpdateRule == AcpiFieldUpdatePreserve) {
            for (CurrentOffset = StartByteOffset;
                 CurrentOffset < EndByteOffset;
                 CurrentOffset += (AccessSize / BITS_PER_BYTE)) {

                //
                // For indexed fields, write the index value, then read from
                // the data register.
                //

                if (IndexRegister != NULL) {
                    IndexValue->U.Integer.Value = CurrentOffset;
                    Status = AcpipWriteToField(Context,
                                               IndexRegister,
                                               IndexValue);

                    if (!KSUCCESS(Status)) {
                        goto WriteToFieldEnd;
                    }

                    Status = AcpipReadFromField(Context,
                                                DataRegister,
                                                &DataResult);

                    if (!KSUCCESS(Status)) {
                        goto WriteToFieldEnd;
                    }

                    //
                    // Copy the result from the read into the destination
                    // buffer.
                    //

                    if (DataResult->Type == AcpiObjectInteger) {
                        RtlCopyMemory(CurrentBuffer,
                                      &(DataResult->U.Integer.Value),
                                      AccessSize);

                    } else if (DataResult->Type == AcpiObjectBuffer) {
                        RtlCopyMemory(CurrentBuffer,
                                      DataResult->U.Buffer.Buffer,
                                      AccessSize);

                    } else {
                        Status = STATUS_INVALID_PARAMETER;
                        goto WriteToFieldEnd;
                    }

                    AcpipObjectReleaseReference(DataResult);
                    DataResult = NULL;

                //
                // Perform a normal region read.
                //

                } else {
                    Status = OperationRegion->FunctionTable->Read(
                                                    OperationRegion->OsContext,
                                                    CurrentOffset,
                                                    AccessSize,
                                                    CurrentBuffer);

                    if (!KSUCCESS(Status)) {
                        break;
                    }
                }

                CurrentBuffer += AccessSize / BITS_PER_BYTE;
            }

        } else if (FieldUnit->UpdateRule == AcpiFieldUpdateWriteAsOnes) {
            for (CurrentOffset = StartByteOffset;
                 CurrentOffset < EndByteOffset;
                 CurrentOffset += (AccessSize / BITS_PER_BYTE)) {

                *CurrentBuffer = 0xFF;
                CurrentBuffer += 1;
            }

        } else {
            RtlZeroMemory(CurrentBuffer, EndByteOffset - StartByteOffset);
        }
    }

    //
    // Modify the result buffer to include the bits being set in the field.
    //

    AcpipWriteFieldBitsIntoBuffer(SourceBuffer,
                                  FieldUnit->BitOffset,
                                  FieldUnit->BitLength,
                                  AccessSize,
                                  ResultBuffer,
                                  BufferSize);

    //
    // If it's an Index/Data style write, create an index data value now.
    //

    if (IndexRegister != NULL) {
        DataResult = AcpipCreateNamespaceObject(Context,
                                                AcpiObjectInteger,
                                                NULL,
                                                NULL,
                                                0);

        if (DataResult == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto WriteToFieldEnd;
        }
    }

    //
    // Perform the writes.
    //

    CurrentBuffer = ResultBuffer;
    for (CurrentOffset = StartByteOffset;
         CurrentOffset < EndByteOffset;
         CurrentOffset += (AccessSize / BITS_PER_BYTE)) {

        //
        // For indexed fields, write the index value, then read from the data
        // register.
        //

        if (IndexRegister != NULL) {
            IndexValue->U.Integer.Value = CurrentOffset;
            Status = AcpipWriteToField(Context, IndexRegister, IndexValue);
            if (!KSUCCESS(Status)) {
                goto WriteToFieldEnd;
            }

            DataResult->U.Integer.Value = 0;
            RtlCopyMemory(&(DataResult->U.Integer.Value),
                          CurrentBuffer,
                          AccessSize);

            Status = AcpipWriteToField(Context, DataRegister, DataResult);
            if (!KSUCCESS(Status)) {
                goto WriteToFieldEnd;
            }

        //
        // Perform a normal region write.
        //

        } else {
            Status = OperationRegion->FunctionTable->Write(
                                                    OperationRegion->OsContext,
                                                    CurrentOffset,
                                                    AccessSize,
                                                    CurrentBuffer);

            if (!KSUCCESS(Status)) {
                goto WriteToFieldEnd;
            }
        }

        CurrentBuffer += AccessSize / BITS_PER_BYTE;
    }

    Status = STATUS_SUCCESS;

WriteToFieldEnd:
    if (LocksHeld != FALSE) {
        if (FieldUnit->AcquireGlobalLock != FALSE) {
            AcpipReleaseGlobalLock();
        }

        if (OperationRegion != NULL) {
            AcpipReleaseMutex(Context, OperationRegion->OsMutex);
        }
    }

    //
    // Release the alternate mutex if acquired.
    //

    if (AlternateOperationRegionMutexAcquired != FALSE) {
        AcpipReleaseMutex(Context,
                          AlternateOperationRegion->U.OperationRegion.OsMutex);
    }

    if (IndexValue != NULL) {
        AcpipObjectReleaseReference(IndexValue);
    }

    if (DataResult != NULL) {
        AcpipObjectReleaseReference(DataResult);
    }

    if (Result != NULL) {
        AcpipObjectReleaseReference(Result);
        Result = NULL;
    }

    return Status;
}

KSTATUS
AcpipReadFromBufferField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT BufferField,
    PACPI_OBJECT *ResultObject
    )

/*++

Routine Description:

    This routine reads from a Buffer Field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    BufferField - Supplies a pointer to the field object to read from.

    ResultObject - Supplies a pointer where a pointer to the result object will
        be returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

{

    ULONGLONG BaseBufferSize;
    PVOID BasePointer;
    PACPI_OBJECT BufferObject;
    ULONGLONG EndBitOffset;
    ULONGLONG EndByteOffset;
    BOOL IntegerWidthIs32;
    PACPI_OBJECT Result;
    PVOID ResultBuffer;
    ULONGLONG ResultBufferSize;
    ULONGLONG StartBitOffset;
    ULONGLONG StartByteOffset;
    KSTATUS Status;

    ASSERT(BufferField->Type == AcpiObjectBufferField);

    Result = NULL;

    //
    // Find the buffer this field points to.
    //

    BufferObject = BufferField->U.BufferField.DestinationObject;
    switch (BufferObject->Type) {
    case AcpiObjectInteger:
        BasePointer = &(BufferObject->U.Integer.Value);
        BaseBufferSize = sizeof(ULONGLONG);
        break;

    case AcpiObjectString:
        BasePointer = &(BufferObject->U.String.String);
        if (BasePointer == NULL) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto ReadFromBufferFieldEnd;
        }

        BaseBufferSize = RtlStringLength(BasePointer) + 1;
        break;

    case AcpiObjectBuffer:
        BasePointer = BufferObject->U.Buffer.Buffer;
        BaseBufferSize = BufferObject->U.Buffer.Length;
        if (BaseBufferSize == 0) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto ReadFromBufferFieldEnd;
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto ReadFromBufferFieldEnd;
    }

    //
    // Perform access checks on the field.
    //

    StartBitOffset = ALIGN_RANGE_DOWN(BufferField->U.BufferField.BitOffset,
                                      BITS_PER_BYTE);

    StartByteOffset = StartBitOffset / BITS_PER_BYTE;
    EndBitOffset = BufferField->U.BufferField.BitOffset +
                   BufferField->U.BufferField.BitLength;

    EndBitOffset = ALIGN_RANGE_UP(EndBitOffset, BITS_PER_BYTE);
    EndByteOffset = EndBitOffset / BITS_PER_BYTE;
    ResultBufferSize = EndByteOffset - StartByteOffset;
    if ((StartByteOffset > BaseBufferSize) ||
        (EndByteOffset > BaseBufferSize) ||
        (EndByteOffset <= StartByteOffset)) {

        Status = STATUS_INVALID_PARAMETER;
        goto ReadFromBufferFieldEnd;
    }

    //
    // Allocate the result buffer.
    //

    IntegerWidthIs32 = FALSE;
    if (Context->CurrentMethod != NULL) {
        IntegerWidthIs32 = Context->CurrentMethod->IntegerWidthIs32;
    }

    if ((ResultBufferSize <= sizeof(ULONG)) ||
        ((ResultBufferSize <= sizeof(ULONGLONG)) &&
         (IntegerWidthIs32 != FALSE))) {

        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectInteger,
                                            NULL,
                                            NULL,
                                            0);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto ReadFromBufferFieldEnd;
        }

        Result->U.Integer.Value = 0;
        ResultBuffer = &(Result->U.Integer.Value);

    } else {
        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectBuffer,
                                            NULL,
                                            NULL,
                                            ResultBufferSize);

        if (Result == NULL) {
            Status = STATUS_UNSUCCESSFUL;
            goto ReadFromBufferFieldEnd;
        }

        ResultBuffer = Result->U.Buffer.Buffer;
    }

    //
    // Copy the naturally aligned memory to the destination.
    //

    RtlCopyMemory(ResultBuffer,
                  BasePointer + StartByteOffset,
                  EndByteOffset - StartByteOffset);

    //
    // Shift the memory into place.
    //

    AcpipShiftBufferIntoFieldPosition(ResultBuffer,
                                      BufferField->U.BufferField.BitOffset,
                                      BufferField->U.BufferField.BitLength,
                                      BITS_PER_BYTE);

    Status = STATUS_SUCCESS;

ReadFromBufferFieldEnd:
    if (!KSUCCESS(Status)) {
        if (Result != NULL) {
            AcpipObjectReleaseReference(Result);
            Result = NULL;
        }
    }

    *ResultObject = Result;
    return Status;
}

KSTATUS
AcpipWriteToBufferField (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT BufferField,
    PACPI_OBJECT ValueToWrite
    )

/*++

Routine Description:

    This routine writes to a Buffer Field.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    BufferField - Supplies a pointer to the field object to read from.

    ValueToWrite - Supplies a pointer to the value to write to the buffer field.

Return Value:

    STATUS_SUCCESS on success.

    Error codes on failure.

--*/

{

    PVOID AllocatedFieldBuffer;
    PACPI_OBJECT BufferObject;
    PVOID DestinationBuffer;
    ULONGLONG DestinationBufferSize;
    ULONGLONG EndBitOffset;
    ULONGLONG EndByteOffset;
    PVOID FieldBuffer;
    ULONGLONG FieldBufferSize;
    ULONGLONG StartBitOffset;
    ULONGLONG StartByteOffset;
    KSTATUS Status;

    AllocatedFieldBuffer = NULL;

    ASSERT(BufferField->Type == AcpiObjectBufferField);

    //
    // Find the buffer this field points to.
    //

    BufferObject = BufferField->U.BufferField.DestinationObject;
    switch (BufferObject->Type) {
    case AcpiObjectInteger:
        DestinationBuffer = &(BufferObject->U.Integer.Value);
        DestinationBufferSize = sizeof(ULONGLONG);
        break;

    case AcpiObjectString:
        DestinationBuffer = &(BufferObject->U.String.String);
        if (DestinationBuffer == NULL) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto WriteToBufferFieldEnd;
        }

        DestinationBufferSize = RtlStringLength(DestinationBuffer) + 1;
        break;

    case AcpiObjectBuffer:
        DestinationBuffer = BufferObject->U.Buffer.Buffer;
        DestinationBufferSize = BufferObject->U.Buffer.Length;
        if (DestinationBufferSize == 0) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto WriteToBufferFieldEnd;
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto WriteToBufferFieldEnd;
    }

    //
    // Find the buffer pointer of the value to write.
    //

    switch (ValueToWrite->Type) {
    case AcpiObjectInteger:
        FieldBuffer = &(ValueToWrite->U.Integer.Value);
        FieldBufferSize = sizeof(ULONGLONG);
        break;

    case AcpiObjectString:
        FieldBuffer = &(ValueToWrite->U.String.String);
        if (FieldBuffer == NULL) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto WriteToBufferFieldEnd;
        }

        FieldBufferSize = RtlStringLength(FieldBuffer) + 1;
        break;

    case AcpiObjectBuffer:
        FieldBuffer = ValueToWrite->U.Buffer.Buffer;
        FieldBufferSize = ValueToWrite->U.Buffer.Length;
        if (FieldBufferSize == 0) {
            Status = STATUS_BUFFER_TOO_SMALL;
            goto WriteToBufferFieldEnd;
        }

        break;

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        goto WriteToBufferFieldEnd;
    }

    //
    // Perform access checks on the field.
    //

    StartBitOffset = ALIGN_RANGE_DOWN(BufferField->U.BufferField.BitOffset,
                                      BITS_PER_BYTE);

    StartByteOffset = StartBitOffset / BITS_PER_BYTE;
    EndBitOffset = BufferField->U.BufferField.BitOffset +
                   BufferField->U.BufferField.BitLength;

    EndBitOffset = ALIGN_RANGE_UP(EndBitOffset, BITS_PER_BYTE);
    EndByteOffset = EndBitOffset / BITS_PER_BYTE;
    if ((StartByteOffset > DestinationBufferSize) ||
        (EndByteOffset > DestinationBufferSize) ||
        (EndByteOffset <= StartByteOffset)) {

        Status = STATUS_INVALID_PARAMETER;
        goto WriteToBufferFieldEnd;
    }

    //
    // If the source object is smaller than the field, zero extend the buffer
    // by allocating a copy that's the correct size.
    //

    if (EndByteOffset - StartByteOffset > FieldBufferSize) {
        AllocatedFieldBuffer =
                          AcpipAllocateMemory(EndByteOffset - StartByteOffset);

        if (AllocatedFieldBuffer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto WriteToBufferFieldEnd;
        }

        RtlCopyMemory(AllocatedFieldBuffer, FieldBuffer, FieldBufferSize);
        RtlZeroMemory(AllocatedFieldBuffer + FieldBufferSize,
                      (EndByteOffset - StartByteOffset) - FieldBufferSize);

        FieldBuffer = AllocatedFieldBuffer;
    }

    //
    // Write the field's bits into the destination buffer.
    //

    AcpipWriteFieldBitsIntoBuffer(FieldBuffer,
                                  BufferField->U.BufferField.BitOffset,
                                  BufferField->U.BufferField.BitLength,
                                  BITS_PER_BYTE,
                                  DestinationBuffer + StartByteOffset,
                                  DestinationBufferSize - StartByteOffset);

    Status = STATUS_SUCCESS;

WriteToBufferFieldEnd:
    if (AllocatedFieldBuffer != NULL) {
        AcpipFreeMemory(AllocatedFieldBuffer);
    }

    return Status;
}

VOID
AcpipPrintOperationRegion (
    PACPI_OBJECT OperationRegion
    )

/*++

Routine Description:

    This routine prints a description of the given Operation Region to the
    debugger.

Arguments:

    OperationRegion - Supplies a pointer to the Operation Region to print.

Return Value:

    None.

--*/

{

    PSTR Name;
    PSTR Space;

    ASSERT(OperationRegion->Type == AcpiObjectOperationRegion);

    Space = "UnknownSpace";
    switch (OperationRegion->U.OperationRegion.Space) {
    case OperationRegionSystemMemory:
        Space = "SystemMemory";
        break;

    case OperationRegionSystemIo:
        Space = "SystemIO";
        break;

    case OperationRegionPciConfig:
        Space = "PCIConfig";
        break;

    case OperationRegionEmbeddedController:
        Space = "EmbeddedController";
        break;

    case OperationRegionSmBus:
        Space = "SMBus";
        break;

    case OperationRegionCmos:
        Space = "CMOS";
        break;

    case OperationRegionPciBarTarget:
        Space = "PCIBarTarget";
        break;

    case OperationRegionIpmi:
        Space = "IPMI";
        break;

    default:
        break;
    }

    Name = (PSTR)&(OperationRegion->Name);
    RtlDebugPrint("OperationRegion (%c%c%c%c, %s, 0x%I64x, 0x%I64x)",
                  Name[0],
                  Name[1],
                  Name[2],
                  Name[3],
                  Space,
                  OperationRegion->U.OperationRegion.Offset,
                  OperationRegion->U.OperationRegion.Length);

    return;
}

VOID
AcpipPrintFieldUnit (
    PACPI_OBJECT FieldUnit
    )

/*++

Routine Description:

    This routine prints a description of the given Field Unit to the
    debugger.

Arguments:

    FieldUnit - Supplies a pointer to the Field Unit to print.

Return Value:

    None.

--*/

{

    PSTR FieldUnitType;
    PSTR Name;

    FieldUnitType = "FieldUnit";
    if (FieldUnit->U.FieldUnit.BankRegister != NULL) {
        FieldUnitType = "BankField";
    }

    if (FieldUnit->U.FieldUnit.IndexRegister != NULL) {
        FieldUnitType = "IndexField";
    }

    Name = (PSTR)&(FieldUnit->Name);
    RtlDebugPrint("%s (%c%c%c%c, ",
                  FieldUnitType,
                  Name[0],
                  Name[1],
                  Name[2],
                  Name[3]);

    switch (FieldUnit->U.FieldUnit.Access) {
    case AcpiFieldAccessAny:
        RtlDebugPrint("AccessAny, ");
        break;

    case AcpiFieldAccessByte:
        RtlDebugPrint("AccessByte, ");
        break;

    case AcpiFieldAccessWord:
        RtlDebugPrint("AccessWord, ");
        break;

    case AcpiFieldAccessDoubleWord:
        RtlDebugPrint("AccessDWord, ");
        break;

    case AcpiFieldAccessQuadWord:
        RtlDebugPrint("AccessQWord, ");
        break;

    case AcpiFieldAccessBuffer:
        RtlDebugPrint("AccessBuffer, ");
        break;

    default:
        RtlDebugPrint("INVALIDACCESS %d, ", FieldUnit->U.FieldUnit.Access);
        break;
    }

    if (FieldUnit->U.FieldUnit.AcquireGlobalLock == FALSE) {
        RtlDebugPrint("No");
    }

    RtlDebugPrint("Lock, ");
    switch (FieldUnit->U.FieldUnit.UpdateRule) {
    case AcpiFieldUpdatePreserve:
        RtlDebugPrint("Preserve, ");
        break;

    case AcpiFieldUpdateWriteAsOnes:
        RtlDebugPrint("WriteAsOnes, ");
        break;

    case AcpiFieldUpdateWriteAsZeros:
        RtlDebugPrint("WriteAsZeros, ");
        break;

    default:
        RtlDebugPrint("INVALIDUPDATERULE %d",
                      FieldUnit->U.FieldUnit.UpdateRule);

        break;
    }

    RtlDebugPrint("%I64d)", FieldUnit->U.FieldUnit.BitLength);
    return;
}

VOID
AcpipPrintBufferField (
    PACPI_OBJECT BufferField
    )

/*++

Routine Description:

    This routine prints a description of the given Buffer Field to the
    debugger.

Arguments:

    BufferField - Supplies a pointer to the Buffer Field to print.

Return Value:

    None.

--*/

{

    PSTR Name;

    Name = (PSTR)&(BufferField->Name);
    RtlDebugPrint("BufferField (%c%c%c%c, %I64x, %I64x, 0x%08x)",
                  Name[0],
                  Name[1],
                  Name[2],
                  Name[3],
                  BufferField->U.BufferField.BitOffset,
                  BufferField->U.BufferField.BitLength,
                  BufferField->U.BufferField.DestinationObject);

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
AcpipShiftBufferIntoFieldPosition (
    PVOID Buffer,
    ULONGLONG BitOffset,
    ULONGLONG BitLength,
    ULONG AccessSize
    )

/*++

Routine Description:

    This routine shifts a naturally aligned buffer into a field result buffer.

Arguments:

    Buffer - Supplies a pointer to the source (and destination buffer) to
        shift right.

    BitOffset - Supplies the bit offset of the field. The entire buffer will
        be shifted this many bits right.

    BitLength - Supplies the bit length of the field. Bits after this length
        will be cleared.

    AccessSize - Supplies the access size of the field in bits.

Return Value:

    None.

--*/

{

    ULONGLONG BufferSize;
    PUCHAR CurrentBuffer;
    ULONGLONG CurrentOffset;
    ULONGLONG EndBitOffset;
    ULONGLONG EndByteOffset;
    ULONG Mask;
    ULONG SaveBits;
    ULONG ShiftAmount;
    ULONGLONG StartBitOffset;
    ULONGLONG StartByteOffset;
    ULONG TotalShiftAmount;

    //
    // Shift the results into place if the field is not aligned to the access
    // size.
    //

    StartBitOffset = ALIGN_RANGE_DOWN(BitOffset, AccessSize);
    EndBitOffset = ALIGN_RANGE_UP(BitOffset + BitLength, AccessSize);
    StartByteOffset = StartBitOffset / BITS_PER_BYTE;
    EndByteOffset = EndBitOffset / BITS_PER_BYTE;
    BufferSize = (EndBitOffset - StartBitOffset) / BITS_PER_BYTE;
    TotalShiftAmount = BitOffset - StartBitOffset;
    while (TotalShiftAmount != 0) {

        //
        // Since the shift algorithm deals in byte pointers (so as not to
        // overstep the array), the max that can be shifted in one iteration is
        // an entire byte.
        //

        if (TotalShiftAmount > BITS_PER_BYTE) {
            ShiftAmount = BITS_PER_BYTE;

        } else {
            ShiftAmount = TotalShiftAmount;
        }

        TotalShiftAmount -= ShiftAmount;
        CurrentBuffer = Buffer;
        for (CurrentOffset = 0;
             CurrentOffset < BufferSize - 1;
             CurrentOffset += 1) {

            *CurrentBuffer = (*CurrentBuffer >> ShiftAmount) |
                        (*(CurrentBuffer + 1) << (BITS_PER_BYTE - ShiftAmount));

            CurrentBuffer += 1;
        }

        //
        // Do the last one, filled with zeroes.
        //

        *CurrentBuffer = *CurrentBuffer >> ShiftAmount;
    }

    //
    // Clip the unwanted portion on the end of the buffer. Remember that the
    // buffer has already been shifted down by the unaligned bit offset. For
    // example, if the bit field started at bit 1 with a length of 1, then
    // after the right shift of one that has already occurred, 1 bit needs to
    // be saved (and 7 zeroed).
    //

    SaveBits = BitLength & (AccessSize - 1);
    if (SaveBits != 0) {
        CurrentBuffer = Buffer +
                        (EndByteOffset - StartByteOffset) -
                        (AccessSize / BITS_PER_BYTE);

        for (CurrentOffset = 0;
             CurrentOffset < (AccessSize / BITS_PER_BYTE);
             CurrentOffset += 1) {

            if (SaveBits > BITS_PER_BYTE) {
                SaveBits -= BITS_PER_BYTE;

            } else {

                //
                // Create a mask that has ones for each bit to save, and zeroes
                // for the more significant parts.
                //

                Mask = (1 << SaveBits) - 1;
                *CurrentBuffer &= Mask;
                SaveBits = 0;
            }

            CurrentBuffer += 1;
        }
    }

    return;
}

VOID
AcpipWriteFieldBitsIntoBuffer (
    PVOID FieldBuffer,
    ULONGLONG BitOffset,
    ULONGLONG BitLength,
    ULONG AccessSize,
    PVOID ResultBuffer,
    ULONGLONG ResultBufferSize
    )

/*++

Routine Description:

    This routine modifies a result buffer to write the bits from a field into
    it.

Arguments:

    FieldBuffer - Supplies a pointer to the bit-aligned field buffer.

    BitOffset - Supplies the bit offset from the start of the region
        where this field refers to.

    BitLength - Supplies the size, in bits, of this field. It is assumed that
        the buffer is at least as big as the number of bits in the field
        rounded up to the nearest byte.

    AccessSize - Supplies the access granularity of the result buffer. The
        bit offset rounded down to the access size determines the start bit
        offset of the result buffer (which is assumed to not be the entire
        region).

    ResultBuffer - Supplies a pointer to the buffer, which is assumed to begin
        at the bit offset of the field, rounded down to the nearest access size.

    ResultBufferSize - Supplies the size of the result buffer, in bytes.

Return Value:

    None.

--*/

{

    ULONG AccessSizeRemainder;
    ULONG ByteBitOffset;
    UCHAR CurrentByte;
    UCHAR Data;
    PUCHAR DestinationBuffer;
    ULONGLONG DestinationOffset;
    BOOL ExtraByte;
    UCHAR Mask;
    UCHAR PreviousByteLeftovers;
    ULONG SaveBits;
    PUCHAR SourceBuffer;
    ULONGLONG SourceBufferSize;
    ULONGLONG SourceIndex;
    ULONGLONG StartBitOffset;

    SourceBuffer = FieldBuffer;
    DestinationBuffer = ResultBuffer;
    StartBitOffset = ALIGN_RANGE_DOWN(BitOffset, AccessSize);
    AccessSizeRemainder = (BitOffset - StartBitOffset) / BITS_PER_BYTE;
    ByteBitOffset = (BitOffset - StartBitOffset) & (BITS_PER_BYTE - 1);

    //
    // Align the source buffer size up to the nearest byte of the field.
    //

    SourceBufferSize = ALIGN_RANGE_UP(BitLength, BITS_PER_BYTE) / BITS_PER_BYTE;

    //
    // Determine if there are more destination bytes to write than source bytes
    // provided due to the shifting of the source bits.
    //

    ExtraByte = FALSE;
    if ((ByteBitOffset != 0) &&
        (ALIGN_RANGE_UP(BitLength, BITS_PER_BYTE) <
         ALIGN_RANGE_UP(BitLength + ByteBitOffset, BITS_PER_BYTE))) {

        ExtraByte = TRUE;
    }

    //
    // Read the bits out of the value to write and put them in the result
    // buffer.
    //

    PreviousByteLeftovers = 0;
    for (SourceIndex = 0; SourceIndex < SourceBufferSize; SourceIndex += 1) {
        Data = SourceBuffer[SourceIndex];
        Mask = 0xFF;

        //
        // If the byte's bit offset is not zero, then extra logic needs to
        // apply to include the bits that may get left shifted away.
        //

        if (ByteBitOffset != 0) {
            CurrentByte = Data;
            Data <<= ByteBitOffset;

            //
            // If this is the first byte in the source, there is no previous
            // data to OR into it. Start the mask at the byte's bit offset.
            //

            if (SourceIndex == 0) {
                Mask <<= ByteBitOffset;

            //
            // Otherwise, OR in the bits taken from the previous byte in the
            // source buffer. The mask should be the full byte, unless this is
            // the last byte to write (handled below).
            //

            } else {
                Data |= PreviousByteLeftovers;
            }

            //
            // Some of the bits may have been shifted out of this round. Save
            // the leftovers.
            //

            PreviousByteLeftovers = CurrentByte >>
                                    (BITS_PER_BYTE - ByteBitOffset);
        }

        //
        // If this is the last byte in the source buffer, potentially clip the
        // mask.
        //

        if ((ExtraByte == FALSE) &&
            (((SourceIndex + 1) * BITS_PER_BYTE) > BitLength)) {

            SaveBits = (BitOffset + BitLength) & (BITS_PER_BYTE - 1);
            Mask &= (1 << SaveBits) - 1;
        }

        //
        // Mask in the appropriate bits to the result buffer.
        //

        DestinationOffset = SourceIndex + AccessSizeRemainder;

        ASSERT(DestinationOffset < ResultBufferSize);

        DestinationBuffer[DestinationOffset] =
              (DestinationBuffer[DestinationOffset] & (~Mask)) | (Data & Mask);
    }

    //
    // Write the extra byte if necessary. The bits are stored in the previous
    // byte's leftovers.
    //

    if (ExtraByte != FALSE) {
        Data = PreviousByteLeftovers;
        SaveBits = (BitOffset + BitLength) & (BITS_PER_BYTE - 1);

        ASSERT(SaveBits != 0);

        Mask = 0xFF & ((1 << SaveBits) - 1);
        DestinationOffset = SourceBufferSize + AccessSizeRemainder;

        ASSERT(DestinationOffset < ResultBufferSize);

        DestinationBuffer[DestinationOffset] =
              (DestinationBuffer[DestinationOffset] & (~Mask)) | (Data & Mask);
    }

    return;
}
