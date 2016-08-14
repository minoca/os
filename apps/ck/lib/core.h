/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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

VOID
CkpRuntimeError (
    PCK_VM Vm,
    PSTR MessageFormat,
    ...
    );

/*++

Routine Description:

    This routine reports a runtime error in the current fiber.

Arguments:

    Vm - Supplies a pointer to the virtual machine.

    MessageFormat - Supplies the printf message format string.

    ... - Supplies the remaining arguments.

Return Value:

    None.

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

PSTR
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

