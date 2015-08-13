/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    intrupt.h

Abstract:

    This header contains definitions for the hardware layer's interrupt
    support.

Author:

    Evan Green 19-Aug-2012

--*/

//
// --------------------------------------------------------------------- Macros
//

//
// This macro translates between a vector and its associated run level. Note
// that this association only holds for primary interrupts (that is, interrupts
// connected to the main interrupt controller).
//

#define VECTOR_TO_RUN_LEVEL(_Vector) ((_Vector) >> 4)

//
// ---------------------------------------------------------------- Definitions
//

//
// This special value indicates that instead of a context pointer, an ISR
// would like the trap frame.
//

#define INTERRUPT_CONTEXT_TRAP_FRAME ((PVOID)-1)

//
// Interrupt controller flags.
//

//
// This flag is set once the controller has been successfully initialized.
//

#define INTERRUPT_CONTROLLER_FLAG_INITIALIZED 0x00000001

//
// This flag is set if the controller has failed its initialization process.
//

#define INTERRUPT_CONTROLLER_FLAG_FAILED 0x00000002

//
// Internal interrupt line state flags.
//

//
// This flag is set if the interrupt line is reserved for use by the system.
//

#define INTERRUPT_LINE_INTERNAL_STATE_FLAG_RESERVED 0x00000001

//
// Define interrupt queue flags.
//

//
// This flag is atomically set to try and queue the DPC.
//

#define INTERRUPT_QUEUE_DPC_QUEUED 0x00000001

//
// This flag is atomically set to race to queue the work item.
//

#define INTERRUPT_QUEUE_WORK_ITEM_QUEUED 0x00000002

//
// This flag is atomically set if the interrupt was deferred and needs to be
// continued.
//

#define INTERRUPT_QUEUE_DEFERRED 0x00000004

//
// Define the maximum number of IPI lines any architecture will need.
//

#define MAX_IPI_LINE_COUNT 5

//
// Define the maximum number of interrupt controllers that can be in the
// system, including GPIO blocks.
//

#define MAX_INTERRUPT_CONTROLLERS 12

//
// Every once in awhile, figure out how long it took a batch of interrupts to
// fire. If they seem to be coming in too fast, report a storm.
//

#define INTERRUPT_STORM_COUNT_MASK 0x0001FFFF
#define INTERRUPT_STORM_DELTA_SECONDS 6

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes internal state for an interrupt line.

Members:

    PublicState - Stores the interrupt line state structure that is seen by
        the interrupt controller plugin.

    Flags - Stores a set of internal flags regarding the line.

    ReferenceCount - Stores the number of devices using the interrupt line.

--*/

typedef struct _INTERRUPT_LINE_INTERNAL_STATE {
    INTERRUPT_LINE_STATE PublicState;
    ULONG Flags;
    LONG ReferenceCount;
} INTERRUPT_LINE_INTERNAL_STATE, *PINTERRUPT_LINE_INTERNAL_STATE;

/*++

Structure Description:

    This structure describes the state of one or more interrupt lines.

Members:

    ListEntry - Stores pointers to the next and previous interrupt line
        segments.

    Type - Stores the general classification for this set of interrupt lines.

    LineStart - Stores the first line, inclusive, of the line segment being
        described.

    LineEnd - Stores one beyond the last line (aka exclusive) of the line
        segment being described.

    Gsi - Stores the GSI base for this range. The GSI number in this member
        corresponds to the interrupt line at LineStart. The GSI numbers go up
        consecutively through the rest of the segment. Specify
        INTERRUPT_LINES_GSI_NONE to indicate that the line segment has no GSI
        mapping.

    OutputControllerIdentifer - Supplies the identifier of the controller this
        line segment refers to. This field is only valid for output line
        segments, as the lines refer to the destination controller's source
        lines.

    State - Stores a pointer to an array of line state structures, one for
        each line described.

--*/

typedef struct _INTERRUPT_LINES {
    LIST_ENTRY ListEntry;
    INTERRUPT_LINES_TYPE Type;
    LONG LineStart;
    LONG LineEnd;
    ULONG Gsi;
    ULONG OutputControllerIdentifier;
    PINTERRUPT_LINE_INTERNAL_STATE State;
} INTERRUPT_LINES, *PINTERRUPT_LINES;

/*++

Structure Description:

    This structure information about an interrupt controller that has been
    registered with the system.

Members:

    FunctionTable - Stores pointers to functions implemented by the hardware
        module abstracting this interrupt controller.

    Flags - Stores pointers to a bitfield of flags defining state of the
        controller. See INTERRUPT_CONTROLLER_FLAG_* definitions.

    Identifier - Stores the unique hardware identifier of the interrupt
        controller.

    PrivateContext - Stores a pointer to the hardware module's private
        context.

    ProcessorCount - Stores the number of processors under the jurisdiction of
        this interrupt controller.

    LinesHead - Stores the list head for the list of interrupt line segments
        that this controller has.

    OutputLinesHead - Stores the list head of the list of output interrupt lines
        this controller outputs to.

    PriorityCount - Stores the number of hardware priority levels exist in the
        interrupt controller.

    RunLevel - Stores the run level that all interrupts occur at for this
        controller. This only applies to secondary interrupt controllers. For
        primary controllers (like the APIC and the GIC), this is set to
        RunLevelCount, indicating an invalid value.

    Features - Stores the bitfield of interurpt controller features. See
        INTERRUPT_FEATURE_* definitions.

--*/

struct _INTERRUPT_CONTROLLER {
    INTERRUPT_FUNCTION_TABLE FunctionTable;
    UINTN Identifier;
    ULONG Flags;
    PVOID PrivateContext;
    ULONG ProcessorCount;
    LIST_ENTRY LinesHead;
    LIST_ENTRY OutputLinesHead;
    ULONG PriorityCount;
    RUNLEVEL RunLevel;
    ULONG Features;
};

//
// -------------------------------------------------------------------- Globals
//

//
// Store the first vector number of the processor's interrupt array.
//

extern ULONG HlFirstConfigurableVector;

//
// Store the list of registered interrupt controller hardware.
//

extern PINTERRUPT_CONTROLLER HlInterruptControllers[MAX_INTERRUPT_CONTROLLERS];
extern ULONG HlInterruptControllerCount;

//
// Store the array of interrupts for each IPI type.
//

extern PKINTERRUPT HlIpiKInterrupt[MAX_IPI_LINE_COUNT];

//
// Store the maximum number of processors in the system.
//

extern ULONG HlMaxProcessors;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
HlpInterruptServiceDpc (
    PDPC Dpc
    );

/*++

Routine Description:

    This routine is called when an interrupt needs DPC service.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

VOID
HlpInterruptServiceWorker (
    PVOID Parameter
    );

/*++

Routine Description:

    This routine contains the generic interrupt service work item handler,
    which calls out to the low level service routine for the interrupt.

Arguments:

    Parameter - Supplies a context pointer, in this case a pointer to the
        KINTERRUPT.

Return Value:

    None.

--*/

KSTATUS
HlpInterruptRegisterHardware (
    PINTERRUPT_CONTROLLER_DESCRIPTION ControllerDescription,
    RUNLEVEL RunLevel,
    PINTERRUPT_CONTROLLER *NewController
    );

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

KSTATUS
HlpInterruptRegisterLines (
    PINTERRUPT_LINES_DESCRIPTION LinesDescription
    );

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

KSTATUS
HlpArchInitializeInterrupts (
    VOID
    );

/*++

Routine Description:

    This routine performs architecture-specific initialization for the interrupt
        subsystem.

Arguments:

    None.

Return Value:

    Status code.

--*/

ULONG
HlpInterruptGetIpiVector (
    IPI_TYPE IpiType
    );

/*++

Routine Description:

    This routine determines the architecture-specific hardware vector to use
    for the given IPI type.

Arguments:

    IpiType - Supplies the IPI type to send.

Return Value:

    Returns the vector that the given IPI type runs on.

--*/

ULONG
HlpInterruptGetRequiredIpiLineCount (
    VOID
    );

/*++

Routine Description:

    This routine determines the number of "software only" interrupt lines that
    are required for normal system operation. This routine is architecture
    dependent.

Arguments:

    None.

Return Value:

    Returns the number of software IPI lines needed for system operation.

--*/

ULONG
HlpInterruptGetVectorForIpiLineIndex (
    ULONG IpiLineIndex
    );

/*++

Routine Description:

    This routine maps an IPI line as reserved at boot to an interrupt vector.
    This routine is needed so that IPI support can know what vector to
    set the line state to for a given IPI line.

Arguments:

    IpiLineIndex - Supplies the index of the reserved IPI line.

Return Value:

    Returns the vector corresponding to the given index.

--*/

ULONG
HlpInterruptGetIpiLineIndex (
    IPI_TYPE IpiType
    );

/*++

Routine Description:

    This routine determines which of the IPI lines should be used for the
    given IPI type.

Arguments:

    IpiType - Supplies the type of IPI to be sent.

Return Value:

    Returns the IPI line index corresponding to the given IPI type.

--*/

VOID
HlpInterruptGetStandardCpuLine (
    PINTERRUPT_LINE Line
    );

/*++

Routine Description:

    This routine determines the architecture-specific standard CPU interrupt
    line that most interrupts get routed to.

Arguments:

    Line - Supplies a pointer where the standard CPU interrupt line will be
        returned.

Return Value:

    None.

--*/

INTERRUPT_CAUSE
HlpInterruptAcknowledge (
    PINTERRUPT_CONTROLLER *ProcessorController,
    PULONG Vector,
    PULONG MagicCandy
    );

/*++

Routine Description:

    This routine begins an interrupt, acknowledging its receipt into the
    processor.

Arguments:

    ProcessorController - Supplies a pointer where on input the interrupt
        controller that owns this processor will be supplied. This pointer may
        pointer to NULL, in which case the interrupt controller that fired the
        interrupt will be returned.

    Vector - Supplies a pointer to the vector on input. For non-vectored
        architectures, the vector corresponding to the interrupt that fired
        will be returned.

    MagicCandy - Supplies a pointer where an opaque token regarding the
        interrupt will be returned. This token is only used by the interrupt
        controller hardware module.

Return Value:

    Returns the cause of the interrupt.

--*/

PKINTERRUPT
HlpCreateAndConnectInternalInterrupt (
    ULONG Vector,
    RUNLEVEL RunLevel,
    PINTERRUPT_SERVICE_ROUTINE ServiceRoutine,
    PVOID Context
    );

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

KSTATUS
HlpInterruptSetLineState (
    PINTERRUPT_LINE Line,
    INTERRUPT_MODE Mode,
    INTERRUPT_ACTIVE_LEVEL Polarity,
    PKINTERRUPT Interrupt,
    PPROCESSOR_SET Target,
    PINTERRUPT_LINE OutputLine,
    ULONG Flags
    );

/*++

Routine Description:

    This routine sets the state of an interrupt line, enabling or disabling it
    and configuring it.

Arguments:

    Line - Supplies a pointer to the interrupt line to configure.

    Mode - Supplies the trigger mode of the interrupt.

    Polarity - Supplies the active level of the interrupt.

    Interrupt - Supplies a pointer to the interrupt this line will be conencted
        to.

    Target - Supplies a pointer to the set of processors that the interrupt
        should target.

    OutputLine - Supplies the output line this interrupt line should interrupt
        to.

    Flags - Supplies a bitfield of flags about the operation. See
        INTERRUPT_LINE_STATE_FLAG_* definitions.

Return Value:

    Status code.

--*/

KSTATUS
HlpInterruptFindLines (
    PINTERRUPT_LINE Line,
    PINTERRUPT_CONTROLLER *Controller,
    PINTERRUPT_LINES *Lines,
    PULONG Offset
    );

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

KSTATUS
HlpInitializeIpis (
    VOID
    );

/*++

Routine Description:

    This routine initialize IPI support in the system. It is called once on
    boot.

Arguments:

    None.

Return Value:

    Status code.

--*/

KSTATUS
HlpSetupProcessorAddressing (
    ULONG Identifier
    );

/*++

Routine Description:

    This routine prepares the system to receive IPIs on the current processor.

Arguments:

    Identifier - Supplies the physical identifier of the processor's local unit.

Return Value:

    Status code.

--*/

PINTERRUPT_CONTROLLER
HlpInterruptGetCurrentProcessorController (
    );

/*++

Routine Description:

    This routine returns the interrupt controller that owns the current
    processor.

Arguments:

    None.

Return Value:

    Returns a pointer to the interrupt controller responsible for this
    processor.

    NULL on a non-multiprocessor capable machine.

--*/

PINTERRUPT_CONTROLLER
HlpInterruptGetProcessorController (
    ULONG ProcessorIndex
    );

/*++

Routine Description:

    This routine returns the interrupt controller that owns the given processor.

Arguments:

    ProcessorIndex - Supplies the zero-based index of the processor whose
        interrupt controller is desired.

Return Value:

    Returns a pointer to the interrupt controller responsible for the given
    processor.

    NULL on a non-multiprocessor capable machine.

--*/

KSTATUS
HlpInterruptConvertProcessorSetToInterruptTarget (
    PPROCESSOR_SET ProcessorSet,
    PINTERRUPT_HARDWARE_TARGET Target
    );

/*++

Routine Description:

    This routine converts a generic processor set into an interrupt target.
    It may not be possible to target the interrupt at all processors specified,
    this routine will do what it can. On success, at least one processor in
    the set will be targeted. This routine will not target interrupts at a
    processor not mentioned in the set.

    This routine must be run at dispatch level or above.

Arguments:

    ProcessorSet - Supplies a pointer to the processor set.

    Target - Supplies a pointer where the interrupt hardware target will be
        returned.

Return Value:

    Status code.

--*/

PKINTERRUPT
HlpInterruptGetClockKInterrupt (
    VOID
    );

/*++

Routine Description:

    This routine returns the clock timer's KINTERRUPT structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the clock KINTERRUPT.

--*/

PKINTERRUPT
HlpInterruptGetProfilerKInterrupt (
    VOID
    );

/*++

Routine Description:

    This routine returns the profiler timer's KINTERRUPT structure.

Arguments:

    None.

Return Value:

    Returns a pointer to the profiler KINTERRUPT.

--*/

KSTATUS
HlpInterruptPrepareStartupStub (
    PPHYSICAL_ADDRESS JumpAddressPhysical,
    PVOID *JumpAddressVirtual,
    PULONG PagesAllocated
    );

/*++

Routine Description:

    This routine prepares the startup stub trampoline, used to bootstrap
    embryonic processors into the kernel.

Arguments:

    JumpAddressPhysical - Supplies a pointer that will receive the physical
        address the new processor(s) should jump to.

    JumpAddressVirtual - Supplies a pointer that will receive the virtual
        address the new processor(s) should jump to.

    PagesAllocated - Supplies a pointer that will receive the number of pages
        needed to create the startup stub.

Return Value:

    Status code.

--*/

VOID
HlpInterruptDestroyStartupStub (
    PHYSICAL_ADDRESS JumpAddressPhysical,
    PVOID JumpAddressVirtual,
    ULONG PageCount
    );

/*++

Routine Description:

    This routine destroys the startup stub trampoline, freeing all allocated
    resources.

Arguments:

    JumpAddressPhysical - Supplies the physical address of the startup stub.

    JumpAddressVirtual - Supplies the virtual address of the startup stub.

    PageCount - Supplies the number of pages in the startup stub.

Return Value:

    None.

--*/

KSTATUS
HlpInterruptPrepareForProcessorStart (
    ULONG ProcessorPhysicalIdentifier,
    PVOID ParkedAddressMapping,
    PHYSICAL_ADDRESS PhysicalJumpAddress,
    PVOID VirtualJumpAddress
    );

/*++

Routine Description:

    This routine performs any per-processor preparations necessary to start the
    given processor.

Arguments:

    ProcessorPhysicalIdentifier - Supplies the physical ID of the processor that
        is about to be started.

    ParkedAddressMapping - Supplies a pointer to the mapping to the processor's
        parked physical address.

    PhysicalJumpAddress - Supplies the physical address of the boot code this
        processor should jump to.

    VirtualJumpAddress - Supplies the virtual address of the boot code this
        processor should jump to.

Return Value:

    Status code.

--*/

