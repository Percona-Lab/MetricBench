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
#include "GenericDriver.hpp"

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

    void Prep();
    void Run(unsigned int& minTs, unsigned int& maxTs);
    void CreateSchema();

    /* return max device_id available for given ts */
    unsigned int getMaxDevIdForTS(unsigned int ts);

private:
    void InsertData(int threadId);
    void InsertQuery(int threadId, 
	unsigned int table_id,
	unsigned int timestamp, 
	unsigned int device_id, 
	sql::Statement & stmt);
    void DeleteQuery(int threadId, unsigned int timestamp, unsigned int device_id, sql::Statement & stmt);
};
