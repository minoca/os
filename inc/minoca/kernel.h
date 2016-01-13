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

#include <minoca/types.h>
#include <minoca/status.h>
#include <minoca/arch.h>
#include <minoca/rtl.h>
#include <minoca/mm.h>
#include <minoca/ob.h>
#include <minoca/ksignals.h>
#include <minoca/im.h>
#include <minoca/process.h>
#include <minoca/ke.h>
#include <minoca/hl.h>
#include <minoca/acpi.h>
#include <minoca/io.h>
#include <minoca/sp.h>
#include <minoca/knet.h>
#include <minoca/syscall.h>

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
