#!/bin/bash

set -ue

SCRIPT_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BUILD_DIR="${SCRIPT_ROOT}/build"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -G Ninja \
	-DCMAKE_BUILD_TYPE=Debug        \
	-DBUILD_SHARED_LIBS=On          \
	-DLLVM_TARGETS_TO_BUILD=host    \
	-DLLVM_BUILD_TOOLS=On           \
	-DLLVM_BUILD_TESTS=Off          \
	-DLLVM_BUILD_DOCS=Off           \
	-DLLVM_ENABLE_LTO=Off           \
	-DLLVM_ENABLE_DOXYGEN=Off       \
	-DLLVM_ENABLE_RTTI=Off          \
	-DLLVM_ENABLE_PROJECTS="compiler-rt;clang"  \
	"$SCRIPT_ROOT/llvm"
ninja
