
# SameSameC (ButDifferent)

Yet Another ANSI C89 Like Language Cross Compiler Targeting 8-bit CPUs. Written by 2021 Ville Helin.

**This is currently under early development, use it on your own risk.** SameSameC is GPL v2 software. Read the LICENSE file for more information. Some pieces of code were taken from WLA DX (https://github.com/vhelin/wla-dx), but not so many. The compiler produces WLA DX ASM files which the linker links together and assembles with WLA DX (included as Git submodule).

**I will not in general approve pull requests as I want to do this myself, up to the point when I decide I've learned enough. :) Ideas, bug reports and feature requests are welcome, though. And build scripts for platforms currently not supported.**


# Azure Pipelines CI

Linux: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Linux?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=4&branchName=master)
Windows: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Windows?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=5&branchName=master)


# Features

## Features missing when compared with ANSI C89

- No const strings. Will be implemented later.
- No pointer arithmetics like
    *ptr = 1;
  Instead do
    ptr[0] = 1;
- No syntactic sugar for multidimensional arrays. Perhaps implemented later. Meanwhile create your own using a one dimensional array.
- No floats or doubles or typedefs. All you have are u8 (8-bit unsigned int), s8 (8-bit signed int), u16 (16-bit unsigned int), s16 (16-bit signed int) and structs/unions.
- No casting. Perhaps implemented later. In calculations all involved will be automatically casted to the highest type in the calculation.
- No free floating blocks inside blocks

## Features not in ANSI C89

- No need to define a function or a global variable before referencing it unless the function or variable is in another source file.
- Binary values can be defined with the prefix 0b (e.g., 0b10001101)
- One line comments with //
- Local variables can be defined anywhere, not just at the begining of a block

## Bonuses

- Write WLA DX ASM using __asm()
- Include binary files using __incbin()
- The "object file" is actually plain WLA DX ASM


# TODO

## Optimize

- Optimize user written calculations like 2+a-1 -> a+2-1 -> a+1
- If a variable is only written to, remove the variable and all assignments
- Local array variable initialization
- Reduce code bloat (pass_2.c)
- Z80: Keep stack frame in IX and possibly IY all the time
- Z80: Remove unnecessary stack writes/reads (i.e., temp "register" access)

## Fix

- Currently "data[i++] += 1;" increments i twice...
- Check that all pointer operations work properly
- When defining an array sometimes [] takes only an integer and not an expression
- When defining an array of pointers to structs/unions it's possible to give too many items

## Add

- Add support for function overloading
- Add support for free const strings
- Add more backends (GB-Z80, 6502, 65816...)
- Add more test projects
- Document all TREE_NODE_TYPE_* use cases
- Document all TAC_OP_* use cases
- Add support for explicit casting and type checks
- Add support for arrays with more than one dimension
- Create a list of supported ANSI C89 features (and the new features)


# Releases

No releases yet


# NOTE

Programmed using

- "Basics of Compiler Design"
  - http://web.archive.org/web/20120915222417/http://www.diku.dk/hjemmesider/ansatte/torbenm/Basics/basics_lulu2.pdf
- Emacs
- Cygwin under Windows 10

Should compile anywhere ANSI C89 source code can be compiled.
