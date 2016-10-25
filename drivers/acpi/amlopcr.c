/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amlopcr.c

Abstract:

    This module implements ACPI AML low level opcode support, specifically to
    create AML statements.

Author:

    Evan Green 13-Nov-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "acpiobj.h"
#include "amlos.h"
#include "amlops.h"
#include "namespce.h"
#include "oprgn.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the opcodes for a dual-name and multi-name string.
//

#define ACPI_NULL_NAME_CHARACTER 0
#define ACPI_DUAL_NAME_PREFIX_CHARACTER 0x2E
#define ACPI_MULTI_NAME_PREFIX_CHARACTER 0x2F

#define ACPI_ARGUMENT_0_OPCODE 0x68
#define ACPI_LOCAL_0_OPCODE 0x60

#define PACKAGE_LENGTH_FOLLOW_BYTE_SHIFT 6
#define PACKAGE_LENGTH_FOLLOW_BYTE_MASK 0x03

//
// Define bitfield masks for the Field List flags.
//

#define FIELD_LIST_FLAG_ACCESS_MASK 0xF
#define FIELD_LIST_FLAG_LOCK_MASK 0x10
#define FIELD_LIST_FLAG_UPDATE_RULE_SHIFT 5
#define FIELD_LIST_FLAG_UPDATE_RULE_MASK \
    (0x3 << FIELD_LIST_FLAG_UPDATE_RULE_SHIFT)

//
// Define reserved bytes that indicate a new byte offset or new attributes.
//

#define FIELD_LIST_RESERVED_FIELD 0x00
#define FIELD_CHANGE_ATTRIBUTES 0x01

//
// Define the constant data prefixes.
//

#define BYTE_PREFIX 0x0A
#define WORD_PREFIX 0x0B
#define DOUBLE_WORD_PREFIX 0x0C
#define STRING_PREFIX 0x0D
#define QUAD_WORD_PREFIX 0x0E

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
AcpipIsValidFirstNameCharacter (
    UCHAR Character
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
AcpipCreateAcquireStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Acquire (mutex) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementAcquire;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateAliasStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcode 6, an alias statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes if the namespace strings were invalid.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementAlias;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 2;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto CreateAliasStatementEnd;
    }

    NextStatement->Argument[1] = AcpipParseNameString(Context);
    if (NextStatement->Argument[1] == NULL) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto CreateAliasStatementEnd;
    }

    Status = STATUS_SUCCESS;

CreateAliasStatementEnd:
    if (!KSUCCESS(Status)) {
        if (NextStatement->Argument[0] != NULL) {
            AcpipObjectReleaseReference(NextStatement->Argument[0]);
        }
    }

    return Status;
}

KSTATUS
AcpipCreateAddStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for an Add statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementAdd;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateAndStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcode 6, an alias statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementAnd;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateArgumentStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcodes 0x68 - 0x6E, ArgX statements.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    ULONG ArgumentNumber;
    UCHAR Instruction;
    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementArgument;
    InstructionPointer = (PUCHAR)(Context->AmlCode + Context->CurrentOffset);
    Instruction = *InstructionPointer;
    ArgumentNumber = Instruction - ACPI_ARGUMENT_0_OPCODE;
    Context->CurrentOffset += 1;

    //
    // Store which argument number it is in the additional data space.
    //

    NextStatement->AdditionalData = ArgumentNumber;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateBankFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a BankField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementBankField;
    Context->CurrentOffset += 1;

    //
    // Parse the package length to get the end offset.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    if (NextStatement->AdditionalData > Context->AmlCodeSize) {
        return STATUS_MALFORMED_DATA_STREAM;
    }

    //
    // Parse the name string.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the bank name string.
    //

    NextStatement->Argument[1] = AcpipParseNameString(Context);
    if (NextStatement->Argument[1] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Leave the current offset pointing at the third argument, a TermArg
    // that must reduce to an Integer.
    //

    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 2;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateBreakStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Break statement (like a break inside
    of a while loop, not break like stop).

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementBreak;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateBreakPointStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a breakpoint statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementBreakPoint;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a buffer declaration statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementBuffer;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Get the package length, compute the end offset, and store that in the
    // additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Watch for integer overflow.
    //

    if ((NextStatement->AdditionalData > Context->AmlCodeSize) ||
        (NextStatement->AdditionalData < Context->CurrentOffset)) {

        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateConcatenateResourceTemplatesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Concatenate Resource Templates" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementConcatenateResourceTemplates;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateConcatenateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a concatenation statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementConcatenate;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateConditionalReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Conditional Reference Of" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementConditionalReferenceOf;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateContinueStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a while loop Continue statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementContinue;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateCopyObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Copy Object" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementCopyObject;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateCreateBitFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateBitField statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferFieldFixed;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Store the size of the field in additional data.
    //

    NextStatement->AdditionalData = 1;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateCreateByteFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateByteField statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferFieldFixed;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Store the size of the field in additional data.
    //

    NextStatement->AdditionalData = BITS_PER_BYTE;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateCreateDoubleWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateDWordField statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferFieldFixed;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Store the size of the field in additional data.
    //

    NextStatement->AdditionalData = sizeof(ULONG) * BITS_PER_BYTE;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateCreateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateField (of a buffer) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferField;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 4;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateCreateQuadWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateQWordField statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferFieldFixed;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Store the size of the field in additional data.
    //

    NextStatement->AdditionalData = sizeof(ULONGLONG) * BITS_PER_BYTE;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateCreateWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a CreateWordField statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementCreateBufferFieldFixed;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Store the size of the field in additional data.
    //

    NextStatement->AdditionalData = sizeof(USHORT) * BITS_PER_BYTE;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateDataStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for ByteData, WordData, DWordData,
    QWordData, and StringData.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;
    UCHAR Prefix;

    NextStatement->Type = AmlStatementData;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    Prefix = *InstructionPointer;
    Context->CurrentOffset += 1;
    InstructionPointer += 1;

    //
    // Store the offset in additional data, and the size of the data in
    // additional data 2. Strings are encoded as zero in size.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    switch (Prefix) {
    case BYTE_PREFIX:
        Context->CurrentOffset += sizeof(UCHAR);
        NextStatement->AdditionalData2 = sizeof(UCHAR);
        break;

    case WORD_PREFIX:
        Context->CurrentOffset += sizeof(USHORT);
        NextStatement->AdditionalData2 = sizeof(USHORT);
        break;

    case DOUBLE_WORD_PREFIX:
        Context->CurrentOffset += sizeof(ULONG);
        NextStatement->AdditionalData2 = sizeof(ULONG);
        break;

    case STRING_PREFIX:
        NextStatement->AdditionalData2 = 0;
        while ((Context->CurrentOffset < Context->AmlCodeSize) &&
               (*InstructionPointer != '\0')) {

            Context->CurrentOffset += 1;
            InstructionPointer += 1;
        }

        //
        // Move past the null terminator.
        //

        Context->CurrentOffset += 1;
        break;

    case QUAD_WORD_PREFIX:
        Context->CurrentOffset += sizeof(ULONGLONG);
        NextStatement->AdditionalData2 = sizeof(ULONGLONG);
        break;

    default:

        ASSERT(FALSE);

        return STATUS_NOT_SUPPORTED;
    }

    //
    // Bounds checking.
    //

    if (Context->CurrentOffset > Context->AmlCodeSize) {
        return STATUS_END_OF_FILE;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateDataTableRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Data Table Region statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementDataTableRegion;

    //
    // TODO: Implement AML DataTableRegion opcode.
    //

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KSTATUS
AcpipCreateDebugStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Debug object statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementDebug;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateDecrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Decrement object statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementDecrement;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateDereferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Dereference Of" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementDereferenceOf;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateDeviceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Device statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementDevice;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Parse the package length. Store the end offset in the additional data
    // field.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the name string, and store as argument 0.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateDivideStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Divide statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementDivide;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 4;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateElseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Else statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementElse;
    Context->CurrentOffset += 1;

    //
    // Grab the package length, use it to calculate the end offset, and store
    // that in additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateEventStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Event statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementEvent;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Parse the name string.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateFatalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Fatal statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementFatal;
    Context->CurrentOffset += 1;

    //
    // Remember the offset, as the first two arguments (a byte and a DWORD) are
    // stored here.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Field (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementField;
    Context->CurrentOffset += 1;

    //
    // Parse the package length to get the end offset.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    if (NextStatement->AdditionalData > Context->AmlCodeSize) {
        return STATUS_MALFORMED_DATA_STREAM;
    }

    //
    // Parse the name string.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the starting flags and store them in additional data 2.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData2 = *InstructionPointer;
    Context->CurrentOffset += 1;

    //
    // Leave the current offset pointing at the start of the field list.
    //

    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateFindSetLeftBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Find Set Left Bit" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementFindSetLeftBit;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateFindSetRightBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Find Set Right Bit" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementFindSetRightBit;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateFromBcdStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Find BCD" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementFromBcd;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateIfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an If statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementIf;
    Context->CurrentOffset += 1;

    //
    // Grab the package length, use it to calculate the end offset, and store
    // that in additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateIncrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Increment object statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementIncrement;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateIndexFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a IndexField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementIndexField;
    Context->CurrentOffset += 1;

    //
    // Parse the package length to get the end offset.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    if (NextStatement->AdditionalData > Context->AmlCodeSize) {
        return STATUS_MALFORMED_DATA_STREAM;
    }

    //
    // Parse the name string of the Index register name.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the Data register name.
    //

    NextStatement->Argument[1] = AcpipParseNameString(Context);
    if (NextStatement->Argument[1] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the starting flags and store them in additional data 2.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData2 = *InstructionPointer;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 2;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateIndexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Index statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementIndex;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateInvalidOpcodeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine is a placeholder function executed when an invalid opcode of
    AML code is parsed.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_INVALID_OPCODE always.

--*/

{

    ASSERT(FALSE);

    return STATUS_INVALID_OPCODE;
}

KSTATUS
AcpipCreateLoadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Load (definition block) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLoad;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLoadTableStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a LoadTable statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLoadTable;

    //
    // TODO: Implement AML Load Table opcode.
    //

    ASSERT(FALSE);

    return STATUS_NOT_IMPLEMENTED;
}

KSTATUS
AcpipCreateLocalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcodes 0x60 - 0x67, LocalX statements.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    UCHAR Instruction;
    PUCHAR InstructionPointer;
    ULONG LocalNumber;

    NextStatement->Type = AmlStatementLocal;
    InstructionPointer = (PUCHAR)(Context->AmlCode + Context->CurrentOffset);
    Instruction = *InstructionPointer;
    LocalNumber = Instruction - ACPI_LOCAL_0_OPCODE;

    //
    // Store which argument number it is in the additional data space.
    //

    NextStatement->AdditionalData = LocalNumber;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalAndStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical And statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalAnd;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalEqualStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical Equal statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalEqual;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalGreaterStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical Greater statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalGreater;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalLessStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical Less statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalLess;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical Not statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalNot;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateLogicalOrStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Logical Or statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementLogicalOr;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateMatchStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Match statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementMatch;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 4;
    NextStatement->ArgumentsAcquired = 0;
    NextStatement->AdditionalData = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Method statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementMethod;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Parse the package length to get the end offset.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the NameString for the method name.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the method flags into additional data 2. This leaves the
    // current offset pointing at the first term in the TermList.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData2 = *InstructionPointer;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateMidStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Mid statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementMid;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 4;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateModStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for an Mod statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementMod;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateMultiplyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Multiply statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementMultiply;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateMutexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Mutex (create) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementMutex;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Get the name string of the mutex.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the sync flags in additional data.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData = *InstructionPointer;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateNameStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Name statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementName;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 1;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateNameStatementEnd;
    }

    Status = STATUS_SUCCESS;

CreateNameStatementEnd:
    return Status;
}

KSTATUS
AcpipCreateNameStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a NameString statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementNameString;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto CreateNameStringStatementEnd;
    }

    Status = STATUS_SUCCESS;

CreateNameStringStatementEnd:
    return Status;
}

KSTATUS
AcpipCreateNandStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Nand statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementNand;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateNoOpStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a No-Op statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementNoOp;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateNorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Nor statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementNor;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateNotifyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Notify statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementNotify;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a bitwise Not statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementNot;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateObjectTypeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Object Type statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementObjectType;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateOrStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for an Or statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementOr;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateOnesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Ones statement, which is a constant of all Fs.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementOnes;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateOneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcode 1, a constant 1.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementOne;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateOperationRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Operation Region statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementOperationRegion;

    //
    // Operation regions define a NameString, RegionOffset, and RegionLength.
    // Immediately after the NameString is a byte constant or the region space,
    // which is not counted as one of the three arguments.
    //

    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Parse the name string now.
    //

    Context->CurrentOffset += 1;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the byte for the address space in the additional data field.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData = *InstructionPointer;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreatePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Package statement (basically an array of objects).

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PUCHAR InstructionPointer;

    NextStatement->Type = AmlStatementPackage;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Get the package length.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the number of elements in Additional Data 2.
    //

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    NextStatement->AdditionalData2 = *InstructionPointer;
    Context->CurrentOffset += 1;
    NextStatement->Reduction = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreatePowerResourceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Power Resource declaration statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementPowerResource;
    Context->CurrentOffset += 1;

    //
    // Store the end offset of the object list in additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the name string argument.
    //

    NextStatement->ArgumentsNeeded = 1;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->ArgumentsAcquired = 1;

    //
    // Store the offset to the additional argumenents (a byte and a short) for
    // later execution, and advance beyond them.
    //

    NextStatement->AdditionalData2 = Context->CurrentOffset;
    Context->CurrentOffset += sizeof(BYTE) + sizeof(USHORT);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateProcessorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Processor declaration statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementProcessor;
    Context->CurrentOffset += 1;

    //
    // Store the end offset of the object list in additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the name string argument.
    //

    NextStatement->ArgumentsNeeded = 1;
    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->ArgumentsAcquired = 1;

    //
    // Store the offset to the additional argumenents (two bytes and a DWORD)
    // for later execution, and advance beyond them.
    //

    NextStatement->AdditionalData2 = Context->CurrentOffset;
    Context->CurrentOffset += sizeof(BYTE) + sizeof(ULONG) + sizeof(BYTE);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "Reference Of" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementReferenceOf;
    Context->CurrentOffset += 1;

    //
    // Start with one argument needed, though upon evaluation a simple name
    // may be evaluated immediately, which means no arguments are needed.
    //

    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateReleaseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Release (mutex) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementRelease;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateResetStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Reset (event) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementReset;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateReturnStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Return statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementReturn;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateRevisionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Revision statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementRevision;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateScopeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Scope statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementScope;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Parse the package length. Store the end offset in the additional data
    // field.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the name string, and store as argument 0.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateShiftLeftStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Shift Left statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementShiftLeft;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateShiftRightStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Shift Right statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementShiftRight;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateSignalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Signal (event) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementSignal;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateSizeOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "SizeOf" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS on success.

    Other error codes on failure.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementSizeOf;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateSleepStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Sleep statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementSleep;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateStallStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Stall statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementStall;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateStoreStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Store statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementStore;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateSubtractStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Subtract statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementSubtract;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateThermalZoneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Thermal Zone statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementThermalZone;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 1;

    //
    // Get the package length.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Parse the name string to get the name of the thermal zone.
    //

    NextStatement->Argument[0] = AcpipParseNameString(Context);
    if (NextStatement->Argument[0] == NULL) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->Reduction = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateTimerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for a Timer statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementTimer;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 0;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToBcdStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To BCD" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToBcd;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To Buffer" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToBuffer;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToDecimalStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To Decimal String" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToDecimalString;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToHexStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To Hex String" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToHexString;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToIntegerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To Integer" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToInteger;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateToStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a "To String" statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementToString;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateUnloadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Unload (definition block) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementUnload;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateVariablePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a Variable package statement, whose size is determined
    by a TermArg rather than a constant.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementVariablePackage;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;

    //
    // Get the package length.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    NextStatement->Reduction = NULL;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateWaitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates an Wait (for Event) statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementWait;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 2;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateWhileStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a While statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    Status code.

--*/

{

    NextStatement->Type = AmlStatementWhile;
    Context->CurrentOffset += 1;

    //
    // Grab the package length, use it to calculate the end offset, and store
    // that in additional data.
    //

    NextStatement->AdditionalData = Context->CurrentOffset;
    NextStatement->AdditionalData += AcpipParsePackageLength(Context);
    if (NextStatement->AdditionalData == Context->CurrentOffset) {
        return STATUS_UNSUCCESSFUL;
    }

    //
    // Store the predicate offset in additional data 2, so it can be
    // re-evaluated on subsequent iterations through the while loop.
    //

    NextStatement->AdditionalData2 = Context->CurrentOffset;
    NextStatement->ArgumentsNeeded = 1;
    NextStatement->ArgumentsAcquired = 0;
    return STATUS_SUCCESS;
}

KSTATUS
AcpipCreateXorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for an Exclusive Or statement.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    KSTATUS Status;

    NextStatement->Type = AmlStatementXor;
    Context->CurrentOffset += 1;
    NextStatement->ArgumentsNeeded = 3;
    NextStatement->ArgumentsAcquired = 0;
    Status = STATUS_SUCCESS;
    return Status;
}

KSTATUS
AcpipCreateZeroStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine creates a statement for opcode 0, a constant 0.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    STATUS_SUCCESS always.

--*/

{

    NextStatement->Type = AmlStatementZero;
    Context->CurrentOffset += 1;
    return STATUS_SUCCESS;
}

PACPI_OBJECT
AcpipParseNameString (
    PAML_EXECUTION_CONTEXT Context
    )

/*++

Routine Description:

    This routine parses a namespace string from the AML stream.

Arguments:

    Context - Supplies a pointer to the AML execution context. The name string
        will be evaluated from the current offset.

Return Value:

    Returns a pointer to a string object (unconnected to any namespace) on
    success.

    NULL on failure. The AML stream current offset will be unchanged on failure.

--*/

{

    PUCHAR AmlBuffer;
    ULONG AmlStringSize;
    UCHAR Character;
    ULONG DestinationOffset;
    ULONG NameCount;
    BOOL RootCharacterFound;
    ULONG SourceOffset;
    KSTATUS Status;
    ULONG StringBufferSize;
    PACPI_OBJECT StringObject;

    RootCharacterFound = FALSE;
    StringObject = NULL;
    AmlStringSize = 0;
    StringBufferSize = 0;
    AmlBuffer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;

    //
    // Determine the length of the string in the AML stream and validate the
    // string at the same time.
    //

    while (Context->CurrentOffset + AmlStringSize < Context->AmlCodeSize) {
        Character = *(AmlBuffer + AmlStringSize);

        //
        // A root character can only come at the beginning and only once.
        //

        if (Character == ACPI_NAMESPACE_ROOT_CHARACTER) {
            AmlStringSize += 1;
            if (AmlStringSize == 1) {
                RootCharacterFound = TRUE;
                StringBufferSize += 1;
                continue;

            } else {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ParseNameStringEnd;
            }
        }

        //
        // Handle a "current parent" string.
        //

        if (Character == ACPI_NAMESPACE_PARENT_CHARACTER) {
            AmlStringSize += 1;
            if (RootCharacterFound != FALSE) {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ParseNameStringEnd;
            }

            StringBufferSize += 1;
            continue;
        }

        //
        // Handle a NULL string.
        //

        if (Character == ACPI_NULL_NAME_CHARACTER) {
            AmlStringSize += 1;
            break;
        }

        //
        // Handle a dual-name string.
        //

        if (Character == ACPI_DUAL_NAME_PREFIX_CHARACTER) {
            AmlStringSize += (ACPI_MAX_NAME_LENGTH * 2) + 1;
            StringBufferSize += ACPI_MAX_NAME_LENGTH * 2;
            break;
        }

        //
        // Handle a multi-name string.
        //

        if (Character == ACPI_MULTI_NAME_PREFIX_CHARACTER) {
            AmlStringSize += 1;
            if (Context->CurrentOffset + AmlStringSize > Context->AmlCodeSize) {
                Status = STATUS_MALFORMED_DATA_STREAM;
                goto ParseNameStringEnd;
            }

            NameCount = *(AmlBuffer + AmlStringSize);
            AmlStringSize += (ACPI_MAX_NAME_LENGTH * NameCount) + 1;
            StringBufferSize += ACPI_MAX_NAME_LENGTH * NameCount;
            break;
        }

        //
        // It must just be a normal character, so the name is being specified.
        //

        if (AcpipIsValidFirstNameCharacter(Character) == FALSE) {
            Status = STATUS_MALFORMED_DATA_STREAM;
            goto ParseNameStringEnd;
        }

        AmlStringSize += ACPI_MAX_NAME_LENGTH;
        StringBufferSize += ACPI_MAX_NAME_LENGTH;
        break;
    }

    //
    // Double check to make sure the string didn't overflow the AML.
    //

    if (Context->CurrentOffset + AmlStringSize > Context->AmlCodeSize) {
        Status = STATUS_MALFORMED_DATA_STREAM;
        goto ParseNameStringEnd;
    }

    //
    // Create the namespace string.
    //

    StringObject = AcpipCreateNamespaceObject(Context,
                                              AcpiObjectString,
                                              NULL,
                                              NULL,
                                              StringBufferSize + 1);

    if (StringObject == NULL) {
        Status = STATUS_UNSUCCESSFUL;
        goto ParseNameStringEnd;
    }

    //
    // Copy the string in, plucking out the control characters.
    //

    SourceOffset = 0;
    DestinationOffset = 0;
    while (SourceOffset < AmlStringSize) {
        Character = *(AmlBuffer + SourceOffset);
        SourceOffset += 1;
        if (Character == ACPI_DUAL_NAME_PREFIX_CHARACTER) {
            continue;
        }

        if (Character == ACPI_MULTI_NAME_PREFIX_CHARACTER) {

            //
            // Skip past the name count byte too.
            //

            SourceOffset += 1;
            continue;
        }

        if (Character == ACPI_NULL_NAME_CHARACTER) {
            break;
        }

        *(StringObject->U.String.String + DestinationOffset) = Character;
        DestinationOffset += 1;
    }

    //
    // Add a null terminator.
    //

    ASSERT(DestinationOffset < StringBufferSize + 1);

    *(StringObject->U.String.String + DestinationOffset) = '\0';
    Status = STATUS_SUCCESS;

ParseNameStringEnd:
    if (!KSUCCESS(Status)) {
        if (StringObject != NULL) {
            AcpipObjectReleaseReference(StringObject);
            StringObject = NULL;
        }

        AmlStringSize = 0;
    }

    Context->CurrentOffset += AmlStringSize;
    return StringObject;
}

KSTATUS
AcpipParseFieldList (
    PAML_EXECUTION_CONTEXT Context,
    AML_STATEMENT_TYPE Type,
    PACPI_OBJECT OperationRegion,
    PACPI_OBJECT BankRegister,
    PACPI_OBJECT BankValue,
    PACPI_OBJECT IndexRegister,
    PACPI_OBJECT DataRegister,
    ULONG EndOffset,
    UCHAR InitialAccessFlags
    )

/*++

Routine Description:

    This routine parses a field list, used in Operation Region field list
    declarations.

Arguments:

    Context - Supplies a pointer to the AML execution context.

    Type - Supplies the type of field to create. Valid values are
        AmlStatementField, AmlStatementBankField, and AmlStatementIndexField.

    OperationRegion - Supplies a pointer to the operation region these fields
        belong to. NULL is only valid if the execution context is not
        executing statements.

    BankRegister - Supplies an optional pointer to the bank register to write
        to before accessing these fields. It is expected that the Bank registers
        will be non-null or the Index/Data registers, but not both.

    BankValue - Supplies a pointer to the value to write to the bank register.
        If the bank register is not supplied, this parameter is ignored. If the
        bank register is supplied, this parameter is required.

    IndexRegister - Supplies an optional pointer to the index register to write
        to before accessing the corresponding data register. It is expected
        that the Index/Data registers are non-null or the Bank registers, but
        not both.

    DataRegister - Supplies a pointer to the data register to use in
        Index/Data mode. If the Index register is not supplied, this parameter
            is ignored. If the index register is supplied, this parameter is
            required.

    EndOffset - Supplies the ending offset (exclusive) of the field list in the
        AML code stream.

    InitialAccessFlags - Supplies the initial attributes of the field
        (until the first AccessAs modifier is parsed).

Return Value:

    Status code.

--*/

{

    UCHAR AccessAttributes;
    UCHAR AccessFlags;
    ACPI_FIELD_UNIT_OBJECT FieldUnit;
    PSTR FieldUnitName;
    PUCHAR InstructionPointer;
    CHAR Name[ACPI_MAX_NAME_LENGTH + 1];
    PACPI_OBJECT NewFieldUnit;

    //
    // If the bank register is filled in, the bank value had better be too.
    //

    ASSERT((BankRegister == NULL) || (BankValue != NULL));

    //
    // If the index register is filled in, the data register had better be too.
    //

    ASSERT((IndexRegister == NULL) || (DataRegister != NULL));

    //
    // Assert that they're not both filled in.
    //

    ASSERT(!((BankRegister != NULL) && (IndexRegister != NULL)));

    FieldUnit.OperationRegion = OperationRegion;
    FieldUnit.BankRegister = BankRegister;
    FieldUnit.BankValue = BankValue;
    FieldUnit.IndexRegister = IndexRegister;
    FieldUnit.DataRegister = DataRegister;
    FieldUnitName = "FieldUnit";
    if (Type == AmlStatementBankField) {
        FieldUnitName = "BankField";

    } else if (Type == AmlStatementIndexField) {
        FieldUnitName = "IndexField";

    } else {

        ASSERT(Type == AmlStatementField);
    }

    //
    // Parse the initial attributes bitfields.
    //

    FieldUnit.Access = InitialAccessFlags & FIELD_LIST_FLAG_ACCESS_MASK;
    FieldUnit.AcquireGlobalLock = FALSE;
    if ((InitialAccessFlags & FIELD_LIST_FLAG_LOCK_MASK) != 0) {
        FieldUnit.AcquireGlobalLock = TRUE;
    }

    FieldUnit.UpdateRule = (InitialAccessFlags &
                            FIELD_LIST_FLAG_UPDATE_RULE_MASK) >>
                           FIELD_LIST_FLAG_UPDATE_RULE_SHIFT;

    ASSERT(FieldUnit.UpdateRule < AcpiFieldUpdateRuleCount);

    //
    // Null terminate the field name.
    //

    Name[ACPI_MAX_NAME_LENGTH] = '\0';

    //
    // Loop parsing fields until the end.
    //

    FieldUnit.BitOffset = 0;
    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    while (Context->CurrentOffset < EndOffset) {

        //
        // A field unit list contains one of the following:
        // * NameSeg PkgLength - Defines a new field of PkgLength bits.
        // * 0x00 PkgLength - Defines an unnamed (reserved) field.
        // * 0x01 AccessType AccessAttrib - Define the new access type and
        // attributes. Both of these are bytes. Start by checking for the
        // special byte 0.
        //

        if (*InstructionPointer == FIELD_LIST_RESERVED_FIELD) {
            InstructionPointer += 1;
            Context->CurrentOffset += 1;
            FieldUnit.BitLength = AcpipParsePackageLength(Context);
            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint("Skip 0x%I64x", FieldUnit.BitLength);
            }

            FieldUnit.BitOffset += FieldUnit.BitLength;

        //
        // Check for the special change of attributes byte.
        //

        } else if (*InstructionPointer == FIELD_CHANGE_ATTRIBUTES) {
            InstructionPointer += 1;
            Context->CurrentOffset += 1;
            AccessFlags = *InstructionPointer;
            FieldUnit.Access = AccessFlags & FIELD_LIST_FLAG_ACCESS_MASK;
            FieldUnit.AcquireGlobalLock = FALSE;
            if ((AccessFlags & FIELD_LIST_FLAG_LOCK_MASK) != 0) {
                FieldUnit.AcquireGlobalLock = TRUE;
            }

            FieldUnit.UpdateRule = (AccessFlags &
                                    FIELD_LIST_FLAG_UPDATE_RULE_MASK) >>
                                   FIELD_LIST_FLAG_UPDATE_RULE_SHIFT;

            ASSERT(FieldUnit.UpdateRule < AcpiFieldUpdateRuleCount);

            InstructionPointer += 1;
            Context->CurrentOffset += 1;

            //
            // TODO: Parse SMBus Access attributes when SMBus access support
            // is implemented.
            //

            AccessAttributes = *InstructionPointer;
            InstructionPointer += 1;
            Context->CurrentOffset += 1;
            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint("AccessAs (0x%02x, 0x%02x)",
                              AccessFlags,
                              AccessAttributes);
            }

        //
        // Parse a normal field name and bit length.
        //

        } else {

            //
            // Create the name string.
            //

            RtlCopyMemory(Name, InstructionPointer, ACPI_MAX_NAME_LENGTH);
            InstructionPointer += ACPI_MAX_NAME_LENGTH;
            Context->CurrentOffset += ACPI_MAX_NAME_LENGTH;

            //
            // Parse the bit length of this field.
            //

            FieldUnit.BitLength = AcpipParsePackageLength(Context);
            if (FieldUnit.BitLength == 0) {
                return STATUS_MALFORMED_DATA_STREAM;
            }

            if (Context->PrintStatements != FALSE) {
                RtlDebugPrint("%s (%s, 0x%I64x, 0x%I64x)",
                              FieldUnitName,
                              Name,
                              FieldUnit.BitOffset,
                              FieldUnit.BitLength);
            }

            if (Context->ExecuteStatements != FALSE) {
                NewFieldUnit = AcpipCreateNamespaceObject(
                                               Context,
                                               AcpiObjectFieldUnit,
                                               Name,
                                               &FieldUnit,
                                               sizeof(ACPI_FIELD_UNIT_OBJECT));

                if (NewFieldUnit == NULL) {
                    return STATUS_UNSUCCESSFUL;
                }

                AcpipObjectReleaseReference(NewFieldUnit);
            }

            //
            // Advance the bit offset past these bits.
            //

            FieldUnit.BitOffset += FieldUnit.BitLength;
        }

        //
        // Update the instruction pointer.
        //

        InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
        AcpipPrintIndentedNewLine(Context);
    }

    ASSERT(Context->CurrentOffset == EndOffset);

    return STATUS_SUCCESS;
}

ULONGLONG
AcpipParsePackageLength (
    PAML_EXECUTION_CONTEXT Context
    )

/*++

Routine Description:

    This routine parses a package length from the AML stream.

Arguments:

    Context - Supplies a pointer to the AML execution context. The name string
        will be evaluated from the current offset.

Return Value:

    Returns the size of the package that follows.

    0 on failure.

--*/

{

    UCHAR ByteIndex;
    UCHAR DataByte;
    PUCHAR InstructionPointer;
    ULONG LengthSize;
    ULONG PackageLength;

    if (Context->CurrentOffset >= Context->AmlCodeSize) {

        ASSERT(FALSE);

        return 0;
    }

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;
    DataByte = *InstructionPointer;
    LengthSize = (DataByte >> PACKAGE_LENGTH_FOLLOW_BYTE_SHIFT) &
                 PACKAGE_LENGTH_FOLLOW_BYTE_MASK;

    if (Context->CurrentOffset + LengthSize >= Context->AmlCodeSize) {

        ASSERT(FALSE);

        return 0;
    }

    Context->CurrentOffset += LengthSize + 1;

    //
    // If there are no additional bytes, then the value must be somewhere
    // between 0 and 63. Simply return that byte as the length.
    //

    if (LengthSize == 0) {
        return DataByte;
    }

    //
    // Add the follow bytes. The farthest out bytes are the highest value bits.
    //

    PackageLength = 0;
    for (ByteIndex = 0; ByteIndex < LengthSize; ByteIndex += 1) {
        PackageLength = (PackageLength << 8) |
                        InstructionPointer[LengthSize - ByteIndex];
    }

    //
    // Add in the first bit. Only bits 0-3 count.
    //

    PackageLength = (PackageLength << 4) | (DataByte & 0x0F);
    return PackageLength;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
AcpipIsValidFirstNameCharacter (
    UCHAR Character
    )

/*++

Routine Description:

    This routine checks to see if the given character is suitable for use as
    the first character of an ACPI name. Valid ACPI name leading characters are
    A - Z and _.

Arguments:

    Character - Supplies the character to evaluate.

Return Value:

    TRUE if the character is a valid first character of an ACPI name.

    FALSE if the character is not valid.

--*/

{

    if ((Character >= 'A') && (Character <= 'Z')) {
        return TRUE;
    }

    if (Character == '_') {
        return TRUE;
    }

    return FALSE;
}

