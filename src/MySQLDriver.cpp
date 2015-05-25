#include <thread>      
#include <chrono>
#include <atomic>

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/warning.h>
#include <cppconn/metadata.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/resultset_metadata.h>
#include <cppconn/statement.h>
/*#include <cppconn/connection.h"*/
#include "mysql_driver.h"
#include "mysql_connection.h"

#include "MySQLDriver.hpp"
#include "tsqueue.hpp"
#include "Config.hpp"

extern tsqueue<unsigned int> tsQueue;

using namespace std;

void MySQLDriver::Run() {
    for (auto i=0; i < 8; i++) {
        std::thread threadInsertData([this](){ InsertData(); });
	threadInsertData.detach();
    }
}

/* This thread waits for a signal to handle timestamp event.
For a given timestamp it loads N devices, each reported M metrics */
void MySQLDriver::InsertData() {

    cout << "InsertData thread started" << endl;
    sql::Driver * driver = sql::mysql::get_driver_instance();                                                           
    std::unique_ptr< sql::Connection > con(driver->connect(url, user, pass));                                             

    con->setSchema(database);

    std::unique_ptr< sql::Statement > stmt(con->createStatement());

    stringstream sql;

    while(true) {
	/* wait on a event from queue */
	auto ts = tsQueue.wait_and_pop();

	auto devicesCnt = PGen->GetNext(Config::MaxDevices, 0);

	/* Devices loop */
	for (auto dc = 1; dc <= devicesCnt ; dc++) {

	    auto metricsCnt = PGen->GetNext(Config::MaxMetrics, 0);
	    stmt->execute("BEGIN");
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
		    << *ts << "), " 
		    << dc << ", " 
		    << mc << "," 
		    << PGen->GetNext(Config::MaxCnt, 0) 
		    << ", " << (v < 0.5 ? 0 : v)  << ")";

	    }
	    stmt->execute(sql.str());
	    stmt->execute("COMMIT");
	}
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

