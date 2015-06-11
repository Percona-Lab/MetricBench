#pragma once

#include <stdint.h>

#include <boost/array.hpp>
#include <boost/multi_array.hpp>

#include "Message.hpp"
#include "RunningMean.hpp"

class LatencyStats {

public:

    const static int32_t MAX_MILLIS = 100;
    const static int32_t MAX_OPTYPES = sizeof(MessageType);

private:

    const static int SUB_MS_BUCKETS = 10;   // Sub-ms granularity
    const static int MS_BUCKETS = 99;       // ms-granularity buckets
    const static int HUNDREDMS_BUCKETS = 9; // 100ms=granularity bucket
    const static int SEC_BUCKETS = 9;       // 1s-granularity buckets

    int maxThreads;

    typedef boost::multi_array<RunningMean *, 2> RunningMeanArray;
    RunningMeanArray means;

    typedef boost::array<double, MAX_OPTYPES> FinalMeanArray;
    FinalMeanArray finalMeans;

    typedef boost::multi_array<int64_t, 3> BucketCountArray;
    BucketCountArray bucketCounts;

    typedef boost::multi_array<int64_t, 2> MaxLatencyArray;
    MaxLatencyArray maxLatency;

    typedef boost::array<int64_t, MAX_OPTYPES> SampleCountsArray;
    SampleCountsArray sampleCounts;

    typedef boost::multi_array<int64_t, 2> BucketCountsCumulativeArray;
    BucketCountsCumulativeArray bucketCountsCumulative;

    void calcMeans();
    void calcCumulativeBuckets();

public:
    const static int NUM_BUCKETS = SUB_MS_BUCKETS + MS_BUCKETS +
                                   HUNDREDMS_BUCKETS + SEC_BUCKETS + 1;

    typedef boost::array<int64_t, 2> MinMaxMS;

    LatencyStats(int maxThreads);
    static int32_t latencyToBucket(int64_t microTime);
    static MinMaxMS bucketBound(int32_t bucket);
    void recordLatency(int32_t threadid, MessageType type, int64_t microtimetaken);

};

