# Chalk

Chalk is a small embeddable scripting language. It contains a compiler which
produces byte-code, and a minimal stack-based VM interpreter. Notable features
include:
 * C-like syntax
 * Built-in support for dictionaries, lists, and printf-like string formatting.
 * No build or runtime dependencies outside of the Minoca OS tree.
 * Simple bindings to C.
 * Single-threaded, but supports multiple "fibers" for cooperative scheduling.
 * Basic I/O, socket, and http libraries.
 * Reasonably fast
 * Easily embeddable in other applications, minimal C library requirements.

Chalk was made because I found myself wanting to program certain things in a
higher level scripting language like Python, but was disappointed by the
build dependency graph that gets pulled in. At the time Python (2) did not
cross compile, which meant if I wanted to utilize Python natively from the
Minoca build and test system it was a manual multistep process: cross-compile
enough to run Minoca, then build enough within Minoca to get Python working,
then finally bring the automation online. Sequencing this and managing the
manual steps became enough of a headache that I decided I needed an in-tree
scripting language.

Lua fit many of my requirements, but I found the syntax unappealing. With such
a simple VM architecture, I also have designs on integrating the interpreter
into the kernel for running safe functions from usermode within the kernel.
No such plans have been implemented, but it was one of the reasons for
implementing a minimal VM.

## Syntax

Syntax is meant to be intuitive, and is best illustrated with an example:
```
// C++ style comments work
/* And so do
 * C-style
 */

// Import statements pull in other modules, either written in Chalk or C.
from app import argv;
from io import open;
from iobase import IoError;

// Basic classes with single inheritance are supported.
class MyFileReader {
    // private member variables
    var _lines;

    // Constructors, depending on number of parameters passed.
    function __init(path) {
        // Unininitialized local variables are Null.
        var file;

        // Exceptions look a lot like Python.
        try {
            file = open(path, "rb");

        } except IoError as e {
            // Basic functions are builtin to the Core module.
            Core.print("Couldn't open path %s" % path);
            Core.raise(e);
        }

        try {
            _lines = _file.readlines(-1);

        } finally {
            file.close();
        }

        // Each object also has a dictionary. Dictionary elements can be set
        // with the dot operator.
        this.lineCount = _lines.length();
        return this;
    }

    function getLine(lineNumber) {
        if (lineNumber >= this.lineCount) {
            // Python single quoted, double quoted, and triple quoted strings
            // are supported.
            return "";
        }

        // Lists are indexed by [].
        return _lines[lineNumber];
    }
}

function categorize(path) {
    var aCount;
    var firstChar;
    var reader = MyFileReader(path);
    var linesByLetter = {};

    // Iterators can take the traditional C-like syntax:
    // for(init; condition; iterator)
    // or the more high level style shown below.
    // Ranges can be exclusive on the end like below, or inclusive on the
    // end with 0...N.
    for (index in 0..reader.lineCount) {
        line = reader.getLine(index);
        if (line.length() == 0) {
            continue;
        }

        // Dictionaries can also be set with square brackets, and can test
        // for being empty with get().
        firstChar = line.charAt(0);
        if (linesByLetter.get(firstChar) == null) {
            linesByLetter[firstChar] = [];
        }

        linesByLetter[firstChar].append(line);
    }

    // Ok, the example's a little dumb.
    aCount = linesByLetter('a');
    if (aCount == null) {
        aCount = 0;
    }

    return aCount;
}

// Code can run at the module level, whenever the module is loaded.
// The app module gives us access to command line arguments.
if (argv.length() != 2) {
    Core.raise(ValueError("Please specify an argument!));
}

categorize(argv[1]);

```

## Things using Chalk
 * Mingen - Our build tool in Minoca that generates Makefiles or Ninja files.
 * Msetup - Our OS installer uses a little bit of embedded Chalk to define
 board install recipes.
 * Santa - The Minoca package manager, build client and server, and CI
 infrastructure.

## Development

### Internal Compoments
This ck directory is divided into a few different components:
 * `lib` - This directory contains libchalk, a dynamic library containing the
 heart of the Chalk core.
   * Files that start with `comp` implement the bytecode compiler.
   * `vm.c` implements the core runtime VM.
   * `vmsys.c` and `dlopen.c` contain the bindings needed to use the VM with
   the standard C library.
   * The grammar is a generated C file called `gram.c`. It is created by the
   `gramgen.c` program.
   * `core.c`, `dict.c`, `list.c`, `int.c`, and `value.c` implement the core
   classes and objects built-in to chalk.
   * `capi.c` implements the language bindings to C code.
   * `gc.c` contains the garbage collector.
 * `app` - This directory contains the outer shell for the interactive Chalk
 interpreter. It's mostly a line-reader that feeds into libchalk.
 * `modules` - This directory contains the Chalk native modules, both those
 written in Chalk, and dynamic C modules. Chalk modules are loaded when imported
 for the first time.

### Embedding Chalk
If you want to integrate chalk into your standard C application, just link
against libchalk.so, and include `minoca/lib/chalk.h`. The main entry points
for running Chalk code are `CkCreateVm()` and `CkInterpret()`. See `app/chalk.c`
for how the interactive interpreter does this. With the `CK_VM` object, you can
create multiple isolated Chalk instances.

If you want to do a deeper Chalk integration, and say don't have full C library
support, you can implement the VM bindings in `CK_CONFIGURATION`, allowing you
to cut nearly all of the C library dependencies.

### Wishlist
Things that haven't been done yet but would be nice:
 * Concurrency - Chalk currently runs units of execution called Fibers in
 a cooperative fashion: fibers yield to each other, and only one Fiber runs at
 a time. It would be nice to get multithreading going, but there is no
 multithreading protection for the core objects, and adding a Big Interpreter
 Lock just seems dumb.
 * Signals - Chalk currently has no support for Unix signals that come in.
 * Refactoring module calls and __get(). There's an awkward syntax wart where
 if you want to do a __get() on something and then call the result, you have to
 use parentheses. For instance, if you've done an `import os;`, you'd have to
 do `(os.open)(somePath)`, because `open` is an object inside the module `os`,
 not a method. This is because the syntax for a method call is
 `identifier.identifier(...)`, and the syntax for a __get is
 `identifier.identifier`. It would be nice to clean this up where method calls
 are still just as fast, but are the same as `object.__get("methodName")()`.
 I guess we need "bound functions" for this, functions that are bound to a
 specific instance.
 * Metaprogramming - Being able to add methods to objects programmatically, and
 an `eval` function.