#TQ2440平台驱动练习
kernel: linux-2.6.30.4  
硬件平台：TQ2440开发板  
编译工具：交叉编译器 EABI-4.3.3_EmbedSky(arm-linux-)	
编译方法：	
	module：直接采用对应文件目录下的Makefile(对应的kernel目录针对修改)， 然后直接使用make即可	
	test文件：格式如下：arm-linux-gcc test.c -o test.bin	

#目录历史	
----------------------
//2015.05.12	
1. 创建git服务	
2. 添加led的驱动和测试程序，并编写makefile文档	

----------------------
//2015.05.13	
1. 添加button输入检测驱动和测试程序，并编写相应的makefile文档	
2. button驱动中添加4个按键检测（目前使用轮询查询寄存器），并对应提供4个LED灯输出	
3. 测试程序中添加按键检测读取，并通过判断不同按键亮暗不同LED等作为肉眼观察。（Key1+Key2都亮，Key3+Key4全暗采用write机制，而其他的采用ioctl机制与驱动对接）	


