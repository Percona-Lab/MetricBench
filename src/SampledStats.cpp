// vim: set tabstop=4 expandtab:

/**
 * Ported to C++ from the LinkBenchX SampledStats.java
 *
 * david.bennett at percona.com - August 2015
 */

/**
 * This class is used to keep track of statistics.  It collects a sample of the
 * total data (controlled by maxsamples) and can then compute statistics based
 * on that sample.
 *
 * The overall idea is to compute reasonably accurate statistics using bounded
 * space.
 *
 * Currently the class is used to print out stats at given intervals, with the
 * sample taken over a given time interval and printed at the end of the interval.
 */

#include "SampledStats.hpp"

// define the lock
std::mutex SampledStats::line_lock;

// random numbers
char srand_init=0;
std::mt19937 gen;
std::uniform_real_distribution<double> unif;

SampledStats::SampledStats(int input_threadID, int input_maxsamples, std::ostream &input_csvOutput) :
    threadID(input_threadID),
    maxsamples(input_maxsamples),
    csvOutput(input_csvOutput),
    samples(boost::extents[MAX_OPTYPES][maxsamples])
{
    // init srand - used to decide which to include in sample
    if (!srand_init) {
      unsigned int seed=Config::randomSeed;
      if (!seed) {
        std::random_device rd;
        seed=rd();
      }
      std::mt19937 gen(seed);
      std::uniform_real_distribution<double> unif(0, 1);
      srand_init++;
    }
}

/**
 * MessageType - SQL Operation
 * timetaken - time in milliseconds (10-3)
 * error was there an error?
 */
void SampledStats::addStats(MessageType type, int64_t timetaken, bool error) {

    if (error) {
        errors[type]++;
    }

    if ((minimums[type] == 0) || (minimums[type] > timetaken)) {
        minimums[type] = timetaken;
    }

    if (timetaken > maximums[type]) {
        maximums[type] = timetaken;
    }

    numops[type]++;
    int32_t opIndex=opsSinceReset[type];
    opsSinceReset[type]++;

    if (opIndex < maxsamples) {
        samples[type][opIndex] = timetaken;
    } else {
        // Replacing with the probability guarantees that each measurement
        // has an equal probability of being included in the sample
        double pReplace = ((double)maxsamples) / opIndex;
        double probability = unif(gen);
        if (probability < pReplace) {
          // Select sample to replace randomly
          int32_t rSample = rand()%maxsamples;
          samples[type][rSample] = timetaken;
        }
    }
}

void SampledStats::resetSamples() {
    opsSinceReset.assign(0);
}
/**
 * display stats for samples from start (inclusive) to end (exclusive)
 * @param type - MessageType
 * @param start - collected samples start (usually 0)
 * @param end - collected samples end (usually opsSinceReset[type])
 * @param startTime_ms
 * @param nowTime_ms
 */

void SampledStats::displayStats(MessageType type, int32_t start, int32_t end,
                int64_t sampleStartTime_ms, int64_t nowTime_ms) {

    if (csvOutput) {

        int32_t elems = end - start;
        int64_t timestamp = nowTime_ms / 1000;
        int64_t sampleDuration = nowTime_ms - sampleStartTime_ms;

        if (elems <= 0) {

            // thread lock to prevent output contention
            std::lock_guard<std::mutex> lock(line_lock);

            // TODO: log
            csvOutput << threadID << "," << timestamp << "," << messageTypeLabel[type] <<
                "," << numops[type] << "," << errors[type] <<
                "," << 0 << "," << sampleDuration <<
                ",0,,,,,,,,," << std::endl;

            return;
        }

        // thread lock to prevent output contention
        std::lock_guard<std::mutex> lock(line_lock);

        // sort  from start (inclusive) to end (exclusive)
        std::sort(samples[type].begin() + start, samples[type].begin() + end);

        RunningMean *meanCalc = new RunningMean(samples[type][0]);
        for (int i = start + 1; i < end; i++) {
          meanCalc->addSample(samples[type][i]);
        }

        int64_t min = samples[type][start];
        int64_t p25 = samples[type][start + elems/4];
        int64_t p50 = samples[type][start + elems/2];
        int64_t p75 = samples[type][end - 1 - elems/4];
        int64_t p90 = samples[type][end - 1 - elems/10];
        int64_t p95 = samples[type][end - 1 - elems/20];
        int64_t p99 = samples[type][end - 1 - elems/100];
        int64_t max = samples[type][end - 1];
        double mean = meanCalc->mean();

        delete meanCalc;

        // TODO: log

        csvOutput << threadID << "," << timestamp << "," << messageTypeLabel[type] <<
          "," << numops[type] << "," << errors[type] <<
          "," << opsSinceReset[type] << "," << sampleDuration <<
          "," << elems << "," << mean << "," << min << "," << p25 << "," << p50 <<
          "," << p75 << "," << p90 << "," << p95 << "," << p99 << "," << max << std::endl;
    }

}

void SampledStats::displayStatsAll(int64_t sampleStartTime_ms, int64_t nowTime_ms) {
    std::vector<int> msgTypes;
    for (int i=0; i < sizeof(MessageType); i++) {
        msgTypes.push_back(i);
    }
    displayStats(sampleStartTime_ms, nowTime_ms, msgTypes);
}

void SampledStats::displayStats(int64_t sampleStartTime_ms, int64_t nowTime_ms, const std::vector<int> &msgTypes) {
    for (int type : msgTypes) {
        displayStats(static_cast<MessageType>(type), 0, std::min(maxsamples,opsSinceReset[type]),
                sampleStartTime_ms, nowTime_ms);
    }
}

/**
 * @return total operation count so far for type
 */
int64_t SampledStats::getCount(MessageType type) {
    return(numops[type]);
}

/**
 * Write a header with column names for a csv file showing progress
 * @param out
 */
void SampledStats::writeCSVHeader(std::ostream &out) {

    if (out) {
        out << "threadID,timestamp,op,totalops,totalerrors,ops," <<
               "sampleDuration_us,sampleOps,mean_us,min_us,p25_us,p50_us," <<
               "p75_us,p90_us,p95_us,p99_us,max_us" << std::endl;
    }
}
