/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    devrem.c

Abstract:

    This module implements a test driver that handles device removal.

Author:

    Chris Stevens 31-May-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Device removal pool tag.
//

#define DEVR_ALLOCATION_TAG 0x52766544 // 'RveD'

//
// Device removal level and children constants.
//

#define DEVICE_REMOVAL_LEVEL_MAX 4
#define DEVICE_REMOVAL_ROOT_LEVEL 0

//
// Device IDs and lengths.
//

#define DEVR_ROOT_ID "DEVREMROOT"
#define DEVR_CHILD_ID "DEVREMCHLD"
#define DEVR_DEVICE_ID_SIZE 11

//
// Class ID format and length.
//

#define DEVR_CLASS_ID_FORMAT "Level%04x"
#define DEVR_CLASS_ID_FORMAT_SIZE 10
#define DEVR_CLASS_ID_SIZE 10

//
// Removal test timer values.
//

#define DEVICE_REMOVAL_TEST_PERIOD (300 * MICROSECONDS_PER_MILLISECOND)
#define DEVICE_REMOVAL_TEST_DUE_TIME (15000 * MICROSECONDS_PER_MILLISECOND)

//
// Defines the rate at which removal IRPs fail when removal IRP failure is
// enabled.
//

#define REMOVAL_IRP_FAILURE_RATE 15

//
// Defines the rate at which the random test cleans up the test tracking tree.
//

#define RANDOM_TEST_CLEAN_TREE_RATE 5
#define RANDOM_REMOVE_START_LEVEL (DEVICE_REMOVAL_ROOT_LEVEL + 1)
#define RANDOM_REMOVE_END_LEVEL (DEVICE_REMOVAL_LEVEL_MAX - 1)
#define RANDOM_ADD_START_LEVEL DEVICE_REMOVAL_ROOT_LEVEL
#define RANDOM_ADD_END_LEVEL (DEVICE_REMOVAL_LEVEL_MAX - 2)
#define RANDOM_TEST_MAX_COUNT 100

//
// Define the rate at which device queue failures should occur.
//

#define DEVICE_QUEUE_FAILURE_RATE 10

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _REMOVAL_DEVICE REMOVAL_DEVICE, *PREMOVAL_DEVICE;

typedef enum _REMOVAL_DEVICE_TEST {
    RemovalDeviceTestAddChild,
    RemovalDeviceTestUnreportedChild,
    RemovalDeviceTestAddSibling,
    RemovalDeviceTestRemoveChild,
    RemovalDeviceTestRemoveSibling,
    RemovalDeviceTestAddRemoveChild,
    RemovalDeviceTestAddRemoveSibling,
    RemovalDeviceTestCascadeRemove,
    RemovalDeviceTestRandom,
    RemovalDeviceTestCleanup,
    RemovalDeviceTestMax
} REMOVAL_DEVICE_TEST, *PREMOVAL_DEVICE_TEST;

/*++

Structure Definition:

    This structure defines an entry in the removal device tree.

Members:

    DeviceToken - Stores an opaque token representing the device.

    BusContext - Stores the bus device's driver context for this device.

    FunctionContext - Stores the function device's driver context for this
        device.

    Attached - Stores a bool indicating whether or not the device is attached.

    RemovalIrp - Stores a bool indicating whether or not the device has
        received a removal IRP.

    SilbingEntry - Stores a list entry pointing to the entries sibling devices.

    ChildListHead - Stores a list entry pointing to the entries children.

--*/

typedef struct _REMOVAL_DEVICE_ENTRY {
    PVOID DeviceToken;
    PREMOVAL_DEVICE BusContext;
    PREMOVAL_DEVICE FunctionContext;
    BOOL Attached;
    BOOL RemovalIrp;
    LIST_ENTRY SiblingEntry;
    LIST_ENTRY ChildListHead;
} REMOVAL_DEVICE_ENTRY, *PREMOVAL_DEVICE_ENTRY;

/*++

Enum Definition:

    This enumerates device removal types.

Values:

    DeviceRemovalInvalid - Represents an invalid device.

    DeviceRemovalBus - Represents a bus device, implying that this driver will
        act as a functional driver for the bus.

    DeviceRemovalFunction - Represents a functional device, implying that this
        driver will act as a bus driver for the function.

--*/

typedef enum _REMOVAL_DEVICE_TYPE {
    DeviceRemovalInvalid,
    DeviceRemovalBus,
    DeviceRemovalFunction
} REMOVAL_DEVICE_TYPE, *PREMOVAL_DEVICE_TYPE;

/*++

Structure Definition:

    This structure defines a removal device.

Members:

    Type - Stores the type of removal device.

    Root - Stores whether or not the device is the root removal device.

    Level - Stores the heirarchy level of the device.

    Children - Stores an array of the device's children.

    ChildCount - Stores the number of children that belong to the device.

    TreeEntry - Stores the devices entry in the global tree.

--*/

struct _REMOVAL_DEVICE {
    REMOVAL_DEVICE_TYPE Type;
    BOOL Root;
    ULONG Level;
    PDEVICE *Children;
    ULONG ChildCount;
    PREMOVAL_DEVICE_ENTRY TreeEntry;
};

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
DeviceRemovalAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
DeviceRemovalDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DeviceRemovalDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
DeviceRemovalReportChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    );

VOID
DeviceRemovalEnumerateChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    );

VOID
DeviceRemovalValidateChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    );

KSTATUS
DeviceRemovalRemoveDevice (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    );

VOID
DeviceRemovalRunTests (
    VOID
    );

VOID
DeviceRemovalDestroyTests (
    PVOID Parameter
    );

VOID
DeviceRemovalServiceRemovalDpc (
    PDPC Dpc
    );

VOID
DeviceRemovalTests (
    PVOID Parameter
    );

VOID
DeviceRemovalTestAddChild (
    VOID
    );

VOID
DeviceRemovalTestUnreportedChild (
    VOID
    );

VOID
DeviceRemovalTestAddSibling (
    VOID
    );

VOID
DeviceRemovalTestRemoveChild (
    VOID
    );

VOID
DeviceRemovalTestRemoveSibling (
    VOID
    );

VOID
DeviceRemovalTestAddRemoveChild (
    VOID
    );

VOID
DeviceRemovalTestAddRemoveSibling (
    VOID
    );

VOID
DeviceRemovalTestCascadeRemove (
    VOID
    );

VOID
DeviceRemovalCascadeRemoveHelper (
    PREMOVAL_DEVICE_ENTRY RootEntry,
    PVOID ParentDeviceToken
    );

VOID
DeviceRemovalTestRandom (
    VOID
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindDeviceAndDetach (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY *ParentEntry,
    ULONG Level
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindDeviceAndAddChild (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY *ParentEntry,
    ULONG Level
    );

VOID
DeviceRemovalDestroyTree (
    PREMOVAL_DEVICE_ENTRY RootEntry
    );

VOID
DeviceRemovalCleanTree (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY ParentEntry
    );

VOID
DeviceRemovalNukeTree (
    VOID
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalInitializeTreeForTest (
    VOID
    );

VOID
DeviceRemovalWaitForTreeCreation (
    PREMOVAL_DEVICE_ENTRY RootEntry
    );

VOID
DeviceRemovalDetachDevice (
    PREMOVAL_DEVICE_ENTRY DeviceEntry
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalAttachChildDevice (
    PREMOVAL_DEVICE Device
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalAttachChildDeviceHelper (
    PREMOVAL_DEVICE Device
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalCreateTreeEntry (
    PREMOVAL_DEVICE DeviceContext,
    PREMOVAL_DEVICE_ENTRY ParentEntry
    );

VOID
DeviceRemovalDeleteTreeEntry (
    PREMOVAL_DEVICE_ENTRY Entry
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindChildByToken (
    PREMOVAL_DEVICE_ENTRY Root,
    PVOID DeviceToken
    );

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindEntryByToken (
    PVOID DeviceToken
    );

//
// -------------------------------------------------------------------- Globals
//

//
// Stores a boolean to toggle running the device removal tests during system
// startup.
//

BOOL DeviceRemovalTestsEnabled = FALSE;

//
// Array of how many children each level should automaticlly create.
//

ULONG LevelChildCount[DEVICE_REMOVAL_LEVEL_MAX] = {0, 2, 4, 0};

//
// Reference to this device driver.
//

PDRIVER DeviceRemovalDriver = NULL;

//
// Tree of device removal devices.
//

PREMOVAL_DEVICE_ENTRY RemovalDeviceTree = NULL;
KSPIN_LOCK DeviceTreeLock;
volatile ULONG DeviceEntryCount = 0;

//
// Device removal initialization, timer and work queue variables.
//

ULONG RemovalTestsInitialized = FALSE;
PKTIMER RemovalTestTimer = NULL;
PWORK_QUEUE RemovalTestWorkQueue = NULL;
PWORK_ITEM RemovalTestWorkItem = NULL;
PDPC RemovalTestDpc = NULL;
KSPIN_LOCK RemovalTestLock;
REMOVAL_DEVICE_TEST RemovalTest;

//
// Random device removal test variables.
//

ULONG RandomTestCount = 0;
BOOL RandomRemoveDevice = TRUE;
ULONG RandomRemoveLevel = RANDOM_REMOVE_START_LEVEL;
ULONG RandomAddLevel = RANDOM_ADD_START_LEVEL;

//
// IRP failure variables.
//

BOOL RemovalIrpFailEnabled = FALSE;
volatile ULONG RemovalIrpFailureCount = 0;

//
// Device queue failure variables.
//

BOOL DeviceQueueFailEnabled = FALSE;
volatile ULONG DeviceQueueFailureCount = 0;

//
// The root device context.
//

PREMOVAL_DEVICE RootDevice;
PDEVICE RootDeviceToken;

//
// Allocation counters.
//

volatile ULONG BusDeviceCount = 0;
volatile ULONG FunctionDeviceCount = 0;

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

    This routine is the entry point for the device removal driver. It registers
    its other dispatch functions, and performs driver-wide initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    DeviceRemovalDriver = Driver;
    KeInitializeSpinLock(&DeviceTreeLock);
    KeInitializeSpinLock(&RemovalTestLock);
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.AddDevice = DeviceRemovalAddDevice;
    FunctionTable.DispatchStateChange = DeviceRemovalDispatchStateChange;
    FunctionTable.DispatchSystemControl = DeviceRemovalDispatchSystemControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Create the root device.
    //

    if (DeviceRemovalTestsEnabled != FALSE) {
        Status = IoCreateDevice(NULL,
                                NULL,
                                NULL,
                                "DEVREMROOT",
                                NULL,
                                NULL,
                                &RootDeviceToken);
    }

DriverEntryEnd:
    return Status;
}

KSTATUS
DeviceRemovalAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the device
    removal driver acts as the function driver. The driver will attach itself
    to the stack.

Arguments:

    Driver - Supplies a pointer to the driver being called.

    DeviceId - Supplies a pointer to a string with the device ID.

    ClassId - Supplies a pointer to a string containing the device's class ID.

    CompatibleIds - Supplies a pointer to a string containing device IDs
        that would be compatible with this device.

    DeviceToken - Supplies an opaque token that the driver can use to identify
        the device in the system. This token should be used when attaching to
        the stack.

Return Value:

    STATUS_SUCCESS on success.

    Failure code if the driver was unsuccessful in attaching itself.

--*/

{

    PREMOVAL_DEVICE DeviceContext;
    ULONG ItemsScanned;
    ULONG Level;
    BOOL Root;
    KSTATUS Status;
    PREMOVAL_DEVICE_ENTRY TreeEntry;

    TreeEntry = NULL;
    DeviceContext = NULL;

    //
    // Determine if this is the device removal root or some child.
    //

    Root = IoAreDeviceIdsEqual(DeviceId, DEVR_ROOT_ID);
    if (Root != FALSE) {
        Level = DEVICE_REMOVAL_ROOT_LEVEL;

    } else {

        ASSERT(IoAreDeviceIdsEqual(DeviceId, DEVR_CHILD_ID) != FALSE);

        //
        // Look at the class ID to determine the level.
        //

        Status = RtlStringScan(ClassId,
                               DEVR_CLASS_ID_SIZE,
                               DEVR_CLASS_ID_FORMAT,
                               DEVR_CLASS_ID_FORMAT_SIZE,
                               CharacterEncodingDefault,
                               &ItemsScanned,
                               &Level);

        if (!KSUCCESS(Status)) {
            goto AddDeviceEnd;
        }

        if (ItemsScanned != 1) {
            Status = STATUS_UNSUCCESSFUL;
            goto AddDeviceEnd;
        }
    }

    //
    // Initialize the functional device context that treats this device as a
    // bus.
    //

    DeviceContext = MmAllocateNonPagedPool(sizeof(REMOVAL_DEVICE),
                                           DEVR_ALLOCATION_TAG);

    if (DeviceContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AddDeviceEnd;
    }

    RtlAtomicAdd32(&BusDeviceCount, 1);
    RtlZeroMemory(DeviceContext, sizeof(REMOVAL_DEVICE));
    DeviceContext->Type = DeviceRemovalBus;
    DeviceContext->Root = Root;
    DeviceContext->Level = Level;

    ASSERT(Level < DEVICE_REMOVAL_LEVEL_MAX);

    DeviceContext->ChildCount = LevelChildCount[Level];

    //
    // The root device needs to create a tree entry for itself.
    //

    if (Root != FALSE) {
        KeAcquireSpinLock(&DeviceTreeLock);
        TreeEntry = DeviceRemovalCreateTreeEntry(DeviceContext,
                                                 NULL);

        KeReleaseSpinLock(&DeviceTreeLock);
        if (TreeEntry == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto AddDeviceEnd;
        }

        TreeEntry->DeviceToken = DeviceToken;
        RemovalDeviceTree = TreeEntry;

    //
    // For other devices, the tree entry was created by the parent, find it and
    // attach it to this devie context.
    //

    } else {
        TreeEntry = DeviceRemovalFindEntryByToken(DeviceToken);

        ASSERT(TreeEntry != NULL);

        DeviceContext->TreeEntry = TreeEntry;
        TreeEntry->BusContext = DeviceContext;
    }

    //
    // Attach the bus driver context to the device.
    //

    Status = IoAttachDriverToDevice(Driver, DeviceToken, DeviceContext);
    if (!KSUCCESS(Status)) {
        goto AddDeviceEnd;
    }

    //
    // When adding the root device, begin the test sequence.
    //

    if (Root != FALSE) {
        RootDevice = DeviceContext;
        DeviceRemovalRunTests();
    }

AddDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (TreeEntry != NULL) {
            DeviceRemovalDeleteTreeEntry(TreeEntry);
        }

        if (DeviceContext != NULL) {
            MmFreeNonPagedPool(DeviceContext);
            RtlAtomicAdd32(&BusDeviceCount, (ULONG)-1);
        }
    }

    return Status;
}

VOID
DeviceRemovalDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles State Change IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE Device;
    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    Device = (PREMOVAL_DEVICE)DeviceContext;

    //
    // Process the IRP based on the minor code and direction.
    //

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {
        case IrpMinorQueryResources:

            //
            // Act on this IRP if the driver is acting as the bus driver or
            // if it is the only driver for the device, which is the case for
            // the root.
            //

            if ((Device->Type == DeviceRemovalFunction) ||
                (Device->Root != FALSE)) {

                IoCompleteIrp(DeviceRemovalDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorStartDevice:

            //
            // Act on this IRP fi the driver is acting as the bus driver or
            // if it is the only driver for the device, which is the case for
            // the root.
            //

            if ((Device->Type == DeviceRemovalFunction) ||
                (Device->Root != FALSE)) {

               IoCompleteIrp(DeviceRemovalDriver, Irp, STATUS_SUCCESS);
            }

            break;

        case IrpMinorQueryChildren:

            //
            // The device is a function and this is operating as a bus driver,
            // so just complete the IRP.
            //

            if (Device->Type == DeviceRemovalFunction) {
                IoCompleteIrp(DeviceRemovalDriver, Irp, STATUS_SUCCESS);

            //
            // The device is a bus and this driver is acting as the
            // functional driver, so report the children.
            //

            } else {

                ASSERT(Device->Type == DeviceRemovalBus);

                DeviceRemovalReportChildren(Irp, Device);

                //
                // The root device has no bus driver, so it has to complete
                // the IRP itself.
                //

                if (Device->Root != FALSE) {
                    IoCompleteIrp(DeviceRemovalDriver, Irp, STATUS_SUCCESS);
                }
            }

            break;

        case IrpMinorRemoveDevice:
            if ((Device->Type == DeviceRemovalFunction) ||
                (Device->Root != FALSE)) {

                Status = DeviceRemovalRemoveDevice(Irp, Device);
                IoCompleteIrp(DeviceRemovalDriver, Irp, Status);
            }

        default:
            break;
        }

    } else {

        ASSERT(Irp->Direction == IrpUp);

        switch (Irp->MinorCode) {
        case IrpMinorRemoveDevice:
            DeviceRemovalRemoveDevice(Irp, Device);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
DeviceRemovalDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles System Control IRPs.

Arguments:

    Irp - Supplies a pointer to the I/O request packet.

    DeviceContext - Supplies the context pointer supplied by the driver when it
        attached itself to the driver stack. Presumably this pointer contains
        driver-specific device context.

    IrpContext - Supplies the context pointer supplied by the driver when
        the IRP was created.

Return Value:

    None.

--*/

{

    ASSERT(Irp->MajorCode == IrpMajorSystemControl);

    //
    // Complete the IRP.
    //

    if (Irp->Direction == IrpDown) {
        IoCompleteIrp(DeviceRemovalDriver, Irp, STATUS_NOT_HANDLED);

    } else {

        ASSERT(Irp->Direction == IrpUp);

    }

    return;
}

VOID
DeviceRemovalReportChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine reports the number of children of the device. If the children
    have not yet been enumerated, it will enumerate them. If they have
    previously been enumerated, then it will validate them to make sure they
    all still exist.

Arguments:

    Irp - Supplies a pointer to the IRP requestion the children.

    Device - Supplies a pointer to the current device context.

Return Value:

    None.

--*/

{

    PDEVICE *Children;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Irp->U.QueryChildren.Children = NULL;
    Irp->U.QueryChildren.ChildCount = 0;

    //
    // If the device has never reported children, enumerate them. Otherwise
    // validate that they all still exist.
    //

    if (Device->Children == NULL) {
        DeviceRemovalEnumerateChildren(Irp, Device);

    } else {
        DeviceRemovalValidateChildren(Irp, Device);
    }

    //
    // If the bus driver has no children, exit immediately.
    //

    if (Device->ChildCount == 0) {
        return;
    }

    //
    // Report the current children in the IRP.
    //

    ASSERT(Device->ChildCount != 0);

    Children = MmAllocatePagedPool(sizeof(PDEVICE) * Device->ChildCount,
                                   DEVR_ALLOCATION_TAG);

    if (Children == NULL) {
        goto ReportChildrenEnd;
    }

    RtlCopyMemory(Children,
                  Device->Children,
                  Device->ChildCount * sizeof(PDEVICE));

    Irp->U.QueryChildren.Children = Children;
    Irp->U.QueryChildren.ChildCount = Device->ChildCount;

ReportChildrenEnd:
    return;
}

VOID
DeviceRemovalEnumerateChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine enumerates the children of the supplied device.

Arguments:

    Irp - Supplies a pointer to the IRP requesting the enumeration.

    Device - Supplies a pointer to the device removal context.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    ULONG ChildIndex;
    CHAR ClassId[DEVR_CLASS_ID_SIZE];
    PREMOVAL_DEVICE NewContext;
    KSTATUS Status;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    Status = STATUS_SUCCESS;

    //
    // Synchronize with device creation.
    //

    KeAcquireSpinLock(&DeviceTreeLock);

    ASSERT(Device->TreeEntry->RemovalIrp == FALSE);

    //
    // Only bus driver should report children.
    //

    ASSERT(Device->Type == DeviceRemovalBus);

    if (Device->ChildCount == 0) {
        goto EnumerateChildrenEnd;
    }

    //
    // Allocate an array for child device pointers.
    //

    Device->Children = MmAllocatePagedPool(sizeof(PDEVICE) * Device->ChildCount,
                                           DEVR_ALLOCATION_TAG);

    if (Device->Children == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto EnumerateChildrenEnd;
    }

    RtlZeroMemory(Device->Children, sizeof(PDEVICE) * Device->ChildCount);

    //
    // Create the class ID for the children.
    //

    RtlPrintToString(ClassId,
                     DEVR_CLASS_ID_SIZE,
                     CharacterEncodingDefault,
                     DEVR_CLASS_ID_FORMAT,
                     (Device->Level + 1));

    //
    // Create devices for the current device's children and track them in the
    // the global device removal tree.
    //

    for (ChildIndex = 0; ChildIndex < Device->ChildCount; ChildIndex += 1) {
        NewContext = MmAllocateNonPagedPool(sizeof(REMOVAL_DEVICE),
                                            DEVR_ALLOCATION_TAG);

        if (NewContext == NULL) {
            Status = STATUS_INSUFFICIENT_RESOURCES;
            goto EnumerateChildrenEnd;
        }

        RtlAtomicAdd32(&FunctionDeviceCount, 1);
        RtlZeroMemory(NewContext, sizeof(REMOVAL_DEVICE));
        NewContext->Type = DeviceRemovalFunction;
        NewContext->Root = FALSE;

        //
        // Create a tree entry for the child device.
        //

        ChildEntry = DeviceRemovalCreateTreeEntry(NewContext,
                                                  Device->TreeEntry);

        if (ChildEntry == NULL) {
            MmFreeNonPagedPool(NewContext);
            RtlAtomicAdd32(&FunctionDeviceCount, (ULONG)-1);
            break;
        }

        //
        // Create the child device and fill out the accounting structures.
        //

        Status = IoCreateDevice(DeviceRemovalDriver,
                                NewContext,
                                Irp->Device,
                                DEVR_CHILD_ID,
                                ClassId,
                                NULL,
                                &(Device->Children[ChildIndex]));

        if (!KSUCCESS(Status)) {
            RtlDebugPrint("IoCreateDevice failed with status %d.\n", Status);
            DeviceRemovalDeleteTreeEntry(ChildEntry);
            MmFreeNonPagedPool(NewContext);
            RtlAtomicAdd32(&FunctionDeviceCount, (ULONG)-1);
            break;
        }

        ChildEntry->DeviceToken = Device->Children[ChildIndex];
    }

    //
    // If child creation ever failed, the current index is the count of how
    // many children were successfully created.
    //

    Device->ChildCount = ChildIndex;

EnumerateChildrenEnd:
    KeReleaseSpinLock(&DeviceTreeLock);
    return;
}

VOID
DeviceRemovalValidateChildren (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine validates that all of the devices children still exist. Where
    a physical device would query hardware, this driver searches through the
    global tree for missing children. If any of the children have been removed,
    it updates the device's child list.

Arguments:

    Irp - Supplies a pointer to the IRP that requires child validation.

    Device - Supplies a pointer to the current device context.

Return Value:

    None.

--*/

{

    ULONG ChildCount;
    PREMOVAL_DEVICE_ENTRY ChildEntry;
    ULONG ChildIndex;
    PDEVICE *CurrentChildren;
    ULONG CurrentIndex;
    PDEVICE *OriginalChildren;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Search through the children to determine how many devices are still
    // attached.
    //

    KeAcquireSpinLock(&DeviceTreeLock);

    ASSERT(Device->TreeEntry->RemovalIrp == FALSE);

    ASSERT(((Device->ChildCount == 0) && (Device->Children == NULL)) ||
           ((Device->ChildCount != 0) && (Device->Children != NULL)));

    ChildCount = 0;
    OriginalChildren = Device->Children;
    for (ChildIndex = 0; ChildIndex < Device->ChildCount; ChildIndex += 1) {
        ChildEntry = DeviceRemovalFindChildByToken(
                         Device->TreeEntry,
                         OriginalChildren[ChildIndex]);

        //
        // If the child has an entry and is attached, count is as validated.
        // Otherwise, remove it from the original array.
        //

        if ((ChildEntry != NULL) && (ChildEntry->Attached != FALSE)) {
            ChildCount += 1;
        }
    }

    //
    // If the count did not change, exit.
    //

    if (Device->ChildCount == ChildCount) {
        goto ValidateChildrenEnd;
    }

    //
    // If there are no children anymore, free the old list and do not create a
    // new list.
    //

    if (ChildCount == 0) {
        MmFreePagedPool(Device->Children);
        Device->Children = NULL;
        Device->ChildCount = 0;
        goto ValidateChildrenEnd;
    }

    //
    // Allocate an array for the new children.
    //

    CurrentChildren = MmAllocatePagedPool(sizeof(PDEVICE) * ChildCount,
                                          DEVR_ALLOCATION_TAG);

    if (CurrentChildren == NULL) {
        goto ValidateChildrenEnd;
    }

    CurrentIndex = 0;
    for (ChildIndex = 0; ChildIndex < Device->ChildCount; ChildIndex += 1) {
        ChildEntry = DeviceRemovalFindChildByToken(
                                 Device->TreeEntry,
                                 OriginalChildren[ChildIndex]);

        if ((ChildEntry != NULL) && (ChildEntry->Attached != FALSE)) {
            CurrentChildren[CurrentIndex] = OriginalChildren[ChildIndex];
            CurrentIndex += 1;
        }
    }

    ASSERT(CurrentIndex == ChildCount);

    Device->Children = CurrentChildren;
    Device->ChildCount = ChildCount;
    MmFreePagedPool(OriginalChildren);

ValidateChildrenEnd:
    KeReleaseSpinLock(&DeviceTreeLock);
    return;
}

KSTATUS
DeviceRemovalRemoveDevice (
    PIRP Irp,
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine prepares the device for removal from the system.

Arguments:

    Irp - Supplies a pointer to the IRP that is requesting the device removal.

    Device - Supplies a pointer to the device context for this driver.

Return Value:

    Status code.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;
    ULONG OldFailureCount;
    PREMOVAL_DEVICE_ENTRY TreeEntry;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // Handle two cases where the driver is acting as the functional driver.
    //

    if (Device->Type == DeviceRemovalBus) {

        //
        // If the bus driver failed the IRP on the way down, exit immediately
        // on the way back up.
        //

        if ((RemovalIrpFailEnabled != FALSE) &&
            (Irp->Status != STATUS_NOT_HANDLED) &&
            (!KSUCCESS(Irp->Status))) {

            return Irp->Status;
        }

        //
        // Otherwise free the device context and return successfully.
        //

        if (Device->Children != NULL) {
            MmFreePagedPool(Device->Children);
        }

        MmFreeNonPagedPool(Device);
        RtlAtomicAdd32(&BusDeviceCount, (ULONG)-1);
        return STATUS_SUCCESS;
    }

    //
    // Fail some removal IRPs once IRP failure is enabled.
    //

    if (RemovalIrpFailEnabled != FALSE) {
        OldFailureCount = RtlAtomicAdd32(&RemovalIrpFailureCount, 1);
        if ((OldFailureCount % REMOVAL_IRP_FAILURE_RATE) == 0) {
            return STATUS_UNSUCCESSFUL;
        }
    }

    //
    // Fail the next device queue action if enabled.
    //

    if (DeviceQueueFailEnabled != FALSE) {
        OldFailureCount = RtlAtomicAdd32(&DeviceQueueFailureCount, 1);
        if ((OldFailureCount % DEVICE_QUEUE_FAILURE_RATE) == 0) {
            IoSetTestHook(IO_FAIL_QUEUE_DEVICE_WORK);
        }
    }

    //
    // Mark the device as detached.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    TreeEntry = Device->TreeEntry;

    ASSERT(TreeEntry != NULL);

    TreeEntry->Attached = FALSE;

    //
    // Assert that the device's children have already been marked as detached
    // and that they have seen a removal IRP.
    //

    CurrentEntry = TreeEntry->ChildListHead.Next;
    while (CurrentEntry != &(TreeEntry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        CurrentEntry = CurrentEntry->Next;

        ASSERT(ChildEntry->Attached == FALSE);
        ASSERT(ChildEntry->RemovalIrp != FALSE);
    }

    TreeEntry->RemovalIrp = TRUE;
    KeReleaseSpinLock(&DeviceTreeLock);
    if (Device->Children != NULL) {
        MmFreePagedPool(Device->Children);
    }

    MmFreeNonPagedPool(Device);
    RtlAtomicAdd32(&FunctionDeviceCount, (ULONG)-1);
    return STATUS_SUCCESS;
}

//
// -------------------------------------------------------- Test Infrastructure
//

VOID
DeviceRemovalRunTests (
    VOID
    )

/*++

Routine Description:

    This routine initializes the device removal test sequence.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONGLONG DueTime;
    BOOL Initialized;
    ULONGLONG Period;
    KSTATUS Status;

    //
    // Test and set the initialization boolean. If the test sequence has
    // already begun, exit. This could be done with the timer pointer, but
    // there is no compare-exchange pointer routine yet.
    //

    Initialized = RtlAtomicCompareExchange32(&RemovalTestsInitialized,
                                             TRUE,
                                             FALSE);

    if (Initialized != FALSE) {
        return;
    }

    ASSERT(RemovalTestTimer == NULL);
    ASSERT(RemovalTestWorkQueue == NULL);
    ASSERT(RemovalTestWorkItem == NULL);

    //
    // Create and queue a timer that will kick off the test sequence.
    //

    ASSERT(KeGetRunLevel() <= RunLevelDispatch);

    RemovalTestTimer = KeCreateTimer(DEVR_ALLOCATION_TAG);
    if (RemovalTestTimer == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RunTestsEnd;
    }

    //
    // Create a work queue that will be filled with a work item when the DPC
    // fires.
    //

    RemovalTestWorkQueue = KeCreateWorkQueue(
                                        WORK_QUEUE_FLAG_SUPPORT_DISPATCH_LEVEL,
                                        "DeviceRemovalTestQueue");

    if (RemovalTestWorkQueue == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RunTestsEnd;
    }

    //
    // Create a work item to be added to the work queue by the DPC.
    //

    RemovalTestWorkItem = KeCreateWorkItem(RemovalTestWorkQueue,
                                           WorkPriorityNormal,
                                           DeviceRemovalTests,
                                           NULL,
                                           DEVR_ALLOCATION_TAG);

    if (RemovalTestWorkItem == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RunTestsEnd;
    }

    //
    // Create a DPC to queue once the timer expires.
    //

    RemovalTestDpc = KeCreateDpc(DeviceRemovalServiceRemovalDpc, NULL);
    if (RemovalTestDpc == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto RunTestsEnd;
    }

    //
    // Now that the test sequence is almost good to go, set the first test.
    // This just needs to happen before the timer first expires.
    //

    RemovalTest = RemovalDeviceTestAddChild;

    //
    // Set the timer to go off at the test intervals.
    //

    DueTime = HlQueryTimeCounter();
    DueTime += KeConvertMicrosecondsToTimeTicks(DEVICE_REMOVAL_TEST_DUE_TIME);
    Period = KeConvertMicrosecondsToTimeTicks(DEVICE_REMOVAL_TEST_PERIOD);
    Status = KeQueueTimer(RemovalTestTimer,
                          TimerQueueSoftWake,
                          DueTime,
                          Period,
                          0,
                          RemovalTestDpc);

    if (!KSUCCESS(Status)) {
        goto RunTestsEnd;
    }

RunTestsEnd:
    if (!KSUCCESS(Status)) {
        if (RemovalTestDpc != NULL) {
            KeDestroyDpc(RemovalTestDpc);
        }

        if (RemovalTestTimer != NULL) {
            KeDestroyTimer(RemovalTestTimer);
        }

        if (RemovalTestWorkQueue != NULL) {
            KeDestroyWorkQueue(RemovalTestWorkQueue);
        }

        if (RemovalTestWorkItem != NULL) {
            KeDestroyWorkItem(RemovalTestWorkItem);
        }
    }

    return;
}

VOID
DeviceRemovalDestroyTests (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine destroys the resources allocated to run the device removal
    tests.

Arguments:

    Parameter - Supplies an optional parameter for the work item routine.

Return Value:

    None.

--*/

{

    KeDestroyTimer(RemovalTestTimer);
    KeDestroyDpc(RemovalTestDpc);
    KeDestroyWorkQueue(RemovalTestWorkQueue);
    KeDestroyWorkItem(RemovalTestWorkItem);
    return;
}

VOID
DeviceRemovalServiceRemovalDpc (
    PDPC Dpc
    )

/*++

Routine Description:

    This routine services the removal DPC that is queued by the test timer.

Arguments:

    Dpc - Supplies a pointer to the DPC that is running.

Return Value:

    None.

--*/

{

    //
    // Only queue more work if there isn't an entry already on the queue. This
    // needs a lock in case two DPCs are on top of each other and they both see
    // that the work item is not currently queued.
    //

    KeAcquireSpinLock(&RemovalTestLock);
    KeQueueWorkItem(RemovalTestWorkItem);
    KeReleaseSpinLock(&RemovalTestLock);
    return;
}

VOID
DeviceRemovalTests (
    PVOID Parameter
    )

/*++

Routine Description:

    This routine runs through a sequence of device tests.

Arguments:

    Parameter - Supplies an optional parameter for the work item routine.

Return Value:

    None.

--*/

{

    switch (RemovalTest) {

    //
    // The child addition test stresses adding a child device "concurrently"
    // with removing the parent device. This will test both notifying the
    // system of the child's addition followed by the parent's removal and vice
    // versa.
    //

    case RemovalDeviceTestAddChild:
        DeviceRemovalTestAddChild();
        break;

    //
    // The unreported child test forces a case that is not guaranteed to be
    // tested by the child add test due to timing. It tests the scenario where
    // a device needs to remove a child device that is yet to be reported.
    //

    case RemovalDeviceTestUnreportedChild:
        DeviceRemovalTestUnreportedChild();
        break;

    //
    // The sibling addition test will stress adding two devices to a bus at the
    // same time. It will test adding with the same notification and in
    // subsequent notifications.
    //

    case RemovalDeviceTestAddSibling:
        DeviceRemovalTestAddSibling();
        break;

    //
    // The child remove test will stress removing a child while removing the
    // parent. This will test both notifying the system of the child's removal
    // followed by the parent's removal and vice versa.
    //

    case RemovalDeviceTestRemoveChild:
        DeviceRemovalTestRemoveChild();
        break;

    //
    // The sibling removal test will stress removing two devices from a bus
    // at the same time. It will test removing the devices in the same
    // notification and in two subsequent notifications.
    //

    case RemovalDeviceTestRemoveSibling:
        DeviceRemovalTestRemoveSibling();
        break;

    //
    // The child add/remove test stresses the scenario where one child gets
    // added and another removed just before a parent device gets removed.
    //

    case RemovalDeviceTestAddRemoveChild:
        DeviceRemovalTestAddRemoveChild();
        break;

    //
    // The sibling add/remove test stresses the scenario where one device gets
    // added while another gets removed.
    //

    case RemovalDeviceTestAddRemoveSibling:
        DeviceRemovalTestAddRemoveSibling();
        break;

    //
    // This test covers the case where non-parent ancestory devices get removed
    // while a device is getting removed.
    //

    case RemovalDeviceTestCascadeRemove:
        DeviceRemovalTestCascadeRemove();
        break;

    //
    // The random test creates and removes devices at various levels of the
    // tree every time it is called. This test is used to flush out any timing
    // related issues that cannot be simulated directly.
    //

    case RemovalDeviceTestRandom:
        if (RandomTestCount == 0) {
            RemovalIrpFailEnabled = TRUE;
            DeviceQueueFailEnabled = TRUE;
        }

        DeviceRemovalTestRandom();
        break;

    case RemovalDeviceTestCleanup:
        DeviceRemovalNukeTree();
        RtlDebugPrint("Device Removal Tests Complete.\n");
        if ((DeviceEntryCount != 1) ||
            (BusDeviceCount != 1) ||
            (FunctionDeviceCount != 0)) {

            RtlDebugPrint("Device Removal Cleanup Failed:\n");
            if (DeviceEntryCount != 1) {
                RtlDebugPrint("\tDeviceEntryCount: %d, expected 1\n",
                              DeviceEntryCount);
            }

            if (BusDeviceCount != 1) {
                RtlDebugPrint("\tBusDeviceCount: %d, expected 1\n",
                              BusDeviceCount);
            }

            if (FunctionDeviceCount != 0) {
                RtlDebugPrint("\tFunctionDeviceCount: %d, expected 1\n",
                              FunctionDeviceCount);
            }

        } else {
            RtlDebugPrint("Device Removal Cleanup Succeeded.\n");
        }

        //
        // Fire off a work item to clean everything up.
        //

        KeCreateAndQueueWorkItem(NULL,
                                 WorkPriorityNormal,
                                 DeviceRemovalDestroyTests,
                                 NULL);

        break;

    default:
        break;
    }

    //
    // If the random test is currently not running or the random test has
    // completed its cycles, increment the test counter.
    //

    if ((RemovalTest != RemovalDeviceTestRandom) ||
        (RandomTestCount == RANDOM_TEST_MAX_COUNT)) {

        RemovalTest += 1;
    }

    return;
}

VOID
DeviceRemovalTestAddChild (
    VOID
    )

/*++

Routine Description:

    This routine performs the child addition test. This stresses adding a child
    device "concurrently" with removing the parent device. This will test both
    notifying the system of the child's addition followed by the parent's
    removal and vice versa.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    BOOL Result;

    RtlDebugPrint("ChildAdd: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("ChildAdd 0: Failed to attach parent device.\n");
        goto TestAddChild;
    }

    //
    // Now that the tree has been fully initialized, add a child device to the
    // parent, notify the system, and then immediately remove the parent,
    // notifying the system again. This will cause the parent to make the start
    // device call on the child before it gets the removal call. This should
    // trigger some bugs if children cannot be removed mid-initialization.
    //

    ChildEntry = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (ChildEntry == NULL) {
        RtlDebugPrint("ChildAdd 0: Failed to attach child device.\n");
    }

    //
    // If the child failed to be attached, there should be nothing to notify,
    // but do it anyway to stress the system.
    //

    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait until the parent receives its removal IRP.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // The child should have been removed even though it was in the middle of
    // being initialized. Validate this.
    //

    Result = TRUE;
    if ((ChildEntry != NULL) &&
        ((ChildEntry->Attached != FALSE) ||
         (ChildEntry->RemovalIrp == FALSE))) {

        RtlDebugPrint("ChildAdd 0: Failed to detach the child!\n");
        Result = FALSE;
    }

    if (ParentEntry->Attached != FALSE) {
        RtlDebugPrint("ChildAdd 0: Failed to detach the parent!\n");
        Result = FALSE;
    }

    //
    // The original device tree should have been destroyed, clean up the tree
    // tracking entries.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);
    if (Result != FALSE) {
        RtlDebugPrint("ChildAdd 0: Succeeded!\n");
    }

    //
    // Now perform the test again, but send the parent removal notification
    // first.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("ChildAdd 1: Failed to attach parent device.\n");
        goto TestAddChild;
    }

    //
    // Now that the tree has been fully initialized, remove the parent device,
    // add the child device, signal the system of the parent change and then
    // the child change. This should either test unreported device removal or
    // handling a query children work item between prepare remove and remove
    // work items.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    ChildEntry = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (ChildEntry == NULL) {
        RtlDebugPrint("ChildAdd 1: Failed to attach child device.\n");
    }

    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);
    KeAcquireSpinLock(&DeviceTreeLock);

    //
    // Only notify the system about the child's creation if the parent is yet
    // to receive a removal IRP and the child was actually created.
    //

    if ((ParentEntry->RemovalIrp == FALSE) && (ChildEntry != NULL))  {
        IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
    }

    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Wait until the parent receives its removal IRP.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // The child should have been removed even though it was in the middle of
    // being initialized. Validate this.
    //

    Result = TRUE;
    if ((ChildEntry != NULL) &&
        ((ChildEntry->Attached != FALSE) ||
         (ChildEntry->RemovalIrp == FALSE))) {

        RtlDebugPrint("ChildAdd 1: Failed to detach the child!\n");
        Result = FALSE;
    }

    if (ParentEntry->Attached != FALSE) {
        RtlDebugPrint("ChildAdd 1: Failed to detach the parent!\n");
        Result = FALSE;
    }

    //
    // The original device tree should have been destroyed, clean up the tree
    // tracking entries.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);
    if (Result != FALSE) {
        RtlDebugPrint("ChildAdd 1: Succeeded!\n");
    }

TestAddChild:
    return;
}

VOID
DeviceRemovalTestUnreportedChild (
    VOID
    )

/*++

Routine Description:

    This routine performs the unreported child test. It causes a device with a
    child in the unreported state to be removed. This triggers some failure
    handling behavior in the IO subsystem.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    BOOL Result;

    RtlDebugPrint("UnreportedChild: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("UnreportedChild: Failed to attach parent device.\n");
        goto TestUnreportedChildEnd;
    }

    //
    // Now that the tree has been fully initialized, remove the parent device,
    // add the child device, and signal the system of the parent change. Do not
    // notify the system of the child's presence. This should test unreported
    // device removal.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    ChildEntry = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (ChildEntry == NULL) {
        RtlDebugPrint("UnreportedChild: Failed to attach child device.\n");
    }

    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait until the parent receives its removal IRP.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // The child should have been removed even though it was in the middle of
    // being initialized. Validate this.
    //

    Result = TRUE;
    if ((ChildEntry != NULL) &&
        ((ChildEntry->Attached != FALSE) ||
         (ChildEntry->RemovalIrp == FALSE))) {

        RtlDebugPrint("UnreportedChild: Failed to detach the child!\n");
        Result = FALSE;
    }

    if (ParentEntry->Attached != FALSE) {
        RtlDebugPrint("UnreportedChild: Failed to detach the parent!\n");
        Result = FALSE;
    }

    //
    // The original device tree should have been destroyed, clean up the tree
    // tracking entries.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);
    if (Result != FALSE) {
        RtlDebugPrint("UnreportedChild: Succeeded!\n");
    }

TestUnreportedChildEnd:
    return;
}

VOID
DeviceRemovalTestAddSibling (
    VOID
    )

/*++

Routine Description:

    This routine tests adding two sibling devices. It first tests adding them
    within one system notification call and then from sequential calls.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY FirstSibling;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    BOOL Result;
    PREMOVAL_DEVICE_ENTRY SecondSibling;

    RtlDebugPrint("AddSibling: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("AddSibling Failed to attach parent device.\n");
        goto TestAddSiblingEnd;
    }

    //
    // Now attach two devices and notify the system.
    //

    FirstSibling = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (FirstSibling == NULL) {
        RtlDebugPrint("AddSibling 0: Failed to allocate first sibling.\n");
    }

    SecondSibling = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (SecondSibling == NULL) {
        RtlDebugPrint("AddSibling 0: Failed to allocate second sibling.\n");
    }

    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);

    //
    // Wait for the tree creation to complete and then check to make sure the
    // devices are fully attached.
    //

    Result = TRUE;
    DeviceRemovalWaitForTreeCreation(ParentEntry);
    if ((FirstSibling != NULL) && (FirstSibling->BusContext == NULL)) {
        RtlDebugPrint("AddSibling 0: First sibling failed to enumerate.\n");
        Result = FALSE;
    }

    if ((SecondSibling != NULL) && (SecondSibling->BusContext == NULL)) {
        RtlDebugPrint("AddSibling 0: Second sibling failed to enumerate.\n");
        Result = FALSE;
    }

    if (Result != FALSE) {
        RtlDebugPrint("AddSibling 0: Succeeded!\n");
    }

    //
    // Now add two additional siblings and notify the system after each
    // addition.
    //

    FirstSibling = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (FirstSibling == NULL) {
        RtlDebugPrint("AddSibling 1: Failed to allocate first sibling.\n");
    }

    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
    SecondSibling = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (SecondSibling == NULL) {
        RtlDebugPrint("AddSibling 1: Failed to allocate second sibling.\n");
    }

    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);

    //
    // Wait for the tree creation to complete and then check to make sure the
    // devices are fully attached.
    //

    Result = TRUE;
    DeviceRemovalWaitForTreeCreation(ParentEntry);
    if ((FirstSibling != NULL) && (FirstSibling->BusContext == NULL)) {
        RtlDebugPrint("AddSibling 1: First sibling failed to enumerate.\n");
        Result = FALSE;
    }

    if ((SecondSibling != NULL) && (SecondSibling->BusContext == NULL)) {
        RtlDebugPrint("AddSibling 1: Second sibling failed to enumerate.\n");
        Result = FALSE;
    }

    if (Result != FALSE) {
        RtlDebugPrint("AddSibling 1: Succeeded!\n");
    }

    //
    // Now detach the parent device and exit.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait until the parent receives its removal IRP.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // The original device tree should have been destroyed, clean up the tree
    // tracking entries.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

TestAddSiblingEnd:
    return;
}

VOID
DeviceRemovalTestRemoveChild (
    VOID
    )

/*++

Routine Description:

    This routine tests removing a child device while removing the devices
    parent. This test should stress scenarios where a child device has already
    entered the removal process by the time the parent tries to remove it and
    where the parent has already pushed the child into the removal process by
    the time the child tries to remove itself.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PREMOVAL_DEVICE_ENTRY ParentEntry;

    RtlDebugPrint("RemoveChild: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("RemoveChild: Failed to attach parent device.\n");
        goto TestRemoveChild;
    }

    //
    // Get one of the children and mark it for removal.
    //

    ASSERT(LIST_EMPTY(&(ParentEntry->ChildListHead)) == FALSE);

    ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                               ParentEntry->ChildListHead.Next,
                                               REMOVAL_DEVICE_ENTRY,
                                               SiblingEntry);

    DeviceRemovalDetachDevice(ChildEntry);
    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);

    //
    // Now remove the parent.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait for the removal process to complete and then validate the tree.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Make sure the child was detached.
    //

    if ((ChildEntry->RemovalIrp == FALSE) || (ChildEntry->Attached != FALSE)) {
        RtlDebugPrint("RemoveChild 0: Failed to properly remove child.\n");

    } else {
        RtlDebugPrint("RemoveChild 0: Succeeded!\n");
    }

    //
    // Clean up the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Now perform the test again but notify the system about the parent's
    // removal first.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("RemoveChild: Failed to attach parent device.\n");
        goto TestRemoveChild;
    }

    //
    // Get one of the children.
    //

    ASSERT(LIST_EMPTY(&(ParentEntry->ChildListHead)) == FALSE);

    ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                               ParentEntry->ChildListHead.Next,
                                               REMOVAL_DEVICE_ENTRY,
                                               SiblingEntry);

    //
    // Mark the parent for removal, notify the system, and then try to remove
    // the child.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);
    DeviceRemovalDetachDevice(ChildEntry);
    KeAcquireSpinLock(&DeviceTreeLock);
    if (ParentEntry->RemovalIrp == FALSE) {
        IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
    }

    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Wait for the removal process to complete and then validate the tree.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Make sure the child was detached.
    //

    if ((ChildEntry->RemovalIrp == FALSE) || (ChildEntry->Attached != FALSE)) {
        RtlDebugPrint("RemoveChild 1: Failed to properly remove child.\n");

    } else {
        RtlDebugPrint("RemoveChild 1: Succeeded!\n");
    }

    //
    // Clean up the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

TestRemoveChild:
    return;
}

VOID
DeviceRemovalTestRemoveSibling (
    VOID
    )

/*++

Routine Description:

    This routine implements the sibling removal test. This test stresses
    removing two devices from a bus at the same time. It tests removing the
    devices in the same system notification and in sequential notifications.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY FirstSibling;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    PREMOVAL_DEVICE_ENTRY SecondSibling;

    RtlDebugPrint("RemoveSibling: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("RemoveSibling: Failed to attach parent device.\n");
        goto TestRemoveSibling;
    }

    //
    // Make sure that the parent has at least two children.
    //

    ASSERT(ParentEntry->ChildListHead.Next !=
           ParentEntry->ChildListHead.Previous);

    FirstSibling = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                               ParentEntry->ChildListHead.Next,
                                               REMOVAL_DEVICE_ENTRY,
                                               SiblingEntry);

    SecondSibling = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                           ParentEntry->ChildListHead.Previous,
                                           REMOVAL_DEVICE_ENTRY,
                                           SiblingEntry);

    //
    // Detach the children and notify the system.
    //

    DeviceRemovalDetachDevice(FirstSibling);
    DeviceRemovalDetachDevice(SecondSibling);
    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);

    //
    // Wait for their removal IRPs.
    //

    while (FirstSibling->RemovalIrp == FALSE) {
        KeYield();
    }

    while (SecondSibling->RemovalIrp == FALSE) {
        KeYield();
    }

    RtlDebugPrint("RemoveSibling: Successful!\n");

    //
    // Make sure removing the children did not remove the parent.
    //

    ASSERT(ParentEntry->RemovalIrp == FALSE);
    ASSERT(ParentEntry->Attached != FALSE);

    //
    // Now destroy the parent and exit.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);
    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Clean up the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

TestRemoveSibling:
    return;
}

VOID
DeviceRemovalTestAddRemoveChild (
    VOID
    )

/*++

Routine Description:

    This routine implements the add/remove child test. This test adds a child
    and removes a different child while removing their parent device.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY AddChild;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    PREMOVAL_DEVICE_ENTRY RemoveChild;
    BOOL Result;

    RtlDebugPrint("AddRemoveChild: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("AddRemoveChild: Failed to attach parent device.\n");
        goto TestAddRemoveChildEnd;
    }

    //
    // Get a child entry to remove and mark it for removal.
    //

    ASSERT(LIST_EMPTY(&(ParentEntry->ChildListHead)) == FALSE);

    RemoveChild = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                               ParentEntry->ChildListHead.Next,
                                               REMOVAL_DEVICE_ENTRY,
                                               SiblingEntry);

    DeviceRemovalDetachDevice(RemoveChild);

    //
    // Attach a new child to the parent device and notify the system that the
    // parent device's topology changed.
    //

    AddChild = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (AddChild == NULL) {
        RtlDebugPrint("AddRemoveChild 0: Failed to allocate child device.\n");
    }

    IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);

    //
    // Now remove the parent device.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait for the removal.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Check the state of the tree.
    //

    Result = TRUE;
    if ((AddChild->RemovalIrp == FALSE) || (AddChild->Attached != FALSE)) {
        RtlDebugPrint("AddRemoveChild 0: Failed to remove added child.\n");
        Result = FALSE;
    }

    if ((RemoveChild->RemovalIrp == FALSE) ||
        (RemoveChild->Attached != FALSE)) {

        RtlDebugPrint("AddRemoveChild 0: Failed to remove child marked "
                      "removed.\n");

        Result = FALSE;
    }

    if (Result != FALSE) {
        RtlDebugPrint("AddRemoveChild 0: Successful!\n");
    }

    //
    // Destroy the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Now do it where the system gets notified of the parent's removal first.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry == NULL) {
        RtlDebugPrint("AddRemoveChild: Failed to attach parent device.\n");
        goto TestAddRemoveChildEnd;
    }

    //
    // Get a child entry to remove and mark it for removal.
    //

    ASSERT(LIST_EMPTY(&(ParentEntry->ChildListHead)) == FALSE);

    RemoveChild = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(
                                               ParentEntry->ChildListHead.Next,
                                               REMOVAL_DEVICE_ENTRY,
                                               SiblingEntry);

    DeviceRemovalDetachDevice(RemoveChild);

    //
    // Attach a new child to the parent device and notify the system that the
    // parent device's topology changed.
    //

    AddChild = DeviceRemovalAttachChildDevice(ParentEntry->BusContext);
    if (AddChild == NULL) {
        RtlDebugPrint("AddRemoveChild 1: Failed to allocate child device.\n");
    }

    //
    // Mark the parent for removal and notify the system.
    //

    DeviceRemovalDetachDevice(ParentEntry);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // If the children haven't received removal IRPs yet, signal the system.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    if ((AddChild->RemovalIrp == FALSE) || (RemoveChild->RemovalIrp == FALSE)) {
        IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
    }

    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Wait for the removal.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Check the state of the tree.
    //

    Result = TRUE;
    if ((AddChild->RemovalIrp == FALSE) || (AddChild->Attached != FALSE)) {
        RtlDebugPrint("AddRemoveChild 1: Failed to remove added child.\n");
        Result = FALSE;
    }

    if ((RemoveChild->RemovalIrp == FALSE) ||
        (RemoveChild->Attached != FALSE)) {

        RtlDebugPrint("AddRemoveChild 1: Failed to remove child marked "
                      "removed.\n");

        Result = FALSE;
    }

    if (Result != FALSE) {
        RtlDebugPrint("AddRemoveChild 1: Successful!\n");
    }

    //
    // Destroy the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

TestAddRemoveChildEnd:
    return;
}

VOID
DeviceRemovalTestAddRemoveSibling (
    VOID
    )

/*++

Routine Description:

    This routine implements the add/remove sibling test. This test adds a
    device tree while removing another.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY FirstSibling;
    PREMOVAL_DEVICE_ENTRY SecondSibling;

    RtlDebugPrint("AddRemoveSibling: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    FirstSibling = DeviceRemovalInitializeTreeForTest();
    if (FirstSibling == NULL) {
        RtlDebugPrint("AddRemoveSibling: Failed to attach first sibling.\n");
        goto TestAddRemoveSiblingEnd;
    }

    //
    // Now attach another device to the root, remove the one that was just
    // created and then notify the system.
    //

    SecondSibling = DeviceRemovalAttachChildDevice(RootDevice);
    if (SecondSibling == NULL) {
        RtlDebugPrint("AddRemoveSibling: Failed to attach second sibling.\n");
    }

    DeviceRemovalDetachDevice(FirstSibling);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);

    //
    // Wait for the first sibling to be removed.
    //

    while (FirstSibling->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Wait for the second sibling to be created.
    //

    DeviceRemovalWaitForTreeCreation(SecondSibling);

    //
    // If it made it this far, it succeeeded.
    //

    RtlDebugPrint("AddRemoveSibling: Successful!\n");

    //
    // Remove the second sibling's tree.
    //

    DeviceRemovalDetachDevice(SecondSibling);
    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);
    while (SecondSibling->RemovalIrp == FALSE) {
        KeYield();
    }

    //
    // Clean up the accounting structures.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(FirstSibling);
    DeviceRemovalDestroyTree(SecondSibling);
    KeReleaseSpinLock(&DeviceTreeLock);

TestAddRemoveSiblingEnd:
    return;
}

VOID
DeviceRemovalTestCascadeRemove (
    VOID
    )

/*++

Routine Description:

    This routine implements the cascade removal test. This test sends removal
    notifications about multiple devices that are in a device hierarchy.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ParentEntry;

    RtlDebugPrint("CascadeRemove: Started.\n");

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    ParentEntry = DeviceRemovalInitializeTreeForTest();
    if (ParentEntry== NULL) {
        RtlDebugPrint("CascadeRemove: Failed to attach root entry.\n");
        goto TestCascadeRemoveEnd;
    }

    //
    // Mark each element in the tree for removal.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalCascadeRemoveHelper(ParentEntry,
                                     RootDevice->TreeEntry->DeviceToken);

    KeReleaseSpinLock(&DeviceTreeLock);

    //
    // Wait for the parent device to be removed. Its removal process will
    // assert if any other removal failed.
    //

    while (ParentEntry->RemovalIrp == FALSE) {
        KeYield();
    }

    RtlDebugPrint("CascadeRemove: Successful!\n");

    //
    // Clean up the accounting structures.
    //

    KeAcquireSpinLock(&DeviceTreeLock);
    DeviceRemovalDestroyTree(ParentEntry);
    KeReleaseSpinLock(&DeviceTreeLock);

TestCascadeRemoveEnd:
    return;
}

VOID
DeviceRemovalCascadeRemoveHelper (
    PREMOVAL_DEVICE_ENTRY RootEntry,
    PVOID ParentDeviceToken
    )

/*++

Routine Description:

    This routine recursively marks every device in a tree for removal,
    notifying the system along the way. It does a post-order traversal.

Arguments:

    RootEntry - Supplies a pointer to the root of the device tree that needs
        to be removed.

    ParentDeviceToken - Supplies a pointer to the device token of the root's
        parent device.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = RootEntry->ChildListHead.Next;
    while (CurrentEntry != &(RootEntry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        CurrentEntry = CurrentEntry->Next;
        DeviceRemovalCascadeRemoveHelper(ChildEntry, RootEntry->DeviceToken);
    }

    DeviceRemovalDetachDevice(RootEntry);
    IoNotifyDeviceTopologyChange(ParentDeviceToken);
    return;
}

VOID
DeviceRemovalTestRandom (
    VOID
    )

/*++

Routine Description:

    This routine walks the device tree and marks some devices as detached.

Arguments:

    Parameter - Supplies an optional parameter for the work item routine.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY DetachEntry;
    PREMOVAL_DEVICE_ENTRY NewEntry;
    PREMOVAL_DEVICE_ENTRY ParentEntry;
    BOOL QueueFailure;
    BOOL TopologyChanged;

    ASSERT(KeGetRunLevel() == RunLevelLow);
    ASSERT(RemovalDeviceTree != NULL);

    TopologyChanged = FALSE;

    //
    // Acquire the tree lock before operating on the tree and the random test's
    // variables.
    //

    KeAcquireSpinLock(&DeviceTreeLock);

    //
    // Clean up the device tree's lingering test structures based on the clean
    // up rate.
    //

    RandomTestCount += 1;
    if ((RandomTestCount % RANDOM_TEST_CLEAN_TREE_RATE) == 0) {
        DeviceRemovalCleanTree(RemovalDeviceTree, NULL);
    }

    //
    // Record that a device queue failure should be added after the system
    // is notified of the change.
    //

    if ((RandomTestCount % DEVICE_QUEUE_FAILURE_RATE) == 0) {
        QueueFailure = TRUE;

    } else {
        QueueFailure = FALSE;
    }

    //
    // This test alternates between adding and removing a device from the tree.
    //

    ParentEntry = NULL;
    if (RandomRemoveDevice != FALSE) {
        RandomRemoveDevice = FALSE;

        ASSERT((RandomRemoveLevel <= RANDOM_REMOVE_END_LEVEL) &&
               (RandomRemoveLevel >= RANDOM_REMOVE_START_LEVEL));

        //
        // Pick a device at the current removal level and detach it.
        //

        DetachEntry = DeviceRemovalFindDeviceAndDetach(
                                                    RemovalDeviceTree,
                                                    &ParentEntry,
                                                    RANDOM_REMOVE_START_LEVEL);

        if (DetachEntry != NULL) {

            ASSERT(DetachEntry->Attached == FALSE);
            ASSERT(ParentEntry != NULL);

            TopologyChanged = TRUE;
        }

        if (RandomRemoveLevel == RANDOM_REMOVE_END_LEVEL) {
            RandomRemoveLevel = RANDOM_REMOVE_START_LEVEL;

        } else {
            RandomRemoveLevel += 1;
        }

    //
    // Add a new device tree at some layer within the existing tree.
    //

    } else {
        RandomRemoveDevice = TRUE;

        ASSERT(RandomAddLevel <= RANDOM_ADD_END_LEVEL);

        NewEntry = DeviceRemovalFindDeviceAndAddChild(RemovalDeviceTree,
                                                      &ParentEntry,
                                                      RANDOM_ADD_START_LEVEL);

        if (NewEntry != NULL) {

            ASSERT(ParentEntry != NULL);

            TopologyChanged = TRUE;
        }

        if (RandomAddLevel == RANDOM_ADD_END_LEVEL) {
            RandomAddLevel = RANDOM_ADD_START_LEVEL;

        } else {
            RandomAddLevel += 1;
        }
    }

    KeReleaseSpinLock(&DeviceTreeLock);
    if (TopologyChanged != FALSE) {

        ASSERT(ParentEntry != NULL);

        //
        // Inform the system that it might want to check the device tree again.
        // This is simulating the actions a bus driver might take when it
        // notices that a child has been detached.
        //

        IoNotifyDeviceTopologyChange(ParentEntry->DeviceToken);
        if (QueueFailure != FALSE) {
            IoSetTestHook(IO_FAIL_QUEUE_DEVICE_WORK);
        }
    }

    return;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindDeviceAndDetach (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY *ParentEntry,
    ULONG Level
    )

/*++

Routine Description:

    This routine recurses over the device tree looking for a device to detach
    at the current removal level. It detaches the device if it finds one.

Arguments:

    Entry - Supplies a pointer to the entry currently being evaluated for
        removal.

    ParentEntry - Supplies a pointer that receives the parent device of the
        device marked for removal.

    Level - Supplies the current tree level of the search.

Return Value:

    Returns a tree entry if the level matches the removal level and the device
    is attached or if a child of the tree entry matches the requirements.
    Returns NULL if the above requirements are not met by the device or any of
    the devices in its tree.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;
    PREMOVAL_DEVICE_ENTRY DetachEntry;

    //
    // Recurse over each child looking for a device to remove from the
    // appropriate level.
    //

    DetachEntry = NULL;
    CurrentEntry = Entry->ChildListHead.Next;
    while (CurrentEntry != &(Entry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        //
        // If the current device fits the criteria, return it.
        //

        if ((Level == RandomRemoveLevel) && (ChildEntry->Attached != FALSE)) {
            DeviceRemovalDetachDevice(ChildEntry);
            *ParentEntry = Entry;
            return ChildEntry;
        }

        //
        // Otherwise recurse on the device.
        //

        DetachEntry = DeviceRemovalFindDeviceAndDetach(ChildEntry,
                                                       ParentEntry,
                                                       Level + 1);

        if (DetachEntry != NULL) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return DetachEntry;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindDeviceAndAddChild (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY *ParentEntry,
    ULONG Level
    )

/*++

Routine Description:

    This routine recurses over the device tree looking for a device to which it
    will add a child. The requirement is that the device is fully initialized
    and remains attached.

Arguments:

    Entry - Supplies a pointer to the entry currently being evaluated for
        child addition.

    ParentEntry - Supplies a pointer that receives the parent entry of the new
        device tree entry.

    Level - Supplies the current tree level of the search.

Return Value:

    Returns a new device that was attached to the current entry. Or NULL if no
    device could be found that are prepared for new children.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;
    PREMOVAL_DEVICE_ENTRY NewEntry;

    //
    // If the current device fits the criteria, then add a child and return the
    // new child.
    //

    Device = (PDEVICE)Entry->DeviceToken;
    if ((Level == RandomAddLevel) &&
        (Entry->Attached != FALSE) &&
        (Entry->BusContext != NULL) &&
        (IoIsDeviceStarted(Device) != FALSE)) {

        ASSERT(Entry->RemovalIrp == FALSE);

        NewEntry = DeviceRemovalAttachChildDeviceHelper(Entry->BusContext);
        *ParentEntry = Entry;
        return NewEntry;
    }

    //
    // Recurse over each child looking for a device to which to add a child.
    //

    NewEntry = NULL;
    CurrentEntry = Entry->ChildListHead.Next;
    while (CurrentEntry != &(Entry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        NewEntry = DeviceRemovalFindDeviceAndAddChild(ChildEntry,
                                                      ParentEntry,
                                                      Level + 1);

        if (NewEntry != NULL) {
            break;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    return NewEntry;
}

VOID
DeviceRemovalDestroyTree (
    PREMOVAL_DEVICE_ENTRY RootEntry
    )

/*++

Routine Description:

    This routine destroys the device removal tree tracking structures.

Arguments:

    RootEntry - Supplies a pointer to the root of the tree that needs ot be
        destroyed.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = RootEntry->ChildListHead.Next;
    while (CurrentEntry != &(RootEntry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        CurrentEntry = CurrentEntry->Next;
        DeviceRemovalDestroyTree(ChildEntry);
    }

    ASSERT(RootEntry->Attached == FALSE);
    ASSERT(RootEntry->RemovalIrp != FALSE);

    DeviceRemovalDeleteTreeEntry(RootEntry);
    return;
}

VOID
DeviceRemovalCleanTree (
    PREMOVAL_DEVICE_ENTRY Entry,
    PREMOVAL_DEVICE_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine destroys the device removal tree tracking structures if they
    have received the removal IRP and their parent has received the removal
    IRP.

Arguments:

    Entry - Supplies a pointer to the local root of the tree that needs ot be
        destroyed.

    ParentEntry - Supplies a pointer to the parent of the local tree root that
        needs to be destroyed.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;

    CurrentEntry = Entry->ChildListHead.Next;
    while (CurrentEntry != &(Entry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        CurrentEntry = CurrentEntry->Next;
        DeviceRemovalCleanTree(ChildEntry, Entry);
    }

    //
    // If both the entry and the parent entry have received the removal IRP,
    // then this structure can be cleaned up. Or if the parent is the root
    // device.
    //

    if ((Entry->RemovalIrp != FALSE) &&
        (ParentEntry != NULL) &&
        ((ParentEntry->RemovalIrp != FALSE) ||
         (ParentEntry == RemovalDeviceTree))) {

        ASSERT(LIST_EMPTY(&(Entry->ChildListHead)) != FALSE);

        DeviceRemovalDeleteTreeEntry(Entry);
    }

    return;
}

VOID
DeviceRemovalNukeTree (
    VOID
    )

/*++

Routine Description:

    This routine removes all the subtrees from the root device. It current
    cannot remove the root because it is attached to the root device which
    doesn't accept query children commands.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;

    //
    // Acquire the tree lock before traversing the tree.
    //

    KeAcquireSpinLock(&DeviceTreeLock);

    //
    // Disable IRP and queue failures before nuking the tree.
    //

    RemovalIrpFailEnabled = FALSE;
    DeviceQueueFailEnabled = FALSE;
    IoClearTestHook(IO_FAIL_QUEUE_DEVICE_WORK);

    //
    // Mark each one of the root device's children as detached.
    //

    CurrentEntry = RemovalDeviceTree->ChildListHead.Next;
    while (CurrentEntry != &(RemovalDeviceTree->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        DeviceRemovalDetachDevice(ChildEntry);
        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Notify the root device that its device topology has changed.
    //

    IoNotifyDeviceTopologyChange((PDEVICE)RemovalDeviceTree->DeviceToken);

    //
    // Wait for the root's children to all receive removal IRPs. Destroy the
    // test tracking tree for each child.
    //

    CurrentEntry = RemovalDeviceTree->ChildListHead.Next;
    while (CurrentEntry != &(RemovalDeviceTree->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        while (ChildEntry->RemovalIrp == FALSE) {
            KeReleaseSpinLock(&DeviceTreeLock);
            KeYield();
            KeAcquireSpinLock(&DeviceTreeLock);
        }

        DeviceRemovalDestroyTree(ChildEntry);
        CurrentEntry = CurrentEntry->Next;
    }

    KeReleaseSpinLock(&DeviceTreeLock);
    return;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalInitializeTreeForTest (
    VOID
    )

/*++

Routine Description:

    This routine initializes a tree for device removal testing. It attaches a
    device to the root node, notifies the system, and then waits for the
    children beneath the tree to be fully created.

Arguments:

    None.

Return Value:

    Returns the a pointer to the root device of the tree created, or NULL on
    failure.

--*/

{

    PREMOVAL_DEVICE_ENTRY TreeEntry;

    //
    // Add a tree of 1->2->4 beneath the root, notify the system, and wait
    // until it is enumerated.
    //

    TreeEntry = DeviceRemovalAttachChildDevice(RootDevice);
    if (TreeEntry == NULL) {
        return NULL;
    }

    IoNotifyDeviceTopologyChange(RootDevice->TreeEntry->DeviceToken);
    DeviceRemovalWaitForTreeCreation(TreeEntry);
    return TreeEntry;
}

VOID
DeviceRemovalWaitForTreeCreation (
    PREMOVAL_DEVICE_ENTRY RootEntry
    )

/*++

Routine Description:

    This routine waits for a device tree to be fully attached. It is used as a
    blocking mechanism to wait for device enumeration to finish before testing.
    It will yield the processor if the tree is not complete.

Arguments:

    RootEntry - Supplies a pointer to the root of the device tree that needs
        to be evaluated.

Return Value:

    None.

--*/

{

    ULONG ChildCount;
    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;
    PDEVICE Device;

    //
    // Wait for the device to hit the started state. If it does not, then exit.
    //

    Device = RootEntry->DeviceToken;
    ObWaitOnObject(Device, 0, WAIT_TIME_INDEFINITE);
    if (IoIsDeviceStarted(Device) == FALSE) {
        return;
    }

    //
    // The bus context should be filled by now.
    //

    ASSERT(RootEntry->BusContext != NULL);

    //
    // Now the child count should be filled in, exit if there are no children.
    //

    if (RootEntry->BusContext->ChildCount == 0) {
        return;
    }

    //
    // Wait for the children tree entries to appear.
    //

    do {
        ChildCount = 0;
        KeAcquireSpinLock(&DeviceTreeLock);
        CurrentEntry = RootEntry->ChildListHead.Next;
        while (CurrentEntry != &(RootEntry->ChildListHead)) {
            ChildCount += 1;
            CurrentEntry = CurrentEntry->Next;
        }

        KeReleaseSpinLock(&DeviceTreeLock);
        KeYield();

    } while (ChildCount != RootEntry->BusContext->ChildCount);

    //
    // Recurse on each child of this device.
    //

    CurrentEntry = RootEntry->ChildListHead.Next;
    while (CurrentEntry != &(RootEntry->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        DeviceRemovalWaitForTreeCreation(ChildEntry);
        CurrentEntry = CurrentEntry->Next;
    }

    return;
}

VOID
DeviceRemovalDetachDevice (
    PREMOVAL_DEVICE_ENTRY DeviceEntry
    )

/*++

Routine Description:

    This routine detachs a device from the system.

Arguments:

    DeviceEntry - Supplies a pointer to the device that will be detached.

Return Value:

    None.

--*/

{

    ASSERT(DeviceEntry != NULL);

    DeviceEntry->Attached = FALSE;
    return;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalAttachChildDevice (
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine attaches a child device to the given device.

Arguments:

    Device - Supplies a pointer to the device to whom a child device will be
        attached.

Return Value:

    Returns the newly created child device, or NULL on failure.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;

    KeAcquireSpinLock(&DeviceTreeLock);
    ChildEntry = DeviceRemovalAttachChildDeviceHelper(Device);
    KeReleaseSpinLock(&DeviceTreeLock);
    return ChildEntry;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalAttachChildDeviceHelper (
    PREMOVAL_DEVICE Device
    )

/*++

Routine Description:

    This routine attaches a child device to the given device.

Arguments:

    Device - Supplies a pointer to the device to whom a child device will be
        attached.

Return Value:

    Returns the newly created child device, or NULL on failure.

--*/

{

    ULONG ChildCount;
    PREMOVAL_DEVICE_ENTRY ChildEntry;
    ULONG ChildIndex;
    CHAR ClassId[DEVR_CLASS_ID_SIZE];
    PDEVICE *NewChildren;
    PREMOVAL_DEVICE NewContext;
    ULONG OldChildCount;
    PDEVICE *OldChildren;
    KSTATUS Status;

    NewChildren = NULL;
    NewContext = NULL;
    ChildEntry = NULL;

    //
    // Allocate an array for child device pointers.
    //

    ChildCount = Device->ChildCount + 1;
    NewChildren = MmAllocatePagedPool(sizeof(PDEVICE) * ChildCount,
                                      DEVR_ALLOCATION_TAG);

    if (NewChildren == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AttachChildDeviceEnd;
    }

    //
    // Copy the current childen to the new array.
    //

    ASSERT(((Device->ChildCount == 0) && (Device->Children == NULL)) ||
           ((Device->ChildCount != 0) && (Device->Children != NULL)));

    OldChildCount = Device->ChildCount;
    RtlZeroMemory(NewChildren, sizeof(PDEVICE) * ChildCount);
    for (ChildIndex = 0; ChildIndex < OldChildCount; ChildIndex += 1) {
        NewChildren[ChildIndex] = Device->Children[ChildIndex];
    }

    OldChildren = Device->Children;

    //
    // Create the class ID for the children.
    //

    RtlPrintToString(ClassId,
                     DEVR_CLASS_ID_SIZE,
                     CharacterEncodingDefault,
                     DEVR_CLASS_ID_FORMAT,
                     (Device->Level + 1));

    //
    // Create a new device and track it in the global device tree.
    //

    NewContext = MmAllocateNonPagedPool(sizeof(REMOVAL_DEVICE),
                                        DEVR_ALLOCATION_TAG);

    if (NewContext == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AttachChildDeviceEnd;
    }

    RtlAtomicAdd32(&FunctionDeviceCount, 1);
    RtlZeroMemory(NewContext, sizeof(REMOVAL_DEVICE));
    NewContext->Type = DeviceRemovalFunction;
    NewContext->Root = FALSE;

    //
    // Create a tree entry for the child device and initialize it.
    //

    ChildEntry = DeviceRemovalCreateTreeEntry(NewContext,
                                              Device->TreeEntry);

    if (ChildEntry == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto AttachChildDeviceEnd;
    }

    //
    // Create the child device and fill out the accounting structures.
    //

    Status = IoCreateDevice(DeviceRemovalDriver,
                            NewContext,
                            Device->TreeEntry->DeviceToken,
                            DEVR_CHILD_ID,
                            ClassId,
                            NULL,
                            &(NewChildren[ChildCount - 1]));

    if (!KSUCCESS(Status)) {
        RtlDebugPrint("IoCreateDevice failed with status %d.\n", Status);
        goto AttachChildDeviceEnd;
    }

    ChildEntry->DeviceToken = NewChildren[ChildCount - 1];

    //
    // Update the devices children now that the routine will be successful.
    //

    Device->Children = NewChildren;
    Device->ChildCount = ChildCount;
    if (OldChildren != NULL) {
        MmFreePagedPool(OldChildren);
    }

AttachChildDeviceEnd:
    if (!KSUCCESS(Status)) {
        if (NewChildren != NULL) {
            MmFreePagedPool(NewChildren);
        }

        if (NewContext != NULL) {
            MmFreeNonPagedPool(NewContext);
            RtlAtomicAdd32(&FunctionDeviceCount, (ULONG)-1);
        }

        if (ChildEntry != NULL) {
            DeviceRemovalDeleteTreeEntry(ChildEntry);
        }
    }

    return ChildEntry;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalCreateTreeEntry (
    PREMOVAL_DEVICE DeviceContext,
    PREMOVAL_DEVICE_ENTRY ParentEntry
    )

/*++

Routine Description:

    This routine allocates and initializes a removal device tree entry.

Arguments:

    DeviceContext - Supplies a pointer to one of two device contexts associated
        with the tree entry.

    ParentEntry - Supplies a pointer to the parent device tree entry.

Return Value:

    Returns a pointer to the tree entry on success, or NULL on failure.

--*/

{

    PREMOVAL_DEVICE_ENTRY DeviceEntry;

    ASSERT(DeviceContext != NULL);

    DeviceEntry = MmAllocatePagedPool(sizeof(REMOVAL_DEVICE_ENTRY),
                                      DEVR_ALLOCATION_TAG);

    if (DeviceEntry == NULL) {
        return NULL;
    }

    RtlZeroMemory(DeviceEntry, sizeof(REMOVAL_DEVICE_ENTRY));
    if (DeviceContext->Type == DeviceRemovalFunction) {
        DeviceEntry->FunctionContext = DeviceContext;

    } else {
        DeviceEntry->BusContext = DeviceContext;
    }

    INITIALIZE_LIST_HEAD(&(DeviceEntry->ChildListHead));
    if (ParentEntry == NULL) {
        INITIALIZE_LIST_HEAD(&(DeviceEntry->SiblingEntry));

    } else {
        INSERT_AFTER(&(DeviceEntry->SiblingEntry),
                     &(ParentEntry->ChildListHead));
    }

    DeviceContext->TreeEntry = DeviceEntry;
    DeviceEntry->Attached = TRUE;
    DeviceEntry->RemovalIrp = FALSE;
    RtlAtomicAdd32(&DeviceEntryCount, 1);
    return DeviceEntry;
}

VOID
DeviceRemovalDeleteTreeEntry (
    PREMOVAL_DEVICE_ENTRY Entry
    )

/*++

Routine Description:

    This routine removes and deletes a device tree entry.

Arguments:

    Entry - Supplies a pointer to the tree entry to be deleted.

Return Value:

    None.

--*/

{

    LIST_REMOVE(&(Entry->SiblingEntry));
    MmFreePagedPool(Entry);
    RtlAtomicAdd32(&DeviceEntryCount, (ULONG)-1);
    return;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindChildByToken (
    PREMOVAL_DEVICE_ENTRY Root,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine searches for a device underneath the supplied root.

Arguments:

    Root - Supplies a pointer to the search's root device entry.

    DeviceToken - Supplies an opaque token used to identify the device.

Return Value:

    Returns a device entry upon success, NULL on failure.

--*/

{

    PREMOVAL_DEVICE_ENTRY ChildEntry;
    PLIST_ENTRY CurrentEntry;
    PREMOVAL_DEVICE_ENTRY ResultEntry;

    ASSERT(KeGetRunLevel() == RunLevelLow);

    //
    // In order to optimize the case where an immediate child is being sought,
    // perform a breadth first search. Look at the root's children first.
    //

    CurrentEntry = Root->ChildListHead.Next;
    while (CurrentEntry != &(Root->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        //
        // The token has to match and it cannot yet be removed. Device tokens
        // get reused.
        //

        if ((ChildEntry->DeviceToken == DeviceToken) &&
            (ChildEntry->RemovalIrp == FALSE)) {

            return ChildEntry;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // Now recurse over each of the children.
    //

    CurrentEntry = Root->ChildListHead.Next;
    while (CurrentEntry != &(Root->ChildListHead)) {
        ChildEntry = (PREMOVAL_DEVICE_ENTRY)LIST_VALUE(CurrentEntry,
                                                       REMOVAL_DEVICE_ENTRY,
                                                       SiblingEntry);

        ResultEntry = DeviceRemovalFindChildByToken(ChildEntry, DeviceToken);
        if (ResultEntry != NULL) {
            return ResultEntry;
        }

        CurrentEntry = CurrentEntry->Next;
    }

    //
    // If nothing is found, then the device does not exist.
    //

    return NULL;
}

PREMOVAL_DEVICE_ENTRY
DeviceRemovalFindEntryByToken (
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine searches for a device underneath the tree root.

Arguments:

    Root - Supplies a pointer to the search's root device entry.

    DeviceToken - Supplies an opaque token used to identify the device.

Return Value:

    Returns a device entry upon success, NULL on failure.

--*/

{

    PREMOVAL_DEVICE_ENTRY TreeEntry;

    KeAcquireSpinLock(&DeviceTreeLock);
    TreeEntry = DeviceRemovalFindChildByToken(RemovalDeviceTree, DeviceToken);
    KeReleaseSpinLock(&DeviceTreeLock);
    return TreeEntry;
}

