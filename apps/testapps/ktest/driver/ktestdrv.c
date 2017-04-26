/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ktestdrv.c

Abstract:

    This module implements the kernel test driver.

Author:

    Evan Green 5-Nov-2013

Environment:

    Kernel

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/kernel/driver.h>
#include "../ktestdrv.h"
#include "testsup.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
KTestUnload (
    PVOID Driver
    );

KSTATUS
KTestAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    );

VOID
KTestDispatchStateChange (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestDispatchSystemControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    );

VOID
KTestpHandleDeviceInformationRequest (
    PIRP Irp,
    PVOID DeviceContext
    );

//
// -------------------------------------------------------------------- Globals
//

PDRIVER KTestDriver = NULL;
PDEVICE KTestDevice = NULL;
BOOL KTestDeviceUnloaded = FALSE;
UUID KTestTestDeviceInformationUuid = TEST_DEVICE_INFORMATION_UUID;

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

    This routine is the entry point for the kernel stress test driver. It
    registers its other dispatch functions, and performs driver-wide
    initialization.

Arguments:

    Driver - Supplies a pointer to the driver object.

Return Value:

    STATUS_SUCCESS on success.

    Failure code on error.

--*/

{

    DRIVER_FUNCTION_TABLE FunctionTable;
    KSTATUS Status;

    ASSERT((KTestDriver == NULL) && (KTestDevice == NULL));

    Status = KTestInitializeTestSupport();
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    KTestDriver = Driver;
    RtlZeroMemory(&FunctionTable, sizeof(DRIVER_FUNCTION_TABLE));
    FunctionTable.Version = DRIVER_FUNCTION_TABLE_VERSION;
    FunctionTable.Unload = KTestUnload;
    FunctionTable.AddDevice = KTestAddDevice;
    FunctionTable.DispatchStateChange = KTestDispatchStateChange;
    FunctionTable.DispatchOpen = KTestDispatchOpen;
    FunctionTable.DispatchClose = KTestDispatchClose;
    FunctionTable.DispatchIo = KTestDispatchIo;
    FunctionTable.DispatchSystemControl = KTestDispatchSystemControl;
    FunctionTable.DispatchUserControl = KTestDispatchUserControl;
    Status = IoRegisterDriverFunctions(Driver, &FunctionTable);
    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

    //
    // Increment the reference count on the driver so it doesn't disappear
    // in between the device being created and enumerated. This extra reference
    // is released in add device.
    //

    IoDriverAddReference(KTestDriver);
    Status = IoCreateDevice(NULL,
                            NULL,
                            NULL,
                            KTEST_DEVICE_NAME,
                            NULL,
                            NULL,
                            &KTestDevice);

    if (!KSUCCESS(Status)) {
        goto DriverEntryEnd;
    }

DriverEntryEnd:
    if (KSUCCESS(Status)) {
        RtlDebugPrint("KTest driver loaded.\n");
    }

    return Status;
}

VOID
KTestUnload (
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

    RtlDebugPrint("KTest driver unloaded.\n");
    return;
}

KSTATUS
KTestAddDevice (
    PVOID Driver,
    PCSTR DeviceId,
    PCSTR ClassId,
    PCSTR CompatibleIds,
    PVOID DeviceToken
    )

/*++

Routine Description:

    This routine is called when a device is detected for which the null device
    acts as the function driver. The driver will attach itself to the stack.

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

    KSTATUS Status;

    Status = IoAttachDriverToDevice(Driver, DeviceToken, NULL);
    if (KSUCCESS(Status)) {

        //
        // On success, release the reference that was taken up in driver entry.
        // The device itself has now taken a reference on the driver.
        //

        IoDriverReleaseReference(KTestDriver);
    }

    return Status;
}

VOID
KTestDispatchStateChange (
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

    KSTATUS Status;

    ASSERT(Irp->MajorCode == IrpMajorStateChange);

    if (Irp->Direction == IrpDown) {
        switch (Irp->MinorCode) {

        case IrpMinorStartDevice:

            //
            // Publish the test information device type.
            //

            Status = IoRegisterDeviceInformation(
                                           Irp->Device,
                                           &KTestTestDeviceInformationUuid,
                                           TRUE);

            IoCompleteIrp(KTestDriver, Irp, Status);
            break;

        case IrpMinorRemoveDevice:
            Status = IoRegisterDeviceInformation(
                                           Irp->Device,
                                           &KTestTestDeviceInformationUuid,
                                           FALSE);

            IoCompleteIrp(KTestDriver, Irp, Status);
            break;

        case IrpMinorQueryResources:
        case IrpMinorQueryChildren:
            IoCompleteIrp(KTestDriver, Irp, STATUS_SUCCESS);
            break;

        default:
            break;
        }
    }

    return;
}

VOID
KTestDispatchOpen (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Open IRPs.

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

    IoCompleteIrp(KTestDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
KTestDispatchClose (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles Close IRPs.

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

    IoCompleteIrp(KTestDriver, Irp, STATUS_SUCCESS);
    return;
}

VOID
KTestDispatchIo (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles I/O IRPs.

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

    RtlDebugPrint("KTestDispatchIo\n");
    return;
}

VOID
KTestDispatchSystemControl (
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

    PVOID Context;
    PSYSTEM_CONTROL_LOOKUP Lookup;
    PFILE_PROPERTIES Properties;
    KSTATUS Status;

    Context = Irp->U.SystemControl.SystemContext;
    switch (Irp->MinorCode) {
    case IrpMinorSystemControlLookup:
        Lookup = (PSYSTEM_CONTROL_LOOKUP)Context;
        Status = STATUS_PATH_NOT_FOUND;
        if (Lookup->Root != FALSE) {

            //
            // Enable opening of the root as a single file.
            //

            Properties = Lookup->Properties;
            Properties->FileId = 0;
            Properties->Type = IoObjectCharacterDevice;
            Properties->HardLinkCount = 1;
            Properties->BlockSize = 1;
            Properties->BlockCount = 1;
            Properties->Size = 0;
            Status = STATUS_SUCCESS;
        }

        IoCompleteIrp(KTestDriver, Irp, Status);
        break;

    case IrpMinorSystemControlWriteFileProperties:
        IoCompleteIrp(KTestDriver, Irp, STATUS_SUCCESS);
        break;

    case IrpMinorSystemControlTruncate:
        IoCompleteIrp(KTestDriver, Irp, STATUS_NOT_SUPPORTED);
        break;

    case IrpMinorSystemControlDeviceInformation:
        KTestpHandleDeviceInformationRequest(Irp, DeviceContext);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        break;
    }

    return;
}

VOID
KTestDispatchUserControl (
    PIRP Irp,
    PVOID DeviceContext,
    PVOID IrpContext
    )

/*++

Routine Description:

    This routine handles User Control IRPs.

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

    KSTATUS Status;
    PVOID UserBuffer;
    UINTN UserBufferSize;

    UserBuffer = Irp->U.UserControl.UserBuffer;
    UserBufferSize = Irp->U.UserControl.UserBufferSize;
    if (KTestDeviceUnloaded != FALSE) {

        ASSERT(FALSE);

        Status = STATUS_TOO_LATE;
        goto DispatchUserControlEnd;
    }

    switch ((KTEST_REQUEST)Irp->MinorCode) {
    case KTestRequestUnload:
        KTestDeviceUnloaded = TRUE;
        KTestFlushAllTests();
        Status = IoRemoveUnreportedDevice(Irp->Device);
        break;

    case KTestRequestStartTest:
        Status = KTestStartTest(UserBuffer, UserBufferSize);
        break;

    case KTestRequestCancelTest:
        Status = KTestRequestCancellation(UserBuffer, UserBufferSize);
        break;

    case KTestRequestPoll:
        Status = KTestPoll(UserBuffer, UserBufferSize);
        break;

    //
    // Ignore everything unrecognized.
    //

    default:

        ASSERT(FALSE);

        Status = STATUS_NOT_IMPLEMENTED;
        break;
    }

DispatchUserControlEnd:
    IoCompleteIrp(KTestDriver, Irp, Status);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
KTestpHandleDeviceInformationRequest (
    PIRP Irp,
    PVOID DeviceContext
    )

/*++

Routine Description:

    This routine handles requests to get and set device information for the
    kernel test device.

Arguments:

    Irp - Supplies a pointer to the IRP making the request.

    DeviceContext - Supplies a pointer to the device context create for this
        kernel test device.

Return Value:

    None. Any completion status is set in the IRP.

--*/

{

    PTEST_DEVICE_INFORMATION Information;
    BOOL Match;
    PSYSTEM_CONTROL_DEVICE_INFORMATION Request;
    KSTATUS Status;

    Request = Irp->U.SystemControl.SystemContext;

    //
    // If this is not a request for the partition device information, ignore it.
    //

    Match = RtlAreUuidsEqual(&(Request->Uuid), &KTestTestDeviceInformationUuid);
    if (Match == FALSE) {
        return;
    }

    //
    // Setting test device information is not supported.
    //

    if (Request->Set != FALSE) {
        Status = STATUS_ACCESS_DENIED;
        goto HandleDeviceInformationRequestEnd;
    }

    //
    // Make sure the size is large enough.
    //

    if (Request->DataSize < sizeof(TEST_DEVICE_INFORMATION)) {
        Request->DataSize = sizeof(TEST_DEVICE_INFORMATION);
        Status = STATUS_BUFFER_TOO_SMALL;
        goto HandleDeviceInformationRequestEnd;
    }

    Request->DataSize = sizeof(TEST_DEVICE_INFORMATION);
    Information = Request->Data;
    RtlZeroMemory(Information, sizeof(TEST_DEVICE_INFORMATION));
    Information->Version = TEST_DEVICE_INFORMATION_VERSION;
    Information->DeviceType = TestDeviceKernel;
    Status = STATUS_SUCCESS;

HandleDeviceInformationRequestEnd:
    IoCompleteIrp(KTestDriver, Irp, Status);
    return;
}

