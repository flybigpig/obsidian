---
created: 2025-07-10T13:41:01 (UTC +08:00)
tags: [流式升级]
source: https://blog.csdn.net/qq_40731414/article/details/115552709
author: 成就一亿技术人!
---

# OTA之流式更新及shell实现_流式升级-CSDN博客

> ## Excerpt
> 文章浏览阅读761次。原文链接：https://www.cnblogs.com/zqb-all/p/10230660.html问题： 在OTA升级时，需要从网络下载OTA包，并写到flash上的对应分区中。最简单的方式是将下载与更新分离，先将完整的数据包下载到本地，再将本地的OTA包更新到flash上。方便可靠。但这种方式的问题是，本地需要有足够的空间存放OTA包，这对一些flash较小的产品来说，会起到很大的限制作用，需要在flash上留出一个不小于系统占用大小的空间，用于存放OTA包。但空间确实不够啊，怎么办呢？_流式升级

---
![](https://csdnimg.cn/release/blogv2/dist/pc/img/reprint.png)

[坂田民工](https://blog.csdn.net/qq_40731414 "坂田民工") ![](https://csdnimg.cn/release/blogv2/dist/pc/img/newCurrentTime2.png) 于 2021-04-09 16:03:44 发布

[原文链接：https://www.cnblogs.com/zqb-all/p/10230660.html](https://www.cnblogs.com/zqb-all/p/10230660.html)

-   问题： 在OTA升级时，需要从网络下载OTA包，并写到flash上的对应分区中。最简单的方式是将下载与更新分离，先将完整的数据包下载到本地，再将本地的OTA包更新到flash上。方便可靠。但这种方式的问题是，本地需要有足够的空间存放OTA包，这对一些flash较小的产品来说，会起到很大的限制作用，需要在flash上留出一个不小于系统占用大小的空间，用于存放OTA包。但空间确实不够啊，怎么办呢？
    
-   解决方法：这个时候就需要能支持流式更新了，让从网络下载的数据，直接写到flash中。安卓在AB升级方案中，就支持了这种流式更新（streaming updates）的方式，无需临时空间存放OTA包。
    
-   实现：那么具体怎么实现了，其实流式更新最简单的实现，只需几行shell脚本，调用外部现成的工具，通过管道的形式配合即可实现。
    

> 例如，使用wget下载ota包的话，则可以使用 wget 的 -S 参数，滤出OTA包的长度

```
file_length=$(wget -S "$file_download" 2>&1 | grep "Content-Length" | awk '{print $2}')
```

> 再通过wget与dd相配合，将数据直接写入对应分区。

```
 wget "$file_download" -q -O - | dd of="$partition"
```

> 最后再下载md5校验值，并将写入的数据流式读出，同样通过管道传给md5sum，算出校验值进行校验。

```
 md51=$(wget "$file_download.md5" -q -O -)
 md52=$(dd if="$partition" bs=512 count="$file_sectors" | md5sum | cut -d ' ' -f 1)
```

以上就是使用shell脚本，调用wget，dd，md5sum ，使用管道进行配合，完成的流式更新的核心部分了。
