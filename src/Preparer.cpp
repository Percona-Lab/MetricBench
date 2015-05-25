#include <thread>      
#include <chrono>
#include <atomic>

#include "Preparer.hpp"
#include "MySQLDriver.hpp"
#include "tsqueue.hpp"

extern tsqueue<unsigned int> tsQueue;

void Preparer::progressPrint() const {

    auto t0 = std::chrono::high_resolution_clock::now();

    while (progressLoad) {
	std::this_thread::sleep_for (std::chrono::seconds(10));
	auto t1 = std::chrono::high_resolution_clock::now();
	auto secFromStart = std::chrono::duration_cast<std::chrono::seconds>(t1-t0).count();

	cout << std::fixed << std::setprecision(2)
	    << "Time: " << secFromStart << "sec, " 
	    << "Progress: " << insertProgress - Config::StartTimestamp 
	    << ", % done: " << 
	    static_cast<double> (insertProgress - Config::StartTimestamp) * 100 / (Config::LoadDays * 86400) 
	    << "%," 
	    << "est total time: " << ( insertProgress > 0 ? secFromStart / (
			static_cast<double> (insertProgress - Config::StartTimestamp)  / (Config::LoadDays * 86400)
			) : 0 )
	    << endl;
    }

}

void Preparer::Run(){

    DataLoader.CreateSchema();

    DataLoader.Run();

    /* create thread printing progress */
    std::thread threadReporter([this](){ progressPrint(); });

    /* Populate the test table with data */
    for (unsigned int ts = Config::StartTimestamp; ts < Config::StartTimestamp + Config::LoadDays * 86400 ; ts += 60) {
	cout << "Timestamp: " << ts << endl;

	tsQueue.push (ts);
	insertProgress = ts;
	tsQueue.wait_empty();

    } 


    cout << "#\t Data Load Finished" << endl;
    progressLoad = false;
    /* wait on reporter to finish */
    threadReporter.join();

}

