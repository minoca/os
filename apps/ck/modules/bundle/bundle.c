/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    bundle.c

Abstract:

    This module implements the Chalk bundle module, which allows for creation
    of a specialized application based on a Chalk environment.

Author:

    Evan Green 19-Oct-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#define _LARGEFILE64_SOURCE 1

#include <minoca/lib/types.h>
#include <minoca/lib/chalk.h>
#include <minoca/lib/chalk/app.h>

#include <errno.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

//
// --------------------------------------------------------------------- Macros
//

#ifdef _WIN32

#define mkdir(_Path, _Permissions) mkdir(_Path)

#define S_IXGRP 0
#define S_IXOTH 0

#endif

#if defined(__APPLE__) || defined(__CYGWIN__) || defined(__FreeBSD__)

#define fseeko64 fseeko
#define ftello64 ftello

#endif

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the magic value to search for to indicate the presence of a bundle.
//

#define CK_BUNDLE_MAGIC 0x7F6C646E75426B43ULL

//
// Define the size of the temporary file name.
//

#define CK_BUNDLE_NAME_SIZE 256

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpBundleModuleInit (
    PCK_VM Vm
    );

VOID
CkpBundleCreate (
    PCK_VM Vm
    );

INT
CkpBundleFreezeInteger (
    FILE *File,
    LONGLONG Value
    );

INT
CkpBundleFreezeBuffer (
    FILE *File,
    PCVOID Buffer,
    UINTN Size
    );

ULONG
CkpBundleChecksum (
    PVOID Buffer,
    UINTN Size
    );

PSTR
CkpFindBundle (
    PSTR Buffer,
    PSTR End,
    PUINTN Size
    );

INT
CkpLoadBundle (
    PCK_VM Vm,
    PSTR Bundle,
    UINTN BundleSize
    );

INT
CkpBundleLoadModules (
    PCK_VM Vm,
    PSTR *Bundle,
    PUINTN BundleSize
    );

INT
CkpBundleLoadModule (
    PCK_VM Vm,
    PSTR *Bundle,
    PUINTN BundleSize
    );

PSTR
CkpBundleThawElement (
    PSTR *Contents,
    PUINTN Size,
    PUINTN NameSize
    );

PSTR
CkpBundleThawString (
    PCK_VM Vm,
    PSTR *Contents,
    PUINTN Size,
    PUINTN StringSize
    );

BOOL
CkpBundleThawInteger (
    PSTR *Contents,
    PUINTN Size,
    PCK_INTEGER Integer
    );

PSTR
CkpBundleGetTemporaryDirectory (
    VOID
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkBundleModuleValues[] = {
    {CkTypeFunction, "create", CkpBundleCreate, 3},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// Define the temporary directory name where modules are stored.
//

CHAR CkBundleDirectory[256];

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadBundleModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the bundle module. It is called to make the presence
    of the module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    BOOL Result;

    Result = CkPreloadForeignModule(Vm,
                                    "bundle",
                                    NULL,
                                    NULL,
                                    CkpBundleModuleInit);

    return Result;
}

INT
CkBundleThaw (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine reloads the modules previously saved in a bundle. The exec
    name global should be set before calling this function.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Returns 0 if the bundle was loaded successfully.

    Returns -1 if no bundle could be found.

    Returns other values on error.

--*/

{

    PSTR Buffer;
    UINTN BundleSize;
    PSTR Current;
    PSTR End;
    FILE *File;
    struct stat Stat;
    INT Status;

    Buffer = NULL;
    if (stat(CkAppExecName, &Stat) != 0) {
        Status = errno;
        goto BundleThawEnd;
    }

    Buffer = malloc(Stat.st_size + 1);
    if (Buffer == NULL) {
        Status = errno;
        goto BundleThawEnd;
    }

    File = fopen(CkAppExecName, "rb");
    if (File == NULL) {
        Status = errno;
        goto BundleThawEnd;
    }

    if (fread(Buffer, 1, Stat.st_size, File) != Stat.st_size) {
        fclose(File);
        Status = errno;
        goto BundleThawEnd;
    }

    fclose(File);
    Buffer[Stat.st_size] = '\0';
    End = Buffer + Stat.st_size;
    Current = Buffer;
    Status = -1;
    while (TRUE) {
        Current = CkpFindBundle(Current, End, &BundleSize);
        if (Current == NULL) {
            break;
        }

        Status = CkpLoadBundle(Vm, Current, BundleSize);
        if (Status != 0) {
            goto BundleThawEnd;
        }

        Current += BundleSize;
    }

BundleThawEnd:
    if (Buffer != NULL) {
        free(Buffer);
    }

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpBundleModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the OS module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkDeclareVariables(Vm, 0, CkBundleModuleValues);
    return;
}

VOID
CkpBundleCreate (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine creates a new application bundle.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PVOID Buffer;
    size_t BufferSize;
    off_t ChecksumOffset;
    off_t EndOffset;
    FILE *Executable;
    FILE *File;
    FILE *ForeignFile;
    CK_INTEGER IsForeign;
    CK_INTEGER ModuleCount;
    UINTN ModuleIndex;
    PCSTR OutputName;
    PCSTR Path;
    UINTN PathSize;
    struct stat Stat;
    INT Status;
    PCSTR String;
    UINTN StringSize;
    off_t TotalSize;
    ULONG Value32;
    ULONGLONG Value64;

    Buffer = NULL;
    Executable = NULL;
    File = NULL;
    ForeignFile = NULL;
    OutputName = NULL;
    if (!CkCheckArguments(Vm, 3, CkTypeString, CkTypeList, CkTypeString)) {
        Status = -1;
        goto BundleCreateEnd;
    }

    //
    // This routine takes 3 arguments:
    // 1) The output file name.
    // 2) The set of modules to add.
    // 3) The expression to execute once all modules are preloaded.
    //

    if ((CkAppExecName == NULL) || (*CkAppExecName == '\0')) {
        Status = EINVAL;
        goto BundleCreateEnd;
    }

    //
    // Create the output file, and copy the executable to it.
    //

    OutputName = CkGetString(Vm, 1, NULL);
    File = fopen(OutputName, "wb+");
    if (File == NULL) {
        Status = errno;
        goto BundleCreateEnd;
    }

    if (stat(CkAppExecName, &Stat) != 0) {
        Status = errno;
        goto BundleCreateEnd;
    }

    Executable = fopen(CkAppExecName, "rb");
    if (Executable == NULL) {
        Status = errno;
        goto BundleCreateEnd;
    }

    BufferSize = Stat.st_size;
    Buffer = malloc(Stat.st_size);
    if (Buffer == NULL) {
        Status = errno;
        goto BundleCreateEnd;
    }

    if ((fread(Buffer, 1, Stat.st_size, Executable) != Stat.st_size) ||
        (fwrite(Buffer, 1, Stat.st_size, File) != Stat.st_size)) {

        Status = errno;
        if (Status == 0) {
            Status = EIO;
        }

        goto BundleCreateEnd;
    }

    //
    // Write out the magic value, and save room for the length and checksum.
    //

    Value64 = CK_BUNDLE_MAGIC;
    if (fwrite(&Value64, 1, sizeof(Value64), File) != sizeof(Value64)) {
        Status = errno;
        goto BundleCreateEnd;
    }

    ChecksumOffset = ftello64(File);
    if (ChecksumOffset == -1) {
        Status = errno;
        goto BundleCreateEnd;
    }

    Value32 = 0;
    if (fwrite(&Value32, 1, sizeof(Value32), File) != sizeof(Value32)) {
        Status = errno;
        goto BundleCreateEnd;
    }

    Value64 = 0;
    if (fwrite(&Value64, 1, sizeof(Value64), File) != sizeof(Value64)) {
        Status = errno;
        goto BundleCreateEnd;
    }

    if (fprintf(File, "{\nExpression: ") < 0) {
        Status = errno;
        goto BundleCreateEnd;
    }

    String = CkGetString(Vm, 3, &StringSize);
    if (String == NULL) {
        Status = EINVAL;
        goto BundleCreateEnd;
    }

    Status = CkpBundleFreezeBuffer(File, String, StringSize);
    if (Status != 0) {
        goto BundleCreateEnd;
    }

    if (fprintf(File, "\nModules: [\n") < 0) {
        Status = errno;
        goto BundleCreateEnd;
    }

    //
    // Now emit all the modules.
    //

    if (!CkGetLength(Vm, 2, &ModuleCount)) {
        Status = -1;
        goto BundleCreateEnd;
    }

    for (ModuleIndex = 0; ModuleIndex < ModuleCount; ModuleIndex += 1) {
        Path = NULL;
        PathSize = 0;
        CkListGet(Vm, 2, ModuleIndex);
        if (!CkCallMethod(Vm, "isForeign", 0)) {
            Status = -1;
            goto BundleCreateEnd;
        }

        IsForeign = CkGetInteger(Vm, -1);
        CkStackPop(Vm);

        //
        // If the module is foreign, attempt to get its path. If it has no
        // path, then skip it.
        //

        if (IsForeign != 0) {
            CkListGet(Vm, 2, ModuleIndex);
            if (!CkCallMethod(Vm, "path", 0)) {
                Status = -1;
                goto BundleCreateEnd;
            }

            Path = CkGetString(Vm, -1, &PathSize);
            if ((Path == NULL) || (PathSize == 0)) {
                CkStackPop(Vm);
                continue;
            }
        }

        //
        // Write the foreign boolean.
        //

        if (fprintf(File, "{\nForeign: ") < 0) {
            Status = errno;
            goto BundleCreateEnd;
        }

        Status = CkpBundleFreezeInteger(File, IsForeign);
        if (Status != 0) {
            goto BundleCreateEnd;
        }

        //
        // Write the module name.
        //

        if (fprintf(File, "\nName: ") < 0) {
            Status = errno;
            goto BundleCreateEnd;
        }

        CkListGet(Vm, 2, ModuleIndex);
        if (!CkCallMethod(Vm, "name", 0)) {
            Status = -1;
            goto BundleCreateEnd;
        }

        String = CkGetString(Vm, -1, &StringSize);
        Status = CkpBundleFreezeBuffer(File, String, StringSize);
        if (Status != 0) {
            goto BundleCreateEnd;
        }

        CkStackPop(Vm);

        //
        // Write out the path if there is one.
        //

        if (IsForeign != FALSE) {
            if (fprintf(File, "\nPath: ") < 0) {
                Status = errno;
                goto BundleCreateEnd;
            }

            Status = CkpBundleFreezeBuffer(File, Path, PathSize);
            if (Status != 0) {
                goto BundleCreateEnd;
            }

            //
            // Read in the shared object file.
            //

            if (stat(Path, &Stat) != 0) {
                Status = errno;
                goto BundleCreateEnd;
            }

            ForeignFile = fopen(Path, "rb");
            CkStackPop(Vm);
            Path = NULL;
            if (ForeignFile == NULL) {
                Status = errno;
                goto BundleCreateEnd;
            }

            if (BufferSize < Stat.st_size) {
                BufferSize = Stat.st_size;
                free(Buffer);
                Buffer = malloc(BufferSize);
                if (Buffer == NULL) {
                    Status = errno;
                    goto BundleCreateEnd;
                }
            }

            if (fread(Buffer, 1, Stat.st_size, ForeignFile) != Stat.st_size) {
                Status = errno;
                goto BundleCreateEnd;
            }

            fclose(ForeignFile);
            ForeignFile = NULL;
            if (fprintf(File, "\nData: ") < 0) {
                Status = -1;
                goto BundleCreateEnd;
            }

            Status = CkpBundleFreezeBuffer(File, Buffer, Stat.st_size);
            if (Status != 0) {
                goto BundleCreateEnd;
            }

        //
        // This is not a foreign module. Freeze it and write out the frozen
        // contents.
        //

        } else {
            if (fprintf(File, "\nData: ") < 0) {
                Status = -1;
                goto BundleCreateEnd;
            }

            CkListGet(Vm, 2, ModuleIndex);
            if (!CkCallMethod(Vm, "freeze", 0)) {
                Status = -1;
                goto BundleCreateEnd;
            }

            String = CkGetString(Vm, -1, &StringSize);
            Status = CkpBundleFreezeBuffer(File, String, StringSize);
            if (Status != 0) {
                goto BundleCreateEnd;
            }

            CkStackPop(Vm);
        }

        //
        // Print the module terminator.
        //

        if (fprintf(File, "\n}") < 0) {
            Status = errno;
            goto BundleCreateEnd;
        }

        //
        // Print the list separator.
        //

        if (ModuleIndex != ModuleCount - 1) {
            if (fprintf(File, ", \n") < 0) {
                Status = errno;
                goto BundleCreateEnd;
            }
        }
    }

    if (fprintf(File, "]\n}\n") < 0) {
        Status = errno;
        goto BundleCreateEnd;
    }

    //
    // Write the length in its final place place.
    //

    EndOffset = ftello64(File);
    if ((EndOffset == -1) || (EndOffset < ChecksumOffset)) {
        Status = EINVAL;
        goto BundleCreateEnd;
    }

    TotalSize = EndOffset - ChecksumOffset;
    Value64 = TotalSize;
    if ((fseeko64(File, ChecksumOffset + sizeof(Value32), SEEK_SET) < 0) ||
        (fwrite(&Value64, 1, sizeof(Value64), File) != sizeof(Value64))) {

        Status = errno;
        goto BundleCreateEnd;
    }

    //
    // Read in the full contents.
    //

    if (BufferSize < TotalSize) {
        free(Buffer);
        BufferSize = TotalSize;
        Buffer = malloc(BufferSize);
        if (Buffer == NULL) {
            Status = errno;
            goto BundleCreateEnd;
        }
    }

    if ((fseeko64(File, ChecksumOffset, SEEK_SET) < 0) ||
        (fread(Buffer, 1, TotalSize, File) != TotalSize)) {

        Status = errno;
        goto BundleCreateEnd;
    }

    //
    // Write out the checksum.
    //

    Value32 = CkpBundleChecksum(Buffer, TotalSize);
    if ((fseeko64(File, ChecksumOffset, SEEK_SET) < 0) ||
        (fwrite(&Value32, 1, sizeof(Value32), File) != sizeof(Value32))) {

        Status = errno;
        goto BundleCreateEnd;
    }

    Status = 0;

BundleCreateEnd:
    if (Executable != NULL) {
        fclose(Executable);
    }

    if (File != NULL) {
        fclose(File);
        if (stat(OutputName, &Stat) == 0) {
            chmod(OutputName, Stat.st_mode | S_IXUSR | S_IXOTH | S_IXGRP);
        }
    }

    if (ForeignFile != NULL) {
        fclose(ForeignFile);
    }

    if (Buffer != NULL) {
        free(Buffer);
    }

    if (Status != 0) {
        if (Status != -1) {
            CkRaiseBasicException(Vm,
                                  "RuntimeError",
                                  "Error during bundle creation: %s",
                                  strerror(Status));
        }
    }

    return;
}

INT
CkpBundleFreezeInteger (
    FILE *File,
    LONGLONG Value
    )

/*++

Routine Description:

    This routine prints an integer to the frozen data.

Arguments:

    File - Supplies a pointer to the file to write out to.

    Value - Supplies the value to write.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    if (fprintf(File, "i%lld ", Value) < 0) {
        return errno;
    }

    return 0;
}

INT
CkpBundleFreezeBuffer (
    FILE *File,
    PCVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine prints a string or raw byte buffer to the file.

Arguments:

    File - Supplies a pointer to the file to write out to.

    Buffer - Supplies a pointer to the buffer to write.

    Size - Supplies the size of the buffer in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    if (fprintf(File, "s%ld\"", Size) < 0) {
        return errno;
    }

    if (fwrite(Buffer, 1, Size, File) != Size) {
        if (errno == 0) {
            return -1;
        }

        return errno;
    }

    if (fprintf(File, "\"") < 0) {
        return errno;
    }

    return 0;
}

ULONG
CkpBundleChecksum (
    PVOID Buffer,
    UINTN Size
    )

/*++

Routine Description:

    This routine performs a simple checksum on the given buffer.

Arguments:

    Buffer - Supplies a pointer to the buffer to sum.

    Size - Supplies the number of bytes to sum.

Return Value:

    Returns the sum of all the bytes.

--*/

{

    PUCHAR Bytes;
    PUCHAR End;
    ULONG Sum;

    Bytes = Buffer;
    End = Buffer + Size;
    Sum = 0;
    while (Bytes < End) {
        Sum += *Bytes;
        Bytes += 1;
    }

    return Sum;
}

PSTR
CkpFindBundle (
    PSTR Buffer,
    PSTR End,
    PUINTN Size
    )

/*++

Routine Description:

    This routine locates a valid bundle within the given bundle.

Arguments:

    Buffer - Supplies a pointer to the buffer to search.

    End - Supplies a pointer one beyond the end of the buffer.

    Size - Supplies a pointer where the size of the bundle will be returned.

Return Value:

    Returns a pointer to the bundle just beyond the magic, checksum, and length.

    NULL if no valid bundle could be found.

--*/

{

    ULONG Checksum;
    ULONG ComputedChecksum;
    ULONGLONG Length;
    ULONGLONG Magic;

    Magic = CK_BUNDLE_MAGIC;
    while (Buffer + sizeof(Magic) + sizeof(Checksum) + sizeof(Length) <= End) {

        //
        // Get excited if the magic value is here.
        //

        if ((*Buffer == (CHAR)Magic) &&
            (memcmp(Buffer, &Magic, sizeof(Magic)) == 0)) {

            Buffer += sizeof(Magic);

            //
            // Grab the checksum and length, and validate them.
            //

            memcpy(&Checksum, Buffer, sizeof(Checksum));
            memcpy(&Length, Buffer + sizeof(Checksum), sizeof(Length));
            if ((Length > sizeof(Checksum) + sizeof(Length)) &&
                (Buffer + Length <= End)) {

                ComputedChecksum = CkpBundleChecksum(Buffer + sizeof(Checksum),
                                                     Length - sizeof(Checksum));

                if (Checksum == ComputedChecksum) {
                    *Size = Length - (sizeof(Checksum) + sizeof(Length));
                    return Buffer + sizeof(Checksum) + sizeof(Length);
                }
            }

        } else {
            Buffer += 1;
        }
    }

    return NULL;
}

INT
CkpLoadBundle (
    PCK_VM Vm,
    PSTR Bundle,
    UINTN BundleSize
    )

/*++

Routine Description:

    This routine loads up the modules in a bundle.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Bundle - Supplies a pointer to the bundle.

    BundleSize - Supplies the size of the bundle in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PCSTR Expression;
    UINTN ExpressionSize;
    PCSTR Name;
    UINTN NameSize;
    INT Status;

    Expression = NULL;
    if ((BundleSize < 2) || (*Bundle != '{')) {
        Status = EINVAL;
        goto LoadBundleEnd;
    }

    Bundle += 1;
    BundleSize -= 1;

    //
    // Loop pulling elements out of the outer dictionary.
    //

    while (TRUE) {
        Name = CkpBundleThawElement(&Bundle, &BundleSize, &NameSize);
        if (Name == NULL) {
            break;
        }

        if ((NameSize == 10) && (memcmp(Name, "Expression", 10) == 0)) {
            Expression = CkpBundleThawString(Vm,
                                             &Bundle,
                                             &BundleSize,
                                             &ExpressionSize);

            if (Expression == NULL) {
                Status = EINVAL;
                goto LoadBundleEnd;
            }

        } else if ((NameSize == 7) && (memcmp(Name, "Modules", 7) == 0)) {
            Status = CkpBundleLoadModules(Vm, &Bundle, &BundleSize);
            if (Status != 0) {
                goto LoadBundleEnd;
            }
        }
    }

    if (*Bundle != '}') {
        Status = EINVAL;
        goto LoadBundleEnd;
    }

    Status = 0;

LoadBundleEnd:
    if (Status != 0) {
        fprintf(stderr, "Failed to load bundle: %s\n", strerror(Status));

    } else {

        //
        // The bundle's all loaded, execute the expression.
        //

        if (Expression != NULL) {
            Status = CkInterpret(Vm,
                                 NULL,
                                 Expression,
                                 ExpressionSize,
                                 1,
                                 FALSE);
        }
    }

    return Status;
}

INT
CkpBundleLoadModules (
    PCK_VM Vm,
    PSTR *Bundle,
    PUINTN BundleSize
    )

/*++

Routine Description:

    This routine loads up the modules portion of a bundle.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Bundle - Supplies a pointer to the bundle.

    BundleSize - Supplies the size of the bundle in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    INT Status;

    if ((*BundleSize < 2) || (**Bundle != '[')) {
        return EINVAL;
    }

    *Bundle += 2;
    *BundleSize -= 2;
    while (TRUE) {
        Status = CkpBundleLoadModule(Vm, Bundle, BundleSize);
        if (Status != 0) {
            return Status;
        }

        if (*BundleSize < 1) {
            return EINVAL;
        }

        if (**Bundle != ',') {
            break;
        }

        *Bundle += 1;
        *BundleSize -= 1;
        while ((*BundleSize > 0) &&
               ((**Bundle == ' ') || (**Bundle == '\n'))) {

            *Bundle += 1;
            *BundleSize -= 1;
        }
    }

    if ((*BundleSize < 1) || (**Bundle != ']')) {
        return EINVAL;
    }

    *Bundle += 1;
    *BundleSize -= 1;
    Status = 0;
    return Status;
}

INT
CkpBundleLoadModule (
    PCK_VM Vm,
    PSTR *Bundle,
    PUINTN BundleSize
    )

/*++

Routine Description:

    This routine loads a single module within a bundle.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Bundle - Supplies a pointer to the bundle.

    BundleSize - Supplies the size of the bundle in bytes.

Return Value:

    0 on success.

    Returns an error number on failure.

--*/

{

    PSTR BaseName;
    PSTR Current;
    PCSTR Data;
    UINTN DataSize;
    PSTR Directory;
    FILE *File;
    CHAR FileName[CK_BUNDLE_NAME_SIZE];
    CK_INTEGER Foreign;
    PCSTR ModuleName;
    UINTN ModuleNameSize;
    PCSTR Name;
    UINTN NameSize;
    PSTR Path;
    UINTN PathSize;
    UINTN Size;
    INT Status;

    Data = NULL;
    Directory = NULL;
    File = NULL;
    Path = NULL;
    ModuleName = NULL;
    Foreign = FALSE;
    Current = *Bundle;
    Size = *BundleSize;

    //
    // Parse out all the elements.
    //

    if ((Size == 0) || (*Current != '{')) {
        Status = EINVAL;
        goto BundleLoadModuleEnd;
    }

    Current += 1;
    Size -= 1;
    while (TRUE) {
        Name = CkpBundleThawElement(&Current, &Size, &NameSize);
        if (Name == NULL) {
            break;
        }

        if ((NameSize == 7) && (memcmp(Name, "Foreign", 7) == 0)) {
            if (!CkpBundleThawInteger(&Current, &Size, &Foreign)) {
                Status = EINVAL;
                goto BundleLoadModuleEnd;
            }

        } else if ((NameSize == 4) && (memcmp(Name, "Path", 4) == 0)) {
            Path = CkpBundleThawString(Vm, &Current, &Size, &PathSize);

        } else if ((NameSize == 4) && (memcmp(Name, "Name", 4) == 0)) {
            ModuleName = CkpBundleThawString(Vm,
                                             &Current,
                                             &Size,
                                             &ModuleNameSize);

        } else if ((NameSize == 4) && (memcmp(Name, "Data", 4) == 0)) {
            Data = CkpBundleThawString(Vm, &Current, &Size, &DataSize);

        } else {
            Status = EILSEQ;
            goto BundleLoadModuleEnd;
        }
    }

    if ((Size == 0) || (*Current != '}')) {
        Status = EINVAL;
        goto BundleLoadModuleEnd;
    }

    Current += 1;
    Size -= 1;
    if ((Data == NULL) || (ModuleName == NULL)) {
        Status = EINVAL;
        goto BundleLoadModuleEnd;
    }

    if (Directory == NULL) {
        Directory = CkpBundleGetTemporaryDirectory();
        if (Directory == NULL) {
            Status = errno;
            goto BundleLoadModuleEnd;
        }
    }

    //
    // Get the module file name.
    //

    if (Foreign != FALSE) {
        if (Path == NULL) {
            Status = EINVAL;
            goto BundleLoadModuleEnd;
        }

        BaseName = basename(Path);
        snprintf(FileName, CK_BUNDLE_NAME_SIZE, "%s/%s", Directory, BaseName);
        FileName[CK_BUNDLE_NAME_SIZE - 1] = '\0';

    } else {
        snprintf(FileName,
                 CK_BUNDLE_NAME_SIZE,
                 "%s/%s.%s",
                 Directory,
                 ModuleName,
                 CK_SOURCE_EXTENSION);
    }

    //
    // Write out the module.
    //

    File = fopen(FileName, "wb");
    if (File == NULL) {
        Status = errno;
        goto BundleLoadModuleEnd;
    }

    if (fwrite(Data, 1, DataSize, File) != DataSize) {
        fclose(File);
        Status = errno;
        goto BundleLoadModuleEnd;
    }

    fclose(File);

    //
    // Fire up the library.
    //

    if (!CkLoadModule(Vm, ModuleName, FileName)) {
        Status = EILSEQ;
        goto BundleLoadModuleEnd;
    }

    Status = 0;

BundleLoadModuleEnd:
    if (Status != 0) {
        fprintf(stderr, "Failed to load bundle: %s\n", strerror(errno));
    }

    *BundleSize = Size;
    *Bundle = Current;
    return Status;
}

PSTR
CkpBundleThawElement (
    PSTR *Contents,
    PUINTN Size,
    PUINTN NameSize
    )

/*++

Routine Description:

    This routine reads a dictionary entry.

Arguments:

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    NameSize - Supplies a pointer where the size of the element name/key will
        be returned.

Return Value:

    Returns the address of the element key, if found.

    NULL on failure.

--*/

{

    PSTR Current;
    PSTR End;
    PSTR Name;

    Current = *Contents;
    End = Current + *Size;

    //
    // Skip any space.
    //

    while ((Current < End) &&
           ((*Current == ' ') ||
            (*Current == '\t') ||
            (*Current == '\r') ||
            (*Current == '\n'))) {

        Current += 1;
    }

    Name = Current;

    //
    // Find a colon.
    //

    while ((Current < End) && (*Current != ':')) {
        Current += 1;
    }

    //
    // If there was no colon, just return the advance past the spaces. If the
    // name is a closing brace, exit.
    //

    if ((Current == End) || ((Name < End) && (*Name == '}'))) {
        *Size = End - Name;
        *Contents = Name;
        return NULL;
    }

    *NameSize = Current - Name;

    //
    // Get past the colon and any additional space.
    //

    Current += 1;
    while ((Current < End) &&
           ((*Current == ' ') ||
            (*Current == '\t') ||
            (*Current == '\r') ||
            (*Current == '\n'))) {

        Current += 1;
    }

    *Size = End - Current;
    *Contents = Current;
    return Name;
}

PSTR
CkpBundleThawString (
    PCK_VM Vm,
    PSTR *Contents,
    PUINTN Size,
    PUINTN StringSize
    )

/*++

Routine Description:

    This routine reads a string.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    StringSize - Supplies a pointer where the size of the string in bytes
        will be returned, not including a null terminator (which won't be
        there).

Return Value:

    Returns a pointer to the string on success.

    NULL on failure.

--*/

{

    PSTR AfterScan;
    PSTR String;

    if ((*Size < 2) || (**Contents != 's')) {
        return NULL;
    }

    *StringSize = strtoull(*Contents + 1, &AfterScan, 10);
    if ((AfterScan == NULL) || (AfterScan == *Contents) ||
        (AfterScan >= *Contents + *Size) ||
        (AfterScan + 2 + *StringSize > *Contents + *Size)) {

        return NULL;
    }

    AfterScan += 1;
    String = AfterScan;
    AfterScan += *StringSize;
    if (*AfterScan != '"') {
        return NULL;
    }

    //
    // Terminate the string inline.
    //

    *AfterScan = '\0';
    AfterScan += 1;
    *Size -= AfterScan - *Contents;
    *Contents = AfterScan;
    return String;
}

BOOL
CkpBundleThawInteger (
    PSTR *Contents,
    PUINTN Size,
    PCK_INTEGER Integer
    )

/*++

Routine Description:

    This routine reads an integer.

Arguments:

    Contents - Supplies a pointer that on input points to the element to read.
        This is updated on output.

    Size - Supplies a pointer to the remaining size, not including a null
        terminator which may not exist.

    Integer - Supplies a pointer to the integer to read.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    PSTR AfterScan;

    if ((*Size < 2) || (**Contents != 'i')) {
        return FALSE;
    }

    AfterScan = NULL;
    *Integer = strtoll(*Contents + 1, &AfterScan, 10);
    if ((AfterScan == NULL) || (AfterScan == *Contents) ||
        (AfterScan >= *Contents + *Size)) {

        return FALSE;
    }

    *Size = (*Contents + *Size) - AfterScan;
    *Contents = AfterScan;
    return TRUE;
}

PSTR
CkpBundleGetTemporaryDirectory (
    VOID
    )

/*++

Routine Description:

    This routine creates a temporary directory.

Arguments:

    None.

Return Value:

    Returns a pointer to the temporary directory path on success.

    NULL on failure.

--*/

{

    INT Digit;
    INT Index;
    INT Length;
    PSTR Temp;
    INT Try;

    if (*CkBundleDirectory != '\0') {
        return CkBundleDirectory;
    }

    Temp = getenv("TMPDIR");
    if (Temp == NULL) {
        Temp = getenv("TEMP");
        if (Temp == NULL) {
            Temp = getenv("TMP");
        }
    }

    if (Temp == NULL) {
        Temp = "/tmp";
    }

    Length = snprintf(CkBundleDirectory,
                      sizeof(CkBundleDirectory),
                      "%s/ck",
                      Temp);

    if ((Length < 0) || (Length + 9 > sizeof(CkBundleDirectory))) {
        return NULL;
    }

    srand(time(NULL) ^ getpid());
    for (Try = 0; Try < 100; Try += 1) {
        for (Index = 0; Index < 8; Index += 1) {
            Digit = rand() % 36;
            if (Digit < 10) {
                Digit += '0';

            } else {
                Digit = Digit - 10 + 'A';
            }

            CkBundleDirectory[Length + Index] = Digit;
        }

        CkBundleDirectory[Length + Index] = '\0';
        if (mkdir(CkBundleDirectory, 0755) == 0) {
            return CkBundleDirectory;
        }

        if (errno != EEXIST) {
            return NULL;
        }
    }

    return NULL;
}

