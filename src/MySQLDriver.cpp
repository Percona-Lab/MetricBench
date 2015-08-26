#include "MySQLDriver.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;

using namespace std;

void MySQLDriver::Prep() {
    const int ls[] = {MessageType::InsertMetric};
    const std::vector<int> loadStats(ls, end(ls));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,loadStats](){ InsertData(i,loadStats); });
	threadInsertData.detach();
    }
}

void MySQLDriver::Run(unsigned int& minTs, unsigned int& maxTs) {
    const int rs[] = {MessageType::InsertMetric, MessageType::DeleteDevice};
    const std::vector<int> runStats(rs, end(rs));

    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    { /* block for ResultSet ptr */
	std::unique_ptr< sql::ResultSet >
	    res(stmt->executeQuery("SELECT "
			"unix_timestamp(min(ts)) mints,unix_timestamp(max(ts)) maxts FROM metrics"));

	if (res->next()) {
	    minTs = res->getUInt("mints");
	    maxTs = res->getUInt("maxts");
	    tsRange = maxTs - minTs;
	    cout << "MinTS: "<< minTs
		<< ", MaxTS: " << maxTs
		<< ", Range: "<< (maxTs-minTs) << endl;
	} else {
	    throw std::runtime_error ("SELECT timestamp from metrics returns no result");
	}
    }

    for (int i = 0; i < Config::LoaderThreads; i++) {
	    std::thread threadInsertData([this,i,runStats](){ InsertData(i,runStats); });
	    threadInsertData.detach();
    }

}


unsigned int MySQLDriver::getMaxDevIdForTS(unsigned int ts) {

    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    unsigned int maxDevID = 0;

    { /* block for ResultSet ptr */
        stringstream sql;
	sql.str("");
	sql << "SELECT max(device_id) maxdev FROM metrics "
	    << "WHERE ts=from_unixtime(" << ts << ")";
	std::unique_ptr< sql::ResultSet > res(stmt->executeQuery(sql.str()));

	if (res->next()) {
	    maxDevID = res->getUInt("maxdev");
	}
    }

    return maxDevID;

}

/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MySQLDriver::InsertData(int threadId, const std::vector<int> & showStats) {

    cout << "InsertData thread started" << endl;
    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));
    SampledStats stats(threadId, Config::maxsamples, *ostreamSampledStats);

    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    Message m;

    int64_t lastDisplayTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()
      ).count();

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(m);

	switch(m.op) {
	    case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats);
		break;
	    case Delete: DeleteQuery(threadId, m.ts, m.device_id, *stmt, stats);
		break;
            default:
                break;
	}

        int64_t endTime =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count();

        if (endTime - lastDisplayTime > Config::displayFreq * 1000) {
          stats.displayStats(lastDisplayTime, endTime, showStats);
          lastDisplayTime = endTime;
        }

    }

    int64_t endTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::high_resolution_clock::now().time_since_epoch()
      ).count();

    stats.displayStats(lastDisplayTime, endTime, showStats);

}


void MySQLDriver::InsertQuery(int threadId,
	unsigned int table_id,
	unsigned int timestamp,
	unsigned int device_id,
	sql::Statement & stmt,
        SampledStats & stats) {

    stringstream sql,sql_val,sql_upd,sql_upd_val;

    auto t0 = std::chrono::high_resolution_clock::now();

    try {
        unsigned int seed=Config::randomSeed;
        if (!seed) {
          std::random_device rd;
          seed=rd();
        }
	std::mt19937 gen(seed);
	std::uniform_int_distribution<> dis(1, Config::MaxMetrics);

	auto metricsCnt = PGen->GetNext(Config::MaxMetricsPerTs, 0);

	sql.str("");
	sql_val.str("");
	sql_upd_val.str("");
	sql << "INSERT INTO metrics"<<table_id<<" (ts, device_id, metric_id, cnt, val ) VALUES ";
	bool notfirst = false;

	/* metrics loop */
	std::unordered_set< int > s;
	while (s.size() < metricsCnt) {
	    auto size_b = s.size();
	    auto mc = dis(gen);
	    s.insert(mc);
	    if (size_b==s.size()) { continue; }

            //for (auto mc = 1; mc <= metricsCnt; mc++) {
	    if (notfirst) {
		sql_val << ",";
		sql_upd_val << ",";
	    }
	    notfirst = true;
	    auto v = PGen->GetNext(0.0, Config::MaxValue);
	    auto cnt = PGen->GetNext(Config::MaxCnt, 0);
	    sql_val << "(from_unixtime("
		<< timestamp << "), "
		<< device_id << ", "
		<< mc << ","
		<< cnt << ","
		<< (v < 0.5 ? 0 : v)  << ")";
	    sql_upd_val << "(from_unixtime(("
		<< timestamp << " div 300)*300), "
		<< device_id << ", "
		<< mc << ","
		<< cnt << ","
		<< (v < 0.5 ? 0 : v)  << ")";

	}

	t0 = std::chrono::high_resolution_clock::now();
	stmt.execute("BEGIN");
	sql << sql_val.str();
	stmt.execute(sql.str());

	sql_upd.str("");
	sql_upd << "INSERT INTO metrics_sum"<<table_id<<" (ts, device_id, metric_id, cnt, val) VALUES ";
	sql_upd << sql_upd_val.str();
	sql_upd << "ON DUPLICATE KEY UPDATE cnt=cnt+VALUES(cnt), val=val+VALUES(val)";
	//stmt.execute(sql_upd.str());
	stmt.execute("COMMIT");
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

        if (latencyStats) {
            latencyStats->recordLatency(threadId, InsertMetric, time_us);
        }

        if (ostreamSampledStats) {
            stats.addStats(InsertMetric, time_us/1000, false);
        }

	StatMessage sm(InsertMetric, time_us, metricsCnt);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
        if (ostreamSampledStats) {
            auto t1 = std::chrono::high_resolution_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
            stats.addStats(InsertMetric, time_ms, true);
        }
	cout << "# ERR: SQLException in " << __FILE__;
	cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	cout << "# ERR: " << e.what();
	cout << " (MySQL error code: " << e.getErrorCode();
	cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	throw std::runtime_error ("Can't execute DELETE");
    }

}

void MySQLDriver::DeleteQuery(int threadId,
                              unsigned int timestamp,
                              unsigned int device_id,
                              sql::Statement & stmt,
                              SampledStats & stats) {

    stringstream sql;
    auto t0 = std::chrono::high_resolution_clock::now();

    try {

	sql.str("");
	sql << "DELETE FROM metrics WHERE ts=from_unixtime(" << timestamp << ")"
	    << " AND device_id=" << device_id;

	t0 = std::chrono::high_resolution_clock::now();
	stmt.execute("BEGIN");
	stmt.execute(sql.str());
	stmt.execute("COMMIT");
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

        if (latencyStats) {
          latencyStats->recordLatency(threadId, DeleteDevice, time_us);
        }

        if (ostreamSampledStats) {
          stats.addStats(DeleteDevice, time_us/1000, false);
        }

	StatMessage sm(DeleteDevice, time_us, 1);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
        if (ostreamSampledStats) {
          stats.addStats(DeleteDevice, time_ms, true);
        }
	cout << "# ERR: SQLException in " << __FILE__;
	cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	cout << "# ERR: " << e.what();
	cout << " (MySQL error code: " << e.getErrorCode();
	cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	throw std::runtime_error ("Can't execute DELETE");
    }

}

/* handle creation of DB schema */
void MySQLDriver::CreateSchema() {

    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));

    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    try {
	for (auto table=1; table <= Config::DBTables; table++)
	{
		stmt->execute("DROP TABLE IF EXISTS metrics" + std::to_string(table));
		if (!Config::preCreateStatement.empty())
			stmt->execute(Config::preCreateStatement);
	stmt->execute("CREATE TABLE metrics" + std::to_string(table) + 
		    " (ts timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',\
		    device_id int(10) unsigned NOT NULL,\
		    metric_id int(10) unsigned NOT NULL,\
		    cnt int(10) unsigned NOT NULL,\
		    val double DEFAULT NULL,\
		    PRIMARY KEY (device_id, metric_id, ts),\
		    KEY k1 (ts, device_id, metric_id, val),\
		    KEY k2 (device_id, ts, metric_id, val),\
		    KEY k3 (metric_id, ts, device_id, val),\
		    KEY k4 (ts, metric_id, val)\
		    ) ENGINE=" + Config::storageEngine + " " + Config::storageEngineExtra +" DEFAULT CHARSET=latin1;");

//	stmt->execute("DROP TABLE IF EXISTS metrics_sum" + std::to_string(table));
//	stmt->execute("CREATE TABLE metrics_sum"+ std::to_string(table) + " LIKE metrics1");
//	stmt->execute("ALTER TABLE metrics_sum"+ std::to_string(table) + " drop primary key, drop column id,add primary key(ts,device_id,metric_id)");

	}
    } catch (sql::SQLException &e) {
	cout << "# ERR: SQLException in " << __FILE__;
	cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	cout << "# ERR: " << e.what();
	cout << " (MySQL error code: " << e.getErrorCode();
	cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	throw std::runtime_error ("Can't create DB schema");
    }

    cout << "#\t Schema created" << endl;
}

