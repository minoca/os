/*++

Copyright (c) 2015 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

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
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

INT
CkpDefaultSaveModule (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCSTR FrozenData,
    UINTN FrozenDataSize
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
CkpReadSource (
    PCK_VM Vm,
    PCSTR ModulePath,
    UINTN PathLength,
    FILE *File,
    UINTN Size,
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
    CkpDefaultSaveModule,
    CkpDefaultUnloadForeignModule,
    CkpDefaultWrite,
    CkpDefaultError,
    CkpDefaultUnhandledException,
    CK_INITIAL_HEAP_DEFAULT,
    CK_MINIMUM_HEAP_DEFAULT,
    CK_HEAP_GROWTH_DEFAULT,
    0
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
        will be separated with dots. If this contains a slash, then it is an
        absolute path that should be loaded directly.

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
    // If the module path contains a slash, just try to load it directly.
    //

    if (strchr(ModulePath, '/') != NULL) {
        Result = CkpLoadDynamicModule(Vm, NULL, ModulePath, ModuleData);
        if (Result != CkLoadModuleForeign) {
            Result = CkpLoadSourceFile(Vm, NULL, ModulePath, ModuleData);
        }

        return Result;
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
        free(ModuleCopy);
        return Result;
    }

    for (PathIndex = 0; PathIndex < PathCount; PathIndex += 1) {
        CkListGet(Vm, -1, PathIndex);
        Directory = CkGetString(Vm, -1, NULL);
        if (Directory == NULL) {
            Directory = "";
        }

        Result = CkpLoadSourceFile(Vm, Directory, ModuleCopy, ModuleData);
        if (Result != CkLoadModuleNotFound) {
            break;
        }

        Result = CkpLoadDynamicModule(Vm, Directory, ModuleCopy, ModuleData);
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
    free(ModuleCopy);
    return Result;
}

INT
CkpDefaultSaveModule (
    PCK_VM Vm,
    PCSTR ModulePath,
    PCSTR FrozenData,
    UINTN FrozenDataSize
    )

/*++

Routine Description:

    This routine is called after a module is compiled, so that the caller can
    save the compilation object.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModulePath - Supplies a pointer to the source file path that was just
        loaded.

    FrozenData - Supplies an opaque binary representation of the compiled
        module. The format of this data is unspecified and may change between
        revisions of the language.

    FrozenDataSize - Supplies the number of bytes in the frozen module data.

Return Value:

    0 always, since failure is non-fatal.

--*/

{

    PSTR Dot;
    FILE *File;
    size_t Length;
    CHAR Path[PATH_MAX];
    ssize_t Written;

    //
    // Use the same path as the source file, but replace the extension with
    // the object extension.
    //

    Length = strlen(ModulePath);
    if (Length + 2 > PATH_MAX) {
        return 0;
    }

    memcpy(Path, ModulePath, Length + 1);
    Dot = strrchr(Path, '.');
    if (Dot == NULL) {
        return 0;
    }

    if (Dot + 1 + strlen(CK_OBJECT_EXTENSION) >= Path + PATH_MAX) {
        return 0;
    }

    strcpy(Dot + 1, CK_OBJECT_EXTENSION);

    //
    // Attempt to save the file.
    //

    File = fopen(Path, "wb");
    if (File == NULL) {
        return 0;
    }

    Written = fwrite(FrozenData, 1, FrozenDataSize, File);
    fclose(File);

    //
    // If not everything was written, delete the file so as not to have half
    // baked objects.
    //

    if (Written != FrozenDataSize) {
        unlink(Path);
    }

    return 0;
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
    CkStackPop(Vm);

    //
    // Set the exception as the return value.
    //

    CkStackReplace(Vm, 0);
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

    ModulePath - Supplies a pointer to the module path to load.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

{

    FILE *File;
    off_t FileSize;
    CK_LOAD_MODULE_RESULT LoadStatus;
    CHAR ObjectPath[PATH_MAX];
    INT ObjectPathLength;
    struct stat ObjectStat;
    INT ObjectStatus;
    CHAR Path[PATH_MAX];
    INT PathLength;
    INT SourceStatus;
    struct stat Stat;

    File = NULL;
    LoadStatus = CkLoadModuleStaticError;

    //
    // Get the full path to the source file.
    //

    if (Directory == NULL) {
        PathLength = snprintf(Path, PATH_MAX, "%s", ModulePath);

    } else if (*Directory == '\0') {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s.%s",
                              ModulePath,
                              CK_SOURCE_EXTENSION);

    } else {
        PathLength = snprintf(Path,
                              PATH_MAX,
                              "%s/%s.%s",
                              Directory,
                              ModulePath,
                              CK_SOURCE_EXTENSION);
    }

    if ((PathLength < 0) || (PathLength > PATH_MAX)) {
        ModuleData->Error = "Max path length exceeded";
        return CkLoadModuleStaticError;
    }

    Path[PATH_MAX - 1] = '\0';

    //
    // Get the path to the pre-compiled object.
    //

    if (Directory == NULL) {
        ObjectPathLength = -1;

    } else if (*Directory == '\0') {
        ObjectPathLength = snprintf(ObjectPath,
                                    PATH_MAX,
                                    "%s.%s",
                                    ModulePath,
                                    CK_OBJECT_EXTENSION);

    } else {
        ObjectPathLength = snprintf(ObjectPath,
                                    PATH_MAX,
                                    "%s/%s.%s",
                                    Directory,
                                    ModulePath,
                                    CK_OBJECT_EXTENSION);
    }

    if ((ObjectPathLength < 0) || (ObjectPathLength > PATH_MAX)) {
        ObjectPath[0] = '\0';
        ObjectPathLength = 0;
    }

    //
    // Stat both the source and the object. Both must be files, and the object
    // must have a non-zero size.
    //

    SourceStatus = stat(Path, &Stat);
    if ((SourceStatus == 0) && (!S_ISREG(Stat.st_mode))) {
        SourceStatus = -1;
    }

    ObjectStatus = -1;
    if (ObjectPathLength != 0) {
        ObjectStatus = stat(ObjectPath, &ObjectStat);
        if ((ObjectStatus == 0) &&
            ((!S_ISREG(ObjectStat.st_mode)) || (ObjectStat.st_size == 0))) {

            ObjectStatus = -1;
        }
    }

    //
    // If neither exists, the the module can't be found.
    //

    if ((SourceStatus != 0) && (ObjectStatus != 0)) {
        LoadStatus = CkLoadModuleNotFound;
        goto LoadSourceFileEnd;
    }

    //
    // If the object path exists and is either 1) the only thing that exists or
    // 2) is newer than the source, then try to open the object file.
    //

    FileSize = Stat.st_size;
    if ((ObjectStatus == 0) &&
        ((SourceStatus != 0) || (ObjectStat.st_mtime >= Stat.st_mtime))) {

        File = fopen(ObjectPath, "rb");
        if (File != NULL) {
            FileSize = ObjectStat.st_size;
            PathLength = ObjectPathLength;
        }

    }

    //
    // Try to open the source if opening the object file was not an option or
    // failed.
    //

    if ((File == NULL) && (SourceStatus == 0)) {
        File = fopen(Path, "rb");
    }

    if (File == NULL) {
        LoadStatus = CkLoadModuleStaticError;
        goto LoadSourceFileEnd;
    }

    LoadStatus = CkpReadSource(Vm,
                               Path,
                               PathLength,
                               File,
                               FileSize,
                               ModuleData);

LoadSourceFileEnd:
    if (LoadStatus == CkLoadModuleStaticError) {
        if ((errno == ENOENT) || (errno == EACCES) || (errno == EPERM)) {
            return CkLoadModuleNotFound;
        }

        ModuleData->Error = strerror(errno);
    }

    if (File != NULL) {
        fclose(File);
    }

    return LoadStatus;
}

CK_LOAD_MODULE_RESULT
CkpReadSource (
    PCK_VM Vm,
    PCSTR ModulePath,
    UINTN PathLength,
    FILE *File,
    UINTN Size,
    PCK_MODULE_HANDLE ModuleData
    )

/*++

Routine Description:

    This routine is called to read in the contents of a Chalk source file.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ModulePath - Supplies a pointer to the opened path.

    PathLength - Supplies the length of the opened path in bytes, not including
        the null terminator.

    File - Supplies a pointer to the open file.

    Size - Supplies the size of the file in bytes.

    ModuleData - Supplies a pointer where the loaded module information will
        be returned on success.

Return Value:

    Returns a load module error code.

--*/

{

    CK_LOAD_MODULE_RESULT LoadStatus;
    PSTR Source;

    LoadStatus = CkLoadModuleStaticError;
    Source = CkAllocate(Vm, Size + 1);
    if (Source == NULL) {
        LoadStatus = CkLoadModuleNoMemory;
        goto ReadSourceEnd;
    }

    if (fread(Source, 1, Size, File) != Size) {
        LoadStatus = CkLoadModuleStaticError;
        goto ReadSourceEnd;
    }

    Source[Size] = '\0';
    ModuleData->Source.Path = CkAllocate(Vm, PathLength + 1);
    if (ModuleData->Source.Path == NULL) {
        LoadStatus = CkLoadModuleNoMemory;
        goto ReadSourceEnd;
    }

    CkCopy(ModuleData->Source.Path, ModulePath, PathLength + 1);
    ModuleData->Source.PathLength = PathLength;
    ModuleData->Source.Text = Source;
    Source = NULL;
    ModuleData->Source.Length = Size;
    LoadStatus = CkLoadModuleSource;

ReadSourceEnd:
    if (Source != NULL) {
        CkFree(Vm, Source);
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

    Directory - Supplies a pointer to the directory to find the module in. If
        this is NULL, then the module path will be used without modification.

    ModulePath - Supplies a pointer to the module path to load.

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
    if (Directory == NULL) {
        PathLength = snprintf(Path, PATH_MAX, "%s", ModulePath);

    } else if (*Directory == '\0') {
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

