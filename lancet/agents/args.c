
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



#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <arpa/inet.h>
#include <string.h>

#include <lancet/agent.h>
#include <lancet/error.h>
#include <lancet/tp_proto.h>
#include <lancet/rand_gen.h>
#include <lancet/app_proto.h>

struct agent_config *parse_arguments(int argc, char **argv)
{
	int c, agent_type;
	struct agent_config *cfg;
	char *token1, *token2;
	struct sockaddr_in sa;
	//char proto[128];

	cfg = calloc(1, sizeof(struct agent_config));
	if (!cfg) {
		lancet_fprintf(stderr, "Failed to allocate cfg\n");
		return NULL;
	}

	while ((c = getopt(argc, argv, "t:s:c:a:p:i:r:")) != -1) {
		switch (c) {
		case 't':
			// Thread count
			cfg->thread_count = atoi(optarg);
			break;
		case 's':
			// Targets ip:port,ip:port
			token1 = strtok_r(optarg, ",", &optarg);
			while (token1) {
				/* Prepare the target */
				token2 = strtok_r(token1, ":", &token1);
				inet_pton(AF_INET, token2, &(sa.sin_addr));
				cfg->targets[cfg->target_count].ip = sa.sin_addr.s_addr;
				token2 = strtok_r(token1, ":", &token1);
				cfg->targets[cfg->target_count++].port = atoi(token2);

				assert(cfg->target_count < 4096);
				token1 = strtok_r(optarg, ",", &optarg);
			}
			break;
		case 'c':
			// Connection count
			cfg->conn_count = atoi(optarg);
			break;
		case 'a':
			// Agent type
			agent_type = atoi(optarg);
			if (agent_type == THROUGHPUT_AGENT)
				cfg->atype = THROUGHPUT_AGENT;
			else if (agent_type == LATENCY_AGENT)
				cfg->atype = LATENCY_AGENT;
			else if (agent_type == SYMMETRIC_NIC_TIMESTAMP_AGENT)
				cfg->atype = SYMMETRIC_NIC_TIMESTAMP_AGENT;
			else if (agent_type == SYMMETRIC_AGENT)
				cfg->atype = SYMMETRIC_AGENT;
			else {
				lancet_fprintf(stderr, "Unknown agent type\n");
				return NULL;
			}
			break;
		case 'p':
			// Communication protocol
			if (!strcmp(optarg, "TCP"))
				cfg->tp_type = TCP;
			else if (!strcmp(optarg, "R2P2"))
				cfg->tp_type = R2P2;
			else {
				lancet_fprintf(stderr, "Unknown transport protocol\n");
				return NULL;
			}
			break;
		case 'i':
			// Interarrival distribution
			cfg->idist = init_rand(optarg);
			init_reference_ia_dist(init_rand(optarg));
			if (!cfg->idist) {
				lancet_fprintf(stderr, "Failed to create iadist\n");
				return NULL;
			}
			break;
		case 'r':
			// Application protocol (request response types)
			cfg->app_proto = init_app_proto(optarg);
			if (!cfg->app_proto) {
				lancet_fprintf(stderr, "Failed to create app proto\n");
				return NULL;
			}
			break;
#if 0
		case 'l':
			if (parse_agent_type(optarg))
				return -1;
			break;
#endif
		default:
			lancet_fprintf(stderr, "Unknown argument\n");
			abort();
		}
	}

	cfg->tp = init_transport_protocol(cfg->tp_type);
	if (!cfg->tp) {
		lancet_fprintf(stderr, "Failed to init transport\n");
		return NULL;
	}

	return cfg;
}
