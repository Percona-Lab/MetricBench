#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <stdlib.h>
#include <thread>

#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/filesystem.hpp>

#include "Logger.hpp"
#include "Config.hpp"
#include "LatencyStats.hpp"
#include "Message.hpp"
#include "Preparer.hpp"
#include "SampledStats.hpp"
#include "Stats.hpp"
#include "tsqueue.hpp"

using namespace std;

namespace po = boost::program_options;

namespace logging = boost::log;

/* Global shared structures */
/* Message queue with timestamps */
tsqueue<Message> tsQueue;
tsqueue<StatMessage> statQueue;

int main(int argc, const char **argv)
{

    std::string runMode = "run";
    std::string runDriver = "mysql";

    // default log level
    std::string logLevelString = loglevel_e_Label[Config::DEFAULT_LOG_LEVEL];
    boost::algorithm::to_lower(logLevelString);

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
            "Statistics display frequency in seconds, for standard output and CSV streaming.")
        ("loglevel", po::value<string>(&logLevelString)->default_value(logLevelString),
            "Log level fatal,error,warning,info,debug,trace")
        ("logfile", po::value<std::string>(&Config::logFile)->default_value(Config::DEFAULT_LOG_FILE),
            "Log file, use - for standard output.")
        ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    // usage (always goes to standard output)
    if (vm.count("help")) {
        std::string appName = boost::filesystem::basename(argv[0]);
        cout << "Usage: " << appName << " --mode=[prepare|run] [options]\n";
        cout << "A windowed time-series benchmark focused on capturing metrics\n"
             << "from devices, processing and purging expired data.\n\n";
        cout << desc << "\n";
        return EXIT_FAILURE;
    }

    // initialize logger
    if (vm.count("loglevel")) {
        boost::algorithm::to_lower(logLevelString);
        for (int l=logFATAL; l < logEND; l++) {
            if (boost::iequals(logLevelString, loglevel_e_Label[l])) {
              Config::logLevel=(loglevel_e)l;
            }
        }
    }

    // initialize log file
    std::ofstream logstream;
    if (vm.count("logfile")) {
        if (Config::logFile != "-") {
            logstream.open(Config::logFile,
                           std::ofstream::out);
            if (logstream.fail()) {
              log(logERROR) << "Could not open --logfile file for output: " <<
                Config::logFile;
              return EXIT_FAILURE;
            } else {
                Config::log = &logstream;
            }
        }
    }

    // require mode
    if (runMode.compare("") == 0) {
        log(logERROR) << "ERROR: You must specify --mode.  Use --help for information.";
        return EXIT_FAILURE;
    }

    if (runDriver.compare("") == 0) {
        log(logERROR) << "ERROR: You must specify --driver.  Use --help for information.";
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
        log(logERROR) << "The mongodb driver only supports tcp:// connection types.";
        return EXIT_FAILURE;
    }

    // csvstream (SampledStats)
    std::ofstream csvstream;
    if (Config::csvStreamingStatsFile.compare("")) {
      csvstream.open(Config::csvStreamingStatsFile,
                     std::ofstream::out);
      if (csvstream.fail()) {
        log(logERROR) << "Could not open --csvstream file for output: " <<
          Config::csvStreamingStatsFile;
        return EXIT_FAILURE;
      }
    }

    // report driver
    log(logINFO) << ("Using Database Driver: " + runDriver);

    // storage engine cannot be specified at runtime for mongodb
    if (vm.count("engine") && runDriver != "mongodb") {
        log(logINFO) << ( "Using Storage engine: "
	    + vm["engine"].as<string>() );
	Config::storageEngine = vm["engine"].as<string>();
    }
    if (vm.count("engine-extra")) {
	log(logINFO) << ( "Using Storage engine extra options: "
	    + vm["engine-extra"].as<string>() );
	Config::storageEngineExtra = vm["engine-extra"].as<string>();
    }
    if (vm.count("pre-create")) {
	log(logINFO) << ( "Using pre-create statement: "
	    + vm["pre-create"].as<string>() );
	Config::preCreateStatement = vm["pre-create"].as<string>();
    }

    string url(Config::connHost);
    const string user(Config::connUser);
    const string pass(Config::connPass);
    const string database(Config::connDb);

    LatencyStats latencyStats(Config::LoaderThreads);

    /* prepare routine */

    ParetoGenerator PG(1.04795);

    GenericDriver *GenDrive;

    if (runDriver == "mysql") {
        GenDrive = new MySQLDriver(user, pass, database, url);
    } else if (runDriver == "mongodb") {
	GenDrive = new MongoDBDriver(user, pass, database, url);
    } else if (runDriver == "cassandra") {
	GenDrive = new CassandraDriver(user, pass, database, url);
    }

    GenDrive->SetGenerator(&PG);
    GenDrive->setLatencyStats(&latencyStats);

    if (csvstream.is_open()) {
      SampledStats::writeCSVHeader(csvstream);
      GenDrive->setOstreamSampledStats(&csvstream);
    }

    Preparer Runner(GenDrive);
    Runner.SetGenerator(&PG);

    int retval=EXIT_SUCCESS;

    Stats st;

    std::thread threadStats(&Stats::Run, &st);
    threadStats.detach();

    if (runMode == "prepare") {

        log(logINFO) << "PREPARE mode";
        Config::runMode = PREPARE;
        try {

            Runner.Prep();

            log(logINFO) << "# done!";

        } catch (std::runtime_error &e) {

            log(logERROR) << "runtime_error in " << __FILE__;
            log(logERROR) << "(" << __FUNCTION__ << ") on line " << __LINE__;
            log(logERROR) << "# ERR: " << e.what();

            retval = EXIT_FAILURE;
        }

    }

    else if (runMode == "run") {

        log(logINFO) << "RUN mode";
        Config::runMode = RUN;
        try {

            Runner.Run();

            log(logINFO) << "# done!";

        } catch (std::runtime_error &e) {

            log(logERROR) << "# ERR: runtime_error in " << __FILE__;
            log(logERROR) << "(" << __FUNCTION__ << ") on line " << __LINE__;
            log(logERROR) << "# ERR: " << e.what();

            retval = EXIT_FAILURE;
        }

    }

    // wait for driver processing to complete
    Config::processingComplete = true;
    GenDrive->JoinThreads();

    // print latency statistics
    latencyStats.displayLatencyStats();

    return retval;
}
