/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    link.h

Abstract:

    This header contains definitions for interfacing with the dynamic linker.

Author:

    Evan Green 4-Aug-2016

--*/

#ifndef _LINK_H
#define _LINK_H

//
// ------------------------------------------------------------------- Includes
//

#include <libcbase.h>
#include <dlfcn.h>
#include <elf.h>
#include <stddef.h>

//
// --------------------------------------------------------------------- Macros
//

#if (__SIZEOF_POINTER__ == 8)

#define ElfW(_Type) Elf64_##_Type

#else

#define ElfW(_Type) Elf32_##_Type

#endif

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

enum __r_state_t {
    RT_CONSISTENT,
    RT_ADD,
    RT_DELETE
};

/*++

Structure Description:

    This structure stores dynamic linker information about a loaded image.

Members:

    l_next - Stores a pointer to the next image's link map.

    l_prev - Stores a pointer to the previous image's link map.

    l_addr - Stores the difference between the loaded lowest address and the
        image's preferred load address.

    l_name - Stores a pointer to the name of the image.

    l_ld - Stores a pointer to the dynamic section of the image.

--*/

struct link_map {
    struct link_map *l_next;
    struct link_map *l_prev;
    unsigned long l_addr;
    char *l_name;
    ElfW(Addr) l_ld;
};

/*++

Structure Description:

    This structure stores the debug information for a dynamically loaded image.

Members:

    r_version - Stores the debug structure version. The current version is 1.

    r_map - Stores a pointer to the link map.

    r_brk - Stores a pointer to a function that is called when a library is
        about to be added or removed.

    r_state - Stores the state of the library each time the r_brk function is
        called. Consistent means no change is occurring to this library, add
        means the library is being added, and delete means the library is
        being removed.

    r_ldbase - Stores the base address the dynamic linker is loaded at.

--*/

struct r_debug {
    int32_t r_version;
    struct link_map *r_map;
    ElfW(Addr) r_brk;
    uint32_t r_state;
    ElfW(Addr) r_ldbase;
};

/*++

Structure Description:

    This structure stores information about a loaded image.

Members:

    dlpi_addr - Stores the loaded base address of the image: the difference
        between the desired lowest VA of the image and its actual lowest VA.

    dlpi_name - Stores the name of the image.

    dlpi_phdr - Stores a pointer to the first program header of the image.

    dlpi_phnum - Stores the number of program headers in the array.

--*/

struct dl_phdr_info {
    ElfW(Addr) dlpi_addr;
    const char *dlpi_name;
    const ElfW(Phdr) *dlpi_phdr;
    ElfW(Half) dlpi_phnum;
};

typedef
int
(*__dl_iterate_phdr_cb_t) (
    struct dl_phdr_info *Header,
    size_t HeaderSize,
    void *Context
    );

/*++

Routine Description:

    This routine describes the prototype of the function that is called for
    each image during dl_iterate_phdr.

Arguments:

    Header - Supplies a pointer to the image information.

    HeaderSize - Supplies the size of the header structure, used for versioning
        new members.

    Context - Supplies the context pointer passed into the dl_iterate_phdr
        function.

Return Value:

    Returns an integer that if this is the last callback is returned from
    dl_iterate_phdr. If this was not the last callback, this value is ignored.

--*/

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

LIBC_API
int
dl_iterate_phdr (
    __dl_iterate_phdr_cb_t Callback,
    void *Context
    );

/*++

Routine Description:

    This routine iterates over all of the currently loaded images in the
    process.

Arguments:

    Callback - Supplies a pointer to a function to call for each image loaded,
        including the main executable. The header parameter points at a
        structure containing the image information. The size parameter
        describes the size of the header structure, and the context parameter
        is passed directly through from this routine.

    Context - Supplies an opaque pointer that is passed directly along to the
        callback.

Return Value:

    Returns the last value returned from the callback.

--*/

#ifdef __cplusplus

}

#endif
#endif

