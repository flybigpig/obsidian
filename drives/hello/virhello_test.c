#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
int main(int argc, char *argv[])
{
    printf("app pid=%d is running\n", getpid());
    int fd = open("/dev/virtdev", O_RDWR);
    printf("ret:%d\n", fd);
    if (fd < 0)
    {
        printf("errno:%d\n", errno);
    }
    else
    {
        sleep(10);
        close(fd);
    }
    printf("app pid=%d is over\n", getpid());
    return 0;
}
