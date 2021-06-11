# Minimal LISP Compiler

This compiler translates a minimal set of LISP primitives to x86_64 assembly code.
It requires a assembler that accepts intel syntax to create a executable.

Any compiled program requires a runtime, containing primitive calls, heap initialization etc.
Currently only a x86_64 runtime is available.

The set of supported LISP primitives are enough to provide the compiler source in LISP itself.

## Build Instructions

First create the bootstrap compiler.
```
cc compiler.c -o bootstrap
```

Now compile the LISP source.
```
cat compiler.lisp | ./bootstrap > output.S
```

Use the provided runtime to create a working binary.
```
cc -static -nostartfiles -nodefaultlibs -masm=intel output.S runtime.S -o LISPC
```
Repeat these steps everytime when compiling LISP source files.

## Customizations

The supported subset of LISP matches the primitives from Paul Graham's "The Roots of Lisp".
This subset itself is only enough for self-interpretation but it provides no interaction with the OS.
To fix this the runtime introduces 2 additional primitives to make a self-hosted code generator possible:
- **read**: Reads an S-Expression and transforms to an internal representation for the compiler. Whitespaces were accepted as valid token for symbols if escaped(e.g. '\ ') except of '\n' which will be replaced with the ASCII newline(0xA).
- **print**: Prints the given object according to its representation. It doesn't create a newline at the end and returns always an empty list.

The LISP compiler and it's runtime have no external dependencies except the entry point ____start___ and the syscalls **read** and **write** on Linux.
For bare-metal platforms, only these 3 parts of the runtime need to be rewritten.

## Related sources
[The Roots of Lisp](http://www.paulgraham.com/rootsoflisp.html) - Minimal primitives to eval LISP  
[A. Carl Douglas' Micro Lisp](https://github.com/carld/micro-lisp) - Inspiration for the parser and source evaluation of the compiler
