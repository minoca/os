/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    except.c

Abstract:

    This module implements support for exceptions in Chalk.

Author:

    Evan Green 26-Sep-2016

Environment:

    Any

--*/

//
// ------------------------------------------------------------------- Includes
//

#include "chalkp.h"
#include "debug.h"

#include <stdio.h>

//
// ---------------------------------------------------------------- Definitions
//

//
// Define the hard coded field values in an exception instance.
//

#define CK_EXCEPTION_FIELD_VALUE 0
#define CK_EXCEPTION_FIELD_STACK_TRACE 1

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

CK_VALUE
CkpExceptionGetField (
    PCK_VM Vm,
    CK_VALUE Exception,
    CK_SYMBOL_INDEX Field
    );

VOID
CkpExceptionSetField (
    PCK_VM Vm,
    CK_VALUE Exception,
    CK_VALUE Value,
    CK_SYMBOL_INDEX Field
    );

//
// -------------------------------------------------------------------- Globals
//

PCSTR CkExceptionKeyNames[] = {
    "args",
    "stackTrace"
};

UCHAR CkExceptionKeyLengths[] = {
    4,
    10
};

//
// ------------------------------------------------------------------ Functions
//

VOID
CkpError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Message
    )

/*++

Routine Description:

    This routine calls the error function when the Chalk interpreter
    experiences an error it cannot itself recover from. Usually the appropriate
    course of action is to clean up and exit without returning.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    ErrorType - Supplies the type of error occurring.

    Message - Supplies a pointer to a string describing the error.

Return Value:

    None.

--*/

{

    if (Vm->Configuration.Error != NULL) {
        Vm->Configuration.Error(Vm, ErrorType, Message);
    }

    return;
}

VOID
CkpRuntimeError (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR MessageFormat,
    ...
    )

/*++

Routine Description:

    This routine reports a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Type - Supplies the name of a builtin exception type.

    MessageFormat - Supplies the printf message format string.

    ... - Supplies the remaining arguments.

Return Value:

    None.

--*/

{

    va_list ArgumentList;

    va_start(ArgumentList, MessageFormat);
    CkpRaiseInternalException(Vm, Type, MessageFormat, ArgumentList);
    va_end(ArgumentList);
    return;
}

VOID
CkpRaiseException (
    PCK_VM Vm,
    CK_VALUE Exception,
    UINTN Skim
    )

/*++

Routine Description:

    This routine raises an exception on the currently running fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Exception - Supplies the exception to raise.

    Skim - Supplies the number of most recently called functions not to include
        in the stack trace. This is usually 0 for exceptions created in C and
        1 for exceptions created in Chalk.

Return Value:

    None.

--*/

{

    PCK_FIBER Fiber;
    PCK_CALL_FRAME Frame;
    UINTN FrameIndex;
    CK_VALUE StackTrace;
    PCK_TRY_BLOCK TryBlock;
    PCK_FOREIGN_FUNCTION UnhandledException;
    CK_VALUE UnhandledName;
    PCK_STRING UnhandledString;

    //
    // Set the stack trace for the exception.
    //

    StackTrace = CkpExceptionGetField(Vm,
                                      Exception,
                                      CK_EXCEPTION_FIELD_STACK_TRACE);

    if (CK_IS_NULL(StackTrace)) {
        StackTrace = CkpCreateStackTrace(Vm, Skim);
        CkpExceptionSetField(Vm,
                             Exception,
                             StackTrace,
                             CK_EXCEPTION_FIELD_STACK_TRACE);
    }

    //
    // If an exception is occurring with no running fiber, create one. If the
    // fiber could not be created, then the memory allocation routine must have
    // already called out to the serious error function, so nothing needs to be
    // done here.
    //

    if (Vm->Fiber == NULL) {
        Vm->Fiber = CkpFiberCreate(Vm, NULL);
        if (Vm->Fiber == NULL) {
            Fiber = NULL;
            goto RaiseExceptionEnd;
        }
    }

    //
    // Loop trying to give the exception to the currently running fiber, and
    // passing back up to the caller if there are no try blocks.
    //

    Fiber = Vm->Fiber;
    while (TRUE) {

        //
        // If there are no open try blocks, move to the calling fiber if
        // possible.
        //

        if (Fiber->TryCount == 0) {
            Fiber->Error = Exception;
            Fiber->FrameCount = 0;
            Fiber->StackTop = Fiber->Stack;
            Fiber = Fiber->Caller;
            if (Fiber == NULL) {
                break;
            }

            Vm->Fiber = Fiber;
            continue;
        }

        TryBlock = &(Fiber->TryStack[Fiber->TryCount - 1]);

        CK_ASSERT((TryBlock->FrameCount != 0) &&
                  (TryBlock->FrameCount <= Fiber->FrameCount));

        //
        // Make sure that none of the call frames being popped off right now
        // are foreign functions, except the topmost one (as that indicates
        // the foreign function raised the exception and knows it needs to
        // return immediately).
        //

        for (FrameIndex = TryBlock->FrameCount;
             FrameIndex < Fiber->FrameCount - 1;
             FrameIndex += 1) {

            Frame = &(Fiber->Frames[FrameIndex]);
            if (Frame->Ip == NULL) {

                //
                // Raising exceptions across foreign function calls is
                // currently not allowed, as the foreign function has no way
                // to clean up any resources it might be in the middle of using.
                //

                CK_ASSERT(FALSE);

                CkpError(Vm,
                         CkErrorRuntime,
                         "Exceptions cannot be raised across foreign "
                         "functions");

                Vm->Fiber = NULL;
                goto RaiseExceptionEnd;
            }
        }

        //
        // Reset execution to the exception handler.
        //

        Fiber->FrameCount = TryBlock->FrameCount;
        TryBlock->FrameCount = 0;
        Frame = &(Fiber->Frames[Fiber->FrameCount - 1]);
        Frame->Ip = TryBlock->Ip;

        CK_ASSERT((TryBlock->Stack <= Fiber->StackTop) &&
                  (TryBlock->Stack >= Frame->StackStart));

        Fiber->StackTop = TryBlock->Stack;

        //
        // Pop the try block off. Any additional exceptions now go further up
        // the call stack.
        //

        Fiber->TryCount -= 1;

        //
        // Push the exception on the stack. There had better be room on the
        // stack for it.
        //

        CK_ASSERT(Fiber->StackTop < Fiber->Stack + Fiber->StackCapacity);

        CK_PUSH(Fiber, Exception);
        break;
    }

RaiseExceptionEnd:

    //
    // If no fiber handled the exception, call the unhandled exception handler.
    // The topmost fiber is still pointed at within the VM.
    //

    if ((Fiber == NULL) && (Vm->Fiber != NULL)) {
        if (Vm->Configuration.UnhandledException != NULL) {

            //
            // Null out the unhandled handler as a hint in case that handler
            // generates another exception.
            //

            UnhandledException = Vm->Configuration.UnhandledException;
            Vm->Configuration.UnhandledException = NULL;

            //
            // Create the unhandled exception closure if needed.
            //

            if (Vm->UnhandledException == NULL) {
                UnhandledName = CkpStringCreate(Vm, "<exception>", 11);
                if (!CK_IS_NULL(UnhandledName)) {
                    UnhandledString = CK_AS_STRING(UnhandledName);
                    Vm->UnhandledException = CkpClosureCreateForeign(
                                              Vm,
                                              UnhandledException,
                                              CkpModuleGet(Vm, CK_NULL_VALUE),
                                              UnhandledString,
                                              1);
                }
            }

            //
            // Call the unhandled exception handler.
            //

            if (Vm->UnhandledException != NULL) {
                Fiber = Vm->Fiber;

                CK_ASSERT(Fiber->StackTop + 3 <=
                          Fiber->Stack + Fiber->StackCapacity);

                //
                // Push the exception an extra time to hold onto it, then push
                // null as the receiver, then the exception as the argument.
                //

                CK_PUSH(Fiber, Exception);
                CK_PUSH(Fiber, CkNullValue);
                CK_PUSH(Fiber, Exception);
                Fiber->Error = CkNullValue;
                CkpCallFunction(Vm, Vm->UnhandledException, 2);
                Fiber->Error = CK_POP(Fiber);

            } else {
                CkpError(Vm,
                         CkErrorRuntime,
                         "Exception occurred");
            }

            //
            // Restore the handler.
            //

            Vm->Configuration.UnhandledException = UnhandledException;

        } else {
            if (Vm->UnhandledException != NULL) {
                CkpError(Vm, CkErrorRuntime, "Double exception");

            } else {
                CkpError(Vm, CkErrorRuntime, "Exception occurred");
            }
        }
    }

    return;
}

VOID
CkpRaiseInternalException (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR Format,
    va_list Arguments
    )

/*++

Routine Description:

    This routine raises an exception from within the Chalk interpreter core.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Type - Supplies a pointer to the name of the exception class to raise. This
        must be visible in the current module.

    Format - Supplies a printf-style format string containing the
        description of the exception.

    Arguments - Supplies any remaining arguments according to the format string.

Return Value:

    None.

--*/

{

    PCK_LIST ArgumentsList;
    CK_VALUE ArgumentsValue;
    PCK_CLASS Class;
    CHAR Description[CK_MAX_ERROR_MESSAGE];
    PCK_INSTANCE Instance;
    INT Length;
    CK_VALUE Value;

    //
    // Do the printf to get a string. To avoid allocations, the internal size
    // is limited.
    //

    Length = vsnprintf(Description, CK_MAX_ERROR_MESSAGE, Format, Arguments);
    if (Length < 0) {
        Length = 0;

    } else if (Length >= CK_MAX_ERROR_MESSAGE) {
        Length = CK_MAX_ERROR_MESSAGE - 1;
    }

    Description[CK_MAX_ERROR_MESSAGE - 1] = '\0';

    //
    // Create a list and create a string in it.
    //

    ArgumentsList = CkpListCreate(Vm, 1);
    if (ArgumentsList != NULL) {
        CkpPushRoot(Vm, &(ArgumentsList->Header));
        ArgumentsList->Elements.Data[0] = CkNullValue;
        ArgumentsList->Elements.Data[0] =
                                      CkpStringCreate(Vm, Description, Length);

    }

    //
    // Get the exception type and create an instance of it.
    //

    Value = *(CkpFindModuleVariable(Vm,
                                    CkpModuleGet(Vm, CK_NULL_VALUE),
                                    Type,
                                    FALSE));

    CK_ASSERT(CK_IS_CLASS(Value));

    Class = CK_AS_CLASS(Value);
    Value = CkpCreateInstance(Vm, Class);
    if (ArgumentsList != NULL) {
        CkpPopRoot(Vm);
    }

    //
    // If creating the exception failed, then things are not looking good. At
    // least set the fiber error to something to indicate an error has occurred.
    //

    if (CK_IS_NULL(Value)) {
        if ((Vm->Fiber != NULL) && (CK_IS_NULL(Vm->Fiber->Error))) {
            CK_INT_VALUE(Vm->Fiber->Error, -1LL);
        }

        return;
    }

    //
    // Set the exception value to the list.
    //

    CK_OBJECT_VALUE(ArgumentsValue, ArgumentsList);
    CkpExceptionSetField(Vm, Value, ArgumentsValue, CK_EXCEPTION_FIELD_VALUE);

    //
    // Raise the newly created exception. Make sure it doesn't get garbage
    // collected during the raise.
    //

    Instance = CK_AS_INSTANCE(Value);
    CkpPushRoot(Vm, &(Instance->Header));
    CkpRaiseException(Vm, Value, 0);
    CkpPopRoot(Vm);
    return;
}

//
// --------------------------------------------------------- Internal Functions
//

CK_VALUE
CkpExceptionGetField (
    PCK_VM Vm,
    CK_VALUE Exception,
    CK_SYMBOL_INDEX Field
    )

/*++

Routine Description:

    This routine returns an exception instance field.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Exception - Supplies the exception.

    Field - Supplies the field to get.

Return Value:

    returns the exception field value.

--*/

{

    PCK_DICT Dict;
    CK_STRING FakeString;
    PCK_INSTANCE Instance;
    CK_VALUE Key;
    PCSTR KeyName;
    CK_VALUE Value;

    CK_ASSERT(
            CkpObjectIsClass(CkpGetClass(Vm, Exception), Vm->Class.Exception));

    //
    // Use the dictionary that all object instances keep at field zero.
    //

    Instance = CK_AS_INSTANCE(Exception);
    if (!CK_IS_DICT(Instance->Fields[0])) {
        return CkNullValue;
    }

    Dict = CK_AS_DICT(Instance->Fields[0]);
    KeyName = CkExceptionKeyNames[Field];
    Key = CkpStringFake(&FakeString, KeyName, CkExceptionKeyLengths[Field]);
    Value = CkpDictGet(Dict, Key);
    if (CK_IS_UNDEFINED(Value)) {
        return CkNullValue;
    }

    return Value;
}

VOID
CkpExceptionSetField (
    PCK_VM Vm,
    CK_VALUE Exception,
    CK_VALUE Value,
    CK_SYMBOL_INDEX Field
    )

/*++

Routine Description:

    This routine sets an exception instance field.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    Exception - Supplies the exception.

    Value - Supplies the value to set in the field.

    Field - Supplies the field to set.

Return Value:

    TRUE on success.

    FALSE if execution caused a runtime error.

--*/

{

    PCK_DICT Dict;
    PCK_INSTANCE Instance;
    CK_VALUE Key;
    PCSTR KeyName;

    CK_ASSERT(
            CkpObjectIsClass(CkpGetClass(Vm, Exception), Vm->Class.Exception));

    //
    // Use the dictionary that all object instances keep at field zero.
    //

    Instance = CK_AS_INSTANCE(Exception);
    CkpPushRoot(Vm, &(Instance->Header));
    if (CK_IS_OBJECT(Value)) {
        CkpPushRoot(Vm, CK_AS_OBJECT(Value));
    }

    if (!CK_IS_DICT(Instance->Fields[0])) {
        Dict = CkpDictCreate(Vm);
        if (Dict == NULL) {
            goto ExceptionSetFieldEnd;
        }

        CK_OBJECT_VALUE(Instance->Fields[0], Dict);
    }

    Dict = CK_AS_DICT(Instance->Fields[0]);
    KeyName = CkExceptionKeyNames[Field];
    Key = CkpStringCreate(Vm, KeyName, CkExceptionKeyLengths[Field]);
    CkpDictSet(Vm, Dict, Key, Value);

ExceptionSetFieldEnd:
    if (CK_IS_OBJECT(Value)) {
        CkpPopRoot(Vm);
    }

    CkpPopRoot(Vm);
    return;
}

