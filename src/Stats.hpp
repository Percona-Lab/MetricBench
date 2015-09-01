#pragma once

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Config.hpp"
#include "Logger.hpp"
#include "Message.hpp"
#include "tsqueue.hpp"

class Stats {
    std::vector<unsigned long> insertTimes;

    mutable std::mutex lockCounts;
    std::unordered_map<MessageType, unsigned long, std::hash<int>> Counts;
    std::unordered_map<MessageType, std::vector<unsigned long>, std::hash<int>> execTimes;

    void statsPrint();

public:
    Stats() { }
    void Run();
};

