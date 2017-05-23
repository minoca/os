/*++

Copyright (c) 2016 Minoca Corp.

    This file is licensed under the terms of the GNU General Public License
    version 3. Alternative licensing terms are available. Contact
    info@minocacorp.com for details. See the LICENSE file at the root of this
    project for complete licensing information.

Module Name:

    ckcore.ck

Abstract:

    This module defines the core Chalk classes. It is loaded into every
    instance of the Chalk virtual machine.

Author:

    Evan Green 12-Jul-2016

Environment:

    Core

--*/

//
// -------------------------------------------------------------------- Imports
//

//
// -------------------------------------------------------------------- Classes
//

//
// Define the core classes.
//

class Core {
    static function repr(value) {
        if (value != null) {
            Core._write(value.__repr() + "\n");
        }
    }

    static function print(value) {
        Core._write(value.__str() + "\n");
    }

    static function write(value) {
        Core._write(value.__str());
    }
}

class Fiber {}
class Null {}

class List {
    function __str() {
        return this.toString("[", ", ", "]");
    }

    function toString(begin, separator, end) {
        var element;
        var first = 1;
        var result = begin;
        for (element in this) {
            if (!first) {
                result += separator;
            }

            first = 0;
            result += element.__repr();
        }

        result += end;
        return result;
    }
}

class Exception {
    function __init() {
        this.args = null;
        this.stackTrace = null;
        return this;
    }

    function __init(args) {
        this.args = args;
        this.stackTrace = null;
        return this;
    }

    function __str() {
        var args = this.args;
        var frame;
        var path;
        var result = this.type().name();
        var stackTrace = this.stackTrace;

        if (args) {
            result += ": ";
            if (args is List) {
                result += args.toString("", ". ", ".\n");

            } else {
                result += args.__str() + ".\n";
            }

        } else {
            result += "\n";
        }

        if (stackTrace is List) {
            for (frame in stackTrace) {
                path = "<none>";
                if (frame[1]) {
                    path = frame[1];
                }

                result += frame[0].__str() + " " + frame[2].__str() +
                          " (" + path + ":" + frame[3].__str() + ")\n";
            }
        }

        return result;
    }
}

//
// Define the builtin exceptions.
//

class LookupError is Exception {}

class CompileError is Exception {}
class ImportError is Exception {}
class IndexError is LookupError {}
class KeyError is LookupError {}
class MemoryError is Exception {}
class NameError is Exception {}
class RuntimeError is Exception {}
class TypeError is Exception {}
class ValueError is Exception {}
class FormatError is Exception {}

class Int {
    function __format(flags, width, precision, specifier) {
        var prefix = "";
        var result;
        var value = this;

        if (specifier == "d") {
            if (value < 0) {
                prefix = "-";
                value = -value;

            } else {
                if (flags.contains(" ")) {
                    prefix = " ";

                } else if (flags.contains("+")) {
                    prefix = "+";
                }
            }

            result = value.base(10, false);

        } else if (specifier == "b") {
            if (flags.contains("#")) {
                prefix = "0b";
            }

            result = this.base(2, false);

        } else if (specifier == "o") {
            if ((flags.contains("#")) && (value != 0)) {
                prefix = "0";
            }

            result = this.base(8, false);

        } else if (specifier == "x") {
            if ((flags.contains("#")) && (value != 0)) {
                prefix = "0x";
            }

            result = this.base(16, false);

        } else if (specifier == "X") {
            if ((flags.contains("#")) && (value != 0)) {
                prefix = "0x";
            }

            result = this.base(16, true);

        } else {
            var errorDescription = "Invalid specifier \"" + specifier +
                                   "\" for Int";

            Core.raise(FormatError(errorDescription));
        }

        var length = prefix.length() + result.length();
        if (length < width) {
            length = width - length;
            if (flags.contains("0")) {
                result = prefix + ("0" * length) + result;
                prefix += "0" * (width - length);

            } else {
                if (flags.contains("-")) {
                    result = prefix + result + (" " * length);

                } else {
                    result = (" " * length) + prefix + result;
                }
            }

        } else {
            result = prefix + result;
        }

        return result;
    }
}

class String {
    function __format(flags, width, precision, specifier) {
        if ((specifier != "s") && (specifier != "r")) {
            var errorDescription = "Invalid specifier \"" + specifier +
                                   "\" for String";

            Core.raise(FormatError(errorDescription));
        }

        var length = this.length();
        var result = this;
        if (length < width) {
            var padding = " " * (width - length);
            if (flags.contains("-")) {
                result = this + padding;

            } else {
                result = padding + this;
            }
        }

        length = result.length();
        if ((precision > 0) && (length > precision)) {
            result = result[0..precision];
        }

        return result;
    }

    function __mod(arguments) {
        var argumentCount = 1;
        var argumentIndex = 0;
        var format = this;
        var result = "";

        if (arguments is List) {
            argumentCount = arguments.length();

        } else {
            arguments = [arguments];
        }

        while (1) {
            var index = format.indexOf("%");
            if (index == -1) {
                result += format;
                break;
            }

            result += format[0..index];
            index += 1;
            var flags = "";
            var character = format[index];
            while ((character == "-") || (character == "+") ||
                   (character == " ") || (character == "0") ||
                   (character == "#")) {

                flags += character;
                index += 1;
                character = format[index];
            }

            var width = 0;
            var zero = "0".byteAt(0);
            var nine = "9".byteAt(0);
            if (character == "*") {
                if (argumentIndex >= argumentCount) {
                    Core.raise(FormatError("Not enough arguments for format"));
                }

                width = arguments[argumentIndex];
                argumentIndex += 1;
                index += 1;
                character = format[index];

            } else {
                while ((character.byteAt(0) >= zero) &&
                       (character.byteAt(0) <= nine)) {

                    width = (width * 10) + (character.byteAt(0) - zero);
                    index += 1;
                    character = format[index];
                }
            }

            var precision = -1;
            if (character == ".") {
                index += 1;
                character = format[index];
                precision = 0;
                while ((character.byteAt(0) >= zero) &&
                       (character.byteAt(0) <= nine)) {

                    precision = (precision * 10) + (character.byteAt(0) - zero);
                    index += 1;
                    character = format[index];
                }
            }

            var specifier = character;
            index += 1;
            if (index == format.length()) {
                format = "";

            } else {
                format = format[index...-1];
            }

            if (specifier == "%") {
                result += "%";

            } else {
                if (argumentIndex >= argumentCount) {
                    Core.raise(FormatError("Not enough arguments for format"));
                }

                var object = arguments[argumentIndex];

                //
                // If the object doesn't implement format, convert it to a
                // string and then run the standard string format.
                //

                if (!object.implements("__format", 4)) {
                    if (specifier == "r") {
                        object = object.__repr();
                        specifier = "s";

                    } else if (specifier == "s") {
                        object = object.__str();

                    } else {
                        var error = "Specifier is " + specifier +
                                    " and object does not respond to __format";

                        Core.raise(FormatError(error));
                    }
                }

                result += object.__format(flags, width, precision, specifier);
                argumentIndex += 1;
            }
        }

        if (argumentIndex != argumentCount) {
            Core.raise(FormatError("Too few arguments for format"));
        }

        return result;
    }

    function __lt(right) {
        return this.compare(right.__str()) < 0;
    }

    function __le(right) {
        return this.compare(right.__str()) <= 0;
    }

    function __ge(right) {
        return this.compare(right.__str()) >= 0;
    }

    function __gt(right) {
        return this.compare(right.__str()) > 0;
    }

    function join(iterable) {
        if (iterable is List) {
            return this.joinList(iterable);
        }

        var realList = [];
        for (item in iterable) {
            realList.append(item);
        }

        return this.joinList(realList);
    }

    function template(substitutions, safe) {
        var character;
        var bracket;
        var end;
        var index;
        var input = this;
        var length;
        var key;
        var result = "";
        var start;
        var value;

        while (1) {

            //
            // Get a dollar sign. If none is present, add the remainder of the
            // string to the result and bail.
            //

            index = input.indexOf("$");
            if (index == -1) {
                result += input;
                break;
            }

            result += input[0..index];
            bracket = false;
            start = index;
            index += 1;

            //
            // $$ translates to a literal $.
            //

            character = input[index];
            if (character == "$") {
                result += "$";
                index += 1;
                input = input[index...-1];
                continue;

            //
            // Remember and advance past an opening {.
            //

            } else if (character == "{") {
                index += 1;
                bracket = true;
            }

            //
            // Advance past the name, which can be A-Z, a-z, 0-9, and _.
            //

            end = index;
            length = input.length();
            while (end < length) {
                character = input[end];
                if (((character >= "A") && (character <= "Z")) ||
                    ((character >= "a") && (character <= "z")) ||
                    ((character >= "0") && (character <= "9")) ||
                    (character == "_")) {

                    end += 1;

                } else {
                    break;
                }
            }

            //
            // Fail an empty variable name.
            //

            key = input[index..end];
            if (key == "") {
                Core.raise(ValueError(
                       "Invalid template expression at character %d" % start));
            }

            //
            // Check for a closing bracket if there was an opening one.
            //

            if (bracket) {
                if ((end >= input.length()) ||
                    (input[end] != "}")) {

                    Core.raise(ValueError(
                        "Expected '}' for template expression at character %d" %
                        start));
                }

                end += 1;
            }

            //
            // In safe mode, if the key doesn't exist, just pass the
            // substitution along to the output. In non-safe mode, raise a
            // KeyError.
            //

            if (safe) {
                try {
                    value = substitutions[key].__str();

                } except KeyError {
                    value = input[start..end];
                }

            } else {
                value = substitutions[key].__str();
            }

            result += value;
            input = input[end...-1];
        }

        return result;
    }
}

class Function {}
class Dict {}
class Range {}
class Module {}

//
// --------------------------------------------------------- Internal Functions
//

