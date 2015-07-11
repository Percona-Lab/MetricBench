#include <thread>
#include <chrono>
#include <atomic>

#include <boost/lockfree/queue.hpp>

#include "MongoDBDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"
#include "Message.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;

using namespace std;

void MongoDBDriver::Prep() {
    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i](){ InsertData(i); });
	threadInsertData.detach();
    }
}

void MongoDBDriver::Run(unsigned int& minTs, unsigned int& maxTs) {


    mongo::DBClientConnection c;

    c.connect("localhost");

    for (int i = 0; i < Config::LoaderThreads; i++) {
	    std::thread threadInsertData([this,i](){ InsertData(i); });
	    threadInsertData.detach();
    }

}


unsigned int MongoDBDriver::getMaxDevIdForTS(unsigned int ts) {


    unsigned int maxDevID = 0;

    return maxDevID;

}

/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MongoDBDriver::InsertData(int threadId) {
    cout << "InsertData thread started" << endl;

    mongo::DBClientConnection c;
    c.connect("localhost");

    Message m;

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(m);

	switch(m.op) {
	    case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, c);
		break;
	    case Delete: DeleteQuery(threadId, m.ts, m.device_id);
		break;
	}

    }


}


void MongoDBDriver::InsertQuery(int threadId, 
	unsigned int table_id, 
	unsigned int timestamp, 
	unsigned int device_id, mongo::DBClientConnection &mongo) {

    std::vector<mongo::BSONObj> bulk_data;

	auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);


	/* metrics loop */
	for (auto mc = 1; mc <= metricsCnt; mc++) {
	    	auto v = PGen->GetNext(0.0, Config::MaxValue);
		mongo::BSONObj record = BSON (
                "ts" << timestamp <<
		"device_id" << device_id <<
		"metric_id" << mc <<
		"cnt" << PGen->GetNext(Config::MaxCnt, 0) <<
		"value" << (v < 0.5 ? 0 : v) );
	
        	bulk_data.push_back(record);

	}


	auto t0 = std::chrono::high_resolution_clock::now();
	mongo.insert("metric_bench.col"+std::to_string(table_id), bulk_data);
	bulk_data.clear();
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

        if (latencyStats) {
          latencyStats->recordLatency(threadId, InsertMetric, time_us);
        }

	StatMessage sm(InsertMetric, time_us, metricsCnt);
	statQueue.push(sm);

}

void MongoDBDriver::DeleteQuery(int threadId, unsigned int timestamp, unsigned int device_id) {


}

/* handle creation of DB schema */
void MongoDBDriver::CreateSchema() {


    try {
    mongo::DBClientConnection c;

    c.connect("localhost");

   } catch( const mongo::DBException &e ) {
        std::cout << "caught " << e.what() << std::endl;
    }
    cout << "#\t Schema created" << endl;
}

