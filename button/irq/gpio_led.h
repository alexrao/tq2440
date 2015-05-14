#ifndef __GPIO_LED_H__
#define __GPIO_LED_H__

extern int gpio_led_init(void);
extern void gpio_led_ioctl(unsigned long cmd);

#endif