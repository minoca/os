/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kdebug.c

Abstract:

    This module implements the kernel debugging functionality.

Author:

    Evan Green 3-Jul-2012

Environment:

    Kernel mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include <minoca/debug/dbgproto.h>
#include "kdp.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the increment to stall for in microseconds when waiting for a packet.
//

#define DEBUG_STALL_INCREMENT 1000

//
// Define the amount of time to wait if just waiting a little bit, not the full
// stall increment yet.
//

#define DEBUG_SMALL_STALL 100

//
// Define the maximum size of the kernel module name.
//

#define MAX_KERNEL_MODULE_NAME 16

//
// Define the number of microseconds to wait for all other processors before
// declaring them lost and entering the debugger anyway.
//

#define DEBUG_PROCESSOR_WAIT_TIME (10 * MICROSECONDS_PER_SECOND)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a "range" breakpoint. This type of breakpoint will
    break on a range of addresses, with an optional "hole" within the range that
    will not cause a break. This type of breakpoint is very slow, as it puts
    the processor into single step mode and manually checks the range on every
    trap.

Members:

    Enabled - Stores a flag indicating whether or not the range breakpoint is
        current enabled. If disabled (FALSE), none of the other fields are
        guaranteed to be initialized.

    BreakRangeStart - Stores a pointer to the first byte of memory that
        qualifies as being in the break range.

    BreakRangeEnd - Stores a pointer to the first byte of memory that does not
        qualify as being in the break range.

    RangeHoleStart - Stores a pointer to the first byte within the range that
        does not generate a break (a "hole" in the break range).

    RangeHoleEnd - Stores a pointer to the first byte within the range that does
        not fall in the range hole (the first byte that again qualifies as
        within the break range).

--*/

typedef struct _BREAK_RANGE {
    BOOL Enabled;
    PVOID BreakRangeStart;
    PVOID BreakRangeEnd;
    PVOID RangeHoleStart;
    PVOID RangeHoleEnd;
} BREAK_RANGE, *PBREAK_RANGE;

/*++

Structure Description:

    This structure stores the set of parameters needed for a kernel debug print
    operation.

Members:

    FormatString - Stores a pointer to the printf format string to print.

    Arguments - Stores the list of arguments needed by the format string.

--*/

typedef struct _PRINT_PARAMETERS {
    PCSTR FormatString;
    va_list Arguments;
} PRINT_PARAMETERS, *PPRINT_PARAMETERS;

//
// -------------------------------------------------------------------- Globals
//

//
// Define globals indicating the initialization and connection state of Kd.
//

BOOL KdDebuggerConnected;
BOOL KdDebuggingEnabled = TRUE;
BOOL KdInitialized = FALSE;
volatile ULONG KdLockAcquired = -1;
volatile ULONG KdProcessorsFrozen = 0;
volatile ULONG KdFreezeOwner = MAX_ULONG;
BOOL KdNmiBroadcastAllowed = FALSE;

//
// Store a pointer to the debugger transport.
//

PDEBUG_DEVICE_DESCRIPTION KdDebugDevice;
DEBUG_HANDOFF_DATA KdHandoffData;

//
// Set this flag to allow notable user mode exceptions to bubble up into the
// kernel mode debugger.
//

BOOL KdEnableUserModeExceptions = TRUE;

//
// Set this flag to debug the time counter itself or situations where the time
// counter may not be accessible or reliable.
//

BOOL KdAvoidTimeCounter = FALSE;

//
// Set this flag to enable encoding of certain characters that might not fly
// across the wire well directly (like XON/XOFF).
//

BOOL KdEncodeBytes = FALSE;

//
// Variable used for one-time assertions.
//

BOOL KdAsserted = FALSE;

//
// Carve off some memory for sending and receiving debug packets, and storing
// the currently loaded modules.
//

DEBUG_PACKET KdTxPacket;
DEBUG_PACKET KdRxPacket;
DEBUG_MODULE_LIST KdLoadedModules;
BOOL KdLoadedModulesInitialized;

//
// Carve off memory to store the kernel module, including its string.
//

UCHAR KdKernelModuleBuffer[sizeof(DEBUG_MODULE) + MAX_KERNEL_MODULE_NAME];

//
// Store information about whether or not the user asked for a single step.
//

BOOL KdUserRequestedSingleStep;
BREAK_RANGE KdBreakRange;
UINTN KdPeriodicBreakInCheck;

//
// Variable used to determine whether or not memory validation should be
// skipped.
//

BOOL KdSkipMemoryValidation = FALSE;

//
// Define the amount of time to wait in microseconds to wait for a connection
// before moving on. Set to -1 to avoid using the stall function and wait
// indefinitely.
//

ULONG KdConnectionTimeout = DEBUG_CONNECTION_TIMEOUT;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
KdpSendPacket (
    PDEBUG_PACKET Packet,
    PBOOL BreakInRequested
    );

KSTATUS
KdpReceivePacket (
    PDEBUG_PACKET Packet,
    ULONG Timeout
    );

KSTATUS
KdpReceivePacketHeader (
    PDEBUG_PACKET_HEADER Packet,
    PULONG Timeout
    );

KSTATUS
KdpProcessCommand (
    PDEBUG_PACKET Packet,
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PBOOL ContinueExecution
    );

KSTATUS
KdpPrint (
    PPRINT_PARAMETERS PrintParameters,
    PBOOL BreakInRequested
    );

KSTATUS
KdpSendProfilingData (
    PBOOL BreakInRequested
    );

USHORT
KdpCalculateChecksum (
    PVOID Data,
    ULONG DataLength
    );

KSTATUS
KdpSendModuleList (
    BOOL SendHeaderOnly
    );

KSTATUS
KdpHandleMemoryAccess (
    VOID
    );

VOID
KdpInitializeBreakNotification (
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PDEBUG_PACKET Packet
    );

VOID
KdpReboot (
    DEBUG_REBOOT_TYPE RebootType,
    ULONG Exception,
    PTRAP_FRAME TrapFrame
    );

VOID
KdpDisconnect (
    VOID
    );

KSTATUS
KdpConnect (
    PBOOL BreakInRequested
    );

VOID
KdpSynchronize (
    VOID
    );

KSTATUS
KdpSendConnectionResponse (
    PCONNECTION_REQUEST ConnectionRequest,
    PBOOL BreakInRequested
    );

VOID
KdpAcquireDebuggerLock (
    PTRAP_FRAME TrapFrame
    );

VOID
KdpReleaseDebuggerLock (
    VOID
    );

KSTATUS
KdpTransmitBytes (
    PVOID Data,
    ULONG Size
    );

KSTATUS
KdpReceiveBuffer (
    PVOID Data,
    PULONG Size,
    PULONG Timeout
    );

KSTATUS
KdpDeviceReceiveBuffer (
    PVOID Data,
    PULONG Size,
    PULONG Timeout
    );

KSTATUS
KdpDeviceReset (
    ULONG BaudRate
    );

KSTATUS
KdpDeviceTransmit (
    PVOID Data,
    ULONG Size
    );

KSTATUS
KdpDeviceReceive (
    PVOID Data,
    PULONG Size
    );

KSTATUS
KdpDeviceGetStatus (
    PBOOL ReceiveDataAvailable
    );

VOID
KdpDeviceDisconnect (
    VOID
    );

VOID
KdpFreezeProcessors (
    VOID
    );

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
VOID
KdConnect (
    VOID
    )

/*++

Routine Description:

    This routine connects to the kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if ((KdInitialized == FALSE) || (KdDebuggingEnabled == FALSE)) {
        return;
    }

    RtlDebugService(EXCEPTION_DEBUGGER_CONNECT, NULL);
    return;
}

KERNEL_API
VOID
KdDisconnect (
    VOID
    )

/*++

Routine Description:

    This routine disconnects from the kernel debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

    if ((KdInitialized == FALSE) ||
        (KdDebuggingEnabled == FALSE) ||
        (KdDebuggerConnected == FALSE)) {

        return;
    }

    RtlDebugService(EXCEPTION_DEBUGGER_DISCONNECT, NULL);
    return;
}

KERNEL_API
KSTATUS
KdGetDeviceInformation (
    PDEBUG_HANDOFF_DATA *Information
    )

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

{

    KSTATUS Status;

    if (Information != NULL) {
        *Information = NULL;
    }

    if (KdDebugDevice == NULL) {
        return STATUS_NO_ELIGIBLE_DEVICES;
    }

    KdHandoffData.PortType = KdDebugDevice->PortType;
    KdHandoffData.PortSubType = KdDebugDevice->PortSubType;
    KdHandoffData.Identifier = KdDebugDevice->Identifier;
    if (KdHandoffData.PortType == DEBUG_PORT_TYPE_USB) {
        Status = KdpUsbGetHandoffData(&KdHandoffData);
        if (!KSUCCESS(Status)) {
            return Status;
        }
    }

    if (Information != NULL) {
        *Information = &KdHandoffData;
    }

    return STATUS_SUCCESS;
}

VOID
KdPrint (
    PCSTR Format,
    ...
    )

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

{

    va_list ArgumentList;

    va_start(ArgumentList, Format);
    KdPrintWithArgumentList(Format, ArgumentList);
    va_end(ArgumentList);
    return;
}

VOID
KdPrintWithArgumentList (
    PCSTR Format,
    va_list ArgumentList
    )

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

{

    PRINT_PARAMETERS PrintParameters;

    if ((KdDebuggingEnabled != FALSE) &&
        (KdDebuggerConnected != FALSE) &&
        (KdInitialized != FALSE)) {

        PrintParameters.FormatString = Format;
        va_copy(PrintParameters.Arguments, ArgumentList);
        RtlDebugService(EXCEPTION_PRINT, &PrintParameters);
        va_end(PrintParameters.Arguments);
    }

    return;
}

KSTATUS
KdInitialize (
    PDEBUG_DEVICE_DESCRIPTION DebugDevice,
    PDEBUG_MODULE CurrentModule
    )

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

{

    PSTR KernelBinaryName;
    PDEBUG_MODULE KernelModule;
    ULONG NameSize;
    KSTATUS Status;

    Status = STATUS_SUCCESS;
    if (DebugDevice != NULL) {
        KdDebugDevice = DebugDevice;
    }

    //
    // Set up the loaded modules list now if it has not been done.
    //

    if (KdLoadedModulesInitialized == FALSE) {
        KernelModule = (PDEBUG_MODULE)KdKernelModuleBuffer;
        KernelBinaryName = CurrentModule->BinaryName;

        //
        // Copy the name string into the kernel module structure.
        //

        NameSize = CurrentModule->StructureSize - sizeof(DEBUG_MODULE) +
                   (ANYSIZE_ARRAY * sizeof(CHAR));

        if (NameSize > MAX_KERNEL_MODULE_NAME) {
            NameSize = MAX_KERNEL_MODULE_NAME;
        }

        RtlStringCopy(KernelModule->BinaryName, KernelBinaryName, NameSize);
        KernelModule->StructureSize = CurrentModule->StructureSize;

        //
        // Fill out the rest of the kernel module information.
        //

        KernelModule->LowestAddress = CurrentModule->LowestAddress;
        KernelModule->Size = CurrentModule->Size;
        KernelModule->Timestamp = CurrentModule->Timestamp;
        KernelModule->Process = 0;

        //
        // Initialize the loaded modules list, inserting the kernel as the first
        // entry.
        //

        KdLoadedModules.ModuleCount = 1;
        KdLoadedModules.Signature = KernelModule->Timestamp +
                                    (UINTN)(KernelModule->LowestAddress);

        INITIALIZE_LIST_HEAD(&(KdLoadedModules.ModulesHead));
        INSERT_AFTER(&(KernelModule->ListEntry),
                     &(KdLoadedModules.ModulesHead));

        KdLoadedModulesInitialized = TRUE;
    }

    //
    // Initialize debugging hardware state.
    //

    KdpInitializeDebuggingHardware();

    //
    // Initialize other runtime globals.
    //

    KdBreakRange.Enabled = FALSE;
    KdUserRequestedSingleStep = FALSE;
    KdInitialized = TRUE;

    //
    // If debugging is not enabled, then initialization is finished.
    //

    if (KdDebuggingEnabled == FALSE) {
        Status = STATUS_SUCCESS;
        goto InitializeEnd;
    }

    //
    // Fire up a connection with the host.
    //

    KdConnect();

InitializeEnd:
    return Status;
}

VOID
KdBreak (
    VOID
    )

/*++

Routine Description:

    This routine breaks into the debugger if one is connected.

Arguments:

    None.

Return Value:

    Kernel status code.

--*/

{

    if ((KdDebuggingEnabled != FALSE) && (KdInitialized != FALSE)) {
        KdpBreak();
    }

    return;
}

VOID
KdReportModuleChange (
    PDEBUG_MODULE Module,
    BOOL Loading
    )

/*++

Routine Description:

    This routine informs the debugger of an image being loaded or unloaded.

Arguments:

    Module - Supplies a pointer to the module being loaded or unloaded. The
        caller is responsible for managing this memory. The memory should
        not be freed until after reporting that the module has unloaded. This
        memory must not be pagable.

    Loading - Supplies a flag indicating whether the module is being loaded or
        unloaded.

Return Value:

    None.

--*/

{

    MODULE_CHANGE_NOTIFICATION Notification;

    Notification.Module = Module;
    Notification.Loading = Loading;
    RtlDebugService(EXCEPTION_MODULE_CHANGE, &Notification);
    return;
}

VOID
KdPollForBreakRequest (
    VOID
    )

/*++

Routine Description:

    This routine polls the debugger connection to determine if the debugger
    has requested to break in.

Arguments:

    None.

Return Value:

    None.

--*/

{

    //
    // If debugging is not enabled, then this shouldn't execute. This does
    // run even if the debugger is disconnected in case the debugger is trying
    // to connect.
    //

    if ((KdInitialized == FALSE) || (KdDebuggingEnabled == FALSE)) {
        return;
    }

    RtlDebugService(EXCEPTION_POLL_DEBUGGER, NULL);
    return;
}

BOOL
KdIsDebuggerConnected (
    VOID
    )

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

{

    return KdDebuggerConnected;
}

BOOL
KdAreUserModeExceptionsEnabled (
    VOID
    )

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

{

    return KdEnableUserModeExceptions;
}

ULONG
KdSetConnectionTimeout (
    ULONG Timeout
    )

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

{

    return RtlAtomicExchange32(&KdConnectionTimeout, Timeout);
}

VOID
KdSendProfilingData (
    VOID
    )

/*++

Routine Description:

    This routine polls the system profiler to determine if there is profiling
    data to be sent to the debugger.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG Flags;

    //
    // If debugging is not enabled, then this shouldn't execute.
    //

    if ((KdInitialized == FALSE) || (KdDebuggingEnabled == FALSE)) {
        return;
    }

    //
    // If the debugger's not actually connected, just poll for a connection
    // request.
    //

    Flags = 0;
    if (KdDebuggerConnected != FALSE) {
        Flags = SpGetProfilerDataStatus();
    }

    if (Flags == 0) {
        KdPollForBreakRequest();

    } else {
        RtlDebugService(EXCEPTION_PROFILER, NULL);
    }

    return;
}

VOID
KdEnableNmiBroadcast (
    BOOL Enable
    )

/*++

Routine Description:

    This routine enables or disables the use of NMI broadcasts by the debugger.

Arguments:

    Enable - Supplies TRUE if NMI broadcast is being enabled, or FALSE if NMI
        broadcast is being disabled.

Return Value:

    None.

--*/

{

    KdNmiBroadcastAllowed = Enable;
    return;
}

VOID
KdDebugExceptionHandler (
    ULONG Exception,
    PVOID Parameter,
    PTRAP_FRAME TrapFrame
    )

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

{

    BOOL BreakInRequested;
    BOOL ContinueExecution;
    ULONG FreezeOwner;
    BOOL FunctionReturning;
    BOOL InsideRange;
    ULONGLONG InstructionPointer;
    PMODULE_CHANGE_NOTIFICATION Notification;
    BOOL OutsideHole;
    PVOID PreviousSingleStepAddress;
    PPROCESSOR_BLOCK ProcessorBlock;
    BOOL ReceiveDataAvailable;
    BOOL SingleStepHandled;
    KSTATUS Status;

    BreakInRequested = FALSE;
    PreviousSingleStepAddress = NULL;
    ProcessorBlock = NULL;
    SingleStepHandled = FALSE;
    KD_TRACE(KdTraceInExceptionHandler);

    //
    // If debugging is not enabled, then this shouldn't execute.
    //

    if ((KdInitialized == FALSE) || (KdDebuggingEnabled == FALSE)) {
        return;
    }

    KD_TRACE(KdTraceDebuggingEnabled);

    //
    // If the exception is a user mode trap, use the trap frame provided by the
    // parameter and treat it as a single step.
    //

    if (Exception == EXCEPTION_USER_MODE) {
        TrapFrame = (PTRAP_FRAME)Parameter;
        Exception = EXCEPTION_BREAK;
    }

    //
    // Disable interrupts. They will get re-enabled by popping flags on the
    // return from this exception. Also acquire the lock so that two processors
    // aren't using the debug port at once.
    //

    KdpDisableInterrupts();
    KdpAcquireDebuggerLock(TrapFrame);
    KdpAtomicAdd32(&KdProcessorsFrozen, 1);
    KD_TRACE(KdTraceLockAcquired);

    //
    // If this is just a poll, check for received bytes before bothering to
    // freeze everyone. Chances are there's nothing.
    //

    if (Exception == EXCEPTION_POLL_DEBUGGER) {
        Status = KdpDeviceGetStatus(&ReceiveDataAvailable);
        if ((!KSUCCESS(Status)) || (ReceiveDataAvailable == FALSE)) {
            KD_TRACE(KdTracePollBailing);
            goto DebugExceptionHandlerEnd;
        }
    }

    //
    // Clear single step mode super early to minimize the chance of infinite
    // breakpoints if using a software-based single step mechanism. Remember to
    // put it back if this routine shortcuts to the exit.
    //

    KdpClearSingleStepMode(&Exception, TrapFrame, &PreviousSingleStepAddress);
    KD_TRACE(KdTraceClearedSingleStep);
    if (KeActiveProcessorCount > 1) {
        ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
        if (KdNmiBroadcastAllowed != FALSE) {
            FreezeOwner = KdpAtomicCompareExchange32(
                                               &KdFreezeOwner,
                                               ProcessorBlock->ProcessorNumber,
                                               MAX_ULONG);

            if (FreezeOwner != MAX_ULONG) {
                KdpReleaseDebuggerLock();

                KD_ASSERT_ONCE(FALSE);

                KdpAcquireDebuggerLock(TrapFrame);
            }

            //
            // Send an NMI to freeze all other processors, and wait until all
            // processors are frozen. Skip this when sending profiling data to
            // allow higher throughput.
            //

            if (Exception != EXCEPTION_PROFILER) {
                KdpFreezeProcessors();
            }

        //
        // There are more than one processors, but for whatever reason the
        // debugger should not be trying to freeze them. Set the freeze owner
        // to the current processor so it acts like the owner.
        //

        } else {
            KdFreezeOwner = ProcessorBlock->ProcessorNumber;
        }

    //
    // If there's only one processor, don't even get the processor block
    // (as it may not be there), just set the freeze owner.
    //

    } else {
        KdFreezeOwner = 0;
    }

    KD_TRACE(KdTraceProcessorsFrozen);

    //
    // If the exception is a result of polling, find out what's being sent.
    // Process this possible exception first because it's super common.
    //

    if (Exception == EXCEPTION_POLL_DEBUGGER) {
        while (TRUE) {
            Status = KdpDeviceGetStatus(&ReceiveDataAvailable);
            if ((!KSUCCESS(Status)) || (ReceiveDataAvailable == FALSE)) {
                KD_TRACE(KdTracePollBailing);
                break;
            }

            Status = KdpReceivePacket(&KdRxPacket, KdConnectionTimeout);
            if (!KSUCCESS(Status)) {
                KD_TRACE(KdTraceReceiveFailure);
                if (Status == STATUS_CONNECTION_RESET) {
                    Exception = EXCEPTION_DEBUGGER_CONNECT;
                }

                break;
            }

            KD_TRACE(KdTraceProcessingCommand);
            if (KdRxPacket.Header.Command == DbgBreakRequest) {
                Exception = EXCEPTION_BREAK;
                KdDebuggerConnected = TRUE;

            } else {
                ContinueExecution = FALSE;
                Status = KdpProcessCommand(&KdRxPacket,
                                           Exception,
                                           TrapFrame,
                                           &ContinueExecution);

                if (!KSUCCESS(Status)) {
                    break;
                }

                if (ContinueExecution == FALSE) {
                    Exception = EXCEPTION_BREAK;
                }
            }
        }

        if (Exception == EXCEPTION_POLL_DEBUGGER) {
            goto DebugExceptionHandlerEnd;
        }

    //
    // If the exception was a disconnect request, do the disconnect and return.
    //

    } else if (Exception == EXCEPTION_DEBUGGER_DISCONNECT) {
        KdpDisconnect();
        goto DebugExceptionHandlerEnd;

    //
    // If the exception was just a print, do the print and return.
    //

    } else if (Exception == EXCEPTION_PRINT) {
        KD_TRACE(KdTracePrinting);
        KdpPrint((PPRINT_PARAMETERS)Parameter, &BreakInRequested);
        if (BreakInRequested == FALSE) {
            goto DebugExceptionHandlerEnd;
        }

        Exception = EXCEPTION_BREAK;

    //
    // If the exception is just to send profiling data, send the data and
    // return.
    //

    } else if (Exception == EXCEPTION_PROFILER) {
        KD_TRACE(KdTraceSendingProfilingData);
        KdpSendProfilingData(&BreakInRequested);
        if (BreakInRequested == FALSE) {
            goto DebugExceptionHandlerEnd;
        }

        //
        // The other cores were not frozen above. Attempt to freeze them now
        // that the debugger needs break in.
        //

        KdpFreezeProcessors();
        Exception = EXCEPTION_BREAK;

    //
    // If the exception was a module state change, update the module
    // information and return.
    //

    } else if (Exception == EXCEPTION_MODULE_CHANGE) {
        KD_TRACE(KdTraceModuleChange);
        Notification = (PMODULE_CHANGE_NOTIFICATION)Parameter;
        if (Notification->Loading == FALSE) {
            KdLoadedModules.ModuleCount -= 1;
            KdLoadedModules.Signature -=
                                  Notification->Module->Timestamp +
                                  (UINTN)(Notification->Module->LowestAddress);

            LIST_REMOVE(&(Notification->Module->ListEntry));

        } else {
            KdLoadedModules.ModuleCount += 1;
            KdLoadedModules.Signature +=
                                  Notification->Module->Timestamp +
                                  (UINTN)(Notification->Module->LowestAddress);

            INSERT_AFTER(&(Notification->Module->ListEntry),
                         &(KdLoadedModules.ModulesHead));
        }

        goto DebugExceptionHandlerEnd;
    }

    //
    // If the exception is a connection request or one of the above exceptions
    // noticed that the host needed to be reconnected, run through the
    // connection process.
    //

    if (Exception == EXCEPTION_DEBUGGER_CONNECT) {
        KD_TRACE(KdTraceConnecting);
        Status = KdpConnect(&BreakInRequested);
        if ((KSUCCESS(Status)) && (BreakInRequested != FALSE)) {
            Exception = EXCEPTION_BREAK;

        } else {
            KD_TRACE(KdTraceConnectBailing);
            goto DebugExceptionHandlerEnd;
        }
    }

    KD_TRACE(KdTraceCheckSingleStep);

    //
    // There are no more shortcuts out of this function, this is going to make
    // it to the user. Don't feel responsible for restoring the cleared single
    // step, as the remainder of the routine will decide whether or not to set
    // a new one.
    //

    PreviousSingleStepAddress = NULL;

    //
    // If the user requested this single step or this is not a single step,
    // cancel the range breakpoint.
    //

    if ((KdUserRequestedSingleStep != FALSE) ||
        (Exception != EXCEPTION_SINGLE_STEP)) {

        KdBreakRange.Enabled = FALSE;
    }

    //
    // If the range breakpoint is active and the user didn't request a single
    // step, then the only purpose of this step is to validate the break range.
    //

    InstructionPointer = (UINTN)KdpGetInstructionPointer(TrapFrame);
    if ((KdBreakRange.Enabled != FALSE) &&
        (Exception == EXCEPTION_SINGLE_STEP)) {

        InsideRange =
                 (InstructionPointer >= (UINTN)KdBreakRange.BreakRangeStart) &&
                 (InstructionPointer < (UINTN)KdBreakRange.BreakRangeEnd);

        OutsideHole =
                   (InstructionPointer < (UINTN)KdBreakRange.RangeHoleStart) ||
                   (InstructionPointer >= (UINTN)KdBreakRange.RangeHoleEnd);

        if ((OutsideHole != FALSE) && (InsideRange != FALSE)) {

            //
            // This instruction fits the range description. Change the exception
            // to a break and turn off the trap flag. This turns it into a real
            // exception that will notify the debugger.
            //

            Exception = EXCEPTION_BREAK;

        //
        // This instruction does not fit the range. Continue single stepping.
        //

        } else {
            KdpSetSingleStepMode(Exception, TrapFrame, NULL);
            SingleStepHandled = TRUE;
            KdPeriodicBreakInCheck += 1;
            if ((KdPeriodicBreakInCheck &
                 DEBUG_PERIODIC_BREAK_CHECK_MASK) == 0) {

                Status = KdpDeviceGetStatus(&ReceiveDataAvailable);
                if ((KSUCCESS(Status)) || (ReceiveDataAvailable != FALSE)) {
                    SingleStepHandled = FALSE;
                }
            }

            //
            // If this instruction is a function return, check to see if it
            // matches the range (but is in the hole). If so, expand the range
            // to the whole address range so the next step won't miss the fact
            // that the function just returned.
            //

            FunctionReturning = KdpIsFunctionReturning(TrapFrame);
            if ((InsideRange != FALSE) && (FunctionReturning != FALSE)) {
                KdBreakRange.BreakRangeStart = 0;
                KdBreakRange.BreakRangeEnd = (PVOID)MAX_ULONG;
            }
        }
    }

    //
    // If the user did not request a single step exception, and it was handled
    // above, then there is no reason to communicate with the debugger.
    //

    if ((Exception == EXCEPTION_SINGLE_STEP) &&
        (SingleStepHandled != FALSE)) {

        goto DebugExceptionHandlerEnd;
    }

    KD_TRACE(KdTraceCommittingToBreak);

    //
    // Unless it isn't connected, this break is going to make it to the
    // debugger. Turn off the break range.
    //

    KdBreakRange.Enabled = FALSE;

    //
    // Now that essential business is taken care of, check to see if there's a
    // debugger to talk to. If not, return.
    //

    if (KdDebuggerConnected == FALSE) {
        KD_TRACE(KdTraceBailingUnconnected);
        goto DebugExceptionHandlerEnd;
    }

    while (TRUE) {
        if (KdFreezeOwner == MAX_ULONG) {
            break;
        }

        if ((KeActiveProcessorCount > 1) &&
            (KdFreezeOwner != ProcessorBlock->ProcessorNumber)) {

            continue;
        }

        //
        // Loop processing commands and handling connection resets. Also send a
        // break request the first time around the loop and if the connection
        // was reset.
        //

        BreakInRequested = TRUE;
        KD_TRACE(KdTraceProcessingCommand);
        ContinueExecution = FALSE;
        do {
            if (BreakInRequested != FALSE) {
                KdpInitializeBreakNotification(Exception,
                                               TrapFrame,
                                               &KdTxPacket);

                Status = KdpSendPacket(&KdTxPacket, NULL);
                if (!KSUCCESS(Status)) {
                    KD_TRACE(KdTraceTransmitFailure);
                    goto DebugExceptionHandlerEnd;
                }

                BreakInRequested = FALSE;
            }

            Status = KdpReceivePacket(&KdRxPacket, MAX_ULONG);
            if (!KSUCCESS(Status)) {
                continue;
            }

            Status = KdpProcessCommand(&KdRxPacket,
                                       Exception,
                                       TrapFrame,
                                       &ContinueExecution);

            if (!KSUCCESS(Status)) {
                break;
            }

        } while (ContinueExecution == FALSE);
    }

DebugExceptionHandlerEnd:
    KD_TRACE(KdTraceThawingProcessors);
    if (PreviousSingleStepAddress != NULL) {
        KdpSetSingleStepMode(Exception, TrapFrame, PreviousSingleStepAddress);
    }

    KdpAtomicCompareExchange32(&KdFreezeOwner, MAX_ULONG, KdFreezeOwner);

    //
    // Wait until all processors are ready to go, then go.
    //

    KdpAtomicAdd32(&KdProcessorsFrozen, -1);
    while (KdProcessorsFrozen != 0) {
        NOTHING;
    }

    KdpReleaseDebuggerLock();
    KD_TRACE(KdTraceExit);
    return;
}

VOID
KdNmiHandler (
    PTRAP_FRAME TrapFrame
    )

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

{

    BOOL ContinueExecution;
    PPROCESSOR_BLOCK ProcessorBlock;
    KSTATUS Status;

    KdpDisableInterrupts();
    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();

    //
    // If there is no freeze owner or no processors are frozen, this may be a
    // real NMI. Crash here unless NMIs are maskable.
    //

    if ((KdFreezeOwner == MAX_ULONG) ||
        (KdProcessorsFrozen == 0) ||
        (KdProcessorsFrozen >= KeActiveProcessorCount)) {

        ASSERT(KdFreezesAreMaskable != FALSE);

        return;
    }

    //
    // Let the freeze owner know that this processor is frozen and listening.
    //

    KdpAtomicAdd32(&KdProcessorsFrozen, 1);

    //
    // Spin waiting for the freeze owner to become this processor or signal to
    // continue execution.
    //

    while (TRUE) {

        //
        // If the freeze owner becomes -1, break out and continue execution.
        //

        if (KdFreezeOwner == MAX_ULONG) {
            break;
        }

        //
        // If the freeze owner becomes this processor, take over and start
        // processing commands. Report this as a regular break exception to
        // distinguish between actual NMIs and debug IPIs.
        //

        if (KdFreezeOwner == ProcessorBlock->ProcessorNumber) {
            KdpInitializeBreakNotification(EXCEPTION_BREAK,
                                           TrapFrame,
                                           &KdTxPacket);

            Status = KdpSendPacket(&KdTxPacket, NULL);
            if (!KSUCCESS(Status)) {
                continue;
            }

            //
            // Loop processing commands.
            //

            ContinueExecution = FALSE;
            do {
                Status = KdpReceivePacket(&KdRxPacket, MAX_ULONG);
                if (!KSUCCESS(Status)) {
                    continue;
                }

                Status = KdpProcessCommand(&KdRxPacket,
                                           EXCEPTION_NMI,
                                           TrapFrame,
                                           &ContinueExecution);

                if (!KSUCCESS(Status)) {
                    break;
                }

            } while (ContinueExecution == FALSE);
        }
    }

    KdpInvalidateInstructionCache();

    //
    // Indicate that this processor is back on its way, and go.
    //

    KdpAtomicAdd32(&KdProcessorsFrozen, -1);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
KdpSendPacket (
    PDEBUG_PACKET Packet,
    PBOOL BreakInRequested
    )

/*++

Routine Description:

    This routine sends a packet across the wire to the debugging client.

Arguments:

    Packet - Supplies a pointer to the debug packet. The checksum field will be
        calculated in this function.

    BreakInRequested - Supplies an optional pointer where a boolean will be
        returned indicating if the debugger would like to break in.

Return Value:

    Status code.

--*/

{

    DEBUG_PACKET_ACKNOWLEDGE Acknowledge;
    USHORT Checksum;
    DEBUG_PACKET_HEADER Header;
    ULONG HeaderSize;
    ULONG PayloadSize;
    ULONG Retries;
    KSTATUS Status;
    ULONG Timeout;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    if (BreakInRequested != NULL) {
        *BreakInRequested = FALSE;
    }

    if ((KdInitialized == FALSE) || (KdDebuggingEnabled == FALSE)) {
        return STATUS_NOT_INITIALIZED;
    }

    if (Packet->Header.PayloadSize > DEBUG_PACKET_SIZE - HeaderSize) {
        return STATUS_INVALID_PARAMETER;
    }

    Packet->Header.Magic = DEBUG_PACKET_MAGIC;
    Packet->Header.PayloadSizeComplement = ~(Packet->Header.PayloadSize);
    Packet->Header.Checksum = 0;
    Checksum = KdpCalculateChecksum(Packet,
                                    Packet->Header.PayloadSize + HeaderSize);

    Packet->Header.Checksum = Checksum;

    //
    // Loop sending the data until an ackowledgement or rejection is received.
    //

    Timeout = KdConnectionTimeout;
    Retries = 10;
    while (Retries > 0) {
        Status = KdpTransmitBytes(Packet,
                                  HeaderSize + Packet->Header.PayloadSize);

        if (!KSUCCESS(Status)) {
            Retries -= 1;
            continue;
        }

        Status = KdpReceivePacketHeader(&Header, &Timeout);
        if ((Status == STATUS_TIMEOUT) || (Status == STATUS_CONNECTION_RESET)) {
            break;
        }

        if (!KSUCCESS(Status)) {
            Retries -= 1;
            continue;
        }

        if (Header.Command == DbgPacketAcknowledge) {

            //
            // Attempt to read the payload, which says whether or not the
            // debugger would like to break in.
            //

            if (Header.PayloadSize == sizeof(DEBUG_PACKET_ACKNOWLEDGE)) {
                PayloadSize = sizeof(DEBUG_PACKET_ACKNOWLEDGE);
                Status = KdpReceiveBuffer(&Acknowledge, &PayloadSize, &Timeout);
                if ((KSUCCESS(Status)) && (BreakInRequested != NULL)) {
                    *BreakInRequested = Acknowledge.BreakInRequested;
                }
            }

            Status = STATUS_SUCCESS;
            break;

        } else if (Header.Command != DbgPacketResend) {
            Status = STATUS_CONNECTION_RESET;
            break;
        }

        Retries -= 1;
    }

    //
    // If the receive timed out or the connection was reset, mark the
    // connection terminated. Don't send sync bytes if not connected, as that's
    // counterproductive during connect.
    //

    if ((Status == STATUS_TIMEOUT) || (Status == STATUS_CONNECTION_RESET)) {
        if (KdDebuggerConnected != FALSE) {
            KdpSynchronize();
            KdDebuggerConnected = FALSE;
        }
    }

    return Status;
}

KSTATUS
KdpReceivePacket (
    PDEBUG_PACKET Packet,
    ULONG Timeout
    )

/*++

Routine Description:

    This routine receives a packet across the wire from the debugger.

Arguments:

    Packet - Supplies a pointer to the buffer that will receive the debug
        packet.

    Timeout - Supplies the timeout in microseconds that this function should
        give up in.

Return Value:

    Status code.

--*/

{

    DEBUG_PACKET_HEADER Acknowledge;
    USHORT CalculatedChecksum;
    USHORT Checksum;
    USHORT HeaderChecksum;
    ULONG HeaderSize;
    ULONG PayloadSize;
    ULONG Retries;
    KSTATUS Status;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    Retries = 10;
    while (TRUE) {
        Status = KdpReceivePacketHeader(&(Packet->Header), &Timeout);
        if ((Status == STATUS_TIMEOUT) || (Status == STATUS_CONNECTION_RESET)) {
            break;
        }

        if (!KSUCCESS(Status)) {
            goto ReceivePacketRetry;
        }

        //
        // If the packet has a payload, get that as well.
        //

        if (Packet->Header.PayloadSize != 0) {
            PayloadSize = Packet->Header.PayloadSize;
            Status = KdpReceiveBuffer(&(Packet->Payload),
                                      &PayloadSize,
                                      &Timeout);

            if (!KSUCCESS(Status)) {
                if (Status == STATUS_TIMEOUT) {
                    break;
                }

                goto ReceivePacketRetry;
            }
        }

        //
        // Ensure that the packet came across okay. The checksum field is not
        // included in the checksum calculation, so zero it out while
        // calculating.
        //

        HeaderChecksum = Packet->Header.Checksum;
        Packet->Header.Checksum = 0;
        CalculatedChecksum = KdpCalculateChecksum(
                                      Packet,
                                      HeaderSize + Packet->Header.PayloadSize);

        Packet->Header.Checksum = HeaderChecksum;
        if (HeaderChecksum != CalculatedChecksum) {
            Status = STATUS_CHECKSUM_MISMATCH;
            goto ReceivePacketRetry;
        }

        //
        // Send the acknowledge and break.
        //

        RtlZeroMemory(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        Acknowledge.Magic = DEBUG_PACKET_MAGIC;
        Acknowledge.Command = DbgPacketAcknowledge;
        Acknowledge.PayloadSizeComplement = ~(Acknowledge.PayloadSize);
        Checksum = KdpCalculateChecksum(&Acknowledge,
                                        sizeof(DEBUG_PACKET_HEADER));

        Acknowledge.Checksum = Checksum;
        Status = KdpTransmitBytes(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        if (!KSUCCESS(Status)) {
            goto ReceivePacketRetry;
        }

        Status = STATUS_SUCCESS;
        break;

ReceivePacketRetry:
        if (Retries == 0) {
            break;
        }

        //
        // Ask the host to resend and loop.
        //

        RtlZeroMemory(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        Acknowledge.Command = DbgPacketResend;
        Acknowledge.PayloadSizeComplement = ~(Acknowledge.PayloadSize);
        Checksum = KdpCalculateChecksum(&Acknowledge,
                                        sizeof(DEBUG_PACKET_HEADER));

        Acknowledge.Checksum = Checksum;
        Status = KdpTransmitBytes(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        if (!KSUCCESS(Status)) {
            break;
        }

        Retries -= 1;
    }

    //
    // If the receive timed out or the connection was reset, mark the
    // connection terminated. Don't send sync bytes if not connected, as that's
    // counterproductive during connect.
    //

    if ((Status == STATUS_TIMEOUT) || (Status == STATUS_CONNECTION_RESET)) {
        if (KdDebuggerConnected != FALSE) {
            KdpSynchronize();
            KdDebuggerConnected = FALSE;
        }
    }

    return Status;
}

KSTATUS
KdpReceivePacketHeader (
    PDEBUG_PACKET_HEADER Packet,
    PULONG Timeout
    )

/*++

Routine Description:

    This routine receives a packet header across the wire from the debugger.

Arguments:

    Packet - Supplies a pointer to the buffer that will receive the debug
        packet header.

    Timeout - Supplies a pointer that on input contains the number of
        microseconds to wait before timing out. On output, will contain the
        remainder of that wait.

Return Value:

    Status code.

--*/

{

    DEBUG_PACKET_HEADER Acknowledge;
    USHORT Checksum;
    ULONG HeaderSize;
    BYTE Magic;
    PBYTE ReceiveBuffer;
    ULONG ReceiveSize;
    ULONG Retries;
    KSTATUS Status;

    HeaderSize = sizeof(DEBUG_PACKET_HEADER);
    Retries = 10;
    Status = STATUS_SUCCESS;
    while (TRUE) {

        //
        // Attempt to synchronize on the magic field.
        //

        Magic = 0;
        ReceiveSize = sizeof(BYTE);
        Status = KdpReceiveBuffer(&Magic, &ReceiveSize, Timeout);
        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            goto ReceivePacketRetry;
        }

        if (Magic != DEBUG_PACKET_MAGIC_BYTE1) {

            //
            // If this was a resync byte from the host, then report the
            // connection as reset.
            //

            if (Magic == DEBUG_SYNCHRONIZE_HOST) {
                Status = STATUS_CONNECTION_RESET;
                break;
            }

            continue;
        }

        Magic = 0;
        ReceiveSize = sizeof(BYTE);
        Status = KdpReceiveBuffer(&Magic, &ReceiveSize, Timeout);
        if (Status == STATUS_TIMEOUT) {
            break;
        }

        if (!KSUCCESS(Status)) {
            goto ReceivePacketRetry;
        }

        if (Magic != DEBUG_PACKET_MAGIC_BYTE2) {
            continue;
        }

        //
        // Get the packet header. Sometimes this is all that's required.
        //

        Packet->Magic = DEBUG_PACKET_MAGIC;
        ReceiveSize = HeaderSize - DEBUG_PACKET_MAGIC_SIZE;
        ReceiveBuffer = (PBYTE)Packet + DEBUG_PACKET_MAGIC_SIZE;
        Status = KdpReceiveBuffer(ReceiveBuffer, &ReceiveSize, Timeout);
        if (!KSUCCESS(Status)) {
            goto ReceivePacketRetry;
        }

        //
        // Validate that the payload size is reasonable by checking its
        // complement against the header.
        //

        if ((USHORT)~(Packet->PayloadSize) != Packet->PayloadSizeComplement) {
            continue;
        }

        if (Packet->PayloadSize > DEBUG_PACKET_SIZE - HeaderSize) {
            Status = STATUS_INVALID_PARAMETER;
            goto ReceivePacketRetry;
        }

        Status = STATUS_SUCCESS;
        break;

ReceivePacketRetry:
        if (Retries == 0) {
            break;
        }

        //
        // Ask the host to resend and loop.
        //

        RtlZeroMemory(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        Acknowledge.Command = DbgPacketResend;
        Acknowledge.PayloadSizeComplement = ~(Acknowledge.PayloadSize);
        Checksum = KdpCalculateChecksum(&Acknowledge,
                                        sizeof(DEBUG_PACKET_HEADER));

        Acknowledge.Checksum = Checksum;
        Status = KdpTransmitBytes(&Acknowledge, sizeof(DEBUG_PACKET_HEADER));
        if (!KSUCCESS(Status)) {
            break;
        }

        Retries -= 1;
    }

    return Status;
}

KSTATUS
KdpProcessCommand (
    PDEBUG_PACKET Packet,
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PBOOL ContinueExecution
    )

/*++

Routine Description:

    This routine processes a received debug packet.

Arguments:

    Packet - Supplies a pointer to the complete packet.

    Exception - Supplies the exception that caused the break.

    TrapFrame - Supplies a pointer to the machine's current context.

    ContinueExecution - Supplies a pointer that receives whether or not the
        debugger should continue execution or process more packets.

Return Value:

    Status code.

--*/

{

    BOOL BreakInRequested;
    PRANGE_STEP RangeStep;
    PDEBUG_REBOOT_REQUEST RebootRequest;
    PSET_SPECIAL_REGISTERS SetSpecialRegisters;
    KSTATUS Status;
    PSWITCH_PROCESSOR_REQUEST SwitchProcessorRequest;

    Status = STATUS_SUCCESS;

    //
    // By default, most commands don't result in continued execution.
    //

    *ContinueExecution = FALSE;
    switch (KdRxPacket.Header.Command) {

    //
    // Re-send the break notification if needed.
    //

    case DbgBreakRequest:
        KdpInitializeBreakNotification(Exception,
                                       TrapFrame,
                                       &KdTxPacket);

        Status = KdpSendPacket(&KdTxPacket, NULL);
        break;

    //
    // The "go" command continues execution. Turn single stepping off in this
    // case.
    //

    case DbgCommandGo:
        KdUserRequestedSingleStep = FALSE;

        //
        // Signal to the caller to break out of the command processing loop,
        // and signal to all other processors to continue execution.
        //

        *ContinueExecution = TRUE;
        KdFreezeOwner = MAX_ULONG;
        break;

    //
    // The single step command is like "go", but turns on the trap flag so that
    // the next instruction to execute will also break into the debugger.
    //

    case DbgCommandSingleStep:
        KdpSetSingleStepMode(Exception, TrapFrame, NULL);
        KdUserRequestedSingleStep = TRUE;
        *ContinueExecution = TRUE;
        KdFreezeOwner = MAX_ULONG;
        break;

    //
    // The range step command puts the machine into single step mode. At every
    // single step, it checks to see if the instruction pointer is within a
    // certain range, and breaks if so.
    //

    case DbgCommandRangeStep:
        RangeStep = (PRANGE_STEP)Packet->Payload;
        KdBreakRange.BreakRangeStart =
                                    (PVOID)(UINTN)RangeStep->BreakRangeMinimum;

        KdBreakRange.BreakRangeEnd = (PVOID)(UINTN)RangeStep->BreakRangeMaximum;
        KdBreakRange.RangeHoleStart = (PVOID)(UINTN)RangeStep->RangeHoleMinimum;
        KdBreakRange.RangeHoleEnd = (PVOID)(UINTN)RangeStep->RangeHoleMaximum;
        KdBreakRange.Enabled = TRUE;
        KdUserRequestedSingleStep = FALSE;
        KdpSetSingleStepMode(Exception, TrapFrame, NULL);
        *ContinueExecution = TRUE;
        KdFreezeOwner = MAX_ULONG;
        break;

    //
    // The set registers command replaces all the general registers in the trap
    // frame with the ones provided by the debugger.
    //

    case DbgCommandSetRegisters:
        KdpSetRegisters(TrapFrame, Packet->Payload);
        break;

    //
    // The module list header request causes the debugger to send information
    // about all the loaded modules in the system so the debugger can determine
    // if it's in sync.
    //

    case DbgModuleListHeaderRequest:
        KdpSendModuleList(TRUE);
        break;

    //
    // The module list entries request causes the debugger to send a complete
    // list of all loaded modules. This is a much slower operation than just
    // sending the header.
    //

    case DbgModuleListEntriesRequest:
        KdpSendModuleList(FALSE);
        break;

    //
    // The read or write virtual memory request sends or edits host memory.
    //

    case DbgMemoryReadVirtual:
    case DbgMemoryWriteVirtual:
        KdpHandleMemoryAccess();
        break;

    //
    // The switch processor command switches the view to another processor.
    //

    case DbgCommandSwitchProcessor:
        SwitchProcessorRequest = (PSWITCH_PROCESSOR_REQUEST)KdRxPacket.Payload;
        KdFreezeOwner = SwitchProcessorRequest->ProcessorNumber;
        *ContinueExecution = TRUE;
        break;

    //
    // Handle commands for getting and setting special registers.
    //

    case DbgCommandGetSpecialRegisters:
        KdpGetSpecialRegisters((PSPECIAL_REGISTERS_UNION)(KdTxPacket.Payload));
        KdTxPacket.Header.Command = DbgCommandReturnSpecialRegisters;
        KdTxPacket.Header.PayloadSize = sizeof(SPECIAL_REGISTERS_UNION);
        Status = KdpSendPacket(&KdTxPacket, NULL);
        break;

    case DbgCommandSetSpecialRegisters:
        SetSpecialRegisters = (PSET_SPECIAL_REGISTERS)(KdRxPacket.Payload);
        KdpSetSpecialRegisters(&(SetSpecialRegisters->Original),
                               &(SetSpecialRegisters->New));

        break;

    //
    // Reboot the system.
    //

    case DbgCommandReboot:
        RebootRequest = (PDEBUG_REBOOT_REQUEST)(KdRxPacket.Payload);
        KdpReboot(RebootRequest->ResetType, Exception, TrapFrame);
        break;

    //
    // If a connection request is found, then the host is out of sync with the
    // target. Resend the connection parameters and break notification.
    //

    case DbgConnectionRequest:
        KdpSendConnectionResponse((PCONNECTION_REQUEST)(KdRxPacket.Payload),
                                  &BreakInRequested);

        if (Exception == EXCEPTION_POLL_DEBUGGER) {

            //
            // If it's just polling and the caller didn't want a break, then
            // continue. Otherwise, turn the poll into a break.
            //

            if (BreakInRequested == FALSE) {
                *ContinueExecution = TRUE;
                KdFreezeOwner = MAX_ULONG;
            }

        } else {

            //
            // If a break is requested, send the notification.
            //

            if (BreakInRequested != FALSE) {
                KdpInitializeBreakNotification(Exception,
                                               TrapFrame,
                                               &KdTxPacket);

                Status = KdpSendPacket(&KdTxPacket, NULL);
            }
        }

        break;

    //
    // Ignore spurious acknowledge commands.
    //

    case DbgPacketAcknowledge:
        break;

    //
    // The command is not recognized. Send the invalid command response.
    //

    default:
        KdTxPacket.Header.Command = DbgConnectionInvalidRequest;
        KdTxPacket.Header.PayloadSize = sizeof(DEBUG_PACKET_HEADER);
        RtlCopyMemory(&KdTxPacket.Payload,
                      &KdRxPacket.Header,
                      sizeof(DEBUG_PACKET_HEADER));

        Status = KdpSendPacket(&KdTxPacket, NULL);
        break;
    }

    return Status;
}

KSTATUS
KdpPrint (
    PPRINT_PARAMETERS PrintParameters,
    PBOOL BreakInRequested
    )

/*++

Routine Description:

    This routine prints a string to the debugger. Currently the maximum length
    string is a little less than one debug packet. This routine MUST be called
    from within the debugger path (not outside kernel code), as it writes to
    the global transmit/receive packets.

Arguments:

    PrintParameters - Supplies a pointer to the required print parameters.

    BreakInRequested - Supplies an optional pointer where a boolean will be
        returned indicating if the debugger would like to break in.

Return Value:

    Status code.

--*/

{

    ULONG MaxStringLength;
    ULONG StringLength;

    KdTxPacket.Header.Command = DbgPrintString;

    //
    // Print the format string, with the packet as the destination buffer.
    //

    MaxStringLength = DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER);
    StringLength = RtlFormatString((PSTR)(&(KdTxPacket.Payload)),
                                   MaxStringLength,
                                   CharacterEncodingDefault,
                                   PrintParameters->FormatString,
                                   PrintParameters->Arguments);

    //
    // Print strings cannot be bigger than the packet size. If they are,
    // print out a warning. Otherwise, print the string.
    //

    if (StringLength > MaxStringLength) {
        StringLength = MaxStringLength;
    }

    KdTxPacket.Header.PayloadSize = StringLength;
    return KdpSendPacket(&KdTxPacket, BreakInRequested);
}

KSTATUS
KdpSendProfilingData (
    PBOOL BreakInRequested
    )

/*++

Routine Description:

    This routine calls the system profiler for data and sends it to the
    debugger.

Arguments:

    BreakInRequested - Supplies a pointer where a boolean will be returned
        indicating if the debugger would like to break in.

Return Value:

    Status code.

--*/

{

    ULONG DataSize;
    ULONG Flags;
    BOOL LocalBreakInRequested;
    PPROCESSOR_BLOCK ProcessorBlock;
    PPROFILER_NOTIFICATION ProfilerNotification;
    KSTATUS Status;

    *BreakInRequested = FALSE;

    //
    // Check to see if there is any data to send. Another core may have
    // collected and sent the data since the flags were checked before the
    // debug exception.
    //

    Flags = SpGetProfilerDataStatus();
    if (Flags == 0) {
        Status = STATUS_SUCCESS;
        goto SendProfilingDataEnd;
    }

    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
    DataSize = DEBUG_PAYLOAD_SIZE - sizeof(PROFILER_NOTIFICATION_HEADER);
    ProfilerNotification = (PPROFILER_NOTIFICATION)KdTxPacket.Payload;
    KdTxPacket.Header.Command = DbgProfilerNotification;

    //
    // Loop as long as there is more profiling data to send.
    //

    while (Flags != 0) {

        //
        // Initialize the debugger packet for a profiler notification message.
        //

        ASSERT(KdTxPacket.Header.Command == DbgProfilerNotification);

        KdTxPacket.Header.PayloadSize = DEBUG_PAYLOAD_SIZE;

        //
        // Collect the pending profiler data. Exit if something went wrong.
        //

        ProfilerNotification->Header.DataSize = DataSize;
        Status = SpGetProfilerData(ProfilerNotification, &Flags);
        if (!KSUCCESS(Status)) {
            goto SendProfilingDataEnd;
        }

        //
        // Don't bother sending an empty packet. Skip to the next one.
        //

        if (ProfilerNotification->Header.DataSize == 0) {
            continue;
        }

        //
        // Send the profiler notification packet.
        //

        Status = KdpSendPacket(&KdTxPacket, &LocalBreakInRequested);
        if (!KSUCCESS(Status)) {
            goto SendProfilingDataEnd;
        }

        if (LocalBreakInRequested != FALSE) {
            *BreakInRequested = TRUE;
        }
    }

    //
    // Send a final packet to the debugger notifying that this round of sending
    // profiling data is complete.
    //

    ASSERT(KdTxPacket.Header.Command == DbgProfilerNotification);

    KdTxPacket.Header.PayloadSize = sizeof(PROFILER_NOTIFICATION_HEADER);
    ProfilerNotification->Header.DataSize = 0;
    ProfilerNotification->Header.Type = ProfilerDataTypeMax;
    ProfilerNotification->Header.Processor = 0;
    if (ProcessorBlock != NULL) {
        ProfilerNotification->Header.Processor =
                                               ProcessorBlock->ProcessorNumber;
    }

    Status = KdpSendPacket(&KdTxPacket, &LocalBreakInRequested);
    if (!KSUCCESS(Status)) {
        goto SendProfilingDataEnd;
    }

    if (LocalBreakInRequested != FALSE) {
        *BreakInRequested = TRUE;
    }

SendProfilingDataEnd:
    return Status;
}

ULONG
KdpValidateMemoryAccess (
    PVOID Address,
    ULONG Length,
    PBOOL Writable
    )

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

{

    if (KdSkipMemoryValidation != FALSE) {
        if (Writable != NULL) {
            *Writable = TRUE;
        }

        return Length;
    }

    return MmValidateMemoryAccessForDebugger(Address, Length, Writable);
}

VOID
KdpModifyAddressMapping (
    PVOID Address,
    BOOL Writable,
    PBOOL WasWritable
    )

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

{

    if (KdSkipMemoryValidation != FALSE) {
        *WasWritable = TRUE;
        return;
    }

    return MmModifyAddressMappingForDebugger(Address, Writable, WasWritable);
}

USHORT
KdpCalculateChecksum (
    PVOID Data,
    ULONG DataLength
    )

/*++

Routine Description:

    This routine computes a checksum over a given length. It can handle both odd
    and even length data.

Arguments:

    Data - Supplies a pointer to the data that is to be checksummed.

    DataLength - Supplies the length of the data buffer, in bytes.

Return Value:

    Returns the checksum.

--*/

{

    USHORT Checksum;
    PUSHORT CurrentData;
    ULONG Index;
    ULONG ShortLength;

    Checksum = 0;
    Index = 0;

    //
    // Checksums are calculated by adding up a series of two-byte values.
    // Convert the pointer to a short pointer and divide bytes by 2 to get size
    // in shorts.
    //

    ShortLength = DataLength / 2;
    CurrentData = (PUSHORT)Data;
    while (Index < ShortLength) {
        Checksum += *CurrentData;
        CurrentData += 1;
        Index += 1;
    }

    //
    // If the data was an odd length, then there's one byte left to be added.
    // Add only that byte.
    //

    if ((ShortLength * 2) != DataLength) {
        Checksum += *((PUCHAR)CurrentData);
    }

    return Checksum;
}

KSTATUS
KdpSendModuleList (
    BOOL SendHeaderOnly
    )

/*++

Routine Description:

    This routine sends information to the debugger client about the binaries
    currently loaded in the system.

Arguments:

    SendHeaderOnly - Supplies a flag indicating that the debugger requested only
        the module list header.

Return Value:

    Status code.

--*/

{

    PDEBUG_MODULE CurrentModule;
    PLIST_ENTRY CurrentModuleListEntry;
    ULONG MaxNameLength;
    PLOADED_MODULE_ENTRY ModuleEntry;
    PMODULE_LIST_HEADER ModuleListHeader;
    PCHAR NameDestination;
    ULONG NameLength;
    PCHAR NameSource;
    ULONG PacketMaxNameLength;
    KSTATUS Status;

    PacketMaxNameLength = sizeof(KdTxPacket.Payload) -
                          sizeof(LOADED_MODULE_ENTRY) +
                          (sizeof(CHAR) * ANYSIZE_ARRAY);

    //
    // If the loaded modules list has not been initialized, send an error, and
    // return as successful.
    //

    if (KdLoadedModulesInitialized == FALSE) {
        KdTxPacket.Header.Command = DbgModuleListError;
        KdTxPacket.Header.PayloadSize = 0;
        Status = KdpSendPacket(&KdTxPacket, NULL);
        goto SendModuleListEnd;
    }

    //
    // Send the module list header.
    //

    KdTxPacket.Header.Command = DbgModuleListHeader;
    KdTxPacket.Header.PayloadSize = sizeof(MODULE_LIST_HEADER);
    ModuleListHeader = (PMODULE_LIST_HEADER)KdTxPacket.Payload;
    ModuleListHeader->ModuleCount = KdLoadedModules.ModuleCount;
    ModuleListHeader->Signature = KdLoadedModules.Signature;
    Status = KdpSendPacket(&KdTxPacket, NULL);
    if ((!KSUCCESS(Status)) || (SendHeaderOnly != FALSE)) {
        goto SendModuleListEnd;
    }

    //
    // Send all modules.
    //

    KdTxPacket.Header.Command = DbgModuleListEntry;
    ModuleEntry = (PLOADED_MODULE_ENTRY)KdTxPacket.Payload;
    CurrentModuleListEntry = KdLoadedModules.ModulesHead.Next;
    while (CurrentModuleListEntry != &(KdLoadedModules.ModulesHead)) {
        CurrentModule = LIST_VALUE(CurrentModuleListEntry,
                                   DEBUG_MODULE,
                                   ListEntry);

        //
        // Copy the name.
        //

        NameSource = CurrentModule->BinaryName;
        NameDestination = ModuleEntry->BinaryName;
        MaxNameLength = CurrentModule->StructureSize - sizeof(DEBUG_MODULE) +
                        (ANYSIZE_ARRAY * sizeof(CHAR));

        if (MaxNameLength > PacketMaxNameLength) {
            MaxNameLength = PacketMaxNameLength;
        }

        NameLength = RtlStringCopy(NameDestination, NameSource, MaxNameLength);

        //
        // Copy the rest of the module information, and send the packet.
        //

        ModuleEntry->StructureSize = sizeof(LOADED_MODULE_ENTRY) +
                                     ((NameLength - ANYSIZE_ARRAY) *
                                      sizeof(CHAR));

        ModuleEntry->Timestamp = CurrentModule->Timestamp;
        ModuleEntry->LowestAddress = (UINTN)CurrentModule->LowestAddress;
        ModuleEntry->Size = CurrentModule->Size;
        ModuleEntry->Process = CurrentModule->Process;
        KdTxPacket.Header.PayloadSize = ModuleEntry->StructureSize;
        Status = KdpSendPacket(&KdTxPacket, NULL);
        if (!KSUCCESS(Status)) {
            goto SendModuleListEnd;
        }

        //
        // Get the next module.
        //

        CurrentModuleListEntry = CurrentModuleListEntry->Next;
    }

SendModuleListEnd:
    return Status;
}

KSTATUS
KdpHandleMemoryAccess (
    VOID
    )

/*++

Routine Description:

    This routine handles memory read and write commands from the debugger.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PBYTE AddressByte;
    PUINTN AddressNatural;
    PVOID Buffer;
    ULONG ByteCount;
    ULONG MaxSize;
    PVOID PageAddress;
    ULONG PageSize;
    BOOL PageWritable;
    PBYTE ReadDestinationByte;
    PUINTN ReadDestinationNatural;
    BOOL RegionWritable;
    PMEMORY_REQUEST Request;
    PMEMORY_CONTENTS Response;
    KSTATUS Status;
    ULONG ValidSize;
    PWRITE_REQUEST_ACKNOWLEDGEMENT WriteAcknowledgement;
    PBYTE WriteSourceByte;
    PUINTN WriteSourceNatural;

    Request = (PMEMORY_REQUEST)KdRxPacket.Payload;

    //
    // Determine that maximum size that could legally be requested and still fit
    // inside a debug packet.
    //

    if (KdRxPacket.Header.Command == DbgMemoryReadVirtual) {
        MaxSize = DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER) -
                                                        sizeof(MEMORY_CONTENTS);

    } else if (KdRxPacket.Header.Command == DbgMemoryWriteVirtual) {
        MaxSize = DEBUG_PACKET_SIZE - sizeof(DEBUG_PACKET_HEADER) -
                                                        sizeof(MEMORY_REQUEST);

    } else {
        MaxSize = 0;
    }

    //
    // If the request is invalid, send back an invalid command packet.
    //

    if (Request->Size > MaxSize) {
        KdTxPacket.Header.Command = DbgInvalidCommand;
        KdTxPacket.Header.PayloadSize = 0;
    }

    //
    // Validate the memory access to make sure the debugger does not cause an
    // access violation.
    //

    AddressByte = (PVOID)(UINTN)Request->Address;
    AddressNatural = (PUINTN)AddressByte;
    ValidSize = KdpValidateMemoryAccess(AddressByte,
                                        Request->Size,
                                        &RegionWritable);

    //
    // For reads, copy the contents from memory into the packet.
    //

    if (KdRxPacket.Header.Command == DbgMemoryReadVirtual) {
        KdTxPacket.Header.Command = DbgMemoryContents;
        KdTxPacket.Header.PayloadSize = sizeof(MEMORY_CONTENTS) + ValidSize;
        Response = (PMEMORY_CONTENTS)KdTxPacket.Payload;
        Response->Address = (UINTN)AddressByte;
        Response->Size = ValidSize;
        Buffer = (PVOID)KdTxPacket.Payload + sizeof(MEMORY_CONTENTS);

        //
        // Copy in natural word size chunks if possible.
        //

        if ((ValidSize & (sizeof(UINTN) - 1)) == 0) {
            ReadDestinationNatural = Buffer;
            for (ByteCount = 0;
                 ByteCount < ValidSize;
                 ByteCount += sizeof(UINTN)) {

                *ReadDestinationNatural = *AddressNatural;
                AddressNatural += 1;
                ReadDestinationNatural += 1;
            }

        //
        // Copy byte for byte.
        //

        } else {
            ReadDestinationByte = Buffer;
            for (ByteCount = 0; ByteCount < ValidSize; ByteCount += 1) {
                *ReadDestinationByte = *AddressByte;
                AddressByte += 1;
                ReadDestinationByte += 1;
            }
        }

    //
    // For writes, copy the contents of the packet into memory.
    //

    } else if (KdRxPacket.Header.Command == DbgMemoryWriteVirtual) {
        KdTxPacket.Header.Command = DbgMemoryWriteAcknowledgement;
        KdTxPacket.Header.PayloadSize = sizeof(WRITE_REQUEST_ACKNOWLEDGEMENT);
        WriteAcknowledgement =
                        (PWRITE_REQUEST_ACKNOWLEDGEMENT)KdTxPacket.Payload;

        WriteAcknowledgement->Address = (UINTN)AddressByte;
        WriteAcknowledgement->BytesWritten = ValidSize;
        WriteSourceByte = (PBYTE)KdRxPacket.Payload + sizeof(MEMORY_REQUEST);
        WriteSourceNatural = (PUINTN)WriteSourceByte;

        //
        // Copy in natural word size chunks if possible.
        //

        PageAddress = NULL;
        PageSize = MmPageSize();
        PageWritable = FALSE;
        if ((ValidSize & (sizeof(UINTN) - 1)) == 0) {
            for (ByteCount = 0;
                 ByteCount < ValidSize;
                 ByteCount += sizeof(UINTN)) {

                //
                // Make sure the current page is mapped writable if the entire
                // region was determined to not be writable.
                //

                if ((RegionWritable == FALSE) && (PageAddress == NULL)) {
                    PageAddress = ALIGN_POINTER_DOWN(AddressNatural, PageSize);
                    KdpModifyAddressMapping(PageAddress, TRUE, &PageWritable);
                }

                *AddressNatural = *WriteSourceNatural;
                KdpCleanMemory(AddressNatural);
                WriteSourceNatural += 1;
                AddressNatural += 1;

                //
                // If the entire region was not writable and a new page is up
                // next, then set the last page back to read-only if it was not
                // originally writable.
                //

                if ((RegionWritable == FALSE) &&
                    (PageAddress !=
                     ALIGN_POINTER_DOWN(AddressNatural, PageSize))) {

                    ASSERT(PageAddress != NULL);

                    if (PageWritable == FALSE) {
                        KdpModifyAddressMapping(PageAddress,
                                                FALSE,
                                                &PageWritable);
                    }

                    PageAddress = NULL;
                }
            }

        //
        // Copy byte for byte.
        //

        } else {
            for (ByteCount = 0; ByteCount < ValidSize; ByteCount += 1) {

                //
                // Make sure the current page is mapped writable if the entire
                // region was determined to not be writable.
                //

                if ((RegionWritable == FALSE) && (PageAddress == NULL)) {
                    PageAddress = ALIGN_POINTER_DOWN(AddressByte, PageSize);
                    KdpModifyAddressMapping(PageAddress, TRUE, &PageWritable);
                }

                *AddressByte = *WriteSourceByte;
                KdpCleanMemory(AddressByte);
                AddressByte += 1;
                WriteSourceByte += 1;

                //
                // If the entire region was not writable and a new page is up
                // next, then set the last page back to read-only if it was not
                // originally writable.
                //

                if ((RegionWritable == FALSE) &&
                    (PageAddress !=
                     ALIGN_POINTER_DOWN(AddressByte, PageSize))) {

                    ASSERT(PageAddress != NULL);

                    if (PageWritable == FALSE) {
                        KdpModifyAddressMapping(PageAddress,
                                                FALSE,
                                                &PageWritable);
                    }

                    PageAddress = NULL;
                }
            }
        }

        //
        // If the entire region was not writable, then the last page might have
        // been left in the incorrect state. If the page address is valid and
        // it was not writable, then modify it to read-only.
        //

        if ((RegionWritable == FALSE) &&
            (PageWritable == FALSE) &&
            (PageAddress != NULL)) {

            KdpModifyAddressMapping(PageAddress, FALSE, &PageWritable);
        }

    //
    // For unknown requests, the response will be an invalid request packet.
    //

    } else {
        KdTxPacket.Header.Command = DbgInvalidCommand;
        KdTxPacket.Header.PayloadSize = 0;
    }

    //
    // Send the response.
    //

    Status = KdpSendPacket(&KdTxPacket, NULL);
    return Status;
}

VOID
KdpInitializeBreakNotification (
    ULONG Exception,
    PTRAP_FRAME TrapFrame,
    PDEBUG_PACKET Packet
    )

/*++

Routine Description:

    This routine initializes a break notification structure to be sent to the
    debugger.

Arguments:

    Exception - Supplies the type of exception to notify the debugger about.

    TrapFrame - Supplies a pointer to the current trap frame.

    Packet - Supplies a pointer to receive the debug packet to send to the
        debugger.

Return Value:

    None.

--*/

{

    PBREAK_NOTIFICATION BreakNotification;
    PVOID InstructionPointer;
    PVOID InstructionPointerAddress;
    PBYTE InstructionStream;
    ULONG InstructionStreamBytes;
    PKPROCESS Process;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG StreamIndex;

    //
    // Begin to initialize the break notification that will be sent to the
    // debugger.
    //

    BreakNotification = (PBREAK_NOTIFICATION)Packet->Payload;
    BreakNotification->LoadedModuleCount = KdLoadedModules.ModuleCount;
    BreakNotification->LoadedModuleSignature = KdLoadedModules.Signature;

    //
    // Initialize the processor number, but be careful about reaching through
    // the processor block as it will be NULL in the loader or very early
    // kernel init.
    //

    BreakNotification->ProcessorOrThreadCount = 1;
    BreakNotification->ProcessorOrThreadNumber = 0;
    BreakNotification->Process = 0;
    BreakNotification->ProcessorBlock = (UINTN)NULL;
    if (KeActiveProcessorCount != 0) {
        ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
        if (ProcessorBlock != NULL) {
            BreakNotification->ProcessorOrThreadCount = KeActiveProcessorCount;
            BreakNotification->ProcessorOrThreadNumber =
                                               ProcessorBlock->ProcessorNumber;

            BreakNotification->ProcessorBlock = (UINTN)ProcessorBlock;

            //
            // Reach out to get the current process ID. It would be safer (but
            // also slower) if each of these pointer reach-throughs was
            // validated first. Doing the validation would really only save
            // cases where the current thread or process structure was severely
            // corrupted.
            //

            if ((ProcessorBlock->RunningThread != NULL) &&
                (ProcessorBlock->RunningThread->OwningProcess != NULL)) {

                Process = ProcessorBlock->RunningThread->OwningProcess;
                BreakNotification->Process = Process->Identifiers.ProcessId;
            }
        }
    }

    InstructionPointer = KdpGetInstructionPointer(TrapFrame);
    InstructionPointerAddress = KdpGetInstructionPointerAddress(TrapFrame);
    BreakNotification->InstructionPointer = (UINTN)InstructionPointer;
    BreakNotification->ErrorCode = 0;

    //
    // If this was a break exception, set the exception type based on whether or
    // not this was an official breakpoint or some random int3.
    //

    if (Exception == EXCEPTION_BREAK) {
        BreakNotification->Exception = ExceptionDebugBreak;

    } else if (Exception == EXCEPTION_SINGLE_STEP) {
        BreakNotification->Exception = ExceptionSingleStep;

    } else if (Exception == EXCEPTION_ASSERTION_FAILURE) {
        BreakNotification->Exception = ExceptionAssertionFailure;

    } else if (Exception == EXCEPTION_ACCESS_VIOLATION) {
        BreakNotification->Exception = ExceptionAccessViolation;
        BreakNotification->ErrorCode = KdpGetErrorCode(Exception, TrapFrame);

    } else if (Exception == EXCEPTION_DOUBLE_FAULT) {
        BreakNotification->Exception = ExceptionDoubleFault;
        BreakNotification->ErrorCode = 0;

    } else if (Exception == EXCEPTION_UNDEFINED_INSTRUCTION) {
        BreakNotification->Exception = ExceptionIllegalInstruction;

    } else if (Exception == EXCEPTION_DEBUGGER_CONNECT) {
        BreakNotification->Exception = ExceptionDebugBreak;

    //
    // This was an unknown exception.
    //

    } else {
        BreakNotification->Exception = ExceptionUnknown;
    }

    //
    // Read in the instruction stream, validating access. As a safety, don't
    // read from instruction pointers that are NULL.
    //

    InstructionStreamBytes = KdpValidateMemoryAccess(
                                                InstructionPointerAddress,
                                                BREAK_NOTIFICATION_STREAM_SIZE,
                                                NULL);

    RtlZeroMemory(BreakNotification->InstructionStream,
                  BREAK_NOTIFICATION_STREAM_SIZE);

    if ((TrapFrame != NULL) && (InstructionPointer != NULL)) {
        InstructionStream = InstructionPointerAddress;
        for (StreamIndex = 0;
             StreamIndex < InstructionStreamBytes;
             StreamIndex += 1) {

            BreakNotification->InstructionStream[StreamIndex] =
                                                            *InstructionStream;

            InstructionStream += 1;
        }
    }

    //
    // Copy in the trap frame registers.
    //

    KdpGetRegisters(TrapFrame, &(BreakNotification->Registers));

    //
    // Send the break notification to the debugger.
    //

    Packet->Header.Command = DbgBreakNotification;
    Packet->Header.PayloadSize = sizeof(BREAK_NOTIFICATION);
    return;
}

VOID
KdpReboot (
    DEBUG_REBOOT_TYPE RebootType,
    ULONG Exception,
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine attempts to reboot the target machine.

Arguments:

    RebootType - Supplies the reboot type.

    Exception - Supplies the original exception code.

    TrapFrame - Supplies a pointer to the trap frame, used to reinitialize the
        debugger connection after a failed reboot attempt.

Return Value:

    None.

--*/

{

    SYSTEM_RESET_TYPE ResetType;
    KSTATUS Status;

    ResetType = SystemResetWarm;
    switch (RebootType) {
    case DebugRebootShutdown:
        ResetType = SystemResetShutdown;
        break;

    case DebugRebootWarm:
        ResetType = SystemResetWarm;
        break;

    case DebugRebootCold:
        ResetType = SystemResetCold;
        break;

    default:
        break;
    }

    KdpDisconnect();
    Status = HlResetSystem(ResetType, NULL, 0);
    KdpConnect(NULL);
    KdpInternalPrint("Reset system failed with status %d\n", Status);
    KdpInitializeBreakNotification(Exception, TrapFrame, &KdTxPacket);
    KdpSendPacket(&KdTxPacket, NULL);
    return;
}

VOID
KdpDisconnect (
    VOID
    )

/*++

Routine Description:

    This routine disconnects the target from the host.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSHUTDOWN_NOTIFICATION Notification;

    if (KdDebuggerConnected == FALSE) {
        return;
    }

    Notification = (PSHUTDOWN_NOTIFICATION)KdTxPacket.Payload;
    Notification->UnloadAllSymbols = TRUE;
    Notification->ShutdownType = ShutdownTypeTransition;
    Notification->Process = 0;
    Notification->ExitStatus = 0;
    KdTxPacket.Header.Command = DbgShutdownNotification;
    KdTxPacket.Header.PayloadSize = sizeof(SHUTDOWN_NOTIFICATION);
    KdpSendPacket(&KdTxPacket, NULL);
    KdpDeviceDisconnect();
    KdDebuggerConnected = FALSE;
    return;
}

KSTATUS
KdpConnect (
    PBOOL BreakInRequested
    )

/*++

Routine Description:

    This routine attempts to connect to the kernel debugger.

Arguments:

    BreakInRequested - Supplies a pointer where a boolean will be returned
        indicating if an initial break is requested.

Return Value:

    Status code. If the status is failing, the debugger connected global will
    be set to FALSE.

--*/

{

    PCONNECTION_REQUEST ConnectionRequest;
    KSTATUS Status;

    if (KdDebuggerConnected != FALSE) {
        return STATUS_SUCCESS;
    }

    //
    // Initialize the serial port for use.
    //

    Status = KdpDeviceReset(DEBUG_DEFAULT_BAUD_RATE);
    if (!KSUCCESS(Status)) {
        goto ConnectEnd;
    }

    if (BreakInRequested != NULL) {
        *BreakInRequested = FALSE;
    }

    //
    // Let the host know a new connection is present.
    //

    KdpSynchronize();

    //
    // Attempt to receive the connection request packet.
    //

    while (TRUE) {
        Status = KdpReceivePacket(&KdRxPacket, KdConnectionTimeout);

        //
        // If a synchronize byte was found, reply to it and try again.
        //

        if (Status == STATUS_CONNECTION_RESET) {
            KdpSynchronize();
            continue;
        }

        if (!KSUCCESS(Status)) {
            goto ConnectEnd;
        }

        if (KdRxPacket.Header.Command != DbgConnectionRequest) {
            KdTxPacket.Header.Command = DbgConnectionUninitialized;
            KdTxPacket.Header.PayloadSize = 0;
            KdpSendPacket(&KdTxPacket, NULL);

        } else {
            break;
        }
    }

    ConnectionRequest = (PCONNECTION_REQUEST)(KdRxPacket.Payload);
    Status = KdpSendConnectionResponse(ConnectionRequest, BreakInRequested);
    if (!KSUCCESS(Status)) {
        goto ConnectEnd;
    }

ConnectEnd:
    return Status;
}

VOID
KdpSynchronize (
    VOID
    )

/*++

Routine Description:

    This routine synchronizes with the kernel debugger in preparation for
    receiving the connection request packet. The synchornization process is a
    simply exchange of bytes where both sides must send a SYN, ACK the other
    side's SYN, and receive an ACK.

Arguments:

    None.

Return Value:

    None. Failure is considered non-fatal.

--*/

{

    UCHAR SynchronizeByte;

    SynchronizeByte = DEBUG_SYNCHRONIZE_TARGET;
    KdpTransmitBytes(&SynchronizeByte, 1);
    return;
}

KSTATUS
KdpSendConnectionResponse (
    PCONNECTION_REQUEST ConnectionRequest,
    PBOOL BreakInRequested
    )

/*++

Routine Description:

    This routine attempts to connect to the kernel debugger.

Arguments:

    ConnectionRequest - Supplies a pointer to the connection request.

    BreakInRequested - Supplies an optional pointer where a boolean will be
        returned indicating if an initial break is requested.

Return Value:

    Status code. If the status is failing, the debugger connected global will
    be set to FALSE.

--*/

{

    BOOL AcknowledgeBreakRequested;
    PCONNECTION_RESPONSE ConnectionResponse;
    KSTATUS OverallStatus;
    KSTATUS Status;
    ULONG StringSize;
    SYSTEM_VERSION_INFORMATION SystemVersion;

    OverallStatus = STATUS_SUCCESS;

    //
    // Compare protocol version numbers.
    //

    if (ConnectionRequest->ProtocolMajorVersion <
        DEBUG_PROTOCOL_MAJOR_VERSION) {

        //
        // Protocols do not match. Send back the kernel's debugger
        // protocol information, and return. Consider the debugger to be
        // disconnected at this point.
        //

        KdTxPacket.Header.Command = DbgConnectionWrongVersion;
        OverallStatus = STATUS_VERSION_MISMATCH;

    } else {
        KdTxPacket.Header.Command = DbgConnectionAcknowledge;
    }

    if (BreakInRequested != NULL) {
        *BreakInRequested = ConnectionRequest->BreakRequested;
    }

    //
    // Fill out and send the connection response.
    //

    ConnectionResponse = (PCONNECTION_RESPONSE)(KdTxPacket.Payload);
    RtlZeroMemory(ConnectionResponse, sizeof(CONNECTION_RESPONSE));
    ConnectionResponse->ProtocolMajorVersion = DEBUG_PROTOCOL_MAJOR_VERSION;
    ConnectionResponse->ProtocolRevision = DEBUG_PROTOCOL_REVISION;
    StringSize = DEBUG_PAYLOAD_SIZE - sizeof(CONNECTION_RESPONSE);
    Status = KeGetSystemVersion(&SystemVersion,
                                ConnectionResponse + 1,
                                &StringSize);

    if (KSUCCESS(Status)) {
        ConnectionResponse->SystemMajorVersion = SystemVersion.MajorVersion;
        ConnectionResponse->SystemMinorVersion = SystemVersion.MinorVersion;
        ConnectionResponse->SystemRevision = SystemVersion.Revision;
        ConnectionResponse->SystemSerialVersion = SystemVersion.SerialVersion;
        ConnectionResponse->SystemReleaseLevel = SystemVersion.ReleaseLevel;
        ConnectionResponse->SystemBuildDebugLevel = SystemVersion.DebugLevel;
        ConnectionResponse->SystemBuildTime = SystemVersion.BuildTime.Seconds;
        ConnectionResponse->ProductNameOffset =
                                           (UINTN)(SystemVersion.ProductName) -
                                           (UINTN)(KdTxPacket.Payload);

        if (SystemVersion.BuildString != NULL) {
            ConnectionResponse->BuildStringOffset =
                                       (UINTN)(SystemVersion.BuildString) -
                                       (UINTN)(KdTxPacket.Payload);
        }
    }

    ConnectionResponse->Machine = KdMachineType;
    KdTxPacket.Header.PayloadSize = sizeof(CONNECTION_RESPONSE) + StringSize;
    Status = KdpSendPacket(&KdTxPacket, &AcknowledgeBreakRequested);
    if (!KSUCCESS(Status)) {
        OverallStatus = Status;
        goto SendConnectionResponseEnd;
    }

    if ((BreakInRequested != NULL) && (AcknowledgeBreakRequested != FALSE)) {
        *BreakInRequested = TRUE;
    }

    if (KdTxPacket.Header.Command == DbgConnectionAcknowledge) {
        KdDebuggerConnected = TRUE;
    }

SendConnectionResponseEnd:
    return OverallStatus;
}

VOID
KdpInternalPrint (
    PCSTR Format,
    ...
    )

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

{

    PRINT_PARAMETERS PrintParameters;

    PrintParameters.FormatString = Format;
    va_start(PrintParameters.Arguments, Format);
    KdpPrint(&PrintParameters, NULL);
    va_end(PrintParameters.Arguments);
    return;
}

VOID
KdpAcquireDebuggerLock (
    PTRAP_FRAME TrapFrame
    )

/*++

Routine Description:

    This routine acquires the master debugger lock, ensuring that only one
    processor is speaking out the debugger port at a time.

Arguments:

    TrapFrame - Supplies a pointer to the trap frame, which is used in case
        this processor needs to respond to a freeze request and become a
        subordinate processor.

Return Value:

    None.

--*/

{

    ULONG LockValue;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorNumber;

    ProcessorNumber = 0;
    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
    if (ProcessorBlock != NULL) {
        ProcessorNumber = ProcessorBlock->ProcessorNumber;
    }

    do {
        LockValue = KdpAtomicCompareExchange32(&KdLockAcquired,
                                               ProcessorNumber,
                                               -1);

        //
        // If the lock was not acquired and freeze requests are maskable,
        // look to see if another processor got the lock and is trying to
        // send a freeze request. If this is the case, there's a freeze
        // interrupt pending, which this processor will never get in this loop
        // as interrupts are disabled. Manually freeze, then mark that the
        // freeze, when it does come in, is expected.
        //

        if ((LockValue != -1) &&
            (KeActiveProcessorCount > 1) &&
            (KdFreezesAreMaskable != FALSE) &&
            (KdFreezeOwner != MAX_ULONG) &&
            (KdNmiBroadcastAllowed != FALSE)) {

            KdNmiHandler(TrapFrame);
        }

    } while ((LockValue != ProcessorNumber) && (LockValue != (ULONG)-1));

    return;
}

VOID
KdpReleaseDebuggerLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the master debugging lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG LockValue;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorNumber;

    ProcessorNumber = 0;
    ProcessorBlock = KeGetCurrentProcessorBlockForDebugger();
    if (ProcessorBlock != NULL) {
        ProcessorNumber = ProcessorBlock->ProcessorNumber;
    }

    LockValue = KdpAtomicCompareExchange32(&KdLockAcquired,
                                           -1,
                                           ProcessorNumber);

    ASSERT(LockValue == ProcessorNumber);

    return;
}

KSTATUS
KdpTransmitBytes (
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine sends bytes down the transmit to the debug host.

Arguments:

    Data - Supplies the data to transmit.

    Size - Supplies the number of bytes to transmit.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the desired number of bytes could not be retrieved in a
    timely fashion.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PUCHAR Bytes;
    UCHAR EncodedByte[2];
    ULONG SendSize;
    KSTATUS Status;

    Bytes = Data;
    Status = STATUS_SUCCESS;
    while (Size != 0) {
        if (KdEncodeBytes != FALSE) {

            //
            // Gather bytes until one is found that needs escaping.
            //

            SendSize = 0;
            while ((SendSize < Size) &&
                   (Bytes[SendSize] != DEBUG_XON) &&
                   (Bytes[SendSize] != DEBUG_XOFF) &&
                   (Bytes[SendSize] != DEBUG_ESCAPE)) {

                SendSize += 1;
            }

        //
        // If not encoding bytes, just send everything.
        //

        } else {
            SendSize = Size;
        }

        //
        // Send off the buffer so far.
        //

        if (SendSize != 0) {
            Status = KdpDeviceTransmit(Bytes, SendSize);
            if (!KSUCCESS(Status)) {
                break;
            }
        }

        Bytes += SendSize;
        Size -= SendSize;

        //
        // Send an encoded byte.
        //

        if (Size != 0) {
            EncodedByte[0] = DEBUG_ESCAPE;
            EncodedByte[1] = *Bytes + DEBUG_ESCAPE;
            Status = KdpDeviceTransmit(EncodedByte, 2);
            if (!KSUCCESS(Status)) {
                break;
            }

            Size -= 1;
            Bytes += 1;
        }
    }

    return Status;
}

KSTATUS
KdpReceiveBuffer (
    PVOID Data,
    PULONG Size,
    PULONG Timeout
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

    Timeout - Supplies a pointer that on input contains the number of
        microseconds to wait before timing out. On output, will contain the
        remainder of that wait.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the desired number of bytes could not be retrieved in a
    timely fashion.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    PUCHAR Bytes;
    ULONG BytesCompleted;
    ULONG BytesRemaining;
    ULONG Count;
    ULONG Index;
    ULONG MoveIndex;
    BOOL NextEscaped;
    KSTATUS Status;

    NextEscaped = FALSE;
    Status = STATUS_SUCCESS;
    Bytes = Data;
    BytesRemaining = *Size;
    while (BytesRemaining != 0) {
        BytesCompleted = BytesRemaining;
        Status = KdpDeviceReceiveBuffer(Bytes, &BytesCompleted, Timeout);
        if (!KSUCCESS(Status)) {
            break;
        }

        //
        // If escaping is on, then remove any escape bytes found, and fix up
        // the escaped byte.
        //

        Count = 0;
        if (KdEncodeBytes != FALSE) {

            //
            // If the last byte received was an escape, then unescape this
            // first byte.
            //

            if (NextEscaped != FALSE) {
                NextEscaped = FALSE;
                Bytes[0] -= DEBUG_ESCAPE;
                Bytes += 1;
                BytesRemaining -= 1;
                BytesCompleted -= 1;
                if (BytesRemaining == 0) {
                    break;
                }
            }

            for (Index = 0; Index < BytesCompleted - 1; Index += 1) {
                if (Bytes[Index] == DEBUG_ESCAPE) {
                    for (MoveIndex = Index;
                         MoveIndex < BytesCompleted - 1;
                         MoveIndex += 1) {

                        Bytes[MoveIndex] = Bytes[MoveIndex + 1];
                    }

                    Count += 1;
                    Bytes[Index] -= DEBUG_ESCAPE;
                }
            }

            //
            // If the last byte received is an escape, remember to unescape the
            // next byte.
            //

            if (Bytes[Index] == DEBUG_ESCAPE) {
                Count += 1;
                NextEscaped = TRUE;
            }
        }

        BytesCompleted -= Count;
        Bytes += BytesCompleted;

        //
        // If the count is non-zero, fewer real bytes were received than
        // expected, so go get the extra ones.
        //

        BytesRemaining -= BytesCompleted;
    }

    *Size -= BytesRemaining;
    return Status;
}

KSTATUS
KdpDeviceReceiveBuffer (
    PVOID Data,
    PULONG Size,
    PULONG Timeout
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

    Timeout - Supplies a pointer that on input contains the number of
        microseconds to wait before timing out. On output, will contain the
        remainder of that wait.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TIMEOUT if the desired number of bytes could not be retrieved in a
    timely fashion.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    ULONG BytesCompleted;
    ULONG BytesRemaining;
    ULONG StallDuration;
    KSTATUS Status;

    //
    // Loop until all data has been received. The first time around, do a
    // smaller delay. This way if a stream is being sent reliably but not
    // back to back, the larger delay doesn't get stuck in between each byte.
    //

    BytesRemaining = *Size;
    StallDuration = DEBUG_SMALL_STALL;
    Status = STATUS_SUCCESS;
    while (BytesRemaining != 0) {
        BytesCompleted = BytesRemaining;
        Status = KdpDeviceReceive(Data, &BytesCompleted);
        BytesRemaining -= BytesCompleted;
        if (Status == STATUS_NO_DATA_AVAILABLE) {

            //
            // Avoid both the time counter and stalls if the boolean is set.
            //

            if (KdAvoidTimeCounter != FALSE) {
                continue;
            }

            //
            // Keep the time counter fresh.
            //

            HlQueryTimeCounter();
            if (*Timeout == MAX_ULONG) {
                continue;

            } else if (*Timeout == 0) {
                Status = STATUS_TIMEOUT;
                break;

            } else if (*Timeout < StallDuration) {
                *Timeout = StallDuration;
            }

            //
            // Stall to count time towards the timeout.
            //

            HlBusySpin(StallDuration);
            *Timeout -= StallDuration;
            StallDuration = DEBUG_STALL_INCREMENT;
            continue;

        } else if (!KSUCCESS(Status)) {
            break;
        }

        Data += BytesCompleted;
        StallDuration = DEBUG_SMALL_STALL;
    }

    *Size -= BytesRemaining;
    return Status;
}

KSTATUS
KdpDeviceReset (
    ULONG BaudRate
    )

/*++

Routine Description:

    This routine initializes and resets a debug device, preparing it to send
    and receive data.

Arguments:

    BaudRate - Supplies the baud rate to set.

Return Value:

    STATUS_SUCCESS on success.

    Other status codes on failure. The device will not be used if a failure
    status code is returned.

--*/

{

    KSTATUS Status;

    KD_DEVICE_TRACE(KdDeviceTraceResetting);
    if (KdDebugDevice == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto DeviceResetEnd;
    }

    Status = KdDebugDevice->FunctionTable.Reset(KdDebugDevice->Context,
                                                BaudRate);

DeviceResetEnd:
    if (!KSUCCESS(Status)) {
        KD_DEVICE_TRACE(KdDeviceTraceResetFailed);

    } else {
        KD_DEVICE_TRACE(KdDeviceTraceResetComplete);
    }

    return Status;
}

KSTATUS
KdpDeviceTransmit (
    PVOID Data,
    ULONG Size
    )

/*++

Routine Description:

    This routine transmits data from the host out through the debug device.

Arguments:

    Data - Supplies a pointer to the data to write.

    Size - Supplies the size to write, in bytes.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    KSTATUS Status;

    KD_DEVICE_TRACE(KdDeviceTraceTransmitting);
    if (KdDebugDevice == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto DeviceTransmitEnd;
    }

    Status = KdDebugDevice->FunctionTable.Transmit(KdDebugDevice->Context,
                                                   Data,
                                                   Size);

DeviceTransmitEnd:
    if (!KSUCCESS(Status)) {
        KD_DEVICE_TRACE(KdDeviceTraceTransmitFailed);

    } else {
        KD_DEVICE_TRACE(KdDeviceTraceTransmitComplete);
    }

    return Status;
}

KSTATUS
KdpDeviceReceive (
    PVOID Data,
    PULONG Size
    )

/*++

Routine Description:

    This routine receives incoming data from the debug device.

Arguments:

    Data - Supplies a pointer where the read data will be returned on success.

    Size - Supplies a pointer that on input contains the size of the receive
        buffer. On output, returns the number of bytes read.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NO_DATA_AVAILABLE if there was no data to be read at the current
    time.

    STATUS_DEVICE_IO_ERROR if a device error occurred.

--*/

{

    KSTATUS Status;

    KD_DEVICE_TRACE(KdDeviceTraceReceiving);
    if (KdDebugDevice == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto DeviceReceiveEnd;
    }

    Status = KdDebugDevice->FunctionTable.Receive(KdDebugDevice->Context,
                                                  Data,
                                                  Size);

DeviceReceiveEnd:
    if (!KSUCCESS(Status)) {
        KD_DEVICE_TRACE(KdDeviceTraceReceiveFailed);

    } else {
        KD_DEVICE_TRACE(KdDeviceTraceReceiveComplete);
    }

    return Status;
}

KSTATUS
KdpDeviceGetStatus (
    PBOOL ReceiveDataAvailable
    )

/*++

Routine Description:

    This routine returns the current device status.

Arguments:

    ReceiveDataAvailable - Supplies a pointer where a boolean will be returned
        indicating whether or not receive data is available.

Return Value:

    Status code.

--*/

{

    KSTATUS Status;

    KD_DEVICE_TRACE(KdDeviceTraceGettingStatus);
    if (KdDebugDevice == NULL) {
        Status = STATUS_NO_SUCH_DEVICE;
        goto DeviceGetStatusEnd;
    }

    Status = KdDebugDevice->FunctionTable.GetStatus(KdDebugDevice->Context,
                                                    ReceiveDataAvailable);

DeviceGetStatusEnd:
    if (!KSUCCESS(Status)) {
        KD_DEVICE_TRACE(KdDeviceTraceGetStatusFailed);

    } else {
        if (*ReceiveDataAvailable != FALSE) {
            KD_DEVICE_TRACE(KdDeviceTraceGetStatusHasData);

        } else {
            KD_DEVICE_TRACE(KdDeviceTraceGetStatusEmpty);
        }
    }

    return Status;
}

VOID
KdpDeviceDisconnect (
    VOID
    )

/*++

Routine Description:

    This routine disconnects a device, taking it offline.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KD_DEVICE_TRACE(KdDeviceTraceDisconnecting);
    if (KdDebugDevice == NULL) {
        goto DeviceDisconnectEnd;
    }

    KdDebugDevice->FunctionTable.Disconnect(KdDebugDevice->Context);

DeviceDisconnectEnd:
    KD_DEVICE_TRACE(KdDeviceTraceDisconnected);
    return;
}

VOID
KdpFreezeProcessors (
    VOID
    )

/*++

Routine Description:

    This routine attempts to freeze all of the processors, assuming that the
    current processor is the freeze owner. It sends an NMI IPI to all of the
    other processors and waits for them to be marked frozen.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PROCESSOR_SET ProcessorSet;
    KSTATUS Status;
    ULONG Timeout;

    ProcessorSet.Target = ProcessorTargetAllExcludingSelf;
    Status = HlSendIpi(IpiTypeNmi, &ProcessorSet);
    if (!KSUCCESS(Status)) {
        goto FreezeProcessorsEnd;
    }

    Timeout = DEBUG_PROCESSOR_WAIT_TIME;

    //
    // Wait until all processors are frozen, or it's time to give
    // up. Keep the time counter fresh too during this period.
    //

    KD_TRACE(KdTraceWaitingForFrozenProcessors);
    while (KdProcessorsFrozen != KeActiveProcessorCount) {
        if ((KdConnectionTimeout != MAX_ULONG) &&
            (KdAvoidTimeCounter == FALSE)) {

            if (Timeout == 0) {
                break;

            } else if (Timeout < DEBUG_STALL_INCREMENT) {
                Timeout = DEBUG_STALL_INCREMENT;
            }

            HlBusySpin(DEBUG_STALL_INCREMENT);
            Timeout -= DEBUG_STALL_INCREMENT;
        }

        if (KdAvoidTimeCounter == FALSE) {
            HlQueryTimeCounter();
        }
    }

FreezeProcessorsEnd:
    return;
}

