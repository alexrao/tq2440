#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/device.h>

#define __IRQT_FALEDGE IRQ_TYPE_EDGE_FALLING
#define __IRQT_RISEDGE IRQ_TYPE_EDGE_RISING
#define __IRQT_LOWLVL IRQ_TYPE_LEVEL_LOW
#define __IRQT_HIGHLVL IRQ_TYPE_LEVEL_HIGH
#define IRQT_NOEDGE (0)
#define IRQT_RISING (__IRQT_RISEDGE)
#define IRQT_FALLING (__IRQT_FALEDGE)
#define IRQT_BOTHEDGE (__IRQT_RISEDGE|__IRQT_FALEDGE)
#define IRQT_LOW (__IRQT_LOWLVL)
#define IRQT_HIGH (__IRQT_HIGHLVL)
#define IRQT_PROBE IRQ_TYPE_PROBE

static struct class *fasync_class;
static struct class_device *fasync_class_dev;

static DECLARE_WAIT_QUEUE_HEAD(button_wairq);

// 中断标记
static volatile int ev_press = 0;

static struct fasync_struct *button_async;

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;	
};

/* 键值: 按下时, 0x01, 0x02, 0x03, 0x04 */
/* 键值: 松开时, 0x81, 0x82, 0x83, 0x84 */
static unsigned char key_val;

struct pin_desc pins_desc[4] = 
{
	{S3C2410_GPF1, 0x01},
	{S3C2410_GPF4, 0x02},
	{S3C2410_GPF2, 0x03},
	{S3C2410_GPF0, 0x04},
};


static int fasync_drv_fasync(int fd, struct file *filp, int mode);


static irqreturn_t button_irq(int irq, void *dev_id)
{
	struct pin_desc *pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		// release
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		key_val = pindesc->key_val;
	}
	ev_press = 1;
	wake_up_interruptible(&button_wairq);

	kill_fasync(&button_async, SIGIO, POLL_IN);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int fasync_drv_open(struct inode *inode, struct file *filp)
{
	printk("fasync_drv_open\n");
	request_irq(IRQ_EINT1, button_irq, IRQT_BOTHEDGE, "K1", &pins_desc[0]);
	request_irq(IRQ_EINT4, button_irq, IRQT_BOTHEDGE, "K2", &pins_desc[1]);
	request_irq(IRQ_EINT2, button_irq, IRQT_BOTHEDGE, "K3", &pins_desc[2]);
	request_irq(IRQ_EINT0, button_irq, IRQT_BOTHEDGE, "K4", &pins_desc[3]);

	return 0;
}

ssize_t fasync_drv_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	if(size != 1)
	{
		return -EINVAL;
	}

	// 如果没有按键动作， 休眠
	wait_event_interruptible(button_wairq, ev_press);

	// 如果有按键动作，返回键值
	copy_to_user(buf, &key_val, 1);
	ev_press = 0;

	return 1;
}

int fasync_drv_close(struct inode *indoe, struct file *filp)
{
	printk("fasync_drv_close\n");
	free_irq(IRQ_EINT1, &pins_desc[0]);
	free_irq(IRQ_EINT4, &pins_desc[1]);
	free_irq(IRQ_EINT2, &pins_desc[2]);
	free_irq(IRQ_EINT0, &pins_desc[3]);

	fasync_drv_fasync(-1,filp, 0);

	return 0;
}

static unsigned fasync_drv_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(filp, &button_wairq, wait);

	printk("fasync_drv_poll\n");
	if(ev_press)
	{
		mask |= POLL_IN | POLLRDNORM;
	}

	return mask;
}

static int fasync_drv_fasync(int fd, struct file *filp, int mode)
{
	printk("drier: fasync_drv_fasync\n");
	return fasync_helper(fd, filp, mode, &button_async);
}

static struct file_operations fasync_drv_fops = 
{
	.owner = THIS_MODULE,
	.open = fasync_drv_open,
	.read = fasync_drv_read,
	.release = fasync_drv_close,
	.poll = fasync_drv_poll,
	.fasync = fasync_drv_fasync,
};

int major;
static int fasync_drv_init(void)
{
	major = register_chrdev(0, "fasync_drv", &fasync_drv_fops);
	fasync_class = class_create(THIS_MODULE, "fasync_drv");
	fasync_class_dev = device_create(fasync_class, NULL, MKDEV(major, 0), NULL, "fasync_drv");
	printk("fasync_drv_init\n");

	return 0;
}

static void fasync_drv_exit(void)
{
	printk("fasync_drv_exit\n");
	unregister_chrdev(major, "fasync_drv");
	device_unregister(fasync_class_dev);
	class_destroy(fasync_class);

	return 0;
}

module_init(fasync_drv_init);
modules_exit(fasync_drv_exit);

MODULE_LICENSE("GPL");
