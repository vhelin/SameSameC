#!/bin/bash

set -e

REPO_ROOT="$PWD"
FAILURE_ARTIFACT_DIR="${SAMESAMEC_FAILURE_ARTIFACT_DIR:-$REPO_ROOT/_ci_test_failure}"
MAKE_POSIX_SHELL=$(command -v bash)

# Azure Windows uses MinGW make. Even with SHELL=bash it still CreateProcess's
# "simple" recipes, and `!` is not a MinGW shell metacharacter, so `! grep`
# is treated as a program named `!`. Test makefiles therefore start those
# lines with `: && ! grep` (`:` is a unixy builtin, which forces the shell).
# Do not use .ONESHELL: huge recipes then exceed Windows' CreateProcess limit.
# Use the full Git Bash path so CreateProcess does not pick WSL bash.exe.
case "$(uname -s)" in
    MINGW*|MSYS*)
        if command -v cygpath >/dev/null 2>&1; then
            MAKE_POSIX_SHELL=$(cygpath -m "$MAKE_POSIX_SHELL")
        fi
        case "$MAKE_POSIX_SHELL" in
            *.exe|*.EXE) ;;
            *) MAKE_POSIX_SHELL="${MAKE_POSIX_SHELL}.exe" ;;
        esac
        ;;
esac

_run_make() {
    make SHELL="$MAKE_POSIX_SHELL" "$@"
}

_save_test_failure_artifacts() {
    local dest="$FAILURE_ARTIFACT_DIR/$1"
    local f

    mkdir -p "$dest"

    for f in main.ssc makefile *.sms *.sym; do
        if [ -e "$f" ]; then
            cp -f "$f" "$dest/"
        fi
    done

    # *.asm is gitignored; Azure artifact publish may skip those names.
    if [ -f main.asm ]; then
        cp -f main.asm "$dest/main.compiler.asm.txt"
    fi
    if [ -f linked.sms.combined.asm ]; then
        cp -f linked.sms.combined.asm "$dest/linked.sms.combined.asm.txt"
    fi

    if [ -f linked.sms ]; then
        if command -v xxd >/dev/null 2>&1; then
            xxd -l 256 linked.sms > "$dest/linked.sms.head.xxd" || true
            xxd -s 128 -l 128 linked.sms > "$dest/linked.sms.0x80-0xFF.xxd" || true
        elif command -v od >/dev/null 2>&1; then
            od -An -tx1 -N 256 linked.sms > "$dest/linked.sms.head.hex" || true
            od -An -tx1 -j 128 -N 128 linked.sms > "$dest/linked.sms.0x80-0xFF.hex" || true
        fi
    fi

    if [ -f linked.sms.combined.asm ]; then
        grep -n -E '\.SECTION|copy_bytes_bank_|global_variables_|mainmain:' linked.sms.combined.asm > "$dest/sections-and-helpers.txt" || true
    fi

    if [ -f linked.sym ]; then
        grep -E 'copy_bytes|_init|mainmain|allocatorRa|^\[labels\]|^\[sections\]' linked.sym > "$dest/labels-of-interest.txt" || true
        awk '
            /^\[labels\]/ { p=1 }
            /^\[sections\]/ { p=1 }
            /^\[/ && $0 != "[labels]" && $0 != "[sections]" { p=0 }
            p { print }
        ' linked.sym > "$dest/labels-and-sections.txt" || true
    fi

    {
        echo "test=$1"
        date
        uname -a 2>/dev/null || true
        echo "samesamecc=$(command -v samesamecc 2>/dev/null || true)"
        echo "samesamecl=$(command -v samesamecl 2>/dev/null || true)"
        echo "wla-z80=$(command -v wla-z80 2>/dev/null || true)"
        echo "wlalink=$(command -v wlalink 2>/dev/null || true)"
        echo "byte_tester=$(command -v byte_tester 2>/dev/null || true)"
    } > "$dest/environment.txt"

    echo "Saved failure artifacts to $dest"
}

runTest() {
    set -e
    cd "$1"
    _run_make clean
    set +e
    _run_make
    status=$?
    set -e
    if [ "$status" -ne 0 ]; then
        _save_test_failure_artifacts "$2"
        cd ..
        return "$status"
    fi
    _run_make clean
    rm -f -- *.sym *.combined.asm
    cd ..
    return 0
}

if [ $# -eq 1 ]; then
    if [ "$1" = "-windows" ]; then
        export PATH="$PWD/windows/Release:$PWD/wla-dx/windows/Release:$PWD/build/binaries/Release:$PWD/build/binaries:$PWD/wla-dx/build/binaries/Release:$PWD/wla-dx/build/binaries:$PATH"
    elif [ "$1" = "-windows-x86" ]; then
        export PATH="$PWD/build-xp/binaries:$PWD/wla-dx/build-xp/binaries:$PWD/binaries:$PWD/wla-dx/binaries:$PATH"
    else
        export PATH="$PWD/binaries:$PWD/build/binaries:$PWD/wla-dx/binaries:$PWD/wla-dx/build/binaries:$PATH"
    fi
else
    export PATH="$PWD/binaries:$PWD/build/binaries:$PWD/wla-dx/binaries:$PWD/wla-dx/build/binaries:$PATH"
fi

# WLA DX CMake writes byte_tester next to its sources, not into binaries/.
# Allocator tests invoke it by name, so those directories must be on PATH.
export PATH="$PWD/wla-dx/byte_tester:$PWD/wla-dx/build/byte_tester:$PWD/wla-dx/build/byte_tester/Debug:$PWD/wla-dx/build/byte_tester/Release:$PWD/wla-dx/build-xp/byte_tester:$PATH"

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
if [ -n "$NO_VALGRIND" ]; then
  echo
  echo '########################################################################'
  echo 'INFO: Valgrind is disabled via NO_VALGRIND environment variable...'
  echo '########################################################################'
  export SAMESAMECVALGRIND=
elif ! [ -x "$(command -v valgrind)" ]; then
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
echo "make=$(command -v make) SHELL=$MAKE_POSIX_SHELL"
make --version | head -n 1
cd tests

for CPU in */; do
  cd $CPU
    for SYSTEM in */; do
      cd $SYSTEM
        for TEST in */; do
          OUT=$(runTest "$TEST" "$CPU$SYSTEM$TEST" 2>&1)
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
