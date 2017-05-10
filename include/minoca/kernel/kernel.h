/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    kernel.h

Abstract:

    This header contains the kernel's internal API.

Author:

    Evan Green 6-Aug-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#define KERNEL_API __DLLPROTECTED
#define CRYPTO_API __DLLPROTECTED

#ifndef RTL_API

#define RTL_API __DLLPROTECTED

#endif

#include <minoca/lib/types.h>
#include <minoca/lib/status.h>
#include <minoca/lib/im.h>
#include <minoca/lib/rtl.h>
#include <minoca/kernel/arch.h>
#include <minoca/kernel/mm.h>
#include <minoca/kernel/ob.h>
#include <minoca/kernel/ksignals.h>
#include <minoca/kernel/ps.h>
#include <minoca/kernel/hmod.h>
#include <minoca/kernel/ke.h>
#include <minoca/kernel/hl.h>
#include <minoca/kernel/acpi.h>
#include <minoca/kernel/io.h>
#include <minoca/kernel/pm.h>
#include <minoca/kernel/sp.h>
#include <minoca/kernel/knet.h>
#include <minoca/kernel/syscall.h>

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

