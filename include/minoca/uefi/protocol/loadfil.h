/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    loadfil.h

Abstract:

    This header contains the definition of the EFI Load File Protocol.

Author:

    Evan Green 13-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_LOAD_FILE_PROTOCOL_GUID                         \
    {                                                       \
        0x56EC3091, 0x954C, 0x11D2,                         \
        {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}    \
    }

//
// Protocol GUID as defined by EFI1.1
//

#define LOAD_FILE_PROTOCOL EFI_LOAD_FILE_PROTOCOL_GUID

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_LOAD_FILE_PROTOCOL EFI_LOAD_FILE_PROTOCOL;

//
// EFI1.1 type definition
//

typedef EFI_LOAD_FILE_PROTOCOL EFI_LOAD_FILE_INTERFACE;

//
// -------------------------------------------------------------------- Globals
//

typedef
EFI_STATUS
(EFIAPI *EFI_LOAD_FILE)(
    EFI_LOAD_FILE_PROTOCOL *This,
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

    BootPolity - Supplies a boolean indicating whether or not the request
        originates from the boot manager and is attempting to load a boot
        selection. If FALSE, then the file path must match as the exact file
        to be loaded.

    BufferSize - Supplies a pointer that on input contains the size of the
        supplied buffer. On output, returns the size of the file buffer.

    Buffer - Supplies the buffer to load the file contents into.

Return Value:

    EFI_SUCCESS if a file was loaded.

    EFI_UNSUPPORTED if the device does not support the provided boot policy.

    EFI_INVALID_PARAMETER if the file path is not a valid device path, or the
    buffer size was NULL.

    EFI_NO_MEDIA if no medium was present.

    EFI_NO_RESPONSE if the remote system did not respond.

    EFI_NOT_FOUND if the file was not found.

    EFI_ABORTED if the file load process was manually cancel.ed.

--*/

/*++

Structure Description:

    This structure defines the EFI load file protocol. It is used to obtain
    files from arbitrary devices.

Members:

    LoadFile - Stores a pointer to a function used to load a file from the
        device.

--*/

struct _EFI_LOAD_FILE_PROTOCOL {
    EFI_LOAD_FILE LoadFile;
};

//
// -------------------------------------------------------- Function Prototypes
//
