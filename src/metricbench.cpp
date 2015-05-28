#include <stdlib.h>
#include <iostream>
#include <iomanip> 
#include <sstream>
#include <stdexcept>

#include <thread>      
#include <chrono>
#include <atomic>

#include <boost/program_options.hpp> 
#include <boost/lockfree/queue.hpp>

#include "tsqueue.hpp"
#include "Preparer.hpp"
#include "Message.hpp"
#include "Stats.hpp"

using namespace std;

namespace po = boost::program_options;

/* Global shared structures */
/* Message queue with timestamps */
tsqueue<Message> tsQueue;
tsqueue<StatMessage> statQueue;

int main(int argc, const char **argv)
{

    std::string runMode = "run";

    po::options_description desc("Commandline options");
    desc.add_options()
	("help", "help message")
	("mode", po::value<string>(&runMode)->default_value("run"), "mode: run (default) or prepare (load "
	"initial dataset)")
	("threads", po::value<unsigned int>(&Config::LoaderThreads)->default_value(8), "working "
	"threads (default: 8)")
	("engine", po::value<string>()->default_value("InnoDB"), "set storage engine (default "
	"InnoDB)")
	("engine-extra", po::value<string>(), "extra storage engine options, e.g. "
	"'ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8'")
	;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

    if (vm.count("help")) {
        cout << desc << "\n";
        return 1;
    }

    if (vm.count("engine")) {
	cout << "Using Storage engine: " 
	    << vm["engine"].as<string>() << endl;
	Config::storageEngine = vm["engine"].as<string>();
    }
    if (vm.count("engine-extra")) {
	cout << "Using Storage engine extra options: " 
	    << vm["engine-extra"].as<string>() << endl;
	Config::storageEngineExtra = vm["engine-extra"].as<string>();
    }

    string url(Config::EXAMPLE_HOST);
    const string user(Config::EXAMPLE_USER);
    const string pass(Config::EXAMPLE_PASS);
    const string database(Config::EXAMPLE_DB);


    /* prepare routine */

	ParetoGenerator PG(1.04795);

	MySQLDriver MLP(user, pass, database, url);
	MLP.SetGenerator(&PG);

	Preparer Runner(MLP);
	Runner.SetGenerator(&PG);

	Stats st;

	std::thread threadStats(&Stats::Run, &st);
	threadStats.detach();
    
	if (runMode == "prepare") {
	    cout << "PREPARE mode" << endl;
	    Config::runMode = PREPARE;
	    try {

		Runner.Prep();

		cout << "# done!" << endl;

	    } catch (std::runtime_error &e) {

		cout << "# ERR: runtime_error in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;

		return EXIT_FAILURE;
	    }
	    return EXIT_SUCCESS;
	}

	if (runMode == "run") {
	    cout << "RUN mode" << endl;
	    Config::runMode = RUN;
	    try {

		Runner.Run();

		cout << "# done!" << endl;

	    } catch (std::runtime_error &e) {

		cout << "# ERR: runtime_error in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
		cout << "# ERR: " << e.what() << endl;

		return EXIT_FAILURE;
	    }
	    return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;
}
