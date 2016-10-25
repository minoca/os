/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    rommem.c

Abstract:

    This module implements support for the OMAP4 ROM memory interface, which
    can communicate with the SD card among other things.

Author:

    Evan Green 2-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include <dev/tirom.h>
#include "util.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

INTN
EfipTiMemOpen (
    UINT8 DeviceType,
    UINT32 ApiBase,
    VOID *DeviceData,
    PTI_ROM_MEM_HANDLE Handle
    )

/*++

Routine Description:

    This routine opens a connection to the ROM API for the memory device on
    OMAP4 and AM335x SoCs.

Arguments:

    DeviceType - Supplies the device type to open.

    ApiBase - Supplies the base address of the public API area.

    DeviceData - Supplies the device data buffer.

    Handle - Supplies a pointer where the connection state will be returned
        on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PTI_ROM_GET_MEM_DRIVER GetDriver;
    PTI_ROM_MMCSD_DEVICE_DATA MmcSdDeviceData;
    UINT16 Options;
    INTN Result;

    EfipInitZeroMemory(Handle, sizeof(TI_ROM_MEM_HANDLE));
    GetDriver = TI_ROM_API(ApiBase + PUBLIC_GET_DRIVER_MEM_OFFSET);
    Result = GetDriver(&(Handle->Driver), DeviceType);
    if (Result != 0) {
        return Result;
    }

    Options = 0;
    Handle->Device.Initialized = 0;
    Handle->Device.DeviceType = DeviceType;
    Handle->Device.BootOptions = &Options;
    Handle->Device.DeviceData = DeviceData;
    Result = Handle->Driver->Initialize(&(Handle->Device));
    if (Result != 0) {
        return Result;
    }

    MmcSdDeviceData = DeviceData;
    MmcSdDeviceData->Mode = TI_ROM_MMCSD_MODE_RAW;
    return 0;
}

INTN
EfipTiMemRead (
    PTI_ROM_MEM_HANDLE Handle,
    UINT32 Sector,
    UINTN SectorCount,
    VOID *Data
    )

/*++

Routine Description:

    This routine reads from the memory device.

Arguments:

    Handle - Supplies a pointer to the ROM handle.

    Sector - Supplies the sector to read from.

    SectorCount - Supplies the number of sectors to read.

    Data - Supplies a pointer where the data will be returned on success.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    TI_ROM_MEM_READ_DESCRIPTOR Descriptor;
    INTN Result;

    Descriptor.SectorStart = Sector;
    Descriptor.SectorCount = SectorCount;
    Descriptor.Destination = Data;
    Result = Handle->Driver->Read(&(Handle->Device), &Descriptor);
    return Result;
}

VOID
EfipInitZeroMemory (
    VOID *Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine zeroes memory.

Arguments:

    Buffer - Supplies a pointer to the buffer to zero.

    Size - Supplies the number of bytes to zero.

Return Value:

    None.

--*/

{

    UINT8 *Bytes;
    UINTN Index;

    Bytes = Buffer;
    for (Index = 0; Index < Size; Index += 1) {
        Bytes[Index] = 0;
    }

    return;
}

//
// --------------------------------------------------------- Internal Functions
//

