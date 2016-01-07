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


#include <cassandra.h>

#include "Config.hpp"
#include "GenericDriver.hpp"
#include "LatencyStats.hpp"
#include "Message.hpp"
#include "pareto.hpp"
#include "SampledStats.hpp"
#include "tsqueue.hpp"

using namespace std;

class CassandraDriver : public GenericDriver {

public:
    CassandraDriver(const string user,
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
    void SelectData(const std::vector<int> &);
    CassError execute_query(CassSession* session, const char* query);
    void InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id, 
	CassSession* session,
        SampledStats & stats);
};
