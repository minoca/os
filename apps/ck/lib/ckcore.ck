/*++

Copyright (c) 2016 Minoca Corp. All Rights Reserved

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
            result += element.__str();
        }

        result += end;
        return result;
    }
}

class Exception {
    function __init(args) {
        this.args = args;
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
        if (specifier != "s") {
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
        var argumentCount = arguments.length();
        var argumentIndex = 0;
        var format = this;
        var result = "";

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
                if (!object.implements("__format", 4)) {
                    object = object.__str();
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
}

class Function {}
class Dict {}
class Range {}
class Module {}

//
// --------------------------------------------------------- Internal Functions
//

