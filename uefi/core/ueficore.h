/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ueficore.h

Abstract:

    This header contains internal definitions for the UEFI core.

Author:

    Evan Green 26-Feb-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Define away API decorators, as everything is linked statically.
//

#define RTL_API
#define KERNEL_API

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/kernel/arch.h>
#include <minoca/lib/rtl.h>
#include <uefifw.h>
#include "image.h"
#include "lock.h"
#include "devpath.h"
#include "handle.h"
#include "memory.h"
#include "runtime.h"
#include "shortcut.h"

//
// --------------------------------------------------------------------- Macros
//

#define EFI_UNPACK_UINT32(_Pointer)     \
    (((UINT8 *)_Pointer)[0] |           \
     (((UINT8 *)_Pointer)[1] << 8) |    \
     (((UINT8 *)_Pointer)[2] << 16) |   \
     (((UINT8 *)_Pointer)[3] << 24))

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

#if defined(EFI_X86)

typedef struct _EFI_JUMP_BUFFER {
    UINT32 Ebx;
    UINT32 Esi;
    UINT32 Edi;
    UINT32 Ebp;
    UINT32 Esp;
    UINT32 Eip;
} EFI_JUMP_BUFFER, *PEFI_JUMP_BUFFER;

#define EFI_JUMP_BUFFER_ALIGNMENT 4

#elif defined(EFI_ARM)

typedef struct _EFI_JUMP_BUFFER {
    UINT32 R3;
    UINT32 R4;
    UINT32 R5;
    UINT32 R6;
    UINT32 R7;
    UINT32 R8;
    UINT32 R9;
    UINT32 R10;
    UINT32 R11;
    UINT32 R12;
    UINT32 R14;
} EFI_JUMP_BUFFER, *PEFI_JUMP_BUFFER;

#define EFI_JUMP_BUFFER_ALIGNMENT 4

#else

#error Unsupported Architecture

#endif

//
// -------------------------------------------------------------------- Globals
//

extern EFI_TPL EfiCurrentTpl;

extern UINTN EfiEventsPending;

//
// Define timer services data.
//

extern UINT32 EfiClockTimerInterruptNumber;

//
// Store the runtime handoff information.
//

extern EFI_RUNTIME_ARCH_PROTOCOL *EfiRuntimeProtocol;

//
// Store the image handle of the firmware itself.
//

extern EFI_HANDLE EfiFirmwareImageHandle;

//
// -------------------------------------------------------- Function Prototypes
//

VOID
EfipInitializeProcessor (
    VOID
    );

/*++

Routine Description:

    This routine initializes processor-specific structures. In the case of x86,
    it initializes the GDT and TSS.

Arguments:

    None.

Return Value:

    None.

--*/

EFIAPI
EFI_TPL
EfiCoreRaiseTpl (
    EFI_TPL NewTpl
    );

/*++

Routine Description:

    This routine raises the current Task Priority Level.

Arguments:

    NewTpl - Supplies the new TPL to set.

Return Value:

    Returns the previous TPL.

--*/

EFIAPI
VOID
EfiCoreRestoreTpl (
    EFI_TPL OldTpl
    );

/*++

Routine Description:

    This routine restores the Task Priority Level back to its original value
    before it was raised.

Arguments:

    OldTpl - Supplies the original TPL to restore back to.

Return Value:

    None.

--*/

EFI_STATUS
EfiCoreInitializeInterruptServices (
    VOID
    );

/*++

Routine Description:

    This routine initializes core interrupt services.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfiCoreTerminateInterruptServices (
    VOID
    );

/*++

Routine Description:

    This routine terminates interrupt services in preparation for transitioning
    out of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
EfiCoreDispatchInterrupt (
    VOID
    );

/*++

Routine Description:

    This routine is called to service an interrupt.

Arguments:

    None.

Return Value:

    None.

--*/

EFIAPI
EFI_STATUS
EfiCoreGetNextMonotonicCount (
    UINT64 *Count
    );

/*++

Routine Description:

    This routine returns a monotonically increasing count for the platform.

Arguments:

    Count - Supplies a pointer where the next count is returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the count is NULL.

    EFI_DEVICE_ERROR if the device is not functioning properly.

--*/

EFIAPI
EFI_STATUS
EfiCoreStall (
    UINTN Microseconds
    );

/*++

Routine Description:

    This routine induces a fine-grained delay.

Arguments:

    Microseconds - Supplies the number of microseconds to stall execution for.

Return Value:

    EFI_SUCCESS on success.

--*/

EFIAPI
EFI_STATUS
EfiCoreSetWatchdogTimer (
    UINTN Timeout,
    UINT64 WatchdogCode,
    UINTN DataSize,
    CHAR16 *WatchdogData
    );

/*++

Routine Description:

    This routine sets the system's watchdog timer.

Arguments:

    Timeout - Supplies the number of seconds to set the timer for.

    WatchdogCode - Supplies a numeric code to log on a watchdog timeout event.

    DataSize - Supplies the size of the watchdog data.

    WatchdogData - Supplies an optional buffer that includes a null-terminated
        string, optionally followed by additional binary data.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the supplied watchdog code is invalid.

    EFI_UNSUPPORTED if there is no watchdog timer.

    EFI_DEVICE_ERROR if an error occurred accessing the device hardware.

--*/

UINT64
EfiCoreReadTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine reads the current time counter value.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value.

    0 if the time counter is not implemented.

--*/

UINT64
EfiCoreReadRecentTimeCounter (
    VOID
    );

/*++

Routine Description:

    This routine reads a relatively recent but not entirely up to date version
    of the time counter.

Arguments:

    None.

Return Value:

    Returns a 64-bit non-decreasing value.

    0 if the time counter is not implemented.

--*/

UINT64
EfiCoreGetTimeCounterFrequency (
    VOID
    );

/*++

Routine Description:

    This routine returns the frequency of the time counter.

Arguments:

    None.

Return Value:

    Returns the frequency of the time counter.

    0 if the time counter is not implemented.

--*/

VOID
EfiCoreServiceClockInterrupt (
    UINT32 InterruptNumber
    );

/*++

Routine Description:

    This routine is called to service the clock interrupt.

Arguments:

    InterruptNumber - Supplies the interrupt number that fired.

Return Value:

    None.

--*/

EFI_STATUS
EfiCoreInitializeTimerServices (
    VOID
    );

/*++

Routine Description:

    This routine initializes platform timer services, including the periodic
    tick and time counter.

Arguments:

    None.

Return Value:

    EFI status code.

--*/

VOID
EfiCoreTerminateTimerServices (
    VOID
    );

/*++

Routine Description:

    This routine terminates timer services in preparation for the termination
    of boot services.

Arguments:

    None.

Return Value:

    None.

--*/

EFIAPI
EFI_STATUS
EfiCoreCreateEvent (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_EVENT *Event
    );

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

EFIAPI
EFI_STATUS
EfiCoreCreateEventEx (
    UINT32 Type,
    EFI_TPL NotifyTpl,
    EFI_EVENT_NOTIFY NotifyFunction,
    VOID *NotifyContext,
    EFI_GUID *EventGroup,
    EFI_EVENT *Event
    );

/*++

Routine Description:

    This routine creates an event.

Arguments:

    Type - Supplies the type of event to create, as well as its mode and
        attributes.

    NotifyTpl - Supplies an optional task priority level of event notifications.

    NotifyFunction - Supplies an optional pointer to the event's notification
        function.

    NotifyContext - Supplies an optional context pointer that will be passed
        to the notify function when the event is signaled.

    EventGroup - Supplies an optional pointer to the unique identifier of the
        group to which this event belongs. If this is NULL, the function
        behaves as if the parameters were passed to the original create event
        function.

    Event - Supplies a pointer where the new event will be returned on success.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if one or more parameters are not valid.

    EFI_OUT_OF_RESOURCES if memory could not be allocated.

--*/

EFIAPI
EFI_STATUS
EfiCoreCloseEvent (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine closes an event.

Arguments:

    Event - Supplies the event to close.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is invalid.

--*/

EFIAPI
EFI_STATUS
EfiCoreSignalEvent (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine signals an event.

Arguments:

    Event - Supplies the event to signal.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the given event is not valid.

--*/

EFIAPI
EFI_STATUS
EfiCoreCheckEvent (
    EFI_EVENT Event
    );

/*++

Routine Description:

    This routine checks whether or not an event is in the signaled state.

Arguments:

    Event - Supplies the event to check.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_READY if the event is not signaled.

    EFI_INVALID_PARAMETER if the event is of type EVT_NOTIFY_SIGNAL.

--*/

EFIAPI
EFI_STATUS
EfiCoreWaitForEvent (
    UINTN NumberOfEvents,
    EFI_EVENT *Event,
    UINTN *Index
    );

/*++

Routine Description:

    This routine stops execution until an event is signaled.

Arguments:

    NumberOfEvents - Supplies the number of events in the event array.

    Event - Supplies the array of EFI_EVENTs.

    Index - Supplies a pointer where the index of the event which satisfied the
        wait will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the number of events is zero, or the event
    indicated by the index return parameter is of type EVT_NOTIFY_SIGNAL.

    EFI_UNSUPPORTED if the current TPL is not TPL_APPLICATION.

--*/

EFIAPI
EFI_STATUS
EfiCoreSetTimer (
    EFI_EVENT Event,
    EFI_TIMER_DELAY Type,
    UINT64 TriggerTime
    );

/*++

Routine Description:

    This routine sets the type of timer and trigger time for a timer event.

Arguments:

    Event - Supplies the timer to set.

    Type - Supplies the type of trigger to set.

    TriggerTime - Supplies the number of 100ns units until the timer expires.
        Zero is legal, and means the timer will be signaled on the next timer
        tick.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the event or type is not valid.

--*/

EFI_STATUS
EfiCoreInitializeEventServices (
    UINTN Phase
    );

/*++

Routine Description:

    This routine initializes event support.

Arguments:

    Phase - Supplies the initialization phase. Valid values are 0 and 1.

Return Value:

    EFI Status code.

--*/

VOID
EfiCoreDispatchEventNotifies (
    EFI_TPL Priority
    );

/*++

Routine Description:

    This routine dispatches all pending events.

Arguments:

    Priority - Supplies the task priority level of the event notifications to
        dispatch.

Return Value:

    None.

--*/

VOID
EfipCoreTimerTick (
    UINT64 CurrentTime
    );

/*++

Routine Description:

    This routine is called when a clock interrupt comes in.

Arguments:

    CurrentTime - Supplies the new current time.

Return Value:

    None.

--*/

VOID
EfipCoreNotifySignalList (
    EFI_GUID *EventGroup
    );

/*++

Routine Description:

    This routine signals all events in the given event group.

Arguments:

    EventGroup - Supplies a pointer to the GUID identifying the event group
        to signal.

Return Value:

    None.

--*/

EFIAPI
EFI_STATUS
EfiCoreConnectController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE *DriverImageHandle,
    EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath,
    BOOLEAN Recursive
    );

/*++

Routine Description:

    This routine connects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are connecting to.

    DriverImageHandle - Supplies a pointer to an ordered list of handles that
        support the EFI_DRIVER_BINDING_PROTOCOL.

    RemainingDevicePath - Supplies an optional pointer to the device path that
        specifies a child of the controller specified by the controller handle.

    Recursive - Supplies a boolean indicating if this routine should be called
        recursively until the entire tree of controllers below the specified
        controller has been connected. If FALSE, then the tree of controllers
        is only expanded one level.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if the controller handle is NULL.

    EFI_NOT_FOUND if either there are no EFI_DRIVER_BINDING_PROTOCOL instances
    present in the system, or no drivers were connected to the controller
    handle.

    EFI_SECURITY_VIOLATION if the user has no permission to start UEFI device
    drivers on the device associated with the controller handle or specified
    by the remaining device path.

--*/

EFIAPI
EFI_STATUS
EfiCoreDisconnectController (
    EFI_HANDLE ControllerHandle,
    EFI_HANDLE DriverImageHandle,
    EFI_HANDLE ChildHandle
    );

/*++

Routine Description:

    This routine disconnects one or more drivers to a controller.

Arguments:

    ControllerHandle - Supplies the handle of the controller which driver(s)
        are disconnecting from.

    DriverImageHandle - Supplies an optional pointer to the driver to
        disconnect from the controller. If NULL, all drivers are disconnected.

    ChildHandle - Supplies an optional pointer to the handle of the child to
        destroy.

Return Value:

    EFI_SUCCESS if one or more drivers were disconnected, no drivers are
    managing the handle, or a driver image handle was supplied and it is not
    controlling the given handle.

    EFI_INVALID_PARAMETER if the controller handle or driver handle is not a
    valid EFI handle, or the driver image handle doesn't support the
    EFI_DRIVER_BINDING_PROTOCOL.

    EFI_OUT_OF_RESOURCES if there are not enough resources are available to
    disconnect the controller(s).

    EFI_DEVICE_ERROR if the controller could not be disconnected because of a
    device error.

--*/

EFI_STATUS
EfiCoreInitializeImageServices (
    VOID *FirmwareBaseAddress,
    VOID *FirmwareLowestAddress,
    UINTN FirmwareSize
    );

/*++

Routine Description:

    This routine initializes image service support for the UEFI core.

Arguments:

    FirmwareBaseAddress - Supplies the base address where the firmware was
        loaded into memory. Supply -1 to indicate that the image is loaded at
        its preferred base address and was not relocated.

    FirmwareLowestAddress - Supplies the lowest address where the firmware was
        loaded into memory.

    FirmwareSize - Supplies the size of the firmware image in memory, in bytes.

Return Value:

    EFI Status code.

--*/

EFIAPI
EFI_STATUS
EfiCoreCalculateCrc32 (
    VOID *Data,
    UINTN DataSize,
    UINT32 *Crc32
    );

/*++

Routine Description:

    This routine computes the 32-bit CRC for a data buffer.

Arguments:

    Data - Supplies a pointer to the buffer to compute the CRC on.

    DataSize - Supplies the size of the data buffer in bytes.

    Crc32 - Supplies a pointer where the 32-bit CRC will be returned.

Return Value:

    EFI_SUCCESS on success.

    EFI_INVALID_PARAMETER if any parameter is NULL, or the data size is zero.

--*/

EFIAPI
EFI_STATUS
EfiCoreInstallConfigurationTable (
    EFI_GUID *Guid,
    VOID *Table
    );

/*++

Routine Description:

    This routine adds, updates, or removes a configuration table entry from the
    EFI System Table.

Arguments:

    Guid - Supplies a pointer to the GUID for the entry to add, update, or
        remove.

    Table - Supplies a pointer to the configuration table for the entry to add,
        update, or remove. This may be NULL.

Return Value:

    EFI_SUCCESS on success.

    EFI_NOT_FOUND if an attempt was made to delete a nonexistant entry.

    EFI_INVALID_PARAMETER if the GUID is NULL.

    EFI_OUT_OF_RESOURCES if an allocation failed.

--*/

EFIAPI
EFI_STATUS
EfiFvInitializeSectionExtraction (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine initializes the section extraction support for firmware
    volumes.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiFvInitializeBlockSupport (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine initializes the Firmware Volume Block I/O support module.

Arguments:

    ImageHandle - Supplies a pointer to the image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiFvDriverInit (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine initializes support for UEFI firmware volumes.

Arguments:

    ImageHandle - Supplies the image handle for this driver. This is probably
        the firmware core image handle.

    SystemTable - Supplies a pointer to the system table.

Return Value:

    EFI status code.

--*/

VOID
EfiCoreInitializeDispatcher (
    VOID
    );

/*++

Routine Description:

    This routine initializes the driver dispatcher.

Arguments:

    None.

Return Value:

    None.

--*/

EFIAPI
EFI_STATUS
EfiCoreDispatcher (
    VOID
    );

/*++

Routine Description:

    This routine runs the driver dispatcher. It drains the scheduled queue
    loading and starting drivers until there are no more drivers to run.

Arguments:

    None.

Return Value:

    EFI_SUCCESS if one or more drivers were loaded.

    EFI_NOT_FOUND if no drivers were loaded.

    EFI_ALREADY_STARTED if the dispatcher is already running.

--*/

EFIAPI
UINTN
EfipArchSetJump (
    PEFI_JUMP_BUFFER JumpBuffer
    );

/*++

Routine Description:

    This routine sets the context in the given jump buffer such that when
    long jump is called, execution continues at the return value from this
    routine with a non-zero return value.

Arguments:

    JumpBuffer - Supplies a pointer where the architecture-specific context
        will be saved.

Return Value:

    0 upon the initial return from this routine.

    Non-zero when returning as the target of a long jump.

--*/

EFIAPI
VOID
EfipArchLongJump (
    PEFI_JUMP_BUFFER JumpBuffer,
    UINTN Value
    );

/*++

Routine Description:

    This routine restores machine context to the state it was in when the
    set jump that saved into the given jump buffer was called. The return
    value will be set to the given value.

Arguments:

    JumpBuffer - Supplies a pointer to the context to restore.

    Value - Supplies the new return value to set from set jump. This should not
        be zero, otherwise the caller of set jump will not be able to
        differentiate it from its initial return.

Return Value:

    This routine does not return.

--*/

VOID
EfiCoreLoadVariablesFromFileSystem (
    VOID
    );

/*++

Routine Description:

    This routine loads variable data from the EFI system partition(s).

Arguments:

    None.

Return Value:

    None. Failure here is not fatal.

--*/

VOID
EfiCoreSaveVariablesToFileSystem (
    VOID
    );

/*++

Routine Description:

    This routine saves variable data to the EFI system partition(s).

Arguments:

    None.

Return Value:

    None. Failure here is not fatal.

--*/

EFIAPI
VOID
EfiBdsEntry (
    VOID
    );

/*++

Routine Description:

    This routine is the entry point into the boot device selection phase of
    the firmware. It attempts to find an OS loader and launch it.

Arguments:

    None.

Return Value:

    None. This routine does not return.

--*/

//
// Built-in drivers
//

EFIAPI
EFI_STATUS
EfiDiskIoDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is the entry point into the disk I/O driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiPartitionDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is the entry point into the partition driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiFatDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is the entry point into the disk I/O driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiGraphicsTextDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine initializes support for UEFI video consoles.

Arguments:

    ImageHandle - Supplies the image handle for this driver. This is probably
        the firmware core image handle.

    SystemTable - Supplies a pointer to the system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiAcpiDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is the entry point into the ACPI driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/

EFIAPI
EFI_STATUS
EfiSmbiosDriverEntry (
    EFI_HANDLE ImageHandle,
    EFI_SYSTEM_TABLE *SystemTable
    );

/*++

Routine Description:

    This routine is the entry point into the SMBIOS driver.

Arguments:

    ImageHandle - Supplies the driver image handle.

    SystemTable - Supplies a pointer to the EFI system table.

Return Value:

    EFI status code.

--*/
