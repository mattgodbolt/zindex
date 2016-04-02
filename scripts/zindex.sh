#!/bin/bash

rm -rf zindex/build
GCC_ROOT=/home/mgodbolt/.fighome/runtime/gcc/5.2.0-1
export LIBRARY_PATH=/usr/lib/$(gcc -print-multiarch)
export C_INCLUDE_PATH=/usr/include/$(gcc -print-multiarch)
export CPLUS_INCLUDE_PATH=/usr/include/$(gcc -print-multiarch)
export CXX=${GCC_ROOT}/bin/g++
export CC=${GCC_ROOT}/bin/gcc
export CMAKE_AR=${GCC_ROOT}/bin/gcc-ar
export CMAKE_RANLIB=${GCC_ROOT}/bin/gcc-ranlib
mkdir -p zindex/build
cd zindex/build
/grid/opt/cmake-3.2.1/bin/cmake .. -DStatic:BOOL=Yes -DUseLTO:BOOL=Yes -DCMAKE_AR=${CMAKE_AR} -DCMAKE_RANLIB=${CMAKE_RANLIB} -DCMAKE_BUILD_TYPE=Release
make VERBOSE=1
make test VERBOSE=1
