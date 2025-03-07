#### 1\. inotify主要功能

Inotify是一种文件变化通知机制，Linux内核从2.6.13开始引入。它是一个内核用于通知用户空间程序文件系统变化的机制。开源社区提出用户态需要内核提供一些机制，以便用户态能够及时地得知内核或底层硬件设备发生了什么，从而能够更好地管理设备，给用户提供更好的服务，如 hotplug、udev 和 inotify 就是这种需求催生的。Hotplug 是一种内核向用户态应用通报关于热插拔设备一些事件发生的机制，桌面系统能够利用它对设备进行有效的管理，udev 动态地维护 /dev 下的设备文件，inotify 是一种文件系统的变化通知机制，如文件增加、删除等事件可以立刻让用户态得知，该机制是著名的桌面搜索引擎项目 beagle 引入的，并在 Gamin 等项目中被应用。  
事实上，在 inotify 之前已经存在一种类似的机制叫 dnotify，但是它存在许多缺陷：  
1) 对于想监视的每一个目录，用户都需要打开一个文件描述符，因此如果需要监视的目录较多，将导致打开许多文件描述符，特别是，如果被监视目录在移动介质上（如光盘和 USB 盘），将导致无法 umount 这些文件系统，因为使用 dnotify 的应用打开的文件描述符在使用该文件系统。  
2) dnotify 是基于目录的，它只能得到目录变化事件，当然在目录内的文件的变化会影响到其所在目录从而引发目录变化事件，但是要想通过目录事件来得知哪个文件变化，需要缓存许多 stat 结构的数据。  
3) Dnotify 的接口非常不友好，它使用 signal。  
Inotify 是为替代 dnotify 而设计的，它克服了 dnotify 的缺陷，提供了更好用的，简洁而强大的文件变化通知机制：  
1) Inotify 不需要对被监视的目标打开文件描述符，而且如果被监视目标在可移动介质上，那么在 umount 该介质上的文件系统后，被监视目标对应的 watch 将被自动删除，并且会产生一个 umount 事件。  
2) Inotify 既可以监视文件，也可以监视目录。  
3) Inotify 使用系统调用而非 SIGIO 来通知文件系统事件。  
4) Inotify 使用文件描述符作为接口，因而可以使用通常的文件 I/O 操作select 和 poll 来监视文件系统的变化。  
Inotify 可以监视的文件系统事件包括：  
IN\_ACCESS，即文件被访问  
IN\_MODIFY，文件被 write  
IN\_ATTRIB，文件属性被修改，如 chmod、chown、touch 等  
IN\_CLOSE\_WRITE，可写文件被 close  
IN\_CLOSE\_NOWRITE，不可写文件被 close  
IN\_OPEN，文件被 open  
IN\_MOVED\_FROM，文件被移走,如 mv  
IN\_MOVED\_TO，文件被移来，如 mv、cp  
IN\_CREATE，创建新文件  
IN\_DELETE，文件被删除，如 rm  
IN\_DELETE\_SELF，自删除，即一个可执行文件在执行时删除自己  
IN\_MOVE\_SELF，自移动，即一个可执行文件在执行时移动自己  
IN\_UNMOUNT，宿主文件系统被 umount  
IN\_CLOSE，文件被关闭，等同于(IN\_CLOSE\_WRITE | IN\_CLOSE\_NOWRITE)

IN\_MOVE，文件被移动，等同于(IN\_MOVED\_FROM | IN\_MOVED\_TO)

#### 2\. 用户接口

在用户态，inotify 通过三个系统调用和在返回的文件描述符上的文件 I/O 操作来使用，使用 inotify 的第一步是创建 inotify 实例：

```
int fd = inotify_init ();
```

每一个inotify 实例对应一个独立的排序的队列。文件系统的变化事件使用Watch对象来描述，每一个Watch是一个二元组（目标，事件掩码），目标可以是文件或目录，事件掩码表示应用希望关注的inotify 事件，每一个位对应一个 inotify 事件。Watch通过文件或目录的路径名来添加。目录Watch将返回在该目录下的所有文件上面发生的事件。添加一个 watch：

```
int wd = inotify_add_watch (fd, path, mask);
```

fd 是 inotify\_init() 返回的文件描述符，path是被监视的目标的路径名（即文件名或目录名），mask是事件掩码, 在头文件 linux/inotify.h 中定义了每一位代表的事件。可以使用同样的方式来修改事件掩码，即改变希望被通知的inotify 事件。Wd是watch描述符句柄。删除一个watch：

```
int ret = inotify_rm_watch (fd, wd);
```

fd是inotify\_init()返回的文件描述符句柄，wd是 inotify\_add\_watch()返回的watch描述符句柄。Ret是函数的返回值。文件事件用一个 inotify\_event结构表示，它通过读取inotify\_init()返回的文件描述符句柄来获得。

```c
struct inotify_event {
    __s32 wd;
    __u32 mask;
    __u32 cookie;
    __u32 len;
    char name[0];
};
```

通过read调用可以一次获得多个事件，只要提供的buf足够大：  
size\_t len = read (fd, buf, BUF\_LEN);  
buf 是一个 inotify\_event 结构的数组指针，BUF\_LEN 指定要读取的总长度，buf 大小至少要不小于BUF\_LEN，该调用返回的事件数取决于 BUF\_LEN 以及事件中文件名的长度。Len 为实际读去的字节数，即获得的事件的总长度。可以在函数inotify\_init()返回的文件描述符fd 上使用 select() 或poll(), 也可以在fd上使用ioctl命令FIONREAD来得到当前队列的长度。close(fd)将删除所有添加到fd中的watch并做必要的清理。  
inotify使用：  
通过inotify\_init创建一个文件描述符，然后使用inotify\_add\_watch附加一个或多个监视器(一个监视器是一个路径和一组事件)，接着使用 read()方法从描述符获取事件信息，read()函数在事件发生之前是被阻塞的。  

#### 3\. 内核实现原理

在内核中，每一个inotify实例对应一个 inotify\_device 结构：

```c
struct inotify_device {
    wait_queue_head_t wq;
    struct idr idr;
    struct semaphore sem;
    struct list_head events;
    struct list_head watches;
    atomic_t count;
    struct user_struct * user;
    unsigned int queue_size;
    unsigned int event_count;
    unsigned int max_events;
    u32 last_wd;
};
```

结构 inotify\_device在用户态调用inotify\_init()时创建，当关闭 inotify\_init()返回的文件描述符时将被释放。

```c
struct inotify_watch {
    struct list_head d_list;
    struct list_head i_list;
    atomic_t count;
    struct inotify_device * dev;
    struct inode * inode;
    s32 wd;
    u32 mask;
};
```

结构inotify\_watch在用户态调用inotify\_add\_watch()时创建，在用户态调用inotify\_rm\_watch()或close(fd)时被释放。无论是目录还是文件，在内核中都对应一个inode结构，inotify 系统在inode结构中增加了两个字段：

```
#ifdef CONFIG_INOTIFYstruct list_headinotify_watches; struct semaphoreinotify_sem;#endif
```

inotify\_watches指向被监视目标上的watch列表，每当用户调用inotify\_add\_watch()时，内核就为添加的watch创建一个inotify\_watch结构，并把它插入到被监视目标对应的inode的inotify\_watches列表。inotify\_sem用于同步对inotify\_watches列表的访问。当文件系统发生事件时，相应的文件系统将显示调用fsnotify\_\* 来把相应的事件报告给 inotify 系统，其中\*号就是相应的事件名，目前实现包括：  
fsnotify\_move，文件从一个目录移动到另一个目录  
fsnotify\_nameremove，文件从目录中删除  
fsnotify\_inoderemove，自删除  
fsnotify\_create，创建新文件  
fsnotify\_mkdir，创建新目录  
fsnotify\_access，文件被读  
fsnotify\_modify，文件被写  
fsnotify\_open，文件被打开  
fsnotify\_close，文件被关闭  
fsnotify\_xattr，文件的扩展属性被修改  
fsnotify\_change，文件被修改或原数据被修改  
以上提到的通知函数最后都调用inotify\_inode\_queue\_event，该函数首先判断对应的inode是否被监视，这通过查看inotify\_watches列表是否为空来实现，如果发现inode没有被监视，什么也不做，立刻返回，反之，遍历inotify\_watches列表，看是否当前的文件操作事件被某个 watch 监视，如果是，调用 inotify\_dev\_queue\_event，否则，返回。函数inotify\_dev\_queue\_event 首先判断该事件是否是上一个事件的重复，如果是就丢弃该事件并返回，否则，它判断是否 inotify 实例即 inotify\_device 的事件队列是否溢出，如果溢出，产生一个溢出事件，否则产生一个当前的文件操作事件，这些事件通过kernel\_event 构建，kernel\_event 将创建一个 inotify\_kernel\_event 结构，然后把该结构插入到对应的 inotify\_device 的 events 事件列表，然后唤醒等待在inotify\_device 结构中的 wq 指向的等待队列。想监视文件系统事件的用户态进程在inotify 实例（即inotify\_init()返回的文件描述符）上调用 read 时但没有事件时就挂在等待队列wq上。

#### 4\. Android文件Observer

Android的文件观察Observer机制就是在Linux文件系统的Inotify机制上实现的：

![](https://img-blog.csdn.net/20131101093638828?watermark/2/text/aHR0cDovL2Jsb2cuY3Nkbi5uZXQveWFuZ3dlbjEyMw==/font/5a6L5L2T/fontsize/400/fill/I0JBQkFCMA==/dissolve/70/gravity/SouthEast)