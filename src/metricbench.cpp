#include <stdlib.h>
#include <iostream>
#include <iomanip> 
#include <sstream>
#include <stdexcept>

#include <thread>      
#include <chrono>
#include <atomic>

#include <boost/program_options.hpp> 

#include "tsqueue.hpp"
#include "Preparer.hpp"

using namespace std;

namespace po = boost::program_options;

/* Global shared structures */
/* Message queue with timestamps */
tsqueue<unsigned int> tsQueue;


int main(int argc, const char **argv)
{
    po::options_description desc("Commandline options");
    desc.add_options()
	("help", "help message")
	("engine", po::value<string>()->default_value("InnoDB"), "set storage engine (default "
	"InnoDB)")
	("engine-extra", po::value<string>(), "extra storage engine options, e.g. "
	"'ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8'")
	;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 

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
