#TQ2440平台驱动练习	
kernel: linux-2.6.30.4  
硬件平台：TQ2440开发板  
编译工具：交叉编译器 EABI-4.3.3_EmbedSky(arm-linux-)	
编译方法：		
  module  ：直接采用对应文件目录下的Makefile(对应的kernel目录针对修改)， 然后直接使用make即可	
  test文件：格式如下：arm-linux-gcc test.c -o test.bin	

#目录历史	
2015.05.12	
1. 创建git服务	
2. 添加led的驱动和测试程序，并编写makefile文档	

2015.05.13	
1. 添加button输入检测驱动和测试程序，并编写相应的makefile文档		
2. button驱动中添加4个按键检测（目前使用轮询查询寄存器），并对应提供4个LED灯输出			
3. 测试程序中添加按键检测读取，并通过判断不同按键亮暗不同LED等作为肉眼观察。（Key1+Key2都亮，Key3+Key4全暗采用write机制，而其他的采用ioctl机制与驱动对接）	

2015.05.14	
1. 基于上一版本的驱动模块添加带有poll机制的处理方式：poll机制主要用于进程休眠
2. 附带一个poll机制网上的说明，详见readme.md

2015.05.18
1. 添加tq2440的驱动处理源码(从韦东山的资料中提取)

2015.08.18
1. 添加异步通知处理方式的数据读取方式

2015.05.20
1. 添加带打开和读写锁的驱动处理方式

2015.05.22
1. 添加两个定时驱动测试程序
位置：	./button/timer/button_timer.c 	在之前的基础上添加的测试程序
		./button/timer/test_timer.c 	简单定时器测试驱动
2015.05.29
1. 添加一个platform_device两个设备驱动之前传送数据的例子
2. 实现通过drv中获取dev中的相关信息
详见 ./resource/readme.md 介绍
