/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

Module Name:

    chkfuncs.c

Abstract:

    This module implements the functions built-in to Chalk for the mingen
    program.

Author:

    Evan Green 15-Jan-2016

Environment:

    POSIX

--*/

//
// ------------------------------------------------------------------- Includes
//

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mingen.h"

//
// ---------------------------------------------------------------- Definitions
//

//
// ------------------------------------------------------ Data Type Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

INT
MingenChalkAssert (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkSplitExtension (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkGetenv (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUnameS (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUnameN (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUnameR (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUnameV (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUnameM (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    );

INT
MingenChalkUname (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue,
    CHAR Flavor
    );

//
// -------------------------------------------------------------------- Globals
//

PSTR MingenChalkAssertArguments[] = {
    "condition",
    "complaint",
    NULL
};

PSTR MingenChalkSplitExtensionArguments[] = {
    "path",
    NULL
};

PSTR MingenChalkNoArguments[] = {
    NULL
};

PSTR MingenChalkGetenvArguments[] = {
    "variable",
    NULL
};

CHALK_FUNCTION_PROTOTYPE MingenChalkFunctions[] = {
    {
        "assert",
        MingenChalkAssertArguments,
        MingenChalkAssert
    },

    {
        "getenv",
        MingenChalkGetenvArguments,
        MingenChalkGetenv
    },

    {
        "split_extension",
        MingenChalkSplitExtensionArguments,
        MingenChalkSplitExtension
    },

    {
        "uname_s",
        MingenChalkNoArguments,
        MingenChalkUnameS
    },

    {
        "uname_n",
        MingenChalkNoArguments,
        MingenChalkUnameN
    },

    {
        "uname_r",
        MingenChalkNoArguments,
        MingenChalkUnameR
    },

    {
        "uname_v",
        MingenChalkNoArguments,
        MingenChalkUnameV
    },

    {
        "uname_m",
        MingenChalkNoArguments,
        MingenChalkUnameM
    },

    {NULL}
};

//
// ------------------------------------------------------------------ Functions
//

INT
MingenAddChalkBuiltins (
    PMINGEN_CONTEXT Context
    )

/*++

Routine Description:

    This routine adds the functions in the global scope of the Chalk
    interpreter for the mingen program.

Arguments:

    Context - Supplies a pointer to the application context.

Return Value:

    0 on success.

    Non-zero on failure.

--*/

{

    INT Status;

    Status = ChalkRegisterFunctions(&(Context->Interpreter),
                                    Context,
                                    MingenChalkFunctions);

    return Status;
}

//
// --------------------------------------------------------- Internal Functions
//

INT
MingenChalkAssert (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the assert function for Chalk in the mingen context.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    PCHALK_OBJECT Condition;

    *ReturnValue = NULL;
    Condition = ChalkCGetVariable(Interpreter, "condition");

    assert(Condition != NULL);

    if (ChalkObjectGetBooleanValue(Condition) != FALSE) {
        return 0;
    }

    fprintf(stderr, "Assertion failure: ");
    ChalkPrintObject(stderr, ChalkCGetVariable(Interpreter, "complaint"), 0);
    return EINVAL;
}

INT
MingenChalkSplitExtension (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the split_extension Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    PCHALK_OBJECT Array[2];
    PSTR Base;
    PCHALK_OBJECT BaseString;
    PSTR Extension;
    PCHALK_OBJECT ExtensionString;
    PCHALK_OBJECT List;
    PCHALK_OBJECT Path;
    INT Status;

    Base = NULL;
    BaseString = NULL;
    ExtensionString = NULL;
    *ReturnValue = NULL;
    Path = ChalkCGetVariable(Interpreter, "path");

    assert(Path != NULL);

    if (Path->Header.Type != ChalkObjectString) {
        fprintf(stderr, "split_extension: String expected\n");
        return EINVAL;
    }

    Extension = NULL;
    Base = MingenSplitExtension(Path->String.String, &Extension);
    if (Base == NULL) {
        BaseString = ChalkCreateString("", 0);

    } else {
        BaseString = ChalkCreateString(Base, strlen(Base));
    }

    if (BaseString == NULL) {
        Status = ENOMEM;
        goto ChalkSplitExtensionEnd;
    }

    if (Extension == NULL) {
        ExtensionString = ChalkCreateString("", 0);

    } else {
        ExtensionString = ChalkCreateString(Extension, strlen(Extension));
    }

    if (ExtensionString == NULL) {
        Status = ENOMEM;
        goto ChalkSplitExtensionEnd;
    }

    Array[0] = BaseString;
    Array[1] = ExtensionString;
    List = ChalkCreateList(Array, 2);
    if (List == NULL) {
        Status = ENOMEM;
        goto ChalkSplitExtensionEnd;
    }

    *ReturnValue = List;
    Status = 0;

ChalkSplitExtensionEnd:
    if (Base != NULL) {
        free(Base);
    }

    if (BaseString != NULL) {
        ChalkObjectReleaseReference(BaseString);
    }

    if (ExtensionString != NULL) {
        ChalkObjectReleaseReference(ExtensionString);
    }

    return Status;
}

INT
MingenChalkGetenv (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the getenv Chalk function, which returns a value
    from the environment.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    PCHALK_OBJECT Name;
    PSTR ValueString;
    UINTN ValueStringSize;

    Name = ChalkCGetVariable(Interpreter, "variable");
    if (Name->Header.Type != ChalkObjectString) {
        fprintf(stderr, "split_extension: String expected\n");
        return EINVAL;
    }

    ValueStringSize = 0;
    ValueString = getenv(Name->String.String);
    if (ValueString == NULL) {
        *ReturnValue = ChalkCreateNull();

    } else {
        ValueStringSize = strlen(ValueString);
        *ReturnValue = ChalkCreateString(ValueString, ValueStringSize);
        if (*ReturnValue == NULL) {
            return ENOMEM;
        }
    }

    return 0;
}

INT
MingenChalkUnameS (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the uname_s Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    return MingenChalkUname(Interpreter, Context, ReturnValue, 's');
}

INT
MingenChalkUnameN (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the uname_n Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    return MingenChalkUname(Interpreter, Context, ReturnValue, 'n');
}

INT
MingenChalkUnameR (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the uname_r Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    return MingenChalkUname(Interpreter, Context, ReturnValue, 'r');
}

INT
MingenChalkUnameV (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the uname_v Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    return MingenChalkUname(Interpreter, Context, ReturnValue, 'v');
}

INT
MingenChalkUnameM (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue
    )

/*++

Routine Description:

    This routine implements the uname_m Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    return MingenChalkUname(Interpreter, Context, ReturnValue, 'm');
}

INT
MingenChalkUname (
    PCHALK_INTERPRETER Interpreter,
    PVOID Context,
    PCHALK_OBJECT *ReturnValue,
    CHAR Flavor
    )

/*++

Routine Description:

    This routine implements the uname_s Chalk function.

Arguments:

    Interpreter - Supplies a pointer to the interpreter context.

    Context - Supplies a pointer's worth of context given when the function
        was registered.

    ReturnValue - Supplies a pointer where a pointer to the return value will
        be returned.

    Flavor - Supplies the flavor of uname to get. Valid values are s, n, r, v,
        and m.

Return Value:

    0 on success.

    Returns an error number on execution failure.

--*/

{

    CHAR Buffer[256];
    INT Status;
    PCHALK_OBJECT String;

    Status = MingenOsUname(Flavor, Buffer, sizeof(Buffer));
    if (Status != 0) {
        Buffer[0] = '\0';
    }

    String = ChalkCreateString(Buffer, strlen(Buffer));
    if (String == NULL) {
        Status = ENOMEM;
        goto ChalkUnameEnd;
    }

    *ReturnValue = String;
    Status = 0;

ChalkUnameEnd:
    return Status;
}

