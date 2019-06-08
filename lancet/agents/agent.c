
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


#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>

#include <lancet/agent.h>
#include <lancet/manager.h>
#include <lancet/error.h>
#include <lancet/tp_proto.h>
#include <lancet/stats.h>
#include <lancet/app_proto.h>
#include <lancet/timestamping.h>

static struct agent_config *cfg;
static __thread struct request to_send;
static __thread struct iovec received;
static __thread int thread_idx;

int get_conn_count(void)
{
	return cfg->conn_count;
}

int get_thread_count(void)
{
	return cfg->thread_count;
}

int get_target_count(void)
{
	return cfg->target_count;
}

struct host_tuple *get_targets(void)
{
	return cfg->targets;
}

struct rand_gen *get_ia_gen(void)
{
	return cfg->idist;
}

long get_ia(void)
{
	return lround(generate(cfg->idist) * 1000);
}

struct request *prepare_request(void)
{
	create_request(cfg->app_proto, &to_send);

	return &to_send;
}

struct byte_req_pair process_response(char *buf, int size)
{
	received.iov_base = buf;
	received.iov_len = size;
	return consume_response(cfg->app_proto, &received);
}

void set_load(uint32_t load)
{
	double per_thread_load;

	set_reference_load(load);
	per_thread_load = load / (double)cfg->thread_count;
	set_avg(cfg->idist, 1e6 / per_thread_load);
}

enum agent_type get_agent_type(void)
{
	return cfg->atype;
}

int get_agent_tid(void)
{
	return thread_idx;
}

static void *agent_main(void *arg)
{
	cpu_set_t cpuset;
	pthread_t thread;
	int s;

	thread = pthread_self();
	thread_idx = (int)(long)arg;
	init_per_thread_stats();

	srand(time(NULL) + thread_idx * 12345);

	CPU_ZERO(&cpuset);
	CPU_SET(thread_idx, &cpuset);

	s = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);
	if (s != 0) {
		lancet_perror("pthread_setaffinity_np");
		return NULL;
	}
	cfg->tp->tp_main[cfg->atype]();

	return NULL;
}

int main(int argc, char **argv)
{
	int i;
	pthread_t *tids;

	cfg = parse_arguments(argc, argv);
	if (!cfg)
		exit(-1);

	if (cfg->atype == SYMMETRIC_NIC_TIMESTAMP_AGENT)
		enable_nic_timestamping(IF_NAME);

	if (manager_init(cfg->thread_count)) {
		lancet_fprintf(stderr, "failed to init the manager\n");
		exit(-1);
	}

	tids = malloc(cfg->thread_count * sizeof(pthread_t));
	if (!tids) {
		lancet_fprintf(stderr, "Failed to allocate tids\n");
		exit(-1);
	}

	for (i = 0; i < cfg->thread_count; i++) {
		if (pthread_create(&tids[i], NULL, agent_main, (void *)(long)i)) {
			lancet_fprintf(stderr, "failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	if (manager_run()) {
		lancet_fprintf(stderr, "error running the manager\n");
		exit(-1);
	}

	for (i = 0; i < cfg->thread_count; i++)
		pthread_join(tids[i], NULL);

	return 0;
}
