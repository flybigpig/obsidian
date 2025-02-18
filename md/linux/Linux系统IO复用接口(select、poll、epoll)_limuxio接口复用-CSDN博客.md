[epoll](https://so.csdn.net/so/search?q=epoll&spm=1001.2101.3001.7020)仅仅是一个异步事件的通知机制，其本身并不作任何的IO读写操作，它只负责告诉你是不是可以读或可以写了，而具体的读写操作，还要应用程序自己来完成。epoll仅提供这种机制是非常好的，它保持了事件通知与IO操作之间彼此的独立性，使得epoll的使用更加灵活。

```
int epoll_create(int size) 
```

该函数生成一个epoll专用的文件描述符。它其实是在内核申请一空间，用来存放你想关注的fd上是否发生的事件。size就是你在这个epoll fd上能关注的最大fd数，这个参数不同于select()中的第一个参数，给出最大监听的fd+1的值。需要注意的是，当创建好epoll句柄后，它就会占用一个fd值，在linux下如果查看/proc/进程id/fd/，是能够看到这个fd的，所以在使用完epoll后，必须调用close()关闭，否则可能导致fd被耗尽。

```
int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) 
```

该函数用于控制某个epoll文件描述符上的事件，可以注册事件，修改事件，删除事件。   
epfd：由 epoll\_create生成的epoll专用的文件描述符；   
op：要进行的操作例如注册事件，可能的取值:  
1）EPOLL\_CTL\_ADD 注册新的fd到epfd中；  
2）EPOLL\_CTL\_MOD修改已经注册的fd的监听事件；  
3）EPOLL\_CTL\_DEL 从epfd中删除一个fd；  
fd：需要监听的fd   
event：指向epoll\_event的指针，告诉内核需要监听的事件，常用的事件类型:   
1）EPOLLIN ：表示对应的文件描述符可以读；  
2）EPOLLOUT：表示对应的文件描述符可以写；  
3）EPOLLPRI：表示对应的文件描述符有紧急的数据可读（这里应该表示有带外数据到来）；  
4）EPOLLERR：表示对应的文件描述符发生错误；  
5）EPOLLHUP：表示对应的文件描述符被挂断；  
6）EPOLLET： 将EPOLL设为边缘触发(Edge Triggered)模式，这是相对于水平触发(Level Triggered)来说的。  
7）EPOLLONESHOT：只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里。  
如果调用成功返回0,不成功返回-1 

```
int epoll_wait(int epfd,struct epoll_event * events,int maxevents,int timeout) 
```

该函数用于轮询I/O事件的发生，如果发生则将发生的fd和事件类型放入到events数组中。 并且将注册在epfd上的fd的事件类型给清空，所以如果下一个循环你还要关注这个fd的话，则需要用epoll\_ctl(epfd,EPOLL\_CTL\_MOD,fd,&ev)来重新设置fd的事件类型。这时不用EPOLL\_CTL\_ADD,因为fd并未清空，只是事件类型清空。  
epfd:由epoll\_create生成的epoll专用的文件描述符；   
epoll\_event:用于回传待处理事件的数组；   
maxevents:每次能处理的事件数；   
timeout:等待I/O事件发生的超时值，为0的时候表示马上返回，为\-1的时候表示一直等下去，直到有事件返回。

epoll程序基本框架：  

```c
for (;;) {
    nfds = epoll_wait(epfd, events, 20, 500);
    for (i = 0; i < nfds; ++i) {
        if (events[i].data.fd == listenfd) {
            connfd = accept(listenfd, (sockaddr * ) & clientaddr, & clilen);
            ev.data.fd = connfd;
            ev.events = EPOLLIN | EPOLLET;
            epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, & ev);
        } else if (events[i].events & EPOLLIN) {
            n = read(sockfd, line, MAXLINE)) < 0 ev.data.ptr = md;
        ev.events = EPOLLOUT | EPOLLET;
        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, & ev);
    } else if (events[i].events & EPOLLOUT) {
        struct myepoll_data * md = (myepoll_data * ) events[i].data.ptr;
        sockfd = md - > fd;
        send(sockfd, md - > ptr, strlen((char * ) md - > ptr), 0);
        ev.data.fd = sockfd;
        ev.events = EPOLLIN | EPOLLET;
        epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, & ev);
    } else {}
}
}
```

  

#### 接口比较

Linux提供了select、poll、epoll接口来实现IO复用，三者的原型如下所示，本文从参数、实现、性能等方面对三者进行对比。

```c
int select(int nfds, fd_set * readfds, fd_set * writefds, fd_set * exceptfds, struct timeval * timeout);
int poll(struct pollfd * fds, nfds_t nfds, int timeout);
int epoll_wait(int epfd, struct epoll_event * events, int maxevents, int timeout);
```

select的第一个参数nfds为fdset集合中最大描述符值加1，fdset是一个位数组，其大小限制为\_\_FD\_SETSIZE（1024），select的第二三四个参数表示需要关注读、写、错误事件的文件描述符位数组，这些参数既是输入参数也是输出参数，可能会被内核修改用于标示哪些描述符上发生了关注的事件。所以每次调用select前都需要重新初始化fdset。select对应于内核中的sys\_select调用，sys\_select首先将第二三四个参数指向的fd\_set拷贝到内核，然后对每个被SET的描述符调用进行poll，并记录在临时结果中（fdset），如果有事件发生，select会将临时结果写到用户空间并返回；当轮询一遍后没有任何事件发生时，如果指定了超时时间，则select会睡眠到超时，睡眠结束后再进行一次轮询，并将临时结果写到用户空间，然后返回。select返回后，需要逐一检查关注的描述符是否被SET（事件是否发生）。

poll与select不同，通过一个pollfd数组向内核传递需要关注的事件，故没有描述符个数的限制，pollfd中的events字段和revents分别用于标示关注的事件和发生的事件，故pollfd数组只需要被初始化一次。poll的实现机制与select类似，其对应内核中的sys\_poll，只不过poll向内核传递pollfd数组，然后对pollfd中的每个描述符进行poll，相比处理fdset来说，poll效率更高。

epoll通过epoll\_create创建一个用于epoll轮询的描述符，通过epoll\_ctl添加/修改/删除事件，通过epoll\_wait检查事件，epoll不是通过轮询，而是通过在等待的描述符上注册回调函数，当事件发生时，回调函数负责把发生的事件存储在就绪事件链表中，最后写到用户空间。epoll返回后，该参数指向的缓冲区中即为发生的事件，即epoll返回时已经明确的知道哪个fd发生了事件，不用再一个个比对。这样就提高了效率。同时select的FD\_SETSIZE是有限止的，而epoll是没有限止的只与系统资源有关。epoll不会随着监听fd数目的增长而降低效率，因为select采用轮询方式，轮询的fd数目越多，自然耗时越多，而epoll是触发式的，所以效率高。