#pragma once

#include <stdlib.h>
#include <iostream>
#include <iomanip> 
#include <sstream>
#include <stdexcept>
#include <atomic>

#include "MySQLDriver.hpp"
#include "Config.hpp"

/* General class to handle benchmark, not DB specific */
class Preparer {
    MySQLDriver& DataLoader;
    std::atomic<unsigned int> insertProgress {0};
    std::atomic<bool> progressLoad {true};
    ParetoGenerator* PGen;

public:
    Preparer(MySQLDriver &ML) :DataLoader(ML) {}
    void SetGenerator(ParetoGenerator* PG) { PGen=PG; }
    /* prepare data, like create schema and insert */
    void Prep();
    /* Run benchmark itself */
    void Run();
private:
    /* periodical print of prepare progress */
    void prepProgressPrint(unsigned int startTs, unsigned int total) const;
};

