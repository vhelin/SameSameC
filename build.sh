#!/bin/bash

#############################################################################
# COMPILER
#############################################################################

cd compiler
make clean
make
cd ..

#############################################################################
# LINKER
#############################################################################

cd linker
make clean
make
cd ..

#############################################################################
# copy the executables...
#############################################################################

mkdir binaries
cp compiler/samesamecc binaries/
cp linker/samesamecl binaries/

#############################################################################
# WLA DX
#############################################################################

cd wla-dx
git submodule init
git submodule update
git fetch
git merge
cmake -G "Unix Makefiles"
make
cd ..