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

find_library(MYSQL_CLIENT
        NAMES
        libmysqlclient.a
        PATH_SUFFIXES
        lib64
        lib64/mysql
        lib lib/mysql)

message(STATUS ${MYSQL_CLIENT})

set(Boost_USE_STATIC_LIBS ON)
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories( ${Boost_INCLUDE_DIRS} )

mark_as_advanced(MYSQLCONNECTORCPP_INCLUDE_DIR MYSQLCONNECTORCPP_LIBRARY)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MysqlConnectorCpp
	DEFAULT_MSG
	MYSQLCONNECTORCPP_INCLUDE_DIR
	MYSQLCONNECTORCPP_LIBRARY)

set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

add_executable(MetricBench metricbench.cpp Preparer.cpp MySQLDriver.cpp Config.cpp Stats.cpp)
set_target_properties(MetricBench PROPERTIES
		LINK_FLAGS "-L/usr/local/lib/x86_64-linux-gnu/"
		COMPILE_FLAGS "-std=c++11")
#target_link_libraries (MetricBench mysqlcppconn-static mysqlclient)
target_link_libraries (MetricBench mysqlcppconn-static ${MYSQL_CLIENT} pthread m rt dl z
    ${Boost_LIBRARIES})
#    ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_QUEUE_LIBRARY})
