#include <stdlib.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <fstream>

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
#include "LatencyStats.hpp"
#include "SampledStats.hpp"

using namespace std;

namespace po = boost::program_options;

/* Global shared structures */
/* Message queue with timestamps */
tsqueue<Message> tsQueue;
tsqueue<StatMessage> statQueue;

int main(int argc, const char **argv)
{

    std::string runMode = "run";
    std::string runDriver = "mysql";

    // TODO: read these from response file
    po::options_description desc("Command line options");
    desc.add_options()
	("help", "Help message")
	("driver", po::value<string>(&runDriver)->default_value(runDriver), "Driver: mysql or mongodb")
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
        ("mins", po::value<unsigned int>(&Config::LoadMins)->default_value(Config::DEFAULT_LOADMINS),
            "minutes of traffic to simulate")
        ("tables", po::value<unsigned int>(&Config::DBTables)->default_value(Config::DEFAULT_DBTABLES),
            "How many DB tables (collections) to use")
        ("devices", po::value<unsigned int>(&Config::MaxDevices)->default_value(Config::DEFAULT_MAXDEVICES),
            "How many devices to populate")
	("threads", po::value<unsigned int>(&Config::LoaderThreads)->default_value(Config::DEFAULT_LOADERTHREADS),
            "Working threads")
	("engine", po::value<string>()->default_value(Config::DEFAULT_STORAGE_ENGINE),
            "Set storage engine")
	("engine-extra", po::value<string>(), "Extra storage engine options, e.g. "
	    "'ROW_FORMAT=COMPRESSED KEY_BLOCK_SIZE=8'")
        ("csvstats", po::value<string>(&Config::csvStatsFile), "CSV final summary stats file.")
        ("csvstream", po::value<string>(&Config::csvStreamingStatsFile), "CSV periodic streaming stats file.")
        ("maxsamples", po::value<int>(&Config::maxsamples)->default_value(Config::DEFAULT_MAXSAMPLES),
            "Maximum number of samples to store for each per-thread statistic")
        ("pre-create", po::value<string>(), "statement(s) to execute before creating table, e.g. "
	    "'SET tokudb_read_block_size=32K'")
        ("random-seed", po::value<unsigned int>(&Config::randomSeed)->default_value(Config::DEFAULT_RANDOM_SEED),
            "Random seed for pseudo-random numbers\n" "(0 = non-deterministic seed)")
        ("displayfreq", po::value<int>(&Config::displayFreq)->default_value(Config::DEFAULT_DISPLAY_FREQ),
            "Statistics display frequency in seconds")
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

    if (runDriver.compare("") == 0) {
        cout << "# ERR: You must specify --driver.  Use --help for information.\n\n";
        return EXIT_FAILURE;
    }

    // url fixups
    //
    // if protocol wasn't specified make it tcp://
    if (Config::connHost.find("://") == string::npos) {
      Config::connHost.insert(0,"tcp://");
    }
    // mongodb only supports tcp
    if (runDriver == "mongodb" &&
        Config::connHost.find("tcp://") == string::npos) {
        cout << "# ERR: The mongodb driver only supports tcp:// connection types." << endl;
        return EXIT_FAILURE;
    }

    // report driver
    cout << "Using Database Driver: " << runDriver << endl;

    // storage engine cannot be specified at runtime for mongodb
    if (vm.count("engine") && runDriver != "mongodb") {
	cout << "Using Storage engine: "
	    << vm["engine"].as<string>() << endl;
	Config::storageEngine = vm["engine"].as<string>();
    }
    if (vm.count("engine-extra")) {
	cout << "Using Storage engine extra options: "
	    << vm["engine-extra"].as<string>() << endl;
	Config::storageEngineExtra = vm["engine-extra"].as<string>();
    }
    if (vm.count("pre-create")) {
	cout << "Using pre-create statement: "
	    << vm["pre-create"].as<string>() << endl;
	Config::preCreateStatement = vm["pre-create"].as<string>();
    }

    string url(Config::connHost);
    const string user(Config::connUser);
    const string pass(Config::connPass);
    const string database(Config::connDb);

    LatencyStats latencyStats(Config::LoaderThreads);


    // csvstream (SampledStats)
    std::ofstream csvstream;
    if (Config::csvStreamingStatsFile.compare("")) {
      csvstream.open(Config::csvStreamingStatsFile,
                     std::ofstream::out);
      if (csvstream.fail()) {
        cout << "# ERR: Could not open --csvstream file for output: " <<
          Config::csvStreamingStatsFile << std::endl;
        return EXIT_FAILURE;
      }
    }

    /* prepare routine */

    ParetoGenerator PG(1.04795);

    GenericDriver *GenDrive;

    if (runDriver == "mysql") {
        GenDrive = new MySQLDriver(user, pass, database, url);
    } else if (runDriver == "mongodb") {
	GenDrive = new MongoDBDriver(user, pass, database, url);
    }

    GenDrive->SetGenerator(&PG);
    GenDrive->setLatencyStats(&latencyStats);

    if (csvstream.is_open()) {
      SampledStats::writeCSVHeader(csvstream);
      GenDrive->setOstreamSampledStats(&csvstream);
    }

    Preparer Runner(GenDrive);
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

        // print latency statistics
        latencyStats.displayLatencyStats();

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

        // print latency statistics
        latencyStats.displayLatencyStats();

        return EXIT_SUCCESS;
    }

    return EXIT_SUCCESS;
}
