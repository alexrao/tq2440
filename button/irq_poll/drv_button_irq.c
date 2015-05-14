#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/regs-gpio.h>
#include <mach/hardware.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/poll.h>

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

static struct class *button_irq_class;
static struct class_device *button_irq_class_dev;

volatile unsigned long *gpbdat = NULL;
volatile unsigned long *gpbcon = NULL;

// LED灯点灯程序
int gpio_led_init(void)
{
	printk("Gpio Leds init");

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

static DECLARE_WAIT_QUEUE_HEAD(button_waitq);

// 中断时间标记，中断服务程序将它置1，read的时候将其清0
static volatile int ev_press = 0;

struct pin_desc
{
	unsigned int pin;
	unsigned int key_val;
};

// 键值
// 按下时：0x01, 0x02, 0x03, 0x04
// 松开时：0x81, 0x82, 0x83, 0x84
static unsigned char key_val;

// K1/K2/K3/K4对应GPF1/GPF4/GPF2/GPF0
struct pin_desc pins_desc[4] = 
{
	{S3C2410_GPF1, 0x01},
	{S3C2410_GPF4, 0x02},
	{S3C2410_GPF2, 0x03},
	{S3C2410_GPF0, 0x04},
};

// 中断处理程序，确定按键值
static irqreturn_t button_irq(int irq, void *dev_id)
{
	struct pin_desc *pindesc = (struct pin_desc *)dev_id;
	unsigned int pinval;

	pinval = s3c2410_gpio_getpin(pindesc->pin);

	if(pinval)
	{
		// realease
		key_val = 0x80 | pindesc->key_val;
	}
	else
	{
		// press
		key_val = pindesc->key_val;
	}

	ev_press = 1; // 表示发生了中断
	wake_up_interruptible(&button_waitq); // 唤醒休眠的进程

	return IRQ_RETVAL(IRQ_HANDLED);
}

static int button_irq_open(struct inode *inode, struct file *file)
{
	// GPF1/GPF4/GPF2/GPF0为中断引脚
	request_irq(IRQ_EINT1, button_irq, IRQT_BOTHEDGE, "K1", &pins_desc[0]);
	request_irq(IRQ_EINT4, button_irq, IRQT_BOTHEDGE, "K2", &pins_desc[1]);
	request_irq(IRQ_EINT2, button_irq, IRQT_BOTHEDGE, "K3", &pins_desc[2]);
	request_irq(IRQ_EINT0, button_irq, IRQT_BOTHEDGE, "K4", &pins_desc[3]);
	printk("irq init ok,now init led\n");
	gpio_led_init();// 初始化LEDs相关参数
	printk("init led ok\n");

	return 0;
}

// 按键数据读取
ssize_t button_irq_read(struct file *file, char __user *buf, ssize_t size, loff_t *ppos)
{
	if(size != 1)
	{
		printk("size is not 1:size=%d",size);
		return -EINVAL;
	}

	// 如果没有按键动作，休眠
	wait_event_interruptible(button_waitq, ev_press);

	// 如果有按键动作，返回键值
	copy_to_user(buf, &key_val, 1);
	ev_press = 0;

	return 1;
}

// ioctl 控制相关函数
static int button_irq_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	if(arg > 4)
	{
		printk("arg > 4\n");
		return -1;
	}	

	gpio_led_ioctl(cmd);
	return 0;
}

// poll 机制处理函数
static unsigned button_irq_poll(struct file *filp, poll_table *wait)
{
	unsigned int mask = 0;
	printk("poll wait\n");
	poll_wait(filp, &button_waitq, wait); // 不会立即休眠
	printk("poll continue\n");
	if(ev_press)
	{
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

// 关闭释放函数
int button_irq_close(struct inode *inode, struct file *file)
{
	free_irq(IRQ_EINT1, &pins_desc[0]);
	free_irq(IRQ_EINT4, &pins_desc[1]);
	free_irq(IRQ_EINT2, &pins_desc[2]);
	free_irq(IRQ_EINT0, &pins_desc[3]);

	return 0;
}

// fops结构体
static struct file_operations button_irq_fops = 
{
    .owner   =  THIS_MODULE,    /* 这是一个宏，推向编译模块时自动创建的__this_module变量 */
	.open = button_irq_open,
	.read = button_irq_read,
	.ioctl = button_irq_ioctl,
	.release = button_irq_close,
	.poll = button_irq_poll,
};

int major;
static int button_irq_init(void)
{
	printk("button irq init\n");
	major = register_chrdev(0, "button_irq", &button_irq_fops);
	button_irq_class = class_create(THIS_MODULE, "button_irq");
	button_irq_class_dev = device_create(button_irq_class, NULL, MKDEV(major, 0), NULL, "button_irq");
	gpbcon = (volatile unsigned long *) ioremap(0x56000010, 16);
	gpbdat = gpbcon+1;

	return 0;
}

static void button_irq_exit(void)
{
	unregister_chrdev(major, "button_irq");
	device_unregister(button_irq_class_dev);
	class_destroy(button_irq_class);
	printk("button irq exit\n");
}

module_init(button_irq_init);
module_exit(button_irq_exit);
MODULE_LICENSE("GPL");
