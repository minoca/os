/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dwline.c

Abstract:

    This module implements support for processing the DWARF 2+ line number
    program.

Author:

    Evan Green 8-Dec-2015

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include "dwarfp.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

#define DWARF_LINE_IS_STATEMENT 0x00000001
#define DWARF_LINE_BASIC_BLOCK 0x00000002
#define DWARF_LINE_END_SEQUENCE 0x00000004
#define DWARF_LINE_PROLOGUE_END 0x00000008
#define DWARF_LINE_EPILOGUE_BEGIN 0x00000010

#define DWARF_INITIAL_INCLUDE_FILE_CAPACITY 4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the state of a row in the DWARF line number matrix.

Members:

    Address - Stores the current address.

    OpIndex - Stores the operation index for Very Long Instruction Word
        instructions. For most architectures this is always zero.

    File - Stores the file index.

    Line - Stores the one-based line number.

    Column - Stores the column number.

    Flags - Stores the bitfield of flags. See DWARF_LINE_* definitions.

    Isa - Stores the instruction set identifier. Zero indicates the
        architecturally defined instruction set.

    Discriminator - Stores the discriminator for multiple blocks that share the
        same file, line, and column. Most of the time this is zero.

--*/

typedef struct _DWARF_LINE {
    ULONGLONG Address;
    ULONG OpIndex;
    ULONG File;
    ULONG Line;
    ULONG Column;
    ULONG Flags;
    ULONG Isa;
    ULONG Discriminator;
} DWARF_LINE, *PDWARF_LINE;

/*++

Structure Description:

    This structure stores the state machine context for a DWARF line number
    program.

Members:

    Registers - Stores the line registers.

    PreviousLine - Stores the previously emitted line number.

--*/

typedef struct _DWARF_LINE_STATE {
    DWARF_LINE Registers;
    PSOURCE_LINE_SYMBOL PreviousLine;
} DWARF_LINE_STATE, *PDWARF_LINE_STATE;

/*++

Structure Description:

    This structure stores the information for a source file in the DWARF line
    number program.

Members:

    Path - Stores a pointer to a string containing the relative or absolute
        path to the file.

    DirectoryIndex - Stores the index of the include directory this file lives
        in.

    ModificationDate - Stores the modification date of the file when it was
        read by the compiler.

    FileSize - Stores the size of the file when it was read by the compiler.

    FileSymbol - Stores a cached pointer to the source file symbol.

--*/

typedef struct _DWARF_LINE_FILE {
    PSTR Path;
    DWARF_LEB128 DirectoryIndex;
    DWARF_LEB128 ModificationDate;
    DWARF_LEB128 FileSize;
    PSOURCE_FILE_SYMBOL FileSymbol;
} DWARF_LINE_FILE, *PDWARF_LINE_FILE;

/*++

Structure Description:

    This structure stores the header of a line table program.

Members:

    UnitLength - Stores the size in bytes of the line number information for
        this compilation unit, not including this field.

    Is64Bit - Stores a boolean indicating whether this table is 64-bit or not.

    Version - Stores the table version number.

    HeaderLength - Stores the number of bytes following the header length
        field to the beginning of the first byte of the line number program
        itself. This is 4/8 bytes depending on whether 64-bit mode is in effect.

    MinimumInstructionLength - Stores the size in bytes of the smallest target
        machine instruction. Line number program opcodes that alter the address
        and op-index registers use this and the maximum operations per
        instruction member in their calculations.

    MaximumOperationsPerInstruction - Stores the maximum number of individual
        operations that may be encoded in an instruction. For non-VLIW
        architectures, this is 1, and the operation pointer is always the
        address register.

    DefaultIsStatement - Stores a boolean indicating the initial value of the
        is-statement flag.

    LineBase - Stores a parameter used in calculating the effects of special
        offsets.

    LineRange - Stores another parameter used in calculating the effects of
        special offsets.

    OpcodeBase - Stores the number assigned to the first special opcode (one
        higher than the highest standard opcode implemented).

    StandardOpcodeLengths - Stores the number of LEB128 operands for each of
        the standard opcodes. The first element of the array corresponds to
        opcode 1, and the last element corresponds to OpcodeBase - 1.

    IncludeDirectories - Stores an array of include paths that the compiler
        searched when generating this compilation unit.

    IncludeDirectoryCount - Stores the number of elements in the include
        directories array, including the space left for the zeroth entry.

    Files - Stores an array of include files.

    FileCount - Stores the number of elements in the files array.

    ProgramStart - Stores a pointer to the start of the line number program.

    End - Stores the end of this table.

--*/

typedef struct _DWARF_LINE_TABLE_HEADER {
    ULONGLONG UnitLength;
    BOOL Is64Bit;
    USHORT Version;
    ULONGLONG HeaderLength;
    UCHAR MinimumInstructionLength;
    UCHAR MaximumOperationsPerInstruction;
    UCHAR DefaultIsStatement;
    SCHAR LineBase;
    UCHAR LineRange;
    UCHAR OpcodeBase;
    UCHAR StandardOpcodeLengths[32];
    PSTR *IncludeDirectories;
    UINTN IncludeDirectoryCount;
    PDWARF_LINE_FILE Files;
    UINTN FileCount;
    PUCHAR ProgramStart;
    PUCHAR End;
} DWARF_LINE_TABLE_HEADER, *PDWARF_LINE_TABLE_HEADER;

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
DwarfpProcessLineTable (
    PDWARF_CONTEXT Context,
    PSTR CompileDirectory,
    ULONG AddressSize,
    PUCHAR *Table,
    PUCHAR End
    );

INT
DwarfpEmitLine (
    PDWARF_CONTEXT Context,
    PDWARF_LINE_TABLE_HEADER Header,
    PDWARF_LINE_STATE State
    );

INT
DwarfpReadLineNumberHeader (
    PUCHAR *Table,
    PUCHAR End,
    PDWARF_LINE_TABLE_HEADER Header
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR DwarfLineStandardOpNames[] = {
    "DwarfLnsCopy",
    "DwarfLnsAdvancePc",
    "DwarfLnsAdvanceLine",
    "DwarfLnsSetFile",
    "DwarfLnsSetColumn",
    "DwarfLnsNegateStatement",
    "DwarfLnsSetBasicBlock",
    "DwarfLnsConstAddPc",
    "DwarfLnsFixedAdvancePc",
    "DwarfLnsSetPrologueEnd",
    "DwarfLnsSetEpilogueBegin",
    "DwarfLnsSetIsa"
};

PSTR DwarfLineExtendedOpNames[] = {
    "DwarfLneEndSequence",
    "DwarfLneSetAddress",
    "DwarfLneDefineFile",
    "DwarfLneSetDiscriminator",
};

//
// ------------------------------------------------------------------ Functions
//

INT
DwarfpProcessStatementList (
    PDWARF_CONTEXT Context,
    PDWARF_DIE Die
    )

/*++

Routine Description:

    This routine is called on a compile unit DIE to process the line numbers.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Die - Supplies a pointer to the compile unit DIE.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR CompileDirectory;
    PUCHAR End;
    PDWARF_LOADING_CONTEXT LoadingContext;
    ULONGLONG Offset;
    PDWARF_ATTRIBUTE_VALUE OffsetAttribute;
    INT Status;
    PUCHAR Table;
    PDWARF_COMPILATION_UNIT Unit;

    assert(Die->Tag == DwarfTagCompileUnit);

    LoadingContext = Context->LoadingContext;
    Unit = LoadingContext->CurrentUnit;
    CompileDirectory = DwarfpGetStringAttribute(Context, Die, DwarfAtCompDir);
    OffsetAttribute = DwarfpGetAttribute(Context, Die, DwarfAtStatementList);
    if (OffsetAttribute == NULL) {
        return 0;
    }

    if (!DWARF_SECTION_OFFSET_FORM(OffsetAttribute->Form, Unit)) {
        return 0;
    }

    Offset = OffsetAttribute->Value.Offset;
    Table = Context->Sections.Lines.Data;
    End = Table + Context->Sections.Lines.Size;
    if ((Table == NULL) || (Table == End) || (Table + Offset >= End)) {
        DWARF_ERROR("DWARF: Missing line number information.\n");
        return 0;
    }

    Table += Offset;
    Status = DwarfpProcessLineTable(Context,
                                    CompileDirectory,
                                    Unit->AddressSize,
                                    &Table,
                                    End);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
DwarfpProcessLineTable (
    PDWARF_CONTEXT Context,
    PSTR CompileDirectory,
    ULONG AddressSize,
    PUCHAR *Table,
    PUCHAR End
    )

/*++

Routine Description:

    This routine processes a single compilation unit's DWARF line table.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    CompileDirectory - Supplies a pointer to the compilation unit's directory.

    AddressSize - Supplies the size of an address on the target.

    Table - Supplies a pointer that on input contains a pointer to the start of
        the section. On output this pointer will be advanced past the fields
        scanned.

    End - Supplies a pointer to the end of the section.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN AllocationSize;
    PDWARF_LINE_FILE File;
    DWARF_LINE_TABLE_HEADER Header;
    UINTN Index;
    ULONG InitialFlags;
    PVOID NewBuffer;
    PUCHAR Next;
    UCHAR Op;
    DWARF_LEB128 Operand;
    ULONG OperationAdvance;
    DWARF_SLEB128 SignedOperand;
    DWARF_LEB128 Size;
    PUCHAR Start;
    DWARF_LINE_STATE State;
    INT Status;

    //
    // Read and and potentially print out the header.
    //

    memset(&Header, 0, sizeof(DWARF_LINE_TABLE_HEADER));
    Start = *Table;
    Status = DwarfpReadLineNumberHeader(Table, End, &Header);
    if (Status != 0) {
        goto ProcessLineTableEnd;
    }

    assert(Header.IncludeDirectoryCount >= 1);

    Header.IncludeDirectories[0] = CompileDirectory;
    if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
        DWARF_PRINT("Line Table at offset %x\n"
                    " UnitLength %I64x %s\n"
                    " Version %d\n"
                    " Header Length 0x%I64x\n"
                    " Minimum Instruction Length %d\n"
                    " Default Is Statement: %d\n"
                    " Line Base: %d\n"
                    " Line Range: %d\n"
                    " Opcode Base: %d\n\n"
                    " Opcodes:\n",
                    (PVOID)Start - Context->Sections.Lines.Data,
                    Header.UnitLength,
                    Header.Is64Bit ? "64-bit" : "32-bit",
                    Header.Version,
                    Header.HeaderLength,
                    Header.MinimumInstructionLength,
                    Header.DefaultIsStatement,
                    Header.LineBase,
                    Header.LineRange,
                    Header.OpcodeBase);

        for (Index = 1; Index < Header.OpcodeBase; Index += 1) {
            DWARF_PRINT("  Opcode %d: %d arguments.\n",
                        Index,
                        Header.StandardOpcodeLengths[Index]);
        }

        DWARF_PRINT("\n Directory Table:\n");
        for (Index = 0; Index < Header.IncludeDirectoryCount; Index += 1) {
            DWARF_PRINT("  %d: %s\n", Index, Header.IncludeDirectories[Index]);
        }

        DWARF_PRINT("\n File Table\n  Index: Directory Time Size Name\n");
        for (Index = 0; Index < Header.FileCount; Index += 1) {
            DWARF_PRINT("  %d: %I64d %I64x %I64d %s\n",
                        Index + 1,
                        Header.Files[Index].DirectoryIndex,
                        Header.Files[Index].ModificationDate,
                        Header.Files[Index].FileSize,
                        Header.Files[Index].Path);
        }

        DWARF_PRINT("\n Line Statements:\n");
    }

    //
    // Initialize the state machine registers.
    //

    InitialFlags = 0;
    if (Header.DefaultIsStatement != FALSE) {
        InitialFlags |= DWARF_LINE_IS_STATEMENT;
    }

    memset(&State, 0, sizeof(DWARF_LINE_STATE));
    State.Registers.File = 1;
    State.Registers.Line = 1;
    State.Registers.Flags = InitialFlags;

    assert(Header.MaximumOperationsPerInstruction != 0);

    //
    // Loop decoding the line program.
    //

    *Table = Header.ProgramStart;
    while (*Table < Header.End) {
        Op = DwarfpRead1(Table);

        //
        // Most opcodes are special opcodes. Each special opcode does the same
        // thing, they only differ in how far they advance the address/opindex
        // and line. How far they advance depends on values in the header that
        // are tuned per architecture, and the formulas below which come out of
        // the DWARF4 specification.
        //

        if (Op >= Header.OpcodeBase) {
            Op -= Header.OpcodeBase;
            if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                DWARF_PRINT("   Special op %d ", Op);
            }

            OperationAdvance = Op / Header.LineRange;
            State.Registers.Address +=
                Header.MinimumInstructionLength *
                ((State.Registers.OpIndex + OperationAdvance) /
                 Header.MaximumOperationsPerInstruction);

            State.Registers.OpIndex =
                (State.Registers.OpIndex + OperationAdvance) %
                Header.MaximumOperationsPerInstruction;

            State.Registers.Line += Header.LineBase + (Op % Header.LineRange);
            Status = DwarfpEmitLine(Context, &Header, &State);
            if (Status != 0) {
                goto ProcessLineTableEnd;
            }

            State.Registers.Flags &= ~(DWARF_LINE_BASIC_BLOCK |
                                       DWARF_LINE_PROLOGUE_END |
                                       DWARF_LINE_EPILOGUE_BEGIN);

        //
        // If the first byte is zero, this is an extended opcode.
        //

        } else if (Op == 0) {
            Size = DwarfpReadLeb128(Table);
            Next = *Table + Size;
            Op = DwarfpRead1(Table);
            if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                if (Op <= DwarfLneSetDiscriminator) {
                    DWARF_PRINT("   Extended: %s ",
                                DwarfLineExtendedOpNames[Op - 1]);

                } else if (Op >= DwarfLneLowUser) {
                    DWARF_PRINT("   Extended: User%d ", Op);

                } else {
                    DWARF_PRINT("   Standard: Unknown%d ", Op);
                }
            }

            switch (Op) {

            //
            // Emit one more row using the current registers, set the end
            // sequence boolean in the registers. Then reset the state machine
            // and move to the next sequence.
            //

            case DwarfLneEndSequence:
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("   End Sequence\n ");
                }

                State.Registers.Flags |= DWARF_LINE_END_SEQUENCE;
                Status = DwarfpEmitLine(Context, &Header, &State);
                if (Status != 0) {
                    goto ProcessLineTableEnd;
                }

                memset(&State, 0, sizeof(DWARF_LINE_STATE));
                State.Registers.File = 1;
                State.Registers.Line = 1;
                State.Registers.Flags = InitialFlags;
                break;

            //
            // Set the address to a relocatable target-address-sized address.
            //

            case DwarfLneSetAddress:
                if (AddressSize == 8) {
                    State.Registers.Address = DwarfpRead8(Table);

                } else {

                    assert(AddressSize == 4);

                    State.Registers.Address = DwarfpRead4(Table);
                }

                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%I64x ", State.Registers.Address);
                }

                break;

            //
            // Create a new file in the array.
            //

            case DwarfLneDefineFile:
                AllocationSize = (Header.FileCount + 1) *
                                 sizeof(DWARF_LINE_FILE);

                NewBuffer = realloc(Header.Files, AllocationSize);
                if (NewBuffer == NULL) {
                    Status = ENOMEM;
                    goto ProcessLineTableEnd;
                }

                Header.Files = NewBuffer;
                File = &(Header.Files[Header.FileCount]);
                File->Path = (PSTR)*Table;
                *Table += strlen(File->Path) + 1;
                File->DirectoryIndex = DwarfpReadLeb128(Table);
                File->ModificationDate = DwarfpReadLeb128(Table);
                File->FileSize = DwarfpReadLeb128(Table);
                File->FileSymbol = NULL;
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%d: %I64d %I64d %I64d %s ",
                                Header.FileCount,
                                File->DirectoryIndex,
                                File->ModificationDate,
                                File->FileSize,
                                File->Path);
                }

                Header.FileCount += 1;
                break;

            case DwarfLneSetDiscriminator:
                State.Registers.Discriminator = DwarfpReadLeb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%u ", State.Registers.Discriminator);
                }

                break;

            default:
                DWARF_ERROR("DWARF: Unknown extended op %d\n", Op);
                *Table = Next;
                break;
            }

            //
            // The known instructions should have fully used up the
            // instructions, and unknown ones should simply be set to jump
            // over.
            //

            assert(*Table == Next);

        //
        // If it's less than the special opcode base, then it's a standard
        // opcode.
        //

        } else {
            if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                if (Op <= DwarfLnsSetIsa) {
                    DWARF_PRINT("   Standard: %s ",
                                DwarfLineStandardOpNames[Op - 1]);

                } else {
                    DWARF_PRINT("   Standard: Unknown%d ", Op);
                }
            }

            switch (Op) {

            //
            // Append a new row to the matrix using the current values of the
            // registers. Then reset the booleans.
            //

            case DwarfLnsCopy:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                Status = DwarfpEmitLine(Context, &Header, &State);
                if (Status != 0) {
                    goto ProcessLineTableEnd;
                }

                State.Registers.Discriminator = 0;
                State.Registers.Flags &= ~(DWARF_LINE_BASIC_BLOCK |
                                           DWARF_LINE_PROLOGUE_END |
                                           DWARF_LINE_EPILOGUE_BEGIN);

                break;

            //
            // Get an single LEB128 operand, and advance the PC and op-index
            // (but not the line) the way the special opcodes do.
            //

            case DwarfLnsAdvancePc:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                Operand = DwarfpReadLeb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%I64u ", Operand);
                }

                State.Registers.Address +=
                    Header.MinimumInstructionLength *
                    ((State.Registers.OpIndex + Operand) /
                     Header.MaximumOperationsPerInstruction);

                State.Registers.OpIndex =
                    (State.Registers.OpIndex + Operand) %
                    Header.MaximumOperationsPerInstruction;

                break;

            //
            // Simply add the operand to the line.
            //

            case DwarfLnsAdvanceLine:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                SignedOperand = DwarfpReadSleb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%I64d ", SignedOperand);
                }

                State.Registers.Line += SignedOperand;
                break;

            //
            // Set a new file.
            //

            case DwarfLnsSetFile:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                State.Registers.File = DwarfpReadLeb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%u ", State.Registers.File);
                }

                break;

            //
            // Set a new column.
            //

            case DwarfLnsSetColumn:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                State.Registers.Column = DwarfpReadLeb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%u ", State.Registers.Column);
                }

                break;

            //
            // Toggle the is statement flag.
            //

            case DwarfLnsNegateStatement:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                State.Registers.Flags ^= DWARF_LINE_IS_STATEMENT;
                break;

            //
            // Set the basic block flag.
            //

            case DwarfLnsSetBasicBlock:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                State.Registers.Flags |= DWARF_LINE_BASIC_BLOCK;
                break;

            //
            // Advance the address and op-index by the increments corresponding
            // to special register 255 (but don't touch the line number). This
            // allows a slightly more efficient "fast-forward" of the PC.
            //

            case DwarfLnsConstAddPc:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                OperationAdvance = (255 - Header.OpcodeBase) / Header.LineRange;
                State.Registers.Address +=
                    Header.MinimumInstructionLength *
                    ((State.Registers.OpIndex + OperationAdvance) /
                     Header.MaximumOperationsPerInstruction);

                State.Registers.OpIndex =
                    (State.Registers.OpIndex + OperationAdvance) %
                    Header.MaximumOperationsPerInstruction;

                break;

            //
            // Advance the PC by a fixed size operand and clear op-index. This
            // is supported for simpler assemblers that cannot emit special
            // opcodes.
            //

            case DwarfLnsFixedAdvancePc:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                Operand = DwarfpRead2(Table);
                State.Registers.Address += Operand;
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%u ", (USHORT)Operand);
                }

                State.Registers.OpIndex = 0;
                break;

            //
            // Set the prologue end flag.
            //

            case DwarfLnsSetPrologueEnd:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                State.Registers.Flags |= DWARF_LINE_PROLOGUE_END;
                break;

            //
            // Set the epilogue begin flag.
            //

            case DwarfLnsSetEpilogueBegin:

                assert(Header.StandardOpcodeLengths[Op] == 0);

                State.Registers.Flags |= DWARF_LINE_EPILOGUE_BEGIN;
                break;

            //
            // Set the current instruction set.
            //

            case DwarfLnsSetIsa:

                assert(Header.StandardOpcodeLengths[Op] == 1);

                State.Registers.Isa = DwarfpReadLeb128(Table);
                if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
                    DWARF_PRINT("%u ", State.Registers.Isa);
                }

                break;

            //
            // Advance over the instruction without knowing what it does.
            //

            default:
                Index = Header.StandardOpcodeLengths[Op];
                while (Index != 0) {
                    Operand = DwarfpReadLeb128(Table);
                    if ((Context->Flags &
                         DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {

                        DWARF_PRINT("%I64x ", Operand);
                    }
                }

                break;
            }
        }

        if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
            DWARF_PRINT("\n");
        }
    }

ProcessLineTableEnd:
    if (Header.IncludeDirectories != NULL) {
        free(Header.IncludeDirectories);
    }

    if (Header.Files != NULL) {
        free(Header.Files);
    }

    return Status;
}

INT
DwarfpEmitLine (
    PDWARF_CONTEXT Context,
    PDWARF_LINE_TABLE_HEADER Header,
    PDWARF_LINE_STATE State
    )

/*++

Routine Description:

    This routine emits a line number symbol based on the current DWARF line
    program state.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Header - Supplies a pointer to the line table header.

    State - Supplies a pointer to the current line state.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN DirectoryIndex;
    PSOURCE_FILE_SYMBOL File;
    ULONG FileIndex;
    ULONG Flags;
    PSOURCE_LINE_SYMBOL Line;
    PDWARF_LINE_FILE LineFile;
    PDWARF_LOADING_CONTEXT LoadingContext;

    LoadingContext = Context->LoadingContext;

    assert(LoadingContext->CurrentFile != NULL);

    //
    // First, find the source file symbol, creating it if it does not exist.
    //

    assert(State->Registers.File != 0);

    FileIndex = State->Registers.File - 1;
    if (FileIndex >= Header->FileCount) {
        DWARF_ERROR("DWARF: File index %d bigger than count %d.\n",
                    FileIndex,
                    Header->FileCount);

        return ERANGE;
    }

    LineFile = &(Header->Files[FileIndex]);
    if (LineFile->FileSymbol != NULL) {
        File = LineFile->FileSymbol;

    } else {
        DirectoryIndex = LineFile->DirectoryIndex;
        if (DirectoryIndex >= Header->IncludeDirectoryCount) {
            DWARF_ERROR("DWARF: Directory index %d bigger than count %d.\n",
                        DirectoryIndex,
                        Header->IncludeDirectoryCount);

            return ERANGE;
        }

        File = DwarfpFindSource(Context,
                                Header->IncludeDirectories[DirectoryIndex],
                                LineFile->Path,
                                TRUE);

        if (File == NULL) {
            return ENOMEM;
        }

        LineFile->FileSymbol = File;
    }

    if ((Context->Flags & DWARF_CONTEXT_DEBUG_LINE_NUMBERS) != 0) {
        DWARF_PRINT("\n    Emit: %s/%s:%d %I64x ",
                    File->SourceDirectory,
                    File->SourceFile,
                    State->Registers.Line,
                    State->Registers.Address);

        if (State->Registers.Column != 0) {
            DWARF_PRINT("Column %d ", State->Registers.Column);
        }

        if (State->Registers.OpIndex != 0) {
            DWARF_PRINT("OpIndex %d ", State->Registers.OpIndex);
        }

        if (State->Registers.Discriminator != 0) {
            DWARF_PRINT("Disc %d ", State->Registers.Discriminator);
        }

        if (State->Registers.Isa != 0) {
            DWARF_PRINT("Isa %d ", State->Registers.Isa);
        }

        Flags = State->Registers.Flags;
        if ((Flags & DWARF_LINE_IS_STATEMENT) != 0) {
            DWARF_PRINT("Stmt ");
        }

        if ((Flags & DWARF_LINE_BASIC_BLOCK) != 0) {
            DWARF_PRINT("BasicBlock ");
        }

        if ((Flags & DWARF_LINE_PROLOGUE_END) != 0) {
            DWARF_PRINT("PrologueEnd ");
        }

        if ((Flags & DWARF_LINE_EPILOGUE_BEGIN) != 0) {
            DWARF_PRINT("EpilogueBegin ");
        }

        if ((Flags & DWARF_LINE_END_SEQUENCE) != 0) {
            DWARF_PRINT("End ");
        }
    }

    //
    // Set the end address of the previous line if there is one.
    //

    if (State->PreviousLine != NULL) {
        State->PreviousLine->End = State->Registers.Address;
    }

    //
    // If it's the end sequence, don't create a new line.
    //

    if ((State->Registers.Flags & DWARF_LINE_END_SEQUENCE) != 0) {
        return 0;
    }

    //
    // TODO: Change StartOffset and EndOffset to be absolute always, and
    // remove the owning function from the line symbol.
    //

    Line = malloc(sizeof(SOURCE_LINE_SYMBOL));
    if (Line == NULL) {
        return ENOMEM;
    }

    memset(Line, 0, sizeof(SOURCE_LINE_SYMBOL));
    Line->ParentSource = File;
    Line->LineNumber = State->Registers.Line;
    Line->Start = State->Registers.Address;
    Line->End = Line->Start + 1;
    State->PreviousLine = Line;
    INSERT_BEFORE(&(Line->ListEntry),
                  &(LoadingContext->CurrentFile->SourceLinesHead));

    return 0;
}

INT
DwarfpReadLineNumberHeader (
    PUCHAR *Table,
    PUCHAR End,
    PDWARF_LINE_TABLE_HEADER Header
    )

/*++

Routine Description:

    This routine reads the DWARF line number program header out of the section.

Arguments:

    Table - Supplies a pointer that on input contains a pointer to the start of
        the section. On output this pointer will be advanced past the fields
        scanned.

    End - Supplies a pointer to the end of the section.

    Header - Supplies a pointer where the header details will be returned on
        success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    UINTN Capacity;
    UINTN Count;
    PDWARF_LINE_FILE File;
    PDWARF_LINE_FILE Files;
    UINTN Index;
    PVOID NewBuffer;
    PSTR String;

    DwarfpReadInitialLength(Table, &(Header->Is64Bit), &(Header->UnitLength));
    Header->End = *Table + Header->UnitLength;
    Header->Version = DwarfpRead2(Table);
    Header->HeaderLength = DWARF_READN(Table, Header->Is64Bit);
    Header->ProgramStart = *Table + Header->HeaderLength;
    Header->MinimumInstructionLength = DwarfpRead1(Table);
    Header->MaximumOperationsPerInstruction = 1;
    if (Header->Version >= 4) {
        Header->MaximumOperationsPerInstruction = DwarfpRead1(Table);
    }

    Header->DefaultIsStatement = DwarfpRead1(Table);
    Header->LineBase = (SCHAR)(DwarfpRead1(Table));
    Header->LineRange = DwarfpRead1(Table);
    Header->OpcodeBase = DwarfpRead1(Table);

    //
    // If too many more standard opcodes are introduced, this structure will
    // need to be adjusted.
    //

    assert(Header->OpcodeBase < sizeof(Header->StandardOpcodeLengths));

    //
    // Opcode zero is ignored since it's the opcode to introduce extended
    // opcodes. Gather the parameter counts for the standard opcodes.
    //

    Header->StandardOpcodeLengths[0] = -1;
    for (Index = 1; Index < Header->OpcodeBase; Index += 1) {
        Header->StandardOpcodeLengths[Index] = DwarfpRead1(Table);
    }

    //
    // Count the include directories, terminated by a null byte. The zeroth
    // entry is always the current compilation unit's compile directory.
    //

    Count = 1;
    String = (PSTR)*Table;
    while (*String != '\0') {
        Count += 1;
        String += strlen(String) + 1;
    }

    Header->IncludeDirectories = malloc((Count + 1) * sizeof(PSTR));
    if (Header->IncludeDirectories == NULL) {
        return ENOMEM;
    }

    memset(Header->IncludeDirectories, 0, (Count + 1) * sizeof(PSTR));

    //
    // Fill in the array of include directories.
    //

    String = (PSTR)*Table;
    Index = 1;
    while (*String != '\0') {
        Header->IncludeDirectories[Index] = String;
        Index += 1;
        String += strlen(String) + 1;
    }

    Header->IncludeDirectoryCount = Index;
    *Table = (PUCHAR)String + 1;

    //
    // Now fill in the array of file entries.
    //

    Capacity = 0;
    Count = 0;
    Files = NULL;
    while (**Table != '\0') {

        //
        // Allocate a new entry if needed.
        //

        if (Capacity <= Count) {
            if (Capacity == 0) {
                Capacity = DWARF_INITIAL_INCLUDE_FILE_CAPACITY;

            } else {
                Capacity *= 2;
            }

            NewBuffer = realloc(Files, Capacity * sizeof(DWARF_LINE_FILE));
            if (NewBuffer == NULL) {
                if (Files != NULL) {
                    free(Files);
                }

                return ENOMEM;
            }

            Files = NewBuffer;
        }

        File = &(Files[Count]);
        File->Path = (PSTR)*Table;
        *Table += strlen(File->Path) + 1;
        File->DirectoryIndex = DwarfpReadLeb128(Table);
        File->ModificationDate = DwarfpReadLeb128(Table);
        File->FileSize = DwarfpReadLeb128(Table);
        File->FileSymbol = NULL;
        Count += 1;
    }

    Header->Files = Files;
    Header->FileCount = Count;
    return 0;
}

