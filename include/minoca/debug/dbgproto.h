/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dbgproto.h

Abstract:

    This header contains definitions for the kernel debugging protocol. It is
    used by both the debugger and the target.

Author:

    Evan Green 3-Jul-2012

--*/

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the debugger protocol version.
//

#define DEBUG_PROTOCOL_MAJOR_VERSION    1
#define DEBUG_PROTOCOL_REVISION         4

//
// Define some size limits.
//

#define BREAK_NOTIFICATION_STREAM_SIZE 16

//
// Define machine types.
//

#define MACHINE_TYPE_X86 0x1
#define MACHINE_TYPE_ARM 0x2
#define MACHINE_TYPE_X64 0x3

//
// Define the maximum size of a debug packet, including the header.
//

#define DEBUG_PACKET_SIZE 1500

//
// Define the maximum size of the debug payload.
//

#define DEBUG_PAYLOAD_SIZE (DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER))

//
// Define the magic value that signifies the beginning of a packet.
//

#define DEBUG_PACKET_MAGIC_BYTE1 0x45
#define DEBUG_PACKET_MAGIC_BYTE2 0x47

#define DEBUG_PACKET_MAGIC \
    ((DEBUG_PACKET_MAGIC_BYTE2 << 8) | DEBUG_PACKET_MAGIC_BYTE1)

//
// Define the size of the magic field, in bytes.
//

#define DEBUG_PACKET_MAGIC_SIZE 2

//
// Define two single-byte resynchronization constants, one that is sent by the
// host, and one that is sent by the target. Them being different prevents
// false positives from a loopback device.
//

#define DEBUG_SYNCHRONIZE_HOST 0x3F
#define DEBUG_SYNCHRONIZE_TARGET 0x21

//
// Define the escaped characters.
//

#define DEBUG_ESCAPE 'X'
#define DEBUG_XON 0x11
#define DEBUG_XOFF 0x13

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _DEBUG_REBOOT_TYPE {
    DebugRebootInvalid,
    DebugRebootShutdown,
    DebugRebootWarm,
    DebugRebootCold,
    DebugRebootTypeCount
} DEBUG_REBOOT_TYPE, *PDEBUG_REBOOT_TYPE;

/*++

Structure Description:

    This structure defines what a debug packet header looks like being sent
    across the wire. It is assumed values are being sent in little endian order.

Members:

    Magic - Stores a magic value that signifies the start of a packet. This
        field is used to get the host and target in sync.

    Command - Stores the definition for the contents of the payload. This could
        either be a command from the debugger or a response from the debuggee.

    Checksum - Stores the checksum of the entire packet, including the packet
        header and the payload. The only field not checksummed is the checksum
        field itself, which is taken to be zero.

    PayloadSize - Stores the size of the rest of the packet, which follows
        immediately after this field.

    PayloadSizeComplement - Stores the one's complement of the payload size,
        for header validation.

    Padding - Stores padding to align the structure.

--*/

typedef struct _DEBUG_PACKET_HEADER {
    USHORT Magic;
    USHORT Command;
    USHORT Checksum;
    USHORT PayloadSize;
    USHORT PayloadSizeComplement;
    USHORT Padding;
    // Data
} PACKED DEBUG_PACKET_HEADER, *PDEBUG_PACKET_HEADER;

/*++

Structure Description:

    This structure stores the format for a debug packet being sent across the
    wire. It is assumed values are being sent in little endian order. A debugger
    or debuggee won't necessarily transmit the entire size of this structure.

Members:

    Header - Stores the packet header.

    Payload - Stores the packet contents, which are interpreted differently
        depending on the contents of the header.

--*/

typedef struct _DEBUG_PACKET {
    DEBUG_PACKET_HEADER Header;
    UCHAR Payload[DEBUG_PAYLOAD_SIZE];
} PACKED DEBUG_PACKET, *PDEBUG_PACKET;

typedef enum _DEBUGGER_COMMAND {
    DbgInvalidCommand,
    DbgConnectionRequest,
    DbgConnectionAcknowledge,
    DbgConnectionWrongVersion,
    DbgConnectionUninitialized,
    DbgConnectionInvalidRequest,
    DbgBreakRequest,
    DbgBreakNotification,
    DbgCommandGo,
    DbgCommandSingleStep,
    DbgCommandRangeStep,
    DbgCommandSetRegisters,
    DbgCommandSwitchProcessor,
    DbgModuleListHeaderRequest,
    DbgModuleListEntriesRequest,
    DbgModuleListHeader,
    DbgModuleListEntry,
    DbgModuleListError,
    DbgMemoryReadVirtual,
    DbgMemoryWriteVirtual,
    DbgMemoryContents,
    DbgMemoryWriteAcknowledgement,
    DbgPrintString,
    DbgShutdownNotification,
    DbgPacketAcknowledge,
    DbgPacketResend,
    DbgProfilerNotification,
    DbgCommandGetSpecialRegisters,
    DbgCommandReturnSpecialRegisters,
    DbgCommandSetSpecialRegisters,
    DbgCommandReboot,
} DEBUGGER_COMMAND, *PDEBUGGER_COMMAND;

typedef enum _EXCEPTION_TYPE {
    ExceptionInvalid,
    ExceptionDebugBreak,
    ExceptionSingleStep,
    ExceptionAssertionFailure,
    ExceptionAccessViolation,
    ExceptionDoubleFault,
    ExceptionSignal,
    ExceptionIllegalInstruction,
    ExceptionUnknown
} EXCEPTION_TYPE, *PEXCEPTION_TYPE;

typedef enum _SHUTDOWN_TYPE {
    ShutdownTypeInvalid,
    ShutdownTypeTransition,
    ShutdownTypeExit,
    ShutdownTypeSynchronizationLost
} SHUTDOWN_TYPE, *PSHUTDOWN_TYPE;

/*++

Structure Description:

    This structure stores a connection request. This is sent by the debugger to
    attempt to connect to the debuggee.

Members:

    ProtocolMajorVersion - Supplies the major version of the debugging protocol.

    ProtocolMinorVersion - Supplies the revision of the debugging protocol.

    BreakRequested - Supplies a flag indicating whether the debugger wants an
        immediate breakpoint (TRUE) or just wants to connect (FALSE).

--*/

typedef struct _CONNECTION_REQUEST {
    ULONG ProtocolMajorVersion;
    ULONG ProtocolRevision;
    UCHAR BreakRequested;
} PACKED CONNECTION_REQUEST, *PCONNECTION_REQUEST;

/*++

Structure Description:

    This structure stores the response to a connection request. It is sent by
    the debuggee in response to a connection request packet. The kernel banner
    immediately follows this structure.

Members:

    ProtocolMajorVersion - Stores the major version of the debuggee's debug
        protocol. This should match with the connection request.

    ProtocolMinorVersion - Stores the minor version of the debuggee's debug
        protocol. It's TBD whether or not this has to be exactly the same as
        the connection request.

    SystemMajorVersion - Stores the major version of the system.

    SystemMinorVersion - Stores the minor version of the system.

    SystemRevision - Stores the revision number of the system.

    SystemReleaseLevel - Stores the release level of the system.

    SystemBuildDebugLevel - Stores the debug level of the system.

    Machine - Stores the machine type of the debuggee.

    ProductNameOffset - Stores the offset from the beginning of this structure
        to the product name string, or 0 if none is present.

    BuildStringOffset - Stores the offset from the beginning of this structure
        to the build string, or 0 if none is present.

    SystemSerialVersion - Stores the serial revision number of the system.

    SystemBuildTime - Stores the build time of the system.

--*/

typedef struct _CONNECTION_RESPONSE {
    USHORT ProtocolMajorVersion;
    USHORT ProtocolRevision;
    USHORT SystemMajorVersion;
    USHORT SystemMinorVersion;
    USHORT SystemRevision;
    USHORT SystemReleaseLevel;
    USHORT SystemBuildDebugLevel;
    USHORT Machine;
    ULONG ProductNameOffset;
    ULONG BuildStringOffset;
    ULONGLONG SystemSerialVersion;
    ULONGLONG SystemBuildTime;
} PACKED CONNECTION_RESPONSE, *PCONNECTION_RESPONSE;

/*++

Structure Description:

    This structure stores information about a "range" breakpoint. The range
    break essentially breaks within a certain range of addresses. It also
    allows a small exception to this range (ie. for the current source line).
    The range step is much slower than a regular breakpoint since it is
    implemented by putting the processor into single step mode and manually
    checking the range at every instruction.

Members:

    BreakRangeMinimum - Stores the first valid address in the breakpoint range.

    BreakRangeMaximum - Stores the first invalid address in the breakpoint
        range. Said differently, everything below (but not including) this
        address is in the break range.

    RangeHoleMinimum - Stores the start of a hole in the break range.
        Addresses in the hole do not cause a breakpoint.

    RangeHoleMaximum - Stores the first invalid address in the range hole.

--*/

typedef struct _RANGE_STEP {
    ULONGLONG BreakRangeMinimum;
    ULONGLONG BreakRangeMaximum;
    ULONGLONG RangeHoleMinimum;
    ULONGLONG RangeHoleMaximum;
} PACKED RANGE_STEP, *PRANGE_STEP;

/*++

Structure Description:

    This structure stores the state of the general registers of an x86 or x64
    processor.

Members:

    Registers - Store the contents of the respective registers. The member
        fields are made 64-bits wide so they can alias correctly on top of the
        64-bit registers.

--*/

typedef struct _X86_GENERAL_REGISTERS {
    ULONGLONG Eax;
    ULONGLONG Ebx;
    ULONGLONG Ecx;
    ULONGLONG Edx;
    ULONGLONG Ebp;
    ULONGLONG Esp;
    ULONGLONG Esi;
    ULONGLONG Edi;
    ULONGLONG Eip;
    ULONGLONG Eflags;
    USHORT Cs;
    USHORT Ds;
    USHORT Es;
    USHORT Fs;
    USHORT Gs;
    USHORT Ss;
} PACKED X86_GENERAL_REGISTERS, *PX86_GENERAL_REGISTERS;

typedef struct _X64_GENERAL_REGISTERS {
    ULONGLONG Rax;
    ULONGLONG Rbx;
    ULONGLONG Rcx;
    ULONGLONG Rdx;
    ULONGLONG Rbp;
    ULONGLONG Rsp;
    ULONGLONG Rsi;
    ULONGLONG Rdi;
    ULONGLONG R8;
    ULONGLONG R9;
    ULONGLONG R10;
    ULONGLONG R11;
    ULONGLONG R12;
    ULONGLONG R13;
    ULONGLONG R14;
    ULONGLONG R15;
    ULONGLONG Rip;
    ULONGLONG Rflags;
    USHORT Cs;
    USHORT Ds;
    USHORT Es;
    USHORT Fs;
    USHORT Gs;
    USHORT Ss;
} PACKED X64_GENERAL_REGISTERS, *PX64_GENERAL_REGISTERS;

/*++

Structure Description:

    This structure stores the state of the general registers of an ARM
    processor

Members:

    Registers - Store the contents of the respective registers.

--*/

typedef struct _ARM_GENERAL_REGISTERS {
    ULONG R0;
    ULONG R1;
    ULONG R2;
    ULONG R3;
    ULONG R4;
    ULONG R5;
    ULONG R6;
    ULONG R7;
    ULONG R8;
    ULONG R9;
    ULONG R10;
    ULONG R11Fp;
    ULONG R12Ip;
    ULONG R13Sp;
    ULONG R14Lr;
    ULONG R15Pc;
    ULONG Cpsr;
} PACKED ARM_GENERAL_REGISTERS, *PARM_GENERAL_REGISTERS;

/*++

Structure Description:

    This union stores the state of the general registers in a processor.

Members:

    X86 - Stores the IA-32 registers.

    Arm - Stores the ARM registers.

--*/

typedef union _REGISTERS_UNION {
    X86_GENERAL_REGISTERS X86;
    X64_GENERAL_REGISTERS X64;
    ARM_GENERAL_REGISTERS Arm;
} PACKED REGISTERS_UNION, *PREGISTERS_UNION;

typedef struct _X86_TABLE_REGISTER {
    ULONG Limit;
    ULONG Base;
} X86_TABLE_REGISTER, *PX86_TABLE_REGISTER;

typedef struct _X86_SPECIAL_REGISTERS {
    ULONGLONG Cr0;
    ULONGLONG Cr2;
    ULONGLONG Cr3;
    ULONGLONG Cr4;
    ULONGLONG Dr0;
    ULONGLONG Dr1;
    ULONGLONG Dr2;
    ULONGLONG Dr3;
    ULONGLONG Dr6;
    ULONGLONG Dr7;
    X86_TABLE_REGISTER Idtr;
    X86_TABLE_REGISTER Gdtr;
    USHORT Tr;
} PACKED X86_SPECIAL_REGISTERS, *PX86_SPECIAL_REGISTERS;

/*++

Structure Description:

    This structure stores the special register state in the ARM architecture.

Members:

    Sctlr - Stores the system control register.

    Actlr - Stores the auxiliary control register.

    Ttbr0 - Stores the first translation table base register.

    Ttbr1 - Stores the second translation table base register.

    Dfsr - Stores the data fault status register.

    Ifsr - Stores the instruction fault status register.

    Dfar - Stores the data fault address register.

    Ifar - Stores the instruction fault address register.

    Prrr - Stores the primary region remap register.

    Nmrr - Stores the normal memory remap register.

    Vbar - Stores the virtual base address register.

    Tpidrprw - Stores the privileged thread register.

    Par - Stores the physical address register.

    Ats1Cpr - Stores the privileged level read translation register.

    Ats1Cpw - Stores the privileged level write translation register.

    Ats1Cur - Stores the unprivileged level read translation register.

    Ats1Cuw - Stores the unprivileged level write translation register.

--*/

typedef struct _ARM_SPECIAL_REGISTERS {
    ULONG Sctlr;
    ULONG Actlr;
    ULONGLONG Ttbr0;
    ULONGLONG Ttbr1;
    ULONG Dfsr;
    ULONG Ifsr;
    ULONGLONG Dfar;
    ULONGLONG Ifar;
    ULONG Prrr;
    ULONG Nmrr;
    ULONG Vbar;
    ULONGLONG Tpidrprw;
    ULONGLONG Par;
    ULONG Ats1Cpr;
    ULONG Ats1Cpw;
    ULONG Ats1Cur;
    ULONG Ats1Cuw;
} PACKED ARM_SPECIAL_REGISTERS, *PARM_SPECIAL_REGISTERS;

/*++

Structure Description:

    This union stores the state of the special registers in a processor.

Members:

    Ia - Stores the Intel PC registers.

    Arm - Stores the ARM registers.

--*/

typedef union _SPECIAL_REGISTERS_UNION {
    X86_SPECIAL_REGISTERS Ia;
    ARM_SPECIAL_REGISTERS Arm;
} PACKED SPECIAL_REGISTERS_UNION, *PSPECIAL_REGISTERS_UNION;

/*++

Structure Description:

    This structure defines the command parameters for a set special registers
    command.

Members:

    Original - Stores the original (current) contents of the registers.

    New - Stores the new (desired) contents of the registers. Only the
        registers that differ from the original will actually be written to.

--*/

typedef struct _SET_SPECIAL_REGISTERS {
    SPECIAL_REGISTERS_UNION Original;
    SPECIAL_REGISTERS_UNION New;
} PACKED SET_SPECIAL_REGISTERS, *PSET_SPECIAL_REGISTERS;

/*++

Structure Description:

    This structure defines an exception notification. It is sent by the
    debuggee to the debugger when a break of some kind has been reached.

Members:

    Exception - Stores the type of exception that occurred. Examples are a
        single step exception, breakpoint, or access violation. See
        EXCEPTION_TYPE values.

    ProcessorOrThreadNumber - Stores which processor is broken in for kernel
        debugger breaks, or which thread is broken in for user mode
        debugger notifications.

    ProcessorOrThreadCount - Stores the number of processors in the system for
        kernel debugger breaks, or the number of threads in the process for
        user mode breaks.

    Process - Stores the process ID of the current process. This may be 0 if
        the process is not known, there is no process, or this is the kernel
        process.

    ProcessorBlock - Stores the virtual address of the processor block for this
        processor.

    LoadedModuleSignature - Stores the sum of the timestamps and loaded lowest
        address of the currently loaded modules in the target. This allows the
        debugger to quickly see if it's modules are in sync with the target.

    LoadedModuleCount - Stores the number of modules loaded in the target.

    ErrorCode - Stores the error code if one was generated by the hardware
        during the exception.

    InstructionPointer - Stores the location of the instruction that is just
        about to execute.

    InstructionStream - Stores the contents of memory at the instruction
        pointer. The instruction stream is guaranteed to be big enough to
        disassemble exactly one instruction (the one at the instruction
        pointer).

    Registers - Stores the current state of all the general registers.

--*/

typedef struct _BREAK_NOTIFICATION {
    ULONG Exception;
    ULONG ProcessorOrThreadNumber;
    ULONG ProcessorOrThreadCount;
    ULONG Process;
    ULONGLONG ProcessorBlock;
    ULONGLONG LoadedModuleSignature;
    ULONG LoadedModuleCount;
    ULONG ErrorCode;
    ULONGLONG InstructionPointer;
    BYTE InstructionStream[BREAK_NOTIFICATION_STREAM_SIZE];
    REGISTERS_UNION Registers;
} PACKED BREAK_NOTIFICATION, *PBREAK_NOTIFICATION;

/*++

Structure Description:

    This structure defines the beginning of a list of all loaded modules in the
    system. This is sent by the debuggee to the debugger when the debugger
    requests a complete list of loaded modules. An array of loaded module
    entries immediately follows this header.

Members:

    ModuleCount - Stores the number of loaded modules in the system.

    Padding - Stores some padding to align the following members.

    Signature - Stores the sum of all the loaded module timestamps and loaded
        lowest addresses. Useful as a quick estimate as to whether or not the
        host and target are in sync.

--*/

typedef struct _MODULE_LIST_HEADER {
    ULONG ModuleCount;
    ULONG Padding;
    ULONGLONG Signature;
    // LOADED_MODULE_ENTRY Modules[ANYSIZE_ARRAY];
} PACKED MODULE_LIST_HEADER, *PMODULE_LIST_HEADER;

/*++

Structure Description:

    This structure defines information about one loaded module in the kernel.
    An array of these structures are sent by the debuggee to the debugger when
    requesting a complete list of loaded modules.

Members:

    StructureSize - Stores the total size of the structure in bytes, including
        the full null terminated binary string. This address plus this size
        will point to the next loaded module entry structure.

    Timestamp - Stores the modification date of this module in seconds since
        2001.

    LowestAddress - Stores the lowest address in memory where the binary has
        memory. Subtracting the base difference from this value results in the
        image's preferred load address.

    Size - Stores the size of this module when loaded into memory.

    Process - Stores the ID of the process this module is specific to.

    BinaryName - Stores the name of the executable.

--*/

typedef struct _LOADED_MODULE_ENTRY {
    ULONG StructureSize;
    ULONGLONG Timestamp;
    ULONGLONG LowestAddress;
    ULONGLONG Size;
    ULONG Process;
    CHAR BinaryName[ANYSIZE_ARRAY];
} PACKED LOADED_MODULE_ENTRY, *PLOADED_MODULE_ENTRY;

/*++

Structure Description:

    This structure defines a memory contents request. It is sent by the
    debugger when reading or writing debuggee memory. If it's a write request
    (as defined in the debug packet header), the data to write immediately
    follows after this request structure.

Members:

    Address - Stores the virtual address of the memory that the debugger wants
        from the debuggee.

    Size - Stores the number of bytes the debuggee wants from the specified
        virtual address.

--*/

typedef struct _MEMORY_REQUEST {
    ULONGLONG Address;
    ULONG Size;
    // Write data follows here.
} PACKED MEMORY_REQUEST, *PMEMORY_REQUEST;

/*++

Structure Description:

    This structure defines the header on debuggee memory contents. It is sent
    by the debuggee to the debugger after the debugger has requested to read
    memory. The actual data follows immediately after this header.

Members:

    Address - Stores the virtual address of the data being returned.

    Size - Stores the number of bytes being returned to the debugger. Note that
        this can be smaller than the number of bytes requested, so it is
        important that the debugger look at this field.

--*/

typedef struct _MEMORY_CONTENTS {
    ULONGLONG Address;
    ULONG Size;
    // Data follows here
} PACKED MEMORY_CONTENTS, *PMEMORY_CONTENTS;

/*++

Structure Description:

    This structure defines a write acknowledgment. It is sent by the debuggee
    to the debugger after the debugger has requested to write to the debuggee's
    memory.

Members:

    Address - Stores the virtual address that was successfully written to.

    BytesWritten - Stores the number of bytes that were successfully written
        into the debuggee's memory. Note that this can be smaller than the
        number of bytes requested, so it is important that the debugger check
        this field.

--*/

typedef struct _WRITE_REQUEST_ACKNOWLEDGEMENT {
    ULONGLONG Address;
    ULONG BytesWritten;
} PACKED WRITE_REQUEST_ACKNOWLEDGEMENT, *PWRITE_REQUEST_ACKNOWLEDGEMENT;

/*++

Structure Description:

    This structure defines a notification of debuggee shutdown. It is sent by
    the debuggee to the debugger whenever the kernel debugging system is torn
    down.

Members:

    ShutdownType - Stores a code indicating why the debugger shut down.

    UnloadAllSymbols - Stores a boolean indicating whether or not the debugger
        should unload all of its loaded module information.

    Process - Stores the identifier of the process exiting for process exit
        notifications.

    ExitStatus - Stores a status code associated with the shutdown.

--*/

typedef struct _SHUTDOWN_NOTIFICATION {
    SHUTDOWN_TYPE ShutdownType;
    UCHAR UnloadAllSymbols;
    ULONG Process;
    ULONG ExitStatus;
} PACKED SHUTDOWN_NOTIFICATION, *PSHUTDOWN_NOTIFICATION;

/*++

Structure Description:

    This structure defines a request to switch to another processor in the
    debugger.

Members:

    ProcessorNumber - Stores the zero-based processor number to switch to.

--*/

typedef struct _SWITCH_PROCESSOR_REQUEST {
    ULONG ProcessorNumber;
} PACKED SWITCH_PROCESSOR_REQUEST, *PSWITCH_PROCESSOR_REQUEST;

/*++

Structure Description:

    This structure defines the payload of an acknowledge command.

Members:

    BreakInRequested - Stores a boolean indicating whether a break in is
        requested by the user or not.

--*/

typedef struct _DEBUG_PACKET_ACKNOWLEDGE {
    UCHAR BreakInRequested;
} PACKED DEBUG_PACKET_ACKNOWLEDGE, *PDEBUG_PACKET_ACKNOWLEDGE;

/*++

Structure Description:

    This structure defines the payload of an acknowledge command.

Members:

    ResetType - Stores the reset type to perform. See the DEBUG_REBOOT_TYPE
        type.

--*/

typedef struct _DEBUG_REBOOT_REQUEST {
    ULONG ResetType;
} PACKED DEBUG_REBOOT_REQUEST, *PDEBUG_REBOOT_REQUEST;

//
// ----------------------------------------------- Internal Function Prototypes
//
