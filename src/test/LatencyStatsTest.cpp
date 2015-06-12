#include "../LatencyStats.hpp"

#define BOOST_TEST_MODULE LatencyStatsTest
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE( latency_stats_test )
{
  for (int64_t us = 0; us < 100 * 1000 * 1000; us += 100) {
    int32_t bucket = LatencyStats::latencyToBucket(us);
    BOOST_CHECK(bucket >= 0);
    BOOST_CHECK(bucket < LatencyStats::NUM_BUCKETS);
    LatencyStats::MinMaxMS range = LatencyStats::bucketBound(bucket);
    BOOST_CHECK(us >= range[0]);
    BOOST_CHECK(us <= range[1]);
  }
}

