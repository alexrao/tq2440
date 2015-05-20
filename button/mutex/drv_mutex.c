#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>


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

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

// 中断时间标记，中断中将其置位，数据读的时候将其清除
static volatile bool ev_press = false;

static struct fasync_struct *button_async;

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

// 按下：0x01/0x02/0x03/0x04
// 松开：0x81/0x82/0x83/0x84
static unsigned char key_val;

#define DEVICE_NAME	"mutex_drv"

// K1/K2/K3/K4对应GPF1、GPF4、GPF2、GPF0
struct pin_desc pins_desc[4] =
{
	{S3C2410_GPF1, 0x01},
	{S3C2410_GPF4, 0x02},
	{S3C2410_GPF2, 0x03},
	{S3C2410_GPF0, 0x04},
};

static DECLARE_MUTEX(button_lock);	// 定义互斥锁


// 中断处理程序
static irqreturn_t buttons_irq(int irq, void *dev_id)
{
	struct pin_desc *pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		// Release
		key_val = 0x80|pindesc->key_val;
	}
	else
	{
		// press
		key_val = pindesc->key_val;
	}

	ev_press = true; // 表示中断发生了

	wake_up_interruptible(&button_waitq); //唤醒中断 (poll和其他等待会被唤醒)

	kill_fasync(&button_async, SIGIO, POLL_IN);

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int mutex_drv_open(struct inode *inode, struct file *filp)
{
	printk("driver: mutex_drv_open\n");
	if(filp->f_flags & O_NONBLOCK) // 非阻塞
	{
		if(down_trylock(&button_lock))
			return -EBUSY;
	}
	else	// 阻塞
	{
		// 获取信号量，如果没有获取到则随眠等待获取
		down(&button_lock);
	}

	// GPF1、GPF4、GPF2、GPF0为中断引脚
	request_irq(IRQ_EINT1, buttons_irq, IRQT_BOTHEDGE, "K1", &pins_desc[0]);
	request_irq(IRQ_EINT4, buttons_irq, IRQT_BOTHEDGE, "K2", &pins_desc[1]);
	request_irq(IRQ_EINT2, buttons_irq, IRQT_BOTHEDGE, "K3", &pins_desc[2]);
	request_irq(IRQ_EINT0, buttons_irq, IRQT_BOTHEDGE, "K4", &pins_desc[3]);

	return 0;
}

ssize_t mutex_drv_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	printk("driver: mutex_drv_read\n");
	if(size != 1)
	{
		printk("mutex_drv_read size != 1\n");
		return -EINVAL;
	}

	if(filp->f_flags & O_NONBLOCK)
	{
		if( !ev_press)
		{
			printk("mutex_drv_read O_NONBLOCK\n");
			return -EAGAIN;
		}
		else
		{
			printk("mutex_drv_read ev_press == ture\n");
		}
	}
	else
	{
		// 如果没有按键则随眠等待
		printk("wait_event_interruptible\n");
		wait_event_interruptible(button_waitq, ev_press);
		printk("exceed wait_event_interruptible\n");
}

	// 如果没有按键动作，则返回键值
	copy_to_user(buf, &key_val, 1);
	ev_press = false;

	return 0;
}

int mutex_drv_close(struct inode *inode, struct file *filp)
{
	printk("driver: mutex_drv_close\n");
	free_irq(IRQ_EINT1, &pins_desc[0]);
	free_irq(IRQ_EINT4, &pins_desc[1]);
	free_irq(IRQ_EINT2, &pins_desc[2]);
	free_irq(IRQ_EINT0, &pins_desc[3]);

	up(&button_lock);

	return 0;
}

static unsigned mutex_drv_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	poll_wait(filp, &button_waitq, wait);	// 不会立即休眠

	printk("driver: mutex_drv_poll\n");
	if(ev_press)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static int mutex_drv_fasync(int fd, struct file *filp, int on)
{
	printk("driver: mutex_drv_fasync\n");
	return fasync_helper(fd, filp, on, &button_async);
}

static struct file_operations mutex_drv_fops = 
{
	.owner = THIS_MODULE,
	.read = mutex_drv_read,
	.open = mutex_drv_open,
	.release = mutex_drv_close,
	.poll = mutex_drv_poll,
	.fasync = mutex_drv_fasync, 
};


static struct miscdevice misc = {

       .minor = MISC_DYNAMIC_MINOR,

       .name = DEVICE_NAME,

       .fops = &mutex_drv_fops,

};

int major;

static int __init mutex_drv_init(void)
{
	int ret;
	ret = misc_register(&misc);
	printk("mutex_drv_init with misc \n");

	return 0;
}

static void __exit mutex_drv_exit(void)
{
	misc_deregister(&misc);
}

module_init(mutex_drv_init);
module_exit(mutex_drv_exit);

MODULE_LICENSE("GPL");
