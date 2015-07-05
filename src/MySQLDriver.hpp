#pragma once

#include <cstddef>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
/*#include <cppconn/connection.h"*/
#include "mysql_driver.h"
#include "mysql_connection.h"

#include "pareto.hpp"
#include "Config.hpp"
#include "LatencyStats.hpp"

using namespace std;

class MySQLDriver {
    const string user;
    const string pass;
    const string database;
    const string url;
    ParetoGenerator* PGen;
    LatencyStats* latencyStats;

    /* range between first and last records in metrics */
    unsigned int tsRange;

public:
    MySQLDriver(const string user,
	    const string pass,
	    const string database,
	    const string url) :user(user),
		    pass(pass),
		    database(database),
		    url(url),
                    latencyStats((LatencyStats *)nullptr) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    void Prep();
    void Run(unsigned int& minTs, unsigned int& maxTs);
    void CreateSchema();

    /* return max device_id available for given ts */
    unsigned int getMaxDevIdForTS(unsigned int ts);

    void setLatencyStats(LatencyStats* ls);

private:
    void InsertData(int threadId);
    void InsertQuery(int threadId, 
	unsigned int table_id,
	unsigned int timestamp, 
	unsigned int org_id, 
	unsigned int device_id, 
	sql::Statement & stmt);
    void DeleteQuery(int threadId, unsigned int timestamp, unsigned int device_id, sql::Statement & stmt);
};
