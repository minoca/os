/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    usbrelay.c

Abstract:

    This module implements a simple app to control the USB relay device from
    One Ring Road.

Author:

    Evan Green 26-Jan-2015

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <minoca/lib/minocaos.h>
#include <minoca/devinfo/onering.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

//
// ---------------------------------------------------------------- Definitions
//

#define USBRELAY_USAGE                                                       \
    "usage: usbrelay <value>\n"                                              \
    "The usbrelay app controls a USB Relay controller from One Ring Road.\n" \
    "Each bit in the value specified corresponds to a relay position, on \n" \
    "or off. The value 0x1F turns all 5 relays on, and the value 0 turns \n" \
    "them all off.\n"

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

UUID UsbRelayDeviceInformationUuid = ONE_RING_USB_RELAY_DEVICE_INFORMATION_UUID;

//
// ------------------------------------------------------------------ Functions
//

INT
main (
    INT ArgumentCount,
    CHAR **Arguments
    )

/*++

Routine Description:

    This routine implements the usbrelay user mode program.

Arguments:

    ArgumentCount - Supplies the number of elements in the arguments array.

    Arguments - Supplies an array of strings. The array count is bounded by the
        previous parameter, and the strings are null-terminated.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSTR AfterScan;
    UINTN BytesCompleted;
    HANDLE Handle;
    PDEVICE_INFORMATION_RESULT InformationResults;
    ULONG ResultCount;
    KSTATUS Status;
    UINT Value;

    Handle = INVALID_HANDLE;
    InformationResults = NULL;
    if (ArgumentCount != 2) {
        printf(USBRELAY_USAGE);
        return 1;
    }

    if (strcmp(Arguments[1], "--help") == 0) {
        printf(USBRELAY_USAGE);
        return 1;
    }

    Value = strtoul(Arguments[1], &AfterScan, 0);
    if (AfterScan == Arguments[1]) {
        printf("usbrelay: Invalid numeric value: %s.\n", Arguments[1]);
        return 1;
    }

    //
    // Locate devices exposing the USB relay information UUID.
    //

    ResultCount = 0;
    Status = OsLocateDeviceInformation(&UsbRelayDeviceInformationUuid,
                                       NULL,
                                       NULL,
                                       &ResultCount);

    if ((!KSUCCESS(Status)) && (Status != STATUS_BUFFER_TOO_SMALL)) {
        fprintf(stderr,
                "usbrelay: Failed to get device information: %d.\n",
                Status);

        goto mainEnd;
    }

    if (ResultCount == 0) {
        fprintf(stderr, "usbrelay: No attached devices.\n");
        goto mainEnd;
    }

    //
    // Allocate space for the results, adding a little extra in case new
    // devices pop in.
    //

    ResultCount += 5;
    InformationResults =
                       malloc(sizeof(DEVICE_INFORMATION_RESULT) * ResultCount);

    if (InformationResults == NULL) {
        Status = STATUS_INSUFFICIENT_RESOURCES;
        goto mainEnd;
    }

    Status = OsLocateDeviceInformation(&UsbRelayDeviceInformationUuid,
                                       NULL,
                                       InformationResults,
                                       &ResultCount);

    if (!KSUCCESS(Status)) {
        fprintf(stderr,
                "usbrelay: Failed to get device information: %d.\n",
                Status);

        goto mainEnd;
    }

    if (ResultCount == 0) {
        fprintf(stderr, "usbrelay: No attached devices.\n");
        goto mainEnd;
    }

    //
    // Pick the first device and open a handle to it.
    //

    Status = OsOpenDevice(InformationResults[0].DeviceId,
                          SYS_OPEN_FLAG_WRITE,
                          &Handle);

    if (!KSUCCESS(Status)) {
        goto mainEnd;
    }

    //
    // Write the single byte value to the device.
    //

    Status = OsPerformIo(Handle,
                         0,
                         1,
                         SYS_IO_FLAG_WRITE,
                         SYS_WAIT_TIME_INDEFINITE,
                         &Value,
                         &BytesCompleted);

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "usbrelay: I/O error: %d\n", Status);
        goto mainEnd;
    }

    Status = STATUS_SUCCESS;

mainEnd:
    if (InformationResults != NULL) {
        free(InformationResults);
    }

    if (Handle != INVALID_HANDLE) {
        OsClose(Handle);
    }

    if (!KSUCCESS(Status)) {
        fprintf(stderr, "usbrelay: Failed: %d\n", Status);
        return 2;
    }

    return 0;
}

//
// --------------------------------------------------------- Internal Functions
//

