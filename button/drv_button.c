#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <asm/irq.h>

static struct class *button_class;
static struct class_device *button_class_dev;

volatile unsigned long *gpbcon = NULL;
volatile unsigned long *gpbdat = NULL;
volatile unsigned long *gpfcon;
volatile unsigned long *gpfdat;

static int button_drv_open(struct inode *inode, struct file *file)
{
	printk("button driver open\n");
		/* 配置GPF1、GPF4、GPF2、GPF0为输入引脚 */
	*gpfcon &= ~((0x3<<(1*2)) | (0x3<<(4*2)) | (0x3<<(2*2)) | (0x3<<(0*2)));
	
	*gpbcon &= ~((0x3<<(5*2)) | (0x3<<(6*2)) | (0x3<<(7*2)) | (0x3<<(8*2)));
	*gpbcon |= ((0x1<<(5*2)) | (0x1<<(6*2)) | (0x1<<(7*2)) | (0x1<<(8*2)));

	return 0;
}

ssize_t button_drv_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
	unsigned char key_vals[4];
	int regval;
	
	if(size != sizeof(key_vals))
	{
		return -EINVAL;
	}
	
	regval = *gpfdat;
	key_vals[0] = (regval & (1<<1)) ? 1 : 0;
	key_vals[1] = (regval & (1<<4)) ? 1 : 0;
	key_vals[2] = (regval & (1<<2)) ? 1 : 0;
	key_vals[3] = (regval & (1<<0)) ? 1 : 0;
	
	copy_to_user(buf, key_vals, sizeof(key_vals));
	
	return sizeof(key_vals);
}

static int button_drv_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	int val;
	printk("button driver data write\n");
	copy_from_user(&val, buf, count);
	if (val == 1)
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

static int button_drv_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
    if(arg > 4)
    {
      return -1;           
    }

	*gpbdat &= ~(0xF0);
	*gpbdat |= (0xF0&(cmd << 5));
	
	return 0;	
}

static struct file_operations button_drv_fops = 
{
	.owner = THIS_MODULE,
	.open = button_drv_open,
	.read = button_drv_read,
	.write = button_drv_write,
	.ioctl = button_drv_ioctl,
};


int major;
static int button_drv_init(void)
{
	major = register_chrdev(0, "button_dev", &button_drv_fops);//注册通知内核
	button_class = class_create(THIS_MODULE, "buttondrv");
	button_class_dev = device_create(button_class, NULL, MKDEV(major, 0), NULL, "button");
	
	gpbcon = (volatile unsigned long *) ioremap(0x56000010, 16);
	gpbdat = gpbcon+1;
	
	gpfcon = (volatile unsigned long *)ioremap(0x56000050, 16);
	gpfdat = gpfcon + 1;
	
	printk("button and led gpio init\n");
	return 0;
}

static void button_drv_exit(void)
{
	printk("button driver exit\n");
	unregister_chrdev(major, "button_dev");
	device_unregister(button_class_dev);
	class_destroy(button_class);
	iounmap(gpbcon);
	iounmap(gpfcon);
	return 0;
}

module_init(button_drv_init);
module_exit(button_drv_exit);

MODULE_LICENSE("GPL"); //遵循GPL协议 
MODULE_AUTHOR("RXP"); 
MODULE_DESCRIPTION("test driver button and gpio");

