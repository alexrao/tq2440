#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>


int main(int argc, char **argv)
{
	int fd;
	int i;
	int val;
	unsigned char key_vals[4];
	
	fd = open("/dev/button", O_RDWR);
	if(fd < 0)
	{
		printf("cat't open /dev/buttons \n");
		return 0;
	}
	(void*)argv;
	(void)argc;
	
	memset(key_vals, 0, sizeof(key_vals));
	
	while(1)
	{
		read(fd, key_vals, sizeof(key_vals));
		if (!key_vals[0] || !key_vals[1] || !key_vals[2] || !key_vals[3])
		{
			printf("key pressed: %d %d %d %d\n", key_vals[0], key_vals[1], key_vals[2], key_vals[3]);
			
			// 如果Key0和Key1都按下则全亮
			if(!key_vals[0] && !key_vals[1])
			{
				val = 1;
				write(fd, &val, 4);
				printf("all leds light\n");
			}
			else if (!key_vals[2] && !key_vals[3])
			{
				val = 0;
				printf("all leds black\n");
				write(fd, &val, 4);
			}
			else
			{
				val = 0;
				for(i=0; i<4; i++)
				{
					if(!key_vals[i])
					{
						val |= 1<<i;
						printf("Led:%d, ",i );
					}
				}
				printf("light.val = 0x%0x\n",val);
				ioctl(fd,val,0);

			}
			//usleep(1);
		}
		usleep(100*1000);
		
	}
	
}