
# SameSameC (ButDifferent)

Yet Another ANSI C89 Like Language Cross Compiler Targetting 8-bit CPUs. Written by 2021 Ville Helin.

This is currently under development, use it on your own risk. SameSameC is GPL v2 software. Read the LICENSE file for more information. Some pieces of code were taken from WLA DX (https://github.com/vhelin/wla-dx), but not so many.

I will not in general approve pull requests as I want to do this myself, up to the point when I decide I've learned enough. :) Ideas, bug reports and feature requests are welcome, though. And build scripts for platforms currently not supported.


# Azure Pipelines CI

Linux: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Linux?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=4&branchName=master)
Windows: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Windows?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=5&branchName=master)


# TODO

## Optimize

- Optimize user written calculations like 2+a-1 -> 1+a
- If a variable is only written to, remove the variable and all assignments
- Local array variable initialization
- Reduce code bloat (parser...)
- Z80: Keep stack frame in IX and possibly IY all the time
- Z80: Remove unnecessary stack writes/reads (i.e., temp "register" access)

## Fix

- Currently "data[i++] += 1;" increments i twice...
- Check that all pointer operations work properly
- When defining an array sometimes [] takes only an integer and not an expression
- When defining an array of pointers to structs/unions it's possible to give too many items

## Add

- Add support for empty statements
- Add support for function overloading
- Add support for free const strings
- Add support for explicit casting and type checks
- Add support for arrays with more than one dimension
- Add more backends (GB-Z80, 6502, 65816...)
- Add more test projects
- Improve struct/union support (ANSI C99?)
- Document all TREE_NODE_TYPE_* use cases
- Document all TAC_OP_* use cases
- Create a list of supported ANSI C89 features (and the new features)


# NOTE

Programmed using

- "Basics of Compiler Design"
  - http://web.archive.org/web/20120915222417/http://www.diku.dk/hjemmesider/ansatte/torbenm/Basics/basics_lulu2.pdf
- emacs
- Cygwin under Windows 10

Should compile anywhere ANSI C89 source code can be compiled.
