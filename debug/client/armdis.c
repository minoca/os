/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
DbgpArmDecodeDataProcessingClass (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeDataProcessing (
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
DbgpArmDecodeLoadStore (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeExtraLoadStore (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMediaInstruction (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeLoadStoreMultiple (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeSynchronization (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeUnconditional (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeMemoryHintSimdMisc (
    PARM_DISASSEMBLY Context
    );

VOID
DbgpArmDecodeCoprocessorSupervisor (
    PARM_DISASSEMBLY Context
    );

PSTR
DbgpArmGetPushPopTypeString (
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

//
// -------------------------------------------------------------------- Globals
//

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
    "ldrexb"
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

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgpArmDisassemble (
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

    PSTR BaseMnemonic;
    ULONG ConditionCode;
    ARM_DISASSEMBLY Context;
    LONG Offset;
    BOOL Result;

    RtlZeroMemory(Disassembly, sizeof(DISASSEMBLED_INSTRUCTION));
    if (BufferLength < ARM_INSTRUCTION_SIZE) {
        Result = FALSE;
        goto ArmDisassembleEnd;
    }

    RtlZeroMemory(&Context, sizeof(ARM_DISASSEMBLY));
    Context.Result = Disassembly;

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

    ASSERT(Language == MachineLanguageArm);

    ConditionCode = Context.Instruction >> ARM_CONDITION_SHIFT;

    //
    // Condition code F extensions are unconditional.
    //

    if (ConditionCode == ARM_CONDITION_NEVER) {
        DbgpArmDecodeUnconditional(&Context);

    } else {
        switch (Context.Instruction & ARM_INSTRUCTION_CLASS_MASK) {
        case ARM_INSTRUCTION_CLASS_DATA_PROCESSING:
        case ARM_INSTRUCTION_CLASS_DATA_PROCESSING2:
            DbgpArmDecodeDataProcessingClass(&Context);
            break;

        case ARM_INSTRUCTION_CLASS_LOAD_STORE:
            DbgpArmDecodeLoadStore(&Context);
            break;

        case ARM_INSTRUCTION_CLASS_LOAD_AND_MEDIA:

            //
            // If op is set, it's a media instruction.
            //

            if ((Context.Instruction & ARM_INSTRUCTION_CLASS_OP) != 0) {
                DbgpArmDecodeMediaInstruction(&Context);

            //
            // If op is clear, it's a load/store word and unsigned byte.
            //

            } else {
                DbgpArmDecodeLoadStore(&Context);
            }

            break;

        case ARM_INSTRUCTION_CLASS_BRANCH_AND_BLOCK:
        case ARM_INSTRUCTION_CLASS_BRANCH_AND_BLOCK2:

            //
            // If bits 20-25 are 10xxxx it's a branch, and if they're 11xxxx
            // it's a bl or blx.
            //

            if ((Context.Instruction & ARM_BRANCH_CLASS_BIT) != 0) {
                if ((Context.Instruction & ARM_BRANCH_LINK_BIT) != 0) {
                    BaseMnemonic = ARM_BL_MNEMONIC;

                } else {
                    BaseMnemonic = ARM_B_MNEMONIC;
                }

                Offset = (Context.Instruction & ARM_IMMEDIATE24_MASK) << 2;
                if ((Offset & 0x02000000) != 0) {
                    Offset |= 0xFC000000;
                }

                //
                // Add 4 because the relation is from the *next* instruction
                // (at pc + 4).
                //

                Offset += 4;
                strcpy(Context.Mnemonic, BaseMnemonic);
                sprintf(Context.Operand1, "#%+d", Offset);
                Disassembly->OperandAddress = (LONGLONG)Offset;
                Disassembly->AddressIsDestination = TRUE;
                Disassembly->OperandAddressRelation = RelationIp;

            //
            // Otherwise, treat it as a load/store multiple, push/pop, etc.
            //

            } else {
                DbgpArmDecodeLoadStoreMultiple(&Context);
            }

            break;

        case ARM_INSTRUCTION_CLASS_COPROCESSOR_SUPERVISOR:
        case ARM_INSTRUCTION_CLASS_COPROCESSOR_SUPERVISOR2:
            DbgpArmDecodeCoprocessorSupervisor(&Context);
            break;

        default:
            RtlDebugPrint("Unknown instruction class for instruction %x.\n",
                          Context.Instruction);

            Result = FALSE;
            goto ArmDisassembleEnd;
        }
    }

    //
    // Fill out the results and return successfully.
    //

    if (strlen(Context.Mnemonic) == 0) {
        strcpy(Context.Mnemonic, "Unknown");
        strcpy(Context.Operand1, "");
        strcpy(Context.Operand2, "");
        strcpy(Context.Operand3, "");
        strcpy(Context.Operand4, "");

    } else {
        strcat(Context.Mnemonic, DbgArmConditionCodes[ConditionCode]);
    }

    Result = TRUE;
    Disassembly->BinaryLength = 4;

ArmDisassembleEnd:
    if (strlen(Context.Mnemonic) + strlen(Context.Operand1) +
        strlen(Context.Operand2) + strlen(Context.Operand3) +
        strlen(Context.Operand4) + 5 > BufferLength) {

        Result = FALSE;
    }

    if (Result != FALSE) {
        strcpy(Buffer, Context.Mnemonic);
        Disassembly->Mnemonic = Buffer;
        Buffer += strlen(Context.Mnemonic) + 1;
        BufferLength -= strlen(Context.Mnemonic) + 1;
        if (strlen(Context.Operand1) != 0) {
            strcpy(Buffer, Context.Operand1);
            Disassembly->DestinationOperand = Buffer;
            Buffer += strlen(Context.Operand1) + 1;
            BufferLength -= strlen(Context.Operand1) + 1;
        }

        if (strlen(Context.Operand2) != 0) {
            strcpy(Buffer, Context.Operand2);
            Disassembly->SourceOperand = Buffer;
            Buffer += strlen(Context.Operand2) + 1;
            BufferLength -= strlen(Context.Operand2) + 1;
        }

        if (strlen(Context.Operand3) != 0) {
            strcpy(Buffer, Context.Operand3);
            Disassembly->ThirdOperand = Buffer;
            Buffer += strlen(Context.Operand3) + 1;
            BufferLength -= strlen(Context.Operand3) + 1;
        }

        if (strlen(Context.Operand4) != 0) {
            strcpy(Buffer, Context.Operand4);
            Disassembly->FourthOperand = Buffer;
        }
    }

    return Result;
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
    ULONG Coprocessor;
    ULONG Instruction;
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

    strcpy(Context->Mnemonic, BaseMnemonic);
    sprintf(Context->Operand1, "p%d, %d", Coprocessor, Opcode1);
    sprintf(Context->Operand3, "c%d, c%d, %d", RegisterN, RegisterM, Opcode2);
    return;
}

VOID
DbgpArmDecodeCoprocessorData (
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
    UCHAR CoprocessorDestination;
    UCHAR CoprocessorNumber;
    ULONG Immediate8;
    ULONG Instruction;
    PSTR MnemonicSuffix;
    UCHAR Rn;
    UCHAR SignCharacter;
    UCHAR WriteBack;

    Instruction = Context->Instruction;

    //
    // Determine whether it's a long load/store or regular.
    //

    MnemonicSuffix = "";
    if ((Instruction & ARM_COPROCESSOR_DATA_LONG_BIT) != 0) {
        MnemonicSuffix = ARM_COPROCESSOR_LONG_MNEMONIC;
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

    sprintf(Context->Mnemonic, "%s%s", BaseMnemonic, MnemonicSuffix);

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
        sprintf(Destination, "#02X", Mode);
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
DbgpArmDecodeDataProcessingClass (
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

    ULONG Op1;
    ULONG Op2;

    Op1 = (Context->Instruction & ARM_DATA_PROCESSING_OP1_MASK) >>
          ARM_DATA_PROCESSING_OP1_SHIFT;

    Op2 = (Context->Instruction & ARM_DATA_PROCESSING_OP2_MASK) >>
          ARM_DATA_PROCESSING_OP2_SHIFT;

    if ((Context->Instruction & ARM_DATA_PROCESSING_OP) == 0) {

        //
        // If op1 is not 10xx0, then it's a data processing (register)
        // operation.
        //

        if ((Op1 & ARM_DATA_PROCESSING_OP1_REGISTER_MASK) !=
            ARM_DATA_PROCESSING_OP1_MISCELLANEOUS) {

            //
            // If op2 is xxx0, then it's a data processing (register)
            // instruction.
            //

            if ((Op2 & ARM_DATA_PROCESSING_OP2_REGISTER_MASK) ==
                ARM_DATA_PROCESSING_OP2_REGISTER_VALUE) {

                DbgpArmDecodeDataProcessing(Context);

            //
            // If op2 are 1xx0, it's a data processing (register shifted
            // register) instruction.
            //

            } else if ((Op2 & ARM_DATA_PROCESSING_OP2_REGISTER_SHIFT_MASK) ==
                       ARM_DATA_PROCESSING_OP2_REGISTER_SHIFT_VALUE) {

                DbgpArmDecodeDataProcessing(Context);
            }

        //
        // If op1 is 10xx0, then it's a miscellaneous instruction.
        //

        } else {

            //
            // If op2 are 0xxx, it's a miscellaneous instruction.
            //

            if ((Op2 & ARM_DATA_PROCESSING_OP2_MISCELLANEOUS_MASK) ==
                ARM_DATA_PROCESSING_OP2_MISCELLANEOUS_VALUE) {

                DbgpArmDecodeMiscellaneous(Context);

            //
            // If op2 is 1xx0, it's a halfword multiply or multiply
            // accumulate.
            //

            } else if ((Op2 & ARM_DATA_PROCESSING_OP2_SMALL_MULTIPLY_MASK) ==
                       ARM_DATA_PROCESSING_OP2_SMALL_MULTIPLY_VALUE) {

                DbgpArmDecodeMultiply(Context);
            }
        }

        //
        // If op1 is 0xxxx and op2 is 1001, it's a multiply or multiply
        // accumulate.
        //

        if (((Op1 & ARM_DATA_PROCESSING_OP1_MULTIPLY_MASK) ==
             ARM_DATA_PROCESSING_OP1_MULTIPLY_VALUE) &&
            (Op2 == ARM_DATA_PROCESSING_OP2_MULTIPLY)) {

            DbgpArmDecodeMultiply(Context);
        }

        //
        // If op1 is 1xxxx and op2 is 1001, it's a synchronization
        // instruction.
        //

        if (((Op1 & ARM_DATA_PROCESSING_OP1_SYNCHRONIZATION_MASK) ==
             ARM_DATA_PROCESSING_OP1_SYNCHRONIZATION_VALUE) &&
            (Op2 == ARM_DATA_PROCESSING_OP2_SYNCHRONIZATION)) {

            DbgpArmDecodeSynchronization(Context);
        }

        //
        // If op1 is not 0xx1x...
        //

        if ((Op1 & ARM_DATA_PROCESSING_OP1_EXTRA_LOAD_STORE_MASK) !=
            ARM_DATA_PROCESSING_OP1_EXTRA_LOAD_STORE_VALUE) {

            //
            // If op2 is 1011 or 11x1, it's an extra load/store insruction.
            //

            if ((Op2 == ARM_DATA_PROCESSING_OP2_EXTRA_LOAD_UNPRIVILEGED) ||
                ((Op2 & ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_MASK) ==
                 ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_VALUE)) {

                DbgpArmDecodeExtraLoadStore(Context);
            }

        //
        // Op1 is 0xx1x.
        //

        } else {

            //
            // If op2 is 1011, it's an extra load/store unprivileged. If it's
            // 11x1, it's an extra load/store.
            //

            if ((Op2 == ARM_DATA_PROCESSING_OP2_EXTRA_LOAD_UNPRIVILEGED) ||
                ((Op2 & ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_MASK) ==
                 ARM_DATA_PROCESSING_OP2_EXTRA_LOAD2_VALUE)) {

                DbgpArmDecodeExtraLoadStore(Context);
            }
        }

    //
    // The op bit is one.
    //

    } else {

        //
        // If op1 is not 10xx0, then it's data processing (immediate).
        //

        if ((Op1 & ARM_DATA_PROCESSING_OP1_IMMEDIATE_MASK) !=
             ARM_DATA_PROCESSING_OP1_IMMEDIATE_VALUE) {

            DbgpArmDecodeDataProcessing(Context);
        }

        //
        // If op1 is 10000, it's a 16-bit immediate load.
        //

        if (Op1 == ARM_DATA_PROCESSING_OP1_LOAD_IMMEDIATE16) {
            DbgpArmDecodeDataProcessing(Context);
        }

        //
        // If op1 is 10100, it's a 16-bit immediate load to the high half of
        // a register.
        //

        if (Op1 == ARM_DATA_PROCESSING_OP1_LOAD_IMMEDIATE16_HIGH) {
            DbgpArmDecodeDataProcessing(Context);
        }

        if ((Op1 & ARM_DATA_PROCESSING_OP1_MSR_IMMEDIATE_MASK) ==
            ARM_DATA_PROCESSING_OP1_MSR_IMMEDIATE_VALUE) {

            DbgpArmDecodeMsrImmediateAndHints(Context);
        }
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

    BaseMnemonic = "ERR";
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

    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s",
             BaseMnemonic,
             MnemonicSuffix);

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

            sprintf(ShiftString,
                    "%s, %s %s",
                    DbgArmRegisterNames[Operand2Register],
                    ShiftType,
                    DbgArmRegisterNames[ShiftRegister]);

        //
        // Shift by an immediate value.
        //

        } else {
            ImmediateValue = (Instruction &
                              ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_MASK) >>
                             ARM_DATA_PROCESSING_SHIFT_IMMEDIATE_SHIFT;

            switch (Instruction & ARM_SHIFT_TYPE) {
            case ARM_SHIFT_LSL:
                if (ImmediateValue != 0) {
                    sprintf(ShiftString,
                            "%s, %s #%d",
                            DbgArmRegisterNames[Operand2Register],
                            ARM_LSL_MNEMONIC,
                            ImmediateValue);

                } else {
                    sprintf(ShiftString,
                            "%s",
                            DbgArmRegisterNames[Operand2Register]);
                }

                break;

            case ARM_SHIFT_LSR:
                if (ImmediateValue == 0) {
                    ImmediateValue = 32;
                }

                sprintf(ShiftString,
                        "%s, %s #%d",
                        DbgArmRegisterNames[Operand2Register],
                        ARM_LSR_MNEMONIC,
                        ImmediateValue);

                break;

            case ARM_SHIFT_ASR:
                if (ImmediateValue == 0) {
                    ImmediateValue = 32;
                }

                sprintf(ShiftString,
                        "%s, %s #%d",
                        DbgArmRegisterNames[Operand2Register],
                        ARM_ASR_MNEMONIC,
                        ImmediateValue);

                break;

            case ARM_SHIFT_ROR:
                if (ImmediateValue == 0) {
                    sprintf(ShiftString,
                            "%s, %s",
                            DbgArmRegisterNames[Operand2Register],
                            ARM_RRX_MNEMONIC);

                } else {
                    sprintf(ShiftString,
                            "%s, %s #%d",
                            DbgArmRegisterNames[Operand2Register],
                            ARM_ROR_MNEMONIC,
                            ImmediateValue);
                }

                break;

            //
            // This case should never hit since all 4 bit combinations are
            // covered.
            //

            default:
                break;
            }
        }
    }

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

        sprintf(Context->Operand2, "#%d", Immediate);
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

    if ((Instruction & ARM_MULTIPLY_SOURCE_HALF) != 0) {
        if ((Instruction & ARM_MULTIPLY_DESTINATION_HALF) != 0) {
            MultiplyHalves = ARM_MULTIPLY_TOP;

        } else {
            MultiplyHalves = ARM_MULTIPLY_TOP;
        }

    } else {
        if ((Instruction & ARM_MULTIPLY_DESTINATION_HALF) != 0) {
            MultiplyHalves = ARM_MULTIPLY_BOTTOM;

        } else {
            MultiplyHalves = ARM_MULTIPLY_BOTTOM;
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

        if ((Instruction & ARM_MULTIPLY_DESTINATION_HALF) != 0) {
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
    // Signed half word multiply accumulate, long dual, and
    // Signed half word multiply subtract, long dual and ,
    // Signed dual multiply add.
    //

    case ARM_SMLXLD_MASK:
        if ((Instruction & ARM_SMLXLD_SUBTRACT_BIT) != 0) {
            BaseMnemonic = ARM_SMLSLD_MNEMONIC;

        } else {
            BaseMnemonic = ARM_SMLALD_MNEMONIC;
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
                        DbgArmRegisterNames);

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
        OffsetRegister = Instruction & ARM_OFFSET_REGISTER;

        //
        // Build up the shift string.
        //

        strcpy(ShiftString, "");
        ShiftValue = (Instruction & ARM_LOAD_STORE_SHIFT_VALUE_MASK) >>
                     ARM_LOAD_STORE_SHIFT_VALUE_SHIFT;

        switch (Instruction & ARM_SHIFT_TYPE) {
        case ARM_SHIFT_LSL:

            //
            // Shift of 0 really does mean shift 0, so don't print anything out.
            //

            if (ShiftValue != 0) {
                sprintf(ShiftString,
                        ", %s #%d",
                        ARM_LSL_MNEMONIC,
                        ShiftValue);
            }

            break;

        //
        // Logical Shift Right can go from 1 to 32.
        //

        case ARM_SHIFT_LSR:
            if (ShiftValue == 0) {
                ShiftValue = 32;
            }

            sprintf(ShiftString,
                    ", %s #%d",
                    ARM_LSR_MNEMONIC,
                    ShiftValue);

            break;

       //
       // Arithmetic Shift Right also goes from 1 to 32.
       //

       case ARM_SHIFT_ASR:
            if (ShiftValue == 0) {
                ShiftValue = 32;
            }

            sprintf(ShiftString,
                    ", %s #%d",
                    ARM_ASR_MNEMONIC,
                    ShiftValue);

            break;

        //
        // Rotate right can go from 1 to 31. A shift value of zero specifies
        // Rotate Right with Extend.
        //

        case ARM_SHIFT_ROR:
            if (ShiftValue == 0) {
                sprintf(", %s", ARM_RRX_MNEMONIC);

            } else {
                sprintf(ShiftString,
                        ", %s #%d",
                        ARM_ROR_MNEMONIC,
                        ShiftValue);

                break;
            }

        //
        // This case will never hit, because all 4 bit possibilities were
        // handled.
        //

        default:
            break;
        }

        //
        // Check out the pre-index bit. If it's zero, the addressing mode is
        // post-indexed.
        //

        if ((Instruction & ARM_PREINDEX_BIT) == 0) {
            sprintf(Context->Operand2,
                    "[%s], %c%s%s",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    DbgArmRegisterNames[OffsetRegister],
                    ShiftString);

        //
        // Pre-indexed or offset addressing.
        //

        } else {
            sprintf(Context->Operand2,
                    "[%s, %c%s%s]%c",
                    DbgArmRegisterNames[BaseRegister],
                    Sign,
                    DbgArmRegisterNames[OffsetRegister],
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
        // pre-indexed or offset based, depending on the U bit.
        //

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

    LONG Value;

    //
    // Decode the instruction further, assuming that it has been correctly
    // categorized as a Media instruction.
    //

    if ((Context->Instruction & ARM_MEDIA_MULTIPLY_MASK) ==
        ARM_MEDIA_MULTIPLY_VALUE) {

        DbgpArmDecodeMultiply(Context);

    //
    // If op1 (bits 20-24) and op2 (bits 5-7) are all one, that's a
    // permanently undefined instruction.
    //

    } else if ((Context->Instruction & ARM_UNDEFINED_INSTRUCTION_MASK) ==
               ARM_UNDEFINED_INSTRUCTION_VALUE) {

        strcpy(Context->Mnemonic, ARM_UNDEFINED_INSTRUCTION_MNEMONIC);
        Value = ARM_SERVICE_BUILD_IMMEDIATE12_4(Context->Instruction);
        sprintf(Context->Operand1, "#%d ; 0x%x", Value, Value);
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
    ULONG RegisterList;
    UCHAR StackRegister;

    Instruction = Context->Instruction;

    //
    // Determine whether this was a load or store.
    //

    if ((Instruction & ARM_LOAD_BIT) != 0) {
        BaseMnemonic = ARM_LOAD_MULTIPLE_MNEMONIC;

    } else {
        BaseMnemonic = ARM_STORE_MULTIPLE_MNEMONIC;
    }

    //
    // Determine the stack type of the load/store.
    //

    MnemonicSuffix = DbgpArmGetPushPopTypeString(Instruction);
    snprintf(Context->Mnemonic,
             ARM_OPERAND_LENGTH,
             "%s%s",
             BaseMnemonic,
             MnemonicSuffix);

    //
    // Write the stack register (the first operand). Add the ! if the operation
    // does a write back.
    //

    StackRegister = (Instruction & ARM_DATA_PROCESSING_OPERAND_REGISTER_MASK) >>
                    ARM_DATA_PROCESSING_OPERAND_REGISTER_SHIFT;

    if ((Instruction & ARM_WRITE_BACK_BIT) != 0) {
        sprintf(Context->Operand1, "%s!", DbgArmRegisterNames[StackRegister]);

    } else {
        sprintf(Context->Operand1, "%s", DbgArmRegisterNames[StackRegister]);
    }

    //
    // Get the list of registers to be loaded or stored.
    //

    RegisterList = Instruction & ARM_REGISTER_LIST_MASK;
    DbgpArmDecodeRegisterList(Context->Operand2,
                              sizeof(Context->Operand2),
                              RegisterList);

    //
    // Indicate whether or not the saved PSR (SPSR) should be used instead of
    // the current PSR (CPSR). This is typically only used for returning from
    // exceptions.
    //

    if ((Instruction & ARM_USE_SAVED_PSR_BIT) != 0) {
        strcat(Context->Operand2, "^");
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

    PSTR MnemonicSuffix;
    LONG Offset;
    ULONG Op1;
    ULONG Rn;

    Op1 = (Context->Instruction & ARM_UNCONDITIONAL_OP1_MASK) >>
          ARM_UNCONDITIONAL_OP1_SHIFT;

    Rn = (Context->Instruction & ARM_UNCONDITIONAL_RN_MASK) >>
         ARM_UNCONDITIONAL_RN_SHIFT;

    //
    // If the high bit of op1 is clear, it's a memory hint, SIMD instruction,
    // or miscellaneous instruction.
    //

    if ((Context->Instruction &
         ARM_UNCONDITIONAL_MEMORY_HINTS_SIMD_MISC_BIT) == 0) {

        DbgpArmDecodeMemoryHintSimdMisc(Context);

    //
    // Handle and SRS (Store Return State) instruction.
    //

    } else if ((Op1 & ARM_UNCONDITIONAL_OP1_SRS_MASK) ==
               ARM_UNCONDITIONAL_OP1_SRS_VALUE) {

        MnemonicSuffix = DbgpArmGetPushPopTypeString(Context->Instruction);
        sprintf(Context->Mnemonic, "%s%s", ARM_SRS_MNEMONIC, MnemonicSuffix);
        DbgpArmPrintMode(Context->Operand2, Context->Instruction);
        if ((Context->Instruction & ARM_WRITE_BACK_BIT) != 0) {
            sprintf(Context->Operand1, "%s!, %s",
                    DbgArmRegisterNames[ARM_STACK_REGISTER],
                    Context->Operand2);

        } else {
            sprintf(Context->Operand1, "%s, %s",
                    DbgArmRegisterNames[ARM_STACK_REGISTER],
                    Context->Operand2);
        }

        Context->Operand2[0] = '\0';

    //
    // Handle an RFE (Return from exception) instruction.
    //

    } else if ((Op1 & ARM_UNCONDITIONAL_OP1_RFE_MASK) ==
               ARM_UNCONDITIONAL_OP1_RFE_VALUE) {

        MnemonicSuffix = DbgpArmGetPushPopTypeString(Context->Instruction);
        sprintf(Context->Mnemonic, "%s%s", ARM_RFE_MNEMONIC, MnemonicSuffix);
        if ((Context->Instruction & ARM_WRITE_BACK_BIT) != 0) {
            sprintf(Context->Operand1, "%s!", DbgArmRegisterNames[Rn]);

        } else {
            sprintf(Context->Operand1, "%s", DbgArmRegisterNames[Rn]);
        }

    //
    // Handle BL (branch with link) and BLX (same but with possible instruction
    // set change) instructions.
    //

    } else if ((Op1 & ARM_UNCONDITIONAL_OP1_BL_MASK) ==
               ARM_UNCONDITIONAL_OP1_BL_VALUE) {

        if ((Context->Instruction & ARM_BRANCH_LINK_X) != 0) {
            strcpy(Context->Mnemonic, ARM_BLX_MNEMONIC);

        } else {
            strcpy(Context->Mnemonic, ARM_BL_MNEMONIC);
        }

        Offset = (Context->Instruction & ARM_IMMEDIATE24_MASK) << 2;
        if ((Offset & 0x02000000) != 0) {
            Offset |= 0xFC000000;
        }

        //
        // Add 4 because the relation is from the *next* instruction (at pc +
        // 4).
        //

        Offset += 4;
        sprintf(Context->Operand1, "#%+d", Offset);
        Context->Result->OperandAddress = (LONGLONG)Offset;
        Context->Result->AddressIsDestination = TRUE;
        Context->Result->OperandAddressRelation = RelationIp;

    //
    // Handle coprocessor moves: MCR, MRC, and CDP.
    //

    } else if ((Op1 & ARM_UNCONDITIONAL_OP1_COPROCESSOR_MOVE_MASK) ==
               ARM_UNCONDITIONAL_OP1_COPROCESSOR_MOVE_VALUE) {

        DbgpArmDecodeCoprocessorMove(Context);
    }

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

    ULONG Instruction;
    ULONG Op1;
    ULONG Op2;

    Instruction = Context->Instruction;
    Op1 = (Instruction & ARM_MISCELLANEOUS2_OP1_MASK) >>
          ARM_MISCELLANEOUS2_OP1_SHIFT;

    Op2 = (Instruction & ARM_MISCELLANEOUS2_OP2_MASK) >>
          ARM_MISCELLANEOUS2_OP2_SHIFT;

    //
    // Handle a CPS instruction.
    //

    if (Op1 == ARM_MISCELLANEOUS2_OP1_CPS) {
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
    }

    //
    // Handle a clrex, dsb, dmb, or isb.
    //

    if (Op1 == ARM_MISCELLANEOUS2_OP1_BARRIERS) {
        if (Op2 == ARM_MISCELLANEOUS2_OP2_CLREX) {
            strcpy(Context->Mnemonic, ARM_CLREX_MNEMONIC);

        } else if (Op2 == ARM_MISCELLANEOUS2_OP2_DSB) {
            DbgpArmPrintBarrierMode(Context->Operand1, Instruction);
            strcpy(Context->Mnemonic, ARM_DSB_MNEMONIC);

        } else if (Op2 == ARM_MISCELLANEOUS2_OP2_DMB) {
            DbgpArmPrintBarrierMode(Context->Operand1, Instruction);
            strcpy(Context->Mnemonic, ARM_DMB_MNEMONIC);

        } else if (Op2 == ARM_MISCELLANEOUS2_OP2_ISB) {
            DbgpArmPrintBarrierMode(Context->Operand1, Instruction);
            strcpy(Context->Mnemonic, ARM_ISB_MNEMONIC);
        }
    }

    return;
}

VOID
DbgpArmDecodeCoprocessorSupervisor (
    PARM_DISASSEMBLY Context
    )

/*++

Routine Description:

    This routine decodes a coprocessor move or supervisor instruction.

Arguments:

    Context - Supplies a pointer to the disassembly context.

Return Value:

    None.

--*/

{

    ULONG Coprocessor;
    ULONG Instruction;
    ULONG Op1;

    Instruction = Context->Instruction;
    Op1 = (Instruction & ARM_SUPERVISOR_OP1_MASK) >> ARM_SUPERVISOR_OP1_SHIFT;
    Coprocessor = (Instruction & ARM_SUPERVISOR_COPROCESSOR_MASK) >>
                  ARM_SUPERVISOR_COPROCESSOR_SHIFT;

    //
    // Handle an SVC instruction.
    //

    if ((Op1 & ARM_SUPERVISOR_SVC_MASK) == ARM_SUPERVISOR_SVC_VALUE) {
        strcpy(Context->Mnemonic, ARM_SVC_MNEMONIC);
        sprintf(Context->Operand1, "#%d", Instruction & ARM_IMMEDIATE24_MASK);

    //
    // If bits 8-11 are 101x, then it's an advanced SIMD or floating point
    // operation.
    //

    } else if ((Coprocessor & ARM_SUPERVISOR_COPROCESSOR_MATH_MASK) ==
               ARM_SUPERVISOR_COPROCESSOR_MATH_VALUE) {

    //
    // If bits 8-11 are not 101x, then it's a coprocessor register or data move.
    //

    } else {

        //
        // If the high bit of op1 is set, it's a CDP, MCR, or MRC operation.
        //

        if ((Op1 & ARM_SUPERVISOR_OP1_REGISTER_BIT) != 0) {
            DbgpArmDecodeCoprocessorMove(Context);

        } else {
            DbgpArmDecodeCoprocessorData(Context);
        }
    }

    return;
}

PSTR
DbgpArmGetPushPopTypeString (
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

    switch (Instruction & ARM_PUSH_POP_TYPE_MASK) {
    case ARM_PUSH_POP_INCREMENT_AFTER:
        return ARM_INCREMENT_AFTER_SUFFIX;

    case ARM_PUSH_POP_INCREMENT_BEFORE:
        return ARM_INCREMENT_BEFORE_SUFFIX;

    case ARM_PUSH_POP_DECREMENT_AFTER:
        return ARM_DECREMENT_AFTER_SUFFIX;

    case ARM_PUSH_POP_DECREMENT_BEFORE:
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

