#!/usr/bin/env bash

HELP="Usage: ./compile.sh MAIN [FILES] [FLAGS]

Mandatory parameters:
  MAIN      Input file containing main()
  FILES     List of additional input files (separated by white spaces)
  FLAGS     Additional flags (separated by white spaces)
"

if (( $# < 1 )); then
  printf "$HELP"
  exit 1
fi

ARGS=$@
MAIN="${ARGS%% *}"
EXT="${MAIN##*.}"

MY_PATH="$(cd "$(dirname "$0")" ; pwd -P)"
LLVM_PROJECT="$MY_PATH/../llvm-project"
LLVM_BIN_PATH="$LLVM_PROJECT/build/bin"
LLVM_LIB_PATH="$LLVM_PROJECT/build/lib"
COMPILER_RT_LIB_PATH="$LLVM_LIB_PATH/clang/12.0.1/lib/linux"
CC="$LLVM_BIN_PATH/clang"
CXX="$LLVM_BIN_PATH/clang++"
LINK="$LLVM_BIN_PATH/llvm-link"
OPT="$LLVM_BIN_PATH/opt"
DIS="$LLVM_BIN_PATH/llvm-dis"
CALL_LIB="$LLVM_LIB_PATH/LLVMSysCallMod.so"
RV_STORE="$COMPILER_RT_LIB_PATH/libclang_rt.rvstore-x86_64.so"
OUT_DIR="$MY_PATH/out"

if [ $EXT = "c" ]; then
  C=$CC
elif [ $EXT = "cpp" ]; then
  C=$CXX
else
  printf "$HELP"
  exit 1
fi

CFLAGS="-O0 -S -emit-llvm"
OFLAGS="-S -debug -load $CALL_LIB -detect-syscall"
LFLAGS="-g -O0 $RV_STORE -L $COMPILER_RT_LIB_PATH -Wl,-rpath,$COMPILER_RT_LIB_PATH"

BC="${MAIN%%.*}.ll"
OC="${MAIN%%.*}.o"
BIN="${MAIN%%.*}"

rm out/$BIN 2>/dev/null

# -include store.cpp

# Compile to bitcode
printf "\n__INFO__ Compile to bitcode, Executing: $CXX $CFLAGS -c $ARGS -o $BC"
$C $CFLAGS -c $ARGS -o $BC

# Run SysCallMod pass
if [ $? -eq 0 ]; then
  printf "\n\n__INFO__ Run SysCallMod pass, Executing: $OPT $OFLAGS $BC -o $BC"
  $OPT $OFLAGS $BC -o $BC

  # Compile to binary and link
  if [ $? -eq 0 ]; then
    printf "\n__INFO__ Compile to object file, Executing: $C $LFLAGS $BC -o $BIN"
    $C $LFLAGS $BC -o $BIN

    mv $BC "${OUT_DIR}/${MAIN%%.*}.ll"
    mv $BIN $OUT_DIR
    # Run binary
    if [ $? -eq 0 ]; then
      printf "\n__INFO__ Run binary, Executing: $OUT_DIR/$BIN"
      $OUT_DIR/$BIN
    fi
  fi
fi

#rm $BC 2>/dev/null
