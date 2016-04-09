/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    elfcomm.h

Abstract:

    This header contains definitions for ELF functions that are agnostic to
    address size (32 vs 64 bit), but would otherwise be private to the ELF
    library.

Author:

    Evan Green 8-Apr-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
ImpElfLoadImportWithPath (
    PLOADED_IMAGE Image,
    PLIST_ENTRY ListHead,
    PSTR LibraryName,
    PSTR Path,
    PLOADED_IMAGE *Import
    );

/*++

Routine Description:

    This routine attempts to load a needed library for an ELF image.

Arguments:

    Image - Supplies a pointer to the image that needs the library.

    ListHead - Supplies a pointer to the head of the list of loaded images.

    LibraryName - Supplies the name of the library to load.

    Path - Supplies a colon-separated list of paths to try.

    Import - Supplies a pointer where a pointer to the loaded image will be
        returned.

Return Value:

    Status code.

--*/

ULONG
ImpElfOriginalHash (
    PSTR SymbolName
    );

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

ULONG
ImpElfGnuHash (
    PSTR SymbolName
    );

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table
    using the GNU style hash function.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

PSTR
ImpElfGetEnvironmentVariable (
    PSTR Variable
    );

/*++

Routine Description:

    This routine gets an environment variable value for the image library.

Arguments:

    Variable - Supplies a pointer to a null terminated string containing the
        name of the variable to get.

Return Value:

    Returns a pointer to the value of the environment variable. The image
    library will not free or modify this value.

    NULL if the given environment variable is not set.

--*/
