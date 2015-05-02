cmake_minimum_required(VERSION 2.8.4)
project(zindex)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Werror -Wextra")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread -Wall -Werror -Wextra")
endif()

if (CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -Wall -Wextra")
endif()

option(UseLTO "Use link-time optimization" OFF)
option(Static "Statically link" OFF)
option(BuildSqlShell "Build the sqlite shell" OFF)
option(ArchNative "Target the computer being built on (march=native)" OFF)

if(UseLTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-linker-plugin")
endif(UseLTO)

set(COMMON_LIBS "")
if(Static)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSQLITE_OMIT_LOAD_EXTENSION=1")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBRARIES OFF)
    set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
else(Static)
    set(COMMON_LIBS "dl")
endif(Static)

if(ArchNative)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif(ArchNative)

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
    src/RangeFetcher.cpp
    src/RangeFetcher.h
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
    tests/RangeFetcherTest.cpp
    tests/LogTest.cpp)

add_library(libzindex ${SOURCE_FILES})
set_target_properties(libzindex PROPERTIES OUTPUT_NAME zindex)

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES} ${COMMON_LIBS})
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES} ${COMMON_LIBS})

add_executable(unit-tests ${TEST_FILES})
target_link_libraries(unit-tests libzindex ${ZLIB_LIBRARIES} ${COMMON_LIBS})

if(BuildSqlShell)
    add_executable(sql-shell ext/sqlite/shell.c ext/sqlite/sqlite3.c)
    target_link_libraries(sql-shell ${COMMON_LIBS})
endif(BuildSqlShell)

enable_testing()
add_test(NAME unit-tests
         COMMAND unit-tests)
