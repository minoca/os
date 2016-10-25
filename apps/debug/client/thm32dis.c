/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    thm32dis.c

Abstract:

    This module implements support for disassembling 32-bit Thumb-2
    instructions.

Author:

    Evan Green 27-Apr-2014

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
DbgpThumb32DecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLoadStoreDualExclusive (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLdrexStrex (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLdrdStrd (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLoadStoreExclusiveFunkySize (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeTableBranch (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeDataProcessingShiftedRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeCoprocessorSimdFloatingPoint (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeUndefined (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeSimdDataProcessing (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeDataModifiedImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeDataPlainImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeBranchAndMiscellaneous (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeMsr (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeCpsAndHints (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeMiscellaneousControl (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeBxj (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeExceptionReturn (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeMrs (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeHvc (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeSmc (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeBranch (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeUdf (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeBranchWithLink (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLoadStoreSingleItem (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLoadStoreImmediate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLoadStoreRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeDataProcessingRegister (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeMultiplyAccumulate (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumb32DecodeLongMultiplyDivide (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpThumbDecodeImmediateShift (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG Register,
    ULONG Type,
    ULONG Immediate
    );

ULONG
DbgpThumb32DecodeModifiedImmediate (
    ULONG Immediate12
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Define mnemonic tables.
//

PSTR DbgThumb32DataProcessingMnemonics[2][16] = {
    {
        "and.w",
        "bic.w",
        "orr.w",
        "orn.w",
        "eor.w",
        "",
        "",
        "",
        "add.w",
        "",
        "adc.w",
        "sbc.w",
        "",
        "sub.w",
        "rsb.w",
        "",
    },
    {
        "ands.w",
        "bics.w",
        "orrs.w",
        "orns.w",
        "eors.w",
        "",
        "",
        "",
        "adds.w",
        "",
        "adcs.w",
        "sbcs.w",
        "",
        "subs.w",
        "rsbs.w",
        "",
    },
};

PSTR DbgThumb32DataProcessingShiftMnemonics[2][5] = {
    {
        "lsl.w",
        "lsr.w",
        "asr.w",
        "ror.w",
        "rrx.w"
    },
    {
        "lsls.w",
        "lsrs.w",
        "asrs.w",
        "rors.w",
        "rrxs.w"
    }
};

PSTR DbgThumb32MovMnemonics[2] = {
    "mov.w",
    "movs.w"
};

PSTR DbgThumb32MvnwMnemonics[2] = {
    "mvn.w",
    "mvns.w"
};

PSTR DbgThumb32HintMnemonics[] = {
    "nop.w",
    "yield.w",
    "wfe.w",
    "wfi.w",
    "sev.w"
};

PSTR DbgThumb32LoadStoreMnemonics[2][4] = {
    {
        "strb.w",
        "strh.w",
        "str.w",
        "Undef str.w"
    },
    {
        "ldrb.w",
        "ldrh.w",
        "ldr.w",
        "Undef ldr.w"
    }
};

PSTR DbgThumb32LoadSetFlagsMnemonics[4] = {
    "ldsrb.w",
    "ldrsh.w",
    "ldrs.w",
    "Undef ldrs.w"
};

PSTR DbgThumb32LoadStoreUnprivilegedMnemonics[2][4] = {
    {
        "strbt",
        "strht",
        "strt",
        "Undef strt"
    },
    {
        "ldrbt",
        "ldrht",
        "ldrt",
        "Undef ldrt"
    }
};

PSTR DbgThumb32LoadSetFlagsUnprivilegedMnemonics[4] = {
    "ldrsbt",
    "ldrsht",
    "ldrst",
    "Undef ldrst"
};

PSTR DbgThumb32PreloadMnemonics[4] = {
    "pli",
    "pldw",
    "pld",
    "Undef pld"
};

PSTR DbgThumb32ExtendAndAddMnemonics[2][6] = {
    {
        "sxtah",
        "uxtah",
        "sxtab16",
        "uxtab16",
        "sxtab",
        "uxtab"
    },
    {
        "sxth",
        "uxth",
        "sxtb16",
        "uxtb16",
        "sxtb",
        "uxtb"
    }
};

PSTR DbgThumb32ParallelArithmeticMnemonics[2][24] = {
    {
        "sadd8",
        "sadd16",
        "sasx",
        "",
        "ssub8",
        "ssub16",
        "ssax",
        "",
        "qadd8",
        "qadd16",
        "qasx",
        "",
        "qsub8",
        "qsub16",
        "qsax",
        "",
        "shadd8",
        "shadd16",
        "shasx",
        "",
        "shsub8",
        "shsub16",
        "shsax",
        "",
    },
    {
        "uadd8",
        "uadd16",
        "uasx",
        "",
        "usub8",
        "usub16",
        "usax",
        "",
        "uqadd8",
        "uqadd16",
        "uqasx",
        "",
        "uqsub8",
        "uqsub16",
        "uqsax",
        "",
        "uhadd8",
        "uhadd16",
        "uhasx",
        "",
        "uhsub8",
        "uhsub16",
        "uhsax",
        "",
    },
};

PSTR DbgThumb32DataProcessingMiscellaneousMnemonics[] = {
    "qadd",
    "qdadd",
    "qsub",
    "qdsub",
    "rev.w",
    "rev16.w",
    "rbit",
    "revsh.w",
    "sel",
    "",
    "",
    "",
    "clz",
    "",
    "",
    ""
};

PSTR DbgThumb32MultiplyMnemonics[2][8] = {
    {
        "mla",
        "smla",
        "smlad",
        "smlaw",
        "smlsd",
        "smmla",
        "smmls",
        "usada8"
    },
    {
        "mul",
        "smul",
        "smuad",
        "smulw",
        "smusd",
        "smmul",
        "smmls",
        "usad8"
    }
};

PSTR DbgThumb32MultiplyTopBottomMnemonics[2] = {
    "b",
    "t",
};

PSTR DbgThumb32LongMultiplyMnemonics[8] = {
    "smull",
    "sdiv",
    "umull",
    "udiv",
    "smlal",
    "smlsld",
    "umlal",
    ""
};

//
// Define decode tables.
//

THUMB_DECODE_BRANCH DbgThumb32TopLevelTable[] = {
    {0x1E400000, 0x08000000, 0, DbgpThumb32DecodeLoadStoreMultiple},
    {0x1E400000, 0x08400000, 0, DbgpThumb32DecodeLoadStoreDualExclusive},
    {0x1E000000, 0x0A000000, 0, DbgpThumb32DecodeDataProcessingShiftedRegister},
    {0x1C000000, 0x0C000000, 0, DbgpThumb32DecodeCoprocessorSimdFloatingPoint},
    {0x1A008000, 0x10000000, 0, DbgpThumb32DecodeDataModifiedImmediate},
    {0x1A008000, 0x12000000, 0, DbgpThumb32DecodeDataPlainImmediate},
    {0x18008000, 0x10008000, 0, DbgpThumb32DecodeBranchAndMiscellaneous},
    {0x1F100000, 0x18000000, 0, DbgpThumb32DecodeLoadStoreSingleItem},
    {0x1E700000, 0x18100000, 0, DbgpThumb32DecodeLoadStoreSingleItem},
    {0x1E700000, 0x18300000, 0, DbgpThumb32DecodeLoadStoreSingleItem},
    {0x1E700000, 0x18500000, 0, DbgpThumb32DecodeLoadStoreSingleItem},
    {0x1E700000, 0x18700000, 0, DbgpThumb32DecodeUndefined},
    {0x1F100000, 0x19000000, 0, DbgpArmDecodeSimdElementLoadStore},
    {0x1F000000, 0x1A000000, 0, DbgpThumb32DecodeDataProcessingRegister},
    {0x1F800000, 0x1B000000, 0, DbgpThumb32DecodeMultiplyAccumulate},
    {0x1F800000, 0x1B800000, 0, DbgpThumb32DecodeLongMultiplyDivide},
    {0x1C000000, 0x1C000000, 0, DbgpThumb32DecodeCoprocessorSimdFloatingPoint},
};

THUMB_DECODE_BRANCH DbgThumb32LoadStoreDualExclusiveTable[] = {
    {0x01B00000, 0x00000000, 0, DbgpThumb32DecodeLdrexStrex},
    {0x01B00000, 0x00100000, 0, DbgpThumb32DecodeLdrexStrex},
    {0x01300000, 0x00200000, 0, DbgpThumb32DecodeLdrdStrd},
    {0x01100000, 0x01000000, 0, DbgpThumb32DecodeLdrdStrd},
    {0x01300000, 0x00300000, 0, DbgpThumb32DecodeLdrdStrd},
    {0x01100000, 0x01100000, 0, DbgpThumb32DecodeLdrdStrd},
    {0x01B000F0, 0x00800040, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
    {0x01B000F0, 0x00800050, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
    {0x01B000F0, 0x00800070, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
    {0x01B000F0, 0x00900000, 0, DbgpThumb32DecodeTableBranch},
    {0x01B000F0, 0x00900010, 0, DbgpThumb32DecodeTableBranch},
    {0x01B000F0, 0x00900040, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
    {0x01B000F0, 0x00900050, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
    {0x01B000F0, 0x00900070, 0, DbgpThumb32DecodeLoadStoreExclusiveFunkySize},
};

THUMB_DECODE_BRANCH DbgThumb32CoprocessorSimdFloatingPointTable[] = {
    {0x03E00000, 0x00000000, 0, DbgpThumb32DecodeUndefined},
    {0x03000000, 0x03000000, 0, DbgpThumb32DecodeSimdDataProcessing},
    {0x03E00E00, 0x00400A00, 0, DbgpArmDecodeSimd64BitTransfers},
    {0x02000E00, 0x00000A00, 0, DbgpArmDecodeSimdLoadStore},
    {0x03000E10, 0x02000A00, 0, DbgpArmDecodeFloatingPoint},
    {0x03000E10, 0x02000A10, 0, DbgpArmDecodeSimdSmallTransfers},
    {0x03E00000, 0x00400000, 0, DbgpArmDecodeCoprocessorMoveTwo},
    {0x02000000, 0x00000000, 0, DbgpArmDecodeCoprocessorLoadStore},
    {0x03000010, 0x02000000, 0, DbgpArmDecodeCoprocessorMove},
    {0x03000010, 0x02000010, 0, DbgpArmDecodeCoprocessorMove},
};

THUMB_DECODE_BRANCH DbgThumb32BranchAndMiscellaneousTable[] = {
    {0x07E05000, 0x03800000, 0, DbgpThumb32DecodeMsr},
    {0x07F05000, 0x03A00000, 0, DbgpThumb32DecodeCpsAndHints},
    {0x07F05000, 0x03B00000, 0, DbgpThumb32DecodeMiscellaneousControl},
    {0x07F05000, 0x03C00000, 0, DbgpThumb32DecodeBxj},
    {0x07F05000, 0x03D00000, 0, DbgpThumb32DecodeExceptionReturn},
    {0x07E05000, 0x03E00000, 0, DbgpThumb32DecodeMrs},
    {0x07F07000, 0x07E00000, 0, DbgpThumb32DecodeHvc},
    {0x07F07000, 0x07F00000, 0, DbgpThumb32DecodeSmc},
    {0x00005000, 0x00001000, 0, DbgpThumb32DecodeBranch},
    {0x00005000, 0x00000000, 0, DbgpThumb32DecodeBranch},
    {0x07F07000, 0x07F02000, 0, DbgpThumb32DecodeUdf},
    {0x00004000, 0x00004000, 0, DbgpThumb32DecodeBranchWithLink},
};

//
// ------------------------------------------------------------------ Functions
//

VOID
DbgpThumb32Decode (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit portion of the Thumb-2 instruction set.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    //
    // Swap the half words, then decode using the table.
    //

    Context->Instruction = ((Context->Instruction << 16) & 0xFFFF0000) |
                           ((Context->Instruction >> 16) & 0x0000FFFF);

    THUMB_DECODE_WITH_TABLE(Context, DbgThumb32TopLevelTable);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpThumb32DecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store multiple instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Mode;
    ULONG Op;
    ULONG RegisterList;
    ULONG Rn;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB32_LOAD_STORE_MULTIPLE_OP_SHIFT) &
         THUMB32_LOAD_STORE_MULTIPLE_OP_MASK;

    Rn = (Instruction >> THUMB32_LOAD_STORE_MULTIPLE_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    //
    // The instruction is either rfe/srs, or ldm/stm.
    //

    switch (Op) {
    case THUMB32_LOAD_STORE_RETURN_STATE_OP:
    case THUMB32_LOAD_STORE_RETURN_STATE_OP2:
        Mode = Instruction & THUMB32_LOAD_STORE_MODE_MASK;
        if ((Instruction & THUMB32_LOAD_BIT) != 0) {
            strcpy(Context->Mnemonic, THUMB_RFE_MNEMONIC);

        } else {
            strcpy(Context->Mnemonic, THUMB_SRS_MNEMONIC);
            DbgpArmPrintMode(Context->Operand2, Mode);
        }

        break;

    case THUMB32_LOAD_STORE_MULTIPLE_OP:
    case THUMB32_LOAD_STORE_MULTIPLE_OP2:
    default:
        if ((Instruction & THUMB32_LOAD_BIT) != 0) {
            strcpy(Context->Mnemonic, THUMB_LDM_MNEMONIC);

        } else {
            strcpy(Context->Mnemonic, THUMB_STM_MNEMONIC);
        }

        RegisterList = Instruction & THUMB_REGISTER16_LIST;
        DbgpArmDecodeRegisterList(Context->Operand2,
                                  sizeof(Context->Operand2),
                                  RegisterList);

        break;
    }

    //
    // Add the decrement-before or increment-after suffix.
    //

    if ((Instruction & THUMB32_LOAD_STORE_INCREMENT) != 0) {
        strcat(Context->Mnemonic, THUMB_IA_SUFFIX);

    } else {
        strcat(Context->Mnemonic, THUMB_DB_SUFFIX);
    }

    //
    // Print operand one, the register.
    //

    if ((Instruction & THUMB32_LOAD_STORE_MULTIPLE_WRITE_BACK_BIT) != 0) {
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "%s!",
                 DbgArmRegisterNames[Rn]);

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
    }

    return;
}

VOID
DbgpThumb32DecodeLoadStoreDualExclusive (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store dual, load/store exclusive,
    and table branch instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    THUMB_DECODE_WITH_TABLE(Context, DbgThumb32LoadStoreDualExclusiveTable);
    return;
}

VOID
DbgpThumb32DecodeLdrexStrex (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store exclusive (32-bit data)
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;
    ULONG Rd;
    ULONG Rn;
    PSTR RnOperand;
    ULONG Rt;
    PSTR RtOperand;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB32_EXCLUSIVE_RD_SHIFT) & THUMB_REGISTER16_MASK;
    Rn = (Instruction >> THUMB32_EXCLUSIVE_RN_SHIFT) & THUMB_REGISTER16_MASK;
    Rt = (Instruction >> THUMB32_EXCLUSIVE_RT_SHIFT) & THUMB_REGISTER16_MASK;
    Immediate8 = (Instruction >> THUMB32_EXCLUSIVE_IMMEDIATE8_SHIFT) &
                 THUMB_IMMEDIATE8_MASK;

    Immediate8 <<= 2;
    if ((Instruction & THUMB32_LOAD_BIT) != 0) {
        strcpy(Context->Mnemonic, THUMB_LDREX_MNEMONIC);
        RtOperand = &(Context->Operand1[0]);
        RnOperand = &(Context->Operand2[0]);

    } else {
        strcpy(Context->Mnemonic, THUMB_STREX_MNEMONIC);
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        RtOperand = &(Context->Operand2[0]);
        RnOperand = &(Context->Operand3[0]);
    }

    strcpy(RtOperand, DbgArmRegisterNames[Rt]);
    if (Immediate8 == 0) {
        sprintf(RnOperand, "[%s]", DbgArmRegisterNames[Rn]);

    } else {
        sprintf(RnOperand,
                "[%s, #%d]",
                DbgArmRegisterNames[Rn],
                Immediate8 * 4);
    }

    return;
}

VOID
DbgpThumb32DecodeLdrdStrd (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store dual (64-bit data).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;
    ULONG Instruction;
    ULONG Rn;
    ULONG Rt;
    ULONG Rt2;

    Instruction = Context->Instruction;
    if ((Instruction & THUMB32_LOAD_BIT) != 0) {
        strcpy(Context->Mnemonic, THUMB_LDRD_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_STRD_MNEMONIC);
    }

    Rn = (Instruction >> THUMB32_DUAL_RN_SHIFT) & THUMB_REGISTER16_MASK;
    Rt = (Instruction >> THUMB32_DUAL_RT_SHIFT) & THUMB_REGISTER16_MASK;
    Rt2 = (Instruction >> THUMB32_DUAL_RT2_SHIFT) & THUMB_REGISTER16_MASK;
    Immediate8 = Instruction & THUMB_IMMEDIATE8_MASK;
    Immediate8 <<= 2;
    strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rt2]);
    if ((Instruction & THUMB32_PREINDEX_BIT) != 0) {
        if ((Instruction & THUMB32_WRITE_BACK_BIT) != 0) {
            snprintf(Context->Operand3,
                     sizeof(Context->Operand3),
                     "[%s, #%d]!",
                     DbgArmRegisterNames[Rn],
                     Immediate8);

        } else {
            if (Immediate8 != 0) {
                snprintf(Context->Operand3,
                         sizeof(Context->Operand3),
                         "[%s, #%d]",
                         DbgArmRegisterNames[Rn],
                         Immediate8);

            } else {
                snprintf(Context->Operand3,
                         sizeof(Context->Operand3),
                         "[%s]",
                         DbgArmRegisterNames[Rn]);
            }
        }

    //
    // If pre-index is not set, then update is assumed to be set.
    //

    } else {
        snprintf(Context->Operand3,
                 sizeof(Context->Operand3),
                 "[%s] #%d",
                 DbgArmRegisterNames[Rn],
                 Immediate8);
    }

    return;
}

VOID
DbgpThumb32DecodeLoadStoreExclusiveFunkySize (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store exclusive instructions for
    non-native sizes (8, 16 and 64 bits).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Op;
    ULONG Rd;
    ULONG Rn;
    PSTR RnRegister;
    ULONG Rt;
    ULONG Rt2;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB32_EXCLUSIVE_FUNKY_RD_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_EXCLUSIVE_FUNKY_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rt = (Instruction >> THUMB32_EXCLUSIVE_FUNKY_RT_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rt2 = (Instruction >> THUMB32_EXCLUSIVE_FUNKY_RT2_SHIFT) &
          THUMB_REGISTER16_MASK;

    //
    // Get the mnemonic. Load instructions look like ldr Rt, [Rn]. Store
    // instructions look like str Rd, Rt, [Rn]. Dual instructions stick Rt2
    // after Rt.
    //

    Op = (Instruction >> THUMB32_EXCLUSIVE_FUNKY_OP_SHIFT) &
         THUMB32_EXCLUSIVE_FUNKY_OP_MASK;

    if ((Instruction & THUMB32_LOAD_BIT) != 0) {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
        RnRegister = &(Context->Operand2[0]);
        if (Op == THUMB32_EXCLUSIVE_FUNKY_OP_BYTE) {
            Mnemonic = THUMB_LDREXB_MNEMONIC;

        } else if (Op == THUMB32_EXCLUSIVE_FUNKY_OP_HALF_WORD) {
            Mnemonic = THUMB_LDREXH_MNEMONIC;

        } else {

            assert(Op == THUMB32_EXCLUSIVE_FUNKY_OP_DUAL);

            Mnemonic = THUMB_LDREXD_MNEMONIC;
            strcpy(Context->Operand2, DbgArmRegisterNames[Rt2]);
            RnRegister = &(Context->Operand3[0]);
        }

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rt]);
        RnRegister = &(Context->Operand3[0]);
        if (Op == THUMB32_EXCLUSIVE_FUNKY_OP_BYTE) {
            Mnemonic = THUMB_STREXB_MNEMONIC;

        } else if (Op == THUMB32_EXCLUSIVE_FUNKY_OP_HALF_WORD) {
            Mnemonic = THUMB_STREXH_MNEMONIC;

        } else {

            assert(Op == THUMB32_EXCLUSIVE_FUNKY_OP_DUAL);

            Mnemonic = THUMB_STREXD_MNEMONIC;
            strcpy(Context->Operand3, DbgArmRegisterNames[Rt2]);
            RnRegister = &(Context->Operand4[0]);
        }
    }

    strcpy(Context->Mnemonic, Mnemonic);
    sprintf(RnRegister, "[%s]", DbgArmRegisterNames[Rn]);
    return;
}

VOID
DbgpThumb32DecodeTableBranch (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit load/store exclusive instructions for
    non-native sizes (8, 16 and 64 bits).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Rm;
    ULONG Rn;

    Instruction = Context->Instruction;
    Rm = (Instruction >> THUMB32_TABLE_BRANCH_RM_SHIFT) & THUMB_REGISTER16_MASK;
    Rn = (Instruction >> THUMB32_TABLE_BRANCH_RN_SHIFT) & THUMB_REGISTER16_MASK;
    if ((Instruction & THUMB32_TABLE_BRANCH_HALF_WORD) != 0) {
        strcpy(Context->Mnemonic, THUMB_TBH_MNEMONIC);
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "[%s, %s, lsl #1]",
                 DbgArmRegisterNames[Rn],
                 DbgArmRegisterNames[Rm]);

    } else {
        strcpy(Context->Mnemonic, THUMB_TBB_MNEMONIC);
        snprintf(Context->Operand1,
                 sizeof(Context->Operand1),
                 "[%s, %s]",
                 DbgArmRegisterNames[Rn],
                 DbgArmRegisterNames[Rm]);

    }

    return;
}

VOID
DbgpThumb32DecodeDataProcessingShiftedRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the 32-bit data processing (shifted register)
    instructions.

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
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;
    ULONG SetFlags;
    BOOL StandardParameters;
    ULONG Type;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB32_DATA_SHIFTED_REGISTER_RD_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rm = (Instruction >> THUMB32_DATA_SHIFTED_REGISTER_RM_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_DATA_SHIFTED_REGISTER_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Type = (Instruction >> THUMB32_DATA_SHIFTED_REGISTER_TYPE_SHIFT) &
           THUMB32_DATA_SHIFTED_REGISTER_TYPE_MASK;

    Immediate5 = (Instruction >>
                  THUMB32_DATA_SHIFTED_REGISTER_IMMEDIATE2_SHIFT) &
                 THUMB32_DATA_SHIFTED_REGISTER_IMMEDIATE2_MASK;

    Immediate5 |= ((Instruction >>
                    THUMB32_DATA_SHIFTED_REGISTER_IMMEDIATE3_SHIFT) &
                   THUMB32_DATA_SHIFTED_REGISTER_IMMEDIATE3_MASK) << 2;

    SetFlags = Instruction & THUMB32_DATA_SET_FLAGS;
    Op = (Instruction >> THUMB32_DATA_SHIFTED_REGISTER_OP_SHIFT) &
         THUMB32_DATA_SHIFTED_REGISTER_OP_MASK;

    StandardParameters = TRUE;
    if (SetFlags != 0) {
        SetFlags = 1;
    }

    //
    // This decoding follows a standard pattern, but there are several
    // exceptions that kick in when 1111 is specified for one of the registers.
    // The exceptions are listed below in this switch statement.
    //

    Mnemonic = DbgThumb32DataProcessingMnemonics[SetFlags][Op];
    switch (Op) {
    case THUMB32_DATA_AND:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_TST_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            DbgpThumbDecodeImmediateShift(Context->Operand2,
                                          sizeof(Context->Operand2),
                                          Rm,
                                          Type,
                                          Immediate5);
        }

        break;

    case THUMB32_DATA_ORR:
        if (Rn == 0xF) {
            StandardParameters = FALSE;
            Mnemonic = DbgThumb32DataProcessingShiftMnemonics[SetFlags][Type];
            switch (Type) {
            case THUMB_SHIFT_TYPE_LSL:
                if (Immediate5 == 0) {
                    Mnemonic = DbgThumb32MovMnemonics[SetFlags];
                }

                break;

            case THUMB_SHIFT_TYPE_ROR:
                if (Immediate5 == 0) {
                    Type += 1;
                    Mnemonic =
                        DbgThumb32DataProcessingShiftMnemonics[SetFlags][Type];

                }

                break;

            default:
                break;
            }

            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
            if (Immediate5 != 0) {
                snprintf(Context->Operand3,
                         sizeof(Context->Operand3),
                         "#%d",
                         Immediate5);
            }
        }

        break;

    case THUMB32_DATA_ORN:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = DbgThumb32MvnwMnemonics[SetFlags];
            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            DbgpThumbDecodeImmediateShift(Context->Operand2,
                                          sizeof(Context->Operand2),
                                          Rm,
                                          Type,
                                          Immediate5);
        }

        break;

    case THUMB32_DATA_EOR:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_TEQ_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            DbgpThumbDecodeImmediateShift(Context->Operand2,
                                          sizeof(Context->Operand2),
                                          Rm,
                                          Type,
                                          Immediate5);
        }

        break;

    case THUMB32_DATA_PKH:
        Type &= ~0x1;
        Mnemonic = THUMB_PKHBT_MNEMONIC;
        if ((Instruction & THUMB32_PACK_HALF_WORD_TB) != 0) {
            Mnemonic = THUMB_PKHTB_MNEMONIC;
        }

        break;

    case THUMB32_DATA_ADD:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_CMN_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            DbgpThumbDecodeImmediateShift(Context->Operand2,
                                          sizeof(Context->Operand2),
                                          Rm,
                                          Type,
                                          Immediate5);
        }

        break;

    case THUMB32_DATA_SUB:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_CMP_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            DbgpThumbDecodeImmediateShift(Context->Operand2,
                                          sizeof(Context->Operand2),
                                          Rm,
                                          Type,
                                          Immediate5);
        }

        break;

    default:
        break;
    }

    strcpy(Context->Mnemonic, Mnemonic);

    //
    // If the switch statement didn't apply, copy in the regular parameters.
    // The pack half-word is a special case, it changed the opcode but still
    // follows the standard parameters.
    //

    if (StandardParameters != FALSE) {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        DbgpThumbDecodeImmediateShift(Context->Operand3,
                                      sizeof(Context->Operand3),
                                      Rm,
                                      Type,
                                      Immediate5);
    }

    return;
}

VOID
DbgpThumb32DecodeCoprocessorSimdFloatingPoint (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes coprocessor, advanced SIMD, and floating point
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    THUMB_DECODE_WITH_TABLE(Context,
                            DbgThumb32CoprocessorSimdFloatingPointTable);

    return;
}

VOID
DbgpThumb32DecodeUndefined (
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
DbgpThumb32DecodeSimdDataProcessing (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes SIMD data processing instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;

    //
    // The 32-bit Thumb instruction and the ARM instruction only differ by one
    // bit. Move the bit in ths 32-bit Thumb instruction and use the ARM
    // decoder.
    //

    Instruction = Context->Instruction;
    if ((Instruction & THUMB32_SIMD_DATA_PROCESSING_UNSIGNED) != 0) {
        Context->Instruction |= ARM_SIMD_DATA_PROCESSING_UNSIGNED;

    } else {
        Context->Instruction &= ~ARM_SIMD_DATA_PROCESSING_UNSIGNED;
    }

    DbgpArmDecodeSimdDataProcessing(Context);
    Context->Instruction = Instruction;
    return;
}

VOID
DbgpThumb32DecodeDataModifiedImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes data processing (modified immediate) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate12;
    ULONG Instruction;
    PSTR Mnemonic;
    ULONG ModifiedImmediate;
    ULONG Op;
    ULONG Rd;
    ULONG Rn;
    ULONG SetFlags;
    BOOL StandardParameters;

    Instruction = Context->Instruction;
    Immediate12 = (Instruction >>
                   THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE8_SHIFT) &
                  THUMB_IMMEDIATE8_MASK;

    Immediate12 |= ((Instruction >>
                     THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE3_SHIFT) &
                    THUMB_IMMEDIATE3_MASK) << 8;

    if ((Instruction & THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE12) != 0) {
        Immediate12 |= 1 << 11;
    }

    Rd = (Instruction >> THUMB32_DATA_MODIFIED_IMMEDIATE_RD_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_DATA_MODIFIED_IMMEDIATE_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    SetFlags = 0;
    if ((Instruction & THUMB32_DATA_SET_FLAGS) != 0) {
        SetFlags = 1;
    }

    Op = (Instruction >> THUMB32_DATA_MODIFIED_IMMEDIATE_OP_SHIFT) &
         THUMB32_DATA_MODIFIED_IMMEDIATE_OP_MASK;

    ModifiedImmediate = DbgpThumb32DecodeModifiedImmediate(Immediate12);
    StandardParameters = TRUE;
    Mnemonic = DbgThumb32DataProcessingMnemonics[SetFlags][Op];
    switch (Op) {
    case THUMB32_DATA_AND:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_TST_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    case THUMB32_DATA_ORR:
        if (Rn == 0xF) {
            StandardParameters = FALSE;
            Mnemonic = DbgThumb32MovMnemonics[SetFlags];
            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    case THUMB32_DATA_ORN:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = DbgThumb32MvnwMnemonics[SetFlags];
            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    case THUMB32_DATA_EOR:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_TEQ_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    case THUMB32_DATA_ADD:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_CMN_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    case THUMB32_DATA_SUB:
        if ((Rd == 0xF) && (SetFlags != 0)) {
            StandardParameters = FALSE;
            Mnemonic = THUMB_CMP_W_MNEMONIC;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "#%d",
                     ModifiedImmediate);
        }

        break;

    default:
        break;
    }

    strcpy(Context->Mnemonic, Mnemonic);

    //
    // If the switch statement didn't apply, copy in the regular parameters.
    // The pack half-word is a special case, it changed the opcode but still
    // follows the standard parameters.
    //

    if (StandardParameters != FALSE) {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        snprintf(Context->Operand3,
                 sizeof(Context->Operand3),
                 "#%d",
                 ModifiedImmediate);
    }

    return;
}

VOID
DbgpThumb32DecodeDataPlainImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes data processing (plain Jane immediate) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate;
    ULONG Immediate12;
    ULONG Immediate3;
    ULONG Immediate5;
    ULONG Instruction;
    PSTR LsbString;
    PSTR Mnemonic;
    PSTR *Mnemonics;
    ULONG Op;
    ULONGLONG OperandAddress;
    ULONG Rd;
    ULONG Rn;
    ULONG SetFlags;
    PSTR ShiftMnemonic;
    LONG SignedImmediate;
    ULONG Width;
    PSTR WidthString;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB32_DATA_PLAIN_IMMEDIATE_RD_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_DATA_PLAIN_IMMEDIATE_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Op = (Instruction >> THUMB32_DATA_PLAIN_IMMEDIATE_OP_SHIFT) &
         THUMB32_DATA_PLAIN_IMMEDIATE_OP_MASK;

    Immediate3 = (Instruction >>
                  THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE3_SHIFT) &
                 THUMB_IMMEDIATE3_MASK;

    Immediate5 = (Instruction >>
                  THUMB32_DATA_PLAIN_IMMEDIATE_IMMEDIATE2_SHIFT) &
                 THUMB_IMMEDIATE2_MASK;

    Immediate5 |= Immediate3 << 2;
    Immediate12 = (Instruction >>
                   THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE8_SHIFT) &
                  THUMB_IMMEDIATE8_MASK;

    Immediate12 |= Immediate3 << 8;
    if ((Instruction & THUMB32_DATA_MODIFIED_IMMEDIATE_IMMEDIATE12) != 0) {
        Immediate12 |= 1 << 11;
    }

    SetFlags = 0;
    if ((Instruction & THUMB32_DATA_SET_FLAGS) != 0) {
        SetFlags = 1;
    }

    Mnemonic = "Unknown thumb.";
    switch (Op) {
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_ADD:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_SUB:
        if (Rn == 0xF) {
            Mnemonic = THUMB_ADR_W_MNEMONIC;
            SignedImmediate = Immediate12;
            strcpy(Context->Operand1, DbgArmRegisterNames[Rn]);
            if (Op == THUMB32_DATA_PLAIN_IMMEDIATE_OP_SUB) {
                SignedImmediate = -SignedImmediate;
            }

            //
            // Calculate the operand address. The immediate is relative to the
            // current PC aligned down to a four-byte boundary.
            //

            OperandAddress = Context->InstructionPointer + 4;
            OperandAddress = THUMB_ALIGN_4(OperandAddress);
            OperandAddress += (LONGLONG)SignedImmediate;
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "[0x%08llx]",
                     OperandAddress);

            Context->Result->OperandAddress = OperandAddress;
            Context->Result->AddressIsDestination = FALSE;
            Context->Result->AddressIsValid = TRUE;

        } else {
            Mnemonics = DbgThumb32DataProcessingMnemonics[SetFlags];
            if (Op == THUMB32_DATA_PLAIN_IMMEDIATE_OP_ADD) {
                Mnemonic = Mnemonics[THUMB32_DATA_ADD];

            } else {
                Mnemonic = Mnemonics[THUMB32_DATA_SUB];
            }

            strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
            strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
            snprintf(Context->Operand3,
                     sizeof(Context->Operand3),
                     "#%d",
                     Immediate12);
        }

        break;

    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_MOV:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_MOVT:
        if (Op == THUMB32_DATA_PLAIN_IMMEDIATE_OP_MOV) {
            Mnemonic = THUMB_MOVW_MNEMONIC;

        } else {
            Mnemonic = THUMB_MOVT_MNEMONIC;
        }

        Immediate = Immediate12 |
                    (((Instruction >>
                       THUMB32_DATA_PLAIN_IMMEDIATE_IMMEDIATE4_SHIFT) &
                      THUMB_IMMEDIATE4_MASK) << 12);

        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "#%d",
                 Immediate);

        break;

    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_SSAT:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_SSAT16:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_USAT:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_USAT16:
        Immediate = (Instruction >>
                     THUMB32_DATA_PLAIN_IMMEDIATE_SAT_IMMEDIATE_SHIFT);

        if (Immediate5 == 0) {
            Immediate &= THUMB32_DATA_PLAIN_IMMEDIATE_SAT_IMMEDIATE4_MASK;

        } else {
            Immediate &= THUMB32_DATA_PLAIN_IMMEDIATE_SAT_IMMEDIATE5_MASK;
        }

        if ((Instruction & THUMB32_DATA_PLAIN_IMMEDIATE_UNSIGNED) != 0) {
            if (Immediate5 == 0) {
                Mnemonic = THUMB_USAT16_MNEMONIC;

            } else {
                Mnemonic = THUMB_USAT_MNEMONIC;
            }

        } else {
            if (Immediate5 == 0) {
                Mnemonic = THUMB_SSAT16_MNEMONIC;

            } else {
                Mnemonic = THUMB_SSAT_MNEMONIC;
            }

            Immediate += 1;
        }

        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        sprintf(Context->Operand2, "#%d", Immediate);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rn]);
        if (Immediate5 != 0) {
            ShiftMnemonic = ARM_LSL_MNEMONIC;
            if ((Instruction & THUMB32_DATA_PLAIN_IMMEDIATE_SHIFT_RIGHT) != 0) {
                ShiftMnemonic = ARM_ASR_MNEMONIC;
            }

            sprintf(Context->Operand4, "%s #%d", ShiftMnemonic, Immediate5);
        }

        break;

    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_BFIC:
        if (Rn == 0xF) {
            Mnemonic = THUMB_BFC_MNEMONIC;
            LsbString = Context->Operand2;
            WidthString = Context->Operand3;

        } else {
            Mnemonic = THUMB_BFI_MNEMONIC;
            strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
            LsbString = Context->Operand3;
            WidthString = Context->Operand4;
        }

        Width = (Instruction >> THUMB32_DATA_PLAIN_IMMEDIATE_MSB_SHIFT) &
                THUMB32_DATA_PLAIN_IMMEDIATE_MSB_MASK;

        Width = Width + 1 - Immediate5;
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        sprintf(LsbString, "#%d", Immediate5);
        sprintf(WidthString, "#%d", Width);
        break;

    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_SBFX:
    case THUMB32_DATA_PLAIN_IMMEDIATE_OP_UBFX:
        if ((Instruction & THUMB32_DATA_PLAIN_IMMEDIATE_UNSIGNED) != 0) {
            Mnemonic = THUMB_UBFX_MNEMONIC;

        } else {
            Mnemonic = THUMB_SBFX_MNEMONIC;
        }

        Width = (Instruction >>
                 THUMB32_DATA_PLAIN_IMMEDIATE_WIDTH_MINUS_1_SHIFT) &
                THUMB32_DATA_PLAIN_IMMEDIATE_WIDTH_MINUS_1_MASK;

        Width += 1;
        strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        sprintf(Context->Operand3, "#%d", Immediate5);
        sprintf(Context->Operand4, "#%d", Width);
        break;

    default:
        break;
    }

    strcpy(Context->Mnemonic, Mnemonic);
    return;
}

VOID
DbgpThumb32DecodeBranchAndMiscellaneous (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes branch and miscellaneous instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    THUMB_DECODE_WITH_TABLE(Context, DbgThumb32BranchAndMiscellaneousTable);
    return;
}

VOID
DbgpThumb32DecodeMsr (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes MSR (move to status from ARM) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Mask;
    ULONG Mode;
    PSTR Register;
    ULONG Rn;

    Instruction = Context->Instruction;
    Rn = (Instruction >> THUMB32_MSR_RN_SHIFT) & THUMB_REGISTER16_MASK;
    strcpy(Context->Mnemonic, THUMB_MSR_MNEMONIC);
    if ((Instruction & THUMB32_MSR_BANKED_REGISTER) != 0) {
        Mode = (Instruction >> THUMB32_MSR_MODE_SHIFT) & THUMB32_MSR_MODE_MASK;
        if ((Instruction & THUMB32_MSR_MODE4) != 0) {
            Mode |= 1 << 4;
        }

        if ((Instruction & THUMB32_MSR_SPSR) != 0) {
            Mode |= 1 << 5;
        }

        strcpy(Context->Operand1, DbgArmBankedRegisters[Mode]);

    } else {
        Mask = (Instruction >> THUMB32_MSR_MASK_SHIFT) & THUMB32_MSR_MASK_MASK;
        Register = THUMB_CPSR_STRING;
        if ((Instruction & THUMB32_MSR_SPSR) != 0) {
            Register = THUMB_SPSR_STRING;
        }

        strcpy(Context->Operand1, Register);
        strcat(Context->Operand1, "_");
        if ((Mask & THUMB32_MSR_MASK_C) != 0) {
            strcat(Context->Operand1, "c");
        }

        if ((Mask & THUMB32_MSR_MASK_X) != 0) {
            strcat(Context->Operand1, "x");
        }

        if ((Mask & THUMB32_MSR_MASK_S) != 0) {
            strcat(Context->Operand1, "s");
        }

        if ((Mask & THUMB32_MSR_MASK_F) != 0) {
            strcat(Context->Operand1, "f");
        }
    }

    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    return;
}

VOID
DbgpThumb32DecodeCpsAndHints (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes the CPS (change processor state) instruction, as well
    as memory hints.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG HintOp;
    ULONG Instruction;
    ULONG Mode;
    ULONG Option;

    Instruction = Context->Instruction;

    //
    // If bits 8:6 are zero, then this is CPS.
    //

    if ((Instruction & THUMB32_CPS_MASK) == THUMB32_CPS_VALUE) {
        Mode = Instruction & THUMB32_CPS_MODE_MASK;
        if ((Instruction & THUMB32_CPS_DISABLE) != 0) {
            strcpy(Context->Mnemonic, THUMB_CPS_DISABLE_W_MNEMONIC);

        } else {
            strcpy(Context->Mnemonic, THUMB_CPS_ENABLE_W_MNEMONIC);
        }

        strcpy(Context->Operand1, "");
        if ((Instruction & THUMB32_CPS_FLAG_A) != 0) {
            strcat(Context->Operand1, ARM_CPS_FLAG_A_STRING);
        }

        if ((Instruction & THUMB32_CPS_FLAG_I) != 0) {
            strcat(Context->Operand1, ARM_CPS_FLAG_I_STRING);
        }

        if ((Instruction & THUMB32_CPS_FLAG_F) != 0) {
            strcat(Context->Operand1, ARM_CPS_FLAG_F_STRING);
        }

        if ((Instruction & THUMB32_CPS_CHANGE_MODE) != 0) {
            DbgpArmPrintMode(Context->Operand2, Mode);
        }

    //
    // This is a hint instruction.
    //

    } else {
        HintOp = Instruction & THUMB32_HINT_MASK;
        if ((HintOp & THUMB32_HINT_DBG_MASK) == THUMB32_HINT_DBG_VALUE) {
            Option = Instruction & THUMB32_DBG_OPTION_MASK;
            strcpy(Context->Mnemonic, THUMB_DBG_MNEMONIC);
            snprintf(Context->Operand1,
                     sizeof(Context->Operand1),
                     "#%d",
                     Option);

        } else {
            if (HintOp >= THUMB32_HINT_OP_COUNT) {
                strcpy(Context->Mnemonic, "Undef hint");

            } else {
                strcpy(Context->Mnemonic, DbgThumb32HintMnemonics[HintOp]);
            }
        }
    }

    return;
}

VOID
DbgpThumb32DecodeMiscellaneousControl (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb miscellaneous control instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    PSTR Mnemonic;
    ULONG Mode;
    ULONG Op;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB32_MISCELLANEOUS_CONTROL_OP_SHIFT) &
         THUMB32_MISCELLANEOUS_CONTROL_OP_MASK;

    Mode = Instruction & THUMB32_BARRIER_MODE_MASK;
    if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_ENTERX) {
        Mnemonic = THUMB_ENTERX_MNEMONIC;

    } else if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_LEAVEX) {
        Mnemonic = THUMB_LEAVEX_MNEMONIC;

    } else if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_CLREX) {
        Mnemonic = THUMB_CLREX_MNEMONIC;

    } else if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_DSB) {
        Mnemonic = THUMB_DSB_MNEMONIC;
        DbgpArmPrintBarrierMode(Context->Operand1, Mode);

    } else if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_DMB) {
        Mnemonic = THUMB_DMB_MNEMONIC;
        DbgpArmPrintBarrierMode(Context->Operand1, Mode);

    } else if (Op == THUMB32_MISCELLANEOUS_CONTROL_OP_ISB) {
        Mnemonic = THUMB_ISB_MNEMONIC;
        DbgpArmPrintBarrierMode(Context->Operand1, Mode);

    } else {
        Mnemonic = "Undef Misc control";
    }

    strcpy(Context->Mnemonic, Mnemonic);
    return;
}

VOID
DbgpThumb32DecodeBxj (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb BXJ instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Rm;

    Rm = (Context->Instruction >> THUMB32_BXJ_RM_SHIFT) & THUMB_REGISTER16_MASK;
    strcpy(Context->Mnemonic, THUMB_BXJ_MNEMONIC);
    strcpy(Context->Operand1, DbgArmRegisterNames[Rm]);
    return;
}

VOID
DbgpThumb32DecodeExceptionReturn (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb ERET (exception return) and SUBS pc, lr.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate8;

    Immediate8 = Context->Instruction & THUMB_IMMEDIATE8_MASK;
    if (Immediate8 == 0) {
        strcpy(Context->Mnemonic, THUMB_ERET_MNEMONIC);

    } else {
        strcpy(Context->Mnemonic, THUMB_SUBS_MNEMONIC);
        strcpy(Context->Operand1, DbgArmRegisterNames[15]);
        strcpy(Context->Operand2, DbgArmRegisterNames[13]);
        snprintf(Context->Operand3,
                 sizeof(Context->Operand3),
                 "#%d",
                 Immediate8);
    }

    return;
}

VOID
DbgpThumb32DecodeMrs (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb MRS (Move to ARM from Status register)
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Mode;
    ULONG Rd;
    PSTR Register;

    Instruction = Context->Instruction;
    Rd = (Instruction >> THUMB32_MRS_RD_SHIFT) & THUMB_REGISTER16_MASK;
    strcpy(Context->Mnemonic, THUMB_MRS_MNEMONIC);
    if ((Instruction & THUMB32_MRS_BANKED_REGISTER) != 0) {
        Mode = (Instruction >> THUMB32_MRS_MODE_SHIFT) & THUMB32_MRS_MODE_MASK;
        if ((Instruction & THUMB32_MRS_MODE4) != 0) {
            Mode |= 1 << 4;
        }

        if ((Instruction & THUMB32_MRS_SPSR) != 0) {
            Mode |= 1 << 5;
        }

        strcpy(Context->Operand2, DbgArmBankedRegisters[Mode]);

    } else {
        Register = THUMB_CPSR_STRING;
        if ((Instruction & THUMB32_MRS_SPSR) != 0) {
            Register = THUMB_SPSR_STRING;
        }

        strcpy(Context->Operand2, Register);
    }

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    return;
}

VOID
DbgpThumb32DecodeHvc (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb HVC (hypervisor call) instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate16;
    ULONG Instruction;

    Instruction = Context->Instruction;
    Immediate16 = (Instruction & THUMB32_HVC_IMMEDIATE12_MASK) |
                  ((Instruction >> THUMB32_HVC_IMMEDIATE4_SHIFT) &
                   THUMB32_HVC_IMMEDIATE4_MASK);

    strcpy(Context->Mnemonic, THUMB_HVC_MNEMONIC);
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "#%d",
             Immediate16);

    return;
}

VOID
DbgpThumb32DecodeSmc (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb SMC (secure monitor call) instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate4;
    ULONG Instruction;

    Instruction = Context->Instruction;
    Immediate4 = (Instruction >> THUMB32_SMC_IMMEDIATE4_SHIFT) &
                  THUMB32_SMC_IMMEDIATE4_MASK;

    strcpy(Context->Mnemonic, THUMB_SMC_MNEMONIC);
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "#%d",
             Immediate4);

    return;
}

VOID
DbgpThumb32DecodeBranch (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb branch (both conditional and
    unconditional) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Bit;
    ULONG Condition;
    PSTR ConditionString;
    LONG Immediate;
    ULONG Instruction;
    ULONGLONG OperandAddress;
    ULONG SBit;

    Instruction = Context->Instruction;
    Immediate = (Instruction >> THUMB32_B_IMMEDIATE11_SHIFT) &
                 THUMB32_B_IMMEDIATE11_MASK;

    Condition = (Instruction >> THUMB32_B_CONDITION_SHIFT) &
                THUMB32_B_CONDITION_MASK;

    ConditionString = "";

    //
    // Handle an unconditional branch, which has a larger range.
    //

    if ((Instruction & THUMB32_B_UNCONDITIONAL_MASK) ==
        THUMB32_B_UNCONDITIONAL_VALUE) {

        Immediate |= ((Instruction >> THUMB32_B_IMMEDIATE10_SHIFT) &
                      THUMB_IMMEDIATE10_MASK) << 11;

        //
        // The next two bits are NOT(J2 EOR S) and NOT(J1 EOR S).
        //

        SBit = 0;
        if ((Instruction & THUMB32_B_S_BIT) != 0) {
            SBit = 1;
        }

        Bit = 0;
        if ((Instruction & THUMB32_B_J1_BIT) != 0) {
            Bit = 1;
        }

        Bit = !(Bit ^ SBit);
        if (Bit != 0) {
            Immediate |= 1 << 21;
        }

        Bit = 0;
        if ((Instruction & THUMB32_B_J2_BIT) != 0) {
            Bit = 1;
        }

        Bit = !(Bit ^ SBit);
        if (Bit != 0) {
            Immediate |= 1 << 22;
        }

        if (SBit != 0) {
            Immediate |= 1 << 23;
        }

        Immediate <<= 1;

        //
        // Sign extend.
        //

        if ((Immediate & 0x01000000) != 0) {
            Immediate |= 0xFE000000;
        }

    //
    // Conditional branches sacrifice some range for the encoded condition.
    //

    } else {
        ConditionString = DbgArmConditionCodes[Condition];
        Immediate |= ((Instruction >> THUMB32_B_IMMEDIATE6_SHIFT) &
                      THUMB_IMMEDIATE6_MASK) << 11;

        if ((Instruction & THUMB32_B_J1_BIT) != 0) {
            Immediate |= (1 << 17);
        }

        if ((Instruction & THUMB32_B_J2_BIT) != 0) {
            Immediate |= (1 << 18);
        }

        if ((Instruction & THUMB32_B_S_BIT) != 0) {
            Immediate |= (1 << 19);
        }

        Immediate <<= 1;

        //
        // Sign extend.
        //

        if ((Immediate & 0x00100000) != 0) {
            Immediate |= 0xFFE00000;
        }
    }

    snprintf(Context->Mnemonic,
             sizeof(Context->Mnemonic),
             THUMB_B_W_MNEMONIC_FORMAT,
             ConditionString);

    //
    // All of these branches are relative to the PC, which is 4 ahead of the
    // instruction pointer. Calculate the absolute operand address.
    //

    OperandAddress = Context->InstructionPointer + 4;
    OperandAddress += (LONGLONG)Immediate;
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "[0x%08llx]",
             OperandAddress);

    Context->Result->OperandAddress = OperandAddress;
    Context->Result->AddressIsDestination = TRUE;
    Context->Result->AddressIsValid = TRUE;
    return;
}

VOID
DbgpThumb32DecodeUdf (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb undefined instruction (like THE undefined
    instruction).

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate20;
    ULONG Instruction;

    Instruction = Context->Instruction;
    Immediate20 = (Instruction & THUMB_IMMEDIATE12_MASK) |
                  ((Instruction >> THUMB32_UDF_IMMEDIATE4_SHIFT) &
                   THUMB_IMMEDIATE4_MASK);

    strcpy(Context->Mnemonic, THUMB_UDF_W_MNEMONIC);
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "#%d",
             Immediate20);

    return;
}

VOID
DbgpThumb32DecodeBranchWithLink (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb branch with link instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Bit;
    LONG Immediate25;
    ULONG Instruction;
    ULONGLONG OperandAddress;
    ULONG SBit;

    Instruction = Context->Instruction;
    Immediate25 = ((Instruction >> THUMB32_BL_IMMEDIATE11_SHIFT) &
                   THUMB_IMMEDIATE11_MASK) |
                  (((Instruction >> THUMB32_BL_IMMEDIATE10_SHIFT) &
                    THUMB_IMMEDIATE10_MASK) << 11);

    if ((Instruction & THUMB32_BL_X_BIT) == 0) {
        Immediate25 &= ~THUMB32_BL_THUMB_BIT;
    }

    //
    // The next two bits are NOT(J1 EOR S) and NOT(J2 EOR S).
    //

    SBit = 0;
    if ((Instruction & THUMB32_B_S_BIT) != 0) {
        SBit = 1;
    }

    Bit = 0;
    if ((Instruction & THUMB32_B_J2_BIT) != 0) {
        Bit = 1;
    }

    Bit = !(Bit ^ SBit);
    if (Bit != 0) {
        Immediate25 |= 1 << 21;
    }

    Bit = 0;
    if ((Instruction & THUMB32_B_J1_BIT) != 0) {
        Bit = 1;
    }

    Bit = !(Bit ^ SBit);
    if (Bit != 0) {
        Immediate25 |= 1 << 22;
    }

    if (SBit != 0) {
        Immediate25 |= 1 << 23;
    }

    Immediate25 <<= 1;

    //
    // Sign extend.
    //

    if ((Immediate25 & 0x00200000) != 0) {
        Immediate25 |= 0xFFC00000;
    }

    //
    // For the BLX encoding, the immediate is relative to "Align(PC, 4)". The
    // PC is four bytes ahead of the instruction pointer and it is an align
    // down operation. The align-down action also strips the low bit from the
    // Thumb instruction point, resulting in the correct ARM address. This is
    // necessay because the destination mode of BLX is ARM.
    //

    OperandAddress = Context->InstructionPointer + 4;
    if ((Instruction & THUMB32_BL_X_BIT) == 0) {
        strcpy(Context->Mnemonic, THUMB_BLX_MNEMONIC);
        OperandAddress = THUMB_ALIGN_4(OperandAddress);

    //
    // BL is relative to the PC.
    //

    } else {
        strcpy(Context->Mnemonic, THUMB_BL_MNEMONIC);
    }

    OperandAddress += (LONGLONG)Immediate25;
    snprintf(Context->Operand1,
             sizeof(Context->Operand1),
             "[0x%08llx]",
             OperandAddress);

    Context->Result->OperandAddress = OperandAddress;
    Context->Result->AddressIsDestination = TRUE;
    Context->Result->AddressIsValid = TRUE;
    return;
}

VOID
DbgpThumb32DecodeLoadStoreSingleItem (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb load/store instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;

    Instruction = Context->Instruction;
    if ((Instruction & THUMB32_LOAD_STORE_REGISTER_MASK) ==
         THUMB32_LOAD_STORE_REGISTER_VALUE) {

        DbgpThumb32DecodeLoadStoreRegister(Context);

    } else {
        DbgpThumb32DecodeLoadStoreImmediate(Context);
    }

    return;
}

VOID
DbgpThumb32DecodeLoadStoreImmediate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb load/store immediate instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    LONG Immediate;
    ULONG Instruction;
    ULONG Load;
    ULONG Op;
    ULONGLONG OperandAddress;
    ULONG Rn;
    ULONG Rt;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB32_LOAD_STORE_OP_SHIFT) &
         THUMB32_LOAD_STORE_OP_MASK;

    Rn = (Instruction >> THUMB32_LOAD_STORE_IMMEDIATE_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rt = (Instruction >> THUMB32_LOAD_STORE_IMMEDIATE_RT_SHIFT) &
         THUMB_REGISTER16_MASK;

    Load = 0;
    if ((Instruction & THUMB32_LOAD_BIT) != 0) {
        Load = 1;
    }

    //
    // Assume the mnemonic is not unprivileged. This may get altered later.
    //

    if ((Load != 0) && ((Instruction & THUMB32_LOAD_SET_FLAGS) != 0)) {
        strcpy(Context->Mnemonic, DbgThumb32LoadSetFlagsMnemonics[Op]);

    } else {
        strcpy(Context->Mnemonic,
               DbgThumb32LoadStoreMnemonics[Load][Op]);
    }

    //
    // If bit 23 is set, then the pre-index is an immediate12.
    //

    if ((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_LARGE) != 0) {
        Immediate = Instruction & THUMB_IMMEDIATE12_MASK;
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[%s, #%d]",
                 DbgArmRegisterNames[Rn],
                 Immediate);

    //
    // There are a few addressing modes, and an immediate8.
    //

    } else {
        Immediate = Instruction & THUMB_IMMEDIATE8_MASK;
        if ((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_ADD) == 0) {
            Immediate = -Immediate;
        }

        if ((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_PREINDEX) != 0) {
            if ((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_WRITE_BACK) != 0) {
                snprintf(Context->Operand2,
                         sizeof(Context->Operand2),
                         "[%s, #%d]!",
                         DbgArmRegisterNames[Rn],
                         Immediate);

            } else {
                snprintf(Context->Operand2,
                         sizeof(Context->Operand2),
                         "[%s, #%d]",
                         DbgArmRegisterNames[Rn],
                         Immediate);
            }

        } else {
            snprintf(Context->Operand2,
                     sizeof(Context->Operand2),
                     "[%s], #%d",
                     DbgArmRegisterNames[Rn],
                     Immediate);
        }

        //
        // It's an unprivileged instruction if both the P (preindex) and U (add)
        // bits are set.
        //

        if (((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_PREINDEX) != 0) &&
            ((Instruction & THUMB32_LOAD_STORE_IMMEDIATE_ADD) != 0)) {

            if ((Load != 0) && ((Instruction & THUMB32_LOAD_SET_FLAGS) != 0)) {
                strcpy(Context->Mnemonic,
                       DbgThumb32LoadSetFlagsUnprivilegedMnemonics[Op]);

            } else {
                strcpy(Context->Mnemonic,
                       DbgThumb32LoadStoreUnprivilegedMnemonics[Load][Op]);
            }
        }
    }

    //
    // If this is a load relative to the PC, then calculate the absolute
    // operand address and override the second operand with the absolute
    // address.
    //

    if ((Load != 0) && (Rn == 15)) {

        //
        // The address is relative to the PC aligned down to a 4-byte boundary.
        //

        OperandAddress = Context->InstructionPointer + 4;
        OperandAddress = THUMB_ALIGN_4(OperandAddress);
        OperandAddress += Immediate;
        Context->Result->OperandAddress = OperandAddress;
        Context->Result->AddressIsDestination = FALSE;
        Context->Result->AddressIsValid = TRUE;
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[0x%08llx]",
                 OperandAddress);
    }

    //
    // If Rt is 15, then this is actually a preload operation. Copy the second
    // operand to the first.
    //

    if (Rt == 15) {
        strcpy(Context->Mnemonic, DbgThumb32PreloadMnemonics[Op]);
        strcpy(Context->Operand1, Context->Operand2);
        Context->Operand2[0] = '\0';

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    }

    return;
}

VOID
DbgpThumb32DecodeLoadStoreRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb load/store register instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Immediate2;
    ULONG Instruction;
    ULONG Load;
    ULONG Op;
    ULONG Rm;
    ULONG Rn;
    ULONG Rt;

    Instruction = Context->Instruction;
    Op = (Instruction >> THUMB32_LOAD_STORE_OP_SHIFT) &
         THUMB32_LOAD_STORE_OP_MASK;

    Rm = (Instruction >> THUMB32_LOAD_STORE_REGISTER_RM_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_LOAD_STORE_REGISTER_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rt = (Instruction >> THUMB32_LOAD_STORE_REGISTER_RT_SHIFT) &
         THUMB_REGISTER16_MASK;

    Immediate2 = (Instruction >> THUMB32_LOAD_STORE_REGISTER_IMMEDIATE2_SHIFT) &
                 THUMB_IMMEDIATE2_MASK;

    Load = 0;
    if ((Instruction & THUMB32_LOAD_BIT) != 0) {
        Load = 1;
    }

    if ((Load != 0) && ((Instruction & THUMB32_LOAD_SET_FLAGS) != 0)) {
        strcpy(Context->Mnemonic, DbgThumb32LoadSetFlagsMnemonics[Op]);

    } else {
        strcpy(Context->Mnemonic, DbgThumb32LoadStoreMnemonics[Load][Op]);
    }

    if (Immediate2 == 0) {
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[%s, %s]",
                 DbgArmRegisterNames[Rn],
                 DbgArmRegisterNames[Rm]);

    } else {
        snprintf(Context->Operand2,
                 sizeof(Context->Operand2),
                 "[%s, %s, %s #%d]",
                 DbgArmRegisterNames[Rn],
                 THUMB_SHIFT_TYPE_LSL_STRING,
                 DbgArmRegisterNames[Rm],
                 Immediate2);
    }

    //
    // If Rt is 15, then this is actually a preload operation. Copy the second
    // operand to the first.
    //

    if (Rt == 15) {
        strcpy(Context->Mnemonic, DbgThumb32PreloadMnemonics[Op]);
        strcpy(Context->Operand1, Context->Operand2);
        strcpy(Context->Operand2, "");

    } else {
        strcpy(Context->Operand1, DbgArmRegisterNames[Rt]);
    }

    return;
}

VOID
DbgpThumb32DecodeDataProcessingRegister (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb data processing (register) instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG MiscellaneousOp;
    ULONG Op1;
    ULONG ParallelOp;
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;
    ULONG Rotate;
    ULONG SetFlags;
    ULONG Unsigned;

    Instruction = Context->Instruction;
    SetFlags = 0;
    if ((Instruction & THUMB32_DATA_SET_FLAGS) != 0) {
        SetFlags = 1;
    }

    Op1 = (Instruction >> THUMB32_DATA_PROCESSING_REGISTER_OP1_SHIFT) &
          THUMB32_DATA_PROCESSING_REGISTER_OP1_MASK;

    Rd = (Instruction >> THUMB32_DATA_PROCESSING_REGISTER_RD_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rm = (Instruction >> THUMB32_DATA_PROCESSING_REGISTER_RM_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_DATA_PROCESSING_REGISTER_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);

    //
    // Handle shift/rotate instructions.
    //

    if ((Instruction & THUMB32_DATA_PROCESSING_REGISTER_SHIFT_MASK) ==
        THUMB32_DATA_PROCESSING_REGISTER_SHIFT_VALUE) {

        strcpy(Context->Mnemonic,
               DbgThumb32DataProcessingShiftMnemonics[SetFlags][Op1 >> 1]);

        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);

    //
    // Handle signed and unsigned extend and add.
    //

    } else if ((Op1 & THUMB32_DATA_PROCESSING_REGISTER_OP1_EXTEND) == 0) {
        Rotate = (Instruction >>
                  THUMB32_DATA_PROCESSING_REGISTER_ROTATE_SHIFT) &
                 THUMB32_DATA_PROCESSING_REGISTER_ROTATE_MASK;

        Rotate <<= 3;
        if (Op1 < THUMB32_DATA_PROCESSING_REGISTER_OP1_EXTEND_COUNT) {
            if (Rn == 15) {
                strcpy(Context->Mnemonic,
                       DbgThumb32ExtendAndAddMnemonics[1][Op1]);

                strcpy(Context->Operand2, DbgArmRegisterNames[Rm]);
                if (Rotate != 0) {
                    snprintf(Context->Operand3,
                             sizeof(Context->Operand4),
                             "ror #%d",
                             Rotate);
                }

            } else {
                strcpy(Context->Mnemonic,
                       DbgThumb32ExtendAndAddMnemonics[0][Op1]);

                strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
                strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
                if (Rotate != 0) {
                    snprintf(Context->Operand4,
                             sizeof(Context->Operand4),
                             "ror #%d",
                             Rotate);
                }
            }
        }

    //
    // Handle parallel addition and subtraction, both signed and unsigned.
    //

    } else if ((Instruction & THUMB32_DATA_PROCESSING_REGISTER_PARALLEL) == 0) {
        Unsigned = 0;
        if ((Instruction & THUMB32_DATA_PROCESSING_REGISTER_UNSIGNED) != 0) {
            Unsigned = 1;
        }

        ParallelOp = (Instruction >>
                      THUMB32_DATA_PROCESSING_PARALLEL_OP1_SHIFT) &
                     THUMB32_DATA_PROCESSING_PARALLEL_OP1_MASK;

        ParallelOp |= ((Instruction >>
                        THUMB32_DATA_PROCESSING_PARALLEL_OP2_SHIFT) &
                       THUMB32_DATA_PROCESSING_PARALLEL_OP2_MASK) << 3;

        if (ParallelOp < THUMB32_DATA_PROCESSING_PARALLEL_OP_COUNT) {
            strcpy(Context->Mnemonic,
                   DbgThumb32ParallelArithmeticMnemonics[Unsigned][ParallelOp]);
        }

        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);

    //
    // Handle miscellaneous instructions.
    //

    } else {
        MiscellaneousOp = (Instruction >>
                           THUMB32_DATA_PROCESSING_MISCELLANEOUS_OP2_SHIFT) &
                          THUMB32_DATA_PROCESSING_MISCELLANEOUS_OP2_MASK;

        MiscellaneousOp |= ((Instruction >>
                             THUMB32_DATA_PROCESSING_MISCELLANEOUS_OP1_SHIFT) &
                            THUMB32_DATA_PROCESSING_MISCELLANEOUS_OP1_MASK) <<
                           2;

        strcpy(Context->Mnemonic,
               DbgThumb32DataProcessingMiscellaneousMnemonics[MiscellaneousOp]);

        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        if (Rn != Rm) {
            strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
        }
    }

    return;
}

VOID
DbgpThumb32DecodeMultiplyAccumulate (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb multiply and multiply/accumulate
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Instruction;
    ULONG Op1;
    ULONG Op2;
    ULONG Ra;
    ULONG Rd;
    ULONG Rm;
    ULONG Rn;
    ULONG Top;

    Instruction = Context->Instruction;
    Ra = (Instruction >> THUMB32_MULTIPLY_RA_SHIFT) & THUMB_REGISTER16_MASK;
    Rd = (Instruction >> THUMB32_MULTIPLY_RD_SHIFT) & THUMB_REGISTER16_MASK;
    Rm = (Instruction >> THUMB32_MULTIPLY_RN_SHIFT) & THUMB_REGISTER16_MASK;
    Rn = (Instruction >> THUMB32_MULTIPLY_RM_SHIFT) & THUMB_REGISTER16_MASK;
    Op1 = (Instruction >> THUMB32_MULTIPLY_OP1_SHIFT) &
          THUMB32_MULTIPLY_OP1_MASK;

    Op2 = (Instruction >> THUMB32_MULTIPLY_OP2_SHIFT) &
           THUMB32_MULTIPLY_OP2_MASK;

    strcpy(Context->Operand1, DbgArmRegisterNames[Rd]);
    strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
    strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    if (Ra != 15) {
        strcpy(Context->Mnemonic, DbgThumb32MultiplyMnemonics[0][Op1]);
        strcpy(Context->Operand4, DbgArmRegisterNames[Ra]);

    } else {
        strcpy(Context->Mnemonic, DbgThumb32MultiplyMnemonics[1][Op1]);
    }

    if ((Op1 == THUMB32_MULTIPLY_OP1_MLS) &&
        (Op2 == THUMB32_MULTIPLY_OP2_MLS)) {

        strcpy(Context->Mnemonic, THUMB_MLS_MNEMONIC);
    }

    //
    // Instructions that operate on only the top or bottom half of some
    // registers (Rn and maybe Rm) get endings for top or bottom.
    //

    if (Op1 == THUMB32_MULTIPLY_OP1_HALF_HALF) {
        Top = 0;
        if ((Instruction & THUMB32_MULTIPLY_RN_TOP) != 0) {
            Top = 1;
        }

        strcat(Context->Mnemonic, DbgThumb32MultiplyTopBottomMnemonics[Top]);
    }

    if ((Op1 == THUMB32_MULTIPLY_OP1_HALF_HALF) ||
        (Op1 == THUMB32_MULTIPLY_OP1_WORD_HALF)) {

        Top = 0;
        if ((Instruction & THUMB32_MULTIPLY_RM_TOP) != 0) {
            Top = 1;
        }

        strcat(Context->Mnemonic, DbgThumb32MultiplyTopBottomMnemonics[Top]);
    }

    //
    // A couple of instructions have an optional X or R tagged on the end.
    //

    if ((Op1 == THUMB32_MULTIPLY_OP1_SMAD) ||
        (Op1 == THUMB32_MULTIPLY_OP1_SMSD)) {

        if ((Instruction & THUMB32_MULTIPLY_DUAL_CROSS) != 0) {
            strcat(Context->Mnemonic, THUMB_MULTIPLY_CROSS_MNEMONIC);
        }

    } else if (Op1 == THUMB32_MULTIPLY_OP1_SMML) {
        if ((Instruction & THUMB32_MULTIPLY_ROUND) != 0) {
            strcat(Context->Mnemonic, THUMB_MULTIPLY_ROUND_MNEMONIC);
        }
    }

    return;
}

VOID
DbgpThumb32DecodeLongMultiplyDivide (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes 32-bit Thumb long multiply and divide instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Cross;
    ULONG Instruction;
    ULONG Op1;
    ULONG Op2;
    ULONG RdHigh;
    ULONG RdLow;
    ULONG Rm;
    ULONG Rn;
    ULONG Top;

    Instruction = Context->Instruction;
    RdHigh = (Instruction >> THUMB32_LONG_MULTIPLY_RD_HIGH_SHIFT) &
             THUMB_REGISTER16_MASK;

    RdLow = (Instruction >> THUMB32_LONG_MULTIPLY_RD_LOW_SHIFT) &
             THUMB_REGISTER16_MASK;

    Rm = (Instruction >> THUMB32_LONG_MULTIPLY_RM_SHIFT) &
         THUMB_REGISTER16_MASK;

    Rn = (Instruction >> THUMB32_LONG_MULTIPLY_RN_SHIFT) &
         THUMB_REGISTER16_MASK;

    Op1 = (Instruction >> THUMB32_LONG_MULTIPLY_OP1_SHIFT) &
          THUMB32_LONG_MULTIPLY_OP1_MASK;

    Op2 = (Instruction >> THUMB32_LONG_MULTIPLY_OP2_SHIFT) &
          THUMB32_LONG_MULTIPLY_OP2_MASK;

    Cross = 0;
    strcpy(Context->Mnemonic, DbgThumb32LongMultiplyMnemonics[Op1]);
    if (Op1 == THUMB32_LONG_MULTIPLY_OP1_SMLA) {
        if ((Op2 & THUMB32_LONG_MULTIPLY_OP2_SMLA_HALF_MASK) ==
            THUMB32_LONG_MULTIPLY_OP2_SMLA_HALF_VALUE) {

            Top = 0;
            if ((Instruction & THUMB32_MULTIPLY_RN_TOP) != 0) {
                Top = 1;
            }

            strcat(Context->Mnemonic,
                   DbgThumb32MultiplyTopBottomMnemonics[Top]);

            Top = 0;
            if ((Instruction & THUMB32_MULTIPLY_RM_TOP) != 0) {
                Top = 1;
            }

            strcat(Context->Mnemonic,
                   DbgThumb32MultiplyTopBottomMnemonics[Top]);

        } else if ((Op2 & THUMB32_LONG_MULTIPLY_OP2_SMLALD_MASK) ==
                   THUMB32_LONG_MULTIPLY_OP2_SMLALD_VALUE) {

            strcpy(Context->Mnemonic, THUMB_SMLALD_MNEMONIC);
            Cross = Instruction & THUMB32_MULTIPLY_DUAL_CROSS;
        }

    } else if (Op1 == THUMB32_LONG_MULTIPLY_OP1_SMLSLD) {
        Cross = Instruction & THUMB32_MULTIPLY_DUAL_CROSS;
    }

    if (Cross != 0) {
        strcat(Context->Mnemonic, THUMB_MULTIPLY_CROSS_MNEMONIC);
    }

    strcpy(Context->Operand1, DbgArmRegisterNames[RdHigh]);
    if (RdLow != 15) {
        strcpy(Context->Operand2, DbgArmRegisterNames[RdLow]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rn]);
        strcpy(Context->Operand4, DbgArmRegisterNames[Rm]);

    } else {
        strcpy(Context->Operand2, DbgArmRegisterNames[Rn]);
        strcpy(Context->Operand3, DbgArmRegisterNames[Rm]);
    }

    return;
}

VOID
DbgpThumbDecodeImmediateShift (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG Register,
    ULONG Type,
    ULONG Immediate
    )

/*++

Routine Description:

    This routine performs the operation known in the ARM ARM as
    DecodeImmShift().

Arguments:

    Destination - Supplies the destination to write to.

    DestinationSize - Supplies the size of the destination.

    Register - Supplies the base register.

    Type - Supplies the shift type.

    Immediate - Supplies the shift value.

Return Value:

    None.

--*/

{

    PSTR ShiftTypeString;

    switch (Type) {
    case THUMB_SHIFT_TYPE_LSL:
        if (Immediate == 0) {
            snprintf(Destination,
                     DestinationSize,
                     "%s",
                     DbgArmRegisterNames[Register]);

        } else {
            snprintf(Destination,
                     DestinationSize,
                     "%s, %s #%d",
                     DbgArmRegisterNames[Register],
                     THUMB_SHIFT_TYPE_LSL_STRING,
                     Immediate);
        }

        break;

    case THUMB_SHIFT_TYPE_LSR:
        if (Immediate == 0) {
            Immediate = 32;
        }

        snprintf(Destination,
                 DestinationSize,
                 "%s, %s #%d",
                 DbgArmRegisterNames[Register],
                 THUMB_SHIFT_TYPE_LSR_STRING,
                 Immediate);

        break;

    case THUMB_SHIFT_TYPE_ASR:
        if (Immediate == 0) {
            Immediate = 32;
        }

        snprintf(Destination,
                 DestinationSize,
                 "%s, %s #%d",
                 DbgArmRegisterNames[Register],
                 THUMB_SHIFT_TYPE_ASR_STRING,
                 Immediate);

        break;

    case THUMB_SHIFT_TYPE_ROR:
    default:
        ShiftTypeString = THUMB_SHIFT_TYPE_ROR_STRING;
        if (Immediate == 0) {
            Immediate = 1;
            ShiftTypeString = THUMB_SHIFT_TYPE_RRX_STRING;
        }

        snprintf(Destination,
                 DestinationSize,
                 "%s, %s #%d",
                 DbgArmRegisterNames[Register],
                 ShiftTypeString,
                 Immediate);

        break;
    }

    return;
}

ULONG
DbgpThumb32DecodeModifiedImmediate (
    ULONG Immediate12
    )

/*++

Routine Description:

    This routine performs the operation known in the ARM ARM as
    ThumbExpandImm(), expanding a modified immediate.

Arguments:

    Immediate12 - Supplies the 12 bit immediate.

Return Value:

    Returns the expanded immediate.

--*/

{

    ULONG Result;
    ULONG RotateCount;

    if ((Immediate12 & THUMB32_MODIFIED_IMMEDIATE_OP_MASK) ==
        THUMB32_MODIFIED_IMMEDIATE_OP_NO_ROTATE) {

        Result = Immediate12 & THUMB_IMMEDIATE8_MASK;
        switch ((Immediate12 >> 8) & 0x3) {

        //
        // 00000000 00000000 00000000 abcdefgh
        //

        case 0x0:
            break;

        //
        // 00000000 abcdefgh 00000000 abcdefgh
        //

        case 0x1:
            Result |= Result << 16;
            break;

        //
        // abcdefgh 00000000 abcdefgh 00000000
        //

        case 0x2:
            Result |= Result << 16;
            Result <<= 8;
            break;

        //
        // abcdefgh abcdefgh abcdefgh abcdefgh
        //

        case 0x3:
            Result |= Result << 16;
            Result |= Result << 8;
            break;

        default:
            break;
        }

    //
    // Rotate bits 6:0 (with a 1 tacked on the MSB) by the amount specified in
    // bits 7-11.
    //

    } else {
        Result = (Immediate12 & THUMB32_MODIFIED_IMMEDIATE_CONSTANT_MASK) |
                 THUMB32_MODIFIED_IMMEDIATE_EXTRA_ONE;

        RotateCount = (Immediate12 >> THUMB32_MODIFIED_IMMEDIATE_ROTATE_SHIFT) &
                      THUMB32_MODIFIED_IMMEDIATE_ROTATE_MASK;

        //
        // Perform the rotate.
        //

        Result = (Result >> RotateCount) | (Result << (32 - RotateCount));
    }

    return Result;
}

