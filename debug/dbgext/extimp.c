/*++

Copyright (c) 2013 Minoca Corp. All Rights Reserved

Module Name:

    extimp.c

Abstract:

    This module implements the import library for debugger extensions. It is
    needed so that the extension doesn't link against a binary name directly
    (as there are several debugger client versions).

Author:

    Evan Green 8-May-2013

Environment:

    Debug Client

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/rtl.h>
#include "dbgext.h"
#include "extimp.h"

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
// Store the import table.
//

PDEBUG_EXTENSION_IMPORT_INTERFACE DbgImportInterface;

//
// ------------------------------------------------------------------ Functions
//

DLLEXPORT
INT
ExtensionEntry (
    ULONG ExtensionApiVersion,
    PDEBUGGER_CONTEXT ApplicationContext,
    PVOID Token,
    PDEBUG_EXTENSION_IMPORT_INTERFACE ImportInterface
    )

/*++

Routine Description:

    This routine defines the extension's internal entry point. This routine will
    get called when the extension is loaded. It is responsible for saving off
    the interface and then calling the debugger extension entry point.

Arguments:

    ExtensionApiVersion - Supplies the revision of the debugger extension API.

    ApplicationContext - Supplies a pointer that represents the debugger
        application instance.

    Token - Supplies a token that uniquely idenfies the extension. This is used
        when registering extensions.

    ImportInterface - Supplies a pointer to the functions that the extension
        can call. This buffer will be constant, the extension does not need
        to make a copy of it.

Return Value:

    0 on success.

    Returns an error code on failure. The extension will be unloaded if it
    returns non-zero.

--*/

{

    //
    // Wire up the import interface, then call the extension.
    //

    DbgImportInterface = ImportInterface;
    return ExtensionMain(ApplicationContext, ExtensionApiVersion, Token);
}

INT
DbgRegisterExtension (
    PDEBUGGER_CONTEXT Context,
    PVOID Token,
    PSTR ExtensionName,
    PSTR OneLineDescription,
    PEXTENSION_PROTOTYPE Routine
    )

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

{

    INT Result;

    Result = DbgImportInterface->RegisterExtension(Context,
                                                   Token,
                                                   ExtensionName,
                                                   OneLineDescription,
                                                   Routine);

    return Result;
}

INT
DbgOut (
    const char *Format,
    ...
    )

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

{

    va_list Arguments;
    int Result;

    va_start(Arguments, Format);
    Result = DbgOutVaList(NULL, Format, Arguments);
    va_end(Arguments);
    return Result;
}

INT
DbgOutVaList (
    PDEBUGGER_CONTEXT Context,
    const char *Format,
    va_list Arguments
    )

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

{

    INT Result;

    Result = DbgImportInterface->OutVaList(Context, Format, Arguments);
    return Result;
}

INT
DbgEvaluate (
    PDEBUGGER_CONTEXT Context,
    PSTR String,
    PULONGLONG Result
    )

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

{

    return DbgImportInterface->Evaluate(Context, String, Result);
}

INT
DbgPrintAddressSymbol (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG Address
    )

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

{

    return DbgImportInterface->PrintAddressSymbol(Context, Address);
}

INT
DbgPrintType (
    PDEBUGGER_CONTEXT Context,
    PSTR TypeString,
    PVOID Data,
    ULONG DataSizeInBytes
    )

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

{

    INT Result;

    Result = DbgImportInterface->PrintType(Context,
                                           TypeString,
                                           Data,
                                           DataSizeInBytes);

    return Result;
}

INT
DbgReadMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToRead,
    PVOID Buffer,
    PULONG BytesRead
    )

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

{

    INT Result;

    Result = DbgImportInterface->ReadMemory(Context,
                                            VirtualMemory,
                                            Address,
                                            BytesToRead,
                                            Buffer,
                                            BytesRead);

    return Result;
}

INT
DbgWriteMemory (
    PDEBUGGER_CONTEXT Context,
    BOOL VirtualMemory,
    ULONGLONG Address,
    ULONG BytesToWrite,
    PVOID Buffer,
    PULONG BytesWritten
    )

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

{

    INT Result;

    Result = DbgImportInterface->WriteMemory(Context,
                                             VirtualMemory,
                                             Address,
                                             BytesToWrite,
                                             Buffer,
                                             BytesWritten);

    return Result;
}

INT
DbgReboot (
    PDEBUGGER_CONTEXT Context,
    ULONG RebootType
    )

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

{

    INT Result;

    Result = DbgImportInterface->Reboot(Context, RebootType);
    return Result;
}

INT
DbgGetCallStack (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG StartingBasePointer,
    ULONGLONG StackPointer,
    ULONGLONG InstructionPointer,
    PSTACK_FRAME Frames,
    ULONG FrameCount,
    PULONG FramesRead
    )

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

{

    INT Result;

    Result = DbgImportInterface->GetCallStack(Context,
                                              StartingBasePointer,
                                              StackPointer,
                                              InstructionPointer,
                                              Frames,
                                              FrameCount,
                                              FramesRead);

    return Result;
}

INT
DbgPrintCallStack (
    PDEBUGGER_CONTEXT Context,
    ULONGLONG InstructionPointer,
    ULONGLONG StackPointer,
    ULONGLONG BasePointer,
    BOOL PrintFrameNumbers
    )

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

{

    INT Result;

    Result = DbgImportInterface->PrintCallStack(Context,
                                                InstructionPointer,
                                                StackPointer,
                                                BasePointer,
                                                PrintFrameNumbers);

    return Result;
}

INT
DbgGetTargetInformation (
    PDEBUGGER_CONTEXT Context,
    PDEBUG_TARGET_INFORMATION TargetInformation,
    ULONG TargetInformationSize
    )

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

{

    INT Result;

    Result = DbgImportInterface->GetTargetInformation(Context,
                                                      TargetInformation,
                                                      TargetInformationSize);

    return Result;
}

ULONG
DbgGetTargetPointerSize (
    PDEBUGGER_CONTEXT Context
    )

/*++

Routine Description:

    This routine returns the size of a pointer on the target machine, in bytes.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    The size of a pointer on the target system, in bytes.

--*/

{

    return DbgImportInterface->GetTargetPointerSize(Context);
}

//
// --------------------------------------------------------- Internal Functions
//

