/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

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

#define KERNEL_API DLLEXPORT_PROTECTED
#define CRYPTO_API DLLEXPORT_PROTECTED

#ifndef RTL_API

#define RTL_API DLLEXPORT_PROTECTED

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
