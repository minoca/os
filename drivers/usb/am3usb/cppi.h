/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    cppi.h

Abstract:

    This header contains definitions for Texas Instruments CPPI 4.1 DMA
    controller support, usually integrated with a USB controller like the
    Mentor Graphics USB OTG controller.

Author:

    Evan Green 18-Sep-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// --------------------------------------------------------------------- Macros
//

//
// This macro converts from a USB endpoint to a zero-based DMA endpoint number.
//

#define CPPI_USB_ENDPOINT_TO_DMA(_Endpoint) (((_Endpoint) & 0xF) - 1)

//
// This macro converts a zero-based DMA endpoint number to a USB endpoint
// number (without the 0x80 bit that USB IN endpoints have).
//

#define CPPI_DMA_ENDPOINT_TO_USB(_DmaEndpoint) ((_DmaEndpoint) + 1)

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the current maximum number of instances supported.
//

#define CPPI_MAX_INSTANCES 2

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
VOID
(*PCPPI_ENDPOINT_COMPLETION) (
    PVOID Context,
    ULONG DmaEndpoint,
    BOOL Transmit
    );

/*++

Routine Description:

    This routine is called when CPPI receives an interrupt telling it that a
    queue completion occurred.

Arguments:

    Context - Supplies an opaque pointer's worth of context for the callback
        routine.

    DmaEndpoint - Supplies the zero-based DMA endpoint number. Add 1 to get
        to a USB endpoint number.

    Transmit - Supplies a boolean indicating if this is a transmit completion
        (TRUE) or a receive completion (FALSE).

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores the state for a CPPI DMA controller.

Members:

    ControllerBase - Stores the virtual address of the hardware registers.

    LinkRegionIoBuffer - Stores a pointer to the I/O buffer containing the
        Link region RAM, managed internally as scratch space by the CPPI
        controller.

    BlockAllocator - Stores a pointer to the block allocator of buffer and
        packet descriptors.

    DescriptorBase - Stores the physical address of the base DMA descriptor
        region. This is only 32 bits because that's all the controller can
        handle.

    TeardownLock - Stores a spin lock that serializes teardowns.

    CompletionRoutines - Stores pointers to functions to call when transfer
        completions occur.

    CompletionContexts - Stores pointers passed as parameters to the completion
        callback routines.

--*/

typedef struct _CPPI_DMA_CONTROLLER {
    PVOID ControllerBase;
    PIO_BUFFER LinkRegionIoBuffer;
    PBLOCK_ALLOCATOR BlockAllocator;
    ULONG DescriptorBase;
    KSPIN_LOCK TeardownLock;
    PCPPI_ENDPOINT_COMPLETION CompletionRoutines[CPPI_MAX_INSTANCES];
    PVOID CompletionContexts[CPPI_MAX_INSTANCES];
} CPPI_DMA_CONTROLLER, *PCPPI_DMA_CONTROLLER;

/*++

Structure Description:

    This structure stores the context for a CPPI DMA descriptor.

Members:

    Descriptor - Stores a pointer to the descriptor.

    Physical - Stores the physical address of the descriptor.

    Endpoint - Stores the zero-based (USB endpoint minus one) channel this
        descriptor is initialized for.

    Transmit - Stores a boolean indicating if this is a transmit (TRUE) or
        receive (FALSE) descriptor.

    Instance - Stores the instance number of the controller submitting the
        transfer.

    Submitted - Stores a boolean indicating whether the transfer is currently
        visible to hardware or not.

--*/

typedef struct _CPPI_DESCRIPTOR_DATA {
    PVOID Descriptor;
    ULONG Physical;
    UCHAR Endpoint;
    UCHAR Transmit;
    UCHAR Instance;
    UCHAR Submitted;
} CPPI_DESCRIPTOR_DATA, *PCPPI_DESCRIPTOR_DATA;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
CppiInitializeControllerState (
    PCPPI_DMA_CONTROLLER Controller,
    PVOID ControllerBase
    );

/*++

Routine Description:

    This routine initializes the CPPI DMA controller state structure.

Arguments:

    Controller - Supplies a pointer to the zeroed controller structure.

    ControllerBase - Supplies the virtual address of the base of the CPPI DMA
        register.

Return Value:

    Status code.

--*/

VOID
CppiDestroyControllerState (
    PCPPI_DMA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine tears down and frees all resources associated with the given
    CPPI DMA controller. The structure itself is owned by the caller.

Arguments:

    Controller - Supplies a pointer to the controller to destroy.

Return Value:

    None.

--*/

VOID
CppiRegisterCompletionCallback (
    PCPPI_DMA_CONTROLLER Controller,
    ULONG Instance,
    PCPPI_ENDPOINT_COMPLETION CallbackRoutine,
    PVOID CallbackContext
    );

/*++

Routine Description:

    This routine registers a DMA completion callback with the CPPI DMA
    controller.

Arguments:

    Controller - Supplies a pointer to the controller.

    Instance - Supplies the instance number.

    CallbackRoutine - Supplies the callback routine to call.

    CallbackContext - Supplies a pointer's worth of context that is passed into
        the callback routine.

Return Value:

    None.

--*/

KSTATUS
CppiResetController (
    PCPPI_DMA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine performs hardware initialization of the CPPI DMA controller.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

VOID
CppiInterruptServiceDispatch (
    PCPPI_DMA_CONTROLLER Controller
    );

/*++

Routine Description:

    This routine is called when a CPPI DMA interrupt occurs. It is called at
    dispatch level.

Arguments:

    Controller - Supplies a pointer to the controller.

Return Value:

    Status code.

--*/

KSTATUS
CppiCreateDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    ULONG Instance,
    PCPPI_DESCRIPTOR_DATA Descriptor
    );

/*++

Routine Description:

    This routine creates a DMA buffer descriptor, and initializes its immutable
    members.

Arguments:

    Controller - Supplies a pointer to the controller.

    Instance - Supplies the USB instance index.

    Descriptor - Supplies a pointer where the allocated descriptor will be
        returned on success.

Return Value:

    Status code.

--*/

VOID
CppiInitializeDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor,
    ULONG DmaEndpoint,
    BOOL Transmit,
    ULONG BufferPhysical,
    ULONG BufferSize
    );

/*++

Routine Description:

    This routine initializes the mutable context of a DMA descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor data.

    DmaEndpoint - Supplies the *zero* based endpoint number (USB endpoint minus
        one).

    Transmit - Supplies a boolean indicating whether or not this is a transmit
        operation (TRUE) or a receive operation (FALSE).

    BufferPhysical - Supplies the physical address of the data buffer.

    BufferSize - Supplies the size of the data buffer in bytes.

Return Value:

    None.

--*/

VOID
CppiDestroyDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Data
    );

/*++

Routine Description:

    This routine frees resources associated with a DMA descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Data - Supplies a pointer to the descriptor information.

Return Value:

    None.

--*/

VOID
CppiSubmitDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    );

/*++

Routine Description:

    This routine adds a descriptor to the DMA hardware queue in preparation for
    takeoff.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor to add to the hardware
        queue.

Return Value:

    Status code.

--*/

VOID
CppiReapCompletedDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor,
    PULONG CompletedSize
    );

/*++

Routine Description:

    This routine checks the descriptor and pulls it out of the completion queue.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor check for completion.

    CompletedSize - Supplies an optional pointer where the number of bytes in
        the packet that have completed is returned.

Return Value:

    None.

--*/

KSTATUS
CppiTearDownDescriptor (
    PCPPI_DMA_CONTROLLER Controller,
    PCPPI_DESCRIPTOR_DATA Descriptor
    );

/*++

Routine Description:

    This routine performs a teardown operation on the given descriptor.

Arguments:

    Controller - Supplies a pointer to the controller.

    Descriptor - Supplies a pointer to the descriptor to tear down.

Return Value:

    None.

--*/

//
// External functions called by CPPI
//

VOID
Am3UsbRequestTeardown (
    PCPPI_DMA_CONTROLLER CppiDma,
    ULONG Instance,
    ULONG Endpoint,
    BOOL Transmit
    );

/*++

Routine Description:

    This routine requests a teardown in the USBOTG control module.

Arguments:

    CppiDma - Supplies a pointer to the CPPI DMA controller.

    Instance - Supplies the USB instance number requesting a teardown.

    Endpoint - Supplies the zero-based DMA endpoint to tear down.

    Transmit - Supplies a boolean indicating whether this is a transmit (TRUE)
        or receive (FALSE) operation.

Return Value:

    None.

--*/

