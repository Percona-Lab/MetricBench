#pragma once

#include <vector>
#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <mutex>
#include <unordered_map>

#include "Message.hpp"

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

