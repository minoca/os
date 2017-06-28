/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgext.h

Abstract:

    This header defines the interface between the debug client and debugger
    extensions.

Author:

    Evan Green 10-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <stdarg.h>
#include <minoca/debug/dbgproto.h>

//
// ---------------------------------------------------------------- Definitions
//

#define EXTENSION_API_VERSION 1
#define MAX_EXTENSION_COMMAND 32

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DEBUGGER_CONTEXT DEBUGGER_CONTEXT, *PDEBUGGER_CONTEXT;
typedef struct _TYPE_SYMBOL TYPE_SYMBOL, *PTYPE_SYMBOL;

typedef
INT
(*PEXTENSION_PROTOTYPE) (
    PDEBUGGER_CONTEXT Context,
    PSTR Command,
    ULONG ArgumentCount,
    PSTR *ArgumentValues
    );

/*++

Routine Description:

    This routine defines a debugger extension prototype. This what gets called
    when the user invokes the extension.

Arguments:

    Context - Supplies a pointer to the debugger applicaton context, which is
        an argument to most of the API functions.

    Command - Supplies the subcommand entered, if applicable, or NULL if no
        subcommand was registered.

    ArgumentCount - Supplies the number of arguments in the ArgumentValues
        array.

    ArgumentValues - Supplies the values of each argument. This memory will be
        reused when the function returns, so extensions must not touch this
        memory after returning from this call. The first argument is always the
        complete name itself (ie "!myext.help").

Return Value:

    0 if the debugger extension command was successful.

    Returns an error code if a failure occurred along the way.

--*/

/*++

Structure Description:

    This structure stores information about the current debugging target.

Members:

    MachineType - Supplies the architecture of the machine being debugged.
        See MACHINE_TYPE_* definitions.

--*/

typedef struct _DEBUG_TARGET_INFORMATION {
    ULONG MachineType;
} DEBUG_TARGET_INFORMATION, *PDEBUG_TARGET_INFORMATION;

/*++

Structure Description:

    This structure defines a frame in a call stack.

Members:

    FramePointer - Stores a pointer to the base of the stack frame. On x86
        architectures, this would be the EBP register.

    ReturnAddress - Stores the return address of the current stack frame.

--*/

typedef struct _STACK_FRAME {
    ULONGLONG FramePointer;
    ULONGLONG ReturnAddress;
} STACK_FRAME, *PSTACK_FRAME;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

//
// Functions to be implemented by the debug extension.
//

INT
ExtensionMain (
    PDEBUGGER_CONTEXT Context,
    ULONG ExtensionApiVersion,
    PVOID Token
    );

/*++

Routine Description:

    This routine defines the extension's main routine. This routine will get
    called when the extension is loaded. It is responsible for registering the
    debugger extensions it supports.

Arguments:

    Context - Supplies the application instance context. This must be passed
        into the registration routines.

    ExtensionApiVersion - Supplies the revision of the debugger extension API.

    Token - Supplies a token that uniquely idenfies the extension. This is used
        when registering extensions.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

//
// Functions callable by the extension.
//

INT
DbgRegisterExtension (
    PDEBUGGER_CONTEXT Context,
    PVOID Token,
    PSTR ExtensionName,
    PSTR OneLineDescription,
    PEXTENSION_PROTOTYPE Routine
    );

/*++

Routine Description:

    This routine registers a debugger extension with the client.

Arguments:

    Context - Supplies a pointer to the application context.

    Token - Supplies the unique token provided to the extension library upon
        initialization.

    ExtensionName - Supplies the name of the extension to register. This name
        must not already be registered by the current extension or any other.

    OneLineDescription - Supplies a quick description of the extension, no
        longer than 60 characters. This parameter is not optional.

    Routine - Supplies the routine to call when the given extension is
        invoked.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgOut (
    const char *Format,
    ...
    );

/*++

Routine Description:

    This routine prints a formatted string to the debugger console.

Arguments:

    Format - Supplies the printf format string.

    ... - Supplies a variable number of arguments, as required by the printf
        format string argument.

Return Value:

    Returns the number of bytes successfully converted, not including the null
    terminator.

    Returns a negative number if an error was encountered.

--*/

INT
DbgOutVaList (
    PDEBUGGER_CONTEXT Context,
    const char *Format,
    va_list Arguments
    );

/*++

Routine Description:

    This routine prints a formatted string to the given debugger console.

Arguments:

    Context - Supplies a pointer to the debugger context to output to.

    Format - Supplies the printf format string.

    Arguments - Supplies the argument list to the format string. The va_end
        macro is not invoked on this list.

Return Value:

    Returns the number of bytes successfully converted. A null terminator is
    not written.

    Returns a negative number if an error was encountered.

--*/

INT
DbgEvaluate (
    PDEBUGGER_CONTEXT Context,
    PSTR String,
    PULONGLONG Result
    );

/*++

Routine Description:

    This routine evaluates a mathematical expression. The following operators
    are supported: +, -, *, /, (, ). No spaces are permitted. Module symbols
    are permitted and will be translated into their corresponding address.

Arguments:

    Context - Supplies a pointer to the debugger application context.

    String - Supplies the string to evaluate.

    Result - Supplies a pointer to the 64-bit unsigned integer where the result
        will be stored.

Return Value:

    0 if the expression was successfully evaluated.

    Returns an error code on failure.

--*/

INT
DbgPrintAddressSymbol (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    );

/*++

Routine Description:

    This routine prints a descriptive version of the given address, including
    the module and function name if possible.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the virtual address of the target to print information
        about.

Return Value:

    0 if information was successfully printed.

    Returns an error code on failure.

--*/

INT
DbgReadMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToRead,
    PVOID Buffer,
    PULONG BytesRead
    );

/*++

Routine Description:

    This routine retrieves the debuggee's memory.

Arguments:

    Context - Supplies a pointer to the application context.

    VirtualMemory - Supplies a flag indicating whether the read should be
        virtual or physical.

    Address - Supplies the address to read from the target's memory.

    BytesToRead - Supplies the number of bytes to be read.

    Buffer - Supplies a pointer to the buffer where the memory contents will be
        returned.

    BytesRead - Supplies a pointer that receive the number of bytes that were
        actually read from the target.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgWriteMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToWrite,
    PVOID Buffer,
    PULONG BytesWritten
    );

/*++

Routine Description:

    This routine writes to the debuggee's memory.

Arguments:

    Context - Supplies a pointer to the application context.

    VirtualMemory - Supplies a flag indicating whether the read should be
        virtual or physical.

    Address - Supplies the address to write to the target's memory.

    BytesToWrite - Supplies the number of bytes to be written.

    Buffer - Supplies a pointer to the buffer containing the values to write.

    BytesWritten - Supplies a pointer that receives the number of bytes that
        were actually written to the target.

Return Value:

    0 if the write was successful.

    Returns an error code on failure.

--*/

INT
DbgReboot (
    PDEBUGGER_CONTEXT Context,
    ULONG RebootType
    );

/*++

Routine Description:

    This routine attempts to reboot the target machine.

Arguments:

    Context - Supplies a pointer to the application context.

    RebootType - Supplies the type of reboot to perform. See the
        DEBUG_REBOOT_TYPE enumeration.

Return Value:

    0 if the write was successful.

    Returns an error code on failure.

--*/

INT
DbgGetCallStack (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PSTACK_FRAME Frames,
    PULONG FrameCount
    );

/*++

Routine Description:

    This routine attempts to unwind the call stack starting at the given
    machine state.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the registers on input. On
        output, these registers will be updated with the unwound value. If this
        is NULL, then the current break notification registers will be used.

    Frames - Supplies a pointer where the array of stack frames will be
        returned.

    FrameCount - Supplies the number of frames allocated in the frames
        argument, representing the maximum number of frames to get. On output,
        returns the number of valid frames in the array.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgStackUnwind (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PBOOL Unwind,
    PSTACK_FRAME Frame
    );

/*++

Routine Description:

    This routine attempts to unwind the stack by one frame.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the registers on input. On output, these
        registers will be updated with the unwound value.

    Unwind - Supplies a pointer that on input should initially be set to TRUE,
        indicating to use the symbol unwinder if possible. If unwinding is not
        possible, this will be set to FALSE, and should remain FALSE for the
        remainder of the stack frames unwound.

    Frame - Supplies a pointer where the basic frame information for this
        frame will be returned.

Return Value:

    0 on success.

    EOF if there are no more stack frames.

    Returns an error code on failure.

--*/

INT
DbgPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    BOOL PrintFrameNumbers
    );

/*++

Routine Description:

    This routine prints a call stack starting with the given registers.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the registers to use when
        unwinding.

    PrintFrameNumbers - Supplies a boolean indicating whether or not frame
        numbers should be printed to the left of every frame.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgGetTargetInformation (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_TARGET_INFORMATION TargetInformation,
    ULONG TargetInformationSize
    );

/*++

Routine Description:

    This routine returns information about the machine being debugged.

Arguments:

    Context - Supplies a pointer to the application context.

    TargetInformation - Supplies a pointer where the target information will
        be returned.

    TargetInformationSize - Supplies the size of the target information buffer.
        This must be the size of a debug target information structure.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

ULONG
DbgGetTargetPointerSize (
    PDEBUGGER_CONTEXT Context
    );

/*++

Routine Description:

    This routine returns the size of a pointer on the target machine, in bytes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    The size of a pointer on the target system, in bytes.

--*/

VOID
DbgGetStackRegisters (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    PULONGLONG StackPointer,
    PULONGLONG FramePointer
    );

/*++

Routine Description:

    This routine returns the stack and/or frame pointer registers from a
    given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies a pointer to the filled out registers union.

    StackPointer - Supplies an optional pointer where the stack register value
        will be returned.

    FramePointer - Supplies an optional pointer where the stack frame base
        register value will be returned.

Return Value:

    None.

--*/

ULONGLONG
DbgGetPc (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers
    );

/*++

Routine Description:

    This routine returns the value of the program counter (instruction pointer)
    register in the given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the filled out registers union.
        If NULL, then the registers from the current frame will be used.

Return Value:

    Returns the instruction pointer member from the given registers.

--*/

VOID
DbgSetPc (
    PDEBUGGER_CONTEXT Context,
    PREGISTERS_UNION Registers,
    ULONGLONG Value
    );

/*++

Routine Description:

    This routine sets the value of the program counter (instruction pointer)
    register in the given registers union.

Arguments:

    Context - Supplies a pointer to the application context.

    Registers - Supplies an optional pointer to the filled out registers union.
        If NULL, then the registers from the current frame will be used.

    Value - Supplies the new value to set.

Return Value:

    None.

--*/

INT
DbgGetMemberOffset (
    PTYPE_SYMBOL StructureType,
    PSTR FieldName,
    PULONG FieldOffset,
    PULONG FieldSize
    );

/*++

Routine Description:

    This routine returns the given field's offset (in bits) within the
    given structure.

Arguments:

    StructureType - Supplies a pointer to a symbol structure type.

    FieldName - Supplies a string containing the name of the field whose offset
        will be returned.

    FieldOffset - Supplies a pointer that will receive the bit offset of the
        given field name within the given structure.

    FieldSize - Supplies a pointer that will receive the size of the field in
        bits.

Return Value:

    0 on success.

    ENOENT if no such field name exists.

    Other error codes on other errors.

--*/

INT
DbgGetTypeByName (
    PDEBUGGER_CONTEXT Context,
    PSTR TypeName,
    PTYPE_SYMBOL *Type
    );

/*++

Routine Description:

    This routine finds a type symbol object by its type name.

Arguments:

    Context - Supplies a pointer to the application context.

    TypeName - Supplies a pointer to the string containing the name of the
        type to find. This can be prefixed with an module name if needed.

    Type - Supplies a pointer where a pointer to the type will be returned.

Return Value:

    0 on success.

    ENOENT if no type with the given name was found.

    Returns an error number on failure.

--*/

INT
DbgReadIntegerMember (
    PDEBUGGER_CONTEXT Context,
    PTYPE_SYMBOL Type,
    PSTR MemberName,
    ULONGLONG Address,
    PVOID Data,
    ULONG DataSize,
    PULONGLONG Value
    );

/*++

Routine Description:

    This routine reads an integer sized member out of an already read-in
    structure.

Arguments:

    Context - Supplies a pointer to the application context.

    Type - Supplies a pointer to the type of the data.

    MemberName - Supplies a pointer to the member name.

    Address - Supplies the address where the data was obtained.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data buffer in bytes.

    Value - Supplies a pointer where the value will be returned on success.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgReadTypeByName (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PSTR TypeName,
    PTYPE_SYMBOL *FinalType,
    PVOID *Data,
    PULONG DataSize
    );

/*++

Routine Description:

    This routine reads in data from the target for a specified type, which is
    given as a string.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    TypeName - Supplies a pointer to a string containing the type name to get.
        This should start with a type name, and can use dot '.' notation to
        specify field members, and array[] notation to specify dereferences.

    FinalType - Supplies a pointer where the final type symbol will be returned
        on success.

    Data - Supplies a pointer where the data will be returned on success. The
        caller is responsible for freeing this data when finished.

    DataSize - Supplies a pointer where the size of the data in bytes will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgReadType (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PTYPE_SYMBOL Type,
    PVOID *Data,
    PULONG DataSize
    );

/*++

Routine Description:

    This routine reads in data from the target for a specified type.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies a target address pointer where the data resides.

    Type - Supplies a pointer to the type symbol to get.

    Data - Supplies a pointer where the data will be returned on success. The
        caller is responsible for freeing this data when finished.

    DataSize - Supplies a pointer where the size of the data in bytes will be
        returned.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

INT
DbgPrintTypeMember (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address,
    PVOID Data,
    ULONG DataSize,
    PTYPE_SYMBOL Type,
    PSTR MemberName,
    ULONG SpaceLevel,
    ULONG RecursionCount
    );

/*++

Routine Description:

    This routine prints a member of a structure or union whose contents have
    already been read in.

Arguments:

    Context - Supplies a pointer to the application context.

    Address - Supplies the address where this data came from.

    Data - Supplies a pointer to the data contents.

    DataSize - Supplies the size of the data contents buffer in bytes.

    Type - Supplies a pointer to the structure type.

    MemberName - Supplies the name of the member to print.

    SpaceLevel - Supplies the number of spaces worth of indentation to print
        for subsequent lines.

    RecursionCount - Supplies the number of substructures to recurse into.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

