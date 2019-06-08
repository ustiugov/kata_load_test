#include <assert.h>
#include <fcntl.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <lancet/misc.h>

#define MAX_EVENTS 64
#define BACKLOG 8192
#define MAX_THREADS 64

static __thread int epollfd;
static int port;
static unsigned long s_ip;

static void setnonblocking(int fd)
{
	int flags;

	flags = fcntl(fd, F_GETFL, 0);
	assert(flags >= 0);
	flags = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	assert(flags >= 0);
}

static void epoll_ctl_add(int fd)
{
	struct epoll_event ev;

	ev.events = EPOLLIN | EPOLLERR;
	ev.data.fd = fd;
	if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		perror("epoll_ctl: EPOLL_CTL_ADD");
		exit(EXIT_FAILURE);
	}
}

static void spin(long usecs)
{
    long start;

     start = time_us();
     while (time_us() < start + usecs)
         asm volatile("pause");
}

static void process_req(int fd)
{
	long to_spin, reply = 42;
	int ret;

	ret = read(fd, &to_spin, sizeof(long));
	assert(ret == sizeof(long));
	spin(to_spin);

	ret = write(fd, &reply, sizeof(long));
	assert(ret == sizeof(long));
}

void *tcp_thread_main(void *arg)
{
	struct sockaddr_in sin;
	int sock;
	int one;
	int ret, i, nfds, conn_sock;
	int thread_no;
	struct epoll_event ev, events[MAX_EVENTS];
	struct conn *conn;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock) {
		perror("socket");
		exit(1);

	}

	setnonblocking(sock);

	one = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void *) &one, sizeof(one))) {
		perror("setsockopt(SO_REUSEPORT)");
		exit(1);

	}

	one = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &one, sizeof(one))) {
		perror("setsockopt(SO_REUSEADDR)");
		exit(1);

	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	//sin.sin_addr.s_addr = htonl(0);
	sin.sin_addr.s_addr = s_ip;
	sin.sin_port = htons(port);

	if (bind(sock, (struct sockaddr*)&sin, sizeof(sin))) {
		perror("bind");
		exit(1);

	}

	if (listen(sock, BACKLOG)) {
		perror("listen");
		exit(1);
	}

	thread_no = (long) arg;
	epollfd = epoll_create1(0);
	ev.events = EPOLLIN;
	ev.data.u32 = 0;
	ret = epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev);
	assert(!ret);

	while (1) {
		nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
		assert(nfds > 0);
		for (i = 0; i < nfds; i++) {
			if (events[i].data.u32 == 0) {
				conn_sock = accept(sock, NULL, NULL);
				if (conn_sock == -1) {
					perror("accept");
					exit(EXIT_FAILURE);
				}
				setnonblocking(conn_sock);
				if (setsockopt(conn_sock, IPPROTO_TCP, TCP_NODELAY, (void *) &one, sizeof(one))) {
					perror("setsockopt(TCP_NODELAY)");
					exit(1);
				}
				epoll_ctl_add(conn_sock);
			} else {
				conn = events[i].data.ptr;
				if (events[i].events & (EPOLLHUP | EPOLLERR))
					close(events[i].data.fd);
				else
					process_req(events[i].data.fd);
			}
		}
	}
}

int main(int argc, char *argv[])
{
	int i, thread_no;
	pthread_t tid;

	if (argc < 3) {
		printf("Usage: %s <thread_count> port [ip_to_listen_on]\n", argv[0]);
		return -1;
	}
	thread_no = atoi(argv[1]);
	port = atoi(argv[2]);
        if (argc == 4) {
            printf("Listening to interface with ip=%s\n", argv[3]);
            s_ip = inet_addr(argv[3]);
        }
        else {
            printf("Listening to all interfaces\n");
            s_ip = htons(0); // all addresses
        }

	for (i = 1; i < thread_no; i++) {
		if (pthread_create(&tid, NULL, tcp_thread_main, (void *) (long) i)) {
			fprintf(stderr, "failed to spawn thread %d\n", i);
			exit(-1);
		}
	}

	tcp_thread_main(0);
}
