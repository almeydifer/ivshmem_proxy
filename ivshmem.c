#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <openssl/sha.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "ivshmem.h"

enum ivshmem_registers {
    IntrMask = 0,
    IntrStatus = 4,
    IVPosition = 8,
    Doorbell = 12
};

char * ivshmem_strings[32] = { "SET_SEMA", "DOWN_SEMA", "EMPTY", "WAIT_EVENT", "WAIT_EVENT_IRQ", "GET_POSN", "GET_LIVELIST", "SEMA_IRQ" };

int ivshmem_recv(int fd)
{

    int rv, buf;

    buf = 0;
    rv = read(fd, &buf, sizeof(buf));

    //printf("[RECVIOCTL] rv is %d\n", rv);

    if (rv == -EAGAIN) {
        fprintf(stderr, "EAGAIN\n");
    } else if (rv < 0) {
        fprintf(stderr, "other error\n");
    }

    /* TODO: check status register */

    return buf;
}

int ivshmem_send(void * regs, int ivshmem_cmd, int destination_vm)
{

    int *array;
    int msg;

    //printf("[SENDIOCTL] %s\n", ivshmem_strings[ivshmem_cmd]);
    array = (int *) regs;
    msg = ((destination_vm & 0xffff) << 16) + (ivshmem_cmd & 0xffff);

    array[Doorbell/sizeof(int)] = msg;

    //printf("[SENDIOCTL] rv is %d\n", rv);

    return 0;
}
