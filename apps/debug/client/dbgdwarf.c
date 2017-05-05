/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgdwarf.c

Abstract:

    This module implements the glue functions that connect the rest of the
    debugger to the DWARF symbol library.

Author:

    Evan Green 17-Dec-2015

Environment:

    Debug

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include <minoca/debug/spproto.h>
#include <minoca/debug/dbgext.h>
#include "dbgrprof.h"
#include "dbgapi.h"
#include "symbols.h"
#include "dbgrcomm.h"
#include "dwarf.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INT
DwarfTargetRead (
    PDWARF_CONTEXT Context,
    ULONGLONG TargetAddress,
    ULONGLONG Size,
    ULONG AddressSpace,
    PVOID Buffer
    )

/*++

Routine Description:

    This routine performs a read from target memory.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    TargetAddress - Supplies the address to read from.

    Size - Supplies the number of bytes to read.

    AddressSpace - Supplies the address space identifier. Supply 0 for normal
        memory.

    Buffer - Supplies a pointer where the read data will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    ULONG BytesRead;
    PDEBUGGER_CONTEXT DebuggerContext;
    INT Status;
    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    DebuggerContext = Symbols->HostContext;

    assert(DebuggerContext != NULL);
    assert(AddressSpace == 0);

    Status = DbgReadMemory(DebuggerContext,
                           TRUE,
                           TargetAddress,
                           Size,
                           Buffer,
                           &BytesRead);

    if (Status != 0) {
        return Status;
    }

    if (BytesRead != Size) {
        return EFAULT;
    }

    return 0;
}

INT
DwarfTargetReadRegister (
    PDWARF_CONTEXT Context,
    ULONG Register,
    PULONGLONG Value
    )

/*++

Routine Description:

    This routine reads a register value.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Register - Supplies the register to read.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDEBUGGER_CONTEXT DebuggerContext;
    PREGISTERS_UNION Registers;
    INT Status;
    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    DebuggerContext = Symbols->HostContext;

    assert(DebuggerContext != NULL);

    Registers = Symbols->RegistersContext;
    if (Registers == NULL) {
        Registers = &(DebuggerContext->FrameRegisters);
    }

    Status = DbgGetRegister(DebuggerContext, Registers, Register, Value);
    return Status;
}

INT
DwarfTargetWriteRegister (
    PDWARF_CONTEXT Context,
    ULONG Register,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes a register value.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Register - Supplies the register to write.

    Value - Supplies the new value of the register.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDEBUGGER_CONTEXT DebuggerContext;
    PREGISTERS_UNION Registers;
    INT Status;
    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    DebuggerContext = Symbols->HostContext;

    assert(DebuggerContext != NULL);

    Registers = Symbols->RegistersContext;
    if (Registers == NULL) {
        Registers = &(DebuggerContext->FrameRegisters);
    }

    Status = DbgSetRegister(DebuggerContext, Registers, Register, Value);
    return Status;
}

INT
DwarfTargetWritePc (
    PDWARF_CONTEXT Context,
    ULONGLONG Value
    )

/*++

Routine Description:

    This routine writes a the instruction pointer register, presumably with the
    return address.

Arguments:

    Context - Supplies a pointer to the DWARF context.

    Value - Supplies the new value of the register.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PDEBUGGER_CONTEXT DebuggerContext;
    PREGISTERS_UNION Registers;
    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    DebuggerContext = Symbols->HostContext;

    assert(DebuggerContext != NULL);

    Registers = Symbols->RegistersContext;
    if (Registers == NULL) {
        Registers = &(DebuggerContext->FrameRegisters);
    }

    DbgSetPc(DebuggerContext, Registers, Value);
    return 0;
}

PSTR
DwarfGetRegisterName (
    PDWARF_CONTEXT Context,
    ULONG Register
    )

/*++

Routine Description:

    This routine returns a string containing the name of the given register.

Arguments:

    Context - Supplies a pointer to the application context.

    Register - Supplies the register number.

Return Value:

    Returns a pointer to a constant string containing the name of the register.

--*/

{

    PDEBUG_SYMBOLS Symbols;

    Symbols = (((PDEBUG_SYMBOLS)Context) - 1);
    return DbgGetRegisterName(Symbols->Machine, Register);
}

//
// --------------------------------------------------------- Internal Functions
//

