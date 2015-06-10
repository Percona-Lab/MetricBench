#include "RunningMean.hpp"

RunningMean::RunningMean(double v1) {
  this->v1 = v1;
  this->n = 1;
  this->running = 0;
}

void RunningMean::addSample(double vi)
{
  this->n++;
  this->running += (vi - this->v1);
}

double RunningMean::mean() {
  return this->v1 + this->running / this->n;
}

int64_t RunningMean::samples() {
  return this->n;
}

