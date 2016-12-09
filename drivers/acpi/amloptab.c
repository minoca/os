/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    amloptab.c

Abstract:

    This module implements ACPI opcode and statement tables used for executing
    AML code.

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
#include "amlops.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the two-byte opcode prefix.
//

#define TWO_BYTE_OPCODE_PREFIX 0x5B

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
AcpipForwardToTwoByteOpcode (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define an array that creates ACPI statements based on the first opcode byte.
//

PAML_CREATE_NEXT_STATEMENT_ROUTINE AcpiCreateStatement[256] = {
    AcpipCreateZeroStatement,          // 0x00
    AcpipCreateOneStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateAliasStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateNameStatement,
    AcpipCreateDataStatement,
    AcpipCreateDataStatement,
    AcpipCreateDataStatement,
    AcpipCreateDataStatement,
    AcpipCreateDataStatement,
    AcpipCreateDataStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateScopeStatement,         // 0x10
    AcpipCreateBufferStatement,
    AcpipCreatePackageStatement,
    AcpipCreateVariablePackageStatement,
    AcpipCreateMethodStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0x20
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateInvalidOpcodeStatement, // 0x30
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0x40
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,    // 0x50
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipForwardToTwoByteOpcode,
    AcpipCreateNameStringStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateNameStringStatement,
    AcpipCreateLocalStatement,         // 0x60
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateLocalStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateArgumentStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateStoreStatement,         // 0x70
    AcpipCreateReferenceOfStatement,
    AcpipCreateAddStatement,
    AcpipCreateConcatenateStatement,
    AcpipCreateSubtractStatement,
    AcpipCreateIncrementStatement,
    AcpipCreateDecrementStatement,
    AcpipCreateMultiplyStatement,
    AcpipCreateDivideStatement,
    AcpipCreateShiftLeftStatement,
    AcpipCreateShiftRightStatement,
    AcpipCreateAndStatement,
    AcpipCreateNandStatement,
    AcpipCreateOrStatement,
    AcpipCreateNorStatement,
    AcpipCreateXorStatement,
    AcpipCreateNotStatement,           // 0x80
    AcpipCreateFindSetLeftBitStatement,
    AcpipCreateFindSetRightBitStatement,
    AcpipCreateDereferenceOfStatement,
    AcpipCreateConcatenateResourceTemplatesStatement,
    AcpipCreateModStatement,
    AcpipCreateNotifyStatement,
    AcpipCreateSizeOfStatement,
    AcpipCreateIndexStatement,
    AcpipCreateMatchStatement,
    AcpipCreateCreateDoubleWordFieldStatement,
    AcpipCreateCreateWordFieldStatement,
    AcpipCreateCreateByteFieldStatement,
    AcpipCreateCreateBitFieldStatement,
    AcpipCreateObjectTypeStatement,
    AcpipCreateCreateQuadWordFieldStatement,
    AcpipCreateLogicalAndStatement,    // 0x90
    AcpipCreateLogicalOrStatement,
    AcpipCreateLogicalNotStatement,
    AcpipCreateLogicalEqualStatement,
    AcpipCreateLogicalGreaterStatement,
    AcpipCreateLogicalLessStatement,
    AcpipCreateToBufferStatement,
    AcpipCreateToDecimalStringStatement,
    AcpipCreateToHexStringStatement,
    AcpipCreateToIntegerStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateToStringStatement,
    AcpipCreateCopyObjectStatement,
    AcpipCreateMidStatement,
    AcpipCreateContinueStatement,
    AcpipCreateIfStatement,            // 0xA0
    AcpipCreateElseStatement,
    AcpipCreateWhileStatement,
    AcpipCreateNoOpStatement,
    AcpipCreateReturnStatement,
    AcpipCreateBreakStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0xB0
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0xC0
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateBreakPointStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0xD0
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0xE0
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement, // 0xF0
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateInvalidOpcodeStatement,
    AcpipCreateOnesStatement
};

//
// Define the two-byte opcode statements.
//

PAML_CREATE_NEXT_STATEMENT_ROUTINE AcpiCreateTwoByteStatement[] = {
    AcpipCreateMutexStatement,
    AcpipCreateEventStatement,
    AcpipCreateConditionalReferenceOfStatement,
    AcpipCreateCreateFieldStatement,
    AcpipCreateLoadTableStatement,
    AcpipCreateLoadStatement,
    AcpipCreateStallStatement,
    AcpipCreateSleepStatement,
    AcpipCreateAcquireStatement,
    AcpipCreateSignalStatement,
    AcpipCreateWaitStatement,
    AcpipCreateResetStatement,
    AcpipCreateReleaseStatement,
    AcpipCreateFromBcdStatement,
    AcpipCreateToBcdStatement,
    AcpipCreateUnloadStatement,
    AcpipCreateRevisionStatement,
    AcpipCreateDebugStatement,
    AcpipCreateFatalStatement,
    AcpipCreateTimerStatement,
    AcpipCreateOperationRegionStatement,
    AcpipCreateFieldStatement,
    AcpipCreateDeviceStatement,
    AcpipCreateProcessorStatement,
    AcpipCreatePowerResourceStatement,
    AcpipCreateThermalZoneStatement,
    AcpipCreateIndexFieldStatement,
    AcpipCreateBankFieldStatement,
    AcpipCreateDataTableRegionStatement
};

//
// Store an array of function pointers that evaluate ACPI AML statements.
//

PAML_EVALUATE_STATEMENT_ROUTINE AcpiEvaluateStatement[AmlStatementCount] = {
    NULL,                                    // AmlStatementInvalid,
    AcpipEvaluateAcquireStatement,           // AmlStatementAcquire,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementAdd,
    AcpipEvaluateAliasStatement,             // AmlStatementAlias,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementAnd,
    AcpipEvaluateArgumentStatement,          // AmlStatementArgument,
    AcpipEvaluateBankFieldStatement,         // AmlStatementBankField,
    AcpipEvaluateWhileModifierStatement,     // AmlStatementBreak,
    AcpipEvaluateBreakPointStatement,        // AmlStatementBreakPoint,
    AcpipEvaluateBufferStatement,            // AmlStatementBuffer,
    AcpipEvaluateConcatenateStatement,       // AmlStatementConcatenate,
    AcpipEvaluateConcatenateResourceTemplatesStatement,
    AcpipEvaluateConditionalReferenceOfStatement,
    AcpipEvaluateWhileModifierStatement,     // AmlStatementContinue,
    AcpipEvaluateCopyObjectStatement,        // AmlStatementCopyObject,
    AcpipEvaluateCreateBufferFieldStatement, // AmlStatementCreateBufferField,
    AcpipEvaluateCreateFixedBufferFieldStatement,
    AcpipEvaluateDataStatement,              // AmlStatementData,
    NULL,                                    // AmlStatementDataTableRegion,
    AcpipEvaluateDebugStatement,             // AmlStatementDebug,
    AcpipEvaluateIncrementOrDecrementStatement, // AmlStatementDecrement,
    AcpipEvaluateDereferenceOfStatement,     // AmlStatementDereferenceOf,
    AcpipEvaluateDeviceStatement,            // AmlStatementDevice,
    AcpipEvaluateDivideStatement,            // AmlStatementDivide,
    AcpipEvaluateElseStatement,              // AmlStatementElse,
    AcpipEvaluateEventStatement,             // AmlStatementEvent,
    AcpipEvaluateExecutingMethodStatement,   // AmlStatementExecutingMethod,
    AcpipEvaluateFatalStatement,             // AmlStatementFatal,
    AcpipEvaluateFieldStatement,             // AmlStatementField,
    AcpipEvaluateFindSetBitStatement,        // AmlStatementFindSetLeftBit,
    AcpipEvaluateFindSetBitStatement,        // AmlStatementFindSetRightBit,
    AcpipEvaluateToFormatStatement,          // AmlStatementFromBcd,
    AcpipEvaluateIfStatement,                // AmlStatementIf,
    AcpipEvaluateIncrementOrDecrementStatement, // AmlStatementIncrement,
    AcpipEvaluateIndexStatement,             // AmlStatementIndex,
    AcpipEvaluateIndexFieldStatement,        // AmlStatementIndexField,
    AcpipEvaluateLoadStatement,              // AmlStatementLoad,
    NULL,                                    // AmlStatementLoadTable,
    AcpipEvaluateLocalStatement,             // AmlStatementLocal,
    AcpipEvaluateLogicalExpressionStatement, // AmlStatementLogicalAnd,
    AcpipEvaluateLogicalExpressionStatement, // AmlStatementLogicalEqual,
    AcpipEvaluateLogicalExpressionStatement, // AmlStatementLogicalGreater,
    AcpipEvaluateLogicalExpressionStatement, // AmlStatementLogicalLess,
    AcpipEvaluateLogicalNotStatement,        // AmlStatementLogicalNot,
    AcpipEvaluateLogicalExpressionStatement, // AmlStatementLogicalOr,
    AcpipEvaluateMatchStatement,             // AmlStatementMatch,
    AcpipEvaluateMethodStatement,            // AmlStatementMethod,
    AcpipEvaluateMidStatement,               // AmlStatementMid,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementMod,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementMultiply,
    AcpipEvaluateMutexStatement,             // AmlStatementMutex,
    AcpipEvaluateNameStatement,              // AmlStatementName,
    AcpipEvaluateNameStringStatement,        // AmlStatementNameString,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementNand,
    AcpipEvaluateNoOpStatement,              // AmlStatementNoOp,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementNor,
    AcpipEvaluateNotStatement,               // AmlStatementNot,
    AcpipEvaluateNotifyStatement,            // AmlStatementNotify,
    AcpipEvaluateObjectTypeStatement,        // AmlStatementObjectType,
    AcpipEvaluateIntegerStatement,           // AmlStatementOne,
    AcpipEvaluateIntegerStatement,           // AmlStatementOnes,
    AcpipEvaluateOperationRegionStatement,   // AmlStatementOperationRegion,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementOr,
    AcpipEvaluatePackageStatement,           // AmlStatementPackage,
    AcpipEvaluatePowerResourceStatement,     // AmlStatementPowerResource,
    AcpipEvaluateProcessorStatement,         // AmlStatementProcessor,
    AcpipEvaluateReferenceOfStatement,       // AmlStatementReferenceOf,
    AcpipEvaluateSyncObjectStatement,        // AmlStatementRelease,
    AcpipEvaluateSyncObjectStatement,        // AmlStatementReset,
    AcpipEvaluateReturnStatement,            // AmlStatementReturn,
    AcpipEvaluateIntegerStatement,           // AmlStatementRevision,
    AcpipEvaluateScopeStatement,             // AmlStatementScope,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementShiftLeft,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementShiftRight,
    AcpipEvaluateSyncObjectStatement,        // AmlStatementSignal,
    AcpipEvaluateSizeOfStatement,            // AmlStatementSizeOf,
    AcpipEvaluateDelayStatement,             // AmlStatementSleep,
    AcpipEvaluateDelayStatement,             // AmlStatementStall,
    AcpipEvaluateStoreStatement,             // AmlStatementStore,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementSubtract,
    AcpipEvaluateThermalZoneStatement,       // AmlStatementThermalZone,
    AcpipEvaluateIntegerStatement,           // AmlStatementTimer,
    AcpipEvaluateToFormatStatement,          // AmlStatementToBcd,
    AcpipEvaluateToFormatStatement,          // AmlStatementToBuffer,
    AcpipEvaluateToFormatStatement,          // AmlStatementToDecimalString,
    AcpipEvaluateToFormatStatement,          // AmlStatementToHexString,
    AcpipEvaluateToFormatStatement,          // AmlStatementToInteger,
    AcpipEvaluateToFormatStatement,          // AmlStatementToString,
    AcpipEvaluateUnloadStatement,            // AmlStatementUnload,
    AcpipEvaluatePackageStatement,           // AmlStatementVariablePackage,
    AcpipEvaluateWaitStatement,              // AmlStatementWait,
    AcpipEvaluateWhileStatement,             // AmlStatementWhile,
    AcpipEvaluateIntegerArithmeticStatement, // AmlStatementXor,
    AcpipEvaluateIntegerStatement,           // AmlStatementZero,
};

//
// ------------------------------------------------------------------ Functions
//

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
AcpipForwardToTwoByteOpcode (
    PAML_EXECUTION_CONTEXT Context,
    PAML_STATEMENT NextStatement
    )

/*++

Routine Description:

    This routine forwards the opcode onto a two-byte opcode handler.

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

    PAML_CREATE_NEXT_STATEMENT_ROUTINE CreateRoutine;
    PUCHAR InstructionPointer;
    UCHAR SecondOpcode;
    ULONG TableIndex;

    InstructionPointer = (PUCHAR)Context->AmlCode + Context->CurrentOffset;

    ASSERT(*InstructionPointer == TWO_BYTE_OPCODE_PREFIX);

    InstructionPointer += 1;
    Context->CurrentOffset += 1;
    if (Context->CurrentOffset >= Context->AmlCodeSize) {
        return STATUS_MALFORMED_DATA_STREAM;
    }

    SecondOpcode = *InstructionPointer;

    //
    // Determine the table index based on the second opcode. The table is
    // tightly packed, and the valid opcodes are 1-2, 0x12-0x13, 0x1F-0x2A,
    // 0x30-0x33, and 0x80-0x88.
    //

    if (SecondOpcode == 0) {
        return STATUS_MALFORMED_DATA_STREAM;

    } else if (SecondOpcode <= 0x2) {
        TableIndex = SecondOpcode - 1;

    } else if (SecondOpcode < 0x12) {
        return STATUS_MALFORMED_DATA_STREAM;

    } else if (SecondOpcode <= 0x13) {
        TableIndex = SecondOpcode - 0x10;

    } else if (SecondOpcode < 0x1F) {
        return STATUS_MALFORMED_DATA_STREAM;

    } else if (SecondOpcode <= 0x2A) {
        TableIndex = SecondOpcode - 0x1B;

    } else if (SecondOpcode < 0x30) {
        return STATUS_MALFORMED_DATA_STREAM;

    } else if (SecondOpcode <= 0x33) {
        TableIndex = SecondOpcode - 0x20;

    } else if (SecondOpcode < 0x80) {
        return STATUS_MALFORMED_DATA_STREAM;

    } else if (SecondOpcode <= 0x88) {
        TableIndex = SecondOpcode - 0x6C;

    } else {
        return STATUS_MALFORMED_DATA_STREAM;
    }

    CreateRoutine = AcpiCreateTwoByteStatement[TableIndex];
    return CreateRoutine(Context, NextStatement);
}

