/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    random.h

Abstract:

    This header contains definitions for the random device interface.

Author:

    Evan Green 14-Jan-2015

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Interface UUID for Pseudo-Random Number Generators.
//

#define UUID_PSEUDO_RANDOM_SOURCE_INTERFACE \
    {{0x2AF9AAD3, 0x0EFC48BD, 0xBCE87270, 0xB6834C26}}

//
// ------------------------------------------------------ Data Type Definitions
//

typedef struct _INTERFACE_PSEUDO_RANDOM_SOURCE INTERFACE_PSEUDO_RANDOM_SOURCE,
    *PINTERFACE_PSEUDO_RANDOM_SOURCE;

//
// Pseudo Random Number Generator interface functions
//

typedef
VOID
(*PPSEUDO_RANDOM_ADD_ENTROPY) (
    PINTERFACE_PSEUDO_RANDOM_SOURCE Interface,
    PVOID Data,
    UINTN Length
    );

/*++

Routine Description:

    This routine adds entropy to a pseudo-random device. This function can be
    called at or below dispatch level.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    Data - Supplies a pointer to the entropy data to add. This data must be
        non-paged.

    Length - Supplies the number of bytes in the data.

Return Value:

    None.

--*/

typedef
VOID
(*PPSEUDO_RANDOM_ADD_TIME_POINT_ENTROPY) (
    PINTERFACE_PSEUDO_RANDOM_SOURCE Interface
    );

/*++

Routine Description:

    This routine adds entropy to a pseudo-random device based on the fact that
    this current moment in time is a random one. Said differently, it adds
    entropy based on the current timestamp, with the assumption that this
    function is called by a source that generates such events randomly. This
    function can be called at or below dispatch level.

Arguments:

    Interface - Supplies a pointer to the interface instance.

Return Value:

    None.

--*/

typedef
VOID
(*PPSEUDO_RANDOM_GET_BYTES) (
    PINTERFACE_PSEUDO_RANDOM_SOURCE Interface,
    PVOID Data,
    UINTN Length
    );

/*++

Routine Description:

    This routine gets random data from a pseudo-random number generator. This
    function can be called at or below dispatch level.

Arguments:

    Interface - Supplies a pointer to the interface instance.

    Data - Supplies a pointer where the random data will be returned. This
        buffer must be non-paged.

    Length - Supplies the number of bytes of random data to return.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure defines the interface for a pseudo-random number generator.

Members:

    DeviceToken - Stores an oqaque token used by the interface functions that
        identifies the device.

    AddEntropy - Stores a pointer to a function used to add entropy to the
        system.

    AddTimePointEntropy - Stores a pointer to a function used to add entropy
        based on the current time, with the assumption that these events
        occur at random intervals.

    GetBytes - Stores a pointer to a function used to read data from the
        pseudo-random number generator.

--*/

struct _INTERFACE_PSEUDO_RANDOM_SOURCE {
    PVOID DeviceToken;
    PPSEUDO_RANDOM_ADD_ENTROPY AddEntropy;
    PPSEUDO_RANDOM_ADD_TIME_POINT_ENTROPY AddTimePointEntropy;
    PPSEUDO_RANDOM_GET_BYTES GetBytes;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//
