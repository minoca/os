/*++

Copyright (c) 2017 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    lzma.c

Abstract:

    This module implements the LZMA Chalk C module.

Author:

    Evan Green 23-May-2017

Environment:

    C

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "lzmap.h"

//
// --------------------------------------------------------------------- Macros
//

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the default size of the buffers to use.
//

#define CK_LZ_DEFAULT_BUFFER_SIZE (1024 * 128)

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure stores the context for an LZMA encoder or decoder class
    instance.

Members:

    Encoder - Stores a boolean indicating whether the instance is an LZMA
        encoder (TRUE) or decoder (FALSE).

    Finished - Stores a boolean indicating whether or not the stream has
        already been finalized.

    Initialized - Stores a boolean indicating whether or not the context has
        been initialized.

    FileWrapper - Stores a boolean indicating whether or not the file wrapper
        is requested.

    Level - Stores the compression level of the stream.

    Status - Stores the last status code returned from an operation.

    Lz - Stores the LZMA context.

--*/

typedef struct _CK_LZ_CONTEXT {
    BOOL Encoder;
    BOOL Finished;
    BOOL Initialized;
    BOOL FileWrapper;
    INT Level;
    LZ_STATUS Status;
    LZ_CONTEXT Lz;
} CK_LZ_CONTEXT, *PCK_LZ_CONTEXT;

//
// ----------------------------------------------- Internal Function Prototypes
//

VOID
CkpLzmaEncoderInitialize (
    PCK_VM Vm
    );

VOID
CkpLzmaCompress (
    PCK_VM Vm
    );

VOID
CkpLzmaEncoderFinish (
    PCK_VM Vm
    );

VOID
CkpLzmaDecoderInitialize (
    PCK_VM Vm
    );

VOID
CkpLzmaDecompress (
    PCK_VM Vm
    );

VOID
CkpLzmaDecoderFinish (
    PCK_VM Vm
    );

VOID
CkpLzmaStats (
    PCK_VM Vm
    );

VOID
CkpLzmaEncode (
    PCK_VM Vm,
    PCVOID Input,
    UINTN InputLength,
    LZ_FLUSH_OPTION FlushOption
    );

VOID
CkpLzmaDecode (
    PCK_VM Vm,
    PCVOID Input,
    UINTN InputLength,
    LZ_FLUSH_OPTION FlushOption
    );

VOID
CkpLzmaRaiseLzError (
    PCK_VM Vm,
    LZ_STATUS Error
    );

PCK_LZ_CONTEXT
CkpLzmaCreateContext (
    VOID
    );

VOID
CkpLzmaDestroyContext (
    PVOID Data
    );

//
// -------------------------------------------------------------------- Globals
//

CK_VARIABLE_DESCRIPTION CkLzmaModuleValues[] = {
    {CkTypeInteger, "LzSuccess", NULL, LzSuccess},
    {CkTypeInteger, "LzStreamComplete", NULL, LzStreamComplete},
    {CkTypeInteger, "LzErrorCorruptData", NULL, LzErrorCorruptData},
    {CkTypeInteger, "LzErrorMemory", NULL, LzErrorMemory},
    {CkTypeInteger, "LzErrorCrc", NULL, LzErrorCrc},
    {CkTypeInteger, "LzErrorUnsupported", NULL, LzErrorUnsupported},
    {CkTypeInteger, "LzErrorInvalidParameter", NULL, LzErrorInvalidParameter},
    {CkTypeInteger, "LzErrorInputEof", NULL, LzErrorInputEof},
    {CkTypeInteger, "LzErrorOutputEof", NULL, LzErrorOutputEof},
    {CkTypeInteger, "LzErrorRead", NULL, LzErrorRead},
    {CkTypeInteger, "LzErrorWrite", NULL, LzErrorWrite},
    {CkTypeInteger, "LzErrorProgress", NULL, LzErrorProgress},
    {CkTypeInteger, "LzErrorMagic", NULL, LzErrorMagic},
    {CkTypeInvalid, NULL, NULL, 0}
};

PCSTR CkLzStatusStrings[LzErrorCount] = {
    "Success",
    "Stream complete",
    "Corrupt data",
    "Allocation failure",
    "CRC error",
    "Unsupported",
    "Invalid parameter",
    "Unexpected end of input",
    "Unexpected end of output",
    "Read error",
    "Write error",
    "Progress error",
    "Invalid magic value"
};

//
// ------------------------------------------------------------------ Functions
//

BOOL
CkPreloadLzmaModule (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine preloads the LZMA module. It is called to make the presence of
    the module known in cases where the module is statically linked.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    TRUE on success.

    FALSE on failure.

--*/

{

    return CkPreloadForeignModule(Vm, "lzma", NULL, NULL, CkpLzmaModuleInit);
}

VOID
CkpLzmaModuleInit (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine populates the LZMA module namespace.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkDeclareVariables(Vm, 0, CkLzmaModuleValues);

    //
    // Create the LzmaEncoder class.
    //

    CkPushString(Vm, "LzmaEncoder", 11);
    CkGetVariable(Vm, 0, "Object");
    CkPushClass(Vm, 0, 1);
    CkPushValue(Vm, -1);
    CkSetVariable(Vm, 0, "LzmaEncoder");
    CkPushFunction(Vm, CkpLzmaEncoderInitialize, "__init", 0, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaEncoderInitialize, "__init", 2, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaCompress, "compress", 1, 0);
    CkPushString(Vm, "compress", 8);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaEncoderFinish, "finish", 0, 0);
    CkPushString(Vm, "finish", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaStats, "stats", 0, 0);
    CkPushString(Vm, "stats", 5);
    CkBindMethod(Vm, 1);
    CkStackPop(Vm);

    //
    // Create the LzmaDecoder class.
    //

    CkPushString(Vm, "LzmaDecoder", 11);
    CkGetVariable(Vm, 0, "Object");
    CkPushClass(Vm, 0, 1);
    CkPushValue(Vm, -1);
    CkSetVariable(Vm, 0, "LzmaDecoder");
    CkPushFunction(Vm, CkpLzmaDecoderInitialize, "__init", 0, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaDecoderInitialize, "__init", 2, 0);
    CkPushString(Vm, "__init", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaDecompress, "decompress", 1, 0);
    CkPushString(Vm, "decompress", 10);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaDecoderFinish, "finish", 0, 0);
    CkPushString(Vm, "finish", 6);
    CkBindMethod(Vm, 1);
    CkPushFunction(Vm, CkpLzmaStats, "stats", 0, 0);
    CkPushString(Vm, "stats", 5);
    CkBindMethod(Vm, 1);
    CkStackPop(Vm);

    //
    // Create the LzmaError exception.
    //

    CkPushString(Vm, "LzmaError", 9);
    CkGetVariable(Vm, 0, "Exception");
    CkPushClass(Vm, 0, 1);
    CkSetVariable(Vm, 0, "LzmaError");
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

VOID
CkpLzmaEncoderInitialize (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine is called when a new encoder class instance is created. It
    takes two arguments: an encoder level number 0-9 and a boolean indicating
    whether or not the file wrapper should be applied.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_LZ_CONTEXT Context;
    CK_INTEGER FileWrapper;
    CK_INTEGER Level;
    LZ_STATUS LzStatus;
    LZMA_ENCODER_PROPERTIES Properties;

    //
    // If this is the __init function with no arguments, supply default
    // parameters.
    //

    if (CkGetStackSize(Vm) == 1) {
        Level = 5;
        FileWrapper = TRUE;

    //
    // Validate the incoming parameters.
    //

    } else {
        if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
            return;
        }

        Level = CkGetInteger(Vm, 1);
        if ((Level < -1) || (Level > 9)) {
            CkRaiseBasicException(Vm,
                                  "ValueError",
                                  "Compression level must be between 0-9");

            goto EncoderInitializeEnd;
        }

        FileWrapper = CkGetInteger(Vm, 2);
        if ((FileWrapper < 0) || (FileWrapper > 1)) {
            CkRaiseBasicException(Vm, "ValueError", "Expected a boolean");
            goto EncoderInitializeEnd;
        }
    }

    //
    // Reuse an old context in case this is not the first time __init is being
    // called, or create a new context.
    //

    CkGetField(Vm, 0);
    Context = CkGetData(Vm, -1);
    CkStackPop(Vm);
    if (Context == NULL) {
        Context = CkpLzmaCreateContext();
        if (Context == NULL) {
            CkRaiseBasicException(Vm, "MemoryError", "Allocation failure");
            return;
        }

        Context->Encoder = TRUE;
        if (CkPushData(Vm, Context, CkpLzmaDestroyContext) == FALSE) {
            CkpLzmaDestroyContext(Context);
            return;
        }

        CkSetField(Vm, 0);
    }

    Context->Finished = FALSE;
    Context->Status = LzSuccess;
    if (Level == -1) {
        Level = 5;
    }

    Context->Level = Level;
    Context->FileWrapper = FileWrapper;
    LzLzmaInitializeProperties(&Properties);
    Properties.Level = Context->Level;
    LzStatus = LzLzmaInitializeEncoder(&(Context->Lz),
                                       &Properties,
                                       Context->FileWrapper);

    if (LzStatus != LzSuccess) {
        Context->Status = LzStatus;
        CkpLzmaRaiseLzError(Vm, LzStatus);
        goto EncoderInitializeEnd;
    }

    Context->Initialized = TRUE;

EncoderInitializeEnd:
    return;
}

VOID
CkpLzmaCompress (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine compresses LZMA data. It takes one argument: the data to
    compress. It returns some or none of the compressed data. Compressed and
    uncompressed data may be stored within the encoder class itself. Anything
    returned should be appended to the result of previous calls to compress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None. Returns some, all, or none of the compressed data, or raises an
    exception on error.

--*/

{

    PCSTR Input;
    UINTN InputLength;

    if (!CkCheckArguments(Vm, 1, CkTypeString)) {
        return;
    }

    Input = CkGetString(Vm, 1, &InputLength);
    CkpLzmaEncode(Vm, Input, InputLength, LzNoFlush);
    return;
}

VOID
CkpLzmaEncoderFinish (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine finishes and cleans up an LZMA encoder instance. It returns
    any remaining output data.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkpLzmaEncode(Vm, NULL, 0, LzInputFinished);
    return;
}

VOID
CkpLzmaDecoderInitialize (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine is called when a new decoder class instance is created. It
    takes two arguments: the compression level (ignored if there's a file
    wrapper), and a boolean indicating whether or not to expect a file wrapper.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_LZ_CONTEXT Context;
    CK_INTEGER FileWrapper;
    CK_INTEGER Level;
    LZ_STATUS LzStatus;
    LZMA_ENCODER_PROPERTIES Properties;

    //
    // Set some defaults if this is the initializer with no arguments.
    //

    if (CkGetStackSize(Vm) == 1) {
        Level = 5;
        FileWrapper = TRUE;

    //
    // Validate the arguments if supplied.
    //

    } else {
        if (!CkCheckArguments(Vm, 2, CkTypeInteger, CkTypeInteger)) {
            return;
        }

        Level = CkGetInteger(Vm, 1);
        if ((Level < -1) || (Level > 9)) {
            CkRaiseBasicException(Vm,
                                  "ValueError",
                                  "Compression level must be between 0-9");

            goto DecoderInitializeEnd;
        }

        FileWrapper = CkGetInteger(Vm, 2);
        if ((FileWrapper < 0) || (FileWrapper > 1)) {
            CkRaiseBasicException(Vm, "ValueError", "Expected a boolean");
            goto DecoderInitializeEnd;
        }
    }

    //
    // Reuse an old context in case this is not the first time __init is being
    // called, or create a new context.
    //

    CkGetField(Vm, 0);
    Context = CkGetData(Vm, -1);
    CkStackPop(Vm);
    if (Context == NULL) {
        Context = CkpLzmaCreateContext();
        if (Context == NULL) {
            CkRaiseBasicException(Vm, "MemoryError", "Allocation failure");
            return;
        }

        Context->Encoder = FALSE;
        if (CkPushData(Vm, Context, CkpLzmaDestroyContext) == FALSE) {
            CkpLzmaDestroyContext(Context);
            return;
        }

        CkSetField(Vm, 0);
    }

    Context->Finished = FALSE;
    if (Level == -1) {
        Level = 5;
    }

    Context->Level = Level;
    Context->FileWrapper = FileWrapper;
    LzLzmaInitializeProperties(&Properties);
    Properties.Level = Context->Level;
    LzStatus = LzLzmaInitializeDecoder(&(Context->Lz),
                                       &Properties,
                                       Context->FileWrapper);

    if (LzStatus != LzSuccess) {
        Context->Status = LzStatus;
        CkpLzmaRaiseLzError(Vm, LzStatus);
        goto DecoderInitializeEnd;
    }

    Context->Initialized = TRUE;

DecoderInitializeEnd:
    return;
}

VOID
CkpLzmaDecompress (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine decompresses LZMA data. It takes one argument: the compressed
    data to decompress. It returns some, all, or none of the compressed data.
    It may store some of the data within the instance context itself. Data
    returned here should be appended to data returned by previous calls to
    decompress.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCSTR Input;
    UINTN InputLength;

    Input = CkGetString(Vm, 1, &InputLength);
    CkpLzmaDecode(Vm, Input, InputLength, LzNoFlush);
    return;
}

VOID
CkpLzmaDecoderFinish (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine finishes and cleans up an LZMA decoder instance.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    CkpLzmaDecode(Vm, NULL, 0, LzInputFinished);
    return;
}

VOID
CkpLzmaStats (
    PCK_VM Vm
    )

/*++

Routine Description:

    This routine returns a statistics dictionary describing the current
    state of the LZMA encoder or decoder.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    None.

--*/

{

    PCK_LZ_CONTEXT Context;

    CkGetField(Vm, 0);
    Context = CkGetData(Vm, -1);
    CkStackPop(Vm);
    CkPushDict(Vm);
    CkPushString(Vm, "finished", 8);
    CkPushInteger(Vm, Context->Finished);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "status", 6);
    CkPushInteger(Vm, Context->Status);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "fileWrapper", 11);
    CkPushInteger(Vm, Context->FileWrapper);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "level", 5);
    CkPushInteger(Vm, Context->Level);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "compressedCrc32", 15);
    CkPushInteger(Vm, Context->Lz.CompressedCrc32);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "uncompressedCrc32", 17);
    CkPushInteger(Vm, Context->Lz.UncompressedCrc32);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "compressedSize", 14);
    CkPushInteger(Vm, Context->Lz.CompressedSize);
    CkDictSet(Vm, 1);
    CkPushString(Vm, "uncompressedSize", 16);
    CkPushInteger(Vm, Context->Lz.UncompressedSize);
    CkDictSet(Vm, 1);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpLzmaEncode (
    PCK_VM Vm,
    PCVOID Input,
    UINTN InputLength,
    LZ_FLUSH_OPTION FlushOption
    )

/*++

Routine Description:

    This routine compresses LZMA data. The resulting compressed data is
    returned in stack slot zero.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Input - Supplies the input data to encode.

    InputLength - Supplies the length of the input data in bytes.

    FlushOption - Supplies the flush option indicating whether or not more data
        is expected.

Return Value:

    Returns some, all, or none of the compressed data, or raises an
    exception on error.

--*/

{

    PCK_LZ_CONTEXT Context;
    LZ_STATUS LzStatus;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PSTR Output;
    UINTN OutputDone;
    UINTN OutputLength;

    //
    // Get the instance context.
    //

    CkGetField(Vm, 0);
    Context = CkGetData(Vm, -1);
    CkStackPop(Vm);

    assert((Context != NULL) && (Context->Encoder != FALSE));

    //
    // Create an output buffer that is as big as the input buffer. Almost
    // certainly this will be too big.
    //

    OutputDone = 0;
    OutputLength = InputLength;
    if (OutputLength == 0) {
        OutputLength = CK_LZ_DEFAULT_BUFFER_SIZE;
    }

    Output = CkPushStringBuffer(Vm, OutputLength);
    if (Output == NULL) {
        return;
    }

    //
    // If the stream is already finished, then complain or return quietly,
    // depending on the input.
    //

    if (Context->Finished != FALSE) {
        if (InputLength != 0) {
            CkRaiseBasicException(Vm,
                                  "ValueError",
                                  "Stream is already complete");

            return;
        }

        CkFinalizeString(Vm, -1, 0);
        CkStackReplace(Vm, 0);
        return;
    }

    Context->Lz.Input = Input;
    Context->Lz.InputSize = InputLength;

    //
    // Loop shoving data into the compressor and pulling it out of the output.
    //

    while (TRUE) {
        Context->Lz.Output = Output + OutputDone;
        Context->Lz.OutputSize = OutputLength - OutputDone;
        LzStatus = LzLzmaEncode(&(Context->Lz), FlushOption);
        Context->Status = LzStatus;
        OutputDone = Context->Lz.Output - (PVOID)Output;
        if (LzStatus == LzStreamComplete) {
            LzLzmaFinishEncode(&(Context->Lz));
            Context->Finished = TRUE;
            break;

        } else if (LzStatus != LzSuccess) {
            CkpLzmaRaiseLzError(Vm, LzStatus);
            return;
        }

        if ((FlushOption == LzNoFlush) && (Context->Lz.InputSize == 0)) {
            break;
        }

        //
        // Reallocate the output buffer and try again.
        //

        assert(Context->Lz.OutputSize == 0);

        NewCapacity = OutputLength * 2;
        if (NewCapacity < OutputLength) {
            CkRaiseBasicException(Vm, "ValueError", "Buffer size overflow");
            return;
        }

        NewBuffer = CkPushStringBuffer(Vm, NewCapacity);
        if (NewBuffer == NULL) {
            return;
        }

        memcpy(NewBuffer, Output, OutputDone);
        CkStackReplace(Vm, -2);
        Output = NewBuffer;
        OutputLength = NewCapacity;
    }

    //
    // Return the output data.
    //

    CkFinalizeString(Vm, -1, OutputDone);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpLzmaDecode (
    PCK_VM Vm,
    PCVOID Input,
    UINTN InputLength,
    LZ_FLUSH_OPTION FlushOption
    )

/*++

Routine Description:

    This routine decompresses LZMA data. The resulting decompressed data is
    written to the return stack slot.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Input - Supplies the input data to encode.

    InputLength - Supplies the length of the input data in bytes.

    FlushOption - Supplies the flush option indicating whether or not more data
        is expected.

Return Value:

    None.

--*/

{

    PCK_LZ_CONTEXT Context;
    LZ_STATUS LzStatus;
    PVOID NewBuffer;
    UINTN NewCapacity;
    PSTR Output;
    UINTN OutputDone;
    UINTN OutputLength;

    //
    // Get the instance context.
    //

    CkGetField(Vm, 0);
    Context = CkGetData(Vm, -1);
    CkStackPop(Vm);

    assert((Context != NULL) && (Context->Encoder == FALSE));

    //
    // Create an output buffer that is four times as big as the input buffer.
    // This is just a wild guess.
    //

    OutputDone = 0;
    OutputLength = InputLength * 4;
    if (OutputLength == 0) {
        OutputLength = CK_LZ_DEFAULT_BUFFER_SIZE;
    }

    Output = CkPushStringBuffer(Vm, OutputLength);
    if (Output == NULL) {
        return;
    }

    //
    // If the stream is already finished, then complain or return quietly,
    // depending on the input.
    //

    if (Context->Finished != FALSE) {
        if (InputLength != 0) {
            CkRaiseBasicException(Vm,
                                  "ValueError",
                                  "Stream is already complete");

            return;
        }

        CkFinalizeString(Vm, -1, 0);
        CkStackReplace(Vm, 0);
        return;
    }

    Context->Lz.Input = Input;
    Context->Lz.InputSize = InputLength;

    //
    // Loop shoving data into the decompressor and pulling it out of the output.
    //

    while (TRUE) {
        Context->Lz.Output = Output + OutputDone;
        Context->Lz.OutputSize = OutputLength - OutputDone;
        LzStatus = LzLzmaDecode(&(Context->Lz), FlushOption);
        OutputDone = Context->Lz.Output - (PVOID)Output;
        Context->Status = LzStatus;
        if (LzStatus == LzStreamComplete) {
            LzLzmaFinishDecode(&(Context->Lz));
            Context->Finished = TRUE;
            break;

        } else if (LzStatus != LzSuccess) {
            CkpLzmaRaiseLzError(Vm, LzStatus);
            return;
        }

        if ((Context->Lz.InputSize == 0) && (FlushOption == LzNoFlush)) {
            break;
        }

        //
        // Reallocate the output buffer and try again.
        //

        assert(Context->Lz.OutputSize == 0);

        NewCapacity = OutputLength * 2;
        if (NewCapacity < OutputLength) {
            CkRaiseBasicException(Vm, "ValueError", "Buffer size overflow");
            return;
        }

        NewBuffer = CkPushStringBuffer(Vm, NewCapacity);
        if (NewBuffer == NULL) {
            return;
        }

        memcpy(NewBuffer, Output, OutputDone);
        CkStackReplace(Vm, -2);
        Output = NewBuffer;
        OutputLength = NewCapacity;
    }

    //
    // Return the output data.
    //

    CkFinalizeString(Vm, -1, OutputDone);
    CkStackReplace(Vm, 0);
    return;
}

VOID
CkpLzmaRaiseLzError (
    PCK_VM Vm,
    LZ_STATUS Error
    )

/*++

Routine Description:

    This routine raises an LzmaError exception.

Arguments:

    Vm - Supplies a pointer to the VM.

    Error - Supplies the error code to raise.

Return Value:

    None.

--*/

{

    PCSTR ErrorString;

    if (Error >= LzErrorCount) {
        ErrorString = "Unknown error";

    } else {
        ErrorString = CkLzStatusStrings[Error];
    }

    //
    // Create an LzmaError exception.
    //

    CkPushModule(Vm, "lzma");
    CkGetVariable(Vm, -1, "LzmaError");
    CkPushString(Vm, ErrorString, strlen(ErrorString));
    CkCall(Vm, 1);

    //
    // Execute instance.status = Error.
    //

    CkPushValue(Vm, -1);
    CkPushString(Vm, "status", 6);
    CkPushInteger(Vm, Error);
    CkCallMethod(Vm, "__set", 2);
    CkStackPop(Vm);

    //
    // Raise the exception.
    //

    CkRaiseException(Vm, -1);
    return;
}

PCK_LZ_CONTEXT
CkpLzmaCreateContext (
    VOID
    )

/*++

Routine Description:

    This routine creates a new LZMA encoder or decoder context.

Arguments:

    None.

Return Value:

    Returns a pointer to the new context on success.

    NULL on allocation failure.

--*/

{

    PCK_LZ_CONTEXT NewContext;

    NewContext = malloc(sizeof(CK_LZ_CONTEXT));
    if (NewContext == NULL) {
        return NULL;
    }

    memset(NewContext, 0, sizeof(CK_LZ_CONTEXT));
    NewContext->Lz.Reallocate = (PLZ_REALLOCATE)realloc;
    return NewContext;
}

VOID
CkpLzmaDestroyContext (
    PVOID Data
    )

/*++

Routine Description:

    This routine is called back when the LZMA context is being destroyed.

Arguments:

    Data - Supplies a pointer to the class context to destroy.

Return Value:

    None.

--*/

{

    PCK_LZ_CONTEXT Context;

    Context = Data;
    if (Context->Initialized != FALSE) {
        Context->Lz.Output = NULL;
        Context->Lz.OutputSize = 0;
        if (Context->Finished == FALSE) {
            if (Context->Encoder != FALSE) {
                LzLzmaFinishEncode(&(Context->Lz));

            } else {
                LzLzmaFinishDecode(&(Context->Lz));
            }

            Context->Finished = TRUE;
        }
    }

    free(Context);
    return;
}

