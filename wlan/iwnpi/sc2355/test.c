#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <string.h>
#include <unistd.h>
#include "test.h"

#define MULTICAST_CMD		"multicast"
#define MULTICAST_ADDR		"239.0.1.6"
#define SOCKET_PORT		33333

static int sprdwl_add_mc_addr(char *mc_addr, int port)
{
	int fd;
	int ret, index = 0;
	int num = 0;
	struct sockaddr_in addr;
	struct sockaddr_in from;
	socklen_t socklen = sizeof(from);
	struct ip_mreq req;
	char buf[128] = {0x00};

	printf("mc addr : %s, socket_port : %d\n", mc_addr, port);
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		printf("create socket failed\n");
		return -1;
	}else
		printf("create socket success\n");

	memset((void *)&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(fd, (void*)&addr, sizeof(addr));
	if (ret < 0) {
		printf("bind error\n");
		close(fd);
		return -1;
	} else
		printf("bind success\n");

	req.imr_interface.s_addr = htonl(INADDR_ANY);
	req.imr_multiaddr.s_addr =  inet_addr(mc_addr);

	ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &req, sizeof(req));
	if(ret < 0) {
		printf("set opt failed.\n");
		close(fd);
		return -1;
	} else {
		printf("set opt success\n");
	}

	while(1) {
		memset(buf, 0, sizeof(buf));

		ret = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &socklen);
		printf("receive packet, num : %d\n", num++);
		index++;

	}

	close(fd);
	return 0;
}

int sprdwl_handle_test(int argc, char **argv)
{
	if (argc < 1) {
		printf("format : iwnpi test cmd\n");
		return 0;
	}
	printf("iwnpi handle test : %s\n", argv[0]);
	if (0 == strcmp(argv[0], MULTICAST_CMD)) {
		if (argc > 1 && 0 == strcmp(argv[1], "help")) {
			printf("format : iwnpi test multicast <mc_addr> <socket_port>\n");
			return 0;
		}
		if (argc == 3) {
			sprdwl_add_mc_addr(argv[1], atoi(argv[2]));
		}
		else {
			printf("%s test using default paras\n", argv[0]);
			sprdwl_add_mc_addr(MULTICAST_ADDR, SOCKET_PORT);
		}
	}
	else
		printf("unsupported test : %s\n", argv[0]);

	return 0;
}
