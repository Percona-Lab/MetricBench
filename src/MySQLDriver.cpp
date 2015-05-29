#include <thread>      
#include <chrono>
#include <atomic>

#include <boost/lockfree/queue.hpp>

#include "MySQLDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"
#include "Message.hpp"

extern tsqueue<Message> tsQueue;
extern tsqueue<StatMessage> statQueue;

using namespace std;

void MySQLDriver::Prep() {
    for (auto i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this](){ InsertData(); });
	threadInsertData.detach();
    }
}

void MySQLDriver::Run(unsigned int& minTs, unsigned int& maxTs) {
        
    sql::Driver * driver = sql::mysql::get_driver_instance();                                                           
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));                                             
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    { /* block for ResultSet ptr */
	std::unique_ptr< sql::ResultSet >
	    res(stmt->executeQuery("SELECT "
			"unix_timestamp(min(period)) mints,unix_timestamp(max(period)) maxts FROM metrics"));

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

    for (auto i = 0; i < Config::LoaderThreads; i++) {
	    std::thread threadInsertData([this](){ InsertData(); });
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
	    << "WHERE period=from_unixtime(" << ts << ")";
	std::unique_ptr< sql::ResultSet > res(stmt->executeQuery(sql.str()));

	if (res->next()) {
	    maxDevID = res->getUInt("maxdev");
	} 
    }

    return maxDevID;

}

/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MySQLDriver::InsertData() {

    cout << "InsertData thread started" << endl;
    sql::Driver * driver = sql::mysql::get_driver_instance();                                                           
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));                                             

    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    Message m;

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(m);

	switch(m.op) {
	    case Insert: InsertQuery(m.ts, m.device_id, *stmt);
		break;
	    case Delete: DeleteQuery(m.ts, m.device_id, *stmt);
		break;
	}

    }

}


void MySQLDriver::InsertQuery(unsigned int timestamp, unsigned int device_id, sql::Statement & stmt) {

    stringstream sql;

    try {
	auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);

	sql.str("");
	sql << "INSERT INTO metrics(period, device_id, metric_id, cnt, val ) VALUES ";
	bool notfirst = false;

	/* metrics loop */
	for (auto mc = 1; mc <= metricsCnt; mc++) {
	    if (notfirst) {
		sql << ",";
	    }
	    notfirst = true;	
	    auto v = PGen->GetNext(0.0, Config::MaxValue);
	    sql << "(from_unixtime(" 
		<< timestamp << "), " 
		<< device_id << ", " 
		<< mc << "," 
		<< PGen->GetNext(Config::MaxCnt, 0) 
		<< ", " << (v < 0.5 ? 0 : v)  << ")";

	}

	auto t0 = std::chrono::high_resolution_clock::now();
	stmt.execute("BEGIN");
	stmt.execute(sql.str());
	stmt.execute("COMMIT");
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

	StatMessage sm(InsertMetric, time_us, metricsCnt);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
	cout << "# ERR: SQLException in " << __FILE__;
	cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
	cout << "# ERR: " << e.what();
	cout << " (MySQL error code: " << e.getErrorCode();
	cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	throw std::runtime_error ("Can't execute DELETE");
    }

}

void MySQLDriver::DeleteQuery(unsigned int timestamp, unsigned int device_id, sql::Statement & stmt) {

    stringstream sql;

    try {

	sql.str("");
	sql << "DELETE FROM metrics WHERE period=from_unixtime(" << timestamp << ")"
	    << " AND device_id=" << device_id;

	auto t0 = std::chrono::high_resolution_clock::now();
	stmt.execute("BEGIN");
	stmt.execute(sql.str());
	stmt.execute("COMMIT");
	auto t1 = std::chrono::high_resolution_clock::now();
	auto time_us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();

	StatMessage sm(DeleteDevice, time_us, 1);
	statQueue.push(sm);

    } catch (sql::SQLException &e) {
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

	stmt->execute("DROP TABLE IF EXISTS metrics");
	if (!Config::preCreateStatement.empty())
	    stmt->execute(Config::preCreateStatement);
	stmt->execute(R"(
	    CREATE TABLE metrics (
		    period timestamp NOT NULL DEFAULT '0000-00-00 00:00:00',
		    device_id int(10) unsigned NOT NULL,
		    metric_id int(10) unsigned NOT NULL,
		    cnt int(10) unsigned NOT NULL,
		    val double DEFAULT NULL,
		    PRIMARY KEY (period,device_id,metric_id),
		    KEY metric_id (metric_id,period),
		    KEY device_id (device_id,period)
		    ) ENGINE=)" + Config::storageEngine + " " + Config::storageEngineExtra +" DEFAULT CHARSET=latin1;");
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

