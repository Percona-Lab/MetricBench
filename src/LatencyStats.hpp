#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>

#include <boost/array.hpp>
#include <boost/foreach.hpp>
#include <boost/multi_array.hpp>

#include "Message.hpp"
#include "RunningMean.hpp"

class LatencyStats {

public:

    const static int32_t MAX_MILLIS = 100;
    const static int32_t MAX_OPTYPES = sizeof(MessageType);

    typedef boost::array<int64_t, 2> MinMaxMS;

private:

    const static int SUB_MS_BUCKETS = 10;   // Sub-ms granularity
    const static int MS_BUCKETS = 99;       // ms-granularity buckets
    const static int HUNDREDMS_BUCKETS = 9; // 100ms=granularity bucket
    const static int SEC_BUCKETS = 9;       // 1s-granularity buckets

    int maxThreads;

    typedef boost::multi_array<RunningMean *, 2> RunningMeanArray;
    RunningMeanArray means;

    typedef boost::array<double, MAX_OPTYPES> FinalMeanArray;
    FinalMeanArray finalMeans={};

    typedef boost::multi_array<int64_t, 3> BucketCountArray;
    BucketCountArray bucketCounts;

    typedef boost::multi_array<int64_t, 2> MaxLatencyArray;
    MaxLatencyArray maxLatency;

    typedef boost::array<int64_t, MAX_OPTYPES> SampleCountsArray;
    SampleCountsArray sampleCounts={};

    typedef boost::multi_array<int64_t, 2> BucketCountsCumulativeArray;
    BucketCountsCumulativeArray bucketCountsCumulative;

    // functions

    void calcMeans();
    void calcCumulativeBuckets();
    MinMaxMS getBucketBounds(MessageType type, int64_t percentile);
    static std::string boundsToString(MinMaxMS bucketBounds);
    double getMean(MessageType type);
    double getMax(MessageType type);
    std::string percentileString(MessageType type, int64_t percentile);

public:
    const static int NUM_BUCKETS = SUB_MS_BUCKETS + MS_BUCKETS +
                                   HUNDREDMS_BUCKETS + SEC_BUCKETS + 1;

    // functions

    LatencyStats(int maxThreads);
    static int32_t latencyToBucket(int64_t microTime);
    static MinMaxMS bucketBound(int32_t bucket);
    void recordLatency(int32_t threadid, MessageType type, int64_t microtimetaken);
    void displayLatencyStats();
    void printCSVStats(std::ostream &out, bool header);

    // We are using a function template for CSV output because we don't know
    // how many elements are in the ops array

    template<std::size_t N>
    void printCSVStats(std::ostream &out, bool header, boost::array<MessageType, N> &ops) {
      boost::array<int,5> percentiles = {25, 50, 75, 95, 99};
      char buf[128]; // love handles
      char fmt2f[]="%.2f";
      char fmt3f[]="%.3f";

      if (header) {
        out << "op,count";
        BOOST_FOREACH( int p, percentiles) {
          std::sprintf(buf,",p%d_low,p%d_high", p, p);
          out << buf;
        }
        out << ",max,mean\n";
      }

      BOOST_FOREACH (MessageType msg, ops) {
        int64_t samples = sampleCounts[msg];
        if (samples == 0) {
          continue;
        }
        out <<  messageTypeLabel[msg];
        out << "," << samples;

        BOOST_FOREACH (int p, percentiles) {
          MinMaxMS bounds = getBucketBounds(msg, p);
          for (int mm=0; mm < 2; mm++) {
            std::sprintf(buf,fmt2f,bounds[mm] / 1000.0);
            out << "," << buf;
          }
        }
        std::sprintf(buf,fmt3f,getMax(msg));
        out << "," << buf;
        std::sprintf(buf,fmt3f,getMean(msg));
        out << "," << buf;
        out << "\n";
      }
    }
};

