---
created: 2025-04-15T10:12:11 (UTC +08:00)
tags: [内存,内存映射,linux,mmap,进程]
source: https://cloud.tencent.com/developer/article/2420465
author: 
---

# 一文读懂 Linux mmap 内存映射-腾讯云开发者社区-腾讯云

> ## Excerpt
> mmap（memory map）即内存映射，用于将一个文件或设备映射到进程的地址空间，或者创建匿名的内存映射。

---
### 1.简介

mmap（memory map）即内存映射，用于将一个文件或设备映射到进程的地址空间，或者创建匿名的内存映射。

请注意，虽然 mmap() 最初是为映射文件而设计的，但它实际上是一个通用映射工具。它可用于将任何适当的对象（例如内存、文件、设备等）映射到进程的地址空间。

以文件映射到内存为例，实现这样的映射后，进程虚拟地址空间中一段内存地址将与文件磁盘地址一一对应，进程就可以采用指针的方式读写这段内存，系统会自动回写脏页到对应的磁盘文件。

![](https://developer.qcloudimg.com/http-save/yehe-2609282/3b674374d93eb965dad685cc41d5cc82.png)

上图表示进程的虚拟地址空间布局，分为多个区域，每个区域存放不同类型的数据。**内存映射区域处于堆与栈之间。**

Linux 内核使用 vm\_area\_struct 结构来表示一个独立的虚拟内存区域，由于每个不同质的虚拟内存区域功能和内部机制都不同，因此一个进程使用多个 vm\_area\_struct 结构来分别表示不同类型的虚拟内存区域。各个 vm\_area\_struct 结构使用链表或者树形结构链接，方便进程快速访问，如下图所示：

![](https://developer.qcloudimg.com/http-save/yehe-2609282/fc41804db30cba14be60b6c7c11cf02e.png)

vm\_area\_struct 结构中包含区域起始和终止地址以及其他相关信息，同时也包含一个 vm\_ops 指针，其内部可引出所有针对这个区域可以使用的系统调用函数。这样，进程对某一虚拟内存区域的任何操作需要用到的信息，都可以从 vm\_area\_struct 中获得。

mmap 函数就是要创建一个新的 vm\_area\_struct 结构，并将其与文件的物理磁盘地址相连。

### 2.实现原理

mmap 实现内存映射，总的来说分为三个阶段：

**（1）进程启动映射过程，并在虚拟地址空间中为映射创建虚拟映射区域。**

1.  进程在用户空间调用函数 mmap(2)。
2.  在当前进程的虚拟地址空间中，寻找一段空闲的满足要求的连续虚拟地址。
3.  为此虚拟区分配一个 vm\_area\_struct 结构，接着对这个结构的各个域进行初始化。
4.  将新建的虚拟区结构（vm\_area\_struct）插入进程的虚拟地址区域链表或树中。

**（2）调用内核空间的系统调用函数 mmap（不同于用户空间函数），实现文件物理地址和进程虚拟地址的映射。**

1.  为映射分配了新的虚拟地址区域后，通过待映射的文件指针，在文件描述符表中找到对应的文件描述符，通过文件描述符，链接到内核“已打开文件集”中该文件的文件结构体（struct file），每个文件结构体维护着和这个已打开文件相关各项信息。
2.  通过该文件的文件结构体，链接到 file\_operations 模块，调用内核函数 mmap，其原型为`int mmap(struct file *filp, struct vm_area_struct *vma)`，不同于用户空间库函数。
3.  内核 mmap 函数通过虚拟文件系统 inode 模块定位到文件磁盘物理地址。
4.  通过`remap_pfn_range`函数建立页表，即实现了文件地址和虚拟地址区域的映射关系。此时，这片虚拟地址并没有任何数据关联到主存中。

**（3）进程发起对这片映射空间的访问，引发缺页异常，实现文件内容到物理内存的拷贝。**

前两个阶段仅在于创建虚拟区间并完成地址映射，但是并没有将任何文件数据拷贝至主存。真正的文件读取是当进程发起读或写操作时。

1.  进程的读或写操作访问虚拟地址空间这一段映射地址，通过查询页表，发现这一段地址对应的物理内存页面上没有数据。因为目前只建立了地址映射，真正的硬盘数据还没有拷贝到内存，因此引发缺页异常。
2.  缺页异常进行一系列判断，确定无非法操作后，内核发起调页过程。
3.  调页过程先在交换缓存空间（swap cache）中寻找需要访问的内存页，如果没有则调用 nopage 函数把所缺的页从磁盘载入主存。
4.  之后进程即可对这片主存进行读写，如果写操作改变了其内容，一定时间后系统会自动回写脏页到对应磁盘地址，即完成了写入到文件的过程。

注：修改过的脏页并不会立即更新回文件中，而是有一段时间的延迟，可以调用`msync(2)`来强制同步，这样所写的内容就能立即保存到文件里了。

### 3.mmap和常规文件操作的区别

首先简单回顾一下常规文件操作（调用read/fread等函数）的函数调用过程：

1.  进程发起读文件请求。
2.  内核通过查找进程文件符表，定位到内核已打开文件集上的文件信息，从而找到此文件的 inode。
3.  inode 在 address\_space 上查找要请求的文件页是否已经缓存在页缓存。如果存在，则直接返回这片文件页的内容。
4.  如果不存在，则通过 inode 定位到文件磁盘地址，将数据从磁盘复制到页缓存。之后再次发起读页请求，进而将页缓存中的数据发给用户进程。

总的来说，常规文件操作为了提高读写效率和保护磁盘，使用了页缓存机制。这样造成读文件时需要先将文件页从磁盘拷贝到页缓存。由于页缓存处在内核空间，不能被用户进程直接寻址，所以还需要将页缓存中数据页再次拷贝到用户空间内存。这样，**通过了两次数据拷贝，才能完成进程对文件内容的获取任务**。

写操作也是一样，待写入的 buffer 在内核空间不能直接访问，必须要先拷贝至内核空间内存，再写回磁盘中（延迟写回），也需要两次数据拷贝。

而使用 mmap 操作文件，创建新的虚拟内存区域和建立文件磁盘地址和虚拟内存区域映射这两步，没有任何文件拷贝操作。而之后访问数据时发现内存中并无数据而发起的缺页异常过程，可以通过已经建立好的映射关系，只使用一次数据拷贝，就从磁盘中将数据传入内存的用户空间中，供进程使用。

**总而言之，常规文件操作需要从磁盘到页缓存再到用户主存的两次数据拷贝。而 mmap 操作文件，只需要从磁盘到用户主存的一次数据拷贝，效率更高。**

### 4.相关函数

#### 创建映射：mmap

```
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
```

成功执行时，mmap() 返回被映射区的指针。失败时，mmap() 返回 MAP\_FAILED，其值为 (void \*)-1，errno 被设为以下的某个值：

```
EACCES访问出错
EAGAIN文件已被锁定，或者太多的内存已被锁定
EBADF不是有效的文件描述词
EINVAL一个或者多个参数无效
ENFILE已达到系统对打开文件的限制
ENODEV指定文件所在的文件系统不支持内存映射
ENOMEM内存不足，或者进程已超出最大内存映射数量
EPERM权能不足，操作不允许
ETXTBSY已写的方式打开文件，同时指定MAP_DENYWRITE标志
SIGSEGV试着向只读区写入
SIGBUS试着访问不属于进程的内存区
```

入参 addr 表示要映射到的内存区域的起始地址，通常用 NULL，表示由内核指定该内存地址。

length 表示映射区的长度，单位字节。

prot 参数描述了映射区所需的保护模式（不得与文件的打开模式冲突）。它是 PROT\_NONE 或以下多个标志位的组合：

```
PROT_EXEC 页面可以被执行
PROT_READ 页面可以被读取
PROT_WRITE 页面可以被写入
PROT_NONE 页面不能被访问
```

flags 指定映射对象的类型，可以是一个或多个以下位的组合体：

```
MAP_FIXED
使用指定的映射起始地址，如果由start和len参数指定的内存区重叠于现存的映射空间，重叠部分将会被丢弃。如果指定的起始地址不可用，操作将会失败。并且起始地址必须落在页的边界上。

MAP_SHARED
与其它所有映射这个对象的进程共享映射空间。对共享区的写入，相当于输出到文件。直到msync()或munmap()被调用，文件实际上不会被更新。

MAP_PRIVATE
建立一个写时拷贝的私有映射。内存区域的写入不会影响到原文件。该选项与 MAP_SHARED 互斥，不能同时存在。

MAP_DENYWRITE
这个标志被忽略。

MAP_EXECUTABLE
这个标志被忽略。

MAP_NORESERVE
不要为这个映射保留交换空间。当交换空间被保留，对映射区修改的可能会得到保证。当交换空间不被保留，同时内存不足，对映射区的修改会引起段违例信号。

MAP_LOCKED
锁定映射区的页面，从而防止页面被交换出内存。

MAP_GROWSDOWN
用于堆栈，告诉内核VM系统，映射区可以向下扩展。

MAP_ANONYMOUS
匿名映射，映射区不与任何文件关联。

MAP_ANON
MAP_ANONYMOUS 的别称，不再被使用。

MAP_FILE
兼容标志，被忽略。

MAP_32BIT
将映射区放在进程地址空间的低2GB，MAP_FIXED指定时会被忽略。当前这个标志只在x86-64平台上得到支持。

MAP_NONBLOCK
仅和MAP_POPULATE一起使用时才有意义。不执行预读，只为已存在于内存中的页面建立页表入口。

MAP_NORESERVE
不要为此映射保留交换空间。 当交换空间被保留时，就可以保证可以修改映射。 当未保留交换空间时，如果没有可用的物理内存，则可能会在写入时收到 SIGSEGV。

MAP_POPULATE
为文件映射通过预读的方式准备好页表。随后对映射区的访问不会被页违例阻塞。

MAP_STACK (since Linux 2.6.27)
将映射分配到适合进程或线程的栈空间。该标志目前是无操作的，但在 glibc 线程实现中有使用。

MAP_UNINITIALIZED (since Linux 2.6.33)
不清除匿名页面。此标志旨在提高嵌入式设备的性能。只有当内核配置了CONFIG_MMAP_ALLOW_UNINITIALIZED 选项时，才会使用这个标志。由于安全问题，该选项通常只在嵌入式设备上启用。
```

fd 有效的文件描述词。如果 MAP\_ANONYMOUS 被设定，为了兼容问题，其值应为 -1。

offset 被映射对象的内容偏移。

#### 解除映射：munmap

```
#include <sys/mman.h>

int munmap(void *addr, size_t length);
```

成功返回 0，失败返回 -1，errno 返回标志和 mmap 一致。

该调用在进程地址空间中解除一个映射关系，addr 是调用 mmap() 时返回的地址，length 是映射区的大小。

当映射关系解除后，对原来映射地址的访问将导致段错误发生。

#### 同步：msync

```
int msync(void *addr, size_t len, int flags)
```

一般说来，进程对映射区的改变并不直接写回到磁盘文件中，往往在调用 munmap() 后才执行该操作。

可以通过调用 msync() 将映射区内容同步到磁盘文件。

#### 扩缩映射：mremap

如果需要运行时动态扩缩映射区域大小，可以使用 mremap(2) 系统调用。

```
void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ... /* void *new_address */)
```

old\_address：旧映射区域的起始地址。 old\_size：旧映射区域的大小。 new\_size：新映射区域的大小。 flags：标志参数，可以为 0 或以下位标志组合：

-   MREMAP\_MAYMOVE 默认情况下，如果没有足够的空间在当前位置扩展映射，则 mremap() 会失败。 如果指定了此标志，则允许内核在必要时将映射重新定位到新的虚拟地址。 如果映射被重新定位，则指向旧映射位置的绝对指针将变得无效
-   MREMAP\_FIXED (since Linux 2.3.31) 该标志的用途与 mmap(2) 的 MAP\_FIXED 标志类似。 如果指定了此标志，则 mremap() 接受第五个参数 void \*new\_address，它指定映射必须移动到的页对齐地址。 new\_address 和 new\_size 指定的地址范围内的任何先前映射都将取消映射。 如果指定了 MREMAP\_FIXED，则还必须指定 MREMAP\_MAYMOVE。

new\_address：新映射区域的起始地址，如果为 NULL，表示由系统选择地址。

### 5.使用场景

Linux mmap 是一个灵活的系统调用，主要用于在进程的虚拟地址空间中创建映射，使得文件、设备、匿名映射等对象能够直接映射到进程的地址空间。以下是一些常见的使用场景：

#### 5.1 映射文件：减少数据拷贝，提高 IO 效率

将文件映射到进程的地址空间，使得进程可以通过直接读写内存来访问文件内容，而不必使用 read 和 write 等系统调用。对文件的读写跨过了内核页缓存，减少数据拷贝次数，提高了文件读写效率。

下面在这个例子中，我们将文件映射到内存中，然后使用内存中的数据进行读写。最后，解除映射并关闭文件。

```
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    const char *file_path = "example.txt";
    const size_t file_size = 4096;  // 文件大小为 4KB

    // 打开文件，如果文件不存在则创建
    int fd = open(file_path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    // 调整文件大小为指定大小
    if (ftruncate(fd, file_size) == -1) {
        perror("ftruncate");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 将文件映射到内存
    char *data = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        exit(EXIT_FAILURE);
    }

    // 写入数据到内存映射区域
    const char *message = "Hello, mmap!";
    snprintf(data, file_size, "%s", message);

    // 刷新映射区域到文件
    if (msync(data, file_size, MS_SYNC) == -1) {
        perror("msync");
    }

    // 从内存映射区域读取数据并打印
    printf("Read from memory-mapped file: %s\n", data);

    // 解除映射
    if (munmap(data, file_size) == -1) {
        perror("munmap");
    }

    // 关闭文件
    close(fd);

    return 0;
}
```

#### 5.2 共享内存：进程间通信

进程间通信分为两种情况：一种是无亲缘关系的进程间通信，一种是有亲缘关系的父子进程间通信。

使用普通文件的内存映射，适用于任何进程之间的通信。

父子进程间通信一般使用匿名映射，此时，不必指定具体的文件，只要设置相应的标志（MAP\_ANONYMOUS）即可。在父进程中先调用 mmap()，然后调用 fork()。那么在调用 fork() 之后，子进程继承父进程匿名映射的地址区域，同样也继承 mmap() 返回的地址。这样，父子进程就可以通过匿名映射区域进行通信了。

下面是父子进程通过匿名映射实现通信示例：

```
#include <sys/mman.h>    
#include <stdio.h>    
#include <stdlib.h>    
#include <unistd.h>    
    
#define BUF_SIZE 100    
    
int main(int argc, char** argv) {    
    char *p_map;

    // 匿名映射：创建一块内存供父子进程通信
    p_map = (char *)mmap(NULL, BUF_SIZE, PROT_READ | PROT_WRITE,    
            MAP_SHARED | MAP_ANONYMOUS, -1, 0);    
    
    if(fork() == 0) {    
        sleep(1);    
        printf("child got a message: %s\n", p_map);    
        sprintf(p_map, "%s", "hi, dad, this is son");    
        munmap(p_map, BUF_SIZE); //实际上，进程终止时，会自动解除映射。    
        exit(0);    
    }    
    
    sprintf(p_map, "%s", "hi, this is father");    
    sleep(2);    
    printf("parent got a message: %s\n", p_map);    

    return 0;    
}
```

#### 5.3 申请内存：可动态扩缩

mmap 的匿名映射，除了可用于父子进程间通信，还可用于申请大块内存，并在运行时动态扩缩映射区域大小，而不需要重新创建映射。

匿名映射不受文件支持，基本上是对内存块的请求。如果你认为这听起来与 malloc 类似，那么你是对的。事实上，大多数 malloc 的实现都会在内部使用匿名 mmap 来提供大的内存区域。

以下是一个简单的示例，演示如何使用 mremap(2) 动态扩展 mmap 映射区域的大小：

```
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    size_t initial_size = 4096;  // 初始大小为 4 KB
    size_t expanded_size = 8192; // 扩展大小为 8 KB

    // 创建映射区域
    void *ptr = mmap(NULL, initial_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap");
        exit(EXIT_FAILURE);
    }

    printf("Initial size: %zuKB\n", initial_size / 1024);

    // 使用 mremap 扩展映射区域的大小
    void *new_ptr = mremap(ptr, initial_size, expanded_size, MREMAP_MAYMOVE);
    if (new_ptr == MAP_FAILED) {
        perror("mremap");
        exit(EXIT_FAILURE);
    }

    printf("Expanded size: %zuKB\n", expanded_size / 1024);

    // 使用新的映射区域进行读写操作...

    // 解除映射
    if (munmap(new_ptr, expanded_size) == -1) {
        perror("munmap");
        exit(EXIT_FAILURE);
    }

    return 0;
}
```

在这个示例中，首先使用 mmap 创建了一个初始大小的映射区域，然后使用 mremap 将其动态地扩展到新的大小。注意，mremap 会返回新映射区域的起始地址，而原始映射区域的地址 ptr 变得无效。

#### 5.4 动态库加载：节省内存

假设有一个文件，很多进程的运行都依赖于此文件，而且还是有一个假设，那就是这些进程是以只读(read-only)的方式依赖于此文件。

你一定在想，这么神奇？很多进程以只读的方式依赖此文件？有这样的文件吗？

答案是肯定的，这就是动态链接库。

要想弄清楚动态链接库，我们就不得不从静态库说起。

假设有三个程序A、B、C依赖一个静态库，那么链接器在生成可执行程序 A、B、C 时会把该静态库 copy 到 A、B、C 中

假设你本身要写的代码只有2MB大小，但却依赖了一个100MB的静态库，那么最终生成的可执行程序就是102MB，尽管你本身的代码只有2MB。

可执行程序 A、B、C 中都有一部分静态库的副本，这里面的内容是完全一样的，那么很显然，这些可执行程序放在磁盘上会浪费磁盘空间，加载到内存中运行时会浪费内存空间。

很简单，可执行程序A、B、C中为什么都要各自保存一份完全一样的数据呢？其实我们只需要在可执行程序A、B、C中保存一小点信息，这点信息里记录了依赖了哪个库，那么当可执行程序运行起来后再把相应的库加载到内存。

依然假设你本身要写的代码只有2MB大小，此时依赖了一个100MB的动态链接库，那么最终生成的可执行程序就是2MB，尽管你依赖了一个100MB的库。此时可执行程序ABC中已经没有冗余信息了，这不但节省磁盘空间，而且节省内存空间，让有限的内存可以同时运行更多的进程。

现在我们已经知道了动态库的妙用，但我们并没有说明动态库是怎么节省内存的，接下来mmap就该登场了。

你不是很多进程都依赖于同一个库嘛，那么我就用 mmap 把该库直接映射到各个进程的地址空间，**尽管每个进程都认为自己地址空间中加载了该库，但实际上该库在内存中只有一份**。

### 6.FAQ

（1）mmap 映射到进程的虚拟地址是一样的吗？

在 Linux 中，mmap 函数可以用于将一个文件或者其他对象映射到进程的地址空间。对于相同的文件或对象，多个进程可以通过 mmap 将其映射到各自的地址空间中。这种映射的地址并不一定相同，因为每个进程有自己独立的虚拟地址空间。

（2）不同进程的虚拟地址共享的是同一块内核内存吗

是的，当多个进程通过 mmap 映射同一个文件时，它们实际上共享同一块物理内存（或者说内核内存）。这意味着它们可以通过这个映射共享数据，对文件的修改可以在各个映射中反映出来。

### 参考文献

[mmap(2) - Linux manual page](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fman7.org%2Flinux%2Fman-pages%2Fman2%2Fmmap.2.html&objectId=2420465&objectType=1&isNewArticle=undefined) [mmap - opengroup.org](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fpubs.opengroup.org%2Fonlinepubs%2F009604499%2Ffunctions%2Fmmap.html&objectId=2420465&objectType=1&isNewArticle=undefined) [认真分析mmap：是什么为什么怎么用- 胡潇](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fwww.cnblogs.com%2Fhuxiao-tee%2Fp%2F4660352.html&objectId=2420465&objectType=1&isNewArticle=undefined) [Linux source code (v6.0) - Elixir Bootlin](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Felixir.bootlin.com%2Flinux%2Flatest%2Fsource&objectId=2420465&objectType=1&isNewArticle=undefined) [How to use mmap function in C language?](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Flinuxhint.com%2Fusing_mmap_function_linux%2F&objectId=2420465&objectType=1&isNewArticle=undefined) [Linux的虚拟内存详解（MMU、页表结构）](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fblog.csdn.net%2Fqq_38410730%2Farticle%2Fdetails%2F81036768&objectId=2420465&objectType=1&isNewArticle=undefined) [When would you use mmap - Stack Overflow](https://cloud.tencent.com/developer/tools/blog-entry?target=https%3A%2F%2Fstackoverflow.com%2Fquestions%2F12095241%2Fwhen-would-you-use-mmap&objectId=2420465&objectType=1&isNewArticle=undefined)

本文参与 [腾讯云自媒体同步曝光计划](https://cloud.tencent.com/developer/support-plan)，分享自作者个人站点/博客。

原始发表：2024-02-22，如有侵权请联系 [cloudcommunity@tencent.com](mailto:cloudcommunity@tencent.com) 删除
