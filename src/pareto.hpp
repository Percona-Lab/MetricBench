#pragma once

#include <random>
#include <stdexcept>

#include "Config.hpp"

/* based on an article "Power laws, Pareto distributions and Zipf's law" M. E. J. Newman */
/* pareto_h = 1.16096 corresponds to 80-20 rule
1.002187 : 99-1 rule
1.01742 : 95-5 rule
1.04795 : 90-10 rule
1.42095 : 70-30 rule

*/

class ParetoGenerator {
    double pareto_h, pareto_power;
    std::default_random_engine generator;
    std::uniform_real_distribution<double> distribution{0.0, 1.0};

public:
    ParetoGenerator(double h) :pareto_h{h} {
        unsigned int seed=Config::randomSeed;
        if (!seed) {
          std::random_device rd;
          seed=rd();
        }
        generator.seed(seed);

	if (h <= 1) { throw std::runtime_error("Incorrect Pareto alpha"); }
	pareto_power = -1 / (h-1); }

    /* return a number from range [a,b], where "b" can be < "a".
       the distribution is skewed toward "a" */
    int GetNext(int a, int b) { return a +
	static_cast<int>( (b - a + 1) / pow( 1 - distribution(generator),  pareto_power)); }
    double GetNext(double a, double b) { return a +
	static_cast<double>( (b - a + 1) / pow( 1 - distribution(generator),  pareto_power)); }

};
