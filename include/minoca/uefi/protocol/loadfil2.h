/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loadfil2.h

Abstract:

    This header contains definitions for the UEFI Load File 2 Protocol.

Author:

    Evan Green 13-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_LOAD_FILE2_PROTOCOL_GUID                        \
    {                                                       \
        0x4006C0C1, 0xFCB3, 0x403E,                         \
        {0x99, 0x6D, 0x4A, 0x6C, 0x87, 0x24, 0xE0, 0x6D}    \
    }

//
// Protocol GUID definition used in earlier versions.
//

#define LOAD_FILE2_PROTOCOL EFI_LOAD_FILE2_PROTOCOL_GUID

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_LOAD_FILE2_PROTOCOL EFI_LOAD_FILE2_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_LOAD_FILE2)(
    EFI_LOAD_FILE2_PROTOCOL *This,
    EFI_DEVICE_PATH_PROTOCOL *FilePath,
    BOOLEAN BootPolicy,
    UINTN *BufferSize,
    VOID *Buffer
    );

/*++

Routine Description:

    This routine loads an EFI file into memory.

Arguments:

    This - Supplies the protocol instance.

    FilePath - Supplies a pointer to the device path of the file to load.

    BootPolity - Supplies a boolean that should always be FALSE.

    BufferSize - Supplies a pointer that on input contains the size of the
        supplied buffer. On output, returns the size of the file buffer.

    Buffer - Supplies the buffer to load the file contents into.

Return Value:

    EFI_SUCCESS if a file was loaded.

    EFI_UNSUPPORTED if the boot policy was TRUE.

    EFI_INVALID_PARAMETER if the file path is not a valid device path, or the
    buffer size was NULL.

    EFI_NO_MEDIA if no medium was present.

    EFI_DEVICE_ERROR if the file was not loaded due to a device error.

    EFI_NO_RESPONSE if the remote system did not respond.

    EFI_NOT_FOUND if the file was not found.

    EFI_ABORTED if the file load process was manually cancel.ed.

    EFI_BUFFER_TOO_SMALL if the buffer size was too small to read the current
    directory entry. The buffer size will have been updated with the needed
    size.

--*/

/*++

Structure Description:

    This structure defines the EFI load file 2 protocol. It is used to obtain
    files from arbitrary devices.

Members:

    LoadFile - Stores a pointer to a function used to load a file from the
        device.

--*/

struct _EFI_LOAD_FILE2_PROTOCOL {
    EFI_LOAD_FILE2 LoadFile;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
