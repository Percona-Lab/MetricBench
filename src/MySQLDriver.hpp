#pragma once

#include <stdlib.h>
#include <iostream>
#include <iomanip> 
#include <sstream>
#include <stdexcept>
#include <string>

#include "pareto.hpp"
#include "Config.hpp"

using namespace std;

class MySQLDriver {
    const string user; 
    const string pass;
    const string database;
    const string url;
    ParetoGenerator* PGen;
	
public:
    MySQLDriver(const string user, 
	    const string pass,
	    const string database,
	    const string url) :user(user),
		    pass(pass),
		    database(database),
		    url(url) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    void Run();
    void CreateSchema();

private:
    void InsertData();
};
