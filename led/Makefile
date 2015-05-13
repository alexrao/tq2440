KERN_DIR = /mnt/workspace/tq2440/linux-2.6.30.4
PWD = $(shell pwd)
all:
	make -C $(KERN_DIR) M=$(PWD) modules

clean:
	make -C $(KERN_DIR) M=$(PWD) modules clean
	rm -rf modules.order

obj-m += drv_led.o
