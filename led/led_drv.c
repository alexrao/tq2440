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

static struct class *gpiodrv_class;
static struct class_device *gpiodrv_class_dev;

volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;

static int gpio_drv_open(struct inode *inode, struct file *file)
{
	//printk("first_drv_open\n");
	/*
	 * LED1,LED2,LED4对应GPB5、GPB6、GPB7、GPB8
	 */
	/* 配置GPB5,6,7,8为输出 */
	printk("gpio data open\n");
	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));
	*gpbcon |= ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));
	return 0;
}

static int gpio_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int val;
	printk("gpio data write\n");

	copy_from_user(&val, buf, count);
	if(val == 1)
	{
		// 点灯
		*gpbdat &= ~((1<<5) | (1<<6) | (1<<7) | (1<<8));
	}
	else
	{
		// 灭灯
		*gpbdat |= (1<<5) | (1<<6) | (1<<7) | (1<<8);
	}
	
	return 0;
}

static int gpio_led_ioctl(struct inode * inode, struct file * file,unsigned int cmd,unsigned long arg)
{
	unsigned long leddat;
    if(arg > 4)

    {

      return -1;           

    }

	*gpbdat &= ~(0xF0);
	*gpbdat |= (0xF0&(cmd << 5));
	
	return 0;

}	
static struct file_operations gpio_drv_fops = {
	.owner = THIS_MODULE,
	.open = gpio_drv_open,
	.write = gpio_drv_write,
	.ioctl = gpio_led_ioctl,
};

int major;
static int gpio_drv_init(void)
{
	major = register_chrdev(0, "gpio_dev", &gpio_drv_fops); // 注册，通知内核
	gpiodrv_class = class_create(THIS_MODULE, "gpiodrv");
	gpiodrv_class_dev = device_create(gpiodrv_class, NULL, MKDEV(major, 0), NULL, "leds");
	gpbcon = (volatile unsigned long *) ioremap(0x56000010, 16);
	gpbdat = gpbcon+1;
	printk("gpio driver init\n");
	return 0;
}

static void gpio_drv_exit(void)
{
	printk("gpio driver exit\n");
	unregister_chrdev(major, "gpio_dev"); // 卸载
	device_unregister(gpiodrv_class_dev);
	class_destroy(gpiodrv_class);
	iounmap(gpbcon);
}

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");
