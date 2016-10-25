/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    varback.h

Abstract:

    This header contains definitions for the UEFI variable backend protocol.

Author:

    Evan Green 27-Mar-2014

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define EFI_VARIABLE_BACKEND_PROTOCOL_GUID                      \
    {                                                           \
        0xAB5CCA39, 0xD7C8, 0x4437,                             \
        {0xB5, 0x29, 0x86, 0xC7, 0x58, 0x66, 0x2F, 0xAA}        \
    }

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _EFI_VARIABLE_BACKEND_PROTOCOL EFI_VARIABLE_BACKEND_PROTOCOL;

typedef
EFI_STATUS
(EFIAPI *EFI_VARIABLE_BACKEND_SET_DATA) (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID *Data,
    UINTN DataSize,
    BOOLEAN Replace
    );

/*++

Routine Description:

    This routine adds or replaces the current EFI variables with the given
    serialized variable buffer.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Data - Supplies a pointer to the serialized EFI variable data.

    DataSize - Supplies the size of the data buffer in bytes.

    Replace - Supplies a boolean indicating if the contents of this buffer
        should be merged with the current variables (FALSE) or if all current
        variables should be destroyed before adding these (TRUE).

Return Value:

    EFI status code.

--*/

typedef
EFI_STATUS
(EFIAPI *EFI_VARIABLE_BACKEND_GET_DATA) (
    EFI_VARIABLE_BACKEND_PROTOCOL *This,
    VOID **Data,
    UINTN *DataSize
    );

/*++

Routine Description:

    This routine returns a serialized form of the given variables. The caller
    must ensure no variable changes are made while using this buffer.

Arguments:

    This - Supplies a pointer to the protocol instance.

    Data - Supplies a pointer where a pointer will be returned to the
        serialized variable data. This data may be live, so the caller may not
        modify it.

    DataSize - Supplies a pointer where the size of the data will be returned
        on success.

Return Value:

    EFI status code.

--*/

/*++

Structure Description:

    This structure describes the variable backend protocol. This protocol
    allows the caller to get and set a serialized form of all the EFI variables.

Members:

    SetData - Stores a pointer to a function used to set the EFI variables
        from a serialized buffer.

    GetData - Stores a pointer to a functio used to get a serialized
        representation of the current EFI variables.

--*/

struct _EFI_VARIABLE_BACKEND_PROTOCOL {
    EFI_VARIABLE_BACKEND_SET_DATA SetData;
    EFI_VARIABLE_BACKEND_GET_DATA GetData;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
