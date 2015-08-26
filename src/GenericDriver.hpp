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
#include "SampledStats.hpp"

using namespace std;

class GenericDriver {
protected:
    const string user;
    const string pass;
    const string database;
    const string url;
    ParetoGenerator* PGen;
    LatencyStats* latencyStats;
    std::ostream* ostreamSampledStats;

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
                    latencyStats((LatencyStats *)nullptr),
                    ostreamSampledStats((std::ostream *)nullptr) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    void setLatencyStats(LatencyStats* ls) { latencyStats = ls; }
    void setOstreamSampledStats(std::ostream* osss) { ostreamSampledStats  = osss; }

    /* functions to redefine in Driver implementation */
    virtual void Prep() = 0;
    virtual void Run(unsigned int& minTs, unsigned int& maxTs) = 0;
    virtual void CreateSchema() = 0;

    /* return max device_id available for given ts */
    virtual unsigned int getMaxDevIdForTS(unsigned int ts) = 0;

};
