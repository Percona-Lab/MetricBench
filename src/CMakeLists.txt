cmake_minimum_required (VERSION 2.6)
project (MetricBench)

# determine if a static or dynamic binary should be built

if (BUILD_STATIC)
  message(STATUS "Building static binary")
  set(MYSQLCPPCONN_LIB_NAME "mysqlcppconn-static")
  set(MONGO_LIB_NAME "mongoclient")
  set(CASS_LIB_NAME "cassandra")
  set(Boost_USE_STATIC_LIBS ON)
  set(CXX_BUILD_FLAGS "-static")
  set(LINK_BUILD_FLAGS "-static -pthread -Wl,--no-as-needed")
  set(PTHREAD_LIB "-Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
  set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
  set(BUILD_SHARED_LIBRARIES OFF)
  set(CMAKE_EXE_LINK_DYNAMIC_CXX_FLAGS)
  set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)
else()
  message(STATUS "Building a dynamically linked binary")
  set(MYSQLCPPCONN_LIB_NAME "mysqlcppconn")
  set(MONGO_LIB_NAME "mongoclient")
  set(CASS_LIB_NAME "cassandra")
  set(Boost_USE_STATIC_LIBS OFF)
  set(CXX_BUILD_FLAGS "")
  set(LINK_BUILD_FLAGS "")
  set(PTHREAD_LIB "pthread")
  set(BUILD_SHARED_LIBRARIES ON)
endif()

# Look for our build script boost and if it exists 
# then set the BOOST_ROOT to point to it

if( EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_boost" )
  set(ENV{BOOST_ROOT} "${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_boost")
endif()

# Find the MySQL library locations

set(MYSQLCONNECTORCPP_ROOT_DIR
        "${MYSQLCONNECTORCPP_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(MYSQLCONNECTORCPP_INCLUDE_DIR
        mysql_connection.h
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_mysqlcpp
        PATH_SUFFIXES
        include)

find_library(MYSQLCONNECTORCPP_LIBRARY
        NAMES
        ${MYSQLCPPCONN_LIB_NAME}
        HINTS
        ${MYSQLCONNECTORCPP_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_mysqlcpp
        PATH_SUFFIXES
        lib64
        lib64/mysql
        lib
        lib/mysql
        lib/x86_64-linux-gnu)

message(STATUS ${MYSQLCONNECTORCPP_LIBRARY})

find_library(MYSQL_CLIENT
        NAMES
        libmysqlclient.a
        PATH_SUFFIXES
        lib64
        lib64/mysql
        lib lib/mysql)

message(STATUS ${MYSQL_CLIENT})

# Find the MongoDB library locations

set(MONGO_LIB_ROOT_DIR
        "${MONGO_LIB_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(MONGO_LIB_INCLUDE_DIR
        mongo/client/dbclient.h
        HINTS
        ${MONGO_LIB_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_mongo-cxx-driver
        PATH_SUFFIXES
        include)

find_library(MONGO_LIBRARY
        NAMES
        ${MONGO_LIB_NAME}
        HINTS
        ${MONGO_LIB_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_mongo-cxx-driver
        PATH_SUFFIXES
        lib64
        lib
        lib/x86_64-linux-gnu)

message(STATUS ${MONGO_LIBRARY})

# Find the Cassandra library locations

set(CASS_LIB_ROOT_DIR
        "${CASS_LIB_ROOT_DIR}"
        CACHE
        PATH
        "Where to start looking for this component.")

find_path(CASS_LIB_INCLUDE_DIR
        cassandra.h
        HINTS
        ${CASS_LIB_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_cassandra-cxx-driver
        PATH_SUFFIXES
        include)

find_library(CASS_LIBRARY
        NAMES
        ${CASS_LIB_NAME}
        HINTS
        ${CASS_LIB_ROOT_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../build/MetricBench_cassandra-cxx-driver
        PATH_SUFFIXES
        lib64
        lib
        lib/x86_64-linux-gnu)

message(STATUS ${CASS_LIBRARY})

# Configure Boost packages

find_package(Boost REQUIRED COMPONENTS program_options system-mt filesystem thread-mt regex)

# Configure header directories

include_directories( ${Boost_INCLUDE_DIRS} ${MYSQLCONNECTORCPP_INCLUDE_DIR} ${MONGO_LIB_INCLUDE_DIR} ${CASS_LIB_INCLUDE_DIR} )

# MySQL client C/C++ headers and libraries are required

mark_as_advanced(MYSQLCONNECTORCPP_INCLUDE_DIR MYSQLCONNECTORCPP_LIBRARY)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MysqlConnectorCpp
        DEFAULT_MSG
        MYSQLCONNECTORCPP_INCLUDE_DIR
        MYSQLCONNECTORCPP_LIBRARY)

# Global compiler flags

set(GCC_COVERAGE_COMPILE_FLAGS "-std=c++11 ${CXX_BUILD_FLAGS}")
add_definitions(${GCC_COVERAGE_COMPILE_FLAGS})

# MetricBench binary settings

add_executable(MetricBench metricbench.cpp Preparer.cpp MySQLDriver.cpp MongoDBDriver.cpp 
  CassandraDriver.cpp Config.cpp Stats.cpp Message.cpp RunningMean.cpp LatencyStats.cpp Uri.cpp SampledStats.cpp)

set_target_properties(MetricBench PROPERTIES
  LINK_FLAGS "${LINK_BUILD_FLAGS}")

target_link_libraries (MetricBench ${MYSQLCONNECTORCPP_LIBRARY} ${MYSQL_CLIENT} 
  ${MONGO_LIBRARY} ${CASS_LIBRARY} ${Boost_LIBRARIES} ${PTHREAD_LIB} m rt dl z c)

# Unit Tests

enable_testing()

# Boost unit test framework is required

find_package(Boost COMPONENTS unit_test_framework REQUIRED)
include_directories (${Boost_INCLUDE_DIRS})

#I like to keep test files in a separate source directory called test
file(GLOB TEST_SRCS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/*.cpp)

if(BUILD_STATIC)
else()
  add_definitions(-DBOOST_TEST_DYN_LINK) 
endif()

#Run through each source
foreach(testSrc ${TEST_SRCS})
        #Extract the filename without an extension (NAME_WE)
        get_filename_component(testName ${testSrc} NAME_WE)

        #Add compile target
        add_executable(${testName} ${testSrc} Message.cpp LatencyStats.cpp Uri.cpp
          RunningMean.cpp)

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
