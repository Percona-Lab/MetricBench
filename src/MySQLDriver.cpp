#include "MySQLDriver.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;

using namespace std;

void MySQLDriver::Prep() {
    const int ls[] = {MessageType::InsertMetric};
    const std::vector<int> loadStats(ls, end(ls));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,loadStats](){ InsertData(i,loadStats); });
        threads.push_back(move(threadInsertData));
    }
}

void MySQLDriver::Run() {
    const int rs[] = {MessageType::InsertMetric, MessageType::DeleteDevice,  MessageType::Select_K1, MessageType::Select_K2};
    const std::vector<int> runStats(rs, end(rs));

    for (int i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this,i,runStats](){ InsertData(i,runStats); });
        threads.push_back(move(threadInsertData));
    }

}


/* get the max timestamp so we don't duplicate existing rows */
GenericDriver::ts_range MySQLDriver::getTimestampRange(unsigned int table_id) {

    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    GenericDriver::ts_range ret={(uint64_t)-1,0};

    ret.min = Config::StartTimestamp;
    ret.max = Config::StartTimestamp;
// + (Config::LoadMins * 60);

    // if table_id == 0 then iterate over all tables to find the maxTimestamp
    // value
    unsigned int start_table=1;
    unsigned int end_table=Config::DBTables;

    // if table_id is specified then just look at that table
    if (table_id) {
      auto start_table = end_table = table_id;
    }

    for (auto table=start_table; table <= end_table; table++) {
        stringstream sql;
	sql.str("");
	sql << "select unix_timestamp(min(ts)) as mints, unix_timestamp(max(ts)) as maxts from metrics"
            << table;
	std::unique_ptr< sql::ResultSet > res(stmt->executeQuery(sql.str()));

	if (res->next()) {
	    ret.min = std::min(ret.min, res->getUInt64("mints"));
	    ret.max = std::max(ret.max, res->getUInt64("maxts"));
	}
        // nothing set
        if (ret.min == -1) {
          ret.min=0;
        }
    }

    stmt->close();
    con->close();

    return ret;

}

// return the min/max device id range for tsRange (if tsRange.max == 0 then
// all devices)
GenericDriver::dev_range MySQLDriver::getDeviceRange(GenericDriver::ts_range tsRange, unsigned int table_id) {

    GenericDriver::dev_range ret={(unsigned int)-1,0};

    sql::Driver * driver = sql::mysql::get_driver_instance();
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    unsigned int maxDevID = 0;

    // if table_id == 0 then iterate over all tables to find the maxTimestamp
    // value
    unsigned int start_table=1;
    unsigned int end_table=Config::DBTables;

    // if table_id is specified then just look at that table
    if (table_id) {
      auto start_table = end_table = table_id;
    }

    for (auto table=start_table; table <= end_table; table++) {
        stringstream sql;
	sql.str("");
	sql << "SELECT min(device_id) mindev, max(device_id) maxdev FROM metrics"
              << table;
        if (tsRange.max > 0) {
          sql << " WHERE ts >= from_unixtime(" << tsRange.min << ") "
              << " AND ts <= from_unixtime(" << tsRange.max << ")";
        }
        std::unique_ptr< sql::ResultSet > res(stmt->executeQuery(sql.str()));

        if (res->next()) {
            ret.min = std::min(ret.min, res->getUInt("mindev"));
            ret.max = std::max(ret.max, res->getUInt("maxdev"));
        }
        else {
            ret={0,0};
        }
      }

      stmt->close();
      con->close();

      return ret;

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

      while(!Config::processingComplete || !tsQueue.empty()  ) {
          /* wait on a event from queue */
          if (!tsQueue.empty()) {

              tsQueue.wait_and_pop(m);

              switch(m.op) {
                  case Insert: InsertQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats);
                      break;
                  case Delete: DeleteQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats);
                      break;
                  case Select_K1: SelectQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats, Select_K1);
                      break;
                  case Select_K2: SelectQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats, Select_K2);
                      break;
                  case Select_K3: SelectQuery(threadId, m.table_id, m.ts, m.device_id, *stmt, stats, Select_K3);
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
      }

      int64_t endTime =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

      stmt->close();
    con->close();
    driver->threadEnd();

    if (ostreamSet) {
        stats.displayStats(lastDisplayTime, endTime, showStats);
    }

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
	std::normal_distribution<> dis(Config::MaxMetrics/2,Config::MaxMetrics/15);

	// we only insert Config::MaxMetricsPerTs per given timestamp
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
	    int mc = std::round(dis(gen));
	    if (mc < 1) { continue; }
	    if (mc > Config::MaxMetrics) { continue; }
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

        if (ostreamSet) {
            stats.addStats(InsertMetric, time_us/1000, false);
        }

	StatMessage sm(InsertMetric, time_us, metricsCnt);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
        if (ostreamSet) {
            auto t1 = std::chrono::high_resolution_clock::now();
            auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
            stats.addStats(InsertMetric, time_ms, true);
        }
	cout << "# ERR: SQLException in " << __FILE__;
	cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	cout << "# ERR: " << e.what();
	cout << " (MySQL error code: " << e.getErrorCode();
	cout << ", SQLState: " << e.getSQLState() << " )" << endl;
        cout << "SQL: " << sql.str() << endl;
        cout << "SQL UPD: " << sql_upd.str() << endl;
	throw std::runtime_error ("Can't execute INSERT");
    }

}

void MySQLDriver::DeleteQuery(int threadId,
                              unsigned int table_id,
                              unsigned int timestamp,
                              unsigned int device_id,
                              sql::Statement & stmt,
                              SampledStats & stats) {

    stringstream sql;
    auto t0 = std::chrono::high_resolution_clock::now();

    try {

	sql.str("");
	sql << "DELETE FROM metrics"<<table_id<<" WHERE ts=from_unixtime(" << timestamp << ")"
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

        if (ostreamSet) {
          stats.addStats(DeleteDevice, time_us/1000, false);
        }

	StatMessage sm(DeleteDevice, time_us, 1);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
        if (ostreamSet) {
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

void MySQLDriver::SelectQuery(int threadId,
                              unsigned int table_id,
                              unsigned int timestamp,
                              unsigned int device_id,
                              sql::Statement & stmt,
                              SampledStats & stats, 
			      MessageType mt) {

    stringstream sql;

    unsigned int seed=Config::randomSeed;
    if (!seed) {
	std::random_device rd;
	seed=rd();
    }
    std::mt19937 gen(seed);
    std::normal_distribution<> dis(Config::MaxMetrics/2,Config::MaxMetrics/15);

    auto t0 = std::chrono::high_resolution_clock::now();

    try {

	sql.str("");
	switch(mt) {
		case Select_K1:
			sql << "SELECT count(distinct metric_id) FROM metrics"<<table_id
				<< " WHERE ts >= DATE_SUB(from_unixtime(" << timestamp << "), INTERVAL 1 HOUR)"
				<< " AND device_id=" << device_id;
			break;
		case Select_K2: {
			int metric_id = std::round(dis(gen));
			if (metric_id < 1) { metric_id=1; }
			if (metric_id > Config::MaxMetrics) { metric_id=Config::MaxMetrics; }
			sql << "SELECT ts,device_id,val FROM metrics"<<table_id
				<< " WHERE ts >= DATE_SUB(from_unixtime(" << timestamp << "), INTERVAL 20 MINUTE)"
				<< " AND metric_id=" << metric_id;
			break; }
		case Select_K3:
			sql << "SELECT device_id,metric_id,avg(val) FROM metrics"<<table_id
				<< " WHERE ts >= DATE_SUB(from_unixtime(" << timestamp << "), INTERVAL 5 MINUTE)"
				<< " GROUP BY 1,2 LIMIT 100";
			break;
		default:
			break;
	}
	

	t0 = std::chrono::high_resolution_clock::now();
	auto res = stmt.executeQuery(sql.str());
	auto rows = 0;
	while (res->next()) {
		rows++;
	}
	delete res;

	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

        //if (latencyStats) {
        //  latencyStats->recordLatency(threadId, Select, time_us);
        //}

        if (ostreamSet) {
          stats.addStats(mt, time_us/1000, false);
        }

	StatMessage sm(mt, time_us, 1);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1-t0).count();
        if (ostreamSet) {
          stats.addStats(mt, time_ms, true);
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
		    KEY k1 (device_id, ts, metric_id, val),\
		    KEY k2 (metric_id, ts, val),\
		    KEY k3 (ts, val)\
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

    con->close();
}


