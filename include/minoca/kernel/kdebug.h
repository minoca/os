/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdebug.h

Abstract:

    This header contains definitions for the kernel debugging subsystem.

Author:

    Evan Green 3-Jul-2012

--*/

//
// ------------------------------------------------------------------ Constants
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time in milliseconds to wait for a connection before
// moving on.
//

#define DEBUG_CONNECTION_TIMEOUT (2 * 1000000)

#define DEBUG_DEFAULT_BAUD_RATE 115200

//
// Define the maximum device path size, which is the maximum hub depth that
// a KD USB device can be plugged in behind.
//

#define DEBUG_USB_DEVICE_PATH_SIZE 8

#define EXCEPTION_NMI                   0x02
#define EXCEPTION_BREAK                 0x03
#define EXCEPTION_SINGLE_STEP           0x04
#define EXCEPTION_ACCESS_VIOLATION      0x05
#define EXCEPTION_UNDEFINED_INSTRUCTION 0x06
#define EXCEPTION_ASSERTION_FAILURE     0x07
#define EXCEPTION_POLL_DEBUGGER         0x08
#define EXCEPTION_MODULE_CHANGE         0x09
#define EXCEPTION_PRINT                 0x0A
#define EXCEPTION_DIVIDE_BY_ZERO        0x0B
#define EXCEPTION_DOUBLE_FAULT          0x0C
#define EXCEPTION_PROFILER              0x0D
#define EXCEPTION_UNHANDLED_INTERRUPT   0x0E
#define EXCEPTION_USER_MODE             0x0F
#define EXCEPTION_DEBUGGER_DISCONNECT   0x10
#define EXCEPTION_DEBUGGER_CONNECT      0x11
#define EXCEPTION_MATH_FAULT            0x12

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores a list of loaded images.

Members:

    ModuleCount - Stores the number of modules in the list.

    Signature - Stores the total of all timestamps and loaded addresses in the
        module list.

    ModulesHead - Stores pointers to the first and last modules in the list.

--*/

typedef struct _DEBUG_MODULE_LIST {
    ULONG ModuleCount;
    ULONGLONG Signature;
    LIST_ENTRY ModulesHead;
} DEBUG_MODULE_LIST, *PDEBUG_MODULE_LIST;

/*++

Structure Description:

    This structure stores information about a loaded image.

Members:

    ListEntry - Stores pointers to the previous and next loaded modules.

    StructureSize - Stores the size of the structure, including the complete
        binary name string, in bytes.

    LowestAddress - Stores the lowest valid virtual address in the image. This
        can be above, below, or equal to the base address.

    Size - Stores the size of the image, in bytes, starting from the lowest
        address.

    Timestamp - Stores the file modification date in seconds since 2001.

    EntryPoint - Stores a pointer to the entry point of the image.

    Image - Stores a pointer to more detailed image information.

    Process - Stores the process ID of the process that this module is specific
        to.

    BinaryName - Stores the name of the binary. The allocated structure
        continues for the length of the string.

--*/

typedef struct _DEBUG_MODULE {
    LIST_ENTRY ListEntry;
    ULONG StructureSize;
    PVOID LowestAddress;
    ULONGLONG Size;
    ULONGLONG Timestamp;
    PVOID EntryPoint;
    PVOID Image;
    ULONG Process;
    CHAR BinaryName[ANYSIZE_ARRAY];
} DEBUG_MODULE, *PDEBUG_MODULE;

/*++

Structure Description:

    This structure stores the information required to hand off primary
    control of the debug device to the real USB drivers.

Members:

    DevicePathSize - Stores the number of valid elements in the device path.

    DevicePath - Stores the device path to the debug device as an array of
        port numbers off the root port. Each element represents a port number
        of the parent hub to the device.

    DeviceAddress - Stores the device address of the debug device.

    HubAddress - Stores the hub address of the debug device. If the debug
        device is high speed, this will be set to zero.

    Configuration - Stores the configuration value of the configuration the
        device is in.

    VendorId - Stores the vendor ID of the debug USB device.

    ProductId - Stores the product ID of the debug USB device.

    HostData - Stores a pointer to the host controller specific data.

    HostDataSize - Stores the size of the host controller data.

--*/

typedef struct _DEBUG_USB_HANDOFF_DATA {
    ULONG DevicePathSize;
    UCHAR DevicePath[DEBUG_USB_DEVICE_PATH_SIZE];
    UCHAR DeviceAddress;
    UCHAR HubAddress;
    UCHAR Configuration;
    USHORT VendorId;
    USHORT ProductId;
    PVOID HostData;
    ULONG HostDataSize;
} DEBUG_USB_HANDOFF_DATA, *PDEBUG_USB_HANDOFF_DATA;

/*++

Structure Description:

    This structure stores additional information about the UART debug device.

Members:

    OemData - Stores a pointer to OEM specific data.

    OemDataSize - Stores the size of the OEM data in bytes.

--*/

typedef struct _DEBUG_UART_HANDOFF_DATA {
    PVOID OemData;
    UINTN OemDataSize;
} DEBUG_UART_HANDOFF_DATA, *PDEBUG_UART_HANDOFF_DATA;

/*++

Structure Description:

    This structure stores the information required to describe the device
    currently in use by the kernel debugger.

Members:

    PortType - Stores the port type of the debug device as defined by the
        debug port table 2 specification.

    PortSubType - Stores the port subtype of the debug device as defined by the
        debug port table 2 specification.

    Identifier - Stores the unique identifier of the device, often its physical
        base address.

--*/

typedef struct _DEBUG_HANDOFF_DATA {
    USHORT PortType;
    USHORT PortSubType;
    ULONGLONG Identifier;
    union {
        DEBUG_USB_HANDOFF_DATA Usb;
        DEBUG_UART_HANDOFF_DATA Uart;
    } U;

} DEBUG_HANDOFF_DATA, *PDEBUG_HANDOFF_DATA;

//
// -------------------------------------------------------------------- Externs
//

extern DEBUG_MODULE_LIST KdLoadedModules;

//
// -------------------------------------------------------- Function Prototypes
//

KERNEL_API
VOID
KdConnect (
    VOID
    );

/*++

Routine Description:

    This routine connects to the kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
VOID
KdDisconnect (
    VOID
    );

/*++

Routine Description:

    This routine disconnects from the kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/

KERNEL_API
KSTATUS
KdGetDeviceInformation (
    PDEBUG_HANDOFF_DATA *Information
    );

/*++

Routine Description:

    This routine returns information about the debug device in use. This
    includes information identifying the device, OEM-specific data, and
    transport-specific data that may be needed to coordinate shared control
    between runtime drivers and the kernel debug subsystem.

Arguments:

    Information - Supplies a pointer where a pointer to the debug information
        will be returned on success. The caller must not modify or free this
        data.

Return Value:

    STATUS_SUCCESS if information was successfully returned.

    STATUS_NO_ELIGIBLE_DEVICES if no debug devices were found or used.

    Other status codes on other errors.

--*/

KSTATUS
KdInitialize (
    PDEBUG_DEVICE_DESCRIPTION DebugDevice,
    PDEBUG_MODULE CurrentModule
    );

/*++

Routine Description:

    This routine initializes the debugger subsystem and connects to the target
    if debugging is enabled.

Arguments:

    DebugDevice - Supplies a pointer to the debug device to communicate on.

    CurrentModule - Supplies the details of the initial running program.

Return Value:

    Kernel status code.

--*/

VOID
KdBreak (
    VOID
    );

/*++

Routine Description:

    This routine breaks into the debugger if one is connected.

Arguments:

    None.

Return Value:

    Kernel status code.

--*/

VOID
KdPrint (
    PCSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints a string to the debugger. Currently the maximum length
    string is a little less than one debug packet.

Arguments:

    Format - Supplies a pointer to the printf-like format string.

    ... - Supplies any needed arguments for the format string.

Return Value:

    None.

--*/

VOID
KdPrintWithArgumentList (
    PCSTR Format,
    va_list ArgumentList
    );

/*++

Routine Description:

    This routine prints a string to the debugger. Currently the maximum length
    string is a little less than one debug packet.

Arguments:

    Format - Supplies a pointer to the printf-like format string.

    ArgumentList - Supplies a pointer to the initialized list of arguments
        required for the format string.

Return Value:

    None.

--*/

VOID
KdReportModuleChange (
    PDEBUG_MODULE Module,
    BOOL Loading
    );

/*++

Routine Description:

    This routine informs the debugger of an image being loaded or unloaded.

Arguments:

    Module - Supplies a pointer to the module being loaded or unloaded. The
        caller is responsible for managing this memory. The memory should
        not be freed until after reporting that the module has unloaded.

    Loading - Supplies a flag indicating whether the module is being loaded or
        unloaded.

Return Value:

    None.

--*/

VOID
KdPollForBreakRequest (
    VOID
    );

/*++

Routine Description:

    This routine polls the debugger connection to determine if the debugger
    has requested to break in.

Arguments:

    None.

Return Value:

    None.

--*/

BOOL
KdIsDebuggerConnected (
    VOID
    );

/*++

Routine Description:

    This routine indicates whether or not a kernel debugger is currently
    connected to the system.

Arguments:

    None.

Return Value:

    TRUE if the debugger is enabled and connected.

    FALSE otherwise.

--*/

BOOL
KdAreUserModeExceptionsEnabled (
    VOID
    );

/*++

Routine Description:

    This routine indicates whether or not noteworthy exceptions caused in
    applications should bubble up to kernel mode debugger breaks.

Arguments:

    None.

Return Value:

    TRUE if user mode exceptions should bubble up to kernel mode.

    FALSE otherwise.

--*/

ULONG
KdSetConnectionTimeout (
    ULONG Timeout
    );

/*++

Routine Description:

    This routine sets the debugger connection timeout.

Arguments:

    Timeout - Supplies the new timeout in microseconds. Supply MAX_ULONG to
        cause the debugger to not call the stall function and never time out
        the connection.

Return Value:

    Returns the original timeout.

--*/

VOID
KdSendProfilingData (
    VOID
    );

/*++

Routine Description:

    This routine polls the system profiler to determine if there is profiling
    data to be sent to the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KdEnableNmiBroadcast (
    BOOL Enable
    );

/*++

Routine Description:

    This routine enables or disables the use of NMI broadcasts by the debugger.

Arguments:

    Enable - Supplies TRUE if NMI broadcast is being enabled, or FALSE if NMI
        broadcast is being disabled.

Return Value:

    None.

--*/

VOID
KdDebugExceptionHandler (
    ULONG Exception,
    PVOID Parameter,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine handles the debug break exception. It is usually called by an
    assembly routine responding to an exception.

Arguments:

    Exception - Supplies the type of exception that this function is handling.
        See EXCEPTION_* definitions for valid values.

    Parameter - Supplies a pointer to a parameter supplying additional
        context in the case of a debug service request.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

Return Value:

    None.

--*/

VOID
KdNmiHandler (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine handles NMI interrupts.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the NMI occurred. Also returns the possibly modified
        machine state.

Return Value:

    None.

--*/

VOID
KdNmiHandlerAsm (
    VOID
    );

/*++

Routine Description:

    This routine is called directly when an NMI occurs. Since it is a hardware
    task switch, no registers need to be saved.

Arguments:

    ReturnEip - Supplies the address after the instruction that caused the trap.

    ReturnCodeSelector - Supplies the code selector the code that trapped was
        running under.

    ReturnEflags - Supplies the EFLAGS register immediately before the trap.

Return Value:

    None.

--*/

