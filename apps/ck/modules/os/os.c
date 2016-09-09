/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    os.c

Abstract:

    This module implements the Chalk os module, which provides functionality
    from the underlying operating system.

Author:

    Evan Green 28-Aug-2016

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "osp.h"
#include <errno.h>
#include <fcntl.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpOsOpen (
    PCK_VM Vm
    );

VOID
CkpOsClose (
    PCK_VM Vm
    );

VOID
CkpOsRead (
    PCK_VM Vm
    );

VOID
CkpOsWrite (
    PCK_VM Vm
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkOsModuleValues[] = {
    {CkTypeFunction, "open", CkpOsOpen, 3},
    {CkTypeFunction, "close", CkpOsClose, 1},
    {CkTypeFunction, "read", CkpOsRead, 2},
    {CkTypeFunction, "write", CkpOsWrite, 2},
    {CkTypeInvalid, NULL, NULL, 0}
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadOsModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the OS module. It is called to make the presence of
    the os module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "os", NULL, NULL, CkpOsModuleInit);
}

VOID
CkpOsModuleInit (
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

    CkDeclareVariables(Vm, 0, CkOsModuleValues);
    return;
}

VOID
CkpOsOpen (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the open call. It takes in a path string, flags
    integer, and creation mode integer. It returns a file descriptor integer
    on success, or -1 on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Descriptor;
    ULONG Flags;
    ULONG Mode;
    PCSTR Path;

    //
    // The function is open(path, flags, mode).
    //

    if (!CkCheckArguments(Vm, 3, CkTypeString, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Path = CkGetString(Vm, 1, NULL);
    Flags = CkGetInteger(Vm, 2);
    Mode = CkGetInteger(Vm, 3);
    Descriptor = open(Path, Flags, Mode);
    CkReturnInteger(Vm, Descriptor);
    return;
}

VOID
CkpOsClose (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the close call. It takes in a file descriptor
    integer, and returns the integer returned by the close call.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    INT Result;

    if (!CkCheckArguments(Vm, 1, CkTypeInteger)) {
        return;
    }

    Result = close(CkGetInteger(Vm, 1));
    CkReturnInteger(Vm, Result);
    return;
}

VOID
CkpOsRead (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the read call. It takes in a file descriptor
    integer and a size, and reads at most that size byte from the descriptor.
    It returns a string containing the bytes read on success, an empty string
    if no bytes were read, or null on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PSTR Buffer;
    ssize_t BytesRead;
    INT Descriptor;
    INT Size;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Size = CkGetInteger(Vm, 2);
    if (Size < 0) {
        Size = 0;
    }

    Buffer = CkPushStringBuffer(Vm, Size);
    if (Buffer == NULL) {
        return;
    }

    do {
        BytesRead = read(Descriptor, Buffer, Size);

    } while ((BytesRead < 0) && (errno == EINTR));

    if (BytesRead < 0) {
        CkReturnNull(Vm);

    } else {
        CkFinalizeString(Vm, -1, BytesRead);
        CkStackReplace(Vm, 0);
    }

    return;
}

VOID
CkpOsWrite (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine implements the write call. It takes in a file descriptor
    integer and a string, and attempts to write that string to the descriptor.
    It returns the number of bytes actually written, which may be less than the
    desired size, or -1 on failure.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Buffer;
    ssize_t BytesWritten;
    INT Descriptor;
    UINTN Size;

    if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeString)) {
        return;
    }

    Descriptor = CkGetInteger(Vm, 1);
    Buffer = CkGetString(Vm, 2, &Size);
    do {
        BytesWritten = write(Descriptor, Buffer, Size);

    } while ((BytesWritten < 0) && (errno == EINTR));

    CkReturnInteger(Vm, BytesWritten);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

