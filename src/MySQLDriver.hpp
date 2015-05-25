#pragma once

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

using namespace std;

class MySQLDriver {
    const string user; 
    const string pass;
    const string database;
    const string url;
    ParetoGenerator* PGen;
	
    /* range between first and last records in metrics */
    unsigned int tsRange;

public:
    MySQLDriver(const string user, 
	    const string pass,
	    const string database,
	    const string url) :user(user),
		    pass(pass),
		    database(database),
		    url(url) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    void Prep();
    unsigned int Run();
    void CreateSchema();

private:
    void InsertData();
    void InsertQuery(unsigned int timestamp, sql::Statement & stmt);
    void DeleteQuery(unsigned int timestamp, sql::Statement & stmt);
};
