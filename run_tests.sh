#!/bin/bash

set -e

runTest() {
    set -e
    cd $1
    make clean
    make
    make clean
    cd ..
}

if [ $# -eq 1 ]; then
    if [ "$1" = "-windows" ]; then
        export PATH=$PATH:$PWD/windows/Release
        export PATH=$PATH:$PWD/wla-dx/windows/Release
    else
        export PATH=$PATH:$PWD/binaries
        export PATH=$PATH:$PWD/wla-dx/binaries
    fi
else
    export PATH=$PATH:$PWD/binaries
    export PATH=$PATH:$PWD/wla-dx/binaries
fi

set +e

TEST_COUNT=0

SHOW_ALL_OUTPUT=false
if [ $# -eq 1 ]; then
    if [ "$1" = "-v" ]; then
        SHOW_ALL_OUTPUT=true
    fi
fi

# Valgrind test...
# Makefiles in the tests folder use SAMESAMECVALGRIND to run Valgrind at the same time
# with samesamecc and samesamecl
if ! [ -x "$(command -v valgrind)" ]; then
  echo
  echo '########################################################################'
  echo 'WARNING: Valgrind is not installed so we cannot perform memory checks...'
  echo '########################################################################'
  export SAMESAMECVALGRIND=
else
  export SAMESAMECVALGRIND='valgrind -s --error-exitcode=1 --tool=memcheck --leak-check=full --errors-for-leak-kinds=all'
fi

echo
echo Running tests...
cd tests

for CPU in */; do
  cd $CPU
    for SYSTEM in */; do
      cd $SYSTEM
        for TEST in */; do
          OUT=$(runTest $TEST 2>&1)
          if [ $? -ne 0 ]; then
            printf "\n\n%s\n\n" "$OUT"
            echo "########"
            echo FAILURE!
            echo "########"
            echo
            echo TEST \"$CPU$SYSTEM$TEST\" - FAILURE
            exit 1
          elif $SHOW_ALL_OUTPUT; then
            echo "#####################################################################"
            echo TEST \"$CPU$SYSTEM$TEST\" - SUCCESS
            echo "#####################################################################"
            printf "\n%s\n\n" "$OUT"
          else
            printf .
          fi
          TEST_COUNT=$((TEST_COUNT+1))
        done
      cd ..
    done
  cd ..
done

printf "\n\n"
echo "DONE ($TEST_COUNT tests)"
