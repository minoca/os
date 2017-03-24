/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    endian.h

Abstract:

    This header contains definitions for the endian-ness of the machine.

Author:

    Evan Green 16-Apr-2015

--*/

#ifndef _ENDIAN_H
#define _ENDIAN_H

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// This machine is little endian.
//

#define BYTE_ORDER LITTLE_ENDIAN

#define LITTLE_ENDIAN 1234
#define BIG_ENDIAN 4321

//
// Define the byteswapping macros. For now, use the builtin, though this is
// a little compiler-specific.
//

#define __byteswap16(_Value) __builtin_bswap16(_Value)
#define __byteswap32(_Value) __builtin_bswap32(_Value)
#define __byteswap64(_Value) __builtin_bswap64(_Value)

#if BYTE_ORDER == LITTLE_ENDIAN

#define htobe16(_Value) __byteswap16(_Value)
#define htole16(_Value) (_Value)
#define be16toh(_Value) __byteswap16(_Value)
#define le16toh(_Value) (_Value)

#define htobe32(_Value) __byteswap32(_Value)
#define htole32(_Value) (_Value)
#define be32toh(_Value) __byteswap32(_Value)
#define le32toh(_Value) (_Value)

#define htobe64(_Value) __byteswap64(_Value)
#define htole64(_Value) (_Value)
#define be64toh(_Value) __byteswap64(_Value)
#define le64toh(_Value) (_Value)

#else

#define htobe16(_Value) (_Value)
#define htole16(_Value) __byteswap16(_Value)
#define be16toh(_Value) (_Value)
#define le16toh(_Value) __byteswap16(_Value)

#define htobe32(_Value) (_Value)
#define htole32(_Value) __byteswap32(_Value)
#define be32toh(_Value) (_Value)
#define le32toh(_Value) __byteswap32(_Value)

#define htobe64(_Value) (_Value)
#define htole64(_Value) __byteswap64(_Value)
#define be64toh(_Value) (_Value)
#define le64toh(_Value) __byteswap64(_Value)

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

//
// -------------------------------------------------------- Function Prototypes
//

#endif

