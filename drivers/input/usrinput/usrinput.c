/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usrinput.c

Abstract:

    This module implements the User Input library.

Author:

    Evan Green 16-Feb-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "inputp.h"
#include <minoca/lib/termlib.h>

//
// --------------------------------------------------------------------- Macros
//

//
// This macros defines the set of keys that do not repeat when held.
//

#define USER_INPUT_IS_NO_REPEAT_KEY(_Key)  \
    (((_Key) == KeyboardKeyPrintScreen) || \
     ((_Key) == KeyboardKeySysRq) ||       \
     ((_Key) == KeyboardKeyScrollLock) ||  \
     ((_Key) == KeyboardKeyBreak) ||       \
     ((_Key) == KeyboardKeyNumLock) ||     \
     ((_Key) == KeyboardKeyCapsLock) ||    \
     ((_Key) == KeyboardKeyLeftShift) ||   \
     ((_Key) == KeyboardKeyRightShift) ||  \
     ((_Key) == KeyboardKeyLeftAlt) ||     \
     ((_Key) == KeyboardKeyRightAlt) ||    \
     ((_Key) == KeyboardKeyApplication) || \
     ((_Key) == KeyboardKeyEscape))

//
// This macro defines the set of keys that should be repeated when held.
//

#define USER_INPUT_IS_REPEAT_KEY(_Key) !USER_INPUT_IS_NO_REPEAT_KEY(_Key)

//
// ---------------------------------------------------------------- Definitions
//

#define USER_INPUT_ALLOCATION_TAG 0x6E497355 // 'nIsU'

//
// Define how long to wait for the terminal buffer to clear up before throwing
// the input away.
//

#define USER_INPUT_TERMINAL_WAIT_TIME 50

//
// Define the size of the terminal input buffer size.
//

#define TERMINAL_INPUT_BUFFER_SIZE 1024

//
// Define terminal keyboard flags.
//

#define TERMINAL_KEYBOARD_SHIFT       0x00000001
#define TERMINAL_KEYBOARD_CONTROL     0x00000002
#define TERMINAL_KEYBOARD_CAPS_LOCK   0x00000004
#define TERMINAL_KEYBOARD_ALT         0x00000008
#define TERMINAL_KEYBOARD_NUM_LOCK    0x00000010
#define TERMINAL_KEYBOARD_SCROLL_LOCK 0x00000020

#define KEYBOARD_REPEAT_DELAY (500 * MICROSECONDS_PER_MILLISECOND)
#define KEYBOARD_REPEAT_RATE (50 * MICROSECONDS_PER_MILLISECOND)

//
// User input debug flags.
//

#define USER_INPUT_DEBUG_REGISTER       0x00000001
#define USER_INPUT_DEBUG_EVENT          0x00000002
#define USER_INPUT_DEBUG_REPEAT_EVENT   0x00000004
#define USER_INPUT_DEBUG_DISABLE_REPEAT 0x00000008

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure describes a user input device.

Members:

    ListEntry - Stores pointers to the next and previous user input devices.

    Type - Stores the device type.

    Identifier - Stores the unique identifier assigned to the device.

    EventCount - Stores the number of events this device has generated.

    Device - Stores a pointer to the OS device associated with this context.

    DeviceContext - Stores a pointer to the OS device's private context for the
        device.

    RepeatEvent - Stores a pointer to the event that is to be replayed when the
        repeat work item runs.

    RepeatTimer - Stores a pointer to the key repeat timer. For keyboards only.

    RepeatDpc - Stores a pointer to the key repeat DPC. For keyboards only.

    RepeatWorkItem - Stores a pointer to the key repeat work item. For
        keyboards only.

    KeyboardInterface - Stores the interface to a user keyboard device.

--*/

struct _USER_INPUT_DEVICE {
    LIST_ENTRY ListEntry;
    USER_INPUT_DEVICE_TYPE Type;
    ULONG Identifier;
    ULONG EventCount;
    PDEVICE Device;
    PVOID DeviceContext;
    PUSER_INPUT_EVENT RepeatEvent;
    PKTIMER RepeatTimer;
    PDPC RepeatDpc;
    PWORK_ITEM RepeatWorkItem;
    union {
        USER_INPUT_KEYBOARD_DEVICE_INTERFACE KeyboardInterface;
    } U;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
InUnloadDriver (
    PVOID Driver
    );

KSTATUS
InpProcessInputEvent (
    PUSER_INPUT_EVENT Event
    );

KSTATUS
InpProcessInputEventForTerminal (
    PUSER_INPUT_EVENT Event
    );

VOID
InpRepeatInputEventDpcRoutine (
    PDPC Dpc
    );

VOID
InpRepeatInputEventWorker (
    PVOID Parameter
    );

VOID
InpUpdateLedStateForTerminal (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER InDriver = NULL;

//
// Define the next ID for a new user input device.
//

volatile ULONG InNextDeviceId = 1;

//
// Define the next event ID.
//

volatile ULONG InNextEventId = 1;

//
// Store a pointer to the global input pipe.
//

PIO_HANDLE InUserInputPipe;

//
// Store a pointer to the master side of the local terminal.
//

PIO_HANDLE InLocalTerminal;

//
// Store the current terminal keyboard mask.
//

volatile ULONG InTerminalKeyboardMask = 0;

//
// Stores a bitfield of enabled user input debug flags. See USER_INPUT_DEBUG_*
// for definitions.
//

ULONG InDebugFlags = 0x0;

//
// Define user input type strings for debugging.
//

PSTR InDeviceTypeStrings[UserInputDeviceTypeCount] = {
    "INVALID",
    "Keyboard"
};

PSTR InEventTypeStrings[UserInputEventCount] = {
    "INVALID",
    "key down",
    "key up"
};

//
// Store a list of user input devices and a lock to protect the list.
//

PQUEUED_LOCK InDeviceListLock;
LIST_ENTRY InDeviceListHead;

//
// ------------------------------------------------------------------ Functions
//

__USED
KSTATUS
DriverEntry (
    PDRIVER Driver
    )

/*++

Routine Description:

    This routine is the entry point for the user input library. It performs
    library wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    FILE_PERMISSIONS Permissions;
    PIO_HANDLE ReadSide;
    KSTATUS Status;

    ASSERT((InDriver == NULL) && (InUserInputPipe == NULL));

    InDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = InUnloadDriver;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Allocate a queued lock to protect the list of devices.
    //

    INITIALIZE_LIST_HEAD(&InDeviceListHead);
    InDeviceListLock = KeCreateQueuedLock();
    if (InDeviceListLock == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto DriverEntryEnd;
    }

    //
    // Create the global input pipe.
    // TODO: This would probably make more sense as a local socket.
    //

    Permissions = FILE_PERMISSION_USER_READ | FILE_PERMISSION_GROUP_READ;
    Status = IoCreatePipe(TRUE,
                          NULL,
                          USER_INPUT_PIPE_NAME,
                          sizeof(USER_INPUT_PIPE_NAME),
                          0,
                          Permissions,
                          &ReadSide,
                          &InUserInputPipe);

    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Close the read side of the pipe.
    //

    IoClose(ReadSide);

    //
    // Get the master side of the local terminal.
    //

    Status = IoOpenLocalTerminalMaster(&InLocalTerminal);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Start with number lock enabled.
    //

    InTerminalKeyboardMask = TERMINAL_KEYBOARD_NUM_LOCK;
    Status = STATUS_SUCCESS;

DriverEntryEnd:
    return Status;
}

USER_INPUT_API
HANDLE
InRegisterInputDevice (
    PUSER_INPUT_DEVICE_DESCRIPTION Description
    )

/*++

Routine Description:

    This routine registers a new user input device.

Arguments:

    Description - Supplies a pointer to the description of the user input
        device being registered.

Return Value:

    Returns a handle to the user input device on success.

    INVALID_HANDLE on failure.

--*/

{

    PUSER_INPUT_DEVICE InputDevice;
    ULONG LedState;
    PUSER_INPUT_EVENT RepeatEvent;
    KSTATUS Status;

    InputDevice = INVALID_HANDLE;
    if (Description == NULL) {
        Status = STATUS_INVALID_PARAMETER;
        goto RegisterInputDeviceEnd;
    }

    if (Description->Type >= UserInputDeviceTypeCount) {
        Status = STATUS_INVALID_PARAMETER;
        goto RegisterInputDeviceEnd;
    }

    //
    // Check the interface version before proceeding.
    //

    if ((Description->Type == UserInputDeviceKeyboard) &&
        (Description->InterfaceVersion !=
         USER_INPUT_KEYBOARD_DEVICE_INTERFACE_VERSION)) {

        Status = STATUS_VERSION_MISMATCH;
        goto RegisterInputDeviceEnd;
    }

    //
    // Create the new input device.
    //

    InputDevice = MmAllocateNonPagedPool(sizeof(USER_INPUT_DEVICE),
                                         USER_INPUT_ALLOCATION_TAG);

    if (InputDevice == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RegisterInputDeviceEnd;
    }

    RtlZeroMemory(InputDevice, sizeof(USER_INPUT_DEVICE));
    InputDevice->Type = Description->Type;
    InputDevice->Identifier = RtlAtomicAdd32(&InNextDeviceId, 1);
    InputDevice->Device = Description->Device;
    InputDevice->DeviceContext = Description->DeviceContext;

    //
    // Copy the keyboard interface and create the keyboard repeat input event,
    // timer, DPC, and work item.
    //

    if (InputDevice->Type == UserInputDeviceKeyboard) {
        RtlCopyMemory(&(InputDevice->U.KeyboardInterface),
                      &(Description->U.KeyboardInterface),
                      sizeof(USER_INPUT_KEYBOARD_DEVICE_INTERFACE));

        RepeatEvent = MmAllocateNonPagedPool(sizeof(USER_INPUT_EVENT),
                                             USER_INPUT_ALLOCATION_TAG);

        if (RepeatEvent == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterInputDeviceEnd;
        }

        RepeatEvent->U.Key = KeyboardKeyInvalid;
        InputDevice->RepeatEvent = RepeatEvent;
        InputDevice->RepeatTimer = KeCreateTimer(USER_INPUT_ALLOCATION_TAG);
        if (InputDevice->RepeatTimer == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterInputDeviceEnd;
        }

        InputDevice->RepeatDpc = KeCreateDpc(InpRepeatInputEventDpcRoutine,
                                             InputDevice);

        if (InputDevice->RepeatDpc == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterInputDeviceEnd;
        }

        InputDevice->RepeatWorkItem = KeCreateWorkItem(
                                                    NULL,
                                                    WorkPriorityNormal,
                                                    InpRepeatInputEventWorker,
                                                    InputDevice,
                                                    USER_INPUT_ALLOCATION_TAG);

        if (InputDevice->RepeatWorkItem == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto RegisterInputDeviceEnd;
        }
    }

    //
    // Insert the device into the list lock. While the lock is held, set the
    // current LED state for any newly arrived keyboard. The terminal updates
    // the LED state underneath the list lock to reach all devices, so it must
    // be done under the lock here as well.
    //

    KeAcquireQueuedLock(InDeviceListLock);
    if ((InputDevice->Type == UserInputDeviceKeyboard) &&
        (InputDevice->U.KeyboardInterface.SetLedState != NULL)) {

        LedState = 0;
        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_NUM_LOCK) != 0) {
            LedState |= USER_INPUT_KEYBOARD_LED_NUM_LOCK;
        }

        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_CAPS_LOCK) != 0) {
            LedState |= USER_INPUT_KEYBOARD_LED_CAPS_LOCK;
        }

        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_SCROLL_LOCK) != 0) {
            LedState |= USER_INPUT_KEYBOARD_LED_SCROLL_LOCK;
        }

        InputDevice->U.KeyboardInterface.SetLedState(InputDevice->Device,
                                                     InputDevice->DeviceContext,
                                                     LedState);
    }

    INSERT_BEFORE(&(InputDevice->ListEntry), &InDeviceListHead);
    KeReleaseQueuedLock(InDeviceListLock);
    if ((InDebugFlags & USER_INPUT_DEBUG_REGISTER) != 0) {
        RtlDebugPrint("USIN: Registered %s Device (0x%08x), identifier: "
                      "0x%08x.\n",
                      InDeviceTypeStrings[InputDevice->Type],
                      InputDevice,
                      InputDevice->Identifier);
    }

    Status = STATUS_SUCCESS;

RegisterInputDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (InputDevice != INVALID_HANDLE) {
            if (InputDevice != NULL) {
                InDestroyInputDevice(InputDevice);
            }

            InputDevice = INVALID_HANDLE;
        }
    }

    return (HANDLE)InputDevice;
}

USER_INPUT_API
VOID
InDestroyInputDevice (
    HANDLE Handle
    )

/*++

Routine Description:

    This routine tears down state associated with a user input device created
    when the device was registered.

Arguments:

    Handle - Supplies the handle to the registered device. When this function
        returns, the handle will be invalid.

Return Value:

    None.

--*/

{

    PUSER_INPUT_DEVICE InputDevice;

    InputDevice = (PUSER_INPUT_DEVICE)Handle;
    KeAcquireQueuedLock(InDeviceListLock);
    LIST_REMOVE(&(InputDevice->ListEntry));
    KeReleaseQueuedLock(InDeviceListLock);
    if (InputDevice->RepeatEvent != NULL) {
        MmFreeNonPagedPool(InputDevice->RepeatEvent);
    }

    if (InputDevice->RepeatTimer != NULL) {
        KeDestroyTimer(InputDevice->RepeatTimer);
    }

    if (InputDevice->RepeatDpc != NULL) {
        KeDestroyDpc(InputDevice->RepeatDpc);
    }

    if (InputDevice->RepeatWorkItem != NULL) {
        KeDestroyWorkItem(InputDevice->RepeatWorkItem);
    }

    if ((InDebugFlags & USER_INPUT_DEBUG_REGISTER) != 0) {
        RtlDebugPrint("USIN: Destroyed %s Device (0x%08x), identifier: "
                      "0x%08x.\n",
                      InDeviceTypeStrings[InputDevice->Type],
                      InputDevice,
                      InputDevice->Identifier);
    }

    MmFreeNonPagedPool(InputDevice);
    return;
}

USER_INPUT_API
KSTATUS
InReportInputEvent (
    HANDLE Handle,
    PUSER_INPUT_EVENT Event
    )

/*++

Routine Description:

    This routine processes a new input event from the given device. This
    routine must be called at low level. The caller is expected to synchronize
    calls to report input for a device.

Arguments:

    Handle - Supplies the handle to the registered device reporting the event.

    Event - Supplies a pointer to the event that occurred. The caller must
        supply this buffer, but it will only be used for the duration of the
        routine (a copy will be made). The caller must fill out the event type
        and union, and should expect all other fields to be overwritten.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_FULL if the input pipe is full of events and this one was
        dropped.

--*/

{

    ULONGLONG DueTime;
    PUSER_INPUT_DEVICE InputDevice;
    ULONGLONG Period;
    BOOL Repeat;
    KSTATUS Status;

    InputDevice = (PUSER_INPUT_DEVICE)Handle;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Event->EventIdentifier = RtlAtomicAdd32(&InNextEventId, 1);
    Event->DeviceIdentifier = InputDevice->Identifier;
    Event->DeviceType = InputDevice->Type;

    //
    // Handle the repeat event for any keyboard devices.
    //

    Repeat = FALSE;
    if (Event->DeviceType == UserInputDeviceKeyboard) {

        //
        // Bring the repeat timer, DPC, and work item to a halt. Cancelling a
        // periodic timer only guarantees that the timer will not fire again.
        // Not much can be said about the associated DPC. So, flush it. If it
        // is queued, this will busy spin until it's done running, but it
        // shouldn't take too long.
        //

        if (InputDevice->RepeatEvent->U.Key != KeyboardKeyInvalid) {
            KeCancelTimer(InputDevice->RepeatTimer);
            KeFlushDpc(InputDevice->RepeatDpc);

            //
            // With the timer cancelled and DPC flushed, there is still the
            // work item to worry about. Try to cancel it. And if that fails,
            // flush it.
            //

            Status = KeCancelWorkItem(InputDevice->RepeatWorkItem);
            if (Status == STATUS_TOO_LATE) {
                KeFlushWorkItem(InputDevice->RepeatWorkItem);
            }
        }

        //
        // If this is a key down, then the new key becomes the repeat key if it
        // should be repeated. Otherwise the repeat remains cancelled.
        //

        if (Event->EventType == UserInputEventKeyDown) {
            if (USER_INPUT_IS_REPEAT_KEY(Event->U.Key) != FALSE) {
                RtlCopyMemory(InputDevice->RepeatEvent,
                              Event,
                              sizeof(USER_INPUT_EVENT));

                Repeat = TRUE;

            } else {
                InputDevice->RepeatEvent->U.Key = KeyboardKeyInvalid;
            }

        //
        // If this is a key up, do not restart the repeat if this is a key up
        // on the repeat key. Otherwise restart the repeat if there is a valid
        // repeat event.
        //

        } else {

            ASSERT(Event->EventType == UserInputEventKeyUp);

            if (Event->U.Key == InputDevice->RepeatEvent->U.Key) {
                InputDevice->RepeatEvent->U.Key = KeyboardKeyInvalid;

            } else if (InputDevice->RepeatEvent->U.Key != KeyboardKeyInvalid) {
                Repeat = TRUE;
            }
        }
    }

    ASSERT(Event->EventType < UserInputEventCount);

    Status = InpProcessInputEvent(Event);

    //
    // If there is an active keyboard repeat event, then queue it.
    //

    if ((Repeat != FALSE) &&
        ((InDebugFlags & USER_INPUT_DEBUG_DISABLE_REPEAT) == 0)) {

        ASSERT(Event->DeviceType == UserInputDeviceKeyboard);

        DueTime = HlQueryTimeCounter();
        DueTime += KeConvertMicrosecondsToTimeTicks(KEYBOARD_REPEAT_DELAY);
        Period = KeConvertMicrosecondsToTimeTicks(KEYBOARD_REPEAT_RATE);
        KeQueueTimer(InputDevice->RepeatTimer,
                     TimerQueueSoftWake,
                     DueTime,
                     Period,
                     0,
                     InputDevice->RepeatDpc);
    }

    if ((InDebugFlags & USER_INPUT_DEBUG_EVENT) != 0) {
        RtlDebugPrint("USIN: %s %s event processed with status %d: event "
                      "0x%08x, device 0x%08x, ",
                      InDeviceTypeStrings[Event->DeviceType],
                      InEventTypeStrings[Event->EventType],
                      Status,
                      Event->EventIdentifier,
                      Event->DeviceIdentifier);

        switch (Event->DeviceType) {
        case UserInputDeviceKeyboard:
            RtlDebugPrint("key %d.\n", Event->U.Key);
            break;

        default:
            RtlDebugPrint("no data.\n");
            break;
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
InUnloadDriver (
    PVOID Driver
    )

/*++

Routine Description:

    This routine is called before a driver is about to be unloaded from memory.
    The driver should take this opportunity to free any resources it may have
    set up in the driver entry routine.

Arguments:

    Driver - Supplies a pointer to the driver being torn down.

Return Value:

    None.

--*/

{

    if (InUserInputPipe != NULL) {
        IoClose(InUserInputPipe);
        InUserInputPipe = NULL;
    }

    if (InLocalTerminal != NULL) {
        IoClose(InLocalTerminal);
        InLocalTerminal = NULL;
    }

    if (InDeviceListLock != NULL) {
        KeDestroyQueuedLock(InDeviceListLock);
        InDeviceListLock = NULL;
    }

    return;
}

KSTATUS
InpProcessInputEvent (
    PUSER_INPUT_EVENT Event
    )

/*++

Routine Description:

    This routine processes an input event, sending it on to the terminal and
    the user input pipe.

Arguments:

    Event - Supplies a pointer to a user input event.

Return Value:

    Status code.

--*/

{

    UINTN BytesWritten;
    PIO_BUFFER IoBuffer;
    KSTATUS Status;

    Event->Timestamp = HlQueryTimeCounter();

    //
    // Create an I/O buffer for the write.
    //

    Status = MmCreateIoBuffer(Event,
                              sizeof(USER_INPUT_EVENT),
                              IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                              &IoBuffer);

    if (!KSUCCESS(Status)) {
        goto ProcessInputEventEnd;
    }

    //
    // Write the event out for anyone listening.
    //

    Status = IoWrite(InUserInputPipe,
                     IoBuffer,
                     sizeof(USER_INPUT_EVENT),
                     0,
                     0,
                     &BytesWritten);

    ASSERT((BytesWritten == 0) || (BytesWritten == sizeof(USER_INPUT_EVENT)));

    if (!KSUCCESS(Status)) {

        //
        // If sending it to the pipe failed, then forward it on to the terminal.
        //

        InpProcessInputEventForTerminal(Event);
        goto ProcessInputEventEnd;
    }

ProcessInputEventEnd:
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

KSTATUS
InpProcessInputEventForTerminal (
    PUSER_INPUT_EVENT Event
    )

/*++

Routine Description:

    This routine processes a new input event and writes it out to the terminal
    if applicable. This routine must be called at low level.

Arguments:

    Event - Supplies a pointer to the event that occurred.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_BUFFER_FULL if the input pipe is full of events and this one was
        dropped.

--*/

{

    UINTN BytesWritten;
    ULONG CharacterCount;
    CHAR Characters[TERMINAL_MAX_KEY_CHARACTERS + 1];
    ULONG ControlMask;
    PIO_BUFFER IoBuffer;
    INT RegularCharacter;
    BOOL Result;
    KSTATUS Status;
    TERMINAL_KEY_DATA TerminalKey;
    BOOL UpdateLedState;

    ControlMask = 0;
    CharacterCount = 0;
    RegularCharacter = -1;
    if (Event->DeviceType != UserInputDeviceKeyboard) {
        return STATUS_SUCCESS;
    }

    TerminalKey.Flags = 0;
    TerminalKey.Key = TerminalKeyInvalid;

    //
    // First handle key up events. There is nothing to write to the terminal
    // for such events, but the control key mask may need to change.
    //

    if (Event->EventType == UserInputEventKeyUp) {
        switch (Event->U.Key) {
        case KeyboardKeyLeftControl:
        case KeyboardKeyRightControl:
            ControlMask = TERMINAL_KEYBOARD_CONTROL;
            break;

        case KeyboardKeyLeftShift:
        case KeyboardKeyRightShift:
            ControlMask = TERMINAL_KEYBOARD_SHIFT;
            break;

        case KeyboardKeyLeftAlt:
        case KeyboardKeyRightAlt:
            ControlMask = TERMINAL_KEYBOARD_ALT;
            break;

        default:
            break;
        }

        if (ControlMask != 0) {
            RtlAtomicAnd32(&InTerminalKeyboardMask, ~ControlMask);
        }

        return STATUS_SUCCESS;

    //
    // Events other than key down and key up are ignored.
    //

    } else if (Event->EventType != UserInputEventKeyDown) {
        return STATUS_SUCCESS;
    }

    //
    // Handle key down events.
    //

    UpdateLedState = FALSE;
    IoBuffer = NULL;
    switch (Event->U.Key) {
    case KeyboardKeyLeftControl:
    case KeyboardKeyRightControl:
        ControlMask = TERMINAL_KEYBOARD_CONTROL;
        break;

    case KeyboardKeyLeftShift:
    case KeyboardKeyRightShift:
        ControlMask = TERMINAL_KEYBOARD_SHIFT;
        break;

    case KeyboardKeyLeftAlt:
    case KeyboardKeyRightAlt:
        ControlMask = TERMINAL_KEYBOARD_ALT;
        break;

    case KeyboardKeyNumLock:
        RtlAtomicXor32(&InTerminalKeyboardMask, TERMINAL_KEYBOARD_NUM_LOCK);
        UpdateLedState = TRUE;
        break;

    case KeyboardKeyScrollLock:
        RtlAtomicXor32(&InTerminalKeyboardMask, TERMINAL_KEYBOARD_SCROLL_LOCK);
        UpdateLedState = TRUE;
        break;

    case KeyboardKeyCapsLock:
        RtlAtomicXor32(&InTerminalKeyboardMask, TERMINAL_KEYBOARD_CAPS_LOCK);
        UpdateLedState = TRUE;
        break;

    case KeyboardKeyUp:
        TerminalKey.Key = TerminalKeyUp;
        break;

    case KeyboardKeyDown:
        TerminalKey.Key = TerminalKeyDown;
        break;

    case KeyboardKeyLeft:
        TerminalKey.Key = TerminalKeyLeft;
        break;

    case KeyboardKeyRight:
        TerminalKey.Key = TerminalKeyRight;
        break;

    case KeyboardKeyPageUp:
        TerminalKey.Key = TerminalKeyPageUp;
        break;

    case KeyboardKeyPageDown:
        TerminalKey.Key = TerminalKeyPageDown;
        break;

    case KeyboardKeyHome:
        TerminalKey.Key = TerminalKeyHome;
        break;

    case KeyboardKeyEnd:
        TerminalKey.Key = TerminalKeyEnd;
        break;

    case KeyboardKeyInsert:
        TerminalKey.Key = TerminalKeyInsert;
        break;

    case KeyboardKeyDelete:
        TerminalKey.Key = TerminalKeyDelete;
        break;

    case KeyboardKeyF1:
    case KeyboardKeyF2:
    case KeyboardKeyF3:
    case KeyboardKeyF4:
    case KeyboardKeyF5:
    case KeyboardKeyF6:
    case KeyboardKeyF7:
    case KeyboardKeyF8:
    case KeyboardKeyF9:
    case KeyboardKeyF10:
    case KeyboardKeyF11:
    case KeyboardKeyF12:
        TerminalKey.Key = TerminalKeyF1 + (Event->U.Key - KeyboardKeyF1);
        break;

    case KeyboardKeyKeypad0:
    case KeyboardKeyKeypad1:
    case KeyboardKeyKeypad2:
    case KeyboardKeyKeypad3:
    case KeyboardKeyKeypad4:
    case KeyboardKeyKeypad5:
    case KeyboardKeyKeypad6:
    case KeyboardKeyKeypad7:
    case KeyboardKeyKeypad8:
    case KeyboardKeyKeypad9:
    case KeyboardKeyKeypadPeriod:

        //
        // If the number lock is off or the shift key is pressed, then the
        // above keypad values turn into special or cursor codes.
        //

        if (((InTerminalKeyboardMask & TERMINAL_KEYBOARD_NUM_LOCK) == 0) ||
            ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_SHIFT) != 0)) {

            switch (Event->U.Key) {
                case KeyboardKeyKeypad0:
                    TerminalKey.Key = TerminalKeyInsert;
                    break;

                case KeyboardKeyKeypad1:
                    TerminalKey.Key = TerminalKeyEnd;
                    break;

                case KeyboardKeyKeypad2:
                    TerminalKey.Key = TerminalKeyDown;
                    break;

                case KeyboardKeyKeypad3:
                    TerminalKey.Key = TerminalKeyPageDown;
                    break;

                case KeyboardKeyKeypad4:
                    TerminalKey.Key = TerminalKeyLeft;
                    break;

                case KeyboardKeyKeypad5:
                    break;

                case KeyboardKeyKeypad6:
                    TerminalKey.Key = TerminalKeyRight;
                    break;

                case KeyboardKeyKeypad7:
                    TerminalKey.Key = TerminalKeyHome;
                    break;

                case KeyboardKeyKeypad8:
                    TerminalKey.Key = TerminalKeyUp;
                    break;

                case KeyboardKeyKeypad9:
                    TerminalKey.Key = TerminalKeyPageUp;
                    break;

                case KeyboardKeyKeypadPeriod:
                    TerminalKey.Key = TerminalKeyDelete;
                    break;

                default:

                    ASSERT(FALSE);

                    break;
            }

        //
        // Otherwise get the regular character. Caps Lock has no effect on the
        // keypad, so this cannot drop down into the regular case.
        //

        } else {
            RegularCharacter = InKeyboardCharacters[Event->U.Key];

            ASSERT(RegularCharacter != '\0');
        }

        break;

    //
    // Process a normal character.
    //

    default:
        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_SHIFT) != 0) {
            RegularCharacter = InShiftedKeyboardCharacters[Event->U.Key];

        } else {
            RegularCharacter = InKeyboardCharacters[Event->U.Key];
            if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_CAPS_LOCK) != 0) {
                RegularCharacter = RtlConvertCharacterToUpperCase(
                                                             RegularCharacter);
            }
        }

        if (RegularCharacter == '\0') {
            RegularCharacter = -1;

        //
        // Do it differently if a control key is down.
        //

        } else if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_CONTROL) != 0) {
            RegularCharacter =
                          RtlConvertCharacterToUpperCase(RegularCharacter);

            //
            // A couple of character alias when control is down.
            //

            if (RegularCharacter == '-') {
                RegularCharacter = '_';

            } else if (RegularCharacter == ' ') {
                RegularCharacter = '@';
            }

            if ((RegularCharacter >= '@') && (RegularCharacter <= '_')) {
                RegularCharacter -= '@';

            //
            // A couple of keys come through even if control is held down.
            //

            } else if ((RegularCharacter != '\r') &&
                       (RegularCharacter != TERMINAL_RUBOUT)) {

                RegularCharacter = -1;
            }
        }

        break;
    }

    //
    // Update the keyboard mask if a control value changed.
    //

    if (ControlMask != 0) {
        RtlAtomicOr32(&InTerminalKeyboardMask, ControlMask);
    }

    //
    // Update the LED state if it changed.
    //

    if (UpdateLedState != FALSE) {
        InpUpdateLedStateForTerminal();
    }

    if (TerminalKey.Key != TerminalKeyInvalid) {
        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_ALT) != 0) {
            TerminalKey.Flags |= TERMINAL_KEY_FLAG_ALT;
        }

        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_SHIFT) != 0) {
            TerminalKey.Flags |= TERMINAL_KEY_FLAG_SHIFT;
        }

        Result = TermCreateInputSequence(&TerminalKey,
                                         Characters,
                                         sizeof(Characters));

        if (Result != FALSE) {
            Characters[sizeof(Characters) - 1] = '\0';
            CharacterCount = RtlStringLength(Characters);

        } else {
            CharacterCount = 0;
        }

    } else if (RegularCharacter != -1) {
        if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_ALT) != 0) {
            Characters[CharacterCount] = ANSI_ESCAPE_CODE;
            CharacterCount += 1;
        }

        Characters[CharacterCount] = RegularCharacter;
        CharacterCount += 1;
    }

    ASSERT(Event->EventType == UserInputEventKeyDown);

    if (CharacterCount != 0) {
        if (InLocalTerminal != NULL) {
            Status = MmCreateIoBuffer(Characters,
                                      CharacterCount,
                                      IO_BUFFER_FLAG_KERNEL_MODE_DATA,
                                      &IoBuffer);

            if (!KSUCCESS(Status)) {
                goto ProcessInputEventForTerminalEnd;
            }

            Status = IoWrite(InLocalTerminal,
                             IoBuffer,
                             CharacterCount,
                             0,
                             USER_INPUT_TERMINAL_WAIT_TIME,
                             &BytesWritten);

            if (Status == STATUS_TOO_LATE) {
                RtlDebugPrint("Shutting down user input on local terminal.\n");
                IoClose(InLocalTerminal);
                InLocalTerminal = NULL;
            }
        }
    }

    Status = STATUS_SUCCESS;

ProcessInputEventForTerminalEnd:
    if (IoBuffer != NULL) {
        MmFreeIoBuffer(IoBuffer);
    }

    return Status;
}

VOID
InpRepeatInputEventDpcRoutine (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine implements the DPC routine that fires when the user input
    repeat event timer expires. It queues the work item.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    PUSER_INPUT_DEVICE InputDevice;

    InputDevice = (PUSER_INPUT_DEVICE)Dpc->UserData;
    KeQueueWorkItem(InputDevice->RepeatWorkItem);
    return;
}

VOID
InpRepeatInputEventWorker (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine completes work for repeated user input events.

Arguments:

    Parameter - Supplies a pointer to the user input device who owns the repeat
        event.

Return Value:

    None.

--*/

{

    PUSER_INPUT_DEVICE InputDevice;
    PUSER_INPUT_EVENT RepeatEvent;
    KSTATUS Status;

    InputDevice = (PUSER_INPUT_DEVICE)Parameter;
    RepeatEvent = InputDevice->RepeatEvent;
    RepeatEvent->EventIdentifier = RtlAtomicAdd32(&InNextEventId, 1);
    Status = InpProcessInputEvent(RepeatEvent);

    //
    // Display optional debug information.
    //

    if ((InDebugFlags & USER_INPUT_DEBUG_REPEAT_EVENT) != 0) {
        RtlDebugPrint("USIN: REPEAT %s %s event processed with status %d: "
                      "event 0x%08x, device 0x%08x, ",
                      InDeviceTypeStrings[RepeatEvent->DeviceType],
                      InEventTypeStrings[RepeatEvent->EventType],
                      Status,
                      RepeatEvent->EventIdentifier,
                      RepeatEvent->DeviceIdentifier);

        switch (InputDevice->RepeatEvent->DeviceType) {
        case UserInputDeviceKeyboard:
            RtlDebugPrint("key %d.\n", RepeatEvent->U.Key);
            break;

        default:
            RtlDebugPrint("no data.\n");
            break;
        }
    }

    return;
}

VOID
InpUpdateLedStateForTerminal (
    VOID
    )

/*++

Routine Description:

    This routine updates the LED state for all terminal devices (i.e.
    keyboards).

Arguments:

    None.

Return Value:

    None.

--*/

{

    PLIST_ENTRY CurrentEntry;
    PUSER_INPUT_DEVICE Device;
    ULONG LedState;

    //
    // Acquire the device list lock and get the LED state by parsing the
    // terminal mask. It is OK if the terminal mask changes while reading, the
    // event that caused the change will have to wait on the device list lock
    // and will set the most up to date LED state.
    //

    LedState = 0;
    KeAcquireQueuedLock(InDeviceListLock);
    if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_NUM_LOCK) != 0) {
        LedState |= USER_INPUT_KEYBOARD_LED_NUM_LOCK;
    }

    if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_CAPS_LOCK) != 0) {
        LedState |= USER_INPUT_KEYBOARD_LED_CAPS_LOCK;
    }

    if ((InTerminalKeyboardMask & TERMINAL_KEYBOARD_SCROLL_LOCK) != 0) {
        LedState |= USER_INPUT_KEYBOARD_LED_SCROLL_LOCK;
    }

    //
    // Iterate over the list of user input devices and set the LED state.
    //

    CurrentEntry = InDeviceListHead.Next;
    while (CurrentEntry != &InDeviceListHead) {
        Device = LIST_VALUE(CurrentEntry, USER_INPUT_DEVICE, ListEntry);
        CurrentEntry = CurrentEntry->Next;
        if (Device->Type != UserInputDeviceKeyboard) {
            continue;
        }

        if (Device->U.KeyboardInterface.SetLedState != NULL) {
            Device->U.KeyboardInterface.SetLedState(Device->Device,
                                                    Device->DeviceContext,
                                                    LedState);
        }
    }

    KeReleaseQueuedLock(InDeviceListLock);
    return;
}

