#!/usr/bin/env bash

HELP="Usage: ./compile.sh MAIN [FILES] [FLAGS]

Mandatory parameters:
  MAIN      Input file containing main()
  FILES     List of additional input files (separated by white spaces)
  FLAGS     Additional flags (separated by white spaces)
"

if (( $# < 1 )); then
  echo -e "$HELP"
  exit 1
fi

ARGS=$@
MAIN="${ARGS%% *}"
EXT="${MAIN##*.}"

MY_PATH="$(cd "$(dirname "$0")" ; pwd -P)"
LLVM_BIN_PATH="$MY_PATH/../llvm-project/build/bin"
LLVM_LIB_PATH="$MY_PATH/../llvm-project/build/lib"
COMPILER_RT_LIB_PATH="$MY_PATH/../llvm-project/build-compiler-rt/lib/linux"
CC="$LLVM_BIN_PATH/clang"
CXX="$LLVM_BIN_PATH/clang++"
OPT="$LLVM_BIN_PATH/opt"
DIS="$LLVM_BIN_PATH/llvm-dis"
MOD_LIB="$LLVM_LIB_PATH/LLVMSysCallRepl.so"
OUT_DIR="$MY_PATH/out"

if [ $EXT = "c" ]; then
  C=$CC
elif [ $EXT = "cpp" ]; then
  C=$CXX
else
  echo -e "$HELP"
  exit 1
fi

CFLAGS="-O0 -S -emit-llvm"
MFLAGS="-S -debug -load $MOD_LIB -replace-syscall"
LFLAGS="-O0"

BC="${MAIN%%.*}.ll"
BIR="${MAIN%%.*}_repl.ir"
BIN="${MAIN%%.*}_repl"

rm $BIN 2>/dev/null
# Compile to bitcode
echo "__INFO__ Compile to bitcode, Executing: $C $CFLAGS -c $ARGS -o $BC"
$C $CFLAGS -c $ARGS -o $BC
# Link bitcode files


# Run SysCallRepl pass
if [ $? -eq 0 ]; then
echo "__INFO__ Run SysCallRepl pass, Executing: $OPT $MFLAGS $BC -o $BC"
$OPT $MFLAGS $BC -o $BC

  # Compile to binary and link
  if [ $? -eq 0 ]; then
    echo "__INFO__ Compile to binary and link, Executing: $C $LFLAGS $BC -o $BIN"
    $C $LFLAGS $BC -o $BIN

    mv $BC "${OUT_DIR}/${MAIN%%.*}_repl.ll"
    mv $BIN $OUT_DIR
    # Run binary
    if [ $? -eq 0 ]; then
      echo "__INFO__ Run binary, Executing: $OUT_DIR/$BIN"
      $OUT_DIR/$BIN
    fi
  fi
fi

#rm $BC 2>/dev/null

