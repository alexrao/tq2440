// platform_device 的使用
1. 程序通过[led_dev.ko]注册led相关的resource信息，然后在[led_drv.ko]中通过获取对应resource信息来获得所需ioremap的地址和数据。
2. 两边通过platform_driver 和 platform_device 所对应的数据名称来链接（就我看来，具体再查资料）

#附上一份相关函数的解析说明

platform_get_resource函数源码如下：
struct resource *platform_get_resource(struct platform_device *dev,
                                   unsigned int type, unsigned int num)
{
       int i;
 
       for (i = 0; i < dev->num_resources; i++) {
              struct resource *r = &dev->resource[i];
 
              if (type == resource_type(r) && num-- == 0)
                     return r;
       }
       return NULL;
}
函数分析：
struct resource *r = &dev->resource[i];
这行代码使得不管你是想获取哪一份资源都从第一份资源开始搜索。
if (type == resource_type(r) && num-- == 0)
这行代码首先通过type == resource_type(r)判断当前这份资源的类型是否匹配，如果匹配则再通过num-- == 0判断是否是你要的，如果不匹配重新提取下一份资源而不会执行num-- == 0这一句代码。
通过以上两步就能定位到你要找的资源了，接着把资源返回即可。如果都不匹配就返回NULL。
实例分析：
下面通过一个例子来看看它是如何拿到设备资源的。
设备资源如下：
static struct resource s3c_buttons_resource[] = {
       [0]={
              .start = S3C24XX_PA_GPIO,
              .end   = S3C24XX_PA_GPIO + S3C24XX_SZ_GPIO - 1,
              .flags = IORESOURCE_MEM,
       },
       [1]={
              .start = IRQ_EINT8,
              .end   = IRQ_EINT8,
              .flags = IORESOURCE_IRQ,
       },
       [2]={
              .start = IRQ_EINT11,
              .end   = IRQ_EINT11,
              .flags = IORESOURCE_IRQ,
       },
       [3]={
              .start = IRQ_EINT13,
              .end   = IRQ_EINT13,
              .flags = IORESOURCE_IRQ,
       },
       [4]={
              .start = IRQ_EINT14,
              .end   = IRQ_EINT14,
              .flags = IORESOURCE_IRQ,
       },
       [5]={
              .start = IRQ_EINT15,
              .end   = IRQ_EINT15,
              .flags = IORESOURCE_IRQ,
       },
       [6]={
              .start = IRQ_EINT19,
              .end   = IRQ_EINT19,
              .flags = IORESOURCE_IRQ,
       }
};
驱动中通过下面代码拿到第一份资源：
struct resource *res;
res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
函数进入for里面，i=0，num_resources=7，拿出resource[0]资源。resource_type(r)提取出该份资源 的资源类型并与函数传递下来的资源类型进行比较，匹配。Num=0(这里先判断是否等于0再自减1)符合要求，从而返回该资源。
获取剩下资源的代码如下：
for(i=0; i<6; i++){
              buttons_irq = platform_get_resource(pdev,IORESOURCE_IRQ,i);
             if(buttons_irq == NULL){
                  dev_err(dev,"no irq resource specified\n");
                   ret = -ENOENT;
                   goto err_map;
              }
              button_irqs[i] = buttons_irq->start; 
}
分析如下：
For第一次循环：
buttons_irq = platform_get_resource(pdev,IORESOURCE_IRQ,0);
在拿出第一份资源进行resource_type(r)判断资源类型时不符合(此时num-- == 0这句没有执行)，进而拿出第二份资源，此时i=1，num_resources=7，num传递下来为0，资源类型判断时候匹配，num也等于0，从而确定资源并返回。
For第二次循环：
buttons_irq = platform_get_resource(pdev,IORESOURCE_IRQ,1);
拿出第二份资源的时候resource_type(r)资源类型匹配，但是num传递下来时候为1，执行num-- == 0时不符合(但num开始自减1，这导致拿出第三份资源时num==0)，只好拿出第三份资源。剩下的以此类推。
 
总结：
struct resource *platform_get_resource(struct platform_device *dev,
                                   unsigned int type, unsigned int num)
unsigned int type决定资源的类型，unsigned int num决定type类型的第几份资源（从0开始）。即使同类型资源在资源数组中不是连续排放也可以定位得到该资源。
比如第一份IORESOURCE_IRQ类型资源在resource[2]，而第二份在resource[5]，那
platform_get_resource(pdev,IORESOURCE_IRQ,0);
可以定位第一份IORESOURCE_IRQ资源；
platform_get_resource(pdev,IORESOURCE_IRQ,1);
可以定位第二份IORESOURCE_IRQ资源。
       之所以能定位到资源，在于函数实现中的这一行代码：
if (type == resource_type(r) && num-- == 0)
该行代码，如果没有匹配资源类型，num-- == 0不会执行而重新提取下一份资源，只有资源匹配了才会寻找该类型的第几份资源，即使这些资源排放不连续。
