/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

//
// ---------------------------------------------------------------- Definitions
//

#define EXTENSION_API_VERSION 1
#define MAX_EXTENSION_COMMAND 32

//
// Define image machine types.
//

#define MACHINE_TYPE_X86 0x1
#define MACHINE_TYPE_ARMV7 0x2
#define MACHINE_TYPE_ARMV6 0x3

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _DEBUGGER_CONTEXT DEBUGGER_CONTEXT, *PDEBUGGER_CONTEXT;

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
DbgPrintType (
    PDEBUGGER_CONTEXT Context,
    PSTR TypeString,
    PVOID Data,
    ULONG DataSizeInBytes
    );

/*++

Routine Description:

    This routine prints the contents of a given type.

Arguments:

    Context - Supplies a pointer to the application context.

    TypeString - Supplies the type to print either information or contents of.

    Data - Supplies a pointer to the data to use as contents of the type.

    DataSizeInBytes - Supplies the size of the Data buffer, in bytes.

Return Value:

    0 if the type was successfully printed.

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
    ULONGLONG StartingBasePointer,
    ULONGLONG StackPointer,
    ULONGLONG InstructionPointer,
    PSTACK_FRAME Frames,
    ULONG FrameCount,
    PULONG FramesRead
    );

/*++

Routine Description:

    This routine attempts to read the call stack starting at the given base
    pointer.

Arguments:

    Context - Supplies a pointer to the application context.

    StartingBasePointer - Supplies the virtual address of the topmost (most
        recent) stack frame base.

    StackPointer - Supplies an optional pointer to the current stack pointer. If
        supplied, it will be used with the instruction pointer to potentially
        make the first stack frame more accurate.

    InstructionPointer - Supplies an optional virtual address of the
        instruction pointer.

    Frames - Supplies a pointer where the array of stack frames will be
        returned.

    FrameCount - Supplies the number of frames allocated in the frames
        argument, representing the maximum number of frames to get.

    FramesRead - Supplies a pointer where the number of valid frames in the
        frames array will be returned.

Return Value:

    0 on success.

    Returns an error code on failure.

--*/

INT
DbgPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG InstructionPointer,
    ULONGLONG StackPointer,
    ULONGLONG BasePointer,
    BOOL PrintFrameNumbers
    );

/*++

Routine Description:

    This routine prints a call stack starting with the given registers.

Arguments:

    Context - Supplies a pointer to the application context.

    InstructionPointer - Supplies the instruction pointer of the thread.

    StackPointer - Supplies the stack pointer of the thread.

    BasePointer - Supplies the base pointer of the thread.

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

