SameSameC (ButDifferent)
------------------------

Yet Another ANSI C89 Like Language Cross Compiler For 8-bit CPUs. Written by 2021 Ville Helin.

This is currently under development, use it on your own risk. SameSameC is
GPL v2 software. Read the LICENSE file for more information. Some pieces of
code were taken from WLA DX (https://github.com/vhelin/wla-dx), but not
so many.

I will not approve pull requests as I want to do this myself. Up to the point
when I decide I've learned enough. :) Ideas, bug reports and feature requests
are welcome, though.


Azure Pipelines
---------------

Linux: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Linux?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=1&branchName=master)
Windows: [![Build Status](https://dev.azure.com/villehelin0486/villehelin/_apis/build/status/vhelin.SameSameC%20Windows?branchName=master)](https://dev.azure.com/villehelin0486/villehelin/_build/latest?definitionId=2&branchName=master)


TODO
----

Optimize:

- Optimize user written calculations like 2+a-1 -> 1+a
- If a variable is only written to, remove the variable and all assignments
- Z80: Keep stack frame in IX and possibly IY all the time
- Local array variable initialization
- Reduce code bloat (parser...)

Fix:

- Currently "data[i++] += 1;" increments i twice...

Add:

- Add support for struct (Work-In-Progress...)
- Add support for free const strings
- Add support for explicit casting and type checks
- Add support for arrays with more than one dimension
- Add more backends (GB-Z80, 6502, 65816...)
- Add scripts so it can be compiled on an Amiga (OS 2.0+) using SAS/C
- Create a Visual Studio solution file (for compiling on/for a Windows computer)
- Add test projects
- Document all TREE_NODE_TYPE_* use cases
- Create a list of supported ANSI C89 features (and the new features)


NOTE
----

Programmed using:

- "Basics of Compiler Design"
  - http://web.archive.org/web/20120915222417/http://www.diku.dk/hjemmesider/ansatte/torbenm/Basics/basics_lulu2.pdf
- beer
- sugar wine
- gin long drinks
- imagination
- emacs

Should compile anywhere ANSI C89 source code can be compiled.
