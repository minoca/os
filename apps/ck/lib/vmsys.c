/*++

Copyright (c) 2015 Minoca Corp. All Rights Reserved

Module Name:

    vmsys.c

Abstract:

    This module implements the default support functions needed to wire a
    Chalk interpreter up to the rest of the system in the default configuration.

Author:

    Evan Green 28-May-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "chalkp.h"
#include "vmsys.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// Define some default garbage collection parameters.
//

#define CK_INITIAL_HEAP_DEFAULT (1024 * 1024 * 10)
#define CK_MINIMUM_HEAP_DEFAULT (1024 * 1024)
#define CK_HEAP_GROWTH_DEFAULT 512

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

PVOID
CkpDefaultReallocate (
    PVOID Allocation,
    UINTN NewSize
    );

CK_LOAD_MODULE_RESULT
CkpDefaultLoadModule (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    );

VOID
CkpDefaultUnloadForeignModule (
    PVOID Data
    );

VOID
CkpDefaultWrite (
    PCK_VM Vm,
    PCSTR String
    );

VOID
CkpDefaultError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Message
    );

VOID
CkpDefaultUnhandledException (
    PCK_VM Vm
    );

CK_LOAD_MODULE_RESULT
CkpLoadSourceFile (
    PCK_VM Vm,
    PCSTR Directory,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    );

CK_LOAD_MODULE_RESULT
CkpLoadDynamicModule (
    PCK_VM Vm,
    PCSTR Directory,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    );

//
// -------------------------------------------------------------------- Globals
//

CK_CONFIGURATION CkDefaultConfiguration = {
    CkpDefaultReallocate,
    CkpDefaultLoadModule,
    CkpDefaultUnloadForeignModule,
    CkpDefaultWrite,
    CkpDefaultError,
    CkpDefaultUnhandledException,
    CK_INITIAL_HEAP_DEFAULT,
    CK_MINIMUM_HEAP_DEFAULT,
    CK_HEAP_GROWTH_DEFAULT
};

//
// ------------------------------------------------------------------ Functions
//

PVOID
CkpDefaultReallocate (
    PVOID Allocation,
    UINTN NewSize
    )

/*++

Routine Description:

    This routine contains the default Chalk reallocate routine, which wires up
    to the C library realloc function.

Arguments:

    Allocation - Supplies an optional pointer to the allocation to resize or
        free. If NULL, then this routine will allocate new memory.

    NewSize - Supplies the size of the desired allocation. If this is 0 and the
        allocation parameter is non-null, the given allocation will be freed.
        Otherwise it will be resized to requested size.

Return Value:

    Returns a pointer to the allocation on success.

    NULL on allocation failure, or in the case the memory is being freed.

--*/

{

    return realloc(Allocation, NewSize);
}

CK_LOAD_MODULE_RESULT
CkpDefaultLoadModule (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    )

/*++

Routine Description:

    This routine is called to load a new Chalk module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModulePath - Supplies a pointer to the module path to load. Directories
        will be separated with dots.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

{

    PSTR Character;
    PCSTR Directory;
    PSTR ModuleCopy;
    UINTN PathCount;
    UINTN PathIndex;
    CK_LOAD_MODULE_RESULT Result;

    if (!CkEnsureStack(Vm, 2)) {
        return CkLoadModuleNoMemory;
    }

    //
    // Copy the module path, and convert mydir.mymod into mydir/mymod.
    //

    ModuleCopy = strdup(ModulePath);
    if (ModuleCopy == NULL) {
        return CkLoadModuleNoMemory;
    }

    Character = ModuleCopy;
    while (*Character != '\0') {
        if (*Character == '.') {
            *Character = '/';
        }

        Character += 1;
    }

    CkPushModulePath(Vm);
    PathCount = CkListSize(Vm, -1);
    Result = CkLoadModuleNotFound;
    if (PathCount == 0) {
        CkStackPop(Vm);
        return Result;
    }

    for (PathIndex = 0; PathIndex < PathCount; PathIndex += 1) {
        CkListGet(Vm, -1, PathIndex);
        Directory = CkGetString(Vm, -1, NULL);
        Result = CkpLoadSourceFile(Vm, Directory, ModulePath, ModuleData);
        if (Result != CkLoadModuleNotFound) {
            break;
        }

        Result = CkpLoadDynamicModule(Vm, Directory, ModulePath, ModuleData);
        if (Result != CkLoadModuleNotFound) {
            break;
        }

        CkStackPop(Vm);
    }

    //
    // Pop the last list entry and the module path list.
    //

    CkStackPop(Vm);
    CkStackPop(Vm);
    return Result;
}

VOID
CkpDefaultUnloadForeignModule (
    PVOID Data
    )

/*++

Routine Description:

    This routine is called when a foreign module is being destroyed.

Arguments:

    Data - Supplies a pointer to the handle returned when the foreign module
        was loaded. This is usually a dynamic library handle.

Return Value:

    None.

--*/

{

    CK_ASSERT(Data != NULL);

    CkpFreeLibrary(Data);
    return;
}

VOID
CkpDefaultWrite (
    PCK_VM Vm,
    PCSTR String
    )

/*++

Routine Description:

    This routine is called to print text in Chalk.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    String - Supplies a pointer to the string to print. This routine should not
        modify or free this string.

Return Value:

    None.

--*/

{

    printf("%s", String);
    return;
}

VOID
CkpDefaultError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Message
    )

/*++

Routine Description:

    This routine when the Chalk interpreter experiences an error.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ErrorType - Supplies the type of error occurring.

    Message - Supplies a pointer to a string describing the error.

Return Value:

    None.

--*/

{

    if (Message == NULL) {
        Message = "";
    }

    switch (ErrorType) {
    case CkErrorNoMemory:
        fprintf(stderr, "Allocation failure\n");
        break;

    case CkErrorRuntime:
        fprintf(stderr, "Error: %s.\n", Message);
        break;

    case CkErrorCompile:
    default:
        fprintf(stderr, "Compile Error: %s.\n", Message);
        break;
    }

    return;
}

VOID
CkpDefaultUnhandledException (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the default handling of an unhandled exception. It
    takes one argument, the exception, and prints the exception and stack trace
    to stderr.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    UINTN Length;
    PCSTR String;

    fprintf(stderr, "Unhandled Exception:\n");
    CkPushValue(Vm, 1);
    CkCallMethod(Vm, "__str", 0);
    String = CkGetString(Vm, -1, &Length);
    fprintf(stderr, "%s\n", String);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_LOAD_MODULE_RESULT
CkpLoadSourceFile (
    PCK_VM Vm,
    PCSTR Directory,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    )

/*++

Routine Description:

    This routine is called to load a new Chalk source file.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Directory - Supplies a pointer to the directory to find the module in.

    ModulePath - Supplies a pointer to the module path to load. Directories
        will be separated with dots.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

{

    FILE *File;
    CK_LOAD_MODULE_RESULT LoadStatus;
    CHAR Path[PATH_MAX];
    INT PathLength;
    PSTR Source;
    struct stat Stat;

    File = NULL;
    LoadStatus = CkLoadModuleStaticError;
    Source = NULL;
    if ((Directory == NULL) || (*Directory == '\0')) {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s%s",
                              ModulePath,
                              CK_SOURCE_EXTENSION);

    } else {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s/%s%s",
                              Directory,
                              ModulePath,
                              CK_SOURCE_EXTENSION);
    }

    if ((PathLength < 0) || (PathLength > PATH_MAX)) {
        ModuleData->Error = "Max path length exceeded";
        return CkLoadModuleStaticError;
    }

    Path[PATH_MAX - 1] = '\0';
    if (stat(Path, &Stat) != 0) {
        goto LoadSourceFileEnd;
    }

    if (!S_ISREG(Stat.st_mode)) {
        return CkLoadModuleNotFound;
    }

    File = fopen(Path, "rb");
    if (File == NULL) {
        goto LoadSourceFileEnd;
    }

    Source = CkAllocate(Vm, Stat.st_size + 1);
    if (Source == NULL) {
        LoadStatus = CkLoadModuleNoMemory;
        goto LoadSourceFileEnd;
    }

    if (fread(Source, 1, Stat.st_size, File) != Stat.st_size) {
        goto LoadSourceFileEnd;
    }

    Source[Stat.st_size] = '\0';
    ModuleData->Source.Path = CkAllocate(Vm, PathLength + 1);
    if (ModuleData->Source.Path == NULL) {
        LoadStatus = CkLoadModuleNoMemory;
        goto LoadSourceFileEnd;
    }

    CkCopy(ModuleData->Source.Path, Path, PathLength + 1);
    ModuleData->Source.PathLength = PathLength;
    ModuleData->Source.Text = Source;
    Source = NULL;
    ModuleData->Source.Length = Stat.st_size;
    LoadStatus = CkLoadModuleSource;

LoadSourceFileEnd:
    if (LoadStatus == CkLoadModuleStaticError) {
        if ((errno == ENOENT) || (errno == EACCES) || (errno == EPERM)) {
            return CkLoadModuleNotFound;
        }

        ModuleData->Error = strerror(errno);
    }

    if (Source != NULL) {
        CkFree(Vm, Source);
    }

    if (File != NULL) {
        fclose(File);
    }

    return LoadStatus;
}

CK_LOAD_MODULE_RESULT
CkpLoadDynamicModule (
    PCK_VM Vm,
    PCSTR Directory,
    PCSTR ModulePath,
    PCK_MODULE_HANDLE ModuleData
    )

/*++

Routine Description:

    This routine is called to load a new Chalk dynamic library module.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Directory - Supplies a pointer to the directory to find the module in.

    ModulePath - Supplies a pointer to the module path to load. Directories
        will be separated with dots.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

{

    PCK_FOREIGN_FUNCTION EntryPoint;
    PVOID Handle;
    CK_LOAD_MODULE_RESULT LoadStatus;
    CHAR Path[PATH_MAX];
    INT PathLength;
    struct stat Stat;

    Handle = NULL;
    LoadStatus = CkLoadModuleStaticError;
    ModuleData->Error = NULL;
    if ((Directory == NULL) || (*Directory == '\0')) {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s%s",
                              ModulePath,
                              CkSharedLibraryExtension);

    } else {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s/%s%s",
                              Directory,
                              ModulePath,
                              CkSharedLibraryExtension);
    }

    if ((PathLength < 0) || (PathLength > PATH_MAX)) {
        ModuleData->Error = "Max path length exceeded";
        return CkLoadModuleStaticError;
    }

    Path[PATH_MAX - 1] = '\0';

    //
    // Validate that this path points to a regular file before trying to open
    // a dynamic library.
    //

    if (stat(Path, &Stat) != 0) {
        if ((errno == ENOENT) || (errno == EACCES) || (errno == EPERM)) {
            LoadStatus = CkLoadModuleNotFound;

        } else {
            ModuleData->Error = strerror(errno);
        }

        goto LoadDynamicModuleEnd;
    }

    if (!S_ISREG(Stat.st_mode)) {
        return CkLoadModuleNotFound;
    }

    //
    // Open up the dynamic library.
    //

    Handle = CkpLoadLibrary(Path);
    if (Handle == NULL) {
        return CkLoadModuleNotFound;
    }

    EntryPoint = CkpGetLibrarySymbol(Handle, CK_MODULE_ENTRY_NAME);
    if (EntryPoint == NULL) {
        LoadStatus = CkLoadModuleNotFound;
        goto LoadDynamicModuleEnd;
    }

    ModuleData->Foreign.Path = CkAllocate(Vm, PathLength + 1);
    if (ModuleData->Source.Path == NULL) {
        LoadStatus = CkLoadModuleNoMemory;
        goto LoadDynamicModuleEnd;
    }

    CkCopy(ModuleData->Foreign.Path, Path, PathLength + 1);
    ModuleData->Foreign.PathLength = PathLength;
    ModuleData->Foreign.Handle = Handle;
    Handle = NULL;
    ModuleData->Foreign.Entry = EntryPoint;
    LoadStatus = CkLoadModuleForeign;

LoadDynamicModuleEnd:
    if (Handle != NULL) {
        CkpFreeLibrary(Handle);
    }

    return LoadStatus;
}

