#### Linux的I/O模型介绍以及同步异步阻塞非阻塞的区别（超级重要）

对于同步异步来说，是对应**两个或多个关系**的。

- 同步操作时，调用者需要等待被调用者返回结果，才会进行下一步操作
- 异步则相反，调用者不需要等待被调用者返回调用，即可进行下一步操作，被调用者通常依靠事件、回调等机制来通知调用者结果。

对于阻塞和非阻塞来说，是**当前进程的状态**。

- 阻塞就是当前进程被挂起，暂时放弃CPU，直到调用线程调用完毕，并返回结果和状态。
- 非阻塞是不能立刻得到结果之前，该调用者不会阻塞当前进程。而是直接返回状态。

https://blog.csdn.net/liyifan687/article/details/106749460

IO过程包括两个阶段：（1）内核从IO设备读写数据读取数据到内核空间的缓冲区；（2）内核把数据从内核空间的缓冲区复制回用户空间的缓冲区。

阻塞IO和非阻塞IO的区别在于第一步发起IO请求是否会被阻塞：如果阻塞直到完成那么就是传统的阻塞IO，如果不阻塞，那么就是非阻塞IO。

同步IO和异步IO的区别就在于第二个步骤是否阻塞：如果不阻塞，而是操作系统帮你做完IO操作再将结果返回给你，那么就是异步IO.

- 阻塞IO模型：应用进程从发起 IO 系统调用，至内核返回成功标识，这整个期间是处于阻塞状态的。

<img src="https://segmentfault.com/img/remote/1460000039898785" alt="阻塞式 IO 模型"  />

- 非阻塞IO模型：应用进程在发起 IO 系统调用后，会立刻返回，应用进程可以轮询发起 IO 系统调用，直到内核返回成功标识。非阻塞只是表示在检查数据有没有准备好的时候是非阻塞的，在数据到达并准备好的时候还是需要等待内核空间的数据复制到用户空间才能够返回。

![非阻塞式 IO 模型](https://segmentfault.com/img/remote/1460000039898786)

- I/O多路复用模型：同时监听多个描述符，一旦某个描述符IO就绪（读就绪或者写就绪），就能够通知进程进行相应的IO操作。

![IO 多路复用模型](https://segmentfault.com/img/remote/1460000039898787)

- 信号驱动I/O模型：通过调用sigaction注册信号函数，等内核数据准备好的时候系统中断当前程序，执行信号函数。

![信号驱动 IO 模型](https://segmentfault.com/img/remote/1460000039898788)

- 异步I/O模型：应用进程发起 IO 系统调用后，会立即返回。当内核中数据完全准备后，并且也复制到了用户空间，会产生一个信号来通知应用进程。

![异步 IO 模型](https://segmentfault.com/img/remote/1460000039898789)

####  IO复用的三种方法（select,poll,epoll）深入理解，包括三者区别，内部原理实现？

**select**

```c++
int select(
  int nfds, fd_set *readfds, fd_set *writefds,
  fd_set *exceptfds, struct timeval *timeout
);
```

select函数可以传入3个fd_set，分别对应了不同事件的file descriptor。返回值是一个int值，代表了就绪的fd数量，这个数量是3个fd_set就绪fd数量总和。

调用后select函数会阻塞，直到有描述符就绪（有数据 可读、可写、或者有except），或者超时（timeout指定等待时间，如果立即返回设为null即可），函数返回。当select函数返回后，可以通过遍历fdset，来找到就绪的描述符。

select把所有监听的文件描述符拷贝到内核中，挂起进程。当某个文件描述符可读或可写的时候，中断程序唤起进程，select将监听的文件描述符再次拷贝到用户空间，然后select遍历这些文件描述符找到IO可用的文件。下次监控的时候需要再次拷贝这些文件描述符到内核空间。select支持监听的描述符最大数量是1024。

**poll**

```c++
int poll (struct pollfd *fds, unsigned int nfds, int timeout);
struct pollfd {
    int fd; /* file descriptor */
    short events; /* requested events to watch */
    short revents; /* returned events witnessed */
};
```

poll使用链表保存文件描述符，其他的跟select没有什么不同。也还是需要遍历的方式查找已经就绪的文件描述符，并唤醒对应的进程。

**epoll**

```c++
int epoll_create(int size); //创建一个epoll的句柄，size用来告诉内核这个监听的数目一共有多大，参数size并不是限制了epoll所能监听的描述符最大个数，只是对内核初始分配内部数据结构的一个建议
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event); //对指定描述符fd执行op操作
int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout); //等待epfd上的io事件，最多返回maxevents个事件。
```

https://segmentfault.com/a/1190000003063859

epoll将文件描述符拷贝到内核空间后使用红黑树进行维护，同时向内核注册每个文件描述符的回调函数，当某个文件描述符可读可写的时候，将这个文件描述符加入到就绪链表里，并唤起进程，返回就绪链表到用户空间。

select缺点：1.单个进程能够监视的文件描述符的数量存在最大限制，通常是1024；2. 内核/用户空间的内存拷贝问题比较严重，每次调用都需要把fd集合从用户态拷贝到内核态；3. 每次调用select都需要遍历fd集合才能找到就绪的文件描述符。

poll缺点：除了select的第一个，其他都有。

#### EPOLL的介绍和了解

https://zhuanlan.zhihu.com/p/56486633 

https://www.jianshu.com/p/397449cadc9a

https://blog.csdn.net/davidsguo008/article/details/73556811


Epoll是Linux进行IO多路复用的一种方式，用于在一个线程里监听多个IO源，在IO源可用的时候返回并进行操作。它的特点是基于事件驱动，性能很高。

**epoll将文件描述符拷贝到内核空间后使用红黑树进行维护，同时向内核注册每个文件描述符的回调函数，当某个文件描述符可读可写的时候，将这个文件描述符加入到就绪链表里，并唤起进程，返回就绪链表到用户空间，由用户程序进行处理。**

Epoll有三个系统调用：epoll_create(),epoll_ctl()和epoll_wait()。

* eoll_create()函数在内核中初始化一个eventpoll对象，同时初始化红黑树和就绪链表。
* epoll_ctl()用来对监听的文件描述符进行管理。将文件描述符插入红黑树，或者从红黑树中删除，这个过程的时间复杂度是log(N)。同时向内核注册文件描述符的回调函数。
* epoll_wait()会将进程放到eventpoll的等待队列中，将进程阻塞，当某个文件描述符IO可用时，内核通过回调函数将该文件描述符放到就绪链表里，epoll_wait()会将就绪链表里的文件描述符返回到用户空间。

https://www.cnblogs.com/Anker/archive/2013/08/17/3263780.html

#### Epoll的ET模式和LT模式（ET的非阻塞）

* **ET模式**：当epoll_wait检测到描述符事件发生并将此事件通知应用程序，应用程序必须立即处理该事件。如果不处理，下次调用epoll_wait时，不会再次响应应用程序并通知此事件。
* ET是边缘触发模式，在这种模式下，只有当描述符从未就绪变成就绪时，内核才会通过epoll进行通知。然后直到下一次变成就绪之前，不会再次重复通知。也就是说，如果一次就绪通知之后不对这个描述符进行IO操作导致它变成未就绪，内核也不会再次发送就绪通知。优点就是只通知一次，减少内核资源浪费，效率高。缺点就是不能保证数据的完整，有些数据来不及读可能就会无法取出。
* **LT模式**：当epoll_wait检测到描述符事件发生并将此事件通知应用程序，应用程序可以不立即处理该事件。下次调用epoll_wait时，会再次响应应用程序并通知此事件。
* LT是水平触发模式，在这个模式下，如果文件描述符IO就绪，内核就会进行通知，如果不对它进行IO操作，只要还有未操作的数据，内核都会一直进行通知。优点就是可以确保数据可以完整输出。缺点就是由于内核会一直通知，会不停从内核空间切换到用户空间，资源浪费严重。

 DMA 技术？简单理解就是，**在进行 I/O 设备和内存的数据传输的时候，数据搬运的工作全部交给 DMA 控制器，而 CPU 不再参与任何与数据搬运相关的事情，这样 CPU 就可以去处理别的事务**。

`mmap()` 系统调用函数会直接把内核缓冲区里的数据「**映射**」到用户空间，这样，操作系统内核与用户空间就不需要再进行任何的数据拷贝操作。

#### 查询进程占用CPU的命令（注意要了解到used，buf，代表意义）

**top命令**

![image-20220306011546810](C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306011546810.png)

PID：进程ID
USER：进程的所有者
PR：进程的优先级
NI：nice值
VIRT：占用的虚拟内存
RES：占用的物理内存
SHR：使用的共享内存
S：进行状态 S：休眠 R运行 Z僵尸进程 N nice值为负
%CPU：占用的CPU
%MEM：占用内存
TIME+： 占用CPU的时间的累加值
COMMAND：启动命令
**uptime命令**

![image-20220306011858084](C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306011858084.png)

**w命令**

![image-20220306011930480](C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306011930480.png)

**vmstat**命令

![image-20220306012034162](C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306012034162.png)

#### 硬连接与连接

硬链接是通过索引节点进行的链接，以文件副本的形式存在。

软连接，其实就是新建立一个文件，这个文件就是专门用来指向别的文件的。

#### 文件权限

每一文件或目录的访问权限都有三组，每组用三位表示，分别为文件属主的读、写和执行权限；与属主同组的用户的读、写和执行权限；系统中其他用户的读、写和执行权限。

r ：读权限，用数字4表示；w ：写权限，用数字2表示；x ：执行权限，用数字1表示

u ：目录或者文件的当前的用户
g ：目录或者文件的当前的群组
o ：除了目录或者文件的当前用户或群组之外的用户或者群组
a ：所有的用户及群组

`chmod a+x 1.txt`

**pwd命令**：显示当前路径

**grep命令**：文本搜索

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306013704136.png" alt="image-20220306013704136" style="zoom: 67%;" />

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306013855491.png" alt="image-20220306013855491" style="zoom:67%;" />

**find命令**：查找和搜索文件

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306014001384.png" alt="image-20220306014001384" style="zoom:67%;" />

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306014038452.png" alt="image-20220306014038452" style="zoom:67%;" />

**ps命令**：显示进程状态

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306014626695.png" alt="image-20220306014626695" style="zoom:80%;" />

**kill命令**：终止进程

kill 命令杀死指定进程 PID，需要配合 ps 使用，而 killall 直接对进程对名字进行操作

**uname命令**：用于查看系统内核与系统版本等信息

![image-20220306014742583](C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306014742583.png)

**free命令**：显示当前系统中内存的使用量信息

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306014846494.png" alt="image-20220306014846494" style="zoom:80%;" />

**wc命令**：统计指定文本的行数、字数、字节数

<img src="C:\Users\bairong\AppData\Roaming\Typora\typora-user-images\image-20220306015019882.png" alt="image-20220306015019882" style="zoom:80%;" />

**touch命令**：用于创建空白文件或设置文件的时间。