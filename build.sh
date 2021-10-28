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
