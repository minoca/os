/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amlopex.c

Abstract:

    This module implements ACPI AML low level opcode support for executing
    AML statements.

Author:

    Evan Green 13-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpip.h"
#include "amlos.h"
#include "amlops.h"
#include "namespce.h"
#include "oprgn.h"

//
// ---------------------------------------------------------------- Definitions
//

#define ACPI_RESOURCE_END_TAG 0x79

//
// Define the longest string that can be created from converting a decimal
// integer to a string.
//

#define MAX_DECIMAL_STRING_LENGTH 22

//
// Define the portion of the mutex sync flags that represent the sync level.
//

#define MUTEX_FLAG_SYNC_LEVEL_MASK 0xF

//
// Define the bitfields of the method flags byte.
//

#define METHOD_ARGUMENT_COUNT_MASK 0x7
#define METHOD_SERIALIZED_FLAG 0x08
#define METHOD_SYNC_LEVEL_SHIFT 4
#define METHOD_SYNC_LEVEL_MASK (0xF << METHOD_SYNC_LEVEL_SHIFT)

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ACPI_MATCH_OPERATOR {
    MatchOperatorTrue                 = 0,
    MatchOperatorEqual                = 1,
    MatchOperatorLessThanOrEqualTo    = 2,
    MatchOperatorLessThan             = 3,
    MatchOperatorGreaterThanOrEqualTo = 4,
    MatchOperatorGreaterThan          = 5,
    MatchOperatorCount
} ACPI_MATCH_OPERATOR, *PACPI_MATCH_OPERATOR;

//
// ----------------------------------------------- Internal Function Prototypes
//

PACPI_OBJECT
AcpipConvertObjectTypeToInteger (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    );

PACPI_OBJECT
AcpipConvertObjectTypeToString (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    );

PACPI_OBJECT
AcpipConvertObjectTypeToBuffer (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    );

BOOL
AcpipEvaluateMatchComparison (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT PackageElement,
    PACPI_OBJECT Operand1,
    ACPI_MATCH_OPERATOR Operator1,
    PACPI_OBJECT Operand2,
    ACPI_MATCH_OPERATOR Operator2
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR AcpiMatchOpcodeString[MatchOperatorCount] = {
    "MTR", // Always TRUE.
    "MEQ", // Equal to.
    "MLE", // Less than or Equal to.
    "MLT", // Less than.
    "MGE", // Greater than or equal to.
    "MGT"  // Greater than.
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipEvaluateAcquireStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Acquire (mutex) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;
    PACPI_OBJECT NewArgument;
    ULONGLONG ResultValue;
    USHORT TimeoutValue;

    //
    // Gather arguments if needed.
    //

    TimeoutValue = 0;
    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Acquire (");
            }
        }

        //
        // An argument is required.
        //

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[0] = NULL;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (NewArgument == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            Statement->Argument[0] = NewArgument;
            AcpipObjectAddReference(NewArgument);
        }

        Statement->ArgumentsAcquired += 1;

        //
        // The first argument should be acquired now, and the second argument
        // is a constant word representing the timeout value.
        //

        InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
        if (Context->CurrentOffset + sizeof(USHORT) >= Context->AmlCodeSize) {
            return STATUS_MALFORMED_DATA_STREAM;
        }

        TimeoutValue = *((PUSHORT)InstructionPointer);
        Context->CurrentOffset += sizeof(USHORT);
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint(", %d)", TimeoutValue);
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {

        ASSERT(Statement->Argument[0]->Type == AcpiObjectMutex);

        ResultValue = AcpipAcquireMutex(Context,
                                        Statement->Argument[0]->U.Mutex.OsMutex,
                                        TimeoutValue);

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ResultValue,
                                                          sizeof(ULONGLONG));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateAliasStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates the alias statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Destination;

    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectString);

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Alias (%s, %s)\n",
                      Statement->Argument[0]->U.String.String,
                      Statement->Argument[1]->U.String.String);
    }

    if (Context->ExecuteStatements != FALSE) {

        //
        // Get the destination object that the alias points to.
        //

        Destination = AcpipGetNamespaceObject(
                                       Statement->Argument[0]->U.String.String,
                                       Context->CurrentScope);

        if (Destination == NULL) {
            return STATUS_NOT_FOUND;
        }

        //
        // Create the alias.
        //

        Statement->Reduction = AcpipCreateNamespaceObject(
                                       Context,
                                       AcpiObjectAlias,
                                       Statement->Argument[1]->U.String.String,
                                       &Destination,
                                       sizeof(PVOID));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateArgumentStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates the ArgX opcodes.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONG ArgumentNumber;
    PACPI_OBJECT ArgumentObject;

    ArgumentNumber = (ULONG)Statement->AdditionalData;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Arg%d", ArgumentNumber);
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        ArgumentObject = Context->CurrentMethod->Argument[ArgumentNumber];
        if (ArgumentObject != NULL) {
            Statement->Reduction = ArgumentObject;
            AcpipObjectAddReference(ArgumentObject);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateBankFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a BankField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    UCHAR AccessFlags;
    PACPI_OBJECT BankRegister;
    PUCHAR InstructionPointer;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT OperationRegion;
    KSTATUS Status;

    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectString);

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("BankField (%s, ",
                              Statement->Argument[0]->U.String.String);

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        ASSERT(Statement->ArgumentsAcquired == 2);

        //
        // Perform a conversion if needed.
        //

        NewArgument = Context->PreviousStatement->Reduction;
        if (NewArgument == NULL) {
            return STATUS_ARGUMENT_EXPECTED;
        }

        if (NewArgument->Type != AcpiObjectInteger) {
            NewArgument = AcpipConvertObjectType(Context,
                                                 NewArgument,
                                                 AcpiObjectInteger);

            if (NewArgument == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

        } else {
            AcpipObjectAddReference(NewArgument);
        }

        Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
        Statement->ArgumentsAcquired += 1;
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[2]->Type == AcpiObjectInteger);

    //
    // Parse the starting flags and store them in additional data 2.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    AccessFlags = *InstructionPointer;
    Context->CurrentOffset += 1;

    //
    // Get the operation region this Field refers to.
    //

    OperationRegion =
                AcpipGetNamespaceObject(Statement->Argument[0]->U.String.String,
                                        Context->CurrentScope);

    if (OperationRegion == NULL) {
        return STATUS_NOT_FOUND;
    }

    BankRegister =
               AcpipGetNamespaceObject(Statement->Argument[1]->U.String.String,
                                       Context->CurrentScope);

    if (BankRegister == NULL) {
        return STATUS_NOT_FOUND;
    }

    //
    // Parse the field list.
    //

    Status = AcpipParseFieldList(Context,
                                 Statement->Type,
                                 OperationRegion,
                                 BankRegister,
                                 Statement->Argument[2],
                                 NULL,
                                 NULL,
                                 Statement->AdditionalData,
                                 AccessFlags);

    return Status;
}

KSTATUS
AcpipEvaluateBreakPointStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates the BreakPoint statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("BreakPoint");
    }

    if (Context->ExecuteStatements != FALSE) {
        RtlDebugBreak();
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a buffer declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Argument;
    PACPI_OBJECT BufferObject;
    ULONG BufferSize;
    ULONG ByteIndex;
    ULONG ByteListLength;
    PUCHAR ByteListPointer;

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PreviousStatement == NULL) {
            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint("Buffer (");
            }

            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Convert the buffer size object to an integer if needed.
            //

            Argument = Context->PreviousStatement->Reduction;
            if (Argument == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            if (Argument->Type != AcpiObjectInteger) {
                Argument = AcpipConvertObjectType(Context,
                                                  Argument,
                                                  AcpiObjectInteger);

            } else {
                AcpipObjectAddReference(Argument);
            }

            Statement->Argument[0] = Argument;

        //
        // Just pretend the argument would have been there.
        //

        } else {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
        }

        Statement->ArgumentsAcquired += 1;
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    //
    // Collect the byte list following the buffer size argument just acquired.
    //

    ByteListLength = Statement->AdditionalData - Context->CurrentOffset;
    ByteListPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(") {");
        for (ByteIndex = 0; ByteIndex < ByteListLength; ByteIndex += 1) {
            RtlDebugPrint("%02x ", ByteListPointer[ByteIndex]);
        }

        RtlDebugPrint("}");
    }

    if (Context->ExecuteStatements != FALSE) {

        ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

        //
        // The buffer size comes from evaluating the argument. If the
        // initializer is bigger than the buffer size, then expand it to fit
        // the initializer.
        //

        BufferSize = Statement->Argument[0]->U.Integer.Value;
        if (BufferSize < ByteListLength) {
            BufferSize = ByteListLength;
        }

        //
        // If the buffer size is greater than the initializer, allocate and
        // initialize in two steps. Otherwise, pass the data directly.
        //

        if (BufferSize > ByteListLength) {
            BufferObject = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectBuffer,
                                                      NULL,
                                                      NULL,
                                                      BufferSize);

            if (BufferObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            RtlCopyMemory(BufferObject->U.Buffer.Buffer,
                          ByteListPointer,
                          ByteListLength);

        } else {
            BufferObject = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectBuffer,
                                                      NULL,
                                                      ByteListPointer,
                                                      ByteListLength);

            if (BufferObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }
        }

        Statement->Reduction = BufferObject;
    }

    //
    // Move the instruction pointer over the byte list.
    //

    Context->CurrentOffset = (ULONG)Statement->AdditionalData;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateConcatenateResourceTemplatesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a "Concatenate Resource Templates" statement, which
    concatenates two buffers that are resources templates. It automatically
    strips the end tags off the two, adds it to the concatenation, and calcuates
    the checksum.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Argument0;
    ULONG Argument0Length;
    PACPI_OBJECT Argument1;
    ULONG Argument1Length;
    PUCHAR BytePointer;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT Result;
    UCHAR SumOfTemplate;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("ConcatResTemplate (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if ((Statement->ArgumentsAcquired == 0) ||
                (Statement->ArgumentsAcquired == 1)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Only buffers are supported.
                //

                if (NewArgument->Type != AcpiObjectBuffer) {
                    return STATUS_INVALID_PARAMETER;
                }
            }

            AcpipObjectAddReference(NewArgument);
            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Argument0 = Statement->Argument[0];
    Argument1 = Statement->Argument[1];

    ASSERT(Argument0->Type == AcpiObjectBuffer);
    ASSERT(Argument1->Type == AcpiObjectBuffer);

    //
    // Strip off the end tag of argument 0, if there is one.
    //

    Argument0Length = Argument0->U.Buffer.Length;
    if (Argument0Length < 2) {
        Argument0Length = 0;

    } else {
        BytePointer = (PUCHAR)Argument0->U.Buffer.Buffer +
                      Argument0->U.Buffer.Length - 2;

        if (*BytePointer == ACPI_RESOURCE_END_TAG) {
            Argument0Length -= 2;
        }
    }

    //
    // Strip off argument 1's end tag.
    //

    Argument1Length = Argument1->U.Buffer.Length;
    if (Argument1Length < 2) {
        Argument1Length = 0;

    } else {
        BytePointer = (PUCHAR)Argument1->U.Buffer.Buffer +
                      Argument1->U.Buffer.Length - 2;

        if (*BytePointer == ACPI_RESOURCE_END_TAG) {
            Argument1Length -= 2;
        }
    }

    //
    // Create the new buffer object with space for an end tag.
    //

    Result = AcpipCreateNamespaceObject(Context,
                                        AcpiObjectBuffer,
                                        NULL,
                                        NULL,
                                        Argument0Length + Argument1Length + 2);

    if (Result == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Copy the templates over, back to back
    //

    RtlCopyMemory(Result->U.Buffer.Buffer,
                  Argument0->U.Buffer.Buffer,
                  Argument0Length);

    RtlCopyMemory(Result->U.Buffer.Buffer + Argument0Length,
                  Argument1->U.Buffer.Buffer,
                  Argument1Length);

    //
    // Slap a new end tag and checksum on that puppy.
    //

    BytePointer = (PUCHAR)(Result->U.Buffer.Buffer) +
                  Argument0Length + Argument1Length;

    *BytePointer = ACPI_RESOURCE_END_TAG;
    BytePointer += 1;
    SumOfTemplate = AcpipChecksumData(Result->U.Buffer.Buffer,
                                      Argument0Length + Argument1Length + 1);

    *BytePointer = -SumOfTemplate;
    Statement->Reduction = Result;

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[2] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[2]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateConcatenateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a concatenate statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Argument0;
    PACPI_OBJECT Argument1;
    PACPI_OBJECT NewArgument;
    ULONG NewLength;
    PACPI_OBJECT Result;
    ULONG String0Length;
    ULONG String1Length;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Concat (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if ((Statement->ArgumentsAcquired == 0) ||
                (Statement->ArgumentsAcquired == 1)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Only integers, strings, and buffers are supported.
                //

                if ((NewArgument->Type != AcpiObjectInteger) &&
                    (NewArgument->Type != AcpiObjectString) &&
                    (NewArgument->Type != AcpiObjectBuffer)) {

                    return STATUS_INVALID_PARAMETER;
                }

                //
                // Perform an implicit conversion on the second argument (to the
                // type of the first argument) if needed.
                //

                if (Statement->ArgumentsAcquired == 0) {
                    AcpipObjectAddReference(Statement->Argument[0]);

                } else if (Statement->ArgumentsAcquired == 1) {
                    if (Statement->Argument[0]->Type != NewArgument->Type) {
                        NewArgument = AcpipConvertObjectType(
                                                 Context,
                                                 NewArgument,
                                                 Statement->Argument[0]->Type);

                        if (NewArgument == NULL) {
                            return STATUS_CONVERSION_FAILED;
                        }

                    //
                    // No conversion is needed, just add to the reference count.
                    //

                    } else {
                        AcpipObjectAddReference(NewArgument);
                    }

                } else {

                    ASSERT(Statement->ArgumentsAcquired == 2);

                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    Argument0 = Statement->Argument[0];
    Argument1 = Statement->Argument[1];
    switch (Argument0->Type) {

    //
    // Two integers get put together and make a buffer.
    //

    case AcpiObjectInteger:
        NewLength = 2 * sizeof(ULONGLONG);
        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            NewLength = 2 * sizeof(ULONG);
        }

        //
        // Create a new buffer.
        //

        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectBuffer,
                                            NULL,
                                            NULL,
                                            NewLength);

        if (Result == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        //
        // Copy the integers in like buffers.
        //

        RtlCopyMemory(Result->U.Buffer.Buffer,
                      &(Argument0->U.Integer.Value),
                      NewLength / 2);

        RtlCopyMemory(Result->U.Buffer.Buffer + NewLength / 2,
                      &(Argument1->U.Integer.Value),
                      NewLength / 2);

        break;

    //
    // Two buffers simply get glommed together.
    //

    case AcpiObjectBuffer:
        NewLength = Argument0->U.Buffer.Length + Argument1->U.Buffer.Length;
        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectBuffer,
                                            NULL,
                                            NULL,
                                            NewLength);

        if (Result == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        RtlCopyMemory(Result->U.Buffer.Buffer,
                      Argument0->U.Buffer.Buffer,
                      Argument0->U.Buffer.Length);

        RtlCopyMemory(Result->U.Buffer.Buffer + Argument0->U.Buffer.Length,
                      Argument1->U.Buffer.Buffer,
                      Argument1->U.Buffer.Length);

        break;

    //
    // Two strings get concatenated into another string.
    //

    case AcpiObjectString:
        String0Length = RtlStringLength(Argument0->U.String.String);
        String1Length = RtlStringLength(Argument1->U.String.String);
        NewLength = String0Length + String1Length + 1;
        Result = AcpipCreateNamespaceObject(Context,
                                            AcpiObjectString,
                                            NULL,
                                            NULL,
                                            NewLength);

        if (Result == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        RtlCopyMemory(Result->U.String.String,
                      Argument0->U.String.String,
                      String0Length);

        RtlCopyMemory(Result->U.String.String + String0Length,
                      Argument1->U.String.String,
                      String1Length);

        Result->U.String.String[NewLength - 1] = '\0';
        break;

    default:

        ASSERT(FALSE);

        return STATUS_CONVERSION_FAILED;
    }

    Statement->Reduction = Result;

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[2] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[2]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateConditionalReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an "Reference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NameString;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT Reference;
    ULONGLONG ResultValue;

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("CondRefOf (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        //
        // The argument for RefOf is a "SuperName", which is a SimpleName,
        // DebugOp, or Type6Opcode. If this is the first time through, try to
        // parse a name string.
        //

        if (Context->PreviousStatement == NULL) {
            NameString = AcpipParseNameString(Context);
            if (NameString == NULL) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            if (Context->ExecuteStatements != FALSE) {
                Statement->Argument[0] =
                           AcpipGetNamespaceObject(NameString->U.String.String,
                                                   Context->CurrentScope);

            } else {
                Statement->Argument[0] = NULL;
            }

            if (Statement->Argument[0] != NULL) {
                AcpipObjectAddReference(Statement->Argument[0]);
            }

            Statement->ArgumentsAcquired += 1;
            AcpipObjectReleaseReference(NameString);

        //
        // Increment the reference count on the object.
        //

        } else {
            if (Context->ExecuteStatements != FALSE) {
                NewArgument = Context->PreviousStatement->Reduction;
                Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {
                Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            }

            Statement->ArgumentsAcquired += 1;
        }

        if (Statement->ArgumentsAcquired != Statement->ArgumentsNeeded) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->ExecuteStatements != FALSE) {

        //
        // The reduction of this statement is a boolean indicating whether the
        // object actually exists or not.
        //

        ResultValue = FALSE;
        if ((Statement->Argument[0] != NULL) &&
            (Statement->Argument[0]->Type != AcpiObjectUninitialized)) {

            ResultValue = TRUE;
        }

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ResultValue,
                                                          sizeof(ULONGLONG));

        if ((Statement->Argument[0] != NULL) &&
            (Statement->Argument[1] != NULL)) {

            Reference = AcpipCreateNamespaceObject(Context,
                                                   AcpiObjectAlias,
                                                   NULL,
                                                   &(Statement->Argument[0]),
                                                   sizeof(PACPI_OBJECT));

            if (Reference == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            AcpipPerformStoreOperation(Context,
                                       Reference,
                                       Statement->Argument[1]);

            AcpipObjectReleaseReference(Reference);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateCopyObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a "Copy Object" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Destination;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT Source;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("CopyObject (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            ASSERT(Statement->ArgumentsAcquired == 0);

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            AcpipObjectAddReference(NewArgument);
            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        Source = Statement->Argument[0];

        //
        // This needs to perform an implicit source converstion into a
        // DataRefObject (i.e. a DataObject or Reference Object).
        //

        Status = AcpipConvertToDataReferenceObject(Context, Source, &Source);
        if (!KSUCCESS(Status)) {
            goto EvaluateCopyObjectStatementEnd;
        }

        Statement->Reduction = AcpipCopyObject(Source);
        AcpipObjectReleaseReference(Source);

        //
        // If the target is supplied, replace it with the copy.
        //

        Destination = Statement->Argument[1];
        if (Destination != NULL) {
            Status = AcpipResolveStoreDestination(Context,
                                                  Destination,
                                                  &Destination);

            if (!KSUCCESS(Status)) {
                goto EvaluateCopyObjectStatementEnd;
            }

            Status = AcpipReplaceObjectContents(Context,
                                                Destination,
                                                Statement->Reduction);

            AcpipObjectReleaseReference(Destination);
            if (!KSUCCESS(Status)) {
                goto EvaluateCopyObjectStatementEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

EvaluateCopyObjectStatementEnd:
    return Status;
}

KSTATUS
AcpipEvaluateCreateBufferFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a CreateField (from a buffer) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ACPI_BUFFER_FIELD_OBJECT BufferField;
    PSTR Name;
    PACPI_OBJECT NewArgument;
    ACPI_OBJECT_TYPE ObjectType;

    NewArgument = NULL;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("CreateField (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1) ||
                       (Statement->ArgumentsAcquired == 2)) {

                RtlDebugPrint(", ");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if ((Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) &&
                (Statement->ArgumentsAcquired != 3)) {

                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // The first required argument is a buffer, the second is the
            // bit index (Integer), and the third is the bit length (Integer).
            //

            ObjectType = AcpiObjectBuffer;
            if ((Statement->ArgumentsAcquired == 1) ||
                (Statement->ArgumentsAcquired == 2)) {

                ObjectType = AcpiObjectInteger;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != ObjectType) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     ObjectType);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsAcquired != 3) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }

        //
        // The fourth argument is a name string, which can be parsed now.
        //

        ASSERT(Statement->ArgumentsAcquired == 3);

        Statement->Argument[3] = AcpipParseNameString(Context);
        if (Statement->Argument[3] == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        Statement->ArgumentsAcquired += 1;
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[3]->Type == AcpiObjectString);

    Name = Statement->Argument[3]->U.String.String;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("%s)", Name);
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectBuffer);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);
    ASSERT(Statement->Argument[2]->Type == AcpiObjectInteger);

    //
    // Create the buffer field object. Remember that additional data holds the
    // bit field length.
    //

    RtlZeroMemory(&BufferField, sizeof(ACPI_BUFFER_FIELD_OBJECT));
    BufferField.DestinationObject = Statement->Argument[0];
    BufferField.BitOffset = Statement->Argument[1]->U.Integer.Value;
    BufferField.BitLength = Statement->Argument[2]->U.Integer.Value;
    Statement->Reduction = AcpipCreateNamespaceObject(
                                             Context,
                                             AcpiObjectBufferField,
                                             Name,
                                             &BufferField,
                                             sizeof(ACPI_BUFFER_FIELD_OBJECT));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateCreateFixedBufferFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a CreateBitField, CreateByteField, CreateWordField,
    CreateDWordField, or CreateQWordField statement, which creates a Buffer
    Field object pointing at a buffer.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ACPI_BUFFER_FIELD_OBJECT BufferField;
    PSTR Name;
    PACPI_OBJECT NewArgument;
    ACPI_OBJECT_TYPE ObjectType;

    NewArgument = NULL;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                switch (Statement->AdditionalData) {
                case 1:
                    RtlDebugPrint("CreateBitField (");
                    break;

                case BITS_PER_BYTE:
                    RtlDebugPrint("CreateByteField (");
                    break;

                case sizeof(USHORT) * BITS_PER_BYTE:
                    RtlDebugPrint("CreateWordField (");
                    break;

                case sizeof(ULONG) * BITS_PER_BYTE:
                    RtlDebugPrint("CreateDWordField (");
                    break;

                case sizeof(ULONGLONG) * BITS_PER_BYTE:
                    RtlDebugPrint("CreateQWordField (");
                    break;

                default:

                    ASSERT(FALSE);

                    return STATUS_NOT_SUPPORTED;
                }

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1)) {

                RtlDebugPrint(", ");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        ASSERT(Statement->ArgumentsAcquired != 2);

        //
        // If not executing, then assume the argument would be there but
        // don't try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        //
        // Grab the first or second argument.
        //

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // The first required argument is a buffer, and the second is the
            // bit index (Integer).
            //

            ObjectType = AcpiObjectBuffer;
            if (Statement->ArgumentsAcquired == 1) {
                ObjectType = AcpiObjectInteger;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != ObjectType) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     ObjectType);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        //
        // If only the first argument has been parsed, then another one is
        // needed. If two have, then continue to parse the third.
        //

        if (Statement->ArgumentsAcquired == 1) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // The third argument is a name string, which can be parsed now.
        //

        if (Statement->ArgumentsAcquired == 2) {

            ASSERT(Statement->ArgumentsAcquired == 2);

            Statement->Argument[2] = AcpipParseNameString(Context);
            if (Statement->Argument[2] == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[2]->Type == AcpiObjectString);

    Name = Statement->Argument[2]->U.String.String;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("%s)", Name);
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectBuffer);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

    //
    // Create the buffer field object. Remember that additional data holds the
    // bit field length.
    //

    RtlZeroMemory(&BufferField, sizeof(ACPI_BUFFER_FIELD_OBJECT));
    BufferField.DestinationObject = Statement->Argument[0];
    BufferField.BitLength = Statement->AdditionalData;
    BufferField.BitOffset = Statement->Argument[1]->U.Integer.Value;

    //
    // Bitfields are specified in bits, but all other sized fields are
    // specified in bytes.
    //

    if (BufferField.BitLength > 1) {
        BufferField.BitOffset *= BITS_PER_BYTE;
    }

    Statement->Reduction = AcpipCreateNamespaceObject(
                                             Context,
                                             AcpiObjectBufferField,
                                             Name,
                                             &BufferField,
                                             sizeof(ACPI_BUFFER_FIELD_OBJECT));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDataStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates constant data coming from ByteData, WordData,
    DWordData, QWordData, and StringData.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR DataPointer;

    DataPointer = (PUCHAR)Context->AmlCode + Statement->AdditionalData;

    //
    // For string data, just create the string from the buffer pointer. A copy
    // will be made.
    //

    if (Statement->AdditionalData2 == 0) {
        Statement->Reduction = AcpipCreateNamespaceObject(
                                       Context,
                                       AcpiObjectString,
                                       NULL,
                                       DataPointer,
                                       RtlStringLength((PSTR)DataPointer) + 1);

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("\"");
            RtlDebugPrint(Statement->Reduction->U.String.String);
            RtlDebugPrint("\"");
        }

    //
    // The other types are integers.
    //

    } else {
        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          NULL,
                                                          0);

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        Statement->Reduction->U.Integer.Value = 0;
        RtlCopyMemory(&(Statement->Reduction->U.Integer.Value),
                      DataPointer,
                      Statement->AdditionalData2);

        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("0x%I64x", Statement->Reduction->U.Integer.Value);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDelayStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates both the Sleep and Stall statements.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    ULONG Operand;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                if (Statement->Type == AmlStatementSleep) {
                    RtlDebugPrint("Sleep (");

                } else {

                    ASSERT(Statement->Type == AmlStatementStall);

                    RtlDebugPrint("Stall (");
                }

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // For sleep statements, use the scheduler and relinquish the processor.
    //

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

    Operand = (ULONG)Statement->Argument[0]->U.Integer.Value;
    if (Statement->Type == AmlStatementSleep) {
        AcpipSleep(Operand);

    //
    // For stall statements, perform a busy spin.
    //

    } else {

        ASSERT(Statement->Type == AmlStatementStall);

        AcpipBusySpin(Operand);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDebugStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Debug statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Debug");
    }

    if (Context->ExecuteStatements != FALSE) {

        //
        // Create a debug object. Simple as that.
        //

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectDebug,
                                                          NULL,
                                                          NULL,
                                                          0);

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDereferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a "Dereference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Source;
    KSTATUS Status;

    //
    // Gather arguments.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("DerefOf (");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;

        } else {
            Statement->Argument[0] = NULL;
            if (Context->ExecuteStatements != FALSE) {
                Statement->Argument[0] = Context->PreviousStatement->Reduction;
                AcpipObjectAddReference(Statement->Argument[0]);
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(")");
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        Source = Statement->Argument[0];
        switch (Source->Type) {
        case AcpiObjectAlias:
            Statement->Reduction = Source->U.Alias.DestinationObject;
            AcpipObjectAddReference(Statement->Reduction);
            break;

        case AcpiObjectString:
            Statement->Reduction =
                               AcpipGetNamespaceObject(Source->U.String.String,
                                                       Context->CurrentScope);

            if (Statement->Reduction == NULL) {
                return STATUS_NOT_FOUND;
            }

            AcpipObjectAddReference(Statement->Reduction);
            break;

        case AcpiObjectBufferField:
            Status = AcpipReadFromBufferField(Context,
                                              Source,
                                              &(Statement->Reduction));

            if (!KSUCCESS(Status)) {
                return Status;
            }

            break;

        default:

            ASSERT(FALSE);

            return STATUS_UNEXPECTED_TYPE;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDeviceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Device declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR DeviceName;
    PACPI_OBJECT DeviceObject;

    if (Context->PreviousStatement == NULL) {
        Statement->SavedScope = NULL;

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        DeviceName = Statement->Argument[0]->U.String.String;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("Device (%s) {", DeviceName);
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Create the device object.
            //

            DeviceObject = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectDevice,
                                                      DeviceName,
                                                      NULL,
                                                      0);

            if (DeviceObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Make this device the current scope.
            //

            Statement->SavedScope = Context->CurrentScope;
            Context->CurrentScope = DeviceObject;
            Statement->Reduction = DeviceObject;
        }

        Context->IndentationLevel += 1;
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Pop this puppy off the stack.
    //

    Context->CurrentScope = Statement->SavedScope;
    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
        AcpipPrintIndentedNewLine(Context);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateDivideStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a divide statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ULONGLONG Dividend;
    ULONGLONG Divisor;
    PACPI_OBJECT NewArgument;
    ULONGLONG Quotient;
    ULONGLONG Remainder;
    PACPI_OBJECT RemainderObject;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                    RtlDebugPrint("Divide (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1) ||
                       (Statement->ArgumentsAcquired == 2)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if ((Statement->ArgumentsAcquired == 0) ||
                (Statement->ArgumentsAcquired == 1)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {

                ASSERT((Statement->ArgumentsAcquired == 2) ||
                       (Statement->ArgumentsAcquired == 3));

                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsAcquired < 3) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    if (Context->ExecuteStatements == FALSE) {

        ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

        return STATUS_SUCCESS;
    }

    //
    // The first time around, perform the divide and try to store the remainder.
    //

    if (Statement->ArgumentsAcquired == 3) {

        ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);
        ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

        Dividend = Statement->Argument[0]->U.Integer.Value;
        Divisor = Statement->Argument[1]->U.Integer.Value;
        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            Dividend &= 0xFFFFFFFF;
            Dividend &= 0xFFFFFFFF;
        }

        //
        // Fail to divide by 0, otherwise do the divide.
        //

        if (Divisor == 0) {
            return STATUS_DIVIDE_BY_ZERO;
        }

        Quotient = Dividend / Divisor;
        Remainder = Dividend % Divisor;
        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            Quotient &= 0xFFFFFFFF;
            Remainder &= 0xFFFFFFFF;
        }

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &Quotient,
                                                          sizeof(ULONGLONG));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        //
        // Store the remainder if supplied.
        //

        if (Statement->Argument[2] != NULL) {
            RemainderObject = AcpipCreateNamespaceObject(Context,
                                                         AcpiObjectInteger,
                                                         NULL,
                                                         &Remainder,
                                                         sizeof(ULONGLONG));

            if (RemainderObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            Status = AcpipPerformStoreOperation(Context,
                                                RemainderObject,
                                                Statement->Argument[2]);

            AcpipObjectReleaseReference(RemainderObject);
            if (!KSUCCESS(Status)) {
                return Status;
            }
        }

        ASSERT(Statement->ArgumentsAcquired < Statement->ArgumentsNeeded);

        Status = STATUS_MORE_PROCESSING_REQUIRED;

    //
    // The second time around store the quotient.
    //

    } else {

        ASSERT(Statement->ArgumentsAcquired == 4);

        //
        // Store the quotient in the target if supplied.
        //

        if (Statement->Argument[3] != NULL) {
            return AcpipPerformStoreOperation(Context,
                                              Statement->Reduction,
                                              Statement->Argument[3]);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateElseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Else statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    //
    // If this is the first time through, up the indentation level.
    //

    if (Context->PreviousStatement == NULL) {
        Context->IndentationLevel += 1;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("Else {");
        }
    }

    if (Context->ExecuteStatements != FALSE) {

        //
        // Check the result of the last If statement. Skip over the Else if the
        // IF succeeded.
        //

        if (Context->LastIfStatementResult != FALSE) {
            Context->CurrentOffset = Statement->AdditionalData;
        }
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateEventStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Event (creation) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR Name;

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    Name = Statement->Argument[0]->U.String.String;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Event (%s)", Name);
    }

    if (Context->ExecuteStatements != FALSE) {
        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectEvent,
                                                          Name,
                                                          NULL,
                                                          0);

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateExecutingMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Executing Method statement. This does not
    translate to a real ACPI opcode, but is a dummy object placed on the
    currently-executing statement stack so that return statements know how
    far to pop back up.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Method;
    PAML_STATEMENT PreviousStatement;
    KSTATUS Status;
    ULONGLONG Zero;

    ASSERT(Context->ExecuteStatements != FALSE);

    //
    // If the method context to wait for is set, wait until the method context
    // comes back to the original routine.
    //

    if (Statement->AdditionalData2 != (UINTN)NULL) {
        if ((UINTN)Context->CurrentMethod == Statement->AdditionalData2) {
            Statement->Reduction = Context->ReturnValue;
            AcpipObjectAddReference(Statement->Reduction);
            Context->IndentationLevel -= 1;
            if (Context->PrintStatements != FALSE) {
                AcpipPrintIndentedNewLine(Context);
                RtlDebugPrint("}");
            }

            return STATUS_SUCCESS;
        }

        //
        // While not in the spec, folklore has it that an old version of the
        // Windows AML interpreter allowed for AML methods without return
        // statements (even though ACPI said it was required). The behavior
        // instead was that the function returned a constant zero integer.
        // Many BIOSes took advantage of that, so now it basically is part
        // of the spec. If the current function seems to have just finished,
        // then pop its context.
        //

        if (Context->CurrentOffset == Context->AmlCodeSize) {
            if (Context->ReturnValue == NULL) {
                Zero = 0;
                Context->ReturnValue =
                                 AcpipCreateNamespaceObject(Context,
                                                            AcpiObjectInteger,
                                                            NULL,
                                                            &Zero,
                                                            sizeof(ULONGLONG));
            }

            AcpipPopExecutingStatements(Context, FALSE, FALSE);
            AcpipPopCurrentMethodContext(Context);
            Context->IndentationLevel -= 1;
            if (Context->PrintStatements != FALSE) {
                AcpipPrintIndentedNewLine(Context);
                RtlDebugPrint("}");
            }

            return STATUS_SUCCESS;
        }

        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    PreviousStatement = Context->PreviousStatement;

    //
    // The evaluate executing method statement is special in that it will not
    // get called once with a previous statement of NULL. Since it is a side
    // effect of another statement spitting out a reduction of type method, this
    // statement always gets passed a previous statement. The first time it's
    // called, the previous statement should have a pointer to the method
    // object. Use that to determine the argument count. Additional data was
    // initialized to 0 to indicate the first time this statement is being
    // evaluated.
    //

    if (Statement->AdditionalData == (UINTN)NULL) {

        ASSERT((PreviousStatement != NULL) &&
               (PreviousStatement->Reduction != NULL) &&
               (PreviousStatement->Reduction->Type == AcpiObjectMethod));

        Method = PreviousStatement->Reduction;
        Statement->AdditionalData = (UINTN)Method;
        Statement->ArgumentsNeeded = Method->U.Method.ArgumentCount;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint(" (");
        }

        if (Statement->ArgumentsNeeded != 0) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    //
    // If not all arguments are acquired, wait for them to come in, and collect
    // 'em.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if ((PreviousStatement == NULL) ||
            (PreviousStatement->Reduction == NULL)) {

            return STATUS_ARGUMENT_EXPECTED;
        }

        Statement->Argument[Statement->ArgumentsAcquired] =
                                                  PreviousStatement->Reduction;

        Statement->ArgumentsAcquired += 1;
        AcpipObjectAddReference(PreviousStatement->Reduction);
    }

    //
    // If all arguments are still not acquired, wait for more.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint(", ");
        }

        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    Context->IndentationLevel += 1;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(") {");
        AcpipPrintIndentedNewLine(Context);
    }

    //
    // Store the current method context in Additional Data 2, and use that
    // to determine when to complete this statement (complete the statement
    // when this context comes back.
    //

    Statement->AdditionalData2 = (UINTN)Context->CurrentMethod;

    //
    // Push the method execution context on as the current context.
    //

    Method = (PVOID)(UINTN)Statement->AdditionalData;
    Status = AcpipPushMethodOnExecutionContext(
                                             Context,
                                             Method,
                                             Method->U.Method.OsMutex,
                                             Method->U.Method.IntegerWidthIs32,
                                             Method->U.Method.AmlCode,
                                             Method->U.Method.AmlCodeSize,
                                             Statement->ArgumentsNeeded,
                                             Statement->Argument);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    //
    // If this was a C method or an empty method, complete it right away.
    //

    if (Method->U.Method.AmlCodeSize == 0) {
        Statement->Reduction = Context->ReturnValue;
        AcpipObjectAddReference(Statement->Reduction);
        Context->IndentationLevel -= 1;
        if (Context->PrintStatements != FALSE) {
            AcpipPrintIndentedNewLine(Context);
            RtlDebugPrint("}");
        }

        return STATUS_SUCCESS;
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

KSTATUS
AcpipEvaluateFatalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a fatal execution statement. This will stop the
    operating system.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR DataPointer;
    ULONGLONG FatalArgument;
    ULONG FatalCode;
    UCHAR FatalType;
    PACPI_OBJECT NewArgument;

    DataPointer = (PUCHAR)Context->AmlCode + Statement->AdditionalData;
    FatalType = *DataPointer;
    FatalCode = *(PULONG)(DataPointer + 1);

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Fatal (%x, %x, ", FatalType, FatalCode);

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            ASSERT(Statement->ArgumentsAcquired == 0);

            //
            // Convert the argument if it is there. The argument is
            // technically required, but since this is a fatal error, be a
            // bit more forgiving.
            //

            if (Context->PreviousStatement->Reduction != NULL) {

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    //
    // Die... so sad.
    //

    FatalArgument = 0;
    if ((Statement->Argument[0] != NULL) &&
        (Statement->Argument[0]->Type == AcpiObjectInteger)) {

        FatalArgument = Statement->Argument[0]->U.Integer.Value;
    }

    RtlDebugPrint("\n\n*** ACPI Fatal Error ***\n"
                  "Type: 0x%x, Code: 0x%x, Argument: 0x%I64x\n"
                  "Execution Context: 0x%x\n",
                  FatalType,
                  FatalCode,
                  FatalArgument,
                  Context);

    AcpipFatalError(ACPI_CRASH_FATAL_INSTRUCTION,
                    FatalType,
                    FatalCode,
                    FatalArgument);

    //
    // Execution will never get here, but the compiler will get sassy if there's
    // just nothing.
    //

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Field (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    PACPI_OBJECT OperationRegion;
    KSTATUS Status;

    OperationRegion = NULL;

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    if (Context->ExecuteStatements != FALSE) {

        //
        // Get the operation region this Field refers to.
        //

        OperationRegion = AcpipGetNamespaceObject(
                                       Statement->Argument[0]->U.String.String,
                                       Context->CurrentScope);

        if (OperationRegion == NULL) {
            return STATUS_NOT_FOUND;
        }
    }

    //
    // Parse the field list.
    //

    Status = AcpipParseFieldList(Context,
                                 Statement->Type,
                                 OperationRegion,
                                 NULL,
                                 NULL,
                                 NULL,
                                 NULL,
                                 Statement->AdditionalData,
                                 (UCHAR)Statement->AdditionalData2);

    return Status;
}

KSTATUS
AcpipEvaluateFindSetBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a find set left bit or find set right bit statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ULONGLONG Mask;
    PACPI_OBJECT NewArgument;
    ULONGLONG Result;
    ULONGLONG Value;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                switch (Statement->Type) {
                case AmlStatementFindSetLeftBit:
                    RtlDebugPrint("FindSetLeftBit (");
                    break;

                case AmlStatementFindSetRightBit:
                    RtlDebugPrint("FindSetRightBit (");
                    break;

                default:

                    ASSERT(FALSE);

                    return STATUS_NOT_SUPPORTED;
                }

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (Statement->ArgumentsAcquired == 0) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {

                ASSERT(Statement->ArgumentsAcquired == 1);

                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

    Value = Statement->Argument[0]->U.Integer.Value;
    if (Value != 0) {
        if (Statement->Type == AmlStatementFindSetLeftBit) {
            Result = 64;
            Mask = 0x8000000000000000ULL;
            if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
                Result = 32;
                Mask = 0x80000000;

                ASSERT(Value <= MAX_ULONG);
            }

            while ((Value & Mask) == 0) {
                Value = Value << 1;
                Result -= 1;
            }

        } else {

            ASSERT(Statement->Type == AmlStatementFindSetRightBit);

            Mask = 1;
            Result = 1;
            while ((Value & Mask) == 0) {
                Value = Value >> 1;
                Result += 1;
            }
        }
    }

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &Result,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[1] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[1]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateIfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an If statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("If (");

            } else {

                ASSERT(Statement->ArgumentsAcquired == 0);

                RtlDebugPrint(") {");
            }
        }

        if (Context->PreviousStatement == NULL) {
            Context->IndentationLevel += 1;
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
        }

        Statement->ArgumentsAcquired += 1;

        ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

        if (Context->ExecuteStatements != FALSE) {

            ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

            //
            // Evaluate the if statement by skipping the package length if it's
            // zero.
            //

            Statement->AdditionalData2 = TRUE;
            if (Statement->Argument[0]->U.Integer.Value == 0) {
                Statement->AdditionalData2 = FALSE;
                Context->CurrentOffset = Statement->AdditionalData;
            }
        }
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);

    //
    // Save the result of the If statement into the context so that an Else
    // can be properly evaluated if it's coming up next.
    //

    if (Statement->AdditionalData2 != FALSE) {
        Context->LastIfStatementResult = TRUE;

    } else {
        Context->LastIfStatementResult = FALSE;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateIncrementOrDecrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Increment or Decrement statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT IntegerObject;
    PACPI_OBJECT NewArgument;

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                if (Statement->Type == AmlStatementIncrement) {
                    RtlDebugPrint("Increment (");

                } else {

                    ASSERT(Statement->Type == AmlStatementDecrement);

                    RtlDebugPrint("Decrement (");
                }

            } else {
                RtlDebugPrint(")");
            }
        }

        //
        // If there is no previous statement, wait for the argument to come in.
        //

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, assume the argument would be there, and move on.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        //
        // Increment the reference count on the object (assuming it's there).
        //

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (NewArgument == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            AcpipObjectAddReference(NewArgument);
            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->ExecuteStatements != FALSE) {

        //
        // Perform an implicit conversion if necessary.
        //

        if (Statement->Argument[0]->Type != AcpiObjectInteger) {
            IntegerObject = AcpipConvertObjectType(Context,
                                                   Statement->Argument[0],
                                                   AcpiObjectInteger);

            if (IntegerObject == NULL) {
                return STATUS_CONVERSION_FAILED;
            }

        } else {
            IntegerObject = Statement->Argument[0];
            AcpipObjectAddReference(IntegerObject);
        }

        //
        // Do the increment or decrement.
        //

        if (Statement->Type == AmlStatementIncrement) {
            IntegerObject->U.Integer.Value += 1;

        } else {
            IntegerObject->U.Integer.Value -= 1;
        }

        //
        // Store the result back if this is not the argument. This also implies
        // a conversion back to the original type is necessary.
        //

        Statement->Reduction = IntegerObject;
        if (IntegerObject != Statement->Argument[0]) {
            AcpipPerformStoreOperation(Context,
                                       IntegerObject,
                                       Statement->Argument[0]);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateIndexFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an IndexField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    PACPI_OBJECT DataRegister;
    PACPI_OBJECT IndexRegister;
    KSTATUS Status;

    IndexRegister = NULL;
    DataRegister = NULL;

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectString);

    if (Context->ExecuteStatements != FALSE) {

        //
        // Get the Index field object.
        //

        IndexRegister = AcpipGetNamespaceObject(
                                       Statement->Argument[0]->U.String.String,
                                       Context->CurrentScope);

        if (IndexRegister == NULL) {
            return STATUS_NOT_FOUND;
        }

        //
        // Get the Data field object.
        //

        DataRegister = AcpipGetNamespaceObject(
                                       Statement->Argument[1]->U.String.String,
                                       Context->CurrentScope);

        if (DataRegister == NULL) {
            return STATUS_NOT_FOUND;
        }
    }

    //
    // Parse the field list.
    //

    Status = AcpipParseFieldList(Context,
                                 Statement->Type,
                                 NULL,
                                 NULL,
                                 NULL,
                                 IndexRegister,
                                 DataRegister,
                                 Statement->AdditionalData,
                                 (UCHAR)Statement->AdditionalData2);

    return Status;
}

KSTATUS
AcpipEvaluateIndexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Index statement, which creates a reference to the
    nth object in a buffer, string, or package.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ACPI_ALIAS_OBJECT Alias;
    ACPI_OBJECT_TYPE ArgumentType;
    ACPI_BUFFER_FIELD_OBJECT BufferField;
    PACPI_OBJECT NewArgument;
    ULONG PackageIndex;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Index (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if ((Statement->ArgumentsAcquired == 0) ||
                (Statement->ArgumentsAcquired == 1)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // The first argument must be a buffer, package, or string.
                //

                ArgumentType = AcpiObjectUninitialized;
                if (Statement->ArgumentsAcquired == 0) {
                    if ((NewArgument->Type != AcpiObjectBuffer) &&
                        (NewArgument->Type != AcpiObjectPackage) &&
                        (NewArgument->Type != AcpiObjectString)) {

                        ArgumentType = AcpiObjectBuffer;
                    }

                //
                // The second object must evaluate to an integer.
                //

                } else if (NewArgument->Type != AcpiObjectInteger) {
                    ArgumentType = AcpiObjectInteger;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (ArgumentType != AcpiObjectUninitialized) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         ArgumentType);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {

                ASSERT(Statement->ArgumentsAcquired == 2);

                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT((Statement->Argument[0]->Type == AcpiObjectBuffer) ||
           (Statement->Argument[0]->Type == AcpiObjectString) ||
           (Statement->Argument[0]->Type == AcpiObjectPackage));

    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

    //
    // For strings, create a field that points into the string.
    //

    if ((Statement->Argument[0]->Type == AcpiObjectString) ||
        (Statement->Argument[0]->Type == AcpiObjectBuffer)) {

        RtlZeroMemory(&BufferField, sizeof(ACPI_BUFFER_FIELD_OBJECT));
        BufferField.DestinationObject = Statement->Argument[0];
        BufferField.BitOffset =
                       Statement->Argument[1]->U.Integer.Value * BITS_PER_BYTE;

        BufferField.BitLength = BITS_PER_BYTE;
        Statement->Reduction = AcpipCreateNamespaceObject(
                                             Context,
                                             AcpiObjectBufferField,
                                             NULL,
                                             &BufferField,
                                             sizeof(ACPI_BUFFER_FIELD_OBJECT));

    } else if (Statement->Argument[0]->Type == AcpiObjectPackage) {
        RtlZeroMemory(&Alias, sizeof(ACPI_ALIAS_OBJECT));
        PackageIndex = (ULONG)Statement->Argument[1]->U.Integer.Value;
        Alias.DestinationObject = AcpipGetPackageObject(Statement->Argument[0],
                                                        PackageIndex,
                                                        TRUE);

        if (Alias.DestinationObject == NULL) {
            return STATUS_NOT_FOUND;
        }

        Statement->Reduction = AcpipCreateNamespaceObject(
                                                    Context,
                                                    AcpiObjectAlias,
                                                    NULL,
                                                    &Alias,
                                                    sizeof(ACPI_ALIAS_OBJECT));
    }

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[2] != NULL) {
        Status = AcpipPerformStoreOperation(Context,
                                            Statement->Reduction,
                                            Statement->Argument[2]);

        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateIntegerArithmeticStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates simple arithmetic operations that take two operands
    and a target.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    ULONGLONG Operand1;
    ULONGLONG Operand2;
    ULONGLONG ResultValue;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                switch (Statement->Type) {
                case AmlStatementAdd:
                    RtlDebugPrint("Add (");
                    break;

                case AmlStatementAnd:
                    RtlDebugPrint("And (");
                    break;

                case AmlStatementMod:
                    RtlDebugPrint("Mod (");
                    break;

                case AmlStatementMultiply:
                    RtlDebugPrint("Multiply (");
                    break;

                case AmlStatementNand:
                    RtlDebugPrint("Nand (");
                    break;

                case AmlStatementNor:
                    RtlDebugPrint("Nor (");
                    break;

                case AmlStatementOr:
                    RtlDebugPrint("Or (");
                    break;

                case AmlStatementSubtract:
                    RtlDebugPrint("Subtract (");
                    break;

                case AmlStatementShiftLeft:
                    RtlDebugPrint("ShiftLeft (");
                    break;

                case AmlStatementShiftRight:
                    RtlDebugPrint("ShiftRight (");
                    break;

                case AmlStatementXor:
                    RtlDebugPrint("XOr (");
                    break;

                default:

                    ASSERT(FALSE);

                    return STATUS_NOT_SUPPORTED;
                }

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if ((Statement->ArgumentsAcquired == 0) ||
                (Statement->ArgumentsAcquired == 1)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {

                ASSERT(Statement->ArgumentsAcquired == 2);

                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

    Operand1 = Statement->Argument[0]->U.Integer.Value;
    Operand2 = Statement->Argument[1]->U.Integer.Value;
    if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
        Operand1 &= 0xFFFFFFFF;
        Operand2 &= 0xFFFFFFFF;
    }

    switch (Statement->Type) {
    case AmlStatementAdd:
        ResultValue = Operand1 + Operand2;
        break;

    case AmlStatementAnd:
        ResultValue = Operand1 & Operand2;
        break;

    case AmlStatementMod:
        if (Operand2 == 0) {
            return STATUS_DIVIDE_BY_ZERO;
        }

        ResultValue = Operand1 % Operand2;
        break;

    case AmlStatementMultiply:
        ResultValue = Operand1 * Operand2;
        break;

    case AmlStatementNand:
        ResultValue = ~(Operand1 & Operand2);
        break;

    case AmlStatementNor:
        ResultValue = ~(Operand1 | Operand2);
        break;

    case AmlStatementOr:
        ResultValue = Operand1 | Operand2;
        break;

    case AmlStatementSubtract:
        ResultValue = Operand1 - Operand2;
        break;

    case AmlStatementShiftLeft:
        ResultValue = Operand1 << Operand2;
        break;

    case AmlStatementShiftRight:
        ResultValue = Operand1 >> Operand2;
        break;

    case AmlStatementXor:
        ResultValue = Operand1 ^ Operand2;
        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
        ResultValue &= 0xFFFFFFFF;
    }

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &ResultValue,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[2] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[2]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateIntegerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates statements that take no arguments and generate an
    integer output. This includes the constant statements Zero, One, and Ones,
    as well as the AML Revision and Timer statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS almost always.

    STATUS_INVALID_PARAMETER if this routine was called for the wrong statement
        type (an internal error for sure).

    STATUS_UNSUCCESSFUL if a namespace object could not be created.

--*/

{

    BOOL PrintParentheses;
    PACPI_OBJECT Reduction;
    ULONGLONG Value;

    Reduction = NULL;
    PrintParentheses = FALSE;
    switch (Statement->Type) {
    case AmlStatementZero:
        Value = 0;
        Reduction = &AcpiZero;
        break;

    case AmlStatementOne:
        Value = 1;
        Reduction = &AcpiOne;
        break;

    case AmlStatementOnes:
        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            Value = 0xFFFFFFFF;
            Reduction = &AcpiOnes32;

        } else {
            Value = 0xFFFFFFFFFFFFFFFFULL;
            Reduction = &AcpiOnes64;
        }

        break;

    case AmlStatementRevision:
        if (Context->PrintStatements != FALSE) {
            PrintParentheses = TRUE;
            RtlDebugPrint("Revision (");
        }

        Value = AML_REVISION;
        break;

    case AmlStatementTimer:
        if (Context->PrintStatements != FALSE) {
            PrintParentheses = TRUE;
            RtlDebugPrint("Timer (");
        }

        Value = AcpipGetTimerValue();
        break;

    default:

        ASSERT(FALSE);

        return STATUS_INVALID_PARAMETER;
    }

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("0x%I64x", Value);
        if (PrintParentheses != FALSE) {
            RtlDebugPrint(")");
        }
    }

    if (Context->ExecuteStatements != FALSE) {
        if (Reduction != NULL) {
            AcpipObjectAddReference(Reduction);

        } else {
            Reduction = AcpipCreateNamespaceObject(Context,
                                                   AcpiObjectInteger,
                                                   NULL,
                                                   &Value,
                                                   sizeof(ULONGLONG));

            if (Reduction == NULL) {
                return STATUS_UNSUCCESSFUL;
            }
        }

        Statement->Reduction = Reduction;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateLoadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Load statement, which adds the contents of a
    memory op-region as an SSDT to the namespace.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PDESCRIPTION_HEADER Buffer;
    PACPI_OBJECT BufferObject;
    UINTN BufferSize;
    PACPI_OBJECT DdbHandle;
    PACPI_OBJECT NewArgument;
    PACPI_OPERATION_REGION_OBJECT OperationRegion;
    PACPI_OBJECT Source;
    KSTATUS Status;

    Buffer = NULL;
    BufferObject = NULL;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Load (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            ASSERT(Statement->ArgumentsAcquired <= 1);

            if (Context->PreviousStatement->Reduction != NULL) {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // The source can either be an operation region itself or a field unit.
    //

    Source = Statement->Argument[0];
    DdbHandle = Statement->Argument[1];

    //
    // If it's an operation region, read it directly. It had better be a memory
    // region.
    //

    if (Source->Type == AcpiObjectOperationRegion) {
        OperationRegion = &(Source->U.OperationRegion);
        if ((OperationRegion->Space != OperationRegionSystemMemory) ||
            (OperationRegion->Length < sizeof(DESCRIPTION_HEADER))) {

            ASSERT(FALSE);

            return STATUS_INVALID_PARAMETER;
        }

        BufferSize = OperationRegion->Length;

        ASSERT(BufferSize == OperationRegion->Length);

        Buffer = AcpipAllocateMemory(BufferSize);
        if (Buffer == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        Status = OperationRegion->FunctionTable->Read(
                                       OperationRegion->OsContext,
                                       0,
                                       OperationRegion->Length * BITS_PER_BYTE,
                                       Buffer);

        ASSERT(KSUCCESS(Status));

    //
    // Convert the field unit into a buffer, which performs a read of the
    // op-region.
    //

    } else if (Source->Type == AcpiObjectFieldUnit) {
        BufferObject = AcpipConvertObjectType(Context,
                                              Source,
                                              AcpiObjectBuffer);

        if (BufferObject == NULL) {
            return STATUS_UNSUCCESSFUL;
        }

        //
        // Steal the buffer from the buffer object.
        //

        Buffer = BufferObject->U.Buffer.Buffer;
        BufferSize = BufferObject->U.Buffer.Length;
        BufferObject->U.Buffer.Buffer = NULL;
        BufferObject->U.Buffer.Length = 0;

    } else {
        RtlDebugPrint("ACPI: Load source should be an op-region or field.\n");

        ASSERT(FALSE);

        return STATUS_UNEXPECTED_TYPE;
    }

    //
    // Validate the buffer a bit.
    //

    if ((BufferSize < sizeof(DESCRIPTION_HEADER)) ||
        (BufferSize < Buffer->Length)) {

        Status = STATUS_DATA_LENGTH_MISMATCH;
        goto EvaluateLoadStatementEnd;
    }

    if (AcpipChecksumData(Buffer, Buffer->Length) != 0) {
        Status = STATUS_CHECKSUM_MISMATCH;
        goto EvaluateLoadStatementEnd;
    }

    //
    // Load the definition block synchronously.
    //

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("\nLoading Definition Block...\n");
    }

    Status = AcpiLoadDefinitionBlock(Buffer, DdbHandle);
    if (!KSUCCESS(Status)) {
        RtlDebugPrint("ACPI: Failed to execute Load: %d\n", Status);
        goto EvaluateLoadStatementEnd;
    }

    //
    // Run any _INI methods.
    //

    Status = AcpipRunInitializationMethods(NULL);
    if (!KSUCCESS(Status)) {
        goto EvaluateLoadStatementEnd;
    }

    //
    // The definition block owns the buffer now.
    //

    Buffer = NULL;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("\nDone Loading Definition Block\n");
    }

EvaluateLoadStatementEnd:
    if (BufferObject != NULL) {
        AcpipObjectReleaseReference(BufferObject);
    }

    if (Buffer != NULL) {
        AcpipFreeMemory(Buffer);
    }

    return Status;
}

KSTATUS
AcpipEvaluateLocalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates the LocalX opcodes.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ULONG LocalNumber;
    PACPI_OBJECT LocalObject;

    LocalNumber = (ULONG)Statement->AdditionalData;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Local%d", LocalNumber);
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        LocalObject = Context->CurrentMethod->LocalVariable[LocalNumber];

        //
        // Create an uninitialized object if none exists yet.
        //

        if (LocalObject == NULL) {
            LocalObject = AcpipCreateNamespaceObject(Context,
                                                     AcpiObjectUninitialized,
                                                     NULL,
                                                     NULL,
                                                     0);

            if (LocalObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            Context->CurrentMethod->LocalVariable[LocalNumber] = LocalObject;
        }

        Statement->Reduction = LocalObject;
        AcpipObjectAddReference(LocalObject);
        Context->CurrentMethod->LastLocalIndex = LocalNumber;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateLogicalExpressionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates logical binary operators (Logical and, equal,
    greater, less, and or).

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    ULONGLONG Operand1;
    ULONGLONG Operand2;
    ULONGLONG ResultValue;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                switch (Statement->Type) {
                case AmlStatementLogicalAnd:
                    RtlDebugPrint("LAnd (");
                    break;

                case AmlStatementLogicalEqual:
                    RtlDebugPrint("LEqual (");
                    break;

                case AmlStatementLogicalGreater:
                    RtlDebugPrint("LGreater (");
                    break;

                case AmlStatementLogicalLess:
                    RtlDebugPrint("LLess (");
                    break;

                case AmlStatementLogicalOr:
                    RtlDebugPrint("LOr (");
                    break;

                default:

                    ASSERT(FALSE);

                    return STATUS_NOT_SUPPORTED;
                }

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

    Operand1 = Statement->Argument[0]->U.Integer.Value;
    Operand2 = Statement->Argument[1]->U.Integer.Value;
    ResultValue = FALSE;
    switch (Statement->Type) {
    case AmlStatementLogicalAnd:
        if ((Operand1 != 0) && (Operand2 != 0)) {
            ResultValue = TRUE;
        }

        break;

    case AmlStatementLogicalEqual:
        if (Operand1 == Operand2) {
            ResultValue = TRUE;
        }

        break;

    case AmlStatementLogicalGreater:
        if (Operand1 > Operand2) {
            ResultValue = TRUE;
        }

        break;

    case AmlStatementLogicalLess:
        if (Operand1 < Operand2) {
            ResultValue = TRUE;
        }

        break;

    case AmlStatementLogicalOr:
        if ((Operand1 != 0) || (Operand2 != 0)) {
            ResultValue = TRUE;
        }

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &ResultValue,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateLogicalNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates logical NOT operator.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    ULONGLONG ResultValue;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("LNot (");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

    ResultValue = FALSE;
    if (Statement->Argument[0]->U.Integer.Value == 0) {
        ResultValue = TRUE;
    }

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &ResultValue,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateMatchStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Match statement, which iterates over a package
    doing some simple comparisons.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;
    ULONGLONG ItemCount;
    ULONGLONG ItemIndex;
    BOOL Match;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT Operand1;
    PACPI_OBJECT Operand2;
    ACPI_MATCH_OPERATOR Operator1;
    ACPI_MATCH_OPERATOR Operator2;
    PACPI_OBJECT Package;
    PACPI_OBJECT PackageElement;
    PACPI_OBJECT StartIndex;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Match (");

            } else if (Statement->ArgumentsAcquired < 3) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // An argument is required.
            //

            if (NewArgument == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // The first object must be a package.
            //

            if (Statement->ArgumentsAcquired == 0) {
                if (NewArgument->Type != AcpiObjectPackage) {
                    return STATUS_CONVERSION_FAILED;
                }

                AcpipObjectAddReference(NewArgument);

            //
            // The second and third arguments must be an integer, buffer, or
            // string. Convert that to an integer. The fourth argument
            // (StartIndex) is also an integer.
            //

            } else if ((Statement->ArgumentsAcquired == 1) ||
                       (Statement->ArgumentsAcquired == 2) ||
                       (Statement->ArgumentsAcquired == 3)) {

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                } else {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            //
            // Save the argument and return if not all arguments have been
            // collected yet.
            //

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        //
        // After the first and second arguments come match opcodes. Pull
        // those out and store them in additional data.
        //

        if ((Statement->ArgumentsAcquired == 1) ||
            (Statement->ArgumentsAcquired == 2)) {

            InstructionPointer =
                       (PUCHAR)(Context->AmlCode) + Context->CurrentOffset;

            if (Context->CurrentOffset >= Context->AmlCodeSize) {
                return STATUS_MALFORMED_DATA_STREAM;
            }

            Statement->AdditionalData = (Statement->AdditionalData << 8) |
                                        *InstructionPointer;

            if (*InstructionPointer >= MatchOperatorCount) {
                return STATUS_MALFORMED_DATA_STREAM;
            }

            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint("%s, ",
                              AcpiMatchOpcodeString[*InstructionPointer]);
            }

            Context->CurrentOffset += 1;
        }

        if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Assert that all the arguments are the expected types.
    //

    Package = Statement->Argument[0];
    Operand1 = Statement->Argument[1];
    Operand2 = Statement->Argument[2];
    StartIndex = Statement->Argument[3];
    Operator1 = (Statement->AdditionalData >> 8) & 0xFF;
    Operator2 = Statement->AdditionalData & 0xFF;

    ASSERT((Package->Type == AcpiObjectPackage) &&
           (StartIndex->Type == AcpiObjectInteger));

    ASSERT((Operand1->Type == AcpiObjectInteger) &&
           (Operand2->Type == AcpiObjectInteger));

    ASSERT((Operator1 < MatchOperatorCount) &&
           (Operator2 < MatchOperatorCount));

    //
    // Perform the match operation.
    //

    ItemIndex = StartIndex->U.Integer.Value;
    ItemCount = Package->U.Package.ElementCount;
    while (ItemIndex < ItemCount) {
        PackageElement = AcpipGetPackageObject(Package, ItemIndex, FALSE);
        Match = AcpipEvaluateMatchComparison(Context,
                                             PackageElement,
                                             Operand1,
                                             Operator1,
                                             Operand2,
                                             Operator2);

        if (Match != FALSE) {
            break;
        }

        ItemIndex += 1;
    }

    //
    // If a match was never found (as evidenced by the index being all the way
    // at the end), the ACPI says to return the constant "Ones".
    //

    if (ItemIndex == ItemCount) {
        Statement->Reduction = &AcpiOnes64;
        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            Statement->Reduction = &AcpiOnes32;
        }

        AcpipObjectAddReference(Statement->Reduction);

    //
    // Otherwise, return the result value.
    //

    } else {
        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ItemIndex,
                                                          sizeof(ULONGLONG));

        if (Statement->Reduction == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Method declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ACPI_METHOD_OBJECT Method;
    UCHAR MethodFlags;
    PACPI_OBJECT MethodObject;
    PSTR Name;

    //
    // If the previous statement is NULL, this is the first time through.
    //

    if (Context->PreviousStatement == NULL) {

        ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        //
        // Get the method flags out of additional data 2.
        //

        MethodFlags = (UCHAR)Statement->AdditionalData2;

        //
        // Initialize the method structure.
        //

        RtlZeroMemory(&Method, sizeof(ACPI_METHOD_OBJECT));
        Method.IntegerWidthIs32 = Context->CurrentMethod->IntegerWidthIs32;
        Method.ArgumentCount = MethodFlags & METHOD_ARGUMENT_COUNT_MASK;
        Method.Serialized = FALSE;
        if ((MethodFlags & METHOD_SERIALIZED_FLAG) != 0) {
            Method.Serialized = TRUE;
        }

        Method.SyncLevel = (MethodFlags & METHOD_SYNC_LEVEL_MASK) >>
                           METHOD_SYNC_LEVEL_SHIFT;

        Method.AmlCode = (PUCHAR)Context->AmlCode + Context->CurrentOffset;

        //
        // Additional Data stored the end offset, so the size is the end offset
        // minus the current offset.
        //

        Method.AmlCodeSize = Statement->AdditionalData - Context->CurrentOffset;
        Name = Statement->Argument[0]->U.String.String;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("Method (%s, 0x%02x)", Name, MethodFlags);
        }

        //
        // Create the object if execution is enabled.
        //

        if (Context->ExecuteStatements != FALSE) {
            MethodObject = AcpipCreateNamespaceObject(
                                                   Context,
                                                   AcpiObjectMethod,
                                                   Name,
                                                   &Method,
                                                   sizeof(ACPI_METHOD_OBJECT));

            if (MethodObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Advance the current offset to the end of the method, as it's not
            // being executed now, and complete the statement.
            //

            Context->CurrentOffset = Statement->AdditionalData;
            return STATUS_SUCCESS;

        //
        // If the context is printing but not executing, add to the indentation
        // level and delve into the function for execution.
        //

        } else {
            Context->IndentationLevel += 1;
            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint(" {");
            }
        }
    }

    //
    // Wait for the end of the routine.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Decrease the indentation level and print the closing curly brace if
    // needed.
    //

    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateMidStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a mid statement, which splits a string up.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PVOID Buffer;
    ULONGLONG BufferLength;
    ULONGLONG MidIndex;
    ULONGLONG MidLength;
    PACPI_OBJECT NewArgument;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Mid (");

            } else if ((Statement->ArgumentsAcquired == 0) ||
                       (Statement->ArgumentsAcquired == 1) ||
                       (Statement->ArgumentsAcquired == 2)) {

                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (Statement->ArgumentsAcquired == 0) {
                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                if ((NewArgument->Type != AcpiObjectString) &&
                    (NewArgument->Type != AcpiObjectBuffer)) {

                    //
                    // Perform an implicit conversion if needed.
                    //

                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectBuffer);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else if ((Statement->ArgumentsAcquired == 1) ||
                       (Statement->ArgumentsAcquired == 2)) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            //
            // Parse the target argument.
            //

            } else {

                ASSERT(Statement->ArgumentsAcquired == 3);

                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT((Statement->Argument[0]->Type == AcpiObjectString) ||
           (Statement->Argument[0]->Type == AcpiObjectBuffer));

    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);
    ASSERT(Statement->Argument[2]->Type == AcpiObjectInteger);

    //
    // The Mid statement gets a portion of a string or buffer at an offset
    // (Index) with a length. If the mid statement tries to go over, the
    // resulting buffer is clipped with the original. If the offset is beyond
    // the end of the buffer, an empty buffer is created.
    //

    MidIndex = Statement->Argument[1]->U.Integer.Value;
    MidLength = Statement->Argument[2]->U.Integer.Value;
    BufferLength = 0;
    if (Statement->Argument[0]->Type == AcpiObjectString) {
        Buffer = Statement->Argument[0]->U.String.String;
        if (Buffer != NULL) {
            BufferLength = RtlStringLength(Buffer) + 1;
        }

    } else {
        Buffer = Statement->Argument[0]->U.Buffer.Buffer;
        BufferLength = Statement->Argument[0]->U.Buffer.Length;
    }

    //
    // Cap the mid statement from going over the buffer.
    //

    if (MidIndex >= BufferLength) {
        MidIndex = 0;
        MidLength = 0;
    }

    if (MidIndex + MidLength > BufferLength) {
        MidLength = BufferLength - MidIndex;
    }

    //
    // Create the mid buffer.
    //

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      Buffer + MidIndex,
                                                      MidLength);

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[3] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[2]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateMutexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Mutex (creation) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR NameString;
    UCHAR SyncLevel;

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    NameString =  Statement->Argument[0]->U.String.String;
    SyncLevel = Statement->AdditionalData & MUTEX_FLAG_SYNC_LEVEL_MASK;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("Mutex (%s, %d)", NameString, SyncLevel);
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Create the mutex object.
    //

    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectMutex,
                                                      NameString,
                                                      NULL,
                                                      0);

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateNameStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Name statement, which creates a new named object
    in the namespace given an existing one.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NamedObject;
    PACPI_OBJECT NewArgument;
    PACPI_OBJECT ObjectWithContents;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Name (%s,",
                              Statement->Argument[0]->U.String.String);

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            AcpipObjectAddReference(NewArgument);
            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    //
    // The arguments should be all gathered.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    //
    // Create the new object without the stuff in it.
    //

    ObjectWithContents = Statement->Argument[1];
    NamedObject = AcpipCreateNamespaceObject(
                                        Context,
                                        AcpiObjectUninitialized,
                                        Statement->Argument[0]->U.String.String,
                                        NULL,
                                        0);

    if (NamedObject == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Put the stuff from the other object in this new named object.
    //

    Status = AcpipPerformStoreOperation(Context,
                                        ObjectWithContents,
                                        NamedObject);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Statement->Reduction = NamedObject;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateNameStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a NameString statement, which evaluates to an
    object that is expected to exist in the namespace.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR Name;
    ACPI_UNRESOLVED_NAME_OBJECT UnresolvedName;

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);
    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    Name = Statement->Argument[0]->U.String.String;
    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(Name);
    }

    //
    // Get the object if the interpreter is executing statements.
    //

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        Statement->Reduction = AcpipGetNamespaceObject(Name,
                                                       Context->CurrentScope);

        //
        // If the object could not be found, then a name is being referenced
        // before it is defined. In certain situations this is alright, such as
        // the definition of a package object during a load operation. Create
        // an unresolved name object to remember to re-evaluate this name
        // when the object is referenced.
        //

        if (Statement->Reduction == NULL) {
            RtlZeroMemory(&UnresolvedName, sizeof(ACPI_UNRESOLVED_NAME_OBJECT));
            UnresolvedName.Name = Name;
            UnresolvedName.Scope = Context->CurrentScope;
            Statement->Reduction = AcpipCreateNamespaceObject(
                                          Context,
                                          AcpiObjectUnresolvedName,
                                          NULL,
                                          &UnresolvedName,
                                          sizeof(ACPI_UNRESOLVED_NAME_OBJECT));

            if (Statement->Reduction == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint(" ?");
            }

        } else {
            AcpipObjectAddReference(Statement->Reduction);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateNoOpStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a No-Op statement, which is really quite easy since
    it doesn't do anything.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    //
    // Finally, an easy one!
    //

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint("NoOp");
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateNotifyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Notify (the operating system) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Notify (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // The first argument needs to be a Thermal Zone, Processor, or
            // Device.
            //

            if (Statement->ArgumentsAcquired == 0) {
                if ((NewArgument->Type != AcpiObjectProcessor) &&
                    (NewArgument->Type != AcpiObjectThermalZone) &&
                    (NewArgument->Type != AcpiObjectDevice)) {

                    return STATUS_INVALID_PARAMETER;

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            //
            // The second argument needs to come out as an integer.
            //

            } else {

                ASSERT(Statement->ArgumentsAcquired == 1);

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT((Statement->Argument[0]->Type == AcpiObjectProcessor) ||
           (Statement->Argument[0]->Type == AcpiObjectThermalZone) ||
           (Statement->Argument[0]->Type == AcpiObjectDevice));

    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

    //
    // Pass the notification on to the rest of the system.
    //

    Status = AcpipNotifyOperatingSystem(
                                      Statement->Argument[0],
                                      Statement->Argument[1]->U.Integer.Value);

    return Status;
}

KSTATUS
AcpipEvaluateNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates bitwise NOT operator.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    ULONGLONG ResultValue;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Not (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (Statement->ArgumentsAcquired == 0) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (NewArgument->Type != AcpiObjectInteger) {
                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {
                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

    //
    // Seems like so much build up just for this...
    //

    ResultValue = ~(Statement->Argument[0]->U.Integer.Value);
    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &ResultValue,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    if (Statement->Argument[1] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[0]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateObjectTypeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Object Type statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    PACPI_OBJECT Object;
    ULONGLONG ObjectType;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("ObjectType (");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (Statement->ArgumentsAcquired == 0) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                AcpipObjectAddReference(NewArgument);

            } else {
                if (NewArgument != NULL) {
                    AcpipObjectAddReference(NewArgument);
                }
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    Object = Statement->Argument[0];
    while (Object->Type == AcpiObjectAlias) {
        Object = Object->U.Alias.DestinationObject;
    }

    ObjectType = Object->Type;
    Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                      AcpiObjectInteger,
                                                      NULL,
                                                      &ObjectType,
                                                      sizeof(ULONGLONG));

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateOperationRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Operation Region statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR Name;
    PACPI_OBJECT NewArgument;
    ULONGLONG RegionLength;
    ULONGLONG RegionOffset;
    ACPI_OPERATION_REGION_SPACE RegionSpace;
    KSTATUS Status;

    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("OpRegion (%s, %x, ",
                              Statement->Argument[0]->U.String.String,
                              Statement->AdditionalData);

            } else if (Statement->ArgumentsAcquired == 1) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    ASSERT(Statement->Argument[0]->Type == AcpiObjectString);
    ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);
    ASSERT(Statement->Argument[2]->Type == AcpiObjectInteger);

    Name = Statement->Argument[0]->U.String.String;
    RegionOffset = Statement->Argument[1]->U.Integer.Value;
    RegionLength = Statement->Argument[2]->U.Integer.Value;
    RegionSpace = (UCHAR)Statement->AdditionalData;
    Status = AcpipCreateOperationRegion(Context,
                                        Name,
                                        RegionSpace,
                                        RegionOffset,
                                        RegionLength);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluatePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Package or Variable Package statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ULONG BufferSize;
    PACPI_OBJECT NewArgument;

    if (Context->PreviousStatement == NULL) {
        Context->IndentationLevel += 1;
        if (Context->PrintStatements != FALSE) {
            if (Statement->Type == AmlStatementPackage) {
                RtlDebugPrint("Package (%d) {", Statement->AdditionalData2);
                AcpipPrintIndentedNewLine(Context);

            } else {

                ASSERT(Statement->Type == AmlStatementVariablePackage);

                RtlDebugPrint("VarPackage (");
            }
        }
    }

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument. This only occurs in a variable package when looking
    // for the package size.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            ASSERT(Statement->ArgumentsAcquired == 0);

            //
            // Fail if there is no argument there.
            //

            if (NewArgument == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        //
        // Finish printing the header for the variable package.
        //

        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint(") {");
            AcpipPrintIndentedNewLine(Context);
        }

        //
        // Assuming the length isn't 0, wait for the first package object.
        //

        if (Context->CurrentOffset != Statement->AdditionalData) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    //
    // If not at the end, add this to the collection.
    //

    if ((Context->PrintStatements != FALSE) &&
        (Context->CurrentOffset != Statement->AdditionalData) &&
        (Context->PreviousStatement != NULL)) {

        RtlDebugPrint(", ");
        AcpipPrintIndentedNewLine(Context);
    }

    if (Context->ExecuteStatements != FALSE) {

        //
        // If the object has never been created before, create it now.
        //

        if (Statement->Reduction == NULL) {
            BufferSize = Statement->AdditionalData2 * sizeof(PACPI_OBJECT);
            Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                              AcpiObjectPackage,
                                                              NULL,
                                                              NULL,
                                                              BufferSize);

            if (Statement->Reduction == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Additional data 2 now becomes a counter of the current
            // element.
            //

            Statement->AdditionalData2 = 0;
        }

        //
        // Add the object to the package/array.
        //

        if (Context->PreviousStatement != NULL) {
            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            AcpipSetPackageObject(Statement->Reduction,
                                  Statement->AdditionalData2,
                                  Context->PreviousStatement->Reduction);

            Statement->AdditionalData2 += 1;
        }
    }

    if (Context->CurrentOffset == Statement->AdditionalData) {
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("}");
        }

        Context->IndentationLevel -= 1;
        return STATUS_SUCCESS;
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

KSTATUS
AcpipEvaluatePowerResourceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Power Resource declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR DataPointer;
    PSTR Name;
    ACPI_POWER_RESOURCE_OBJECT PowerResource;
    PACPI_OBJECT PowerResourceObject;

    if (Context->PreviousStatement == NULL) {
        Statement->SavedScope = NULL;

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        Name = Statement->Argument[0]->U.String.String;

        //
        // Get the system level and resource order.
        //

        DataPointer = Context->AmlCode + Statement->AdditionalData2;
        PowerResource.SystemLevel = *DataPointer;
        PowerResource.ResourceOrder = *(PUSHORT)(DataPointer + 1);
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("PowerResource (%s, %d, %d) {",
                          Name,
                          PowerResource.SystemLevel,
                          PowerResource.ResourceOrder);
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Create the power resource object.
            //

            PowerResourceObject = AcpipCreateNamespaceObject(
                                           Context,
                                           AcpiObjectPowerResource,
                                           Name,
                                           &PowerResource,
                                           sizeof(ACPI_POWER_RESOURCE_OBJECT));

            if (PowerResourceObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Make this device the current scope.
            //

            Statement->SavedScope = Context->CurrentScope;
            Context->CurrentScope = PowerResourceObject;
            Statement->Reduction = PowerResourceObject;
        }

        Context->IndentationLevel += 1;
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Pop this puppy off the stack.
    //

    Context->CurrentScope = Statement->SavedScope;
    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateProcessorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Power Resource declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PUCHAR DataPointer;
    PSTR DeviceName;
    ACPI_PROCESSOR_OBJECT Processor;
    PACPI_OBJECT ProcessorObject;

    if (Context->PreviousStatement == NULL) {
        Statement->SavedScope = NULL;

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        DeviceName = Statement->Argument[0]->U.String.String;

        //
        // Get the processor ID and processor block register information.
        //

        RtlZeroMemory(&Processor, sizeof(ACPI_PROCESSOR_OBJECT));
        DataPointer = Context->AmlCode + Statement->AdditionalData2;
        Processor.ProcessorId = *DataPointer;
        DataPointer += sizeof(BYTE);
        Processor.ProcessorBlockAddress = *(PULONG)DataPointer;
        DataPointer += sizeof(ULONG);
        Processor.ProcessorBlockLength = *DataPointer;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("Processor (%s, %d, 0x%x, %d) {",
                          DeviceName,
                          Processor.ProcessorId,
                          Processor.ProcessorBlockAddress,
                          Processor.ProcessorBlockLength);
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Create the processor object.
            //

            ProcessorObject = AcpipCreateNamespaceObject(
                                                Context,
                                                AcpiObjectProcessor,
                                                DeviceName,
                                                &Processor,
                                                sizeof(ACPI_PROCESSOR_OBJECT));

            if (ProcessorObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Make this device the current scope.
            //

            Statement->SavedScope = Context->CurrentScope;
            Context->CurrentScope = ProcessorObject;
            Statement->Reduction = ProcessorObject;
        }

        Context->IndentationLevel += 1;
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Pop this puppy off the stack.
    //

    Context->CurrentScope = Statement->SavedScope;
    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an "Reference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("RefOf (");

            } else {
                RtlDebugPrint(")");
            }
        }

        //
        // If there is no previous statement, wait for the argument to come in.
        //

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;

        //
        // Increment the reference count on the object.
        //

        } else {
            if (Context->ExecuteStatements == FALSE) {
                Statement->Argument[0] = NULL;

            } else {
                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                Statement->Argument[0] = Context->PreviousStatement->Reduction;
                AcpipObjectAddReference(Context->PreviousStatement->Reduction);
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->ExecuteStatements != FALSE) {
        Statement->Reduction = AcpipCreateNamespaceObject(
                                                     Context,
                                                     AcpiObjectAlias,
                                                     NULL,
                                                     &(Statement->Argument[0]),
                                                     sizeof(PACPI_OBJECT));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateReturnStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Return statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Return (");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;

        //
        // Increment the reference count on the object.
        //

        } else {
            if (Context->ExecuteStatements != FALSE) {
                if (Context->PreviousStatement->Reduction != NULL) {
                    Statement->Argument[0] =
                                         Context->PreviousStatement->Reduction;

                    AcpipObjectAddReference(Statement->Argument[0]);
                }

            } else {
                Statement->Argument[0] = NULL;
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(")");
    }

    //
    // Return from the function.
    //

    if (Context->ExecuteStatements != FALSE) {

        //
        // If there was an old return value there (possibly from a nested
        // function call), release it.
        //

        if (Context->ReturnValue != NULL) {
            AcpipObjectReleaseReference(Context->ReturnValue);
        }

        Context->ReturnValue = Statement->Argument[0];
        AcpipObjectAddReference(Context->ReturnValue);
        AcpipPopExecutingStatements(Context, FALSE, FALSE);
        AcpipPopCurrentMethodContext(Context);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateScopeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Device declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Object;
    PSTR ScopeName;

    if (Context->PreviousStatement == NULL) {
        Statement->SavedScope = NULL;

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        ScopeName = Statement->Argument[0]->U.String.String;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("Scope (%s) {", ScopeName);
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Go find the object.
            //

            Object = AcpipGetNamespaceObject(ScopeName, Context->CurrentScope);

            //
            // Make this device the current scope.
            //

            Statement->SavedScope = Context->CurrentScope;
            Context->CurrentScope = Object;
        }

        Context->IndentationLevel += 1;
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Pop this puppy off the stack.
    //

    Context->CurrentScope = Statement->SavedScope;
    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    Statement->Reduction = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateSizeOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a "Size Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT Object;
    ULONGLONG Size;

    //
    // Gather arguments.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("SizeOf (");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;

        } else {
            Statement->Argument[0] = NULL;
            if (Context->ExecuteStatements != FALSE) {
                Statement->Argument[0] = Context->PreviousStatement->Reduction;
                AcpipObjectAddReference(Statement->Argument[0]);
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(")");
    }

    Object = NULL;
    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {

        //
        // If the object is an alias, use the destination.
        //

        Object = Statement->Argument[0];
        while (Object->Type == AcpiObjectAlias) {
            Object = Statement->Argument[0]->U.Alias.DestinationObject;
        }

        Size = 0;
        switch (Object->Type) {
        case AcpiObjectString:
            if (Object->U.String.String != NULL) {
                Size = RtlStringLength(Object->U.String.String);
            }

            break;

        case AcpiObjectBuffer:
            Size = Object->U.Buffer.Length;
            break;

        case AcpiObjectPackage:
            Size = Object->U.Package.ElementCount;
            break;

        default:

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        //
        // Create the integer result.
        //

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &Size,
                                                          sizeof(ULONGLONG));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateStoreStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Store statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;
    KSTATUS Status;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Store (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            AcpipObjectAddReference(NewArgument);
            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // All arguments have been acquired.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Store the first argument into the second. The reduction of the statement
    // is the second operand.
    //

    Status = AcpipPerformStoreOperation(Context,
                                        Statement->Argument[0],
                                        Statement->Argument[1]);

    if (!KSUCCESS(Status)) {
        return Status;
    }

    Statement->Reduction = Statement->Argument[1];
    AcpipObjectAddReference(Statement->Reduction);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateSyncObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Release (mutex), Reset (event), or Signal (event)
    statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NameString;
    PACPI_OBJECT NewArgument;

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                if (Statement->Type == AmlStatementRelease) {
                    RtlDebugPrint("Release (");

                } else if (Statement->Type == AmlStatementSignal) {
                    RtlDebugPrint("Signal (");

                } else {

                    ASSERT(Statement->Type == AmlStatementReset);

                    RtlDebugPrint("Reset (");
                }

            }
        }

        //
        // The argument for Release, Reset, and Signal is a "SuperName", which
        // is a SimpleName, DebugOp, or Type6Opcode. If this is the first time
        // through, try to parse a name string.
        //

        if (Context->PreviousStatement == NULL) {
            NameString = AcpipParseNameString(Context);
            if (NameString == NULL) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint(NameString->U.String.String);
            }

            if (Context->ExecuteStatements != FALSE) {
                Statement->Argument[0] =
                           AcpipGetNamespaceObject(NameString->U.String.String,
                                                   Context->CurrentScope);

                if (Statement->Argument[0] == NULL) {
                    return STATUS_NOT_FOUND;
                }

            } else {
                Statement->Argument[0] = NULL;
            }

            if (Statement->Argument[0] != NULL) {
                AcpipObjectAddReference(Statement->Argument[0]);
            }

            Statement->ArgumentsAcquired += 1;
            AcpipObjectReleaseReference(NameString);

        //
        // Increment the reference count on the object.
        //

        } else {
            if (Context->ExecuteStatements != FALSE) {
                NewArgument = Context->PreviousStatement->Reduction;
                if (NewArgument == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
                AcpipObjectAddReference(NewArgument);
            }

            Statement->ArgumentsAcquired += 1;
        }
    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    if (Context->PrintStatements != FALSE) {
        RtlDebugPrint(")");
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {
        if (Statement->Type == AmlStatementRelease) {

            ASSERT(Statement->Argument[0]->Type == AcpiObjectMutex);

            AcpipReleaseMutex(Context,
                              Statement->Argument[0]->U.Mutex.OsMutex);

        } else if (Statement->Type == AmlStatementSignal) {

            ASSERT(Statement->Argument[0]->Type == AcpiObjectEvent);

            AcpipSignalEvent(Statement->Argument[0]->U.Event.OsEvent);

        } else {

            ASSERT(Statement->Type == AmlStatementReset);
            ASSERT(Statement->Argument[0]->Type == AcpiObjectEvent);

            AcpipResetEvent(Statement->Argument[0]->U.Event.OsEvent);
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateThermalZoneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a Thermal Zone declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PSTR Name;
    PACPI_OBJECT ThermalZoneObject;

    if (Context->PreviousStatement == NULL) {
        Statement->SavedScope = NULL;

        ASSERT(Statement->Argument[0]->Type == AcpiObjectString);

        Name = Statement->Argument[0]->U.String.String;
        if (Context->PrintStatements != FALSE) {
            RtlDebugPrint("ThermalZone (%s) {", Name);
        }

        if (Context->ExecuteStatements != FALSE) {

            //
            // Create the thermal zone object.
            //

            ThermalZoneObject = AcpipCreateNamespaceObject(
                                                       Context,
                                                       AcpiObjectPowerResource,
                                                       Name,
                                                       NULL,
                                                       0);

            if (ThermalZoneObject == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Make this device the current scope.
            //

            Statement->SavedScope = Context->CurrentScope;
            Context->CurrentScope = ThermalZoneObject;
            Statement->Reduction = ThermalZoneObject;
        }

        Context->IndentationLevel += 1;
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    //
    // Pop this puppy off the stack.
    //

    Context->CurrentScope = Statement->SavedScope;
    Context->IndentationLevel -= 1;
    if (Context->PrintStatements != FALSE) {
        AcpipPrintIndentedNewLine(Context);
        RtlDebugPrint("}");
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateToFormatStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates the "To" something and "From" something statements,
    including ToBCD, ToBuffer, ToDecimalString, ToHexString, ToInteger,
    ToString, ToUUID, Unicode, and FromBcd.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    ULONGLONG BcdValue;
    ULONG ByteCount;
    ULONG ByteIndex;
    PUCHAR BytePointer;
    ULONG ByteStringSize;
    ULONG Digit;
    ULONGLONG IntegerValue;
    PACPI_OBJECT NewArgument;
    ULONG Nibble;
    CHAR ResultString[MAX_DECIMAL_STRING_LENGTH];
    ULONGLONG ResultValue;
    PSTR String;
    ULONG StringSize;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                switch (Statement->Type) {
                case AmlStatementFromBcd:
                    RtlDebugPrint("FromBCD (");
                    break;

                case AmlStatementToBcd:
                    RtlDebugPrint("ToBCD (");
                    break;

                case AmlStatementToBuffer:
                    RtlDebugPrint("ToBuffer (");
                    break;

                case AmlStatementToDecimalString:
                    RtlDebugPrint("ToDecimalString (");
                    break;

                case AmlStatementToHexString:
                    RtlDebugPrint("ToHexString (");
                    break;

                case AmlStatementToInteger:
                    RtlDebugPrint("ToInteger (");
                    break;

                case AmlStatementToString:
                    RtlDebugPrint("ToString (");
                    break;

                default:

                    ASSERT(FALSE);

                    return STATUS_NOT_SUPPORTED;
                }

            } else {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

        } else {
            NewArgument = Context->PreviousStatement->Reduction;
            if (Statement->ArgumentsAcquired == 0) {

                //
                // Fail if there is no argument there.
                //

                if (Context->PreviousStatement->Reduction == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // If it's not an integer, buffer, or string, fail.
                //

                if ((NewArgument->Type != AcpiObjectInteger) &&
                    (NewArgument->Type != AcpiObjectBuffer) &&
                    (NewArgument->Type != AcpiObjectString)) {

                    return STATUS_INVALID_PARAMETER;
                }

                //
                // Perform an implicit conversion if needed.
                //

                if (((Statement->Type == AmlStatementToBcd) ||
                     (Statement->Type == AmlStatementFromBcd)) &&
                    (NewArgument->Type != AcpiObjectInteger)) {

                    NewArgument = AcpipConvertObjectType(Context,
                                                         NewArgument,
                                                         AcpiObjectInteger);

                    if (NewArgument == NULL) {
                        return STATUS_CONVERSION_FAILED;
                    }

                //
                // The object is fine, take ownership of it.
                //

                } else {
                    AcpipObjectAddReference(NewArgument);
                }

            } else {

                ASSERT(Statement->ArgumentsAcquired == 1);

                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
            if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }
        }
    }

    //
    // Evaluate the result.
    //

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    switch (Statement->Type) {
    case AmlStatementFromBcd:

        //
        // Convert the BCD value to an integer.
        //

        ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

        BcdValue = Statement->Argument[0]->U.Integer.Value;
        ResultValue = 0;
        for (Nibble = 0; Nibble < sizeof(ULONGLONG) * 2; Nibble += 1) {
            Digit = (BcdValue & 0xF000000000000000ULL) >> 60;
            ResultValue = (ResultValue * 10) + Digit;
            Digit = Digit << 4;
        }

        if (Context->CurrentMethod->IntegerWidthIs32 != FALSE) {
            ResultValue &= 0xFFFFFFFF;
        }

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ResultValue,
                                                          sizeof(ULONGLONG));

        break;

    case AmlStatementToBcd:

        //
        // Convert the integer to a BCD value.
        //

        ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

        IntegerValue = Statement->Argument[0]->U.Integer.Value;
        ResultValue = 0;
        for (Nibble = 0; Nibble < sizeof(ULONGLONG) * 2; Nibble += 1) {
            if (IntegerValue == 0) {
                break;
            }

            Digit = IntegerValue % 10;
            ResultValue = (ResultValue << 4) | Digit;
            IntegerValue = IntegerValue / 10;
        }

        if (IntegerValue != 0) {
            return STATUS_CONVERSION_FAILED;
        }

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ResultValue,
                                                          sizeof(ULONGLONG));

        break;

    case AmlStatementToBuffer:
        Statement->Reduction = AcpipConvertObjectType(Context,
                                                      Statement->Argument[0],
                                                      AcpiObjectBuffer);

        break;

    case AmlStatementToDecimalString:

        //
        // If the result is already a string, no action is performed.
        //

        if (Statement->Argument[0]->Type == AcpiObjectString) {
            Statement->Reduction = Statement->Argument[0];
            AcpipObjectAddReference(Statement->Reduction);

        //
        // Convert the integer to a string.
        //

        } else if (Statement->Argument[0]->Type == AcpiObjectInteger) {
            RtlPrintToString(ResultString,
                             MAX_DECIMAL_STRING_LENGTH,
                             CharacterEncodingAscii,
                             "%I64d",
                             Statement->Argument[0]->U.Integer.Value);

            Statement->Reduction = AcpipCreateNamespaceObject(
                                                    Context,
                                                    AcpiObjectString,
                                                    NULL,
                                                    ResultString,
                                                    MAX_DECIMAL_STRING_LENGTH);

        //
        // Convert the buffer to a comma delimited string of decimal integers.
        //

        } else if (Statement->Argument[0]->Type == AcpiObjectBuffer) {

            //
            // Create the result string with buffer first. The size is up to
            // three decimal digits, plus one comma per byte, minus the comma
            // at the end, plus the null delimiter.
            //

            StringSize = Statement->Argument[0]->U.Buffer.Length * 4;
            if (StringSize == 0) {
                StringSize = 1;
            }

            Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                              AcpiObjectString,
                                                              NULL,
                                                              NULL,
                                                              StringSize);

            if (Statement->Reduction == NULL) {
                return STATUS_UNSUCCESSFUL;
            }

            //
            // Print out each byte individually, except the last one.
            //

            String = Statement->Reduction->U.String.String;
            BytePointer = Statement->Argument[0]->U.Buffer.Buffer;
            ByteCount = Statement->Argument[0]->U.Buffer.Length;
            for (ByteIndex = 0; ByteIndex < ByteCount - 1; ByteIndex += 1) {
                ByteStringSize = RtlPrintToString(String,
                                                  StringSize,
                                                  CharacterEncodingAscii,
                                                  "%d,",
                                                  *BytePointer);

                if (ByteStringSize > StringSize) {
                    ByteStringSize = StringSize;
                }

                BytePointer += 1;
                String += ByteStringSize;
                StringSize -= ByteStringSize;
            }

            //
            // Do the last one without a comma.
            //

            RtlPrintToString(String,
                             StringSize,
                             CharacterEncodingAscii,
                             "%d",
                             *BytePointer);

        } else {

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        break;

    case AmlStatementToHexString:

        //
        // If the result is already a string, no action is performed.
        //

        if (Statement->Argument[0]->Type == AcpiObjectString) {
            Statement->Reduction = Statement->Argument[0];
            AcpipObjectAddReference(Statement->Reduction);

        //
        // Convert the integer or buffer to a string.
        //

        } else {
            Statement->Reduction = AcpipConvertObjectType(
                                                        Context,
                                                        Statement->Argument[0],
                                                        AcpiObjectString);
        }

        break;

    case AmlStatementToInteger:
        if (Statement->Argument[0]->Type == AcpiObjectInteger) {
            Statement->Reduction = Statement->Argument[0];
            AcpipObjectAddReference(Statement->Reduction);

        } else if (Statement->Argument[0]->Type == AcpiObjectBuffer) {
            Statement->Reduction = AcpipConvertObjectType(
                                                        Context,
                                                        Statement->Argument[0],
                                                        AcpiObjectInteger);

        } else if (Statement->Argument[0]->Type != AcpiObjectString) {

            ASSERT(FALSE);

            return STATUS_NOT_SUPPORTED;
        }

        String = Statement->Argument[0]->U.String.String;
        if (String == NULL) {
            break;
        }

        //
        // Parse the string as a decimal or a hex string depending on whether
        // there is an 0x prepending or not.
        //

        IntegerValue = 0;
        if ((*String == '0') && (*(String + 1) == 'x')) {
            while (((*String >= '0') && (*String <= '9')) ||
                   ((*String >= 'a') && (*String <= 'f')) ||
                   ((*String >= 'A') && (*String <= 'F'))) {

                if ((*String >= '0') && (*String <= '9')) {
                    Digit = *String - '0';

                } else if (((*String >= 'a') && (*String <= 'f'))) {
                    Digit = *String - 'a';

                } else {

                    ASSERT((*String >= 'A') && (*String <= 'F'));

                    Digit = *String - 'A';
                }

                IntegerValue = (IntegerValue << 4) | Digit;
                String += 1;
            }

        //
        // Parse it as a decimal string.
        //

        } else {
            while ((*String >= '0') && (*String <= '9')) {
                Digit = *String - '0';
                IntegerValue = (IntegerValue * 10) + Digit;
                String += 1;
            }
        }

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &IntegerValue,
                                                          sizeof(ULONGLONG));

        break;

    case AmlStatementToString:
        Statement->Reduction = AcpipConvertObjectType(Context,
                                                      Statement->Argument[0],
                                                      AcpiObjectString);

        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    if (Statement->Reduction == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the result in the target if supplied.
    //

    if (Statement->Argument[2] != NULL) {
        return AcpipPerformStoreOperation(Context,
                                          Statement->Reduction,
                                          Statement->Argument[2]);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateUnloadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Unload statement, which unloads a previously
    loaded definition block.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT DdbHandle;
    PACPI_OBJECT NewArgument;

    //
    // If not all arguments are acquired, evaluate the previous statement to get
    // the next argument.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Unload (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(")");
            }
        }

        if (Context->PreviousStatement == NULL) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            ASSERT(Statement->ArgumentsAcquired <= 1);

            if (Context->PreviousStatement->Reduction != NULL) {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
            return STATUS_MORE_PROCESSING_REQUIRED;
        }
    }

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements == FALSE) {
        return STATUS_SUCCESS;
    }

    DdbHandle = Statement->Argument[0];

    ASSERT(DdbHandle != NULL);

    AcpiUnloadDefinitionBlock(DdbHandle);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateWaitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates an Wait (for Event) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT IntegerObject;
    PACPI_OBJECT NameString;
    PACPI_OBJECT NewArgument;
    ULONGLONG ResultValue;
    ULONG TimeoutValue;

    //
    // Gather arguments if needed.
    //

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {
        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("Wait (");

            } else if (Statement->ArgumentsAcquired == 0) {
                RtlDebugPrint(", ");

            } else {
                RtlDebugPrint(")");
            }
        }

        //
        // The argument for Wait is a "SuperName", which is a SimpleName,
        // DebugOp, or Type6Opcode. If this is the first time through, try to
        // parse a name string.
        //

        if (Context->PreviousStatement == NULL) {
            NameString = AcpipParseNameString(Context);
            if (NameString == NULL) {
                return STATUS_MORE_PROCESSING_REQUIRED;
            }

            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint(NameString->U.String.String);
            }

            if (Context->ExecuteStatements != FALSE) {
                Statement->Argument[0] =
                           AcpipGetNamespaceObject(NameString->U.String.String,
                                                   Context->CurrentScope);

                if (Statement->Argument[0] == NULL) {
                    return STATUS_NOT_FOUND;
                }

                AcpipObjectAddReference(Statement->Argument[0]);
            }

            Statement->ArgumentsAcquired += 1;
            AcpipObjectReleaseReference(NameString);

        //
        // Get the argument from the previous statement.
        //

        } else {
            if (Context->ExecuteStatements != FALSE) {
                NewArgument = Context->PreviousStatement->Reduction;
                if (NewArgument == NULL) {
                    return STATUS_ARGUMENT_EXPECTED;
                }

                //
                // The first argument is the Event object.
                //

                if (Statement->ArgumentsAcquired == 0) {
                    if (NewArgument->Type != AcpiObjectEvent) {
                        return STATUS_INVALID_PARAMETER;
                    }

                    Statement->Argument[Statement->ArgumentsAcquired] =
                                                                   NewArgument;

                    AcpipObjectAddReference(NewArgument);

                //
                // The second argument should evaluate to an integer specifying
                // the number of milliseconds to wait for the given event.
                //

                } else {
                    if (NewArgument->Type == AcpiObjectInteger) {
                        Statement->Argument[Statement->ArgumentsAcquired] =
                                                                   NewArgument;

                        AcpipObjectAddReference(NewArgument);

                    } else {
                        IntegerObject = AcpipConvertObjectType(
                                                            Context,
                                                            NewArgument,
                                                            AcpiObjectInteger);

                        if (IntegerObject == NULL) {
                            return STATUS_UNSUCCESSFUL;
                        }

                        Statement->Argument[Statement->ArgumentsAcquired] =
                                                                 IntegerObject;
                    }
                }
            }

            Statement->ArgumentsAcquired += 1;
        }

    }

    ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

    Statement->Reduction = NULL;
    if (Context->ExecuteStatements != FALSE) {

        ASSERT(Statement->Argument[0]->Type == AcpiObjectEvent);
        ASSERT(Statement->Argument[1]->Type == AcpiObjectInteger);

        TimeoutValue = Statement->Argument[1]->U.Integer.Value;
        ResultValue = AcpipWaitForEvent(Statement->Argument[0]->U.Event.OsEvent,
                                        TimeoutValue);

        Statement->Reduction = AcpipCreateNamespaceObject(Context,
                                                          AcpiObjectInteger,
                                                          NULL,
                                                          &ResultValue,
                                                          sizeof(ULONGLONG));

        if (Statement->Reduction == NULL) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateWhileModifierStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates one of the statements that modifies a While loop,
    a Break or Continue.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    BOOL Continue;

    if (Context->PrintStatements != FALSE) {
        if (Statement->Type == AmlStatementBreak) {
            RtlDebugPrint("Break");

        } else {

            ASSERT(Statement->Type == AmlStatementContinue);

            RtlDebugPrint("Continue");
        }
    }

    if (Context->ExecuteStatements != FALSE) {
        if (Statement->Type == AmlStatementContinue) {
            Continue = TRUE;

        } else {
            Continue = FALSE;
        }

        AcpipPopExecutingStatements(Context, TRUE, Continue);
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipEvaluateWhileStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    )

/*++

Routine Description:

    This routine evaluates a While statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT NewArgument;

    if (Statement->ArgumentsNeeded != Statement->ArgumentsAcquired) {

        //
        // Print out the piece of the statement depending on the number of
        // arguments acquired.
        //

        if (Context->PrintStatements != FALSE) {
            if (Context->PreviousStatement == NULL) {
                RtlDebugPrint("While (");

            } else {

                ASSERT(Statement->ArgumentsAcquired == 0);

                RtlDebugPrint(") {");
            }
        }

        if (Context->PreviousStatement == NULL) {
            Context->IndentationLevel += 1;
            return STATUS_MORE_PROCESSING_REQUIRED;
        }

        //
        // If not executing, then assume the argument would be there but don't
        // try to dink with it.
        //

        if (Context->ExecuteStatements == FALSE) {
            Statement->Argument[Statement->ArgumentsAcquired] = NULL;
            Statement->ArgumentsAcquired += 1;

        } else {
            NewArgument = Context->PreviousStatement->Reduction;

            //
            // Fail if there is no argument there.
            //

            if (Context->PreviousStatement->Reduction == NULL) {
                return STATUS_ARGUMENT_EXPECTED;
            }

            //
            // Perform an implicit conversion if needed.
            //

            if (NewArgument->Type != AcpiObjectInteger) {
                NewArgument = AcpipConvertObjectType(Context,
                                                     NewArgument,
                                                     AcpiObjectInteger);

                if (NewArgument == NULL) {
                    return STATUS_CONVERSION_FAILED;
                }

            //
            // The object is fine, take ownership of it.
            //

            } else {
                AcpipObjectAddReference(NewArgument);
            }

            Statement->Argument[Statement->ArgumentsAcquired] = NewArgument;
            Statement->ArgumentsAcquired += 1;
        }

        //
        // Evaluate the predicate.
        //

        ASSERT(Statement->ArgumentsNeeded == Statement->ArgumentsAcquired);

        if (Context->ExecuteStatements != FALSE) {

            ASSERT(Statement->Argument[0]->Type == AcpiObjectInteger);

            //
            // Evaluate the while statement by skipping the package length if
            // it's zero, and completing the while statement.
            //

            if (Statement->Argument[0]->U.Integer.Value == 0) {
                Context->CurrentOffset = Statement->AdditionalData;
                Context->IndentationLevel -= 1;
                if (Context->PrintStatements != FALSE) {
                    AcpipPrintIndentedNewLine(Context);
                    RtlDebugPrint("}");
                }

                return STATUS_SUCCESS;
            }
        }
    }

    //
    // If execution is not done with the scope, keep this statement on the
    // stack.
    //

    if (Context->CurrentOffset < Statement->AdditionalData) {
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    ASSERT(Context->IndentationLevel != 0);

    //
    // Move the offset back to the predicate, release the argument to pretend
    // like the predicate was never seen before, and start again.
    //

    if (Context->ExecuteStatements != FALSE) {
        Context->CurrentOffset = Statement->AdditionalData2;
        AcpipObjectReleaseReference(Statement->Argument[0]);
        Statement->Argument[0] = NULL;
        Statement->ArgumentsAcquired = 0;
        AcpipPrintIndentedNewLine(Context);
        return STATUS_MORE_PROCESSING_REQUIRED;
    }

    AcpipPrintIndentedNewLine(Context);
    return STATUS_SUCCESS;
}

PACPI_OBJECT
AcpipConvertObjectType (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object,
    ACPI_OBJECT_TYPE NewType
    )

/*++

Routine Description:

    This routine performs a conversion between supported ACPI object types.

Arguments:

    Context - Supplies a pointer to the current execution context.

    Object - Supplies a pointer to the object to convert.

    NewType - Supplies the type to convert the given object to.

Return Value:

    Returns a pointer to a new object (unlinked to the namespace) of the
    requested type. The caller is responsible for this memory once its returned.

    NULL on failure.

--*/

{

    PACPI_OBJECT NewObject;
    PACPI_OBJECT ReadResult;
    KSTATUS Status;

    NewObject = NULL;
    ReadResult = NULL;

    //
    // Get the real object being pointed to here.
    //

    while (Object->Type == AcpiObjectAlias) {
        Object = Object->U.Alias.DestinationObject;
    }

    //
    // Attempting to convert from a Field Unit to something results in a read
    // from the field.
    //

    if (Object->Type == AcpiObjectFieldUnit) {
        Status = AcpipReadFromField(Context, Object, &ReadResult);
        if (!KSUCCESS(Status)) {
            return NULL;
        }

        if (ReadResult->Type == NewType) {
            return ReadResult;
        }

        //
        // The new thing to convert is the result of the field read.
        //

        Object = ReadResult;

    } else if (Object->Type == AcpiObjectBufferField) {
        Status = AcpipReadFromBufferField(Context, Object, &ReadResult);
        if (!KSUCCESS(Status)) {
            return NULL;
        }

        if (ReadResult->Type == NewType) {
            return ReadResult;
        }

        //
        // The new thing to convert is the result of the buffer field read.
        //

        Object = ReadResult;
    }

    switch (NewType) {
    case AcpiObjectInteger:
        NewObject = AcpipConvertObjectTypeToInteger(Context, Object);
        break;

    case AcpiObjectString:
        NewObject = AcpipConvertObjectTypeToString(Context, Object);
        break;

    case AcpiObjectBuffer:
        NewObject = AcpipConvertObjectTypeToBuffer(Context, Object);
        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // Release the intermediate read result object.
    //

    if (ReadResult != NULL) {
        AcpipObjectReleaseReference(ReadResult);
    }

    return NewObject;
}

KSTATUS
AcpipResolveStoreDestination (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Destination,
    PACPI_OBJECT *ResolvedDestination
    )

/*++

Routine Description:

    This routine resolves a store destination to the proper ACPI object based
    on its type and the statement type.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Destination - Supplies a pointer to the original store destination object.

    ResolvedDestination - Supplies a pointer that receives a pointer to the
        resolved destination object. This may return a pointer to the
        original destination, but with an extra reference. The caller is always
        responsible for releasing a reference on this object.

Return Value:

    Status code.

--*/

{

    ULONG LastLocalIndex;
    PACPI_OBJECT *LocalVariable;
    PACPI_OBJECT ResolvedObject;

    //
    // Follow all aliases.
    //

    while (Destination->Type == AcpiObjectAlias) {
        Destination = Destination->U.Alias.DestinationObject;
    }

    //
    // If it is a local, then store is meant to release the reference on the
    // existing local variable and set the local to a new copy of the source.
    //

    ResolvedObject = Destination;
    LastLocalIndex = Context->CurrentMethod->LastLocalIndex;
    LocalVariable = Context->CurrentMethod->LocalVariable;
    if ((LastLocalIndex != AML_INVALID_LOCAL_INDEX) &&
        (LocalVariable[LastLocalIndex] == Destination)) {

        AcpipObjectReleaseReference(Destination);
        LocalVariable[LastLocalIndex] = NULL;
        ResolvedObject = AcpipCreateNamespaceObject(Context,
                                                    AcpiObjectUninitialized,
                                                    NULL,
                                                    NULL,
                                                    0);

        if (ResolvedObject == NULL) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        LocalVariable[LastLocalIndex] = ResolvedObject;
    }

    AcpipObjectAddReference(ResolvedObject);
    *ResolvedDestination = ResolvedObject;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipConvertToDataReferenceObject (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object,
    PACPI_OBJECT *ResultObject
    )

/*++

Routine Description:

    This routine performs a conversion of an object to a type in the set of
    DataRefObject types.

Arguments:

    Context - Supplies a pointer to the current execution context.

    Object - Supplies a pointer to the object to convert.

    ResultObject - Supplies a pointer that receives a pointer to the result
        object after the conversion. If no conversion is necessary, then this
        may be a pointer to the original object. If a conversion is necessary,
        then this will be a pointer to a new object. Either way the caller is
        responsible for releasing one reference on the result object on
        success.

Return Value:

    Status code.

--*/

{

    PACPI_OBJECT DataReferenceObject;
    KSTATUS Status;

    //
    // Get the real object being pointed to here.
    //

    while (Object->Type == AcpiObjectAlias) {
        Object = Object->U.Alias.DestinationObject;
    }

    DataReferenceObject = NULL;
    Status = STATUS_SUCCESS;
    switch (Object->Type) {

    //
    // Convert a field unit to an integer or buffer.
    //

    case AcpiObjectFieldUnit:
        Status = AcpipReadFromField(Context, Object, &DataReferenceObject);
        if (!KSUCCESS(Status)) {
            goto ConvertToDataReferenceObjectEnd;
        }

        ASSERT((DataReferenceObject->Type == AcpiObjectInteger) ||
               (DataReferenceObject->Type == AcpiObjectBuffer));

        break;

    //
    // Convert a buffer field into an integer or buffer.
    //

    case AcpiObjectBufferField:
        Status = AcpipReadFromBufferField(Context,
                                          Object,
                                          &DataReferenceObject);

        if (!KSUCCESS(Status)) {
            goto ConvertToDataReferenceObjectEnd;
        }

        ASSERT((DataReferenceObject->Type == AcpiObjectInteger) ||
               (DataReferenceObject->Type == AcpiObjectBuffer));

        break;

    //
    // Just add a new reference if it is already a DataReferenceObject type.
    //

    case AcpiObjectInteger:
    case AcpiObjectString:
    case AcpiObjectBuffer:
    case AcpiObjectPackage:
    case AcpiObjectDdbHandle:
        DataReferenceObject = Object;
        AcpipObjectAddReference(DataReferenceObject);
        break;

    //
    // Anything else cannot be converted and results in failure.
    //

    default:
        RtlDebugPrint("\nACPI: Unable to convert object of type %d to a "
                      "DataRefObject. Context: 0x%08x, Object 0x%08x.\n",
                      Object->Type,
                      Context,
                      Object);

        ASSERT(FALSE);

        Status = STATUS_NOT_SUPPORTED;
        break;
    }

ConvertToDataReferenceObjectEnd:
    if (!KSUCCESS(Status)) {
        if (DataReferenceObject != NULL) {
            AcpipObjectReleaseReference(DataReferenceObject);
            DataReferenceObject = NULL;
        }
    }

    *ResultObject = DataReferenceObject;
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

PACPI_OBJECT
AcpipConvertObjectTypeToInteger (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine converts the given object into an Integer object.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    Object - Supplies a pointer to the object to convert.

Return Value:

    Returns a pointer to a new object (unlinked to the namespace) of the
    requested type. The caller is responsible for this memory once its returned.

    NULL on failure.

--*/

{

    ULONG CopySize;
    UCHAR Digit;
    ULONGLONG IntegerValue;
    PACPI_OBJECT NewObject;
    PSTR String;

    NewObject = NULL;
    IntegerValue = 0;
    switch (Object->Type) {
    case AcpiObjectInteger:
        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectInteger,
                                               NULL,
                                               &(Object->U.Integer.Value),
                                               sizeof(ULONGLONG));

        break;

    //
    // Convert from a buffer to an integer by basically just casting.
    //

    case AcpiObjectBuffer:
        CopySize = Object->U.Buffer.Length;
        if (CopySize > sizeof(ULONGLONG)) {
            CopySize = sizeof(ULONGLONG);
        }

        RtlCopyMemory(&IntegerValue, Object->U.Buffer.Buffer, CopySize);
        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectInteger,
                                               NULL,
                                               &IntegerValue,
                                               sizeof(ULONGLONG));

        break;

    //
    // To convert from a string to an integer, parse hex digits 0-9, A-F
    // (and a-f) until a non-digit is found. A leading 0x is not allowed.
    //

    case AcpiObjectString:
        String = Object->U.String.String;
        if (String == NULL) {
            break;
        }

        while (((*String >= '0') && (*String <= '9')) ||
               ((*String >= 'a') && (*String <= 'f')) ||
               ((*String >= 'A') && (*String <= 'F'))) {

            if ((*String >= '0') && (*String <= '9')) {
                Digit = *String - '0';

            } else if (((*String >= 'a') && (*String <= 'f'))) {
                Digit = *String - 'a' + 10;

            } else {

                ASSERT((*String >= 'A') && (*String <= 'F'));

                Digit = *String - 'A' + 10;
            }

            IntegerValue = (IntegerValue << 4) | Digit;
            String += 1;
        }

        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectInteger,
                                               NULL,
                                               &IntegerValue,
                                               sizeof(ULONGLONG));

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return NewObject;
}

PACPI_OBJECT
AcpipConvertObjectTypeToString (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine converts the given object into a String object.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Object - Supplies a pointer to the object to convert.

Return Value:

    Returns a pointer to a new object (unlinked to the namespace) of the
    requested type. The caller is responsible for this memory once its returned.

    NULL on failure.

--*/

{

    ULONG BufferLength;
    ULONG ByteIndex;
    PUCHAR CurrentBufferPosition;
    PSTR CurrentPosition;
    PACPI_OBJECT NewObject;
    ULONG NewStringLength;

    switch (Object->Type) {

    //
    // To convert an integer to a string, create an 8 or 16 byte string buffer
    // depending on whether integers are 32 or 64 bits, and then write the hex
    // value in.
    //

    case AcpiObjectInteger:
        NewStringLength = 16;
        if ((Context->CurrentMethod != NULL) &&
            (Context->CurrentMethod->IntegerWidthIs32 != FALSE)) {

            NewStringLength = 8;
        }

        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectString,
                                               NULL,
                                               NULL,
                                               NewStringLength);

        if (NewObject == NULL) {
            return NULL;
        }

        RtlPrintToString(NewObject->U.String.String,
                         NewStringLength,
                         CharacterEncodingAscii,
                         "%I64x",
                         Object->U.Integer.Value);

        break;

    case AcpiObjectString:
        NewStringLength = RtlStringLength(Object->U.String.String) + 1;
        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectString,
                                               NULL,
                                               Object->U.String.String,
                                               NewStringLength);

        break;

    //
    // To convert from a buffer to a string, print out all characters as two
    // digit hex values, separated by spaces.
    //

    case AcpiObjectBuffer:

        //
        // The new string length is 3 times the number of bytes there are (two
        // digits plus one space for each character), minus one since the last
        // character doesn't get a space, plus one for the null terminator.
        //

        NewStringLength = (Object->U.Buffer.Length * 3);
        NewObject = AcpipCreateNamespaceObject(NULL,
                                               AcpiObjectString,
                                               NULL,
                                               NULL,
                                               NewStringLength);

        if (NewObject == NULL) {
            return NULL;
        }

        BufferLength =  Object->U.Buffer.Length;
        if (BufferLength == 0) {
            NewObject->U.String.String[0] = '\0';

        } else {
            CurrentPosition = NewObject->U.String.String;
            CurrentBufferPosition = Object->U.Buffer.Buffer;

            //
            // Print out all except the last one.
            //

            for (ByteIndex = 0; ByteIndex < BufferLength - 1; ByteIndex += 1) {
                RtlPrintToString(CurrentPosition,
                                 NewStringLength,
                                 CharacterEncodingAscii,
                                 "%02x ",
                                 *CurrentBufferPosition);

                CurrentPosition += 3;
                NewStringLength -= 3;
                CurrentBufferPosition += 1;
            }

            //
            // Do the last one without a space.
            //

            RtlPrintToString(CurrentPosition,
                             NewStringLength,
                             CharacterEncodingAscii,
                             "%02x",
                             *CurrentBufferPosition);
        }

        break;

    default:

        ASSERT(FALSE);

        return NULL;
    }

    return NewObject;
}

PACPI_OBJECT
AcpipConvertObjectTypeToBuffer (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object
    )

/*++

Routine Description:

    This routine converts the given object into a Buffer object.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Object - Supplies a pointer to the object to convert.

Return Value:

    Returns a pointer to a new object (unlinked to the namespace) of the
    requested type. The caller is responsible for this memory once its returned.

    NULL on failure.

--*/

{

    ULONG BufferSize;
    PACPI_OBJECT NewObject;

    switch (Object->Type) {

    //
    // Converting from an integer to a buffer is basically a matter of casting.
    //

    case AcpiObjectInteger:
        BufferSize = sizeof(ULONGLONG);
        if ((Context->CurrentMethod != NULL) &&
            (Context->CurrentMethod->IntegerWidthIs32 != FALSE)) {

            BufferSize = sizeof(ULONG);
        }

        NewObject = AcpipCreateNamespaceObject(Context,
                                               AcpiObjectBuffer,
                                               NULL,
                                               &(Object->U.Integer.Value),
                                               BufferSize);

        break;

    case AcpiObjectString:
        BufferSize = RtlStringLength(Object->U.String.String);
        if (BufferSize != 0) {
            BufferSize += 1;
        }

        NewObject = AcpipCreateNamespaceObject(Context,
                                               AcpiObjectBuffer,
                                               NULL,
                                               Object->U.String.String,
                                               BufferSize);

        break;

    case AcpiObjectBuffer:
        BufferSize = Object->U.Buffer.Length;
        NewObject = AcpipCreateNamespaceObject(Context,
                                               AcpiObjectBuffer,
                                               NULL,
                                               Object->U.Buffer.Buffer,
                                               BufferSize);

        break;

    default:

        ASSERT(FALSE);

        return NULL;
    }

    return NewObject;
}

BOOL
AcpipEvaluateMatchComparison (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT PackageElement,
    PACPI_OBJECT Operand1,
    ACPI_MATCH_OPERATOR Operator1,
    PACPI_OBJECT Operand2,
    ACPI_MATCH_OPERATOR Operator2
    )

/*++

Routine Description:

    This routine performs a comparison of two or more objects as defined in the
    rules for a Match statement.

Arguments:

    Context - Supplies a pointer to the execution context.

    PackageElement - Supplies a pointer to the element indexed from the
        Match package being iterated over.

    Operand1 - Supplies a pointer to the first value to compare against. This
        must be an integer.

    Operator1 - Supplies the operator to use to compare the package element to
        Operand1.

    Operand2 - Supplies a pointer to the second value to compare against. This
        must be an integer.

    Operator2 - Supplies the operator to use to compare the package element to
        Operand2.

Return Value:

    TRUE if the condition matches the given operators against the given
    operands.

    FALSE if the comparison does not match.

--*/

{

    ULONGLONG Operand;
    ULONGLONG PackageValue;
    BOOL Result;

    ASSERT((Operator1 < MatchOperatorCount) &&
           (Operator2 < MatchOperatorCount));

    ASSERT((Operator1 == MatchOperatorTrue) ||
           (Operand1->Type == AcpiObjectInteger));

    ASSERT((Operator2 == MatchOperatorTrue) ||
           (Operand2->Type == AcpiObjectInteger));

    //
    // The ACPI spec says to skip uninitialized elements.
    //

    if ((PackageElement == NULL) ||
        (PackageElement->Type == AcpiObjectUninitialized)) {

        return FALSE;
    }

    //
    // Get an object that can be evaluated as an integer. If the conversion
    // fails, the ACPI spec says to quietly skip this value.
    //

    if (PackageElement->Type == AcpiObjectInteger) {
        PackageValue = PackageElement->U.Integer.Value;

    } else {
        PackageElement = AcpipConvertObjectType(Context,
                                                PackageElement,
                                                AcpiObjectInteger);

        if (PackageElement == NULL) {
            return FALSE;
        }

        PackageValue = PackageElement->U.Integer.Value;
        AcpipObjectReleaseReference(PackageElement);
    }

    //
    // Perform the comparison on object 1.
    //

    Result = FALSE;
    Operand = 0;
    if (Operator1 != MatchOperatorTrue) {
        Operand = Operand1->U.Integer.Value;
    }

    switch (Operator1) {
    case MatchOperatorTrue:
        Result = TRUE;
        break;

    case MatchOperatorEqual:
        if (PackageValue == Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorLessThanOrEqualTo:
        if (PackageValue <= Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorLessThan:
        if (PackageValue < Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorGreaterThanOrEqualTo:
        if (PackageValue >= Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorGreaterThan:
        if (PackageValue > Operand) {
            Result = TRUE;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    //
    // The function is only a match if both return TRUE. If this returned FALSE,
    // don't bother evaluating the other side.
    //

    if (Result == FALSE) {
        return Result;
    }

    //
    // Evalute operand 2.
    //

    Result = FALSE;
    if (Operator2 != MatchOperatorTrue) {
        Operand = Operand2->U.Integer.Value;
    }

    switch (Operator2) {
    case MatchOperatorTrue:
        Result = TRUE;
        break;

    case MatchOperatorEqual:
        if (PackageValue == Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorLessThanOrEqualTo:
        if (PackageValue <= Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorLessThan:
        if (PackageValue < Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorGreaterThanOrEqualTo:
        if (PackageValue >= Operand) {
            Result = TRUE;
        }

        break;

    case MatchOperatorGreaterThan:
        if (PackageValue > Operand) {
            Result = TRUE;
        }

        break;

    default:

        ASSERT(FALSE);

        break;
    }

    return Result;
}

