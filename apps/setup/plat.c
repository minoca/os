/*++

Copyright (c) 2014 Minoca Corp. All Rights Reserved

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

    Script - Stores a pointer to the script corresponding to this recipe.

    ScriptEnd - Stores the pointer to the end of the script (the first invalid
        character). It's assumed there is no null terminator.

--*/

typedef struct _SETUP_RECIPE {
    SETUP_RECIPE_ID Id;
    PSTR ShortName;
    PSTR Description;
    PSTR SmbiosProductName;
    ULONG Flags;
    PVOID Script;
    PVOID ScriptEnd;
} SETUP_RECIPE, *PSETUP_RECIPE;

//
// ----------------------------------------------- Internal Function Prototypes
//

//
// -------------------------------------------------------------------- Globals
//

//
// The objcopy utility provides symbols for the start, size, and end of the
// text files converted to object files. These correspond directly to the
// file names in the config directory, and are alphabetized.
//

extern PVOID _binary_bbone_txt_start;
extern PVOID _binary_bbone_txt_end;
extern PVOID _binary_common_txt_start;
extern PVOID _binary_common_txt_end;
extern PVOID _binary_galileo_txt_start;
extern PVOID _binary_galileo_txt_end;
extern PVOID _binary_instarm_txt_start;
extern PVOID _binary_instarm_txt_end;
extern PVOID _binary_instx86_txt_start;
extern PVOID _binary_instx86_txt_end;
extern PVOID _binary_integrd_txt_start;
extern PVOID _binary_integrd_txt_end;
extern PVOID _binary_panda_txt_start;
extern PVOID _binary_panda_txt_end;
extern PVOID _binary_pandausb_txt_start;
extern PVOID _binary_pandausb_txt_end;
extern PVOID _binary_pc_txt_start;
extern PVOID _binary_pc_txt_end;
extern PVOID _binary_pcefi_txt_start;
extern PVOID _binary_pcefi_txt_end;
extern PVOID _binary_pctiny_txt_start;
extern PVOID _binary_pctiny_txt_end;
extern PVOID _binary_rpi_txt_start;
extern PVOID _binary_rpi_txt_end;
extern PVOID _binary_rpi2_txt_start;
extern PVOID _binary_rpi2_txt_end;
extern PVOID _binary_veyron_txt_start;
extern PVOID _binary_veyron_txt_end;

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
        NULL,
        NULL
    },

    {
        SetupRecipeBeagleBoneBlack,
        "beagleboneblack",
        "TI BeagleBone Black",
        "BBBK",
        0,
        &_binary_bbone_txt_start,
        &_binary_bbone_txt_end
    },

    {
        SetupRecipeCommon,
        "common",
        "Common initialization",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_common_txt_start,
        &_binary_common_txt_end
    },

    {
        SetupRecipeGalileo,
        "galileo",
        "Intel Galileo",
        "QUARK",
        0,
        &_binary_galileo_txt_start,
        &_binary_galileo_txt_end
    },

    {
        SetupRecipeInstallArm,
        "install-arm",
        "ARM Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_instarm_txt_start,
        &_binary_instarm_txt_end
    },

    {
        SetupRecipeInstallX86,
        "install-x86",
        "x86 Install Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_instx86_txt_start,
        &_binary_instx86_txt_end
    },

    {
        SetupRecipeIntegratorCpRamDisk,
        "integrd",
        "Integrator/CP RAM Disk Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_integrd_txt_start,
        &_binary_integrd_txt_end
    },

    {
        SetupRecipePandaBoard,
        "panda",
        "TI PandaBoard",
        "PandaBoard",
        0,
        &_binary_panda_txt_start,
        &_binary_panda_txt_end
    },

    {
        SetupRecipePandaBoard,
        "panda-es",
        "TI PandaBoard ES",
        "PandaBoard ES",
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_panda_txt_start,
        &_binary_panda_txt_end
    },

    {
        SetupRecipePandaBoardUsb,
        "panda-usb",
        "TI PandaBoard USB Image Recipe",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_pandausb_txt_start,
        &_binary_pandausb_txt_end
    },

    {
        SetupRecipePc,
        "pc",
        "Standard x86 BIOS PC",
        NULL,
        0,
        &_binary_pc_txt_start,
        &_binary_pc_txt_end
    },

    {
        SetupRecipePcEfi,
        "pcefi",
        "Standard x86 UEFI-based PC",
        NULL,
        0,
        &_binary_pcefi_txt_start,
        &_binary_pcefi_txt_end
    },

    {
        SetupRecipePcTiny,
        "pc-tiny",
        "Minimal PC installation for Qemu",
        NULL,
        SETUP_RECIPE_FLAG_HIDDEN,
        &_binary_pctiny_txt_start,
        &_binary_pctiny_txt_end
    },

    {
        SetupRecipeRaspberryPi,
        "rasbperrypi",
        "Rasbperry Pi",
        "Raspberry Pi",
        0,
        &_binary_rpi_txt_start,
        &_binary_rpi_txt_end
    },

    {
        SetupRecipeRaspberryPi2,
        "rasbperrypi2",
        "Rasbperry Pi 2",
        "Raspberry Pi 2",
        0,
        &_binary_rpi2_txt_start,
        &_binary_rpi2_txt_end
    },

    {
        SetupRecipeVeyron,
        "veyron",
        "ASUS C201",
        "C201",
        0,
        &_binary_veyron_txt_start,
        &_binary_veyron_txt_end
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

            Context->RecipeIndex = RecipeIndex;
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

    PlatformName = NULL;
    Fallback = SetupRecipeNone;

    //
    // If the user specified a platform, just use it.
    //

    if (Context->RecipeIndex != -1) {
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

        if ((PlatformName != NULL) &&
            (Recipe->SmbiosProductName != NULL) &&
            (strcasecmp(Recipe->SmbiosProductName, PlatformName) == 0)) {

            Context->RecipeIndex = RecipeIndex;
            break;
        }
    }

    if (Context->RecipeIndex == -1) {
        if (FallbackRecipeIndex == -1) {
            fprintf(stderr,
                    "Failed to convert platform name %s to recipe.\n",
                    PlatformName);

            Result = EINVAL;
            goto DeterminePlatformEnd;
        }

        Context->RecipeIndex = FallbackRecipeIndex;
    }

    if ((Context->Flags & SETUP_FLAG_VERBOSE) != 0) {
        Recipe = &(SetupRecipes[Context->RecipeIndex]);
        printf("Platform: %s\n", Recipe->Description);
    }

    Result = 0;

DeterminePlatformEnd:
    if (PlatformName != NULL) {
        free(PlatformName);
    }

    return Result;
}

INT
SetupAddRecipeScript (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine adds the platform recipe script.

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_RECIPE Recipe;
    UINTN ScriptSize;
    INT Status;

    if (Context->RecipeIndex == -1) {
        return 0;
    }

    Recipe = &(SetupRecipes[Context->RecipeIndex]);
    if (Recipe->Script == NULL) {
        return 0;
    }

    ScriptSize = (UINTN)(Recipe->ScriptEnd) - (UINTN)(Recipe->Script);
    Status = SetupLoadScriptBuffer(&(Context->Interpreter),
                                   Recipe->ShortName,
                                   Recipe->Script,
                                   ScriptSize,
                                   0);

    return Status;
}

INT
SetupAddCommonScripts (
    PSETUP_CONTEXT Context
    )

/*++

Routine Description:

    This routine adds the common scripts that are added no matter what.

Arguments:

    Context - Supplies a pointer to the setup context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    PSETUP_RECIPE Recipe;
    ULONG RecipeCount;
    ULONG RecipeIndex;
    UINTN ScriptSize;
    INT Status;

    Status = 0;
    RecipeCount = sizeof(SetupRecipes) / sizeof(SetupRecipes[0]);
    for (RecipeIndex = 0; RecipeIndex < RecipeCount; RecipeIndex += 1) {
        Recipe = &(SetupRecipes[RecipeIndex]);
        if (Recipe->Id != SetupRecipeCommon) {
            continue;
        }

        ScriptSize = (UINTN)(Recipe->ScriptEnd) - (UINTN)(Recipe->Script);
        Status = SetupLoadScriptBuffer(&(Context->Interpreter),
                                       Recipe->ShortName,
                                       Recipe->Script,
                                       ScriptSize,
                                       0);

        if (Status != 0) {
            fprintf(stderr, "Failed to add common scripts.\n");
            break;
        }
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

