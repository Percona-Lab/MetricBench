#include "Stats.hpp"

#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>

#include "tsqueue.hpp"

#include "Message.hpp"

extern tsqueue<StatMessage> statQueue;

unsigned long percentile(std::vector<unsigned long> &vectorIn, double percent)
{
    auto nth = vectorIn.begin() + (percent*vectorIn.size())/100;
    std::nth_element(vectorIn.begin(), nth, vectorIn.end());
    return *nth;
}

void Stats::statsPrint() {

    auto t0 = std::chrono::high_resolution_clock::now();

    while (true) {
	std::this_thread::sleep_for (std::chrono::seconds(10));
	auto t1 = std::chrono::high_resolution_clock::now();
	auto secFromStart = std::chrono::duration_cast<std::chrono::seconds>(t1-t0).count();

	lockCounts.lock();


	auto cnts = Counts[InsertMetric];

	unsigned int biggest = 0;
	unsigned int pct99 = 0;

	if (cnts > 0) {
	    pct99 = percentile(execTimes[InsertMetric], 0.99);
	    auto tmp  = std::max_element(std::begin(execTimes[InsertMetric]),
		std::end(execTimes[InsertMetric]));
	    biggest = *tmp;
	}

	std::cout << std::fixed << std::setprecision(2)
	    << "[Stats] Time: " << secFromStart << "sec, "
	    << messageTypeLabel[InsertMetric] << ": " << cnts
	    << ", max time(us): " << biggest
	    << ", 99% time(us): " << pct99 << ", qsize: " << statQueue.size()
	    << std::endl;

	cnts = Counts[DeleteDevice];

	if (cnts > 0){

	    auto tmp  = std::max_element(std::begin(execTimes[DeleteDevice]),
		    std::end(execTimes[DeleteDevice]));
	    biggest = *tmp;
	    pct99 = percentile(execTimes[DeleteDevice], 0.99);

	    std::cout << std::fixed << std::setprecision(2)
		<< "[Stats] Time: " << secFromStart << "sec, "
		<< messageTypeLabel[DeleteDevice] << ": " << cnts
		<< ", max time(us): " << biggest
		<< ", 99% time(us): " << pct99
		<< std::endl;
	}

	Counts.clear();
	execTimes.clear();
	lockCounts.unlock();

    }

}


void Stats::Run() {

    /* create thread printing progress */
    std::thread statReporter(&Stats::statsPrint, this);

    std::cout << "Stats thread started..." << std::endl;

    StatMessage sm;

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	statQueue.wait_and_pop(sm);

	lockCounts.lock();
	Counts[sm.op] += sm.cnt;
	execTimes[sm.op].push_back(sm.time_us);
//	insertTimes.push_back(sm.time_us);
	lockCounts.unlock();
	/*
	switch(sm.op) {
	    case InsertMetric:
		lockCounts.lock();
		Counts[InsertMetric] += sm.cnt;
		insertTimes.push_back(sm.time_us);
		lockCounts.unlock();

//		std::cout << "[Stats] Recived count: "
//		    << sm.cnt << ", total: "
//		    << Counts[InsertMetric]
//		    << std::endl;
		break;
	}*/

    }

}
