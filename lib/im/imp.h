/*++

Copyright (c) 2012 Minoca Corp. All Rights Reserved

Module Name:

    imp.h

Abstract:

    This header contains definitions internal to the Image Library.

Author:

    Evan Green 13-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

#define RTL_API DLLEXPORT

#include <minoca/driver.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the macros to the various functions.
//

#define ImAllocateMemory ImImportTable->AllocateMemory
#define ImFreeMemory ImImportTable->FreeMemory
#define ImOpenFile ImImportTable->OpenFile
#define ImCloseFile ImImportTable->CloseFile
#define ImLoadFile ImImportTable->LoadFile
#define ImUnloadFile ImImportTable->UnloadFile
#define ImAllocateAddressSpace ImImportTable->AllocateAddressSpace
#define ImFreeAddressSpace ImImportTable->FreeAddressSpace
#define ImMapImageSegment ImImportTable->MapImageSegment
#define ImUnmapImageSegment ImImportTable->UnmapImageSegment
#define ImNotifyImageLoad ImImportTable->NotifyImageLoad
#define ImNotifyImageUnload ImImportTable->NotifyImageUnload
#define ImInvalidateInstructionCacheRegion \
    ImImportTable->InvalidateInstructionCacheRegion

#define ImGetEnvironmentVariable ImImportTable->GetEnvironmentVariable
#define ImFinalizeSegments ImImportTable->FinalizeSegments

//
// Define the maximum import recursion depth.
//

#define MAX_IMPORT_RECURSION_DEPTH 1000

//
// ------------------------------------------------------ Data Type Definitions
//

//
// -------------------------------------------------------------------- Globals
//

extern PIM_IMPORT_TABLE ImImportTable;

//
// -------------------------------------------------------- Function Prototypes
//
