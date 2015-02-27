#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define MAX_CONNECT_COUNT 10


int listen_socket(int listen_port)
{
	struct sockaddr_in a;
	int s;
	int yes;

	//printf("[listen_socket] enter\n");

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror ("socket");
		return -1;
	}
	yes = 1;
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) <0)
	{
		perror ("setsockopt");
		close(s);
		return -1;
	}

	memset(&a, 0, sizeof(a));
	a.sin_port = htons(listen_port);
	a.sin_family = AF_INET;

	if (bind(s, (struct sockaddr *)&a, sizeof(a)) < 0)
	{
		perror("bind");
		close(s);
		return -1;
	}

	printf ("accepting connections on port %d\n", (int)listen_port);
	listen(s, MAX_CONNECT_COUNT);


	//printf("[listen_socket] out\n");
	return s;
}

int connect_socket(int connect_port, char *address)
{
	struct sockaddr_in a;
	int s;

	//printf("[connect_socket] enter\n");

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		perror("socket");
		close(s);
		return -1;
	}

	memset(&a, 0, sizeof(a));
	a.sin_port = htons(connect_port);
	a.sin_family = AF_INET;

	if (!inet_aton(address, (struct in_addr *)&a.sin_addr.s_addr))
	{
		perror("bad IP address format");
		close(s);
		return -1;
	}

	if (connect(s, (struct sockaddr *)&a, sizeof(a)) < 0)
	{
		perror("connect()");
		shutdown(s, SHUT_RDWR);
		close(s);
		return -1;
	}

	//printf("[connect_socket] out\n");

	return s;
}