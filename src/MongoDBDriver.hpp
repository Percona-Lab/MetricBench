#pragma once

#include <cstddef>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include "mongo/client/dbclient.h"
#include "mongo/client/index_spec.h"

#include "Config.hpp"
#include "GenericDriver.hpp"
#include "LatencyStats.hpp"
#include "Logger.hpp"
#include "pareto.hpp"
#include "SampledStats.hpp"

using namespace std;

class MongoDBDriver : public GenericDriver {
public:
    MongoDBDriver(const string user,
	    const string pass,
	    const string database,
	    const string url);

    virtual void CreateSchema();    // Create the schema (during prepare)
    virtual void Prep();            // Prepare the data
    virtual void Run();             // Run mode

    /**
     * return max device_id available for given ts
     *
     * @param table/collection id,  or 0 for all
     */
    virtual GenericDriver::ts_range getTimestampRange(unsigned int);

    /**
     * return the device range for the given ts range
     *
     * @param timestamp range (if .max == 0 then all)
     * @param table/collection id (if 0 then all)
     */
    virtual GenericDriver::dev_range getDeviceRange(GenericDriver::ts_range tsRange, unsigned int);

private:
    bool getConnection(mongo::DBClientConnection &);
    void InsertData(int threadId, const std::vector<int> &);
    void InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id,
        mongo::DBClientConnection & mongo,
        SampledStats & stats);
    void DeleteQuery(int threadId,
        unsigned int timestamp,
        unsigned int device_id,
        mongo::DBClientConnection & mongo,
        SampledStats & stats);
};
