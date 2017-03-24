/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    elfcomm.c

Abstract:

    This module implements ELF support functions that are agnostic to the
    address size (the same in 32-bit or 64-bit).

Author:

    Evan Green 8-Apr-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// Note that elfn.h is not included here because all functions in this file
// should be agnostic to 32 vs 64 bit. Needing to include that headers means
// the function belongs in a different file.
//

#include "imp.h"
#include "elf.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _ELF_LIBRARY_PATH_VARIABLE {
    ElfLibraryPathOrigin,
    ElfLibraryPathLib,
    ElfLibraryPathPlatform
} ELF_LIBRARY_PATH_VARIABLE, *PELF_LIBRARY_PATH_VARIABLE;

/*++

Structure Description:

    This structure stores an entry in the table of variables that can be
    substituted in ELF library paths.

Members:

    Variable - Stores the variable code.

    Name - Stores the variable name.

--*/

typedef struct _ELF_LIBRARY_PATH_VARIABLE_ENTRY {
    ELF_LIBRARY_PATH_VARIABLE Variable;
    PSTR Name;
} ELF_LIBRARY_PATH_VARIABLE_ENTRY, *PELF_LIBRARY_PATH_VARIABLE_ENTRY;

//
// ----------------------------------------------- Internal Function Prototypes
//

KSTATUS
ImpElfPerformLibraryPathSubstitutions (
    PLOADED_IMAGE Image,
    PSTR *Path,
    PUINTN PathCapacity
    );

//
// -------------------------------------------------------------------- Globals
//

ELF_LIBRARY_PATH_VARIABLE_ENTRY ElfLibraryPathVariables[] = {
    {ElfLibraryPathOrigin, "ORIGIN"},
    {ElfLibraryPathLib, "LIB"},
    {ElfLibraryPathPlatform, "PLATFORM"}
};

//
// ------------------------------------------------------------------ Functions
//

KSTATUS
ImpElfOpenWithPathList (
    PLOADED_IMAGE PathImage,
    PCSTR LibraryName,
    PSTR PathList,
    PIMAGE_FILE_INFORMATION File,
    PSTR *Path
    )

/*++

Routine Description:

    This routine attempts to load a needed library for an ELF image.

Arguments:

    PathImage - Supplies a pointer to the image that owns the path list. This
        will be the image that needs the library or an ancestor of the image
        that needs the library.

    LibraryName - Supplies the name of the library to load.

    PathList - Supplies a colon-separated list of paths to try.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

    Path - Supplies a pointer where the real path to the opened file will be
        returned. The caller is responsible for freeing this memory.

Return Value:

    Status code.

--*/

{

    PSTR CompletePath;
    UINTN CompletePathCapacity;
    UINTN CompletePathSize;
    PSTR CurrentPath;
    UINTN LibraryLength;
    PSTR NextSeparator;
    UINTN PrefixLength;
    KSTATUS Status;

    LibraryLength = RtlStringLength(LibraryName);
    CompletePath = NULL;
    CompletePathCapacity = 0;
    CurrentPath = PathList;
    while (TRUE) {
        NextSeparator = RtlStringFindCharacter(CurrentPath, ':', -1);
        if (NextSeparator == NULL) {
            PrefixLength = RtlStringLength(CurrentPath);

        } else {
            PrefixLength = (UINTN)NextSeparator - (UINTN)CurrentPath;
        }

        //
        // The complete path is "prefix/library". Reallocate the buffer if
        // needed.
        //

        CompletePathSize = PrefixLength + LibraryLength + 2;
        if (CompletePathSize > CompletePathCapacity) {
            if (CompletePath != NULL) {
                ImFreeMemory(CompletePath);
            }

            CompletePath = ImAllocateMemory(CompletePathSize,
                                            IM_ALLOCATION_TAG);

            if (CompletePath == NULL) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            CompletePathCapacity = CompletePathSize;
        }

        if (PrefixLength != 0) {
            RtlCopyMemory(CompletePath, CurrentPath, PrefixLength);
            if (CompletePath[PrefixLength - 1] != '/') {
                CompletePath[PrefixLength] = '/';
                PrefixLength += 1;
            }
        }

        RtlCopyMemory(CompletePath + PrefixLength,
                      LibraryName,
                      LibraryLength);

        CompletePath[PrefixLength + LibraryLength] = '\0';
        Status = ImpElfPerformLibraryPathSubstitutions(PathImage,
                                                       &CompletePath,
                                                       &CompletePathCapacity);

        if (!KSUCCESS(Status)) {
            break;
        }

        Status = ImOpenFile(PathImage->SystemContext, CompletePath, File);
        if (KSUCCESS(Status)) {
            break;
        }

        if (NextSeparator == NULL) {
            break;
        }

        CurrentPath = NextSeparator + 1;
    }

    //
    // If the file could be opened, get its real path.
    //

    if (KSUCCESS(Status)) {
        if (Path != NULL) {
            *Path = CompletePath;
            CompletePath = NULL;
        }
    }

    if (CompletePath != NULL) {
        ImFreeMemory(CompletePath);
    }

    return Status;
}

ULONG
ImpElfOriginalHash (
    PCSTR SymbolName
    )

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

{

    ULONG Hash;
    ULONG Temporary;

    Hash = 0;
    while (*SymbolName != '\0') {
        Hash = (Hash << 4) + *SymbolName;
        Temporary = Hash & 0xF0000000;
        if (Temporary != 0) {
            Hash ^= Temporary >> 24;
        }

        Hash &= ~Temporary;
        SymbolName += 1;
    }

    return Hash;
}

ULONG
ImpElfGnuHash (
    PCSTR SymbolName
    )

/*++

Routine Description:

    This routine hashes a symbol name to get the index into the ELF hash table
    using the GNU style hash function.

Arguments:

    SymbolName - Supplies a pointer to the name to hash.

Return Value:

    Returns the hash of the name.

--*/

{

    ULONG Hash;

    Hash = 5381;
    while (*SymbolName != '\0') {

        //
        // It's really hash * 33 + Character, but multiply by 33 is expanded
        // into multiply by 32 plus one.
        //

        Hash = ((Hash << 5) + Hash) + (UCHAR)*SymbolName;
        SymbolName += 1;
    }

    return Hash;
}

PSTR
ImpElfGetEnvironmentVariable (
    PSTR Variable
    )

/*++

Routine Description:

    This routine gets an environment variable value for the image library.

Arguments:

    Variable - Supplies a pointer to a null terminated string containing the
        name of the variable to get.

Return Value:

    Returns a pointer to the value of the environment variable. The image
    library will not free or modify this value.

    NULL if the given environment variable is not set.

--*/

{

    if (ImGetEnvironmentVariable != NULL) {
        return ImGetEnvironmentVariable(Variable);
    }

    return NULL;
}

//
// --------------------------------------------------------- Internal Functions
//

KSTATUS
ImpElfPerformLibraryPathSubstitutions (
    PLOADED_IMAGE PathImage,
    PSTR *Path,
    PUINTN PathCapacity
    )

/*++

Routine Description:

    This routine performs any variable substitutions in a library path.

Arguments:

    PathImage - Supplies a pointer to the image that owns the library path (not
        the library itself obviously, that hasn't been loaded yet).

    Path - Supplies a pointer that on input contains the complete path. On
        output this will contain the complete path with variables expanded.

    PathCapacity - Supplies a pointer that on input contains the size of the
        path allocation. This will be updated on output if the string is
        reallocated.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_INSUFFICIENT_RESOURCES if memory could not be allocated.

--*/

{

    PSTR CurrentCharacter;
    PSTR CurrentPath;
    UINTN Delta;
    PELF_LIBRARY_PATH_VARIABLE_ENTRY Entry;
    UINTN EntryCount;
    UINTN EntryIndex;
    UINTN Index;
    PSTR Name;
    UINTN NameLength;
    PSTR NewBuffer;
    UINTN RemainderOffset;
    PSTR Replacement;
    UINTN ReplacementLength;
    PSTR Separator;
    KSTATUS Status;
    UINTN VariableLength;
    UINTN VariableOffset;
    PSTR VariableStart;

    EntryCount = sizeof(ElfLibraryPathVariables) /
                 sizeof(ElfLibraryPathVariables[0]);

    CurrentCharacter = RtlStringFindCharacter(*Path, '$', -1);
    while (CurrentCharacter != NULL) {

        //
        // Find the name of the variable and the size of the region to replace.
        //

        VariableStart = CurrentCharacter;
        CurrentCharacter += 1;
        if (*CurrentCharacter == '{') {
            CurrentCharacter += 1;
            Name = CurrentCharacter;
            while ((*CurrentCharacter != '\0') && (*CurrentCharacter != '}')) {
                CurrentCharacter += 1;
            }

            if (*CurrentCharacter != '}') {
                RtlDebugPrint("ELF: Missing closing brace on %s.\n", *Path);
                Status = STATUS_INVALID_SEQUENCE;
                goto ElfPerformLibraryPathSubstitutionsEnd;
            }

            NameLength = (UINTN)CurrentCharacter - (UINTN)Name;
            CurrentCharacter += 1;

        } else {
            Name = CurrentCharacter;
            while (RtlIsCharacterAlphabetic(*CurrentCharacter) != FALSE) {
                CurrentCharacter += 1;
            }

            NameLength = (UINTN)CurrentCharacter - (UINTN)Name;
        }

        VariableLength = (UINTN)CurrentCharacter - (UINTN)VariableStart;
        VariableOffset = (UINTN)VariableStart - (UINTN)(*Path);

        //
        // Decode the variable.
        //

        for (EntryIndex = 0; EntryIndex < EntryCount; EntryIndex += 1) {
            Entry = &(ElfLibraryPathVariables[EntryIndex]);
            if ((RtlAreStringsEqual(Name, Entry->Name, NameLength) != FALSE) &&
                (Entry->Name[NameLength] == '\0')) {

                break;
            }
        }

        if (EntryIndex == EntryCount) {
            RtlDebugPrint("ELF: Warning: Unknown variable starting at %s.\n",
                          Name);

        } else {
            ReplacementLength = 0;
            switch (Entry->Variable) {
            case ElfLibraryPathOrigin:
                Separator = RtlStringFindCharacterRight(PathImage->FileName,
                                                        '/',
                                                        -1);

                if (Separator != NULL) {
                    Replacement = PathImage->FileName;
                    ReplacementLength = (UINTN)Separator - (UINTN)Replacement;

                } else {
                    Replacement = ".";
                }

                break;

            case ElfLibraryPathLib:
                if (PathImage->Format == ImageElf64) {
                    Replacement = "lib64";

                } else {
                    Replacement = "lib";
                }

                break;

            case ElfLibraryPathPlatform:
                switch (PathImage->Machine) {
                case ImageMachineTypeX86:
                    Replacement = "i686";
                    break;

                case ImageMachineTypeX64:
                    Replacement = "x86_64";
                    break;

                case ImageMachineTypeArm32:
                    Replacement = "armv7";
                    break;

                case ImageMachineTypeArm64:
                    Replacement = "armv8";
                    break;

                default:

                    ASSERT(FALSE);

                    Replacement = ".";
                    break;
                }

                break;

            default:

                ASSERT(FALSE);

                Replacement = ".";
                break;
            }

            if (ReplacementLength == 0) {
                ReplacementLength = RtlStringLength(Replacement);
            }

            //
            // If the replacement is shorter than the original variable, then
            // just copy the replacement over and shift the rest to the left.
            //

            if (ReplacementLength <= VariableLength) {
                RtlCopyMemory(VariableStart,
                              Replacement,
                              ReplacementLength);

                Delta = VariableLength - ReplacementLength;
                if (Delta != 0) {
                    CurrentPath = *Path;
                    for (Index = VariableOffset + ReplacementLength;
                         Index < *PathCapacity - Delta;
                         Index += 1) {

                        CurrentPath[Index] = CurrentPath[Index + Delta];
                    }

                    CurrentCharacter -= Delta;
                }

            //
            // The replacement is bigger than the variable it's replacing.
            //

            } else {
                Delta = ReplacementLength - VariableLength;
                NewBuffer = ImAllocateMemory(*PathCapacity + Delta,
                                             IM_ALLOCATION_TAG);

                if (NewBuffer == NULL) {
                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    goto ElfPerformLibraryPathSubstitutionsEnd;
                }

                RtlCopyMemory(NewBuffer, *Path, VariableOffset);
                RtlCopyMemory(NewBuffer + VariableOffset,
                              Replacement,
                              ReplacementLength);

                RemainderOffset = VariableOffset + VariableLength;
                RtlCopyMemory(NewBuffer + VariableOffset + ReplacementLength,
                              *Path + RemainderOffset,
                              *PathCapacity - (RemainderOffset));

                CurrentCharacter = (PSTR)((UINTN)NewBuffer +
                                          VariableOffset +
                                          ReplacementLength);

                ImFreeMemory(*Path);
                *Path = NewBuffer;
                *PathCapacity += Delta;
            }
        }

        //
        // Find the next variable.
        //

        CurrentCharacter = RtlStringFindCharacter(CurrentCharacter, '$', -1);
    }

    Status = STATUS_SUCCESS;

ElfPerformLibraryPathSubstitutionsEnd:
    return Status;
}

