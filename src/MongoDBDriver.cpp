#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_set>

#include <boost/lockfree/queue.hpp>

#include "MongoDBDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"
#include "Message.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;


using namespace std;

// initialize the first time
char mongo_init=0;

MongoDBDriver::MongoDBDriver(const string user, const string pass,
                             const string database, const string url)
                             : GenericDriver(user,pass,database,url){
    if (!mongo_init) {
        mongo::client::initialize();
        mongo_init++;
    }
}


void MongoDBDriver::Prep() {
    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i](){ InsertData(i); });
	threadInsertData.detach();
    }
}

void MongoDBDriver::Run(unsigned int& minTs, unsigned int& maxTs) {


    mongo::DBClientConnection c;

    c.connect(url);

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
    c.connect(url);

    Message m;

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(m);

	switch(m.op) {
	    case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, c);
		break;
	    case Delete: DeleteQuery(threadId, m.ts, m.device_id);
		break;
            default:
                break;
	}

    }


}


void MongoDBDriver::InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id, mongo::DBClientConnection &mongo) {

    std::vector<mongo::BSONObj> bulk_data;

	//auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(1, Config::MaxMetrics);

	auto metricsCnt = PGen->GetNext(Config::MaxMetricsPerTs, 0);


	/* metrics loop */
	std::unordered_set< int > s;
	while (s.size() < metricsCnt) {
	   auto size_b = s.size();
	   auto mc = dis(gen);
	   s.insert(mc);
	   if (size_b==s.size()) { continue; }
	//for (auto mc = 1; mc <= metricsCnt; mc++) {
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
	mongo.insert(database+".metrics"+std::to_string(table_id), bulk_data);
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
		c.connect(url);
		for (auto table=1; table <= Config::DBTables; table++)
		{
			c.dropCollection(database+".metrics"+std::to_string(table));
			c.createCollection(database+".metrics"+std::to_string(table));
		   // PRIMARY KEY (device_id, metric_id, ts),
			c.createIndex(database+".metrics"+std::to_string(table),
			mongo::IndexSpec().addKeys( BSON (
				"device_id" << 1 <<
				"metric_id" << 1 <<
				"ts" << 1 )).unique() );
		   // KEY k1 (ts, device_id, metric_id, val),
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"ts" << 1 <<
				"device_id" << 1 <<
				"metric_id" << 1 <<
				"val" << 1 ));
		   // KEY k4 (ts, metric_id, val)
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"ts" << 1 <<
				"metric_id" << 1 <<
				"val" << 1 ));
		   // KEY k2 (device_id, ts, metric_id, val),
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"device_id" << 1 <<
				"ts" << 1 <<
				"metric_id" << 1 <<
				"val" << 1 ));
		   // KEY k3 (metric_id, ts, device_id, val),
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"metric_id" << 1 <<
				"ts" << 1 <<
				"device_id" << 1 <<
				"val" << 1 ));

		}

	} catch( const mongo::DBException &e ) {
		std::cout << "caught " << e.what() << std::endl;
	}
	cout << "#\t Schema created" << endl;
}

