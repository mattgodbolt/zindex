cmake_minimum_required(VERSION 2.8.4)
project(zindex)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS} ext src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES
    src/File.h
    src/Index.cpp
    src/Index.h
    src/KeyValue.h
    src/LineIndexer.cpp
    src/LineIndexer.h
    src/LineSink.h
    src/Sqlite.cpp src/Sqlite.h
    ext/sqlite/sqlite3.c)

set(TEST_FILES
    tests/catch.hpp
    tests/LineIndexerTest.cpp
    tests/test_main.cpp)

add_library(libzindex ${SOURCE_FILES} src/SqliteError.h)

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES} dl)
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES} dl)

add_executable(unit-tests ${TEST_FILES} tests/SqliteTest.cpp)
target_link_libraries(unit-tests libzindex ${ZLIB_LIBRARIES} dl)

enable_testing()
add_test(NAME unit-tests
         COMMAND unit-tests)
