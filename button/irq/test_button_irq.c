
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

/* thirddrvtest 
  */
int main(int argc, char **argv)
{
	int fd;
	unsigned char key_val;
	
	fd = open("/dev/button_irq", O_RDWR);
	if (fd < 0)
	{
		printf("can't open!\n");
	}

	while (1)
	{
		
		if(read(fd, &key_val, 1) == 1)
		{
			ioctl(fd,key_val,0);
			printf("key_val = 0x%x\n", key_val);
		}
	//	usleep(100*1000);
	}
	
	return 0;
}

