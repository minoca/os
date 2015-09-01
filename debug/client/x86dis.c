/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    x86dis.c

Abstract:

    This module contains routines for disassembling x86 binary code.

Author:

    Evan Green 21-Jun-2012

Environment:

    Debugging client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/types.h>
#include "disasm.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the meanings of some of size characters used in the encoding table.
//

#define X86_WIDTH_BYTE 'b'
#define X86_WIDTH_WORD 'w'
#define X86_WIDTH_LONG 'l'
#define X86_WIDTH_LONGLONG 'q'
#define X86_FLOATING_POINT_REGISTER 'f'

//
// Define the internal bitfields of the ModR/M and SIB byte.
//

#define X86_MOD_MASK 0xC0
#define X86_REG_MASK 0x38
#define X86_RM_MASK 0x07
#define X86_MOD_SHIFT 6
#define X86_REG_SHIFT 3
#define X86_RM_SHIFT 0
#define X86_SCALE_MASK 0xC0
#define X86_INDEX_MASK 0x38
#define X86_BASE_MASK 0x07
#define X86_SCALE_SHIFT 6
#define X86_INDEX_SHIFT 3
#define X86_BASE_SHIFT 0

//
// Define some of the prefixes that can come at the beginning of an instruction.
//

#define X86_MAX_PREFIXES 4
#define X86_OPERAND_OVERRIDE 0x66
#define X86_ADDRESS_OVERRIDE 0x67
#define X86_ESCAPE_OPCODE 0x0F
#define X86_PREFIX_LOCK 0xF0
#define X86_PREFIX_REP1 0xF2
#define X86_PREFIX_REP2 0xF3
#define X86_PREFIX_CS 0x2E
#define X86_PREFIX_DS 0x3E
#define X86_PREFIX_ES 0x26
#define X86_PREFIX_SS 0x36

//
// This mask/value combination covers the FS prefix, GS prefix, Operand
// override, and Address override.
//

#define X86_PREFIX_FS_GS_OVERRIDE_MASK 0xFC
#define X86_PREFIX_FS_GS_OVERRIDE_VALUE 0x64

//
// For opcode groups with fewer than the maximum number of possible opcodes,
// define how many opcodes each group does have.
//

#define X86_GROUP_4_INSTRUCTION_COUNT 2
#define X86_GROUP_5_INSTRUCTION_COUNT 7
#define X86_GROUP_6_INSTRUCTION_COUNT 6
#define X86_GROUP_8_FIRST_INSTRUCTION 4
#define X86_GROUP_9_ONLY_VALID_INSTRUCTION 1
#define X86_INVALID_GROUP 99

//
// Define the sizes of the register name arrays.
//

#define X86_DEBUG_REGISTER_COUNT 8
#define X86_SEGMENT_REGISTER_COUNT 6
#define X86_REGISTER_NAME_COUNT 8

//
// Define the size of the working buffers.
//

#define X86_WORKING_BUFFER_SIZE 100

//
// Define the multiplication and shift opcodes that have 3 operands.
//

#define X86_OPCODE1_IMUL1 0x69
#define X86_OPCODE1_IMUL2 0x6B
#define X86_OPCODE2_SHLD1 0xA4
#define X86_OPCODE2_SHLD2 0xA5
#define X86_OPCODE2_SHRD1 0xAC
#define X86_OPCODE2_SHRD2 0xAD

//
// Define some x87 floating point support definitions, constants used in
// decoding an x87 coprocessor instruction.
//

#define X87_ESCAPE_OFFSET 0xD8
#define X87_FCOM_MASK 0xF8
#define X87_FCOM_OPCODE 0xD0
#define X87_D9_E0_OFFSET 0xE0
#define X87_DA_C0_MASK 0x38
#define X87_DA_CO_SHIFT 3
#define X87_FUCOMPP_OPCODE 0xE9
#define X87_DB_C0_MASK 0x38
#define X87_DB_C0_SHIFT 3
#define X87_DB_E0_INDEX 4
#define X87_DB_E0_MASK 0x7
#define X87_DF_C0_MASK 0x38
#define X87_DF_C0_SHIFT 3
#define X87_DF_E0_INDEX 4
#define X87_DF_E0_MASK 0x07
#define X87_DF_E0_COUNT 3

#define X87_REGISTER_TARGET "Rf"
#define X87_ST0_TARGET "! st"
#define X87_FLD_MNEMONIC "fld"
#define X87_FXCH_MNEMONIC "fxch"
#define X87_NOP_MNEMONIC "fnop"
#define X87_FSTP1_MNEMONIC "fstp1"
#define X87_FUCOMPP_MNEMONIC "fucompp"
#define X87_DF_E0_TARGET "! ax"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure provides basic information about an instruction, including
    its mnemonic name, operand encodings, and additional parsing information.

Members:

    Mnemonic - Stores a pointer to the string containing the opcode's mnemonic.

    Target - Stores a pointer to a string describing the destination operand's
        encoding.

    Source - Stores a pointer to a string describing the source operand's
        encoding.

    Group - Stores the opcode group number. Some instructions require furthur
        decoding, the group number indicates that.

--*/

typedef struct _X86_INSTRUCTION_DEFINITION {
    PSTR Mnemonic;
    PSTR Target;
    PSTR Source;
    INT Group;
} X86_INSTRUCTION_DEFINITION, *PX86_INSTRUCTION_DEFINITION;

/*++

Structure Description:

    This structure stores information about an instructions mnemonics and
    encoding when an array index is wasteful for describing the actual opcode
    number.

Members:

    Prefix - Stores the specfic prefix value for which this instruction is
        valid.

    Opcode - Stores the opcode this definition defines.

    Instruction - Stores an array describing the mnemonics and encoding of the
        instruction.

--*/

typedef struct _X86_SPARSE_INSTRUCTION_DEFINITION {
    BYTE Prefix;
    BYTE Opcode;
    X86_INSTRUCTION_DEFINITION Instruction;
} X86_SPARSE_INSTRUCTION_DEFINITION, *PX86_SPARSE_INSTRUCTION_DEFINITION;

/*++

Structure Description:

    This structure stores all binary information about a decoded instruction.

Members:

    Prefix - Stores up to 4 prefix bytes, which is the maximum number of
        allowed prefixes in x86 instructions.

    Opcode - Stores the first (and many times only) opcode byte.

    Opcode2 - Stores the second opcode byte, if necessary (as determined by the
        first opcode byte).

    ModRm - Stores the ModR/M byte of the instruction, if one exists. Bits 6-7
        describe the Mod part of the instruction, which can describe what sort
        of addressing/displacement is encoded in the instruction. Bits 5-3 hold
        the Reg data, which stores either a register number or additional
        decoding information for instruction groups. Bits 2-0 store the R/M
        byte, which either stores another register value or describes how the
        memory information is encoded.

    Sib - Stores the Scale/Index/Base byte of the opcode, if one exists.
        Addressing with an sib byte usually looks like (Base + index * 2^Scale),
        where Base and Index are both general registers. Bits 6-7 describe the
        scale, which is raised to the power of two (and can therefore describe a
        scale of 1, 2, 4, or 8). Bits 5-3 describe the index register, and bits
        2-0 describe the base register, both of which are encoded like the Reg
        field for general registers.

    Displacement - Stores the displacement of the instruction operand.

    Immediate - Stores the immediate value that may or may not be encoded in the
        instruction.

    Length - Stores the total size of this instruction encoding in bytes.

    DisplacementSize - Stores the size in bytes of the displacement value. Once
        the Displacement field is populated, this field is not too useful.

    ImmediateSize - Stores the size in bytes of the immediate value. Once the
        Immediate field is populated, this field is not too useful.

    OperandOverride - Stores a flag indicating whether or not the operand
        override prefix was on this instruction.

    AddressOverrride - Stores a flag indicating whether or not the address
        override prefix was specified on this instruction.

    Definition - Stores a the instruction decoding information,
        including the instruction mnemonic.

--*/

typedef struct _X86_INSTRUCTION {
    BYTE Prefix[X86_MAX_PREFIXES];
    BYTE Opcode;
    BYTE Opcode2;
    BYTE ModRm;
    BYTE Sib;
    ULONG Displacement;
    ULONG Immediate;
    ULONG Length;
    ULONG DisplacementSize;
    ULONG ImmediateSize;
    BOOL OperandOverride;
    BOOL AddressOverride;
    X86_INSTRUCTION_DEFINITION Definition;
} X86_INSTRUCTION, *PX86_INSTRUCTION;

typedef enum _X86_REGISTER {
    X86RegisterValueEax,
    X86RegisterValueEcx,
    X86RegisterValueEdx,
    X86RegisterValueEbx,
    X86RegisterValueEsp,
    X86RegisterValueEbp,
    X86RegisterValueEsi,
    X86RegisterValueEdi,
    X86RegisterValueScaleIndexBase,
    X86RegisterValueDisplacement32,
} X86_REGISTER, *PX86_REGISTER;

typedef enum _X86_MOD_VALUE {
    X86ModValueNoDisplacement,
    X86ModValueDisplacement8,
    X86ModValueDisplacement32,
    X86ModValueRegister
} X86_MOD_VALUE, *PX86_MOD_VALUE;

//
// -------------------------------------------------------------------- Globals
//

//
// Define working buffers. Note that this makes the disassembly code not thread
// safe.
//

CHAR DbgX86DisassemblyBuffer[X86_WORKING_BUFFER_SIZE];
CHAR DbgX86OperandBuffer[X86_WORKING_BUFFER_SIZE];

//
// Define the x86 instruction encodings.
//

X86_INSTRUCTION_DEFINITION DbgX86Instructions[256] = {
    {"add", "Eb", "Gb", 0},                     /* 00 */
    {"add", "Ev", "Gv", 0},                     /* 01 */
    {"add", "Gb", "Eb", 0},                     /* 02 */
    {"add", "Gv", "Ev", 0},                     /* 03 */
    {"add", "!bal", "Ib", 0},                   /* 04 */
    {"add", "!rax", "Iz", 0},                   /* 05 */
    {"push", "!wes", "", 0},                    /* 06 */
    {"pop", "!wes", "", 0},                     /* 07 */
    {"or", "Eb", "Gb", 0},                      /* 08 */
    {"or", "Ev", "Gv", 0},                      /* 09 */
    {"or", "Gb", "Eb", 0},                      /* 0A */
    {"or", "Gv", "Ev", 0},                      /* 0B */
    {"or", "!bal", "Ib", 0},                    /* 0C */
    {"or", "!rax", "Iz", 0},                    /* 0D */
    {"push", "!wcs", "", 0},                    /* 0E */
    {"2BYTE", "", "", X86_INVALID_GROUP},       /* 0F */ /* Two Byte Opcodes */
    {"adc", "Eb", "Gb", 0},                     /* 10 */
    {"adc", "Ev", "Gv", 0},                     /* 11 */
    {"adc", "Gb", "Eb", 0},                     /* 12 */
    {"adc", "Gv", "Ev", 0},                     /* 13 */
    {"adc", "!bal", "Ib", 0},                   /* 14 */
    {"adc", "!rax", "Iz", 0},                   /* 15 */
    {"push", "!wss", "", 0},                    /* 16 */
    {"pop", "!wss", "", 0},                     /* 17 */
    {"sbb", "Eb", "Gb", 0},                     /* 18 */
    {"sbb", "Ev", "Gv", 0},                     /* 19 */
    {"sbb", "Gb", "Eb", 0},                     /* 1A */
    {"sbb", "Gv", "Ev", 0},                     /* 1B */
    {"sbb", "!bal", "Ib", 0},                   /* 1C */
    {"sbb", "!rax", "Iz", 0},                   /* 1D */
    {"push", "!wds", "", 0},                    /* 1E */
    {"pop", "!wds", "", 0},                     /* 1F */
    {"and", "Eb", "Gb", 0},                     /* 20 */
    {"and", "Ev", "Gv", 0},                     /* 21 */
    {"and", "Gb", "Eb", 0},                     /* 22 */
    {"and", "Gv", "Ev", 0},                     /* 23 */
    {"and", "!bal", "Ib", 0},                   /* 24 */
    {"and", "!rax", "Iz", 0},                   /* 25 */
    {"ES:", "", "", X86_INVALID_GROUP},         /* 26 */ /* ES prefix */
    {"daa", "", "", 0},                         /* 27 */
    {"sub", "Eb", "Gb", 0},                     /* 28 */
    {"sub", "Ev", "Gv", 0},                     /* 29 */
    {"sub", "Gb", "Eb", 0},                     /* 2A */
    {"sub", "Gv", "Ev", 0},                     /* 2B */
    {"sub", "!bal", "Ib", 0},                   /* 2C */
    {"sub", "!rax", "Iz", 0},                   /* 2D */
    {"CS:", "", "", X86_INVALID_GROUP},         /* 2E */ /* CS prefix */
    {"das", "", "", 0},                         /* 2F */
    {"xor", "Eb", "Gb", 0},                     /* 30 */
    {"xor", "Ev", "Gv", 0},                     /* 31 */
    {"xor", "Gb", "Eb", 0},                     /* 32 */
    {"xor", "Gv", "Ev", 0},                     /* 33 */
    {"xor", "!bal", "Ib", 0},                   /* 34 */
    {"xor", "!rax", "Iz", 0},                   /* 35 */
    {"SS:", "", "", X86_INVALID_GROUP},         /* 36 */ /* SS prefix */
    {"aaa", "", "", 0},                         /* 37 */
    {"cmp", "Eb", "Gb", 0},                     /* 38 */
    {"cmp", "Ev", "Gv", 0},                     /* 39 */
    {"cmp", "Gb", "Eb", 0},                     /* 3A */
    {"cmp", "Gv", "Ev", 0},                     /* 3B */
    {"cmp", "!bal", "Ib", 0},                   /* 3C */
    {"cmp", "!rax", "Iz", 0},                   /* 3D */
    {"DS:", "", "", X86_INVALID_GROUP},         /* 3E */ /* DS prefix */
    {"aas", "", "", 0},                         /* 3F */
    {"inc", "!eax", "", 0},                     /* 40 */
    {"inc", "!ecx", "", 0},                     /* 41 */
    {"inc", "!edx", "", 0},                     /* 42 */
    {"inc", "!ebx", "", 0},                     /* 43 */
    {"inc", "!esp", "", 0},                     /* 44 */
    {"inc", "!ebp", "", 0},                     /* 45 */
    {"inc", "!esi", "", 0},                     /* 46 */
    {"inc", "!edi", "", 0},                     /* 47 */
    {"dec", "!eax", "", 0},                     /* 48 */
    {"dec", "!ecx", "", 0},                     /* 49 */
    {"dec", "!edx", "", 0},                     /* 4A */
    {"dec", "!ebx", "", 0},                     /* 4B */
    {"dec", "!esp", "", 0},                     /* 4C */
    {"dec", "!ebp", "", 0},                     /* 4D */
    {"dec", "!esi", "", 0},                     /* 4E */
    {"dec", "!edi", "", 0},                     /* 4F */
    {"push", "!rax", "", 0},                    /* 50 */
    {"push", "!rcx", "", 0},                    /* 51 */
    {"push", "!rdx", "", 0},                    /* 52 */
    {"push", "!rbx", "", 0},                    /* 53 */
    {"push", "!rsp", "", 0},                    /* 54 */
    {"push", "!rbp", "", 0},                    /* 55 */
    {"push", "!rsi", "", 0},                    /* 56 */
    {"push", "!rdi", "", 0},                    /* 57 */
    {"pop", "!rax", "", 0},                     /* 58 */
    {"pop", "!rcx", "", 0},                     /* 59 */
    {"pop", "!rdx", "", 0},                     /* 5A */
    {"pop", "!rbx", "", 0},                     /* 5B */
    {"pop", "!rsp", "", 0},                     /* 5C */
    {"pop", "!rbp", "", 0},                     /* 5D */
    {"pop", "!rsi", "", 0},                     /* 5E */
    {"pop", "!rdi", "", 0},                     /* 5F */
    {"pushad", "", "", 0},                      /* 60 */
    {"popad", "", "", 0},                       /* 61 */
    {"bound", "Gv", "Ma", 0},                   /* 62 */
    {"arpl", "Ew", "Gw", 0},                    /* 63 */
    {"FS:", "", "", X86_INVALID_GROUP},         /* 64 */ /* FS prefix */
    {"GS:", "", "", X86_INVALID_GROUP},         /* 65 */ /* GS prefix */
    {"OPSIZE:", "", "", X86_INVALID_GROUP},     /* 66 */ /* Operand override */
    {"ADSIZE:", "", "", X86_INVALID_GROUP},     /* 67 */ /* Address override */
    {"push", "Iz", "", 0},                      /* 68 */
    {"imul", "Gv", "Ev", 0},                    /* 69 */ /* Also has Iz */
    {"push", "Ib", "", 0},                      /* 6A */
    {"imul", "Gv", "Ev", 0},                    /* 6B */ /* Also has Ib */
    {"ins", "Yb", "!wdx", 0},                   /* 6C */
    {"ins", "Yz", "!wdx", 0},                   /* 6D */
    {"outs", "!wdx", "Xb", 0},                  /* 6E */
    {"outs", "!wdx", "Xz", 0},                  /* 6F */
    {"jo ", "Jb", "", 0},                       /* 70 */
    {"jno", "Jb", "", 0},                       /* 71 */
    {"jb ", "Jb", "", 0},                       /* 72 */
    {"jnb", "Jb", "", 0},                       /* 73 */
    {"jz ", "Jb", "", 0},                       /* 74 */
    {"jnz", "Jb", "", 0},                       /* 75 */
    {"jbe", "Jb", "", 0},                       /* 76 */
    {"jnbe", "Jb", "", 0},                      /* 77 */
    {"js ", "Jb", "", 0},                       /* 78 */
    {"jns", "Jb", "", 0},                       /* 79 */
    {"jp ", "Jb", "", 0},                       /* 7A */
    {"jnp", "Jb", "", 0},                       /* 7B */
    {"jl ", "Jb", "", 0},                       /* 7C */
    {"jnl", "Jb", "", 0},                       /* 7D */
    {"jle", "Jb", "", 0},                       /* 7E */
    {"jnle", "Jb", "", 0},                      /* 7F */
    {"GRP1", "Eb", "Ib", 1},                    /* 80 */ /* Group 1 opcodes. */
    {"GRP1", "Ev", "Iz", 1},                    /* 81 */ /* Reg of ModR/M */
    {"GRP1", "Eb", "Ib", 1},                    /* 82 */ /* extends opcode.*/
    {"GRP1", "Ev", "Ib", 1},                    /* 83 */
    {"test", "Eb", "Gb", 0},                    /* 84 */
    {"test", "Ev", "Gv", 0},                    /* 85 */
    {"xchg", "Eb", "Eb", 0},                    /* 86 */
    {"xchg", "Ev", "Gv", 0},                    /* 87 */
    {"mov", "Eb", "Gb", 0},                     /* 88 */
    {"mov", "Ev", "Gv", 0},                     /* 89 */
    {"mov", "Gb", "Eb", 0},                     /* 8A */
    {"mov", "Gv", "Ev", 0},                     /* 8B */
    {"mov", "Ev", "Sw", 0},                     /* 8C */
    {"lea", "Gv", "Ml", 0},                     /* 8D */
    {"mov", "Sw", "Ev", 0},                     /* 8E */
    {"pop", "Ev", "", 10},                      /* 8F */ /* Group 10 */
    {"nop", "", "", 0},                         /* 90 */ /* nop */
    {"xchg", "!rcx", "!rax", 0},                /* 91 */
    {"xchg", "!rdx", "!rax", 0},                /* 92 */
    {"xchg", "!rbx", "!rax", 0},                /* 93 */
    {"xchg", "!rsp", "!rax", 0},                /* 94 */
    {"xchg", "!rbp", "!rax", 0},                /* 95 */
    {"xchg", "!rsi", "!rax", 0},                /* 96 */
    {"xchg", "!rdi", "!rax", 0},                /* 97 */
    {"cwde", "", "", 0},                        /* 98 */
    {"cdq", "", "", 0},                         /* 99 */
    {"call", "Ap", "", 0},                      /* 9A */
    {"fwait", "", "", 0},                       /* 9B */
    {"pushf", "", "", 0},                       /* 9C */ /* arg1 = Fv */
    {"popf", "", "", 0},                        /* 9D */ /* arg1 = Fv */
    {"sahf", "", "", 0},                        /* 9E */
    {"lafh", "", "", 0},                        /* 9F */
    {"mov", "!bal", "Ob", 0},                   /* A0 */
    {"mov", "!rax", "Ov", 0},                   /* A1 */
    {"mov", "Ob", "!bal", 0},                   /* A2 */
    {"mov", "Ov", "!rax", 0},                   /* A3 */
    {"movs", "Yb", "Xb", 0},                    /* A4 */
    {"movs", "Yv", "Xv", 0},                    /* A5 */
    {"cmps", "Yb", "Xb", 0},                    /* A6 */
    {"cmps", "Yv", "Xv", 0},                    /* A7 */
    {"test", "!bal", "Ib", 0},                  /* A8 */
    {"test", "!rax", "Iz", 0},                  /* A9 */
    {"stos", "Yb", "!bal", 0},                  /* AA */
    {"stos", "Yv", "!rax", 0},                  /* AB */
    {"lods", "!bal", "Xb", 0},                  /* AC */
    {"lods", "!rax", "Xv", 0},                  /* AD */
    {"scas", "Yb", "!bal", 0},                  /* AE */
    {"scas", "Yv", "!rax", 0},                  /* AF */
    {"mov", "!bal", "Ib", 0},                   /* B0 */
    {"mov", "!bcl", "Ib", 0},                   /* B1 */
    {"mov", "!bdl", "Ib", 0},                   /* B2 */
    {"mov", "!bbl", "Ib", 0},                   /* B3 */
    {"mov", "!bah", "Ib", 0},                   /* B4 */
    {"mov", "!bch", "Ib", 0},                   /* B5 */
    {"mov", "!bdh", "Ib", 0},                   /* B6 */
    {"mov", "!bbh", "Ib", 0},                   /* B7 */
    {"mov", "!rax", "Iv", 0},                   /* B8 */
    {"mov", "!rcx", "Iv", 0},                   /* B9 */
    {"mov", "!rdx", "Iv", 0},                   /* BA */
    {"mov", "!rbx", "Iv", 0},                   /* BB */
    {"mov", "!rsp", "Iv", 0},                   /* BC */
    {"mov", "!rbp", "Iv", 0},                   /* BD */
    {"mov", "!rsi", "Iv", 0},                   /* BE */
    {"mov", "!rdi", "Iv", 0},                   /* BF */
    {"GRP2", "Eb", "Ib", 2},                    /* C0 */ /* Group 2 */
    {"GRP2", "Ev", "Ib", 2},                    /* C1 */ /* Group 2 */
    {"ret", "Iw", "", 0},                       /* C2 */
    {"ret", "", "", 0},                         /* C3 */
    {"les", "Gz", "Mp", 0},                     /* C4 */
    {"lds", "Gz", "Mp", 0},                     /* C5 */
    {"mov", "Eb", "Ib", 12},                    /* C6 */ /* Group 12 */
    {"mov", "Ev", "Iz", 12},                    /* C7 */ /* Group 12 */
    {"enter", "Iw", "Ib", 0},                   /* C8 */
    {"leave", "", "", 0},                       /* C9 */
    {"retf", "Iw", "", 0},                      /* CA */
    {"retf", "", "", 0},                        /* CB */
    {"int", "!b3", "", 0},                      /* CC */ /* Int 3 */
    {"int", "Ib", "", 0},                       /* CD */
    {"into", "", "", 0},                        /* CE */
    {"iret", "", "", 0},                        /* CF */
    {"GRP2", "Eb", "!b1", 2},                   /* D0 */ /* Group 2, arg2 = 1 */
    {"GRP2", "Ev", "!b1", 2},                   /* D1 */ /* Group 2, arg2 = 1 */
    {"GRP2", "Eb", "!bcl", 2},                  /* D2 */ /* Group 2 */
    {"GRP2", "Ev", "!bcl", 2},                  /* D3 */ /* Group 2 */
    {"aam", "Ib", "", 0},                       /* D4 */
    {"aad", "Ib", "", 0},                       /* D5 */
    {"setalc", "", "", 0},                      /* D6 */
    {"xlat", "", "", 0},                        /* D7 */
    {"ESC0", "Ev", "", 0x87},                   /* D8 */ /* x87 Floating Pt */
    {"ESC1", "Ev", "", 0x87},                   /* D9 */
    {"ESC2", "Ev", "", 0x87},                   /* DA */
    {"ESC3", "Ev", "", 0x87},                   /* DB */
    {"ESC4", "Ev", "", 0x87},                   /* DC */
    {"ESC5", "Ev", "", 0x87},                   /* DD */
    {"ESC6", "Ev", "", 0x87},                   /* DE */
    {"ESC7", "Ev", "", 0x87},                   /* DF */
    {"loopnz", "Jb", "", 0},                    /* E0 */
    {"loopz", "Jb", "", 0},                     /* E1 */
    {"loop", "Jb", "", 0},                      /* E2 */
    {"jecx", "Jb", "", 0},                      /* E3 */
    {"in ", "!bal", "Ib", 0},                   /* E4 */
    {"in ", "!eax", "Iv", 0},                   /* E5 */
    {"out", "Ib", "!bal", 0},                   /* E6 */
    {"out", "Ib", "!eax", 0},                   /* E7 */
    {"call", "Jz", "", 0},                      /* E8 */
    {"jmp", "Jz", "", 0},                       /* E9 */
    {"jmp", "Ap", "", 0},                       /* EA */
    {"jmp", "Jb", "", 0},                       /* EB */
    {"in ", "!bal", "!wdx", 0},                 /* EC */
    {"in ", "!eax", "!wdx", 0},                 /* ED */
    {"out", "!wdx", "!bal", 0},                 /* EE */
    {"out", "!wdx", "!eax", 0},                 /* EF */
    {"LOCK:", "", "", 0},                       /* F0 */ /* Lock prefix */
    {"int", "!b1", "", 0},                      /* F1 */ /* Int 1 */
    {"REPNE:", "", "", 0},                      /* F2 */ /* Repne prefix */
    {"REP:", "", "", 0},                        /* F3 */ /* Rep prefix */
    {"hlt", "", "", 0},                         /* F4 */
    {"cmc", "", "", 0},                         /* F5 */
    {"GRP3", "Eb", "", 3},                      /* F6 */ /* Group 3 */
    {"GRP3", "Ev", "", 0x3A},                   /* F7 */ /* Group 3A */
    {"clc", "", "", 0},                         /* F8 */
    {"stc", "", "", 0},                         /* F9 */
    {"cli", "", "", 0},                         /* FA */
    {"sti", "", "", 0},                         /* FB */
    {"cld", "", "", 0},                         /* FC */
    {"std", "", "", 0},                         /* FD */
    {"GRP4", "Eb", "", 4},                      /* FE */ /* Group 4 */
    {"GRP5", "Ev", "", 5},                      /* FF */ /* Group 5 */
};

X86_SPARSE_INSTRUCTION_DEFINITION DbgX86TwoByteInstructions[] = {
    {0, 0x0, {"GRP6", "", "", 6}},              /* 00 */ /* Group 6 */
    {0, 0x1, {"GRP7", "", "", 7}},              /* 01 */ /* Group 7 */
    {0, 0x2, {"lar", "Gv", "Ew", 0}},           /* 02 */
    {0, 0x3, {"lsl", "Gv", "Ew", 0}},           /* 03 */
    {0, 0x5, {"loadall/syscall", "", "", 0}},   /* 05 */
    {0, 0x6, {"clts", "", "", 0}},              /* 06 */
    {0, 0x7, {"loadall/sysret", "", "", 0}},    /* 07 */
    {0, 0x8, {"invd", "", "", 0}},              /* 08 */
    {0, 0x9, {"wbinvd", "", "", 0}},            /* 09 */
    {0, 0xB, {"ud1", "", "", 0}},               /* 0B */

    {0, 0x10, {"umov", "Eb", "Gb", 0}},         /* 10 */
    {0, 0x11, {"umov", "Ev", "Gv", 0}},         /* 11 */
    {0, 0x12, {"umov", "Gb", "Eb", 0}},         /* 12 */
    {0, 0x13, {"umov", "Gv", "Ev", 0}},         /* 13 */

    {0, 0x20, {"mov", "Rd", "Cd", 0}},          /* 20 */
    {0, 0x21, {"mov", "Rd", "Dd", 0}},          /* 21 */
    {0, 0x22, {"mov", "Cd", "Rd", 0}},          /* 22 */
    {0, 0x23, {"mov", "Dd", "Rd", 0}},          /* 23 */

    {0, 0x30, {"wrmsr", "", "", 0}},            /* 30 */
    {0, 0x31, {"rdtsc", "", "", 0}},            /* 31 */
    {0, 0x32, {"rdmsr", "", "", 0}},            /* 32 */
    {0, 0x33, {"rdpmc", "", "", 0}},            /* 33 */
    {0, 0x34, {"sysenter", "", "", 0}},         /* 34 */
    {0, 0x35, {"sysexit", "", "", 0}},          /* 35 */
    {0, 0x37, {"getsec", "", "", 0}},           /* 37 */

    {0, 0x40, {"cmovo", "Gv", "Ev", 0}},        /* 40 */
    {0, 0x41, {"cmovno", "Gv", "Ev", 0}},       /* 41 */
    {0, 0x42, {"cmovb", "Gv", "Ev", 0}},        /* 42 */
    {0, 0x43, {"cmovnb", "Gv", "Ev", 0}},       /* 43 */
    {0, 0x44, {"cmovz", "Gv", "Ev", 0}},        /* 44 */
    {0, 0x45, {"cmovnz", "Gv", "Ev", 0}},       /* 45 */
    {0, 0x46, {"cmovbe", "Gv", "Ev", 0}},       /* 46 */
    {0, 0x47, {"cmovnbe", "Gv", "Ev", 0}},      /* 47 */
    {0, 0x48, {"cmovs", "Gv", "Ev", 0}},        /* 48 */
    {0, 0x49, {"cmovns", "Gv", "Ev", 0}},       /* 49 */
    {0, 0x4A, {"cmovp", "Gv", "Ev", 0}},        /* 4A */
    {0, 0x4B, {"cmovnp", "Gv", "Ev", 0}},       /* 4B */
    {0, 0x4C, {"cmovl", "Gv", "Ev", 0}},        /* 4C */
    {0, 0x4D, {"cmovnl", "Gv", "Ev", 0}},       /* 4D */
    {0, 0x4E, {"cmovle", "Gv", "Ev", 0}},       /* 4E */
    {0, 0x4F, {"cmovnle", "Gv", "Ev", 0}},      /* 4F */

    {0, 0x80, {"jo ", "Jz", "", 0}},            /* 80 */
    {0, 0x81, {"jno", "Jz", "", 0}},            /* 81 */
    {0, 0x82, {"jb ", "Jz", "", 0}},            /* 82 */
    {0, 0x83, {"jnb", "Jz", "", 0}},            /* 83 */
    {0, 0x84, {"jz ", "Jz", "", 0}},            /* 84 */
    {0, 0x85, {"jnz", "Jz", "", 0}},            /* 85 */
    {0, 0x86, {"jbe", "Jz", "", 0}},            /* 86 */
    {0, 0x87, {"jnbe", "Jz", "", 0}},           /* 87 */
    {0, 0x88, {"js ", "Jz", "", 0}},            /* 88 */
    {0, 0x89, {"jns", "Jz", "", 0}},            /* 89 */
    {0, 0x8A, {"jp", "Jz", "", 0}},             /* 8A */
    {0, 0x8B, {"jnp", "Jz", "", 0}},            /* 8B */
    {0, 0x8C, {"jl ", "Jz", "", 0}},            /* 8C */
    {0, 0x8D, {"jnl", "Jz", "", 0}},            /* 8D */
    {0, 0x8E, {"jle", "Jz", "", 0}},            /* 8E */
    {0, 0x8F, {"jnle", "Jz", "", 0}},           /* 8F */

    {0, 0x90, {"seto", "Eb", "", 0}},           /* 90 */
    {0, 0x91, {"setno", "Eb", "", 0}},          /* 91 */
    {0, 0x92, {"setb", "Eb", "", 0}},           /* 92 */
    {0, 0x93, {"setnb", "Eb", "", 0}},          /* 93 */
    {0, 0x94, {"setz", "Eb", "", 0}},           /* 94 */
    {0, 0x95, {"setnz", "Eb", "", 0}},          /* 95 */
    {0, 0x96, {"setbe", "Eb", "", 0}},          /* 96 */
    {0, 0x97, {"setnbe", "Eb", "", 0}},         /* 97 */
    {0, 0x98, {"sets", "Eb", "", 0}},           /* 98 */
    {0, 0x99, {"setns", "Eb", "", 0}},          /* 99 */
    {0, 0x9A, {"setp", "Eb", "", 0}},           /* 9A */
    {0, 0x9B, {"setnp", "Eb", "", 0}},          /* 9B */
    {0, 0x9C, {"setl", "Eb", "", 0}},           /* 9C */
    {0, 0x9D, {"setnl", "Eb", "", 0}},          /* 9D */
    {0, 0x9E, {"setle", "Eb", "", 0}},          /* 9E */
    {0, 0x9F, {"setnle", "Eb", "", 0}},         /* 9F */

    {0, 0xA0, {"push", "!wfs", "", 0}},         /* A0 */
    {0, 0xA1, {"pop", "!wfs", "", 0}},          /* A1 */
    {0, 0xA2, {"cpuid", "", "", 0}},            /* A2 */
    {0, 0xA3, {"bt ", "Ev", "Gv", 0}},          /* A3 */
    {0, 0xA4, {"shld", "Ev", "Gv", 0}},         /* A4 */ /* also has Ib */
    {0, 0xA5, {"shld", "Ev", "Gv", 0}},         /* A5 */ /* also has !bcl */
    {0, 0xA6, {"cmpxchg", "", "", 0}},          /* A6 */
    {0, 0xA7, {"cmpxchg", "", "", 0}},          /* A7 */
    {0, 0xA8, {"push", "!wgs", "", 0}},         /* A8 */
    {0, 0xA9, {"pop", "!gs", "", 0}},           /* A9 */
    {0, 0xAA, {"rsm", "", "", 0}},              /* AA */
    {0, 0xAB, {"bts", "Ev", "Gv", 0}},          /* AB */
    {0, 0xAC, {"shrd", "Ev", "Gv", 0}},         /* AC */ /* Also has Ib */
    {0, 0xAD, {"shrd", "Ev", "Gv", 0}},         /* AD */ /* Also has !bcl */
    {0, 0xAE, {"GRP15", "", "", 15}},           /* AE */ /* Group 15 */
    {0, 0xAF, {"imul", "Gv", "Ev", 0}},         /* AF */

    {0, 0xB0, {"cmpxchg", "Eb", "Gb", 0}},      /* B0 */
    {0, 0xB1, {"cmpxchg", "Ev", "Gv", 0}},      /* B1 */
    {0, 0xB2, {"lss", "Gz", "Mp", 0}},          /* B2 */
    {0, 0xB3, {"btr", "Ev", "Gv", 0}},          /* B3 */
    {0, 0xB4, {"lfs", "Gz", "Mp", 0}},          /* B4 */
    {0, 0xB5, {"lgs", "Gz", "Mp", 0}},          /* B5 */
    {0, 0xB6, {"movzx", "Gv", "Eb", 0}},        /* B6 */
    {0, 0xB7, {"movxz", "Gv", "Ew", 0}},        /* B7 */
    {0, 0xB8, {"jmpe", "Jz", "", 0}},           /* B8 */
    {0, 0xB9, {"ud2", "", "", 11}},             /* B9 */ /* Group 11 */
    {0, 0xBA, {"GRP8", "Ev", "Ib", 8}},         /* BA */ /* Group 8 */
    {0, 0xBB, {"btc", "Ev", "Gv", 0}},          /* BB */
    {0, 0xBC, {"bsf", "Gv", "Ev", 0}},          /* BC */
    {0, 0xBD, {"bsr", "Gv", "Ev", 0}},          /* BD */
    {0, 0xBE, {"movsx", "Gv", "Eb", 0}},        /* BE */
    {0, 0xBF, {"movsx", "Gv", "Ew", 0}},        /* BF */

    {0xF3, 0xB8, {"popcnt", "Gv", "Ev", 0}},    /* B8 */
    {0xF3, 0xBD, {"lzcnt", "Gv", "Ev", 0}},     /* BD */

    {0, 0xC0, {"xadd", "Eb", "Gb", 0}},         /* C0 */
    {0, 0xC1, {"xadd", "Ev", "Gv", 0}},         /* C1 */
    {0, 0xC7, {"GRP9", "", "", 9}},             /* C7 */  /* Group 9 */
    {0, 0xC8, {"bswap", "!leax", "", 0}},       /* C8 */
    {0, 0xC9, {"bswap", "!lecx", "", 0}},       /* C9 */
    {0, 0xCA, {"bswap", "!ledx", "", 0}},       /* CA */
    {0, 0xCB, {"bswap", "!lebx", "", 0}},       /* CB */
    {0, 0xCC, {"bswap", "!lesp", "", 0}},       /* CC */
    {0, 0xCD, {"bswap", "!lebp", "", 0}},       /* CD */
    {0, 0xCE, {"bswap", "!lesi", "", 0}},       /* CE */
    {0, 0xCF, {"bswap", "!ledi", "", 0}},       /* CF */

    {0, 0xFF, {"ud", "", "", 0}},               /* FF */
    {0x66, 0xFF, {"ud", "", "", 0}},            /* FF */

    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
    {0, 0x0, {"", "", "", 0}},                  /* 00 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group1Instructions[8] = {
    {"add", "", "", 0},                         /* 00 */
    {"or ", "", "", 0},                         /* 01 */
    {"adc", "", "", 0},                         /* 02 */
    {"sbb", "", "", 0},                         /* 03 */
    {"and", "", "", 0},                         /* 04 */
    {"sub", "", "", 0},                         /* 05 */
    {"xor", "", "", 0},                         /* 06 */
    {"cmp", "", "", 0},                         /* 07 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group2Instructions[8] = {
    {"rol", "", "", 0},                         /* 00 */
    {"ror", "", "", 0},                         /* 01 */
    {"rcl", "", "", 0},                         /* 02 */
    {"rcr", "", "", 0},                         /* 03 */
    {"shl", "", "", 0},                         /* 04 */
    {"shr", "", "", 0},                         /* 05 */
    {"sal", "", "", 0},                         /* 06 */
    {"sar", "", "", 0},                         /* 07 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group3Instructions[8] = {
    {"test", "Ev", "Ib", 0},                    /* 00 */
    {"test", "Ev", "Ib", 0},                    /* 01 */
    {"not", "", "", 0},                         /* 02 */
    {"neg", "", "", 0},                         /* 03 */
    {"mul", "", "!rax", 0},                     /* 04 */
    {"mul", "", "!rax", 0},                     /* 05 */
    {"div", "", "!rax", 0},                     /* 06 */
    {"div", "", "!rax", 0},                     /* 07 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group3AInstructions[8] = {
    {"test", "Ev", "Iz", 0},                    /* 00 */
    {"test", "Ev", "Iz", 0},                    /* 01 */
    {"not", "", "", 0},                         /* 02 */
    {"neg", "", "", 0},                         /* 03 */
    {"mul", "", "!rax", 0},                     /* 04 */
    {"mul", "", "!rax", 0},                     /* 05 */
    {"div", "", "!rax", 0},                     /* 06 */
    {"div", "", "!rax", 0},                     /* 07 */
};

X86_INSTRUCTION_DEFINITION
                    DbgX86Group4Instructions[X86_GROUP_4_INSTRUCTION_COUNT] = {

    {"inc", "Eb", "", 0},                       /* 00 */
    {"dec", "Eb", "", 0},                       /* 01 */
};

X86_INSTRUCTION_DEFINITION
                    DbgX86Group5Instructions[X86_GROUP_5_INSTRUCTION_COUNT] = {

    {"inc", "Ev", "", 0},                       /* 00 */
    {"dec", "Ev", "", 0},                       /* 01 */
    {"call", "Ev", "", 0},                      /* 02 */
    {"call", "Mp", "", 0},                      /* 03 */
    {"jmp", "Ev", "", 0},                       /* 04 */
    {"jmp", "Mp", "", 0},                       /* 05 */
    {"push", "Ev", "", 0},                      /* 06 */
};

X86_INSTRUCTION_DEFINITION
                    DbgX86Group6Instructions[X86_GROUP_6_INSTRUCTION_COUNT] = {

    {"sldt", "Ev", "", 0},                      /* 00 */
    {"str", "Ev", "", 0},                       /* 01 */
    {"lldt", "Ev", "", 0},                      /* 02 */
    {"ltr", "Ev", "", 0},                       /* 03 */
    {"verr", "Ev", "", 0},                      /* 04 */
    {"verw", "Ev", 0},                          /* 05 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group7Instructions[8] = {
    {"sgdt", "Ms", "", 0},                      /* 00 */
    {"sidt", "Ms", "", 0},                      /* 01 */
    {"lgdt", "Ms", "", 0},                      /* 02 */
    {"lidt", "Ms", "", 0},                      /* 03 */
    {"smsw", "Mw", "", 0},                      /* 04 */
    {"", "", "", X86_INVALID_GROUP},            /* 05 */
    {"lmsw", "Mw", "", 0},                      /* 06 */
    {"invlpg", "Ml", "", 0},                    /* 07 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group8Instructions[8] = {
    {"", "", "", X86_INVALID_GROUP},            /* 00 */
    {"", "", "", X86_INVALID_GROUP},            /* 01 */
    {"", "", "", X86_INVALID_GROUP},            /* 02 */
    {"", "", "", X86_INVALID_GROUP},            /* 03 */
    {"bt ", "", "", 0},                         /* 04 */
    {"bts", "", 0},                             /* 05 */
    {"btr", "", "", 0},                         /* 06 */
    {"btc", "", "", 0},                         /* 07 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group9Instructions[8] = {
    {"", "", "", X86_INVALID_GROUP},            /* 00 */
    {"cmpxchg", "Mq", "", 0},                   /* 01 */
};

X86_INSTRUCTION_DEFINITION DbgX86Group15Instructions[8] = {
    {"fxsave", "M", "", 0},                     /* 00 */
    {"fxrstor", "M", "", 0},                    /* 01 */
    {"vldmxcsr", "Md", "", 0},                  /* 02 */
    {"vstmxcsr", "Md", "", 0},                  /* 03 */
    {"xsave", "M", "", 0},                      /* 04 */
    {"xrstor", "M", "", 0},                     /* 05 */
    {"xsaveopt", "M", "", 0},                   /* 06 */
    {"clflush", "M", "", 0},                    /* 07 */
};

//
// Define the various x87 floating point mnemonics. The first index is the
// first opcode (offset from 0xD8), and the second index is the reg2 portion
// of the ModR/M byte. These are only valid if the mod portion of ModR/M
// does not specify a register. If it specifies a register, then there are
// different arrays used for decoding.
//

PSTR DbgX87Instructions[8][8] = {
    {
        "fadd",
        "fmul",
        "fcom",
        "fcomp",
        "fsub",
        "fsubr",
        "fdiv",
        "fdivr"
    },

    {
        "fld",
        NULL,
        "fst",
        "fstp",
        "fldenv",
        "fldcw",
        "fstenv",
        "fstcw"
    },

    {
        "fiadd",
        "fimul",
        "ficom",
        "ficomp",
        "fisub",
        "fisubr",
        "fidiv",
        "fidivr"
    },

    {
        "fild",
        "fisttp",
        "fist",
        "fistp",
        NULL,
        "fld",
        NULL,
        "fstp"
    },

    {
        "fadd",
        "fmul",
        "fcom",
        "fcomp",
        "fsub",
        "fsubr",
        "fdiv",
        "fdivr"
    },

    {
        "fld",
        "fisttp",
        "fst",
        "fstp",
        "frstor",
        NULL,
        "fsave",
        "fstsw"
    },

    {
        "fiadd",
        "fimul",
        "ficom",
        "ficomp",
        "fisub",
        "fisubr",
        "fidiv",
        "fidivr"
    },

    {
        "fild",
        "fisttp",
        "fist",
        "fistp",
        "fbld",
        "fild",
        "fbstp",
        "fistp"
    }
};

PSTR DbgX87D9E0Instructions[32] = {
    "fchs",
    "fabs",
    NULL,
    NULL,
    "ftst",
    "fxam",
    "ftstp",
    NULL,
    "fld1",
    "fldl2t",
    "fldl2e",
    "fldpi",
    "fldlg2",
    "fldln2",
    "fldz",
    NULL,
    "f2xm1",
    "fyl2x",
    "fptan",
    "fpatan",
    "fxtract",
    "fprem1",
    "fdecstp",
    "fincstp",
    "fprem",
    "fyl2xp1",
    "fsqrt",
    "fsincos",
    "frndint",
    "fscale",
    "fsin",
    "fcos",
};

PSTR DbgX87DAC0Instructions[8] = {
    "fcmovb",
    "fcmove",
    "fcmovbe",
    "fcmovu",
    NULL,
    NULL,
    NULL,
    NULL
};

PSTR DbgX87DBC0Instructions[8] = {
    "fcmovnb",
    "fcmovne",
    "fcmovnbe",
    "fcmovnu",
    NULL,
    "fucomi",
    "fcomi",
    NULL
};

PSTR DbgX87DBE0Instructions[8] = {
    "feni",
    "fdisi",
    "fclex",
    "finit",
    "fsetpm",
    "frstpm",
    NULL,
    NULL
};

PSTR DbgX87DCC0Instructions[8] = {
    "fadd",
    "fmul",
    "fcom",
    "fcomp",
    "fsubr",
    "fsub",
    "fdivr",
    "fdiv",
};

PSTR DbgX87DDC0Instructions[8] = {
    "ffree",
    "fxch",
    "fst",
    "fstp",
    "fucom",
    "fucomp",
    NULL,
    NULL,
};

PSTR DbgX87DEC0Instructions[8] = {
    "faddp",
    "fmulp",
    "fcomp",
    NULL,
    "fsubrp",
    "fsubp",
    "fdivrp",
    "fdivp",
};

PSTR DbgX87DFC0Instructions[8] = {
    "freep",
    "fxch",
    "fstp",
    "fstp",
    NULL,
    "fucomip",
    "fcomip",
    NULL,
};

PSTR DbgX87DFE0Instructions[X87_DF_E0_COUNT] = {
    "fstsw",
    "fstdw",
    "fstsg",
};

//
// Define the register name constants.
//

PSTR DbgX86DebugRegisterNames[X86_DEBUG_REGISTER_COUNT] = {
    "dr0",
    "dr1",
    "dr2",
    "dr3",
    "dr4",
    "dr5",
    "dr6",
    "dr7"
};

PSTR DbgX86SegmentRegisterNames[X86_SEGMENT_REGISTER_COUNT] = {
    "es",
    "cs",
    "ss",
    "ds",
    "fs",
    "gs"
};

PSTR DbgX86RegisterNames8Bit[X86_REGISTER_NAME_COUNT] = {
    "al",
    "cl",
    "dl",
    "bl",
    "ah",
    "ch",
    "dh",
    "bh",
};

PSTR DbgX86RegisterNames16Bit[X86_REGISTER_NAME_COUNT] = {
    "ax",
    "cx",
    "dx",
    "bx",
    "sp",
    "bp",
    "si",
    "di"
};

PSTR DbgX86RegisterNames32Bit[X86_REGISTER_NAME_COUNT] = {
    "eax",
    "ecx",
    "edx",
    "ebx",
    "esp",
    "ebp",
    "esi",
    "edi"
};

PSTR DbgX87RegisterNames[X86_REGISTER_NAME_COUNT] = {
    "st(0)",
    "st(1)",
    "st(2)",
    "st(3)",
    "st(4)",
    "st(5)",
    "st(6)",
    "st(7)",
};

//
// ----------------------------------------------- Internal Function Prototypes
//

BOOL
DbgpX86PrintOperand (
    PX86_INSTRUCTION Instruction,
    PSTR OperandFormat,
    PSTR Operand,
    ULONG BufferLength,
    PULONGLONG Address,
    PADDRESS_RELATION AddressRelation
    );

PSTR
DbgpX86PrintMnemonic (
    PX86_INSTRUCTION Instruction
    );

BOOL
DbgpX86GetInstructionComponents (
    PBYTE InstructionStream,
    PX86_INSTRUCTION Instruction
    );

BOOL
DbgpX86GetInstructionParameters (
    PBYTE InstructionStream,
    PX86_INSTRUCTION Instruction,
    PBOOL ModRmExists,
    PBOOL SibExists,
    PULONG DisplacementSize,
    PULONG ImmediateSize
    );

PSTR
DbgpX86GetControlRegister (
    BYTE ModRm
    );

PSTR
DbgpX86GetDebugRegister (
    BYTE ModRm
    );

PSTR
DbgpX86GetSegmentRegister (
    BYTE ModRm
    );

PSTR
DbgpX86GetGenericRegister (
    X86_REGISTER RegisterNumber,
    CHAR Type
    );

VOID
DbgpX86GetDisplacement (
    PX86_INSTRUCTION Instruction,
    PSTR Buffer,
    PLONGLONG DisplacementValue
    );

PX86_INSTRUCTION_DEFINITION
DbgpX86GetTwoByteInstruction (
    PX86_INSTRUCTION Instruction
    );

BOOL
DbgpX86DecodeFloatingPointInstruction (
    PX86_INSTRUCTION Instruction
    );

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgpX86Disassemble (
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly
    )

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

{

    ULONGLONG Address;
    ADDRESS_RELATION AddressRelation;
    X86_INSTRUCTION Instruction;
    PSTR Mnemonic;
    BOOL Result;
    PSTR ThirdOperandFormat;

    if ((Disassembly == NULL) || (Buffer == NULL)) {
        return FALSE;
    }

    memset(Buffer, 0, BufferLength);
    memset(Disassembly, 0, sizeof(DISASSEMBLED_INSTRUCTION));

    //
    // Dissect the instruction into more managable components.
    //

    Result = DbgpX86GetInstructionComponents(InstructionStream, &Instruction);
    if (Result == FALSE) {
        goto DisassembleEnd;
    }

    Disassembly->BinaryLength = Instruction.Length;

    //
    // Print the mnemonic.
    //

    Mnemonic = DbgpX86PrintMnemonic(&Instruction);
    if ((Mnemonic == NULL) || (strlen(Mnemonic) >= BufferLength)) {
        Result = FALSE;
        goto DisassembleEnd;
    }

    //
    // Copy the mnemonic into the buffer, and advance the buffer to the next
    // free spot.
    //

    Disassembly->Mnemonic = Buffer;
    strcpy(Disassembly->Mnemonic, Mnemonic);
    Buffer += strlen(Mnemonic) + 1;
    BufferLength -= (strlen(Mnemonic) + 1);

    //
    // Get the destination operand.
    //

    Result = DbgpX86PrintOperand(&Instruction,
                                 Instruction.Definition.Target,
                                 DbgX86DisassemblyBuffer,
                                 X86_WORKING_BUFFER_SIZE,
                                 &Address,
                                 &AddressRelation);

    if ((Result == FALSE) ||
        (strlen(DbgX86DisassemblyBuffer) >= BufferLength)) {

        Result = FALSE;
        goto DisassembleEnd;
    }

    //
    // If an address came out of that, plug it into the result.
    //

    if (AddressRelation != RelationInvalid) {
        Disassembly->OperandAddress = Address;
        Disassembly->OperandAddressRelation = AddressRelation;
        Disassembly->AddressIsDestination = TRUE;
    }

    //
    // Copy the operand into the buffer, and advance the buffer.
    //

    Disassembly->DestinationOperand = Buffer;
    strcpy(Disassembly->DestinationOperand, DbgX86DisassemblyBuffer);
    Buffer += strlen(DbgX86DisassemblyBuffer) + 1;
    BufferLength -= (strlen(DbgX86DisassemblyBuffer) + 1);

    //
    // Get the source operand.
    //

    Result = DbgpX86PrintOperand(&Instruction,
                                 Instruction.Definition.Source,
                                 DbgX86DisassemblyBuffer,
                                 X86_WORKING_BUFFER_SIZE,
                                 &Address,
                                 &AddressRelation);

    if ((Result == FALSE) ||
        (strlen(DbgX86DisassemblyBuffer) >= BufferLength)) {

        Result = FALSE;
        goto DisassembleEnd;
    }

    //
    // If an address came out of the operand, plug it into the result.
    //

    if (AddressRelation != RelationInvalid) {
         Disassembly->OperandAddress = Address;
         Disassembly->OperandAddressRelation = AddressRelation;
         Disassembly->AddressIsDestination = FALSE;
    }

    //
    // Copy the operand into the buffer, and advance the buffer.
    //

    if (strlen(DbgX86DisassemblyBuffer) > 0) {
        Disassembly->SourceOperand = Buffer;
        strcpy(Disassembly->SourceOperand, DbgX86DisassemblyBuffer);
        Buffer += strlen(DbgX86DisassemblyBuffer) + 1;
        BufferLength -= (strlen(DbgX86DisassemblyBuffer) - 1);
    }

    //
    // Handle the MUL, SHLD, and SHRD instructions, which have 3 operands.
    //

    ThirdOperandFormat = NULL;
    if (Instruction.Opcode == X86_OPCODE1_IMUL1) {
        ThirdOperandFormat = "Iz";

    } else if (Instruction.Opcode == X86_OPCODE1_IMUL2) {
        ThirdOperandFormat = "Ib";

    } else if ((Instruction.Opcode == X86_ESCAPE_OPCODE) &&
               ((Instruction.Opcode2 == X86_OPCODE2_SHLD1) ||
                (Instruction.Opcode2 == X86_OPCODE2_SHRD1))) {

        ThirdOperandFormat = "Ib";

    } else if ((Instruction.Opcode == X86_ESCAPE_OPCODE) &&
               ((Instruction.Opcode2 == X86_OPCODE2_SHLD2) ||
                (Instruction.Opcode2 == X86_OPCODE2_SHRD2))) {

        ThirdOperandFormat = "!bcl";
    }

    if (ThirdOperandFormat != NULL) {
        Result = DbgpX86PrintOperand(&Instruction,
                                     ThirdOperandFormat,
                                     DbgX86DisassemblyBuffer,
                                     X86_WORKING_BUFFER_SIZE,
                                     &Address,
                                     &AddressRelation);

        if ((Result == FALSE) ||
            (strlen(DbgX86DisassemblyBuffer) > BufferLength)) {

            Result = FALSE;
            goto DisassembleEnd;
        }

        Disassembly->ThirdOperand = Buffer;
        strcpy(Disassembly->ThirdOperand, DbgX86DisassemblyBuffer);
    }

DisassembleEnd:
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

BOOL
DbgpX86PrintOperand (
    PX86_INSTRUCTION Instruction,
    PSTR OperandFormat,
    PSTR Operand,
    ULONG BufferLength,
    PULONGLONG Address,
    PADDRESS_RELATION AddressRelation
    )

/*++

Routine Description:

    This routine prints an operand in an IA instruction stream depending on the
    supplied format.

Arguments:

    Instruction - Supplies a pointer to the instruction structure.

    OperandFormat - Supplies the format of the operand in two or more
        characters. These are largely compatible with the Intel Opcode
        Encodings, except for the ! prefix, which indicates an absolute
        register name.

    Operand - Supplies a pointer to the string that will receive the human
        readable operand.

    BufferLength - Supplies the length of the supplied buffer.

    Address - Supplies a pointer that receives the memory address encoded in the
        operand.

    AddressRelation - Supplies a pointer that receives information about whether
        or not the address in the Address parameter is relative to some value.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR Base;
    BYTE BaseValue;
    PSTR Index;
    BYTE IndexValue;
    X86_MOD_VALUE Mod;
    X86_REGISTER Register;
    PSTR RegisterString;
    X86_REGISTER Rm;
    ULONG Scale;
    CHAR Type;
    CHAR Width;

    Base = NULL;
    Index = NULL;
    IndexValue = 0xFF;
    Scale = 0;
    RegisterString = NULL;

    //
    // Start by doing some parameter checking.
    //

    if ((Operand == NULL) || BufferLength == 0) {
        return FALSE;
    }

    strcpy(Operand, "");
    strcpy(DbgX86OperandBuffer, "");
    *Address = 0ULL;
    *AddressRelation = RelationInvalid;
    if (strlen(OperandFormat) < 2) {
        return TRUE;
    }

    Type = OperandFormat[0];
    Width = OperandFormat[1];

    //
    // 'd' means dword, which gets translated to long here for simplicity.
    //

    if (Width == 'd') {
        Width = X86_WIDTH_LONG;
    }

    //
    // If the width is variable, it is probably a dword unless an override is
    // specified.
    //

    if ((Width == 'v') || (Width == 'z')) {
        Width = X86_WIDTH_LONG;
        if ((Instruction->OperandOverride == TRUE) ||
            (Instruction->AddressOverride == TRUE)) {

            Width = X86_WIDTH_WORD;
        }
    }

    switch (Type) {

    //
    // The ! encoding indicates that a register is hardcoded. Unless an override
    // is set, append an e to the beginning of the hardcoded register (to make
    // ax into eax).
    //

    case '!':
        if ((Width == 'r') || (Width == 'e')) {
            if ((Instruction->ImmediateSize == 0) &&
                (Instruction->OperandOverride == FALSE)) {

                strcat(Operand, "e");

            } else if (Instruction->ImmediateSize == 4) {
                strcat(Operand, "e");
            }
        }

        strcat(Operand, OperandFormat + 2);
        break;

    //
    // A - Direct address, no mod R/M byte; address of operand is encoded in
    // instruction. No base, index, or scaling can be applied.
    //

    case 'A':
        sprintf(DbgX86OperandBuffer, "[%x]", Instruction->Immediate);
        strcat(Operand, DbgX86OperandBuffer);
        *Address = Instruction->Immediate;
        *AddressRelation = RelationAbsolute;
        break;

    //
    // C - Reg field of mod R/M byte selects a control register.
    //

    case 'C':
        sprintf(DbgX86OperandBuffer,
                "%s",
                DbgpX86GetControlRegister(Instruction->ModRm));

        strcat(Operand, DbgX86OperandBuffer);
        break;

    //
    // D - Reg field of mod R/M byte selects a debug register.
    //

    case 'D':
        sprintf(DbgX86OperandBuffer, "%s",
                DbgpX86GetDebugRegister(Instruction->ModRm));

        strcat(Operand, DbgX86OperandBuffer);
        break;

    //
    // E - Mod R/M bytes follows opcode and specifies operand. Operand is either
    // a general register or a memory address. If it is a memory address, the
    // address is computed from a segment register and any of the following
    // values: a base register, an index register, a scaling factor, and a
    // displacement.
    // M - Mod R/M byte may only refer to memory.
    //

    case 'E':
    case 'M':
        Mod = (Instruction->ModRm & X86_MOD_MASK) >> X86_MOD_SHIFT;
        Rm = (Instruction->ModRm & X86_RM_MASK) >> X86_RM_SHIFT;
        if (Mod == X86ModValueRegister) {
            if (Type == 'M') {
                return FALSE;
            }

            RegisterString = DbgpX86GetGenericRegister(Rm, Width);

        } else {

            //
            // An R/M value of 4 actually indicates an SIB byte is present, not
            // ESP.
            //

            if (Rm == X86RegisterValueEsp) {
                Rm = X86RegisterValueScaleIndexBase;
                BaseValue = (Instruction->Sib & X86_BASE_MASK) >>
                                                                X86_BASE_SHIFT;

                IndexValue = (Instruction->Sib & X86_INDEX_MASK) >>
                                                               X86_INDEX_SHIFT;

                Base = DbgpX86GetGenericRegister(BaseValue, X86_WIDTH_LONG);
                Index = DbgpX86GetGenericRegister(IndexValue, X86_WIDTH_LONG);

                //
                // A base value of 5 (ebp) indicates that the base field is not
                // used, and a displacement is present. The Mod field then
                // specifies the size of the displacement.
                //

                if (BaseValue == X86RegisterValueEbp) {
                    Base = "";
                    sprintf(DbgX86OperandBuffer,
                            "0x%x",
                            Instruction->Displacement);

                    strcat(Operand, DbgX86OperandBuffer);
                }

                //
                // Raise the scale to 2^(Scale).
                //

                Scale = (Instruction->Sib & X86_SCALE_MASK) >> X86_SCALE_SHIFT;
                if (Scale == 0) {
                    Scale = 1;

                } else if (Scale == 1) {
                    Scale = 2;

                } else if (Scale == 2) {
                    Scale = 4;

                } else if (Scale == 3) {
                    Scale = 8;
                }

            } else if ((Mod == X86ModValueNoDisplacement) &&
                       (Rm == X86RegisterValueEbp)) {

                Rm = X86RegisterValueDisplacement32;

            } else {
                RegisterString = DbgpX86GetGenericRegister(Rm, X86_WIDTH_LONG);
            }
        }

        //
        // The operand is simply a register.
        //

        if (Mod == X86ModValueRegister) {
            strcat(Operand, RegisterString);

        //
        // The operand is an address with a scale/index/base.
        //

        } else if (Rm == X86RegisterValueScaleIndexBase) {
            sprintf(DbgX86OperandBuffer, "[%s", Base);
            strcat(Operand, DbgX86OperandBuffer);

            //
            // An index of 4 indicates that the index and scale fields are not
            // used.
            //

            if (IndexValue != 4) {
                if (strlen(Base) != 0) {
                    strcat(Operand, "+");
                }

                sprintf(DbgX86OperandBuffer, "%s*%d", Index, Scale);
                strcat(Operand, DbgX86OperandBuffer);
            }

            DbgpX86GetDisplacement(Instruction, DbgX86OperandBuffer, NULL);
            strcat(Operand, DbgX86OperandBuffer);
            strcat(Operand, "]");

        //
        // The operand is a 32-bit address.
        //

        } else if (Rm == X86RegisterValueDisplacement32) {
            sprintf(DbgX86OperandBuffer, "[0x%x]", Instruction->Displacement);
            strcat(Operand, DbgX86OperandBuffer);
            *Address = Instruction->Displacement;
            *AddressRelation = RelationAbsolute;

        //
        // The operand is an address in a register, possibly with some
        // additional displacement.
        //

        } else {
            sprintf(DbgX86OperandBuffer, "[%s", RegisterString);
            strcat(Operand, DbgX86OperandBuffer);
            DbgpX86GetDisplacement(Instruction, DbgX86OperandBuffer, NULL);
            strcat(Operand, DbgX86OperandBuffer);
            strcat(Operand, "]");
        }

    break;

    //
    // F - EFLAGS register.
    //

    case 'F':
        strcat(Operand, "eflags");
        break;

    //
    // G - Reg field of Mod R/M byte selects a general register.
    //

    case 'G':
        Register = (Instruction->ModRm & X86_REG_MASK) >> X86_REG_SHIFT;
        strcat(Operand, DbgpX86GetGenericRegister(Register, Width));
        break;

    //
    // I - Immediate data: value of operand is encoded in Immediate field.
    // O - Direct offset: no ModR/M byte. Offset of operand is encoded in
    // instruction. No Base/Index/Scale can be applied.
    //

    case 'I':
    case 'O':
        sprintf(DbgX86OperandBuffer, "0x%x", Instruction->Immediate);
        strcat(Operand, DbgX86OperandBuffer);
        break;

    //
    // J - Instruction contains a relative offset to be added to the instruction
    // pointer.
    //

    case 'J':
        DbgpX86GetDisplacement(Instruction,
                               DbgX86OperandBuffer,
                               (PLONGLONG)Address);

        strcat(Operand, DbgX86OperandBuffer);
        *AddressRelation = RelationIp;
        break;

    //
    // R - R/M field of modR/M byte selects a general register. Mod field should
    // be set to 11.
    //

    case 'R':
        Mod = (Instruction->ModRm & X86_MOD_MASK) >> X86_MOD_SHIFT;
        Rm = (Instruction->ModRm & X86_RM_MASK) >> X86_RM_SHIFT;
        if (Mod != X86ModValueRegister) {
            return FALSE;
        }

        strcat(Operand, DbgpX86GetGenericRegister(Rm, Width));
        break;

    //
    // S - Reg field of ModR/M byte selects a segment register.
    //

    case 'S':
        strcat(Operand, DbgpX86GetSegmentRegister(Instruction->ModRm));
        break;

    //
    // X - Memory addressed by DS:SI register pair (eg. MOVS CMPS, OUTS, LODS).
    //

    case 'X':
        strcat(Operand, "DS:[esi]");
        break;

    //
    // Y - Memory addressed by ES:DI register pair (eg. MOVS INS, STOS, SCAS).
    //

    case 'Y':
        strcat(Operand, "ES:[edi]");
        break;

    default:
        return FALSE;
    }

    return TRUE;
}

PSTR
DbgpX86PrintMnemonic (
    PX86_INSTRUCTION Instruction
    )

/*++

Routine Description:

    This routine prints an instruction mnemonic.

Arguments:

    Instruction - Supplies a pointer to the instruction structure.

Return Value:

    A pointer to the mnemonic on success.

    NULL on failure.

--*/

{

    BYTE RegByte;

    if (Instruction == NULL) {
        return NULL;
    }

    if (Instruction->Definition.Group == 0) {
        return Instruction->Definition.Mnemonic;
    }

    RegByte = (Instruction->ModRm & X86_REG_MASK) >> X86_REG_SHIFT;
    switch (Instruction->Definition.Group) {
    case 1:
        return DbgX86Group1Instructions[RegByte].Mnemonic;

    case 2:
        return DbgX86Group2Instructions[RegByte].Mnemonic;

    case 3:
        return DbgX86Group3Instructions[RegByte].Mnemonic;

    case 0x3A:
        return DbgX86Group3AInstructions[RegByte].Mnemonic;

    case 4:
        if (RegByte >= X86_GROUP_4_INSTRUCTION_COUNT) {
            return NULL;
        }

        return DbgX86Group4Instructions[RegByte].Mnemonic;

    case 5:
        if (RegByte >= X86_GROUP_5_INSTRUCTION_COUNT) {
            return "(bad)";
        }

        return DbgX86Group5Instructions[RegByte].Mnemonic;

    case 10:
    case 12:
        if (RegByte != 0) {
            return "(bad)";
        }

        return Instruction->Definition.Mnemonic;

    case 15:
        return DbgX86Group15Instructions[RegByte].Mnemonic;
    }

    return NULL;
}

BOOL
DbgpX86GetInstructionComponents (
    PBYTE InstructionStream,
    PX86_INSTRUCTION Instruction
    )

/*++

Routine Description:

    This routine reads an instruction stream and decomposes it into its
    respective components.

Arguments:

    InstructionStream - Supplies a pointer to the raw binary instruction stream.

    Instruction - Supplies a pointer to the structure that will accept the
        instruction decomposition.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    ULONG Base;
    PBYTE Beginning;
    PBYTE CurrentPrefix;
    ULONG DisplacementSize;
    ULONG Group;
    ULONG ImmediateSize;
    ULONG Mod;
    BOOL ModRmExists;
    BYTE RegByte;
    BOOL Result;
    BOOL SibExists;
    PX86_INSTRUCTION_DEFINITION TopLevelDefinition;
    PX86_INSTRUCTION_DEFINITION TwoByteInstruction;

    if (Instruction == NULL) {
        return FALSE;
    }

    CurrentPrefix = &(Instruction->Prefix[0]);
    Beginning = InstructionStream;
    Result = TRUE;
    memset(Instruction, 0, sizeof(X86_INSTRUCTION));

    //
    // Begin by handling any prefixes. The prefixes are: F0 (LOCK), F2 (REP),
    // F3 (REP), 2E (CS), 36 (SS), 3E (DS), 26 (ES), 64 (FS), 65 (GS),
    // 66 (Operand-size override), 67 (Address-size override)).
    //

    while ((*InstructionStream == X86_PREFIX_LOCK) ||
           (*InstructionStream == X86_PREFIX_REP1) ||
           (*InstructionStream == X86_PREFIX_REP2) ||
           ((*InstructionStream & X86_PREFIX_FS_GS_OVERRIDE_MASK) ==
            X86_PREFIX_FS_GS_OVERRIDE_VALUE) ||
           (*InstructionStream == X86_PREFIX_CS) ||
           (*InstructionStream == X86_PREFIX_DS) ||
           (*InstructionStream == X86_PREFIX_ES) ||
           (*InstructionStream == X86_PREFIX_SS)) {

        if (*InstructionStream == X86_OPERAND_OVERRIDE) {
            Instruction->OperandOverride = TRUE;

        } else if (*InstructionStream == X86_ADDRESS_OVERRIDE) {
            Instruction->AddressOverride = TRUE;
        }

        *CurrentPrefix = *InstructionStream;
        CurrentPrefix += 1;
        InstructionStream += 1;
        Instruction->Length += 1;

        //
        // No more than 4 prefixes are allowed in one instruction.
        //

        if (Instruction->Length == X86_MAX_PREFIXES) {
            break;
        }
    }

    Instruction->Opcode = *InstructionStream;
    Instruction->Length += 1;
    InstructionStream += 1;

    //
    // Check for a two byte opcode.
    //

    if (Instruction->Opcode == X86_ESCAPE_OPCODE) {
        Instruction->Opcode2 = *InstructionStream;
        Instruction->Length += 1;
        InstructionStream += 1;
        TwoByteInstruction = DbgpX86GetTwoByteInstruction(Instruction);
        if (TwoByteInstruction == NULL) {
            Result = FALSE;
            goto GetInstructionComponentsEnd;
        }

        TopLevelDefinition = TwoByteInstruction;

    } else {
        TopLevelDefinition = &(DbgX86Instructions[Instruction->Opcode]);
    }

    //
    // Modify the instruction definition for groups. If the opcode is in a
    // group, then it must have a modR/M byte, so cheat a little and get it.
    //

    Instruction->Definition = *TopLevelDefinition;
    Group = Instruction->Definition.Group;
    if ((Group != 0) && (Group != X86_INVALID_GROUP)) {
        RegByte = (*InstructionStream & X86_REG_MASK) >> X86_REG_SHIFT;
        switch (Instruction->Definition.Group) {
        case 1:
        case 2:
            break;

        case 3:
            Instruction->Definition.Source =
                                      DbgX86Group3Instructions[RegByte].Source;

            break;

        case 0x3A:
            Instruction->Definition.Source =
                                     DbgX86Group3AInstructions[RegByte].Source;

            break;

        case 4:
        case 5:
            break;

        case 6:
            if (RegByte >= X86_GROUP_6_INSTRUCTION_COUNT) {
                Result = FALSE;
                goto GetInstructionComponentsEnd;
            }

            Instruction->Definition = DbgX86Group6Instructions[RegByte];
            break;

        case 7:
            Instruction->Definition = DbgX86Group7Instructions[RegByte];
            break;

        case 8:
            if (RegByte < X86_GROUP_8_FIRST_INSTRUCTION) {
                Result = FALSE;
                goto GetInstructionComponentsEnd;
            }

            Instruction->Definition = DbgX86Group8Instructions[RegByte];
            break;

        case 9:
            if (RegByte != X86_GROUP_9_ONLY_VALID_INSTRUCTION) {
                Result = FALSE;
                goto GetInstructionComponentsEnd;
            }

            Instruction->Definition = DbgX86Group9Instructions[RegByte];
            break;

        case 10:
        case 12:
        case 0x87:
            break;

        case 15:
            Instruction->Definition = DbgX86Group15Instructions[RegByte];
            break;

        default:

            assert(FALSE);

            break;
        }
    }

    //
    // Get the structure of the instruction.
    //

    Result = DbgpX86GetInstructionParameters(InstructionStream,
                                             Instruction,
                                             &ModRmExists,
                                             &SibExists,
                                             &DisplacementSize,
                                             &ImmediateSize);

    if (Result == FALSE) {
        goto GetInstructionComponentsEnd;
    }

    if (Group != 0) {
        ModRmExists = TRUE;
    }

    //
    // Populate the various pieces of the instruction.
    //

    Instruction->DisplacementSize = DisplacementSize;
    Instruction->ImmediateSize = ImmediateSize;
    if (ModRmExists == TRUE) {
        Instruction->ModRm = *InstructionStream;
        InstructionStream += 1;
    }

    if (SibExists == TRUE) {
        Instruction->Sib = *InstructionStream;
        InstructionStream += 1;

        //
        // Check to see if the SIB byte requires a displacement. EBP is not a
        // valid base, since that can be specified in the Mod bits.
        //

        Base = (Instruction->Sib & X86_BASE_MASK) >> X86_BASE_SHIFT;
        Mod = (Instruction->ModRm & X86_MOD_MASK) >> X86_MOD_SHIFT;
        if (Base == X86RegisterValueEbp) {
            if (Mod == X86ModValueDisplacement8) {
                DisplacementSize = 1;

            } else {
                DisplacementSize = 4;
            }
        }
    }

    //
    // Grab the displacement and immediates from the instruction stream if
    // they're there.
    //

    if (DisplacementSize != 0) {
        memcpy(&(Instruction->Displacement),
               InstructionStream,
               DisplacementSize);

        InstructionStream += DisplacementSize;
    }

    if (ImmediateSize != 0) {
        memcpy(&(Instruction->Immediate), InstructionStream, ImmediateSize);
        InstructionStream += ImmediateSize;
    }

    Instruction->Length = InstructionStream - Beginning;

    //
    // If it's an x87 floating point instruction, decode it now that the
    // ModR/M byte was grabbed.
    //

    if (Group == 0x87) {
        Result = DbgpX86DecodeFloatingPointInstruction(Instruction);
        if (Result == FALSE) {
            goto GetInstructionComponentsEnd;
        }
    }

GetInstructionComponentsEnd:
    return Result;
}

BOOL
DbgpX86GetInstructionParameters (
    PBYTE InstructionStream,
    PX86_INSTRUCTION Instruction,
    PBOOL ModRmExists,
    PBOOL SibExists,
    PULONG DisplacementSize,
    PULONG ImmediateSize
    )

/*++

Routine Description:

    This routine determines the format of the rest of the instruction based on
    the opcode, any prefixes, and possibly the ModRM byte.

Arguments:

    InstructionStream - Supplies a pointer to the binary instruction stream,
        after the prefixes and opcode.

    Instruction - Supplies a pointer to an instruction. The Prefixes, Opcode,
        and Definition must be filled out.

    ModRmExists - Supplies a pointer where a boolean will be returned
        indicating whether or not a ModRM byte is present.

    SibExists - Supplies a pointer where a boolean will be returned indicating
        whether or not a Scale/Index/Base byte is present in the instruction
        stream.

    DisplacementSize - Supplies a pointer where the size of the Displacement
        value will be returned. 0 indicates there is no displacement in the
        instruction.

    ImmediateSize - Supplies a pointer where  the size of the Immediate field
        in the instruction will be returned. 0 indicates there is no Immediate
        field in the instruction.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

{

    X86_MOD_VALUE Mod;
    BYTE ModRm;
    ULONG ParseCount;
    BYTE RmValue;
    CHAR Type;
    CHAR Width;

    *ModRmExists = FALSE;
    *SibExists = FALSE;
    *DisplacementSize = 0;
    *ImmediateSize = 0;
    if (Instruction->Definition.Target[0] == '\0') {
        return TRUE;
    }

    Type = Instruction->Definition.Target[0];
    Width = Instruction->Definition.Target[1];
    ParseCount = 0;
    do {
        switch (Type) {

        //
        // A - Direct address. No Mod/RM, Immediate specifies address. No SIB.
        //

        case 'A':
            *ImmediateSize = 4;
            break;

        //
        // C - Control register in ModR/M.
        // D - Debug register in ModR/M.
        // S - Segment register in Reg field of ModR/M.
        // T - Test register in ModR/M.
        // V - SIMD floating point register in ModR/M.
        //

        case 'C':
        case 'D':
        case 'S':
        case 'T':
        case 'V':
            *ModRmExists = TRUE;
            break;

       //
       // E - Mod R/M bytes follows opcode and specifies operand. Operand is
       // either a general register or a memory address. If it is a memory
       // address, the address is computed from a segment register and any of
       // the following values: a base register, an index register, a scaling
       // factor, and a displacement.
       // M - Mod R/M byte may only refer to memory.
       // R - Mod R/M byte may only refer to a general register.
       //

        case 'E':
        case 'M':
        case 'R':
            *ModRmExists = TRUE;
            ModRm = *InstructionStream;
            Mod = (ModRm & X86_MOD_MASK) >> X86_MOD_SHIFT;
            RmValue = (ModRm & X86_RM_MASK) >> X86_RM_SHIFT;
            if (Mod != X86ModValueRegister) {

                //
                // An R/M value of 4 actually indicates an SIB byte is present,
                // not ESP.
                //

                if (RmValue == X86RegisterValueEsp) {
                    RmValue = X86RegisterValueScaleIndexBase;
                    *SibExists = TRUE;
                }

                //
                // An R/M value of 5 when Mod is 0 means that the address is
                // actually just a 32bit displacment.
                //

                if ((Mod == X86ModValueNoDisplacement) &&
                    (RmValue == X86RegisterValueEbp)) {

                    RmValue = X86RegisterValueDisplacement32;
                    *DisplacementSize = 4;
                }
            }

            //
            // Get any displacements as specified by the MOD bits.
            //

            if (Mod == X86ModValueDisplacement8) {
                *DisplacementSize = 1;

            } else if (Mod == X86ModValueDisplacement32) {
                *DisplacementSize = 4;
            }

            break;

        //
        // F - Flags register. No additional bytes.
        // X - Memory addressed by DS:SI pair.
        // Y - Memory addressed by ES:DI pair.
        // ! - Hardcoded register.
        //

        case 'F':
        case 'X':
        case 'Y':
        case '!':
            break;

        //
        // G - General register specified in Reg field of ModR/M byte.
        //

        case 'G':
            *ModRmExists = TRUE;
            break;

        //
        // I - Immediate data is encoded in subsequent bytes.
        //

        case 'I':
            switch (Width) {
            case X86_WIDTH_BYTE:
                *ImmediateSize = 1;
                break;

            case X86_WIDTH_WORD:
                *ImmediateSize = 2;
                break;

            case X86_WIDTH_LONG:
                *ImmediateSize = 4;
                break;

            case 'v':
            case 'z':
                *ImmediateSize = 4;
                if (Instruction->OperandOverride == TRUE) {
                    *ImmediateSize = 2;
                }

                break;
            }

            break;

        //
        // O - Direct Offset. No ModR/M byte, offset of operand is encoded in
        // instruction. No SIB.
        //

        case 'O':
            *ImmediateSize = 4;
            if (Instruction->AddressOverride != FALSE) {
                *ImmediateSize = 2;
            }

            break;

        //
        // J - Instruction contains relative offset.
        //

        case 'J':
            switch (Width) {
            case X86_WIDTH_BYTE:
                *DisplacementSize = 1;
                break;

            case X86_WIDTH_WORD:
                *DisplacementSize = 2;
                break;

            case X86_WIDTH_LONG:
                *DisplacementSize = 4;
                break;

            case 'v':
            case 'z':
                *DisplacementSize = 4;
                if (Instruction->AddressOverride == TRUE) {
                    *DisplacementSize = 2;
                }

                break;
            }

            break;

        default:
            return FALSE;
        }

        //
        // Now that the target has been processed, loop again to process the
        // source.
        //

        ParseCount += 1;
        if (Instruction->Definition.Source[0] == '\0') {
            break;
        }

        Type = Instruction->Definition.Source[0];
        Width = Instruction->Definition.Source[1];

    } while (ParseCount < 2);

    //
    // Handle the special instructions that actually have three operands.
    //

    if (Instruction->Opcode == X86_OPCODE1_IMUL1) {
        *ImmediateSize = 4;
        if (Instruction->OperandOverride == TRUE) {
            *ImmediateSize = 2;
        }
    }

    if (Instruction->Opcode == X86_OPCODE1_IMUL2) {
        *ImmediateSize = 1;
    }

    if ((Instruction->Opcode == X86_ESCAPE_OPCODE) &&
        ((Instruction->Opcode2 == X86_OPCODE2_SHLD1) ||
         (Instruction->Opcode2 == X86_OPCODE2_SHRD1))) {

        *ImmediateSize = 1;
    }

    return TRUE;
}

PSTR
DbgpX86GetControlRegister (
    BYTE ModRm
    )

/*++

Routine Description:

    This routine reads the REG bits of a ModR/M byte and returns a string
    representing the control register in those bits.

Arguments:

    ModRm - Supplies the ModR/M byte of the instruction. Only bits 5:3 are used.

Return Value:

    The control register specifed, in string form.

--*/

{

    BYTE RegisterNumber;

    RegisterNumber = (ModRm & X86_REG_MASK) >> X86_REG_SHIFT;
    switch (RegisterNumber) {
    case 0:
        return "cr0";
        break;

    case 2:
        return "cr2";
        break;

    case 3:
        return "cr3";
        break;

    case 4:
        return "cr4";
        break;
    }

    return "ERR";
}

PSTR
DbgpX86GetDebugRegister (
    BYTE ModRm
    )

/*++

Routine Description:

    This routine reads the REG bits of a ModR/M byte and returns a string
    representing the debug register in those bits.

Arguments:

    ModRm - Supplies the ModR/M byte of the instruction. Only bits 5:3 are used.

Return Value:

    The debug register specifed, in string form.

--*/

{

    BYTE RegisterNumber;

    RegisterNumber = (ModRm & X86_REG_MASK) >> X86_REG_SHIFT;
    if (RegisterNumber >= X86_DEBUG_REGISTER_COUNT) {
        return "ERR";
    }

    return DbgX86DebugRegisterNames[RegisterNumber];
}

PSTR
DbgpX86GetSegmentRegister (
    BYTE ModRm
    )

/*++

Routine Description:

    This routine reads the REG bits of a ModR/M byte and returns a string
    representing the segment register in those bits.

Arguments:

    ModRm - Supplies the ModR/M byte of the instruction. Only bits 5:3 are used.

Return Value:

    The segment register specifed, in string form.

--*/

{

    BYTE RegisterNumber;

    RegisterNumber = (ModRm & X86_REG_MASK) >> X86_REG_SHIFT;
    if (RegisterNumber >= X86_SEGMENT_REGISTER_COUNT) {
        return "ER";
    }

    return DbgX86SegmentRegisterNames[RegisterNumber];
}

PSTR
DbgpX86GetGenericRegister (
    X86_REGISTER RegisterNumber,
    CHAR Type
    )

/*++

Routine Description:

    This routine reads a register number and a width and returns a string
    representing that register. The register number should be in the same format
    as specified in the REG bits of the ModR/M byte.

Arguments:

    RegisterNumber - Supplies which register to print out, as specified by the
        REG bits of the ModR/M byte.

    Type - Supplies a width or special register format.

Return Value:

    The register specifed, in string form.

--*/

{

    if (RegisterNumber >= X86_REGISTER_NAME_COUNT) {
        return "ERR";
    }

    switch (Type) {
    case X86_WIDTH_BYTE:
        return DbgX86RegisterNames8Bit[RegisterNumber];

    case X86_WIDTH_WORD:
        return DbgX86RegisterNames16Bit[RegisterNumber];

    case X86_WIDTH_LONG:
        return DbgX86RegisterNames32Bit[RegisterNumber];

    case X86_FLOATING_POINT_REGISTER:
        return DbgX87RegisterNames[RegisterNumber];

    default:

        assert(FALSE);

        return "ERR";
    }
}

VOID
DbgpX86GetDisplacement (
    PX86_INSTRUCTION Instruction,
    PSTR Buffer,
    PLONGLONG DisplacementValue
    )

/*++

Routine Description:

    This routine prints an address displacement value.

Arguments:

    Instruction - Supplies a pointer to the instruction containing the
        displacement.

    Buffer - Supplies a pointer to the output buffer the displacement will be
        printed to.

    DisplacementValue - Supplies a pointer to the variable that will receive the
        numerical displacement value. This can be NULL.

Return Value:

    The instruction displacement field, in string form.

--*/

{

    LONG Displacement;

    if ((Buffer == NULL) || (Instruction == NULL)) {
        return;
    }

    strcpy(Buffer, "");
    if (Instruction->Displacement == 0) {
        return;
    }

    switch (Instruction->DisplacementSize) {
    case 1:
        Displacement = (CHAR)Instruction->Displacement;
        break;

    case 2:
        Displacement = (SHORT)Instruction->Displacement;
        break;

    case 4:
        Displacement = (LONG)Instruction->Displacement;
        break;

    default:
        return;
    }

    if (Displacement < 0) {
        sprintf(Buffer, "-0x%x", -Displacement);

    } else {
        sprintf(Buffer, "+0x%x", Displacement);
    }

    if (DisplacementValue != NULL) {
        *DisplacementValue = Displacement;
    }
}

PX86_INSTRUCTION_DEFINITION
DbgpX86GetTwoByteInstruction (
    PX86_INSTRUCTION Instruction
    )

/*++

Routine Description:

    This routine finds a two-byte instruction definition corresponding to the
    instruction opcode and prefixes.

Arguments:

    Instruction - Supplies a pointer to the instruction containing the
        two byte opcode and any prefixes.

Return Value:

    Returns a pointer to the instruction definition, or NULL if one could not
    be found.

--*/

{

    PX86_INSTRUCTION_DEFINITION Definition;
    ULONG InstructionIndex;
    ULONG InstructionLength;
    ULONG PrefixIndex;

    PrefixIndex = 0;
    InstructionLength = sizeof(DbgX86TwoByteInstructions) /
                        sizeof(DbgX86TwoByteInstructions[0]);

    //
    // First search through the array looking for a version with the first
    // corresponding prefix.
    //

    while (Instruction->Prefix[PrefixIndex] != 0) {
        InstructionIndex = 0;
        while (InstructionIndex < InstructionLength) {
            if ((DbgX86TwoByteInstructions[InstructionIndex].Prefix ==
                 Instruction->Prefix[PrefixIndex]) &&
                (DbgX86TwoByteInstructions[InstructionIndex].Opcode ==
                 Instruction->Opcode2)) {

                Definition =
                    &(DbgX86TwoByteInstructions[InstructionIndex].Instruction);

                return Definition;
            }

            InstructionIndex += 1;
        }

        PrefixIndex += 1;
    }

    //
    // The search for the specific prefix instruction was not successful, or
    // no prefixes were present. Search for the opcode with a prefix of zero,
    // indicating that the prefix field is not applicable.
    //

    InstructionIndex = 0;
    while (InstructionIndex < InstructionLength) {
        if ((DbgX86TwoByteInstructions[InstructionIndex].Opcode ==
             Instruction->Opcode2) &&
             (DbgX86TwoByteInstructions[InstructionIndex].Prefix == 0)) {

            return &(DbgX86TwoByteInstructions[InstructionIndex].Instruction);
        }

        InstructionIndex += 1;
    }

    //
    // The search yielded no results. Return NULL.
    //

    return NULL;
}

BOOL
DbgpX86DecodeFloatingPointInstruction (
    PX86_INSTRUCTION Instruction
    )

/*++

Routine Description:

    This routine decodes the given x87 floating point instruction by
    manipulating the instruction definition.

Arguments:

    Instruction - Supplies a pointer to the instruction.

Return Value:

    TRUE on success.

    FALSE if the instruction is invalid. Well, let's be more PC and say that no
    instruction is "invalid", only "executionally challenged".

--*/

{

    BYTE Index;
    BYTE Mod;
    BYTE ModRm;
    BYTE Opcode;
    BYTE Opcode2;

    ModRm = Instruction->ModRm;
    Mod = (ModRm & X86_MOD_MASK) >> X86_MOD_SHIFT;
    Opcode = Instruction->Opcode - X87_ESCAPE_OFFSET;
    Opcode2 = (ModRm & X86_REG_MASK) >> X86_REG_SHIFT;

    //
    // Reset the group to 0 so that after this routine tweaks everything it
    // gets treated like a normal instruction.
    //

    Instruction->Definition.Group = 0;
    Instruction->Definition.Mnemonic = NULL;

    //
    // If the ModR/M byte does not specify a register, then use the big
    // table to figure out the mnemonic.
    //

    if (Mod != X86ModValueRegister) {
        Instruction->Definition.Mnemonic = DbgX87Instructions[Opcode][Opcode2];
        if (Instruction->Definition.Mnemonic == NULL) {
            return FALSE;
        }

        return TRUE;
    }

    switch (Opcode) {

    //
    // Handle D8 instructions.
    //

    case 0:
        Instruction->Definition.Mnemonic = DbgX87Instructions[0][Opcode2];

        //
        // The fcom and fcomp instructions take only ST(i). Everything else
        // has two operands, st, and st(i).
        //

        if ((ModRm & X87_FCOM_MASK) == X87_FCOM_OPCODE) {
            Instruction->Definition.Target = X87_REGISTER_TARGET;

        } else {
            Instruction->Definition.Target = X87_ST0_TARGET;
            Instruction->Definition.Source = X87_REGISTER_TARGET;
        }

        break;

    //
    // Handle D9 instructions.
    //

    case 1:
        switch (Opcode2) {

        //
        // C0-C7 is FLD ST(i).
        //

        case 0:
            Instruction->Definition.Mnemonic = X87_FLD_MNEMONIC;
            Instruction->Definition.Target = X87_REGISTER_TARGET;
            break;

        //
        // C8-CF is FXCH ST(i).
        //

        case 1:
            Instruction->Definition.Mnemonic = X87_FXCH_MNEMONIC;
            Instruction->Definition.Target = X87_REGISTER_TARGET;
            break;

        //
        // D0-D7 is just a NOP (really only at D0, but let it slide).
        //

        case 2:
            Instruction->Definition.Mnemonic = X87_NOP_MNEMONIC;
            Instruction->Definition.Target = "";
            break;

        //
        // D8-DF is FSTP1 ST(i).
        //

        case 3:
            Instruction->Definition.Mnemonic = X87_FSTP1_MNEMONIC;
            Instruction->Definition.Target = X87_REGISTER_TARGET;
            break;

        //
        // E0-FF is a grab bag of instructions with no operands.
        //

        default:
            Instruction->Definition.Mnemonic =
                        DbgX87D9E0Instructions[ModRm - X87_D9_E0_OFFSET];

            Instruction->Definition.Target = "";
            break;
        }

        break;

    //
    // Handle DA instructions.
    //

    case 2:

        //
        // The fucompp instruction lives off by itself in a wasteland.
        //

        if (ModRm == X87_FUCOMPP_OPCODE) {
            Instruction->Definition.Mnemonic = X87_FUCOMPP_MNEMONIC;
            Instruction->Definition.Target = "";

        } else {

            //
            // There are 8 instructions (4 valid), each of which take the form
            // xxx ST, ST(i). So each instruction takes up 8 bytes.
            //

            Index = (ModRm & X87_DA_C0_MASK) >> X87_DA_CO_SHIFT;
            Instruction->Definition.Mnemonic = DbgX87DAC0Instructions[Index];
            Instruction->Definition.Target = X87_ST0_TARGET;
            Instruction->Definition.Source = X87_REGISTER_TARGET;
        }

        break;

    //
    // Handle DB instructions.
    //

    case 3:
        Index = (ModRm & X87_DB_C0_MASK) >> X87_DB_C0_SHIFT;

        //
        // There's a small rash of inidividual instructions in the E0-E7
        // range.
        //

        if (Index == X87_DB_E0_INDEX) {
            Index = ModRm & X87_DB_E0_MASK;
            Instruction->Definition.Mnemonic = DbgX87DBE0Instructions[Index];
            Instruction->Definition.Target = "";

        //
        // Otherwise there are swaths of instructions that take up 8 bytes
        // each as they take the form xxx ST, ST(i).
        //

        } else {
            Instruction->Definition.Mnemonic = DbgX87DBC0Instructions[Index];
            Instruction->Definition.Target = X87_ST0_TARGET;
            Instruction->Definition.Source = X87_REGISTER_TARGET;
        }

        break;

    //
    // DC is the same as D8, except it handles doubles instead of singles
    // (floats). There's one other annoying detail which is that the FSUB and
    // FSUBR are switched above 0xC0. The same goes for FDIV and FDIVR.
    //

    case 4:
        Instruction->Definition.Mnemonic = DbgX87DCC0Instructions[Opcode2];

        //
        // The fcom and fcomp instructions take only ST(i). Everything else
        // has two operands, st, and st(i).
        //

        if ((ModRm & X87_FCOM_MASK) == X87_FCOM_OPCODE) {
            Instruction->Definition.Target = X87_REGISTER_TARGET;

        } else {
            Instruction->Definition.Target = X87_ST0_TARGET;
            Instruction->Definition.Source = X87_REGISTER_TARGET;
        }

        break;

    //
    // Handle DD instructions.
    //

    case 5:
        Instruction->Definition.Mnemonic = DbgX87DDC0Instructions[Opcode2];
        Instruction->Definition.Target = X87_REGISTER_TARGET;
        break;

    //
    // Handle DE instructions.
    //

    case 6:
        Instruction->Definition.Mnemonic = DbgX87DEC0Instructions[Opcode2];
        Instruction->Definition.Target = X87_REGISTER_TARGET;
        Instruction->Definition.Source = X87_ST0_TARGET;
        break;

    //
    // Handle DF instructions.
    //

    case 7:
        Index = (ModRm & X87_DF_C0_MASK) >> X87_DF_C0_SHIFT;

        //
        // There's a small rash of individual instructions in the E0-E7
        // range. They're pretty old school.
        //

        if (Index == X87_DF_E0_INDEX) {
            Index = ModRm & X87_DF_E0_MASK;
            if (Index < X87_DF_E0_COUNT) {
                Instruction->Definition.Mnemonic =
                                                 DbgX87DFE0Instructions[Index];

                Instruction->Definition.Target = X87_DF_E0_TARGET;
            }

        } else {
            Instruction->Definition.Mnemonic = DbgX87DFC0Instructions[Opcode2];
            Instruction->Definition.Target = X87_REGISTER_TARGET;
            Instruction->Definition.Source = X87_ST0_TARGET;
        }

        break;

    //
    // This function was inappropriately called.
    //

    default:

        assert(FALSE);

        break;
    }

    if (Instruction->Definition.Mnemonic == NULL) {
        return FALSE;
    }

    return TRUE;
}

