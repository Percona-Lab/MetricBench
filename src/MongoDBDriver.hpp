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
#include "pareto.hpp"
#include "SampledStats.hpp"

using namespace std;

class MongoDBDriver : public GenericDriver {
public:
    MongoDBDriver(const string user,
	    const string pass,
	    const string database,
	    const string url);

    virtual void Prep();
    virtual void Run(unsigned int& minTs, unsigned int& maxTs);
    virtual void CreateSchema();

    /* return max device_id available for given ts */
    virtual unsigned int getMaxDevIdForTS(unsigned int ts);

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
