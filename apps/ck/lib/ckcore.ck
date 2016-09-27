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

class Fiber {}
class Null {}
class Int {}
class String {}
class Function {}
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

class Dict {}
class Range {}
class Core {}
class Module {}

class Exception {
    var args;
    var stackTrace;

    function __str() {
        var frame;
        var path;
        var result = this.type().name();
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

//
// --------------------------------------------------------- Internal Functions
//

