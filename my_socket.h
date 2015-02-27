#ifndef MY_SOCKET
#define MY_SOCKET

int listen_socket(int listen_port);
int connect_socket(int connect_port, char *address);

#endif