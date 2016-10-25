/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    core.h

Abstract:

    This header contains definitions for the Chalk core classes.

Author:

    Evan Green 28-May-2016

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

#define CK_FREEZE_SIGNATURE_SIZE 4

//
// ------------------------------------------------------ Data Type Definitions
//

/*++

Structure Description:

    This structure contains the state for the Chalk interpreter.

Members:

    Name - Stores the name string of the function to attach to the class.

    Arity - Stores the number of arguments the function takes.

    Primitive - Stores a pointer to the primitive function to call for the
        function

--*/

typedef struct _CK_PRIMITIVE_DESCRIPTION {
    PSTR Name;
    CK_ARITY Arity;
    PCK_PRIMITIVE_FUNCTION Primitive;
} CK_PRIMITIVE_DESCRIPTION, *PCK_PRIMITIVE_DESCRIPTION;

//
// -------------------------------------------------------------------- Globals
//

extern CK_PRIMITIVE_DESCRIPTION CkIntPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkIntStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkRangePrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkStringPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkStringStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkListPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkDictPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkFiberPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkFiberStaticPrimitives[];
extern CK_PRIMITIVE_DESCRIPTION CkModulePrimitives[];

extern const UCHAR CkModuleFreezeSignature[CK_FREEZE_SIGNATURE_SIZE];

//
// -------------------------------------------------------- Function Prototypes
//

CK_ERROR_TYPE
CkpInitializeCore (
    PCK_VM Vm
    );

/*++

Routine Description:

    This routine initialize the Chalk VM, creating and wiring up the root
    classes.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

Return Value:

    Chalk status.

--*/

CK_ARITY
CkpGetFunctionArity (
    PCK_CLOSURE Closure
    );

/*++

Routine Description:

    This routine returns the number of argument required to pass to the given
    function.

Arguments:

    Closure - Supplies a pointer to the closure.

Return Value:

    Returns the arity of the function.

--*/

PCK_STRING
CkpGetFunctionName (
    PCK_CLOSURE Closure
    );

/*++

Routine Description:

    This routine returns the original name for a function.

Arguments:

    Closure - Supplies a pointer to the closure.

Return Value:

    Returns a pointer to a string containing the name of the function.

--*/

BOOL
CkpObjectIsClass (
    PCK_CLASS ObjectClass,
    PCK_CLASS QueryClass
    );

/*++

Routine Description:

    This routine determines if the given object is of the given type.

Arguments:

    ObjectClass - Supplies a pointer to the object class to query.

    QueryClass - Supplies a pointer to the class to check membership for.

Return Value:

    TRUE if the object class is a subclass of the query class.

    FALSE if the object class is not a member of the query class.

--*/

//
// Exception functions
//

VOID
CkpError (
    PCK_VM Vm,
    CK_ERROR_TYPE ErrorType,
    PSTR Message
    );

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

VOID
CkpRuntimeError (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR MessageFormat,
    ...
    );

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

VOID
CkpRaiseException (
    PCK_VM Vm,
    CK_VALUE Exception,
    UINTN Skim
    );

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

VOID
CkpRaiseInternalException (
    PCK_VM Vm,
    PCSTR Type,
    PCSTR Format,
    va_list Arguments
    );

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

