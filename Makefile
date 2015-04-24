default: all

BUILD_TYPE ?= Release
BUILD_DIR := build/${BUILD_TYPE}
ROOT_DIR := $(shell pwd)

${BUILD_DIR}/Makefile: CMakeLists.txt
	mkdir -p ${BUILD_DIR}
	cd ${BUILD_DIR} && cmake ${ROOT_DIR} -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

all: ${BUILD_DIR}/Makefile
	$(MAKE) -C ${BUILD_DIR} all

test: ${BUILD_DIR}/Makefile all
	$(MAKE) -C ${BUILD_DIR} test

clean:
	rm -rf build
