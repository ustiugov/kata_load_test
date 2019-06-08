
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

#include <sys/uio.h>

#include <lancet/stats.h>
#include <lancet/rand_gen.h>

struct request {
	void *meta;
	int iov_cnt;
	struct iovec iovs[64];
};

enum app_proto_type {
	PROTO_ECHO,
	PROTO_SYNTHETIC,
	PROTO_ASCII_MEMCACHED,
	PROTO_ASCII_MEMCACHED_SVC,
};

struct application_protocol {
	enum app_proto_type type;
	void *arg;
	int (*create_request)(struct application_protocol *proto,
			struct request *req);
	struct byte_req_pair (*consume_response)(struct application_protocol *proto,
			struct iovec *response);
};

struct application_protocol *init_app_proto(char *proto);
static inline int create_request(struct application_protocol *proto,
		struct request *req)
{
	return proto->create_request(proto, req);
};

static inline struct byte_req_pair consume_response(
		struct application_protocol *proto, struct iovec *response)
{
	return proto->consume_response(proto, response);
};

/*
 * Specific datastructure for ascii-memecached protocol
 */
struct ascii_mem_svc_info {
	struct rand_gen *svc_time_gen;
};

struct ascii_mem_thread_data {
	uint32_t current_key;
	char req[4096];
};
