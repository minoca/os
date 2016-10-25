/*++

Copyright (c) 2014 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    basepe.c

Abstract:

    This module implements basic PE/COFF file loader support.

Author:

    Evan Green 13-Mar-2014

Environment:

    Firmware

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "ueficore.h"
#include "imagep.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID *
EfipPeLoaderGetAddress (
    PEFI_PE_LOADER_CONTEXT Context,
    UINTN Address,
    UINTN TeStrippedOffset
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

EFIAPI
RETURN_STATUS
EfiPeLoaderLoadImage (
    PEFI_PE_LOADER_CONTEXT Context
    )

/*++

Routine Description:

    This routine loads a PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context. Before calling this
        function the caller must have allocated the load buffer and filled in
        the image address and size fields.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_INVALID_PARAMETER if the image address is invalid.

    RETURN_LOAD_ERROR if the image is a PE/COFF runtime image with no
    relocations.

    RETURN_BUFFER_TOO_SMALL if the caller provided buffer was not large enough.

--*/

{

    CHAR8 *Base;
    EFI_PE_LOADER_CONTEXT CheckContext;
    EFI_IMAGE_DATA_DIRECTORY *Directories;
    EFI_IMAGE_DATA_DIRECTORY *DirectoryEntry;
    CHAR8 *End;
    UINTN EntryPoint;
    EFI_IMAGE_SECTION_HEADER *FirstSection;
    UINTN FirstSectionOffset;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header;
    UINTN Index;
    UINT16 Magic;
    UINT32 NumberOfRvaAndSizes;
    UINT32 Offset;
    EFI_IMAGE_RESOURCE_DATA_ENTRY *ResourceDataEntry;
    EFI_IMAGE_RESOURCE_DIRECTORY *ResourceDirectory;
    EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *ResourceDirectoryEntry;
    EFI_IMAGE_RESOURCE_DIRECTORY_STRING *ResourceDirectoryString;
    EFI_IMAGE_SECTION_HEADER *Section;
    UINTN SectionCount;
    UINTN Size;
    RETURN_STATUS Status;
    CHAR16 *String;
    UINT32 TeStrippedOffset;

    ASSERT(Context != NULL);

    Context->ImageError = IMAGE_ERROR_SUCCESS;
    NumberOfRvaAndSizes = 0;
    Directories = NULL;

    //
    // Copy the provided context information into the local version.
    //

    EfiCoreCopyMemory(&CheckContext, Context, sizeof(EFI_PE_LOADER_CONTEXT));
    Status = EfiPeLoaderGetImageInfo(&CheckContext);
    if (RETURN_ERROR(Status)) {
        return Status;
    }

    //
    // Make sure there is enough allocated space for the image being loaded.
    //

    if (Context->ImageSize < CheckContext.ImageSize) {
        Context->ImageError = IMAGE_ERROR_INVALID_IMAGE_SIZE;
        return RETURN_BUFFER_TOO_SMALL;
    }

    if (Context->ImageAddress == 0) {
        Context->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
        return RETURN_INVALID_PARAMETER;
    }

    //
    // If there are no relocations, it had better be loaded at its linked
    // address and not be a runtime driver.
    //

    if (CheckContext.RelocationsStripped != FALSE) {
        if (CheckContext.ImageType == EFI_IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER) {
            Context->ImageError = IMAGE_ERROR_INVALID_SUBSYSTEM;
            return RETURN_LOAD_ERROR;
        }

        if (CheckContext.ImageAddress != Context->ImageAddress) {
            Context->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
            return RETURN_INVALID_PARAMETER;
        }
    }

    //
    // Make sure the allocated space has the proper alignment.
    //

    if (Context->IsTeImage == FALSE) {
        if (Context->ImageAddress !=
            ALIGN_VALUE(Context->ImageAddress, CheckContext.SectionAlignment)) {

            Context->ImageError = IMAGE_ERROR_INVALID_SECTION_ALIGNMENT;
            return RETURN_INVALID_PARAMETER;
        }
    }

    //
    // Read the entire PE or TE header into memory.
    //

    Status = Context->ImageRead(Context->Handle,
                                0,
                                &(Context->SizeOfHeaders),
                                (VOID *)(UINTN)(Context->ImageAddress));

    if (Context->IsTeImage == FALSE) {
        Header.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)((UINTN)Context->ImageAddress +
                                                 Context->PeCoffHeaderOffset);

        FirstSectionOffset = Context->PeCoffHeaderOffset + sizeof(UINT32) +
                             sizeof(EFI_IMAGE_FILE_HEADER) +
                             Header.Pe32->FileHeader.SizeOfOptionalHeader;

        FirstSection =
                    (EFI_IMAGE_SECTION_HEADER *)((UINTN)Context->ImageAddress +
                                                 FirstSectionOffset);

        SectionCount = (UINTN)(Header.Pe32->FileHeader.NumberOfSections);
        TeStrippedOffset = 0;

    } else {
        Header.Te = (EFI_TE_IMAGE_HEADER *)(UINTN)(Context->ImageAddress);
        FirstSection =
                    (EFI_IMAGE_SECTION_HEADER *)((UINTN)Context->ImageAddress +
                                                 sizeof(EFI_TE_IMAGE_HEADER));

        SectionCount = (UINTN)(Header.Te->NumberOfSections);
        TeStrippedOffset = (UINT32)(Header.Te->StrippedSize) -
                           sizeof(EFI_TE_IMAGE_HEADER);
    }

    if (RETURN_ERROR(Status)) {
        Context->ImageError = IMAGE_ERROR_IMAGE_READ;
        return RETURN_LOAD_ERROR;
    }

    //
    // Load each section of the image.
    //

    Section = FirstSection;
    for (Index = 0; Index < SectionCount; Index += 1) {
        Size = (UINTN)(Section->Misc.VirtualSize);
        if ((Size == 0) || (Size > Section->SizeOfRawData)) {
            Size = (UINTN)Section->SizeOfRawData;
        }

        //
        // Compute the section addresses.
        //

        Base = EfipPeLoaderGetAddress(Context,
                                      Section->VirtualAddress,
                                      TeStrippedOffset);

        End = EfipPeLoaderGetAddress(
                       Context,
                       Section->VirtualAddress + Section->Misc.VirtualSize - 1,
                       TeStrippedOffset);

        if ((Size > 0) && ((Base == NULL) || (End == NULL))) {
            Context->ImageError = IMAGE_ERROR_SECTION_NOT_LOADED;
            return RETURN_LOAD_ERROR;
        }

        if (Section->SizeOfRawData > 0) {
            Status = Context->ImageRead(
                                  Context->Handle,
                                  Section->PointerToRawData - TeStrippedOffset,
                                  &Size,
                                  Base);

            if (RETURN_ERROR(Status)) {
                Context->ImageError = IMAGE_ERROR_IMAGE_READ;
                return RETURN_LOAD_ERROR;
            }
        }

        //
        // If the raw size is less than the virtual size, zero fill the
        // remainder.
        //

        if (Size < Section->Misc.VirtualSize) {
            EfiSetMem(Base + Size, Section->Misc.VirtualSize - Size, 0);
        }

        Section += 1;
    }

    //
    // Get the image entry point.
    //

    Magic = EfiPeLoaderGetPeHeaderMagicValue(Header);
    if (Context->IsTeImage == FALSE) {
        if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            EntryPoint =
                      (UINTN)(Header.Pe32->OptionalHeader.AddressOfEntryPoint);

        } else {
            EntryPoint =
                  (UINTN)(Header.Pe32Plus->OptionalHeader.AddressOfEntryPoint);
        }

    } else {
        EntryPoint = (UINTN)(Header.Te->AddressOfEntryPoint);
    }

    Context->EntryPoint = (PHYSICAL_ADDRESS)(UINTN)EfipPeLoaderGetAddress(
                                                             Context,
                                                             EntryPoint,
                                                             TeStrippedOffset);

    //
    // Determine the size of the fixup data.
    //

    if (Context->IsTeImage == FALSE) {
        if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            NumberOfRvaAndSizes =
                               Header.Pe32->OptionalHeader.NumberOfRvaAndSizes;

            Directories = &(Header.Pe32->OptionalHeader.DataDirectory[0]);
            DirectoryEntry =
                           &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);

        } else {
            NumberOfRvaAndSizes =
                           Header.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;

            Directories = &(Header.Pe32Plus->OptionalHeader.DataDirectory[0]);
            DirectoryEntry =
                           &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);
        }

        Context->FixupDataSize = 0;
        if (NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC) {
            Context->FixupDataSize =
                         DirectoryEntry->Size / sizeof(UINT16) * sizeof(UINTN);
        }

    } else {
        DirectoryEntry = &(Header.Te->DataDirectory[0]);
        Context->FixupDataSize =
                         DirectoryEntry->Size / sizeof(UINT16) * sizeof(UINTN);
    }

    //
    // The consumer must allocate a buffer for the relocation fixup log.
    // This is used by the runtime relocation code.
    //

    Context->FixupData = NULL;

    //
    // Get the image's HII resource section.
    //

    Context->HiiResourceData = 0;
    if (Context->IsTeImage == FALSE) {
        DirectoryEntry = &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE]);
        if ((NumberOfRvaAndSizes > EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE) &&
            (DirectoryEntry->Size != 0)) {

            ASSERT(Directories != NULL);

            Base = EfipPeLoaderGetAddress(Context,
                                          DirectoryEntry->VirtualAddress,
                                          0);

            if (Base != NULL) {
                ResourceDirectory = (EFI_IMAGE_RESOURCE_DIRECTORY *)Base;
                Offset = sizeof(EFI_IMAGE_RESOURCE_DIRECTORY) +
                         (sizeof(EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) *
                          (ResourceDirectory->NumberOfNamedEntries +
                           ResourceDirectory->NumberOfIdEntries));

                if (Offset > DirectoryEntry->Size) {
                    Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                    return RETURN_UNSUPPORTED;
                }

                //
                // Loop through every directory entry.
                //

                ResourceDirectoryEntry =
                     (EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY *)(ResourceDirectory +
                                                            1);

                for (Index = 0;
                     Index < ResourceDirectory->NumberOfNamedEntries;
                     Index += 1) {

                    //
                    // Skip entries whose names are not strings.
                    //

                    if (ResourceDirectoryEntry->u1.s.NameIsString == 0) {
                        ResourceDirectoryEntry += 1;
                        continue;
                    }

                    if (ResourceDirectoryEntry->u1.s.NameOffset >=
                        DirectoryEntry->Size) {

                        Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                        return RETURN_UNSUPPORTED;
                    }

                    //
                    // Skip entries not named "HII".
                    //

                    Offset = ResourceDirectoryEntry->u1.s.NameOffset;
                    ResourceDirectoryString =
                        (EFI_IMAGE_RESOURCE_DIRECTORY_STRING *)(Base + Offset);

                    String = &(ResourceDirectoryString->String[0]);
                    if ((ResourceDirectoryString->Length != 3) ||
                        (String[0] != L'H') ||
                        (String[1] != L'I') ||
                        (String[2] != L'I')) {

                        ResourceDirectoryEntry += 1;
                        continue;
                    }

                    //
                    // A HII resource was found.
                    //

                    if (ResourceDirectoryEntry->u2.s.DataIsDirectory != 0) {

                        //
                        // Move to the next level - Resource Name.
                        //

                        if (ResourceDirectoryEntry->u2.s.OffsetToDirectory >=
                            DirectoryEntry->Size) {

                            Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                            return RETURN_UNSUPPORTED;
                        }

                        Offset = ResourceDirectoryEntry->u2.s.OffsetToDirectory;
                        ResourceDirectory =
                               (EFI_IMAGE_RESOURCE_DIRECTORY *)(Base + Offset);

                        Offset += sizeof(EFI_IMAGE_RESOURCE_DIRECTORY) +
                                  (sizeof(EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) *
                                   (ResourceDirectory->NumberOfNamedEntries +
                                    ResourceDirectory->NumberOfIdEntries));

                        if (Offset > DirectoryEntry->Size) {
                            Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                            return RETURN_UNSUPPORTED;
                        }

                        ResourceDirectoryEntry =
                                               (VOID *)(ResourceDirectory + 1);

                        if (ResourceDirectoryEntry->u2.s.DataIsDirectory != 0) {

                            //
                            // Move to the next level - Resource Language.
                            //

                            Offset =
                                ResourceDirectoryEntry->u2.s.OffsetToDirectory;

                            if (Offset >= DirectoryEntry->Size) {
                                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                                return RETURN_UNSUPPORTED;
                            }

                            ResourceDirectory = (VOID *)(Base + Offset);
                            Offset +=
                                  sizeof(EFI_IMAGE_RESOURCE_DIRECTORY) +
                                  (sizeof(EFI_IMAGE_RESOURCE_DIRECTORY_ENTRY) *
                                   (ResourceDirectory->NumberOfNamedEntries +
                                    ResourceDirectory->NumberOfIdEntries));

                            if (Offset > DirectoryEntry->Size) {
                                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                                return RETURN_UNSUPPORTED;
                            }

                            ResourceDirectoryEntry =
                                               (VOID *)(ResourceDirectory + 1);
                        }
                    }

                    //
                    // Now it ought to be resource data.
                    //

                    if (ResourceDirectoryEntry->u2.s.DataIsDirectory != 0) {
                        if (ResourceDirectoryEntry->u2.OffsetToData >=
                            DirectoryEntry->Size) {

                            Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                            return RETURN_UNSUPPORTED;
                        }

                        Offset = ResourceDirectoryEntry->u2.OffsetToData;
                        ResourceDataEntry =
                              (EFI_IMAGE_RESOURCE_DATA_ENTRY *)(Base + Offset);

                        Offset = ResourceDataEntry->OffsetToData;
                        Context->HiiResourceData =
                               (PHYSICAL_ADDRESS)(UINTN)EfipPeLoaderGetAddress(
                                                                       Context,
                                                                       Offset,
                                                                       0);

                        break;
                    }

                    ResourceDirectoryEntry += 1;
                }
            }
        }
    }

    return Status;
}

EFIAPI
RETURN_STATUS
EfiPeLoaderRelocateImage (
    PEFI_PE_LOADER_CONTEXT Context
    )

/*++

Routine Description:

    This routine relocates a loaded PE image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_LOAD_ERROR if the image is not valid.

    RETURN_UNSUPPORTED if an unsupported relocation type was encountered.

--*/

{

    UINT64 Adjust;
    PHYSICAL_ADDRESS BaseAddress;
    EFI_IMAGE_DATA_DIRECTORY *Directories;
    UINTN EndOffset;
    CHAR8 *Fixup;
    UINT16 *Fixup16;
    UINT32 *Fixup32;
    UINT64 *Fixup64;
    CHAR8 *FixupBase;
    CHAR8 *FixupData;
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header;
    UINT16 Magic;
    UINT32 NumberOfRvaAndSizes;
    UINT16 *Relocation;
    EFI_IMAGE_BASE_RELOCATION *RelocationBase;
    EFI_IMAGE_BASE_RELOCATION *RelocationBaseEnd;
    EFI_IMAGE_DATA_DIRECTORY *RelocationDirectory;
    UINT16 *RelocationEnd;
    UINT32 TeStrippedOffset;

    ASSERT(Context != NULL);

    Context->ImageError = IMAGE_ERROR_SUCCESS;
    if (Context->RelocationsStripped != FALSE) {
        return RETURN_SUCCESS;
    }

    //
    // If the destination address is not zero, use that rather than the image
    // address.
    //

    BaseAddress = Context->ImageAddress;
    if (Context->DestinationAddress != 0) {
        BaseAddress = Context->DestinationAddress;
    }

    if (Context->IsTeImage == FALSE) {
        Header.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)((UINTN)Context->ImageAddress +
                                                 Context->PeCoffHeaderOffset);

        TeStrippedOffset = 0;
        Magic = EfiPeLoaderGetPeHeaderMagicValue(Header);
        if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            Adjust = (UINT64)BaseAddress -
                     Header.Pe32->OptionalHeader.ImageBase;

            if (Adjust != 0) {
                Header.Pe32->OptionalHeader.ImageBase = (UINT32)BaseAddress;
            }

            NumberOfRvaAndSizes =
                               Header.Pe32->OptionalHeader.NumberOfRvaAndSizes;

            Directories = &(Header.Pe32->OptionalHeader.DataDirectory[0]);
            RelocationDirectory =
                           &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);

        } else {
            Adjust = (UINT64)BaseAddress -
                     Header.Pe32Plus->OptionalHeader.ImageBase;

            if (Adjust != 0) {
                Header.Pe32Plus->OptionalHeader.ImageBase = (UINT64)BaseAddress;
            }

            NumberOfRvaAndSizes =
                           Header.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes;

            Directories = &(Header.Pe32Plus->OptionalHeader.DataDirectory[0]);
            RelocationDirectory =
                           &(Directories[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC]);
        }

        if (NumberOfRvaAndSizes < EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC) {
            RelocationDirectory = NULL;
        }

    } else {
        Header.Te = (EFI_TE_IMAGE_HEADER *)(UINTN)(Context->ImageAddress);
        TeStrippedOffset = (UINT32)(Header.Te->StrippedSize) -
                           sizeof(EFI_TE_IMAGE_HEADER);

        Adjust = (UINT64)(BaseAddress -
                          (Header.Te->ImageBase + TeStrippedOffset));

        if (Adjust != 0) {
            Header.Te->ImageBase = (UINT64)(BaseAddress - TeStrippedOffset);
        }

        RelocationDirectory = &(Header.Te->DataDirectory[0]);
    }

    if ((RelocationDirectory != NULL) && (RelocationDirectory->Size != 0)) {
        RelocationBase = (EFI_IMAGE_BASE_RELOCATION *)EfipPeLoaderGetAddress(
                                           Context,
                                           RelocationDirectory->VirtualAddress,
                                           TeStrippedOffset);

        EndOffset = RelocationDirectory->VirtualAddress +
                    RelocationDirectory->Size - 1;

        RelocationBaseEnd = (EFI_IMAGE_BASE_RELOCATION *)EfipPeLoaderGetAddress(
                                                             Context,
                                                             EndOffset,
                                                             TeStrippedOffset);

        if ((RelocationBase == NULL) || (RelocationBaseEnd == NULL)) {
            Context->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
            return RETURN_LOAD_ERROR;
        }

    } else {
        RelocationBase = NULL;
        RelocationBaseEnd = NULL;
    }

    //
    // If there are adjustments to be made, relocate the image.
    //

    if (Adjust != 0) {
        FixupData = Context->FixupData;

        //
        // Loop across every relocation page.
        //

        while (RelocationBase < RelocationBaseEnd) {
            Relocation = (UINT16 *)((CHAR8 *)RelocationBase +
                                    sizeof(EFI_IMAGE_BASE_RELOCATION));

            if ((RelocationBase->SizeOfBlock == 0) ||
                (RelocationBase->SizeOfBlock > RelocationDirectory->Size)) {

                Context->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
                return RETURN_LOAD_ERROR;
            }

            RelocationEnd = (UINT16 *)((CHAR8 *)RelocationBase +
                                       RelocationBase->SizeOfBlock);

            FixupBase = EfipPeLoaderGetAddress(Context,
                                               RelocationBase->VirtualAddress,
                                               TeStrippedOffset);

            if (FixupBase == NULL) {
                Context->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
                return RETURN_LOAD_ERROR;
            }

            //
            // Run every relocation in the page.
            //

            while (Relocation < RelocationEnd) {
                Fixup = FixupBase + (*Relocation & 0xFFF);
                switch ((*Relocation) >> 12) {
                case EFI_IMAGE_REL_BASED_ABSOLUTE:
                    break;

                case EFI_IMAGE_REL_BASED_HIGH:
                    Fixup16 = (UINT16 *)Fixup;
                    *Fixup16 =
                           (UINT16)(*Fixup + ((UINT16)((UINT32)Adjust >> 16)));

                    if (FixupData != NULL) {
                        *(UINT16 *)FixupData = *Fixup16;
                        FixupData = FixupData + sizeof(UINT16);
                    }

                    break;

                case EFI_IMAGE_REL_BASED_LOW:
                    Fixup16 = (UINT16 *)Fixup;
                    *Fixup16 = (UINT16)(*Fixup + (UINT16)Adjust);
                    if (FixupData != NULL) {
                        *(UINT16 *)FixupData = *Fixup16;
                        FixupData = FixupData + sizeof(UINT16);
                    }

                    break;

                case EFI_IMAGE_REL_BASED_HIGHLOW:
                    Fixup32 = (UINT32 *)Fixup;
                    *Fixup32 = *Fixup32 + (UINT32)Adjust;
                    if (FixupData != NULL) {
                        FixupData = ALIGN_POINTER(FixupData, sizeof(UINT32));
                        *(UINT32 *)FixupData = *Fixup32;
                        FixupData = FixupData + sizeof(UINT32);
                    }

                    break;

                case EFI_IMAGE_REL_BASED_DIR64:
                    Fixup64 = (UINT64 *)Fixup;
                    *Fixup64 = *Fixup64 + (UINT64)Adjust;
                    if (FixupData != NULL) {
                        FixupData = ALIGN_POINTER(FixupData, sizeof(UINT64));
                        *(UINT64 *)FixupData = *Fixup64;
                        FixupData = FixupData + sizeof(UINT64);
                    }

                    break;

                default:
                    RtlDebugPrint("Error: Unknown relocation type.\n");
                    Context->ImageError = IMAGE_ERROR_FAILED_RELOCATION;
                    return RETURN_LOAD_ERROR;
                }

                //
                // Move to the next relocation record.
                //

                Relocation += 1;
            }

            //
            // Move to the next relocation page.
            //

            RelocationBase = (EFI_IMAGE_BASE_RELOCATION *)RelocationEnd;
        }

        //
        // Adjust the entry point.
        //

        if (Context->DestinationAddress != 0) {
            Context->EntryPoint -= (UINT64)Context->ImageAddress;
            Context->EntryPoint += (UINT64)Context->DestinationAddress;
        }
    }

    return RETURN_SUCCESS;
}

EFIAPI
RETURN_STATUS
EfiPeLoaderGetImageInfo (
    PEFI_PE_LOADER_CONTEXT Context
    )

/*++

Routine Description:

    This routine extracts information about the given PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_SUCCESS on success.

    RETURN_INVALID_PARAMETER if the image context is NULL.

    RETURN_UNSUPPORTED if the image format is not supported.

--*/

{

    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header;
    EFI_IMAGE_OPTIONAL_HEADER_UNION HeaderData;
    UINT16 Magic;
    RETURN_STATUS Status;
    UINT32 TeStrippedOffset;

    if (Context == NULL) {
        return EFI_INVALID_PARAMETER;
    }

    Context->ImageError = IMAGE_ERROR_SUCCESS;
    Header.Union = &HeaderData;
    Status = EfiPeLoaderGetPeHeader(Context, Header);
    if (RETURN_ERROR(Status)) {
        return Status;
    }

    Magic = EfiPeLoaderGetPeHeaderMagicValue(Header);

    //
    // Get the base address of the image.
    //

    if (Context->IsTeImage == FALSE) {
        TeStrippedOffset = 0;
        if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            Context->ImageAddress = Header.Pe32->OptionalHeader.ImageBase;

        } else {
            Context->ImageAddress = Header.Pe32Plus->OptionalHeader.ImageBase;
        }

    } else {
        TeStrippedOffset = (UINT32)(Header.Te->StrippedSize -
                                    sizeof(EFI_TE_IMAGE_HEADER));

        Context->ImageAddress = (EFI_PHYSICAL_ADDRESS)(Header.Te->ImageBase +
                                                       TeStrippedOffset);
    }

    Context->DestinationAddress = 0;
    Context->DebugDirectoryEntryRva = 0;
    Context->CodeView = NULL;
    Context->PdbPointer = NULL;

    //
    // Look at the file header to determine if relocations have been stripped.
    //

    if ((Context->IsTeImage == FALSE) &&
        ((Header.Pe32->FileHeader.Characteristics &
          EFI_IMAGE_FILE_RELOCS_STRIPPED) != 0)) {

        Context->RelocationsStripped = TRUE;

    } else if ((Context->IsTeImage != FALSE) &&
               (Header.Te->DataDirectory[0].Size == 0) &&
               (Header.Te->DataDirectory[0].VirtualAddress == 0)) {

        Context->RelocationsStripped = TRUE;

    } else {
        Context->RelocationsStripped = FALSE;
    }

    return EFI_SUCCESS;
}

EFIAPI
RETURN_STATUS
EfiPeLoaderUnloadImage (
    PEFI_PE_LOADER_CONTEXT Context
    )

/*++

Routine Description:

    This routine unloads the PE/COFF image.

Arguments:

    Context - Supplies a pointer to the image context.

Return Value:

    RETURN_* status code.

--*/

{

    return RETURN_SUCCESS;
}

UINT16
EfiPeLoaderGetPeHeaderMagicValue (
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    )

/*++

Routine Description:

    This routine returns the magic value out of the PE/COFF header.

Arguments:

    Header - Supplies a pointer to the header.

Return Value:

    Returns the magic value from the header.

--*/

{

    return Header.Pe32->OptionalHeader.Magic;
}

RETURN_STATUS
EfiPeLoaderGetPeHeader (
    PEFI_PE_LOADER_CONTEXT Context,
    EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Header
    )

/*++

Routine Description:

    This routine retrieves the PE or TE header from a PE/COFF or TE image.

Arguments:

    Context - Supplies a pointer to the loader context.

    Header - Supplies a pointer to the header.

Return Value:

    RETURN_* error code.

--*/

{

    CHAR8 BufferData;
    EFI_IMAGE_DOS_HEADER DosHeader;
    UINT32 HeaderWithoutDataDirectory;
    UINT32 Index;
    UINTN LastByteOffset;
    UINT16 Magic;
    UINTN ReadSize;
    UINTN SectionCount;
    EFI_IMAGE_SECTION_HEADER SectionHeader;
    UINT32 SectionHeaderOffset;
    UINTN Size;
    RETURN_STATUS Status;
    UINT32 TeStrippedOffset;

    //
    // Read the DOS image header to check for its existence.
    //

    ASSERT(Context->ImageRead != NULL);

    Size = sizeof(EFI_IMAGE_DOS_HEADER);
    ReadSize = Size;
    Status = Context->ImageRead(Context->Handle,
                                0,
                                &Size,
                                &DosHeader);

    if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
        Context->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
            Status = RETURN_UNSUPPORTED;
        }

        return Status;
    }

    //
    // Assume the PE header is at the beginning of the image. If the DOS
    // header is valid, then the PE header comes at some point after the DOS
    // header.
    //

    Context->PeCoffHeaderOffset = 0;
    if (DosHeader.e_magic == EFI_IMAGE_DOS_SIGNATURE) {
        Context->PeCoffHeaderOffset = DosHeader.e_lfanew;
    }

    //
    // Read the PE/COFF header. This may read too much, but that's alright.
    //

    Size = sizeof(EFI_IMAGE_OPTIONAL_HEADER_UNION);
    ReadSize = Size;
    Status = Context->ImageRead(Context->Handle,
                                Context->PeCoffHeaderOffset,
                                &Size,
                                Header.Pe32);

    if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
        Context->ImageError = IMAGE_ERROR_IMAGE_READ;
        if (Size != ReadSize) {
            Status = RETURN_UNSUPPORTED;
        }

        return Status;
    }

    //
    // Use the signature to figure out the image offset. Start with TE images.
    //

    if (Header.Te->Signature == EFI_TE_IMAGE_HEADER_SIGNATURE) {
        Context->IsTeImage = TRUE;
        Context->Machine = Header.Te->Machine;
        Context->ImageType = (UINT16)(Header.Te->Subsystem);
        Context->ImageSize = 0;
        Context->SectionAlignment = 0;
        Context->SizeOfHeaders = sizeof(EFI_TE_IMAGE_HEADER) +
                                 (UINTN)(Header.Te->BaseOfCode) -
                                 (UINTN)(Header.Te->StrippedSize);

        if ((sizeof(EFI_TE_IMAGE_HEADER) >= (UINT32)Header.Te->StrippedSize) ||
            (Header.Te->BaseOfCode <= Header.Te->StrippedSize)) {

            Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
            return RETURN_UNSUPPORTED;
        }

        //
        // Read the last byte of the header from the file.
        //

        Size = 1;
        ReadSize = Size;
        Status = Context->ImageRead(Context->Handle,
                                    Context->SizeOfHeaders - 1,
                                    &Size,
                                    &BufferData);

        if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
            Context->ImageError = IMAGE_ERROR_IMAGE_READ;
            if (Size != ReadSize) {
                Status = RETURN_UNSUPPORTED;
            }

            return Status;
        }

        //
        // If the TE image data directory entry size is non-zero, but the data
        // directory VA is zero, then that's not valid.
        //

        if (((Header.Te->DataDirectory[0].Size != 0) &&
             (Header.Te->DataDirectory[0].VirtualAddress == 0)) ||
            ((Header.Te->DataDirectory[1].Size != 0) &&
             (Header.Te->DataDirectory[1].VirtualAddress == 0))) {

            Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
            return RETURN_UNSUPPORTED;
        }

    //
    // It's not a TE image, handle it being a PE image.
    //

    } else if (Header.Pe32->Signature == EFI_IMAGE_NT_SIGNATURE) {
        Context->IsTeImage = FALSE;
        Context->Machine = Header.Pe32->FileHeader.Machine;
        Magic = EfiPeLoaderGetPeHeaderMagicValue(Header);

        //
        // Handle a 32-bit image.
        //

        if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR32_MAGIC) {

            //
            // Check the number of RVA and sizes.
            //

            if (Header.Pe32->OptionalHeader.NumberOfRvaAndSizes >
                EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the size of the optional header.
            //

            HeaderWithoutDataDirectory =
                                       sizeof(EFI_IMAGE_OPTIONAL_HEADER32) -
                                       (sizeof(EFI_IMAGE_DATA_DIRECTORY) *
                                        EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES);

            if ((UINT32)(Header.Pe32->FileHeader.SizeOfOptionalHeader) -
                HeaderWithoutDataDirectory !=
                Header.Pe32->OptionalHeader.NumberOfRvaAndSizes *
                sizeof(EFI_IMAGE_DATA_DIRECTORY)) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the number of sections.
            //

            SectionHeaderOffset = Context->PeCoffHeaderOffset +
                                  sizeof(UINT32) +
                                  sizeof(EFI_IMAGE_FILE_HEADER) +
                                  Header.Pe32->FileHeader.SizeOfOptionalHeader;

            if (Header.Pe32->OptionalHeader.SizeOfImage < SectionHeaderOffset) {
                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            if ((Header.Pe32->OptionalHeader.SizeOfImage -
                 SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <=
                Header.Pe32->FileHeader.NumberOfSections) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the size of headers field.
            //

            if ((Header.Pe32->OptionalHeader.SizeOfHeaders <
                 SectionHeaderOffset) ||
                (Header.Pe32->OptionalHeader.SizeOfHeaders >=
                 Header.Pe32->OptionalHeader.SizeOfImage) ||
                ((Header.Pe32->OptionalHeader.SizeOfHeaders -
                  SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <
                 (UINT32)(Header.Pe32->FileHeader.NumberOfSections))) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Read the last byte of the headers from the file.
            //

            Size = 1;
            ReadSize = Size;
            Status = Context->ImageRead(
                                 Context->Handle,
                                 Header.Pe32->OptionalHeader.SizeOfHeaders - 1,
                                 &Size,
                                 &BufferData);

            if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
                Context->ImageError = IMAGE_ERROR_IMAGE_READ;
                if (Size != ReadSize) {
                    Status = RETURN_UNSUPPORTED;
                }

                return Status;
            }

            Context->ImageType = Header.Pe32->OptionalHeader.Subsystem;
            Context->ImageSize =
                             (UINT64)(Header.Pe32->OptionalHeader.SizeOfImage);

            Context->SectionAlignment =
                                  Header.Pe32->OptionalHeader.SectionAlignment;

            Context->SizeOfHeaders = Header.Pe32->OptionalHeader.SizeOfHeaders;

        //
        // Handle a 64-bit image.
        //

        } else if (Magic == EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC) {

            //
            // Check the number of RVA and sizes.
            //

            if (Header.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes >
                EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the size of the optional header.
            //

            HeaderWithoutDataDirectory =
                                       sizeof(EFI_IMAGE_OPTIONAL_HEADER64) -
                                       (sizeof(EFI_IMAGE_DATA_DIRECTORY) *
                                        EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES);

            if ((UINT32)(Header.Pe32Plus->FileHeader.SizeOfOptionalHeader) -
                HeaderWithoutDataDirectory !=
                Header.Pe32Plus->OptionalHeader.NumberOfRvaAndSizes *
                sizeof(EFI_IMAGE_DATA_DIRECTORY)) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the number of sections.
            //

            SectionHeaderOffset =
                              Context->PeCoffHeaderOffset +
                              sizeof(UINT32) +
                              sizeof(EFI_IMAGE_FILE_HEADER) +
                              Header.Pe32Plus->FileHeader.SizeOfOptionalHeader;

            if (Header.Pe32Plus->OptionalHeader.SizeOfImage <
                SectionHeaderOffset) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            if ((Header.Pe32Plus->OptionalHeader.SizeOfImage -
                 SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <=
                Header.Pe32Plus->FileHeader.NumberOfSections) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Check the size of headers field.
            //

            if ((Header.Pe32Plus->OptionalHeader.SizeOfHeaders <
                 SectionHeaderOffset) ||
                (Header.Pe32Plus->OptionalHeader.SizeOfHeaders >=
                 Header.Pe32Plus->OptionalHeader.SizeOfImage) ||
                ((Header.Pe32Plus->OptionalHeader.SizeOfHeaders -
                  SectionHeaderOffset) / EFI_IMAGE_SIZEOF_SECTION_HEADER <
                 (UINT32)(Header.Pe32Plus->FileHeader.NumberOfSections))) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Read the last byte of the headers from the file.
            //

            Size = 1;
            ReadSize = Size;
            Status = Context->ImageRead(
                             Context->Handle,
                             Header.Pe32Plus->OptionalHeader.SizeOfHeaders - 1,
                             &Size,
                             &BufferData);

            if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
                Context->ImageError = IMAGE_ERROR_IMAGE_READ;
                if (Size != ReadSize) {
                    Status = RETURN_UNSUPPORTED;
                }

                return Status;
            }

            Context->ImageType = Header.Pe32Plus->OptionalHeader.Subsystem;
            Context->ImageSize =
                         (UINT64)(Header.Pe32Plus->OptionalHeader.SizeOfImage);

            Context->SectionAlignment =
                              Header.Pe32Plus->OptionalHeader.SectionAlignment;

            Context->SizeOfHeaders =
                                 Header.Pe32Plus->OptionalHeader.SizeOfHeaders;

        //
        // The magic isn't known.
        //

        } else {
            Context->ImageError = IMAGE_ERROR_INVALID_MACHINE_TYPE;
            return RETURN_UNSUPPORTED;
        }

    //
    // This header signature is not recognized.
    //

    } else {
        Context->ImageError = IMAGE_ERROR_INVALID_MACHINE_TYPE;
        return RETURN_UNSUPPORTED;
    }

    if (!EFI_IMAGE_MACHINE_TYPE_SUPPORTED(Context->Machine)) {
        return RETURN_UNSUPPORTED;
    }

    //
    // Check each section field.
    //

    if (Context->IsTeImage != FALSE) {
        SectionHeaderOffset = sizeof(EFI_TE_IMAGE_HEADER);
        SectionCount = (UINTN)(Header.Te->NumberOfSections);

    } else {
        SectionHeaderOffset = Context->PeCoffHeaderOffset + sizeof(UINT32) +
                              sizeof(EFI_IMAGE_FILE_HEADER) +
                              Header.Pe32->FileHeader.SizeOfOptionalHeader;

        SectionCount = (UINTN)(Header.Pe32->FileHeader.NumberOfSections);
    }

    for (Index = 0; Index < SectionCount; Index += 1) {

        //
        // Read the section header from the file.
        //

        Size = sizeof(EFI_IMAGE_SECTION_HEADER);
        ReadSize = Size;
        Status = Context->ImageRead(Context->Handle,
                                    SectionHeaderOffset,
                                    &Size,
                                    &SectionHeader);

        if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
            Context->ImageError = IMAGE_ERROR_IMAGE_READ;
            if (Size != ReadSize) {
                Status = RETURN_UNSUPPORTED;
            }

            return Status;
        }

        //
        // Adjust the offsets in the section header for TE images.
        //

        if (Context->IsTeImage != FALSE) {
            TeStrippedOffset = (UINT32)Header.Te->StrippedSize -
                               sizeof(EFI_TE_IMAGE_HEADER);

            SectionHeader.VirtualAddress -= TeStrippedOffset;
            SectionHeader.PointerToRawData -= TeStrippedOffset;
        }

        if (SectionHeader.SizeOfRawData != 0) {

            //
            // Section data should be beyond the PE header, and shouldn't
            // overflow.
            //

            if ((SectionHeader.VirtualAddress < Context->SizeOfHeaders) ||
                (SectionHeader.PointerToRawData < Context->SizeOfHeaders) ||
                ((UINT32)(~0) - SectionHeader.PointerToRawData <
                 SectionHeader.SizeOfRawData)) {

                Context->ImageError = IMAGE_ERROR_UNSUPPORTED;
                return RETURN_UNSUPPORTED;
            }

            //
            // Read the last byte of the section data.
            //

            Size = 1;
            ReadSize = Size;
            LastByteOffset = SectionHeader.PointerToRawData +
                             SectionHeader.SizeOfRawData - 1;

            Status = Context->ImageRead(Context->Handle,
                                        LastByteOffset,
                                        &Size,
                                        &BufferData);

            if ((RETURN_ERROR(Status)) || (Size != ReadSize)) {
                Context->ImageError = IMAGE_ERROR_IMAGE_READ;
                if (Size != ReadSize) {
                    Status = RETURN_UNSUPPORTED;
                }

                return Status;
            }
        }

        //
        // Move to the next section.
        //

        SectionHeaderOffset += sizeof(EFI_IMAGE_SECTION_HEADER);
    }

    return RETURN_SUCCESS;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID *
EfipPeLoaderGetAddress (
    PEFI_PE_LOADER_CONTEXT Context,
    UINTN Address,
    UINTN TeStrippedOffset
    )

/*++

Routine Description:

    This routine converts an image address into a loaded in-memory address.

Arguments:

    Context - Supplies a pointer to the loader context.

    Address - Supplies the address to translate.

    TeStrippedOffset - Supplies the stripped offset for TE images.

Return Value:

    Returns the in memory address.

    NULL on failure.

--*/

{

    if (Address >= Context->ImageSize + TeStrippedOffset) {
        Context->ImageError = IMAGE_ERROR_INVALID_IMAGE_ADDRESS;
        return NULL;
    }

    return (CHAR8 *)((UINTN)Context->ImageAddress + Address - TeStrippedOffset);
}

