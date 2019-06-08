
//Open Source License.
//
//Copyright 2019 Ecole Polytechnique Federale Lausanne (EPFL)
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in
//all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWARE.


#pragma once

#include <stdint.h>
#include <stdlib.h>

struct rand_gen {
	/* Void pointer to hold any relevant data for each distribution */
	void *params;
	/* Set distribution's average */
	void (*set_avg)(struct rand_gen *gen, double avg);
	/*
	 * Inverse CDF takes a number in [0,1] (cummulative probability) and
	 * returns the corresponding number
	 */
	double (*inv_cdf)(struct rand_gen *gen, double y);
	/* Set only if the random + inv_cdf pattern is not followed */
	double (*generate)(struct rand_gen *generator);
};

/* Initialise random generator */
struct rand_gen *init_rand(char *gen_type);
/* Generate a random number */
static inline double generate(struct rand_gen *generator)
{
	double y;

	if (generator->generate)
		return generator->generate(generator);
	else {
		y = drand48();
		return generator->inv_cdf(generator, y);
	}
}

static inline void set_avg(struct rand_gen *gen, double avg)
{
	return gen->set_avg(gen, avg);
}

struct param_1 {
	double a;
};

struct param_2 {
	double a;
	double b;
};

struct param_3 {
	double a;
	double b;
	double c;
};

struct param_lss {
	double loc;
	double scale;
	double shape;
};

struct bimodal_param {
	double low;
	double up;
	double prob;
};

struct lognorm_params {
	double sigma;
	double mu;
	struct cpp_gen *ng;
};

struct gamma_params {
	struct cpp_gen *gg;
};
