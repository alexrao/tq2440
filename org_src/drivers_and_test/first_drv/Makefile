KERN_DIR = /mnt/workspace/tq2440/linux-2.6.30.4

all:
	make -C $(KERN_DIR) M=`pwd` modules 

clean:
	make -C $(KERN_DIR) M=`pwd` modules clean
	rm -rf modules.order

obj-m	+= first_drv.o
