MediaExtractor



> 定义: MediaExtractor是多媒体提取器,专门提取多媒体的配置信息

##### 1.1.1作用:

它在Android的音视频开发里主要负责提取视频或者音频中的信息和数据流

- 例如将视频文件剥离出音频与视频
- 如何分别音频和视频轨道和获取视频里的一些信息. 

> extractor.getTrackCount()   获取轨道的数量
>
>  extractor.getTrackFormat(i) 获取当前轨道的配置信息  如( 类型(音频还是视频 )，轨道ID，时长,)

```
public static int selectTrack(MediaExtractor extractor, boolean audio) {
        int numTracks = extractor.getTrackCount();
        for (int i = 0; i < numTracks; i++) {
            MediaFormat format = extractor.getTrackFormat(i);
            String mime = format.getString(MediaFormat.KEY_MIME);
            if (audio) {
                if (mime.startsWith("audio/")) {
                    return i;
                }
            } else {
                if (mime.startsWith("video/")) {
                    return i;
                }
            }
        }
        return -5;
    }
```

##### 1.1.2 MediaFormat配置信息如下

![image-20200519170455550](1.png)



##### 1.1.3 MediaCodec支持两种模式编解码器，

> 1 同步synchronous
> 2  异步asynchronous

​		所谓同步模式是指编解码器数据的输入和输出是同步的，编解码器只有处理输出完毕才会再次接收输入数据；
​		异步编解码器数据的输入和输出是异步的，编解码器不会等待输出数据处理完毕才再次接收输入数据
这里，我们主要介绍下同步编解码，因为这种方式我们用得比较多。

我们知道当编解码器被启动后，每个编解码器都会拥有一组**输入和输出缓存区**，但是这些缓存区暂时无法被使用，只有通过MediaCodec的dequeueInputBuffer/dequeueOutputBuffer方法获取输入输出缓存区授权，通过返回的ID来操作 

##### 1.1.4 获取可使用的缓冲区索引

```
int outputBufferIndex = mMediaCodec.dequeueInputBuffer(TIMES_OUT);
```



> 返回一个填充了有效数据的input buffer的索引，
>
> 如果没有可用的buffer则返回-1
>
> 参数为超时时间(TIMES_OUT)，单位是微秒
>
> 当timeoutUs==0时，该方法立即返回；
>
> 当timeoutUs<0时，无限期地等待一个可用的input buffer
>
> 当timeoutUs>0时，等待时间为传入的微秒值。



​		 上面输入缓存的index，通过getInputBuffers()得到的是输入缓存数组，通过index和输入缓存数组可以得到当前请求的输入缓存，在使用之前要clear一下，避免之前的缓存数据影响当前数据，接着就是把数据添加到输入缓存中，并调用queueInputBuffer(…)把缓存数据入队；







##### 1.2 MediaCodec 流控之花屏	

流控就是流量控制。为什么要控制，就是为了在一定的限制条件下，**收益最大化**！

涉及到了 TCP 和视频编码：
对 TCP 来说就是控制单位时间内发送数据包的数据量，对编码来说就是控制单位时间内输出数据的数据量。

TCP 的限制条件是**网络带宽**，流控就是在避免造成或者加剧网络拥塞的前提下，

> 尽可能利用网络带宽。带宽够、网络好，我们就加快速度发送数据包，
>
> 出现了延迟增大、丢包之后，就放慢发包的速度（因为继续高速发包，可能会加剧网络拥塞，反而发得更慢）。

​		视频编码的限制条件最初是解码器的能力，码率太高就会无法解码，后来随着 codec 的发展，解码能力不再是瓶颈，限制条件变成了文件大小，我们希望在控制数据量的前提下，画面质量尽可能高。

​		一般编码器都可以设置一个目标码率，但编码器的实际输出码率不会完全符合设置，因为在编码过程中实际可以控制的并不是最终输出的码率，而是编码过程中的一个量化参数（Quantization Parameter，QP），

​			它和码率并没有固定的关系，而是取决于图像内容。 这一点不在这里展开，感兴趣的朋友可以阅读视频压缩编码和音频压缩编码的基本原理。

无论是要发送的 TCP 数据包，还是要编码的图像，都可能出现“尖峰”，也就是短时间内出现较大的数据量。TCP 面对尖峰，可以选择不为所动（尤其是网络已经拥塞的时候），这没有太大的问题，

​			但如果视频编码也对尖峰不为所动，那**图像质量就会大打折扣了**。如果有几帧数据量特别大，但仍要把码率控制在原来的水平，那势必要损失更多的信息，因此图像失真就会更严重。这种情况通常的表现是画面出现很多小方块，看上去像是打了马赛克一样，**导致画面的局部或者整体看不清楚**的情况





#### 1.4

1 播放音频

```
ffplay -ar 44100 -channels 2 -f s16le -i video_1589883340278.pcm
```



