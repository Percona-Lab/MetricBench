#pragma once

#include <cstddef>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include "pareto.hpp"
#include "Config.hpp"
#include "LatencyStats.hpp"

using namespace std;

class GenericDriver {
protected:
    const string user;
    const string pass;
    const string database;
    const string url;
    ParetoGenerator* PGen;
    LatencyStats* latencyStats;

    /* range between first and last records in metrics */
    unsigned int tsRange;

public:
    GenericDriver(const string user,
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

    void setLatencyStats(LatencyStats* ls) { latencyStats = ls; }
};
