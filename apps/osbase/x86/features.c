/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    features.c

Abstract:

    This module implements support for return processor features.

Author:

    Evan Green 11-Nov-2015

Environment:

    User mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "../osbasep.h"

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

ULONG OsProcessorFeatureMasks[OsX86FeatureCount] = {
    0,
    X86_FEATURE_SYSENTER,
    X86_FEATURE_I686,
    X86_FEATURE_FXSAVE
};

//
// ------------------------------------------------------------------ Functions
//

OS_API
BOOL
OsTestProcessorFeature (
    ULONG Feature
    )

/*++

Routine Description:

    This routine determines if a given processor features is supported or not.

Arguments:

    Feature - Supplies the feature to test, which is an enum of type
        OS_<arch>_PROCESSOR_FEATURE.

Return Value:

    TRUE if the feature is set.

    FALSE if the feature is not set or not recognized.

--*/

{

    PUSER_SHARED_DATA Data;
    ULONG Mask;

    Data = OspGetUserSharedData();
    if (Feature >= OsX86FeatureCount) {
        return FALSE;
    }

    Mask = OsProcessorFeatureMasks[Feature];
    if ((Data->ProcessorFeatures & Mask) != 0) {
        return TRUE;
    }

    return FALSE;
}

//
// --------------------------------------------------------- Internal Functions
//

