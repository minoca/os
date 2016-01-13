/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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
    MachineLanguageThumb2
} MACHINE_LANGUAGE, *PMACHINE_LANGUAGE;

typedef enum _ADDRESS_RELATION {
    RelationInvalid,
    RelationAbsolute,
    RelationIp
} ADDRESS_RELATION, *PADDRESS_RELATION;

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

    OperandAddressRelation - Stores information about what the OperandAddress
        parameter is relative to, or whether or not the address is valid at all.
        For example, some operands will give an address relative to the
        instruction pointer.

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
    ADDRESS_RELATION OperandAddressRelation;
    BOOL AddressIsDestination;
    ULONG BinaryLength;
} DISASSEMBLED_INSTRUCTION, *PDISASSEMBLED_INSTRUCTION;

//
// -------------------------------------------------------- Function Prototypes
//

BOOL
DbgDisassemble (
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
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly
    );

/*++

Routine Description:

    This routine decodes one instruction from an IA-32 binary instruction
    stream into a human readable form.

Arguments:

    InstructionStream - Supplies a pointer to the binary instruction stream.

    Buffer - Supplies a pointer to the buffer where the human
        readable strings will be printed. This buffer must be allocated by the
        caller.

    BufferLength - Supplies the length of the supplied buffer.

    Disassembly - Supplies a pointer to the structure that will receive
        information about the instruction.

Return Value:

    TRUE on success.

    FALSE if the instruction was unknown.

--*/

BOOL
DbgpArmDisassemble (
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

