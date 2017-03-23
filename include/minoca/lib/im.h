/*++

Copyright (c) 2012 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    im.h

Abstract:

    This header contains definitions for manipulating binary images.

Author:

    Evan Green 13-Oct-2012

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the allocation tag used by the image library: Imag.
//

#define IM_ALLOCATION_TAG 0x67616D49

#define IMAGE_DEBUG_VERSION 1

//
// Define image load flags.
//

//
// Set this flag to indicate that this is the interpreter, or that generally
// any interpreter directives specified in the program header should be
// ignored.
//

#define IMAGE_LOAD_FLAG_IGNORE_INTERPRETER 0x00000001

//
// Set this flag to indicate that this is the primary executable being loaded.
//

#define IMAGE_LOAD_FLAG_PRIMARY_EXECUTABLE 0x00000002

//
// This flag is set on all images that were loaded as a result of loading the
// primary executable. It is also set on the primary executable itself.
//

#define IMAGE_LOAD_FLAG_PRIMARY_LOAD 0x00000004

//
// Set this flag to indicate the loaded image structure is just a placeholder
// to keep track of image accounting, but doesn't actually contain the guts of
// a loaded image.
//

#define IMAGE_LOAD_FLAG_PLACEHOLDER 0x00000008

//
// Set this flag to skip finding static constructor and destructor functions.
//

#define IMAGE_LOAD_FLAG_NO_STATIC_CONSTRUCTORS 0x00000010

//
// Set this flag to skip processing relocations.
//

#define IMAGE_LOAD_FLAG_NO_RELOCATIONS 0x00000020

//
// Set this flag to only load the images, but not process their dynamic
// sections at all.
//

#define IMAGE_LOAD_FLAG_LOAD_ONLY 0x00000040

//
// Set this flag to bind all symbols at load time, rather than lazily bind them
//

#define IMAGE_LOAD_FLAG_BIND_NOW 0x00000080

//
// Set this flag to place the image in the global scope.
//

#define IMAGE_LOAD_FLAG_GLOBAL 0x00000100

//
// Set this flag if loading a dynamic library. This casues the load to search
// through the primary executable's dynamic library paths.
//

#define IMAGE_LOAD_FLAG_DYNAMIC_LIBRARY 0x00000200

//
// Define flags passed into the map image section routine.
//

#define IMAGE_MAP_FLAG_WRITE 0x00000001
#define IMAGE_MAP_FLAG_EXECUTE 0x00000002
#define IMAGE_MAP_FLAG_FIXED 0x00000004

//
// Define the name of the dynamic library path variable.
//

#define IMAGE_LOAD_LIBRARY_PATH_VARIABLE "LD_LIBRARY_PATH"

//
// Define image flags.
//

#define IMAGE_FLAG_IMPORTS_LOADED     0x00000001
#define IMAGE_FLAG_RELOCATED          0x00000002
#define IMAGE_FLAG_INITIALIZED        0x00000004
#define IMAGE_FLAG_RELOCATABLE        0x00000008
#define IMAGE_FLAG_STATIC_TLS         0x00000010
#define IMAGE_FLAG_GNU_HASH           0x00000020
#define IMAGE_FLAG_TEXT_RELOCATIONS   0x00000040

//
// ------------------------------------------------------ Data Type Definitions
//

typedef enum _IMAGE_FORMAT {
    ImageInvalidFormat,
    ImageUnknownFormat,
    ImagePe32,
    ImageElf32,
    ImageElf64,
    MaxImageFormats
} IMAGE_FORMAT, *PIMAGE_FORMAT;

typedef enum _IMAGE_MACHINE_TYPE {
    ImageMachineTypeInvalid,
    ImageMachineTypeUnknown,
    ImageMachineTypeX86,
    ImageMachineTypeArm32,
    ImageMachineTypeX64,
    ImageMachineTypeArm64,
} IMAGE_MACHINE_TYPE, *PIMAGE_MACHINE_TYPE;

typedef enum _IMAGE_SEGMENT_TYPE {
    ImageSegmentInvalid = 0,
    ImageSegmentFileSection,
    ImageSegmentZeroedMemory
} IMAGE_SEGMENT_TYPE, *PIMAGE_SEGMENT_TYPE;

typedef enum _IMAGE_LOAD_STATE {
    ImageLoadConsistent,
    ImageLoadAdd,
    ImageLoadDelete,
} IMAGE_LOAD_STATE, *PIMAGE_LOAD_STATE;

typedef struct _LOADED_IMAGE LOADED_IMAGE, *PLOADED_IMAGE;

typedef
VOID
(*PIMAGE_STATIC_FUNCTION) (
    VOID
    );

/*++

Routine Description:

    This routine defines the prototype for image static constructors and
    destructors such as _init, _fini, and those in .preinit_array, .init_array,
    and .fini_array.

Arguments:

    None.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores information about an executable image.

Members:

    Format - Stores the basic file format of the executable image.

    Machine - Stores the machine type this image was built for.

    ImageBase - Stores the default image base of the image.

    EntryPoint - Stores the default (unrelocated) entry point of the image.

--*/

typedef struct _IMAGE_INFORMATION {
    IMAGE_FORMAT Format;
    IMAGE_MACHINE_TYPE Machine;
    ULONGLONG ImageBase;
    ULONGLONG EntryPoint;
} IMAGE_INFORMATION, *PIMAGE_INFORMATION;

/*++

Structure Description:

    This structure stores information about a file for the image library.

Members:

    Handle - Stores an open handle to the file.

    Size - Stores the size of the file in bytes.

    ModificationDate - Stores the modification date of the file in seconds
        since 2001.

    DeviceId - Stores the device identifier this file resides on.

    FileId - Stores the file identifier this device resides on.

--*/

typedef struct _IMAGE_FILE_INFORMATION {
    HANDLE Handle;
    ULONGLONG Size;
    ULONGLONG ModificationDate;
    ULONGLONG DeviceId;
    ULONGLONG FileId;
} IMAGE_FILE_INFORMATION, *PIMAGE_FILE_INFORMATION;

/*++

Structure Description:

    This structure stores information about a loaded image buffer that the
    image loader can access portions of the image at.

Members:

    Context - Stores a context pointer to additional information stored by the
        functions supporting this image library.

    Data - Stores a pointer to the file data.

    Size - Stores the data size in bytes.

--*/

typedef struct _IMAGE_BUFFER {
    PVOID Context;
    PVOID Data;
    UINTN Size;
} IMAGE_BUFFER, *PIMAGE_BUFFER;

/*++

Structure Description:

    This structure stores information about a segment or region of an
    executable image loaded into memory.

Members:

    Type - Stores the type of segment this structure represents.

    VirtualAddress - Stores the virtual address of the image segment.

    Size - Stores the size, in bytes, of the image segment.

    FileSize - Stores the size, in bytes, of the segment mapped to the file.

    MemorySize - Stores the size, in bytes, of the segment in memory. This must
        be at least as big as the file size, and bytes after the file size
        will be initialized to 0.

    Flags - Stores the bitfield of attributes about the mapping. See
        IMAGE_MAP_FLAG_* definitions.

    MappingStart - Stores an optional pointer not used by the image library
        indicating the location where the memory mapping of the segment began.

--*/

typedef struct _IMAGE_SEGMENT {
    IMAGE_SEGMENT_TYPE Type;
    PVOID VirtualAddress;
    UINTN FileSize;
    UINTN MemorySize;
    ULONG Flags;
    PVOID MappingStart;
} IMAGE_SEGMENT, *PIMAGE_SEGMENT;

/*++

Structure Description:

    This structure stores information about static constructors and destructors
    in the image. All pointers are final virtual addresses. The order these
    are called in is .preinit_array, _init, .init_array, .fini_array (in
    reverse order), and _fini.

Members:

    PreinitArray - Stores an optional pointer to the array of pre-init
        functions in a dynamic library.

    PreinitArraySize - Stores the size of the preinit array in bytes.

    InitArray - Stores an optional pointer to the array of static constructor
        functions in a dynamic library.

    InitArraySize - Stores the size of the init array in bytes.

    FiniArray - Stores an optional pointer to the array of static destructor
        functions in a dynamic library.

    FiniArraySize - Stores the size of the fini array in bytes.

    InitFunction - Stores an optional pointer to the _init function in a
        dynamic library.

    FiniFunction - Stores an optional pointer to the _fini function in a
        dynamic library.

--*/

typedef struct _IMAGE_STATIC_FUNCTIONS {
    PIMAGE_STATIC_FUNCTION *PreinitArray;
    UINTN PreinitArraySize;
    PIMAGE_STATIC_FUNCTION *InitArray;
    UINTN InitArraySize;
    PIMAGE_STATIC_FUNCTION *FiniArray;
    UINTN FiniArraySize;
    PIMAGE_STATIC_FUNCTION InitFunction;
    PIMAGE_STATIC_FUNCTION FiniFunction;
} IMAGE_STATIC_FUNCTIONS, *PIMAGE_STATIC_FUNCTIONS;

/*++

Structure Description:

    This structure stores information used when debugging dynamic images. This
    structure lines up with the r_debug structure in the C library.

Members:

    Version - Stores the debug structure version information. This is set to
        IMAGE_DEBUG_VERSION.

    Image - Stores a pointer to the image structure itself.

    ImageChangeFunction - Stores a pointer to a function that is called when
        an image is loaded or unloaded. A breakpoint can be set on this
        function. The image load state will inform the debugger the state of
        the current image.

    ImageLoadState - Stores the loading state of the image. This is of type
        IMAGE_LOAD_STATE.

    DynamicLinkerBase - Stores the base address of the dynamic linker.

--*/

typedef struct _IMAGE_DEBUG {
    ULONG Version;
    PLOADED_IMAGE Image;
    PVOID ImageChangeFunction;
    ULONG ImageLoadState;
    PVOID DynamicLinkerBase;
} IMAGE_DEBUG, *PIMAGE_DEBUG;

/*++

Structure Description:

    This structure stores information about a loaded executable image. Be
    careful moving members, as the first few members in this structure line up
    with the C library link_map structure.

Members:

    ListEntry - Stores pointers to the next and previous images. This is not
        used by the Image Library, and can be used by the subsystem managing
        the image library. This member lines up with the l_next and l_prev
        members of the link_map structure.

    FileName - Stores the complete path to the file. This member lines up with
        the l_name member of the link_map structure.

    BaseDifference - Stores the difference between the image's loaded lowest
        address and its preferred lowest address. That is, the loaded lowest
        address minus the preferred lowest address. This member lines up with
        the l_addr member of the link_map structure.

    DynamicSection - Stores a pointer to the dynamic section. This member
        lines up with the l_ld member of the link_map structure.

    LibraryName - Stores the name of the library, according to itself.

    Parent - Stores an optional pointer to the image that caused this image
        to need to be loaded.

    ModuleNumber - Stores the module identifier. This is not used by the image
        library, but can be assigned by the consumer of the image library.

    TlsOffset - Stores the offset from the thread pointer to the start of the
        static TLS block for this module. This only applies to modules using
        the static TLS regime. This will be initialized to -1 if the module
        has no TLS offset or is loaded dynamically.

    Format - Stores an integer indicating the binary image format.

    Machine - Stores the machine type for the image.

    File - Stores information about the file itself, including potentially an
        open handle to it during the load process.

    Size - Stores the size of the image as expanded in memory, in bytes.

    PreferredLowestAddress - Stores the image's default lowest virtual address.

    ImageContext - Stores a pointer to context specific to the image backend.

    LoadedImageBuffer - Stores a pointer to the image's in-memory layout. In
        a live system, this is probably the same as the actual loaded VA of the
        image. In offline situations, this may be a different buffer.
        Relocations and other modifications to the image are made through this
        pointer.

    SystemContext - Stores a pointer of context that gets passed to system
        backend functions.

    AllocatorHandle - Stores the handle associated with the overall allocation
        of virtual address space.

    SegmentCount - Stores the number of segments in the loaded image.

    Segments - Stores a pointer to the loaded image segments.

    EntryPoint - Stores the entry point of the image. This pointer is
        absolute (it has already been rebased).

    ReferenceCount - Stores the reference count on this image.

    ExportSymbolTable - Stores a pointer to the export symbol table.

    ExportStringTable - Stores a pointer to the export string table.

    ExportStringTableSize - Stores the size of the export string table in bytes.

    ExportHashTable - Stores a pointer to the export hash table, not used in
        all image formats.

    PltRelocations - Stores a pointer to the procedure linkage table relocation
        section.

    PltRelocationsAddends - Stores a boolean indicating if the PLT relocations
        are of type REL (FALSE) or RELA (TRUE).

    ImportDepth - Stores the import depth of the image (the number of images
        between the image and some image that was actually requested to be
        loaded). An image's imports, unless already loaded, have an import
        depth of one greater than the image itself.

    ImportCount - Stores the number of import images this image requires.

    Imports - Stores a pointer to an array of loaded images that this image
        imports from.

    TlsImage - Stores a pointer to the thread-local storage initialization
        data.

    TlsImageSize - Stores the size of the thread-local storage initialization
        data, in bytes.

    TlsSize - Stores the size of the thread-local storage region, in bytes.
        This may be bigger than the TLS image size if there is uninitialized
        data.

    TlsAlignment - Stores the alignment requirement of the TLS section.

    DebuggerModule - Stores an optional pointer to the debugger's module
        information if this module is loaded in the kernel debugger.

    SystemExtension - Stores a pointer to the additional information the
        system stores attached to this image.

    Flags - Stores internal image flags. See IMAGE_FLAG_* definitions.

    LoadFlags - Stores the flags passed in when the image load was requested.

    StaticFunctions - Stores an optional pointer to an array of static
        functions.

    Debug - Stores debug information.

    Scope - Stores an array of all images in the dependency tree rooted at
        this image. This is used for breadth first search of symbols.

    ScopeSize - Stores the number of elements in the scope array.

    ScopeCapacity - Stores the maximum number of elements that can be put in
        the scope tree before it will have to be reallocated.

--*/

struct _LOADED_IMAGE {
    LIST_ENTRY ListEntry;
    UINTN BaseDifference;
    PSTR FileName;
    PVOID DynamicSection;
    PSTR LibraryName;
    PLOADED_IMAGE Parent;
    UINTN ModuleNumber;
    UINTN TlsOffset;
    IMAGE_FORMAT Format;
    IMAGE_MACHINE_TYPE Machine;
    IMAGE_FILE_INFORMATION File;
    UINTN Size;
    PVOID PreferredLowestAddress;
    PVOID LoadedImageBuffer;
    PVOID ImageContext;
    PVOID SystemContext;
    HANDLE AllocatorHandle;
    ULONG SegmentCount;
    PIMAGE_SEGMENT Segments;
    PVOID EntryPoint;
    ULONG ReferenceCount;
    PVOID ExportSymbolTable;
    PVOID ExportStringTable;
    ULONG ExportStringTableSize;
    PVOID ExportHashTable;
    PVOID PltRelocations;
    BOOL PltRelocationsAddends;
    ULONG ImportDepth;
    ULONG ImportCount;
    PLOADED_IMAGE *Imports;
    PVOID TlsImage;
    UINTN TlsImageSize;
    UINTN TlsSize;
    UINTN TlsAlignment;
    PVOID DebuggerModule;
    PVOID SystemExtension;
    ULONG Flags;
    ULONG LoadFlags;
    PIMAGE_STATIC_FUNCTIONS StaticFunctions;
    IMAGE_DEBUG Debug;
    PLOADED_IMAGE *Scope;
    UINTN ScopeSize;
    UINTN ScopeCapacity;
};

/*++

Structure Description:

    This structure stores information about a symbol.

Members:

    Image - Stores a pointer to the loaded image that contains the symbol.

    Name - Stores the name of the symbol.

    Address - Stores the address of the symbol.

    TlsAddress - Stores a boolean indicating whether or not the symbol's
        address is TLS relative.

--*/

typedef struct _IMAGE_SYMBOL {
    PLOADED_IMAGE Image;
    PSTR Name;
    PVOID Address;
    BOOL TlsAddress;
} IMAGE_SYMBOL, *PIMAGE_SYMBOL;

//
// Outside support routines needed by the image library.
//

typedef
PVOID
(*PIM_ALLOCATE_MEMORY) (
    ULONG Size,
    ULONG Tag
    );

/*++

Routine Description:

    This routine allocates memory for the image library.

Arguments:

    Size - Supplies the number of bytes required for the memory allocation.

    Tag - Supplies a 32-bit ASCII identifier used to tag the memroy allocation.

Return Value:

    Returns a pointer to the memory allocation on success.

    NULL on failure.

--*/

typedef
VOID
(*PIM_FREE_MEMORY) (
    PVOID Allocation
    );

/*++

Routine Description:

    This routine frees memory allocated by the image library.

Arguments:

    Allocation - Supplies a pointer the allocation to free.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PIM_OPEN_FILE) (
    PVOID SystemContext,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION File
    );

/*++

Routine Description:

    This routine opens a file.

Arguments:

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    BinaryName - Supplies the name of the executable image to open.

    File - Supplies a pointer where the information for the file including its
        open handle will be returned.

Return Value:

    Status code.

--*/

typedef
VOID
(*PIM_CLOSE_FILE) (
    PIMAGE_FILE_INFORMATION File
    );

/*++

Routine Description:

    This routine closes an open file, invalidating any memory mappings to it.

Arguments:

    File - Supplies a pointer to the file information.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PIM_LOAD_FILE) (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine loads an entire file into memory so the image library can
    access it.

Arguments:

    File - Supplies a pointer to the file information.

    Buffer - Supplies a pointer where the buffer will be returned on success.

Return Value:

    Status code.

--*/

typedef
KSTATUS
(*PIM_READ_FILE) (
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG Offset,
    UINTN Size,
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine reads a portion of the given file into a buffer, allocated by
    this function.

Arguments:

    File - Supplies a pointer to the file information.

    Offset - Supplies the file offset to read from in bytes.

    Size - Supplies the size to read, in bytes.

    Buffer - Supplies a pointer where the buffer will be returned on success.

Return Value:

    Status code.

--*/

typedef
VOID
(*PIM_UNLOAD_BUFFER) (
    PIMAGE_FILE_INFORMATION File,
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine unloads a file buffer created from either the load file or
    read file function, and frees the buffer.

Arguments:

    File - Supplies a pointer to the file information.

    Buffer - Supplies the buffer returned by the load file function.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PIM_ALLOCATE_ADDRESS_SPACE) (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine allocates a section of virtual address space that an image
    can be mapped in to.

Arguments:

    Image - Supplies a pointer to the image being loaded. The system context,
        size, file information, load flags, and preferred virtual address will
        be initialized. This routine should set up the loaded image buffer,
        loaded lowest address, and allocator handle if needed.

Return Value:

    Status code.

--*/

typedef
VOID
(*PIM_FREE_ADDRESS_SPACE) (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine frees a section of virtual address space that was previously
    allocated.

Arguments:

    Image - Supplies a pointer to the loaded (or partially loaded) image.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PIM_MAP_IMAGE_SEGMENT) (
    HANDLE AddressSpaceHandle,
    PVOID AddressSpaceAllocation,
    PIMAGE_FILE_INFORMATION File,
    ULONGLONG FileOffset,
    PIMAGE_SEGMENT Segment,
    PIMAGE_SEGMENT PreviousSegment
    );

/*++

Routine Description:

    This routine maps a section of the image to the given virtual address.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    AddressSpaceAllocation - Supplies the original lowest virtual address for
        this image.

    File - Supplies an optional pointer to the file being mapped. If this
        parameter is NULL, then a zeroed memory section is being mapped.

    FileOffset - Supplies the offset from the beginning of the file to the
        beginning of the mapping, in bytes.

    Segment - Supplies a pointer to the segment information to map. On output,
        the virtual address will contain the actual mapped address, and the
        mapping handle may be set.

    PreviousSegment - Supplies an optional pointer to the previous segment
        that was mapped, so this routine can handle overlap appropriately. This
        routine can assume that segments are always mapped in increasing order.

Return Value:

    Status code.

--*/

typedef
VOID
(*PIM_UNMAP_IMAGE_SEGMENT) (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segment
    );

/*++

Routine Description:

    This routine maps unmaps an image segment.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    Segment - Supplies a pointer to the segment information to unmap.

Return Value:

    None.

--*/

typedef
KSTATUS
(*PIM_NOTIFY_IMAGE_LOAD) (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine notifies the primary consumer of the image library that an
    image has been loaded.

Arguments:

    Image - Supplies the image that has just been loaded. This image should
        be subsequently returned to the image library upon requests for loaded
        images with the given name.

Return Value:

    Status code. Failing status codes veto the image load.

--*/

typedef
VOID
(*PIM_NOTIFY_IMAGE_UNLOAD) (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine notifies the primary consumer of the image library that an
    image is about to be unloaded from memory. Once this routine returns, the
    image should not be referenced again as it will be freed.

Arguments:

    Image - Supplies the image that is about to be unloaded.

Return Value:

    None.

--*/

typedef
VOID
(*PIM_INVALIDATE_INSTRUCTION_CACHE_REGION) (
    PVOID Address,
    ULONG Size
    );

/*++

Routine Description:

    This routine invalidates an instruction cache region after code has been
    modified.

Arguments:

    Address - Supplies the virtual address of the revion to invalidate.

    Size - Supplies the number of bytes to invalidate.

Return Value:

    None.

--*/

typedef
PSTR
(*PIM_GET_ENVIRONMENT_VARIABLE) (
    PSTR Variable
    );

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

typedef
KSTATUS
(*PIM_FINALIZE_SEGMENTS) (
    HANDLE AddressSpaceHandle,
    PIMAGE_SEGMENT Segments,
    UINTN SegmentCount
    );

/*++

Routine Description:

    This routine applies the final memory protection attributes to the given
    segments. Read and execute bits can be applied at the time of mapping, but
    write protection may be applied here.

Arguments:

    AddressSpaceHandle - Supplies the handle used to claim the overall region
        of address space.

    Segments - Supplies the final array of segments.

    SegmentCount - Supplies the number of segments.

Return Value:

    Status code.

--*/

typedef
VOID
(*PIM_RESOLVE_PLT_ENTRY) (
    VOID
    );

/*++

Routine Description:

    This routine is called to lazily resolve a PLT entry that has been called.
    It is architecture specific and usually must be implemented in assembly.
    Although it is listed here as having no arguments, it usually takes at
    least two arguments: a pointer to the loaded image, and the byte offset
    into the PLT relocation section.

Arguments:

    None.

Return Value:

    None.

--*/

/*++

Structure Description:

    This structure stores pointers to all the functions the image library
    requires as imports.

Members:

    AllocateMemory - Stores a pointer to a function used by the image
        library to allocate memory.

    FreeMemory - Stores a pointer to a function used by the image library to
        free memory.

    OpenFile - Stores a pointer to a function used by the image library to
        open a handle to a file.

    CloseFile - Stores a pointer to a function used by the image library to
        close a handle to a file.

    LoadFile - Stores a pointer to a function used by the image library to
        load an entire file into memory.

    ReadFile - Stores a pointer to t a function used by the image library to
        load a portion of a file into memory.

    UnloadBuffer - Stores a pointer to a function used by the image library to
        unload a file buffer created from the load file or read file functions.

    AllocateAddressSpace - Stores a pointer to a function used by the image
        library to allocate a section of virtual address space.

    FreeAddressSpace - Stores a pointer to a function used by the image
        library to free a section of virtual address space.

    MapImageSegment - Store a pointer to a function used by the image
        library to map a segment of a file into virtual memory.

    UnmapImageSegment - Stores a pointer to a function used by the image
        library to unmap segments from virtual memory.

    NotifyImageLoad - Stores a pointer to a function used by the image library
        to notify consumers that an image has been loaded.

    NotifyImageUnload - Stores a pointer to a function used by the image library
        to notify consumers that an image is about to be unloaded.

    InvalidateInstructionCacheRegion - Stores a pointer to a function that is
        called after a code region is modified.

    GetEnvironmentVariable - Stores an optional pointer to a function used to
        query the environment.

    FinalizeSegments - Stores an optional pointer to a function used to set the
        final permissions on all segments.

    ResolvePltEntry - Stores an optional pointer to an assembly function used
        to resolve procedure linkage table entries on the fly.

--*/

typedef struct _IM_IMPORT_TABLE {
    PIM_ALLOCATE_MEMORY AllocateMemory;
    PIM_FREE_MEMORY FreeMemory;
    PIM_OPEN_FILE OpenFile;
    PIM_CLOSE_FILE CloseFile;
    PIM_LOAD_FILE LoadFile;
    PIM_READ_FILE ReadFile;
    PIM_UNLOAD_BUFFER UnloadBuffer;
    PIM_ALLOCATE_ADDRESS_SPACE AllocateAddressSpace;
    PIM_FREE_ADDRESS_SPACE FreeAddressSpace;
    PIM_MAP_IMAGE_SEGMENT MapImageSegment;
    PIM_UNMAP_IMAGE_SEGMENT UnmapImageSegment;
    PIM_NOTIFY_IMAGE_LOAD NotifyImageLoad;
    PIM_NOTIFY_IMAGE_UNLOAD NotifyImageUnload;
    PIM_INVALIDATE_INSTRUCTION_CACHE_REGION InvalidateInstructionCacheRegion;
    PIM_GET_ENVIRONMENT_VARIABLE GetEnvironmentVariable;
    PIM_FINALIZE_SEGMENTS FinalizeSegments;
    PIM_RESOLVE_PLT_ENTRY ResolvePltEntry;
} IM_IMPORT_TABLE, *PIM_IMPORT_TABLE;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the primary executable, the root of the global scope.
//

extern PLOADED_IMAGE ImPrimaryExecutable;

//
// -------------------------------------------------------- Function Prototypes
//

KSTATUS
ImInitialize (
    PIM_IMPORT_TABLE ImportTable
    );

/*++

Routine Description:

    This routine initializes the image library. It must be called before any
    other image library routines are called.

Arguments:

    ImportTable - Supplies a pointer to a table of functions that will be used
        by the image library to provide basic memory allocation and loading
        support. This memory must stick around, the given pointer is cached.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_TOO_LATE if the image library has already been initialized.

    STATUS_INVALID_PARAMETER if one of the required functions is not
        implemented.

--*/

KSTATUS
ImGetExecutableFormat (
    PSTR BinaryName,
    PVOID SystemContext,
    PIMAGE_FILE_INFORMATION ImageFile,
    PIMAGE_BUFFER ImageBuffer,
    PIMAGE_FORMAT Format
    );

/*++

Routine Description:

    This routine determines the executable format of a given image path.

Arguments:

    BinaryName - Supplies the name of the binary executable image to examine.

    SystemContext - Supplies the context pointer passed to the load executable
        function.

    ImageFile - Supplies an optional pointer where the file handle and other
        information will be returned on success.

    ImageBuffer - Supplies an optional pointer where the image buffer
        information will be returned.

    Format - Supplies a pointer where the format will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
ImLoad (
    PLIST_ENTRY ListHead,
    PCSTR BinaryName,
    PIMAGE_FILE_INFORMATION BinaryFile,
    PIMAGE_BUFFER ImageBuffer,
    PVOID SystemContext,
    ULONG Flags,
    PLOADED_IMAGE *LoadedImage,
    PLOADED_IMAGE *Interpreter
    );

/*++

Routine Description:

    This routine loads an executable image into memory.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images.

    BinaryName - Supplies the name of the binary executable image to load. If
        this is NULL, then a pointer to the first (primary) image loaded, with
        a reference added.

    BinaryFile - Supplies an optional handle to the file information. The
        handle should be positioned to the beginning of the file. Supply NULL
        if the caller does not already have an open handle to the binary. On
        success, the image library takes ownership of the handle.

    ImageBuffer - Supplies an optional pointer to the image buffer. This can
        be a complete image file buffer, or just a partial load of the file.

    SystemContext - Supplies an opaque token that will be passed to the
        support functions called by the image support library.

    Flags - Supplies a bitfield of flags governing the load. See
        IMAGE_LOAD_FLAG_* flags.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

    Interpreter - Supplies an optional pointer where a pointer to the loaded
        interpreter structure will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
ImAddImage (
    PIMAGE_BUFFER Buffer,
    PLOADED_IMAGE *LoadedImage
    );

/*++

Routine Description:

    This routine adds the accounting structures for an image that has already
    been loaded into memory.

Arguments:

    Buffer - Supplies the image buffer containing the loaded image.

    LoadedImage - Supplies an optional pointer where a pointer to the loaded
        image structure will be returned on success.

Return Value:

    Status code.

--*/

KSTATUS
ImLoadImports (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine loads all import libraries for a given image list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images to
        load import libraries for.

Return Value:

    Status code.

--*/

KSTATUS
ImRelocateImages (
    PLIST_ENTRY ListHead
    );

/*++

Routine Description:

    This routine relocates all images that have not yet been relocated on the
    given list.

Arguments:

    ListHead - Supplies a pointer to the head of the list of loaded images to
        apply relocations for.

Return Value:

    Status code.

--*/

VOID
ImImageAddReference (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine increments the reference count on an image.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

VOID
ImImageReleaseReference (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine releases a reference on a loaded executable image from memory.
    If this is the last reference, the image will be unloaded.

Arguments:

    Image - Supplies a pointer to the loaded image.

Return Value:

    None.

--*/

KSTATUS
ImGetImageInformation (
    PIMAGE_BUFFER Buffer,
    PIMAGE_INFORMATION Information
    );

/*++

Routine Description:

    This routine gets various pieces of information about an image. This is the
    generic form that can get information from any supported image type.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    Information - Supplies a pointer to the information structure that will be
        filled out by this function. It is assumed the memory pointed to here
        is valid.

Return Value:

    STATUS_SUCCESS on success.

    STATUS_UNKNOWN_IMAGE_FORMAT if the image is unknown or corrupt.

--*/

BOOL
ImGetImageSection (
    PIMAGE_BUFFER Buffer,
    PSTR SectionName,
    PVOID *Section,
    PULONGLONG VirtualAddress,
    PULONG SectionSizeInFile,
    PULONG SectionSizeInMemory
    );

/*++

Routine Description:

    This routine gets a pointer to the given section in a PE image given a
    memory mapped file.

Arguments:

    Buffer - Supplies a pointer to the image buffer.

    SectionName - Supplies the name of the desired section.

    Section - Supplies a pointer where the pointer to the section will be
        returned.

    VirtualAddress - Supplies a pointer where the virtual address of the section
        will be returned, if applicable.

    SectionSizeInFile - Supplies a pointer where the size of the section as it
        appears in the file will be returned.

    SectionSizeInMemory - Supplies a pointer where the size of the section as it
        appears after being loaded in memory will be returned.

Return Value:

    TRUE on success.

    FALSE otherwise.

--*/

IMAGE_FORMAT
ImGetImageFormat (
    PIMAGE_BUFFER Buffer
    );

/*++

Routine Description:

    This routine determines the file format for an image mapped in memory.

Arguments:

    Buffer - Supplies a pointer to the image buffer to determine the type of.

Return Value:

    Returns the file format of the image.

--*/

KSTATUS
ImGetSymbolByName (
    PLOADED_IMAGE Image,
    PSTR SymbolName,
    PLOADED_IMAGE Skip,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to find an exported symbol with the given name in the
    given binary. This routine also looks through the image imports if the
    recursive flag is specified.

Arguments:

    Image - Supplies a pointer to the image to query.

    SymbolName - Supplies a pointer to the string containing the name of the
        symbol to search for.

    Skip - Supplies an optional pointer to an image to skip when searching.

    Symbol - Supplies a pointer to a structure that receives the symbol's
        information on success.

Return Value:

    Status code.

--*/

PLOADED_IMAGE
ImGetImageByAddress (
    PLIST_ENTRY ListHead,
    PVOID Address
    );

/*++

Routine Description:

    This routine attempts to find the image that covers the given address.

Arguments:

    ListHead - Supplies the list of loaded images.

    Address - Supplies the address to search for.

Return Value:

    Returns a pointer to an image covering the given address on success.

    NULL if no loaded image covers the given address.

--*/

KSTATUS
ImGetSymbolByAddress (
    PLOADED_IMAGE Image,
    PVOID Address,
    PIMAGE_SYMBOL Symbol
    );

/*++

Routine Description:

    This routine attempts to resolve the given address into a symbol.

Arguments:

    Image - Supplies a pointer to the image to query.

    Address - Supplies the address to search for.

    Symbol - Supplies a pointer to a structure that receives the address's
        symbol information on success.

Return Value:

    Status code.

--*/

VOID
ImRelocateSelf (
    PVOID Base,
    PIM_RESOLVE_PLT_ENTRY PltResolver
    );

/*++

Routine Description:

    This routine relocates the currently running image.

Arguments:

    Base - Supplies a pointer to the base of the loaded image.

    PltResolver - Supplies a pointer to the function used to resolve PLT
        entries.

Return Value:

    None.

--*/

PVOID
ImResolvePltEntry (
    PLOADED_IMAGE Image,
    UINTN RelocationOffset
    );

/*++

Routine Description:

    This routine implements the slow path for a Procedure Linkable Table entry
    that has not yet been resolved to its target function address. This routine
    is only called once for each PLT entry, as subsequent calls jump directly
    to the destination function address. It resolves the appropriate GOT
    relocation and returns a pointer to the function to jump to.

Arguments:

    Image - Supplies a pointer to the loaded image whose PLT needs resolution.
        This is really whatever pointer is in GOT + 4.

    RelocationOffset - Supplies the byte offset from the start of the
        relocation section where the relocation for this PLT entry resides, or
        the PLT index, depending on the architecture.

Return Value:

    Returns a pointer to the function to jump to (in addition to writing that
    address in the GOT at the appropriate spot).

--*/

