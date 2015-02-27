#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>

#include "my_socket.h"
#include "ivshmem.h"

#define SHM_DEV "/dev/uio0"
#define MSI_VECTOR 0 /* the default vector */
#define LOCALHOST "127.0.0.1"
#define BUF_SIZE (1024 * 1024 * 2)
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))
#define MAX_CONNECT_COUNT 10 /* 最大支持连接的数目，因为用数组保存连接，并且经常遍历数组，该值不宜过大 */

int do_select(int fd)
{
    fd_set readset;
    FD_ZERO(&readset);
    FD_SET(fd, &readset);
    select(fd + 1, &readset, NULL, NULL, NULL);
    return 1;
}

int main(int argc, char **argv)
{
	int fd_shm_dev, fd_sock[MAX_CONNECT_COUNT], fd_listen;
	void * memptr, * regptr;
	int port;
	int other_vm;
	char *buf[MAX_CONNECT_COUNT];
	int buf_pos;
	int size[MAX_CONNECT_COUNT], sin_size;
	int nfds;
	struct timeval timeout, t;
	t.tv_sec = timeout.tv_sec = 0;
	t.tv_usec = timeout.tv_usec = 0;

	int cer_fd;
	for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
	{
		fd_sock[cer_fd] = -1;
		size[cer_fd] = 0;
		buf[cer_fd] = (char *)malloc(BUF_SIZE);
	}

	if (3 != argc)
	{
		printf("Usage: %s other_vm_ID port\n", argv[0]);
		exit(1);
	}

	other_vm = atoi(argv[1]);
	port = atoi(argv[2]);

	fd_shm_dev = open(SHM_DEV, O_RDWR);
	printf("[main] opening file %s\n", SHM_DEV);
	if (fd_shm_dev <= 0)
	{
		printf("[main] open file %s\n failed\n", SHM_DEV);
		exit(1);
	}

	if ((regptr = mmap(NULL, 256, PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm_dev, 0 * getpagesize())) == (void *) -1)
	{
	    printf("[main] regptr mmap failed (0x%p)\n", regptr);
	    close(fd_shm_dev);
	    exit (-1);
	}

	if ((memptr = mmap(NULL, BUF_SIZE * (MAX_CONNECT_COUNT + 1), PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm_dev, 1 * getpagesize())) == (void *) -1)
	{
	    printf("[main] memptr mmap failed (0x%p)\n", memptr );
	    close(fd_shm_dev);
	    exit (-1);
	}

	/* 初始化完毕 */

	while(1)
	{
		/* 等待client端处理数据 */
		do_select(fd_shm_dev);
		ivshmem_recv(fd_shm_dev);

		/* 从ivshmem中读取数据 */
		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{
			size[cer_fd] = ((int *)memptr)[cer_fd];
			//printf("fd_sock[%d] is %d, size[%d] is %d\n", cer_fd, fd_sock[cer_fd], cer_fd, size[cer_fd]);
			if (size[cer_fd] > 0)
			{
				memcpy(buf[cer_fd], memptr + BUF_SIZE * (cer_fd + 1), size[cer_fd]);

				/* size>0 说明client端有连接并且发来数据，如果server端对应的socket fd号为-1
				 * 说明这是一个新连接，因此server端也建立一个新连接
				 */
				if (fd_sock[cer_fd] < 0)
				{
					fd_sock[cer_fd] = connect_socket(port, LOCALHOST);
					printf("[main] connect to %s:%d\n", LOCALHOST, port);
				}
				
			}
			else if (size[cer_fd] == -1)
			{
				/* 如果size值为-1，说明client端对应的socket连接已经断开，因此也要断开server端
				 * 对应对连接
				 */
				if (fd_sock[cer_fd] > 0)
				{
					shutdown(fd_sock[cer_fd], SHUT_RDWR);
					close(fd_sock[cer_fd]);
					fd_sock[cer_fd] = -1;
				}
			}
			else if (size[cer_fd] == -2)
			{
				/* 如果size值为-2，则建立新的连接 */
				if (fd_sock[cer_fd] < 0)
				{
					fd_sock[cer_fd] = connect_socket(port, LOCALHOST);
					printf("[main] connect to %s:%d\n", LOCALHOST, port);
				}
			}
		}		

		/* 把从ivshmem中获取到的数据发动到对应的socket连接去 */
		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{
			if (fd_sock[cer_fd] > 0)
			{
				buf_pos = 0;
				while(size[cer_fd] > 0)
				{
					/* 分段发送，每次最长2KB */
					if (send(fd_sock[cer_fd], buf[cer_fd] + buf_pos, size[cer_fd] > 2048 ? 2048 : size[cer_fd], 0) == -1)
					{
						printf("[main] error at send()\n");
						close(fd_sock[cer_fd]);
						exit(1);
					}
					else
					{
						buf_pos += 2048;
						size[cer_fd] -= 2048;
					}
				}	
			}
		}
		
		nfds = 0;
		fd_set readfds;
		FD_ZERO(&readfds);
		/* 把现有的socket连接都添加进可读集合中 */
		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{
			if (fd_sock[cer_fd] > 0)
			{
				FD_SET(fd_sock[cer_fd] , &readfds);
				nfds = max(nfds, fd_sock[cer_fd]);
			}
		}

		/* 开始select */
		timeout.tv_usec = t.tv_usec;
		select(nfds + 1, &readfds, NULL, NULL, &timeout);

		/* 从server程序读取数据 */
		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{	
			if (fd_sock[cer_fd] > 0)
			{
				if (FD_ISSET(fd_sock[cer_fd], &readfds))
				{
					size[cer_fd] = read(fd_sock[cer_fd], buf[cer_fd], BUF_SIZE);

					/* 如果一个连接接收到可读信号（或中断），但是没有读到任何内容，
					 * 则关闭这个连接，同时把size值设为-1通知server端VM该连接已经断开
					 */
					if (size[cer_fd] < 1)
					{
						shutdown(fd_sock[cer_fd], SHUT_RDWR);
						close(fd_sock[cer_fd]);
						size[cer_fd] = -1;
						fd_sock[cer_fd] = -1;
					}
				}
				else
				{
					size[cer_fd] = 0;
				}
			}
		}

		/* 把从server程序读取到的数据写进ivshmem中 */
		//printf("write data into ivshmem\n");
		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{
			if (fd_sock[cer_fd] > 0)
			{
				((int *)memptr)[cer_fd] = size[cer_fd];
				//printf("write %d bytes", ((int *)memptr)[0]);
				if (size[cer_fd] > 0)
				{
					memcpy(memptr + BUF_SIZE * (cer_fd + 1), buf[cer_fd], size[cer_fd]);
					t.tv_usec = 0;
				}
			}
		}

		for (cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
		{
			if (size[cer_fd] == -2 || size[cer_fd] == 0)
				if (t.tv_usec < 1000)
					t.tv_usec += 20;
		}

		//printf("timeout.tv_usec is %d\n", t.tv_usec);
			

		/* 向clinet端VM发送可读信号 */
		ivshmem_send(regptr, MSI_VECTOR, other_vm);
	}


	for(cer_fd = 0; cer_fd < MAX_CONNECT_COUNT; cer_fd++)
	{
		if (fd_sock > 0)
			close(fd_sock[cer_fd]);
		free(buf[cer_fd]);
	}
	close(fd_shm_dev);

	return 0;
}