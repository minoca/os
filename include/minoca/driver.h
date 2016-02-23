/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    driver.h

Abstract:

    This header contains all kernel definitions used by drivers.

Author:

    Evan Green 16-Sep-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#define KERNEL_API DLLIMPORT

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
#include <minoca/ioport.h>
#include <minoca/io.h>
#include <minoca/pm.h>
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
