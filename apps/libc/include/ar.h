/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU Lesser General Public
    License version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details.

Module Name:

    ar.h

Abstract:

    This header defines the portable archive format.

Author:

    Evan Green 2-Aug-2013

--*/

#ifndef _AR_H
#define _AR_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#ifdef __cplusplus

extern "C" {

#endif

//
// Define the magic string at the beginning of every archive.
//

#define ARMAG "!<arch>\n"

//
// Define the size of the archive magic string.
//

#define SARMAG 8

//
// Define the magic value that goes at the end of each header.
//

#define ARFMAG "`\n"

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the portable archive file header format.

Members:

    ar_name - Stores the name of the file if it fits, terminated with a / and
        with blanks beyond that if the file name is shorter. If the file name
        does not fit, there are non-standardized values in this field.

    ar_date - Stores the ASCII representation of the file date in decimal
        seconds since the 1970 epoch.

    ar_uid - Stores the ASCII decimal representation of the user ID that owns
        the file.

    ar_gid - Stores the ASCII decimal representation of the group ID that owns
        the file.

    ar_mode - Stores the file mode, in ASCII octal.

    ar_size - Stores the file size, in ASCII decimal.

    ar_fmag - Contains the magic value ARFMAG.

--*/

struct ar_hdr {
    char ar_name[16];
    char ar_date[12];
    char ar_uid[6];
    char ar_gid[6];
    char ar_mode[8];
    char ar_size[10];
    char ar_fmag[2];
};

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#ifdef __cplusplus

}

#endif
#endif

