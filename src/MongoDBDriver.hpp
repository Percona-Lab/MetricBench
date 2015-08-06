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

#include "pareto.hpp"
#include "Config.hpp"
#include "LatencyStats.hpp"
#include "GenericDriver.hpp"

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
    void InsertData(int threadId);
    void InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id,  mongo::DBClientConnection &mongo);
    void DeleteQuery(int threadId, unsigned int timestamp, unsigned int device_id);
};
