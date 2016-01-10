#include "CassandraDriver.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;

using namespace std;

void CassandraDriver::Prep() {
    const int ls[] = {MessageType::InsertMetric};
    const std::vector<int> loadStats(ls, end(ls));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,loadStats](){ InsertData(i,loadStats); });
        threads.push_back(move(threadInsertData));
    }
}

void CassandraDriver::Run() {
    const int rs[] = {MessageType::InsertMetric, MessageType::DeleteDevice,  MessageType::Select_K1, MessageType::Select_K2};
    const std::vector<int> runStats(rs, end(rs));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,runStats](){ InsertData(i,runStats); });
        threads.push_back(move(threadInsertData));
    }

    std::thread threadSelectData([this,runStats](){ SelectData(runStats); });
    threads.push_back(move(threadSelectData));

}

CassError CassandraDriver::execute_query(CassSession* session, const char* query) {
  CassError rc = CASS_OK;
  CassFuture* future = NULL;
  CassStatement* statement = cass_statement_new(query, 0);

  future = cass_session_execute(session, statement);
  cass_future_wait(future);

  //cout << "Query: " << query << endl;

  rc = cass_future_error_code(future);
  if (rc != CASS_OK) {
    const char* message;
    size_t message_length;   
    cass_future_error_message(future, &message, &message_length);
    cerr <<  "Query error: " << message << endl;
  }

  cass_future_free(future);
  cass_statement_free(statement);

  return rc;
}


/* get the max timestamp so we don't duplicate existing rows */
GenericDriver::ts_range CassandraDriver::getTimestampRange(unsigned int table_id) {

    GenericDriver::ts_range ret={(uint64_t)-1,0};

    ret.min = Config::StartTimestamp;
    ret.max = Config::StartTimestamp;

    return ret;

}

// return the min/max device id range for tsRange (if tsRange.max == 0 then
// all devices)
GenericDriver::dev_range CassandraDriver::getDeviceRange(GenericDriver::ts_range tsRange, unsigned int table_id) {

    GenericDriver::dev_range ret={(unsigned int)-1,0};

}

/* This thread waits for a signal to handle timestamp event.
   For a given timestamp it loads N devices, each reported M metrics */
void CassandraDriver::InsertData(int threadId, const std::vector<int> & showStats) {

    CassFuture* connect_future = NULL;
    CassSession* session = cass_session_new();
    //cass_cluster_set_contact_points(cluster, "10.60.23.57,10.60.23.184");
    CassCluster* cluster = cass_cluster_new();
    cass_cluster_set_protocol_version(cluster, 3);
    cass_cluster_set_contact_points(cluster, "127.0.0.1");

    cass_cluster_set_num_threads_io(cluster, 4);
    cass_cluster_set_queue_size_io(cluster, 10000);
    cass_cluster_set_pending_requests_low_water_mark(cluster, 5000);
    cass_cluster_set_pending_requests_high_water_mark(cluster, 10000);
    cass_cluster_set_core_connections_per_host(cluster, 1);
    cass_cluster_set_max_connections_per_host(cluster, 2);
    cass_cluster_set_max_requests_per_flush(cluster, 10000);

    connect_future = cass_session_connect(session, cluster);

    cout << "InsertData thread started" << endl;

    Message m;

    SampledStats stats(threadId, Config::maxsamples, *ostreamSampledStats);

    int64_t lastDisplayTime =
	    std::chrono::duration_cast<std::chrono::milliseconds>(
			    std::chrono::high_resolution_clock::now().time_since_epoch()
			    ).count();

    if (cass_future_error_code(connect_future) == CASS_OK) {


	    while(true) { // TODO:: have a boolean flag to stop
		    /* wait on a event from queue */
		    tsQueue.wait_and_pop(m);

		    switch(m.op) {
			    case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, session, stats);
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
    } else {
	    /* Handle error */
	    const char* message ;
	    size_t message_length;
	    cass_future_error_message(connect_future, &message, &message_length);
	    cerr <<  "Unable to connect: " << message << endl;
    }

    cass_future_free(connect_future);
    cass_cluster_free(cluster);
    cass_session_free(session);

    int64_t endTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()
      ).count();

    if (ostreamSet) {
        stats.displayStats(lastDisplayTime, endTime, showStats);
    }

}

/* This thread waits executes select queries in a loop */
void CassandraDriver::SelectData(const std::vector<int> & showStats) {


}

void CassandraDriver::InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id, 
	CassSession* session,
        SampledStats & stats) {

    //auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);
    unsigned int seed=Config::randomSeed;
    if (!seed) {
        std::random_device rd;
        seed=rd();
    }
    std::mt19937 gen(seed);
    std::uniform_int_distribution<> dis(1, Config::MaxMetrics);

    stringstream sql;

    auto metricsCnt = PGen->GetNext(Config::MaxMetricsPerTs, 0);

    CassError rc = CASS_OK;
    CassFuture* future = NULL;
    CassBatch* batch = cass_batch_new(CASS_BATCH_TYPE_LOGGED);

    /* metrics loop */
    std::unordered_set< int > s;
    while (s.size() < metricsCnt) {
        auto size_b = s.size();
        auto mc = dis(gen);
        s.insert(mc);
        if (size_b==s.size()) {
            continue;
        }
	sql.str("");
        auto v = PGen->GetNext(0.0, Config::MaxValue);
	    sql<< "INSERT INTO metricsdata2.metrics (" <<
		  "device_id, metric_id, ts, cnt, val) VALUES (" << 
            	  device_id << "," <<
            	  mc << "," <<
		  timestamp*1000 << "," <<
            	  PGen->GetNext(Config::MaxCnt, 0) << "," <<
            	  (v < 0.5 ? 0 : v) << ");";
	    //execute_query(session,sql.str().c_str());
	    CassStatement* statement = cass_statement_new(sql.str().c_str(),0);
	    cass_batch_add_statement(batch, statement);
	    cass_statement_free(statement);
	    //cout << "Query: " << sql.str() << endl;
    }

   future = cass_session_execute_batch(session, batch);
   cass_future_wait(future);
   rc = cass_future_error_code(future);
   if (rc != CASS_OK) {
	    /* Handle error */
	    const char* message ;
	    size_t message_length;
	    cass_future_error_message(future, &message, &message_length);
	    cerr <<  "Unable to connect: " << message << endl;
   }

  cass_future_free(future);
  cass_batch_free(batch);



    auto t0 = std::chrono::high_resolution_clock::now();

    auto t1 = std::chrono::high_resolution_clock::now();
    auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

    if (ostreamSet) {
        stats.addStats(InsertMetric, time_us/1000, false);
    }

    StatMessage sm(InsertMetric, time_us, metricsCnt);
    statQueue.push(sm);

}


/* handle creation of DB schema */
void CassandraDriver::CreateSchema() {

    CassFuture* connect_future = NULL;
    CassCluster* cluster = cass_cluster_new();
    CassSession* session = cass_session_new();
    //cass_cluster_set_contact_points(cluster, "10.60.23.57,10.60.23.184");
    cass_cluster_set_contact_points(cluster, "127.0.0.1");

    connect_future = cass_session_connect(session, cluster);

    if (cass_future_error_code(connect_future) == CASS_OK) {
	    CassFuture* close_future = NULL;
	    execute_query(session,
                  "CREATE KEYSPACE metricsdata2 WITH replication = { \
                  'class': 'SimpleStrategy', 'replication_factor': '3' };");

	    execute_query(session,
                  "CREATE TABLE metricsdata2.metrics ( \
		  device_id int, \
		  metric_id int, \
		  ts timestamp, \
		  cnt int, \
		  val double, \
		  PRIMARY KEY ((device_id, metric_id), ts));");
	    close_future = cass_session_close(session);
	    cass_future_wait(close_future);
	    cass_future_free(close_future);

    } else {
	    /* Handle error */
	    const char* message ;
	    size_t message_length;
	    cass_future_error_message(connect_future, &message, &message_length);
	    cerr <<  "Unable to connect: " << message << endl;
    }

    cass_future_free(connect_future);
    cass_cluster_free(cluster);
    cass_session_free(session);

    cout << "#\t Schema created" << endl;
}
