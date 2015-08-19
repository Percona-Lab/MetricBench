// vim: set tabstop=4 expandtab:

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/multi_array.hpp>

#include "Message.hpp"
#include "RunningMean.hpp"

class SampledStats {

public:
    const static int32_t MAX_OPTYPES = sizeof(MessageType);

private:
    // Actual number of operations per type that caller did
    typedef boost::array<int64_t, MAX_OPTYPES> NumOpsArray;
    NumOpsArray numops={};

    // Max samples per type
    int maxsamples;

    // samples for various optypes
    typedef boost::multi_array<int64_t, 2> SamplesArray;
    SamplesArray samples;

    // Number of operations that the sample is drawn from
    typedef boost::array<int, MAX_OPTYPES> OpsSinceResetArray;
    OpsSinceResetArray opsSinceReset={};

    // minimums encounetered per operation type
    typedef boost::array<int, MAX_OPTYPES> MinimumsArray;
    MinimumsArray minimums={};

    // maximums encountered per operation type
    typedef boost::array<int, MAX_OPTYPES> MaximumsArray;
    MaximumsArray maximums={};

    // #errors encountered per type
    typedef boost::array<int64_t, MAX_OPTYPES> ErrorsArray;
    ErrorsArray errors={};

    // Displayed along with stats
    int32_t threadID;

    // Stream to write csv output to ( null if no csv output )
    std::ostream &csvOutput;

public:
    SampledStats(int32_t, int32_t, std::ostream &);
    void addStats(MessageType, int64_t, bool);
    void resetSamples();
    void displayStats(MessageType, int32_t, int32_t, int64_t, int64_t);
    void displayStatsAll(int64_t, int64_t);
    void displayStats(int64_t, int64_t, std::vector<int> &);
    int64_t getCount(MessageType);

    static void writeCSVHeader(std::ostream &);

};
