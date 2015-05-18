#include <stdio.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

int fd;
int flag;

void my_signal_fun(int signum)
{
	unsigned char key_val;
	read(fd, &key_val, 1);

	flag ++;
	printf("key_val:0x%02x\n", key_val);
}

int main(int argc, char **argv)
{
	unsigned char key_val;
	int ret;
	int oflags;

	flag = 0;
	signal(SIGIO, my_signal_fun);

	fd = open("/dev/fasync_drv", O_RDWR);
	if(fd < 0)
	{
		printf("Can't open!\n");
	}
	fcntl(fd, F_SETOWN, getpid());
	oflags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, oflags|FASYNC);

	while(flag < 10)
	{
		sleep(1);
	}
	close(fd);
	return 0;
}