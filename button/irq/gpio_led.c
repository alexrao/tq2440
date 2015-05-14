#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

volatile unsigned long *gpbdat = NULL;
volatile unsigned long *gpbcon = NULL;

// LED灯点灯程序
int gpio_led_init(void)
{
	printk("Gpio Leds init");
	gpbcon = (volatile unsigned long *) ioremap(0x56000010, 16);
	gpbdat = gpbcon+1;

	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));
	*gpbcon |= ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));
	return 0;
}

// LED等控制处理函数
void gpio_led_ioctl(unsigned long cmd)
{
	printk("led cmd %lu \n",cmd);

	if(cmd > 0x80)
	{
		printk("turn off led %lu\n", cmd&0x0F);
		*gpbdat |= (1 << ((cmd&0x0F)+4));
	}
	else
	{
		printk("turn on led %lu\n", cmd&0x0F);
		*gpbdat &= ~(1<<((cmd&0x0F)+4));
	}
}
