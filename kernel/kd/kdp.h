/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdp.h

Abstract:

    This header contains internal definitions for the kernel debugging
    subsystem.

Author:

    Evan Green 10-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Periodically check for incoming data when flying through single steps.
//

#define DEBUG_PERIODIC_BREAK_CHECK_MASK 0x1FF

//
// --------------------------------------------------------------------- Macros
//

#define KD_ASSERT_ONCE(_Expression)                       \
    if ((KdAsserted == FALSE) && (!(_Expression))) {      \
        KdAsserted = TRUE;                                \
        KdBreak();                                        \
    }                                                     \

//
// Define these trace macros to something to debug KD (I/O port out, video
// print, alternate UART, etc). See the KD_TRACE_EVENT and
// KD_DEVICE_TRACE_EVENT for possible values.
//

#define KD_TRACE(_KdTraceEvent)
#define KD_DEVICE_TRACE(_KdDeviceTraceEvent)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a module state change notification.

Members:

    Module - Supplies a pointer to the module that is being loaded or unloaded.

    Loading - Supplies a flag indicating whether the module is loading (TRUE)
        or unloading.

--*/

typedef struct _MODULE_CHANGE_NOTIFICATION {
    PDEBUG_MODULE Module;
    BOOL Loading;
} MODULE_CHANGE_NOTIFICATION, *PMODULE_CHANGE_NOTIFICATION;

typedef enum _KD_TRACE_EVENT {
    KdTraceExit,
    KdTraceInExceptionHandler,
    KdTraceDebuggingEnabled,
    KdTraceLockAcquired,
    KdTracePollBailing,
    KdTraceClearedSingleStep,
    KdTraceWaitingForFrozenProcessors,
    KdTraceProcessorsFrozen,
    KdTraceReceiveFailure,
    KdTraceProcessingCommand,
    KdTraceConnecting,
    KdTraceConnectBailing,
    KdTracePrinting,
    KdTraceSendingProfilingData,
    KdTraceModuleChange,
    KdTraceCheckSingleStep,
    KdTraceCommittingToBreak,
    KdTraceBailingUnconnected,
    KdTraceTransmitFailure,
    KdTraceThawingProcessors,
} KD_TRACE_EVENT, *PKD_TRACE_EVENT;

typedef enum _KD_DEVICE_TRACE_EVENT {
    KdDeviceTraceDisconnected,
    KdDeviceTraceResetting,
    KdDeviceTraceResetFailed,
    KdDeviceTraceResetComplete,
    KdDeviceTraceTransmitting,
    KdDeviceTraceTransmitFailed,
    KdDeviceTraceTransmitComplete,
    KdDeviceTraceReceiving,
    KdDeviceTraceReceiveFailed,
    KdDeviceTraceReceiveComplete,
    KdDeviceTraceGettingStatus,
    KdDeviceTraceGetStatusFailed,
    KdDeviceTraceGetStatusHasData,
    KdDeviceTraceGetStatusEmpty,
    KdDeviceTraceDisconnecting,
} KD_DEVICE_TRACE_EVENT, *PKD_DEVICE_TRACE_EVENT;

//
// -------------------------------------------------------------------- Globals
//

//
// Access some of the executive's variables directly. Though function calls are
// available, using them makes those functions undebuggable.
//

extern volatile ULONG KeActiveProcessorCount;

//
// Variable used for one-time assertions.
//

extern BOOL KdAsserted;

//
// Store the machine architecture.
//

extern ULONG KdMachineType;

//
// Store a variable indicating whether freeze request are maskable interrupts
// or NMIs.
//

extern BOOL KdFreezesAreMaskable;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
KdpUsbGetHandoffData (
    PDEBUG_HANDOFF_DATA Data
    );

/*++

Routine Description:

    This routine returns a pointer to the handoff data the USB driver needs to
    operate with a USB debug host controller.

Arguments:

    Data - Supplies a pointer where a pointer to the handoff data is returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_ELIGIBLE_DEVICES if there is no USB debug device.

--*/

ULONG
KdpValidateMemoryAccess (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    );

/*++

Routine Description:

    This routine validates that access to a specified location in memory will
    not cause a page fault.

Arguments:

    Address - Supplies the virtual address of the memory that will be read or
        written.

    Length - Supplies how many bytes at that location the caller would like to
        read or write.

    Writable - Supplies an optional pointer that receives a boolean indicating
        whether or not the memory range is mapped writable.

Return Value:

    Returns the number of bytes from the beginning of the address that are
    accessible. If the memory is completely available, the return value will be
    equal to the Length parameter. If the memory is completely paged out, 0
    will be returned.

--*/

VOID
KdpModifyAddressMapping (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    );

/*++

Routine Description:

    This routine modifies the mapping properties for the page that contains the
    given address.

Arguments:

    Address - Supplies the virtual address of the memory whose mapping
        properties are to be changed.

    Writable - Supplies a boolean indicating whether or not to make the page
        containing the address writable (TRUE) or read-only (FALSE).

    WasWritable - Supplies a pointer that receives a boolean indicating whether
        or not the page was writable (TRUE) or read-only (FALSE) before any
        modifications.

Return Value:

    None.

--*/

VOID
KdpInitializeDebuggingHardware (
    VOID
    );

/*++

Routine Description:

    This routine initializes any architecture dependent debugging hardware.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KdpBreak (
    VOID
    );

/*++

Routine Description:

    This routine causes a break into the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KdpInternalPrint (
    PCSTR Format,
    ...
    );

/*++

Routine Description:

    This routine prints to the debug client window. This routine can only be
    called from *inside* the debug exception handler.

Arguments:

    Format - Supplies the printf-style format string.

    ... - Supplies additional values as dictated by the format.

Return Value:

    None.

--*/

VOID
KdpClearSingleStepMode (
    PULONG Exception,
    PTRAP_FRAME TrapFrame,
    PVOID *PreviousSingleStepAddress
    );

/*++

Routine Description:

    This routine turns off single step mode.

Arguments:

    Exception - Supplies the type of exception that this function is handling.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

    PreviousSingleStepAddress - Supplies a pointer where the address the
        single step breakpoint was set will be returned, if a software-based
        single step mechanism is in use.

Return Value:

    None.

--*/

VOID
KdpSetSingleStepMode (
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PVOID SingleStepAddress
    );

/*++

Routine Description:

    This routine turns on single step mode.

Arguments:

    Exception - Supplies the type of exception that this function is handling.

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred. Also returns the possibly modified
        machine state.

    SingleStepAddress - Supplies an optional pointer where the breakpoint
        should be set. This is only used by software based single step
        mechanisms to restore a previously unset single step breakpoint. If
        this is NULL, then the next instruction will be calculated from the
        current trap frame.

Return Value:

    None.

--*/

VOID
KdpInvalidateInstructionCache (
    VOID
    );

/*++

Routine Description:

    This routine invalidates the instruction cache to PoU inner shareable.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
KdpCleanMemory (
    PVOID Address
    );

/*++

Routine Description:

    This routine cleans memory modified by the kernel debugger, flushing it
    out of the instruciton and data caches.

Arguments:

    Address - Supplies the address whose associated cache line will be
        cleaned.

Return Value:

    None.

--*/

PVOID
KdpGetInstructionPointer (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine returns the instruction pointer in the trap frame.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred.

Return Value:

    Returns the current instruction pointer.

--*/

PVOID
KdpGetInstructionPointerAddress (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine returns the memory address corresponding to the current
    instruction pointer.

Arguments:

    TrapFrame - Supplies a pointer to the state of the machine immediately
        before the debug exception occurred.

Return Value:

    Returns the current instruction pointer address.

--*/

VOID
KdpGetRegisters (
    PTRAP_FRAME TrapFrame,
    PVOID Registers
    );

/*++

Routine Description:

    This routine writes the register values from the trap frame into the
    debugger packet.

Arguments:

    TrapFrame - Supplies a pointer to the current processor state.

    Registers - Supplies a pointer where the register values will be written in
        for the debugger. For x86, this is a pointer to an
        X86_GENERAL_REGISTERS structure.

Return Value:

    None.

--*/

ULONG
KdpGetErrorCode (
    ULONG Exception,
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine gets the error code out of the trap frame.

Arguments:

    Exception - Supplies the exception that generated the error code.

    TrapFrame - Supplies a pointer to the current processor state.

Return Value:

    Returns the error code, or 0 if there was no error code.

--*/

VOID
KdpSetRegisters (
    PTRAP_FRAME TrapFrame,
    PVOID Registers
    );

/*++

Routine Description:

    This routine writes the register values from the debugger to the trap
    frame.

Arguments:

    TrapFrame - Supplies a pointer to the current processor state.

    Registers - Supplies a pointer to the new register values to use.

Return Value:

    None.

--*/

BOOL
KdpIsFunctionReturning (
    PTRAP_FRAME TrapFrame
    );

/*++

Routine Description:

    This routine will determine if the current instruction (the instruction
    about to get executed) is going to return from the current function.

Arguments:

    TrapFrame - Supplies a pointer to the current machine state.

Return Value:

    Returns TRUE if the function is about to return, or FALSE if this is not
    a return instruction.

--*/

VOID
KdpGetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION SpecialRegisters
    );

/*++

Routine Description:

    This routine retrieves the special registers from the current processor.

Arguments:

    SpecialRegisters - Supplies a pointer where the contents of the special
        registers will be returned.

Return Value:

    None.

--*/

VOID
KdpSetSpecialRegisters (
    PSPECIAL_REGISTERS_UNION OriginalRegisters,
    PSPECIAL_REGISTERS_UNION NewRegisters
    );

/*++

Routine Description:

    This routine sets the special registers from the current processor.

Arguments:

    OriginalRegisters - Supplies a pointer to the current special register
        context.

    NewRegisters - Supplies a pointer to the values to write. Only values
        different from the original registers will actually be written.

Return Value:

    None.

--*/

ULONG
KdpAtomicCompareExchange32 (
    volatile ULONG *Address,
    ULONG ExchangeValue,
    ULONG CompareValue
    );

/*++

Routine Description:

    This routine atomically compares memory at the given address with a value
    and exchanges it with another value if they are equal.

Arguments:

    Address - Supplies the address of the value to compare and potentially
        exchange.

    ExchangeValue - Supplies the value to write to Address if the comparison
        returns equality.

    CompareValue - Supplies the value to compare against.

Return Value:

    Returns the original value at the given address.

--*/

ULONG
KdpAtomicAdd32 (
    volatile ULONG *Address,
    ULONG Increment
    );

/*++

Routine Description:

    This routine atomically adds the given amount to a 32-bit variable.

Arguments:

    Address - Supplies the address of the value to atomically add to.

    Increment - Supplies the amount to add.

Return Value:

    Returns the value before the atomic addition was performed.

--*/

VOID
KdpDisableInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine disables all interrupts on the current processor.

Arguments:

    None.

Return Value:

    None.

--*/

