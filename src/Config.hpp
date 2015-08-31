#pragma once

#include <string>

using namespace std;

enum  RunModeE { PREPARE = 1, RUN };

/* Connection parameter and sample data */
namespace Config
{
    constexpr auto DEFAULT_DB = "test" ;
    constexpr auto DEFAULT_HOST = "tcp://localhost";
    constexpr auto DEFAULT_USER = "root";
    constexpr auto DEFAULT_PASS = "";

    constexpr unsigned int DEFAULT_LOADERTHREADS = 8;

    constexpr unsigned int DEFAULT_LOADMINS = 6 * 60;
    constexpr unsigned int DEFAULT_DBTABLES = 10;
    constexpr unsigned int DEFAULT_MAXDEVICES = 1000;

    constexpr auto DEFAULT_STORAGE_ENGINE = "InnoDB";

    constexpr int DEFAULT_MAXSAMPLES = 10000;

    extern std::string connDb;
    extern std::string connHost;
    extern std::string connUser;
    extern std::string connPass;

    constexpr uint64_t StartTimestamp = 946684800;
    constexpr unsigned int SecInDay = 24*60*60;

    // TODO:  Make these configurable
    constexpr unsigned int MaxMetrics = 1000;
    constexpr unsigned int MaxMetricsPerTs = 100;
    constexpr double MaxValue = 10000.0;
    constexpr unsigned int MaxCnt = 60;

    extern unsigned int LoadMins;

    extern unsigned int LoaderThreads;
    extern unsigned int DBTables;
    extern unsigned int MaxDevices;
    extern std::string storageEngine;
    extern std::string storageEngineExtra;
    extern std::string preCreateStatement;

    extern RunModeE runMode;

    extern std::string csvStatsFile;
    extern std::string csvStreamingStatsFile;

    extern int maxsamples;  // maximum samples to store for each per-thread statistic

    constexpr unsigned int DEFAULT_RANDOM_SEED=0;
    extern unsigned int randomSeed;

    constexpr int DEFAULT_DISPLAY_FREQ=10;
    extern int displayFreq;  // display frequency

    // used to instruct driver threads to exit
    extern bool processingComplete;

    // log level
    constexpr auto DEFAULT_LOG_LEVEL = "info";
    extern std::string logLevel;

    // log file
    constexpr auto DEFAULT_LOG_FILE = "-";   // - for stdout
    extern std::string logFile;

}
