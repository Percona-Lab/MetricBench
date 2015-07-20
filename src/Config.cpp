#include <string>
#include "Config.hpp"

namespace Config
{
    unsigned int LoaderThreads = Config::DEFAULT_LOADERTHREADS;

    unsigned int LoadMins = Config::DEFAULT_LOADMINS;
    unsigned int DBTables = Config::DEFAULT_DBTABLES;
    unsigned int MaxDevices = Config::DEFAULT_MAXDEVICES;

    std::string storageEngine = Config::DEFAULT_STORAGE_ENGINE;
    std::string storageEngineExtra = "";

    std::string preCreateStatement = "";
    
    std::string connDb = Config::DEFAULT_DB;
    std::string connHost = Config::DEFAULT_HOST;
    std::string connUser = Config::DEFAULT_USER;
    std::string connPass = Config::DEFAULT_PASS;

    RunModeE runMode = RUN;

    std::string csvStatsFile = "";
    std::string csvStreamingStatsFile = "";
}
