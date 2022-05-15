#!/bin/bash

#############################################################################
# COMPILER
#############################################################################

cd compiler
make -f makefile.gcc clean
make -f makefile.gcc
cd ..

#############################################################################
# LINKER
#############################################################################

cd linker
make -f makefile-gcc clean
make -f makefile.gcc
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

./update_wla_dx.sh

cd wla-dx
cmake -G "Unix Makefiles"
make
cd ..
