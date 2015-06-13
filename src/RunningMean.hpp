#pragma once

#include <stdint.h>

class RunningMean {

private:
    int64_t n;
    double v1;
    double running;

public:
    RunningMean(double v1);
    void addSample(double vi);
    double mean();
    int64_t samples();
};
