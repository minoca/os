/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disasm.h

Abstract:

    This header contains definitions for the disassembler.

Author:

    Evan Green 21-Jun-2012

--*/

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _MACHINE_LANGUAGE {
    MachineLanguageInvalid,
    MachineLanguageX86,
    MachineLanguageArm,
    MachineLanguageThumb2,
    MachineLanguageX64,
    MachineLanguageCount
} MACHINE_LANGUAGE, *PMACHINE_LANGUAGE;

/*++

Structure Description:

    This structure stores a disassembled instruction for use by external
    consumers of this module.

Members:

    Mnemonic - Stores a pointer to the string containing the human readable
        assembly mnemonic associated with this instruction.

    DestinationOperand - Stores a pointer to the string containing the
        destination operand. If the instruction has only one operand, it will
        be this one. This can be NULL if the instruction has 0 operands.

    SourceOperand - Stores a pointer to the string containing the source
        operand. If the instruction has 2 operands, this will be the second one.
        This can be NULL if the instruction has 0 or 1 operands.

    ThirdOperand - Stores a pointer to the string containing the third operand.
        This will be NULL for most x86 instructions, which have no third
        parameter.

    FourthOperand - Stores a pointer to the string containing the fourth
        operand. This is only used on ARM.

    OperandAddress - Stores the numeric address if one of the operands contains
        an address.

    AddressIsValid - Stores a boolean indicating whether the address in
        OperandAddress is valid.

    AddressIsDestionation - Stores a flag indicating whether the address in
        OperandAddress refers to the source operand or the destination operand.

    BinaryLength - Stores the size of the instruction, in bytes. This can be
        useful for advancing the instruction stream by the number of bytes just
        disassembled in this instruction.

--*/

typedef struct _DISASSEMBLED_INSTRUCTION {
    PSTR Mnemonic;
    PSTR DestinationOperand;
    PSTR SourceOperand;
    PSTR ThirdOperand;
    PSTR FourthOperand;
    ULONGLONG OperandAddress;
    BOOL AddressIsValid;
    BOOL AddressIsDestination;
    ULONG BinaryLength;
} DISASSEMBLED_INSTRUCTION, *PDISASSEMBLED_INSTRUCTION;

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
DbgDisassemble (
    ULONGLONG InstructionPointer,
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly,
    MACHINE_LANGUAGE Language
    );

/*++

Routine Description:

    This routine decodes one instruction from a binary instruction stream into
    a human readable form.

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

BOOL
DbgpX86Disassemble (
    ULONGLONG InstructionPointer,
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly,
    MACHINE_LANGUAGE Language
    );

/*++

Routine Description:

    This routine decodes one instruction from an IA-32 binary instruction
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

    Language - Supplies the type of machine langage being decoded.

Return Value:

    TRUE on success.

    FALSE if the instruction was unknown.

--*/

BOOL
DbgpArmDisassemble (
    ULONGLONG InsructionPointer,
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly,
    MACHINE_LANGUAGE Language
    );

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

