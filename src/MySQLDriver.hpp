#pragma once

#include <atomic>
#include <boost/lockfree/queue.hpp>
#include <chrono>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unordered_set>

#include "mysql_driver.h"
#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>

#include "Config.hpp"
#include "GenericDriver.hpp"
#include "LatencyStats.hpp"
#include "Message.hpp"
#include "pareto.hpp"
#include "SampledStats.hpp"
#include "tsqueue.hpp"

using namespace std;

class MySQLDriver : public GenericDriver {

public:
    MySQLDriver(const string user,
	    const string pass,
	    const string database,
	    const string url) :
		GenericDriver(user,
		    pass,
		    database,
		    url) {}

    virtual void Prep();
    virtual void Run();
    virtual void CreateSchema();

    /* return max device_id available for given ts */
    virtual GenericDriver::ts_range getTimestampRange(unsigned int);
    virtual GenericDriver::dev_range getDeviceRange(GenericDriver::ts_range, unsigned int);

private:
    void InsertData(const int threadId, const std::vector<int> &);
    void InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id,
	sql::Statement & stmt,
        SampledStats & stats);
    void DeleteQuery(int threadId,
                     unsigned int table_id,
                     unsigned int timestamp,
                     unsigned int device_id,
                     sql::Statement & stmt,
                     SampledStats & stats);
    void SelectQuery(int threadId,
                     unsigned int table_id,
                     unsigned int timestamp,
                     unsigned int device_id,
                     sql::Statement & stmt,
                     SampledStats & stats,
		     MessageType mt);
};
