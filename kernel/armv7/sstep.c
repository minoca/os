/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    sstep.c

Abstract:

    This module implements support for getting the next PC instruction on ARM.
    This is most commonly used to implement single stepping support.

Author:

    Evan Green 11-Aug-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/arm.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Definitions that relate to the condition code of an ARM instruction. The
// values here are shifted down by one, as the low bit just negates the
// condition.
//

#define ARM_CONDITION_CODE_MASK 0xF0000000
#define ARM_CONDITION_CODE_SHIFT 28
#define ARM_CONDITION_CODE_UNCONDITIONAL 0xF

#define ARM_CONDITION_EQUAL 0x0
#define ARM_CONDITION_CARRY 0x1
#define ARM_CONDITION_NEGATIVE 0x2
#define ARM_CONDITION_OVERFLOW 0x3
#define ARM_CONDITION_UNSIGNED_GREATER 0x4
#define ARM_CONDITION_SIGNED_GREATER_OR_EQUAL 0x5
#define ARM_CONDITION_SIGNED_GREATER 0x6
#define ARM_CONDITION_ALWAYS 0x7

//
// Branch and exchange instruction (BX).
//

#define BRANCH_EXCHANGE_MASK        0x0FFFFFF0
#define BRANCH_EXCHANGE_VALUE       0x012FFF10
#define BRANCH_EXCHANGE_LINK_REGISTER 14
#define BRANCH_EXCHANGE_X_MASK      0x0FFFFFF0
#define BRANCH_EXCHANGE_X_VALUE     0x012FFF30

//
// Branch instruction.
//

#define BRANCH_MASK                 0x0E000000
#define BRANCH_VALUE                0x0A000000
#define BRANCH_H_BIT                0x01000000

//
// Media instructions.
//

#define MEDIA_MASK                  0x0E000010
#define MEDIA_VALUE                 0x06000010

//
// Load/Store instructions (LDR/STR).
//

#define LOAD_STORE_SINGLE_MASK      0x0C000000
#define LOAD_STORE_SINGLE_VALUE     0x04000000
#define LOAD_STORE_BYTE_BIT         0x00400000
#define DESTINATION_REGISTER_MASK   0x0000F000
#define DESTINATION_REGISTER_SHIFT  12
#define LOAD_BIT                    0x00100000
#define PREINDEX_BIT                0x01000000
#define IMMEDIATE_BIT               0x02000000
#define SET_FLAGS_BIT               0x00100000
#define ADD_SUBTRACT_BIT            0x00800000
#define SHIFT_TYPE                  0x00000060
#define SHIFT_LSL                   0x00000000
#define SHIFT_LSR                   0x00000020
#define SHIFT_ASR                   0x00000040
#define SHIFT_ROR                   0x00000060
#define LOAD_STORE_BYTE_BIT         0x00400000
#define LOAD_STORE_BASE_MASK        0x000F0000
#define LOAD_STORE_BASE_SHIFT       16
#define REGISTER_REGISTER_SHIFT_BIT 0x00000010
#define REGISTER_PC                 0xF

//
// Load/Store Multiple (LDM/STM).
//

#define LOAD_STORE_MULTIPLE_MASK    0x0E000000
#define LOAD_STORE_MULTIPLE_VALUE   0x08000000
#define PUSH_POP_TYPE_MASK          0x01800000
#define PUSH_POP_INCREMENT_AFTER    0x00800000
#define PUSH_POP_INCREMENT_BEFORE   0x01800000
#define PUSH_POP_DECREMENT_AFTER    0x00000000
#define PUSH_POP_DECREMENT_BEFORE   0x01000000
#define LOAD_STORE_MULTIPLE_PC_BIT  0x00008000
#define REGISTER_LIST_MASK          0x0000FFFF

//
// Data processing instructions.
//

#define DATA_PROCESSING_MASK        0x0C000000
#define DATA_PROCESSING_VALUE       0x00000000
#define DATA_PROCESSING_OPCODE_MASK 0x01E00000
#define DATA_PROCESSING_OPCODE_SHIFT 21
#define DATA_PROCESSING_OPERAND1_MASK   0x000F0000
#define DATA_PROCESSING_OPERAND1_SHIFT  16
#define DATA_PROCESSING_NOT_IMMEDIATE_MASK  0x01900000
#define DATA_PROCESSING_NOT_IMMEDIATE_VALUE 0x01000000
#define SHIFT_REGISTER_MASK         0x00000F00
#define SHIFT_REGISTER_SHIFT        8
#define SHIFT_REGISTER_EMPTY_BIT    0x00000080
#define SHIFT_IMMEDIATE_MASK        0x00000F80
#define SHIFT_IMMEDIATE_SHIFT       7
#define OPERAND2_REGISTER_MASK      0x0000000F
#define IMMEDIATE8_MASK             0x000000FF
#define IMMEDIATE_ROTATE_MASK       0x00000F00
#define IMMEDIATE_ROTATE_SHIFT      8
#define OPCODE_AND                  0
#define OPCODE_EOR                  1
#define OPCODE_SUB                  2
#define OPCODE_RSB                  3
#define OPCODE_ADD                  4
#define OPCODE_ADC                  5
#define OPCODE_SBC                  6
#define OPCODE_RSC                  7
#define OPCODE_TST                  8
#define OPCODE_TEQ                  9
#define OPCODE_CMP                  10
#define OPCODE_CMN                  11
#define OPCODE_ORR                  12
#define OPCODE_MOV                  13
#define OPCODE_BIC                  14
#define OPCODE_MVN                  15

//
// Define RFE instruction bits.
//

#define ARM_RFE_MASK  0xFE50FFFF
#define ARM_RFE_VALUE 0xF8100A00
#define ARM_RFE_PREINDEX (1 << 24)
#define ARM_RFE_INCREMENT (1 << 23)
#define ARM_RFE_REGISTER_MASK  0x000F0000
#define ARM_RFE_REGISTER_SHIFT 16

//
// Define Thumb decoding constants.
//

//
// Common Thumb definitions
//

#define THUMB_REGISTER8_MASK 0x7
#define THUMB_REGISTER16_MASK 0xF
#define THUMB_CONDITION_MASK 0xF
#define THUMB_IMMEDIATE5_MASK 0x1F
#define THUMB_IMMEDIATE6_MASK 0x3F
#define THUMB_IMMEDIATE8_MASK 0xFF
#define THUMB_IMMEDIATE10_MASK 0x3FF
#define THUMB_IMMEDIATE11_MASK 0x7FF

//
// 16-bit Thumb decoding constants
//

#define THUMB16_IT_MASK  0xFF00
#define THUMB16_IT_VALUE 0xBF00
#define THUMB16_IT_STATE_MASK 0x00FF

#define THUMB16_BX_MASK  0xFF07
#define THUMB16_BX_VALUE 0x4700
#define THUMB16_BX_RM_SHIFT 3

#define THUMB16_B_CONDITIONAL_MASK  0xF000
#define THUMB16_B_CONDITIONAL_VALUE 0xD000
#define THUMB16_B_CONDITIONAL_CONDITION_SHIFT 8

#define THUMB16_B_UNCONDITIONAL_MASK  0xF800
#define THUMB16_B_UNCONDITIONAL_VALUE 0xE000

#define THUMB16_CBZ_MASK  0xF500
#define THUMB16_CBZ_VALUE 0xB100
#define THUMB16_CBZ_IMMEDIATE5_SHIFT 3
#define THUMB16_CBZ_IMMEDIATE5 (1 << 9)
#define THUMB16_CBZ_NOT (1 << 11)

#define THUMB16_POP_MASK  0xFE00
#define THUMB16_POP_VALUE 0xBC00
#define THUMB16_POP_PC (1 << 8)
#define THUMB16_POP_REGISTER_LIST 0xFF

//
// 32-BIT Thumb decoding constants
//

#define THUMB32_RFE_MASK    0xFFD0FFFF
#define THUMB32_RFEIA_VALUE 0xF810C000
#define THUMB32_RFEDB_VALUE 0xF990C000
#define THUMB32_RFE_REGISTER_MASK 0x000F0000
#define THUMB32_RFE_REGISTER_SHIFT 16

#define THUMB32_LDM_MASK  0xFE500000
#define THUMB32_LDM_VALUE 0xE8100000
#define THUMB32_LDM_RN_SHIFT 16
#define THUMB32_LDM_INCREMENT (1 << 23)

#define THUMB32_TB_MASK  0xFFF0FFE0
#define THUMB32_TB_VALUE 0xE8D0F000
#define THUMB32_TB_RN_SHIFT 16
#define THUMB32_TB_RM_SHIFT 0
#define THUMB32_TB_HALF_WORD (1 << 4)

#define THUMB32_SUBS_PC_LR_MASK  0xFFFFFF00
#define THUMB32_SUBS_PC_LR_VALUE 0xF3DE8F00

#define THUMB32_B_CONDITIONAL_MASK  0xF800D000
#define THUMB32_B_CONDITIONAL_VALUE 0xF0008000
#define THUMB32_B_IMMEDIATE11_SHIFT 0
#define THUMB32_B_IMMEDIATE11_MASK 0x7FF
#define THUMB32_B_J2_BIT (1 << 11)
#define THUMB32_B_J1_BIT (1 << 13)
#define THUMB32_B_S_BIT (1 << 26)
#define THUMB32_B_CONDITIONAL_IMMEDIATE6_SHIFT 16
#define THUMB32_B_CONDITIONAL_CONDITION_SHIFT 22
#define THUMB32_B_CONDITIONAL_CONDITION_MASK 0xF

#define THUMB32_B_UNCONDITIONAL_MASK  0xF800D000
#define THUMB32_B_UNCONDITIONAL_VALUE 0xF0009000
#define THUMB32_B_UNCONDITIONAL_IMMEDIATE10_SHIFT 16

#define THUMB32_BL_MASK  0xF800C000
#define THUMB32_BL_VALUE 0xF000C000
#define THUMB32_BL_IMMEDIATE11_SHIFT 0
#define THUMB32_BL_IMMEDIATE10_SHIFT 16
#define THUMB32_BL_X_BIT (1 << 12)

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ArpGetNextPcThumb (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    );

KSTATUS
ArpGetNextPcThumb16 (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    );

KSTATUS
ArpGetNextPcThumb32 (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    );

BOOL
ArpIsMaskedByThumbItState (
    ULONG Instruction,
    ULONG Cpsr,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PVOID *NextPc
    );

ULONG
ArpGetArmRegister (
    PTRAP_FRAME TrapFrame,
    ULONG RegisterNumber
    );

ULONG
ArpDecodeShiftedOperand (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction
    );

ULONG
ArpThumbGetInstructionSize (
    ULONG Instruction
    );

BOOL
ArpArmCheckConditionCode (
    ULONG Cpsr,
    UCHAR Condition
    );

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ArGetNextPc (
    PTRAP_FRAME TrapFrame,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    )

/*++

Routine Description:

    This routine attempts to predict the next instruction to be executed. It
    will decode the current instruction, check if the condition matches, and
    attempt to follow any branches.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    IsFunctionReturning - Supplies an optional pointer where a boolean will be
        stored indicating if the current instruction is a return of some kind.

    NextPcValue - Supplies a pointer of the next executing address.

Return Value:

    Status code. This routine will attempt to make a guess at the next PC even
    if the status code is failing, but chances it's right go way down if a
    failing status is returned.

--*/

{

    ULONG Address;
    ULONG BaseRegister;
    BOOL Condition;
    UCHAR ConditionCode;
    ULONG DestinationRegister;
    BOOL FunctionReturning;
    ULONG Instruction;
    ULONG NextPc;
    BOOL NotDataProcessing;
    ULONG Offset;
    ULONG Operand1;
    ULONG Operand1Register;
    ULONG Operand2;
    ULONG RegisterCount;
    ULONG Registers;
    ULONG Result;
    ULONG ShiftImmediate;
    KSTATUS Status;

    FunctionReturning = FALSE;
    Result = 0;

    //
    // Get the current instruction.
    //

    Address = REMOVE_THUMB_BIT((UINTN)(TrapFrame->Pc));
    Status = ReadMemoryFunction((PVOID)Address,
                                ARM_INSTRUCTION_LENGTH,
                                &Instruction);

    if (!KSUCCESS(Status)) {
        goto GetNextPcEnd;
    }

    //
    // If executing in Thumb mode, use that encoding and skip all this ARM
    // mode stuff.
    //

    if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
        Status = ArpGetNextPcThumb(TrapFrame,
                                   Instruction,
                                   ReadMemoryFunction,
                                   &FunctionReturning,
                                   (PVOID *)&NextPc);

        goto GetNextPcEnd;
    }

    //
    // The default guess is just PC + 4 in ARM mode.
    //

    NextPc = TrapFrame->Pc + 4;

    //
    // Determine whether the condition code is satisfied. If the condition is
    // not satisfied, there's no need to decode the instruction.
    //

    ConditionCode = (Instruction & ARM_CONDITION_CODE_MASK) >>
                    ARM_CONDITION_CODE_SHIFT;

    if (ConditionCode != ARM_CONDITION_CODE_UNCONDITIONAL) {
        Condition = ArpArmCheckConditionCode(TrapFrame->Cpsr, ConditionCode);
        if (Condition == FALSE) {
            goto GetNextPcEnd;
        }
    }

    //
    // Attempt to decode a return from exception (RFE).
    //

    if ((Instruction & ARM_RFE_MASK) == ARM_RFE_VALUE) {
        BaseRegister = (Instruction & ARM_RFE_REGISTER_MASK) >>
                       ARM_RFE_REGISTER_SHIFT;

        Address = ArpGetArmRegister(TrapFrame, BaseRegister);

        //
        // The RFE instruction pops the PC and CPSR. Determine the location of
        // the PC based on the mode.
        //

        if ((Instruction & ARM_RFE_INCREMENT) == 0) {
            Address -= (sizeof(PVOID) * 2);
            if ((Instruction & ARM_RFE_PREINDEX) == 0) {
                Address += sizeof(PVOID);
            }

        } else if ((Instruction & ARM_RFE_PREINDEX) != 0) {
            Address += sizeof(PVOID);
        }

        Status = ReadMemoryFunction((PVOID)Address, sizeof(PVOID), &NextPc);
        goto GetNextPcEnd;
    }

    //
    // Attempt to decode a branch and exchange instruction. It branches to the
    // contents of a register indexed by the last 4 bits of the instruction.
    //

    if ((ConditionCode != ARM_CONDITION_CODE_UNCONDITIONAL) &&
        (((Instruction & BRANCH_EXCHANGE_MASK) == BRANCH_EXCHANGE_VALUE) ||
         ((Instruction & BRANCH_EXCHANGE_X_MASK) == BRANCH_EXCHANGE_X_VALUE))) {

        if ((Instruction & 0xF) == BRANCH_EXCHANGE_LINK_REGISTER) {
            FunctionReturning = TRUE;
        }

        NextPc = ArpGetArmRegister(TrapFrame, Instruction & 0xF);
        goto GetNextPcEnd;
    }

    //
    // Attempt to decode a branch instruction. These instructions branch to
    // PC + immediate24, where the PC is 8 bytes ahead of the ARM instruction
    // being decoded. Recall that the guess of NextPc is already 4 ahead of the
    // current instruction. This mask works for both conditional and
    // unconditional branches.
    //

    if ((Instruction & BRANCH_MASK) == BRANCH_VALUE) {

        //
        // If this is an unconditional BLX instruction, the immediate value is
        // formed differently and the destination is Thumb, so the low bit
        // should be set in the address.
        //

        if (ConditionCode == ARM_CONDITION_CODE_UNCONDITIONAL) {
            Offset = (Instruction & 0x00FFFFFF) << 2;
            if ((Instruction & BRANCH_H_BIT) != 0) {
                Offset |= 0x2;
            }

            Offset |= ARM_THUMB_BIT;

        //
        // Otherwise The offset is formed by taking the lower 24 bits from the
        // instruction, right shifting by 2, and then sign extending.
        //

        } else {
            Offset = (Instruction & 0x00FFFFFF) << 2;
        }

        if ((Offset & 0x02000000) != 0) {
            Offset |= 0xFC000000;
        }

        NextPc += Offset + 4;
        goto GetNextPcEnd;
    }

    //
    // Attempt to decode a load register (LDR) instruction.
    //

    if ((ConditionCode != ARM_CONDITION_CODE_UNCONDITIONAL) &&
        ((Instruction & LOAD_STORE_SINGLE_MASK) == LOAD_STORE_SINGLE_VALUE) &&
        ((Instruction & MEDIA_MASK) != MEDIA_VALUE)) {

        DestinationRegister = (Instruction & DESTINATION_REGISTER_MASK) >>
                              DESTINATION_REGISTER_SHIFT;

        //
        // This instruction only affects the PC if it's a load instruction and
        // the PC is the destination. Technically writebacks could affect the
        // PC too, but it's unlikely anyone would ever use that side effect
        // to manipulate the PC.
        //

        if (((Instruction & LOAD_BIT) != 0) &&
            (DestinationRegister == REGISTER_PC)) {

            BaseRegister = (Instruction & LOAD_STORE_BASE_MASK) >>
                           LOAD_STORE_BASE_SHIFT;

            //
            // In the immediate addressing form, the address is [Rn +/- #imm12],
            // where the immediate is in the lower 12 bits of the instruction.
            //

            if ((Instruction & IMMEDIATE_BIT) == 0) {
                Offset = 0;
                if ((Instruction & PREINDEX_BIT) != 0) {
                    Offset = Instruction & 0x00000FFF;
                }

            //
            // In the pre-indexed register addressing form, the address is
            // [Rn +/- Rm <shift> #<shift_imm>].
            //

            } else if ((Instruction & PREINDEX_BIT) != 0) {
                Offset = ArpDecodeShiftedOperand(TrapFrame, Instruction);

            //
            // Post-indexing uses only the base register as the address.
            //

            } else {
                Offset = 0;
            }

            //
            // Now form the actual address.
            //

            if ((Instruction & ADD_SUBTRACT_BIT) != 0) {
                Address = ArpGetArmRegister(TrapFrame, BaseRegister) +
                          Offset;

            } else {
                Address = ArpGetArmRegister(TrapFrame, BaseRegister) -
                          Offset;
            }

            //
            // Get that byte or word.
            //

            if ((Instruction & LOAD_STORE_BYTE_BIT) != 0) {
                Status = ReadMemoryFunction((PVOID)Address, 1, &NextPc);
                if (!KSUCCESS(Status)) {
                    goto GetNextPcEnd;
                }

                if ((NextPc & 0x00000080) != 0) {
                    NextPc |= 0xFFFFFF00;
                }

            } else {
                Status = ReadMemoryFunction((PVOID)Address, 4, &NextPc);
                if (!KSUCCESS(Status)) {
                    goto GetNextPcEnd;
                }
            }
        }

        goto GetNextPcEnd;
    }

    //
    // Attempt to decode a load/store multiple instruction.
    //

    if ((ConditionCode != ARM_CONDITION_CODE_UNCONDITIONAL) &&
        ((Instruction & LOAD_STORE_MULTIPLE_MASK) ==
         LOAD_STORE_MULTIPLE_VALUE)) {

        //
        // Only care about load instructions that affect the PC register.
        //

        if (((Instruction & LOAD_BIT) != 0) &&
            ((Instruction & LOAD_STORE_MULTIPLE_PC_BIT) != 0)) {

            FunctionReturning = TRUE;
            BaseRegister = (Instruction & LOAD_STORE_BASE_MASK) >>
                           LOAD_STORE_BASE_SHIFT;

            //
            // Count the number of registers being popped.
            //

            Registers = Instruction & REGISTER_LIST_MASK;
            RegisterCount = 0;
            while (Registers != 0) {
                if ((Registers & 0x1) != 0) {
                    RegisterCount += 1;
                }

                Registers = Registers >> 1;
            }

            switch (Instruction & PUSH_POP_TYPE_MASK) {
            case PUSH_POP_INCREMENT_AFTER:
                Offset = RegisterCount - 1;
                break;

            case PUSH_POP_DECREMENT_AFTER:
                Offset = -(RegisterCount - 1);
                break;

            case PUSH_POP_INCREMENT_BEFORE:
                Offset = RegisterCount;
                break;

            case PUSH_POP_DECREMENT_BEFORE:
                Offset = -RegisterCount;
                break;

            default:
                goto GetNextPcEnd;
            }

            Address = ArpGetArmRegister(TrapFrame, BaseRegister) +
                      (Offset * sizeof(ULONG));

            Status = ReadMemoryFunction((PVOID)Address, 4, &NextPc);
            if (!KSUCCESS(Status)) {
                goto GetNextPcEnd;
            }
        }

        goto GetNextPcEnd;
    }

    //
    // Decode data processing instructions.
    //

    if ((ConditionCode != ARM_CONDITION_CODE_UNCONDITIONAL) &&
        ((Instruction & DATA_PROCESSING_MASK) == DATA_PROCESSING_VALUE)) {

        NotDataProcessing = FALSE;

        //
        // The immediate form is an 8 bit value rotated right by 2 times the
        // shift amount.
        //

        if ((Instruction & IMMEDIATE_BIT) != 0) {

            //
            // The 16-bit immediate load and MSR instructions do not follow the
            // same pattern as the data processing instructions.
            //

            if ((Instruction & DATA_PROCESSING_NOT_IMMEDIATE_MASK) ==
                DATA_PROCESSING_NOT_IMMEDIATE_VALUE) {

                NotDataProcessing = TRUE;
            }

            ShiftImmediate = 2 * ((Instruction & IMMEDIATE_ROTATE_MASK) >>
                                  IMMEDIATE_ROTATE_SHIFT);

            Operand2 = Instruction & IMMEDIATE8_MASK;
            while (ShiftImmediate > 0) {
                if ((Operand2 & 0x1) != 0) {
                    Operand2 = (Operand2 >> 1) | 0x80000000;

                } else {
                    Operand2 = (Operand2 >> 1) & 0x7FFFFFFF;
                }

                ShiftImmediate -= 1;
            }

        //
        // The register form is either an immediate shift or a register shift.
        //

        } else {

            //
            // If the immediate bit is not set and it's a register shift, then
            // check the bit that must be zero. If it's not zero, then this
            // isn't actually a data processing instruction (it's a multiply).
            //

            if (((Instruction & REGISTER_REGISTER_SHIFT_BIT) != 0) &&
                ((Instruction & SHIFT_REGISTER_EMPTY_BIT) != 0)) {

                NotDataProcessing = TRUE;
            }

            Operand2 = ArpDecodeShiftedOperand(TrapFrame, Instruction);
        }

        Operand1Register = (Instruction & DATA_PROCESSING_OPERAND1_MASK) >>
                           DATA_PROCESSING_OPERAND1_SHIFT;

        Operand1 = ArpGetArmRegister(TrapFrame, Operand1Register);

        //
        // Determine what to do based on the opcode.
        //

        switch ((Instruction & DATA_PROCESSING_OPCODE_MASK) >>
                DATA_PROCESSING_OPCODE_SHIFT) {

        case OPCODE_AND:
            Result = Operand1 & Operand2;
            break;

        case OPCODE_EOR:
            Result = Operand1 ^ Operand2;
            break;

        case OPCODE_SUB:
            Result = Operand1 - Operand2;
            break;

        case OPCODE_RSB:
            Result = Operand2 - Operand1;
            break;

        case OPCODE_ADD:
            Result = Operand1 + Operand2;
            break;

        case OPCODE_ADC:
            Result = Operand1 + Operand2;
            if ((TrapFrame->Cpsr & PSR_FLAG_CARRY) != 0) {
                Result += 1;
            }

            break;

        case OPCODE_SBC:
            Result = Operand1 - Operand2;
            if ((TrapFrame->Cpsr & PSR_FLAG_CARRY) == 0) {
                Result -= 1;
            }

            break;

        case OPCODE_RSC:
            Result = Operand2 - Operand1;
            if ((TrapFrame->Cpsr & PSR_FLAG_CARRY) == 0) {
                Result -= 1;
            }

            break;

        case OPCODE_ORR:
            Result = Operand1 | Operand2;
            break;

        case OPCODE_BIC:
            Result = Operand1 & (~Operand2);
            break;

        case OPCODE_MOV:
            Result = Operand2;
            if (Operand1 != 0) {
                NotDataProcessing = TRUE;
            }

            break;

        case OPCODE_MVN:
            Result = ~Operand2;
            if (Operand1 != 0) {
                NotDataProcessing = TRUE;
            }

            break;

        //
        // Compare instructions can't update the PC. If this is in fact a
        // data processing instruction, then there's nothing left to do.
        //

        case OPCODE_TST:
        case OPCODE_TEQ:
        case OPCODE_CMP:
        case OPCODE_CMN:
            if ((Instruction & SET_FLAGS_BIT) == 0) {
                NotDataProcessing = TRUE;
            }

            if (NotDataProcessing == FALSE) {
                goto GetNextPcEnd;
            }
        }

        //
        // If the destination register is the PC, then the next PC is the
        // result of the operation.
        //

        DestinationRegister = (Instruction & DESTINATION_REGISTER_MASK) >>
                              DESTINATION_REGISTER_SHIFT;

        if (NotDataProcessing == FALSE) {
            if (DestinationRegister == REGISTER_PC) {
                NextPc = Result;
            }

            goto GetNextPcEnd;
        }
    }

GetNextPcEnd:
    if (IsFunctionReturning != NULL) {
        *IsFunctionReturning = FunctionReturning;
    }

    *NextPcValue = (PVOID)NextPc;
    return Status;
}

VOID
ArBackUpIfThenState (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine backs up the Thumb if-then state in the CPSR by one
    instruction, assuming that the previous instruction tested positively for
    being executed.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

Return Value:

    Status code. This routine will attempt to make a guess at the next PC even
    if the status code is failing, but chances it's right go way down if a
    failing status is returned.

--*/

{

    ULONG Condition;
    ULONG ItState;

    //
    // If the if-then state is no longer active, then it doesn't need to be
    // backed up (even if it was just previously active, as this instruction
    // is going to get executed).
    //

    if (!PSR_IS_IT_ACTIVE(TrapFrame->Cpsr)) {
        return;
    }

    ItState = PSR_GET_IT_STATE(TrapFrame->Cpsr);
    Condition = THUMB_CONDITION_FROM_IT_STATE(ItState);
    if (ArpArmCheckConditionCode(TrapFrame->Cpsr, Condition) != FALSE) {
        ItState = THUMB_RETREAT_IT_STATE(ItState, Condition & 0x1);

    } else {
        ItState = THUMB_RETREAT_IT_STATE(ItState, ((~Condition) & 0x1));
    }

    TrapFrame->Cpsr = PSR_SET_IT_STATE(TrapFrame->Cpsr, ItState);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ArpGetNextPcThumb (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    )

/*++

Routine Description:

    This routine attempts to predict the next instruction to be executed for
    Thumb-2 execution.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

    Instruction - Supplies the instruction itself.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    IsFunctionReturning - Supplies an optional pointer where a boolean will be
        stored indicating if the current instruction is a return of some kind.

    NextPcValue - Supplies a pointer of the next executing address.

Return Value:

    Status code. This routine will attempt to make a guess at the next PC even
    if the status code is failing, but chances it's right go way down if a
    failing status is returned.

--*/

{

    ULONG InstructionSize;
    BOOL IsMasked;
    KSTATUS Status;

    InstructionSize = ArpThumbGetInstructionSize(Instruction);
    *NextPcValue = (PVOID)((TrapFrame->Pc + InstructionSize) | ARM_THUMB_BIT);

    //
    // Determine if the if-then state dictates the next instruction. The
    // if-then instruction itself is also decoded in this routine.
    //

    IsMasked = ArpIsMaskedByThumbItState(Instruction,
                                         TrapFrame->Cpsr,
                                         ReadMemoryFunction,
                                         NextPcValue);

    if (IsMasked != FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Determine whether this is a 16 or 32-bit thumb instruction.
    //

    if (InstructionSize == THUMB32_INSTRUCTION_LENGTH) {

        //
        // Reverse the words of the 32-bit instruction.
        //

        Instruction = ((Instruction >> 16) & 0x0000FFFF) |
                      ((Instruction << 16) & 0xFFFF0000);

        Status = ArpGetNextPcThumb32(TrapFrame,
                                     Instruction,
                                     ReadMemoryFunction,
                                     IsFunctionReturning,
                                     NextPcValue);

        return Status;
    }

    //
    // It's a 16-bit instruction.
    //

    Status = ArpGetNextPcThumb16(TrapFrame,
                                 Instruction,
                                 ReadMemoryFunction,
                                 IsFunctionReturning,
                                 NextPcValue);

    return Status;
}

KSTATUS
ArpGetNextPcThumb16 (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    )

/*++

Routine Description:

    This routine attempts to predict the next instruction to be executed for
    32-bit Thumb instructions.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

    Instruction - Supplies the instruction itself.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    IsFunctionReturning - Supplies an optional pointer where a boolean will be
        stored indicating if the current instruction is a return of some kind.

    NextPcValue - Supplies a pointer of the next executing address.

Return Value:

    Status code.

--*/

{

    PVOID Address;
    UCHAR Condition;
    UINTN NextPc;
    ULONG RegisterCount;
    ULONG RegisterList;
    UINTN Rm;
    UINTN Rn;
    LONG SignedImmediate;
    KSTATUS Status;
    ULONG UnsignedImmediate;
    ULONG Value;

    NextPc = (UINTN)*NextPcValue;

    //
    // Handle bx and blx. Not adding the thumb bit is intentional, as blx may
    // change modes.
    //

    if ((Instruction & THUMB16_BX_MASK) == THUMB16_BX_VALUE) {
        Rm = (Instruction >> THUMB16_BX_RM_SHIFT) & THUMB_REGISTER16_MASK;
        if (Rm == 14) {
            *IsFunctionReturning = TRUE;
        }

        NextPc = ArpGetArmRegister(TrapFrame, Rm);

    //
    // Handle conditional branches.
    //

    } else if ((Instruction & THUMB16_B_CONDITIONAL_MASK) ==
               THUMB16_B_CONDITIONAL_VALUE) {

        Condition = (Instruction >> THUMB16_B_CONDITIONAL_CONDITION_SHIFT) &
                    THUMB_CONDITION_MASK;

        if (((Condition >> 1) != ARM_CONDITION_ALWAYS) &&
            (ArpArmCheckConditionCode(TrapFrame->Cpsr, Condition) != FALSE)) {

            SignedImmediate = (CHAR)(Instruction & THUMB_IMMEDIATE8_MASK);
            if ((SignedImmediate & 0x80) != 0) {
                SignedImmediate |= 0xFFFFFF00;
            }

            SignedImmediate <<= 1;

            //
            // The signed offset is PC-relative, but the Next PC guess is only
            // 2 bytes ahead of the instruction pointer, when the real PC is
            // always 4 bytes ahead on Thumb.
            //

            NextPc += SignedImmediate + THUMB16_INSTRUCTION_LENGTH;
        }

    //
    // Handle unconditional branches. Sign extend the immediate.
    //

    } else if ((Instruction & THUMB16_B_UNCONDITIONAL_MASK) ==
               THUMB16_B_UNCONDITIONAL_VALUE) {

        SignedImmediate = Instruction & THUMB_IMMEDIATE11_MASK;
        if ((SignedImmediate & (1 << 10)) != 0) {
            SignedImmediate |= 0xFFFFF800;
        }

        SignedImmediate <<= 1;

        //
        // The signed offset is PC-relative, but the Next PC guess is only
        // 2 bytes ahead of the instruction pointer, when the real PC is
        // always 4 bytes ahead on Thumb.
        //

        NextPc += SignedImmediate + THUMB16_INSTRUCTION_LENGTH;

    //
    // Handle compare and branch if zero (or not zero), cbz and cbnz. This
    // compares the encoded register value with zero (or not zero), and
    // branches if the comparison succeeded.
    //

    } else if ((Instruction & THUMB16_CBZ_MASK) == THUMB16_CBZ_VALUE) {
        Rn = Instruction & THUMB_REGISTER8_MASK;
        Value = ArpGetArmRegister(TrapFrame, Rn);
        UnsignedImmediate = (Instruction >> THUMB16_CBZ_IMMEDIATE5_SHIFT) &
                            THUMB_IMMEDIATE5_MASK;

        if ((Instruction & THUMB16_CBZ_IMMEDIATE5) != 0) {
            UnsignedImmediate |= 1 << 5;
        }

        UnsignedImmediate <<= 1;
        Condition = (Value == 0);
        if ((Instruction & THUMB16_CBZ_NOT) != 0) {
            Condition = !Condition;
        }

        //
        // The offset is PC-relative, but the Next PC guess is only 2 bytes
        // ahead of the instruction pointer, when the real PC is always 4 bytes
        // ahead on Thumb.
        //

        if (Condition != 0) {
            NextPc += THUMB16_INSTRUCTION_LENGTH + UnsignedImmediate;
        }

    //
    // Handle a pop instruction.
    //

    } else if ((Instruction & THUMB16_POP_MASK) == THUMB16_POP_VALUE) {
        if ((Instruction & THUMB16_POP_PC) != 0) {
            *IsFunctionReturning = TRUE;
            RegisterList = Instruction & THUMB16_POP_REGISTER_LIST;

            //
            // Count the number of registers being popped.
            //

            RegisterCount = 0;
            while (RegisterList != 0) {
                if ((RegisterList & 0x1) != 0) {
                    RegisterCount += 1;
                }

                RegisterList = RegisterList >> 1;
            }

            Address = (PVOID)ArpGetArmRegister(TrapFrame, 13);

            //
            // The pop action is always increment after.
            //

            Address += RegisterCount * sizeof(PVOID);
            Status = ReadMemoryFunction((PVOID)Address,
                                        sizeof(PVOID),
                                        &NextPc);

            if (!KSUCCESS(Status)) {
                return Status;
            }
        }
    }

    *NextPcValue = (PVOID)NextPc;
    return STATUS_SUCCESS;
}

KSTATUS
ArpGetNextPcThumb32 (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PBOOL IsFunctionReturning,
    PVOID *NextPcValue
    )

/*++

Routine Description:

    This routine attempts to predict the next instruction to be executed for
    32-bit Thumb instructions.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

    Instruction - Supplies the instruction itself.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    IsFunctionReturning - Supplies an optional pointer where a boolean will be
        stored indicating if the current instruction is a return of some kind.

    NextPcValue - Supplies a pointer of the next executing address.

Return Value:

    Status code.

--*/

{

    UINTN Address;
    ULONG Bit;
    ULONG Condition;
    LONG Immediate;
    UINTN Offset;
    ULONG Register;
    ULONG RegisterCount;
    ULONG RegisterList;
    ULONG Rm;
    ULONG Rn;
    ULONG SBit;
    KSTATUS Status;

    Status = STATUS_SUCCESS;

    //
    // Handle the rfe (return from exception) instruction.
    //

    if (((Instruction & THUMB32_RFE_MASK) == THUMB32_RFEIA_VALUE) ||
        ((Instruction & THUMB32_RFE_MASK) == THUMB32_RFEDB_VALUE)) {

        *IsFunctionReturning = TRUE;
        Register = (Instruction & THUMB32_RFE_REGISTER_MASK) >>
                   THUMB32_RFE_REGISTER_SHIFT;

        Address = ArpGetArmRegister(TrapFrame, Register);

        //
        // RFE pops the PC and CPSR from the register. For Thumb, there is only
        // IA and DB. For increment after, PC is located at the address stored
        // in the register. For decrement before, the register value minus 8 is
        // the location of the PC.
        //

        if ((Instruction & THUMB32_RFE_MASK) == THUMB32_RFEDB_VALUE) {
            Address -= 8;
        }

        Status = ReadMemoryFunction((PVOID)Address, sizeof(PVOID), NextPcValue);

    //
    // Handle ldm (load multiple) registers. They only matter if they pop the
    // PC.
    //

    } else if ((Instruction & THUMB32_LDM_MASK) == THUMB32_LDM_VALUE) {
        RegisterList = Instruction & REGISTER_LIST_MASK;
        if ((RegisterList & LOAD_STORE_MULTIPLE_PC_BIT) != 0) {
            *IsFunctionReturning = TRUE;

            //
            // Count the number of registers being popped.
            //

            RegisterCount = 0;
            while (RegisterList != 0) {
                if ((RegisterList & 0x1) != 0) {
                    RegisterCount += 1;
                }

                RegisterList = RegisterList >> 1;
            }

            Rn = (Instruction >> THUMB32_LDM_RN_SHIFT) & THUMB_REGISTER16_MASK;
            Address = ArpGetArmRegister(TrapFrame, Rn);

            //
            // The pop action is either increment after or decrement before.
            //

            if ((Instruction & THUMB32_LDM_INCREMENT) != 0) {
                Address += (RegisterCount - 1) * sizeof(PVOID);

            } else {
                Address -= (RegisterCount + 1) * sizeof(PVOID);
            }

            Status = ReadMemoryFunction((PVOID)Address,
                                        sizeof(PVOID),
                                        NextPcValue);
        }

    //
    // Handle the tbb and tbh, which are table branch instructions. Rn
    // specifies a base of a table, and Rm specifies an index into the table.
    // Table branch causes a forward PC jump by the value in the table entry.
    //

    } else if ((Instruction & THUMB32_TB_MASK) == THUMB32_TB_VALUE) {
        Rm = (Instruction >> THUMB32_TB_RM_SHIFT) & THUMB_REGISTER16_MASK;
        Rn = (Instruction >> THUMB32_TB_RN_SHIFT) & THUMB_REGISTER16_MASK;
        Rm = ArpGetArmRegister(TrapFrame, Rm);
        Rn = ArpGetArmRegister(TrapFrame, Rn);
        Offset = 0;
        if ((Instruction & THUMB32_TB_HALF_WORD) != 0) {
            Address = Rn + (Rm << 1);
            Status = ReadMemoryFunction((PVOID)Address, 2, &Offset);

        } else {
            Address = Rn + Rm;
            Status = ReadMemoryFunction((PVOID)Address, 1, &Offset);
        }

        if (!KSUCCESS(Status)) {
            return Status;
        }

        //
        // The next PC value was already guessed to be 4 bytes ahead of the
        // instruction being decoded. Conveniently, that is the location of the
        // actually PC (from the instruction's perspective) and the offset is
        // PC-relative.
        //

        *NextPcValue += (Offset << 1);

    //
    // Handle the subs pc, lr, #imm8 instruction, which performs an exception
    // return without the stack. It copies SPSR into CPSR, and moves the link
    // register (offset by an unsigned immediate) to the PC. The ERET
    // instruction is the same as subs pc, lr, #0.
    //

    } else if ((Instruction & THUMB32_SUBS_PC_LR_MASK) ==
               THUMB32_SUBS_PC_LR_VALUE) {

        *IsFunctionReturning = TRUE;
        Offset = Instruction & THUMB_IMMEDIATE8_MASK;
        *NextPcValue = (PVOID)(TrapFrame->SvcLink + Offset);

    //
    // Handle a conditional branch, which contains a signed however-many-bit
    // immediate and a condition code.
    //

    } else if ((Instruction & THUMB32_B_CONDITIONAL_MASK) ==
               THUMB32_B_CONDITIONAL_VALUE) {

        Condition = (Instruction >> THUMB32_B_CONDITIONAL_CONDITION_SHIFT) &
                    THUMB32_B_CONDITIONAL_CONDITION_MASK;

        if (((Condition >> 1) < ARM_CONDITION_ALWAYS) &&
            (ArpArmCheckConditionCode(TrapFrame->Cpsr, Condition) != FALSE)) {

            Immediate = (Instruction >> THUMB32_B_IMMEDIATE11_SHIFT) &
                         THUMB32_B_IMMEDIATE11_MASK;

            Immediate |= ((Instruction >>
                           THUMB32_B_CONDITIONAL_IMMEDIATE6_SHIFT) &
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

            //
            // This immediate offset is PC relative. On Thumb, the PC is 4
            // bytes ahead of the current instruction. The original guess for
            // the next PC was four bytes ahead, so just add the immediate.
            //

            *NextPcValue += Immediate;
        }

    //
    // Handle an unconditional branch instruction.
    //

    } else if ((Instruction & THUMB32_B_UNCONDITIONAL_MASK) ==
               THUMB32_B_UNCONDITIONAL_VALUE) {

        Immediate = (Instruction >> THUMB32_B_IMMEDIATE11_SHIFT) &
                     THUMB32_B_IMMEDIATE11_MASK;

        Immediate |= ((Instruction >>
                       THUMB32_B_UNCONDITIONAL_IMMEDIATE10_SHIFT) &
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
        // This immediate offset is PC relative. On Thumb, the PC is 4 bytes
        // ahead of the current instruction. The original guess for the next PC
        // was four bytes ahead, so just add the immediate.
        //

        *NextPcValue += Immediate;

    //
    // Handle the bl and blx (immediate) instructions.
    //

    } else if ((Instruction & THUMB32_BL_MASK) == THUMB32_BL_VALUE) {
        Immediate = ((Instruction >> THUMB32_BL_IMMEDIATE11_SHIFT) &
                     THUMB_IMMEDIATE11_MASK) |
                    (((Instruction >> THUMB32_BL_IMMEDIATE10_SHIFT) &
                      THUMB_IMMEDIATE10_MASK) << 11);

        if ((Instruction & THUMB32_BL_X_BIT) == 0) {
            Immediate &= ~ARM_THUMB_BIT;
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
            Immediate |= 1 << 21;
        }

        Bit = 0;
        if ((Instruction & THUMB32_B_J1_BIT) != 0) {
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

        if ((Immediate & 0x00200000) != 0) {
            Immediate |= 0xFFC00000;
        }

        //
        // BLX instructions transfer from Thumb to ARM. The low bit of the
        // address will be removed when aligning the PC down to a 4-byte
        // boundary.
        //

        Address = (UINTN)(*NextPcValue);
        if (((Instruction & THUMB32_BL_X_BIT) == 0) &&
            ((Instruction & ARM_THUMB_BIT) == 0)) {

            Address = ALIGN_RANGE_DOWN(Address, ARM_INSTRUCTION_LENGTH);
        }

        Address += Immediate;
        *NextPcValue = (PVOID)Address;
    }

    return Status;
}

BOOL
ArpIsMaskedByThumbItState (
    ULONG Instruction,
    ULONG Cpsr,
    PGET_NEXT_PC_READ_MEMORY_FUNCTION ReadMemoryFunction,
    PVOID *NextPc
    )

/*++

Routine Description:

    This routine determines if the current instruction is disabled due to
    Thumb if/then state masking it.

Arguments:

    Instruction - Supplies the next instruction itself to be executed.

    Cpsr - Supplies the current Program Status Register value.

    ReadMemoryFunction - Supplies a pointer to a function this routine can
        call when it needs to read target memory.

    NextPc - Supplies a pointer where the next PC value will be returned if it
        turns out that the If-Then state masks the normal next PC instruction.

Return Value:

    TRUE if the instruction is not actually executed because the if/then state
    is disabling it.

    FALSE if the if/then state does not affect the next instruction's execution
    (or the if-then state dictates that the next instruction will be executed).

--*/

{

    UCHAR Condition;
    ULONG InstructionSize;
    ULONG ItState;
    PVOID NextPcAddress;
    BOOL Result;
    KSTATUS Status;

    Result = FALSE;

    //
    // Figure out the current if-then state. If the next instruction is an
    // if-then instruction, use the mask set up by that.
    //

    if ((Instruction & THUMB16_IT_MASK) == THUMB16_IT_VALUE) {
        ItState = Instruction & THUMB16_IT_STATE_MASK;

    } else {
        ItState = PSR_GET_IT_STATE(Cpsr);

        //
        // Assume that the next instruction to execute is the one that the
        // breakpoint that got in here is sitting on, and advance beyond it.
        //

        ItState = THUMB_ADVANCE_IT_STATE(ItState);
    }

    //
    // Loop skipping instructions that are going to be masked by the if-then
    // state.
    //

    while (TRUE) {

        //
        // If all the if-then business is not on, return now.
        //

        if (!IS_THUMB_IT_STATE_ACTIVE(ItState)) {
            break;
        }

        //
        // If the if-then state works for the next instruction, return now.
        //

        Condition = THUMB_CONDITION_FROM_IT_STATE(ItState);
        if (ArpArmCheckConditionCode(Cpsr, Condition) != FALSE) {
            break;
        }

        //
        // The if-then state is going to mask the next instruction, so advance
        // the next PC and if-then state to the following instruction, maybe
        // it will get executed. The architecture specifies that branching out
        // in the middle of an if-then block is not allowed unless it's the
        // last instruction. It also specifies that instructions in an if-then
        // block are allowed to modify the flags. This loop doesn't handle
        // that case.
        //

        NextPcAddress = (PVOID)REMOVE_THUMB_BIT((UINTN)*NextPc);
        Status = ReadMemoryFunction(NextPcAddress,
                                    THUMB32_INSTRUCTION_LENGTH,
                                    &Instruction);

        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // Skip over this instruction that won't get executed.
        //

        InstructionSize = ArpThumbGetInstructionSize(Instruction);
        *NextPc += InstructionSize;
        ItState = THUMB_ADVANCE_IT_STATE(ItState);
        Result = TRUE;
    }

    return Result;
}

ULONG
ArpGetArmRegister (
    PTRAP_FRAME TrapFrame,
    ULONG RegisterNumber
    )

/*++

Routine Description:

    This routine returns the register corresponding to one encoded in an ARM
    instruction. 0 returns the contents of r0, 1 returns the contents of r1,
    etc.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state. This is where
        the register contents come from.

    RegisterNumber - Supplies the register number to return.

Return Value:

    Returns the contents of the desired register, or MAX_ULONG if the parameter
    was invalid.

--*/

{

    BOOL UserMode;

    UserMode = FALSE;
    if ((TrapFrame->Cpsr & ARM_MODE_MASK) == ARM_MODE_USER) {
        UserMode = TRUE;
    }

    switch (RegisterNumber) {
    case 0:
        return TrapFrame->R0;

    case 1:
        return TrapFrame->R1;

    case 2:
        return TrapFrame->R2;

    case 3:
        return TrapFrame->R3;

    case 4:
        return TrapFrame->R4;

    case 5:
        return TrapFrame->R5;

    case 6:
        return TrapFrame->R6;

    case 7:
        return TrapFrame->R7;

    case 8:
        return TrapFrame->R8;

    case 9:
        return TrapFrame->R9;

    case 10:
        return TrapFrame->R10;

    case 11:
        return TrapFrame->R11;

    case 12:
        return TrapFrame->R12;

    case 13:
        if (UserMode != FALSE) {
            return TrapFrame->UserSp;

        } else {
            return TrapFrame->SvcSp;
        }

    case 14:
        if (UserMode != FALSE) {
            return TrapFrame->UserLink;

        } else {
            return TrapFrame->SvcLink;
        }

    //
    // When PC is used as an operand for a Thumb instruction, it is 4 ahead of
    // the current instruction (i.e. the PC stored in the trap frame). When PC
    // is used by an ARM instruction, it is 8 ahead of the current instruction.
    //

    case 15:
        if ((TrapFrame->Cpsr & PSR_FLAG_THUMB) != 0) {
            return TrapFrame->Pc + ARM_INSTRUCTION_LENGTH;

        } else {
            return TrapFrame->Pc + (ARM_INSTRUCTION_LENGTH * 2);
        }

    default:
        return MAX_ULONG;
    }

    return MAX_ULONG;
}

ULONG
ArpDecodeShiftedOperand (
    PTRAP_FRAME TrapFrame,
    ULONG Instruction
    )

/*++

Routine Description:

    This routine decodes the operand offset for instructions that have
    addressing modes of immediate shifts and register shifts.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state. This is where
        the register contents come from.

    Instruction - Supplies the instruction being decoded. It is assumed that
        the instruction has been decoded sufficiently to know that this is the
        correct addressing form.

Return Value:

    Returns the contents of the shifted operand, which is usually added to the
    base register operand.

--*/

{

    ULONG Offset;
    ULONG OffsetRegister;
    ULONG ShiftAmount;
    BOOL ShiftByImmediate;
    ULONG ShiftRegister;

    Offset = 0;

    //
    // Determine whether or not to shift by an immediate value or register
    // value.
    //

    if ((Instruction & REGISTER_REGISTER_SHIFT_BIT) != 0) {
        ShiftByImmediate = FALSE;
        ShiftRegister = (Instruction & SHIFT_REGISTER_MASK) >>
                        SHIFT_REGISTER_SHIFT;

        ShiftAmount = ArpGetArmRegister(TrapFrame, ShiftRegister);
        if (ShiftAmount > 32) {
            ShiftAmount = 0;
        }

    } else {
        ShiftByImmediate = TRUE;
        ShiftAmount = (Instruction & SHIFT_IMMEDIATE_MASK) >>
                      SHIFT_IMMEDIATE_SHIFT;
    }

    OffsetRegister = ArpGetArmRegister(TrapFrame,
                                       Instruction & OPERAND2_REGISTER_MASK);

    //
    // Determine the offset.
    //

    switch (Instruction & SHIFT_TYPE) {
    case SHIFT_LSL:
        Offset = OffsetRegister << ShiftAmount;
        break;

    //
    // Logical shift right fills the leftmost bits with zeroes.
    // Since in C this behavior is technically not
    // defined, enforce the zero fill by doing the shift manually.
    //

    case SHIFT_LSR:
        if ((ShiftByImmediate != FALSE) && (ShiftAmount == 0)) {
            ShiftAmount = 32;
        }

        Offset = OffsetRegister;
        while (ShiftAmount > 0) {
            Offset = (Offset >> 1) & 0x7FFFFFFF;
            ShiftAmount -= 1;
        }

        break;

    //
    // Arithmetic shift right fills the leftmost bits with zeroes
    // or ones depending on whether or not the previous bit was 0.
    // Again, since C is not well defined, enforce this manually.
    //

    case SHIFT_ASR:
        if ((ShiftByImmediate != FALSE) && (ShiftAmount == 0)) {
            ShiftAmount = 32;
        }

        Offset = OffsetRegister;
        while (ShiftAmount > 0) {
            if ((Offset & 0x80000000) != 0) {
                Offset = (Offset >> 1) | 0x80000000;

            } else {
                Offset = (Offset >> 1) & 0x7FFFFFFF;
            }

            ShiftAmount -= 1;
        }

        break;

    //
    // Rotate right is what it sounds like. Rotate right with
    // extend uses the carry bit as a 33rd bit. The extend is
    // specified with a shift immediate of 0.
    //

    case SHIFT_ROR:
        Offset = OffsetRegister;
        if ((ShiftByImmediate != FALSE) && (ShiftAmount == 0)) {
            if ((TrapFrame->Cpsr & PSR_FLAG_CARRY) != 0) {
                Offset = (Offset >> 1) | 0x80000000;

            } else {
                Offset = (Offset >> 1) & 0x7FFFFFFF;
            }

        //
        // Normal rotate right.
        //

        } else {
            while (ShiftAmount > 0) {
                if ((Offset & 0x00000001) != 0) {
                    Offset = (Offset >> 1) | 0x80000000;

                } else {
                    Offset = (Offset >> 1) & 0x7FFFFFFF;
                }

                ShiftAmount -= 1;
            }
        }

        break;
    }

    return Offset;
}

ULONG
ArpThumbGetInstructionSize (
    ULONG Instruction
    )

/*++

Routine Description:

    This routine determines the size of the given Thumb instruction.

Arguments:

    Instruction - Supplies the instruction itself.

Return Value:

    2 or 4, depending on whether this is a 16-bit Thumb instruction or a 32-bit
    instruction.

--*/

{

    ULONG Op;

    Op = (Instruction >> THUMB32_OP_SHIFT) & THUMB32_OP_MASK;

    //
    // Determine whether this is a 16 or 32-bit thumb instruction.
    //

    if (Op >= THUMB32_OP_MIN) {
        return THUMB32_INSTRUCTION_LENGTH;
    }

    return THUMB16_INSTRUCTION_LENGTH;
}

BOOL
ArpArmCheckConditionCode (
    ULONG Cpsr,
    UCHAR Condition
    )

/*++

Routine Description:

    This routine determines whether or not the condition code matches the
    current execution flags.

Arguments:

    Cpsr - Supplies the current program status register flags.

    Condition - Supplies the condition code.

Return Value:

    TRUE if the current condition applies.

    FALSE if the condition does not apply.

--*/

{

    BOOL Result;

    Result = FALSE;

    //
    // Handle the upper three bits.
    //

    switch (Condition >> 1) {
    case ARM_CONDITION_EQUAL:
        if ((Cpsr & PSR_FLAG_ZERO) != 0) {
            Result = TRUE;
        }

        break;

    case ARM_CONDITION_CARRY:
        if ((Cpsr & PSR_FLAG_CARRY) != 0) {
            Result = TRUE;
        }

        break;

    case ARM_CONDITION_NEGATIVE:
        if ((Cpsr & PSR_FLAG_NEGATIVE) != 0) {
            Result = TRUE;
        }

        break;

    case ARM_CONDITION_OVERFLOW:
        if ((Cpsr & PSR_FLAG_OVERFLOW) != 0) {
            Result = TRUE;
        }

        break;

    case ARM_CONDITION_UNSIGNED_GREATER:
        if (((Cpsr & PSR_FLAG_CARRY) != 0) &&
            ((Cpsr & PSR_FLAG_ZERO) == 0)) {

            Result = TRUE;
        }

        break;

    case ARM_CONDITION_SIGNED_GREATER_OR_EQUAL:

        //
        // Signed greater than or equal to is true when N == V. The XOR reports
        // when bits are different, so if it's zero, then N == V. The
        // comparisons against zero are necessary, otherwise different bits
        // are XORed.
        //

        if ((((Cpsr & PSR_FLAG_NEGATIVE) != 0) ^
            ((Cpsr & PSR_FLAG_OVERFLOW) != 0)) == 0) {

            Result = TRUE;
        }

        break;

    case ARM_CONDITION_SIGNED_GREATER:

        //
        // Signed greater than is true when N == V and Z == 0. See above for
        // how N == V is computed if the XOR is throwing you.
        //

        if (((Cpsr & PSR_FLAG_ZERO) == 0) &&
            ((((Cpsr & PSR_FLAG_NEGATIVE) != 0) ^
              ((Cpsr & PSR_FLAG_OVERFLOW) != 0)) == 0)) {

            Result = TRUE;
        }

        break;

    case ARM_CONDITION_ALWAYS:
    default:
        Result = TRUE;
        break;
    }

    //
    // The lowest bit, if set, simply negates the result.
    //

    if ((Condition & 0x1) != 0) {
        Result = !Result;
    }

    return Result;
}

