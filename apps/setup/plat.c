/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    plat.c

Abstract:

    This module contains platform specific setup instructions.

Author:

    Evan Green 7-May-2014

Environment:

    User Mode

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "setup.h"

//
// ---------------------------------------------------------------- Definitions
//

#define SETUP_RECIPE_FLAG_HIDDEN 0x00000001

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores installation directions for a specific platform or
    configuration.

Members:

    Id - Stores the recipe identifier.

    ShortName - Stores the short name of the platform, usually used at the
        command line.

    Description - Stores the longer name of the platform, used when printing.

    SmbiosProductName - Stores the platform name in the SMBIOS product name
        field.

    Flags - Stores a bitfield of flags. See SETUP_RECIPE_FLAG_* definitions.

    Architecture - Stores a pointer to the architecture of the platform.

--*/

typedef struct _SETUP_RECIPE {
    SETUP_RECIPE_ID Id;
    PSTR ShortName;
    PSTR Description;
    PSTR SmbiosProductName;
    ULONG Flags;
    PSTR Architecture;
} SETUP_RECIPE, *PSETUP_RECIPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// Define the recipes used to install to specific platforms. These are kept
// in the same order as above.
//

SETUP_RECIPE SetupRecipes[] = {
    {
        SetupRecipeNone,
        "None",
        "Complete user customization",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "None"
    },

    {
        SetupRecipeBeagleBoneBlack,
        "beagleboneblack",
        "TI BeagleBone Black",
        "A335BNLT",
        0,
        "armv7"
    },

    {
        SetupRecipeGalileo,
        "galileo",
        "Intel Galileo",
        "QUARK",
        0,
        "x86"
    },

    {
        SetupRecipeInstallArmv6,
        "install-armv6",
        "ARMv6 Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "armv6"
    },

    {
        SetupRecipeInstallArmv7,
        "install-armv7",
        "ARMv7 Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "armv7"
    },

    {
        SetupRecipeInstallX86,
        "install-x86",
        "x86 Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "x86"
    },

    {
        SetupRecipeInstallX64,
        "install-x64",
        "x86-64 Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "x64"
    },

    {
        SetupRecipeIntegratorCpRamDisk,
        "integrd",
        "Integrator/CP RAM Disk Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "armv7"
    },

    {
        SetupRecipePandaBoard,
        "panda",
        "TI PandaBoard",
        "PandaBoard",
        0,
        "armv7"
    },

    {
        SetupRecipePandaBoard,
        "panda-es",
        "TI PandaBoard ES",
        "PandaBoard ES",
        SETUP_RECIPE_FLAG_HIDDEN,
        "armv7"
    },

    {
        SetupRecipePandaBoardUsb,
        "panda-usb",
        "TI PandaBoard USB Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "armv7"
    },

    {
        SetupRecipePc,
        "pc",
        "Standard x86 BIOS PC",
        NULL,
        0,
        "x86"
    },

    {
        SetupRecipePcEfi,
        "pcefi",
        "Standard x86 UEFI-based PC",
        NULL,
        0,
        "x86"
    },

    {
        SetupRecipePcTiny,
        "pc-tiny",
        "Minimal PC installation for Qemu",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "x86"
    },

    //
    // TODO: Remove this once x64 compiles enough to match the x86 builds.
    //

    {
        SetupRecipePcTiny,
        "pc64",
        "Temporary x86-64 PC target",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        "x64"
    },

    {
        SetupRecipeRaspberryPi2,
        "raspberrypi2",
        "Raspberry Pi 2",
        "Raspberry Pi 2",
        0,
        "armv7"
    },

    {
        SetupRecipeRaspberryPi,
        "raspberrypi",
        "Raspberry Pi",
        "Raspberry Pi",
        0,
        "armv6"
    },

    {
        SetupRecipeVeyron,
        "veyron",
        "ASUS C201",
        "C201",
        0,
        "armv7"
    },
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
SetupParsePlatformString (
    PSETUP_CONTEXT Context,
    PSTR PlatformString
    )

/*++

Routine Description:

    This routine converts a platform string into a platform identifier, and
    sets it in the setup context.

Arguments:

    Context - Supplies a pointer to the setup context.

    PlatformString - Supplies a pointer to the string to convert to a
        platform identifier.

Return Value:

    TRUE if the platform name was successfully converted.

    FALSE if the name was invalid.

--*/

{

    PSETUP_RECIPE Recipe;
    ULONG RecipeCount;
    ULONG RecipeIndex;

    RecipeCount = sizeof(SetupRecipes) / sizeof(SetupRecipes[0]);
    for (RecipeIndex = 0; RecipeIndex < RecipeCount; RecipeIndex += 1) {
        Recipe = &(SetupRecipes[RecipeIndex]);
        if (((Recipe->ShortName != NULL) &&
             (strcasecmp(PlatformString, Recipe->ShortName) == 0)) ||
            ((Recipe->Description != NULL) &&
             (strcasecmp(PlatformString, Recipe->Description) == 0))) {

            Context->PlatformName = Recipe->ShortName;
            Context->ArchName = Recipe->Architecture;
            return TRUE;
        }
    }

    return FALSE;
}

VOID
SetupPrintPlatformList (
    VOID
    )

/*++

Routine Description:

    This routine prints the supported platform list.

Arguments:

    None.

Return Value:

    None.

--*/

{

    PSETUP_RECIPE Recipe;
    ULONG RecipeCount;
    ULONG RecipeIndex;

    printf("Supported platforms:\n");
    RecipeCount = sizeof(SetupRecipes) / sizeof(SetupRecipes[0]);
    for (RecipeIndex = 0; RecipeIndex < RecipeCount; RecipeIndex += 1) {
        Recipe = &(SetupRecipes[RecipeIndex]);
        if ((Recipe->Flags & SETUP_RECIPE_FLAG_HIDDEN) != 0) {
            continue;
        }

        printf("    %s -- %s\n", Recipe->ShortName, Recipe->Description);
    }

    return;
}

INT
SetupDeterminePlatform (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine finalizes the setup platform recipe to use.

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    SETUP_RECIPE_ID Fallback;
    LONG FallbackRecipeIndex;
    PSTR PlatformName;
    PSETUP_RECIPE Recipe;
    ULONG RecipeCount;
    ULONG RecipeIndex;
    INT Result;
    INT SmbiosLength;
    PSTR SmbiosName;

    PlatformName = NULL;
    Fallback = SetupRecipeNone;

    //
    // If the user specified a platform, just use it.
    //

    if (Context->PlatformName != NULL) {
        return 0;
    }

    //
    // Ask the OS to detect the current platform.
    //

    Result = SetupOsGetPlatformName(&PlatformName, &Fallback);
    if (Result != 0) {
        if (Result != ENOSYS) {
            fprintf(stderr,
                    "Failed to detect platform: %s\n",
                    strerror(Result));
        }

        goto DeterminePlatformEnd;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        printf("SMBIOS Platform Name: %s\n", PlatformName);
    }

    FallbackRecipeIndex = -1;
    RecipeCount = sizeof(SetupRecipes) / sizeof(SetupRecipes[0]);
    for (RecipeIndex = 0; RecipeIndex < RecipeCount; RecipeIndex += 1) {
        Recipe = &(SetupRecipes[RecipeIndex]);
        if (Recipe->Id == Fallback) {
            FallbackRecipeIndex = RecipeIndex;
        }

        //
        // Compare the name, ignoring case and ignoring any extra stuff that
        // might be on the end like a version.
        //

        SmbiosName = Recipe->SmbiosProductName;
        if ((PlatformName != NULL) && (SmbiosName != NULL)) {
            SmbiosLength = strlen(SmbiosName);
            if (strncasecmp(SmbiosName, PlatformName, SmbiosLength) == 0) {
                Context->PlatformName = Recipe->ShortName;
                Context->ArchName = Recipe->Architecture;
                break;
            }
        }
    }

    if (Context->PlatformName == NULL) {
        if (FallbackRecipeIndex == -1) {
            fprintf(stderr,
                    "Failed to convert platform name %s to recipe.\n",
                    PlatformName);

            Result = EINVAL;
            goto DeterminePlatformEnd;
        }

        RecipeIndex = FallbackRecipeIndex;
        Context->PlatformName = SetupRecipes[RecipeIndex].ShortName;
        Context->ArchName = SetupRecipes[RecipeIndex].Architecture;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        Recipe = &(SetupRecipes[RecipeIndex]);
        printf("Platform: %s\n", Recipe->Description);
    }

    Result = 0;

DeterminePlatformEnd:
    if (PlatformName != NULL) {
        free(PlatformName);
    }

    return Result;
}

//
// --------------------------------------------------------- Internal Functions
//

