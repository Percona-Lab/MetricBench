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

    mongo::client::initialize();

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


}


void MongoDBDriver::InsertQuery(int threadId, 
	unsigned int table_id, 
	unsigned int timestamp, 
	unsigned int device_id) {


}

void MongoDBDriver::DeleteQuery(int threadId, unsigned int timestamp, unsigned int device_id) {


}

/* handle creation of DB schema */
void MongoDBDriver::CreateSchema() {

    mongo::client::initialize();

    mongo::DBClientConnection c;

    c.connect("localhost");

    cout << "#\t Schema created" << endl;
}

