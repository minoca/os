/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    dlfcn.h

Abstract:

    This header contains definitions for loading dynamic libraries at runtime.

Author:

    Evan Green 17-Oct-2013

--*/

#ifndef _DLFCN_H
#define _DLFCN_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define flags that can be passed to the dlopen function.
//

//
// Set this flag to have relocations performed on an as-needed basis.
//

#define RTLD_LAZY 0x00000000

//
// Set this flag to have relocations performed when an object is loaded.
//

#define RTLD_NOW 0x00000080

//
// Set this flag to have all symbols be available to other modules for
// dynamic linking.
//

#define RTLD_GLOBAL 0x00000100

//
// Set this flag to prevent symbols from being available to other modules for
// dynamic linking.
//

#define RTLD_LOCAL 0x00000000

//
// Provide this handle to search for symbols in the executing program's global
// scope.
//

#define RTLD_DEFAULT ((void *)0)

//
// Provide this handle to search for symbols in the executable after the
// currently executing program. "Next" is defined in terms of load order.
//

#define RTLD_NEXT ((void *)-1)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines dynamic library information for an address.

Members:

    dli_fname - Stores the path name of the library that stores the address.

    dli_fbase - Stores the base address at which the library is loaded.

    dli_sname - Stores the name of the symbol that contains the address.

    dli_saddr - Stores the address of the symbol. This may differ from the
        address used to look up the symbol.

--*/

typedef struct {
    const char *dli_fname;
    void *dli_fbase;
    const char *dli_sname;
    void *dli_saddr;
} Dl_info;

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
void *
dlopen (
    const char *Library,
    int Flags
    );

/*++

Routine Description:

    This routine opens and loads a dynamic library object with the given name.
    Only one instance of a given binary will be loaded per process.

Arguments:

    Library - Supplies a pointer to the null terminated string of the library
        to open. Supply NULL to open a handle to a global symbol table.

    Flags - Supplies a bitfield of flags governing the behavior of the library.
        See RTLD_* definitions.

Return Value:

    Returns an opaque handle to the library that can be used in calls to dlsym.

    NULL on failure, and more information can be retrieved via the dlerror
    function.

--*/

LIBC_API
int
dlclose (
    void *Handle
    );

/*++

Routine Description:

    This routine closes a previously opened dynamic library. This may or may
    not result in the library being unloaded, depending on what else has
    references out on it. Either way, callers should assume the handle is
    not valid for any future calls to the dlsym function.

Arguments:

    Handle - Supplies the opaque handle returned when the library was opened.

Return Value:

    0 on success.

    Non-zero on failure, and dlerror will be set to contain more information.

--*/

LIBC_API
char *
dlerror (
    void
    );

/*++

Routine Description:

    This routine returns a null terminated string (with no trailing newline)
    that describes the last error that occurred during dynamic linking
    processing. If no errors have occurred since the last invocation, NULL is
    returned. Invoking this routine a second time immediately following a prior
    invocation will return NULL. This routine is neither thread-safe nor
    reentrant.

Arguments:

    None.

Return Value:

    Returns a pointer to a string describing the last error on success. The
    caller is not responsible for freeing this memory.

    NULL if nothing went wrong since the last invocation.

--*/

__HIDDEN
void *
dlsym (
    void *Handle,
    const char *SymbolName
    );

/*++

Routine Description:

    This routine returns the address of a symbol defined within an object made
    accessible through a call to dlopen. This routine searches both this object
    and any objects loaded as a result of this one.

Arguments:

    Handle - Supplies a pointer to the opaque handle returned by the dlopen
        routine. Additionally, supply RTLD_DEFAULT to search through the
        executable (global) scope. Supply RTLD_NEXT to search for the next
        instance of the symbol after instance defined in the module that
        called dlsym.

    SymbolName - Supplies a pointer to a null-terminated string containing the
        name of the symbol whose address should be retrieved.

Return Value:

    Returns the address of the symbol on success.

    NULL if the handle was not valid or the symbol could not be found. More
    information can be retrieved via the dlerror function.

--*/

LIBC_API
void *
__dlsym (
    void *Handle,
    const char *SymbolName,
    void *CallerAddress
    );

/*++

Routine Description:

    This routine returns the address of a symbol defined within an object made
    accessible through a call to dlopen. This is an internal routine that
    should not be called directly by users.

Arguments:

    Handle - Supplies a pointer to the opaque handle returned by the dlopen
        routine.

    SymbolName - Supplies a pointer to a null-terminated string containing the
        name of the symbol whose address should be retrieved.

    CallerAddress - Supplies an address within the dynamic object of the
        calling executable. This routine will use this address to determine
        which object to start from and skip if RTLD_NEXT is provided as the
        handle.

Return Value:

    Returns the address of the symbol on success.

    NULL if the handle was not valid or the symbol could not be found. More
    information can be retrieved via the dlerror function.

--*/

LIBC_API
int
dladdr (
    void *Address,
    Dl_info *Information
    );

/*++

Routine Description:

    This routine resolves an address into the symbol and dynamic library
    information.

Arguments:

    Address - Supplies the address being resolved.

    Information - Supplies a pointer that recevies that dynamic library
        information for the given address.

Return Value:

    Non-zero on success.

    0 on failure, but dlerror will not be set to contain more information.

--*/

#ifdef __cplusplus

}

#endif
#endif

