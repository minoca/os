/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

#define ARM_SERVICE_BUILD_IMMEDIATE12_4(_Instruction) \
    (((_Instruction) & 0x000FFF00) >> 4) | ((_Instruction) & 0x0000000F)

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

//
// Condition Codes.
//

#define ARM_CONDITION_SHIFT             28
#define ARM_CONDITION_NEVER             0xF

//
// Define high level instruction classes
//

#define ARM_INSTRUCTION_CLASS_MASK                      0x0E000000
#define ARM_INSTRUCTION_CLASS_OP                        0x00000010
#define ARM_INSTRUCTION_CLASS_DATA_PROCESSING           0x00000000
#define ARM_INSTRUCTION_CLASS_DATA_PROCESSING2          0x02000000
#define ARM_INSTRUCTION_CLASS_LOAD_STORE                0x04000000
#define ARM_INSTRUCTION_CLASS_LOAD_AND_MEDIA            0x06000000
#define ARM_INSTRUCTION_CLASS_BRANCH_AND_BLOCK          0x08000000
#define ARM_INSTRUCTION_CLASS_BRANCH_AND_BLOCK2         0x0A000000
#define ARM_INSTRUCTION_CLASS_COPROCESSOR_SUPERVISOR    0x0C000000
#define ARM_INSTRUCTION_CLASS_COPROCESSOR_SUPERVISOR2   0x0E000000

//
// Bit definitions common to several instruction classes.
//

#define ARM_OFFSET_REGISTER             0x0000000F
#define ARM_DESTINATION_REGISTER_MASK   0x0000F000
#define ARM_DESTINATION_REGISTER_SHIFT  12
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
// Data processing class encodings.
//

#define ARM_DATA_PROCESSING_OP              0x02000000
#define ARM_DATA_PROCESSING_OP1_MASK        0x01F00000
#define ARM_DATA_PROCESSING_OP1_SHIFT       20
#define ARM_DATA_PROCESSING_OP2_MASK        0x000000F0
#define ARM_DATA_PROCESSING_OP2_SHIFT       4

#define ARM_DATA_PROCESSING_OP1_REGISTER_MASK               0x19
#define ARM_DATA_PROCESSING_OP1_MISCELLANEOUS               0x10
#define ARM_DATA_PROCESSING_OP1_MULTIPLY_MASK               0x10
#define ARM_DATA_PROCESSING_OP1_MULTIPLY_VALUE              0x00
#define ARM_DATA_PROCESSING_OP1_SYNCHRONIZATION_MASK        0x10
#define ARM_DATA_PROCESSING_OP1_SYNCHRONIZATION_VALUE       0x10
#define ARM_DATA_PROCESSING_OP1_EXTRA_LOAD_STORE_MASK       0x12
#define ARM_DATA_PROCESSING_OP1_EXTRA_LOAD_STORE_VALUE      0x02
#define ARM_DATA_PROCESSING_OP1_IMMEDIATE_MASK              0x19
#define ARM_DATA_PROCESSING_OP1_IMMEDIATE_VALUE             0x10
#define ARM_DATA_PROCESSING_OP1_LOAD_IMMEDIATE16            0x10
#define ARM_DATA_PROCESSING_OP1_LOAD_IMMEDIATE16_HIGH       0x14
#define ARM_DATA_PROCESSING_OP1_MSR_IMMEDIATE_MASK          0x1B
#define ARM_DATA_PROCESSING_OP1_MSR_IMMEDIATE_VALUE         0x12

#define ARM_DATA_PROCESSING_OP2_REGISTER_MASK               0x1
#define ARM_DATA_PROCESSING_OP2_REGISTER_VALUE              0x0
#define ARM_DATA_PROCESSING_OP2_REGISTER_SHIFT_MASK         0x9
#define ARM_DATA_PROCESSING_OP2_REGISTER_SHIFT_VALUE        0x1
#define ARM_DATA_PROCESSING_OP2_MISCELLANEOUS_MASK          0x8
#define ARM_DATA_PROCESSING_OP2_MISCELLANEOUS_VALUE         0x0
#define ARM_DATA_PROCESSING_OP2_SMALL_MULTIPLY_MASK         0x9
#define ARM_DATA_PROCESSING_OP2_SMALL_MULTIPLY_VALUE        0x8
#define ARM_DATA_PROCESSING_OP2_MULTIPLY                    0x9
#define ARM_DATA_PROCESSING_OP2_SYNCHRONIZATION             0x9
#define ARM_DATA_PROCESSING_OP2_EXTRA_LOAD                  0x9
#define ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_MASK            0xD
#define ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_VALUE           0xD
#define ARM_DATA_PROCESSING_OP2_EXTRA_LOAD_UNPRIVILEGED     0x9

//
// Data processing instruction encodings
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
#define ARM_MULTIPLY_SOURCE_HALF        0x00000020
#define ARM_MULTIPLY_DESTINATION_HALF   0x00000040
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
#define ARM_SMLXLD_MASK                 0x07400000
#define ARM_SMLXLD_SUBTRACT_BIT         0x00000040
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
// Media extension encondings
//

#define ARM_MEDIA_MULTIPLY_MASK         0x0F800010
#define ARM_MEDIA_MULTIPLY_VALUE        0x07000010

//
// Load store multiple / block transfer encodings.
//

#define ARM_LOAD_STORE_MULTIPLE_MASK    0x0E000000
#define ARM_LOAD_STORE_MULTIPLE_VALUE   0x08000000
#define ARM_PUSH_POP_TYPE_MASK          0x01800000
#define ARM_PUSH_POP_INCREMENT_AFTER    0x00800000
#define ARM_PUSH_POP_INCREMENT_BEFORE   0x01800000
#define ARM_PUSH_POP_DECREMENT_AFTER    0x00000000
#define ARM_PUSH_POP_DECREMENT_BEFORE   0x01000000
#define ARM_USE_SAVED_PSR_BIT           0x00400000
#define ARM_REGISTER_LIST_MASK          0x0000FFFF
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

#define ARM_BRANCH_LINK_X   (1 << 24)

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
#define ARM_CPS_FLAG_I_STRING            "i"
#define ARM_CPS_FLAG_A_STRING           "a"

#define ARM_CPS_IMOD_DISABLE            (1 << 18)
#define ARM_CPS_MNEMONIC_DISABLE        "cpsid"
#define ARM_CPS_MNEMONIC_ENABLE         "cpsie"

//
// Coprocessor move definitions
//

#define ARM_COPROCESSOR_REGISTER_MASK   0x0F000000
#define ARM_COPROCESSOR_REGISTER_VALUE  0x0E000000
#define ARM_COPROCESSOR_CDP_BIT         0x00000010
#define ARM_COPROCESSOR_MRC_BIT         0x00100000
#define ARM_COPROCESSOR_NUMBER_MASK     0x00000F00
#define ARM_COPROCESSOR_NUMBER_SHIFT    8
#define ARM_CDP_OPCODE1_MASK            0x00F00000
#define ARM_CDP_OPCODE1_SHIFT           20
#define ARM_MCR_MRC_OPCODE1_MASK        0x00E00000
#define ARM_MCR_MRC_OPCODE1_SHIFT       21
#define ARM_COPROCESSOR_OPCODE2_MASK    0x000000E0
#define ARM_COPROCESSOR_OPCODE2_SHIFT   5
#define ARM_COPROCESSOR_RN_MASK         0x000F0000
#define ARM_COPROCESSOR_RN_SHIFT        16
#define ARM_COPROCESSOR_RM_MASK         0x0000000F
#define ARM_COPROCESSOR_RM_SHIFT        0
#define ARM_CDP_MNEMONIC                "cdp"
#define ARM_MRC_MNEMONIC                "mrc"
#define ARM_MCR_MNEMONIC                "mcr"

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
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _ARM_DISASSEMBLY {
    ULONG Instruction;
    PDISASSEMBLED_INSTRUCTION Result;
    CHAR Mnemonic[ARM_OPERAND_LENGTH];
    CHAR Operand1[ARM_OPERAND_LENGTH];
    CHAR Operand2[ARM_OPERAND_LENGTH];
    CHAR Operand3[ARM_OPERAND_LENGTH];
    CHAR Operand4[ARM_OPERAND_LENGTH];
} ARM_DISASSEMBLY, *PARM_DISASSEMBLY;

//
// -------------------------------------------------------------------- Globals
//

extern PSTR DbgArmRegisterNames[];
extern PSTR DbgArmConditionCodes[16];
extern PSTR DbgArmBankedRegisters[64];

//
// -------------------------------------------------------- Function Prototypes
//

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
DbgpArmDecodeCoprocessorData (
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
