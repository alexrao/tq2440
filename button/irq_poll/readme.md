 linux的poll机制
分类： Linux 驱动2013-10-07 21:14 521人阅读 评论(0) 收藏 举报
selectpoll
linux的poll机制


Poll就是监控文件是否可读的一种机制，作用与select一样。
应用程序的调用函数如下：
int poll(struct pollfd *fds,nfds_t nfds, int timeout);
Poll机制会判断fds中的文件是否可读，如果可读则会立即返回，返回的值就是可读fd的数量，如果不可读，那么就进程就会休眠timeout这么长的时间，然后再来判断是否有文件可读，如果有，返回fd的数量，如果没有，则返回0. 


内核实现流程：
当应用程序调用poll函数的时候，会调用到系统调用sys_poll函数，该函数最终调用do_poll函数，do_poll函数中有一个死循
 环，
在里面又会利用do_pollfd函数去调用驱动中的poll函数（fds中每个成员的字符驱动程序都会被扫描到），驱动程序中的Poll函数的工作
 有两个，
一个就是调用poll_wait 函数，把进程挂到等待队列中去（这个是必须的，你要睡眠，必须要在一个等待队列上面，否则到哪里去唤醒你呢？？），
另一个是确定相关的fd是否有内容可 读，如果可读，就返回1，否则返回0，如果返回1 ，do_poll函数中的count++，
    
然后  do_poll函数然后判断三个条件（if (count ||!timeout || signal_pending(current))）如果成立就直接跳出，如果不成立，
就睡眠timeout个jiffes这么长的时间（调用schedule_timeout实现睡眠），如果在这段时间内没有其他进程去唤醒它，
那么第二次执行判断的时候就会跳出死循环。如果在这段时间内有其他进程唤醒它，那么也可以跳出死循环返回
（例如我们可以利用中断处理函数去唤醒它，这样的话一有数据可读，就可以让它立即返回）。


1.      sys_poll函数位于fs/select.c文件中，代码如下：
asmlinkagelong sys_poll(struct pollfd __user *ufds, unsigned int nfds, long timeout_msecs)
{        
 s64 timeout_jiffies;
         if (timeout_msecs > 0) {
#ifHZ > 1000
             /* We can only overflow if HZ >1000 */
             if (timeout_msecs / 1000 >(s64)0x7fffffffffffffffULL / (s64)HZ)
                 timeout_jiffies = -1;
             else
#endif
                 timeout_jiffies =msecs_to_jiffies(timeout_msecs);
         } 
else
 {
             /* Infinite (< 0) or no (0)timeout */
             timeout_jiffies = timeout_msecs;
         } 
         return do_sys_poll(ufds,nfds, &timeout_jiffies);
}
它对超时参数稍作处理后，直接调用do_sys_poll。 
2.      do_sys_poll函数也位于位于fs/select.c文件中，我们忽略其他代码：
intdo_sys_poll(struct pollfd __user *ufds, unsigned int nfds, s64 *timeout)
{
……
poll_initwait(&table);
……
         fdcount = do_poll(nfds, head,&table, timeout);
……
} 
poll_initwait函数非常简单，它初始化一个poll_wqueues变量table：
poll_initwait> init_poll_funcptr(&pwq->pt, __pollwait); > pt->qproc = qproc;
即table->pt->qproc= __pollwait，__pollwait将在驱动的poll函数里用到。 
3.      do_sys_poll函数位于fs/select.c文件中，代码如下：
 
static int do_poll(unsigned int nfds,  struct poll_list *list,   struct poll_wqueues *wait, s64 *timeout)
{
01 ……
02   for (;;){
03 ……
04                   if(do_pollfd(pfd, pt)) {
05                           count++;
06                           pt = NULL;
07                   }
08 ……
09       if(count || !*timeout || signal_pending(current))
10           break;
11       count= wait->error;
12       if(count)
13           break;14
15       if(*timeout < 0) {
16           /*Wait indefinitely */
17           __timeout= MAX_SCHEDULE_TIMEOUT;
18       }else if (unlikely(*timeout >= (s64)MAX_SCHEDULE_TIMEOUT-1)) {
19           /*
20           * Wait for longer than MAX_SCHEDULE_TIMEOUT. Do it in
21           * a loop
22           */
23           __timeout= MAX_SCHEDULE_TIMEOUT - 1;
24           *timeout-= __timeout;
25       }else {
26           __timeout= *timeout;
27           *timeout= 0;
28       }29
30       __timeout= schedule_timeout(__timeout); // 休眠时间由应用提供
31       if(*timeout >= 0)
32           *timeout+= __timeout;
33   }
34   __set_current_state(TASK_RUNNING);
35   returncount;
36 } 
分析其中的代码，可以发现，它的作用如下：
①    从02行可以知道，这是个循环，它退出的条件为：
a.      09行的3个条件之一(count非0，超时、有信号等待处理)
count顺0表示04行的do_pollfd至少有一个成功。
b.      11、12行：发生错误
②    重点在do_pollfd函数，后面再分析
③    第30行，让本进程休眠一段时间，注意：应用程序执行poll调用后，如果①②的条件不满足，进程就会进入休眠。那么，谁唤醒呢？除了休眠到指定时间被系统唤醒外，还可以被驱动程序唤醒──记住这点，这就是为什么驱动的poll里要调用poll_wait的原因，后面分析。 
4.      do_pollfd函数位于fs/select.c文件中，代码如下：
static inline unsigned int do_pollfd(struct pollfd*pollfd, poll_table *pwait)
{
……
             if(file->f_op && file->f_op->poll)
                     mask= file->f_op->poll(file, pwait);
……
} 
可见，它就是调用我们的驱动程序里注册的poll函数。 
二、驱动程序：
驱动程序里与poll相关的地方有两处：一是构造file_operation结构时，要定义自己的poll函数。二是通过poll_wait来调用上面说到的__pollwait函数，pollwait的代码如下：
staticinline void poll_wait(struct file * filp, wait_queue_head_t * wait_address,poll_table *p)
{
         if (p && wait_address)
             p->qproc(filp, wait_address, p);
}
p->qproc就是__pollwait函数，从它的代码可知，它只是把当前进程挂入我们驱动程序里定义的一个队列里而已。它的代码如下：
staticvoid __pollwait(struct file *filp, wait_queue_head_t *wait_address,  poll_table *p)
{
         struct poll_table_entry *entry =poll_get_entry(p);
         if (!entry)
             return;
         get_file(filp);
         entry->filp = filp;
         entry->wait_address = wait_address;
         init_waitqueue_entry(&entry->wait,current);
         add_wait_queue(wait_address,&entry->wait);
} 
执行到驱动程序的poll_wait函数时，进程并没有休眠，我们的驱动程序里实现的poll函数是不会引起休眠的。让进程进入休眠，是前面分析的do_sys_poll函数的30行“__timeout = schedule_timeout(__timeout)”。
poll_wait只是把本进程挂入某个队列，应用程序调用poll > sys_poll> do_sys_poll > poll_initwait，do_poll > do_pollfd > 我们自己写的poll函数后，再调用schedule_timeout进入休眠。如果我们的驱动程序发现情况就绪，可以把这个队列上挂着的进程唤醒。可见，poll_wait的作用，只是为了让驱动程序能找到要唤醒的进程。即使不用poll_wait，我们的程序也有机会被唤醒：chedule_timeout(__timeout)，只是休眠__time_out这段时间。
 
现在来总结一下poll机制：
1. poll > sys_poll > do_sys_poll >poll_initwait，poll_initwait函数注册一下回调函数__pollwait，它就是我们的驱动程序执行poll_wait时，真正被调用的函数。 
2. 接下来执行file->f_op->poll，即我们驱动程序里自己实现的poll函数
   它会调用poll_wait把自己挂入某个队列，这个队列也是我们的驱动自己定义的；
   它还判断一下设备是否就绪。 
3. 如果设备未就绪，do_sys_poll里会让进程休眠一定时间，这个时间是应用提供的“超时时间” 
4. 进程被唤醒的条件有2：一是上面说的“一定时间”到了，二是被驱动程序唤醒。驱动程序发现条件就绪时，就把“某个队列”上挂着的进程唤醒，这个队列，就是前面通过poll_wait把本进程挂过去的队列。 
5. 如果驱动程序没有去唤醒进程，那么chedule_timeout(__timeou)超时后，会重复2、3动作1次，直到应用程序给定的时间， 然后返回。

驱动的poll函数编写模板如下：
static DECLARE_WAIT_QUEUE_HEAD(my_waitq);  //休眠要挂的等待队列
static unsigned drv_poll(struct file *file, poll_table *wait)
{
unsigned int mask = 0;
poll_wait(file, &my_waitq, wait); // 不会立即休眠
if (有数据)
mask |= POLLIN | POLLRDNORM;
return mask;
}
