
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


/*
 * Interface for the communication protocol e.g. TCP, UDP, R2P2 etc
 */
#pragma once

#include <stdlib.h>
#include <lancet/agent.h>
#include <lancet/stats.h>

struct transport_protocol {
	void (*tp_main[AGENT_NR])(void);
};

struct transport_protocol *init_tcp(void);
//struct transport_protocol *init_r2p2(void);

static inline struct transport_protocol *init_transport_protocol(enum transport_protocol_type tp_type)
{
	struct transport_protocol *res;

	if (tp_type == TCP)
		res = init_tcp();
//	else if (tp_type == R2P2)
//		res = init_r2p2();
	else
		res = NULL;

	return res;
}

/*
 * TCP specific
 */
//#ifdef SINGLE_REQ
//#define MAX_PENDING_REQS 1
//#else
//#ifdef REQ_4
//#define MAX_PENDING_REQS 1
//#else
//#define MAX_PENDING_REQS 256
//#endif
//#endif
#define MAX_PENDING_REQS 16

#define MAX_PAYLOAD 4000
struct tcp_connection {
	uint32_t fd;
	uint16_t idx;
	uint16_t closed;
	uint16_t pending_reqs;
	uint16_t buffer_idx;
	char buffer[MAX_PAYLOAD];
};
