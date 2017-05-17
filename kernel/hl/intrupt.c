/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    intrupt.c

Abstract:

    This module implements generic interrupt support for the hardware layer.

Author:

    Evan Green 28-Oct-2012

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/kernel.h>
#include <minoca/kernel/bootload.h>
#include "intrupt.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the amount of time to wait in seconds for a processor to come down
// from any interrupt runlevel. This value is already quite generous, it
// really shouldn't need to be increated.
//

#define INTERRUPT_COMPLETION_TIMEOUT 5

//
// Pick a value for dynamic GSIs to start that's not expected to conflict with
// any real interrupt controller lines.
//

#define DYNAMIC_GSI_BASE 0x8000
#define DYNAMIC_GSI_LIMIT 0xA000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
HlpInterruptInitializeController (
    PINTERRUPT_CONTROLLER Controller
    );

KSTATUS
HlpInterruptInitializeLocalUnit (
    PINTERRUPT_CONTROLLER Controller
    );

VOID
HlpInterruptAcquireLock (
    VOID
    );

VOID
HlpInterruptReleaseLock (
    VOID
    );

PINTERRUPT_CONTROLLER
HlpInterruptGetControllerByIdentifier (
    UINTN Identifier
    );

KSTATUS
HlpInterruptConvertLineToControllerSpecified (
    PINTERRUPT_LINE Line
    );

KSTATUS
HlpInterruptDetermineRouting (
    PINTERRUPT_CONTROLLER Controller,
    PINTERRUPT_LINE Destination,
    PINTERRUPT_LINE Route
    );

ULONG
HlpInterruptConvertRunLevelToHardwarePriority (
    PINTERRUPT_CONTROLLER Controller,
    RUNLEVEL RunLevel
    );

INTERRUPT_STATUS
HlpInterruptNullHandler (
    PVOID Context
    );

//
// -------------------------------------------------------------------- Globals
//

//
// This low level lock synchronizes configuration changes to interrupt
// controllers. It is obviously not acquired during interrupt dispatching and
// processing.
//

PQUEUED_LOCK HlInterruptLock;

//
// Store the list of registered interrupt controller hardware.
//

PINTERRUPT_CONTROLLER HlInterruptControllers[MAX_INTERRUPT_CONTROLLERS];
ULONG HlInterruptControllerCount;

//
// Store the next GSI to be dynamically allocated.
//

ULONG HlNextDynamicGsi = DYNAMIC_GSI_BASE;

//
// Store the number of spurious interrupts that have occurred.
//

volatile ULONG HlSpuriousInterruptCount;

//
// ------------------------------------------------------------------ Functions
//

KERNEL_API
INTERRUPT_MODEL
HlGetInterruptModel (
    VOID
    )

/*++

Routine Description:

    This routine returns the general system interrupt model currently in use.
    This routine is only useful to firmware or interrupt configuration parties.

Arguments:

    None.

Return Value:

    Returns the interrupt model in use.

--*/

{

    return InterruptModelApic;
}

KERNEL_API
KSTATUS
HlCreateInterruptController (
    ULONG ParentGsi,
    ULONG ParentVector,
    ULONG LineCount,
    PINTERRUPT_CONTROLLER_DESCRIPTION Registration,
    PINTERRUPT_CONTROLLER_INFORMATION ResultingInformation
    )

/*++

Routine Description:

    This routine creates an interrupt controller outside of the normal hardware
    module context. It is used primarily by GPIO controllers that function as a
    kind of secondary interrupt controller.

Arguments:

    ParentGsi - Supplies the global system interrupt number of the interrupt
        controller line this controller wires up to.

    ParentVector - Supplies the vector of the interrupt that this interrupt
        controller wires up to.

    LineCount - Supplies the number of lines this interrupt controller contains.

    Registration - Supplies a pointer to the interrupt controller information,
        filled out correctly by the caller.

    ResultingInformation - Supplies a pointer where the interrupt controller
        handle and other information will be returned.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG Gsi;
    INTERRUPT_LINES_DESCRIPTION Lines;
    PINTERRUPT_CONTROLLER OutputController;
    INTERRUPT_LINE OutputLine;
    RUNLEVEL RunLevel;
    KSTATUS Status;

    if ((LineCount == 0) || (Registration->ProcessorCount != 0) ||
        (Registration->PriorityCount != 0)) {

        return STATUS_INVALID_PARAMETER;
    }

    HlpInterruptAcquireLock();

    //
    // Find the controller this controller is hooked up to by converting its
    // parent GSI to an interrupt controller and line.
    //

    OutputLine.Type = InterruptLineGsi;
    OutputLine.U.Gsi = ParentGsi;
    Status = HlpInterruptConvertLineToControllerSpecified(&OutputLine);
    if (!KSUCCESS(Status)) {
        goto CreateInterruptControllerEnd;
    }

    //
    // If this controller wires to a real interrupt controller, then the
    // run-level of all these secondary interrupts is related to the vector
    // the source interrupt comes in on. If it's wired to a secondary
    // interrupt controller, then the run-level of these interrupts is the same
    // as its parent.
    //

    OutputController = HlpInterruptGetControllerByIdentifier(
                                                OutputLine.U.Local.Controller);

    ASSERT(OutputController != NULL);

    if (OutputController->RunLevel == RunLevelCount) {
        RunLevel = VECTOR_TO_RUN_LEVEL(ParentVector);

    } else {
        RunLevel = OutputController->RunLevel;
    }

    Status = HlpInterruptRegisterHardware(Registration, RunLevel, &Controller);
    if (!KSUCCESS(Status)) {
        goto CreateInterruptControllerEnd;
    }

    //
    // Go find a GSI range for this controller.
    //

    Status = STATUS_SUCCESS;
    Gsi = 0;
    if (HlNextDynamicGsi + LineCount > DYNAMIC_GSI_LIMIT) {
        Status = STATUS_RESOURCE_IN_USE;
        goto CreateInterruptControllerEnd;

    } else {
        Gsi = HlNextDynamicGsi;
        HlNextDynamicGsi += LineCount;
    }

    //
    // Register the output line of the controller as the input line of another.
    //

    RtlZeroMemory(&Lines, sizeof(INTERRUPT_LINES_DESCRIPTION));
    Lines.Version = INTERRUPT_LINES_DESCRIPTION_VERSION;
    Lines.Controller = Registration->Identifier;
    Lines.Type = InterruptLinesOutput;
    Lines.LineStart = OutputLine.U.Local.Line;
    Lines.LineEnd = Lines.LineStart + 1;
    Lines.Gsi = INTERRUPT_LINES_GSI_NONE;
    Lines.OutputControllerIdentifier = OutputLine.U.Local.Controller;
    Status = HlpInterruptRegisterLines(&Lines);
    if (!KSUCCESS(Status)) {
        goto CreateInterruptControllerEnd;
    }

    //
    // Now register the input lines this controller has, on its dynamically
    // allocated GSI.
    //

    Lines.Type = InterruptLinesStandardPin;
    Lines.LineStart = 0;
    Lines.LineEnd = LineCount;
    Lines.Gsi = Gsi;
    Status = HlpInterruptRegisterLines(&Lines);
    if (!KSUCCESS(Status)) {
        goto CreateInterruptControllerEnd;
    }

    //
    // Now that it's all ready to go, initialize the controller.
    //

    Status = HlpInterruptInitializeController(Controller);
    if (!KSUCCESS(Status)) {
        goto CreateInterruptControllerEnd;
    }

    ResultingInformation->Controller = Controller;
    ResultingInformation->StartingGsi = Gsi;
    ResultingInformation->LineCount = LineCount;
    Status = STATUS_SUCCESS;

CreateInterruptControllerEnd:
    HlpInterruptReleaseLock();
    return Status;
}

KERNEL_API
VOID
HlDestroyInterruptController (
    PINTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine destroys an interrupt controller, taking it offline and
    releasing all resources associated with it.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

{

    UINTN Count;
    UINTN Index;

    //
    // Acquire the lock to synchronize with creation, but the array is still
    // used by other cores servicing interrupts, so be careful.
    //

    HlpInterruptAcquireLock();
    Count = HlInterruptControllerCount;
    for (Index = 0; Index < Count; Index += 1) {

        //
        // Replace this index with the last index, and shrink the array size.
        // There will be a period where the same controller is visible in the
        // array twice, but that causes no damage.
        //

        if (HlInterruptControllers[Index] == Controller) {
            HlInterruptControllers[Index] = HlInterruptControllers[Count - 1];
            HlInterruptControllerCount -= 1;
            HlInterruptControllers[Count - 1] = NULL;
            break;
        }
    }

    HlpInterruptReleaseLock();

    ASSERT(Index != Count);

    //
    // Stall before returning to "ensure" that other cores servicing interrupts
    // have completed. This is weak, but interrupt controllers aren't really
    // expected to be coming and going. The memory and GSI range are leaked.
    //

    HlBusySpin(100000);
    return;
}

KERNEL_API
KSTATUS
HlGetInterruptControllerInformation (
    UINTN Identifier,
    PINTERRUPT_CONTROLLER_INFORMATION Information
    )

/*++

Routine Description:

    This routine returns information about an interrupt controller with a
    specific ID.

Arguments:

    Identifier - Supplies the identifier of the interrupt controller.

    Information - Supplies a pointer where the interrupt controller information
        will be returned on success.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_NOT_FOUND if no interrupt controller matching the given identifier
    exists in the system.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    PINTERRUPT_LINES Lines;
    KSTATUS Status;

    Status = STATUS_NOT_FOUND;
    Information->StartingGsi = INTERRUPT_LINES_GSI_NONE;
    Information->LineCount = 0;
    HlpInterruptAcquireLock();
    Controller = HlpInterruptGetControllerByIdentifier(Identifier);
    if (Controller != NULL) {
        Status = STATUS_SUCCESS;
        Information->Controller = Controller;
        if (!LIST_EMPTY(&(Controller->LinesHead))) {
            Lines = LIST_VALUE(Controller->LinesHead.Next,
                               INTERRUPT_LINES,
                               ListEntry);

            Information->StartingGsi = Lines->Gsi;
            Information->LineCount = Lines->LineEnd - Lines->LineStart;
        }
    }

    HlpInterruptReleaseLock();
    return Status;
}

PKINTERRUPT
HlCreateInterrupt (
    ULONG Vector,
    PINTERRUPT_SERVICE_ROUTINE InterruptServiceRoutine,
    PINTERRUPT_SERVICE_ROUTINE DispatchServiceRoutine,
    PINTERRUPT_SERVICE_ROUTINE LowLevelServiceRoutine,
    PVOID Context
    )

/*++

Routine Description:

    This routine creates and initialize a new KINTERRUPT structure.

Arguments:

    Vector - Supplies the vector that the interrupt will come in on.

    InterruptServiceRoutine - Supplies a pointer to the function to call at
        interrupt runlevel when this interrupt comes in.

    DispatchServiceRoutine - Supplies a pointer to the function to call at
        dispatch level when this interrupt comes in.

    LowLevelServiceRoutine - Supplies a pointer to the function to call at
        low runlevel when this interrupt comes in.

    Context - Supplies a pointer's worth of data that will be passed in to the
        service routine when it is called.

Return Value:

    Returns a pointer to the newly created interrupt on success. The interrupt
    is not connected at this point.

    NULL on failure.

--*/

{

    PKINTERRUPT Interrupt;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    //
    // If the interrupt only runs at low level, assign a dummy handler that
    // always returns the defer choice. This will be faster for the common case
    // than putting a conditional around every ISR call in the common interrupt
    // code.
    //

    if (InterruptServiceRoutine == NULL) {
        InterruptServiceRoutine = HlpInterruptNullHandler;
    }

    //
    // Allocate space for the new interrupt.
    //

    Interrupt = MmAllocateNonPagedPool(sizeof(KINTERRUPT), HL_POOL_TAG);
    if (Interrupt == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateInterruptEnd;
    }

    //
    // Initialize the interrupt.
    //

    RtlZeroMemory(Interrupt, sizeof(KINTERRUPT));
    Interrupt->Vector = Vector;
    Interrupt->Context = Context;
    Interrupt->InterruptServiceRoutine = InterruptServiceRoutine;

    //
    // Assume the interrupt is coming in with the primary vector to runlevel
    // mapping. The set line state function changes this value, but for MSI
    // interrupts for instance set line state is never called.
    //

    Interrupt->RunLevel = VECTOR_TO_RUN_LEVEL(Vector);
    if ((DispatchServiceRoutine != NULL) || (LowLevelServiceRoutine != NULL)) {
        Interrupt->DispatchServiceRoutine = DispatchServiceRoutine;
        Interrupt->Dpc = KeCreateDpc(HlpInterruptServiceDpc, Interrupt);
        if (Interrupt->Dpc == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto CreateInterruptEnd;
        }

        if (LowLevelServiceRoutine != NULL) {
            Interrupt->LowLevelServiceRoutine = LowLevelServiceRoutine;
            Interrupt->WorkItem = KeCreateWorkItem(NULL,
                                                   WorkPriorityHigh,
                                                   HlpInterruptServiceWorker,
                                                   Interrupt,
                                                   HL_POOL_TAG);

            if (Interrupt->WorkItem == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                goto CreateInterruptEnd;
            }
        }
    }

    Status = STATUS_SUCCESS;

CreateInterruptEnd:
    if (!KSUCCESS(Status)) {
        if (Interrupt != NULL) {
            if (Interrupt->Dpc != NULL) {
                KeDestroyDpc(Interrupt->Dpc);
            }

            if (Interrupt->WorkItem != NULL) {
                KeDestroyWorkItem(Interrupt->WorkItem);
            }

            MmFreeNonPagedPool(Interrupt);
            Interrupt = NULL;
        }
    }

    return Interrupt;
}

VOID
HlDestroyInterrupt (
    PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine destroys a KINTERRUPT structure.

Arguments:

    Interrupt - Supplies a pointer to the interrupt to destroy.

Return Value:

    None.

--*/

{

    //
    // The interrupt had better not still be connected.
    //

    ASSERT(Interrupt->NextInterrupt == NULL);

    //
    // Once the DPC and work items are destroyed, they should no longer be
    // queued.
    //

    if (Interrupt->Dpc != NULL) {
        KeDestroyDpc(Interrupt->Dpc);
    }

    ASSERT((Interrupt->QueueFlags & INTERRUPT_QUEUE_DPC_QUEUED) == 0);

    if (Interrupt->WorkItem != NULL) {
        KeDestroyWorkItem(Interrupt->WorkItem);
    }

    ASSERT((Interrupt->QueueFlags & INTERRUPT_QUEUE_WORK_ITEM_QUEUED) == 0);

    MmFreeNonPagedPool(Interrupt);
    return;
}

KSTATUS
HlConnectInterrupt (
    PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine commits an interrupt service routine to active duty. When this
    call is completed, it will be called for interrupts coming in on the
    specified vector.

Arguments:

    Interrupt - Supplies a pointer to the initialized interrupt.

Return Value:

    Status code.

--*/

{

    ULONG ArrayIndex;
    PKINTERRUPT *InterruptTable;
    RUNLEVEL OldRunLevel;
    PPROCESSOR_BLOCK ProcessorBlock;

    //
    // Use the global interrupt controller lock to synchronize with other
    // processors connecting and disconnecting.
    //

    ArrayIndex = Interrupt->Vector - HlFirstConfigurableVector;
    HlpInterruptAcquireLock();
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    InterruptTable = (PKINTERRUPT *)ProcessorBlock->InterruptTable;
    Interrupt->NextInterrupt = InterruptTable[ArrayIndex];
    KeLowerRunLevel(OldRunLevel);

    //
    // Make sure the new interrupt's pointer is visible everywhere before
    // linking it in.
    //

    RtlMemoryBarrier();

    //
    // Link it in.
    //

    InterruptTable[ArrayIndex] = Interrupt;
    HlpInterruptReleaseLock();
    return STATUS_SUCCESS;
}

VOID
HlDisconnectInterrupt (
    PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine removes an interrupt service routine from active duty. When
    this call is completed, no new interrupts will come in for this device and
    vector.

Arguments:

    Interrupt - Supplies a pointer to the initialized interrupt.

Return Value:

    None.

--*/

{

    ULONG ArrayIndex;
    PKINTERRUPT *InterruptTable;
    RUNLEVEL OldRunLevel;
    PKINTERRUPT Previous;
    ULONG Processor;
    PPROCESSOR_BLOCK ProcessorBlock;
    ULONG ProcessorCount;
    PKINTERRUPT Search;
    ULONGLONG Timeout;
    volatile RUNLEVEL *VolatileRunLevel;

    //
    // Use the global lock to synchronize with other processors connecting and
    // disconnecting.
    //

    ArrayIndex = Interrupt->Vector - HlFirstConfigurableVector;
    HlpInterruptAcquireLock();
    OldRunLevel = KeRaiseRunLevel(RunLevelDispatch);
    ProcessorBlock = KeGetCurrentProcessorBlock();
    InterruptTable = (PKINTERRUPT *)ProcessorBlock->InterruptTable;

    //
    // Find the interrupt in the singly linked list.
    //

    Previous = NULL;
    Search = InterruptTable[ArrayIndex];
    while ((Search != Interrupt) && (Search != NULL)) {
        Previous = Search;
        Search = Search->NextInterrupt;
    }

    if (Search == NULL) {
        HlpInterruptReleaseLock();
        KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                      HL_CRASH_INVALID_INTERRUPT_DISCONNECT,
                      (UINTN)Interrupt,
                      (UINTN)InterruptTable,
                      ArrayIndex);
    }

    //
    // Unlink the interrupt.
    //

    if (Previous != NULL) {
        Previous->NextInterrupt = Interrupt->NextInterrupt;

    } else {
        InterruptTable[ArrayIndex] = Interrupt->NextInterrupt;
    }

    KeLowerRunLevel(OldRunLevel);
    HlpInterruptReleaseLock();

    //
    // The current runlevel had better be at or below dispatch otherwise
    // processors could be spinning at interrupt level waiting for this one to
    // do something.
    //

    ASSERT((ArAreInterruptsEnabled() != FALSE) &&
           (KeGetRunLevel() <= RunLevelDispatch));

    //
    // Other processors could still be looking at this interrupt. Wait for
    // each one to run some code lower than the interrupt's runlevel to
    // ensure the interrupt is not running.
    //

    ProcessorCount = KeGetActiveProcessorCount();
    Timeout = KeGetRecentTimeCounter() +
              (HlQueryTimeCounterFrequency() * INTERRUPT_COMPLETION_TIMEOUT);

    for (Processor = 0; Processor < ProcessorCount; Processor += 1) {
        ProcessorBlock = KeGetProcessorBlock(Processor);

        ASSERT(ProcessorBlock != NULL);

        VolatileRunLevel = (volatile RUNLEVEL *)&(ProcessorBlock->RunLevel);
        while (*VolatileRunLevel >= Interrupt->RunLevel) {
            if (KeGetRecentTimeCounter() > Timeout) {
                KeCrashSystem(CRASH_HARDWARE_LAYER_FAILURE,
                              HL_CRASH_PROCESSOR_HUNG,
                              (UINTN)ProcessorBlock,
                              Processor,
                              (UINTN)Interrupt);
            }
        }
    }

    //
    // Finally the interrupt is clear.
    //

    Interrupt->NextInterrupt = NULL;
    if (Interrupt->Dpc != NULL) {
        KeFlushDpc(Interrupt->Dpc);
    }

    if (Interrupt->WorkItem != NULL) {
        KeFlushWorkItem(Interrupt->WorkItem);
    }

    return;
}

KSTATUS
HlEnableInterruptLine (
    ULONGLONG GlobalSystemInterruptNumber,
    PINTERRUPT_LINE_STATE LineState,
    PKINTERRUPT Interrupt,
    PVOID ResourceData,
    UINTN ResourceDataSize
    )

/*++

Routine Description:

    This routine enables the given interrupt line.

Arguments:

    GlobalSystemInterruptNumber - Supplies the global system interrupt number
        to enable.

    LineState - Supplies a pointer to the desired line state. Only the mode,
        polarity and flags are required by this routine.

    TriggerMode - Supplies the trigger mode of the interrupt.

    Polarity - Supplies the polarity of the interrupt.

    LineStateFlags - Supplies additional line state flags to set. The flags
        INTERRUPT_LINE_STATE_FLAG_ENABLED will be ORed in automatically.

    Interrupt - Supplies a pointer to the interrupt structure this line will
        be connected to.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

{

    INTERRUPT_LINE Line;
    KSTATUS Status;
    PROCESSOR_SET Target;

    RtlZeroMemory(&Line, sizeof(INTERRUPT_LINE));
    RtlZeroMemory(&Target, sizeof(PROCESSOR_SET));
    Line.Type = InterruptLineGsi;
    Line.U.Gsi = GlobalSystemInterruptNumber;
    Target.Target = ProcessorTargetAny;
    HlpInterruptGetStandardCpuLine(&(LineState->Output));
    LineState->Flags |= INTERRUPT_LINE_STATE_FLAG_ENABLED |
                        INTERRUPT_LINE_STATE_FLAG_LOWEST_PRIORITY;

    HlpInterruptAcquireLock();
    Status = HlpInterruptSetLineState(&Line,
                                      LineState,
                                      Interrupt,
                                      &Target,
                                      ResourceData,
                                      ResourceDataSize);

    HlpInterruptReleaseLock();
    return Status;
}

VOID
HlDisableInterruptLine (
    PKINTERRUPT Interrupt
    )

/*++

Routine Description:

    This routine disables the given interrupt line. Note that if the line is
    being shared by multiple interrupts, it may stay open for the other
    devices connected to it.

Arguments:

    Interrupt - Supplies a pointer to the interrupt line to disconnect.

Return Value:

    None.

--*/

{

    KSTATUS Status;

    //
    // Not all interrupts have a valid line. For example, MSI-based interrupts
    // do not have an interrupt line.
    //

    if (Interrupt->Line.Type == InterruptLineInvalid) {
        return;
    }

    HlpInterruptAcquireLock();
    Status = HlpInterruptSetLineState(&(Interrupt->Line),
                                      NULL,
                                      Interrupt,
                                      NULL,
                                      NULL,
                                      0);

    HlpInterruptReleaseLock();

    //
    // Disconnecting shouldn't fail.
    //

    ASSERT(KSUCCESS(Status));

    return;
}

KERNEL_API
KSTATUS
HlGetMsiInformation (
    ULONGLONG Vector,
    ULONGLONG VectorCount,
    PPROCESSOR_SET Processors,
    PMSI_INFORMATION Information
    )

/*++

Routine Description:

    This routine gathers the appropriate MSI/MSI-X address and data information
    for the given set of contiguous interrupt vectors.

Arguments:

    Vector - Supplies the first vector for which information is being requested.

    VectorCount - Supplies the number of contiguous vectors for which
        information is being requested.

    Processors - Supplies the set of processors that the MSIs should utilize.

    Information - Supplies a pointer to an array of MSI/MSI-X information to
        be filled in by the routine.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;
    ULONG Flags;
    PINTERRUPT_GET_MESSAGE_INFORMATION GetMessageInformation;
    INTERRUPT_LINE OutputLine;
    KSTATUS Status;
    INTERRUPT_HARDWARE_TARGET Target;

    //
    // Compute the interrupt target in terms the hardware can understand.
    //

    Status = HlpInterruptConvertProcessorSetToInterruptTarget(Processors,
                                                              &Target);

    if (!KSUCCESS(Status)) {
        goto GetMsiInformationEnd;
    }

    //
    // Get the default CPU interrupt line and associated flags.
    //

    HlpInterruptGetStandardCpuLine(&OutputLine);
    Flags = INTERRUPT_LINE_STATE_FLAG_LOWEST_PRIORITY;

    //
    // Find an interrupt controller that supports MSI/MSI-X. There should
    // really only ever be one.
    //

    Status = STATUS_NOT_SUPPORTED;
    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if (Controller == NULL) {
            continue;
        }

        GetMessageInformation = Controller->FunctionTable.GetMessageInformation;
        if (GetMessageInformation != NULL) {
            Status = GetMessageInformation(Vector,
                                           VectorCount,
                                           &Target,
                                           &OutputLine,
                                           Flags,
                                           Information);

            break;
        }
    }

GetMsiInformationEnd:
    return Status;
}

KSTATUS
HlpInitializeInterrupts (
    PKERNEL_INITIALIZATION_BLOCK Parameters
    )

/*++

Routine Description:

    This routine initializes the interrupt subsystem.

Arguments:

    Parameters - Supplies a pointer to the kernel's initialization information.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;
    PPROCESSOR_BLOCK ProcessorBlock;
    KSTATUS Status;

    ProcessorBlock = KeGetCurrentProcessorBlock();

    //
    // Initialize the interrupt queues.
    //

    ProcessorBlock->PendingInterruptCount = 0;

    //
    // If on the boot processor, do the one time initialization steps.
    //

    if (KeGetCurrentProcessorNumber() == 0) {
        HlInterruptLock = KeCreateQueuedLock();
        if (HlInterruptLock == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto InitializeInterruptsEnd;
        }

        //
        // Perform architecture-specific initialization.
        //

        Status = HlpArchInitializeInterrupts();
        if (!KSUCCESS(Status)) {
            goto InitializeInterruptsEnd;
        }

        //
        // Initialize all controllers.
        //

        ControllerCount = HlInterruptControllerCount;
        for (ControllerIndex = 0;
             ControllerIndex < ControllerCount;
             ControllerIndex += 1) {

            Controller = HlInterruptControllers[ControllerIndex];
            if (Controller == NULL) {
                continue;
            }

            Status = HlpInterruptInitializeController(Controller);
            if (!KSUCCESS(Status)) {
                goto InitializeInterruptsEnd;
            }
        }

        //
        // Initialize IPIs.
        //

        Status = HlpInitializeIpis();
        if (!KSUCCESS(Status)) {
            goto InitializeInterruptsEnd;
        }
    }

    //
    // Initialize the local units of all controllers. This code is run on all
    // processors. P0 is included here because the processor targeting wasn't
    // set up the first time around, as IPIs weren't initialized.
    //

    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if (Controller == NULL) {
            continue;
        }

        Status = HlpInterruptInitializeLocalUnit(Controller);
        if (!KSUCCESS(Status)) {
            goto InitializeInterruptsEnd;
        }
    }

    Status = STATUS_SUCCESS;

InitializeInterruptsEnd:
    return Status;
}

KSTATUS
HlpInterruptRegisterHardware (
    PINTERRUPT_CONTROLLER_DESCRIPTION ControllerDescription,
    RUNLEVEL RunLevel,
    PINTERRUPT_CONTROLLER *NewController
    )

/*++

Routine Description:

    This routine is called to register a new interrupt controller with the
    system.

Arguments:

    ControllerDescription - Supplies a pointer describing the new interrupt
        controller.

    RunLevel - Supplies the runlevel that all interrupts from this controller
        come in on. Set to RunLevelCount if this interrupt controller is wired
        directly to the processor.

    NewController - Supplies an optional pointer where a pointer to the
        newly created interrupt controller will be returned on success.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    UINTN SaveSize;
    KSTATUS Status;

    Controller = NULL;

    //
    // Check the table version.
    //

    if (ControllerDescription->TableVersion <
        INTERRUPT_CONTROLLER_DESCRIPTION_VERSION) {

        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterHardwareEnd;
    }

    //
    // Check required function pointers.
    //

    if ((ControllerDescription->FunctionTable.InitializeIoUnit == NULL) ||
        (ControllerDescription->FunctionTable.SetLineState == NULL) ||
        (ControllerDescription->FunctionTable.MaskLine == NULL)) {

        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterHardwareEnd;
    }

    //
    // If the number of processors is non-zero then the enumerate and set
    // addressing functions are required.
    //

    if ((ControllerDescription->ProcessorCount != 0) &&
        ((ControllerDescription->FunctionTable.EnumerateProcessors == NULL) ||
         (ControllerDescription->FunctionTable.InitializeLocalUnit == NULL) ||
         (ControllerDescription->FunctionTable.RequestInterrupt == NULL) ||
         (ControllerDescription->FunctionTable.StartProcessor == NULL) ||
         (ControllerDescription->FunctionTable.SetLocalUnitAddressing ==
                                                                      NULL))) {

        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterHardwareEnd;
    }

    //
    // A multi-processor controller must support at least 3 hardware priorities
    // so that the send and receive IPI levels are different.
    //

    if ((ControllerDescription->ProcessorCount != 0) &&
        (ControllerDescription->PriorityCount < 3)) {

        Status = STATUS_NOT_SUPPORTED;
        goto InterruptRegisterHardwareEnd;
    }

    //
    // Allocate the new controller object.
    //

    SaveSize = ControllerDescription->SaveContextSize;
    if ((SaveSize != 0) && (ControllerDescription->ProcessorCount != 0)) {
        SaveSize *= ControllerDescription->ProcessorCount;
    }

    Controller = MmAllocateNonPagedPool(sizeof(INTERRUPT_CONTROLLER) + SaveSize,
                                        HL_POOL_TAG);

    if (Controller == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InterruptRegisterHardwareEnd;
    }

    RtlZeroMemory(Controller, sizeof(INTERRUPT_CONTROLLER) + SaveSize);
    Controller->RunLevel = RunLevel;
    if (SaveSize != 0) {
        Controller->SaveSize = SaveSize;
        Controller->SaveRegion = Controller + 1;
    }

    INITIALIZE_LIST_HEAD(&(Controller->LinesHead));
    INITIALIZE_LIST_HEAD(&(Controller->OutputLinesHead));

    //
    // Initialize the new controller based on the description.
    //

    RtlCopyMemory(&(Controller->FunctionTable),
                  &(ControllerDescription->FunctionTable),
                  sizeof(INTERRUPT_FUNCTION_TABLE));

    Controller->Identifier = ControllerDescription->Identifier;
    Controller->ProcessorCount = ControllerDescription->ProcessorCount;
    Controller->PrivateContext = ControllerDescription->Context;
    Controller->PriorityCount = ControllerDescription->PriorityCount;
    Controller->Flags = 0;
    Controller->Features = ControllerDescription->Flags;

    //
    // Insert the controller on the array. Synchronization here comes from the
    // fact that 1) during early init everything is single threaded and
    // 2) later calls coming from create interrupt controller are synchronized
    // with the interrupt lock.
    //

    Status = STATUS_SUCCESS;
    if (HlInterruptControllerCount < MAX_INTERRUPT_CONTROLLERS) {
        HlInterruptControllers[HlInterruptControllerCount] = Controller;
        HlInterruptControllerCount += 1;

    } else {
        Status = STATUS_BUFFER_FULL;
    }

    if (NewController != NULL) {
        *NewController = Controller;
    }

InterruptRegisterHardwareEnd:
    if (!KSUCCESS(Status)) {
        if (Controller != NULL) {
            MmFreeNonPagedPool(Controller);
        }
    }

    return Status;
}

KSTATUS
HlpInterruptRegisterLines (
    PINTERRUPT_LINES_DESCRIPTION LinesDescription
    )

/*++

Routine Description:

    This routine is called to register one or more interrupt lines onto an
    interrupt controller.

Arguments:

    LinesDescription - Supplies a pointer to a structure describing the
        segment of interrupt lines.

Return Value:

    Status code.

--*/

{

    ULONG AllocationSize;
    PINTERRUPT_CONTROLLER Controller;
    PLIST_ENTRY CurrentEntry;
    ULONG ExistingLineCount;
    ULONG LineCount;
    PINTERRUPT_LINES Lines;
    PLIST_ENTRY ListHead;
    PINTERRUPT_LINES NewLines;
    BOOL Overlaps;
    KSTATUS Status;

    if ((LinesDescription == NULL) ||
        (LinesDescription->Version < INTERRUPT_LINES_DESCRIPTION_VERSION)) {

        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterLinesEnd;
    }

    //
    // Fail if the structure describes zero or fewer lines.
    //

    if (LinesDescription->LineEnd <= LinesDescription->LineStart) {
        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterLinesEnd;
    }

    LineCount = LinesDescription->LineEnd - LinesDescription->LineStart;

    //
    // Find the controller the lines are describing.
    //

    Controller =
           HlpInterruptGetControllerByIdentifier(LinesDescription->Controller);

    if (Controller == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto InterruptRegisterLinesEnd;
    }

    //
    // Determine which list this segment of lines belongs on.
    //

    if (LinesDescription->Type == InterruptLinesOutput) {
        ListHead = &(Controller->OutputLinesHead);

    } else {
        ListHead = &(Controller->LinesHead);
    }

    //
    // Cruise through the list to ensure this segment doesn't overlap any
    // other declared ranges.
    //

    CurrentEntry = ListHead->Next;
    while (CurrentEntry != ListHead) {
        Lines = LIST_VALUE(CurrentEntry, INTERRUPT_LINES, ListEntry);
        CurrentEntry = CurrentEntry->Next;

        //
        // Start by assuming they DO overlap.
        //

        Overlaps = TRUE;

        //
        // If the both the start and the end are beyond the boundaries of the
        // segments, then they don't overlap.
        //

        if (((Lines->LineStart < LinesDescription->LineStart) &&
             (Lines->LineEnd <= LinesDescription->LineStart)) ||
            (Lines->LineStart >= LinesDescription->LineEnd)) {

            Overlaps = FALSE;
        }

        if ((LinesDescription->Type == InterruptLinesOutput) &&
            (Lines->OutputControllerIdentifier !=
             LinesDescription->OutputControllerIdentifier)) {

            Overlaps = FALSE;
        }

        if (Overlaps != FALSE) {
            Status = STATUS_INVALID_PARAMETER;
            goto InterruptRegisterLinesEnd;
        }

        //
        // Also check the GSI range for overlaps (except for output lines).
        //

        if ((LinesDescription->Gsi != INTERRUPT_LINES_GSI_NONE) &&
            (Lines->Gsi != INTERRUPT_LINES_GSI_NONE) &&
            (LinesDescription->Type != InterruptLinesOutput)) {

            Overlaps = TRUE;
            ExistingLineCount = Lines->LineEnd - Lines->LineStart;
            if (((Lines->Gsi < LinesDescription->Gsi) &&
                 (Lines->Gsi + ExistingLineCount <= LinesDescription->Gsi)) ||
                (Lines->Gsi >= LinesDescription->Gsi + LineCount)) {

                Overlaps = FALSE;
            }

            if (Overlaps != FALSE) {
                Status = STATUS_INVALID_PARAMETER;
                goto InterruptRegisterLinesEnd;
            }
        }
    }

    //
    // The lines look good. Allocate and initialize the structure.
    //

    AllocationSize = sizeof(INTERRUPT_LINES) +
                     (LineCount * sizeof(INTERRUPT_LINE_INTERNAL_STATE));

    NewLines = MmAllocateNonPagedPool(AllocationSize, HL_POOL_TAG);
    if (NewLines == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto InterruptRegisterLinesEnd;
    }

    RtlZeroMemory(NewLines, AllocationSize);
    NewLines->Type = LinesDescription->Type;
    NewLines->LineStart = LinesDescription->LineStart;
    NewLines->LineEnd = LinesDescription->LineEnd;
    NewLines->Gsi = LinesDescription->Gsi;
    NewLines->OutputControllerIdentifier =
                                 LinesDescription->OutputControllerIdentifier;

    NewLines->State = (PINTERRUPT_LINE_INTERNAL_STATE)(NewLines + 1);

    //
    // Insert these lines onto the back of the list.
    //

    INSERT_BEFORE(&(NewLines->ListEntry), ListHead);
    Status = STATUS_SUCCESS;

InterruptRegisterLinesEnd:
    return Status;
}

PKINTERRUPT
HlpCreateAndConnectInternalInterrupt (
    ULONG Vector,
    RUNLEVEL RunLevel,
    PINTERRUPT_SERVICE_ROUTINE ServiceRoutine,
    PVOID Context
    )

/*++

Routine Description:

    This routine allocates, initializes, and connects an interrupt structure on
    behalf of the hardware layer.

Arguments:

    Vector - Supplies the vector to connect the service routine to.

    RunLevel - Supplies the runlevel to connect the service routine at.

    ServiceRoutine - Supplies the service routine to run when the interrupt
        fires.

    Context - Supplies a pointer's worth of context that is passed to the
        interrupt service routine when it is called.

Return Value:

    Returns a pointer to the KINTERRUPT structure on success.

    NULL on failure.

--*/

{

    PKINTERRUPT Interrupt;
    KSTATUS Status;

    //
    // Create the interrupt.
    //

    Interrupt = HlCreateInterrupt(Vector, ServiceRoutine, NULL, NULL, Context);
    if (Interrupt == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto CreateAndConnectInternalInterruptEnd;
    }

    Interrupt->RunLevel = RunLevel;

    //
    // Connect the interrupt, making it live.
    //

    Status = HlConnectInterrupt(Interrupt);
    if (!KSUCCESS(Status)) {
        goto CreateAndConnectInternalInterruptEnd;
    }

CreateAndConnectInternalInterruptEnd:
    if (!KSUCCESS(Status)) {
        if (Interrupt != NULL) {
            HlDestroyInterrupt(Interrupt);
            Interrupt = NULL;
        }
    }

    return Interrupt;
}

KSTATUS
HlpInterruptSetLineState (
    PINTERRUPT_LINE Line,
    PINTERRUPT_LINE_STATE State,
    PKINTERRUPT Interrupt,
    PPROCESSOR_SET Target,
    PVOID ResourceData,
    UINTN ResourceDataSize
    )

/*++

Routine Description:

    This routine sets the state of an interrupt line, enabling or disabling it
    and configuring it.

Arguments:

    Line - Supplies a pointer to the interrupt line to configure.

    State - Supplies an optional pointer to the line state to set. Only the
        mode, polarity, flags, and output line are used by this routine. This
        is not required when disabling an interrupt line.

    Interrupt - Supplies a pointer to the interrupt this line will be connected
        to.

    Target - Supplies a pointer to the set of processors that the interrupt
        should target.

    ResourceData - Supplies an optional pointer to the device specific resource
        data for the interrupt line.

    ResourceDataSize - Supplies the size of the resource data, in bytes.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG HardwarePriority;
    BOOL InterruptEnabled;
    ULONG LineOffset;
    PINTERRUPT_LINES Lines;
    BOOL ModifiedState;
    INTERRUPT_LINE_INTERNAL_STATE OldState;
    INTERRUPT_LINE SourceLine;
    KSTATUS Status;

    Controller = NULL;
    LineOffset = 0;
    Lines = NULL;
    ModifiedState = FALSE;
    SourceLine = *Line;
    InterruptEnabled = FALSE;
    if ((State != NULL) &&
        ((State->Flags & INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0)) {

        InterruptEnabled = TRUE;
    }

    //
    // Convert the interrupt line to a controller specified one if needed.
    //

    Status = HlpInterruptConvertLineToControllerSpecified(&SourceLine);
    if (!KSUCCESS(Status)) {
        goto InterruptSetLineStateEnd;
    }

    //
    // Copy this line information into the interrupt structure so that
    // disconnection works.
    //

    RtlCopyMemory(&(Interrupt->Line), &SourceLine, sizeof(INTERRUPT_LINE));

    ASSERT(SourceLine.Type == InterruptLineControllerSpecified);

    //
    // Get the controller and line segment associated with these lines.
    //

    Status = HlpInterruptFindLines(&SourceLine,
                                   &Controller,
                                   &Lines,
                                   &LineOffset);

    if (!KSUCCESS(Status)) {
        goto InterruptSetLineStateEnd;
    }

    //
    // Save the old state and mark that it needs to be reverted if failed.
    //

    RtlCopyMemory(&OldState,
                  &(Lines->State[LineOffset]),
                  sizeof(INTERRUPT_LINE_INTERNAL_STATE));

    ModifiedState = TRUE;

    //
    // The interrupt is being disabled. Unless this is the last one,
    // just decrement the reference count and return.
    //

    if (InterruptEnabled == FALSE) {

        ASSERT(Lines->State[LineOffset].ReferenceCount > 0);

        Lines->State[LineOffset].ReferenceCount -= 1;
        if ((Lines->State[LineOffset].ReferenceCount > 0) &&
            (Lines->Type == InterruptLinesStandardPin)) {

            Status = STATUS_SUCCESS;
            goto InterruptSetLineStateEnd;
        }

        //
        // If this was the last reference, re-program the line.
        //

        Lines->State[LineOffset].PublicState.Flags &=
                                            ~INTERRUPT_LINE_STATE_FLAG_ENABLED;

        Status = Controller->FunctionTable.SetLineState(
                                       Controller->PrivateContext,
                                       &SourceLine,
                                       &(Lines->State[LineOffset].PublicState),
                                       ResourceData,
                                       ResourceDataSize);

        if (!KSUCCESS(Status)) {
            goto InterruptSetLineStateEnd;
        }

        RtlZeroMemory(&(Lines->State[LineOffset]),
                      sizeof(INTERRUPT_LINE_INTERNAL_STATE));

        ModifiedState = FALSE;
        goto InterruptSetLineStateEnd;
    }

    //
    // If this is a primary interrupt controller (like the APIC or the GIC),
    // then the run-level is a function of the vector. If this is a secondary
    // interrupt controller, then this interrupt comes in at the same run-level
    // as its parent.
    //

    if (Controller->RunLevel == RunLevelCount) {
        Interrupt->RunLevel = VECTOR_TO_RUN_LEVEL(Interrupt->Vector);

    } else {
        Interrupt->RunLevel = Controller->RunLevel;
    }

    ASSERT(State != NULL);

    Interrupt->Mode = State->Mode;
    Interrupt->LastTimestamp = 0;
    Interrupt->InterruptCount = 0;
    Interrupt->Controller = Controller;

    //
    // This is an enable; adjust the reference count.
    //

    ASSERT(Lines->State[LineOffset].ReferenceCount >= 0);

    Lines->State[LineOffset].ReferenceCount += 1;
    if (Lines->State[LineOffset].ReferenceCount > 1) {

        //
        // The line had better already be programmed.
        //

        ASSERT((Lines->State[LineOffset].PublicState.Flags &
                INTERRUPT_LINE_STATE_FLAG_ENABLED) != 0);

        //
        // For standard interrupt lines, there's no need to program
        // them again.
        //

        if (Lines->Type == InterruptLinesStandardPin) {
            Status = STATUS_SUCCESS;
            goto InterruptSetLineStateEnd;
        }
    }

    //
    // Determine the line configuration.
    //

    Lines->State[LineOffset].PublicState.Flags = State->Flags;
    Lines->State[LineOffset].PublicState.Vector = Interrupt->Vector;
    Lines->State[LineOffset].PublicState.Mode = State->Mode;
    Lines->State[LineOffset].PublicState.Polarity = State->Polarity;
    Lines->State[LineOffset].Flags |=
                                   INTERRUPT_LINE_INTERNAL_STATE_FLAG_RESERVED;

    //
    // Figure out the output pin this interrupt line should go to.
    //

    Status = HlpInterruptDetermineRouting(
                               Controller,
                               &(State->Output),
                               &(Lines->State[LineOffset].PublicState.Output));

    if (!KSUCCESS(Status)) {
        goto InterruptSetLineStateEnd;
    }

    //
    // Convert the processor set to an interrupt target that the controller
    // can understand.
    //

    Status = HlpInterruptConvertProcessorSetToInterruptTarget(
                               Target,
                               &(Lines->State[LineOffset].PublicState.Target));

    if (!KSUCCESS(Status)) {
        goto InterruptSetLineStateEnd;
    }

    //
    // Get the hardware priority level corresponding to this run level
    // (inferred from the vector).
    //

    HardwarePriority = HlpInterruptConvertRunLevelToHardwarePriority(
                                                          Controller,
                                                          Interrupt->RunLevel);

    Lines->State[LineOffset].PublicState.HardwarePriority = HardwarePriority;

    //
    // Program the line state.
    //

    Status = Controller->FunctionTable.SetLineState(
                                       Controller->PrivateContext,
                                       &SourceLine,
                                       &(Lines->State[LineOffset].PublicState),
                                       ResourceData,
                                       ResourceDataSize);

    if (!KSUCCESS(Status)) {
        goto InterruptSetLineStateEnd;
    }

    //
    // The state is now consistent with the controller, so forget the need
    // to undo.
    //

    ModifiedState = FALSE;

    ASSERT(Lines->State[LineOffset].PublicState.Output.Type ==
           InterruptLineControllerSpecified);

    Status = STATUS_SUCCESS;

InterruptSetLineStateEnd:
    if (!KSUCCESS(Status)) {

        //
        // Restore the interrupt line state if needed.
        //

        if (ModifiedState != FALSE) {
            RtlCopyMemory(&(Lines->State[LineOffset]),
                          &OldState,
                          sizeof(INTERRUPT_LINE_INTERNAL_STATE));
        }
    }

    return Status;
}

KSTATUS
HlpInterruptFindLines (
    PINTERRUPT_LINE Line,
    PINTERRUPT_CONTROLLER *Controller,
    PINTERRUPT_LINES *Lines,
    PULONG Offset
    )

/*++

Routine Description:

    This routine locates the controller, interrupt line segment, and offset
    within that segment for a given interrupt line.

Arguments:

    Line - Supplies a pointer to the interrupt line to look up. This line must
        be controller specified.

    Controller - Supplies a pointer where the interrupt controller that owns
        this line will be returned.

    Lines - Supplies a pointer where a pointer to the interrupt line segment
        owning this line will be returned.

    Offset - Supplies a pointer where the offset of the beginning of the
        segment will be returned.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the line is not controller specified.

    STATUS_NOT_FOUND if the interrupt line could not be located.

--*/

{

    ULONG ControllerCount;
    ULONG ControllerIndex;
    PLIST_ENTRY CurrentLinesEntry;
    PINTERRUPT_CONTROLLER LineController;
    PINTERRUPT_LINES LineSegment;

    if (Line->Type != InterruptLineControllerSpecified) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Loop through every controller in the system.
    //

    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        LineController = HlInterruptControllers[ControllerIndex];
        if ((LineController == NULL) ||
            (LineController->Identifier != Line->U.Local.Controller)) {

            continue;
        }

        //
        // Loop through every segment of interrupt lines in the current
        // controller.
        //

        CurrentLinesEntry = LineController->LinesHead.Next;
        while (CurrentLinesEntry != &(LineController->LinesHead)) {
            LineSegment = LIST_VALUE(CurrentLinesEntry,
                                     INTERRUPT_LINES,
                                     ListEntry);

            CurrentLinesEntry = CurrentLinesEntry->Next;

            //
            // Check to see if this segment owns the line, and return it if so.
            //

            if ((Line->U.Local.Line >= LineSegment->LineStart) &&
                (Line->U.Local.Line < LineSegment->LineEnd)) {

                *Controller = LineController;
                *Lines = LineSegment;
                *Offset = Line->U.Local.Line - LineSegment->LineStart;
                return STATUS_SUCCESS;
            }
        }
    }

    //
    // Well, if the loop completed without returning, then no dice.
    //

    return STATUS_NOT_FOUND;
}

KSTATUS
HlpInterruptSaveState (
    VOID
    )

/*++

Routine Description:

    This routine saves the state of all interrupt controllers for this
    processor in preparation for a power transition.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG Count;
    ULONG Index;
    ULONG Processor;
    PVOID SaveBuffer;
    KSTATUS Status;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    Processor = KeGetCurrentProcessorNumber();
    Count = HlInterruptControllerCount;
    for (Index = 0; Index < Count; Index += 1) {
        Controller = HlInterruptControllers[Index];
        if (Controller->FunctionTable.SaveState != NULL) {
            SaveBuffer = Controller->SaveRegion +
                         (Processor * Controller->SaveSize);

            Status = Controller->FunctionTable.SaveState(
                                                    Controller->PrivateContext,
                                                    SaveBuffer);

            if (!KSUCCESS(Status)) {
                goto InterruptSaveStateEnd;
            }

            Controller->Flags |= INTERRUPT_CONTROLLER_FLAG_SAVED;
        }
    }

    Status = STATUS_SUCCESS;

InterruptSaveStateEnd:
    return Status;
}

KSTATUS
HlpInterruptRestoreState (
    VOID
    )

/*++

Routine Description:

    This routine restores the state of all interrupt controllers for this
    processor after a power transition has occurred.

Arguments:

    None.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG Count;
    ULONG Index;
    ULONG Processor;
    PVOID SaveBuffer;
    KSTATUS Status;

    ASSERT(ArAreInterruptsEnabled() == FALSE);

    Processor = KeGetCurrentProcessorNumber();
    Count = HlInterruptControllerCount;
    for (Index = 0; Index < Count; Index += 1) {
        Controller = HlInterruptControllers[Index];
        if ((Controller->Flags & INTERRUPT_CONTROLLER_FLAG_SAVED) != 0) {
            SaveBuffer = Controller->SaveRegion +
                         (Processor * Controller->SaveSize);

            Status = Controller->FunctionTable.RestoreState(
                                                    Controller->PrivateContext,
                                                    SaveBuffer);

            if (!KSUCCESS(Status)) {
                goto InterruptRestoreStateEnd;
            }

            Controller->Flags &= ~INTERRUPT_CONTROLLER_FLAG_SAVED;
        }
    }

    Status = STATUS_SUCCESS;

InterruptRestoreStateEnd:
    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
HlpInterruptInitializeController (
    PINTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes or reinitializes an interrupt controller.

Arguments:

    Controller - Supplies a pointer to the controller to initialize or
        reinitialize.

Return Value:

    Status code.

--*/

{

    PINTERRUPT_INITIALIZE_IO_UNIT InitializeIoUnit;
    KSTATUS Status;

    //
    // Perform normal initialization. Initialize the local unit first.
    //

    Status = HlpInterruptInitializeLocalUnit(Controller);
    if (!KSUCCESS(Status)) {
        goto InterruptInitializeControllerEnd;
    }

    //
    // Reinitialize the I/O Unit.
    //

    InitializeIoUnit = Controller->FunctionTable.InitializeIoUnit;
    Status = InitializeIoUnit(Controller->PrivateContext);
    if (!KSUCCESS(Status)) {
        goto InterruptInitializeControllerEnd;
    }

InterruptInitializeControllerEnd:
    if (KSUCCESS(Status)) {
        Controller->Flags &= ~INTERRUPT_CONTROLLER_FLAG_FAILED;
        Controller->Flags |= INTERRUPT_CONTROLLER_FLAG_INITIALIZED;

    } else {
        Controller->Flags &= ~INTERRUPT_CONTROLLER_FLAG_INITIALIZED;
        Controller->Flags |= INTERRUPT_CONTROLLER_FLAG_FAILED;
    }

    return Status;
}

KSTATUS
HlpInterruptInitializeLocalUnit (
    PINTERRUPT_CONTROLLER Controller
    )

/*++

Routine Description:

    This routine initializes or reinitializes the local unit of an interrupt
    controller.

Arguments:

    Controller - Supplies a pointer to the controller whose local unit should be
        initialized or reinitialized.

Return Value:

    Status code.

--*/

{

    ULONG Identifier;
    PINTERRUPT_INITIALIZE_LOCAL_UNIT InitializeLocalUnit;
    KSTATUS Status;

    InitializeLocalUnit = Controller->FunctionTable.InitializeLocalUnit;
    if (InitializeLocalUnit == NULL) {

        ASSERT(Controller->ProcessorCount <= 1);

        Status = STATUS_SUCCESS;
        goto InterruptInitializeLocalUnitEnd;
    }

    Status = InitializeLocalUnit(Controller->PrivateContext, &Identifier);
    if (!KSUCCESS(Status)) {
        goto InterruptInitializeLocalUnitEnd;
    }

    Status = HlpSetupProcessorAddressing(Identifier);
    if (!KSUCCESS(Status)) {
        goto InterruptInitializeLocalUnitEnd;
    }

InterruptInitializeLocalUnitEnd:
    return Status;
}

VOID
HlpInterruptAcquireLock (
    VOID
    )

/*++

Routine Description:

    This routine disables interrupts and acquires the interrupt controller lock.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ASSERT(KeGetRunLevel() == RunLevelLow);

    KeAcquireQueuedLock(HlInterruptLock);
    return;
}

VOID
HlpInterruptReleaseLock (
    VOID
    )

/*++

Routine Description:

    This routine releases the interrupt controller lock and restores interrupts.

Arguments:

    None.

Return Value:

    None.

--*/

{

    KeReleaseQueuedLock(HlInterruptLock);
    return;
}

PINTERRUPT_CONTROLLER
HlpInterruptGetControllerByIdentifier (
    UINTN Identifier
    )

/*++

Routine Description:

    This routine returns a pointer to the interrupt controller with the
    given identifier.

Arguments:

    Identifier - Supplies the identifier of the interrupt controller to return.

Return Value:

    Returns a pointer to the interrupt controller on success.

    NULL if no such controller exists with the given identifier.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;

    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if (Controller == NULL) {
            continue;
        }

        if (Controller->Identifier == Identifier) {
            return Controller;
        }
    }

    return NULL;
}

KSTATUS
HlpInterruptConvertLineToControllerSpecified (
    PINTERRUPT_LINE Line
    )

/*++

Routine Description:

    This routine converts an interrupt line into the "controller specified"
    form needed by interrupt controller modules.

Arguments:

    Line - Supplies a pointer to the line to convert. On output, will contain
        the controller specified form of the same line.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INVALID_PARAMETER if the input line specification is invalid.

    STATUS_NOT_FOUND if the line cannot be converted because no physical
        line exists.

--*/

{

    PINTERRUPT_CONTROLLER Controller;
    ULONG ControllerCount;
    ULONG ControllerIndex;
    PLIST_ENTRY CurrentLinesEntry;
    ULONG LineCount;
    PINTERRUPT_LINES Lines;

    //
    // If the line is already controller specified, there's nothing to do.
    //

    if (Line->Type == InterruptLineControllerSpecified) {
        return STATUS_SUCCESS;
    }

    //
    // If the line is not GSI specified, then the caller did something crazy.
    //

    if (Line->Type != InterruptLineGsi) {
        return STATUS_INVALID_PARAMETER;
    }

    //
    // Loop through every controller in the system.
    //

    ControllerCount = HlInterruptControllerCount;
    for (ControllerIndex = 0;
         ControllerIndex < ControllerCount;
         ControllerIndex += 1) {

        Controller = HlInterruptControllers[ControllerIndex];
        if (Controller == NULL) {
            continue;
        }

        //
        // Loop through every segment of interrupt lines in the current
        // controller.
        //

        CurrentLinesEntry = Controller->LinesHead.Next;
        while (CurrentLinesEntry != &(Controller->LinesHead)) {
            Lines = LIST_VALUE(CurrentLinesEntry, INTERRUPT_LINES, ListEntry);
            CurrentLinesEntry = CurrentLinesEntry->Next;

            //
            // If the segment has no GSI range, skip it.
            //

            if (Lines->Gsi == INTERRUPT_LINES_GSI_NONE) {
                continue;
            }

            //
            // Check to see if this segment owns the GSI, and return it if so.
            //

            LineCount = Lines->LineEnd - Lines->LineStart;
            if ((Line->U.Gsi >= Lines->Gsi) &&
                (Line->U.Gsi < Lines->Gsi + LineCount)) {

                //
                // This is a union, so be very careful about the order in
                // which these writes are done!
                //

                Line->U.Local.Line = Lines->LineStart +
                                     (Line->U.Gsi - Lines->Gsi);

                Line->U.Local.Controller = Controller->Identifier;
                Line->Type = InterruptLineControllerSpecified;
                return STATUS_SUCCESS;
            }
        }
    }

    //
    // Well, if the loop completed without returning, then no dice.
    //

    return STATUS_NOT_FOUND;
}

KSTATUS
HlpInterruptDetermineRouting (
    PINTERRUPT_CONTROLLER Controller,
    PINTERRUPT_LINE Destination,
    PINTERRUPT_LINE Route
    )

/*++

Routine Description:

    This routine determines the output line selection that should be used
    given a controller and a destination line.

Arguments:

    Controller - Supplies a pointer to the controller being routed from.

    Destination - Supplies a pointer to the destination line to reach.

    Route - Supplies a pointer to the route to use.

Return Value:

    Returns a hardware priority level that the interrupt controller can set
    for that run level.

--*/

{

    PINTERRUPT_LINES OutputLines;

    ASSERT(LIST_EMPTY(&(Controller->OutputLinesHead)) == FALSE);
    ASSERT(Destination->Type == InterruptLineControllerSpecified);

    OutputLines = LIST_VALUE(Controller->OutputLinesHead.Next,
                             INTERRUPT_LINES,
                             ListEntry);

    //
    // The current implementation assumes that an interrupt controller will
    // really only have one segment of output lines. First check to see if
    // the lines encapsulate the destination, and happily return if so.
    //

    if ((OutputLines->OutputControllerIdentifier ==
         Destination->U.Local.Controller) &&
        (Destination->U.Local.Line >= OutputLines->LineStart) &&
        (Destination->U.Local.Line < OutputLines->LineEnd)) {

        *Route = *Destination;
        return STATUS_SUCCESS;
    }

    //
    // The simplistic current implementation assumes that if the first segment
    // of lines aren't the destination, that it should route to the first line
    // of the first output segment.
    //

    ASSERT(OutputLines->LineEnd > OutputLines->LineStart);

    Route->Type = InterruptLineControllerSpecified;
    Route->U.Local.Controller = OutputLines->OutputControllerIdentifier;
    Route->U.Local.Line = OutputLines->LineStart;
    return STATUS_SUCCESS;
}

ULONG
HlpInterruptConvertRunLevelToHardwarePriority (
    PINTERRUPT_CONTROLLER Controller,
    RUNLEVEL RunLevel
    )

/*++

Routine Description:

    This routine converts an abstracted hardware priority like a run level into
    an actual hardware priority number that the given interrupt controller can
    program.

Arguments:

    Controller - Supplies a pointer to the interrupt controller the runlevel
        will be programmed to.

    RunLevel - Supplies a pointer to the desired runlevel to set.

Return Value:

    Returns a hardware priority level that the interrupt controller can set
    for that run level.

--*/

{

    if (Controller->PriorityCount == 0) {
        return 0;
    }

    if (RunLevelCount - RunLevel > Controller->PriorityCount) {
        return 0;
    }

    return Controller->PriorityCount - (RunLevelCount - RunLevel);
}

INTERRUPT_STATUS
HlpInterruptNullHandler (
    PVOID Context
    )

/*++

Routine Description:

    This routine represents an interrupt service routine that always returns
    the defer option, used for interrupts that simply cannot query their
    status without dropping down to low level.

Arguments:

    Context - Supplies an unused context pointer.

Return Value:

    InterruptStatusDefer always.

--*/

{

    return InterruptStatusDefer;
}

