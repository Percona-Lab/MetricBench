#pragma once

#include <cstddef>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>

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
    bool ostreamSet;

    /* range between first and last records in metrics */
    unsigned int tsRange;

    std::vector<std::thread> threads;

public:

    /* timestamp range */
    struct ts_range {
      uint64_t min;
      uint64_t max;
    };

    /* device range */
    struct dev_range {
      unsigned int min;
      unsigned int max;
    };

    GenericDriver(const string user,
	    const string pass,
	    const string database,
	    const string url) :user(user),
		    pass(pass),
		    database(database),
		    url(url),
                    latencyStats((LatencyStats *)nullptr),
                    ostreamSampledStats((std::ostream *)nullptr),
                    ostreamSet(false),
                    threads(std::vector<std::thread>()) {
    }

    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    void setLatencyStats(LatencyStats* ls) { latencyStats = ls; }
    void setOstreamSampledStats(std::ostream* osss) { ostreamSampledStats  = osss; ostreamSet=true; }

    /* functions to redefine in Driver implementation */

    virtual void CreateSchema() = 0;  // create schema (before prepare)
    virtual void Prep() = 0;  // prepare phase
    virtual void Run() = 0;   // run phase

    /**
     * return max device_id available for given ts
     *
     * @param table/collection id,  or 0 for all
     */
    virtual ts_range getTimestampRange(unsigned int) = 0;

    /**
     * return the device range for the given ts range
     *
     * @param timestamp range (if .max == 0 then all)
     * @param table/collection id (if 0 then all)
     */
    virtual dev_range getDeviceRange(ts_range tsRange, unsigned int) = 0;

    // wait for threads to complete
    void JoinThreads() {
        for (std::thread & th : threads) {
            th.join();
        }
    }

};
