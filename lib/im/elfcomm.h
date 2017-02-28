/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
ImpElfOpenWithPathList (
    PLOADED_IMAGE Parent,
    PCSTR LibraryName,
    PSTR PathList,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    );

/*++

Routine Description:

    This routine attempts to load a needed library for an ELF image.

Arguments:

    Parent - Supplies a pointer to the image that needs the library.

    LibraryName - Supplies the name of the library to load.

    PathList - Supplies a colon-separated list of paths to try.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

ULONG
ImpElfOriginalHash (
    PCSTR SymbolName
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
    PCSTR SymbolName
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
