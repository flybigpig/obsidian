_努比亚技术团队原创内容，转载请务必注明出处。_

[Android画面显示流程分析(1)](https://www.jianshu.com/p/df46e4b39428)  
[Android画面显示流程分析(2)](https://www.jianshu.com/p/f96ab6646ae3)  
[Android画面显示流程分析(3)](https://www.jianshu.com/p/3c61375cc15b)  
[Android画面显示流程分析(4)](https://www.jianshu.com/p/7a18666a43ce)  
[Android画面显示流程分析(5)](https://www.jianshu.com/p/dcaf1eeddeb1)

目录截图：

  
![[Pasted image 20250106162059.png]]

企业微信截图_16324493913554.png

## 1. 前言

本文尝试从硬件认识开始介绍Android的显示系统是如何更新画面的，希望能就android系统是如何更新画面的问题，给读者带来一个感性认知。文中将尝试解释从App画出一帧画面到这帧画面是如何到达屏幕并最终被人眼看到的整个过程，这其中会涉及硬件的一些基础知识以及Android系统下一些重要的软件基础组件。本文将先分别介绍画面显示过程中所涉及到的重要组件及其工作原理，然后从Android app渲染第一帧画面开始逐步串联起各个组件，期望最终对于Android系统下画面是如何显示出来的给读者一个宏观的认识。本文适合对Android显示系统有过一定了解的同学阅读。由于文章较长，这里会分成10个章节，共五篇博文来介绍。

由于本文是通过阅读各种文章及代码，总结出来的，难免有些地方理解得不对，欢迎大家批评指正。

## 2. 显示硬件基础

无论软件的架构设计多么高端大气上档次，最终都离不开硬件的支持，软件的架构是建构在硬件的运行原理之上的，所以在讨论软件的各个设计之前我们有必要对显示硬件的原理做一个初步的了解。

### 2.1. 常见显示设备

**LCD**(Liquid Crystal Display)俗称液晶。

液晶是一种材料，液晶这种材料具有一种特点：可以在电信号的驱动下液晶分子进行旋转，旋转时会影响透光性，

因此我们可以在整个液晶面板后面用白光照（称为背光），可以通过不同电信号让液晶分子进行选择性的透光，此时在液晶面板前面看到的就是各种各样不同的颜色，这就是LCD显示画面的原理。

有些显示器（譬如LED显示器、CRT显示器）自己本身会发光称为主动发光，有些（LCD）本身不会发光只会透光，需要背光的协助才能看起来是发光的，称为被动发光。

其他一些主流的显示设备：

**CRT**：阴极摄像管显示器。 以前的那种大屁股电视机就是CRT显示，它曾是应用最广泛的显示器之一，不过现在基本没有在使用这种技术了。

**OLED**：有机发光二极管又称为有机电激光显示(Organic Light-Emitting Diode，OLED)，OLED显示技术具有自发光的特性，采用非常薄的有机材料涂层

和玻璃基板，可以做得更轻更薄，可视角度更大，并且能够显著节省电能。目前未成为主流，但是很有市场潜力，将来很可能取代LCD。

**LED**：主要用在户外大屏幕

### 2.2. LCD的接口技术

**VGA**(Video Graphics Array)即视频图形阵列，具有分辨率高、显示速率快、颜色丰富等优点。VGA接口不但是CRT显示设备的标准接口，同样也是LcD液晶显示设备的标准接口，具有广泛的应用范围。相信很多朋友都不会陌生，因为这种接口是电脑显示器上最主要的接口，从块头巨大的CRT显示器时代开始，VGA接口就被使用，并且一直沿用至今。

**HDMI**（High Definition Multimedia Interface） 高清多媒体接口，是一种全数字化视频和声音发送接口，可以发送未压缩的音频及视频信号。HDMI可用于机顶盒,DVD播放机、个人计算机、电视、游戏主机、综合扩大机、数字音响与电视机等设备。HDMI可以同时发送音频和视频信号，由于音频和视频信号采用同一条线材，大大简化系统线路的安装难度。

**DVI**（Digital Visual Interface），即数字视频接口。它是1998年9月，在Intel开发者论坛上成立的，由Silicon Image、Intel（英特尔）、Compaq（康柏）、IBM（国际商业机器公司）、HP（惠普）、NEC（日本电气股份有限公司）、Fujitsu(富士通)等公司共同组成的DDWG（Digital Display Working Group，数字显示工作组）推出的接口标准

**LVDS**（Low Voltage Differential Signaling，即低电压差分信号）接口又称RS-644总线接口，是20世纪90年代才提出的一种数据传输和接口技术。LVDS接口是美国NS美国国家半导体公司为克服以TTL电平方式传输宽带高码率数据时功耗大，电磁干扰大等缺点而研制的一种数字视频信号传输方式。由于其采用低压和低电流驱动方式，因此，实现了低噪声和低功耗。LVDS技术具有低功耗、低误码率、低串扰和低辐射等特点，其传输介质可以是铜质的PCB连线，也可以是平衡电缆。LVDS在对信号完整性、低抖动及共模特性要求较高的系统中得到了越来越广泛的应用，常见于液晶电视中。

**MIPI**(Mobile Industry Processor Interface)是2003年由ARM, Nokia, ST ,TI等公司成立的一个联盟，目的是把手机内部的接口如摄像头、显示屏接口、射频/基带接口等标准化，从而减少手机设计的复杂程度和增加设计灵活性。MIPI联盟下面有不同的WorkGroup，分别定义了一系列的手机内部接口标准，比如摄像头接口**CSI**、显示接口**DSI**、射频接口DigRF、麦克风/喇叭接口SLIMbus等。统一接口标准的好处是手机厂商根据需要可以从市面上灵活选择不同的芯片和模组，更改设计和功能时更加快捷方便。  

![](//upload-images.jianshu.io/upload_images/26874665-28066b6e4c400cda.png?imageMogr2/auto-orient/strip|imageView2/2/w/678/format/webp)

display_lcdinterfaces.png

![](//upload-images.jianshu.io/upload_images/26874665-837d4a877ceb8eeb.png?imageMogr2/auto-orient/strip|imageView2/2/w/700/format/webp)

display_dsi.png

目前手机屏幕和SOC间多使用MIPI接口来传输屏幕数据，其实物如下图所示，图中的条状芯片就是负责更新显示屏的显示内容的芯片DDIC, 它一边通过mipi协议和SOC通信，一边把获取到的显示数据写入到显示存储器GRAM内， 屏幕(Panel)通过不停扫描GRAM来不停更新液晶显示点的颜色，实现画面的更新。

![](//upload-images.jianshu.io/upload_images/26874665-54a55f0afefdaece.png?imageMogr2/auto-orient/strip|imageView2/2/w/810/format/webp)

displaydsi.png

屏幕坐标系

显示屏幕采用如下图所示的二维坐标系，以屏幕左上角为原点，X方向向右，Y轴方向向下，屏幕上的显示单元（像素）以行列式整齐排列，如下图所示，如下图示中以六边形块来代表一个像素点，如无例外说明，本文中所有图示都将以该六边形块来代表屏幕上的一个像素点。

  

![](//upload-images.jianshu.io/upload_images/26874665-df87bef2d7b25aef.png?imageMogr2/auto-orient/strip|imageView2/2/w/399/format/webp)

image-20210903204315892.png

在一个典型的Android显示系统中，一般包括SOC、DDIC、Panel三个部分， SOC负责绘画与多图层的合成，把合成好的数据通过硬件接口按某种协议传输给DDIC，然后DDIC负责把buffer里的数据呈现到Panel上。如下图所示为高通平台上的画面更新简单示意图，首先CPU或GPU负责绘画，画出的多个layer交由MDP进行合成，合成的数据通过mipi协议和DSI总线传输给DDIC, DDIC将数据存到GRAM内（非video屏）， Panel不断scanGRAM来显示内容。

![](//upload-images.jianshu.io/upload_images/26874665-6cd5e5d5e0e40ec3.png?imageMogr2/auto-orient/strip|imageView2/2/w/571/format/webp)

image-20210903202111863.png

那么对于DDIC来讲它的职责就是按mipi协议和SOC交互，获取到从SOC合成好的一帧画面的数据，然后将数据写入GRAM,对GRAM的写入要符合一定的时序。

### 2.3. LCD时序

这个写入时序要首先从过去的 CRT 显示器原理说起。CRT 的电子枪按照从上到下一行行扫描，扫描完成后显示器就呈现一帧画面，随后电子枪回到初始位置继续下一次扫描。为了把显示器的显示过程和系统的视频控制器进行同步，显示器（或者其他硬件）会用硬件时钟产生一系列的定时信号。当电子枪换到新的一行，准备进行扫描时，显示器会发出一个水平同步信号（horizonal synchronization），简称 HSync；而当一帧画面绘制完成后，电子枪回复到原位，准备画下一帧前，显示器会发出一个垂直同步信号（vertical synchronization），简称 VSync。显示器通常以固定频率进行刷新，这个刷新率就是 VSync 信号产生的频率。尽管现在的设备大都是液晶显示屏了，但原理仍然没有变。

**CLK**：像素时钟，像素数据只有在时钟上升或下降沿时才有效  
**ENB**：是数据使能信号，当它为高时，CLK信号到达时输出有效数据。  
**HBP**：行同步信号的前肩，水平同步信号的上升沿到ENABLE的上升沿的间隔。  
**HSW**：水平同步信号的低电平(非有效电平)持续时间  
**HFP**：行同步信号的后肩，ENABLE的下降沿到水平同步信号的下升沿的间隔 如下图所示

![](//upload-images.jianshu.io/upload_images/26874665-060a07b81de205f9.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210906101156688.png

图中从HSYNC的下降沿开始，等待两个CLK产生一个上升沿，在等待一个HBP时间后开始传输一行的像素数据，像素数据只有在像素时钟和上升沿或下降沿时才会写入（这里我们只讨论原理，并不是某个特定平台的具体实现，所以HSW所对应的时钟数字并不是以图中所示为准，具体产品中都会有调整和变化），经过N个像素时钟后成功将一行像素数据写入D1-Dn

类似地，在两帧画面之间也存在一些间隔：

**VSW**: VSYNC信号下降沿到上升沿间的时间  
**VBP**: 帧同步信号的前肩  
**VFP**:帧同步信号的后肩  
**VPROCH**: 被称为消隐区，它是指 VSW+VBP+VFP , 这个时间段内Panel不更新像素点的颜色

![](//upload-images.jianshu.io/upload_images/26874665-53b0c94da78ce7d3.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210906105428975.png

在一个Vsync周期内是由多个Hsync周期组成的，其个数就是屏幕在Y方向的像素点个数，也就是屏幕上有多少行像素点， 每个Hsync周期内传输一行内所有像素点的数据。通常屏厂给的spec文档还会给出类似下面这样的图，但都是表达一个意思，数据是以行为单位写入的，Hsync是协调行和行之间的同步信号，多个行依次写入构成一帧数据，帧和帧数据之间由Vsync信号来同步协调。

![](//upload-images.jianshu.io/upload_images/26874665-f9d216e7da74ed1b.png?imageMogr2/auto-orient/strip|imageView2/2/w/779/format/webp)

image-20210903204542586.png

当多帧画面依次输出到屏幕的时候我们就可以看到运动的画面了，通常这个速度到达每秒60帧时人眼就已经感觉画面很流畅了。下图演示了在vsync和hsync同步下两帧画面间的切换时序，以及消隐区(VPorch)在其中的位置关系。在消隐区结束（或开始）时DDIC会向SOC发出一个中断信号，这个信号称为TE信号， SOC这边就是通过该中断信号来判断上一帧数据是否已被DDIC读走，进而决定是否可以将buffer写入下一帧数据。

![](//upload-images.jianshu.io/upload_images/26874665-000308cc3105069f.png?imageMogr2/auto-orient/strip|imageView2/2/w/848/format/webp)

image-20210914145139687.png

### 2.4. LCD上的画面更新流程

让我们以下面这张图来说明从SOC到到DDIC再到Panel的画面更新过程，首先SOC准备画面A, DDIC上一帧画面更新完毕进入消隐区，同时向SOC侧发送TE信号，SOC收到TE信号后，A画面的数据开始通过DSI总线向DDIC传输（DSI Write）,当消隐区时间结束时开始这一帧数据从数据变为像素点颜色的过程(Disp Scan), Disp Scan是以行为单位将GRAM内一行的数据内容通过改变电流电压等方式改变Panel上像素点的颜色。进而实现一行画面的更新，按下来Disp Scan将以一定速度逐行读取GRAM的内容，而于此同时DSI Write也还在进行中，由于DSI Write较Disp Scan早了一个Vporch的时间，所以Disp Scan扫描到的数据都是A画面的数据。那么人眼会看到画面“逐渐”出现到显示屏上，当A画面的所有行都经Disp Scan到达屏幕后，下一个Vporch开始，DDIC再次向SOC发出TE信号， 下帧B画面的数据开始经过DSI总线传输到DDIC, 如此循环往复可以将连续的A, B， C画面更新到屏幕上。

![](//upload-images.jianshu.io/upload_images/26874665-5e15399032676ad4.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210914154226941.png

这里我们思考一个问题： 就像上图中所绘一样总会存在一段时间GRAM内的数据会出现一部分是新画面的数据，一部分是旧画面的数据，那么从上帝视角看GRAM里的画面是“撕裂”的，那Disp Scan会不会把这个“撕裂”的画面显示到Panel上呢？ 用户有没有可能看到一个“撕裂”的画面呢？

答案是有可能会。

为讨论方便我们这里先把DSI write记作写， 把Disp Scan记作读， 正常情况下，我们会将读速度调到和写速度差不多，由于写是在进入vporch时就开始了，而读的动作是离开vproch时，所以在读和写的这场“百米跑”竞赛中总是写跑在前面，这样保证读始终读到的是同一帧画面的数据，这个过程如下面图1所示。

![](//upload-images.jianshu.io/upload_images/26874665-85fcbf461f9d7d99.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image.png

由于读和写的速度受到环境或各种电气因素影响并不是一成不变的，在实际运行过程中会有波动，当读和写两条线离的较近时由于速度波动的存在会出现两条线交叉的情况，如图2所示，那么这时读会先后读到前后两帧的数据，出现花屏现象。

有时我们为了避免由于速率波动引起花屏问题，会放大vporch的时间让读和写尽量拉开距离，这样可以减少花屏现象出现的概率，这里的原理如图3所示

继续思考下一问题： 从上面的分析可以看出Panel上的像素数据是一行行更新下来的，那人眼为什么没有看到更新了一半的画面呢？

这个问题我并不知道答案，这可能涉及到人眼的生物学构造，人眼对事物的成像原理等复杂的原理，总之是在这个时间尺度内人眼看不到。

接下来我们从另一角度来研究一下这个问题，我们把视角切换到上帝视角，假如存在上帝之眼的话，他应该能看到这个更新了一半的画面，我们尝试用高速摄影机来模拟“上帝之眼”。

首先我们先准备一些测试代码， 我们写一个测试用apk, 下面这段代码是向Android框架注册一个Choreographer的监听，Choreographer是Android提供的一个获取vsync信号的通道。当下图中doFrame被调用到时我们这里可以暂时粗略理解为是上面所讨论的TE信号， 这里收到Vsync信号后通过Pthread的Condition通知到另一线程。这用于模拟一个vsync的消息泵。

```csharp
Choreographer.getInstance().postFrameCallback(new Choreographer.FrameCallback() {
    @Override
    public void doFrame(long frameTimeNanos) {//当vsync信号来时会调用到这里
            Global.lock.lock();
            try {
                Global.syncCondition.signal();//通知另一条线程更新画面
            } catch (Exception e) {
                e.printStackTrace();
            } finally {
                Global.lock.unlock();
            }
            Choreographer.getInstance().removeFrameCallback(this);
            Choreographer.getInstance().postFrameCallback(this);
    }
});
```

下面代码中每收到一个vsync信号都会去画一帧画面，同时给帧编号编号加1（相当于给每一帧一个编号），如果当前的编号为奇数则画绿色，如果为偶数则画蓝色，那么画面应该是蓝绿交替出现：

```java
public class MySurfaceView extends SurfaceView implements SurfaceHolder.Callback, Runnable {
    .......

    @Override
    public void run() {
        while(true) {
            Global.lock.lock();
            try {
                Global.syncCondition.await();//在这里等待vsync到来的通知消息
            } catch (InterruptedException e) {
                e.printStackTrace();
            } finally {
                Global.lock.unlock();
            }
            draw();//画蓝色或绿色
        }
    }
    ........
    private void draw() {

        Canvas mCanvas = null;
        try {
            mCanvas = mSurfaceHolder.lockCanvas();
            if(autoNum %2 == 0) {
                mPaint.setColor(Color.BLUE);//如果为双数则画面画成蓝色
            } else {
                mPaint.setColor(Color.GREEN);//如果为单数则画面画成绿色
            }
            mCanvas.drawRect(0, 0, getRight(), getBottom(), mPaint);
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (mCanvas != null) {
                mSurfaceHolder.unlockCanvasAndPost(mCanvas);
            }
        }
        autoNum++;//数字加1
    }

}
```

下面是高速摄影机看到的画面变化情况，从下图中可以看出来，画面从绿变蓝的过程中总是能看到先是上面部分变蓝色，接下来才会看到全部变蓝，由蓝变绿也是同样的现象，先是图像上面变成绿色接下来全部变绿色，在“上帝之眼”看来画面的变化是有前后两个画面各占一部分的情况的，这情况是上面是新画面下面是旧画面，但人眼是看不出来这种画面。

下面的图中发现画面是颜色渐变的，这是因为通过高速摄影机是画面再次感光形成的影像，由于物理世界中光线是有衍射及曝光时长因素的，所以最终在高速摄影机留下的图中是颜色渐变的。

![](//upload-images.jianshu.io/upload_images/26874665-2fc2099d84f4d721.png?imageMogr2/auto-orient/strip|imageView2/2/w/1200/format/webp)

image-20210904121328054.png

### 2.5. 本章小结

在本章节中，我们了解到了一些显示硬件的一些组成要件，以及屏幕是如何按时序更新画面的，以及在画面更新过程中屏和SOC间的一些互动是如何完成的。那么接下来我们再来了解下SOC内部是如何把画面送过来的。

  
  
作者：努比亚技术团队  
链接：https://www.jianshu.com/p/df46e4b39428  
来源：简书  
著作权归作者所有。商业转载请联系作者获得授权，非商业转载请注明出处。