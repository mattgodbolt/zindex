default: all

build/Makefile: CMakeLists.txt
	mkdir -p build
	cd build && cmake ..

all: build/Makefile
	$(MAKE) -C build all

test: build/Makefile all
	$(MAKE) -C build test

clean:
	rm -rf build
