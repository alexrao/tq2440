KERNEL_DIR := /mnt/workspace/tq2440/linux-2.6.30.4
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules clean
	rm -rf modules.order

obj-m +=	led_drv.o \
			led_dev.o