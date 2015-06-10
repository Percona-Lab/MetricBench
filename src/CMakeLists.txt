cmake_minimum_required (VERSION 2.6)
project (MetricBench)

set(MYSQLCONNECTORCPP_ROOT_DIR
        "${MYSQLCONNECTORCPP_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(MYSQLCONNECTORCPP_INCLUDE_DIR
        mysql_connection.h
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        PATH_SUFFIXES
        include)

find_library(MYSQLCONNECTORCPP_LIBRARY
        NAMES
        mysqlcppconn-static
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        PATH_SUFFIXES
        lib64
        lib64/mysql
        lib
        lib/mysql)

message(STATUS ${MYSQLCONNECTORCPP_LIBRARY})

find_library(MYSQL_CLIENT
        NAMES
        libmysqlclient.a
        PATH_SUFFIXES
        lib64
        lib64/mysql
        lib lib/mysql)

message(STATUS ${MYSQL_CLIENT})

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS program_options system filesystem)

include_directories( ${Boost_INCLUDE_DIRS} ${MYSQLCONNECTORCPP_INCLUDE_DIR} )

mark_as_advanced(MYSQLCONNECTORCPP_INCLUDE_DIR MYSQLCONNECTORCPP_LIBRARY)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MysqlConnectorCpp
        DEFAULT_MSG
        MYSQLCONNECTORCPP_INCLUDE_DIR
        MYSQLCONNECTORCPP_LIBRARY)

set(GCC_COVERAGE_COMPILE_FLAGS "-std=c++11")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
set(BUILD_SHARED_LIBRARIES OFF)
set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)

add_executable(MetricBench metricbench.cpp Preparer.cpp MySQLDriver.cpp Config.cpp Stats.cpp Message.cpp)
set_target_properties(MetricBench PROPERTIES
                LINK_FLAGS "-static")
#target_link_libraries (MetricBench mysqlcppconn-static mysqlclient)
target_link_libraries (MetricBench ${MYSQLCONNECTORCPP_LIBRARY} ${MYSQL_CLIENT}
  ${Boost_LIBRARIES} pthread m rt dl z c)
#    ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_QUEUE_LIBRARY})


# Unit Tests

enable_testing()

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})

#I like to keep test files in a separate source directory called test
file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/*.cpp)

#Run through each source
foreach(testSrc ${TEST_SRCS})
        #Extract the filename without an extension (NAME_WE)
        get_filename_component(testName ${testSrc} NAME_WE)

        #Add compile target
        add_executable(${testName} ${testSrc} Message.cpp)

        #link to Boost libraries AND your targets and dependencies
        target_link_libraries(${testName} ${Boost_LIBRARIES}
             ${MYSQLCONNECTORCPP_LIBRARY} ${MYSQL_CLIENT} pthread m rt dl z )

       #I like to move testing binaries into a testBin directory
        set_target_properties(${testName} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY  ${CMAKE_CURRENT_SOURCE_DIR}/testBin)

        #Finally add it to test execution -
        #Notice the WORKING_DIRECTORY and COMMAND
        add_test(NAME ${testName}
                 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/testBin
                 COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/testBin/${testName} )
endforeach(testSrc)
