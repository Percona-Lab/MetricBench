#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_set>

#include "Preparer.hpp"
#include "MySQLDriver.hpp"
#include "Message.hpp"
#include "tsqueue.hpp"

extern tsqueue<Message> tsQueue;

void Preparer::prepProgressPrint(uint64_t startTs, uint64_t total) const {

    auto t0 = std::chrono::high_resolution_clock::now();

    while (progressLoad) {
	std::this_thread::sleep_for (std::chrono::seconds(10));
	auto t1 = std::chrono::high_resolution_clock::now();
	auto secFromStart = std::chrono::duration_cast<std::chrono::seconds>(t1-t0).count();

	auto estTotalTime = ( insertProgress > 0 ? secFromStart / (
			static_cast<double> (insertProgress - startTs)  / total
			) : 0 );

	log(logINFO) << std::fixed << std::setprecision(2)
	    << "[Progress] Time: " << secFromStart << "[sec], "
	    << "Progress: " << insertProgress
	    << "/" << total << " = "
	    << static_cast<double> (insertProgress - startTs) * 100 / (total)
	    << "%, "
	    << "Time left: " << estTotalTime - secFromStart << "[sec] "
	    << "Est total: " << estTotalTime << "[sec]";
    }

}

void Preparer::Prep(){

    DataLoader->CreateSchema();
    DataLoader->Prep();

    /* create thread printing progress */
    std::thread threadReporter(&Preparer::prepProgressPrint,
	this,
	0,
	Config::LoadMins * Config::MaxDevices * Config::DBTables);

    /* Populate the test table with data */
	insertProgress = 0;


    /* Device Loop */
    for (auto dc = 1; dc <= Config::MaxDevices ; dc++) {

        GenericDriver::ts_range tsRange=DataLoader->getTimestampRange(0);

        uint64_t startTimestamp = std::max(tsRange.max+60,Config::StartTimestamp);

        /* Time Loop */
        for (uint64_t ts = startTimestamp;
            ts < startTimestamp + Config::LoadMins * 60 ; ts += 60) {

            log(logDEBUG) << "Timestamp: " << ts;

            /* tables loop */
            for (auto table = 1; table <= Config::DBTables; table++) {
                Message m(Insert, ts, dc, table);
                tsQueue.push(m);
            }
            insertProgress+=Config::DBTables;
            log(logDEBUG) << std::fixed << std::setprecision(2) << " insertProgress: " << insertProgress;
            tsQueue.wait_size(Config::LoaderThreads*2);
        }

	tsQueue.wait_empty();

    }

    log(logINFO) << "Data Load Finished";

    progressLoad = false;
    /* wait on reporter to finish */
    threadReporter.join();

}

void Preparer::Run(){

    // get the existing range that spans across all
    // tables
    GenericDriver::ts_range tsRange = DataLoader->getTimestampRange(0);

    // start the driver run threads
    DataLoader->Run();

    // our processing window is load seconds
    int64_t tsWindow = Config::LoadMins * 60;

    uint64_t startTimestamp=tsRange.max + 60;
    uint64_t endTimestamp=tsRange.max + tsWindow;

    // start deleting from the oldest recorded timestamp
    uint64_t deleteTimestamp=tsRange.min;

    /* create thread printing progress */
    std::thread threadReporter(&Preparer::prepProgressPrint,
	this,
        startTimestamp,
	Config::LoadMins * 60);

    log(logINFO) << "Running benchmark from ts: "
	<< startTimestamp << ", to ts: "
	<< endTimestamp
	<< ", range: "
	<< endTimestamp - startTimestamp;

        /* Time loop */
        for (auto ts=startTimestamp; ts <= endTimestamp; ts += 60) {

            unsigned int devicesCnt = PGen->GetNext(Config::MaxDevices, 0);
            GenericDriver::dev_range deviceRange = DataLoader->getDeviceRange({0,0},0);
            unsigned int oldDevicesCnt = deviceRange.max;

            /* Devices loop */
            for (auto dc = 1; dc <= max(devicesCnt,oldDevicesCnt) ; dc++) {

                log(logDEBUG) << "Timestamp: " << ts
                    << ", Devices: "
                    << devicesCnt
                    << ", Old Devices: "
                    << oldDevicesCnt;

                /* Table/Collection loop */
                for (unsigned int table_id=1; table_id <= Config::DBTables; table_id++) {

                    if (dc <= devicesCnt) {
                        Message m(Insert, ts, dc, table_id);
                        tsQueue.push(m);
                    }
                    if (dc <= oldDevicesCnt) {
                        Message m(Delete, deleteTimestamp, dc, table_id);
                        tsQueue.push(m);
                    }
                }
	}

        // advance our trailing delete
        deleteTimestamp += 60;

	tsQueue.wait_size(Config::LoaderThreads*10);
	insertProgress = ts;
    }

    log(logINFO) << "Benchmark Finished";
    progressLoad = false;
    /* wait on reporter to finish */
    threadReporter.join();
}

void Preparer::setLatencyStats(LatencyStats* ls)
{
  latencyStats=ls;
}

