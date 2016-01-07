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

	auto estTotalTime = ( (insertProgress > startTs) ? secFromStart / (
			static_cast<double> (insertProgress - startTs)  / total
			) : 0 );

	cout << std::fixed << std::setprecision(2)
	    << "[Progress] Time: " << secFromStart << "[sec], "
	    << "Progress: " << (insertProgress - startTs)
	    << "/" << total << " = "
	    << static_cast<double> (insertProgress - startTs) * 100 / (total)
	    << "%, "
	    << "Time left: " << estTotalTime - secFromStart << "[sec] "
	    << "Est total: " << estTotalTime << "[sec]"
	    << endl;
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

        GenericDriver::ts_range tsRange=DataLoader->getTimestampRange(0);
        uint64_t startTimestamp = std::max(tsRange.max+60,Config::StartTimestamp);
	cout << "Running prepare from ts: "
	<< startTimestamp << ", to ts: "
	<< startTimestamp + Config::LoadMins * 60
	<< ", range: "
	<< startTimestamp + Config::LoadMins * 60 - startTimestamp
	<< endl;

	/* Time Loop */
	for (uint64_t ts = startTimestamp;
			ts < startTimestamp + Config::LoadMins * 60 ; ts += 60) {
		/* Device Loop */
		for (auto dc = 1; dc <= Config::MaxDevices ; dc++) {


			/* tables loop */
			for (auto table_id = 1; table_id <= Config::DBTables; table_id++) {
				Message m(Insert, ts, dc, table_id);
				tsQueue.push(m);
			}
			insertProgress+=Config::DBTables;
			//cout << std::fixed << std::setprecision(2) << "DBG: insertProgress" << insertProgress << endl;
			tsQueue.wait_size(Config::LoaderThreads*2);
		}

		tsQueue.wait_empty();

	}

    cout << "#\t Data Load Finished" << endl;
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
    insertProgress = startTimestamp;

    // start deleting from the oldest recorded timestamp
    uint64_t deleteTimestamp=tsRange.min;

    /* create thread printing progress */
    std::thread threadReporter(&Preparer::prepProgressPrint,
	this,
        startTimestamp,
	Config::LoadMins * 60);

    cout << "Running benchmark from ts: "
	<< startTimestamp << ", to ts: "
	<< endTimestamp
	<< ", range: "
	<< endTimestamp - startTimestamp
	<< endl;


        /* Time loop */
        for (auto ts=startTimestamp; ts <= endTimestamp; ts += 60) {

            unsigned int devicesCnt = PGen->GetNext(Config::MaxDevices, 0);
            //GenericDriver::dev_range deviceRange = DataLoader->getDeviceRange({0,0},0);
            //unsigned int oldDevicesCnt = deviceRange.max;

            /* Devices loop */
            for (auto dc = 1; dc <= devicesCnt ; dc++) {

		    /* Table/Collection loop */
		    for (auto table_id=1; table_id <= Config::DBTables; table_id++) {

			    Message m(Insert, ts, dc, table_id);
			    tsQueue.push(m);
			    /* do not do DELETE for now, add by option 
			       if (dc <= oldDevicesCnt) {
			       Message m(Delete, deleteTimestamp, dc, table_id);
			       tsQueue.push(m);
			       }
			     */
		    }
	    }


        // advance our trailing delete
        deleteTimestamp += 60;

	tsQueue.wait_size(Config::LoaderThreads*100000);
	insertProgress = ts;
    }

    cout << "#\t Benchmark Finished" << endl;
    progressLoad = false;
    /* wait on reporter to finish */
    threadReporter.join();
}

void Preparer::setLatencyStats(LatencyStats* ls)
{
  latencyStats=ls;
}

