cmake_minimum_required(VERSION 2.8.4)
project(zindex)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall -Werror")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

option(UseLTO "Use link-time optimization" OFF)
option(Static "Statically link" OFF)
option(BuildSqlShell "Build the sqlite shell" OFF)

if(UseLTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-linker-plugin -fuse-ld=gold")
endif(UseLTO)

if(Static)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSQLITE_OMIT_LOAD_EXTENSION=1")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBRARIES OFF)
endif(Static)

find_package(ZLIB REQUIRED)
include_directories(BEFORE SYSTEM ext/sqlite)
include_directories(${ZLIB_INCLUDE_DIRS} src ext)

set(SOURCE_FILES
    src/File.h
    src/Index.cpp
    src/Index.h
    src/LineFinder.cpp
    src/LineFinder.h
    src/LineSink.h
    src/Sqlite.cpp
    src/Sqlite.h
    src/SqliteError.h
    src/RegExp.h
    src/RegExp.cpp
    src/RegExpIndexer.cpp
    src/RegExpIndexer.h
    src/LineIndexer.h
    src/IndexSink.h
    src/Log.h
    src/ConsoleLog.h
    src/ConsoleLog.cpp
    src/StringView.cpp
    src/StringView.h
    src/PrettyBytes.h
    src/PrettyBytes.cpp
    ext/sqlite/sqlite3.c)

set(TEST_FILES
    tests/catch.hpp
    tests/LineFinderTest.cpp
    tests/test_main.cpp
    tests/SqliteTest.cpp 
    tests/RegExpTest.cpp
    tests/RegExpIndexerTest.cpp
    tests/TempDir.h
    tests/TempDir.cpp
    tests/PrettyBytesTest.cpp
    tests/IndexTest.cpp
    tests/LogTest.cpp)

add_library(libzindex ${SOURCE_FILES})
set_target_properties(libzindex PROPERTIES OUTPUT_NAME zindex)

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES} dl)
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES} dl)

add_executable(unit-tests ${TEST_FILES} )
target_link_libraries(unit-tests libzindex ${ZLIB_LIBRARIES} dl)

if(BuildSqlShell)
    add_executable(sql-shell ext/sqlite/shell.c ext/sqlite/sqlite3.c)
    target_link_libraries(sql-shell dl)
endif(BuildSqlShell)

enable_testing()
add_test(NAME unit-tests
         COMMAND unit-tests)
