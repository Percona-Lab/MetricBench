#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_set>

#include <boost/lockfree/queue.hpp>

#include "MongoDBDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"
#include "Message.hpp"
#include "Uri.hpp"

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

bool MongoDBDriver::getConnection(mongo::DBClientConnection &conn) {

    Uri u = Uri::Parse(url);
    int p=-1; // default
    if (!u.Port.empty()) {
        try {
            p = stoi(u.Port,nullptr,10);
        } catch(...) {
        }
    }

    mongo::HostAndPort hp(u.Host,p);

    std::string errmsg = url;

    return(conn.connect(hp,errmsg));

}

void MongoDBDriver::Prep() {
    const int ls[] = {MessageType::InsertMetric};
    const std::vector<int> loadStats(ls, end(ls));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,loadStats](){ InsertData(i,loadStats); });
	threadInsertData.detach();
    }
}

void MongoDBDriver::Run() {
    const int ls[] = {MessageType::InsertMetric,MessageType::DeleteDevice};
    const std::vector<int> runStats(ls, end(ls));

    for (int i = 0; i < Config::LoaderThreads; i++) {
	    std::thread threadInsertData([this,i,runStats](){ InsertData(i,runStats); });
	    threadInsertData.detach();
    }

}


GenericDriver::dev_range MongoDBDriver::getDeviceRange(GenericDriver::ts_range, unsigned int collection_id) {

    // TODO: implement

    GenericDriver::dev_range ret;

    return ret;

}

GenericDriver::ts_range MongoDBDriver::getTimestampRange(unsigned int collection_id) {

    mongo::DBClientConnection c;
    getConnection(c);

    GenericDriver::ts_range ret={(uint64_t)-1,0};

    ret.min = Config::StartTimestamp;
    ret.max = Config::StartTimestamp;
    unsigned int start_table=1;
    unsigned int end_table=Config::DBTables;

    if (collection_id) {
      auto start_table = end_table = collection_id;
    }

    for (auto table=start_table; table <= end_table; table++) {
	//auto_ptr<mongo::DBClientCursor> cursor = c.query(
	//	database+".metrics"+std::to_string(table), 
	//	 mongo::Query().sort("ts", -1));
	 mongo::BSONObj max_ts= c.findOne(
		database+".metrics"+std::to_string(table), 
		 mongo::Query().sort("ts", -1));
	if (!max_ts.isEmpty()) { 
                auto be = max_ts.getField("ts");
	    	ret.max = std::max(ret.max, static_cast<long unsigned int>(be.Int()));
        } 
	/*
	if (res->next()) {
	    ret.min = std::min(ret.min, res->getUInt64("mints"));
	    ret.max = std::max(ret.max, res->getUInt64("maxts"));
	}
	*/
        // nothing set
        if (ret.min == -1) {
          ret.min=0;
        }
    }

    return ret;

}


/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MongoDBDriver::InsertData(int threadId, const std::vector<int> & showStats) {
    cout << "InsertData thread started" << endl;

    mongo::DBClientConnection c;
    getConnection(c);

    Message m;

    SampledStats stats(threadId, Config::maxsamples, *ostreamSampledStats);

    int64_t lastDisplayTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()
      ).count();

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(m);

	switch(m.op) {
	    case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, c, stats);
		break;
	    case Delete: DeleteQuery(threadId, m.ts, m.device_id, c, stats);
		break;
            default:
                break;
	}

        int64_t endTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();

        if (ostreamSet && endTime - lastDisplayTime > Config::displayFreq * 1000) {
          stats.displayStats(lastDisplayTime, endTime, showStats);
          lastDisplayTime = endTime;
        }

    }

    int64_t endTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()
      ).count();

    if (ostreamSet) {
        stats.displayStats(lastDisplayTime, endTime, showStats);
    }

}


void MongoDBDriver::InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id, mongo::DBClientConnection & mongo,
        SampledStats & stats) {

    std::vector<mongo::BSONObj> bulk_data;

    //auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);
    unsigned int seed=Config::randomSeed;
    if (!seed) {
        std::random_device rd;
        seed=rd();
    }
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(1, Config::MaxMetrics);

    auto metricsCnt = PGen->GetNext(Config::MaxMetricsPerTs, 0);

    /* metrics loop */
    std::unordered_set< int > s;
    while (s.size() < metricsCnt) {
        auto size_b = s.size();
        auto mc = dis(gen);
        s.insert(mc);
        if (size_b==s.size()) {
            continue;
        }
        auto v = PGen->GetNext(0.0, Config::MaxValue);
        mongo::BSONObj record = BSON (
            "ts" << timestamp <<
            "device_id" << device_id <<
            "metric_id" << mc <<
            "cnt" << PGen->GetNext(Config::MaxCnt, 0) <<
            "value" << (v < 0.5 ? 0 : v)
        );

        bulk_data.push_back(record);
    }

    auto t0 = std::chrono::high_resolution_clock::now();
    mongo.insert(database+".metrics"+std::to_string(table_id), bulk_data);
    bulk_data.clear();
    auto t1 = std::chrono::high_resolution_clock::now();
    auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

    //if (latencyStats) {
    //    latencyStats->recordLatency(threadId, InsertMetric, time_us);
    //}

    if (ostreamSet) {
        stats.addStats(InsertMetric, time_us/1000, false);
    }

    StatMessage sm(InsertMetric, time_us, metricsCnt);
    statQueue.push(sm);

}

void MongoDBDriver::DeleteQuery(int threadId,
                                unsigned int timestamp,
                                unsigned int device_id,
                                mongo::DBClientConnection & mongo,
                                SampledStats & stats) {


}

/* handle creation of DB schema */
void MongoDBDriver::CreateSchema() {


	try {
		mongo::DBClientConnection c;
                getConnection(c);
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
		   // KEY k1 (device_id, ts, metric_id, val),
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"device_id" << 1 <<
				"ts" << 1 <<
				"metric_id" << 1 <<
				"val" << 1 ));
		   // KEY k2 (metric_id, ts, val),
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"metric_id" << 1 <<
				"ts" << 1 <<
				"val" << 1 ));
		   // KEY k3 (ts, val)
			c.createIndex(database+".metrics"+std::to_string(table),
				BSON (
				"ts" << 1 <<
				"val" << 1 ));

		}

	} catch( const mongo::DBException &e ) {
		std::cout << "caught " << e.what() << std::endl;
	}
	cout << "#\t Schema created" << endl;
}

