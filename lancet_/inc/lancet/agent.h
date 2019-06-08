
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

#include <lancet/rand_gen.h>
#include <lancet/app_proto.h>

#define IF_NAME "enp65s0"
#define MAX_THREADS 16

struct host_tuple {
	uint32_t ip;
	uint16_t port;
};

enum agent_type {
	THROUGHPUT_AGENT,
	LATENCY_AGENT,
	SYMMETRIC_NIC_TIMESTAMP_AGENT,
	SYMMETRIC_AGENT,
	AGENT_NR,
};

enum transport_protocol_type {
	TCP,
	R2P2,
};

struct agent_config {
	int thread_count;
	int conn_count;
	struct host_tuple targets[4096];
	int target_count;
	enum agent_type atype;
	enum transport_protocol_type tp_type;
	struct transport_protocol *tp;
	struct rand_gen *idist;
	struct application_protocol *app_proto;
};


struct agent_config *parse_arguments(int argc, char **argv);
int get_conn_count(void);
int get_thread_count(void);
int get_target_count(void);
struct host_tuple *get_targets(void);
struct rand_gen *get_ia_gen(void);
struct request *prepare_request(void);
struct byte_req_pair process_response(char *buf, int size);
long get_ia(void);
void set_load(uint32_t load);
enum agent_type get_agent_type(void);
int get_agent_tid(void);
