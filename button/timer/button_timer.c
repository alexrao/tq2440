#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/mutex.h>

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

static struct class *timer_class;
static struct class_device *timer_class_device;

static struct timer_list buttons_timer;

static DECLARE_WAIT_QUEUE_HEAD(button_waitq); // 等待队列

static volatile int ev_press = 0;
static struct fasync_struct *button_async; //异步通知用

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

static unsigned char key_val;


struct pin_desc pins_desc[4] =
{
	{S3C2410_GPF1, 0x01},
	{S3C2410_GPF4, 0x02},
	{S3C2410_GPF2, 0x03},
	{S3C2410_GPF0, 0x04},
};

static struct pin_desc *irq_pd;

static DECLARE_MUTEX(button_lock);

static irqreturn_t button_irq(int irq, void *dev_id)
{
	irq_pd = (struct pin_desc *)dev_id;
	mod_timer(&buttons_timer, jiffies+HZ/100);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int timer_drv_open(struct inode *inode, struct file *filp)
{
	if(filp->f_flags & O_NONBLOCK)
	{
		if(down_trylock(&button_lock))
			return -EBUSY;
	}
	else
	{
		down(&button_lock);
	}

	request_irq(IRQ_EINT1, button_irq, IRQT_BOTHEDGE, "K1", &pins_desc[0]);
	request_irq(IRQ_EINT4, button_irq, IRQT_BOTHEDGE, "K2", &pins_desc[1]);
	request_irq(IRQ_EINT2, button_irq, IRQT_BOTHEDGE, "K3", &pins_desc[2]);
	request_irq(IRQ_EINT0, button_irq, IRQT_BOTHEDGE, "K4", &pins_desc[3]);

	return 0;
}

ssize_t timer_drv_read(struct file *filp, char __user *buf, ssize_t size, loff_t *ppos)
{
	if(size != 1)
		return -EINVAL;

	if(filp->f_flags & O_NONBLOCK)
	{
		if(!ev_press)
			return -EINVAL;
	}
	else
	{
		wait_event_interruptible(button_waitq, ev_press);
	}

	copy_to_user(buf, &key_val, 1);
	ev_press = 0;

	return 1;
}

int timer_drv_close(struct inode *inode, struct file *filp)
{
	free_irq(IRQ_EINT1, &pins_desc[0]);
	free_irq(IRQ_EINT4, &pins_desc[1]);
	free_irq(IRQ_EINT2, &pins_desc[2]);
	free_irq(IRQ_EINT0, &pins_desc[3]);
	up(&button_lock);
	return 1;
}

int timer_drv_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;

	printk("timer_drv_poll\n");
	poll_wait(filp, &button_waitq, wait);
	printk("timer_drv_poll continue\n");

	if(ev_press)
		mask |= POLLIN | POLLRDNORM;

	return mask;
}

static int timer_drv_fasync(int fd, struct file *filp, int mode)
{
	printk("timer_drv_fasync\n");
	return fasync_helper(fd, filp, mode, &button_async);
}

static struct file_operations timer_drv_fops = 
{
	.owner = THIS_MODULE,
	.open = timer_drv_open,
	.read = timer_drv_read,
	.release = timer_drv_close,
	.poll = timer_drv_poll,
	.fasync = timer_drv_fasync,
};

int major;
static void button_timer_function(unsigned long data)
{
	struct pin_desc *pindesc = irq_pd;
	unsigned int pinval;
	printk("button_timer_function\n");

	if(!pindesc)
		return;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		key_val = 0x80|pindesc->key_val;
	}
	else
	{
		key_val = pindesc->key_val;
	}

	ev_press = 1;
	wake_up_interruptible(&button_waitq);

	kill_fasync(&button_async, SIGIO, POLL_IN);

}

static int timer_drv_init(void)
{
	printk("timer_drv_init\n");
	init_timer(&buttons_timer);
	buttons_timer.function = button_timer_function;

	add_timer(&buttons_timer);

	major = register_chrdev(0, "timebutton", &timer_drv_fops);
	timer_class = class_create(THIS_MODULE, "timebutton");
	timer_class_device = device_create(timer_class, NULL, MKDEV(major, 0), NULL, "timebutton");

	return 0;
}

static void timer_drv_exit(void)
{
	printk("timer_drv_exit\n");
	unregister_chrdev(major, "timebutton");
	device_unregister(timer_class_device);
	class_destroy(timer_class);

	return 0;
}



module_init(timer_drv_init);
module_exit(timer_drv_exit);

MODULE_LICENSE("GPL");
