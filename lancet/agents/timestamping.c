
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
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <linux/net_tstamp.h>
#include <string.h>
#include <linux/sockios.h>
#include <assert.h>
#include <linux/if.h>
#include <linux/errqueue.h>
#include <linux/net_tstamp.h>
#include <linux/sockios.h>
#include <linux/version.h>
#include <errno.h>


#include <lancet/error.h>
#include <lancet/timestamping.h>

#define CONTROL_LEN 1024

static __thread int received_opid;

static int set_timestamping_filter(int fd, char * if_name, int rx_filter, int tx_type)
{
	struct ifreq ifr;
	struct hwtstamp_config config;

	config.flags = 0;
	config.tx_type = tx_type;
	config.rx_filter = rx_filter;

	strcpy(ifr.ifr_name, if_name);
	ifr.ifr_data = (caddr_t)&config;

	if(ioctl(fd, SIOCSHWTSTAMP, &ifr)){
		lancet_perror("ERROR setting NIC timestamping: ioctl SIOCSHWTSTAMP");
		return -1;
	}
	return 0;
}

/*
 * Returns -1 if no new timestamp found
 * 0 if timestamp found but optid doesn't match
 * 1 if timestamp found and optid matches
 */
static int extract_timestamps(struct msghdr * hdr, struct timestamp_info *dest)
{
	struct cmsghdr *cmsg;
	struct scm_timestamping *ts;
	int found = -1;

	for(cmsg = CMSG_FIRSTHDR(hdr); cmsg != NULL; cmsg = CMSG_NXTHDR(hdr, cmsg)){
		if(cmsg->cmsg_type == SCM_TIMESTAMPING){
			ts = (struct scm_timestamping *)CMSG_DATA(cmsg);
			if(ts->ts[2].tv_sec != 0){
				//make sure we don't get multiple timestamps for the same
				assert(found == -1);
				dest->time = ts->ts[2];
				found = 1;
			}
		} else if(cmsg->cmsg_type == IP_RECVERR) {
			struct sock_extended_err * se = (struct sock_extended_err*)CMSG_DATA(cmsg);
			/*
			 * Make sure we got the timestamp for the right request
			 */
			if(se->ee_errno == ENOMSG && se->ee_origin == SO_EE_ORIGIN_TIMESTAMPING) {
				if (dest->optid < (se->ee_data+1)) {
					found = 0;
					received_opid = se->ee_data;
				} else if (dest->optid > (se->ee_data+1)) {
					found = -1;
				}  else
					found = 1;
			}
			else
				lancet_fprintf(stderr, "Received IP_RECVERR: errno = %d %s\n",
						se->ee_errno, strerror(se->ee_errno));
		}
	}
	return found;
}

int enable_nic_timestamping(char * if_name)
{
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int filter = HWTSTAMP_FILTER_ALL;
	int tx_type = HWTSTAMP_TX_ON;
	int ret;

	ret = set_timestamping_filter(fd, if_name, filter, tx_type);
	close(fd);
	return ret;
}

int disable_nic_timestamping(char * if_name)
{
	int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	int ret = set_timestamping_filter(fd, if_name, HWTSTAMP_FILTER_NONE, HWTSTAMP_TX_OFF);
	close(fd);
	return ret;
}

int sock_enable_timestamping(int fd)
{
	int ts_mode = 0;

	ts_mode |= SOF_TIMESTAMPING_RX_HARDWARE | SOF_TIMESTAMPING_RAW_HARDWARE | SOF_TIMESTAMPING_TX_HARDWARE;
	ts_mode |= SOF_TIMESTAMPING_OPT_TSONLY | SOF_TIMESTAMPING_OPT_ID;

	if(setsockopt(fd, SOL_SOCKET, SO_TIMESTAMPING , &ts_mode, sizeof(ts_mode)) < 0){
		lancet_perror("ERROR enabling socket timestamping: setsockopt SO_TIMESTAMPING.");
		return -1;
	}
	return 0;
}

ssize_t timestamp_recv(int sockfd, void *buf, size_t len, int flags,
		struct timestamp_info *last_rx_time)
{
	char recv_control[CONTROL_LEN] = {0};
	int nbytes;
	struct msghdr hdr = {0};
	struct iovec recv_iov = {buf, len};

	hdr.msg_iov = &recv_iov;
	hdr.msg_iovlen = 1;
	hdr.msg_control = recv_control;
	hdr.msg_controllen = CONTROL_LEN;

	nbytes = recvmsg(sockfd, &hdr, flags);

	if(nbytes <= 0)
		return nbytes;

	bzero(last_rx_time, sizeof(struct timestamp_info));
	extract_timestamps(&hdr, last_rx_time);

	return nbytes;
}

/*
 * Used only for NIC timestamping
 * 1 if timestamp found
 * 0 if timestamp not found
 */
int get_tx_timestamp(int sockfd, struct pending_tx_timestamps *tx_timestamps)
{
	char tx_control[CONTROL_LEN] = {0};
	struct msghdr mhdr = {0};
	struct iovec junk_iov = {NULL, 0};
	int n;
	struct timestamp_info *ts_info, *curr;

	mhdr.msg_iov = &junk_iov;
	mhdr.msg_iovlen = 1;
	mhdr.msg_control = tx_control;
	mhdr.msg_controllen = CONTROL_LEN;

	n = recvmsg(sockfd, &mhdr, MSG_ERRQUEUE);
	if (n<0)
		return 0;

	assert(n==0);

	// Many requests might have the same timestamp because they got coalesced
	ts_info = &tx_timestamps->pending[tx_timestamps->tail % MAX_PENDING_REQS];
	n = extract_timestamps(&mhdr, ts_info);
	if (n == -1) {
		n = 0;
		goto OUT;
	}
	add_tx_timestamp(&ts_info->time);
	tx_timestamps->tail++;
	if (n == 1) {
		goto OUT;
	}
	curr = ts_info;
	ts_info = &tx_timestamps->pending[tx_timestamps->tail % MAX_PENDING_REQS];
	while ((ts_info->optid <= received_opid+1) &&
		(tx_timestamps->tail < tx_timestamps->head)) {
		//lancet_fprintf(stderr, "loop1\n");
		ts_info->time = curr->time;
		add_tx_timestamp(&ts_info->time);
		ts_info = &tx_timestamps->pending[++tx_timestamps->tail % MAX_PENDING_REQS];
	}
	n = 1;
OUT:
	//lancet_fprintf(stderr, "tid:%d\tget_tx_timestamp = %d\n", get_agent_tid(), n);
	return n;
}

void add_pending_tx_timestamp(struct pending_tx_timestamps *tx_timestamps,
		uint32_t bytes)
{
	tx_timestamps->tx_byte_counter += bytes;
	tx_timestamps->pending[tx_timestamps->head++ % MAX_PENDING_REQS].optid = tx_timestamps->tx_byte_counter;
}

struct timestamp_info *pop_pending_tx_timestamps(struct pending_tx_timestamps
		*tx_timestamps)
{
	struct timestamp_info *ret;
	assert(tx_timestamps->consumed <= tx_timestamps->head);
	if (tx_timestamps->consumed < tx_timestamps->tail)
		ret = &tx_timestamps->pending[tx_timestamps->consumed++ % MAX_PENDING_REQS];
	else {
		//lancet_fprintf(stderr, "tid:%d\tHaven't received tx timestamp yet\n", get_agent_tid());
		//tx_timestamps->consumed++;
		ret = NULL;
	}
	return ret;
}

void push_complete_tx_timestamp(struct pending_tx_timestamps *tx_timestamps,
		struct timespec *to_add)
{
	struct timestamp_info *ts_info;

	ts_info = &tx_timestamps->pending[tx_timestamps->tail % MAX_PENDING_REQS];
	ts_info->time = *to_add;
	// this is confusing but the consumed is used when receiving the reply
	tx_timestamps->head++;
	tx_timestamps->tail++;
	add_tx_timestamp(&ts_info->time);
}

int timespec_diff(struct timespec * res, struct timespec * a, struct timespec * b)
{
	uint32_t billion = 1e9;

	if (!a || !b)
		return -1;
	if(a->tv_sec == 0 || b->tv_sec == 0)
		return -1;

	if(a->tv_nsec < b->tv_nsec) {
		res->tv_nsec = b->tv_nsec - a->tv_nsec;
		res->tv_nsec = billion - res->tv_nsec;
		res->tv_sec = a->tv_sec - 1 - b->tv_sec;

	} else {
		res->tv_nsec = a->tv_nsec - b->tv_nsec;
		res->tv_sec = a->tv_sec - b->tv_sec;
	}

	return 0;
}

