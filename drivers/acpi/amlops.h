/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amlops.h

Abstract:

    This header contains definitions for ACPI AML opcodes and instructions.

Author:

    Evan Green 13-Nov-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
KSTATUS
(*PAML_CREATE_NEXT_STATEMENT_ROUTINE) (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

/*++

Routine Description:

    This routine creates the next AML statement based on the current AML
    execution context and the first opcode byte.

Arguments:

    Context - Supplies a pointer to an initialized AML execution context. The
        next statement will be created based on the current execution offset.
        The current offset of the context will be incremented beyond the portion
        of this statement that was successfully parsed.

    NextStatement - Supplies a pointer where the next statement will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code indicating whether a statement was successfully created.

--*/

typedef
KSTATUS
(*PAML_EVALUATE_STATEMENT_ROUTINE) (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an AML statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS if the statement was completely evaluated.

    STATUS_MORE_PROCESSING_REQUIRED if additional AML code needs to be executed
    so that all arguments to the statement can be evaluated.

    Other error codes on failure.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// Define an array that creates ACPI statements based on the first opcode byte.
//

extern PAML_CREATE_NEXT_STATEMENT_ROUTINE AcpiCreateStatement[256];

//
// Store an array of function pointers that evaluate ACPI AML statements.
//

extern PAML_EVALUATE_STATEMENT_ROUTINE AcpiEvaluateStatement[AmlStatementCount];

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
AcpipCreateAcquireStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateAliasStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateAddStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateAndStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateArgumentStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateBankFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateBreakStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateBreakPointStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateConcatenateResourceTemplatesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateConcatenateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateConditionalReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateContinueStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCopyObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateBitFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateByteFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateDoubleWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateQuadWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateCreateWordFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDataStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDataTableRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDebugStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDecrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDereferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDeviceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateDivideStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateElseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateEventStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateFatalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateFindSetLeftBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateFindSetRightBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateFromBcdStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateIfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateIncrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateIndexFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateIndexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateInvalidOpcodeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLoadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLoadTableStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLocalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalAndStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalEqualStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalGreaterStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalLessStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateLogicalOrStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateMatchStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateMidStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateModStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateMultiplyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateMutexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNameStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNameStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNandStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNoOpStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNotifyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateObjectTypeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateOrStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateOnesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateOneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateOperationRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreatePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreatePowerResourceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateProcessorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateReleaseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateResetStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateReturnStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateRevisionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateScopeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateShiftLeftStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateShiftRightStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateSignalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateSizeOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateSleepStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateStallStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateStoreStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateSubtractStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateThermalZoneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateTimerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToBcdStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToDecimalStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToHexStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToIntegerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateToStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateUnloadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateVariablePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateWaitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateWhileStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateXorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipCreateZeroStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

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

KSTATUS
AcpipEvaluateAcquireStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Acquire (mutex) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateAliasStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates the alias statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateArgumentStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates the ArgX opcodes.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

KSTATUS
AcpipEvaluateBankFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a BankField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

KSTATUS
AcpipEvaluateBreakPointStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates the BreakPoint statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

KSTATUS
AcpipEvaluateBufferStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a buffer declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateConcatenateResourceTemplatesStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateConcatenateStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a concatenate statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateConditionalReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an "Reference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateCopyObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a "Copy Object" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateCreateBufferFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a CreateField (from a buffer) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateCreateFixedBufferFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateDataStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateDelayStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates both the Sleep and Stall statements.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateDebugStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Debug statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateDereferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a "Dereference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateDeviceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Device declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateDivideStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a divide statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateElseStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Else statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateEventStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Event (creation) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateExecutingMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateFatalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Field (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

KSTATUS
AcpipEvaluateFindSetBitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a find set left bit or find set right bit statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateIfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an If statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateIncrementOrDecrementStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Increment or Decrement statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateIndexFieldStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an IndexField (in an Operation Region) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    STATUS_SUCCESS always.

--*/

KSTATUS
AcpipEvaluateIndexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateIntegerArithmeticStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateIntegerStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateLoadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateLocalStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates the LocalX opcodes.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateLogicalExpressionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateLogicalNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates logical NOT operator.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateMatchStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateMethodStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Method declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateMidStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a mid statement, which splits a string up.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateMutexStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Mutex (creation) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateNameStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateNameStringStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateNoOpStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateNotifyStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Notify (the operating system) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateNotStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates bitwise NOT operator.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateObjectTypeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Object Type statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateOperationRegionStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Operation Region statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluatePackageStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Package or Variable Package statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluatePowerResourceStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Power Resource declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateProcessorStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Power Resource declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateReferenceOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an "Reference Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateReturnStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Return statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateScopeStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Device declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateSizeOfStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a "Size Of" statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateStoreStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Store statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateSyncObjectStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateThermalZoneStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a Thermal Zone declaration statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateToFormatStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateUnloadStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateWaitStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates an Wait (for Event) statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

KSTATUS
AcpipEvaluateWhileModifierStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

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

KSTATUS
AcpipEvaluateWhileStatement (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT Statement
    );

/*++

Routine Description:

    This routine evaluates a While statement.

Arguments:

    Context - Supplies a pointer to the current AML execution context.

    Statement - Supplies a pointer to the statement to evaluate.

Return Value:

    Status code.

--*/

//
// Helper functions.
//

PACPI_OBJECT
AcpipConvertObjectType (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object,
    ACPI_OBJECT_TYPE NewType
    );

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

KSTATUS
AcpipResolveStoreDestination (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Destination,
    PACPI_OBJECT *ResolvedDestination
    );

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

KSTATUS
AcpipConvertToDataReferenceObject (
    PAML_EXECUTION_CONTEXT Context,
    PACPI_OBJECT Object,
    PACPI_OBJECT *ResultObject
    );

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

PACPI_OBJECT
AcpipParseNameString (
    PAML_EXECUTION_CONTEXT Context
    );

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
    );

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

ULONGLONG
AcpipParsePackageLength (
    PAML_EXECUTION_CONTEXT Context
    );

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

