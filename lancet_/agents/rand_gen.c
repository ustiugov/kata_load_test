
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


#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <lancet/error.h>
#include <lancet/rand_gen.h>
#include <lancet/cpp_rand.h>

/*
 * Deterministic distribution
 * params holds the number to return
 */

static double fixed_inv_cdf(struct rand_gen *gen,
							__attribute__((unused)) double y)
{
	return *((double *)gen->params);
}

static void fixed_set_avg(struct rand_gen *gen, double avg)
{
	double *avg_param = (double *)gen->params;
	*avg_param = avg;
}

static void fixed_init(struct rand_gen *gen, struct param_1 *param)
{
	gen->params = malloc(sizeof(double));
	gen->set_avg = fixed_set_avg;
	gen->inv_cdf = fixed_inv_cdf;
	gen->generate = NULL;

	gen->set_avg(gen, param->a);
	free(param);
}

/*
 * Exponential distribution
 * params holds lambda (1/avg)
 */

static double exp_inv_cdf(struct rand_gen *gen, double y)
{
	double *lambda = (double *)gen->params;
	return -log(y) / *lambda;
}

static void exp_set_avg(struct rand_gen *gen, double avg)
{
	double *lambda = (double *)gen->params;
	*lambda = (double)1.0 / avg;
}

static void exp_init(struct rand_gen *gen, struct param_1 *param)
{
	gen->params = malloc(sizeof(double));
	gen->set_avg = exp_set_avg;
	gen->inv_cdf = exp_inv_cdf;
	gen->generate = NULL;

	gen->set_avg(gen, param->a);
	free(param);
}

/*
 * Generalised pareto distribution
 * params hold a struct of param_lss with loc, shape and scale
 */
static double gpar_inv_cdf(struct rand_gen *gen, double y)
{
	struct param_lss *params = (struct param_lss *)gen->params;
	return params->loc +
		   params->scale * (pow(1 - y, -params->shape) - 1) / params->shape;
}

/*
 * Change only the scale parameter (legacy)
 */
static void gpar_set_avg(struct rand_gen *gen, double avg)
{
	struct param_lss *params = (struct param_lss *)gen->params;
	params->scale = (avg - params->loc) * (1 - params->shape);
}

static void gpar_init(struct rand_gen *gen, struct param_3 *param)
{
	gen->params = param;
	gen->set_avg = gpar_set_avg;
	gen->inv_cdf = gpar_inv_cdf;
	gen->generate = NULL;
}

/*
 * GEV distribution
 * params holds a struct param_lss
 */
static double gev_inv_cdf(struct rand_gen *gen, double y)
{
	struct param_lss *params = (struct param_lss *)gen->params;
	return params->loc +
		   params->scale * (pow(-exp(y), -params->shape) - 1) / params->shape;
}

/*
 * Not implemented
 */
static void gev_set_avg(__attribute__((unused)) struct rand_gen *gen,
						__attribute__((unused)) double avg)
{
	assert(0);
}

static void gev_init(struct rand_gen *gen, struct param_3 *param)
{
	gen->params = param;
	gen->set_avg = gev_set_avg;
	gen->inv_cdf = gev_inv_cdf;
	gen->generate = NULL;
}

/*
 * Bimodal distribution.
 * maxi1:maxi2:Prob1
 */
static void bimodal_set_avg(__attribute__((unused)) struct rand_gen *gen,
							__attribute__((unused)) double avg)
{
	// Should never be called.
	assert(0);
}

static double bimodal_inv_cdf(struct rand_gen *gen, double y)
{
	struct bimodal_param *params = (struct bimodal_param *)(gen->params);
	if (y <= params->prob)
		return params->low;
	return params->up;
}

static void bimodal_init(struct rand_gen *gen, struct param_3 *param)
{
	gen->params = param;
	gen->set_avg = bimodal_set_avg;
	gen->inv_cdf = bimodal_inv_cdf;
	gen->generate = NULL;
}

/*
 * Lognormal distribution
 */
static double lognorm_generate(struct rand_gen *gen)
{
	struct lognorm_params *params = (struct lognorm_params *)gen->params;
	double y = get_normal_rand(params->ng);
	return exp(params->mu+y*params->sigma);
}

static double lognorm_inv_cdf(struct rand_gen *gen, double y)
{
	assert(0);
}

static void lognorm_set_avg(__attribute__((unused)) struct rand_gen *gen,
		double avg)
{
	assert(0);
}

static void lognormal_init(struct rand_gen *gen, struct param_2 *param)
{
	struct lognorm_params *params;

	params = malloc(sizeof(struct lognorm_params));
	assert(params);

	params->ng = new_normal_gen();
	params->mu = param->a;
	params->sigma = param->b;

	gen->params = params;
	gen->generate = lognorm_generate;
	gen->set_avg = lognorm_set_avg;
	gen->inv_cdf = lognorm_inv_cdf;
	free(param);
}

/*
 * Gamma distribution
 */
static double gamma_generate(struct rand_gen *gen)
{
	struct gamma_params *params = (struct gamma_params *)gen->params;
	int res;
	res = get_gamma_rand(params->gg);
	return res;
}

static double gamma_inv_cdf(struct rand_gen *gen, double y)
{
	assert(0);
}

static void gamma_set_avg(__attribute__((unused)) struct rand_gen *gen,
		double avg)
{
	assert(0);
}

static void gamma_init(struct rand_gen *gen, struct param_2 *param)
{
	assert(0);
	struct gamma_params *params;

    params = malloc(sizeof(struct gamma_params));
	assert(params);

	params->gg = new_gamma_gen(param->a, param->b);

	gen->params = params;
	gen->generate = gamma_generate;
	gen->set_avg = gamma_set_avg;
	gen->inv_cdf = gamma_inv_cdf;
	free(param);
}

static struct param_1 *parse_param_1(char *type)
{
	char *tok;
	struct param_1 *res;

	res = malloc(sizeof(struct param_1));
	assert(res);

	tok = strtok(type, ":");
	tok = strtok(NULL, ":");
	res->a = (tok == NULL) ? 0 : atof(tok);

	return res;
}

static struct param_2 *parse_param_2(char *type)
{
	char *tok;
	struct param_2 *params;

	params = malloc(sizeof(struct param_3));
	assert(params);

	tok = strtok(type, ":");
	tok = strtok(NULL, ":");
	params->a = atof(tok);
	tok = strtok(NULL, ":");
	params->b = atof(tok);

	return params;
}

static struct param_3 *parse_param_3(char *type)
{
	char *tok;
	struct param_3 *params;

	params = malloc(sizeof(struct param_3));
	assert(params);

	tok = strtok(type, ":");
	tok = strtok(NULL, ":");
	params->a = atof(tok);
	tok = strtok(NULL, ":");
	params->b = atof(tok);
	tok = strtok(NULL, ":");
	params->c = atof(tok);

	return params;
}

struct rand_gen *init_rand(char *gen_type)
{
	struct rand_gen *gen = malloc(sizeof(struct rand_gen));

	if (strncmp(gen_type, "fixed", 5) == 0)
		fixed_init(gen, parse_param_1(gen_type));
	else if (strncmp(gen_type, "exp", 3) == 0)
		exp_init(gen, parse_param_1(gen_type));
	else if (strncmp(gen_type, "pareto", 6) == 0)
		gpar_init(gen, parse_param_3(gen_type));
	else if (strncmp(gen_type, "gev", 3) == 0)
		gev_init(gen, parse_param_3(gen_type));
	else if (strcmp(gen_type, "fb_key") == 0) {
		char type[] = "gev:30.7984:8.20449:0.078688";
		gev_init(gen, parse_param_3(type));
	} else if (strcmp(gen_type, "fb_ia") == 0) {
		char type[] = "gpar:0:16.0292:0.154971";
		gpar_init(gen, parse_param_3(type));
	} else if (strcmp(gen_type, "fb_val") == 0) {
		/* WARNING: this is not exactly the same as mutilate */
		char type[] = "gpar:15.0:214.476:0.348238";
		gpar_init(gen, parse_param_3(type));
	} else if (strncmp(gen_type, "bimodal", 7) == 0)
		bimodal_init(gen, parse_param_3(gen_type));
	else if (strncmp(gen_type, "lognorm", 7) == 0)
		lognormal_init(gen, parse_param_2(gen_type));
	else if (strncmp(gen_type, "gamma", 5) == 0)
		gamma_init(gen, parse_param_2(gen_type));
	else {
		lancet_fprintf(stderr, "Unknown generator type\n");
		return NULL;
	}
	return gen;
}
