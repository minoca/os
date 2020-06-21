/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    armdis.c

Abstract:

    This module implements support for the ARM disassembler.

Author:

    Evan Green 20-Mar-2014

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "dbgrtl.h"
#include "disasm.h"
#include "armdis.h"
#include "thmdis.h"

#include <string.h>
#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the bitmask of vector list printing flags.
//

#define DBG_ARM_VECTOR_LIST_FLAG_ALL_LANES 0x00000001
#define DBG_ARM_VECTOR_LIST_FLAG_INDEX     0x00000002

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgpArmDecodeUnconditional (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeDataProcessingAndMiscellaneous (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMediaInstruction (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeBranchAndBlockTransfer (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeCoprocessorSupervisor (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMemoryHintSimdMisc (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeStoreReturnState (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeReturnFromException (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeBranch (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeUndefined (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeUnpredictable (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeNop (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeChangeProcessorState (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSetEndianness (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodePreloadInstruction (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeClearExclusive (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeDataSynchronizationBarrier (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeDataMemoryBarrier (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeInstructionSynchronizationBarrier (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeParallelAdditionSubtraction (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodePackingInstructions (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeExtensionWithRotation (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSelectBytes (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodePackHalfword (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeReverse (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSaturate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSumofAbsoluteDifferences (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeBitFieldInstructions (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodePermanentlyUndefined (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeLoadStore (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeExtraLoadStore (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeDataProcessing (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeLoadImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMiscellaneous (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMsrImmediateAndHints (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMultiply (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSynchronization (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSupervisorCall (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeFloatingPointTwoRegisters (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeFloatingPointThreeRegisters (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeFloatingPointVectorConvert (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeFloatingPointVectorCompare (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdSmallMove (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdSpecialMove (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdDuplicate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdLoadStoreRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdElementLoadAllLanes (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdElementLoadStoreSingle (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdElementLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdThreeRegistersSameLength (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdOneRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdTwoRegistersWithShift (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdThreeRegistersDifferentLength (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdTwoRegistersWithScalar (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdTwoRegistersMiscellaneous (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdVectorExtract (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdVectorTableLookup (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSimdVectorDuplicate (
    PARM_DISASSEMBLY Context
    );

PSTR
DbgpArmGetLoadStoreTypeString (
    ULONG Instruction
    );

PSTR
DbgpArmGetBankedRegisterString (
    ULONG Instruction
    );

VOID
DbgpArmPrintStatusRegister (
    PSTR Operand,
    ULONG Instruction
    );

VOID
DbgpArmPrintVectorList (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG VectorStart,
    ULONG VectorCount,
    ULONG VectorIncrement,
    PSTR VectorTypeString,
    ULONG VectorIndex,
    ULONG Flags
    );

VOID
DbgpArmDecodeImmediateShift (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG Register,
    ULONG Type,
    ULONG Immediate
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define decode tables.
//

ARM_DECODE_BRANCH DbgArmTopLevelTable[] = {
    {0xF0000000, 0xF0000000, 0, DbgpArmDecodeUnconditional},
    {0x0C000000, 0x00000000, 0, DbgpArmDecodeDataProcessingAndMiscellaneous},
    {0x0E000010, 0x06000010, 0, DbgpArmDecodeMediaInstruction},
    {0x0C000000, 0x04000000, 0, DbgpArmDecodeLoadStore},
    {0x0C000000, 0x08000000, 0, DbgpArmDecodeBranchAndBlockTransfer},
    {0x0C000000, 0x0C000000, 0, DbgpArmDecodeCoprocessorSupervisor},
};

ARM_DECODE_BRANCH DbgArmUnconditionalTable[] = {
    {0x08000000, 0x00000000, 0, DbgpArmDecodeMemoryHintSimdMisc},
    {0x0E500000, 0x08400000, 0, DbgpArmDecodeStoreReturnState},
    {0x0E500000, 0x08100000, 0, DbgpArmDecodeReturnFromException},
    {0x0E000000, 0x0A000000, 0, DbgpArmDecodeBranch},
    {0x0F000010, 0x0E000000, 0, DbgpArmDecodeCoprocessorMove},
    {0x000000E0, 0x000000A0, 0, DbgpArmDecodeUndefined},
    {0x0FE00000, 0x0C400000, 0, DbgpArmDecodeCoprocessorMoveTwo},
    {0x0FA00000, 0x0C000000, 0, DbgpArmDecodeUndefined},
    {0x0E000000, 0x0C000000, 0, DbgpArmDecodeCoprocessorLoadStore},
    {0x0F100010, 0x0E000010, 0, DbgpArmDecodeCoprocessorMove},
    {0x0F100010, 0x0E100010, 0, DbgpArmDecodeCoprocessorMove},
};

ARM_DECODE_BRANCH DbgArmMemoryHintSimdMiscTable[] = {
    {0x07F10020, 0x01000000, 0, DbgpArmDecodeChangeProcessorState},
    {0x07F100F0, 0x01010000, 0, DbgpArmDecodeSetEndianness},
    {0x06000000, 0x02000000, 0, DbgpArmDecodeSimdDataProcessing},
    {0x07100000, 0x04000000, 0, DbgpArmDecodeSimdElementLoadStore},
    {0x07700000, 0x04100000, 0, DbgpArmDecodeNop},
    {0x07700000, 0x04500000, 0, DbgpArmDecodePreloadInstruction},
    {0x07300000, 0x04300000, 0, DbgpArmDecodeUnpredictable},
    {0x077F0000, 0x051F0000, 0, DbgpArmDecodeUnpredictable},
    {0x07300000, 0x05100000, 0, DbgpArmDecodePreloadInstruction},
    {0x07F00000, 0x05300000, 0, DbgpArmDecodeUnpredictable},
    {0x07F000F0, 0x05700000, 0, DbgpArmDecodeUnpredictable},
    {0x07F000E0, 0x05700020, 0, DbgpArmDecodeUnpredictable},
    {0x07F000F0, 0x05700070, 0, DbgpArmDecodeUnpredictable},
    {0x07F00080, 0x05700080, 0, DbgpArmDecodeUnpredictable},
    {0x07F000F0, 0x05700010, 0, DbgpArmDecodeClearExclusive},
    {0x07F000F0, 0x05700040, 0, DbgpArmDecodeDataSynchronizationBarrier},
    {0x07F000F0, 0x05700050, 0, DbgpArmDecodeDataMemoryBarrier},
    {0x07F000F0, 0x05700060, 0, DbgpArmDecodeInstructionSynchronizationBarrier},
    {0x07B00000, 0x05B00000, 0, DbgpArmDecodeUnpredictable},
    {0x07700010, 0x06100000, 0, DbgpArmDecodeNop},
    {0x07700010, 0x06500000, 0, DbgpArmDecodePreloadInstruction},
    {0x07300010, 0x07100000, 0, DbgpArmDecodePreloadInstruction},
    {0x06300010, 0x06300000, 0, DbgpArmDecodeUnpredictable},
    {0x07F000F0, 0x07F000F0, 0, DbgpArmDecodeUndefined},
};

ARM_DECODE_BRANCH DbgArmSimdDataProcessingTable[] = {
    {0x00800000, 0x00000000, 0, DbgpArmDecodeSimdThreeRegistersSameLength},
    {0x00B80090, 0x00800010, 0, DbgpArmDecodeSimdOneRegister},
    {0x00800010, 0x00800010, 0, DbgpArmDecodeSimdTwoRegistersWithShift},
    {0x00A00050, 0x00800000, 0, DbgpArmDecodeSimdThreeRegistersDifferentLength},
    {0x00B00050, 0x00A00000, 0, DbgpArmDecodeSimdThreeRegistersDifferentLength},
    {0x00A00050, 0x00800040, 0, DbgpArmDecodeSimdTwoRegistersWithScalar},
    {0x00B00050, 0x00A00040, 0, DbgpArmDecodeSimdTwoRegistersWithScalar},
    {0x01B00010, 0x00B00000, 0, DbgpArmDecodeSimdVectorExtract},
    {0x01B00810, 0x01B00000, 0, DbgpArmDecodeSimdTwoRegistersMiscellaneous},
    {0x01B00C10, 0x01B00800, 0, DbgpArmDecodeSimdVectorTableLookup},
    {0x01B00F90, 0x01B00C00, 0, DbgpArmDecodeSimdVectorDuplicate},
};

ARM_DECODE_BRANCH DbgArmSimdElementLoadStoreTable[] = {
    {0x00A00C00, 0x00A00C00, 0, DbgpArmDecodeSimdElementLoadAllLanes},
    {0x00800000, 0x00800000, 0, DbgpArmDecodeSimdElementLoadStoreSingle},
    {0x00800000, 0x00000000, 0, DbgpArmDecodeSimdElementLoadStoreMultiple},
};

ARM_DECODE_BRANCH DbgArmDataProcessingAndMiscellaneousTable[] = {
    {0x03900080, 0x01000000, 0, DbgpArmDecodeMiscellaneous},
    {0x03900090, 0x01000080, 0, DbgpArmDecodeMultiply},
    {0x02000010, 0x00000000, 0, DbgpArmDecodeDataProcessing},
    {0x02000090, 0x00000010, 0, DbgpArmDecodeDataProcessing},
    {0x030000F0, 0x00000090, 0, DbgpArmDecodeMultiply},
    {0x030000F0, 0x01000090, 0, DbgpArmDecodeSynchronization},
    {0x032000F0, 0x002000B0, 0, DbgpArmDecodeExtraLoadStore},
    {0x032000D0, 0x002000D0, 0, DbgpArmDecodeExtraLoadStore},
    {0x020000F0, 0x000000B0, 0, DbgpArmDecodeExtraLoadStore},
    {0x020000D0, 0x000000D0, 0, DbgpArmDecodeExtraLoadStore},
    {0x03F00000, 0x03000000, 0, DbgpArmDecodeLoadImmediate},
    {0x03F00000, 0x03400000, 0, DbgpArmDecodeLoadImmediate},
    {0x03B00000, 0x03200000, 0, DbgpArmDecodeMsrImmediateAndHints},
    {0x02000000, 0x02000000, 0, DbgpArmDecodeDataProcessing},
};

ARM_DECODE_BRANCH DbgArmMediaInstructionTable[] = {
    {0x01800000, 0x00000000, 0, DbgpArmDecodeParallelAdditionSubtraction},
    {0x01800000, 0x00800000, 0, DbgpArmDecodePackingInstructions},
    {0x01800000, 0x01000000, 0, DbgpArmDecodeMultiply},
    {0x01F000E0, 0x01800000, 0, DbgpArmDecodeSumofAbsoluteDifferences},
    {0x01A00060, 0x01A00040, 0, DbgpArmDecodeBitFieldInstructions},
    {0x01E00060, 0x01C00000, 0, DbgpArmDecodeBitFieldInstructions},
    {0xF1F000E0, 0xE1F000E0, 0, DbgpArmDecodePermanentlyUndefined},
};

ARM_DECODE_BRANCH DbgArmPackingInstructionTable[] = {
    {0x000000E0, 0x00000060, 0, DbgpArmDecodeExtensionWithRotation},
    {0x007000E0, 0x000000A0, 0, DbgpArmDecodeSelectBytes},
    {0x00700020, 0x00000000, 0, DbgpArmDecodePackHalfword},
    {0x00300060, 0x00300020, 0, DbgpArmDecodeReverse},
    {0x003000E0, 0x00200020, 0, DbgpArmDecodeSaturate},
    {0x00200020, 0x00200000, 0, DbgpArmDecodeSaturate}
};

ARM_DECODE_BRANCH DbgArmBranchAndBlockTransferTable[] = {
    {0x02000000, 0x02000000, 0, DbgpArmDecodeBranch},
    {0x02000000, 0x00000000, 0, DbgpArmDecodeLoadStoreMultiple},
};

ARM_DECODE_BRANCH DbgArmCoprocessorSupervisorTable[] = {
    {0x03E00000, 0x00000000, 0, DbgpArmDecodeUndefined},
    {0x03000000, 0x03000000, 0, DbgpArmDecodeSupervisorCall},
    {0x03E00E00, 0x00400A00, 0, DbgpArmDecodeSimd64BitTransfers},
    {0x02000E00, 0x00000A00, 0, DbgpArmDecodeSimdLoadStore},
    {0x03000E10, 0x02000A00, 0, DbgpArmDecodeFloatingPoint},
    {0x03000E10, 0x02000A10, 0, DbgpArmDecodeSimdSmallTransfers},
    {0x03E00000, 0x00400000, 0, DbgpArmDecodeCoprocessorMoveTwo},
    {0x02000000, 0x00000000, 0, DbgpArmDecodeCoprocessorLoadStore},
    {0x03000000, 0x02000000, 0, DbgpArmDecodeCoprocessorMove},
};

ARM_DECODE_BRANCH DbgArmFloatingPointTable[] = {
    {0x00B00040, 0x00B00000, 0, DbgpArmDecodeFloatingPointTwoRegisters},
    {0x00BE0040, 0x00B00040, 0, DbgpArmDecodeFloatingPointTwoRegisters},
    {0x00BE0040, 0x00B20040, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00BE0040, 0x00B40040, 0, DbgpArmDecodeFloatingPointVectorCompare},
    {0x00BF00C0, 0x00B700C0, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00BF0040, 0x00B80040, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00BE0040, 0x00BA0040, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00BE0040, 0x00BC0040, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00BE0040, 0x00BE0040, 0, DbgpArmDecodeFloatingPointVectorConvert},
    {0x00B00000, 0x00B00000, 0, DbgpArmDecodeUndefined},
    {0x00B00040, 0x00800040, 0, DbgpArmDecodeUndefined},
    {0x00000000, 0x00000000, 0, DbgpArmDecodeFloatingPointThreeRegisters},
};

ARM_DECODE_BRANCH DbgArmSimdSmallTransferTable[] = {
    {0x00F00100, 0x00000000, 0, DbgpArmDecodeSimdSmallMove},
    {0x00F00100, 0x00E00000, 0, DbgpArmDecodeSimdSpecialMove},
    {0x00900100, 0x00000100, 0, DbgpArmDecodeSimdSmallMove},
    {0x00900140, 0x00800100, 0, DbgpArmDecodeSimdDuplicate},
    {0x00F00100, 0x00100000, 0, DbgpArmDecodeSimdSmallMove},
    {0x00F00100, 0x00F00000, 0, DbgpArmDecodeSimdSpecialMove},
    {0x00100100, 0x00100100, 0, DbgpArmDecodeSimdSmallMove},
};

ARM_DECODE_BRANCH DbgArmSimdLoadStoreTable[] = {
    {0x01200000, 0x01000000, 0, DbgpArmDecodeSimdLoadStoreRegister},
    {0x01800000, 0x00800000, 0, DbgpArmDecodeSimdLoadStoreMultiple},
    {0x01800000, 0x01000000, 0, DbgpArmDecodeSimdLoadStoreMultiple},
};

PSTR DbgArmRegisterNames[] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "r6",
    "r7",
    "r8",
    "r9",
    "r10",
    "fp",
    "ip",
    "sp",
    "lr",
    "pc",
    "f0"
    "f1",
    "f2",
    "f3",
    "f4",
    "f5",
    "f6",
    "f7",
    "fps",
    "cpsr"
};

PSTR DbgArmSpecialRegisterNames[16] = {
    "fpsid",
    "fpscr",
    "<arch>",
    "<arch>",
    "<arch>",
    "<arch>",
    "mvfr1",
    "mvfr0",
    "fpexc",
    "fpinst",
    "fpinst2",
    "<arch>",
    "<arch>",
    "<arch>",
    "<arch>",
    "<arch>",
};

PSTR DbgArmConditionCodes[16] = {
    "eq",
    "ne",
    "cs",
    "cc",
    "mi",
    "pl",
    "vs",
    "vc",
    "hi",
    "ls",
    "ge",
    "lt",
    "gt",
    "le",
    "",
    "",
};

PSTR DbgArmDataProcessingMnemonics[16] = {
    "and",
    "eor",
    "sub",
    "rsb",
    "add",
    "adc",
    "sbc",
    "rsc",
    "tst",
    "teq",
    "cmp",
    "cmn",
    "orr",
    "mov",
    "bic",
    "mvn"
};

PSTR DbgArmSynchronizationMnemonics[8] = {
    "strex",
    "ldrex",
    "strexd",
    "ldrexd",
    "strexb",
    "ldrexb",
    "strexh",
    "ldrexh"
};

PSTR DbgArmBankedRegisters[64] = {
    "r8_usr",
    "r9_usr",
    "r10_usr",
    "r11_usr",
    "r12_usr",
    "sp_usr",
    "lr_usr",
    "UNPREDICTABLE",
    "r8_fiq",
    "r9_fiq",
    "r10_fiq",
    "r11_fiq",
    "r12_fiq",
    "sp_fiq",
    "lr_fiq",
    "UNPREDICTABLE",
    "lr_irq",
    "sp_irq",
    "lr_svc",
    "sp_svc",
    "lr_abr",
    "sp_abt",
    "lr_und",
    "sp_und",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "lr_mon",
    "sp_mon",
    "elr_hyp",
    "sp_hyp",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "spsr_fiq",
    "UNPREDICTABLE",
    "spsr_irq",
    "UNPREDICTABLE",
    "spsr_svc",
    "UNPREDICTABLE",
    "spsr_abt",
    "UNPREDICTABLE",
    "spsr_und",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "UNPREDICTABLE",
    "spsr_mon",
    "spsr_hyp"
};

PSTR DbgArmParallelArithmeticMnemonics[2][24] = {
    {
        "sadd16",
        "sasx",
        "ssax",
        "ssub16",
        "sadd8",
        NULL,
        NULL,
        "ssub8",
        "qadd16",
        "qasx",
        "qsax",
        "qsub16",
        "qadd8",
        NULL,
        NULL,
        "qsub8",
        "shadd16",
        "shasx",
        "shsax",
        "shsub16",
        "shadd8",
        NULL,
        NULL,
        "shsub8"
    },

    {
        "uadd16",
        "uasx",
        "usax",
        "usub16",
        "uadd8",
        NULL,
        NULL,
        "usub8",
        "uqadd16",
        "uqasx"
        "uqsax"
        "uqsub16",
        "uqadd8",
        NULL,
        NULL,
        "uqsub8",
        "uhadd16",
        "uhasx",
        "uhsax",
        "uhsub16",
        "uhadd8",
        NULL,
        NULL,
        "uhsub8"
    }
};

PSTR DbgArmExtensionRotationMnemonics[2][8] = {
    {
        "sxtab16",
        NULL,
        "sxtab",
        "sxtah",
        "uxtab16",
        NULL,
        "uxtab",
        "uxtah"
    },

    {
        "sxtb16",
        NULL,
        "sxtb",
        "sxth",
        "uxtb16",
        NULL,
        "uxtb",
        "uxth"
    }
};

PSTR DbgArmReverseMnemonics[4] = {
    "rev",
    "rbit",
    "rev16",
    "revsh"
};

PSTR DbgArmSimdElementLoadStoreMultipleElementSuffix[] = {
    ARM_SIMD_ELEMENT_LOAD_STORE_4_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_4_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_3_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_3_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX,
    ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX
};

ULONG DbgArmSimdElementLoadStoreMultipleVectorCount[] = {
    4, 4, 4, 4, 3, 3, 1, 2, 2, 2
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgpArmDisassemble (
    ULONGLONG InstructionPointer,
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly,
    MACHINE_LANGUAGE Language
    )

/*++

Routine Description:

    This routine decodes one instruction from an ARM binary instruction
    stream into a human readable form.

Arguments:

    InstructionPointer - Supplies the instruction pointer for the start of the
        instruction stream.

    InstructionStream - Supplies a pointer to the binary instruction stream.

    Buffer - Supplies a pointer to the buffer where the human
        readable strings will be printed. This buffer must be allocated by the
        caller.

    BufferLength - Supplies the length of the supplied buffer.

    Disassembly - Supplies a pointer to the structure that will receive
        information about the instruction.

    Language - Supplies the machine language to interpret this stream as.

Return Value:

    TRUE on success.

    FALSE if the instruction was unknown.

--*/

{

    ULONG ConditionCode;
    ARM_DISASSEMBLY Context;
    BOOL Decoded;
    BOOL Result;

    RtlZeroMemory(Disassembly, sizeof(DISASSEMBLED_INSTRUCTION));
    if (BufferLength < ARM_INSTRUCTION_SIZE) {
        Result = FALSE;
        goto ArmDisassembleEnd;
    }

    RtlZeroMemory(&Context, sizeof(ARM_DISASSEMBLY));
    Context.Result = Disassembly;
    Context.InstructionPointer = InstructionPointer;

    //
    // Get the instruction word. Always take the max, four bytes, even if the
    // instruction might only end up being two.
    //

    Context.Instruction = *((PULONG)InstructionStream);

    //
    // If this is Thumb, then just call the thumb decode function and skip the
    // rest of this.
    //

    if (Language == MachineLanguageThumb2) {
        DbgpThumbDecode(&Context);
        Result = TRUE;
        goto ArmDisassembleEnd;
    }

    //
    // Use the ARM tables to decode the instruction.
    //

    ASSERT(Language == MachineLanguageArm);

    strcpy(Context.Mnemonic, "Unknown");
    Decoded = ARM_DECODE_WITH_TABLE(&Context, DbgArmTopLevelTable);
    if (Decoded != FALSE) {
        ConditionCode = Context.Instruction >> ARM_CONDITION_SHIFT;
        strcat(Context.Mnemonic, DbgArmConditionCodes[ConditionCode]);
    }

    Result = TRUE;
    Disassembly->BinaryLength = 4;

ArmDisassembleEnd:
    if ((strlen(Context.Mnemonic) +
         strlen(Context.PostConditionMnemonicSuffix) +
         strlen(Context.Operand1) +
         strlen(Context.Operand2) +
         strlen(Context.Operand3) +
         strlen(Context.Operand4) + 5) > BufferLength) {

        Result = FALSE;
    }

    if (Result != FALSE) {
        strcat(Context.Mnemonic, Context.PostConditionMnemonicSuffix);
        strcpy(Buffer, Context.Mnemonic);
        Disassembly->Mnemonic = Buffer;
        Buffer += strlen(Context.Mnemonic) + 1;
        BufferLength -= strlen(Context.Mnemonic) + 1;
        if (*(Context.Operand1) != '\0') {
            strcpy(Buffer, Context.Operand1);
            Disassembly->DestinationOperand = Buffer;
            Buffer += strlen(Context.Operand1) + 1;
            BufferLength -= strlen(Context.Operand1) + 1;
        }

        if (*(Context.Operand2) != '\0') {
            strcpy(Buffer, Context.Operand2);
            Disassembly->SourceOperand = Buffer;
            Buffer += strlen(Context.Operand2) + 1;
            BufferLength -= strlen(Context.Operand2) + 1;
        }

        if (*(Context.Operand3) != '\0') {
            strcpy(Buffer, Context.Operand3);
            Disassembly->ThirdOperand = Buffer;
            Buffer += strlen(Context.Operand3) + 1;
            BufferLength -= strlen(Context.Operand3) + 1;
        }

        if (*(Context.Operand4) != '\0') {
            strcpy(Buffer, Context.Operand4);
            Disassembly->FourthOperand = Buffer;
        }
    }

    return Result;
}

BOOL
DbgpArmDecodeWithTable (
    PARM_DISASSEMBLY Context,
    PARM_DECODE_BRANCH Table,
    ULONG TableSize
    )

/*++

Routine Description:

    This routine checks the masks and values specified by the given table, and
    calls the appropriate disassembly routine.

Arguments:

    Context - Supplies a pointer to the disassembly context.

    Table - Supplies a pointer to the decode branch table.

    TableSize - Supplies the number of elements in the table.

Return Value:

    TRUE if a match was found.

--*/

{

    ULONG Instruction;
    ULONG Mask;
    ULONG Shift;
    ULONG TableIndex;
    ULONG Value;

    Instruction = Context->Instruction;
    for (TableIndex = 0; TableIndex < TableSize; TableIndex += 1) {
        Shift = Table[TableIndex].Shift;
        Mask = Table[TableIndex].Mask << Shift;
        Value = Table[TableIndex].Value << Shift;
        if ((Instruction & Mask) == Value) {

            //
            // Call the disassembly routine, this table entry matched.
            //

            Table[TableIndex].Disassemble(Context);
            return TRUE;
        }
    }

    //
    // Nothing matched.
    //

    return FALSE;
}

VOID
DbgpArmDecodeCoprocessorMove (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a coprocessor move instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Condition;
    ULONG Coprocessor;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Opcode1;
    ULONG Opcode2;
    ULONG RegisterD;
    ULONG RegisterM;
    ULONG RegisterN;

    Instruction = Context->Instruction;

    //
    // Get the basic fields for CDP, MRC, and MCR instructions.
    //

    Coprocessor = (Instruction & ARM_COPROCESSOR_NUMBER_MASK) >>
                  ARM_COPROCESSOR_NUMBER_SHIFT;

    RegisterD = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                ARM_DESTINATION_REGISTER_SHIFT;

    RegisterN = (Instruction & ARM_COPROCESSOR_RN_MASK) >>
                ARM_COPROCESSOR_RN_SHIFT;

    RegisterM = (Instruction & ARM_COPROCESSOR_RM_MASK) >>
                ARM_COPROCESSOR_RM_SHIFT;

    Opcode2 = (Instruction & ARM_COPROCESSOR_OPCODE2_MASK) >>
              ARM_COPROCESSOR_OPCODE2_SHIFT;

    //
    // CDP has a different opcode 1 shift, so this needs to be adjusted for
    // that instruction.
    //

    Opcode1 = (Instruction & ARM_MCR_MRC_OPCODE1_MASK) >>
              ARM_MCR_MRC_OPCODE1_SHIFT;

    //
    // If the CDP bit is 0, then this instruction is a CDP instruction.
    //

    if ((Instruction & ARM_COPROCESSOR_CDP_BIT) == 0) {
        BaseMnemonic = ARM_CDP_MNEMONIC;
        Opcode1 = (Instruction & ARM_CDP_OPCODE1_MASK) >> ARM_CDP_OPCODE1_SHIFT;
        sprintf(Context->Operand2, "c%d", RegisterD);

    //
    // If it's not a CDP instruction, check the other constant bit, which if
    // set indicates a MRC, if clear indicates MCR.
    //

    } else if ((Instruction & ARM_COPROCESSOR_MRC_BIT) != 0) {
        BaseMnemonic = ARM_MRC_MNEMONIC;
        sprintf(Context->Operand2, "r%d", RegisterD);

    } else {
        BaseMnemonic = ARM_MCR_MNEMONIC;
        sprintf(Context->Operand2, "r%d", RegisterD);
    }

    //
    // If the condition is 0xF, then these are CDP2, MRC2, and MCR2
    // instructions.
    //

    MnemonicSuffix = "";
    Condition = Context->Instruction >> ARM_CONDITION_SHIFT;
    if (Condition == ARM_CONDITION_UNCONDITIONAL) {
        MnemonicSuffix = "2";
    }

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, MnemonicSuffix);
    sprintf(Context->Operand1, "p%d, %d", Coprocessor, Opcode1);
    sprintf(Context->Operand3, "c%d, c%d, %d", RegisterN, RegisterM, Opcode2);
    return;
}

VOID
DbgpArmDecodeCoprocessorMoveTwo (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a coprocessor move instruction to/from two ARM
    registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Condition;
    ULONG Coprocessor;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Opcode1;
    ULONG RegisterM;
    ULONG RegisterT;
    ULONG RegisterT2;

    Instruction = Context->Instruction;

    //
    // Get the basic fields for MRRC and MCRR instructions.
    //

    Coprocessor = (Instruction & ARM_COPROCESSOR_NUMBER_MASK) >>
                  ARM_COPROCESSOR_NUMBER_SHIFT;

    RegisterT = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                ARM_DESTINATION_REGISTER_SHIFT;

    RegisterT2 = (Instruction & ARM_DESTINATION_REGISTER2_MASK) >>
                ARM_DESTINATION_REGISTER2_SHIFT;

    RegisterM = (Instruction & ARM_COPROCESSOR_RM_MASK) >>
                ARM_COPROCESSOR_RM_SHIFT;

    Opcode1 = (Instruction & ARM_MCRR_MRRC_OPCODE1_MASK) >>
              ARM_MCRR_MRRC_OPCODE1_SHIFT;

    //
    // Check the non-constant bit to determine if this is MRRC or MCRR.
    //

    if ((Instruction & ARM_COPROCESSOR_MRRC_BIT) != 0) {
        BaseMnemonic = ARM_MRRC_MNEMONIC;

    } else {
        BaseMnemonic = ARM_MCRR_MNEMONIC;
    }

    //
    // If the condition is 0xF, then these are MRRC2 and MCRR2 instructions.
    //

    MnemonicSuffix = "";
    Condition = Context->Instruction >> ARM_CONDITION_SHIFT;
    if (Condition == ARM_CONDITION_UNCONDITIONAL) {
        MnemonicSuffix = "2";
    }

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, MnemonicSuffix);
    sprintf(Context->Operand1, "p%d, %d", Coprocessor, Opcode1);
    sprintf(Context->Operand2,
            "%s, %s",
            DbgArmRegisterNames[RegisterT],
            DbgArmRegisterNames[RegisterT2]);

    sprintf(Context->Operand3, "c%d", RegisterM);
    return;
}

VOID
DbgpArmDecodeCoprocessorLoadStore (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a coprocessor data instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Condition;
    UCHAR CoprocessorDestination;
    UCHAR CoprocessorNumber;
    PSTR FirstSuffix;
    ULONG Immediate8;
    ULONG Instruction;
    UCHAR Rn;
    PSTR SecondSuffix;
    UCHAR SignCharacter;
    UCHAR WriteBack;

    Instruction = Context->Instruction;

    //
    // Determine whether it's a long load/store or regular.
    //

    SecondSuffix = "";
    if ((Instruction & ARM_COPROCESSOR_DATA_LONG_BIT) != 0) {
        SecondSuffix = ARM_COPROCESSOR_LONG_MNEMONIC;
    }

    //
    // Determine whether the immediate is added or subtracted.
    //

    if ((Instruction & ARM_ADD_SUBTRACT_BIT) != 0) {
        SignCharacter = '+';

    } else {
        SignCharacter = '-';
    }

    //
    // Get the register numbers and values involved.
    //

    CoprocessorDestination =
                    (Instruction & ARM_COPROCESSOR_DATA_DESTINATION_MASK) >>
                    ARM_COPROCESSOR_DATA_DESTINATION_SHIFT;

    Rn = (Instruction & ARM_COPROCESSOR_RN_MASK) >> ARM_COPROCESSOR_RN_SHIFT;
    Immediate8 = (UCHAR)(Instruction & 0xFF);
    CoprocessorNumber = (Instruction & ARM_COPROCESSOR_NUMBER_MASK) >>
                        ARM_COPROCESSOR_NUMBER_SHIFT;

    //
    // Determine the mnemonic.
    //

    if ((Instruction & ARM_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_COPROCESSOR_LOAD_MNEMONIC;

    } else {
        BaseMnemonic = ARM_COPROCESSOR_STORE_MNEMONIC;
    }

    //
    // If the condition is 0xF, then these are MRRC2 and MCRR2 instructions.
    //

    FirstSuffix = "";
    Condition = Context->Instruction >> ARM_CONDITION_SHIFT;
    if (Condition == ARM_CONDITION_UNCONDITIONAL) {
        FirstSuffix = "2";
    }

    sprintf(Context->Mnemonic,
            "%s%s%s",
            BaseMnemonic,
            FirstSuffix,
            SecondSuffix);

    //
    // Write out the first two operands.
    //

    sprintf(Context->Operand1, "p%d", CoprocessorNumber);
    sprintf(Context->Operand2, "c%d", CoprocessorDestination);

    //
    // Depending on the addressing mode, write out the third operand. If the
    // pre-index bit is set, the addressing mode is either pre-indexed or
    // offset.
    //

    if ((Instruction & ARM_PREINDEX_BIT) != 0) {
        if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
            WriteBack = '!';

        } else {
            WriteBack = ' ';
        }

        sprintf(Context->Operand3,
                "[%s, #%c%d]%c",
                DbgArmRegisterNames[Rn],
                SignCharacter,
                Immediate8 * 4,
                WriteBack);

    //
    // The pre-index bit is not set, so the addressing mode is either post-
    // indexed or unindexed.
    //

    } else {
        if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
            sprintf(Context->Operand3,
                    "[%s], #%c%d",
                    DbgArmRegisterNames[Rn],
                    SignCharacter,
                    Immediate8 * 4);

        } else {
            sprintf(Context->Operand3,
                    "[%s], {%d}",
                    DbgArmRegisterNames[Rn],
                    Immediate8);
        }
    }

    return;
}

VOID
DbgpArmDecodeFloatingPoint (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a floating point data processing instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmFloatingPointTable);
    return;
}

VOID
DbgpArmDecodeSimdSmallTransfers (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a transfers between SIMD and floating point 8-bit,
    16-bit, and 32-bit registers and the ARM core.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmSimdSmallTransferTable);
    return;
}

VOID
DbgpArmDecodeSimd64BitTransfers (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a transfers between SIMD and floating point 64-bit
    registers and the ARM core.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR RegisterString;
    ULONG Rt;
    ULONG Rt2;
    ULONG Vector;
    PSTR VectorString;

    Instruction = Context->Instruction;
    Vector = (Instruction & ARM_SIMD_TRANSFER_64_VECTOR_MASK) >>
             ARM_SIMD_TRANSFER_64_VECTOR_SHIFT;

    Rt = (Instruction & ARM_SIMD_TRANSFER_64_RT_MASK) >>
         ARM_SIMD_TRANSFER_64_RT_SHIFT;

    Rt2 = (Instruction & ARM_SIMD_TRANSFER_64_RT2_MASK) >>
          ARM_SIMD_TRANSFER_64_RT2_SHIFT;

    if ((Instruction & ARM_SIMD_TRANSFER_64_TO_REGISTER) != 0) {
        RegisterString = Context->Operand1;
        VectorString = Context->Operand2;

    } else {
        VectorString = Context->Operand1;
        RegisterString = Context->Operand2;
    }

    strcpy(Context->Mnemonic, ARM_VMOV_MNEMONIC);
    sprintf(RegisterString,
            "%s, %s",
            DbgArmRegisterNames[Rt],
            DbgArmRegisterNames[Rt2]);

    if ((Instruction & ARM_SIMD_TRANSFER_64_DOUBLE) != 0) {
        if ((Instruction & ARM_SIMD_TRANSFER_64_VECTOR_BIT) != 0) {
            Vector |= (1 << 4);
        }

        sprintf(VectorString,
                "%s%d",
                ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
                Vector);

    } else {
        Vector <<= 1;
        if ((Instruction & ARM_SIMD_TRANSFER_64_VECTOR_BIT) != 0) {
            Vector |= 1;
        }

        sprintf(VectorString,
                "%s%d, %s%d",
                ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR,
                Vector,
                ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR,
                Vector + 1);
    }

    return;
}

VOID
DbgpArmDecodeSimdLoadStore (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a load/store instructions involving SIMD and floating
    point registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmSimdLoadStoreTable);
    return;
}

VOID
DbgpArmDecodeSimdElementLoadStore (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a SIMD element and structure load and store
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmSimdElementLoadStoreTable);
    return;
}

VOID
DbgpArmDecodeSimdDataProcessing (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the SIMD data processing instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmSimdDataProcessingTable);
    return;
}

VOID
DbgpArmDecodeRegisterList (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG RegisterList
    )

/*++

Routine Description:

    This routine converts an ARM register list to a string.

Arguments:

    Destination - Supplies a pointer to the destination string.

    DestinationSize - Supplies the size of the destination string.

    RegisterList - Supplies the register bitmask.

Return Value:

    None.

--*/

{

    ULONG CurrentRegister;
    BOOL FirstRegister;

    strcpy(Destination, "{");

    //
    // Loop through the registers, adding the ones specified in the bitfield.
    //

    FirstRegister = TRUE;
    for (CurrentRegister = 0; CurrentRegister < 16; CurrentRegister += 1) {
        if ((RegisterList & 0x00000001) != 0) {
            if (FirstRegister == FALSE) {
                strcat(Destination, ", ");
            }

            strcat(Destination, DbgArmRegisterNames[CurrentRegister]);
            FirstRegister = FALSE;
        }

        RegisterList = RegisterList >> 1;
    }

    strcat(Destination, "}");
    return;
}

VOID
DbgpArmPrintMode (
    PSTR Destination,
    ULONG Mode
    )

/*++

Routine Description:

    This routine prints the given ARM processor mode.

Arguments:

    Destination - Supplies a pointer where the mode will be printed.

    Mode - Supplies the mode to print. Only the bottom 5 bits will be examined.

Return Value:

    None.

--*/

{

    PSTR ModeString;

    Mode &= ARM_MODE_MASK;
    ModeString = NULL;
    if (Mode == ARM_MODE_USER) {
        ModeString = ARM_MODE_USER_STRING;

    } else if (Mode == ARM_MODE_FIQ) {
        ModeString = ARM_MODE_FIQ_STRING;

    } else if (Mode == ARM_MODE_IRQ) {
        ModeString = ARM_MODE_IRQ_STRING;

    } else if (Mode == ARM_MODE_SVC) {
        ModeString = ARM_MODE_SVC_STRING;

    } else if (Mode == ARM_MODE_ABORT) {
        ModeString = ARM_MODE_ABORT_STRING;

    } else if (Mode == ARM_MODE_UNDEF) {
        ModeString = ARM_MODE_UNDEF_STRING;

    } else if (Mode == ARM_MODE_SYSTEM) {
        ModeString = ARM_MODE_SYSTEM_STRING;
    }

    if (ModeString != NULL) {
        sprintf(Destination, "#%s", ModeString);

    } else {
        sprintf(Destination, "%02X", Mode);
    }

    return;
}

VOID
DbgpArmPrintBarrierMode (
    PSTR Destination,
    ULONG Mode
    )

/*++

Routine Description:

    This routine prints the memory barrier (dsb, dmb, isb) type. For full
    system (sy), nothing is printed.

Arguments:

    Destination - Supplies a pointer where the mode will be printed.

    Mode - Supplies the mode to print. Only the bottom 4 bits will be examined.

Return Value:

    None.

--*/

{

    PSTR ModeString;

    ModeString = NULL;
    Mode &= ARM_BARRIER_MODE_MASK;
    if (Mode == ARM_BARRIER_MODE_FULL) {
        ModeString = ARM_BARRIER_MODE_FULL_STRING;

    } else if (Mode == ARM_BARRIER_MODE_ST) {
        ModeString = ARM_BARRIER_MODE_ST_STRING;

    } else if (Mode == ARM_BARRIER_MODE_ISH) {
        ModeString = ARM_BARRIER_MODE_ISH_STRING;

    } else if (Mode == ARM_BARRIER_MODE_ISHST) {
        ModeString = ARM_BARRIER_MODE_ISHST_STRING;

    } else if (Mode == ARM_BARRIER_MODE_NSH) {
        ModeString = ARM_BARRIER_MODE_NSH_STRING;

    } else if (Mode == ARM_BARRIER_MODE_NSHST) {
        ModeString = ARM_BARRIER_MODE_NSHST_STRING;

    } else if (Mode == ARM_BARRIER_MODE_OSH) {
        ModeString = ARM_BARRIER_MODE_OSH_STRING;

    } else if (Mode == ARM_BARRIER_MODE_OSHST) {
        ModeString = ARM_BARRIER_MODE_OSHST_STRING;
    }

    if (ModeString != NULL) {
        strcpy(Destination, ModeString);

    } else {
        sprintf(Destination, "#%02X", Mode);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpArmDecodeUnconditional (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes instructions with the unconditional condition code 0xF.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmUnconditionalTable);
    return;
}

VOID
DbgpArmDecodeDataProcessingAndMiscellaneous (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the data processing and miscellaneous instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmDataProcessingAndMiscellaneousTable);
    return;
}

VOID
DbgpArmDecodeMediaInstruction (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an instruction that falls into the Media Extension
    class of instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmMediaInstructionTable);
    return;
}

VOID
DbgpArmDecodeBranchAndBlockTransfer (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes both branch and block transfer instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmBranchAndBlockTransferTable);
    return;
}

VOID
DbgpArmDecodeCoprocessorSupervisor (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a coprocessor move or supervisor instruction. This
    routine also decodes floating point instructions and SIMD to floating point
    transfers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmCoprocessorSupervisorTable);
    return;
}

VOID
DbgpArmDecodeMemoryHintSimdMisc (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes instructions that are either memory hints, advanced
    SIMD instructions, or miscellaneous instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmMemoryHintSimdMiscTable);
    return;
}

VOID
DbgpArmDecodeStoreReturnState (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the store return state (SRS) instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR MnemonicSuffix;
    INT Result;

    MnemonicSuffix = DbgpArmGetLoadStoreTypeString(Context->Instruction);
    sprintf(Context->Mnemonic, "%s%s", ARM_SRS_MNEMONIC, MnemonicSuffix);
    DbgpArmPrintMode(Context->Operand2, Context->Instruction);
    if ((Context->Instruction & ARM_WRITE_BACK_BIT) != 0) {
        Result = snprintf(Context->Operand1,
                          sizeof(Context->Operand1),
                          "%s!, %s",
                          DbgArmRegisterNames[ARM_STACK_REGISTER],
                          Context->Operand2);

    } else {
        Result = snprintf(Context->Operand1,
                          sizeof(Context->Operand1),
                          "%s, %s",
                          DbgArmRegisterNames[ARM_STACK_REGISTER],
                          Context->Operand2);
    }

    if (Result < 0) {
        Context->Operand1[0] = '\0';
    }

    Context->Operand2[0] = '\0';
    return;
}

VOID
DbgpArmDecodeReturnFromException (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the return from exception (RFE) instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR MnemonicSuffix;
    ULONG Rn;

    Rn = (Context->Instruction & ARM_UNCONDITIONAL_RN_MASK) >>
         ARM_UNCONDITIONAL_RN_SHIFT;

    MnemonicSuffix = DbgpArmGetLoadStoreTypeString(Context->Instruction);
    sprintf(Context->Mnemonic, "%s%s", ARM_RFE_MNEMONIC, MnemonicSuffix);
    if ((Context->Instruction & ARM_WRITE_BACK_BIT) != 0) {
        sprintf(Context->Operand1, "%s!", DbgArmRegisterNames[Rn]);

    } else {
        sprintf(Context->Operand1, "%s", DbgArmRegisterNames[Rn]);
    }

    return;
}

VOID
DbgpArmDecodeBranch (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the branch instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Condition;
    ULONG Instruction;
    LONG Offset;
    ULONGLONG OperandAddress;

    Instruction = Context->Instruction;
    Offset = (Instruction & ARM_IMMEDIATE24_MASK) << 2;

    //
    // If the condition is set to unconditional, then this is a BLX
    // instruction. The current instruction set is ARM and the target
    // instruction set is Thumb.
    //

    Condition = Instruction >> ARM_CONDITION_SHIFT;
    if (Condition == ARM_CONDITION_UNCONDITIONAL) {
        BaseMnemonic = ARM_BLX_MNEMONIC;
        if ((Instruction & ARM_BLX_H_BIT) != 0) {
            Offset |= 0x2;
        }

        //
        // Or in the bottom bit as this is a transition to Thumb mode and all
        // addresses should off by 1.
        //

        Offset |= 0x1;

    //
    // Otherwise if the link bit is set, then it is a BL instruction with the
    // current and target instruction set both being ARM.
    //

    } else if ((Instruction & ARM_BRANCH_LINK_BIT) != 0) {
        BaseMnemonic = ARM_BL_MNEMONIC;

    //
    // Otherwise it is just a plain branch instruction.
    //

    } else {
        BaseMnemonic = ARM_B_MNEMONIC;
    }

    //
    // Sign-extend the offset.
    //

    if ((Offset & 0x02000000) != 0) {
        Offset |= 0xFC000000;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);

    //
    // The immediate value in the instruction is relative to the PC value of
    // this instruction, which is this instruction's address plus 8.
    //

    OperandAddress = Context->InstructionPointer + 8;
    OperandAddress += (LONGLONG)Offset;
    Context->Result->OperandAddress = OperandAddress;
    Context->Result->AddressIsDestination = TRUE;
    Context->Result->AddressIsValid = TRUE;
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "[0x%08llx]",
             OperandAddress);

    return;
}

VOID
DbgpArmDecodeUndefined (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine catches undefined corners of the instruction space.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    strcpy(Context->Mnemonic, "Undefined");
    return;
}

VOID
DbgpArmDecodeUnpredictable (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine catches unpredictable corners of the instruction space.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    strcpy(Context->Mnemonic, "Unpredictable");
    return;
}

VOID
DbgpArmDecodeNop (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine handles instructions that are treated as no operation.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    strcpy(Context->Mnemonic, "NOP");
    return;
}

VOID
DbgpArmDecodeChangeProcessorState (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the change processor state instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;

    Instruction = Context->Instruction;
    if ((Instruction & ARM_CPS_IMOD_DISABLE) != 0) {
        strcpy(Context->Mnemonic, ARM_CPS_MNEMONIC_DISABLE);

    } else {
        strcpy(Context->Mnemonic, ARM_CPS_MNEMONIC_ENABLE);
    }

    Context->Operand1[0] = '\0';
    if ((Instruction & ARM_CPS_FLAG_A) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_A_STRING);
    }

    if ((Instruction & ARM_CPS_FLAG_I) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_I_STRING);
    }

    if ((Instruction & ARM_CPS_FLAG_F) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_F_STRING);
    }

    if ((Instruction & ARM_MODE_MASK) != 0) {
        DbgpArmPrintMode(Context->Operand2, Instruction);
        if ((Instruction &
             (ARM_CPS_FLAG_A | ARM_CPS_FLAG_I | ARM_CPS_FLAG_F)) != 0) {

             strcat(Context->Operand1, ", ");
        }

        strcat(Context->Operand1, Context->Operand2);
        Context->Operand2[0] = '\0';
    }

    return;
}

VOID
DbgpArmDecodeSetEndianness (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the set endianness instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    if ((Context->Instruction & ARM_SETEND_BIG_ENDIAN) != 0) {
        strcpy(Context->Operand1, ARM_SETEND_BE_STRING);

    } else {
        strcpy(Context->Operand1, ARM_SETEND_LE_STRING);
    }

    strcpy(Context->Mnemonic, ARM_SETEND_MNEMONIC);
    return;
}

VOID
DbgpArmDecodePreloadInstruction (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the preload instructions, both the immediate/literal
    versions and the register based versions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Immediate;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    LONG Offset;
    ULONGLONG OperandAddress;
    ULONG RegisterM;
    ULONG RegisterN;
    CHAR ShiftString[35];
    CHAR SignCharacter;

    //
    // Sort out the bits that are common to the immediate and register preload
    // instructions.
    //

    Instruction = Context->Instruction;
    MnemonicSuffix = "";
    if ((Instruction & ARM_PRELOAD_DATA_BIT) != 0) {
        BaseMnemonic = ARM_PRELOAD_DATA_MNEMONIC;
        if ((Instruction & ARM_PRELOAD_DATA_READ_BIT) == 0) {
            MnemonicSuffix = "w";
        }

    } else {
        BaseMnemonic = ARM_PRELOAD_MNEMONIC;
    }

    RegisterN = (Instruction & ARM_PRELOAD_RN_MASK) >> ARM_PRELOAD_RN_SHIFT;
    if ((Instruction & ARM_PRELOAD_ADD_BIT) != 0) {
        SignCharacter = '+';

    } else {
        SignCharacter = '-';
    }

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, MnemonicSuffix);

    //
    // If this is a register preload instruction, get the second register and
    // calculate the shift value.
    //

    if ((Instruction & ARM_PRELOAD_REGISTER_BIT) != 0) {
        Immediate = (Instruction & ARM_PRELOAD_IMMEDIATE5_MASK) >>
                    ARM_PRELOAD_IMMEDIATE5_SHIFT;

        RegisterM = (Instruction & ARM_PRELOAD_RM_MASK) >> ARM_PRELOAD_RM_SHIFT;
        DbgpArmDecodeImmediateShift(ShiftString,
                                    sizeof(ShiftString),
                                    RegisterM,
                                    (Instruction & ARM_SHIFT_TYPE),
                                    Immediate);

        sprintf(Context->Operand1,
                "[%s, %c%s]",
                DbgArmRegisterNames[RegisterN],
                SignCharacter,
                ShiftString);

    //
    // Otherwise build out the immediate/literal value.
    //

    } else {
        Offset = (Instruction & ARM_PRELOAD_IMMEDIATE12_MASK) >>
                 ARM_PRELOAD_IMMEDIATE12_SHIFT;

        //
        // If the register is the PC, then the immediate value in the
        // instruction is relative to the PC value of this instruction, which
        // is this instruction's address plus 8.
        //

        if (RegisterN == ARM_PC_REGISTER) {
            OperandAddress = Context->InstructionPointer + 8;
            if ((Instruction & ARM_PRELOAD_ADD_BIT) == 0) {
                Offset = -Offset;
            }

            OperandAddress += (LONGLONG)Offset;
            Context->Result->OperandAddress = OperandAddress;
            Context->Result->AddressIsDestination = TRUE;
            Context->Result->AddressIsValid = TRUE;
            snprintf(Context->Operand1,
                     sizeof(Context->Operand1),
                     "[0x%08llx]",
                     OperandAddress);

        } else {
            snprintf(Context->Operand1,
                     sizeof(Context->Operand1),
                     "[%s, #%c%d]",
                     DbgArmRegisterNames[RegisterN],
                     SignCharacter,
                     Offset);
        }
    }

    return;
}

VOID
DbgpArmDecodeClearExclusive (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the clear exclusive instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    strcpy(Context->Mnemonic, ARM_CLREX_MNEMONIC);
    return;
}

VOID
DbgpArmDecodeDataSynchronizationBarrier (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the data synchronization barrier instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    DbgpArmPrintBarrierMode(Context->Operand1, Context->Instruction);
    strcpy(Context->Mnemonic, ARM_DSB_MNEMONIC);
    return;
}

VOID
DbgpArmDecodeDataMemoryBarrier (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the data memory barrier instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    DbgpArmPrintBarrierMode(Context->Operand1, Context->Instruction);
    strcpy(Context->Mnemonic, ARM_DMB_MNEMONIC);
    return;
}

VOID
DbgpArmDecodeInstructionSynchronizationBarrier (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the instruction synchronization barrier instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    DbgpArmPrintBarrierMode(Context->Operand1, Context->Instruction);
    strcpy(Context->Mnemonic, ARM_ISB_MNEMONIC);
    return;
}

VOID
DbgpArmDecodeParallelAdditionSubtraction (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the parallel addition and subtraction instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Op2;
    ULONG ParallelOp;
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;
    ULONG Unsigned;

    Instruction = Context->Instruction;
    Unsigned = 0;
    if ((Instruction & ARM_PARALLEL_ARITHMETIC_UNSIGNED) != 0) {
        Unsigned = 1;
    }

    ParallelOp = (Instruction & ARM_PARALLEL_ARITHMETIC_OP1_MASK) >>
                  ARM_PARALLEL_ARITHMETIC_OP1_SHIFT;

    Op2 = (Instruction & ARM_PARALLEL_ARITHMETIC_OP2_MASK) >>
          ARM_PARALLEL_ARITHMETIC_OP2_SHIFT;

    ParallelOp |= ((Op2 - 1) << 2);
    BaseMnemonic = NULL;
    if (ParallelOp < ARM_PARALLEL_ARITHMETIC_OP_MAX) {
        BaseMnemonic = DbgArmParallelArithmeticMnemonics[Unsigned][ParallelOp];
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    Rn = (Instruction & ARM_PARALLEL_ARITHMETIC_RN_MASK) >>
         ARM_PARALLEL_ARITHMETIC_RN_SHIFT;

    Rd = (Instruction & ARM_PARALLEL_ARITHMETIC_RD_MASK) >>
         ARM_PARALLEL_ARITHMETIC_RD_SHIFT;

    Rm = (Instruction & ARM_PARALLEL_ARITHMETIC_RM_MASK) >>
         ARM_PARALLEL_ARITHMETIC_RM_SHIFT;

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpArmDecodePackingInstructions (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the packing, unpacking, saturation, and reversal
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ARM_DECODE_WITH_TABLE(Context, DbgArmPackingInstructionTable);
    return;
}

VOID
DbgpArmDecodeExtensionWithRotation (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes extension with rotation instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Op1;
    ULONG Rd;
    ULONG Rm;
    PSTR RmString;
    ULONG Rn;
    ULONG Rotation;
    PSTR RotationString;
    BOOL TwoRegisters;

    Instruction = Context->Instruction;
    Rn = (Instruction & ARM_PACKING_RN_MASK) >> ARM_PACKING_RN_SHIFT;
    Rd = (Instruction & ARM_PACKING_RD_MASK) >> ARM_PACKING_RD_SHIFT;
    Rm = (Instruction & ARM_PACKING_RM_MASK) >> ARM_PACKING_RM_SHIFT;
    Op1 = (Instruction & ARM_PACKING_OP1_MASK) >> ARM_PACKING_OP1_SHIFT;
    TwoRegisters = FALSE;
    if (Rn == 0xF) {
        BaseMnemonic = DbgArmExtensionRotationMnemonics[1][Op1];
        RmString = Context->Operand2;
        RotationString = Context->Operand3;
        TwoRegisters = TRUE;

    } else {
        BaseMnemonic = DbgArmExtensionRotationMnemonics[0][Op1];
        RmString = Context->Operand3;
        RotationString = Context->Operand4;
    }

    Rotation = (Instruction & ARM_PACKING_ROTATION_MASK) >>
               ARM_PACKING_ROTATION_SHIFT;

    Rotation <<= 3;

    //
    // If no mnemonic was found for the given op value, then the instruction is
    // undefined.
    //

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    if (TwoRegisters == FALSE) {
        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    }

    strcpy(RmString, DbgArmRegisterNames[Rm]);
    if (Rotation != 0) {
        sprintf(RotationString, "%s #%d", ARM_ROR_MNEMONIC, Rotation);
    }

    return;
}

VOID
DbgpArmDecodeSelectBytes (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the select byte instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;

    Instruction = Context->Instruction;
    Rn = (Instruction & ARM_PACKING_RN_MASK) >> ARM_PACKING_RN_SHIFT;
    Rd = (Instruction & ARM_PACKING_RD_MASK) >> ARM_PACKING_RD_SHIFT;
    Rm = (Instruction & ARM_PACKING_RM_MASK) >> ARM_PACKING_RM_SHIFT;
    strcpy(Context->Mnemonic, ARM_SEL_MNEMONIC);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpArmDecodePackHalfword (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the pack halfword instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Immediate;
    ULONG Instruction;
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;
    PSTR ShiftMnemonic;

    Instruction = Context->Instruction;
    Rn = (Instruction & ARM_PACKING_RN_MASK) >> ARM_PACKING_RN_SHIFT;
    Rd = (Instruction & ARM_PACKING_RD_MASK) >> ARM_PACKING_RD_SHIFT;
    Rm = (Instruction & ARM_PACKING_RM_MASK) >> ARM_PACKING_RM_SHIFT;
    Immediate = (Instruction & ARM_PACKING_IMMEDIATE5_MASK) >>
                ARM_PACKING_IMMEDIATE5_SHIFT;

    if ((Instruction & ARM_PACKING_TB_BIT) != 0) {
        BaseMnemonic = ARM_PKHTB_MNEMONIC;
        ShiftMnemonic = ARM_ASR_MNEMONIC;
        if (Immediate == 0) {
            Immediate = 32;
        }

    } else {
        BaseMnemonic = ARM_PKHBT_MNEMONIC;
        ShiftMnemonic = ARM_LSL_MNEMONIC;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    if (Immediate != 0) {
        sprintf(Context->Operand4, "%s #%d", ShiftMnemonic, Immediate);
    }

    return;
}

VOID
DbgpArmDecodeReverse (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the reverse instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Op1;
    ULONG Op2;
    ULONG Rd;
    ULONG ReverseIndex;
    ULONG Rm;

    Instruction = Context->Instruction;
    Rd = (Instruction & ARM_PACKING_RD_MASK) >> ARM_PACKING_RD_SHIFT;
    Rm = (Instruction & ARM_PACKING_RM_MASK) >> ARM_PACKING_RM_SHIFT;
    Op1 = (Instruction & ARM_PACKING_OP1_MASK) >> ARM_PACKING_OP1_SHIFT;
    Op2 = (Instruction & ARM_PACKING_OP2_MASK) >> ARM_PACKING_OP2_SHIFT;
    ReverseIndex = (Op1 & ARM_PACKING_OP1_REV_MASK) >>
                   ARM_PACKING_OP1_REV_SHIFT;

    ReverseIndex |= ((Op2 & ARM_PACKING_OP2_REV_MASK) >>
                     ARM_PACKING_OP2_REV_SHIFT) << 1;

    BaseMnemonic = DbgArmReverseMnemonics[ReverseIndex];
    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpArmDecodeSaturate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the saturate instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR MnemonicPrefix;
    PSTR MnemonicSuffix;
    ULONG Rd;
    ULONG Rm;
    ULONG SaturateImmediate;
    ULONG ShiftImmediate;
    PSTR ShiftMnemonic;

    Instruction = Context->Instruction;
    Rd = (Instruction & ARM_PACKING_RD_MASK) >> ARM_PACKING_RD_SHIFT;
    Rm = (Instruction & ARM_PACKING_RM_MASK) >> ARM_PACKING_RM_SHIFT;
    SaturateImmediate = (Instruction & ARM_PACKING_SAT_IMMEDIATE_MASK) >>
                        ARM_PACKING_SAT_IMMEDIATE_SHIFT;

    MnemonicPrefix = ARM_USAT_MNEMONIC;
    if ((Instruction & ARM_PACKING_SAT_UNSIGNED) == 0) {
        SaturateImmediate += 1;
        MnemonicPrefix = ARM_SSAT_MNEMONIC;
    }

    //
    // If this is a two 16-bit saturate, then there is no shift at the end.
    //

    ShiftImmediate = 0;
    MnemonicSuffix = ARM_SAT16_MNEMONIC;
    if ((Instruction & ARM_PACKING_SAT16_BIT) == 0) {
        MnemonicSuffix = "";
        ShiftImmediate = (Instruction & ARM_PACKING_IMMEDIATE5_MASK) >>
                         ARM_PACKING_IMMEDIATE5_SHIFT;

        ShiftMnemonic = ARM_LSL_MNEMONIC;
        if ((Instruction & ARM_PACKING_SHIFT_BIT) != 0) {
            ShiftMnemonic = ARM_ASR_MNEMONIC;
            if (ShiftImmediate == 0) {
                ShiftImmediate = 32;
            }
        }
    }

    sprintf(Context->Mnemonic,
            "%s%s%s",
            MnemonicPrefix,
            ARM_SAT_MNEMONIC,
            MnemonicSuffix);

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    sprintf(Context->Operand2, "#%d", SaturateImmediate);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    if (ShiftImmediate != 0) {
        sprintf(Context->Operand4, "%s #%d", ShiftMnemonic, ShiftImmediate);
    }

    return;
}

VOID
DbgpArmDecodeSumofAbsoluteDifferences (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the sum of absolute differences instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG RegisterA;
    ULONG RegisterD;
    ULONG RegisterM;
    ULONG RegisterN;
    BOOL ThreeOperands;

    Instruction = Context->Instruction;
    RegisterA = (Instruction & ARM_USAD_RA_MASK) >> ARM_USAD_RA_SHIFT;
    RegisterD = (Instruction & ARM_USAD_RD_MASK) >> ARM_USAD_RD_SHIFT;
    RegisterM = (Instruction & ARM_USAD_RM_MASK) >> ARM_USAD_RM_SHIFT;
    RegisterN = (Instruction & ARM_USAD_RN_MASK) >> ARM_USAD_RN_SHIFT;
    if (RegisterD == 0xF) {
        BaseMnemonic = ARM_USAD_MNEMONIC;
        ThreeOperands = TRUE;

    } else {
        BaseMnemonic = ARM_USADA_MNEMONIC;
        ThreeOperands = FALSE;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[RegisterD]);
    strcpy(Context->Operand2, DbgArmRegisterNames[RegisterN]);
    strcpy(Context->Operand3, DbgArmRegisterNames[RegisterM]);
    if (ThreeOperands == FALSE) {
        strcpy(Context->Operand1, DbgArmRegisterNames[RegisterA]);
    }

    return;
}

VOID
DbgpArmDecodeBitFieldInstructions (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the bit field instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Lsb;
    PSTR LsbString;
    BOOL OneRegister;
    ULONG RegisterD;
    ULONG RegisterN;
    ULONG Width;

    Instruction = Context->Instruction;
    RegisterD = (Instruction & ARM_BIT_FIELD_RD_MASK) >> ARM_BIT_FIELD_RD_SHIFT;
    RegisterN = (Instruction & ARM_BIT_FIELD_RN_MASK) >> ARM_BIT_FIELD_RN_SHIFT;
    Lsb = (Instruction & ARM_BIT_FIELD_LSB_MASK) >> ARM_BIT_FIELD_LSB_SHIFT;
    Width = (Instruction & ARM_BIT_FIELD_WIDTH_MINUS_1_MASK) >>
            ARM_BIT_FIELD_WIDTH_MINUS_1_SHIFT;

    Width += 1;
    OneRegister = FALSE;
    if ((Instruction & ARM_BIT_FIELD_EXTRACT_BIT) != 0) {
        if ((Instruction & ARM_BIT_FIELD_UNSIGNED_BIT) != 0) {
            BaseMnemonic = ARM_UBFX_MNEMONIC;

        } else {
            BaseMnemonic = ARM_SBFX_MNEMONIC;
        }

    } else {
        if (RegisterN == 0xF) {
            BaseMnemonic = ARM_BFC_MNEMONIC;
            OneRegister = TRUE;

        } else {
            BaseMnemonic = ARM_BFI_MNEMONIC;
        }

        Width -= Lsb;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[RegisterD]);
    if (OneRegister == FALSE) {
        strcpy(Context->Operand2, DbgArmRegisterNames[RegisterN]);
        LsbString = Context->Operand3;

    } else {
        LsbString = Context->Operand2;
    }

    sprintf(LsbString, "#%d, #%d", Lsb, Width);
    return;
}

VOID
DbgpArmDecodePermanentlyUndefined (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the permanently undefined instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Value;

    strcpy(Context->Mnemonic, ARM_UNDEFINED_INSTRUCTION_MNEMONIC);
    Value = ARM_SERVICE_BUILD_IMMEDIATE12_4(Context->Instruction);
    sprintf(Context->Operand1, "#%d  ; 0x%x", Value, Value);
    return;
}

VOID
DbgpArmDecodeLoadStore (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a load/store to a word or single byte.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    UCHAR BaseRegister;
    UCHAR DestinationRegister;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Offset;
    UCHAR OffsetRegister;
    CHAR ShiftString[15];
    UCHAR ShiftValue;
    UCHAR Sign;
    UCHAR WriteBack;

    BaseMnemonic = "";
    MnemonicSuffix = "";
    Instruction = Context->Instruction;

    //
    // Get the destination register.
    //

    DestinationRegister = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                          ARM_DESTINATION_REGISTER_SHIFT;

    //
    // Determine the mnemonic.
    //

    if ((Instruction & ARM_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_LOAD_MNEMONIC;

    } else {
        BaseMnemonic = ARM_STORE_MNEMONIC;
    }

    //
    // Determine the suffix. The translate bit only applies if P == 0.
    //

    if ((Instruction & ARM_LOAD_STORE_BYTE_BIT) != 0) {
        if (((Instruction & ARM_PREINDEX_BIT) == 0) &&
            ((Instruction & ARM_LOAD_STORE_TRANSLATE_BIT) != 0)) {

            MnemonicSuffix = ARM_TRANSLATE_BYTE_SUFFIX;

        } else {
            MnemonicSuffix = ARM_BYTE_TRANSFER_SUFFIX;
        }

    } else if (((Instruction & ARM_PREINDEX_BIT) == 0) &&
               ((Instruction & ARM_LOAD_STORE_TRANSLATE_BIT) != 0)) {

        MnemonicSuffix = ARM_TRANSLATE_SUFFIX;
    }

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, MnemonicSuffix);

    //
    // For immediate and register offsets, determine the sign of the offset.
    //

    if ((Instruction & ARM_ADD_SUBTRACT_BIT) != 0) {
        Sign = '+';

    } else {
        Sign = '-';
    }

    //
    // For pre-index and offset modes, determine whether a writeback is
    // performed.
    //

    WriteBack = 0;
    if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
        WriteBack = '!';
    }

    BaseRegister = (Instruction & ARM_LOAD_STORE_BASE_MASK) >>
                   ARM_LOAD_STORE_BASE_SHIFT;

    //
    // Print the operand in the correct addressing form. There are 9 unique
    // forms. Start with the immediate bit, which when 0 means the immediate
    // form is used.
    //

    if ((Instruction & ARM_IMMEDIATE_BIT) == 0) {
        Offset = Instruction & 0x00000FFF;

        //
        // Post-Indexed addressing.
        //

        if ((Instruction & ARM_PREINDEX_BIT) == 0) {
            if (Offset == 0) {
                sprintf(Context->Operand2,
                        "[%s]",
                        DbgArmRegisterNames[BaseRegister]);

            } else {
                sprintf(Context->Operand2,
                        "[%s], #%c%d",
                        DbgArmRegisterNames[BaseRegister],
                        Sign,
                        Offset);
            }

        //
        // Pre-indexed or offset addressing.
        //

        } else {
            if (Offset == 0) {
                sprintf(Context->Operand2,
                        "[%s]%c",
                        DbgArmRegisterNames[BaseRegister],
                        WriteBack);

            } else {
                sprintf(Context->Operand2,
                        "[%s, #%c%d]%c",
                        DbgArmRegisterNames[BaseRegister],
                        Sign,
                        Offset,
                        WriteBack);
            }
        }

    //
    // Register offset/index or scaled register offset/index.
    //

    } else {

        //
        // Decode the shifted register string.
        //

        OffsetRegister = Instruction & ARM_OFFSET_REGISTER;
        ShiftValue = (Instruction & ARM_LOAD_STORE_SHIFT_VALUE_MASK) >>
                     ARM_LOAD_STORE_SHIFT_VALUE_SHIFT;

        DbgpArmDecodeImmediateShift(ShiftString,
                                    sizeof(ShiftString),
                                    OffsetRegister,
                                    (Instruction & ARM_SHIFT_TYPE),
                                    ShiftValue);

        //
        // Check out the pre-index bit. If it's zero, the addressing mode is
        // post-indexed.
        //

        if ((Instruction & ARM_PREINDEX_BIT) == 0) {
            sprintf(Context->Operand2,
                    "[%s], %c%s",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    ShiftString);

        //
        // Pre-indexed or offset addressing.
        //

        } else {
            sprintf(Context->Operand2,
                    "[%s, %c%s]%c",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    ShiftString,
                    WriteBack);
        }
    }

    //
    // Write the first operand.
    //

    strcpy(Context->Operand1, DbgArmRegisterNames[DestinationRegister]);
    return;
}

VOID
DbgpArmDecodeExtraLoadStore (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an "extra load/store" in both the register and
    immediate forms.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    UCHAR BaseRegister;
    UCHAR DestinationRegister;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    UCHAR Offset;
    UCHAR OffsetRegister;
    UCHAR Sign;
    UCHAR WriteBack;

    BaseMnemonic = "ERR";
    MnemonicSuffix = "";
    Instruction = Context->Instruction;

    //
    // Determine whether or not this is a load or store, and what data size it
    // is.
    //

    switch (Instruction & ARM_HALF_WORD_TRANSFER_MASK) {
    case ARM_STORE_HALF_WORD:
        BaseMnemonic = ARM_STORE_MNEMONIC;
        MnemonicSuffix = ARM_HALF_WORD_SUFFIX;
        break;

    case ARM_LOAD_DOUBLE_WORD:
        BaseMnemonic = ARM_LOAD_MNEMONIC;
        MnemonicSuffix = ARM_DOUBLE_WORD_SUFFIX;
        break;

    case ARM_STORE_DOUBLE_WORD:
        BaseMnemonic = ARM_STORE_MNEMONIC;
        MnemonicSuffix = ARM_DOUBLE_WORD_SUFFIX;
        break;

    case ARM_LOAD_UNSIGNED_HALF_WORD:
        BaseMnemonic = ARM_LOAD_MNEMONIC;
        MnemonicSuffix = ARM_HALF_WORD_SUFFIX;
        break;

    case ARM_LOAD_SIGNED_BYTE:
        BaseMnemonic = ARM_LOAD_MNEMONIC;
        MnemonicSuffix = ARM_SIGNED_BYTE_SUFFIX;
        break;

    case ARM_LOAD_SIGNED_HALF_WORD:
        BaseMnemonic = ARM_LOAD_MNEMONIC;
        MnemonicSuffix = ARM_SIGNED_HALF_WORD_SUFFIX;
        break;

    //
    // Invalid configuration.
    //

    default:
        return;
    }

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s",
             BaseMnemonic,
             MnemonicSuffix);

    //
    // Determine whether to add or subtract the offset.
    //

    if ((Instruction & ARM_ADD_SUBTRACT_BIT) != 0) {
        Sign = '+';

    } else {
        Sign = '-';
    }

    //
    // For pre-indexed addressing modes, determine whether or not the calculated
    // address is written back. (If it's not, that's called offset addressing).
    //

    WriteBack = 0;
    if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
        WriteBack = '!';
    }

    //
    // Print the destination register in the first operand.
    //

    DestinationRegister = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                          ARM_DESTINATION_REGISTER_SHIFT;

    sprintf(Context->Operand1, "%s", DbgArmRegisterNames[DestinationRegister]);
    BaseRegister = (Instruction & 0x000F0000) >> 16;

    //
    // Handle the register form.
    //

    if ((Instruction & ARM_HALF_WORD_REGISTER_MASK) ==
        ARM_HALF_WORD_REGISTER_VALUE) {

        OffsetRegister = Instruction & 0x0000000F;

        //
        // If P is 0, then it's post-indexed addressing. W had better be zero
        // in this case. Post-indexed addressing means the base register is
        // used as the address, then the offset register is added to the base
        // and written back to the base. It takes the form of [Rn], +/-Rm.
        //

        if ((Instruction & ARM_PREINDEX_BIT) == 0) {
            if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
                return;
            }

            sprintf(Context->Operand2,
                    "[%s], %c%s",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    DbgArmRegisterNames[OffsetRegister]);

        //
        // P is 1, which means the addressing form is either pre-indexed or
        // offset based. Pre-indexed means the offset register is added to the
        // base to form the address, and is then written back. Offset addressing
        // is the same but no writeback is performed.
        //

        } else {
            sprintf(Context->Operand2,
                   "[%s, %c%s]%c",
                   DbgArmRegisterNames[BaseRegister],
                   Sign,
                   DbgArmRegisterNames[OffsetRegister],
                   WriteBack);
        }

    //
    // Handle the immediate form.
    //

    } else {
        Offset = ((Instruction & 0x00000F00) >> 4) | (Instruction & 0x0000000F);

        //
        // Like in the register form, P == 0 indicates post-indexed addressing.
        // W must be zero (just don't print if it it's not).
        //

        if ((Instruction & ARM_PREINDEX_BIT) == 0) {
            sprintf(Context->Operand2,
                    "[%s], #%c%d",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    Offset);

        //
        // Like the register case P == 1 means the addressing form is either
        // pre-indexed or offset based, depending on the U bit. If it is
        // offset based (i.e. no write-back) and the offset is zero, don't
        // print the offset.
        //

        } else if ((WriteBack == 0) && (Offset == 0)) {
            sprintf(Context->Operand2,
                   "[%s]",
                   DbgArmRegisterNames[BaseRegister]);

        } else {
            sprintf(Context->Operand2,
                   "[%s, #%c%d]%c",
                   DbgArmRegisterNames[BaseRegister],
                   Sign,
                   Offset,
                   WriteBack);
        }
    }

    return;
}

VOID
DbgpArmDecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a block load or store of multiple registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Operation;
    BOOL PushPop;
    UCHAR Register;
    ULONG RegisterList;
    ULONG RegisterListCount;
    PSTR RegisterListString;

    Instruction = Context->Instruction;
    PushPop = FALSE;
    Operation = Instruction & ARM_LOAD_STORE_OP_MASK;
    Register = (Instruction & ARM_LOAD_STORE_REGISTER_MASK) >>
               ARM_LOAD_STORE_REGISTER_SHIFT;

    RegisterList = Instruction & ARM_REGISTER_LIST_MASK;
    RegisterListCount = RtlCountSetBits32(RegisterList);

    //
    // If the instruction is targeting the stack register, then it may be a
    // push or a pop.
    //

    if ((Register == ARM_STACK_REGISTER) &&
        (RegisterListCount > 1) &&
        ((Operation == ARM_LOAD_STORE_OP_POP) ||
         (Operation == ARM_LOAD_STORE_OP_PUSH))) {

        if ((Instruction & ARM_LOAD_BIT) != 0) {
            BaseMnemonic = ARM_LOAD_POP_MNEMONIC;

        } else {
            BaseMnemonic = ARM_STORE_PUSH_MNEMONIC;
        }

        MnemonicSuffix = "";
        PushPop = TRUE;

    //
    // Otherwise determine if it is a load or a store and get the appropriate
    // suffix.
    //

    } else {
        if ((Instruction & ARM_LOAD_BIT) != 0) {
            BaseMnemonic = ARM_LOAD_MULTIPLE_MNEMONIC;

        } else {
            BaseMnemonic = ARM_STORE_MULTIPLE_MNEMONIC;
        }

        MnemonicSuffix = DbgpArmGetLoadStoreTypeString(Instruction);
    }

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s",
             BaseMnemonic,
             MnemonicSuffix);

    //
    // Write the register (the first operand). Add the ! if the operation does
    // a write back. Push/pop operations are always write back.
    //

    if (PushPop == FALSE) {
        if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
            sprintf(Context->Operand1, "%s!", DbgArmRegisterNames[Register]);

        } else {
            sprintf(Context->Operand1, "%s", DbgArmRegisterNames[Register]);
        }

        RegisterListString = Context->Operand2;

    } else {
        RegisterListString = Context->Operand1;
    }

    //
    // Get the list of registers to be loaded or stored.
    //

    DbgpArmDecodeRegisterList(RegisterListString,
                              ARM_OPERAND_LENGTH,
                              RegisterList);

    //
    // Indicate whether or not the saved PSR (SPSR) should be used instead of
    // the current PSR (CPSR). This is typically only used for returning from
    // exceptions.
    //

    if ((Instruction & ARM_USE_SAVED_PSR_BIT) != 0) {
        strcat(RegisterListString, "^");
    }

    return;
}

VOID
DbgpArmDecodeDataProcessing (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a standard data processing instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    UCHAR DestinationRegister;
    UCHAR ImmediateShift;
    ULONG ImmediateValue;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    UCHAR Opcode;
    UCHAR Operand2Register;
    UCHAR OperandRegister;
    UCHAR ShiftRegister;
    CHAR ShiftString[35];
    PSTR ShiftType;

    Instruction = Context->Instruction;
    MnemonicSuffix = "";
    ShiftString[0] = '\0';

    //
    // Get the opcode.
    //

    Opcode = (Instruction & ARM_DATA_PROCESSING_OP_MASK) >>
             ARM_DATA_PROCESSING_OP_SHIFT;

    //
    // Ignore the low bit.
    //

    Opcode >>= 1;
    BaseMnemonic = DbgArmDataProcessingMnemonics[Opcode];

    //
    // Determine whether to add the S bit. Compare instructions don't need the
    // S because it's assumed (it's the whole point of a compare to set the
    // flags).
    //

    if (((Instruction & ARM_SET_FLAGS_BIT) != 0) &&
        ((Opcode < ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MIN) ||
         (Opcode > ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MAX))) {

        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;
    }

    //
    // Build the shift operand string.
    //

    if ((Context->Instruction & ARM_IMMEDIATE_BIT) != 0) {

        //
        // The immediate form takes an 8-bit integer and shifts it by any even
        // number in the shift_imm bits.
        //

        ImmediateValue = Instruction & ARM_DATA_PROCESSING_IMMEDIATE8_MASK;
        ImmediateShift = ((Instruction &
                           ARM_DATA_PROCESSING_IMMEDIATE_ROTATE_MASK) >>
                          ARM_DATA_PROCESSING_IMMEDIATE_ROTATE_SHIFT) * 2;

        //
        // Rotate the value right by the specified number of bits.
        //

        while (ImmediateShift > 0) {
            ImmediateShift -= 1;
            if ((ImmediateValue & 0x1) != 0) {
                ImmediateValue = (ImmediateValue >> 1) | 0x80000000;

            } else {
                ImmediateValue = ImmediateValue >> 1;
            }
        }

        sprintf(ShiftString, "#%d  ; 0x%x", ImmediateValue, ImmediateValue);

    } else {
        Operand2Register = Instruction &
                           ARM_DATA_PROCESSING_OPERAND2_REGISTER_MASK;

        //
        // The register form can be shifted, by either an immediate or another
        // register. Handle the register shift case first.
        //

        if ((Instruction &
             ARM_DATA_PROCESSING_REGISTER_REGISTER_SHIFT_BIT) != 0) {

            ShiftRegister = (Instruction &
                             ARM_DATA_PROCESSING_SHIFT_REGISTER_MASK) >>
                            ARM_DATA_PROCESSING_SHIFT_REGISTER_SHIFT;

            ShiftType = "ERR";
            switch (Instruction & ARM_SHIFT_TYPE) {
            case ARM_SHIFT_LSL:
                ShiftType = ARM_LSL_MNEMONIC;
                break;

            case ARM_SHIFT_LSR:
                ShiftType = ARM_LSR_MNEMONIC;
                break;

            case ARM_SHIFT_ASR:
                ShiftType = ARM_ASR_MNEMONIC;
                break;

            case ARM_SHIFT_ROR:
                ShiftType = ARM_ROR_MNEMONIC;
                break;

            //
            // This case should never hit since all 4 bit combinations were
            // handled.
            //

            default:
                break;
            }

            //
            // If this is the move instruction, then the canonical form
            // actually uses the shift mnemonic for the instruction mnemonic.
            //

            if (Opcode == ARM_DATA_PROCESSING_MOVE_OPCODE) {
                BaseMnemonic = ShiftType;
                sprintf(ShiftString,
                        "%s, %s",
                        DbgArmRegisterNames[Operand2Register],
                        DbgArmRegisterNames[ShiftRegister]);

            } else {
                sprintf(ShiftString,
                        "%s, %s %s",
                        DbgArmRegisterNames[Operand2Register],
                        ShiftType,
                        DbgArmRegisterNames[ShiftRegister]);
            }

        //
        // Shift by an immediate value.
        //

        } else {
            ImmediateValue = (Instruction &
                              ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_MASK) >>
                             ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_SHIFT;

            //
            // If this is a move instruction, then it may have a canonical
            // form.
            //

            if (Opcode == ARM_DATA_PROCESSING_MOVE_OPCODE) {
                ShiftType = NULL;
                switch (Instruction & ARM_SHIFT_TYPE) {
                case ARM_SHIFT_LSL:
                    if (ImmediateValue != 0) {
                        ShiftType = ARM_LSL_MNEMONIC;
                    }

                    break;

                case ARM_SHIFT_LSR:
                    if (ImmediateValue == 0) {
                        ImmediateValue = 32;
                    }

                    ShiftType = ARM_LSR_MNEMONIC;
                    break;

                case ARM_SHIFT_ASR:
                    if (ImmediateValue == 0) {
                        ImmediateValue = 32;
                    }

                    ShiftType = ARM_ASR_MNEMONIC;
                    break;

                case ARM_SHIFT_ROR:
                    if (ImmediateValue == 0) {
                        ShiftType = ARM_RRX_MNEMONIC;

                    } else {
                        ShiftType = ARM_ROR_MNEMONIC;
                    }

                    break;

                //
                // This case should never hit since all 4 bit combinations were
                // handled.
                //

                default:
                    break;
                }

                //
                // If a shift type was set, then use the canonical form and
                // override the base mnemonic.
                //

                if (ShiftType != NULL) {
                    BaseMnemonic = ShiftType;
                }

                //
                // A MOV with no shift and RRX do no print an immediate value.
                // There are the only cases where the immediate value is 0.
                //

                if (ImmediateValue == 0) {
                    sprintf(ShiftString,
                            "%s",
                            DbgArmRegisterNames[Operand2Register]);

                } else {
                    sprintf(ShiftString,
                            "%s, #%d",
                            DbgArmRegisterNames[Operand2Register],
                            ImmediateValue);
                }

            } else {
                DbgpArmDecodeImmediateShift(ShiftString,
                                            sizeof(ShiftString),
                                            Operand2Register,
                                            (Instruction & ARM_SHIFT_TYPE),
                                            ImmediateValue);
            }
        }
    }

    //
    // Print out the mnemonic, it may have been modified while computing the
    // shift string.
    //

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s",
             BaseMnemonic,
             MnemonicSuffix);

    DestinationRegister = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                          ARM_DESTINATION_REGISTER_SHIFT;

    OperandRegister = (Instruction &
                       ARM_DATA_PROCESSING_OPERAND_REGISTER_MASK) >>
                      ARM_DATA_PROCESSING_OPERAND_REGISTER_SHIFT;

    //
    // Print the operands depending on the opcode. Compare instructions take
    // the form Rn, <shifter_operand>
    //

    if ((Opcode >= ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MIN) &&
        (Opcode <= ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MAX)) {

        strcpy(Context->Operand1, DbgArmRegisterNames[OperandRegister]);
        strcpy(Context->Operand2, ShiftString);

    //
    // Move instructions take the form Rd, <shift_operand>.
    //

    } else if ((Opcode == ARM_DATA_PROCESSING_MOVE_OPCODE) ||
               (Opcode == ARM_DATA_PROCESSING_MOVE_NOT_OPCODE)) {

        strcpy(Context->Operand1, DbgArmRegisterNames[DestinationRegister]);
        strcpy(Context->Operand2, ShiftString);

    //
    // All normal data processing instructions take the form Rd, Rn,
    // <shift_operand>.
    //

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[DestinationRegister]);
        strcpy(Context->Operand2, DbgArmRegisterNames[OperandRegister]);
        strcpy(Context->Operand3, ShiftString);
    }

    return;
}

VOID
DbgpArmDecodeLoadImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a 16-bit immediate load instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    UCHAR DestinationRegister;
    ULONG ImmediateValue;
    ULONG Instruction;

    Instruction = Context->Instruction;

    //
    // Get the opcode.
    //

    switch (Instruction & ARM_IMMEDIATE_LOAD_OP_MASK) {
    case ARM_IMMEDIATE_LOAD_OP_MOVW:
        BaseMnemonic = ARM_MOVW_MNEMONIC;
        break;

    case ARM_IMMEDIATE_LOAD_OP_MOVT:
        BaseMnemonic = ARM_MOVT_MNEMONIC;
        break;

    //
    // Invalid configuration.
    //

    default:
        return;
    }

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s",
             BaseMnemonic);

    //
    // Build the immediate value string.
    //

    ImmediateValue = (Instruction & ARM_IMMEDIATE_LOAD_IMMEDIATE4_MASK) >>
                     ARM_IMMEDIATE_LOAD_IMMEDIATE4_SHIFT;

    ImmediateValue <<= 12;
    ImmediateValue |= ((Instruction & ARM_IMMEDIATE_LOAD_IMMEDIATE12_MASK) >>
                       ARM_IMMEDIATE_LOAD_IMMEDIATE12_SHIFT);

    //
    // Determine the destination register.
    //

    DestinationRegister = (Instruction & ARM_DESTINATION_REGISTER_MASK) >>
                          ARM_DESTINATION_REGISTER_SHIFT;

    //
    // The 16 immediate load instructions take the form Rn, <immediate_operand>
    //

    strcpy(Context->Operand1, DbgArmRegisterNames[DestinationRegister]);
    sprintf(Context->Operand2, "#%d  ; 0x%x", ImmediateValue, ImmediateValue);
    return;
}

VOID
DbgpArmDecodeMiscellaneous (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a miscellaneous instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Op;
    ULONG Op2;
    ULONG R0;
    ULONG Rd;
    LONG Value;

    Mnemonic = "ERR";
    Instruction = Context->Instruction;
    Op2 = (Instruction & ARM_MISCELLANEOUS1_OP2_MASK) >>
          ARM_MISCELLANEOUS1_OP2_SHIFT;

    Op = (Instruction & ARM_MISCELLANEOUS1_OP_MASK) >>
         ARM_MISCELLANEOUS1_OP_SHIFT;

    R0 = Instruction & ARM_MOVE_STATUS_R0_MASK;

    //
    // Handle an MSR or MRS instruction.
    //

    if (Op2 == ARM_MISCELLANEOUS1_OP2_STATUS) {
        Rd = (Instruction & ARM_MOVE_STATUS_RD_MASK) >>
             ARM_MOVE_STATUS_RD_SHIFT;

        //
        // Handle an MSR.
        //

        if ((Op & ARM_MISCELLANEOUS1_OP_MSR) != 0) {
            Mnemonic = ARM_MSR_MNEMONIC;
            strcpy(Context->Operand2, DbgArmRegisterNames[R0]);

            //
            // Handle banked MSR vs non-banked.
            //

            if ((Instruction & ARM_MOVE_STATUS_BANKED) != 0) {
                strcpy(Context->Operand1,
                       DbgpArmGetBankedRegisterString(Instruction));

            } else {
                DbgpArmPrintStatusRegister(Context->Operand1, Instruction);
            }

        //
        // This is an MRS instruction.
        //

        } else {
            Mnemonic = ARM_MRS_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            if ((Instruction & ARM_MOVE_STATUS_BANKED) != 0) {
                strcpy(Context->Operand2,
                       DbgpArmGetBankedRegisterString(Instruction));

            } else {
                DbgpArmPrintStatusRegister(Context->Operand2, Instruction);
            }
        }

    //
    // Handle either a BX or CLZ.
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_BX_CLZ) {
        if (Op == ARM_MISCELLANEOUS1_OP_BX) {
            Mnemonic = ARM_BX_MNEMONIC;

        } else if (Op == ARM_MISCELLANEOUS1_OP_CLZ) {
            Mnemonic = ARM_CLZ_MNEMONIC;
        }

        strcpy(Context->Operand1, DbgArmRegisterNames[R0]);

    //
    // Handle a BXJ (register).
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_BXJ) {
        Mnemonic = ARM_BXJ_MNEMONIC;
        strcpy(Context->Operand1, DbgArmRegisterNames[R0]);

    //
    // Handle a BLX (register).
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_BLX) {
        Mnemonic = ARM_BLX_MNEMONIC;
        strcpy(Context->Operand1, DbgArmRegisterNames[R0]);

    //
    // Handle (or don't) saturating addition or subtraction.
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_SATURATING_ADDITION) {

    //
    // Handle a simple ERET.
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_ERET) {
        Mnemonic = ARM_ERET_MNEMONIC;

    //
    // Handle a service call: BKPT, HVC, or SMC.
    //

    } else if (Op2 == ARM_MISCELLANEOUS1_OP2_SERVICE) {
        Value = ARM_SERVICE_BUILD_IMMEDIATE12_4(Instruction);
        if ((Value & 0x00008000) != 0) {
            Value |= 0xFFFF0000;
        }

        if (Op == ARM_MISCELLANEOUS1_OP_BKPT) {
            Mnemonic = ARM_BKPT_MNEMONIC;
            sprintf(Context->Operand1, "#%d", Value);

        } else if (Op == ARM_MISCELLANEOUS1_OP_HVC) {
            Mnemonic = ARM_HVC_MNEMONIC;
            sprintf(Context->Operand1, "#%d", Value);

        } else if (Op == ARM_MISCELLANEOUS1_OP_SMC) {
            Mnemonic = ARM_SMC_MNEMONIC;
            sprintf(Context->Operand1, "#%d", Value & 0xF);
        }
    }

    strcpy(Context->Mnemonic, Mnemonic);
    return;
}

VOID
DbgpArmDecodeMsrImmediateAndHints (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an MSR immediate instruction or memory hints.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    LONG Immediate;
    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Op1;
    ULONG Op2;

    Mnemonic = "";
    Instruction = Context->Instruction;
    Op1 = (Instruction & ARM_HINTS_OP1_MASK) >> ARM_HINTS_OP1_SHIFT;
    Op2 = Instruction & ARM_HINTS_OP2_MASK;
    if (Op1 == ARM_HINTS_OP1_HINTS) {
        if (Op2 == ARM_HINTS_OP2_NOP) {
            Mnemonic = ARM_NOP_MNEMONIC;

        } else if (Op2 == ARM_HINTS_OP2_YIELD) {
            Mnemonic = ARM_YIELD_MNEMONIC;

        } else if (Op2 == ARM_HINTS_OP2_WFE) {
            Mnemonic = ARM_WFE_MNEMONIC;

        } else if (Op2 == ARM_HINTS_OP2_WFI) {
            Mnemonic = ARM_WFI_MNEMONIC;

        } else if (Op2 == ARM_HINTS_OP2_SEV) {
            Mnemonic = ARM_SEV_MNEMONIC;

        } else if ((Op2 & ARM_HINTS_OP2_DBG_MASK) == ARM_HINTS_OP2_DBG_VALUE) {
            Mnemonic = ARM_DBG_MNEMONIC;
            sprintf(Context->Operand1,
                    "#%d",
                    Op2 & ARM_HINTS_OP2_DBG_OPTION_MASK);
        }

    //
    // If not hints, then this is an MSR (immediate) instruction.
    //

    } else {
        Mnemonic = ARM_MSR_MNEMONIC;
        DbgpArmPrintStatusRegister(Context->Operand1, Instruction);
        Immediate = Instruction & ARM_MSR_IMMEDIATE12_MASK;
        if ((Immediate & 0x00001000) != 0) {
            Immediate |= 0xFFFFF000;
        }

        sprintf(Context->Operand2, "#%d  ; 0x%x", Immediate, Immediate);
    }

    strcpy(Context->Mnemonic, Mnemonic);
    return;
}

VOID
DbgpArmDecodeMultiply (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a multiply or long multiply instruction. This function
    assumes that the instruction is in fact a multiply instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    BOOL LongMultiply;
    PSTR MnemonicSuffix;
    PSTR MultiplyHalves;
    PSTR PreConditionMnemonicSuffix;
    UCHAR Rd;
    UCHAR RdHigh;
    UCHAR RdLow;
    UCHAR Rm;
    UCHAR Rn;
    PSTR Rounded;
    UCHAR Rs;
    BOOL ThreeOperands;
    PSTR XBit;

    MultiplyHalves = NULL;
    LongMultiply = FALSE;
    Rounded = NULL;
    ThreeOperands = FALSE;
    XBit = NULL;
    Instruction = Context->Instruction;
    BaseMnemonic = "ERR";
    PreConditionMnemonicSuffix = "";
    MnemonicSuffix = "";

    //
    // Get the top and bottom bits. These bits aren't actually defined for all
    // multiply instructions, so they'll get ignored in some cases.
    //

    if ((Instruction & ARM_MULTIPLY_SOURCE_HIGH) != 0) {
        if ((Instruction & ARM_MULTIPLY_DESTINATION_HIGH) != 0) {
            MultiplyHalves = ARM_MULTIPLY_TOP_TOP;

        } else {
            MultiplyHalves = ARM_MULTIPLY_TOP_BOTTOM;
        }

    } else {
        if ((Instruction & ARM_MULTIPLY_DESTINATION_HIGH) != 0) {
            MultiplyHalves = ARM_MULTIPLY_BOTTOM_TOP;

        } else {
            MultiplyHalves = ARM_MULTIPLY_BOTTOM_BOTTOM;
        }
    }

    //
    // Get the X bit, which indicates that the multiplications are
    // bottom * top and top * bottom. If X is cleared, the multiplications are
    // bottom * bottom and top * top.
    //

    if ((Instruction & ARM_MULTIPLY_X_BIT) != 0) {
        XBit = ARM_MULTIPLY_X_MNEMONIC;
    }

    //
    // Get the rounding bit, which indicates for a couple of instructions that
    // the multiplication is rounded.
    //

    if ((Instruction & ARM_MULTIPLY_ROUND_BIT) != 0) {
        Rounded = ARM_MULTIPLY_ROUND_MNEMONIC;
    }

    //
    // For a non-long multiply, get the 4 registers.
    //

    Rd = (Instruction & ARM_MULTIPLY_RD_MASK) >> ARM_MULTIPLY_RD_SHIFT;
    Rm = (Instruction & ARM_MULTIPLY_RM_MASK) >> ARM_MULTIPLY_RM_SHIFT;
    Rn = (Instruction & ARM_MULTIPLY_RN_MASK) >> ARM_MULTIPLY_RN_SHIFT;
    Rs = (Instruction & ARM_MULTIPLY_RS_MASK) >> ARM_MULTIPLY_RS_SHIFT;

    //
    // For long multiplies, get the high and low destination registers. Rs and
    // Rm are the same as for non-long multiplies.
    //

    RdHigh = (Instruction & ARM_MULTIPLY_RD_HIGH_MASK) >>
             ARM_MULTIPLY_RD_HIGH_SHIFT;

    RdLow = (Instruction & ARM_MULTIPLY_RD_LOW_MASK) >>
            ARM_MULTIPLY_RD_LOW_SHIFT;

    //
    // Get the mnemonic and characteristics of the instruction.
    //

    switch (Instruction & ARM_MULTIPLY_OPCODE_MASK) {

    //
    // Standard Multiply and accumulate.
    //

    case ARM_MLA_MASK | ARM_SET_FLAGS_BIT:
        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_MLA_MASK:
        BaseMnemonic = ARM_MLA_MNEMONIC;
        break;

    //
    // Standard Multiply.
    //

    case ARM_MUL_MASK | ARM_SET_FLAGS_BIT:
        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_MUL_MASK:
        BaseMnemonic =ARM_MUL_MNEMONIC;
        ThreeOperands = TRUE;
        break;

    //
    // Signed half word multiply and accumulate.
    //

    case ARM_SMLA_MASK:
        BaseMnemonic = ARM_SMLA_MNEMONIC;
        PreConditionMnemonicSuffix = MultiplyHalves;
        break;

    //
    // Signed half word multiply accumulate, dual,
    // Signed half word multiply subtract, dual,
    // Signed dual multiply add, and
    // Signed dual multiply subtract.
    //

    case ARM_SMLXD_MASK:
        if ((Instruction & ARM_SMLXD_OPCODE2_MASK) == ARM_SMLAD_OPCODE2_VALUE) {
            if (Rn == 0xF) {
                BaseMnemonic = ARM_SMUAD_MNEMONIC;
                ThreeOperands = TRUE;

            } else {
                BaseMnemonic = ARM_SMLAD_MNEMONIC;
            }

        } else if ((Instruction & ARM_SMLXD_OPCODE2_MASK) ==
                   ARM_SMLSD_OPCODE2_VALUE) {

            if (Rn == 0xF) {
                BaseMnemonic = ARM_SMUSD_MNEMONIC;
                ThreeOperands = TRUE;

            } else {
                BaseMnemonic = ARM_SMLSD_MNEMONIC;
            }

        } else {
            return;
        }

        PreConditionMnemonicSuffix = XBit;
        break;

    //
    // Signed half word by word, accumulate, and
    // Signed multiply word B and T.
    //

    case ARM_SMLAW_SMULW_MASK:
        if ((Instruction & ARM_SMULW_DIFFERENT_BIT) != 0) {
            BaseMnemonic = ARM_SMULW_MNEMONIC;
            ThreeOperands = TRUE;

        } else {
            BaseMnemonic = ARM_SMLAW_MNEMONIC;
        }

        if ((Instruction & ARM_MULTIPLY_DESTINATION_HIGH) != 0) {
            PreConditionMnemonicSuffix = ARM_MULTIPLY_TOP;

        } else {
            PreConditionMnemonicSuffix = ARM_MULTIPLY_BOTTOM;
        }

        break;

    //
    // Signed multiply accumulate, long.
    //

    case ARM_SMLAL_MASK | ARM_SET_FLAGS_BIT:
        PreConditionMnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_SMLAL_MASK:
        BaseMnemonic = ARM_SMLAL_MNEMONIC;
        LongMultiply = TRUE;
        break;

    //
    // Signed halfword multiply accumulate, long.
    //

    case ARM_SMLAL_XY_MASK:
        BaseMnemonic = ARM_SMLAL_MNEMONIC;
        PreConditionMnemonicSuffix = MultiplyHalves;
        LongMultiply = TRUE;
        break;

    //
    // Signed divide.
    //

    case ARM_SDIV_MASK:
        BaseMnemonic = ARM_SDIV_MNEMONIC;
        ThreeOperands = TRUE;
        break;

    //
    // Unsigned divide.
    //

    case ARM_UDIV_MASK:
        BaseMnemonic = ARM_UDIV_MNEMONIC;
        ThreeOperands = TRUE;
        break;

    //
    // Signed half word multiply accumulate, long dual, and
    // Signed half word multiply subtract, long dual.
    //

    case ARM_SMLXLD_MASK:
        if ((Instruction & ARM_SMLXLD_OPCODE2_MASK) ==
            ARM_SMLALD_OPCODE2_VALUE) {

            BaseMnemonic = ARM_SMLALD_MNEMONIC;

        } else if ((Instruction & ARM_SMLXLD_OPCODE2_MASK) ==
                   ARM_SMLSLD_OPCODE2_VALUE) {

            BaseMnemonic = ARM_SMLSLD_MNEMONIC;

        } else {
            return;
        }

        PreConditionMnemonicSuffix = XBit;
        LongMultiply = TRUE;
        break;

    //
    // Signed most significant word multiply accumulate, and
    // Signed most significant word multiply subtract, and
    // Signed most significant word multiply.
    //

    case ARM_SMMLX_MASK:
        if ((Instruction & ARM_SMMLX_OPCODE2_MASK) == ARM_SMMLA_OPCODE2_VALUE) {
            if (Rn == 0xF) {
                BaseMnemonic = ARM_SMMUL_MNEMONIC;
                ThreeOperands = TRUE;

            } else {
                BaseMnemonic = ARM_SMMLA_MNEMONIC;
            }

        } else if ((Instruction & ARM_SMMLX_OPCODE2_MASK) ==
                   ARM_SMMLS_OPCODE2_VALUE) {

            BaseMnemonic = ARM_SMMLS_MNEMONIC;

        } else {
            return;
        }

        PreConditionMnemonicSuffix = Rounded;
        break;

    //
    // Signed multiply.
    //

    case ARM_SMUL_MASK:
        BaseMnemonic = ARM_SMUL_MNEMONIC;
        PreConditionMnemonicSuffix = MultiplyHalves;
        ThreeOperands = TRUE;
        break;

    //
    // Signed multiply, long.
    //

    case ARM_SMULL_MASK | ARM_SET_FLAGS_BIT:
        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_SMULL_MASK:
        BaseMnemonic = ARM_SMULL_MNEMONIC;
        LongMultiply = TRUE;
        break;

    //
    // Unsigned multiply accumulate accumulate long.
    //

    case ARM_UMAAL_MASK:
        BaseMnemonic = ARM_UMAAL_MNEMONIC;
        LongMultiply = TRUE;
        break;

    //
    // Unsigned multiply accumulate long.
    //

    case ARM_UMLAL_MASK | ARM_SET_FLAGS_BIT:
        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_UMLAL_MASK:
        BaseMnemonic = ARM_UMLAL_MNEMONIC;
        LongMultiply = TRUE;
        break;

    //
    // Unsigned multiply long.
    //

    case ARM_UMULL_MASK | ARM_SET_FLAGS_BIT:
        MnemonicSuffix = ARM_SET_FLAGS_MNEMONIC;

        //
        // Fall through.
        //

    case ARM_UMULL_MASK:
        BaseMnemonic = ARM_UMULL_MNEMONIC;
        LongMultiply = TRUE;
        break;

    default:
        return;
    }

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s%s",
             BaseMnemonic,
             PreConditionMnemonicSuffix,
             MnemonicSuffix);

    //
    // Create the operands, depending on whether the instruction was a long
    // multiply or not.
    //

    if (LongMultiply != FALSE) {
        strcpy(Context->Operand1, DbgArmRegisterNames[RdLow]);
        strcpy(Context->Operand2, DbgArmRegisterNames[RdHigh]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
        strcpy(Context->Operand4, DbgArmRegisterNames[Rs]);

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rs]);
        if (ThreeOperands == FALSE) {
            strcpy(Context->Operand4, DbgArmRegisterNames[Rn]);
        }
    }

    return;
}

VOID
DbgpArmDecodeSynchronization (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a synchronization primitive instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Op;
    ULONG R0;
    ULONG R12;
    ULONG Rn;

    Instruction = Context->Instruction;
    Op = (Instruction & ARM_SYNCHRONIZATION_OPCODE_MASK) >>
         ARM_SYNCHRONIZATION_OPCODE_SHIFT;

    Rn = (Instruction & ARM_SYNCHRONIZATION_RN_MASK) >>
         ARM_SYNCHRONIZATION_RN_SHIFT;

    R0 = Instruction & ARM_SYNCHRONIZATION_R0_MASK;
    R12 = (Instruction & ARM_SYNCHRONIZATION_R12_MASK) >>
          ARM_SYNCHRONIZATION_R12_SHIFT;

    //
    // If the high bit of the op field is not set, then it's a swap instruction.
    //

    if ((Op & ARM_SYNCHRONIZATION_OPCODE_EXCLUSIVE) == 0) {
        if ((Instruction & ARM_SYNCHRONIZATION_SWAP_BYTE) != 0) {
            Mnemonic = ARM_SWPB_MNEMONIC;

        } else {
            Mnemonic = ARM_SWP_MNEMONIC;
        }

        strcpy(Context->Operand1, DbgArmRegisterNames[R12]);
        strcpy(Context->Operand2, DbgArmRegisterNames[R0]);
        sprintf(Context->Operand3, "[%s]", DbgArmRegisterNames[Rn]);

    //
    // It's an ldrex or strex instruction of some kind.
    //

    } else {
        Op &= ~ARM_SYNCHRONIZATION_OPCODE_EXCLUSIVE;
        Mnemonic = DbgArmSynchronizationMnemonics[Op];

        //
        // If the lowest bit of the op region is set, it's an ldrex{b,h,d}.
        //

        if ((Op & ARM_SYNCHRONIZATION_OPCODE_LOAD) != 0) {
            strcpy(Context->Operand1, DbgArmRegisterNames[R12]);
            sprintf(Context->Operand2, "[%s]", DbgArmRegisterNames[Rn]);

        } else {
            strcpy(Context->Operand1, DbgArmRegisterNames[R12]);
            strcpy(Context->Operand2, DbgArmRegisterNames[R0]);
            sprintf(Context->Operand3, "[%s]", DbgArmRegisterNames[Rn]);
        }
    }

    strcpy(Context->Mnemonic, Mnemonic);
    return;
}

VOID
DbgpArmDecodeSupervisorCall (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a supervisor call instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate;
    ULONG Instruction;

    Instruction = Context->Instruction;
    Immediate = Instruction & ARM_IMMEDIATE24_MASK;
    strcpy(Context->Mnemonic, ARM_SVC_MNEMONIC);
    sprintf(Context->Operand1, "#%d  ; 0x%x", Immediate, Immediate);
    return;
}

VOID
DbgpArmDecodeFloatingPointTwoRegisters (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a two-register floating point data processing
    instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ARM_IMMEDIATE_DOUBLE Double;
    ARM_IMMEDIATE_FLOAT Float;
    ULONG Immediate8;
    ULONG Instruction;
    ULONG Mask;
    PSTR MnemonicSuffix;
    BOOL TwoRegisters;
    ULONG VectorD;
    ULONG VectorM;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;

    //
    // Collect the vector values. If the double-precision (SZ) bit is set, then
    // the extra bit for each vector is the high bit. If the double-precision
    // bit is not set, then the extra bit is the low bit.
    //

    VectorD = (Instruction & ARM_FLOATING_POINT_VD_MASK) >>
              ARM_FLOATING_POINT_VD_SHIFT;

    VectorM = (Instruction & ARM_FLOATING_POINT_VM_MASK) >>
              ARM_FLOATING_POINT_VM_SHIFT;

    if ((Instruction & ARM_FLOATING_POINT_SZ_BIT) != 0) {
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= (1 << 4);
        }

        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= (1 << 4);
        }

        MnemonicSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;

    } else {
        VectorD <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= 1;
        }

        VectorM <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= 1;
        }

        MnemonicSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
    }

    //
    // If the op bit is not set, then this is actually a VMOV immediate and not
    // a two register instruction.
    //

    if ((Instruction & ARM_FLOATING_POINT_OP_BIT) == 0) {
        BaseMnemonic = ARM_VMOV_MNEMONIC;
        Immediate8 = ARM_FLOATING_POINT_BUILD_IMMEDIATE8(Instruction);
        if ((Instruction & ARM_FLOATING_POINT_SZ_BIT) != 0) {
            Double.Immediate = ARM_FLOATING_POINT_BUILD_IMMEDIATE64(Immediate8);
            sprintf(Context->Operand2,
                    "#%d  ; 0x%llx %g",
                    Immediate8,
                    Double.Immediate,
                    Double.Double);

        } else {
            Float.Immediate= ARM_FLOATING_POINT_BUILD_IMMEDIATE32(Immediate8);
            sprintf(Context->Operand2,
                    "#%d  ; 0x%x %g",
                    Immediate8,
                    Float.Immediate,
                    Float.Float);
        }

        TwoRegisters = FALSE;

    } else {
        Mask = Instruction & ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_MASK;
        switch (Mask) {
        case ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VMOV:
            BaseMnemonic = ARM_VMOV_MNEMONIC;
            break;

        case ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VABS:
            BaseMnemonic = ARM_VABS_MNEMONIC;
            break;

        case ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VNEG:
            BaseMnemonic = ARM_VNEG_MNEMONIC;
            break;

        case ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VSQRT:
            BaseMnemonic = ARM_VSQRT_MNEMONIC;
            break;

        default:
            return;
        }

        TwoRegisters = TRUE;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->PostConditionMnemonicSuffix, MnemonicSuffix);
    sprintf(Context->Operand1, "%s%d", VectorTypeString, VectorD);
    if (TwoRegisters != FALSE) {
        sprintf(Context->Operand2, "%s%d", VectorTypeString, VectorM);
    }

    return;
}

VOID
DbgpArmDecodeFloatingPointThreeRegisters (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a three-register floating point data processing
    instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG VectorD;
    ULONG VectorM;
    ULONG VectorN;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;

    //
    // Collect the vector values. If the double-precision (SZ) bit is set, then
    // the extra bit for each vector is the high bit. If the double-precision
    // bit is not set, then the extra bit is the low bit.
    //

    VectorD = (Instruction & ARM_FLOATING_POINT_VD_MASK) >>
              ARM_FLOATING_POINT_VD_SHIFT;

    VectorM = (Instruction & ARM_FLOATING_POINT_VM_MASK) >>
              ARM_FLOATING_POINT_VM_SHIFT;

    VectorN = (Instruction & ARM_FLOATING_POINT_VN_MASK) >>
              ARM_FLOATING_POINT_VN_SHIFT;

    if ((Instruction & ARM_FLOATING_POINT_SZ_BIT) != 0) {
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= (1 << 4);
        }

        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= (1 << 4);
        }

        if ((Instruction & ARM_FLOATING_POINT_N_BIT) != 0) {
            VectorN |= (1 << 4);
        }

        MnemonicSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;

    } else {
        VectorD <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= 1;
        }

        VectorM <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= 1;
        }

        VectorN <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_N_BIT) != 0) {
            VectorN |= 1;
        }

        MnemonicSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
    }

    //
    // Get the instruction based on the value of opcode 1 and the op bit.
    //

    BaseMnemonic = "ERR";
    switch (Instruction & ARM_FLOATING_POINT_INSTRUCTION_MASK) {
    case ARM_FLOATING_POINT_INSTRUCTION_VMLA_VMLS:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VMLS_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VMLA_MNEMONIC;
        }

        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VNMLA_VNMLS:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VNMLS_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VNMLA_MNEMONIC;
        }

        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VMUL_VNMUL:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VNMUL_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VMUL_MNEMONIC;
        }

        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VADD_VSUB:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VSUB_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VADD_MNEMONIC;
        }

        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VDIV:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            return;
        }

        BaseMnemonic = ARM_VDIV_MNEMONIC;
        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VFNMA_VFNMS:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VFNMA_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VFNMS_MNEMONIC;
        }

        break;

    case ARM_FLOATING_POINT_INSTRUCTION_VFMA_VFMS:
        if ((Instruction & ARM_FLOATING_POINT_OP_BIT) != 0) {
            BaseMnemonic = ARM_VFMS_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VFMA_MNEMONIC;
        }

        break;

    default:
        break;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->PostConditionMnemonicSuffix, MnemonicSuffix);
    sprintf(Context->Operand1, "%s%d", VectorTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorTypeString, VectorN);
    sprintf(Context->Operand3, "%s%d", VectorTypeString, VectorM);
    return;
}

VOID
DbgpArmDecodeFloatingPointVectorConvert (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a floating point vector convert instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR DestinationHalfSuffix;
    PSTR DestinationSuffix;
    PSTR FixedSuffix;
    ULONG FractionBits;
    ULONG Instruction;
    PSTR PreConditionMnemonicSuffix;
    BOOL RepeatVectorD;
    PSTR SourceHalfSuffix;
    PSTR SourceSuffix;
    ULONG VectorD;
    BOOL VectorDDouble;
    PSTR VectorDTypeString;
    ULONG VectorM;
    BOOL VectorMDouble;
    PSTR VectorMTypeString;

    //
    // Save somem values that are common to most instructions.
    //

    Instruction = Context->Instruction;
    RepeatVectorD = FALSE;
    VectorDDouble = FALSE;
    VectorD = (Instruction & ARM_FLOATING_POINT_VD_MASK) >>
              ARM_FLOATING_POINT_VD_SHIFT;

    VectorMDouble = FALSE;
    VectorM = (Instruction & ARM_FLOATING_POINT_VM_MASK) >>
              ARM_FLOATING_POINT_VM_SHIFT;

    DestinationHalfSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
    SourceHalfSuffix = ARM_FLOATING_POINT_HALF_PRECISION_SUFFIX;
    DestinationSuffix = "";
    PreConditionMnemonicSuffix = "";
    SourceSuffix = "";

    //
    // Compute the fraction bits and suffix for the fixed point instructions.
    // The fixed 32-bit and unsigned bit are the same for float to fixed as
    // they are for fixed to float.
    //

    FractionBits = (Instruction & ARM_FLOATING_POINT_IMMEDIATE4_LOW_MASK) >>
                   ARM_FLOATING_POINT_IMMEDIATE4_LOW_SHIFT;

    FractionBits <<= 1;
    if ((Instruction & ARM_FLOATING_POINT_I_BIT) != 0) {
        FractionBits |= 1;
    }

    if ((Instruction & ARM_VCVT_FIXED_32_TO_FLOAT) != 0) {
        if ((Instruction & ARM_VCVT_FIXED_UNSIGNED_TO_FLOAT) != 0) {
            FixedSuffix = ARM_FLOATING_POINT_UNSIGNED_INTEGER_SUFFIX;

        } else {
            FixedSuffix = ARM_FLOATING_POINT_SIGNED_INTEGER_SUFFIX;
        }

        FractionBits = 32 - FractionBits;

    } else {
        if ((Instruction & ARM_VCVT_FIXED_UNSIGNED_TO_FLOAT) != 0) {
            FixedSuffix = ARM_FLOATING_POINT_UNSIGNED_HALF_SUFFIX;

        } else {
            FixedSuffix = ARM_FLOATING_POINT_SIGNED_HALF_SUFFIX;
        }

        FractionBits = 16 - FractionBits;
    }

    //
    // Determine the suffices and vector sizes baced on the instruction mask.
    //

    switch (Instruction & ARM_VCVT_MASK) {

    //
    // Handle VCVTT single to half.
    //

    case ARM_VCVT_TOP | ARM_VCVT_SINGLE_TO_HALF:
        DestinationHalfSuffix = ARM_FLOATING_POINT_HALF_PRECISION_SUFFIX;
        SourceHalfSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;

        //
        // Fall through.
        //

    //
    // Handle VCVTT half to single.
    //

    case ARM_VCVT_TOP | ARM_VCVT_HALF_TO_SINGLE:
        PreConditionMnemonicSuffix = ARM_FLOATING_POINT_TOP;
        DestinationSuffix = DestinationHalfSuffix;
        SourceSuffix = SourceHalfSuffix;
        break;

    //
    // Handle VCVTB single to half.
    //

    case ARM_VCVT_BOTTOM | ARM_VCVT_SINGLE_TO_HALF:
        DestinationHalfSuffix = ARM_FLOATING_POINT_HALF_PRECISION_SUFFIX;
        SourceHalfSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;

        //
        // Fall through.
        //

    //
    // Handle VCVTB half to single.
    //

    case ARM_VCVT_BOTTOM | ARM_VCVT_HALF_TO_SINGLE:
        PreConditionMnemonicSuffix = ARM_FLOATING_POINT_BOTTOM;
        DestinationSuffix = DestinationHalfSuffix;
        SourceSuffix = SourceHalfSuffix;
        break;

    //
    // Handle VCVT single-precision to double-precision conversions and
    // double-precision to single-precision conversion.
    //

    case ARM_VCVT_FLOAT_TO_FLOAT:

        //
        // Here the double bit indicates that the conversion is from a double.
        //

        if ((Instruction & ARM_VCVT_DOUBLE) != 0) {
            VectorMDouble = TRUE;
            SourceSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;
            DestinationSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;

        } else {
            VectorDDouble = TRUE;
            SourceSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
            DestinationSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;
        }

        break;

    //
    // Handle conversions from floats to integers.
    //

    case ARM_VCVT_FLOAT_TO_INTEGER:
    case ARM_VCVT_FLOAT_TO_INTEGER | ARM_VCVT_FLOAT_TO_INTEGER_SIGNED:
    case ARM_VCVT_FLOAT_TO_INTEGER | ARM_VCVT_FLOAT_TO_INTEGER_ROUND_TO_ZERO:
    case (ARM_VCVT_FLOAT_TO_INTEGER |
          ARM_VCVT_FLOAT_TO_INTEGER_SIGNED |
          ARM_VCVT_FLOAT_TO_INTEGER_ROUND_TO_ZERO):

        if ((Instruction & ARM_VCVT_DOUBLE) != 0) {
            VectorMDouble = TRUE;
            SourceSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;

        } else {
            SourceSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        }

        if ((Instruction & ARM_VCVT_FLOAT_TO_INTEGER_SIGNED) != 0) {
            DestinationSuffix = ARM_FLOATING_POINT_SIGNED_INTEGER_SUFFIX;

        } else {
            DestinationSuffix = ARM_FLOATING_POINT_UNSIGNED_INTEGER_SUFFIX;
        }

        if ((Instruction & ARM_VCVT_FLOAT_TO_INTEGER_ROUND_TO_ZERO) == 0) {
            PreConditionMnemonicSuffix = ARM_FLOATING_POINT_ROUNDING;
        }

        break;

    //
    // Handle conversions from integers to floats.
    //

    case ARM_VCVT_INTEGER_TO_FLOAT:
    case ARM_VCVT_INTEGER_TO_FLOAT | ARM_VCVT_INTEGER_TO_FLOAT_SIGNED:
        if ((Instruction & ARM_VCVT_DOUBLE) != 0) {
            VectorDDouble = TRUE;
            DestinationSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;

        } else {
            DestinationSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        }

        if ((Instruction & ARM_VCVT_INTEGER_TO_FLOAT_SIGNED) != 0) {
            SourceSuffix = ARM_FLOATING_POINT_SIGNED_INTEGER_SUFFIX;

        } else {
            SourceSuffix = ARM_FLOATING_POINT_UNSIGNED_INTEGER_SUFFIX;
        }

        break;

    //
    // Handle conversions from floats to fixed point.
    //

    case ARM_VCVT_FLOAT_TO_FIXED:
    case ARM_VCVT_FLOAT_TO_FIXED | ARM_VCVT_FLOAT_TO_FIXED_UNSIGNED:
    case ARM_VCVT_FLOAT_TO_FIXED | ARM_VCVT_FLOAT_TO_FIXED_32:
    case (ARM_VCVT_FLOAT_TO_FIXED |
          ARM_VCVT_FLOAT_TO_FIXED_UNSIGNED |
          ARM_VCVT_FLOAT_TO_FIXED_32):

        if ((Instruction & ARM_VCVT_DOUBLE) != 0) {
            VectorDDouble = TRUE;
            SourceSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;

        } else {
            SourceSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        }

        DestinationSuffix = FixedSuffix;
        RepeatVectorD = TRUE;
        break;

    //
    // Handle conversions from fixed point to floats.
    //

    case ARM_VCVT_FIXED_TO_FLOAT:
    case ARM_VCVT_FIXED_TO_FLOAT | ARM_VCVT_FIXED_UNSIGNED_TO_FLOAT:
    case ARM_VCVT_FIXED_TO_FLOAT | ARM_VCVT_FIXED_32_TO_FLOAT:
    case (ARM_VCVT_FIXED_TO_FLOAT |
          ARM_VCVT_FIXED_UNSIGNED_TO_FLOAT |
          ARM_VCVT_FIXED_32_TO_FLOAT):

        if ((Instruction & ARM_VCVT_DOUBLE) != 0) {
            VectorDDouble = TRUE;
            DestinationSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;

        } else {
            DestinationSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        }

        SourceSuffix = FixedSuffix;
        RepeatVectorD = TRUE;
        break;

    default:
        break;
    }

    //
    // Convert the vectors into the correct double-precision or
    // single-precision values.
    //

    if (VectorDDouble != FALSE) {
        VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= (1 << 4);
        }

    } else {
        VectorDTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
        VectorD <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= 1;
        }
    }

    if (VectorMDouble != FALSE) {
        VectorMTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= (1 << 4);
        }

    } else {
        VectorMTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
        VectorM <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= 1;
        }
    }

    sprintf(Context->Mnemonic,
            "%s%s",
            ARM_VCVT_MNEMONIC,
            PreConditionMnemonicSuffix);

    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            DestinationSuffix,
            SourceSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    if (RepeatVectorD != FALSE) {
        sprintf(Context->Operand2, "%s%d", VectorDTypeString, VectorD);

    } else {
        sprintf(Context->Operand2, "%s%d", VectorMTypeString, VectorM);
    }

    return;
}

VOID
DbgpArmDecodeFloatingPointVectorCompare (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a floating point vector compare instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG VectorD;
    ULONG VectorM;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;

    //
    // Collect the vector values. If the double-precision (SZ) bit is set, then
    // the extra bit for each vector is the high bit. If the double-precision
    // bit is not set, then the extra bit is the low bit.
    //

    VectorD = (Instruction & ARM_FLOATING_POINT_VD_MASK) >>
              ARM_FLOATING_POINT_VD_SHIFT;

    VectorM = (Instruction & ARM_FLOATING_POINT_VM_MASK) >>
              ARM_FLOATING_POINT_VM_SHIFT;

    if ((Instruction & ARM_FLOATING_POINT_SZ_BIT) != 0) {
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= (1 << 4);
        }

        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= (1 << 4);
        }

        MnemonicSuffix = ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;

    } else {
        VectorD <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_D_BIT) != 0) {
            VectorD |= 1;
        }

        VectorM <<= 1;
        if ((Instruction & ARM_FLOATING_POINT_M_BIT) != 0) {
            VectorM |= 1;
        }

        MnemonicSuffix = ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX;
        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
    }

    //
    // Get the base mnemonic and fill out the context.
    //

    if ((Instruction & ARM_FLOATING_POINT_VCMP_E_BIT) != 0) {
        BaseMnemonic = ARM_VCMPE_MNEMONIC;

    } else {
        BaseMnemonic = ARM_VCMP_MNEMONIC;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->PostConditionMnemonicSuffix, MnemonicSuffix);
    sprintf(Context->Operand1, "%s%d", VectorTypeString, VectorD);
    if ((Instruction & ARM_FLOATING_POINT_VCMP_ZERO) != 0) {
        sprintf(Context->Operand2, "#0.0");

    } else {
        sprintf(Context->Operand2, "%s%d", VectorTypeString, VectorM);
    }

    return;
}

VOID
DbgpArmDecodeSimdSmallMove (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a floating point to ARM register move instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Register;
    PSTR RegisterString;
    ULONG Size;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG Vector;
    BOOL VectorDouble;
    ULONG VectorIndex;
    PSTR VectorString;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;
    Register = (Instruction & ARM_SIMD_TRANSFER_REGISTER_MASK) >>
               ARM_SIMD_TRANSFER_REGISTER_SHIFT;

    VectorDouble = FALSE;
    Vector = (Instruction & ARM_SIMD_TRANSFER_VECTOR_MASK) >>
             ARM_SIMD_TRANSFER_VECTOR_SHIFT;

    //
    // Determine the mnemonic suffices and vector index for the to/from scalar
    // instructions.
    //

    SizeTypeSuffix = "";
    SizeValueSuffix = "";
    VectorIndex = 0;
    if ((Instruction & ARM_SIMD_TRANSFER_MOVE_SCALAR) != 0) {
        VectorDouble = TRUE;
        Size = ARM_SIMD_TRANSFER_SCALAR_BUILD_SIZE_ENCODING(Instruction);
        if ((Size & ARM_SIMD_TRANSFER_SCALAR_SIZE_8_MASK) ==
            ARM_SIMD_TRANSFER_SCALAR_SIZE_8_VALUE) {

            VectorIndex = (Size & ~ARM_SIMD_TRANSFER_SCALAR_SIZE_8_MASK) >>
                          ARM_SIMD_TRANSFER_SCALAR_SIZE_8_SHIFT;

            SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;

        } else if ((Size & ARM_SIMD_TRANSFER_SCALAR_SIZE_16_MASK) ==
                   ARM_SIMD_TRANSFER_SCALAR_SIZE_16_VALUE) {

            VectorIndex = (Size & ~ARM_SIMD_TRANSFER_SCALAR_SIZE_16_MASK) >>
                          ARM_SIMD_TRANSFER_SCALAR_SIZE_16_SHIFT;

            SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;

        } else if ((Size & ARM_SIMD_TRANSFER_SCALAR_SIZE_32_MASK) ==
                   ARM_SIMD_TRANSFER_SCALAR_SIZE_32_VALUE) {

            VectorIndex = (Size & ~ARM_SIMD_TRANSFER_SCALAR_SIZE_32_MASK) >>
                          ARM_SIMD_TRANSFER_SCALAR_SIZE_32_SHIFT;

            if (((Instruction & ARM_SIMD_TRANSFER_TO_REGISTER) != 0) &&
                ((Instruction & ARM_SIMD_TRANSFER_SCALAR_UNSIGNED) != 0)) {

                DbgpArmDecodeUndefined(Context);
                return;
            }

            SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;

        } else {
            DbgpArmDecodeUndefined(Context);
            return;
        }

        if ((Instruction & ARM_SIMD_TRANSFER_TO_REGISTER) != 0) {
            if ((Instruction & ARM_SIMD_TRANSFER_SCALAR_UNSIGNED) != 0) {
                SizeTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                SizeTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

        } else {
            SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;
        }
    }

    //
    // Finalize the vector and get its type string.
    //

    if (VectorDouble != FALSE) {
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        if ((Instruction & ARM_SIMD_TRANSFER_VECTOR_BIT) != 0) {
            Vector |= (1 << 4);
        }

    } else {
        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
        Vector <<= 1;
        if ((Instruction & ARM_SIMD_TRANSFER_VECTOR_BIT) != 0) {
            Vector |= 1;
        }
    }

    if ((Instruction & ARM_SIMD_TRANSFER_TO_REGISTER) != 0) {
        RegisterString = Context->Operand1;
        VectorString = Context->Operand2;

    } else {
        VectorString = Context->Operand1;
        RegisterString = Context->Operand2;
    }

    strcpy(Context->Mnemonic, ARM_VMOV_MNEMONIC);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    strcpy(RegisterString, DbgArmRegisterNames[Register]);
    if ((Instruction & ARM_SIMD_TRANSFER_MOVE_SCALAR) != 0) {
        sprintf(VectorString,
                "%s%d[%d]",
                VectorTypeString,
                Vector,
                VectorIndex);

    } else {
        sprintf(VectorString, "%s%d", VectorTypeString, Vector);
    }

    return;
}

VOID
DbgpArmDecodeSimdSpecialMove (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an ARM register to special register move instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Register;
    PSTR RegisterName;
    ULONG SpecialRegister;

    Instruction = Context->Instruction;
    Register = (Instruction & ARM_SIMD_TRANSFER_REGISTER_MASK) >>
               ARM_SIMD_TRANSFER_REGISTER_SHIFT;

    SpecialRegister = (Instruction & ARM_SIMD_TRANSFER_SPECIAL_MASK) >>
                      ARM_SIMD_TRANSFER_SPECIAL_SHIFT;

    if ((Instruction & ARM_SIMD_TRANSFER_TO_REGISTER) != 0) {
        BaseMnemonic = ARM_VMRS_MNEMONIC;
        if ((Register == 0xF) && (SpecialRegister == 1)) {
            RegisterName = ARM_SIMD_APSR_REGISTER;

        } else {
            RegisterName = DbgArmRegisterNames[Register];
        }

        strcpy(Context->Operand1, RegisterName);
        strcpy(Context->Operand2, DbgArmSpecialRegisterNames[SpecialRegister]);

    } else {
        BaseMnemonic = ARM_VMSR_MNEMONIC;
        strcpy(Context->Operand1, DbgArmSpecialRegisterNames[SpecialRegister]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Register]);
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    return;
}

VOID
DbgpArmDecodeSimdDuplicate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an ARM register to floating point duplicate
    instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Register;
    ULONG Size;
    ULONG Vector;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;
    Register = (Instruction & ARM_SIMD_TRANSFER_REGISTER_MASK) >>
               ARM_SIMD_TRANSFER_REGISTER_SHIFT;

    Vector = (Instruction & ARM_SIMD_TRANSFER_VECTOR_MASK) >>
             ARM_SIMD_TRANSFER_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_TRANSFER_VECTOR_BIT) != 0) {
        Vector |= (1 << 4);
    }

    //
    // Determine the size of the transfers.
    //

    Size = ARM_SIMD_TRANSFER_DUP_BUILD_SIZE_ENCODING(Instruction);
    switch (Size) {
    case ARM_SIMD_TRANSFER_DUP_SIZE_8:
        MnemonicSuffix = ARM_SIMD_DATA_SIZE_8;
        break;

    case ARM_SIMD_TRANSFER_DUP_SIZE_16:
        MnemonicSuffix = ARM_SIMD_DATA_SIZE_16;
        break;

    case ARM_SIMD_TRANSFER_DUP_SIZE_32:
        MnemonicSuffix = ARM_SIMD_DATA_SIZE_32;
        break;

    default:
        DbgpArmDecodeUndefined(Context);
        return;
    }

    //
    // Get the vector type.
    //

    if ((Instruction & ARM_SIMD_TRANSFER_DUP_QUADWORD) != 0) {
        VectorTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;

    } else {
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    }

    strcpy(Context->Mnemonic, ARM_VDUP_MNEMONIC);
    strcpy(Context->PostConditionMnemonicSuffix, MnemonicSuffix);
    sprintf(Context->Operand1, "%s%d", VectorTypeString, Vector);
    strcpy(Context->Operand2, DbgArmRegisterNames[Register]);
    return;
}

VOID
DbgpArmDecodeSimdLoadStoreRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD and floating point register load/store
    instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Offset;
    ULONG Register;
    CHAR Sign;
    ULONG Vector;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;
    Register = (Instruction & ARM_SIMD_LOAD_STORE_REGISTER_MASK) >>
               ARM_SIMD_LOAD_STORE_REGISTER_SHIFT;

    if ((Instruction & ARM_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_VLD_MNEMONIC;

    } else {
        BaseMnemonic = ARM_VST_MNEMONIC;
    }

    //
    // Get the correct vector value based on whether it is single or double
    // precision.
    //

    Vector = (Instruction & ARM_SIMD_LOAD_STORE_VECTOR_MASK) >>
             ARM_SIMD_LOAD_STORE_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_LOAD_STORE_DOUBLE) != 0) {
        if ((Instruction & ARM_SIMD_LOAD_STORE_VECTOR_BIT) != 0) {
            Vector |= (1 << 4);
        }

        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;

    } else {
        Vector <<= 1;
        if ((Instruction & ARM_SIMD_LOAD_STORE_VECTOR_BIT) != 0) {
            Vector |= 1;
        }

        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
    }

    //
    // Get the immediate offset and its sign.
    //

    Offset = (Instruction & ARM_SIMD_LOAD_STORE_IMMEDIATE8_MASK) >>
             ARM_SIMD_LOAD_STORE_IMMEDIATE8_SHIFT;

    Offset <<= 2;
    if ((Instruction & ARM_SIMD_LOAD_STORE_ADD_BIT) != 0) {
        Sign = '+';

    } else {
        Sign = '-';
    }

    sprintf(Context->Mnemonic,
            "%s%s",
            BaseMnemonic,
            ARM_FLOATING_POINT_REGISTER);

    sprintf(Context->Operand1, "%s%d", VectorTypeString, Vector);
    if (Offset == 0) {
        sprintf(Context->Operand2, "[%s]", DbgArmRegisterNames[Register]);

    } else {
        sprintf(Context->Operand2,
                "[%s, #%c%d]",
                DbgArmRegisterNames[Register],
                Sign,
                Offset);
    }

    return;
}

VOID
DbgpArmDecodeSimdLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD and floating point multiple register
    load/store instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    ULONG Operation;
    PSTR PreConditionMnemonicSuffix;
    BOOL PushPop;
    ULONG Register;
    ULONG Vector;
    ULONG VectorCount;
    PSTR VectorListString;
    ULONG VectorListStringSize;
    PSTR VectorTypeString;
    PSTR WriteBack;

    Instruction = Context->Instruction;
    Operation = Instruction & ARM_SIMD_LOAD_STORE_OP_MASK;
    Register = (Instruction & ARM_SIMD_LOAD_STORE_REGISTER_MASK) >>
               ARM_SIMD_LOAD_STORE_REGISTER_SHIFT;

    //
    // Determine if this is a load, store, push or pop.
    //

    PushPop = FALSE;
    if ((Register == ARM_STACK_REGISTER) &&
        ((Operation == ARM_SIMD_LOAD_STORE_OP_VPOP) ||
         (Operation == ARM_SIMD_LOAD_STORE_OP_VPUSH))) {

        if ((Instruction & ARM_LOAD_BIT) != 0) {
            BaseMnemonic = ARM_VPOP_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VPUSH_MNEMONIC;
        }

        PreConditionMnemonicSuffix = "";
        MnemonicSuffix = "";
        PushPop = TRUE;

    } else {
        if ((Instruction & ARM_LOAD_BIT) != 0) {
            BaseMnemonic = ARM_VLD_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VST_MNEMONIC;
        }

        PreConditionMnemonicSuffix = ARM_FLOATING_POINT_MULTIPLE;
        MnemonicSuffix = DbgpArmGetLoadStoreTypeString(Instruction);
    }

    sprintf(Context->Mnemonic,
            "%s%s%s",
            BaseMnemonic,
            PreConditionMnemonicSuffix,
            MnemonicSuffix);

    //
    // Gather the starting vector and the vector count.
    //

    VectorCount = (Instruction & ARM_SIMD_LOAD_STORE_IMMEDIATE8_MASK) >>
                  ARM_SIMD_LOAD_STORE_IMMEDIATE8_SHIFT;

    Vector = (Instruction & ARM_SIMD_LOAD_STORE_VECTOR_MASK) >>
             ARM_SIMD_LOAD_STORE_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_LOAD_STORE_DOUBLE) != 0) {
        if ((Instruction & ARM_SIMD_LOAD_STORE_VECTOR_BIT) != 0) {
            Vector |= (1 << 4);
        }

        VectorCount >>= 1;
        VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;

    } else {
        Vector <<= 1;
        if ((Instruction & ARM_SIMD_LOAD_STORE_VECTOR_BIT) != 0) {
            Vector |= 1;
        }

        VectorTypeString = ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR;
    }

    //
    // Write the register (the first operand) nad add the ! if the operation
    // does a write back. Push/pop operations are always write back.
    //

    if (PushPop == FALSE) {
        WriteBack = "";
        if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
            WriteBack = "!";
        }

        sprintf(Context->Operand1,
                "%s%s",
                DbgArmRegisterNames[Register],
                WriteBack);

        VectorListString = Context->Operand2;
        VectorListStringSize = sizeof(Context->Operand2);

    } else {
        VectorListString = Context->Operand1;
        VectorListStringSize = sizeof(Context->Operand1);
    }

    //
    // Now print the vector list.
    //

    DbgpArmPrintVectorList(VectorListString,
                           VectorListStringSize,
                           Vector,
                           VectorCount,
                           1,
                           VectorTypeString,
                           0,
                           0);

    return;
}

VOID
DbgpArmDecodeSimdElementLoadAllLanes (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD element load to all lanes instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR AlignString;
    PSTR ElementSuffix;
    ULONG Instruction;
    ULONG Rm;
    ULONG Rn;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG Vector;
    ULONG VectorCount;
    ULONG VectorIncrement;
    PSTR WriteBack;

    Instruction = Context->Instruction;
    Rm = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RM_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RM_SHIFT;

    Rn = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RN_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RN_SHIFT;

    Vector = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_MASK) >>
             ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_BIT) != 0) {
        Vector |= (1 << 4);
    }

    //
    // Determine the number of elements in the structure being loaded and the
    // number and spacing of the vectors. Also collect the alignment string,
    // which depends on the size and the element count.
    //

    VectorIncrement = 1;
    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_TWO_BIT) != 0) {
        VectorIncrement = 2;
    }

    AlignString = "";
    ElementSuffix = "";
    VectorCount = 0;
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_1:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX;
        VectorCount = 1;
        VectorIncrement = 1;
        if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_TWO_BIT) != 0) {
            VectorCount = 2;
        }

        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_16:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_16;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_32:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        default:
            break;
        }

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_2:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX;
        VectorCount = 2;
        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_8:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_16;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_16:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_32:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_64;
            }

            break;

        default:
            break;
        }

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_3:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_3_ELEMENT_SUFFIX;
        VectorCount = 3;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_4:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_4_ELEMENT_SUFFIX;
        VectorCount = 4;
        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_8:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_16:
        case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_32:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_64;
            }

            break;

        default:
            if ((Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN) != 0) {
                AlignString = ARM_SIMD_ALIGN_128;
            }

            break;
        }

        break;

    //
    // This should never hit as all values are accounted for above.
    //

    default:
        break;
    }

    //
    // Get the size suffix.
    //

    SizeValueSuffix = "";
    SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_8:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
        break;

    case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_16:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        break;

    case ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_32:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        break;

    default:
        SizeTypeSuffix = "";
        break;
    }

    sprintf(Context->Mnemonic, "%s%s", ARM_VLD_MNEMONIC, ElementSuffix);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    //
    // Assemble the vector list.
    //

    DbgpArmPrintVectorList(Context->Operand1,
                           sizeof(Context->Operand1),
                           Vector,
                           VectorCount,
                           VectorIncrement,
                           ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
                           0,
                           DBG_ARM_VECTOR_LIST_FLAG_INDEX |
                           DBG_ARM_VECTOR_LIST_FLAG_ALL_LANES);

    //
    // Assemble the register operands.
    //

    WriteBack = "";
    if (Rm == ARM_STACK_REGISTER) {
        WriteBack = "!";
    }

    sprintf(Context->Operand2,
            "[%s%s]%s",
            DbgArmRegisterNames[Rn],
            AlignString,
            WriteBack);

    if ((Rm != ARM_STACK_REGISTER) && (Rm != ARM_PC_REGISTER)) {
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    }

    return;
}

VOID
DbgpArmDecodeSimdElementLoadStoreSingle (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD element load/store from/to a single structure.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR AlignString;
    ULONG AlignValue;
    PSTR BaseMnemonic;
    PSTR ElementSuffix;
    ULONG Instruction;
    ULONG Rm;
    ULONG Rn;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG Vector;
    ULONG VectorCount;
    ULONG VectorIncrement;
    ULONG VectorIndex;
    PSTR WriteBack;

    AlignString = "";
    ElementSuffix = "";
    SizeValueSuffix = "";
    SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;

    //
    // The base mnemonic is either vector load or vector store.
    //

    Instruction = Context->Instruction;
    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_VLD_MNEMONIC;

    } else {
        BaseMnemonic = ARM_VST_MNEMONIC;
    }

    Rm = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RM_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RM_SHIFT;

    Rn = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RN_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RN_SHIFT;

    Vector = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_MASK) >>
             ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_BIT) != 0) {
        Vector |= (1 << 4);
    }

    //
    // Get the size suffix, vector index, vector increment, and alignment value.
    //

    AlignValue = 0;
    VectorIndex = 0;
    VectorIncrement = 1;
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
        VectorIndex = (Instruction &
                       ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_INDEX_MASK) >>
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_INDEX_SHIFT;

        AlignValue = (Instruction &
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_ALIGN_MASK) >>
                     ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_ALIGN_SHIFT;

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        VectorIndex = (Instruction &
                       ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INDEX_MASK) >>
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INDEX_SHIFT;

        if ((Instruction &
             ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INCREMENT) != 0) {

            VectorIncrement = 2;
        }

        AlignValue = (Instruction &
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_ALIGN_MASK) >>
                     ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_ALIGN_SHIFT;

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        VectorIndex = (Instruction &
                       ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INDEX_MASK) >>
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INDEX_SHIFT;

        if ((Instruction &
             ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INCREMENT) != 0) {

            VectorIncrement = 2;
        }

        AlignValue = (Instruction &
                      ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_MASK) >>
                     ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_SHIFT;

        break;

    default:
        SizeTypeSuffix = "";
        break;
    }

    //
    // Determine the number of elements being loaded/stored and the alignment
    // string.
    //

    VectorCount = 0;
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_1:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX;
        VectorCount = 1;
        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_16;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        default:
            break;
        }

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_2:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX;
        VectorCount = 2;
        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_16;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_64;
            }

            break;

        default:
            break;
        }

        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_3:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_3_ELEMENT_SUFFIX;
        VectorCount = 3;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_4:
        ElementSuffix = ARM_SIMD_ELEMENT_LOAD_STORE_4_ELEMENT_SUFFIX;
        VectorCount = 4;
        switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_MASK) {
        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_32;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16:
            if (AlignValue != 0) {
                AlignString = ARM_SIMD_ALIGN_64;
            }

            break;

        case ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32:
            if (AlignValue ==
                ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_64) {

                AlignString = ARM_SIMD_ALIGN_64;

            } else if (AlignValue ==
                       ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_128) {

                AlignString = ARM_SIMD_ALIGN_128;
            }

            break;

        default:
            break;
        }

        break;

    //
    // This should never hit as all values are accounted for above.
    //

    default:
        break;
    }

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, ElementSuffix);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    //
    // Assemble the vector list.
    //

    DbgpArmPrintVectorList(Context->Operand1,
                           sizeof(Context->Operand1),
                           Vector,
                           VectorCount,
                           VectorIncrement,
                           ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
                           VectorIndex,
                           DBG_ARM_VECTOR_LIST_FLAG_INDEX);

    //
    // Assemble the register operands.
    //

    WriteBack = "";
    if (Rm == ARM_STACK_REGISTER) {
        WriteBack = "!";
    }

    sprintf(Context->Operand2,
            "[%s%s]%s",
            DbgArmRegisterNames[Rn],
            AlignString,
            WriteBack);

    if ((Rm != ARM_STACK_REGISTER) && (Rm != ARM_PC_REGISTER)) {
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    }

    return;
}

VOID
DbgpArmDecodeSimdElementLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD element load/store from/to multiple structures.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR AlignString;
    PSTR BaseMnemonic;
    PSTR ElementSuffix;
    ULONG Instruction;
    ULONG Rm;
    ULONG Rn;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG Type;
    ULONG Vector;
    ULONG VectorCount;
    ULONG VectorIncrement;
    PSTR WriteBack;

    //
    // The base mnemonic is either vector load or vector store.
    //

    Instruction = Context->Instruction;
    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_VLD_MNEMONIC;

    } else {
        BaseMnemonic = ARM_VST_MNEMONIC;
    }

    Rm = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RM_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RM_SHIFT;

    Rn = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_RN_MASK) >>
         ARM_SIMD_ELEMENT_LOAD_STORE_RN_SHIFT;

    Vector = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_MASK) >>
             ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_SHIFT;

    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_BIT) != 0) {
        Vector |= (1 << 4);
    }

    //
    // Get the size suffix.
    //

    SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;
    SizeValueSuffix = "";
    VectorIncrement = 1;
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_8:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_16:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_32:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_64:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_64;
        break;

    default:
        break;
    }

    //
    // Get the alignment string.
    //

    AlignString = "";
    switch (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_MASK) {
    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_64:
        AlignString = ARM_SIMD_ALIGN_64;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_128:
        AlignString = ARM_SIMD_ALIGN_128;
        break;

    case ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_256:
        AlignString = ARM_SIMD_ALIGN_256;
        break;

    default:
        break;
    }

    //
    // Determine the number of elements being loaded/stored, the vector count,
    // vector increment based on the type field.
    //

    VectorIncrement = 1;
    if ((Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_INCREMENT) != 0) {
        VectorIncrement = 2;
    }

    Type = (Instruction & ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_TYPE_MASK) >>
           ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_TYPE_SHIFT;

    ElementSuffix = DbgArmSimdElementLoadStoreMultipleElementSuffix[Type];
    VectorCount = DbgArmSimdElementLoadStoreMultipleVectorCount[Type];
    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, ElementSuffix);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    //
    // Assemble the vector list.
    //

    DbgpArmPrintVectorList(Context->Operand1,
                           sizeof(Context->Operand1),
                           Vector,
                           VectorCount,
                           VectorIncrement,
                           ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
                           0,
                           0);

    //
    // Assemble the register operands.
    //

    WriteBack = "";
    if (Rm == ARM_STACK_REGISTER) {
        WriteBack = "!";
    }

    sprintf(Context->Operand2,
            "[%s%s]%s",
            DbgArmRegisterNames[Rn],
            AlignString,
            WriteBack);

    if ((Rm != ARM_STACK_REGISTER) && (Rm != ARM_PC_REGISTER)) {
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    }

    return;
}

VOID
DbgpArmDecodeSimdThreeRegistersSameLength (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data processing instructions with three
    registers of the same length.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    BOOL FloatSize;
    ULONG Instruction;
    BOOL IntegerSize;
    BOOL NoSizeSuffix;
    BOOL PolynomialSize;
    BOOL SignedSize;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    BOOL TwoVectors;
    ULONG VectorD;
    PSTR VectorDString;
    ULONG VectorM;
    PSTR VectorMString;
    ULONG VectorN;
    PSTR VectorNString;
    PSTR VectorTypeString;

    FloatSize = FALSE;
    IntegerSize = FALSE;
    NoSizeSuffix = FALSE;
    PolynomialSize = FALSE;
    SignedSize = TRUE;
    TwoVectors = FALSE;
    Instruction = Context->Instruction;
    VectorDString = Context->Operand1;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorMString = Context->Operand3;
    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    VectorNString = Context->Operand2;
    VectorN = (Instruction & ARM_SIMD_DATA_PROCESSING_VN_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VN_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VN_BIT) != 0) {
        VectorN |= (1 << 4);
    }

    VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
        VectorTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    }

    //
    // Determine the base mnemonic. Some instructions ignore the size encodings
    // and others have integer or float encodings rather than the default
    // signed/unsigned. Take note so that the correct size suffix can be
    // calculated.
    //

    BaseMnemonic = NULL;
    switch (Instruction & ARM_SIMD_DATA_PROCESSING_3_SAME_OPERATION_MASK) {
    case ARM_SIMD_VHADD_MASK:
        BaseMnemonic = ARM_VHADD_MNEMONIC;
        break;

    case ARM_SIMD_VQADD_MASK:
        BaseMnemonic = ARM_VQADD_MNEMONIC;
        break;

    case ARM_SIMD_VRHADD_MASK:
        BaseMnemonic = ARM_VRHADD_MNEMONIC;
        break;

    case ARM_SIMD_BITWISE_MASK:
        switch (Instruction & ARM_SIMD_BITWISE_OP_MASK) {
        case ARM_SIMD_BITWISE_VAND_VALUE:
            BaseMnemonic = ARM_VAND_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VBIC_VALUE:
            BaseMnemonic = ARM_VBIC_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VORR_VALUE:
            if (VectorM == VectorN) {
                BaseMnemonic = ARM_VMOV_MNEMONIC;
                TwoVectors = TRUE;

            } else {
                BaseMnemonic = ARM_VORR_MNEMONIC;
            }

            break;

        case ARM_SIMD_BITWISE_VORN_VALUE:
            BaseMnemonic = ARM_VORN_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VEOR_VALUE:
            BaseMnemonic = ARM_VEOR_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VBSL_VALUE:
            BaseMnemonic = ARM_VBSL_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VBIT_VALUE:
            BaseMnemonic = ARM_VBIT_MNEMONIC;
            break;

        case ARM_SIMD_BITWISE_VBIF_VALUE:
            BaseMnemonic = ARM_VBIF_MNEMONIC;
            break;

        default:
            break;
        }

        NoSizeSuffix = TRUE;
        break;

    case ARM_SIMD_VHSUB_MASK:
        BaseMnemonic = ARM_VHSUB_MNEMONIC;
        break;

    case ARM_SIMD_VQSUB_MASK:
        BaseMnemonic = ARM_VQSUB_MNEMONIC;
        break;

    case ARM_SIMD_VCGT_MASK:
        BaseMnemonic = ARM_VCGT_MNEMONIC;
        break;

    case ARM_SIMD_VCGE_MASK:
        BaseMnemonic = ARM_VCGE_MNEMONIC;
        break;

    case ARM_SIMD_VSHL_REG_MASK:
        BaseMnemonic = ARM_VSHL_MNEMONIC;
        VectorMString = Context->Operand2;
        VectorNString = Context->Operand3;
        break;

    case ARM_SIMD_VQSHL_REG_MASK:
        BaseMnemonic = ARM_VQSHL_MNEMONIC;
        VectorMString = Context->Operand2;
        VectorNString = Context->Operand3;
        break;

    case ARM_SIMD_VRSHL_MASK:
        BaseMnemonic = ARM_VRSHL_MNEMONIC;
        VectorMString = Context->Operand2;
        VectorNString = Context->Operand3;
        break;

    case ARM_SIMD_VQRSHL_MASK:
        BaseMnemonic = ARM_VQRSHL_MNEMONIC;
        VectorMString = Context->Operand2;
        VectorNString = Context->Operand3;
        break;

    case ARM_SIMD_VMAX_INT_MASK:
        BaseMnemonic = ARM_VMAX_MNEMONIC;
        break;

    case ARM_SIMD_VMIN_INT_MASK:
        BaseMnemonic = ARM_VMIN_MNEMONIC;
        break;

    case ARM_SIMD_VABD_MASK:
        BaseMnemonic = ARM_VABD_MNEMONIC;
        break;

    case ARM_SIMD_VABA_MASK:
        BaseMnemonic = ARM_VABA_MNEMONIC;
        break;

    case ARM_SIMD_VADD_INT_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VSUB_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VADD_MNEMONIC;
        }

        IntegerSize = TRUE;
        break;

    case ARM_SIMD_VTST_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VCEQ_MNEMONIC;
            IntegerSize = TRUE;

        } else {
            BaseMnemonic = ARM_VTST_MNEMONIC;
            SignedSize = FALSE;
        }

        break;

    case ARM_SIMD_VMLA_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VMLS_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VMLA_MNEMONIC;
        }

        IntegerSize = TRUE;
        break;

    case ARM_SIMD_VMUL_MASK:
        BaseMnemonic = ARM_VMUL_MNEMONIC;
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            PolynomialSize = TRUE;
        }

        break;

    case ARM_SIMD_VPMAX_INT_MASK:
        BaseMnemonic = ARM_VPMAX_MNEMONIC;
        break;

    case ARM_SIMD_VPMIN_INT_MASK:
        BaseMnemonic = ARM_VPMIN_MNEMONIC;
        break;

    case ARM_SIMD_VQDMULH_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VQRDMULH_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VQDMULH_MNEMONIC;
        }

        Instruction &= ~ARM_SIMD_DATA_PROCESSING_UNSIGNED;
        break;

    case ARM_SIMD_VPADD_INT_MASK:
        BaseMnemonic = ARM_VPADD_MNEMONIC;
        IntegerSize = TRUE;
        break;

    case ARM_SIMD_VFMA_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_VFM_SUBTRACT) != 0) {
            BaseMnemonic = ARM_VFMS_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VFMA_MNEMONIC;
        }

        FloatSize = TRUE;
        break;

    case ARM_SIMD_FP_MATH_MASK:
    case ARM_SIMD_FP_MATH_MASK | ARM_SIMD_FP_MULT:
        switch (Instruction & ARM_SIMD_FP_MATH_OP_MASK) {
        case ARM_SIMD_FP_MATH_VADD_VALUE:
            BaseMnemonic = ARM_VADD_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VSUB_VALUE:
            BaseMnemonic = ARM_VSUB_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VPADD_VALUE:
            BaseMnemonic = ARM_VPADD_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VABD_VALUE:
            BaseMnemonic = ARM_VABD_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VMLA_VALUE:
            BaseMnemonic = ARM_VMLA_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VMLS_VALUE:
            BaseMnemonic = ARM_VMLS_MNEMONIC;
            break;

        case ARM_SIMD_FP_MATH_VMUL_VALUE:
            BaseMnemonic = ARM_VMUL_MNEMONIC;
            break;

        default:
            break;
        }

        FloatSize = TRUE;
        break;

    case ARM_SIMD_COMPARE_MASK:
    case ARM_SIMD_COMPARE_MASK | ARM_SIMD_ABSOLUTE:
        switch (Instruction & ARM_SIMD_COMPARE_OP_MASK) {
        case ARM_SIMD_COMPARE_VCEQ_VALUE:
            BaseMnemonic = ARM_VCEQ_MNEMONIC;
            break;

        case ARM_SIMD_COMPARE_VCGE_VALUE:
            BaseMnemonic = ARM_VCGE_MNEMONIC;
            break;

        case ARM_SIMD_COMPARE_VCGT_VALUE:
            BaseMnemonic = ARM_VCGT_MNEMONIC;
            break;

        case ARM_SIMD_COMPARE_VACGE_VALUE:
            BaseMnemonic = ARM_VACGE_MNEMONIC;
            break;

        case ARM_SIMD_COMPARE_VACGT_VALUE:
            BaseMnemonic = ARM_VACGT_MNEMONIC;
            break;

        default:
            break;
        }

        FloatSize = TRUE;
        break;

    case ARM_SIMD_MIN_MAX_FLOAT_MASK:
        switch (Instruction & ARM_SIMD_MIN_MAX_FLOAT_OP_MASK) {
        case ARM_SIMD_MIN_MAX_FLOAT_VMAX_VALUE:
            BaseMnemonic = ARM_VMAX_MNEMONIC;
            break;

        case ARM_SIMD_MIN_MAX_FLOAT_VMIN_VALUE:
            BaseMnemonic = ARM_VMIN_MNEMONIC;
            break;

        case ARM_SIMD_MIN_MAX_FLOAT_VPMAX_VALUE:
            BaseMnemonic = ARM_VPMAX_MNEMONIC;
            break;

        case ARM_SIMD_MIN_MAX_FLOAT_VPMIN_VALUE:
            BaseMnemonic = ARM_VPMIN_MNEMONIC;
            break;

        default:
            break;
        }

        FloatSize = TRUE;
        break;

    case ARM_SIMD_RECIPROCOL_MASK:
        switch (Instruction & ARM_SIMD_RECIPROCOL_OP_MASK) {
        case ARM_SIMD_RECIPROCOL_VRECPS_VALUE:
            BaseMnemonic = ARM_VRECPS_MNEMONIC;
            break;

        case ARM_SIMD_RECIPROCOL_VRSQRTS_VALUE:
            BaseMnemonic = ARM_VRSQRTS_MNEMONIC;
            break;

        default:
            break;
        }

        FloatSize = TRUE;
        break;

    default:
        break;
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    //
    // Parse the instruction assuming it uses the default size suffix.
    //

    SizeTypeSuffix = "";
    SizeValueSuffix = "";
    if (NoSizeSuffix == FALSE) {
        if (FloatSize != FALSE) {
            SizeTypeSuffix = ARM_SIMD_DATA_FLOAT;
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;

        } else if (IntegerSize != FALSE) {
            SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;

        } else if (PolynomialSize != FALSE) {
            SizeTypeSuffix = ARM_SIMD_DATA_POLYNOMIAL;

        } else if (SignedSize == FALSE) {
            SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;

        } else if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            SizeTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

        } else {
            SizeTypeSuffix = ARM_SIMD_DATA_SIGNED;
        }
    }

    if ((NoSizeSuffix == FALSE) && (FloatSize == FALSE)) {
        switch (Instruction & ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_MASK) {
        case ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_8:
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
            break;

        case ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_16:
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
            break;

        case ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_32:
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
            break;

        case ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_64:
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_64;
            break;

        default:
            break;
        }
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    sprintf(VectorDString, "%s%d", VectorTypeString, VectorD);
    sprintf(VectorNString, "%s%d", VectorTypeString, VectorN);
    if (TwoVectors == FALSE) {
        sprintf(VectorMString, "%s%d", VectorTypeString, VectorM);
    }

    return;
}

VOID
DbgpArmDecodeSimdOneRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data processing instrution that uses one
    register and a modified immediate value.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Cmode;
    ARM_IMMEDIATE_FLOAT Float;
    ULONGLONG Immediate;
    ULONG Immediate8;
    ULONG Instruction;
    BOOL PrintFloat;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG Vector;
    PSTR VectorTypeString;

    PrintFloat = FALSE;
    Float.Immediate = 0;
    Instruction = Context->Instruction;

    //
    // Decode the immediate value and the size suffix using the cmode value and
    // the op bit.
    //

    Immediate8 = ARM_SIMD_BUILD_IMMEDIATE8(Instruction);
    Cmode = (Instruction & ARM_SIMD_DATA_PROCESSING_1_REGISTER_CMODE_MASK) >>
            ARM_SIMD_DATA_PROCESSING_1_REGISTER_CMODE_SHIFT;

    switch (Cmode & ARM_SIMD_CMODE_TYPE_MASK) {
    case ARM_SIMD_CMODE_TYPE_I32_NO_SHIFT:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = Immediate8;
        break;

    case ARM_SIMD_CMODE_TYPE_I32_SHIFT_8:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = Immediate8 << 8;
        break;

    case ARM_SIMD_CMODE_TYPE_I32_SHIFT_16:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = Immediate8 << 16;
        break;

    case ARM_SIMD_CMODE_TYPE_I32_SHIFT_24:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = Immediate8 << 24;
        break;

    case ARM_SIMD_CMODE_TYPE_I16_NO_SHIFT:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        Immediate = Immediate8;
        break;

    case ARM_SIMD_CMODE_TYPE_I16_SHIFT_8:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        Immediate = Immediate8 << 8;
        break;

    case ARM_SIMD_CMODE_TYPE_I32_SHIFT_ONES:
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        if ((Cmode & ARM_SIMD_CMODE_SHIFT_ONES_16) != 0) {
            Immediate = Immediate8 << 16;
            Immediate |= 0xFFFF;

        } else {
            Immediate = Immediate8 << 8;
            Immediate |= 0xFF;
        }

        break;

    default:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_1_REGISTER_OP_BIT) != 0) {
            if ((Cmode & ARM_SIMD_CMODE_UNDEFINED) != 0) {
                DbgpArmDecodeUndefined(Context);
                return;
            }

            SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
            SizeValueSuffix = ARM_SIMD_DATA_SIZE_64;
            Immediate = ARM_SIMD_BUILD_IMMEDIATE64(Instruction);

        } else {
            if ((Cmode & ARM_SIMD_CMODE_FLOAT_32) != 0) {
                SizeTypeSuffix = ARM_SIMD_DATA_FLOAT;
                SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
                Float.Immediate = ARM_SIMD_BUILD_IMMEDIATE32(Instruction);
                PrintFloat = TRUE;

            } else {
                SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
                SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
                Immediate = Immediate8;
            }
        }

        break;
    }

    //
    // Get the mnemonic based on the cmode value and the op bit.
    //

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_1_REGISTER_OP_BIT) == 0) {

        //
        // For all modes less than 12, the even modes are vmov and the odds are
        // vorr.
        //

        if ((Cmode < ARM_SIMD_CMODE_NO_OP_VORR_MAX) &&
            ((Cmode & ARM_SIMD_CMODE_NO_OP_VORR_BIT) != 0)) {

            BaseMnemonic = ARM_VORR_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VMOV_MNEMONIC;
        }

    } else {

        //
        // With the two exceptions of cmodes 14 and 15, the odd modes are vbic
        // and the even modes are vmvn.
        //

        if ((Cmode < ARM_SIMD_CMODE_OP_VBIC_MAX) &&
            ((Cmode & ARM_SIMD_CMODE_OP_VBIC_BIT) != 0)) {

            BaseMnemonic = ARM_VBIC_MNEMONIC;

        } else if (Cmode == ARM_SIMD_CMODE_OP_VMOV) {
            BaseMnemonic = ARM_VMOV_MNEMONIC;

        } else if (Cmode == ARM_SIMD_CMODE_OP_UNDEFINED) {
            DbgpArmDecodeUndefined(Context);
            return;

        } else {
            BaseMnemonic = ARM_VMVN_MNEMONIC;
        }
    }

    Vector = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        Vector |= (1 << 4);
    }

    VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
        VectorTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    sprintf(Context->Operand1, "%s%d", VectorTypeString, Vector);
    if (PrintFloat == FALSE) {
        sprintf(Context->Operand2, "#%lld  ; 0x%lld", Immediate, Immediate);

    } else {
        sprintf(Context->Operand2,
                "#%d  ; 0x%x %g",
                Immediate8,
                Float.Immediate,
                Float.Float);
    }

    return;
}

VOID
DbgpArmDecodeSimdTwoRegistersWithShift (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data instruction with two registers and a
    shift amount.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    PSTR DestinationSizeSuffix;
    PSTR DestinationTypeSuffix;
    ULONG Immediate;
    ULONG Immediate6;
    ULONG Instruction;
    PSTR SourceSizeSuffix;
    PSTR SourceTypeSuffix;
    ULONG VectorD;
    PSTR VectorDTypeString;
    ULONG VectorM;
    PSTR VectorMTypeString;

    SourceSizeSuffix = "";

    //
    // Gather the information that is shared by most of the two register shift
    // instructions.
    //

    Instruction = Context->Instruction;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorMTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
        VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    }

    SourceTypeSuffix = "";
    DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
        DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;
    }

    DestinationSizeSuffix = "";
    Immediate6 = (Instruction & ARM_SIMD_2_REGISTER_SHIFT_IMMEDIATE6_MASK) >>
                 ARM_SIMD_2_REGISTER_SHIFT_IMMEDIATE6_SHIFT;

    if ((Instruction & ARM_SIMD_2_REGISTER_SHIFT_64) == 0) {
        if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            Immediate = 32;
            Immediate -= (Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32_MASK);

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_16;
            Immediate = 16;
            Immediate -= (Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16_MASK);

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_8;
            Immediate = 8;
            Immediate -= (Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8_MASK);

        } else {
            DbgpArmDecodeUndefined(Context);
            return;
        }

    } else {
        DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_64;
        Immediate = 64 - Immediate6;
    }

    //
    // Determine the base mnemonic and override any of the size or type
    // information collected above.
    //

    BaseMnemonic = NULL;
    switch (Instruction & ARM_SIMD_2_REGISTER_SHIFT_OPERATION_MASK) {
    case ARM_SIMD_VSHR_MASK:
        BaseMnemonic = ARM_VSHR_MNEMONIC;
        break;

    case ARM_SIMD_VSRA_MASK:
        BaseMnemonic = ARM_VSRA_MNEMONIC;
        break;

    case ARM_SIMD_VRSHR_MASK:
        BaseMnemonic = ARM_VRSHR_MNEMONIC;
        break;

    case ARM_SIMD_VRSRA_MASK:
        BaseMnemonic = ARM_VRSRA_MNEMONIC;
        break;

    case ARM_SIMD_VSRI_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VSRI_MNEMONIC;
        }

        break;

    case ARM_SIMD_VSHL_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VSLI_MNEMONIC;
            DestinationTypeSuffix = "";

        } else {
            BaseMnemonic = ARM_VSHL_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
        }

        break;

    case ARM_SIMD_VQSHLU_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VQSHLU_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
        }

        break;

    case ARM_SIMD_VQSHL_IMM_MASK:
        BaseMnemonic = ARM_VQSHL_MNEMONIC;
        break;

    case ARM_SIMD_VSHRN_MASK:
        switch (Instruction & ARM_SIMD_VSHRN_OP_MASK) {
        case ARM_SIMD_VSHRN_OP_VALUE:
            BaseMnemonic = ARM_VSHRN_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            break;

        case ARM_SIMD_VRSHRN_OP_VALUE:
            BaseMnemonic = ARM_VRSHRN_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            break;

        case ARM_SIMD_VQSHRUN_OP_VALUE:
            BaseMnemonic = ARM_VQSHRUN_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            break;

        case ARM_SIMD_VQRSHRUN_OP_VALUE:
            BaseMnemonic = ARM_VQRSHRUN_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            break;

        default:
            break;
        }

        //
        // The size suffix is twice that of the normal encoding.
        //

        if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_64;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_16;
        }

        VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VQSHRN_MASK:
        switch (Instruction & ARM_SIMD_VQSHRN_OP_MASK) {
        case ARM_SIMD_VQSHRN_OP_VALUE:
            BaseMnemonic = ARM_VQSHRN_MNEMONIC;
            break;

        case ARM_SIMD_VQRSHRN_OP_VALUE:
            BaseMnemonic = ARM_VQRSHRN_MNEMONIC;
            break;

        default:
            break;
        }

        //
        // The size suffix is twice that of the normal encoding.
        //

        if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_64;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8) != 0) {
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_16;
        }

        VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VSHLL_MASK:
        if ((Instruction & ARM_SIMD_VSHLL_OP_MASK) != ARM_SIMD_VSHLL_OP_VALUE) {
            break;
        }

        //
        // The size suffix is twice that of the normal encoding.
        //

        if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32) != 0) {
            Immediate = Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_32_MASK;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16) != 0) {
            Immediate = Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_16_MASK;

        } else if ((Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8) != 0) {
            Immediate = Immediate6 & ARM_SIMD_2_REGISTER_SHIFT_SIZE_8_MASK;
        }

        if (Immediate == 0) {
            BaseMnemonic = ARM_VMOVL_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VSHLL_MNEMONIC;
        }

        VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        break;

    case ARM_SIMD_VCVT_TO_FLOAT_MASK:
        BaseMnemonic = ARM_VCVT_MNEMONIC;
        SourceSizeSuffix = ARM_SIMD_DATA_SIZE_32;
        DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;
        DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = 64 - Immediate6;
        break;

    case ARM_SIMD_VCVT_TO_FIXED_MASK:
        BaseMnemonic = ARM_VCVT_MNEMONIC;
        SourceTypeSuffix = ARM_SIMD_DATA_FLOAT;
        SourceSizeSuffix = ARM_SIMD_DATA_SIZE_32;
        DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
        Immediate = 64 - Immediate6;
        break;

    default:
        break;
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s%s%s",
            DestinationTypeSuffix,
            DestinationSizeSuffix,
            SourceTypeSuffix,
            SourceSizeSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorMTypeString, VectorM);
    sprintf(Context->Operand3, "#%d  ; 0x%x", Immediate, Immediate);
    return;
}

VOID
DbgpArmDecodeSimdThreeRegistersDifferentLength (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data instruction with three registers of
    different lengths.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG Size;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG VectorD;
    PSTR VectorDTypeString;
    ULONG VectorM;
    PSTR VectorMTypeString;
    ULONG VectorN;
    PSTR VectorNTypeString;

    //
    // Gather the values that are common to most of the instructions.
    //

    Instruction = Context->Instruction;
    VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorMTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    VectorNTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorN = (Instruction & ARM_SIMD_DATA_PROCESSING_VN_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VN_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VN_BIT) != 0) {
        VectorN |= (1 << 4);
    }

    SizeTypeSuffix = ARM_SIMD_DATA_SIGNED;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
        SizeTypeSuffix = ARM_SIMD_DATA_UNSIGNED;
    }

    Size = (Instruction & ARM_SIMD_3_DIFF_SIZE_MASK) >>
           ARM_SIMD_3_DIFF_SIZE_SHIFT;

    //
    // Sort out which instruction is actually being decoded and modify the
    // common values as necessary.
    //

    BaseMnemonic = NULL;
    switch (Instruction & ARM_SIMD_3_DIFF_OPERATION_MASK) {
    case ARM_SIMD_VADDL_MASK:
        BaseMnemonic = ARM_VADDL_MNEMONIC;
        break;

    case ARM_SIMD_VADDW_MASK:
        BaseMnemonic = ARM_VADDW_MNEMONIC;
        VectorNTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VSUBL_MASK:
        BaseMnemonic = ARM_VSUBL_MNEMONIC;
        break;

    case ARM_SIMD_VSUBW_MASK:
        BaseMnemonic = ARM_VSUBW_MNEMONIC;
        VectorNTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VADDHN_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VRADDHN_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VADDHN_MNEMONIC;
        }

        //
        // The size is double the normal encoding, so add 1 to the encoding.
        //

        Size += 1;
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        VectorNTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VABAL_MASK:
        BaseMnemonic = ARM_VABAL_MNEMONIC;
        break;

    case ARM_SIMD_VSUBHN_MASK:
        if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
            BaseMnemonic = ARM_VRSUBHN_MNEMONIC;

        } else {
            BaseMnemonic = ARM_VSUBHN_MNEMONIC;
        }

        //
        // The size is double the normal encoding, so add 1 to the encoding.
        //

        Size += 1;
        SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        VectorNTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        break;

    case ARM_SIMD_VABDL_MASK:
        BaseMnemonic = ARM_VABDL_MNEMONIC;
        break;

    case ARM_SIMD_VMLAL_MASK:
        BaseMnemonic = ARM_VMLAL_MNEMONIC;
        break;

    case ARM_SIMD_VMLSL_MASK:
        BaseMnemonic = ARM_VMLSL_MNEMONIC;
        break;

    case ARM_SIMD_VQDMLAL_MASK:
        BaseMnemonic = ARM_VQDMLAL_MNEMONIC;
        break;

    case ARM_SIMD_VQDMLSL_MASK:
        BaseMnemonic = ARM_VQDMLSL_MNEMONIC;
        break;

    case ARM_SIMD_VMULL_INT_MASK:
        BaseMnemonic = ARM_VMULL_MNEMONIC;
        break;

    case ARM_SIMD_VQDMULL_MASK:
        BaseMnemonic = ARM_VQDMULL_MNEMONIC;
        break;

    case ARM_SIMD_VMULL_POLY_MASK:
        BaseMnemonic = ARM_VMULL_MNEMONIC;
        SizeTypeSuffix = ARM_SIMD_DATA_POLYNOMIAL;
        break;

    default:
        break;
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    //
    // Get the size suffix now that it has been adjusted for the particular
    // instruction being decoded.
    //

    switch (Size) {
    case ARM_SIMD_3_DIFF_SIZE_8:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
        break;

    case ARM_SIMD_3_DIFF_SIZE_16:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        break;

    case ARM_SIMD_3_DIFF_SIZE_32:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        break;

    case ARM_SIMD_3_DIFF_SIZE_64:
        SizeValueSuffix = ARM_SIMD_DATA_SIZE_64;
        break;

    //
    // This should never hit as all possible values are accounted for.
    //

    default:
        SizeValueSuffix = "";
        break;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorNTypeString, VectorN);
    sprintf(Context->Operand3, "%s%d", VectorMTypeString, VectorM);
    return;
}

VOID
DbgpArmDecodeSimdTwoRegistersWithScalar (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data instruction with two registers and a
    scalar.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    BOOL FloatValid;
    ULONG Instruction;
    BOOL QuadwordValid;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG VectorD;
    PSTR VectorDTypeString;
    ULONG VectorM;
    ULONG VectorMIndex;
    ULONG VectorN;
    PSTR VectorNTypeString;

    Instruction = Context->Instruction;
    VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorNTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorN = (Instruction & ARM_SIMD_DATA_PROCESSING_VN_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VN_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VN_BIT) != 0) {
        VectorN |= (1 << 4);
    }

    //
    // Vector M stores both the vector and the index. The division of the bits
    // depend on the instruction's encoded size.
    //

    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    switch (Instruction & ARM_SIMD_2_REGISTER_SCALAR_SIZE_MASK) {
    case ARM_SIMD_2_REGISTER_SCALAR_SIZE_16:
        VectorMIndex = (VectorM &
                        ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_INDEX_MASK) >>
                       ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_INDEX_SHIFT;

        VectorM = (VectorM &
                   ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_VECTOR_MASK) >>
                  ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_VECTOR_SHIFT;

        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_SIZE_32:
        VectorMIndex = (VectorM &
                        ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_INDEX_MASK) >>
                       ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_INDEX_SHIFT;

        VectorM = (VectorM &
                   ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_VECTOR_MASK) >>
                  ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_VECTOR_SHIFT;

        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        break;

    default:
        DbgpArmDecodeUndefined(Context);
        return;
    }

    SizeTypeSuffix = ARM_SIMD_DATA_SIGNED;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
        SizeTypeSuffix = ARM_SIMD_DATA_UNSIGNED;
    }

    //
    // Get the base mnemonic and finalize the type suffix.
    //

    FloatValid = FALSE;
    QuadwordValid = FALSE;
    BaseMnemonic = NULL;
    switch (Instruction & ARM_SIMD_2_REGISTER_SCALAR_OPERATION_MASK) {
    case ARM_SIMD_2_REGISTER_SCALAR_VMLA_MASK:
    case (ARM_SIMD_2_REGISTER_SCALAR_VMLA_MASK |
          ARM_SIMD_2_REGISTER_SCALAR_FLOAT):

        FloatValid = TRUE;
        QuadwordValid = TRUE;
        BaseMnemonic = ARM_VMLA_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VMLS_MASK:
    case (ARM_SIMD_2_REGISTER_SCALAR_VMLS_MASK |
          ARM_SIMD_2_REGISTER_SCALAR_FLOAT):

        FloatValid = TRUE;
        QuadwordValid = TRUE;
        BaseMnemonic = ARM_VMLS_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VMLAL_MASK:
        BaseMnemonic = ARM_VMLAL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VMLSL_MASK:
        BaseMnemonic = ARM_VMLSL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VQDMLAL_MASK:
        BaseMnemonic = ARM_VQDMLAL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VQDMLSL_MASK:
        BaseMnemonic = ARM_VQDMLSL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VMUL_MASK:
    case (ARM_SIMD_2_REGISTER_SCALAR_VMUL_MASK |
          ARM_SIMD_2_REGISTER_SCALAR_FLOAT):

        FloatValid = TRUE;
        QuadwordValid = TRUE;
        BaseMnemonic = ARM_VMUL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VMULL_MASK:
        BaseMnemonic = ARM_VMULL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VQDMULL_MASK:
        BaseMnemonic = ARM_VQDMULL_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VQDMULH_MASK:
        QuadwordValid = TRUE;
        BaseMnemonic = ARM_VQDMULH_MNEMONIC;
        break;

    case ARM_SIMD_2_REGISTER_SCALAR_VQRDMULH_MASK:
        QuadwordValid = TRUE;
        BaseMnemonic = ARM_VQRDMULH_MNEMONIC;
        break;

    default:
        break;
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    //
    // Process the quadword and float bits if they are valid for the
    // instruction being decoded.
    //

    if (QuadwordValid != FALSE) {
        if ((Instruction & ARM_SIMD_2_REGISTER_SCALAR_QUADWORD) != 0) {
            VectorNTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;

        } else {
            VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
        }
    }

    if (FloatValid != FALSE) {
        if ((Instruction & ARM_SIMD_2_REGISTER_SCALAR_FLOAT) != 0) {
            SizeTypeSuffix = ARM_SIMD_DATA_FLOAT;

        } else {
            SizeTypeSuffix = ARM_SIMD_DATA_INTEGER;
        }
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorNTypeString, VectorN);
    sprintf(Context->Operand3,
            "%s%d[%d]",
            ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
            VectorM,
            VectorMIndex);

    return;
}

VOID
DbgpArmDecodeSimdTwoRegistersMiscellaneous (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data vector extract instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    PSTR DestinationSizeSuffix;
    PSTR DestinationTypeSuffix;
    BOOL GetSizeSuffix;
    ULONG Immediate;
    ULONG Instruction;
    BOOL PrintImmediate;
    ULONG Size;
    PSTR SourceSizeSuffix;
    PSTR SourceTypeSuffix;
    ULONG VectorD;
    PSTR VectorDTypeString;
    ULONG VectorM;
    PSTR VectorMTypeString;

    Instruction = Context->Instruction;
    VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorMTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    SourceSizeSuffix = "";
    SourceTypeSuffix = "";
    DestinationTypeSuffix = ARM_SIMD_DATA_DEFAULT;
    DestinationSizeSuffix = "";
    GetSizeSuffix = TRUE;
    Size = (Instruction & ARM_SIMD_2_REGISTER_MISC_SIZE_MASK) >>
            ARM_SIMD_2_REGISTER_MISC_SIZE_SHIFT;

    //
    // Some instructions include an immediate value. Default to not print it.
    //

    Immediate = 0;
    PrintImmediate = FALSE;

    //
    // Determine the base mnemonic and perform and instruction specific
    // modifications to the vector and size information.
    //

    BaseMnemonic = NULL;
    switch (Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_MASK) {
    case ARM_SIMD_2_REGISTER_MISC_TYPE_0:
        switch (Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_0_OP_MASK) {
        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV64_MASK:
            BaseMnemonic = ARM_VREV64_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV32_MASK:
            BaseMnemonic = ARM_VREV32_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV16_MASK:
            BaseMnemonic = ARM_VREV16_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADDL_MASK:
        case (ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADDL_MASK |
              ARM_SIMD_2_REGISTER_MISC_TYPE_0_UNSIGNED):

            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_0_UNSIGNED) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

            BaseMnemonic = ARM_VPADDL_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCLS_MASK:
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            BaseMnemonic = ARM_VCLS_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCLZ_MASK:
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            BaseMnemonic = ARM_VCLZ_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCNT_MASK:
            BaseMnemonic = ARM_VCNT_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VMVN_MASK:
            GetSizeSuffix = FALSE;
            DestinationTypeSuffix = "";
            BaseMnemonic = ARM_VMVN_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADAL_MASK:
        case (ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADAL_MASK |
              ARM_SIMD_2_REGISTER_MISC_TYPE_0_UNSIGNED):

            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_0_UNSIGNED) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

            BaseMnemonic = ARM_VPADAL_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VQABS_MASK:
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            BaseMnemonic = ARM_VQABS_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_0_VQNEG_MASK:
            DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            BaseMnemonic = ARM_VQNEG_MNEMONIC;
            break;

        default:
            break;
        }

        //
        // All of the type 0 instructions depend on the quadword bit.
        //

        if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
            VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        }

        break;

    case ARM_SIMD_2_REGISTER_MISC_TYPE_1:

        //
        // The majority of these instructions have an immediate 0 value and
        // default to being signed.
        //

        PrintImmediate = TRUE;
        DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
        switch (Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_1_OP_MASK) {
        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCGT_MASK:
            BaseMnemonic = ARM_VCGT_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCGE_MASK:
            BaseMnemonic = ARM_VCGE_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCEQ_MASK:
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            BaseMnemonic = ARM_VCEQ_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCLE_MASK:
            BaseMnemonic = ARM_VCLE_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCLT_MASK:
            BaseMnemonic = ARM_VCLT_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VABS_MASK:
            PrintImmediate = FALSE;
            BaseMnemonic = ARM_VABS_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_1_VNEG_MASK:
            PrintImmediate = FALSE;
            BaseMnemonic = ARM_VNEG_MNEMONIC;
            break;

        default:
            break;
        }

        //
        // All of the type 1 instructions depend on the type specific float bit.
        //

        if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_1_FLOAT) != 0) {
            DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;
        }

        //
        // All of the type 1 instructions depend on the quadword bit.
        //

        if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
            VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        }

        break;

    case ARM_SIMD_2_REGISTER_MISC_TYPE_2:
        switch (Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_2_OP_MASK) {
        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VSWP_MASK:
            GetSizeSuffix = FALSE;
            DestinationTypeSuffix = "";
            if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
                VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
                VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            }

            BaseMnemonic = ARM_VSWP_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VTRN_MASK:
            if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
                VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
                VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            }

            BaseMnemonic = ARM_VTRN_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VUZP_MASK:
            if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
                VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
                VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            }

            BaseMnemonic = ARM_VUZP_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VZIP_MASK:
            if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
                VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
                VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            }

            BaseMnemonic = ARM_VZIP_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VMOVN_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_2_UNSIGNED) != 0) {
                BaseMnemonic = ARM_VQMOVUN_MNEMONIC;

            } else {
                BaseMnemonic = ARM_VMOVN_MNEMONIC;
            }

            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;

            //
            // The size encodings are doubled, so add 1 to get the correct
            // destination size suffix below.
            //

            Size += 1;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VQMOVN_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_2_UNSIGNED) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;

            //
            // The size encodings are doubled, so add 1 to get the correct
            // destination size suffix below.
            //

            Size += 1;
            BaseMnemonic = ARM_VQMOVN_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VSHLL_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_2_UNSIGNED) != 0) {
                break;
            }

            Immediate = Size;
            VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            BaseMnemonic = ARM_VSHLL_MNEMONIC;
            DestinationTypeSuffix = ARM_SIMD_DATA_INTEGER;
            PrintImmediate = TRUE;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VCVT_HALF_TO_SINGLE_MASK:
            VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            SourceTypeSuffix = ARM_SIMD_DATA_FLOAT;
            SourceSizeSuffix = ARM_SIMD_DATA_SIZE_16;
            GetSizeSuffix = FALSE;
            BaseMnemonic = ARM_VCVT_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_2_VCVT_SINGLE_TO_HALF_MASK:
            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_16;
            SourceTypeSuffix = ARM_SIMD_DATA_FLOAT;
            SourceSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            GetSizeSuffix = FALSE;
            BaseMnemonic = ARM_VCVT_MNEMONIC;
            break;

        default:
            break;
        }

        break;

    case ARM_SIMD_2_REGISTER_MISC_TYPE_3:
        switch (Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_3_OP_MASK) {
        case ARM_SIMD_2_REGISTER_MISC_TYPE_3_VRECPE_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_3_FLOAT) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;
            }

            BaseMnemonic = ARM_VRECPE_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_3_VRSQRTE_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_3_FLOAT) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;
            }

            BaseMnemonic = ARM_VRSQRTE_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_3_VCVT_TO_INTEGER_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_3_UNSIGNED) != 0) {
                DestinationTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                DestinationTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            SourceTypeSuffix = ARM_SIMD_DATA_FLOAT;
            SourceSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            GetSizeSuffix = FALSE;
            BaseMnemonic = ARM_VCVT_MNEMONIC;
            break;

        case ARM_SIMD_2_REGISTER_MISC_TYPE_3_VCVT_FROM_INTEGER_MASK:
            if ((Instruction & ARM_SIMD_2_REGISTER_MISC_TYPE_3_UNSIGNED) != 0) {
                SourceTypeSuffix = ARM_SIMD_DATA_UNSIGNED;

            } else {
                SourceTypeSuffix = ARM_SIMD_DATA_SIGNED;
            }

            SourceSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            DestinationTypeSuffix = ARM_SIMD_DATA_FLOAT;
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            GetSizeSuffix = FALSE;
            BaseMnemonic = ARM_VCVT_MNEMONIC;
            break;

        default:
            break;
        }

        //
        // All of the type 3 instructions depend on the quadword bit.
        //

        if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
            VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
            VectorMTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
        }

        break;

    default:
        break;
    }

    if (BaseMnemonic == NULL) {
        DbgpArmDecodeUndefined(Context);
        return;
    }

    if (GetSizeSuffix != FALSE) {
        switch (Size) {
        case ARM_SIMD_2_REGISTER_MISC_SIZE_8:
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_8;
            break;

        case ARM_SIMD_2_REGISTER_MISC_SIZE_16:
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_16;
            break;

        case ARM_SIMD_2_REGISTER_MISC_SIZE_32:
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_32;
            break;

        case ARM_SIMD_2_REGISTER_MISC_SIZE_64:
            DestinationSizeSuffix = ARM_SIMD_DATA_SIZE_64;
            break;

        default:
            return;
        }
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s%s%s",
            DestinationTypeSuffix,
            DestinationSizeSuffix,
            SourceTypeSuffix,
            SourceSizeSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorMTypeString, VectorM);
    if (PrintImmediate != FALSE) {
        sprintf(Context->Operand3, "#%d ; 0x%x", Immediate, Immediate);
    }

    return;
}

VOID
DbgpArmDecodeSimdVectorExtract (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data vector extract instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate;
    ULONG Instruction;
    ULONG VectorD;
    ULONG VectorM;
    ULONG VectorN;
    PSTR VectorTypeString;

    Instruction = Context->Instruction;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    VectorN = (Instruction & ARM_SIMD_DATA_PROCESSING_VN_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VN_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VN_BIT) != 0) {
        VectorN |= (1 << 4);
    }

    VectorTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
        VectorTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    }

    Immediate = (Instruction & ARM_SIMD_VEXT_IMMEDIATE4_MASK) >>
                ARM_SIMD_VEXT_IMMEDIATE4_SHIFT;

    strcpy(Context->Mnemonic, ARM_VEXT_MNEMONIC);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            ARM_SIMD_DATA_DEFAULT,
            ARM_SIMD_DATA_SIZE_8);

    sprintf(Context->Operand1, "%s%d", VectorTypeString, VectorD);
    sprintf(Context->Operand2, "%s%d", VectorTypeString, VectorN);
    sprintf(Context->Operand3, "%s%d", VectorTypeString, VectorM);
    sprintf(Context->Operand4, "#%d  ; 0x%x", Immediate, Immediate);
    return;
}

VOID
DbgpArmDecodeSimdVectorTableLookup (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD vector table lookup instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Instruction;
    ULONG VectorCount;
    ULONG VectorD;
    ULONG VectorM;
    ULONG VectorN;

    Instruction = Context->Instruction;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    VectorN = (Instruction & ARM_SIMD_DATA_PROCESSING_VN_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VN_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VN_BIT) != 0) {
        VectorN |= (1 << 4);
    }

    VectorCount = (Instruction & ARM_SIMD_VTB_LENGTH_MASK) >>
                  ARM_SIMD_VTB_LENGTH_SHIFT;

    VectorCount += 1;
    if ((Instruction & ARM_SIMD_VTB_EXTENSION) != 0) {
        BaseMnemonic = ARM_VTBX_MNEMONIC;

    } else {
        BaseMnemonic = ARM_VTBL_MNEMONIC;
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            ARM_SIMD_DATA_DEFAULT,
            ARM_SIMD_DATA_SIZE_8);

    sprintf(Context->Operand1,
            "%s%d",
            ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
            VectorD);

    DbgpArmPrintVectorList(Context->Operand2,
                           sizeof(Context->Operand2),
                           VectorN,
                           VectorCount,
                           1,
                           ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
                           0,
                           0);

    sprintf(Context->Operand3,
            "%s%d",
            ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
            VectorM);

    return;
}

VOID
DbgpArmDecodeSimdVectorDuplicate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes an SIMD data vector duplicate instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR SizeTypeSuffix;
    PSTR SizeValueSuffix;
    ULONG VectorD;
    PSTR VectorDTypeString;
    ULONG VectorM;
    ULONG VectorMIndex;

    Instruction = Context->Instruction;
    SizeTypeSuffix = ARM_SIMD_DATA_DEFAULT;
    SizeValueSuffix = "";
    VectorMIndex = 0;
    VectorD = (Instruction & ARM_SIMD_DATA_PROCESSING_VD_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VD_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VD_BIT) != 0) {
        VectorD |= (1 << 4);
    }

    VectorM = (Instruction & ARM_SIMD_DATA_PROCESSING_VM_MASK) >>
              ARM_SIMD_DATA_PROCESSING_VM_SHIFT;

    if ((Instruction & ARM_SIMD_DATA_PROCESSING_VM_BIT) != 0) {
        VectorM |= (1 << 4);
    }

    if ((Instruction & ARM_SIMD_VDUP_SIZE_8_MASK) ==
        ARM_SIMD_VDUP_SIZE_8_VALUE) {

        SizeValueSuffix = ARM_SIMD_DATA_SIZE_8;
        VectorMIndex = (Instruction & ARM_SIMD_VDUP_SIZE_8_INDEX_MASK) >>
                       ARM_SIMD_VDUP_SIZE_8_INDEX_SHIFT;

    } else if ((Instruction & ARM_SIMD_VDUP_SIZE_16_MASK) ==
               ARM_SIMD_VDUP_SIZE_16_VALUE) {

        SizeValueSuffix = ARM_SIMD_DATA_SIZE_16;
        VectorMIndex = (Instruction & ARM_SIMD_VDUP_SIZE_16_INDEX_MASK) >>
                       ARM_SIMD_VDUP_SIZE_16_INDEX_SHIFT;

    } else if ((Instruction & ARM_SIMD_VDUP_SIZE_32_MASK) ==
               ARM_SIMD_VDUP_SIZE_32_VALUE) {

        SizeValueSuffix = ARM_SIMD_DATA_SIZE_32;
        VectorMIndex = (Instruction & ARM_SIMD_VDUP_SIZE_32_INDEX_MASK) >>
                       ARM_SIMD_VDUP_SIZE_32_INDEX_SHIFT;

    } else {
        SizeTypeSuffix = "";
    }

    VectorDTypeString = ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR;
    if ((Instruction & ARM_SIMD_DATA_PROCESSING_QUADWORD) != 0) {
        VectorDTypeString = ARM_FLOATING_POINT_QUADWORD_VECTOR;
    }

    strcpy(Context->Mnemonic, ARM_VDUP_MNEMONIC);
    sprintf(Context->PostConditionMnemonicSuffix,
            "%s%s",
            SizeTypeSuffix,
            SizeValueSuffix);

    sprintf(Context->Operand1, "%s%d", VectorDTypeString, VectorD);
    sprintf(Context->Operand2,
            "%s%d[%d]",
            ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR,
            VectorM,
            VectorMIndex);

    return;
}

PSTR
DbgpArmGetLoadStoreTypeString (
    ULONG Instruction
    )

/*++

Routine Description:

    This routine returns the push/pop type string.

Arguments:

    Instruction - Supplies the instruction. The push/pop type is always in the
        same bits.

Return Value:

    Returns a static string for the push/pop type.

--*/

{

    switch (Instruction & ARM_LOAD_STORE_TYPE_MASK) {
    case ARM_LOAD_STORE_INCREMENT_AFTER:
        return ARM_INCREMENT_AFTER_SUFFIX;

    case ARM_LOAD_STORE_INCREMENT_BEFORE:
        return ARM_INCREMENT_BEFORE_SUFFIX;

    case ARM_LOAD_STORE_DECREMENT_AFTER:
        return ARM_DECREMENT_AFTER_SUFFIX;

    case ARM_LOAD_STORE_DECREMENT_BEFORE:
        return ARM_DECREMENT_BEFORE_SUFFIX;

    default:
        break;
    }

    return "";
}

PSTR
DbgpArmGetBankedRegisterString (
    ULONG Instruction
    )

/*++

Routine Description:

    This routine returns the banked register/mode string, encoded in
    instructions as the m1 and R fields.

Arguments:

    Instruction - Supplies the instruction.

Return Value:

    Returns a static string for the encoding mode.

--*/

{

    ULONG Index;

    Index = (Instruction & ARM_BANKED_MODE_MASK) >> ARM_BANKED_MODE_SHIFT;
    if ((Instruction & ARM_BANKED_MODE_R_BIT) != 0) {
        Index |= 0x20;
    }

    return DbgArmBankedRegisters[Index];
}

VOID
DbgpArmPrintStatusRegister (
    PSTR Operand,
    ULONG Instruction
    )

/*++

Routine Description:

    This routine prints the status register and flags for a given instruction.

Arguments:

    Operand - Supplies the buffer where the completed string will be returned.

    Instruction - Supplies the instruction.

Return Value:

    None.

--*/

{

    ULONG ExtraFlagCount;
    CHAR ExtraFlags[5];
    PSTR Register;

    memset(ExtraFlags, 0, sizeof(ExtraFlags));
    ExtraFlagCount = 0;
    if ((Instruction & ARM_MOVE_STATUS_SPSR) != 0) {
        Register = ARM_SPSR_STRING;

    } else {
        Register = ARM_CPSR_STRING;
    }

    if ((Instruction & ARM_MSR_MASK_C) != 0) {
        ExtraFlags[ExtraFlagCount] = ARM_MSR_MASK_C_FLAG;
        ExtraFlagCount += 1;
    }

    if ((Instruction & ARM_MSR_MASK_X) != 0) {
        ExtraFlags[ExtraFlagCount] = ARM_MSR_MASK_X_FLAG;
        ExtraFlagCount += 1;
    }

    if ((Instruction & ARM_MSR_MASK_S) != 0) {
        ExtraFlags[ExtraFlagCount] = ARM_MSR_MASK_S_FLAG;
        ExtraFlagCount += 1;
    }

    if ((Instruction & ARM_MSR_MASK_F) != 0) {
        ExtraFlags[ExtraFlagCount] = ARM_MSR_MASK_F_FLAG;
        ExtraFlagCount += 1;
    }

    sprintf(Operand, "%s_%s", Register, ExtraFlags);
    return;
}

VOID
DbgpArmPrintVectorList (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG VectorStart,
    ULONG VectorCount,
    ULONG VectorIncrement,
    PSTR VectorTypeString,
    ULONG VectorIndex,
    ULONG Flags
    )

/*++

Routine Description:

    This routine converts a count of vectors starting at a given vector into
    a string.

Arguments:

    Destination - Supplies a pointer to the destination string.

    DestinationSize - Supplies the size of the destination string.

    VectorStart - Supplies the starting vector.

    VectorCount - Supplies the total number of vectors to convert.

    VectorIncrement - Supplies the interval between vectors.

    VectorTypeString - Supplies the character to use to describe each vector.

    VectorIndex - Supplies the option index into each vector.

    Flags - Supplies a bitmask of flags. See DBG_ARM_VECTOR_LIST_FLAG_* for
        definitions.

Return Value:

    None.

--*/

{

    ULONG CurrentVector;
    PSTR Separator;
    INT Size;
    CHAR VectorString[16];

    if (DestinationSize > 1) {
        strcpy(Destination, "{");
        Size = strlen(Destination);
        Destination += Size;
        DestinationSize -= Size;
    }

    Separator = "";
    for (CurrentVector = VectorStart;
         CurrentVector < VectorStart + VectorCount;
         CurrentVector += VectorIncrement) {

        if ((Flags & DBG_ARM_VECTOR_LIST_FLAG_INDEX) != 0) {
            if ((Flags & DBG_ARM_VECTOR_LIST_FLAG_ALL_LANES) != 0) {
                snprintf(VectorString,
                         sizeof(VectorString),
                         "%s%s%d[]",
                         Separator,
                         VectorTypeString,
                         CurrentVector);

            } else {
                snprintf(VectorString,
                         sizeof(VectorString),
                         "%s%s%d[%d]",
                         Separator,
                         VectorTypeString,
                         CurrentVector,
                         VectorIndex);
            }

        } else {
            snprintf(VectorString,
                     sizeof(VectorString),
                     "%s%s%d",
                     Separator,
                     VectorTypeString,
                     CurrentVector);
        }

        Size = strlen(VectorString);
        if (Size < DestinationSize) {
            strcpy(Destination, VectorString);
            Destination += Size;
            DestinationSize -= Size;
        }

        Separator = ", ";
    }

    if (DestinationSize > 1) {
        strcpy(Destination, "}");
    }

    return;
}

VOID
DbgpArmDecodeImmediateShift (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG Register,
    ULONG Type,
    ULONG Immediate
    )

/*++

Routine Description:

    This routine converts a register, type and immediate value into a string
    representing the register shifted by the immediate value.

Arguments:

    Destination - Supplies a pointer to the destination string.

    DestinationSize - Supplies the size of the destination string.

    Register - Supplies the register that is to be shifted.

    Type - Supplies the type of shift. See ARM_SHIFT_* for definitions.

    Immediate - Supplies the immediate value by which to shift.

Return Value:

    None.

--*/

{

    PSTR RegisterName;
    PSTR ShiftType;

    ShiftType = NULL;
    switch (Type) {
    case ARM_SHIFT_LSL:
        if (Immediate != 0) {
            ShiftType = ARM_LSL_MNEMONIC;
        }

        break;

    case ARM_SHIFT_LSR:
        if (Immediate == 0) {
            Immediate = 32;
        }

        ShiftType = ARM_LSR_MNEMONIC;
        break;

    case ARM_SHIFT_ASR:
        if (Immediate == 0) {
            Immediate = 32;
        }

        ShiftType = ARM_ASR_MNEMONIC;
        break;

    case ARM_SHIFT_ROR:
        if (Immediate == 0) {
            ShiftType = ARM_RRX_MNEMONIC;

        } else {
            ShiftType = ARM_ROR_MNEMONIC;
        }

        break;

    //
    // This case should never hit since all 4 bit combinations are
    // covered.
    //

    default:
        break;
    }

    RegisterName = DbgArmRegisterNames[Register];
    if (Immediate != 0) {
        sprintf(Destination, "%s, %s #%d", RegisterName, ShiftType, Immediate);

    } else if (ShiftType != NULL) {
        sprintf(Destination, "%s, %s", RegisterName, ShiftType);

    } else {
        sprintf(Destination, "%s", RegisterName);
    }

    return;
}

