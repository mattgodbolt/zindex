cmake_minimum_required(VERSION 2.8.4)
project(zindex)

find_package(ZLIB REQUIRED)
include_directories(${ZLIB_INCLUDE_DIRS} src)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES
    src/tclap/Arg.h
    src/tclap/ArgException.h
    src/tclap/ArgTraits.h
    src/tclap/CmdLine.h
    src/tclap/CmdLineInterface.h
    src/tclap/CmdLineOutput.h
    src/tclap/Constraint.h
    src/tclap/DocBookOutput.h
    src/tclap/HelpVisitor.h
    src/tclap/IgnoreRestVisitor.h
    src/tclap/MultiArg.h
    src/tclap/MultiSwitchArg.h
    src/tclap/OptionalUnlabeledTracker.h
    src/tclap/StandardTraits.h
    src/tclap/StdOutput.h
    src/tclap/SwitchArg.h
    src/tclap/UnlabeledMultiArg.h
    src/tclap/UnlabeledValueArg.h
    src/tclap/ValueArg.h
    src/tclap/ValuesConstraint.h
    src/tclap/VersionVisitor.h
    src/tclap/Visitor.h
    src/tclap/XorHandler.h
    src/tclap/ZshCompletionOutput.h
    src/File.h
    src/Index.cpp
    src/Index.h
    src/KeyValue.h
    src/LineIndexer.cpp
    src/LineIndexer.h
    src/LineSink.h)

set(TEST_FILES
    tests/catch.hpp
    tests/LineIndexerTest.cpp
    tests/test_main.cpp)

add_library(libzindex ${SOURCE_FILES})

add_executable(zindex src/zindex.cpp)
target_link_libraries(zindex libzindex ${ZLIB_LIBRARIES})
add_executable(zq src/zq.cpp)
target_link_libraries(zq libzindex ${ZLIB_LIBRARIES})

add_executable(unit-tests ${TEST_FILES})
target_link_libraries(unit-tests libzindex ${ZLIB_LIBRARIES})

enable_testing()
add_test(NAME unit-tests
         COMMAND unit-tests)