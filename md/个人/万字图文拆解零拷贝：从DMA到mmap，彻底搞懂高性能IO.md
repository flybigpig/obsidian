
## DMA 的缺陷

前面的文章介绍了DMA的工作原理。

引人DMA后，内存和外设之间的数据传输就不再需要 CPU 的直接干预和参与了。CPU只需要在设置好传输参数后等待DMA完成任务。这样，CPU就能腾出时间去处理其他任务，提高系统的性能和效率。

但系统仍有改进的空间！人类在追求性能的道路上是永无止境的。

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1wAMmDiah7vaGtjylibvINIdT1rDvdHnXBlMGhqVKvUflgibhNuasqpuDw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

典型的 read() 过程如上图所示。

首先，当应用程序需要从磁盘读取数据时，它会调用 read() 系统调用发起读取请求，这会发生从用户态到内核态的模式切换。

CPU 接收到 read() 命令后，首先它会触发 DMA 传输过程。CPU通过发送命令通知 DMA控制器，要求其启动数据的 I/O 传输。

DMA控制器 通知 磁盘，开始从磁盘进行数据传输，此时，磁盘从 磁盘缓存（Disk Cache）读取数据，并将数据传输给 DMA控制器。

一旦磁盘将数据传输到 磁盘缓存，DMA控制器 会直接将数据从磁盘缓存传输到 内核缓冲区。此时，CPU不需要介入，DMA控制器通过总线直接完成数据传输。

DMA将数据传输到内核缓冲区完成后，然后向CPU发送中断请求，通知CPU数据传输已完成。

CPU接收到中断后停止正在执行的任务，转而将数据从内核缓冲区传输到用户缓冲区。最后从内核态切换到用户态。最终应用程序就可以访问到从磁盘读取的数据了。

我们来看一个更复杂的例子，从磁盘读取数据并将其写入网络接口。此操作可能是client-server模式下 Web 应用程序中最常见的操作之一。

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1Ih3z2c4LUIZm1UjJyxGJM0KUFOVMrmEiamSCmF68AejxKo7MHYicRQgQ/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

在这个例子中，首先应用程序 调用 read() 函数，导致 CPU 模式切换（用户态->内核态），并触发 DMA 数据从磁盘复制到内核缓冲区（图中的kernel buffer）。然后 CPU 负责将数据复制到用户缓冲区，再从内核态切换到用户态。

接着，应用程序可能会调用 write() 系统调用，发起将数据写入网络接口（如将数据发送到 Web 服务器）。

write() 函数对网络接口和套接字缓冲区执行类似操作：CPU从用户态切换到内核态，CPU将数据从用户缓冲区复制到套接字缓冲区（图中的socket buffer），然后从内核态切换回用户态，由 DMA 将数据从套接字缓冲区复制到网络接口。

这个过程绝对不能说是高效的，因为总共4次数据复制（2次CPU复制，2次DMA复制），4次用户态与内核态之间的切换。

## 零拷贝（Zero-copy）

前面已经说了 DMA 的局限性，在 DMA 的帮助下，即使大大减少了 CPU 在数据传输过程中的参与，也还需要进行2次 CPU 复制，以及四次用户态与内核态的切换，不必要的复制和模式切换仍然影响了效率。

因此如何减少数据传输过程中的数据复制和模式切换就成了一个急需解决的问题，接下来要介绍的零拷贝就是为了解决这个问题的。

零拷贝的目的很简单，就是消除或减少 CPU 在内核缓冲区和用户缓冲区之间不必要的数据复制，以及模式切换的次数，从而实现性能的提升。

## 零拷贝的实现方式

零拷贝可以通过以下几种方式实现。

### 使用 mmap()

##### mmap介绍

mmap 是一种将文件或其他对象映射到进程地址空间的方法。通过这种方法，操作系统将文件或设备的内容映射到内存中，允许程序像操作内存一样直接访问文件的数据。这种方式使得文件内容与进程的虚拟内存之间建立了一个一对一的映射关系。

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1Y7ibMeQ2kDm08jwlsGIH9OTasOJaxtib2ce6U9o9rL3xWcQaGn2IWBLw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

实现这样的映射关系后，进程就可以采用指针的方式读写操作这一段内存，而系统会自动回写脏页面到对应的文件磁盘上，即完成了对文件的操作而不必再调用read,write等系统调用函数。

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1RXIR2VDSicficKdP6Hh00QNzk1UFHAGASVG0nX0LYicCvm2ibvGMpLlQoQ/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

如上图所示，当应用程序需要访问一个文件时，首先调用 mmap() 将文件映射到进程的虚拟内存地址空间。应用程序就可以直接使用指针来访问内存映射区域中的数据达到访问文件数据的效果。

这样文件的数据在内存中就可以直接访问，避免了不必要的CPU数据复制。同时不需要进行read.write系统调用了，减少了模式切换的次数。

##### mmap系统调用

```
#include   
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```

参数说明：

- addr：指定映射的起始地址。通常设置为 NULL，表示让内核选择地址。注意，这个地址指的是进程的虚拟地址空间中的地址，而不是文件在磁盘上的地址。
    
- length：这个参数指定了映射区域的大小（以字节为单位）。它告诉内核要将文件的多少内容映射到内存中，通常这个值应该是大于0的有效字节数。
    
- prot：这个参数设置了映射区域的访问权限。通过这些标志，你可以控制该内存区域是否可以被读、写或执行。其值可以是：
    

- `PROT_READ`：允许对映射区域的内容进行读取操作。
    
- `PROT_WRITE`：允许对映射区域进行写操作。
    
- `PROT_EXEC`：允许映射区域内容执行（适用于可执行文件映射）。
    
- `PROT_NONE`：不允许对映射区域进行任何操作。
    

- flags：这个参数设置映射区域的行为特征，常见的标志包括：
    

- `MAP_SHARED`：映射区域的修改会反映到文件中，并且对其他进程也是可见的。例如，当你修改映射的文件内容时，修改会同步到文件中。
    
- `MAP_PRIVATE`：映射区域的修改不会反映到文件中，也不会对其他进程可见。这个标志通常用于临时修改数据，不会持久化到文件中。
    
- `MAP_ANONYMOUS`：映射区域不与任何文件关联，通常用于分配内存。在这种情况下，文件描述符 fd 应该设置为 -1，offset 可以设置为 0。
    

- fd：文件描述符，表示要映射的文件。
    
- offset：文件映射的起始偏移量，表示从文件的哪个位置开始映射到内存，其值必须是页大小（通常是 4096 字节）的整数倍。
    

下面这张图可以帮助我们理解这些参数。![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1JEDN81HZl4t1CaB6GeQT3NzWun6wwSkdSfBqm28BGdNCskpUC2EQqA/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

mmap 函数的返回值是一个 void * 类型的指针，指向被映射到文件的内存映射区域的起始地址，有了这个指针，后续就可以通过它来访问映射区域中的数据，也就是对应映射文件的内容。

##### 示例

下面是一个使用 mmap 的简单示例，演示如何将文件映射到内存，并读写其内容：

假设有一个存储数字的文件 numbers.txt，内容如下：

`1   2   3   4   5   `

现在想读取这个文件，将其中的一些数字加倍，然后将修改后的数据保存回文件。

```
#include <sys/mman.h>  
#include <fcntl.h>  
#include <unistd.h>  
#include <stdio.h>  
  
int main() {  
    // 打开文件  
    int fd = open("numbers.txt", O_RDWR);  
    if (fd == -1) {  
        perror("open");  
        return1;  
    }  
  
    // 获取文件的长度  
    off_t length = lseek(fd, 0, SEEK_END);  
  
    // 使用 mmap 映射文件到内存，允许读取和写入  
    int* data = (int*)mmap(NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);  
    if (data == MAP_FAILED) {  
        perror("mmap");  
        return1;  
    }  
  
    // 修改映射到内存中的数据（比如将数字加倍）  
    for (int i = 0; i < length / sizeof(int); i++) {  
        data[i] *= 2;  
    }  
  
    // 由于使用了 MAP_SHARED，修改的数据会直接反映到文件中  
    // 关闭文件描述符，映射成功后文件描述符不再需要  
    close(fd);  
      
        // 刷新映射区域到文件  
    if (msync(data, sb.st_size, MS_SYNC) == -1)     {  
        perror("msync");  
        exit(EXIT_FAILURE);  
    }  
  
    // 解除映射  
    if (munmap(data, sb.st_size) == -1) {  
        perror("munmap");  
        exit(EXIT_FAILURE);  
    }  
  
    return0;  
}
```

上述代码经过编译然后执行后，文件内容变为：

`2   4   6   8   10   `

这个示例展示了如何使用 mmap 实现文件的读取和写入操作。通过将文件映射到内存中，我们可以通过修改内存区域来修改文件内容。

### 使用 sendfile()

##### sendfile介绍

Linux 内核 2.1 为我们提供了一个新的系统调用 sendfile()，用于替代 read() 和 write()，只需1个系统调用而不是2个，我们就可以省去2次用户态与内核态的模式切换。

sendfile() 系统调用通常用于将数据从文件直接发送到网络套接字（例如，在服务器上传输文件）。

下图是sendfile() 的工作流程。![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG16EibcusNrrrBTKTya0tvicWDzuZJrlvkvWd95SuTk2jcvlYlOmM3lAHw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

应用程序通过调用 sendfile() 发起数据传输，这时CPU从 用户态 切换到 内核态，通过DMA 复制将数据从磁盘传输到 内核缓冲区（kernel buffer）。接下来CPU执行数据复制，将数据从内核缓冲区复制到套接字缓冲区，然后DMA 复制将数据从套接字缓冲区socket buffer）复制到 网络接口。

可以看出使用 sendfile()，需要进行3次数据复制（1次CPU复制，2次DMA复制），以及2次用户态与内核态的切换，相比最初的read() 和 write()，节省了1次CPU数据复制，2次用户态与内核态的切换。

##### sendfile系统调用

```
#include   
ssize_t sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
```

参数说明：

- out_fd：目标文件或目标套接字的文件描述符。
    
- in_fd：源文件的文件描述符，表示要从哪个文件读取数据。
    
- offset：指定从源文件的哪个位置开始读取，如果这个值为 NULL，则表示从文件的开头开始读取。
    
- count：要拷贝的字节数。
    

##### 示例

以下是在 C 语言中使用 sendfile() 函数进行文件拷贝的简单示例：

```
#include <stdio.h>  
#include <fcntl.h>  
#include <sys/stat.h>  
#include <sys/sendfile.h>  
#include <unistd.h>  
  
int main() {  
    int source_fd, dest_fd;  
    struct stat stat_source;  
    off_t offset = 0;  
    ssize_t bytes_sent;  
  
    // 打开源文件和目标文件  
    source_fd = open("source.txt", O_RDONLY);  
    dest_fd = open("destination.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);  
  
    // 获取源文件的大小  
    fstat(source_fd, &stat_source);  
  
    // 使用 sendfile 将源文件内容发送到目标文件  
    bytes_sent = sendfile(dest_fd, source_fd, &offset, stat_source.st_size);  
  
    if (bytes_sent == -1) {  
        perror("sendfile");  
        return1;  
    }  
  
    printf("Successfully copied %ld bytes\n", bytes_sent);  
  
    // 关闭文件描述符  
    close(source_fd);  
    close(dest_fd);  
  
    return0;  
}
```
除了在可以使用sendfile在本地的两个文件拷贝数据外，特别是在网络编程中常用于将文件内容发送给客户端，这种情况下，out_fd是一个socket套接字文件描述符。

### 使用 sendfile() 和 DMA Gather

Linux 2.4 对 sendfile() 系统调用进行了一些改进，其中最重要的就是 DMA Scatter/Gather 的出现，通过这项改进，我们终于可以消除上述场景中的所有 CPU 拷贝，实现真正的 Zero-copy。

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1Rj6yMj2Oian5chFdTfhQgPYyF49NuCr646V5LyuZ8gL88cte1XyeeCg/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

上图展示了这一过程，当应用程序调用 sendfile() 时，DMA 控制器通过 DMA scatter 将数据从 内核缓冲区 传输到  网络接口。整个数据传输过程不再需要将数据从内核缓冲区拷贝到 socket 缓冲区中。

使用 sendfile() 和 DMA Gather只需要2次数据复制（2次DMA复制），以及2次用户态与内核态的切换，相比最初的read() 和 write()，节省了一半操作。

这就是所谓的零拷贝（Zero-copy）技术，因为我们没有在CPU层面去拷贝数据，也就是说全程没有通过 CPU 来搬运数据，所有的数据都是通过 DMA 来进行传输的。

需要注意的是DMA Scatter/Gather 不是在所有系统中都默认启用，它取决于硬件支持（DMA 控制器的功能）以及操作系统和驱动程序的支持。

## Java零拷贝实验

在Java中，FileChannel类提供了使用mmap()和sendfile()机制的API。在下面的实验中，我在Ubuntu 22.04系统上使用三种方法分别复制了同一个880 MB的文件。对比了耗时情况，以下是实验结果。

```
private static void sendfileCopyFile(String inputFilePath, String outputFilePath) {  
  
    long start = System.currentTimeMillis();  
  
    try (  
            FileChannel channelIn = new FileInputStream(inputFilePath).getChannel();  
            FileChannel channelOut  = new FileOutputStream(outputFilePath).getChannel();  
    ) {  
        channelIn.transferTo(0, channelIn.size(), channelOut);  
  
    } catch (IOException e) {  
        e.printStackTrace();  
    }  
  
    long end = System.currentTimeMillis();  
    System.out.println("Total time spent: " + (end - start));  
}  
  
private static void mmapCopyFile(String inputFilePath, String outputFilePath) {  
  
    long start = System.currentTimeMillis();  
  
    try (  
            FileChannel channelIn = new FileInputStream(inputFilePath).getChannel();  
            FileChannel channelOut = new RandomAccessFile(outputFilePath, "rw").getChannel();  
  
    ) {  
        long size = channelIn.size();  
        MappedByteBuffer mbbi = channelIn.map(FileChannel.MapMode.READ_ONLY, 0, size);  
        MappedByteBuffer mbbo = channelOut.map(FileChannel.MapMode.READ_WRITE, 0, size);  
        for (int i = 0; i < size; i++) {  
            byte b = mbbi.get(i);  
            mbbo.put(i, b);  
        }  
  
    } catch (Exception e) {  
        e.printStackTrace();  
    }  
  
    long end = System.currentTimeMillis();  
    System.out.println("Total time spent: " + (end - start));  
}  
  
private static void bufferInputStreamCopyFile(String inputFilePath, String outputFilePath) {  
  
    long start = System.currentTimeMillis();  
    try(  
            BufferedInputStream bis = new BufferedInputStream(new FileInputStream(inputFilePath));  
            BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(outputFilePath));  
    ){  
        byte[] buf = newbyte[1];  
        int len;  
        while ((len = bis.read(buf)) != -1) {  
            bos.write(buf);  
        }  
  
    }catch(Exception e){  
        e.printStackTrace();  
    }  
    long end = System.currentTimeMillis();  
    System.out.println("Total time spent: " + (end - start));  
}
```  

![图片](https://mmbiz.qpic.cn/mmbiz_png/FWANMMXDrgKfYNu32vibNbhPRJ34LBTG1G36vU0iaMO5TKTDmrdWibKHfwLBePamgtaiakOoOJ6TtUlGvfv03ibFGbw/640?wx_fmt=png&from=appmsg&tp=webp&wxfrom=5&wx_lazy=1)

从结果来看，两种类型的零拷贝实现都有显着的改进。mmap() 的速度提高了 81%，sendfile() 的速度提高了 91%，这也是为什么人们在业界大量使用这种技术的原因。