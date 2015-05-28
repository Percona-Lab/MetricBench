#pragma once

enum  RunModeE { PREPARE = 1, RUN };

/* Connection parameter and sample data */
namespace Config
{


    constexpr auto EXAMPLE_DB = "test" ;
    constexpr auto EXAMPLE_HOST = "tcp://127.0.0.1:3306";
    constexpr auto EXAMPLE_USER = "root";
    constexpr auto EXAMPLE_PASS = "";

    constexpr unsigned int StartTimestamp = 946684800;
    constexpr unsigned int SecInDay = 24*60*60;

    constexpr unsigned int MaxDevices = 32;
    constexpr unsigned int MaxMetrics = 1000;
    constexpr double MaxValue = 10000.0;
    constexpr unsigned int MaxCnt = 60;

    constexpr unsigned int LoadDays = 10;

    extern unsigned int LoaderThreads;

    extern std::string storageEngine;
    extern std::string storageEngineExtra;

    extern RunModeE runMode;
}
