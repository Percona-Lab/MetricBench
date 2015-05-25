#pragma once

#include <stdlib.h>
#include <iostream>
#include <iomanip> 
#include <sstream>
#include <stdexcept>
#include <atomic>

#include "MySQLDriver.hpp"
#include "Config.hpp"

/* General class to load data, not DB specific */
class Preparer {
	MySQLDriver& DataLoader;
	std::atomic<unsigned int> insertProgress {0};
	std::atomic<bool> progressLoad {true};

public:
	Preparer(MySQLDriver &ML) :DataLoader(ML) {}
	void Run();
	void progressPrint() const;
};

