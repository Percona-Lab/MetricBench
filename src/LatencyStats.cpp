#include <algorithm>
#include <cstddef>

#include "LatencyStats.hpp"

/**
 * Ported from the LinkBench LatencyStats class
 *
 * david.bennett at percona.com - June 2015
 *
 */

/**
 * Class used to track and compute latency statistics, particularly
 * percentiles. Times are divided into buckets, with counts maintained
 * per bucket.  The division into buckets is based on typical latencies
 * for database operations: most are in the range of 0.1ms to 100ms.
 * we have 0.1ms-granularity buckets up to 1ms, then 1ms-granularity from
 * 1-100ms, then 100ms-granularity, and then 1s-granularity.
 */
LatencyStats::LatencyStats(int maxThreads) :
  maxThreads(maxThreads),
  means(boost::extents[maxThreads][MAX_MILLIS]),
  bucketCounts(boost::extents[maxThreads][MAX_OPTYPES][NUM_BUCKETS]),
  maxLatency(boost::extents[maxThreads][MAX_OPTYPES]),
  bucketCountsCumulative(boost::extents[MAX_OPTYPES][NUM_BUCKETS])
{
  // initialize our arrays
  std::fill( means.origin(), means.origin() + means.size(),(RunningMean *)NULL);
  std::fill( bucketCounts.origin(), bucketCounts.origin() + bucketCounts.size(), 0);
  std::fill( maxLatency.origin(), maxLatency.origin() + maxLatency.size(), 0);
}

/**
 * Determine which bucket to record operation based on latency value
 *
 */
int32_t LatencyStats::latencyToBucket(int64_t microTime)
{
    int64_t ms = int32_t(1000);
    auto msTime = microTime / ms;
    if(msTime == 0) {
        return static_cast< int32_t >((microTime / int32_t(100)));
    } else if(msTime < 100) {
        auto msBucket = static_cast< int32_t >(msTime) - int32_t(1);
        return SUB_MS_BUCKETS + msBucket;
    } else if(msTime < 1000) {
        auto hundredMSBucket = static_cast< int32_t >((msTime / int32_t(100))) - int32_t(1);
        return SUB_MS_BUCKETS + MS_BUCKETS + hundredMSBucket;
    } else if(msTime < 10000) {
        auto secBucket = static_cast< int32_t >((msTime / int32_t(1000))) - int32_t(1);
        return SUB_MS_BUCKETS + MS_BUCKETS + HUNDREDMS_BUCKETS+ secBucket;
    } else {
        return NUM_BUCKETS - int32_t(1);
    }
}

/**
 *
 * @param bucket
 * @return inclusive min and exclusive max time in microsecs for bucket
 */
LatencyStats::MinMaxMS LatencyStats::bucketBound(int32_t bucket)
{
    auto ms = int32_t(1000);
    int64_t s = ms * int32_t(1000);
    LatencyStats::MinMaxMS res;
    if(bucket < SUB_MS_BUCKETS) {
        res[int32_t(0)] = bucket * int32_t(100);
        res[int32_t(1)] = (bucket + int32_t(1)) * int32_t(100);
    } else if(bucket < SUB_MS_BUCKETS + MS_BUCKETS) {
        res[int32_t(0)] = (bucket - SUB_MS_BUCKETS + int32_t(1)) * ms;
        res[int32_t(1)] = (bucket - SUB_MS_BUCKETS + int32_t(2)) * ms;
    } else if(bucket < SUB_MS_BUCKETS + MS_BUCKETS + HUNDREDMS_BUCKETS) {
        auto hundredMS = bucket - SUB_MS_BUCKETS - MS_BUCKETS + int32_t(1);
        res[int32_t(0)] = hundredMS * int32_t(100) * ms;
        res[int32_t(1)] = (hundredMS + int32_t(1)) * int32_t(100) * ms;
    } else if(bucket < SUB_MS_BUCKETS + MS_BUCKETS + HUNDREDMS_BUCKETS+ SEC_BUCKETS) {
        auto secBucket = bucket - SUB_MS_BUCKETS - MS_BUCKETS- SEC_BUCKETS + int32_t(1);
        res[int32_t(0)] = secBucket * s;
        res[int32_t(1)] = (secBucket + int32_t(1)) * s;
    } else {
        res[int32_t(0)] = (SEC_BUCKETS + int32_t(1)) * s;
        res[int32_t(1)] = int32_t(100) * s;
    }
    return res;
}

/**
 * This method is used to record the latency of each individual operation.
 */
void LatencyStats::recordLatency(int32_t threadid, MessageType type, int64_t microtimetaken)
{
  boost::multi_array<int64_t, 1> opBuckets = bucketCounts[threadid][type];
  int bucket = latencyToBucket(microtimetaken);
  opBuckets[bucket]++;

  auto time_ms = microtimetaken / 1000.0;
  if (means[threadid][type] == (RunningMean *)NULL) {
    means[threadid][type] = new RunningMean(time_ms);
  } else {
    means[threadid][type]->addSample(time_ms);
  }
  if (maxLatency[threadid][type] < microtimetaken) {
    maxLatency[threadid][type] = microtimetaken;
  }
}

// TODO: displayLatencyStats
//
// TODO: printCSVStats
//
// TODO: printCSVStats(...)


void LatencyStats::calcMeans()
{
  boost::array<int64_t, MAX_OPTYPES> sampleCounts={};
  boost::array<double, MAX_OPTYPES> finalMeans={};

  for (int i = 0; i < MAX_OPTYPES; i++) {
    int64_t samples = 0;
    for (int thread = 0; i < maxThreads; thread++) {
      if (means[thread][i] != (RunningMean *)NULL) {
        samples += means[thread][i]->samples();
      }
    }
    sampleCounts[i] = samples;
    double weightedMean = 0.0;
    for (int32_t thread = 0; thread < maxThreads; thread++) {
      if (means[thread][i] != (RunningMean *)NULL) {
        weightedMean += (means[thread][i]->samples() / static_cast< double >(samples)) *
          means[thread][i]->mean();
      }
    }
    finalMeans[i] = weightedMean;
  }
}

/**
 * Calculate the cumulative operation counts by bucket for each type
 */
void LatencyStats::calcCumulativeBuckets()
{
  std::fill( bucketCountsCumulative.origin(),
             bucketCountsCumulative.origin() + bucketCountsCumulative.size(), 0);
}
