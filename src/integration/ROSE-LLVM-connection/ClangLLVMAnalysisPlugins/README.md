# Clang-LLVM-Plugins

## Description

This simply demostrates a Clang plugin and a LLVM plugin pass (without ROSE).

## Software dependency

- Clang/LLVM: 20.x.x

## Building ROSE plugin  & ROSE LLVM Pass

- mkdir build; cd build
- cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++  ../
- make

libRoseConnectionClangPlugin.so (Clang plugin) and libRoseConnectionLLVMPassPlugin.so (LLVM plugin pass) will be created in build directory.


## Running with Clang plugin

- with clang driver: clang -Xclang -load -Xclang libRoseConnectionClangPlugin.so  test.c
- Using the cc1 command line: clang -cc1 -load libRoseConnectionClangPlugin.so -plugin ROSE test.c

## Running with LLVM Pass plugin

- with clang river: clang -O0 -fpass-plugin=libRoseConnectionLLVMPassPlugin.so test.c
