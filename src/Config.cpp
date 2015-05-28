#include <string>
#include "Config.hpp"

namespace Config
{
    unsigned int LoaderThreads = 8;

    std::string storageEngine = "InnoDB";
    std::string storageEngineExtra = "";

    std::string connDb = Config::DEFAULT_DB;
    std::string connHost = Config::DEFAULT_HOST;
    std::string connUser = Config::DEFAULT_USER;
    std::string connPass = Config::DEFAULT_PASS;

    RunModeE runMode = RUN;
}
