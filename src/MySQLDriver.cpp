#include <thread>      
#include <chrono>
#include <atomic>


#include "MySQLDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"

extern tsqueue<unsigned int> tsQueue;

using namespace std;

void MySQLDriver::Prep() {
    for (auto i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this](){ InsertData(); });
	threadInsertData.detach();
    }
}

unsigned int MySQLDriver::Run() {
    
    sql::Driver * driver = sql::mysql::get_driver_instance();                                                           
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));                                             
    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    unsigned int maxTs;
    
    { /* block for ResultSet ptr */
	std::unique_ptr< sql::ResultSet > res(stmt->executeQuery("SELECT "
		    "unix_timestamp(min(period)) mints,unix_timestamp(max(period)) maxts FROM metrics"));

	if (res->next()) {
	    auto minTs = res->getUInt("mints");
	    maxTs = res->getUInt("maxts");
	    tsRange = maxTs - minTs;
	    cout << "MinTS: "<< minTs 
		<< ", MaxTS: " << maxTs 
		<< ", Range: "<< (maxTs-minTs) 
		<< endl;
	} else {
	    throw std::runtime_error ("SELECT timestamp from metrics returns no result");
	}
    }

    for (auto i = 0; i < Config::LoaderThreads; i++) {
        std::thread threadInsertData([this](){ InsertData(); });
	threadInsertData.detach();
    }

    return maxTs;

}

/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MySQLDriver::InsertData() {

    cout << "InsertData thread started" << endl;
    sql::Driver * driver = sql::mysql::get_driver_instance();                                                           
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));                                             

    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());


    unsigned int ts;

    while(true) { // TODO:: have a boolean flag to stop
	/* wait on a event from queue */
	tsQueue.wait_and_pop(ts);
	cout << "Insert thread received ts: " << ts << endl;

	InsertQuery(ts, *stmt);

	if (Config::runMode == RUN) {
	    DeleteQuery( ts - tsRange - 60, *stmt);
	}

    }

}


void MySQLDriver::InsertQuery(unsigned int timestamp, sql::Statement & stmt) {
	auto devicesCnt = PGen->GetNext(Config::MaxDevices, 0);

	stringstream sql;

	/* Devices loop */
	for (auto dc = 1; dc <= devicesCnt ; dc++) {

	    auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);
	    stmt.execute("BEGIN");
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
		    << dc << ", " 
		    << mc << "," 
		    << PGen->GetNext(Config::MaxCnt, 0) 
		    << ", " << (v < 0.5 ? 0 : v)  << ")";

	    }
	    stmt.execute(sql.str());
	    stmt.execute("COMMIT");
	}

}

void MySQLDriver::DeleteQuery(unsigned int timestamp, sql::Statement & stmt) {

    stringstream sql;
    cout << "Delete received ts: " << timestamp << endl;

    try {

	stmt.execute("BEGIN");
	sql.str("");
	sql << "DELETE FROM metrics WHERE period=from_unixtime(" << timestamp << ")";
	stmt.execute(sql.str());
	stmt.execute("COMMIT");

	/* Devices loop */
	/*for (auto dc = 1; dc <= Config::MaxDevices; dc++) {
	    stmt.execute("BEGIN");
	    sql.str("");
	    sql << "DELETE FROM metrics WHERE period=from_unixtime(" << timestamp
		<< ") AND device_id=" << dc;
	    stmt.execute(sql.str());
	    stmt.execute("COMMIT");
	}*/
	cout << "Delete finished ts: " << timestamp << endl;
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

