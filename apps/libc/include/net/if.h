/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    if.h

Abstract:

    This header contains definitions for listing network interfaces in the
    C Library.

Author:

    Chris Stevens 21-Jul-2016

--*/

#ifndef _IF_H
#define _IF_H

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
// Define the maximum length of an network interface name, including the NULL
// terminator.
//

#define IF_NAMESIZE 16
#define IFNAMSIZ IF_NAMESIZE

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure defines a network interface.

Members:

    if_index - Stores the numeric index of the interface.

    if_name - Stores the null-terminated name of the interface.

--*/

struct if_nameindex {
    unsigned if_index;
    char *if_name;
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
struct if_nameindex *
if_nameindex (
    void
    );

/*++

Routine Description:

    This routine returns an array of all the available network interfaces on
    the system.

Arguments:

    None.

Return Value:

    An array of interface structures on success. The end of the array is
    indicated by a structure with a 0 index and NULL name.

    NULL on error, and errno will be set to contain more information.

--*/

LIBC_API
void
if_freenameindex (
    struct if_nameindex *Interfaces
    );

/*++

Routine Description:

    This routine releases an array of network interfaces.

Arguments:

    Interfaces - Supplies a pointer to the array of network interfaces to
        release.

Return Value:

    None.

--*/

LIBC_API
char *
if_indextoname (
    unsigned Index,
    char *Name
    );

/*++

Routine Description:

    This routine returns the name of the network interface with the given index.

Arguments:

    Index - Supplies the index of a network interface.

    Name - Supplies a pointer to a buffer where the interface name will be
        stored. The buffer must be at least IF_NAMESIZE.

Return Value:

    A pointer to the supplied name buffer on success.

    NULL on error, and errno will be set to contain more information.

--*/

LIBC_API
unsigned
if_nametoindex (
    const char *Name
    );

/*++

Routine Description:

    This routine returns the index of the network interface with the given name.

Arguments:

    Name - Supplies the name of a network interface.

Return Value:

    The index of the network interface on success.

    0 otherwise.

--*/

#ifdef __cplusplus

}

#endif
#endif

