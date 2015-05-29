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
#include <boost/filesystem.hpp>

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

    po::options_description desc("Command line options");
    desc.add_options()
	("help", "Help message")
	("mode", po::value<string>(&runMode)->default_value(""), "Mode - run or prepare (load "
	    "initial dataset)")
        ("url",  po::value<string>(&Config::connHost)->default_value(Config::DEFAULT_HOST),
            "Connection URL, e.g. tcp://[host[:port]], unix://path/socket_file ")
        ("database", po::value<string>(&Config::connDb)->default_value(Config::DEFAULT_DB),
            "Connection Database (schema) to use")
        ("user", po::value<string>(&Config::connUser)->default_value(Config::DEFAULT_USER),
            "Connection User for login")
        ("password", po::value<string>(&Config::connPass)->default_value(Config::DEFAULT_PASS),
            "Connection Password for login")
        ("days", po::value<unsigned int>(&Config::LoadDays)->default_value(Config::DEFAULT_LOADDAYS),
            "Days of traffic to simulate")
	("threads", po::value<unsigned int>(&Config::LoaderThreads)->default_value(Config::DEFAULT_LOADERTHREADS),
            "Working threads")
	("engine", po::value<string>()->default_value(Config::DEFAULT_STORAGE_ENGINE),
            "Set storage engine")
	("engine-extra", po::value<string>(), "Extra storage engine options, e.g. "
	    "'ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8'")
	;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // usage
    if (vm.count("help")) {
        std::string appName = boost::filesystem::basename(argv[0]);
        cout << "Usage: " << appName << " --mode=[prepare|run] [options]\n";
        cout << "A windowed time-series benchmark focused on capturing metrics\n"
             << "from devices, processing and purging expired data.\n\n";
        cout << desc << "\n";
        return EXIT_FAILURE;
    }

    // require mode
    if (runMode.compare("") == 0) {
        cout << "# ERR: You must specify --mode.  Use --help for information.\n\n";
        return EXIT_FAILURE;
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

    string url(Config::connHost);
    const string user(Config::connUser);
    const string pass(Config::connPass);
    const string database(Config::connDb);


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
