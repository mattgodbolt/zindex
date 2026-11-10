# 3.19 is what Catch2 v3 needs for catch_discover_tests.
cmake_minimum_required(VERSION 3.19)
project(zindex C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

if (APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

# Warnings apply to our own C++ code only, never to the vendored C sources. -Werror is opt-in so that new compilers' new warnings
# don't break builds for people just trying to install the tool.
option(WarningsAsErrors "Treat warnings in zindex's own code as errors" OFF)
add_library(zindex_warnings INTERFACE)
target_compile_options(zindex_warnings INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Wall -Wextra>)
if (WarningsAsErrors)
    target_compile_options(zindex_warnings INTERFACE $<$<COMPILE_LANGUAGE:CXX>:-Werror>)
endif ()

option(UseLTO "Use link-time optimization" OFF)
option(Static "Statically link" OFF)
option(BuildSqlShell "Build the sqlite shell" OFF)
option(ArchNative "Target the computer being built on (march=native)" OFF)
option(PGO "Set PGO flags" "")
option(Coverage "Enable coverage reporting" OFF)

if (Coverage)
    add_compile_options(--coverage -O0)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif (Coverage)

if (PGO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${PGO}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${PGO}")
endif (PGO)

if (UseLTO)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -flto -Wno-maybe-uninitialized")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto -Wno-maybe-uninitialized")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-linker-plugin")
endif (UseLTO)

set(COMMON_LIBS Threads::Threads)
if (Static)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    set(BUILD_SHARED_LIBRARIES OFF)
    set(CMAKE_EXE_LINK_DYNAMIC_C_FLAGS)       # remove -Wl,-Bdynamic
    set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
else (Static)
    list(APPEND COMMON_LIBS ${CMAKE_DL_LIBS})
endif (Static)

if (ArchNative)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=native")
endif (ArchNative)

include(CTest)

find_package(ZLIB REQUIRED)

# Prefer a system-provided Catch2 (e.g. from a distro package), falling back to
# fetching a pinned release so a plain checkout still builds out of the box.
find_package(Catch2 3 QUIET)
if (NOT Catch2_FOUND)
    include(FetchContent)
    FetchContent_Declare(Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.16.1)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif ()
include(Catch)

# SQLite is consumed as the official amalgamation, pinned by hash. To build
# offline or against a local copy, pass -DFETCHCONTENT_SOURCE_DIR_SQLITE3=<dir>
# (a directory containing sqlite3.c, sqlite3.h and shell.c).
include(FetchContent)
if (POLICY CMP0135)
    # Use the extraction time, not the archive's, as the file timestamps.
    cmake_policy(SET CMP0135 NEW)
endif ()
FetchContent_Declare(sqlite3
        URL https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
        URL_HASH SHA3_256=92281c86e2957f9b9305890f9ac04204caa7b677cc868dc326d942689df28e30)
FetchContent_MakeAvailable(sqlite3)
add_library(sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
# Position-independent so it can still be linked into a shared libzindex.
set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(sqlite3 SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
target_link_libraries(sqlite3 PUBLIC Threads::Threads ${CMAKE_DL_LIBS})
if (UNIX)
    target_link_libraries(sqlite3 PUBLIC m)
endif ()
if (Static)
    target_compile_definitions(sqlite3 PRIVATE SQLITE_OMIT_LOAD_EXTENSION=1)
endif ()
include_directories(${ZLIB_INCLUDE_DIRS} src ext)

set(SOURCE_FILES
        src/File.h
        src/Index.cpp
        src/Index.h
        src/IndexParser.cpp
        src/IndexParser.h
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
        ext/cJSON/cJSON.h
        ext/cJSON/cJSON.c
        src/ConsoleLog.h
        src/ConsoleLog.cpp
        src/StringView.cpp
        src/StringView.h
        src/PrettyBytes.h
        src/PrettyBytes.cpp
        src/RangeFetcher.cpp
        src/RangeFetcher.h
        src/FieldIndexer.cpp
        src/FieldIndexer.h
        src/ExternalIndexer.cpp
        src/ExternalIndexer.h
        src/Pipe.cpp
        src/Pipe.h)

set(TEST_FILES
        tests/LineFinderTest.cpp
        tests/SqliteTest.cpp
        tests/RegExpTest.cpp
        tests/RegExpIndexerTest.cpp
        tests/TempDir.h
        tests/TempDir.cpp
        tests/PrettyBytesTest.cpp
        tests/IndexTest.cpp
        tests/RangeFetcherTest.cpp
        tests/FieldIndexerTest.cpp
        tests/ExternalIndexerTest.cpp
        tests/LogTest.cpp)

add_library(libzindex ${SOURCE_FILES})
target_link_libraries(libzindex PUBLIC zindex_warnings sqlite3 Threads::Threads)
set_target_properties(libzindex PROPERTIES OUTPUT_NAME zindex)

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES} ${COMMON_LIBS})
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES} ${COMMON_LIBS})

add_executable(unit-tests ${TEST_FILES})
target_link_libraries(unit-tests libzindex Catch2::Catch2WithMain ${ZLIB_LIBRARIES} ${COMMON_LIBS})

if (BuildSqlShell)
    add_executable(sql-shell ${sqlite3_SOURCE_DIR}/shell.c)
    target_link_libraries(sql-shell sqlite3 ${COMMON_LIBS})
    if (Static)
        # Must match the library, or the shell's .load refers to missing symbols.
        target_compile_definitions(sql-shell PRIVATE SQLITE_OMIT_LOAD_EXTENSION=1)
    endif ()
endif (BuildSqlShell)

find_program(MEMORYCHECK_COMMAND valgrind)
enable_testing()
# PRE_TEST discovers at ctest time rather than running the binary straight after
# linking, which fails for Xcode builds on Apple Silicon (the binary is not yet
# code-signed at that point).
catch_discover_tests(unit-tests DISCOVERY_MODE PRE_TEST)

add_test(NAME no-indexer-line-offsets
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/no_indexer_e2e.sh
                 $<TARGET_FILE:zindex> $<TARGET_FILE:zq>)

add_test(NAME uri-index-file
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/uri_index_e2e.sh
                 $<TARGET_FILE:zindex> $<TARGET_FILE:zq>)
