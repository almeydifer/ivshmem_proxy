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
#define BUF_SIZE 1024 * 1024 * 2

int do_select (int fd)
{
    fd_set readset;
    FD_ZERO(&readset);
    /* conn socket is in Live_vms at posn 0 */
    FD_SET(fd, &readset);
    select(fd + 1, &readset, NULL, NULL, NULL);
    return 1;
}

int main(int argc, char **argv)
{
	int fd_shm_dev, fd_sock = -1, fd_listen;
	void * memptr, * regptr;
	int port;
	int other_vm;
	char buf[BUF_SIZE];
	int buf_pos;
	int size, sin_size;
	struct timeval timeout, t;
	t.tv_sec = timeout.tv_sec = 0;
	t.tv_usec = timeout.tv_usec = 0;

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

	if ((memptr = mmap(NULL, BUF_SIZE + 4, PROT_READ|PROT_WRITE, MAP_SHARED, fd_shm_dev, 1 * getpagesize())) == (void *) -1)
	{
	    printf("[main] memptr mmap failed (0x%p)\n", memptr );
	    close(fd_shm_dev);
	    exit (-1);
	}


	while(1)
	{
		if (fd_sock < 0)
		{
			fd_sock = connect_socket(port, LOCALHOST);
			printf("[main] connect to %s:%d\n", LOCALHOST, port);
		}
		//printf("fd_sock id %d\n", fd_sock);
		
		/* waiting for frontend to finish writing */
		//printf("waiting for vm%d\n", other_vm);
		do_select(fd_shm_dev);
		ivshmem_recv(fd_shm_dev);
		//printf("ivshmem_recv run\n");

		/* read data from ivshmem */
		//printf("read data from ivshmem\n");
		size = ((int *)memptr)[0];
		if (size > 0)
		{
			memcpy(buf, memptr + 4, size);
			/*printf("read %d bytes\n", size);
			printf("the data from ivshmem is \n");
			int i;
			for(i = 0; i < size; i++)
			{
				printf("%c", buf[i]);
			}
			printf("\n");*/
			
		}
		else if (size == -1)
		{
			shutdown(fd_sock, SHUT_RDWR);
			close(fd_sock);
			fd_sock = -1;
			continue;
		}

		

		/* send data to local program */
		buf_pos = 0;
		while(size > 0)
		{
			//printf("send data to local program\n");
			if (send(fd_sock, buf + buf_pos, size > 2048 ? 2048 : size, 0) == -1)
			{
				printf("[main] error at send()\n");
				close(fd_sock);
				exit(1);
			}
			else
			{
				buf_pos += 2048;
				size -= 2048;
			}
		}

		/* read data from local program */
		//printf("read data from local program\n");
		fd_set readset;
		FD_ZERO(&readset);
		FD_SET(fd_sock, &readset);
		timeout.tv_usec = t.tv_usec;
		select(fd_sock + 1, &readset, NULL, NULL, &timeout);
		if (FD_ISSET(fd_sock, &readset))
		{
			//printf("read data from local program\n");
			size = read(fd_sock, buf, BUF_SIZE);
			/*if (size != 0)
			{
				printf("the data from local program is\n");
				int i;
				for(i = 0; i < size; i++)
				{
					printf("%c", buf[i]);
				}
				printf("\n");
			}*/
		}
		else
		{
			size = 0;
		}
		//printf("the data read from local program:\n%s\n", buf);
		

		/* write data into ivshmem */
		//printf("write data into ivshmem\n");
		((int *)memptr)[0] = size;
		//printf("write %d bytes", ((int *)memptr)[0]);
		if (size != 0)
		{
			memcpy(memptr + 4, buf, size);
			t.tv_usec = 0;
		}
		else
		{
			if (t.tv_usec < 1000)
				t.tv_usec += 100;
		}
		//printf("timeout.tv_usec is %d\n", t.tv_usec);
			

		/* tell the backend to read */
		ivshmem_send(regptr, MSI_VECTOR, other_vm);
		//printf("ivshmem_send run\n");
	}

	close(fd_sock);
	close(fd_shm_dev);

	return 0;
}