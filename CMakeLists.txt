cmake_minimum_required(VERSION 2.8.4)
project(zindex)

find_package(ZLIB REQUIRED)
include_directories(BEFORE SYSTEM ext/sqlite)
include_directories(${ZLIB_INCLUDE_DIRS} src ext)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

set(SOURCE_FILES
    src/File.h
    src/Index.cpp
    src/Index.h
    src/KeyValue.h
    src/LineIndexer.cpp
    src/LineIndexer.h
    src/LineSink.h
    src/Sqlite.cpp
    src/Sqlite.h
    src/SqliteError.h
    src/RegExp.h
    src/RegExp.cpp
    ext/sqlite/sqlite3.c)

set(TEST_FILES
    tests/catch.hpp
    tests/LineIndexerTest.cpp
    tests/test_main.cpp
    tests/SqliteTest.cpp
    tests/RegExpTest.cpp)

add_library(libzindex ${SOURCE_FILES})

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES} dl)
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES} dl)

add_executable(unit-tests ${TEST_FILES})
target_link_libraries(unit-tests libzindex ${ZLIB_LIBRARIES} dl)

add_executable(sql-shell ext/sqlite/shell.c ext/sqlite/sqlite3.c)
target_link_libraries(sql-shell dl)

enable_testing()
add_test(NAME unit-tests
         COMMAND unit-tests)
