
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
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <time.h>

#include <lancet/tp_proto.h>
#include <lancet/error.h>
#include <lancet/misc.h>
#include <lancet/manager.h>
#include <lancet/timestamping.h>

static __thread struct tcp_connection *connections;
static __thread int epoll_fd;
static __thread struct pending_tx_timestamps *per_conn_tx_timestamps;
static __thread int avail_reqs;

static inline struct tcp_connection *pick_conn()
{
	int idx, i;
	struct tcp_connection *c;

	// try 10 times
	for (i=0;i<10;i++) {
		idx = rand() % (get_conn_count() / get_thread_count()) ;
		c = &connections[idx];
		if ((c->pending_reqs < MAX_PENDING_REQS) && (!c->closed))
			return c;
	}
	return NULL;
}

static int latency_open_connections(void)
{
	struct sockaddr_in addr;
	int i, ret, sock, per_thread_conn, million = 1e6, one = 1, dest_idx;
	struct linger linger;
	struct host_tuple *targets;

	addr.sin_family = AF_INET;

	per_thread_conn = get_conn_count() / get_thread_count();
	connections = calloc(per_thread_conn, sizeof(struct tcp_connection));
	targets = get_targets();

	for (i = 0; i < per_thread_conn; i++) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == -1) {
			lancet_perror("Error creating socket");
			return -1;
		}
		dest_idx = i % get_target_count();
		addr.sin_port = htons(targets[dest_idx].port);
		addr.sin_addr.s_addr = targets[dest_idx].ip;
		ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (ret) {
			lancet_perror("Error connecting");
			return -1;
		}
		/* Disable Nagle */
		ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		if (ret) {
			lancet_perror("Error setsockopt TCP_NODELAY");
			return -1;
		}
		/* Close with RST not FIN */
		linger.l_onoff = 1;
		linger.l_linger = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger,
					   sizeof(linger))) {
			perror("setsockopt(SO_LINGER)");
			exit(1);
		}
		/* Enable busy polling */
		ret = setsockopt(sock, SOL_SOCKET, SO_BUSY_POLL, &million,
						 sizeof(million));
		if (ret) {
			lancet_perror("Error setsockopt SO_BUSY_POLL");
			return -1;
		}
		connections[i].fd = sock;
		connections[i].closed = 0;
#if 0
		if (dest_idx == 0)
			t_ctx->connections[i].master_conn = NULL;
		else
			t_ctx->connections[i].master_conn = &t_ctx->connections[i - dest_idx];
#endif
	}
	return 0;
}

static int throughput_open_connections(void)
{
	/*init epoll*/
	struct sockaddr_in addr;
	int i, efd, ret, sock, per_thread_conn,dest_idx,n;
	int one = 1;
	struct epoll_event event;
	struct linger linger;
	struct host_tuple *targets;

	addr.sin_family = AF_INET;
	efd = epoll_create(1);
	if (efd < 0) {
		lancet_perror("epoll_create error");
		return -1;
	}

	per_thread_conn = get_conn_count() / get_thread_count();
	avail_reqs = per_thread_conn * MAX_PENDING_REQS;
	connections = calloc(per_thread_conn, sizeof(struct tcp_connection));
	assert(connections);
	if ((get_agent_type() == SYMMETRIC_NIC_TIMESTAMP_AGENT) || (get_agent_type() == SYMMETRIC_AGENT)) {
		per_conn_tx_timestamps= calloc(per_thread_conn, sizeof(struct pending_tx_timestamps));
		assert(per_conn_tx_timestamps);
	}
	targets = get_targets();

	for (i = 0; i < per_thread_conn; i++) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == -1) {
			lancet_perror("Error creating socket");
			return -1;
		}
		dest_idx = i % get_target_count();
		addr.sin_port = htons(targets[dest_idx].port);
		addr.sin_addr.s_addr = targets[dest_idx].ip;
		ret = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
		if (ret) {
			lancet_perror("Error connecting");
			return -1;
		}
		ret = fcntl(sock, F_SETFL, O_NONBLOCK);
		if (ret == -1) {
			lancet_perror("Error while setting nonblocking");
			return -1;
		}
		n = 524288;
		ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &n, sizeof(n));
		if (ret) {
			lancet_perror("Error setsockopt");
			return -1;
		}
		n = 524288;
		ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &n, sizeof(n));
		if (ret) {
			lancet_perror("Error setsockopt");
			return -1;
		}

		if(get_agent_type() == SYMMETRIC_NIC_TIMESTAMP_AGENT) {
			if(setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, IF_NAME, strlen(IF_NAME))) {
				lancet_perror("setsockopt SO_BINDTODEVICE");
				return -1;
			}
			ret = sock_enable_timestamping(sock);
			if (ret) {
				lancet_fprintf(stderr, "sock enable timestamping failed\n");
				return -1;
			}
		}

		/* Disable Nagle's algorithm */
		ret = setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		if (ret) {
			lancet_perror("Error setsockopt");
			return -1;
		}

		/* Close with RST not FIN */
		linger.l_onoff = 1;
		linger.l_linger = 0;
		if (setsockopt(sock, SOL_SOCKET, SO_LINGER, (void *)&linger,
					   sizeof(linger))) {
			perror("setsockopt(SO_LINGER)");
			exit(1);
		}
		event.events = EPOLLIN;
		event.data.u32 = i;
		ret = epoll_ctl(efd, EPOLL_CTL_ADD, sock, &event);
		if (ret) {
			lancet_perror("Error while adding to epoll group");
			return -1;
		}
		connections[i].fd = sock;
		connections[i].pending_reqs = 0;
		connections[i].idx = i;
		connections[i].buffer_idx = 0;
		connections[i].closed = 0;
#if 0
		// FIXME: Use app proto meta to pick a connection
		if (dest_idx == 0)
			t_ctx->connections[i].master_conn = NULL;
		else
			t_ctx->connections[i].master_conn = &t_ctx->connections[i - dest_idx];
#endif
	}
	epoll_fd = efd;
	return 0;
}

static void throughput_tcp_main(void) {
	int ready, idx, i, conn_per_thread, ret, bytes_to_send;
	long next_tx, diff;
	struct epoll_event *events;
	struct tcp_connection *conn;
	struct request *to_send;
	struct byte_req_pair read_res;
	struct byte_req_pair send_res;

	if (throughput_open_connections())
		return;

	/*Initializations*/
	conn_per_thread = get_conn_count() / get_thread_count();
	events = malloc(conn_per_thread * sizeof(struct epoll_event));

	next_tx = time_ns();
	while (1) {
		if (!should_load()) {
			next_tx = time_ns();
			continue;
		}
		diff = time_ns() - next_tx;
		if (diff >= 0) {
			conn = pick_conn();
			if (!conn)
				goto REP_PROC;
			to_send = prepare_request();
			bytes_to_send = 0;
			for (i=0;i<to_send->iov_cnt;i++)
				bytes_to_send += to_send->iovs[i].iov_len;
			ret = writev(conn->fd, to_send->iovs, to_send->iov_cnt);
			if ((ret < 0) && (errno != EWOULDBLOCK)) {
				lancet_perror("Unknown connection error write\n");
				return;
			}
			assert(ret == bytes_to_send);
			conn->pending_reqs++;
			avail_reqs--;

			/*BookKeeping*/
			send_res.bytes = ret;
			send_res.reqs = 1;
			add_throughput_tx_sample(send_res);

			/*Schedule next*/
			next_tx += get_ia();
		}
	REP_PROC:
		/* process responses */
		ready = epoll_wait(epoll_fd, events, conn_per_thread, 0);
		for (i = 0; i < ready; i++) {
			idx = events[i].data.u32;
			conn = &connections[idx];
			/* Handle incoming packet */
			if (events[i].events & EPOLLIN) {
				// read into the connection buffer
				ret = recv(conn->fd, &conn->buffer[conn->buffer_idx],
						MAX_PAYLOAD-conn->buffer_idx, 0);
				if ((ret < 0) && (errno != EWOULDBLOCK)) {
					lancet_perror("Unknow connection error read\n");
					return;
				}
				if (ret == 0) {
					close(conn->fd);
					lancet_fprintf(stderr, "Connection closed\n");
					conn->closed = 1;
					continue;
				}
				conn->buffer_idx += ret;
				read_res = process_response(conn->buffer, conn->buffer_idx);
				if (read_res.bytes == conn->buffer_idx)
					conn->buffer_idx = 0;
				else if (read_res.bytes < conn->buffer_idx) {
					lancet_fprintf(stderr, "Received half request\n");
					memcpy(conn->buffer, &conn->buffer[read_res.bytes],
							conn->buffer_idx-read_res.bytes);
					conn->buffer_idx -= read_res.bytes;
				} else
					assert(0);

				assert(read_res.reqs > 0);
				conn->pending_reqs -= read_res.reqs;
				avail_reqs += read_res.reqs;

				/* Bookkeeping */
				add_throughput_rx_sample(read_res);

			} else if (events[i].events & EPOLLHUP)
				assert(0);
			else
				assert(0);
		}
	}

	return;
}

static void latency_tcp_main(void)
{
	int i, ret, bytes_to_send;
	long now, start_time, end_time, next_tx;
	struct tcp_connection *conn;
	struct request *to_send;
	struct byte_req_pair read_res;
	struct byte_req_pair send_res;

	if (latency_open_connections())
		exit(-1);

	next_tx = time_ns();
	while (1) {
		if (!should_load()) {
			next_tx = time_ns();
			continue;
		}
		now = time_ns();
		if (now < next_tx)
			continue;
		conn = pick_conn();
		if (!conn)
			continue;
		start_time = time_ns();

		to_send = prepare_request();
		bytes_to_send = 0;
		for (i=0;i<to_send->iov_cnt;i++)
			bytes_to_send += to_send->iovs[i].iov_len;
		ret = writev(conn->fd, to_send->iovs, to_send->iov_cnt);
		if (ret<0) {
			lancet_perror("Writev failed\n");
			return;
		}
		assert(ret == bytes_to_send);
		/* Bookkeeping */
		send_res.bytes = ret;
		send_res.reqs = 1;
		add_throughput_tx_sample(send_res);

		ret = recv(conn->fd, conn->buffer, MAX_PAYLOAD, 0);
		if (ret < 0) {
			lancet_perror("Error read\n");
			return;
		}
		if (ret == 0) {
			close(conn->fd);
			lancet_fprintf(stderr, "Connection closed\n");
			conn->closed = 1;
			continue;
		}
		read_res = process_response(conn->buffer, ret);
		assert(read_res.bytes == ret);
		end_time = time_ns();
		/*BookKeeping*/
		add_throughput_rx_sample(read_res);
		add_latency_sample((end_time - start_time), NULL);

		/*Schedule next*/
		next_tx += get_ia();
	}
	return;
}

static void symmetric_nic_tcp_main(void) {
	int ready, idx, i, conn_per_thread, ret, bytes_to_send, error, to_process;
	long next_tx, diff;
	struct epoll_event *events;
	struct tcp_connection *conn;
	struct request *to_send;
	struct byte_req_pair read_res;
	struct byte_req_pair send_res;
	struct timestamp_info rx_timestamp, *tx_timestamp;
	struct msghdr hdr;
	struct timespec latency;

	if (throughput_open_connections())
		return;

	/*Initializations*/
	conn_per_thread = get_conn_count() / get_thread_count();
	events = malloc(4 * conn_per_thread * sizeof(struct epoll_event));

	next_tx = time_ns();
	while (1) {
		if (!should_load()) {
			next_tx = time_ns();
			continue;
		}
		if (!avail_reqs)
			goto REP_PROC;
		diff = time_ns() - next_tx;
		while (diff >= 0) {
			conn = pick_conn();
			if (!conn)
				goto REP_PROC;
			to_send = prepare_request();

			// send once
			bytes_to_send = 0;
			for (i=0;i<to_send->iov_cnt;i++)
				bytes_to_send += to_send->iovs[i].iov_len;

			bzero(&hdr, sizeof(hdr));
			hdr.msg_iov = to_send->iovs;
			hdr.msg_iovlen = to_send->iov_cnt;
			ret = sendmsg(conn->fd, &hdr, 0);
			if ((ret < 0) && (errno != EWOULDBLOCK)) {
				lancet_perror("Unknown connection error write\n");
				return;
			}
			assert(ret == bytes_to_send);
			add_pending_tx_timestamp(&per_conn_tx_timestamps[conn->idx], bytes_to_send);
			conn->pending_reqs++;
			avail_reqs--;

			/*BookKeeping*/
			send_res.bytes = ret;
			send_res.reqs = 1;
			add_throughput_tx_sample(send_res);

			/*Schedule next*/
			next_tx += get_ia();
			diff = time_ns() - next_tx;
		}
	REP_PROC:
		/* process responses */
		//ready = epoll_wait(epoll_fd, events, conn_per_thread, 0);
		//diff = (-diff) / 2 + 1;
		//to_process =  (4 * conn_per_thread > diff) ? diff : 4 * conn_per_thread;
		//to_process = 4 * conn_per_thread;
		to_process = 1;
		ready = epoll_wait(epoll_fd, events, to_process, 0);
		for (i = 0; i < ready; i++) {
			//if ((time_ns() - next_tx) > 0)
			//	break;
			idx = events[i].data.u32;
			conn = &connections[idx];
			/* Handle incoming packet */
			if (events[i].events & EPOLLIN) {
				// read into the connection buffer
				ret = timestamp_recv(conn->fd, &conn->buffer[conn->buffer_idx],
						MAX_PAYLOAD-conn->buffer_idx, 0, &rx_timestamp);
				if ((ret < 0) && (errno != EWOULDBLOCK)) {
					lancet_perror("Unknow connection error read\n");
					return;
				}
				if (ret == 0) {
					close(conn->fd);
					lancet_fprintf(stderr, "Connection closed\n");
					conn->closed = 1;
					continue;
				}
				conn->buffer_idx += ret;
				read_res = process_response(conn->buffer, conn->buffer_idx);
				if (read_res.bytes == conn->buffer_idx)
					conn->buffer_idx = 0;
				else if (read_res.bytes < conn->buffer_idx) {
					lancet_fprintf(stderr, "Received half request: %d %ld\n", conn->buffer_idx, read_res.bytes);
					assert(0);
					memcpy(conn->buffer, &conn->buffer[read_res.bytes],
							conn->buffer_idx-read_res.bytes);
					conn->buffer_idx -= read_res.bytes;
				} else
					assert(0);

				assert(read_res.reqs >= 1);
				conn->pending_reqs -= read_res.reqs;
				avail_reqs += read_res.reqs;

				/*
				 * Assume only the last request will have an rx timestamp!
				 */
				if (read_res.reqs > 1)
					blind_skip_tx_timestamps(&per_conn_tx_timestamps[conn->idx], read_res.reqs - 1);

				tx_timestamp = pop_pending_tx_timestamps(&per_conn_tx_timestamps[conn->idx]);
				if (!tx_timestamp) {
					//lancet_fprintf(stderr, "tid:%d\tTry again...\n", get_agent_tid());
					ret = get_tx_timestamp(conn->fd, &per_conn_tx_timestamps[conn->idx]);
					tx_timestamp = pop_pending_tx_timestamps(&per_conn_tx_timestamps[conn->idx]);
					if (!tx_timestamp)
						per_conn_tx_timestamps[conn->idx].consumed++;
				}
				ret = timespec_diff(&latency, &rx_timestamp.time, &tx_timestamp->time);
				if (ret == 0) {
					add_latency_sample(latency.tv_nsec + latency.tv_sec * 1e9,
							&tx_timestamp->time);
				}

				/* Bookkeeping */
				add_throughput_rx_sample(read_res);
			} else if (events[i].events & EPOLLERR) {
				/* Get tx timetamps */
				ret = get_tx_timestamp(conn->fd, &per_conn_tx_timestamps[conn->idx]);
			}
			else if (events[i].events & EPOLLHUP)
				assert(0);
			else {
				error = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
					    lancet_fprintf(stderr, "error = %s\n", strerror(error));

				}
				lancet_fprintf(stderr, "Received unknown event: %x\n", events[i].events);
				//assert(0);
			}
		}
	}

	return;
}

static void symmetric_tcp_main(void) {
	int ready, idx, i, conn_per_thread, ret, bytes_to_send, error;
	long next_tx, diff;
	struct epoll_event *events;
	struct tcp_connection *conn;
	struct request *to_send;
	struct byte_req_pair read_res;
	struct byte_req_pair send_res;
	struct msghdr hdr;
	struct timespec tx_timestamp, rx_timestamp, latency;
	struct timestamp_info *pending_tx;

	if (throughput_open_connections())
		return;

	/*Initializations*/
	conn_per_thread = get_conn_count() / get_thread_count();
	events = malloc(conn_per_thread * sizeof(struct epoll_event));

	next_tx = time_ns();
	while (1) {
		if (!should_load()) {
			next_tx = time_ns();
			continue;
		}
		diff = time_ns() - next_tx;
		if (diff >= 0) {
			conn = pick_conn();
			if (!conn)
				goto REP_PROC;
			to_send = prepare_request();

			// send once
			bytes_to_send = 0;
			for (i=0;i<to_send->iov_cnt;i++)
				bytes_to_send += to_send->iovs[i].iov_len;

			bzero(&hdr, sizeof(hdr));
			hdr.msg_iov = to_send->iovs;
			hdr.msg_iovlen = to_send->iov_cnt;
			time_ns_to_ts(&tx_timestamp);
			ret = sendmsg(conn->fd, &hdr, 0);
			if ((ret < 0) && (errno != EWOULDBLOCK)) {
				lancet_perror("Unknown connection error write\n");
				return;
			}
			assert(ret == bytes_to_send);
			push_complete_tx_timestamp(&per_conn_tx_timestamps[conn->idx], &tx_timestamp);
			conn->pending_reqs++;
			avail_reqs--;

			/*BookKeeping*/
			send_res.bytes = ret;
			send_res.reqs = 1;
			add_throughput_tx_sample(send_res);

			/*Schedule next*/
			next_tx += get_ia();
		}
	REP_PROC:
		/* process responses */
		ready = epoll_wait(epoll_fd, events, conn_per_thread, 0);
		for (i = 0; i < ready; i++) {
			idx = events[i].data.u32;
			conn = &connections[idx];
			/* Handle incoming packet */
			if (events[i].events & EPOLLIN) {
				// read into the connection buffer
				ret = recv(conn->fd, &conn->buffer[conn->buffer_idx],
						MAX_PAYLOAD-conn->buffer_idx, 0);
				if ((ret < 0) && (errno != EWOULDBLOCK)) {
					lancet_perror("Unknow connection error read\n");
					return;
				}
				if (ret == 0) {
					close(conn->fd);
					lancet_fprintf(stderr, "Connection closed\n");
					conn->closed = 1;
					continue;
				}
				time_ns_to_ts(&rx_timestamp);

				conn->buffer_idx += ret;
				read_res = process_response(conn->buffer, conn->buffer_idx);
				if (read_res.bytes == conn->buffer_idx)
					conn->buffer_idx = 0;
				else if (read_res.bytes < conn->buffer_idx) {
					lancet_fprintf(stderr, "Received half request: %d %ld\n", conn->buffer_idx, read_res.bytes);
					assert(0);
					memcpy(conn->buffer, &conn->buffer[read_res.bytes],
							conn->buffer_idx-read_res.bytes);
					conn->buffer_idx -= read_res.bytes;
				} else
					assert(0);

				assert(read_res.reqs >= 1);
				conn->pending_reqs -= read_res.reqs;
				avail_reqs += read_res.reqs;

				/*
				 * Assume only the last request will have an rx timestamp!
				 */
				if (read_res.reqs > 1)
					blind_skip_tx_timestamps(&per_conn_tx_timestamps[conn->idx], read_res.reqs - 1);

				pending_tx = pop_pending_tx_timestamps(&per_conn_tx_timestamps[conn->idx]);
				ret = timespec_diff(&latency, &rx_timestamp, &pending_tx->time);
				if (ret == 0) {
					assert(latency.tv_sec == 0);
					add_latency_sample(latency.tv_nsec, &pending_tx->time);
				}

				/* Bookkeeping */
				add_throughput_rx_sample(read_res);
			} else if (events[i].events & EPOLLERR) {
				/* Get tx timetamps */
				ret = get_tx_timestamp(conn->fd, &per_conn_tx_timestamps[conn->idx]);
			}
			else if (events[i].events & EPOLLHUP)
				assert(0);
			else {
				error = 0;
				socklen_t errlen = sizeof(error);
				if (getsockopt(conn->fd, SOL_SOCKET, SO_ERROR, (void *)&error, &errlen) == 0) {
					    lancet_fprintf(stderr, "error = %s\n", strerror(error));

				}
				lancet_fprintf(stderr, "Received unknown event: %x\n", events[i].events);
				//assert(0);
			}
		}
	}

	return;
}

struct transport_protocol *init_tcp(void)
{
	struct transport_protocol *tp;

	tp = malloc(sizeof(struct transport_protocol));
	if (!tp) {
		lancet_fprintf(stderr, "Failed to alloc transport_protocol\n");
		return NULL;
	}

	tp->tp_main[THROUGHPUT_AGENT] = throughput_tcp_main;
	tp->tp_main[LATENCY_AGENT] = latency_tcp_main;
	tp->tp_main[SYMMETRIC_NIC_TIMESTAMP_AGENT] = symmetric_nic_tcp_main;
	tp->tp_main[SYMMETRIC_AGENT] = symmetric_tcp_main;

	return tp;
}
