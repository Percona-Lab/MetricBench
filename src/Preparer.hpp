#pragma once

#include <algorithm>
#include <cstddef>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <atomic>

#include "MySQLDriver.hpp"
#include "MongoDBDriver.hpp"
#include "CassandraDriver.hpp"
#include "Config.hpp"
#include "LatencyStats.hpp"
#include "Logger.hpp"

/* General class to handle benchmark, not DB specific */
class Preparer {
    GenericDriver* DataLoader;
    std::atomic<unsigned int> insertProgress {0};
    std::atomic<bool> progressLoad {true};
    ParetoGenerator* PGen;
    LatencyStats* latencyStats=(LatencyStats *)nullptr;

public:
    Preparer(GenericDriver *ML) :DataLoader(ML) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    /* prepare data, like create schema and insert */
    void Prep();
    /* Run benchmark itself */
    void Run();
    /* Pass the LatencyStats object to us */
    void setLatencyStats(LatencyStats * ls);
private:
    /* periodical print of prepare progress */
    void prepProgressPrint(uint64_t startTs, uint64_t total) const;
};

