/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    getopt.ck

Abstract:

    This module implements support for command line option parsing.

Author:

    Evan Green 6-Jan-2016

Environment:

    Chalk

--*/

//
// ------------------------------------------------------------------- Includes
//

//
// ---------------------------------------------------------------- Definitions
//

//
// ----------------------------------------------- Internal Function Prototypes
//

function
_doLongs (
    opts,
    opt,
    longopts,
    args
    );

function
_doShorts (
    opts,
    optstring,
    shortopts,
    args
    );

function
_findLongArg (
    opt,
    longopts
    );

function
_findShortArg (
    opt,
    shortopts
    );

//
// -------------------------------------------------------------------- Globals
//

//
// ------------------------------------------------------------------ Functions
//

//
// Define the getopt exception.
//

class GetoptError is Exception {
    var _message;
    var _option;

    function
    __init (
        message,
        option
        )

    /*++

    Routine Description:

        This routine initializes a new getopt exception.

    Arguments:

        message - Supplies the error description.

        option - Supplies the option that caused the exception.

    Return Value:

        Returns the initialized exception.

    --*/

    {

        _message = message;
        _option = option;
        return super.__init([message, option]);
    }

    function
    __str (
        )

    /*++

    Routine Description:

        This routine returns the string representation of the given object.

    Arguments:

        None.

    Return Value:

        Returns a string describing the object.

    --*/

    {

        return _message;
    }
}

function
getopt (
    args,
    shortopts,
    longopts
    )

/*++

Routine Description:

    This routine parses command line arguments.

Arguments:

    args - Supplies the list of command line arguments, without the traditional
        first argument describing the running program.

    shortopts - Supplies a string of short options characters to allow, which
        take the form -c (where c is a character) in the command line. For each
        character here, if a colon comes after it, then it requires an argument.

    longopts - Supplies a list of long options to allow, which take the form
        --option or --option=argument in the command line. If the option ends
        in "=", then the option takes a required argument.

Return Value:

    Returns a list of options and remaining arguments. Each option is a list
    that contains the option string (prefixed with either - or -- to indicate
    short vs long) and an optional argument, if supplied. If no argument is
    provided, then the argument is an empty string.

--*/

{

    var opts = [];
    var result = [opts, args];

    longopts ?= [];
    if (longopts.type() == String) {
        longopts = [longopts];
    }

    while ((args.length()) && (args[0][0] == "-") && (args[0] != "-")) {
        if (args[0] == "--") {
            result = [opts, args[1...-1]];
            break;
        }

        if (args[0][0..2] == "--") {
            result = _doLongs(opts, args[0][2...-1], longopts, args[1...-1]);

        } else {
            result = _doShorts(opts, args[0][1...-1], shortopts, args[1...-1]);
        }

        opts = result[0];
        args = result[1];
    }

    return result;
}

function
gnuGetopt (
    args,
    shortopts,
    longopts
    )

/*++

Routine Description:

    This routine parses command line arguments in GNU style. It operates just
    like the getopt function, except that options can be anywhere (before a
    lone --) in the arguments list.

Arguments:

    args - Supplies the list of command line arguments, without the traditional
        first argument describing the running program.

    shortopts - Supplies a string of short options characters to allow, which
        take the form -c (where c is a character) in the command line. For each
        character here, if a colon comes after it, then it requires an argument.

    longopts - Supplies a list of long options to allow, which take the form
        --option or --option=argument in the command line. If the option ends
        in "=", then the option takes a required argument.

Return Value:

    Returns a list of options and remaining arguments. Each option is a list
    that contains the option string (prefixed with either - or -- to indicate
    short vs long) and an optional argument, if supplied. If no argument is
    provided, then the argument is an empty string.

--*/

{

    var allOptionsFirst = false;
    var opts = [];
    var progArgs = [];
    var result = [opts, args];

    longopts ?= [];
    if (longopts.type() == String) {
        longopts = [longopts];
    }

    try {
        if (shortopts[0] == "+") {
            shortopts = shortopts[1...-1];
            allOptionsFirst = true;
        }

    } except IndexError {}

    while (args.length()) {
        if (args[0] == "--") {
            progArgs += args[1...-1];
            break;
        }

        if (args[0][0..2] == "--") {
            result = _doLongs(opts, args[0][2...-1], longopts, args[1...-1]);

        } else if ((args[0][0] == "-") && (args[0] != "-")) {
            result = _doShorts(opts, args[0][1...-1], shortopts, args[1...-1]);

        } else {
            if (allOptionsFirst) {
                progArgs += args;
                break;
            }

            progArgs.append(args[0]);
            args = args[1...-1];
            result = [opts, args];
        }

        opts = result[0];
        args = result[1];
    }

    return [opts, progArgs];
}

//
// --------------------------------------------------------- Internal Functions
//

function
_doLongs (
    opts,
    opt,
    longopts,
    args
    )

/*++

Routine Description:

    This routine matches the given option for any long arguments.

Arguments:

    opts - Supplies the parsed list of options so far.

    opt - Supplies the current option being processed, without the leading --.

    longopts - Supplies a list of long options to allow, which take the form
        --option or --option=argument in the command line. If the option ends
        in "=", then the option takes a required argument.

    args - Supplies the remaining arguments.

Return Value:

    Returns a list of modified [opts, args].

--*/

{

    var equalsIndex = opt.indexOf("=");
    var hasArgument;
    var optarg = null;
    var result;

    if (equalsIndex >= 0) {
        optarg = opt[(equalsIndex + 1)...-1];
        opt = opt[0..equalsIndex];
    }

    result = _findLongArg(opt, longopts);
    hasArgument = result[0];
    opt = result[1];
    if (hasArgument) {
        if (optarg == null) {
            if (args.length() == 0) {
                Core.raise(
                      GetoptError("option --%s requires argument" % opt, opt));
            }

            optarg = args[0];
            args = args[1...-1];
        }

    } else if (optarg != null) {
        Core.raise(
              GetoptError("option --%s does not take an argument" % opt, opt));
    }

    optarg ?= "";
    opts.append(["--" + opt, optarg]);
    result = [opts, args];
    return result;
}

function
_doShorts (
    opts,
    optstring,
    shortopts,
    args
    )

/*++

Routine Description:

    This routine processes the short options.

Arguments:

    opts - Supplies the parsed list of options so far.

    optstring - Supplies the current argument being processed, without the
        leading -.

    shortopts - Supplies a string of short option characters. Any option
        character with a colon after it requires an argument after it.

    args - Supplies the remaining arguments.

Return Value:

    Returns a list of modified [opts, args].

--*/

{

    var opt;
    var optarg;

    while (optstring != "") {
        opt = optstring[0];
        optstring = optstring[1...-1];
        if (_findShortArg(opt, shortopts)) {
            if (optstring == "") {
                if (args.length() == 0) {
                    Core.raise(
                        GetoptError("option -%s requires an argument" % opt,
                                    opt));
                }

                optstring = args[0];
                args = args[1...-1];
            }

            optarg = optstring;
            optstring = "";

        } else {
            optarg = "";
        }

        opts.append(["-" + opt, optarg]);
    }

    return [opts, args];
}

function
_findLongArg (
    opt,
    longopts
    )

/*++

Routine Description:

    This routine attempts to match a given option with a valid long argument.

Arguments:

    opt - Supplies the current option being processed, without the leading --.

    longopts - Supplies the list of long options to allow.

Return Value:

    Returns a list of [does the option need an argument, option].

--*/

{
    var hasArgument;
    var match;
    var possibilities = [];

    for (longopt in longopts) {
        if (longopt.startsWith(opt)) {
            possibilities.append(longopt);
        }
    }

    if (possibilities.length() == 0) {
        Core.raise(GetoptError("invalid option --%s" % opt, opt));
    }

    if (possibilities.contains(opt)) {
        return [false, opt];

    } else if (possibilities.contains(opt + "=")) {
        return [true, opt];
    }

    //
    // The option is not an exact match.
    //

    if (possibilities.length() > 1) {
        Core.raise(GetoptError("ambiguous option --%s" % opt, opt));
    }

    match = possibilities[0];
    hasArgument = match.endsWith("=");
    if (hasArgument) {
        match = match[0..-1];
    }

    return [hasArgument, match];
}

function
_findShortArg (
    opt,
    shortopts
    )

/*++

Routine Description:

    This routine attempts to find a matching short option character for the
    given option.

Arguments:

    opt - Supplies the current option being processed, without the leading -.

    shortopts - Supplies a string of short option characters. Any option
        character with a colon after it requires an argument after it.

Return Value:

    Returns a boolean indicating whether or not the found option needs an
    argument or not.

--*/

{

    for (index in 0..shortopts.length()) {
        if ((opt == shortopts[index]) && (opt != ":")) {
            return shortopts[index + 1] == ":";
        }
    }

    Core.raise(GetoptError("invalid option -%s" % opt, opt));
}

