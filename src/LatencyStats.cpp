#include "LatencyStats.hpp"

/**
 * Ported to C++ from the LinkBench LatencyStats.java
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
 * @param[bucket] The bucket to compute the bounds for
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

/**
 * Print out percentile values
 */
void LatencyStats::displayLatencyStats()
{
  char buf[128]; // big elbow
  char fmt[]="%.3f";
  calcMeans();
  calcCumulativeBuckets();

  for (int type; type < MAX_OPTYPES; type++) {
    if (sampleCounts[type] == 0) {
      continue;
    }
    MessageType msg=static_cast<MessageType>(type);

    std::cout << messageTypeLabel[type] <<
      " count = " << sampleCounts[type] << " " <<
      " p25 = " << percentileString(msg, 25) << "ms " <<
      " p50 = " << percentileString(msg, 50) << "ms " <<
      " p75 = " << percentileString(msg, 75) << "ms " <<
      " p95 = " << percentileString(msg, 95) << "ms " <<
      " p99 = " << percentileString(msg, 99) << "ms ";

      sprintf(buf, fmt, getMax(msg));
      std::cout << " max = " << buf << "ms ";

      sprintf(buf, fmt, getMean(msg));
      std::cout << " mean = " << buf << "ms\n";
  }
}

/**
 * Print latency statistics for all of the operation types to a CSV file
 * (see LatencyStats.hpp) for function template)
 */
void LatencyStats::printCSVStats(std::ostream &out, bool header) {
  boost::array<MessageType, MAX_OPTYPES> ops;
  for (int i; i < MAX_OPTYPES; i++) {
    ops[i]=static_cast<MessageType>(i);
  }
  printCSVStats(out, header, ops);
}

void LatencyStats::calcMeans()
{
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
  for (int type = 0; type < MAX_OPTYPES; type++) {
    int64_t count = 0;
    for (int bucket = 0; bucket < NUM_BUCKETS; bucket++) {
      for (int thread = 0; thread < maxThreads; thread++) {
        count += bucketCounts[thread][type][bucket];
      }
      bucketCountsCumulative[type][bucket] = count;
    }
  }
}

/**
 * Get the minumum and maximum time for an operation type
 */
LatencyStats::MinMaxMS LatencyStats::getBucketBounds(MessageType type, int64_t percentile) {
  auto n = sampleCounts[type];
  //neededRank is the rank of the sample at the desired percentile
  auto neededRank = static_cast<int64_t> (( percentile / 100.0 ) * n );
  int bucketNum = -1;
  for (int i = 0; i < NUM_BUCKETS; i++) {
    auto rank = bucketCountsCumulative[type][i];
    if(neededRank <= rank) {
      bucketNum = i;
      break;
    }
  }
  assert(bucketNum >= 0); // should be found
  return LatencyStats::bucketBound(bucketNum);
}

/**
 * @return A human-readable string for the bucket bounds
 */
std::string LatencyStats::percentileString(MessageType type, int64_t percentile)
{
  return boundsToString(getBucketBounds(type, percentile));
}


std::string LatencyStats::boundsToString(LatencyStats::MinMaxMS bucketBounds)
{
  char buff[128]; // defined paranoically
  char fmt[]="%.2f";

  double minMs = bucketBounds[0] / 1000.0;
  double maxMs = bucketBounds[1] / 1000.0;

  std::ostringstream outstr;

  sprintf(buff, fmt, minMs);
  outstr << "[" << buff << ",";

  sprintf(buff, fmt, maxMs);
  outstr << buff << "]";

  return outstr.str();
}

double LatencyStats::getMean(MessageType type)
{
  return finalMeans[type];
}

double LatencyStats::getMax(MessageType type)
{
  int64_t max_us = 0;
  for (int thread = 0; thread < maxThreads; thread++) {
    max_us = std::max(max_us,maxLatency[thread][type]);
  }
  return max_us / 1000.0;
}

