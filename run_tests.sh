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

export PATH=$PATH:$PWD/binaries
export PATH=$PATH:$PWD/wla-dx/binaries

set +e

TEST_COUNT=0

SHOW_ALL_OUTPUT=false
if [ $# -eq 1 ]; then
    if [ "$1" = "-v" ]; then
        SHOW_ALL_OUTPUT=true
    fi
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
