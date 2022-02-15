#!/bin/bash

#############################################################################
# WLA DX
#############################################################################

cd wla-dx
git submodule init
git submodule update
git fetch
git merge
git checkout master
git pull
cd ..
