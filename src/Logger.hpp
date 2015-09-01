#pragma once

// based on code from Robert S. Barnes - Stackoverflow question #6168107

#include <sstream>

#include "Config.hpp"

/* consider adding boost thread id since we'll want to know whose writting and
 * won't want to repeat it for every single call */

/* consider adding policy class to allow users to redirect logging to specific
 * files via the command line
 */


class logIt
{
public:

    logIt(loglevel_e _loglevel = logERROR) {
        _buffer << loglevel_e_Label[_loglevel] << ": ";
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
