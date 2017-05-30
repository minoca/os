/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    disasm.c

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

#include <minoca/lib/types.h>
#include "disasm.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ------------------------------------------------------------------ Functions
//

BOOL
DbgDisassemble (
    ULONGLONG InstructionPointer,
    PBYTE InstructionStream,
    PSTR Buffer,
    ULONG BufferLength,
    PDISASSEMBLED_INSTRUCTION Disassembly,
    MACHINE_LANGUAGE Language
    )

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

{

    BOOL Result;

    switch (Language) {
    case MachineLanguageX86:
    case MachineLanguageX64:
        Result = DbgpX86Disassemble(InstructionPointer,
                                    InstructionStream,
                                    Buffer,
                                    BufferLength,
                                    Disassembly,
                                    Language);

        break;

    case MachineLanguageArm:
    case MachineLanguageThumb2:
        Result = DbgpArmDisassemble(InstructionPointer,
                                    InstructionStream,
                                    Buffer,
                                    BufferLength,
                                    Disassembly,
                                    Language);

        break;

    default:
        Result = FALSE;
        goto DisassembleEnd;
    }

DisassembleEnd:
    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

