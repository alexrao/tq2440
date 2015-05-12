#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	int fd;
	int i;
	int val = 1;
	fd = open("/dev/leds", O_RDWR);
	if(fd < 0)
	{
		printf("can't open !\n");
	}
	
	

	// if (argc != 2)
	// {
		// printf("Usage :\n");
		// printf("%s <on|off>\n", argv[0]);
		// return 0;
	// }
	
	// if(argv[1], "on" == 0)
	// {
		// val = 1;
	// }
	// else
	// {
		// val = 0;
	// }
	
	for(i=0; i<3; i++)
	{
		val = i%2;
		if(val > 0)
			printf("val =%d Led on\n",val);
		else
			printf("val =%d Led OFF\n",val);
		
		write(fd, &val, 4);
		sleep(1);
	}
	printf("ioctl set\n");
	for(i=0; i< 100; i++)
	{
		printf("led:%d\n",i%4);
		ioctl(fd,i%16,0);
		sleep(1);
	
	}
	return 0;
}