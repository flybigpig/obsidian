  前一章节简单介绍了串口、串口与USB的区别、Android上的[串口通信](https://so.csdn.net/so/search?q=%E4%B8%B2%E5%8F%A3%E9%80%9A%E4%BF%A1&spm=1001.2101.3001.7020)实现，本章节我们来了解一下`USB通信协议`以及`Android上实现USB通信`的相关知识。

## 1 USB简介

  通用串行总线 (Universal Serial Bus，USB) 是一种新兴的并逐渐取代其他接口标准的[数据通信](https://so.csdn.net/so/search?q=%E6%95%B0%E6%8D%AE%E9%80%9A%E4%BF%A1&spm=1001.2101.3001.7020)方式，由 Intel、Compaq、Digital、IBM、Microsoft、NEC及Northern Telecom 等计算机公司和通信公司于1995年联合制定，并逐渐形成了行业标准。USB 总线作为一种高速串行总线，其极高的传输速度可以满足高速数据传输的应用环境要求，且该总线还兼有供电简单（可总线供电）、安装配置便捷（支持即插即用和热插拔）、扩展端口简易（通过集线器最多可扩展127 个外设）、传输方式多样化（4 种传输模式），以及兼容良好（产品升级后向下兼容）等优点。  
  通用串行总线(universal serial bus，USB)自推出以来，已成功替代串口和并口，成为21世纪大量计算机和智能设备的标准扩展接口和必备接口之一，现已发展到USB 4版本。`USB 具有传输速度快、使用方便、支持热插拔、连接灵活、独立供电等优点，可以连接键盘、鼠标、大容量存储设备等多种外设，该接口也被广泛用于智能 手机中`。计算机等智能设备与外界数据的交互主要以网络和USB接口为主。

### 1.1 USB协议

  USB与I2C/SPI/UART类似，都是一种`传输数据的协议规范`，但USB主要设计是用于计算机与外接设备的数据交互和文件传输，这一点也正是它现在演变为相对高速的对外接口的原因，最高速的当然是PCIe。历史就不展开讲了，下图为各个USB协议版本的带宽及发布时间，基本上就是一些行业巨头联合开发的一种协议  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4c0975408ea33ac7106cddcf0f69f3d9.webp?x-image-process=image/format,png#pic_center)

### 1.2 USB协议特点

  USB的速度一直是跟着时代在飞速发展，并且得益于摩尔定律，最近几年USB速度也在不断翻倍提升。  
  USB 1.0就不提了，早已化作时代的眼泪。年轻的我都没接触过这个96年的老朋友。  
  USB 2.0 可能是目前最经典的接口，`480Mbps的速度`已经可以满足一般人的日常文件传输需求，所以这个千禧年的接口协议仍然活跃在市场上，而且并将继续活跃，服务器交换机目前还是使用USB 2.0居多。  
  USB 3，这里只谈USB 3.2 Gen2，因为这个是最新的。从USB 3x开始，这一协议开始展现出统治力，3x开始和type C结合，将曾经的文件传输接口增加为`文件加视频传输接口`，并且视频一上来就是最新的DP接口。  
  USB 4，这兄弟更猛，除了USB、DP，还额外增加了`PCIe`的功能，1 LANE支持到10Gbps，通过`隧道技术`来最大限度发挥物理带宽性能，这个还没有开始投向商用，应该还要一两年。

### 1.3 USB 3.2

>   2013年USB 3.0改名为USB 3.1 Gen 1，同时推出了10Gbps带宽的USB 3.1 Gen 2，两者统称为USB 3.1。  
>   到了2017年，USB 3.1 Gen 1和USB 3.1 Gen 2分别改名为USB 3.2 Gen 1和USB 3.2Gen 2 。同时加入了带宽为10Gbps的USB 3.2 Gen 1x2和带宽为20Gbps的USB 3.2 Gen 2x2，这4个统称USB 3.2。至此进入了USB 3.2时代，而USB 3.0的名字已经成为历史。  
>   总之，USB 2.0还保留着，而USB 3.0现在已经被USB-IF协会改名为USB 3.2 Gen 1了，而且还多了USB 3.2 Gen2、USB 3.2 Gen 1x2和USB 3.2 Gen 2x2。其中USB 3.2 Gen 1x2和USB 3.2 Gen 2x2表示USB3.2 Gen 1和USB 3.2 Gen 2的 双通道模式，而USB 3.2 Gen 1和USB 3.2Gen 2是单通道模式 。

  接口上看，USB 3.2之前大致上有两到三种插件类型，A/B/C，但从USB3.2开始，就只有type-C了，原因就是之前都是单通道模式，两组差分一发一收就可以了，但USB 3.2为了有更高的带宽，就将type-C的本来不用的另一组差分也用上了。具体来说，下图中的蓝色两个通道在USB 3.2之前正插反插都是只用到一组的，`从USB 3.2开始，正差反差就都是两组全用了`。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/e46b855f2ae98391e105d23d00de6196.webp?x-image-process=image/format,png#pic_center)  
下面分析下type C接口上的这些信号，在USB条件下分别有几个：

> USB 2.0数据信号4个，其实是两个，为了满足正反插需求所以正反都有；  
> USB 3.2数据信号8个，包含两通道的差分收发信号；  
> VBUS信号4个，GND信号4个，一共8个信号处理电源；  
> CC信号两个；  
> SBU信号两个。

### 1.4 USB 4.0

  对于USB 4，它的Gen 2和USB 3在每个lane的速度上是暂时一样的，Gen 3 则在USB 3.2的基础上再翻倍，但数据传输的内容和方式就有很大的差别。USB 3 是在传递USB的信号，但`USB 4 使用了隧道技术`，类似于将USB、DP、PCIe封装到一起的技术，不过这里面三个通道各自有其带宽上限。

## 2 USB通信

  USB通信两端分别称为：`HOST(USB主机) 与 Device(USB从机/USB配件)`，常见的主机就是我们的计算机。而Android 可以支持USB主机模式与USB配件模式，意思就是Android既可以是主机也可以是配件（从机）。

### 2.1 反向不归零编码

  NRZI 编码（Non Return Zero Inverted Code），即反向不归零编码。其实NRZI编码方式非常的简单，`即信号电平翻转表示0，信号电平不变表示1`；例如想要表示00100010(B)，则信号波形如下图所示：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3a508b8443439492154b49b0e8baaf73.png#pic_center)  
  由图可以看到，当电平状态发生变化时，表示的数据为0。在传输的数据中，很少出现全1的状态，故接收端可以根据发送端的电平变化确定采样时钟频率。但是有时候依然会出现数据为全1的状态，也就是说信号线一直保持一个状态，这个时候时钟信号就无法传输，接收端就无法同步时钟信号，这该如何解决呢？解决方式就是`在一定数量的1之后强行插入一个0`，就是说若信号线状态一直持续一段时间不变的话，发送端强行改变信号线的状态，接收端则只需要将这个变化忽略掉就可以了  
  例如有一段数据为：1111 1111 (B)要发送，则整个传输线上的电平状态是这样的：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/c3f513e3fb93161ecfc1069cf73c6ba9.png#pic_center)

### 2.2 主机模式

  主机模式表示Android设备作为主机为USB总线供电与外部设备进行通信。进行USB通信的大致流程如下：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7d09c52dd36fef04f910fb0d0689e327.png#pic_center)

### 2.3 连接检测

  这一步由电气工程师负责，软件开发人员无需关心。以低速设备连接检测为例：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/07d905671e66de63be2f4093396ac306.png#pic_center)

-   USB主机端口在D+（正极）和D-（负极）上都有15K的下拉电阻，在没有设备连接时，在下拉电阻的作用下，使得两条数据线的电压都是近地的，也就是0V；
-   低速USB设备在D-上有1.5K的上拉电阻；
-   在设备连接后，此时主机上D-的下拉电阻和设备上D-的上拉电阻构成分压。此时在D-上会出现3V左右的电压，D+仍然是0V；
-   主机检测电压维持2ms，则认为当前有USB设备连接，并且是一个低速USB设备。

### 2.4 枚举

  USB只是一个总线，只提供一个数据通路而已。USB总线驱动程序并不知道一个设备具体如何操作，有哪些行为。具体设备实现什么功能由设备自己决定，`设备回复主机的请求，会返回描述符，也会修改本地设置，这是枚举`。USB主机需要通过描述符了解。这些描述符主要包括：

-   `设备描述符`：一个设备只有一个，固定18字节长度，指出设备设备使用的USB协议号、设备信息、厂商和产品ID（vid与pid）等；
-   `配置描述符`：描述设备配置的集合，包含的接口数、配置编号、供电方式、电流需求量等，每个配置描述符中又定义了此配置里有多少接口；
-   `接口描述符`：每个接口有一个接口描述符，接口描述符定义了该接口的编号、接口端点数、接口的类、子类、协议等，每个端点对应一个端点描述符；
-   `端点描述符`：定义了端点大小，类型（读、写）等。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/fcc0e1c2da926322c88c5109fae0d0d5.png#pic_center)

#### 2.4.1 描述符间的关系

  USB设备会分出一些端点，如端点0、端点1等，因此USB主机要和设备通信，光有设备地址还不够，还需要一个端点地址。有了设备地址和端点地址，就可以准确的对端点发送和读取数据。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/aaedb61ecc979756c9609b316bc5268c.png#pic_center)  
  就好像我们在一台主机上运行多个Java程序，不同的Java程序就是不同的配置，而接口就是一个Java程序中实现个多个功能，包括文件操作、用户操作等功能。文件操作功能中开启多个Socket，其中Socket1接收文件数据，提供完成文件上传；Socket2发送文件数据，提供文件下载。我们需要通过主机IP地址与不同的Socket绑定的端口号去完成通信。  
  `枚举阶段就是从设备获取各种描述符，选择不同的接口（功能），根据端点完成通信！`

### 2.5 Android枚举设备

  在Android SDK中已经完成了枚举的封装。Android提供两种方式进行枚举：

#### 2.5.1 通过Intent过滤器

  当USB设备连接时，Android会启动包含：android.hardware.usb.action.USB\_DEVICE\_ATTACHED Action的意图，我们可以在Manifest中注册某个Activity支持处理该Action，让系统自动将连接设备的抽象对象 UsbDevice 发送至我们注册的Activity：

```
<activity>
<intent-filter>
<action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
</intent-filter>
<meta-data 
android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
android:resource="@xml/device_filter" />
</activity>
```

  除了指定 Intent 过滤器 之外，还需要指定一个资源文件来指定支持处理的USB设备的属性，在工程res/xml目录下创建 device\_filter.xml 文件，文件内容如下：

```
<resources>
<!-- 表示只对vid：1234，pid：5678 感兴趣，其他设备接入并不会调起我们的Activity。 -->
<usb-device vendor-id="1234" product-id="5678" />
</resources>
```

> VID：供应商ID，由供应商向USB-IF(Implementers Forum,应用者论坛)申请；  
> PID：产品ID，由供应商自行决定。不同的产品、相同产品的不同型号、相同型号的不同设计的产品采用不同的PID，以便区别相同厂家的不同设备。

如果需要处理所有USB设备，则需要将内容修改为：

```
<resources>
<usb-device  />
</resources>
```

配置完成后，在USB设备加入时，就能通过以下方式从 Intent 获取代表所连接设备的 UsbDevice ：

```
  @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        UsbDevice device = getIntent().getParcelableExtra(UsbManager.EXTRA_DEVICE);
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
    }
```

#### 2.5.2 主动枚举设备

  除了使用Intent过滤器在设备插入时接收连接设备的 UsbDevice 之外，还可以使用 getDeviceList() 方法获取连接的所有 USB 设备的哈希映射。

```
   @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
   UsbManager manager = (UsbManager) getSystemService(Context.USB_SERVICE);
        Map<String, UsbDevice> deviceList = manager.getDeviceList();
        Collection<UsbDevice> values = deviceList.values();
        Iterator<UsbDevice> iterator = values.iterator();
        while (iterator.hasNext()) {
            UsbDevice device = iterator.next();
        }
    }
```

### 2.6 获得通信权限

  如果应用使用 Intent 过滤器来发现已连接的 USB 设备，则它会在用户允许应用处理 Intent 时自动获得权限。否则，必须在应用中明确请求权限，然后才能连接到设备。因此在尝试与设备通信之前，必须先检查是否具有访问设备的权限。  
  如果还未具备设备访问权限则需要通过 requestPermission() 完成请求：

```

public class UsbReceiver extends BroadcastReceiver {
    public static final String ACTION_PERMISSION = "com.enjoy.usb.USB_PERMISSION";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent.getAction();
        if (ACTION_PERMISSION.equals(action)) {
            UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
            if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                Log.d("Lance", "获取权限 ");
            } else {
                Log.d("Lance", "拒绝权限 ");
            }
        }
    }
}

public class MainActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState); // 注册广播接收者 
        usbReceiver = new UsbReceiver();
        IntentFilter filter = new IntentFilter(UsbReceiver.ACTION_PERMISSION);
        registerReceiver(usbReceiver, filter);
        if (!usbManager.hasPermission(usbDevice)) { //申请权限，系统弹出对话框，用户选择后发出广播，触发注册的广播接收者 
            PendingIntent permissionIntent = PendingIntent.getBroadcast(this, 0, new Intent(UsbReceiver.ACTION_PERMISSION), 0);
            usbManager.requestPermission(usbDevice, permissionIntent);
        }
    }
}
```

### 2.7 通信准备

  在获得通信权限进行通信之前，一般会根据USB设备的描述信息来判断程序是否支持当前设备的通信。  
如下示例为判断是否为大容量存储设备（U盘）：

```
  private void judgeDevices() {
        for (int i = 0; i < usbDevice.getInterfaceCount(); i++) {
            UsbInterface usbInterface = usbDevice.getInterface(i);
            // 1、类为：USB_CLASS_MASS_STORAGE 0x08
            // 2、子类为：0x06 （大部分U盘使用）
            // 3、协议为：0x50 (Bulk-Only协议 批量传输)
            if (usbInterface.getInterfaceClass() == UsbConstants.USB_CLASS_MASS_STORAGE
                    && usbInterface.getInterfaceSubclass() == 0x06
                    && usbInterface.getInterfaceProtocol() == 0x50) {
                //每个存储设备一定有两个端点：in 和 out
                UsbEndpoint outEndpoint = null, inEndpoint = null;
                for (int j = 0; j < usbInterface.getEndpointCount(); j++) {
                    UsbEndpoint endpoint = usbInterface.getEndpoint(j);
                    if (endpoint.getType() == UsbConstants.USB_ENDPOINT_XFER_BULK) {
                        if (endpoint.getDirection() == UsbConstants.USB_DIR_OUT) {
                            outEndpoint = endpoint;
                        } else {
                            inEndpoint = endpoint;
                        }
                    }
                }
            }
        }
    }
```

### 2.8 数据传输

  在完成设备枚举后，选定 UsbDevice 对象并获取权限后，接下来就可以与之进行数据交互。在USB通信过程中主要有四种数据传输方式：Control Transaction(控制传输)、Interrupt Transaction(中断传输)、Bulk Transaction(批量传输)和Isochronous Transaction(等时传输)。

> **控制传输：**  
> 所有USB设备与主机必须支持的方式，特点是数据量小、正确性高，一般用于信息获取、命令控制与参数配置等。在获取设备描述符阶段采用此方式。  
> **中断传输：**  
> 中断传输是一种保证查询频率的传输，主机会保证在小于这个时间间隔的范围内安排一次传输，常用于数据量不大，但是对时间要求严格的设备，比如键盘按一个键值，鼠标移动的位移量。  
> **批量传输:**  
> 主要用于传输大量的，但是对时间无要求的数据，比如读取U盘的数据。  
> **等时传输：**  
> 适用于数据量大且恒定的数据，比如USB摄像头。

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/3b6aecffd22e388ecda88aa98333d63b.png#pic_center)  
Android提供使用 批量传输 `bulkTransfer()` 与控制传输 `controlTransfer()` 在端点上传输的数据。  
如果要完成中断与等时传输可以借助后面介绍的第三方C库：libusb。

在进行传输之前，首先需要获得设备对应的 UsbDeviceConnection 对象：

```
 deviceConnection = usbManager.openDevice(usbDevice);
//锁定此接口UsbInterface （其实就是锁定端口，同时只能一处使用） 
deviceConnection.claimInterface(usbInterface, true); 
// 控制传输 
byte[] maxLun = new byte[1];
deviceConnection.controlTransfer(requestType, request, value, index, bytes, bytes.length, TIMEOUT);
// 批量传输 
deviceConnection.bulkTransfer(endpoint, bytes, bytes.length, TIMEOUT);
```

### 2.9 关闭通信

  当完成与设备的通信后，需要调用 `releaseInterface()` 和 `close()` 来关闭 `UsbInterface` 和 `UsbDeviceConnection`。

```
//关闭 
deviceConnection.releaseInterface(usbInterface); 
deviceConnection.close();
```

而如果是设备断开连接，可以通过广播监听：

```
BroadcastReceiver usbReceiver = new BroadcastReceiver() {
            public void onReceive(Context context, Intent intent) {
                String action = intent.getAction();
                //某个USB设备断开连接
                if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    // 注意：当前通信设备
                    if (device != null && isCurDevice()) {
                        deviceConnection.releaseInterface(usbInterface);
                        deviceConnection.close();
                    }
                }
            }
        };
```