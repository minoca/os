/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    test.h

Abstract:

    This header contains definitions for test devices.

Author:

    Chris Stevens 13-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define TEST_DEVICE_INFORMATION_UUID \
    {{0x88193972, 0x9b7c11e4, 0xa78e0401, 0x0fdd7401}}

#define TEST_DEVICE_INFORMATION_VERSION 0x00010000

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _TEST_DEVICE_TYPE {
    TestDeviceInvalid,
    TestDeviceKernel,
} TEST_DEVICE_TYPE, *PTEST_DEVICE_TYPE;

/*++

Structure Description:

    This structure stores the device information published by the device.

Members:

    Version - Stores the table version. Future revisions will be backwards
        compatible. Set to TEST_DEVICE_INFORMATION_VERSION.

    DeviceType - Stores the device type.

--*/

typedef struct _TEST_DEVICE_INFORMATION {
    ULONG Version;
    TEST_DEVICE_TYPE DeviceType;
} TEST_DEVICE_INFORMATION, *PTEST_DEVICE_INFORMATION;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
