#!/bin/bash

# Somewhat hacky approach to try building a full PGO/LTO binary
# Pass the root path to GCC as the first parameter

set -e

ROOT_DIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." > /dev/null && pwd )

if [[ $# -ne 1 ]] && [[ $# -ne 2 ]]; then
    echo "Usage: pgo.sh <path to gcc root> [<pgo exercise script>]"
    exit 1
fi

GCC_ROOT=$1
EXERCISE_SCRIPT=$2

export LIBRARY_PATH=/usr/lib/$(gcc -print-multiarch)
export C_INCLUDE_PATH=/usr/include/$(gcc -print-multiarch)
export CPLUS_INCLUDE_PATH=/usr/include/$(gcc -print-multiarch)
export CXX=${GCC_ROOT}/bin/g++
export CC=${GCC_ROOT}/bin/gcc
export CMAKE_AR=${GCC_ROOT}/bin/gcc-ar
export CMAKE_RANLIB=${GCC_ROOT}/bin/gcc-ranlib

cd ${ROOT_DIR}
mkdir -p build
cd build
mkdir -p pgo-gen
pushd pgo-gen
PGO_GEN=$(pwd)
cmake ${ROOT_DIR} -DStatic:BOOL=Yes -DUseLTO:BOOL=Yes -DCMAKE_AR=${CMAKE_AR} -DCMAKE_RANLIB=${CMAKE_RANLIB} -DArchNative:BOOL=yes -DCMAKE_BUILD_TYPE=Release -DPGO="-fprofile-generate=${PGO_GEN}" -DCMAKE_PREFIX_PATH=${ROOT_DIR}/build/zlib
make zq zindex
popd

pushd pgo-gen
if [[ -z "${EXERCISE_SCRIPT}" ]]; then
    echo "Using built-in exercise script"
    if [[ ! -e testfile.gz ]]; then
        echo "Creating a test file"
        seq 1000000 > testfile
        echo "Gzipping test file"
        gzip -f -9 testfile
    fi
    echo "Indexing test file"
    ./zindex --regex '([0-9]+)' testfile.gz
    echo "Querying test file"
    for i in $(seq 1 12345 1000000); do ./zq testfile.gz $i $((i + 100)) $((i + 200)); done
else
    echo "Using external exercise script"
    ${EXERCISE_SCRIPT}
fi
popd

echo "Making PGO executables"

mkdir -p pgo
cd pgo
cmake ${ROOT_DIR} -DStatic:BOOL=Yes -DUseLTO:BOOL=Yes -DCMAKE_AR=${CMAKE_AR} -DCMAKE_RANLIB=${CMAKE_RANLIB} -DArchNative:BOOL=yes -DCMAKE_BUILD_TYPE=Release -DPGO="-fprofile-use=${PGO_GEN}" -DCMAKE_PREFIX_PATH=${ROOT_DIR}/build/zlib
make zq zindex
echo "Done!"
