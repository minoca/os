/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    armdis.h

Abstract:

    This header contains internal definitions for instruction encodings used by
    the ARM disassembler.

Author:

    Evan Green 23-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro builds an immediate value out of the 12 and 4 bit fields.
//

#define ARM_SERVICE_BUILD_IMMEDIATE12_4(_Instruction) \
    (((_Instruction) & 0x000FFF00) >> 4) | ((_Instruction) & 0x0000000F)

//
// This macro builds a floating point 8-bit immediate value.
//

#define ARM_FLOATING_POINT_BUILD_IMMEDIATE8(_Instruction) \
    (((_Instruction) & 0x000F0000) >> 12) | ((_Instruction) & 0x0000000F)

//
// This macros builds an floating point 32-bit immediate value from an 8-bit
// immediate value.
//

#define ARM_FLOATING_POINT_BUILD_IMMEDIATE32(_Immediate8) \
    (((_Immediate8) & 0x80) << 24) |                      \
    ((~((_Immediate8) & 0x40) & 0x40) << 24) |            \
    (((_Immediate8) & 0x40) << 23) |                      \
    (((_Immediate8) & 0x40) << 22) |                      \
    (((_Immediate8) & 0x40) << 21) |                      \
    (((_Immediate8) & 0x40) << 20) |                      \
    (((_Immediate8) & 0x7F) << 19)

//
// This macros builds an floating point 64-bit immediate value from an 8-bit
// immediate value.
//

#define ARM_FLOATING_POINT_BUILD_IMMEDIATE64(_Immediate8) \
    ((ULONGLONG)((_Immediate8) & 0x80) << 56) |           \
    ((ULONGLONG)(~((_Immediate8) & 0x40) & 0x40) << 56) | \
    ((ULONGLONG)((_Immediate8) & 0x40) << 55) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 54) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 53) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 52) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 51) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 50) |           \
    ((ULONGLONG)((_Immediate8) & 0x40) << 49) |           \
    ((ULONGLONG)((_Immediate8) & 0x7F) << 48)

//
// This macro builds an SIMD 8-bit immediate value.
//

#define ARM_SIMD_BUILD_IMMEDIATE8(_Instruction) \
    (((_Instruction) & 0x01000000) >> 17) |     \
    (((_Instruction) & 0x00070000) >> 12) |     \
    ((_Instruction) & 0x0000000F)

//
// Thie macro build an SIMD 32-bit immediate value.
//

#define ARM_SIMD_BUILD_IMMEDIATE32(_Instruction)            \
    (((_Instruction) & 0x01000000) << 7) |                  \
    ((~((_Instruction) & 0x00040000) & 0x00040000) << 12) | \
    (((_Instruction) & 0x00040000) << 11) |                 \
    (((_Instruction) & 0x00040000) << 10) |                 \
    (((_Instruction) & 0x00040000) << 9) |                  \
    (((_Instruction) & 0x00040000) << 8) |                  \
    (((_Instruction) & 0x00070000) << 7) |                  \
    (((_Instruction) & 0x0000000F) << 19)

//
// Thie macro build an SIMD 64-bit immediate value.
//

#define ARM_SIMD_BUILD_IMMEDIATE64(_Instruction)       \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 39) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 38) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 37) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 36) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 35) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 34) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 33) | \
    ((ULONGLONG)((_Instruction) & 0x01000000) << 32) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 37) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 36) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 35) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 34) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 33) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 32) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 31) | \
    ((ULONGLONG)((_Instruction) & 0x00040000) << 30) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 30) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 29) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 28) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 27) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 26) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 25) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 24) | \
    ((ULONGLONG)((_Instruction) & 0x00020000) << 23) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 23) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 22) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 21) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 20) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 19) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 18) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 17) | \
    ((ULONGLONG)((_Instruction) & 0x00010000) << 16) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 28) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 27) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 26) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 25) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 24) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 23) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 22) | \
    ((ULONGLONG)((_Instruction) & 0x00000008) << 21) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 21) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 20) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 19) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 18) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 17) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 16) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 15) | \
    ((ULONGLONG)((_Instruction) & 0x00000004) << 14) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 14) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 13) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 12) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 11) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 10) | \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 9) |  \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 8) |  \
    ((ULONGLONG)((_Instruction) & 0x00000002) << 7) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 7) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 6) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 5) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 4) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 3) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 2) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 1) |  \
    ((ULONGLONG)((_Instruction) & 0x00000001) << 0)

//
// This macro builds an SIMD scalar size encoding for the given instruction.
//

#define ARM_SIMD_TRANSFER_SCALAR_BUILD_SIZE_ENCODING(_Instruction) \
    (((_Instruction) & 0x00600000) >> 19) | (((_Instruction) & 0x00000060) >> 5)

//
// This macro builds an SIMD duplicate instruction's size encoding.
//

#define ARM_SIMD_TRANSFER_DUP_BUILD_SIZE_ENCODING(_Instruction) \
    (((_Instruction) & 0x00400000) >> 21) | (((_Instruction) & 0x00000020) >> 5)

//
// This macro returns the number of elements in the given table.
//

#define ARM_TABLE_SIZE(_Table) (sizeof(_Table) / sizeof((_Table)[0]))

//
// This macro calls the thumb decode with table function, using the table
// size macro to compute the size of the table.
//

#define ARM_DECODE_WITH_TABLE(_Context, _Table) \
    DbgpArmDecodeWithTable(_Context, _Table, ARM_TABLE_SIZE(_Table))

//
// ---------------------------------------------------------------- Definitions
//

#define ARM_INSTRUCTION_SIZE 4

//
// Define the maximum size of an ARM operand string. This has got to be big
// enough to hold "{r0, r1, r2, r3, r4 ... lr, pc}"
//

#define ARM_OPERAND_LENGTH 100

#define ARM_STACK_REGISTER 13
#define ARM_PC_REGISTER 15

//
// Condition Codes.
//

#define ARM_CONDITION_SHIFT             28
#define ARM_CONDITION_UNCONDITIONAL     0xF

//
// Bit definitions common to several instruction classes.
//

#define ARM_OFFSET_REGISTER             0x0000000F
#define ARM_DESTINATION_REGISTER_MASK   0x0000F000
#define ARM_DESTINATION_REGISTER_SHIFT  12
#define ARM_DESTINATION_REGISTER2_MASK  0x000F0000
#define ARM_DESTINATION_REGISTER2_SHIFT 16
#define ARM_SET_FLAGS_BIT               0x00100000
#define ARM_IMMEDIATE_BIT               0x02000000
#define ARM_WRITE_BACK_BIT              0x00200000
#define ARM_LOAD_BIT                    0x00100000
#define ARM_PREINDEX_BIT                0x01000000
#define ARM_ADD_SUBTRACT_BIT            0x00800000

#define ARM_SHIFT_TYPE  0x00000060
#define ARM_SHIFT_LSL   0x00000000
#define ARM_SHIFT_LSR   0x00000020
#define ARM_SHIFT_ASR   0x00000040
#define ARM_SHIFT_ROR   0x00000060

#define ARM_SET_FLAGS_MNEMONIC  "s"
#define ARM_LSL_MNEMONIC        "lsl"
#define ARM_LSR_MNEMONIC        "lsr"
#define ARM_ASR_MNEMONIC        "asr"
#define ARM_ROR_MNEMONIC        "ror"
#define ARM_RRX_MNEMONIC        "rrx"

//
// Data processing instruction encodings.
//

#define ARM_DATA_PROCESSING_OP_MASK                 0x01F00000
#define ARM_DATA_PROCESSING_OP_SHIFT                20

#define ARM_DATA_PROCESSING_IMMEDIATE8_MASK         0x000000FF
#define ARM_DATA_PROCESSING_IMMEDIATE_ROTATE_MASK   0x00000F00
#define ARM_DATA_PROCESSING_IMMEDIATE_ROTATE_SHIFT  8
#define ARM_DATA_PROCESSING_REGISTER_REGISTER_SHIFT_BIT 0x00000010
#define ARM_DATA_PROCESSING_SHIFT_OPCODE_MASK       0x00000060
#define ARM_DATA_PROCESSING_SHIFT_OPCODE_SHIFT      5
#define ARM_DATA_PROCESSING_SHIFT_REGISTER_MASK     0x00000F00
#define ARM_DATA_PROCESSING_SHIFT_REGISTER_SHIFT    8
#define ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_MASK    0x00000F80
#define ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_SHIFT   7
#define ARM_DATA_PROCESSING_OPERAND_REGISTER_MASK   0x000F0000
#define ARM_DATA_PROCESSING_OPERAND_REGISTER_SHIFT  16
#define ARM_DATA_PROCESSING_OPERAND2_REGISTER_MASK  0x0000000F

#define ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MIN 8
#define ARM_DATA_PROCESSING_COMPARE_INSTRUCTION_MAX 11

#define ARM_DATA_PROCESSING_OPERAND2_REGISTER_MASK  0x0000000F

#define ARM_DATA_PROCESSING_MOVE_OPCODE     13
#define ARM_DATA_PROCESSING_MOVE_NOT_OPCODE 15

//
// 16-bit immediate load instructions.
//

#define ARM_IMMEDIATE_LOAD_OP_MASK           0x00F00000
#define ARM_IMMEDIATE_LOAD_OP_MOVW           0x00000000
#define ARM_IMMEDIATE_LOAD_OP_MOVT           0x00400000
#define ARM_IMMEDIATE_LOAD_IMMEDIATE4_MASK   0x000F0000
#define ARM_IMMEDIATE_LOAD_IMMEDIATE4_SHIFT  16
#define ARM_IMMEDIATE_LOAD_IMMEDIATE12_MASK  0x00000FFF
#define ARM_IMMEDIATE_LOAD_IMMEDIATE12_SHIFT 0

#define ARM_MOVW_MNEMONIC "movw"
#define ARM_MOVT_MNEMONIC "movt"

//
// Miscellaneous instructions (coming out of data processing).
//

#define ARM_MISCELLANEOUS1_OP2_MASK     0x00000070
#define ARM_MISCELLANEOUS1_OP2_SHIFT    4
#define ARM_MISCELLANEOUS1_OP_MASK      0x00600000
#define ARM_MISCELLANEOUS1_OP_SHIFT     21
#define ARM_MISCELLANEOUS1_OP_MSR       0x1
#define ARM_MISCELLANEOUS1_OP_BX        0x1
#define ARM_MISCELLANEOUS1_OP_CLZ       0x3
#define ARM_MISCELLANEOUS1_OP_BKPT      0x1
#define ARM_MISCELLANEOUS1_OP_HVC       0x2
#define ARM_MISCELLANEOUS1_OP_SMC       0x3

#define ARM_MISCELLANEOUS1_OP2_STATUS   0x00
#define ARM_MISCELLANEOUS1_OP2_BX_CLZ   0x01
#define ARM_MISCELLANEOUS1_OP2_BXJ      0x02
#define ARM_MISCELLANEOUS1_OP2_BLX      0x03
#define ARM_MISCELLANEOUS1_OP2_SATURATING_ADDITION 0x5
#define ARM_MISCELLANEOUS1_OP2_ERET     0x06
#define ARM_MISCELLANEOUS1_OP2_SERVICE  0x07

#define ARM_MOVE_STATUS_BANKED          (1 << 9)
#define ARM_MOVE_STATUS_SPSR            (1 << 22)
#define ARM_MOVE_STATUS_RD_MASK         0x00000F00
#define ARM_MOVE_STATUS_RD_SHIFT        8
#define ARM_MOVE_STATUS_R0_MASK         0x0000000F

#define ARM_MSR_MASK_C                  0x00010000
#define ARM_MSR_MASK_X                  0x00020000
#define ARM_MSR_MASK_S                  0x00040000
#define ARM_MSR_MASK_F                  0x00080000

#define ARM_MSR_MNEMONIC                "msr"
#define ARM_MRS_MNEMONIC                "mrs"
#define ARM_SPSR_STRING                 "spsr"
#define ARM_CPSR_STRING                 "cpsr"
#define ARM_MSR_MASK_C_FLAG             'c'
#define ARM_MSR_MASK_X_FLAG             'x'
#define ARM_MSR_MASK_S_FLAG             's'
#define ARM_MSR_MASK_F_FLAG             'f'
#define ARM_BX_MNEMONIC                 "bx"
#define ARM_CLZ_MNEMONIC                "clz"
#define ARM_BXJ_MNEMONIC                "bxj"
#define ARM_ERET_MNEMONIC               "eret"
#define ARM_BKPT_MNEMONIC               "bkpt"
#define ARM_HVC_MNEMONIC                "hvc"
#define ARM_SMC_MNEMONIC                "smc"

//
// MSR immediate and hints encodings
//

#define ARM_HINTS_OP1_MASK              0x000F0000
#define ARM_HINTS_OP1_SHIFT             16
#define ARM_HINTS_OP1_HINTS             0x0

#define ARM_HINTS_OP2_MASK              0x000000FF
#define ARM_HINTS_OP2_NOP               0x00
#define ARM_HINTS_OP2_YIELD             0x01
#define ARM_HINTS_OP2_WFE               0x02
#define ARM_HINTS_OP2_WFI               0x03
#define ARM_HINTS_OP2_SEV               0x04
#define ARM_HINTS_OP2_DBG_MASK          0xF0
#define ARM_HINTS_OP2_DBG_VALUE         0xF0
#define ARM_HINTS_OP2_DBG_OPTION_MASK   0x0F

#define ARM_NOP_MNEMONIC                "nop"
#define ARM_YIELD_MNEMONIC              "yield"
#define ARM_WFE_MNEMONIC                "wfe"
#define ARM_WFI_MNEMONIC                "wfi"
#define ARM_SEV_MNEMONIC                "sev"
#define ARM_DBG_MNEMONIC                "dbg"
#define ARM_MSR_IMMEDIATE12_MASK        0x00000FFF

//
// Multiply instruction encodings
//

#define ARM_MULTIPLY_MASK               0x0F0000F0
#define ARM_MULTIPLY_VALUE              0x00000090
#define ARM_MULTIPLY_SOURCE_HIGH        0x00000020
#define ARM_MULTIPLY_DESTINATION_HIGH   0x00000040
#define ARM_MULTIPLY_X_BIT              0x00000020
#define ARM_MULTIPLY_ROUND_BIT          0x00000020
#define ARM_MULTIPLY_OPCODE_MASK        0x0FF00000
#define ARM_MULTIPLY_RD_MASK            0x000F0000
#define ARM_MULTIPLY_RD_SHIFT           16
#define ARM_MULTIPLY_RM_MASK            0x0000000F
#define ARM_MULTIPLY_RM_SHIFT           0
#define ARM_MULTIPLY_RN_MASK            0x0000F000
#define ARM_MULTIPLY_RN_SHIFT           12
#define ARM_MULTIPLY_RS_MASK            0x00000F00
#define ARM_MULTIPLY_RS_SHIFT           8
#define ARM_MULTIPLY_RD_HIGH_MASK       0x000F0000
#define ARM_MULTIPLY_RD_HIGH_SHIFT      16
#define ARM_MULTIPLY_RD_LOW_MASK        0x0000F000
#define ARM_MULTIPLY_RD_LOW_SHIFT       12
#define ARM_MLA_MASK                    0x00200000
#define ARM_MUL_MASK                    0x00000000
#define ARM_SMLA_MASK                   0x01000000
#define ARM_SMLXD_MASK                  0x07000000
#define ARM_SMLXD_OPCODE2_MASK          0x000000D0
#define ARM_SMLAD_OPCODE2_VALUE         0x00000010
#define ARM_SMLSD_OPCODE2_VALUE         0x00000050
#define ARM_SMLAW_SMULW_MASK            0x01200000
#define ARM_SMULW_DIFFERENT_BIT         0x00000020
#define ARM_SMLAL_MASK                  0x00E00000
#define ARM_SMLAL_XY_MASK               0x01400000
#define ARM_SDIV_MASK                   0x07100000
#define ARM_UDIV_MASK                   0x07300000
#define ARM_SMLXLD_MASK                 0x07400000
#define ARM_SMLXLD_OPCODE2_MASK         0x000000D0
#define ARM_SMLALD_OPCODE2_VALUE        0x00000010
#define ARM_SMLSLD_OPCODE2_VALUE        0x00000050
#define ARM_SMMLX_MASK                  0x07500000
#define ARM_SMMLX_OPCODE2_MASK          0x000000C0
#define ARM_SMMLA_OPCODE2_VALUE         0x00000000
#define ARM_SMMLS_OPCODE2_VALUE         0x000000C0
#define ARM_SMUL_MASK                   0x01600000
#define ARM_SMULL_MASK                  0x00C00000
#define ARM_UMAAL_MASK                  0x00400000
#define ARM_UMLAL_MASK                  0x00A00000
#define ARM_UMULL_MASK                  0x00800000
#define ARM_MULTIPLY_X_MNEMONIC         "x"
#define ARM_MULTIPLY_ROUND_MNEMONIC     "r"
#define ARM_MULTIPLY_TOP_TOP            "tt"
#define ARM_MULTIPLY_TOP_BOTTOM         "tb"
#define ARM_MULTIPLY_BOTTOM_TOP         "bt"
#define ARM_MULTIPLY_BOTTOM_BOTTOM      "bb"
#define ARM_MULTIPLY_BOTTOM             "b"
#define ARM_MULTIPLY_TOP                "t"
#define ARM_MUL_MNEMONIC                "mul"
#define ARM_MLA_MNEMONIC                "mla"
#define ARM_SMLA_MNEMONIC               "smla"
#define ARM_SMLAD_MNEMONIC              "smlad"
#define ARM_SMLAW_MNEMONIC              "smlaw"
#define ARM_SMLAL_MNEMONIC              "smlal"
#define ARM_SMLALD_MNEMONIC             "smlald"
#define ARM_SMLSD_MNEMONIC              "smlsd"
#define ARM_SMLSLD_MNEMONIC             "smlsld"
#define ARM_SMMLA_MNEMONIC              "smmla"
#define ARM_SMMLS_MNEMONIC              "smmls"
#define ARM_SMMUL_MNEMONIC              "smmul"
#define ARM_SMUAD_MNEMONIC              "smuad"
#define ARM_SMUL_MNEMONIC               "smul"
#define ARM_SMULL_MNEMONIC              "smull"
#define ARM_SMULW_MNEMONIC              "smulw"
#define ARM_SMUSD_MNEMONIC              "smusd"
#define ARM_UMAAL_MNEMONIC              "umaal"
#define ARM_UMLAL_MNEMONIC              "umlal"
#define ARM_UMULL_MNEMONIC              "umull"
#define ARM_SDIV_MNEMONIC               "sdiv"
#define ARM_UDIV_MNEMONIC               "udiv"

//
// Permanently undefined instruction encodings
//

#define ARM_UNDEFINED_INSTRUCTION_MASK  0x0FF000F0
#define ARM_UNDEFINED_INSTRUCTION_VALUE 0x07F000F0
#define ARM_UNDEFINED_INSTRUCTION_MNEMONIC "udf"

//
// Load/store instruction encodings
//

#define ARM_LOAD_STORE_SINGLE_MASK      0x0C000000
#define ARM_LOAD_STORE_SINGLE_VALUE     0x04000000
#define ARM_LOAD_STORE_BYTE_BIT         0x00400000
#define ARM_LOAD_STORE_TRANSLATE_BIT    0x00200000
#define ARM_LOAD_STORE_BASE_MASK        0x000F0000
#define ARM_LOAD_STORE_BASE_SHIFT       16
#define ARM_LOAD_STORE_REGISTER_ZERO_MASK 0x00000010
#define ARM_LOAD_STORE_SHIFT_VALUE_MASK 0x00000F80
#define ARM_LOAD_STORE_SHIFT_VALUE_SHIFT 7
#define ARM_BYTE_TRANSFER_SUFFIX        "b"
#define ARM_TRANSLATE_SUFFIX            "t"
#define ARM_TRANSLATE_BYTE_SUFFIX       "bt"

//
// Extra load/store instruction encodings
//

#define ARM_HALF_WORD_REGISTER_MASK     0x0E400F90
#define ARM_HALF_WORD_REGISTER_VALUE    0x00000090
#define ARM_HALF_WORD_TRANSFER_MASK     0x00100060
#define ARM_STORE_HALF_WORD             0x00000020
#define ARM_LOAD_DOUBLE_WORD            0x00000040
#define ARM_STORE_DOUBLE_WORD           0x00000060
#define ARM_LOAD_UNSIGNED_HALF_WORD     0x00100020
#define ARM_LOAD_SIGNED_BYTE            0x00100040
#define ARM_LOAD_SIGNED_HALF_WORD       0x00100060
#define ARM_HALF_WORD_ILLEGAL_MASK      0x00000060
#define ARM_HALF_WORD_ILLEGAL_VALUE     0x00000000
#define ARM_LOAD_MNEMONIC               "ldr"
#define ARM_STORE_MNEMONIC              "str"
#define ARM_HALF_WORD_SUFFIX            "h"
#define ARM_DOUBLE_WORD_SUFFIX          "d"
#define ARM_SIGNED_HALF_WORD_SUFFIX     "sh"
#define ARM_SIGNED_BYTE_SUFFIX          "sb"

//
// Media extension encondings.
//

#define ARM_MEDIA_MULTIPLY_MASK         0x0F800010
#define ARM_MEDIA_MULTIPLY_VALUE        0x07000010

//
// Parallel arithmetic encodings.
//

#define ARM_PARALLEL_ARITHMETIC_UNSIGNED  0x00400000
#define ARM_PARALLEL_ARITHMETIC_OP1_MASK  0x00300000
#define ARM_PARALLEL_ARITHMETIC_OP1_SHIFT 20
#define ARM_PARALLEL_ARITHMETIC_RN_MASK   0x000F0000
#define ARM_PARALLEL_ARITHMETIC_RN_SHIFT  16
#define ARM_PARALLEL_ARITHMETIC_RD_MASK   0x0000F000
#define ARM_PARALLEL_ARITHMETIC_RD_SHIFT  12
#define ARM_PARALLEL_ARITHMETIC_OP2_MASK  0x000000E0
#define ARM_PARALLEL_ARITHMETIC_OP2_SHIFT 5
#define ARM_PARALLEL_ARITHMETIC_RM_MASK   0x0000000F
#define ARM_PARALLEL_ARITHMETIC_RM_SHIFT  0

#define ARM_PARALLEL_ARITHMETIC_OP_MAX 24

//
// Packing, unpacking, saturation, and reversal instruction encodings.
//

#define ARM_PACKING_OP1_MASK            0x00700000
#define ARM_PACKING_OP1_SHIFT           20
#define ARM_PACKING_SAT_UNSIGNED        0x00400000
#define ARM_PACKING_SAT_IMMEDIATE_MASK  0x001F0000
#define ARM_PACKING_SAT_IMMEDIATE_SHIFT 16
#define ARM_PACKING_RN_MASK             0x000F0000
#define ARM_PACKING_RN_SHIFT            16
#define ARM_PACKING_RD_MASK             0x0000F000
#define ARM_PACKING_RD_SHIFT            12
#define ARM_PACKING_ROTATION_MASK       0x00000C00
#define ARM_PACKING_ROTATION_SHIFT      10
#define ARM_PACKING_IMMEDIATE5_MASK     0x00000F80
#define ARM_PACKING_IMMEDIATE5_SHIFT    7
#define ARM_PACKING_OP2_MASK            0x000000E0
#define ARM_PACKING_OP2_SHIFT           5
#define ARM_PACKING_TB_BIT              0x00000040
#define ARM_PACKING_SHIFT_BIT           0x00000040
#define ARM_PACKING_SAT16_BIT           0x00000020
#define ARM_PACKING_RM_MASK             0x0000000F
#define ARM_PACKING_RM_SHIFT            0

#define ARM_PACKING_OP1_REV_MASK  0x4
#define ARM_PACKING_OP1_REV_SHIFT 2

#define ARM_PACKING_OP2_REV_MASK  0x4
#define ARM_PACKING_OP2_REV_SHIFT 2

#define ARM_PKHBT_MNEMONIC "pkhbt"
#define ARM_PKHTB_MNEMONIC "pkhtb"
#define ARM_SEL_MNEMONIC   "sel"
#define ARM_SAT_MNEMONIC   "sat"
#define ARM_SAT16_MNEMONIC "16"
#define ARM_USAT_MNEMONIC  "u"
#define ARM_SSAT_MNEMONIC  "s"

//
// Load store multiple / block transfer encodings.
//

#define ARM_LOAD_STORE_MULTIPLE_MASK    0x0E000000
#define ARM_LOAD_STORE_MULTIPLE_VALUE   0x08000000
#define ARM_LOAD_STORE_OP_MASK          0x03F00000
#define ARM_LOAD_STORE_OP_POP           0x00B00000
#define ARM_LOAD_STORE_OP_PUSH          0x01200000
#define ARM_LOAD_STORE_TYPE_MASK        0x01800000
#define ARM_LOAD_STORE_INCREMENT_AFTER  0x00800000
#define ARM_LOAD_STORE_INCREMENT_BEFORE 0x01800000
#define ARM_LOAD_STORE_DECREMENT_AFTER  0x00000000
#define ARM_LOAD_STORE_DECREMENT_BEFORE 0x01000000
#define ARM_USE_SAVED_PSR_BIT           0x00400000
#define ARM_LOAD_STORE_REGISTER_MASK    0x000F0000
#define ARM_LOAD_STORE_REGISTER_SHIFT   16
#define ARM_REGISTER_LIST_MASK          0x0000FFFF
#define ARM_LOAD_POP_MNEMONIC           "pop"
#define ARM_STORE_PUSH_MNEMONIC         "push"
#define ARM_LOAD_MULTIPLE_MNEMONIC      "ldm"
#define ARM_STORE_MULTIPLE_MNEMONIC     "stm"
#define ARM_INCREMENT_AFTER_SUFFIX      "ia"
#define ARM_INCREMENT_BEFORE_SUFFIX     "ib"
#define ARM_DECREMENT_AFTER_SUFFIX      "da"
#define ARM_DECREMENT_BEFORE_SUFFIX     "db"

//
// Synchronization encodings
//

#define ARM_SYNCHRONIZATION_OPCODE_MASK         0x00F00000
#define ARM_SYNCHRONIZATION_OPCODE_SHIFT        20
#define ARM_SYNCHRONIZATION_OPCODE_EXCLUSIVE    0x8
#define ARM_SYNCHRONIZATION_OPCODE_LOAD         0x1

#define ARM_SYNCHRONIZATION_RN_MASK             0x000F0000
#define ARM_SYNCHRONIZATION_RN_SHIFT            16
#define ARM_SYNCHRONIZATION_R0_MASK             0x0000000F
#define ARM_SYNCHRONIZATION_R12_MASK            0x0000F000
#define ARM_SYNCHRONIZATION_R12_SHIFT           12

#define ARM_SYNCHRONIZATION_SWAP_BYTE           (1 << 22)
#define ARM_SWP_MNEMONIC                        "swp"
#define ARM_SWPB_MNEMONIC                       "swpb"

//
// Unconditional opcode encodings
//

#define ARM_UNCONDITIONAL_OP1_MASK              0x0FF00000
#define ARM_UNCONDITIONAL_OP1_SHIFT             20
#define ARM_UNCONDITIONAL_RN_MASK               0x000F0000
#define ARM_UNCONDITIONAL_RN_SHIFT              16

#define ARM_UNCONDITIONAL_MEMORY_HINTS_SIMD_MISC_BIT 0x08000000
#define ARM_UNCONDITIONAL_OP1_SRS_MASK          0xE5
#define ARM_UNCONDITIONAL_OP1_SRS_VALUE         0x84
#define ARM_UNCONDITIONAL_OP1_RFE_MASK          0xE5
#define ARM_UNCONDITIONAL_OP1_RFE_VALUE         0x81
#define ARM_UNCONDITIONAL_OP1_BL_MASK           0xE0
#define ARM_UNCONDITIONAL_OP1_BL_VALUE          0xA0
#define ARM_UNCONDITIONAL_OP1_COPROCESSOR_MOVE_MASK 0xF0
#define ARM_UNCONDITIONAL_OP1_COPROCESSOR_MOVE_VALUE 0xE0

#define ARM_BLX_H_BIT (1 << 24)

#define ARM_SRS_MNEMONIC    "srs"
#define ARM_RFE_MNEMONIC    "rfe"
#define ARM_B_MNEMONIC      "b"
#define ARM_BL_MNEMONIC     "bl"
#define ARM_BLX_MNEMONIC    "blx"

//
// Miscellaneous, memory hints, and advanced SIMD encodings
//

#define ARM_MISCELLANEOUS2_OP1_MASK     0x07F00000
#define ARM_MISCELLANEOUS2_OP1_SHIFT    20
#define ARM_MISCELLANEOUS2_OP2_MASK     0x000000F0
#define ARM_MISCELLANEOUS2_OP2_SHIFT    4

#define ARM_MISCELLANEOUS2_OP1_CPS      0x10
#define ARM_MISCELLANEOUS2_OP1_BARRIERS 0x57

#define ARM_MISCELLANEOUS2_OP2_CLREX    0x1
#define ARM_MISCELLANEOUS2_OP2_DSB      0x4
#define ARM_MISCELLANEOUS2_OP2_DMB      0x5
#define ARM_MISCELLANEOUS2_OP2_ISB      0x6

#define ARM_CLREX_MNEMONIC              "clrex"
#define ARM_DSB_MNEMONIC                "dsb"
#define ARM_DMB_MNEMONIC                "dmb"
#define ARM_ISB_MNEMONIC                "isb"

#define ARM_MODE_MASK                   0x0000001F
#define ARM_CPS_FLAG_F                  (1 << 6)
#define ARM_CPS_FLAG_I                  (1 << 7)
#define ARM_CPS_FLAG_A                  (1 << 8)
#define ARM_CPS_FLAG_F_STRING           "f"
#define ARM_CPS_FLAG_I_STRING           "i"
#define ARM_CPS_FLAG_A_STRING           "a"

#define ARM_CPS_IMOD_DISABLE            (1 << 18)
#define ARM_CPS_MNEMONIC_DISABLE        "cpsid"
#define ARM_CPS_MNEMONIC_ENABLE         "cpsie"

#define ARM_SETEND_BIG_ENDIAN            (1 << 9)
#define ARM_SETEND_MNEMONIC              "setend"
#define ARM_SETEND_BE_STRING             "be"
#define ARM_SETEND_LE_STRING             "le"

//
// Coprocessor move definitions
//

#define ARM_COPROCESSOR_REGISTER_MASK   0x0F000000
#define ARM_COPROCESSOR_REGISTER_VALUE  0x0E000000
#define ARM_COPROCESSOR_CDP_BIT         0x00000010
#define ARM_COPROCESSOR_MRC_BIT         0x00100000
#define ARM_COPROCESSOR_MRRC_BIT        0x00100000
#define ARM_COPROCESSOR_NUMBER_MASK     0x00000F00
#define ARM_COPROCESSOR_NUMBER_SHIFT    8
#define ARM_CDP_OPCODE1_MASK            0x00F00000
#define ARM_CDP_OPCODE1_SHIFT           20
#define ARM_MCR_MRC_OPCODE1_MASK        0x00E00000
#define ARM_MCR_MRC_OPCODE1_SHIFT       21
#define ARM_MCRR_MRRC_OPCODE1_MASK      0x000000F0
#define ARM_MCRR_MRRC_OPCODE1_SHIFT     4
#define ARM_COPROCESSOR_OPCODE2_MASK    0x000000E0
#define ARM_COPROCESSOR_OPCODE2_SHIFT   5
#define ARM_COPROCESSOR_RN_MASK         0x000F0000
#define ARM_COPROCESSOR_RN_SHIFT        16
#define ARM_COPROCESSOR_RM_MASK         0x0000000F
#define ARM_COPROCESSOR_RM_SHIFT        0
#define ARM_CDP_MNEMONIC                "cdp"
#define ARM_MRC_MNEMONIC                "mrc"
#define ARM_MCR_MNEMONIC                "mcr"
#define ARM_MCRR_MNEMONIC               "mcrr"
#define ARM_MRRC_MNEMONIC               "mrrc"

//
// Coprocessor data definitions
//

#define ARM_COPROCESSOR_DATA_MASK       0x0E000000
#define ARM_COPROCESSOR_DATA_VALUE      0x0C000000
#define ARM_COPROCESSOR_DATA_LONG_BIT   0x00400000
#define ARM_COPROCESSOR_DATA_DESTINATION_MASK  0x0000F000
#define ARM_COPROCESSOR_DATA_DESTINATION_SHIFT 12
#define ARM_COPROCESSOR_LOAD_MNEMONIC   "ldc"
#define ARM_COPROCESSOR_STORE_MNEMONIC  "stc"
#define ARM_COPROCESSOR_LONG_MNEMONIC   "l"

//
// Supervisor call encodings
//

#define ARM_SUPERVISOR_OP1_MASK         0x03F00000
#define ARM_SUPERVISOR_OP1_SHIFT        20
#define ARM_SUPERVISOR_OP1_REGISTER_BIT 0x20

#define ARM_SUPERVISOR_COPROCESSOR_MASK 0x00000F00
#define ARM_SUPERVISOR_COPROCESSOR_SHIFT 8

#define ARM_SUPERVISOR_COPROCESSOR_MATH_MASK 0xE
#define ARM_SUPERVISOR_COPROCESSOR_MATH_VALUE 0xA

#define ARM_SUPERVISOR_SVC_MASK         0x30
#define ARM_SUPERVISOR_SVC_VALUE        0x30
#define ARM_SVC_MNEMONIC                "svc"
#define ARM_IMMEDIATE24_MASK            0x00FFFFFF

//
// Program Status Register modes.
//

#define ARM_MODE_USER                   0x00000010
#define ARM_MODE_FIQ                    0x00000011
#define ARM_MODE_IRQ                    0x00000012
#define ARM_MODE_SVC                    0x00000013
#define ARM_MODE_ABORT                  0x00000017
#define ARM_MODE_UNDEF                  0x0000001B
#define ARM_MODE_SYSTEM                 0x0000001F
#define ARM_MODE_MASK                   0x0000001F
#define ARM_MODE_USER_STRING            "usr"
#define ARM_MODE_FIQ_STRING             "fiq"
#define ARM_MODE_IRQ_STRING             "irq"
#define ARM_MODE_SVC_STRING             "svc"
#define ARM_MODE_ABORT_STRING           "abt"
#define ARM_MODE_UNDEF_STRING           "undef"
#define ARM_MODE_SYSTEM_STRING          "sys"

//
// Memory barrier modes
//

#define ARM_BARRIER_MODE_MASK           0x0000000F
#define ARM_BARRIER_MODE_FULL           0xF
#define ARM_BARRIER_MODE_ST             0xE
#define ARM_BARRIER_MODE_ISH            0xB
#define ARM_BARRIER_MODE_ISHST          0xA
#define ARM_BARRIER_MODE_NSH            0x7
#define ARM_BARRIER_MODE_NSHST          0x6
#define ARM_BARRIER_MODE_OSH            0x3
#define ARM_BARRIER_MODE_OSHST          0x2
#define ARM_BARRIER_MODE_FULL_STRING    ""
#define ARM_BARRIER_MODE_ST_STRING      "st"
#define ARM_BARRIER_MODE_ISH_STRING     "ish"
#define ARM_BARRIER_MODE_ISHST_STRING   "ishst"
#define ARM_BARRIER_MODE_NSH_STRING     "nsh"
#define ARM_BARRIER_MODE_NSHST_STRING   "nshst"
#define ARM_BARRIER_MODE_OSH_STRING     "osh"
#define ARM_BARRIER_MODE_OSHST_STRING   "oshst"

//
// Banked register mask
//

#define ARM_BANKED_MODE_R_BIT   (1 << 22)
#define ARM_BANKED_MODE_MASK    0x000F0000
#define ARM_BANKED_MODE_SHIFT   16

//
// Branch class encodings.
//

#define ARM_BRANCH_CLASS_BIT    0x02000000
#define ARM_BRANCH_LINK_BIT     0x01000000

//
// Preload instruction encodings.
//

#define ARM_PRELOAD_REGISTER_BIT      0x02000000
#define ARM_PRELOAD_DATA_BIT          0x01000000
#define ARM_PRELOAD_ADD_BIT           0x00800000
#define ARM_PRELOAD_DATA_READ_BIT     0x00400000
#define ARM_PRELOAD_RN_MASK           0x000F0000
#define ARM_PRELOAD_RN_SHIFT          16
#define ARM_PRELOAD_IMMEDIATE5_MASK   0x00000F80
#define ARM_PRELOAD_IMMEDIATE5_SHIFT  7
#define ARM_PRELOAD_IMMEDIATE12_MASK  0x00000FFF
#define ARM_PRELOAD_IMMEDIATE12_SHIFT 0
#define ARM_PRELOAD_RM_MASK           0x0000000F
#define ARM_PRELOAD_RM_SHIFT          0

#define ARM_PRELOAD_MNEMONIC "pli"
#define ARM_PRELOAD_DATA_MNEMONIC "pld"

//
// Unsigned sum of absolute differences instruction encodings.
//

#define ARM_USAD_RD_MASK  0x000F0000
#define ARM_USAD_RD_SHIFT 16
#define ARM_USAD_RA_MASK  0x0000F000
#define ARM_USAD_RA_SHIFT 12
#define ARM_USAD_RM_MASK  0x00000F00
#define ARM_USAD_RM_SHIFT 8
#define ARM_USAD_RN_MASK  0x0000000F
#define ARM_USAD_RN_SHIFT 0

#define ARM_USAD_MNEMONIC  "usad8"
#define ARM_USADA_MNEMONIC "usada8"

//
// Bit field instruction encodings.
//

#define ARM_BIT_FIELD_UNSIGNED_BIT        0x00400000
#define ARM_BIT_FIELD_EXTRACT_BIT         0x00200000
#define ARM_BIT_FIELD_WIDTH_MINUS_1_MASK  0x001F0000
#define ARM_BIT_FIELD_WIDTH_MINUS_1_SHIFT 16
#define ARM_BIT_FIELD_RD_MASK             0x0000F000
#define ARM_BIT_FIELD_RD_SHIFT            12
#define ARM_BIT_FIELD_LSB_MASK            0x00000F80
#define ARM_BIT_FIELD_LSB_SHIFT           7
#define ARM_BIT_FIELD_RN_MASK             0x0000000F
#define ARM_BIT_FIELD_RN_SHIFT            0

#define ARM_SBFX_MNEMONIC "sbfx"
#define ARM_UBFX_MNEMONIC "ubfx"
#define ARM_BFC_MNEMONIC  "bfc"
#define ARM_BFI_MNEMONIC  "bfi"

//
// Define the SIMD and floating point mnemonics.
//

#define ARM_VMLA_MNEMONIC  "vmla"
#define ARM_VMLS_MNEMONIC  "vmls"
#define ARM_VNMLA_MNEMONIC "vnmla"
#define ARM_VNMLS_MNEMONIC "vnmls"
#define ARM_VNMUL_MNEMONIC "vnmul"
#define ARM_VMUL_MNEMONIC  "vmul"
#define ARM_VADD_MNEMONIC  "vadd"
#define ARM_VSUB_MNEMONIC  "vsub"
#define ARM_VDIV_MNEMONIC  "vdic"
#define ARM_VFNMA_MNEMONIC "vfnma"
#define ARM_VFNMS_MNEMONIC "vfnms"
#define ARM_VFMA_MNEMONIC  "vfma"
#define ARM_VFMS_MNEMONIC  "vfms"
#define ARM_VMOV_MNEMONIC  "vmov"
#define ARM_VABS_MNEMONIC  "vabs"
#define ARM_VNEG_MNEMONIC  "vneg"
#define ARM_VSQRT_MNEMONIC "vsqrt"
#define ARM_VCMP_MNEMONIC  "vcmp"
#define ARM_VCMPE_MNEMONIC "vcmpe"
#define ARM_VCVT_MNEMONIC  "vcvt"
#define ARM_VMSR_MNEMONIC  "vmsr"
#define ARM_VMRS_MNEMONIC  "vmrs"
#define ARM_VDUP_MNEMONIC  "vdup"
#define ARM_VST_MNEMONIC   "vst"
#define ARM_VLD_MNEMONIC   "vld"
#define ARM_VPOP_MNEMONIC  "vpop"
#define ARM_VPUSH_MNEMONIC "vpush"

//
// Define SIMD and floating point precondition mnemonics.
//

#define ARM_FLOATING_POINT_TOP      "t"
#define ARM_FLOATING_POINT_BOTTOM   "b"
#define ARM_FLOATING_POINT_ROUNDING "r"
#define ARM_FLOATING_POINT_REGISTER "r"
#define ARM_FLOATING_POINT_MULTIPLE "m"

//
// Floating point data processing encodings.
//

#define ARM_FLOATING_POINT_OP1_MASK  0x00F00000
#define ARM_FLOATING_POINT_OP1_SHIFT 20
#define ARM_FLOATING_POINT_OP2_MASK  0x000F0000
#define ARM_FLOATING_POINT_OP2_SHIFT 16
#define ARM_FLOATING_POINT_OP3_MASK  0x000000C0
#define ARM_FLOATING_POINT_OP3_SHIFT 6
#define ARM_FLOATING_POINT_OP4_MASK  0x0000000F
#define ARM_FLOATING_POINT_OP4_SHIFT 0

//
// Floating point instruction encodings within opcode 1.
//

#define ARM_FLOATING_POINT_INSTRUCTION_MASK        0x00B00000
#define ARM_FLOATING_POINT_INSTRUCTION_OTHER       0x00B00000
#define ARM_FLOATING_POINT_INSTRUCTION_VFMA_VFMS   0x00A00000
#define ARM_FLOATING_POINT_INSTRUCTION_VFNMA_VFNMS 0x00900000
#define ARM_FLOATING_POINT_INSTRUCTION_VDIV        0x00800000
#define ARM_FLOATING_POINT_INSTRUCTION_VADD_VSUB   0x00300000
#define ARM_FLOATING_POINT_INSTRUCTION_VMUL_VNMUL  0x00200000
#define ARM_FLOATING_POINT_INSTRUCTION_VNMLA_VNMLS 0x00100000
#define ARM_FLOATING_POINT_INSTRUCTION_VMLA_VMLS   0x00000000

//
// Floating point two register instrutions mask.
//

#define ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_MASK  0x000F00C0
#define ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VMOV  0x00000040
#define ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VABS  0x000000C0
#define ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VNEG  0x00010040
#define ARM_FLOATING_POINT_TWO_REGISTER_INSTRUCTION_VSQRT 0x000100C0

//
// Floating point vector convert instructions.
//

#define ARM_VCVT_MASK                           0x000F00C0
#define ARM_VCVT_TOP                            0x000200C0
#define ARM_VCVT_BOTTOM                         0x00020040
#define ARM_VCVT_SINGLE_TO_HALF                 0x00010000
#define ARM_VCVT_HALF_TO_SINGLE                 0x00000000
#define ARM_VCVT_FLOAT_TO_FLOAT                 0x000700C0
#define ARM_VCVT_FLOAT_TO_INTEGER               0x000C0040
#define ARM_VCVT_FLOAT_TO_INTEGER_SIGNED        0x00010000
#define ARM_VCVT_FLOAT_TO_INTEGER_ROUND_TO_ZERO 0x00000080
#define ARM_VCVT_INTEGER_TO_FLOAT               0x00080040
#define ARM_VCVT_INTEGER_TO_FLOAT_SIGNED        0x00000080
#define ARM_VCVT_FIXED_TO_FLOAT                 0x000A0040
#define ARM_VCVT_FIXED_UNSIGNED_TO_FLOAT        0x00010000
#define ARM_VCVT_FIXED_32_TO_FLOAT              0x00000080
#define ARM_VCVT_FLOAT_TO_FIXED                 0x000E0040
#define ARM_VCVT_FLOAT_TO_FIXED_UNSIGNED        0x00010000
#define ARM_VCVT_FLOAT_TO_FIXED_32              0x00000080
#define ARM_VCVT_DOUBLE                         0x00000100

//
// Floating point instruction encodings for option 2 with a mask.
//

#define ARM_FLOATING_POINT_OP2_VCVT_VCMP_MASK     0xE
#define ARM_FLOATING_POINT_OP2_VCVT_FP_TO_FIXED   0xE
#define ARM_FLOATING_POINT_OP2_VCVT_FIXED_TO_FP   0xA
#define ARM_FLOATING_POINT_OP2_VCVT_FP_TO_INTEGER 0xC
#define ARM_FLOATING_POINT_OP2_VCMP               0x4
#define ARM_FLOATING_POINT_OP2_VCVT_BOTTOM_TOP    0x2

//
// Floating point instruction encodings for option 2 without a mask.
//

#define ARM_FLOATING_POINT_OP2_VCVT_INTEGER_TO_FP 0x8
#define ARM_FLOATING_POINT_OP2_VCVT_DP_SP         0x7
#define ARM_FLOATING_POINT_OP2_VNEG_VSQRT         0x1
#define ARM_FLOATING_POINT_OP2_VMOV_VABS          0x0

//
// Floating point instruction encodings bits for option 3.
//

#define ARM_FLOATING_POINT_OP3_NOT_VDIV   0x1
#define ARM_FLOATING_POINT_OP3_VSUB       0x1
#define ARM_FLOATING_POINT_OP3_VMUL       0x1
#define ARM_FLOATING_POINT_OP3_NOT_VMOV   0x1
#define ARM_FLOATING_POINT_OP3_VABS       0x2
#define ARM_FLOATING_POINT_OP3_VSQRT      0x2
#define ARM_FLOATING_POINT_OP3_VCVT_DP_SP 0x3

//
// Floating point instruction encoding bits. These values account for various
// different incodings for the 32-bit of each floating point instruction.
//

#define ARM_FLOATING_POINT_D_BIT                 0x00400000
#define ARM_FLOATING_POINT_TO_INTEGER            0x00040000
#define ARM_FLOATING_POINT_FIXED_POINT_OP_BIT    0x00040000
#define ARM_FLOATING_POINT_UNSIGNED              0x00010000
#define ARM_FLOATING_POINT_SIGNED                0x00010000
#define ARM_FLOATING_POINT_VCVTB_OP_BIT          0x00010000
#define ARM_FLOATING_POINT_VCMP_ZERO             0x00010000
#define ARM_FLOATING_POINT_VN_MASK               0x000F0000
#define ARM_FLOATING_POINT_VN_SHIFT              16
#define ARM_FLOATING_POINT_IMMEDIATE4_HIGH_MASK  0x000F0000
#define ARM_FLOATING_POINT_IMMEDIATE4_HIGH_SHIFT 16
#define ARM_FLOATING_POINT_VD_MASK               0x0000F000
#define ARM_FLOATING_POINT_VD_SHIFT              12
#define ARM_FLOATING_POINT_SZ_BIT                0x00000100
#define ARM_FLOATING_POINT_SF_BIT                0x00000100
#define ARM_FLOATING_POINT_SX_BIT                0x00000080
#define ARM_FLOATING_POINT_N_BIT                 0x00000080
#define ARM_FLOATING_POINT_VCVT_TOP_BIT          0x00000080
#define ARM_FLOATING_POINT_VCMP_E_BIT            0x00000080
#define ARM_FLOATING_POINT_VCVT_OP_BIT           0x00000080
#define ARM_FLOATING_POINT_OP_BIT                0x00000040
#define ARM_FLOATING_POINT_M_BIT                 0x00000020
#define ARM_FLOATING_POINT_I_BIT                 0x00000020
#define ARM_FLOATING_POINT_VM_MASK               0x0000000F
#define ARM_FLOATING_POINT_VM_SHIFT              0
#define ARM_FLOATING_POINT_IMMEDIATE4_LOW_MASK   0x0000000F
#define ARM_FLOATING_POINT_IMMEDIATE4_LOW_SHIFT  0

#define ARM_FLOATING_POINT_QUADWORD_VECTOR         "q"
#define ARM_FLOATING_POINT_DOUBLE_PRECISION_VECTOR "d"
#define ARM_FLOATING_POINT_SINGLE_PRECISION_VECTOR "s"
#define ARM_FLOATING_POINT_DOUBLE_PRECISION_SUFFIX ".f64"
#define ARM_FLOATING_POINT_SINGLE_PRECISION_SUFFIX ".f32"
#define ARM_FLOATING_POINT_HALF_PRECISION_SUFFIX   ".f16"
#define ARM_FLOATING_POINT_SIGNED_INTEGER_SUFFIX   ".s32"
#define ARM_FLOATING_POINT_UNSIGNED_INTEGER_SUFFIX ".u32"
#define ARM_FLOATING_POINT_SIGNED_HALF_SUFFIX      ".s16"
#define ARM_FLOATING_POINT_UNSIGNED_HALF_SUFFIX    ".u16"

//
// SIMD and floating point small transfer encodings.
//

#define ARM_SIMD_TRANSFER_SCALAR_UNSIGNED 0x00800000
#define ARM_SIMD_TRANSFER_DUP_QUADWORD    0x00200000
#define ARM_SIMD_TRANSFER_TO_REGISTER     0x00100000
#define ARM_SIMD_TRANSFER_VECTOR_MASK     0x000F0000
#define ARM_SIMD_TRANSFER_VECTOR_SHIFT    16
#define ARM_SIMD_TRANSFER_SPECIAL_MASK    0x000F0000
#define ARM_SIMD_TRANSFER_SPECIAL_SHIFT   16
#define ARM_SIMD_TRANSFER_REGISTER_MASK   0x0000F000
#define ARM_SIMD_TRANSFER_REGISTER_SHIFT  12
#define ARM_SIMD_TRANSFER_MOVE_SCALAR     0x00000100
#define ARM_SIMD_TRANSFER_VECTOR_BIT      0x00000080

#define ARM_SIMD_TRANSFER_SCALAR_SIZE_8_MASK   0x8
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_8_VALUE  0x8
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_8_SHIFT  0
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_16_MASK  0x9
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_16_VALUE 0x1
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_16_SHIFT 1
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_32_MASK  0xB
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_32_VALUE 0x0
#define ARM_SIMD_TRANSFER_SCALAR_SIZE_32_SHIFT 2

#define ARM_SIMD_TRANSFER_DUP_SIZE_8  0x2
#define ARM_SIMD_TRANSFER_DUP_SIZE_16 0x1
#define ARM_SIMD_TRANSFER_DUP_SIZE_32 0x0

#define ARM_SIMD_APSR_REGISTER "APSR_nzcv"

#define ARM_SIMD_DATA_DEFAULT    "."
#define ARM_SIMD_DATA_SIGNED     ".s"
#define ARM_SIMD_DATA_UNSIGNED   ".u"
#define ARM_SIMD_DATA_INTEGER    ".i"
#define ARM_SIMD_DATA_POLYNOMIAL ".p"
#define ARM_SIMD_DATA_FLOAT      ".f"
#define ARM_SIMD_DATA_SIZE_8     "8"
#define ARM_SIMD_DATA_SIZE_16    "16"
#define ARM_SIMD_DATA_SIZE_32    "32"
#define ARM_SIMD_DATA_SIZE_64    "64"

#define ARM_SIMD_ALIGN_16  ":16"
#define ARM_SIMD_ALIGN_32  ":32"
#define ARM_SIMD_ALIGN_64  ":64"
#define ARM_SIMD_ALIGN_128 ":128"
#define ARM_SIMD_ALIGN_256 ":256"

//
// SIMD and floating point 64-bit transfer encodings.
//

#define ARM_SIMD_TRANSFER_64_TO_REGISTER  0x00100000
#define ARM_SIMD_TRANSFER_64_RT2_MASK     0x000F0000
#define ARM_SIMD_TRANSFER_64_RT2_SHIFT    16
#define ARM_SIMD_TRANSFER_64_RT_MASK      0x0000F000
#define ARM_SIMD_TRANSFER_64_RT_SHIFT     12
#define ARM_SIMD_TRANSFER_64_DOUBLE       0x00000100
#define ARM_SIMD_TRANSFER_64_VECTOR_BIT   0x00000020
#define ARM_SIMD_TRANSFER_64_VECTOR_MASK  0x0000000F
#define ARM_SIMD_TRANSFER_64_VECTOR_SHIFT 0

//
// SIMD and floating point load/store encodings.
//

#define ARM_SIMD_LOAD_STORE_OP_MASK          0x01B00000
#define ARM_SIMD_LOAD_STORE_OP_VPUSH         0x01200000
#define ARM_SIMD_LOAD_STORE_OP_VPOP          0x00B00000
#define ARM_SIMD_LOAD_STORE_ADD_BIT          0x00800000
#define ARM_SIMD_LOAD_STORE_VECTOR_BIT       0x00400000
#define ARM_SIMD_LOAD_STORE_REGISTER_MASK    0x000F0000
#define ARM_SIMD_LOAD_STORE_REGISTER_SHIFT   16
#define ARM_SIMD_LOAD_STORE_VECTOR_MASK      0x0000F000
#define ARM_SIMD_LOAD_STORE_VECTOR_SHIFT     12
#define ARM_SIMD_LOAD_STORE_DOUBLE           0x00000100
#define ARM_SIMD_LOAD_STORE_IMMEDIATE8_MASK  0x000000FF
#define ARM_SIMD_LOAD_STORE_IMMEDIATE8_SHIFT 0

//
// SIMD element or structure load/store encodings.
//

#define ARM_SIMD_ELEMENT_LOAD_BIT                0x00200000
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE       0x00800000
#define ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_BIT   0x00400000
#define ARM_SIMD_ELEMENT_LOAD_STORE_RN_MASK      0x000F0000
#define ARM_SIMD_ELEMENT_LOAD_STORE_RN_SHIFT     16
#define ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_MASK  0x0000F000
#define ARM_SIMD_ELEMENT_LOAD_STORE_VECTOR_SHIFT 12
#define ARM_SIMD_ELEMENT_LOAD_STORE_TYPE_MASK    0x00000F00
#define ARM_SIMD_ELEMENT_LOAD_STORE_TYPE_SHIFT   8
#define ARM_SIMD_ELEMENT_LOAD_STORE_RM_MASK      0x0000000F
#define ARM_SIMD_ELEMENT_LOAD_STORE_RM_SHIFT     0

#define ARM_SIMD_ELEMENT_LOAD_STORE_1_ELEMENT_SUFFIX "1"
#define ARM_SIMD_ELEMENT_LOAD_STORE_2_ELEMENT_SUFFIX "2"
#define ARM_SIMD_ELEMENT_LOAD_STORE_3_ELEMENT_SUFFIX "3"
#define ARM_SIMD_ELEMENT_LOAD_STORE_4_ELEMENT_SUFFIX "4"

#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_MASK           0x00000C00
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8              0x00000000
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16             0x00000400
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32             0x00000800
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_MASK        0x00000300
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_1           0x00000000
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_2           0x00000100
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_3           0x00000200
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_ELEMENT_4           0x00000300
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_INDEX_MASK   0x000000E0
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_INDEX_SHIFT  5
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_ALIGN_MASK   0x00000010
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_8_ALIGN_SHIFT  4
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INDEX_MASK  0x000000C0
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INDEX_SHIFT 6
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_INCREMENT   0x00000020
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_ALIGN_MASK  0x00000010
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_16_ALIGN_SHIFT 4
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INDEX_MASK  0x00000080
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INDEX_SHIFT 7
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_INCREMENT   0x00000040
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_MASK  0x00000030
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_SHIFT 4
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_32    0x3
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_64    0x1
#define ARM_SIMD_ELEMENT_LOAD_STORE_SINGLE_SIZE_32_ALIGN_128   0x2

#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_TYPE_MASK   0x00000F00
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_TYPE_SHIFT  8
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_INCREMENT   0x00000100
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_MASK   0x000000C0
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_8      0x00000000
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_16     0x00000010
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_32     0x00000020
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_SIZE_64     0x00000030
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_MASK  0x00000030
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_64    0x00000010
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_128   0x00000020
#define ARM_SIMD_ELEMENT_LOAD_STORE_MULTIPLE_ALIGN_256   0x00000030

#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_MASK 0x000000C0
#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_8    0x00000000
#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_16   0x00000040
#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_SIZE_32   0x00000080
#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_TWO_BIT   0x00000020
#define ARM_SIMD_ELEMENT_LOAD_ALL_LANES_ALIGN     0x00000010

//
// SIMD data processing instruction encodings.
//

#define ARM_SIMD_DATA_PROCESSING_UNSIGNED         0x01000000
#define ARM_SIMD_DATA_PROCESSING_VD_BIT           0x00400000
#define ARM_SIMD_DATA_PROCESSING_VN_MASK          0x000F0000
#define ARM_SIMD_DATA_PROCESSING_VN_SHIFT         16
#define ARM_SIMD_DATA_PROCESSING_VD_MASK          0x0000F000
#define ARM_SIMD_DATA_PROCESSING_VD_SHIFT         12
#define ARM_SIMD_DATA_PROCESSING_VN_BIT           0x00000080
#define ARM_SIMD_DATA_PROCESSING_QUADWORD         0x00000040
#define ARM_SIMD_DATA_PROCESSING_VM_BIT           0x00000020
#define ARM_SIMD_DATA_PROCESSING_VM_MASK          0x0000000F
#define ARM_SIMD_DATA_PROCESSING_VM_SHIFT         0

//
// SIMD data processing instruction encodings for instructions with three
// vectors of the same length.
//

#define ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_MASK       0x00300000
#define ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_8          0x00000000
#define ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_16         0x00100000
#define ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_32         0x00200000
#define ARM_SIMD_DATA_PROCESSING_3_SAME_SIZE_64         0x00300000
#define ARM_SIMD_DATA_PROCESSING_3_SAME_OPERATION_MASK  0x00000F10

#define ARM_SIMD_DATA_PROCESSING_VFM_SUBTRACT     0x00200000

#define ARM_SIMD_VHADD_MASK         0x00000000
#define ARM_SIMD_VQADD_MASK         0x00000010
#define ARM_SIMD_VRHADD_MASK        0x00000100
#define ARM_SIMD_BITWISE_MASK       0x00000110
#define ARM_SIMD_VHSUB_MASK         0x00000200
#define ARM_SIMD_VQSUB_MASK         0x00000210
#define ARM_SIMD_VCGT_MASK          0x00000300
#define ARM_SIMD_VCGE_MASK          0x00000310
#define ARM_SIMD_VSHL_REG_MASK      0x00000400
#define ARM_SIMD_VQSHL_REG_MASK     0x00000410
#define ARM_SIMD_VRSHL_MASK         0x00000500
#define ARM_SIMD_VQRSHL_MASK        0x00000510
#define ARM_SIMD_VMAX_INT_MASK      0x00000600
#define ARM_SIMD_VMIN_INT_MASK      0x00000610
#define ARM_SIMD_VABD_MASK          0x00000700
#define ARM_SIMD_VABA_MASK          0x00000710
#define ARM_SIMD_VADD_INT_MASK      0x00000800
#define ARM_SIMD_VTST_MASK          0x00000810
#define ARM_SIMD_VMLA_MASK          0x00000900
#define ARM_SIMD_VMUL_MASK          0x00000910
#define ARM_SIMD_VPMAX_INT_MASK     0x00000A00
#define ARM_SIMD_VPMIN_INT_MASK     0x00000A10
#define ARM_SIMD_VQDMULH_MASK       0x00000B00
#define ARM_SIMD_VPADD_INT_MASK     0x00000B10
#define ARM_SIMD_VFMA_MASK          0x00000C10
#define ARM_SIMD_FP_MATH_MASK       0x00000D00
#define ARM_SIMD_FP_MULT            0x00000010
#define ARM_SIMD_COMPARE_MASK       0x00000E00
#define ARM_SIMD_ABSOLUTE           0x00000010
#define ARM_SIMD_MIN_MAX_FLOAT_MASK 0x00000F00
#define ARM_SIMD_RECIPROCOL_MASK    0x00000F10

#define ARM_SIMD_BITWISE_OP_MASK    0x01300000
#define ARM_SIMD_BITWISE_VAND_VALUE 0x00000000
#define ARM_SIMD_BITWISE_VBIC_VALUE 0x00100000
#define ARM_SIMD_BITWISE_VORR_VALUE 0x00200000
#define ARM_SIMD_BITWISE_VORN_VALUE 0x00300000
#define ARM_SIMD_BITWISE_VEOR_VALUE 0x01000000
#define ARM_SIMD_BITWISE_VBSL_VALUE 0x01100000
#define ARM_SIMD_BITWISE_VBIT_VALUE 0x01200000
#define ARM_SIMD_BITWISE_VBIF_VALUE 0x01300000

#define ARM_SIMD_FP_MATH_OP_MASK     0x01200010
#define ARM_SIMD_FP_MATH_VADD_VALUE  0x00000000
#define ARM_SIMD_FP_MATH_VSUB_VALUE  0x00200000
#define ARM_SIMD_FP_MATH_VPADD_VALUE 0x01000000
#define ARM_SIMD_FP_MATH_VABD_VALUE  0x01200000
#define ARM_SIMD_FP_MATH_VMLA_VALUE  0x00000010
#define ARM_SIMD_FP_MATH_VMLS_VALUE  0x00200010
#define ARM_SIMD_FP_MATH_VMUL_VALUE  0x01000010

#define ARM_SIMD_COMPARE_OP_MASK     0x01200010
#define ARM_SIMD_COMPARE_VCEQ_VALUE  0x00000000
#define ARM_SIMD_COMPARE_VCGE_VALUE  0x01000000
#define ARM_SIMD_COMPARE_VCGT_VALUE  0x01200000
#define ARM_SIMD_COMPARE_VACGE_VALUE 0x01000010
#define ARM_SIMD_COMPARE_VACGT_VALUE 0x01200010

#define ARM_SIMD_MIN_MAX_FLOAT_OP_MASK     0x01200000
#define ARM_SIMD_MIN_MAX_FLOAT_VMAX_VALUE  0x00000000
#define ARM_SIMD_MIN_MAX_FLOAT_VMIN_VALUE  0x00200000
#define ARM_SIMD_MIN_MAX_FLOAT_VPMAX_VALUE 0x01000000
#define ARM_SIMD_MIN_MAX_FLOAT_VPMIN_VALUE 0x01200000

#define ARM_SIMD_RECIPROCOL_OP_MASK       0x01200000
#define ARM_SIMD_RECIPROCOL_VRECPS_VALUE  0x00000000
#define ARM_SIMD_RECIPROCOL_VRSQRTS_VALUE 0x00200000

#define ARM_VHADD_MNEMONIC    "vhadd"
#define ARM_VHSUB_MNEMONIC    "vhsub"
#define ARM_VQADD_MNEMONIC    "vqadd"
#define ARM_VRHADD_MNEMONIC   "vrhadd"
#define ARM_VAND_MNEMONIC     "vand"
#define ARM_VBIC_MNEMONIC     "vbic"
#define ARM_VORR_MNEMONIC     "vorr"
#define ARM_VORN_MNEMONIC     "vorn"
#define ARM_VEOR_MNEMONIC     "veor"
#define ARM_VBSL_MNEMONIC     "vbsl"
#define ARM_VBIT_MNEMONIC     "vbit"
#define ARM_VBIF_MNEMONIC     "vbif"
#define ARM_VQSUB_MNEMONIC    "vqsub"
#define ARM_VCGT_MNEMONIC     "vcgt"
#define ARM_VCGE_MNEMONIC     "vcge"
#define ARM_VSHL_MNEMONIC     "vshl"
#define ARM_VQSHL_MNEMONIC    "vqshl"
#define ARM_VRSHL_MNEMONIC    "vrshl"
#define ARM_VQRSHL_MNEMONIC   "vqrshl"
#define ARM_VMAX_MNEMONIC     "vmax"
#define ARM_VMIN_MNEMONIC     "vmin"
#define ARM_VABD_MNEMONIC     "vabd"
#define ARM_VABA_MNEMONIC     "vaba"
#define ARM_VTST_MNEMONIC     "vtst"
#define ARM_VCEQ_MNEMONIC     "vceq"
#define ARM_VPMAX_MNEMONIC    "vpmax"
#define ARM_VPMIN_MNEMONIC    "vpmin"
#define ARM_VQDMULH_MNEMONIC  "vqdmulh"
#define ARM_VQRDMULH_MNEMONIC "vqrdmulh"
#define ARM_VPADD_MNEMONIC    "vpadd"
#define ARM_VCEQ_MNEMONIC     "vceq"
#define ARM_VCGE_MNEMONIC     "vcge"
#define ARM_VCGT_MNEMONIC     "vcgt"
#define ARM_VACGE_MNEMONIC    "vacge"
#define ARM_VACGT_MNEMONIC    "vacgt"
#define ARM_VACLE_MNEMONIC    "vacle"
#define ARM_VACLT_MNEMONIC    "vaclt"
#define ARM_VMAX_MNEMONIC     "vmax"
#define ARM_VMIN_MNEMONIC     "vmin"
#define ARM_VRECPS_MNEMONIC   "vrecps"
#define ARM_VRSQRTS_MNEMONIC  "vrsqrts"

//
// SIMD instruction encodings for instructions with one register and a modified
// immediate value.
//

#define ARM_SIMD_DATA_PROCESSING_1_REGISTER_CMODE_MASK  0x00000F00
#define ARM_SIMD_DATA_PROCESSING_1_REGISTER_CMODE_SHIFT 8
#define ARM_SIMD_DATA_PROCESSING_1_REGISTER_OP_BIT      0x00000020

#define ARM_SIMD_CMODE_TYPE_MASK           0xE
#define ARM_SIMD_CMODE_TYPE_I32_NO_SHIFT   0x0
#define ARM_SIMD_CMODE_TYPE_I32_SHIFT_8    0x2
#define ARM_SIMD_CMODE_TYPE_I32_SHIFT_16   0x4
#define ARM_SIMD_CMODE_TYPE_I32_SHIFT_24   0x6
#define ARM_SIMD_CMODE_TYPE_I16_NO_SHIFT   0x8
#define ARM_SIMD_CMODE_TYPE_I16_SHIFT_8    0xA
#define ARM_SIMD_CMODE_TYPE_I32_SHIFT_ONES 0xC
#define ARM_SIMD_CMODE_SHIFT_ONES_16       0x1
#define ARM_SIMD_CMODE_FLOAT_32            0x1
#define ARM_SIMD_CMODE_UNDEFINED           0x1

#define ARM_SIMD_CMODE_NO_OP_VORR_MAX 0xC
#define ARM_SIMD_CMODE_NO_OP_VORR_BIT 0x1

#define ARM_SIMD_CMODE_OP_VBIC_MAX  0xC
#define ARM_SIMD_CMODE_OP_VBIC_BIT  0x1
#define ARM_SIMD_CMODE_OP_VMOV      0xE
#define ARM_SIMD_CMODE_OP_UNDEFINED 0xF

#define ARM_VMVN_MNEMONIC "vmvn"

//
// SIMD instruction encodings for instructions with two registers and a shift
// amount.
//

#define ARM_SIMD_2_REGISTER_SHIFT_IMMEDIATE6_MASK  0x003F0000
#define ARM_SIMD_2_REGISTER_SHIFT_IMMEDIATE6_SHIFT 16
#define ARM_SIMD_2_REGISTER_SHIFT_64               0x00000080
#define ARM_SIMD_2_REGISTER_SHIFT_OPERATION_MASK   0x00000F00

#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_32      0x20
#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_32_MASK 0x1F
#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_16      0x10
#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_16_MASK 0x0F
#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_8       0x08
#define ARM_SIMD_2_REGISTER_SHIFT_SIZE_8_MASK  0x07

#define ARM_SIMD_VSHR_MASK          0x00000000
#define ARM_SIMD_VSRA_MASK          0x00000100
#define ARM_SIMD_VRSHR_MASK         0x00000200
#define ARM_SIMD_VRSRA_MASK         0x00000300
#define ARM_SIMD_VSRI_MASK          0x00000400
#define ARM_SIMD_VSHL_MASK          0x00000500
#define ARM_SIMD_VQSHLU_MASK        0x00000600
#define ARM_SIMD_VQSHL_IMM_MASK     0x00000700
#define ARM_SIMD_VSHRN_MASK         0x00000800
#define ARM_SIMD_VQSHRN_MASK        0x00000900
#define ARM_SIMD_VSHLL_MASK         0x00000A00
#define ARM_SIMD_VCVT_TO_FLOAT_MASK 0x00000E00
#define ARM_SIMD_VCVT_TO_FIXED_MASK 0x00000F00

#define ARM_SIMD_VSHRN_OP_MASK     0x010000C0
#define ARM_SIMD_VSHRN_OP_VALUE    0x00000000
#define ARM_SIMD_VRSHRN_OP_VALUE   0x00000040
#define ARM_SIMD_VQSHRUN_OP_VALUE  0x01000000
#define ARM_SIMD_VQRSHRUN_OP_VALUE 0x01000040

#define ARM_SIMD_VQSHRN_OP_MASK    0x000000C0
#define ARM_SIMD_VQSHRN_OP_VALUE   0x00000000
#define ARM_SIMD_VQRSHRN_OP_VALUE  0x00000040

#define ARM_SIMD_VSHLL_OP_MASK     0x000000C0
#define ARM_SIMD_VSHLL_OP_VALUE    0x00000000

#define ARM_VSHR_MNEMONIC     "vshr"
#define ARM_VSRA_MNEMONIC     "vsra"
#define ARM_VRSHR_MNEMONIC    "vrshr"
#define ARM_VRSRA_MNEMONIC    "vrsra"
#define ARM_VSRI_MNEMONIC     "vsri"
#define ARM_VSHL_MNEMONIC     "vshl"
#define ARM_VSLI_MNEMONIC     "vsli"
#define ARM_VQSHLU_MNEMONIC   "vqshlu"
#define ARM_VSHRN_MNEMONIC    "vshrn"
#define ARM_VRSHRN_MNEMONIC   "vrshrn"
#define ARM_VQSHRN_MNEMONIC   "vqshrn"
#define ARM_VQSHRUN_MNEMONIC  "vqshrun"
#define ARM_VQRSHRN_MNEMONIC  "vqrshrn"
#define ARM_VQRSHRUN_MNEMONIC "vqrshrun"
#define ARM_VSHLL_MNEMONIC    "vshll"
#define ARM_VMOVL_MNEMONIC    "vmovl"

//
// SIMD instruction encodings for instructions with three registers of
// different lengths.
//

#define ARM_SIMD_3_DIFF_OPERATION_MASK 0x00000F00
#define ARM_SIMD_3_DIFF_SIZE_MASK      0x00300000
#define ARM_SIMD_3_DIFF_SIZE_SHIFT     20
#define ARM_SIMD_3_DIFF_SIZE_8         0x0
#define ARM_SIMD_3_DIFF_SIZE_16        0x1
#define ARM_SIMD_3_DIFF_SIZE_32        0x2
#define ARM_SIMD_3_DIFF_SIZE_64        0x3

#define ARM_SIMD_VADDL_MASK      0x00000000
#define ARM_SIMD_VADDW_MASK      0x00000100
#define ARM_SIMD_VSUBL_MASK      0x00000200
#define ARM_SIMD_VSUBW_MASK      0x00000300
#define ARM_SIMD_VADDHN_MASK     0x00000400
#define ARM_SIMD_VABAL_MASK      0x00000500
#define ARM_SIMD_VSUBHN_MASK     0x00000600
#define ARM_SIMD_VABDL_MASK      0x00000700
#define ARM_SIMD_VMLAL_MASK      0x00000800
#define ARM_SIMD_VQDMLAL_MASK    0x00000900
#define ARM_SIMD_VMLSL_MASK      0x00000A00
#define ARM_SIMD_VQDMLSL_MASK    0x00000B00
#define ARM_SIMD_VMULL_INT_MASK  0x00000C00
#define ARM_SIMD_VQDMULL_MASK    0x00000D00
#define ARM_SIMD_VMULL_POLY_MASK 0x00000E00

#define ARM_VADDL_MNEMONIC   "vaddl"
#define ARM_VADDW_MNEMONIC   "vaddw"
#define ARM_VSUBL_MNEMONIC   "vsubl"
#define ARM_VSUBW_MNEMONIC   "vsubw"
#define ARM_VADDHN_MNEMONIC  "vaddhn"
#define ARM_VRADDHN_MNEMONIC "vraddhn"
#define ARM_VABAL_MNEMONIC   "vabal"
#define ARM_VSUBHN_MNEMONIC  "vsubhn"
#define ARM_VRSUBHN_MNEMONIC "vrsubhn"
#define ARM_VABDL_MNEMONIC   "vabdl"
#define ARM_VMLAL_MNEMONIC   "vmlal"
#define ARM_VMLSL_MNEMONIC   "vmlsl"
#define ARM_VQDMLAL_MNEMONIC "vqdmlal"
#define ARM_VQDMLSL_MNEMONIC "vqdmlsl"
#define ARM_VMULL_MNEMONIC   "vmull"
#define ARM_VQDMULL_MNEMONIC "vqdmull"

//
// SIMD instruction encodings for instructions with two registers and a scalar
// value.
//

#define ARM_SIMD_2_REGISTER_SCALAR_QUADWORD       0x01000000
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_MASK      0x00300000
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_16        0x00100000
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_32        0x00200000
#define ARM_SIMD_2_REGISTER_SCALAR_OPERATION_MASK 0x00000F00
#define ARM_SIMD_2_REGISTER_SCALAR_FLOAT          0x00000100

#define ARM_SIMD_2_REGISTER_SCALAR_VMLA_MASK      0x00000000
#define ARM_SIMD_2_REGISTER_SCALAR_VMLAL_MASK     0x00000200
#define ARM_SIMD_2_REGISTER_SCALAR_VQDMLAL_MASK   0x00000300
#define ARM_SIMD_2_REGISTER_SCALAR_VMLS_MASK      0x00000400
#define ARM_SIMD_2_REGISTER_SCALAR_VMLSL_MASK     0x00000600
#define ARM_SIMD_2_REGISTER_SCALAR_VQDMLSL_MASK   0x00000700
#define ARM_SIMD_2_REGISTER_SCALAR_VMUL_MASK      0x00000800
#define ARM_SIMD_2_REGISTER_SCALAR_VMULL_MASK     0x00000A00
#define ARM_SIMD_2_REGISTER_SCALAR_VQDMULL_MASK   0x00000B00
#define ARM_SIMD_2_REGISTER_SCALAR_VQDMULH_MASK   0x00000C00
#define ARM_SIMD_2_REGISTER_SCALAR_VQRDMULH_MASK  0x00000D00

#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_VECTOR_MASK  0x07
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_VECTOR_SHIFT 0
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_INDEX_MASK   0x18
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_16_VM_INDEX_SHIFT  3
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_VECTOR_MASK  0x0F
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_VECTOR_SHIFT 0
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_INDEX_MASK   0x10
#define ARM_SIMD_2_REGISTER_SCALAR_SIZE_32_VM_INDEX_SHIFT  4

//
// SIMD instruction encodings for miscellaneous instructions with two registers.
//

#define ARM_SIMD_2_REGISTER_MISC_SIZE_MASK  0x000C0000
#define ARM_SIMD_2_REGISTER_MISC_SIZE_SHIFT 18
#define ARM_SIMD_2_REGISTER_MISC_SIZE_8     0x0
#define ARM_SIMD_2_REGISTER_MISC_SIZE_16    0x1
#define ARM_SIMD_2_REGISTER_MISC_SIZE_32    0x2
#define ARM_SIMD_2_REGISTER_MISC_SIZE_64    0x3
#define ARM_SIMD_2_REGISTER_MISC_TYPE_MASK  0x00030000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0     0x00000000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1     0x00010000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2     0x00020000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3     0x00030000

#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_UNSIGNED    0x00000080
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_OP_MASK     0x00000780
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV64_MASK 0x00000000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV32_MASK 0x00000080
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VREV16_MASK 0x00000100
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADDL_MASK 0x00000200
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCLS_MASK   0x00000400
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCLZ_MASK   0x00000480
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VCNT_MASK   0x00000500
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VMVN_MASK   0x00000580
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VPADAL_MASK 0x00000600
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VQABS_MASK  0x00000700
#define ARM_SIMD_2_REGISTER_MISC_TYPE_0_VQNEG_MASK  0x00000780

#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_FLOAT     0x00000400
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_OP_MASK   0x00000380
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCGT_MASK 0x00000000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCGE_MASK 0x00000080
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCEQ_MASK 0x00000100
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCLE_MASK 0x00000180
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VCLT_MASK 0x00000200
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VABS_MASK 0x00000300
#define ARM_SIMD_2_REGISTER_MISC_TYPE_1_VNEG_MASK 0x00000380

#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_UNSIGNED                 0x00000040
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_OP_MASK                  0x00000780
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VSWP_MASK                0x00000000
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VTRN_MASK                0x00000080
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VUZP_MASK                0x00000100
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VZIP_MASK                0x00000180
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VMOVN_MASK               0x00000200
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VQMOVN_MASK              0x00000280
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VSHLL_MASK               0x00000300
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VCVT_SINGLE_TO_HALF_MASK 0x00000600
#define ARM_SIMD_2_REGISTER_MISC_TYPE_2_VCVT_HALF_TO_SINGLE_MASK 0x00000700

#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_FLOAT                  0x00000100
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_UNSIGNED               0x00000080
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_OP_MASK                0x00000680
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_VRECPE_MASK            0x00000400
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_VRSQRTE_MASK           0x00000480
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_VCVT_TO_INTEGER_MASK   0x00000700
#define ARM_SIMD_2_REGISTER_MISC_TYPE_3_VCVT_FROM_INTEGER_MASK 0x00000600

#define ARM_VREV64_MNEMONIC  "vrev64"
#define ARM_VREV32_MNEMONIC  "vrev32"
#define ARM_VREV16_MNEMONIC  "vrev16"
#define ARM_VPADDL_MNEMONIC  "vpaddl"
#define ARM_VCLS_MNEMONIC    "vcls"
#define ARM_VCLZ_MNEMONIC    "vclz"
#define ARM_VCNT_MNEMONIC    "vcnt"
#define ARM_VPADAL_MNEMONIC  "vpadal"
#define ARM_VQABS_MNEMONIC   "vqabs"
#define ARM_VQNEG_MNEMONIC   "vqneg"
#define ARM_VCLE_MNEMONIC    "vcle"
#define ARM_VCLT_MNEMONIC    "vclt"
#define ARM_VSWP_MNEMONIC    "vswp"
#define ARM_VTRN_MNEMONIC    "vtrn"
#define ARM_VUZP_MNEMONIC    "vuzp"
#define ARM_VZIP_MNEMONIC    "vzip"
#define ARM_VMOVN_MNEMONIC   "vmovn"
#define ARM_VQMOVN_MNEMONIC  "vqmovn"
#define ARM_VQMOVUN_MNEMONIC "vqmovun"
#define ARM_VRECPE_MNEMONIC  "vrecpe"
#define ARM_VRSQRTE_MNEMONIC "vrsqrte"

//
// SIMD vector extract instruction encodings.
//

#define ARM_SIMD_VEXT_IMMEDIATE4_MASK  0x00000F00
#define ARM_SIMD_VEXT_IMMEDIATE4_SHIFT 8
#define ARM_VEXT_MNEMONIC "vext"

//
// SIMD vector table lookup instruction encodings.
//

#define ARM_SIMD_VTB_LENGTH_MASK  0x00000300
#define ARM_SIMD_VTB_LENGTH_SHIFT 8
#define ARM_SIMD_VTB_EXTENSION    0x00000040
#define ARM_VTBL_MNEMONIC "vtbl"
#define ARM_VTBX_MNEMONIC "vtbx"

//
// SIMD vector duplicate scalar instruction encodings.
//

#define ARM_SIMD_VDUP_SIZE_8_MASK         0x00010000
#define ARM_SIMD_VDUP_SIZE_8_VALUE        0x00010000
#define ARM_SIMD_VDUP_SIZE_8_INDEX_MASK   0x000E0000
#define ARM_SIMD_VDUP_SIZE_8_INDEX_SHIFT  17
#define ARM_SIMD_VDUP_SIZE_16_MASK        0x00030000
#define ARM_SIMD_VDUP_SIZE_16_VALUE       0x00020000
#define ARM_SIMD_VDUP_SIZE_16_INDEX_MASK  0x000C0000
#define ARM_SIMD_VDUP_SIZE_16_INDEX_SHIFT 18
#define ARM_SIMD_VDUP_SIZE_32_MASK        0x00070000
#define ARM_SIMD_VDUP_SIZE_32_VALUE       0x00040000
#define ARM_SIMD_VDUP_SIZE_32_INDEX_MASK  0x00080000
#define ARM_SIMD_VDUP_SIZE_32_INDEX_SHIFT 19

//
// ------------------------------------------------------ Data Type Definitions
//

typedef union _ARM_IMMEDIATE_FLOAT {
    ULONG Immediate;
    float Float;
} ARM_IMMEDIATE_FLOAT, *PARM_IMMEDIATE_FLOAT;

typedef union _ARM_IMMEDIATE_DOUBLE {
    ULONGLONG Immediate;
    double Double;
} ARM_IMMEDIATE_DOUBLE, *PARM_IMMEDIATE_DOUBLE;

/*++

Structure Description:

    This structure defines the disassembly context used to store the pieces of
    the disassembled instruction.

Members:

    InstructionPointer - Stores the instruction pointer of the given
        instruction.

    Instruction - Stores the instruction to disassemble.

    Result - Stores the disassemlbed instruction.

    Mnemonic - Stores an array that holds the human readable assembly mnemonic.

    PostConditionMnemonicSuffix - Stores an array that holds the mnemonic
        suffix that must be appended after the condition codes.

    Operand1 - Stores an array that holds the first operand to the instruction.

    Operand2 - Stores an array that holds the second operand to the instruction.

    Operand3 - Stores an array that holds the third operand to the instruction.

    Operand4 - Stores an array that holds the fourth operand to the instruction.

--*/

typedef struct _ARM_DISASSEMBLY {
    ULONGLONG InstructionPointer;
    ULONG Instruction;
    PDISASSEMBLED_INSTRUCTION Result;
    CHAR Mnemonic[ARM_OPERAND_LENGTH];
    CHAR PostConditionMnemonicSuffix[ARM_OPERAND_LENGTH];
    CHAR Operand1[ARM_OPERAND_LENGTH];
    CHAR Operand2[ARM_OPERAND_LENGTH];
    CHAR Operand3[ARM_OPERAND_LENGTH];
    CHAR Operand4[ARM_OPERAND_LENGTH];
} ARM_DISASSEMBLY, *PARM_DISASSEMBLY;

typedef
VOID
(*PARM_DISASSEMBLE_ROUTINE) (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine disassembles a subset of the Thumb instruction set.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines a tuple of the instruction value for a certain mask
    and the function used to decode that subset of the instruction set.

Members:

    Mask - Stores the mask of the instruction to check against.

    Value - Stores the value of the instruction the mask should match.

    Shift - Stores the value to shift both the mask and the value by.

    Disassemble - Stores a pointer to a routine used to decode instructions
        that match the value.

--*/

typedef struct _ARM_DECODE_BRANCH {
    ULONG Mask;
    ULONG Value;
    ULONG Shift;
    PARM_DISASSEMBLE_ROUTINE Disassemble;
} ARM_DECODE_BRANCH, *PARM_DECODE_BRANCH;

//
// -------------------------------------------------------------------- Globals
//

extern PSTR DbgArmRegisterNames[];
extern PSTR DbgArmConditionCodes[16];
extern PSTR DbgArmBankedRegisters[64];

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
DbgpArmDecodeWithTable (
    PARM_DISASSEMBLY Context,
    PARM_DECODE_BRANCH Table,
    ULONG TableSize
    );

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

VOID
DbgpArmDecodeCoprocessorMove (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a coprocessor move instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeCoprocessorMoveTwo (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a coprocessor move instruction to/from two ARM
    registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeCoprocessorLoadStore (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a coprocessor data instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeFloatingPoint (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a floating point data processing instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeSimdSmallTransfers (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a transfers between SIMD and floating point 8-bit,
    16-bit, and 32-bit registers and the ARM core.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeSimd64BitTransfers (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a transfers between SIMD and floating point 64-bit
    registers and the ARM core.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeSimdLoadStore (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a load/store instructions involving SIMD and floating
    point registers.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeSimdElementLoadStore (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes a SIMD element and structure load and store
    instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeSimdDataProcessing (
    PARM_DISASSEMBLY Context
    );

/*++

Routine Description:

    This routine decodes the SIMD data processing instructions.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

VOID
DbgpArmDecodeRegisterList (
    PSTR Destination,
    ULONG DestinationSize,
    ULONG RegisterList
    );

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

VOID
DbgpArmPrintMode (
    PSTR Destination,
    ULONG Mode
    );

/*++

Routine Description:

    This routine prints the given ARM processor mode.

Arguments:

    Destination - Supplies a pointer where the mode will be printed.

    Mode - Supplies the mode to print. Only the bottom 5 bits will be examined.

Return Value:

    None.

--*/

VOID
DbgpArmPrintBarrierMode (
    PSTR Destination,
    ULONG Mode
    );

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

