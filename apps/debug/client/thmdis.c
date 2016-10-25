/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    thmdis.c

Abstract:

    This module implements support for disassembling the Thumb-2 instruction
    set on ARM processors.

Author:

    Evan Green 26-Apr-2014

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

#include <assert.h>
#include <string.h>
#include <stdio.h>

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
DbgpThumb16DecodeShiftMoveCompare (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeShiftImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeAddSubtractRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeAddSubtractImmediate3 (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeMoveCompareAddSubtractImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeDataProcessing (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeSpecialDataAndBx (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLdrLiteral (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLoadStoreSingle (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLoadStoreSingleRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLoadStoreSingleImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLoadStoreSingleSpRelative (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeAdrAddSp (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeMiscellaneous (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeConditionalBranchAndSvc (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeUnconditionalBranch (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeAdjustStackPointer (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeCompareBranchIfZero (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeSignZeroExtend (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodePushPop (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeSetEndianness (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeChangeState (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeReverseBytes (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeBreakpoint (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb16DecodeIfThenAndHints (
    PARM_DISASSEMBLY Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define mnemonic tables.
//

PSTR DbgThumb16ShiftImmediateMnemonics[] = {
    "lsls",
    "lsrs",
    "asrs"
};

PSTR DbgThumb16MoveCompareImmediateOpcodes[] = {
    THUMB_MOVS_MNEMONIC,
    THUMB_CMP_MNEMONIC,
    THUMB_ADDS_MNEMONIC,
    THUMB_SUBS_MNEMONIC
};

PSTR DbgThumb16DataProcessingMnemonics[] = {
    "ands",
    "eors",
    "lsls",
    "lsrs",
    "asrs",
    "adcs",
    "sbcs",
    "rors",
    "tst",
    "rsbs",
    "cmp",
    "cmns",
    "orrs",
    "muls",
    "bics",
    "mvns",
};

PSTR DbgThumb16SpecialDataProcessingMnemonics[] = {
    "add",
    "cmp",
    "mov"
};

PSTR DbgThumb16LoadStoreSingleRegisterMnemonics[] = {
    "str",
    "strh",
    "strb",
    "ldrsb",
    "ldr",
    "ldrh",
    "ldrb",
    "ldrsh"
};

PSTR DbgThumb16SignZeroExtendMnemonics[] = {
    "sxth",
    "sxtb",
    "uxth",
    "uxtb"
};

PSTR DbgThumb16ReverseBytesMnemonics[] = {
    "rev",
    "rev16",
    "rev??",
    "revsh"
};

//
// Define the two if then suffix arrays, one for when the least significant bit
// of the first condition is clear, and the other for when it's set.
//

PSTR DbgThumb16IfThenSuffixes[2][16] = {
    {
        "",
        "ttt",
        "tt",
        "tte",
        "t",
        "tet",
        "te",
        "tee",
        "",
        "ett",
        "et",
        "ete",
        "e",
        "eet",
        "ee",
        "eee",
    },
    {
        "",
        "eee",
        "ee",
        "eet",
        "e",
        "ete",
        "et",
        "ett",
        "",
        "tee",
        "te",
        "tet",
        "t",
        "tte",
        "tt",
        "ttt",
    }
};

PSTR DbgThumb16HintsMnemonics[16] = {
    "nop",
    "yield",
    "wfe",
    "wfi",
    "srv",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???",
    "hints???"
};

//
// Define decode tables.
//

THUMB_DECODE_BRANCH DbgThumb32Table[] = {
    {0x1F, 0x1D, 11, DbgpThumb32Decode},
    {0x1F, 0x1E, 11, DbgpThumb32Decode},
    {0x1F, 0x1F, 11, DbgpThumb32Decode}
};

THUMB_DECODE_BRANCH DbgThumb16TopLevelTable[] = {
    {0x30, 0x00, 10, DbgpThumb16DecodeShiftMoveCompare},
    {0x3F, 0x10, 10, DbgpThumb16DecodeDataProcessing},
    {0x3F, 0x11, 10, DbgpThumb16DecodeSpecialDataAndBx},
    {0x3E, 0x12, 10, DbgpThumb16DecodeLdrLiteral},
    {0x3C, 0x14, 10, DbgpThumb16DecodeLoadStoreSingle},
    {0x38, 0x18, 10, DbgpThumb16DecodeLoadStoreSingle},
    {0x38, 0x20, 10, DbgpThumb16DecodeLoadStoreSingle},
    {0x3E, 0x28, 10, DbgpThumb16DecodeAdrAddSp},
    {0x3E, 0x2A, 10, DbgpThumb16DecodeAdrAddSp},
    {0x3C, 0x2C, 10, DbgpThumb16DecodeMiscellaneous},
    {0x3E, 0x30, 10, DbgpThumb16DecodeLoadStoreMultiple},
    {0x3E, 0x30, 10, DbgpThumb16DecodeLoadStoreMultiple},
    {0x3C, 0x34, 10, DbgpThumb16DecodeConditionalBranchAndSvc},
    {0x3E, 0x38, 10, DbgpThumb16DecodeUnconditionalBranch}
};

THUMB_DECODE_BRANCH DbgThumb16ShiftAddSubMovCmpTable[] = {
    {0x1C, 0x00, 9, DbgpThumb16DecodeShiftImmediate},
    {0x1C, 0x04, 9, DbgpThumb16DecodeShiftImmediate},
    {0x1C, 0x08, 9, DbgpThumb16DecodeShiftImmediate},
    {0x1F, 0x0C, 9, DbgpThumb16DecodeAddSubtractRegister},
    {0x1F, 0x0D, 9, DbgpThumb16DecodeAddSubtractRegister},
    {0x1F, 0x0E, 9, DbgpThumb16DecodeAddSubtractImmediate3},
    {0x1F, 0x0F, 9, DbgpThumb16DecodeAddSubtractImmediate3},
    {0x1C, 0x10, 9, DbgpThumb16DecodeMoveCompareAddSubtractImmediate},
    {0x1C, 0x14, 9, DbgpThumb16DecodeMoveCompareAddSubtractImmediate},
    {0x1C, 0x18, 9, DbgpThumb16DecodeMoveCompareAddSubtractImmediate},
    {0x1C, 0x1C, 9, DbgpThumb16DecodeMoveCompareAddSubtractImmediate},
};

THUMB_DECODE_BRANCH DbgThumb16LoadStoreSingleTable[] = {
    {0xF, 0x5, 12, DbgpThumb16DecodeLoadStoreSingleRegister},
    {0xF, 0x6, 12, DbgpThumb16DecodeLoadStoreSingleImmediate},
    {0xF, 0x7, 12, DbgpThumb16DecodeLoadStoreSingleImmediate},
    {0xF, 0x8, 12, DbgpThumb16DecodeLoadStoreSingleImmediate},
    {0xF, 0x9, 12, DbgpThumb16DecodeLoadStoreSingleSpRelative},
};

THUMB_DECODE_BRANCH DbgThumb16MiscellaneousTable[] = {
    {0xF, 0x0, 8, DbgpThumb16DecodeAdjustStackPointer},
    {0x5, 0x1, 8, DbgpThumb16DecodeCompareBranchIfZero},
    {0xF, 0x2, 8, DbgpThumb16DecodeSignZeroExtend},
    {0x6, 0x4, 8, DbgpThumb16DecodePushPop},
    {0xFF, 0x65, 4, DbgpThumb16DecodeSetEndianness},
    {0xFE, 0x66, 4, DbgpThumb16DecodeChangeState},
    {0xF, 0xA, 8, DbgpThumb16DecodeReverseBytes},
    {0xF, 0xE, 8, DbgpThumb16DecodeBreakpoint},
    {0xF, 0xF, 8, DbgpThumb16DecodeIfThenAndHints},
};

//
// ------------------------------------------------------------------ Functions
//

VOID
DbgpThumbDecode (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb-2 instruction set.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    BOOL Decoded;

    //
    // It's a 32-bit instruction if it matches one of the table values,
    // otherwise it's a 16-bit instruction.
    //

    strcpy(Context->Mnemonic, "Unknown thumb");
    Decoded = THUMB_DECODE_WITH_TABLE(Context, DbgThumb32Table);
    if (Decoded != FALSE) {
        Context->Result->BinaryLength = THUMB32_INSTRUCTION_LENGTH;

    } else {
        Context->Result->BinaryLength = THUMB16_INSTRUCTION_LENGTH;

        //
        // Use the 16 bit table.
        //

        THUMB_DECODE_WITH_TABLE(Context, DbgThumb16TopLevelTable);
    }

    return;
}

BOOL
DbgpThumbDecodeWithTable (
    PARM_DISASSEMBLY Context,
    PTHUMB_DECODE_BRANCH Table,
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

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpThumb16DecodeShiftMoveCompare (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes shift (immediate), add, subtract, move, and compare
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    BOOL Decoded;

    Decoded = THUMB_DECODE_WITH_TABLE(Context,
                                      DbgThumb16ShiftAddSubMovCmpTable);

    assert(Decoded != FALSE);

    return;
}

VOID
DbgpThumb16DecodeShiftImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes shift (immediate) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate5;
    ULONG Instruction;
    ULONG Op;
    ULONG Rd;
    ULONG Rm;

    Instruction = Context->Instruction;

    //
    // Watch out for the special case of all zero, it's a MOV.
    //

    if ((Instruction & THUMB16_MOVS_MASK) == THUMB16_MOVS_VALUE) {
        Rm = (Instruction >> THUMB16_MOVS_RM_SHIFT) & THUMB_REGISTER8_MASK;
        Rd = (Instruction >> THUMB16_MOVS_RD_SHIFT) & THUMB_REGISTER8_MASK;
        strcpy(Context->Mnemonic, THUMB_MOVS_MNEMONIC);
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
        return;
    }

    Op = (Instruction >> THUMB16_SHIFT_IMMEDIATE_OP_SHIFT) &
         THUMB16_SHIFT_IMMEDIATE_OP_MASK;

    assert(Op != 0x3);

    Rm = (Instruction >> THUMB16_SHIFT_IMMEDIATE_RM_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rd = (Instruction >> THUMB16_SHIFT_IMMEDIATE_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Immediate5 = (Instruction >> THUMB16_SHIFT_IMMEDIATE5_SHIFT) &
                 THUMB_IMMEDIATE5_MASK;

    if (Immediate5 == 0) {
        Immediate5 = 32;
    }

    strcpy(Context->Mnemonic, DbgThumb16ShiftImmediateMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
    snprintf(Context->Operand3, sizeof(Context->Operand3), "#%d", Immediate5);
    return;
}

VOID
DbgpThumb16DecodeAddSubtractRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes Thumb 16-bit add/subtract (register) instructions.

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
    Rd = (Instruction >> THUMB16_ADD_SUBTRACT_REGISTER_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rm = (Instruction >> THUMB16_ADD_SUBTRACT_REGISTER_RM_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rn = (Instruction >> THUMB16_ADD_SUBTRACT_REGISTER_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    if ((Instruction & THUMB16_SUBTRACT) != 0) {
        strcpy(Context->Mnemonic, THUMB_SUBS_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_ADDS_MNEMONIC);
    }

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpThumb16DecodeAddSubtractImmediate3 (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes Thumb 16-bit add/subtract (3 bit immediate)
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate3;
    ULONG Instruction;
    ULONG Rd;
    ULONG Rn;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB16_ADD_SUBTRACT_IMMEDIATE3_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rn = (Instruction >> THUMB16_ADD_SUBTRACT_IMMEDIATE3_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    Immediate3 = (Instruction >> THUMB16_ADD_SUBTRACT_IMMEDIATE3_SHIFT) &
                 THUMB_IMMEDIATE3_MASK;

    if ((Instruction & THUMB16_SUBTRACT) != 0) {
        strcpy(Context->Mnemonic, THUMB_SUBS_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_ADDS_MNEMONIC);
    }

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    snprintf(Context->Operand3, sizeof(Context->Operand3), "#%d", Immediate3);
    return;
}

VOID
DbgpThumb16DecodeMoveCompareAddSubtractImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes Thumb 16-bit move and compare (immediate) instructions,
    as well as the add/subtract (8-bit immediate) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;
    ULONG Op;
    ULONG Register;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_MOVE_COMPARE_IMMEDIATE_OP_SHIFT) &
         THUMB16_MOVE_COMPARE_IMMEDIATE_OP_MASK;

    Register = (Instruction >> THUMB16_MOVE_COMPARE_IMMEDIATE_REGISTER_SHIFT) &
               THUMB_REGISTER8_MASK;

    Immediate8 = (Instruction >> THUMB16_MOVE_COMPARE_IMMEDIATE_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    strcpy(Context->Mnemonic, DbgThumb16MoveCompareImmediateOpcodes[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Register]);
    snprintf(Context->Operand2, sizeof(Context->Operand2), "#%d", Immediate8);
    return;
}

VOID
DbgpThumb16DecodeDataProcessing (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes Thumb 16-bit data processing instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op;
    ULONG Rd;
    ULONG Rm;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_DATA_PROCESSING_OP_SHIFT) &
         THUMB16_DATA_PROCESSING_OP_MASK;

    Rd = (Instruction >> THUMB16_DATA_PROCESSING_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rm = (Instruction >> THUMB16_DATA_PROCESSING_RM_SHIFT) &
         THUMB_REGISTER8_MASK;

    strcpy(Context->Mnemonic, DbgThumb16DataProcessingMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
    if (Op == THUMB16_DATA_PROCESSING_RSB) {
        strcpy(Context->Operand3, "#0");

    } else if (Op == THUMB16_DATA_PROCESSING_MUL) {
        strcpy(Context->Operand3, Context->Operand1);
    }

    return;
}

VOID
DbgpThumb16DecodeSpecialDataAndBx (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes Thumb 16-bit special data processing
    (accessing R8-R14) and branch with exchange (bl and blx) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op;
    ULONG Rd;
    ULONG Rm;

    Instruction = Context->Instruction;

    //
    // These registers can access the full R0-R15. The Rd register's high bit
    // is not stored sequentially though.
    //

    Rm = (Instruction >> THUMB16_SPECIAL_DATA_RM_SHIFT) & THUMB_REGISTER16_MASK;
    Rd = (Instruction >> THUMB16_SPECIAL_DATA_RD_SHIFT) & THUMB_REGISTER8_MASK;
    if ((Instruction & THUMB16_SPECIAL_DATA_RD_HIGH) != 0) {
        Rd |= 0x8;
    }

    Op = (Instruction >> THUMB16_SPECIAL_DATA_OP_SHIFT) &
         THUMB16_SPECIAL_DATA_OP_MASK;

    //
    // Handle bl and blx, which are also nestled in this branch (get it) of
    // the instruction set.
    //

    if (Op == THUMB16_SPECIAL_DATA_OP_BRANCH) {
        if ((Instruction & THUMB16_SPECIAL_DATA_BRANCH_LINK) != 0) {
            strcpy(Context->Mnemonic, THUMB_BLX_MNEMONIC);

        } else {
            strcpy(Context->Mnemonic, THUMB_BX_MNEMONIC);
        }

        strcpy(Context->Operand1, DbgArmRegisterNames[Rm]);
        return;
    }

    strcpy(Context->Mnemonic, DbgThumb16SpecialDataProcessingMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpThumb16DecodeLdrLiteral (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit LDR (load literal from PC-relative
    address).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;
    ULONGLONG OperandAddress;
    ULONG Rt;

    Instruction = Context->Instruction;
    Immediate8 = (Instruction >> THUMB16_LDR_IMMEDIATE8_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    Immediate8 <<= 2;
    Rt = (Instruction >> THUMB16_LDR_RT_SHIFT) & THUMB_REGISTER8_MASK;
    strcpy(Context->Mnemonic, THUMB_LDR_MNEMONIC);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);

    //
    // The immediate value is relative to the PC aligned down to a 4-byte
    // boundary. On Thumb, the PC is always 4 bytes ahead of the instruction
    // pointer.
    //

    OperandAddress = Context->InstructionPointer + 4;
    OperandAddress = THUMB_ALIGN_4(OperandAddress);
    OperandAddress += Immediate8;
    Context->Result->OperandAddress = OperandAddress;
    Context->Result->AddressIsDestination = FALSE;
    Context->Result->AddressIsValid = TRUE;
    snprintf(Context->Operand2,
             sizeof(Context->Operand2),
             "[0x%08llx]",
             OperandAddress);

    return;
}

VOID
DbgpThumb16DecodeLoadStoreSingle (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit LDR and STR single item instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    THUMB_DECODE_WITH_TABLE(Context, DbgThumb16LoadStoreSingleTable);
    return;
}

VOID
DbgpThumb16DecodeLoadStoreSingleRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit load and store (LDR and STR) single
    items from registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op;
    ULONG Rm;
    ULONG Rn;
    ULONG Rt;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_LOAD_STORE_REGISTER_OP_SHIFT) &
         THUMB16_LOAD_STORE_REGISTER_OP_MASK;

    Rm = (Instruction >> THUMB16_LOAD_STORE_REGISTER_RM_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rn = (Instruction >> THUMB16_LOAD_STORE_REGISTER_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rt = (Instruction >> THUMB16_LOAD_STORE_REGISTER_RT_SHIFT) &
         THUMB_REGISTER8_MASK;

    strcpy(Context->Mnemonic, DbgThumb16LoadStoreSingleRegisterMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    snprintf(Context->Operand2,
             sizeof(Context->Operand2),
             "[%s, %s]",
             DbgArmRegisterNames[Rn],
             DbgArmRegisterNames[Rm]);

    return;
}

VOID
DbgpThumb16DecodeLoadStoreSingleImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit load and store (LDR and STR) single
    items from immediates.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate5;
    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Op;
    ULONG Rn;
    ULONG Rt;

    Instruction = Context->Instruction;
    Immediate5 = (Instruction >> THUMB16_LOAD_STORE_IMMEDIATE5_SHIFT) &
                 THUMB_IMMEDIATE5_MASK;

    Immediate5 <<= 2;
    Rn = (Instruction >> THUMB16_LOAD_STORE_IMMEDIATE_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rt = (Instruction >> THUMB16_LOAD_STORE_IMMEDIATE_RT_SHIFT) &
         THUMB_REGISTER8_MASK;

    Op = (Instruction >> THUMB16_LOAD_STORE_IMMEDIATE_OP_SHIFT) &
         THUMB16_LOAD_STORE_IMMEDIATE_OP_MASK;

    //
    // Figure out the mnemonic. Check the higher level opcode mask to figure
    // out if it's a half-word load/store. If not, then it's a 32-bit or 8-bit
    // load or store.
    //

    if (Op == THUMB16_LOAD_STORE_IMMEDIATE_OP_HALF_WORD) {
        if ((Instruction & THUMB16_LOAD_BIT) != 0) {
            Mnemonic = THUMB_LDRH_MNEMONIC;

        } else {
            Mnemonic = THUMB_STRH_MNEMONIC;
        }

    } else {
        if ((Instruction & THUMB16_LOAD_BIT) != 0) {
            if ((Instruction & THUMB16_LOAD_STORE_BYTE) != 0) {
                Mnemonic = THUMB_LDRB_MNEMONIC;

            } else {
                Mnemonic = THUMB_LDR_MNEMONIC;
            }

        } else {
            if ((Instruction & THUMB16_LOAD_STORE_BYTE) != 0) {
                Mnemonic = THUMB_STRB_MNEMONIC;

            } else {
                Mnemonic = THUMB_STR_MNEMONIC;
            }
        }
    }

    strcpy(Context->Mnemonic, Mnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    if (Immediate5 != 0) {
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[%s, #%d]",
                 DbgArmRegisterNames[Rn],
                 Immediate5);

    } else {
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[%s]",
                 DbgArmRegisterNames[Rn]);
    }

    return;
}

VOID
DbgpThumb16DecodeLoadStoreSingleSpRelative (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit load and store (LDR and STR) from a
    stack pointer relative address.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;
    ULONG Rt;

    Instruction = Context->Instruction;
    Immediate8 = (Instruction >>
                  THUMB16_LOAD_STORE_SP_RELATIVE_IMMEDIATE8_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    Immediate8 <<= 2;
    Rt = (Instruction >> THUMB16_LOAD_STORE_SP_RELATIVE_IMMEDIATE8_SHIFT) &
         THUMB_REGISTER8_MASK;

    if ((Instruction & THUMB16_LOAD_BIT) != 0) {
        strcpy(Context->Mnemonic, THUMB_LDR_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_STR_MNEMONIC);
    }

    strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    if (Immediate8 != 0) {
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[sp, #%d]",
                 Immediate8);

    } else {
        strcpy(Context->Operand2, "[sp]");
    }

    return;
}

VOID
DbgpThumb16DecodeAdrAddSp (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit ADR instruction, which loads a
    PC-relative address. It also decodes the ADD (sp relative) instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    PSTR BaseMnemonic;
    ULONG Immediate8;
    ULONG Instruction;
    ULONGLONG OperandAddress;
    ULONG Rd;

    Instruction = Context->Instruction;
    Immediate8 = (Instruction >> THUMB16_ADR_IMMEDIATE8_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    Immediate8 <<= 2;
    Rd = (Instruction >> THUMB16_ADR_RD_SHIFT) & THUMB_REGISTER8_MASK;

    //
    // The second operand is either SP with an immediate or an absolute address
    // calculated from the PC aligned down to a 4-byte boundary.
    //

    if ((Instruction & THUMB16_ADR_SP) != 0) {
        BaseMnemonic = THUMB_ADD_MNEMONIC;
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "%s, #%d",
                 DbgArmRegisterNames[13],
                 Immediate8);

    } else {
        BaseMnemonic = THUMB_ADR_MNEMONIC;

        //
        // The label here is relative to the PC aligned down to 4-byte boundary.
        // On Thumb, the PC is always 4 bytes ahead of the instruction pointer.
        //

        OperandAddress = Context->InstructionPointer + 4;
        OperandAddress = THUMB_ALIGN_4(OperandAddress);
        OperandAddress += Immediate8;
        Context->Result->OperandAddress = OperandAddress;
        Context->Result->AddressIsDestination = FALSE;
        Context->Result->AddressIsValid = TRUE;
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[0x%08llx]",
                 OperandAddress);
    }

    strcpy(Context->Mnemonic, BaseMnemonic);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    return;
}

VOID
DbgpThumb16DecodeMiscellaneous (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit miscellaneous instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    THUMB_DECODE_WITH_TABLE(Context, DbgThumb16MiscellaneousTable);
    return;
}

VOID
DbgpThumb16DecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit load and store multiple (LDM and STM)
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG RegisterList;
    ULONG Rn;

    Instruction = Context->Instruction;
    Rn = (Instruction >> THUMB16_LOAD_STORE_MULTIPLE_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    if ((Instruction & THUMB16_LOAD_BIT) != 0) {
        strcpy(Context->Mnemonic, THUMB_LDM_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_STM_MNEMONIC);
    }

    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "%s!",
             DbgArmRegisterNames[Rn]);

    RegisterList = Instruction & THUMB_REGISTER8_LIST;
    DbgpArmDecodeRegisterList(Context->Operand2,
                              sizeof(Context->Operand2),
                              RegisterList);

    return;
}

VOID
DbgpThumb16DecodeConditionalBranchAndSvc (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit conditional branch, undefined
    instruction (UDF), and supervisor call (SVC, previously SWI).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    UCHAR Immediate8;
    ULONG Instruction;
    ULONG Op;
    ULONGLONG OperandAddress;
    LONG SignedImmediate8;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_BRANCH_SVC_OP_SHIFT) &
         THUMB16_BRANCH_SVC_OP_MASK;

    Immediate8 = Instruction & THUMB_IMMEDIATE8_MASK;
    SignedImmediate8 = (CHAR)Immediate8;
    SignedImmediate8 <<= 1;
    if (Op == THUMB16_BRANCH_SVC_OP_UDF) {
        strcpy(Context->Mnemonic, THUMB_UDF_MNEMONIC);
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "#%d",
                 Immediate8);

    } else if (Op == THUMB16_BRANCH_SVC_OP_SVC) {
        strcpy(Context->Mnemonic, THUMB_SVC_MNEMONIC);
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "#%d",
                 Immediate8);

    } else {
        snprintf(Context->Mnemonic,
                 sizeof(Context->Mnemonic),
                 "%s%s",
                 THUMB_B_MNEMONIC,
                 DbgArmConditionCodes[Op]);

        //
        // The destination address is relative to the PC value, which is always
        // 4 bytes ahead of the instruction pointer when in Thumb mode.
        //

        OperandAddress = Context->InstructionPointer + 4;
        OperandAddress += (LONGLONG)SignedImmediate8;
        Context->Result->OperandAddress = OperandAddress;
        Context->Result->AddressIsDestination = TRUE;
        Context->Result->AddressIsValid = TRUE;
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "[0x%08llx]",
                 OperandAddress);
    }

    return;
}

VOID
DbgpThumb16DecodeUnconditionalBranch (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit unconditional branch.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONGLONG OperandAddress;
    LONG SignedImmediate12;

    Instruction = Context->Instruction;
    SignedImmediate12 = Instruction & THUMB_IMMEDIATE11_MASK;
    SignedImmediate12 <<= 1;

    //
    // Sign extend the 12-bit immediate.
    //

    if ((SignedImmediate12 & (1 << 11)) != 0) {
        SignedImmediate12 |= 0xFFFFF000;
    }

    strcpy(Context->Mnemonic, THUMB_B_MNEMONIC);

    //
    // The destination address is relative to the PC value, which is always 4
    // bytes ahead of the instruction pointer on Thumb.
    //

    OperandAddress = Context->InstructionPointer + 4;
    OperandAddress += (LONGLONG)SignedImmediate12;
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
DbgpThumb16DecodeAdjustStackPointer (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit add immediate to and subtract
    immediate from the stack pointer instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate7;
    ULONG Instruction;

    Instruction = Context->Instruction;
    if ((Instruction & THUMB16_ADJUST_STACK_POINTER_SUBTRACT) != 0) {
        strcpy(Context->Mnemonic, THUMB_SUB_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_ADD_MNEMONIC);
    }

    Immediate7 = Instruction & THUMB_IMMEDIATE7_MASK;
    Immediate7 <<= 2;
    strcpy(Context->Operand1, "sp");
    snprintf(Context->Operand2,
             sizeof(Context->Operand2),
             "#%d",
             Immediate7);

    return;
}

VOID
DbgpThumb16DecodeCompareBranchIfZero (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit compare and branch if (or if not)
    zero (CBZ and CBNZ) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate6;
    ULONG Instruction;
    ULONGLONG OperandAddress;
    ULONG Rn;

    Instruction = Context->Instruction;
    if ((Instruction & THUMB16_COMPARE_BRANCH_NOT_ZERO) != 0) {
        strcpy(Context->Mnemonic, THUMB_CBNZ_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_CBZ_MNEMONIC);
    }

    Immediate6 = (Instruction >> THUMB16_COMPARE_BRANCH_ZERO_IMMEDIATE5_SHIFT) &
                 THUMB_IMMEDIATE5_MASK;

    if ((Instruction & THUMB16_COMPARE_BRANCH_ZERO_IMMEDIATE6) != 0) {
        Immediate6 |= 1 << 5;
    }

    Immediate6 <<= 1;
    Rn = (Instruction >> THUMB16_COMPARE_BRANCH_ZERO_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);

    //
    // The branch address is the immediate value added to the PC value of the
    // instruction. For Thumb, the PC is always 4 bytes ahead of the
    // instruction pointer.
    //

    OperandAddress = Context->InstructionPointer + 4;
    OperandAddress += Immediate6;
    Context->Result->OperandAddress = OperandAddress;
    Context->Result->AddressIsDestination = FALSE;
    Context->Result->AddressIsValid = TRUE;
    snprintf(Context->Operand2,
             sizeof(Context->Operand2),
             "[0x%08llx]",
             OperandAddress);

    return;
}

VOID
DbgpThumb16DecodeSignZeroExtend (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit sign extend and zero extend
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op;
    ULONG Rd;
    ULONG Rm;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_SIGN_ZERO_EXTEND_OP_SHIFT) &
         THUMB16_SIGN_ZERO_EXTEND_OP_MASK;

    Rd = (Instruction >> THUMB16_SIGN_ZERO_EXTEND_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rm = (Instruction >> THUMB16_SIGN_ZERO_EXTEND_RM_SHIFT) &
         THUMB_REGISTER8_MASK;

    strcpy(Context->Mnemonic, DbgThumb16SignZeroExtendMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpThumb16DecodePushPop (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit load and store multiple registers
    (PUSH and POP) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG RegisterList;

    Instruction = Context->Instruction;
    RegisterList = Instruction & THUMB_REGISTER8_LIST;
    if ((Instruction & THUMB16_LOAD_BIT) != 0) {
        strcpy(Context->Mnemonic, THUMB_POP_MNEMONIC);
        if ((Instruction & THUMB16_PUSH_POP_LINK_OR_PC) != 0) {
            RegisterList |= 1 << 15;
        }

    } else {
        strcpy(Context->Mnemonic, THUMB_PUSH_MNEMONIC);
        if ((Instruction & THUMB16_PUSH_POP_LINK_OR_PC) != 0) {
            RegisterList |= 1 << 14;
        }
    }

    DbgpArmDecodeRegisterList(Context->Operand1,
                              sizeof(Context->Operand1),
                              RegisterList);

    return;
}

VOID
DbgpThumb16DecodeSetEndianness (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit SETEND instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    strcpy(Context->Mnemonic, THUMB_SETEND_MNEMONIC);
    if ((Context->Instruction & THUMB16_SET_ENDIAN_BIG) != 0) {
        strcpy(Context->Operand1, THUMB16_BIG_ENDIAN_MNEMONIC);

    } else {
        strcpy(Context->Operand1, THUMB16_LITTLE_ENDIAN_MNEMONIC);
    }

    return;
}

VOID
DbgpThumb16DecodeChangeState (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit change processor state (CPS)
    instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;

    Instruction = Context->Instruction;
    if ((Instruction & THUMB16_CPS_DISABLE) != 0) {
        strcpy(Context->Mnemonic, THUMB_CPS_DISABLE_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_CPS_ENABLE_MNEMONIC);
    }

    strcpy(Context->Operand1, "");
    if ((Instruction & THUMB16_CPS_FLAG_A) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_A_STRING);
    }

    if ((Instruction & THUMB16_CPS_FLAG_I) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_I_STRING);
    }

    if ((Instruction & THUMB16_CPS_FLAG_F) != 0) {
        strcat(Context->Operand1, ARM_CPS_FLAG_F_STRING);
    }

    return;
}

VOID
DbgpThumb16DecodeReverseBytes (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit reverse bytes instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op;
    ULONG Rd;
    ULONG Rn;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB16_REVERSE_BYTES_OP_SHIFT) &
         THUMB16_REVERSE_BYTES_OP_MASK;

    Rd = (Instruction >> THUMB16_REVERSE_BYTES_RD_SHIFT) &
         THUMB_REGISTER8_MASK;

    Rn = (Instruction >> THUMB16_REVERSE_BYTES_RN_SHIFT) &
         THUMB_REGISTER8_MASK;

    strcpy(Context->Mnemonic, DbgThumb16ReverseBytesMnemonics[Op]);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    return;
}

VOID
DbgpThumb16DecodeBreakpoint (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit breakpoint instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;

    Instruction = Context->Instruction;
    Immediate8 = (Instruction >> THUMB16_BREAKPOINT_IMMEDIATE8_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    strcpy(Context->Mnemonic, THUMB_BKPT_MNEMONIC);
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "#%d",
             Immediate8);

    return;
}

VOID
DbgpThumb16DecodeIfThenAndHints (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the Thumb 16-bit if-then instruction, as well as
    the category ARM describes as "hints" (NOP, YIELD, WFE, WFI, and SEV).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Condition;
    ULONG Condition0;
    ULONG Instruction;
    ULONG Mask;
    ULONG Op;

    Instruction = Context->Instruction;
    Mask = Instruction & THUMB16_IF_THEN_MASK;
    Condition = (Instruction >> THUMB16_IF_THEN_CONDITION_SHIFT) &
                THUMB16_IF_THEN_CONDITION_MASK;

    //
    // If the mask is non-zero, then it's an if-then statement.
    //

    if (Mask != 0) {
        Condition0 = Condition & 0x1;
        snprintf(Context->Mnemonic,
                 sizeof(Context->Mnemonic),
                 "%s%s",
                 THUMB_IT_MNEMONIC,
                 DbgThumb16IfThenSuffixes[Condition0][Mask]);

        strcpy(Context->Operand1, DbgArmConditionCodes[Condition]);

    } else {
        Op = (Instruction >> THUMB16_HINTS_OP_SHIFT) &
             THUMB16_HINTS_OP_MASK;

        strcpy(Context->Mnemonic, DbgThumb16HintsMnemonics[Op]);
    }

    return;
}

