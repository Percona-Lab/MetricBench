#include <string>
#include "Config.hpp"

namespace Config
{
    unsigned int LoaderThreads = 8;

    std::string storageEngine = "InnoDB";
    std::string storageEngineExtra = "";
    
    RunModeE runMode = RUN;
}
