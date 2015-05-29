#include <string>
#include "Config.hpp"

namespace Config
{
    unsigned int LoaderThreads = Config::DEFAULT_LOADERTHREADS;

    unsigned int LoadDays = Config::DEFAULT_LOADDAYS;

    std::string storageEngine = Config::DEFAULT_STORAGE_ENGINE;
    std::string storageEngineExtra = "";

    std::string preCreateStatement = "";
    
    std::string connDb = Config::DEFAULT_DB;
    std::string connHost = Config::DEFAULT_HOST;
    std::string connUser = Config::DEFAULT_USER;
    std::string connPass = Config::DEFAULT_PASS;

    RunModeE runMode = RUN;
}
