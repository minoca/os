/*++

Copyright (c) 2013 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    osbasep.h

Abstract:

    This header contains internal definitions for the Operating System Base
    library.

Author:

    Evan Green 25-Feb-2013

--*/

//
// ------------------------------------------------------------------- Includes
//

#define OS_API __DLLPROTECTED
#define RTL_API __DLLPROTECTED

#include <minoca/lib/minocaos.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// TODO: Implement syscall system call mechanism on x64, in addition to keeping
// the old int mechanism for full save/restore.
//

#if defined(__amd64)

#define OsSystemCall OspSystemCallFull

#endif

//
// ------------------------------------------------------ Data Type Definitions
//

typedef
INTN
(*POS_SYSTEM_CALL) (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine executes a system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

/*++

Structure Description:

    This structure stores the thread control block, a structure used in user
    mode to contain information unique to each thread.

Members:

    Self - Stores a pointer to the thread control block itself. This member
        is mandated by many application ABIs.

    TlsVector - Stores an array of pointers to TLS regions for each module. The
        first element is a generation number, indicating whether or not the
        array needs to be resized. This member is access directly from assembly.

    ModuleCount - Stores the count of loaded modules this thread is aware of.

    BaseAllocation - Stores a pointer to the actual allocation pointer returned
        to free this structure and all the initial TLS blocks.

    StackGuard - Stores the stack guard value. This is referenced directly by
        GCC, and must be at offset 0x14 on 32-bit systems, 0x28 on 64-bit
        systems.

    BaseAllocationSize - Stores the size of the base allocation region in bytes.

    ListEntry - Stores pointers to the next and previous threads in the OS
        Library thread list.

--*/

typedef struct _THREAD_CONTROL_BLOCK {
    PVOID Self;
    PVOID *TlsVector;
    UINTN ModuleCount;
    PVOID BaseAllocation;
    UINTN StackGuard;
    UINTN BaseAllocationSize;
    LIST_ENTRY ListEntry;
} THREAD_CONTROL_BLOCK, *PTHREAD_CONTROL_BLOCK;

//
// -------------------------------------------------------------------- Globals
//

//
// Store a pointer to the environment.
//

extern PPROCESS_ENVIRONMENT OsEnvironment;

//
// On x86, the system call is a function pointer depending on which processor
// features are supported.
//

#if defined(__i386)

extern POS_SYSTEM_CALL OsSystemCall;

#endif

//
// Store a pointer to the list head of all loaded images.
//

extern LIST_ENTRY OsLoadedImagesHead;

//
// Store the module generation number, which increments whenever a module is
// loaded or unloaded. It is protected under the image list lock.
//

extern UINTN OsImModuleGeneration;

//
// Store the page shift and mask for easy use during image section mappings.
//

extern UINTN OsPageShift;
extern UINTN OsPageSize;

//
// -------------------------------------------------------- Function Prototypes
//

INTN
OspSystemCallFull (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine executes a system call using the traditional method that looks
    a lot like an interrupt. On some architectures, this method is highly
    compatible, but slow. On other architectures, this is the only system call
    mechanism.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

#if defined (__arm__)

INTN
OsSystemCall (
    ULONG SystemCallNumber,
    PVOID SystemCallParameter
    );

/*++

Routine Description:

    This routine executes a system call.

Arguments:

    SystemCallNumber - Supplies the system call number.

    SystemCallParameter - Supplies the system call parameter.

Return Value:

    STATUS_SUCCESS or positive integer on success.

    Error status code on failure.

--*/

#endif

VOID
OspSetUpSystemCalls (
    VOID
    );

/*++

Routine Description:

    This routine sets up the system call handler.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspSignalHandler (
    PSIGNAL_PARAMETERS Parameters,
    PSIGNAL_CONTEXT Context
    );

/*++

Routine Description:

    This routine is called directly by the kernel when a signal occurs. It
    marshals the parameters and calls the C routine for handling the signal.
    The parameters are stored on the stack with the signal parameters followed
    by the signal context.

Arguments:

    Parameters - Supplies a pointer to the signal parameters.

    Context - Supplies a pointer to the signal context from the kernel.

Return Value:

    None.

--*/

VOID
OspInitializeMemory (
    VOID
    );

/*++

Routine Description:

    This routine initializes the memory heap portion of the OS base library.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspInitializeImageSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes the image library for use in the image creation
    tool.

Arguments:

    None.

Return Value:

    None.

--*/

VOID
OspAcquireImageLock (
    BOOL Exclusive
    );

/*++

Routine Description:

    This routine acquires the global image lock.

Arguments:

    Exclusive - Supplies a boolean indicating whether the lock should be
        held shared (FALSE) or exclusive (TRUE).

Return Value:

    None.

--*/

VOID
OspReleaseImageLock (
    VOID
    );

/*++

Routine Description:

    This routine releases the global image lock.

Arguments:

    None.

Return Value:

    None.

--*/

PUSER_SHARED_DATA
OspGetUserSharedData (
    VOID
    );

/*++

Routine Description:

    This routine returns a pointer to the user shared data.

Arguments:

    None.

Return Value:

    Returns a pointer to the user shared data area.

--*/

//
// Thread-Local storage functions
//

VOID
OspInitializeThreadSupport (
    VOID
    );

/*++

Routine Description:

    This routine initializes thread and TLS support in the OS library.

Arguments:

    None.

Return Value:

    None.

--*/

KSTATUS
OspTlsAllocate (
    PLIST_ENTRY ImageList,
    PVOID *ThreadData,
    BOOL CopyInitImage
    );

/*++

Routine Description:

    This routine creates the OS library data necessary to manage a new thread.
    This function is usually called by the C library.

Arguments:

    ImageList - Supplies a pointer to the head of the list of loaded images.
        Elements on this list have type LOADED_IMAGE.

    ThreadData - Supplies a pointer where a pointer to the thread data will be
        returned on success. It is the callers responsibility to destroy this
        thread data.

    CopyInitImage - Supplies a boolean indicating whether or not to copy the
        initial image over to the new TLS area or not. If this is the initial
        program load and images have not yet been relocated, then the copies
        are skipped since they need to be done after relocations are applied.

Return Value:

    Status code.

--*/

VOID
OspTlsDestroy (
    PVOID ThreadData
    );

/*++

Routine Description:

    This routine destroys a previously created thread data structure. Callers
    may not use OS library assisted TLS after this routine completes. Signals
    should also probably be masked.

Arguments:

    ThreadData - Supplies a pointer to the thread data to destroy.

Return Value:

    None.

--*/

VOID
OspTlsTearDownModule (
    PLOADED_IMAGE Image
    );

/*++

Routine Description:

    This routine is called when a module is unloaded. It goes through and
    frees all the TLS images for the module.

Arguments:

    Image - Supplies a pointer to the image being unloaded.

Return Value:

    None.

--*/

