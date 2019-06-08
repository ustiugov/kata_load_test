
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
#include <assert.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <string.h>
#include <math.h>

#include <lancet/app_proto.h>
#include <lancet/error.h>
#include <lancet/rand_gen.h>

static __thread void *per_thread_arg = NULL;

/*
 * Echo protocol
 */
int echo_create_request(struct application_protocol *proto, struct request *req)
{
	struct iovec *fixed_req = (struct iovec *)proto->arg;
	req->iovs[0] = *fixed_req;
	req->iov_cnt = 1;
	req->meta = NULL;

	return 0;
}

struct byte_req_pair echo_consume_response(struct application_protocol *proto,
		struct iovec *response)
{
	struct byte_req_pair res;
	struct iovec *msg = (struct iovec *)proto->arg;

	res.reqs = response->iov_len / msg->iov_len;
	res.bytes = res.reqs * msg->iov_len;

	return res;
}

static int echo_init(char *proto, struct application_protocol *app_proto)
{
	char *token;
	struct iovec *arg;
	int message_len;

	token = strtok(proto, ":");
	token = strtok(NULL, ":");

	message_len = atoi(token);
	arg = malloc(sizeof(struct iovec));
	assert(arg);
	arg->iov_base = malloc(message_len);
	assert(arg->iov_base);
	memset(arg->iov_base, '#', message_len);
	arg->iov_len = message_len;

	app_proto->type = PROTO_ECHO;
	// The proto arg is the iovec with the message
	app_proto->arg = arg;
	app_proto->create_request = echo_create_request;
	app_proto->consume_response = echo_consume_response;

	return 0;
}

/*
 * Synthetic protocol
 */
int synthetic_create_request(struct application_protocol *proto,
	struct request *req)
{
	struct rand_gen *generator = (struct rand_gen *)proto->arg;

	if (!per_thread_arg) {
		per_thread_arg = malloc(sizeof(long));
		assert(per_thread_arg);
	}
	per_thread_arg = (void *)lround(generate(generator));
	req->iovs[0].iov_base = &per_thread_arg;
	req->iovs[0].iov_len = sizeof(long);
	req->iov_cnt = 1;
	req->meta = NULL;

	return 0;
}

struct byte_req_pair synthetic_consume_response(
	struct application_protocol *proto, struct iovec *response)
{
	struct byte_req_pair res;

	res.reqs = response->iov_len / sizeof(long);
	res.bytes = res.reqs * sizeof(long);
	return res;
}

static int synthetic_init(char *proto, struct application_protocol *app_proto)
{
	char *token;
	struct rand_gen *gen = NULL;

	// Remove the type.
	token = strtok(proto, ":");
	token = strtok(NULL, "");

	gen = init_rand(token);
	assert(gen);

	app_proto->type = PROTO_SYNTHETIC;
	// The proto arg is the random generator
	app_proto->arg = gen;
	app_proto->create_request = synthetic_create_request;
	app_proto->consume_response = synthetic_consume_response;

	return 0;
}

/*
 * ASCII Memcached protocol
 */
int ascii_mem_create_request(struct application_protocol *proto,
	struct request *req)
{
	int n;
	struct ascii_mem_thread_data *data;

	if (!per_thread_arg) {
		per_thread_arg = malloc(sizeof(struct ascii_mem_thread_data));
		assert(per_thread_arg);
		data = (struct ascii_mem_thread_data *)per_thread_arg;
		data->current_key = 0;
	}
	data = (struct ascii_mem_thread_data *)per_thread_arg;
	//n = snprintf(data->req, 4096, "get %d\r\n", data->current_key++);
	n = snprintf(data->req, 4096, "get %019d\r\n", data->current_key++ % 1000000);
	assert(n>0);
	req->iovs[0].iov_base = data->req;
	req->iovs[0].iov_len = n;
	req->iov_cnt = 1;
	req->meta = NULL;

	return 0;
}

#define RESPONSE_SIZE 40
struct byte_req_pair ascii_mem_consume_response(
	struct application_protocol *proto, struct iovec *response)
{
	//char expected[] = "END\r\n";
	//char *r;
	struct byte_req_pair res;

	res.bytes = 0;
	res.reqs = 0;
	//r = (char *)response->iov_base;

	assert(response->iov_len >= RESPONSE_SIZE);
	while (res.bytes < response->iov_len) {
	//	assert(strncmp(&r[res.bytes], expected, 5) == 0);
		res.reqs += 1;
		res.bytes += RESPONSE_SIZE;
		if ((res.bytes + RESPONSE_SIZE) > response->iov_len)
			break;
	}
	return res;
}

static int ascii_mem_init(char *proto, struct application_protocol *app_proto)
{
	app_proto->type = PROTO_ASCII_MEMCACHED;
	app_proto->arg = NULL;
	app_proto->create_request = ascii_mem_create_request;
	app_proto->consume_response = ascii_mem_consume_response;

	return 0;
}

/*
 * ASCII Memcached service protocol
 */
int ascii_mem_svc_create_request(struct application_protocol *proto,
	struct request *req)
{
	int n;
	struct ascii_mem_thread_data *data;
	struct ascii_mem_svc_info *svc_info;

	svc_info = (struct ascii_mem_svc_info *)proto->arg;
	if (!per_thread_arg) {
		per_thread_arg = malloc(sizeof(struct ascii_mem_thread_data));
		assert(per_thread_arg);
		data = (struct ascii_mem_thread_data *)per_thread_arg;
	}
	data = (struct ascii_mem_thread_data *)per_thread_arg;
	n = snprintf(data->req, 4096, "get %ld\r\n",
			lround(generate(svc_info->svc_time_gen)));
	assert(n>0);
	req->iovs[0].iov_base = data->req;
	req->iovs[0].iov_len = n;
	req->iov_cnt = 1;
	req->meta = NULL;

	return 0;
}

static int ascii_mem_svc_init(char *proto, struct application_protocol *app_proto)
{
	struct ascii_mem_svc_info *data;

	data = malloc(sizeof(struct ascii_mem_svc_info));
	assert(data);
	data->svc_time_gen = init_rand(&proto[14]);

	app_proto->type = PROTO_ASCII_MEMCACHED_SVC;
	app_proto->arg = data;
	app_proto->create_request = ascii_mem_svc_create_request;
	app_proto->consume_response = ascii_mem_consume_response;

	return 0;
}

struct application_protocol *init_app_proto(char *proto)
{
	struct application_protocol *app_proto;

	app_proto = malloc(sizeof(struct application_protocol));
	assert(app_proto);

	if (strncmp(proto, "echo", 4) == 0)
		echo_init(proto, app_proto);
	else if (strncmp(proto, "synthetic", 9) == 0)
		synthetic_init(proto, app_proto);
	else if (strncmp(proto, "ascii-mem-svc", 13) == 0)
		ascii_mem_svc_init(proto, app_proto);
	else if (strncmp(proto, "ascii-mem", 9) == 0)
		ascii_mem_init(proto, app_proto);
	else {
		lancet_fprintf(stderr, "Unknown application protocol\n");
		return NULL;
	}

	return app_proto;
}
