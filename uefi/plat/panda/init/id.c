/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    id.c

Abstract:

    This module implements support for getting the OMAP4 chip revision.

Author:

    Evan Green 1-Apr-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <uefifw.h>
#include "init.h"

//
// ---------------------------------------------------------------- Definitions
//

#define OMAP4_CONTROL_ID_REGISTER 0x4A002204

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines the mapping between an OMAP4 revision code and its
    register value.

Members:

    Revision - Stores the revision number.

    Value - Stores the value found in the ID register for the revision number.

--*/

typedef struct _OMAP4_REVISION_VALUE {
    OMAP4_REVISION Revision;
    UINT32 Value;
} OMAP4_REVISION_VALUE, *POMAP4_REVISION_VALUE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

OMAP4_REVISION_VALUE EfiOmap4RevisionValues[] = {
    {Omap4430RevisionEs10, 0x0B85202F},
    {Omap4430RevisionEs20, 0x1B85202F},
    {Omap4430RevisionEs21, 0x3B95C02F},
    {Omap4430RevisionEs22, 0x4B95C02F},
    {Omap4430RevisionEs23, 0x6B95C02F},
    {Omap4460RevisionEs10, 0x0B94E02F},
    {Omap4460RevisionEs11, 0x2B94E02F}
};

//
// ------------------------------------------------------------------ Functions
//

OMAP4_REVISION
EfipOmap4GetRevision (
    VOID
    )

/*++

Routine Description:

    This routine returns the OMAP4 revision number.

Arguments:

    None.

Return Value:

    Returns the SoC revision value.

--*/

{

    UINT32 Code;
    UINTN CodeCount;
    UINTN Index;

    Code = OMAP4_READ32(OMAP4_CONTROL_ID_REGISTER);
    CodeCount = sizeof(EfiOmap4RevisionValues) /
                sizeof(EfiOmap4RevisionValues[0]);

    for (Index = 0; Index < CodeCount; Index += 1) {
        if (EfiOmap4RevisionValues[Index].Value == Code) {
            return EfiOmap4RevisionValues[Index].Revision;
        }
    }

    return Omap4RevisionInvalid;
}

//
// --------------------------------------------------------- Internal Functions
//

