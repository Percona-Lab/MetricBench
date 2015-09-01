#pragma once

// based on code from Robert S. Barnes - Stackoverflow question #6168107

#include <sstream>

#include "Config.hpp"

class logIt
{
public:

    logIt(loglevel_e _loglevel = logERROR) {
        // only display loglevel prefix on non-info lines
        if (_loglevel != logINFO)
          _buffer << loglevel_e_Label[_loglevel] << ": ";
    }

    // for trace
    logIt(const std::string file, uint64_t line) {
        _buffer << loglevel_e_Label[logTRACE] << ": " << file << ":" << line;
    }

    template <typename T>
    logIt & operator<<(T const & value)
    {
        _buffer << value;
        return *this;
    }

    ~logIt()
    {
        _buffer << std::endl;
        std::lock_guard<std::mutex> lock(Config::log_lock); // just in case
        *Config::log << _buffer.str();
    }

private:
    std::ostringstream _buffer;
};

#define log(level) \
if (level > Config::logLevel) ; \
else logIt(level)

#define logtrace() \
if (logTRACE > Config::logLevel) ; \
else logIt(__FILE__,__LINE__)

