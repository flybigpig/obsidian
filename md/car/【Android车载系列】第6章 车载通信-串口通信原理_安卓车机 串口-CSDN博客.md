## 1 串口

### 1.1 串口简介

  串行接口简称串口，也称串行通信接口或串行通讯接口（通常指COM接口），是采用串行通信方式的扩展接口。串行接口 （Serial Interface）是指数据一位一位地顺序传送。其特点是`通信线路简单`，只要一对传输线就可以实现双向通信（可以直接利用电话线作为传输线），从而`大大降低了成本`，特别`适用于远距离通信`，但`传送速度较慢`。

### 1.2 [串口通信](https://so.csdn.net/so/search?q=%E4%B8%B2%E5%8F%A3%E9%80%9A%E4%BF%A1&spm=1001.2101.3001.7020)

  当两个设备使用UART（通用异步收发器）进行通信时，它们至少通过三根导线连接：**TXD串口发送、RXD串口接收、GND**。`串口设备通过改变TXD信号线上的电压来发送数据，接收端通过检测RXD信号线上的电压来读取数据`  
  计算机一次传输信息（数据）一位或多个比特位。串行是指传输数据一次只传输一位。当进行串口通信时发送或者接收的每个字（即字节或字符）一次发送一位。每一位都是逻辑‘1’或者‘0’。也用Mark表示逻辑1，Space表示逻辑0。  
  串口数据速率使用 bits-per-second (“[bps](https://so.csdn.net/so/search?q=bps&spm=1001.2101.3001.7020)”) 或者 baud rate (“baud”)。这表示一秒内可以传输多少逻辑1 和0。当波特率超过 1000，你会经常看到用Kbps表示的速率。对于超过 1000000 的速率一般用Mbps来表示。

### 1.3 常见串口

  TTL、RS-232、RS-485指的是串口的电平标准(电信号)。电平标准简单来说就是：什么电压值表示0，什么电压值表示1：

> 1.TTL电平标准 是 低电平为0，高电平为1（电平信号）  
> 2.RS-232电平标准 是 正电平为0，负电平为1（电平信号）  
> 3.RS-485、RS-422 与RS-232类似，但是采用_差分信号_逻辑，更适合长距离、高速传输。  
> 4.TTL电平与RS232电平的转换：常用MAX232芯片

> 注意：DB9接口的协议常用的只有三种：RS-232、RS-485和RS-422。绝不会是TTL电平，80%的可能性是RS-232。

串口的硬件实现主要有两种：  
**D型9针插头（DB9）**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b5edc2857ba384eb64f74f535f3a647c.png#pic_center)  
**4针杜邦头**  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/d0bf39631679d188ca75b93839a33fd8.png#pic_center)

#### 1.3.1 TTL串口

  实际应用中，使用TTL电平逐渐成为趋势，在MCU与串口转接芯片提供的串口中比较常见，此时逻辑1：电压值：+5V或+3.3V等。逻辑0：电压值：0V（逻辑地）。

#### 1.3.2 RS-232串口

  RS-232 是由 EIA 定义的用于串口通信的标准电气接口。RS-232 实际上有三种不同的类型（A，B和 C），每一种对于逻辑1和逻辑0，电平定义了不同的电压范围。最常用的种类是 RS-232C，它定义逻辑1。电压范围：-3V~-12V 和逻辑0电压范围：+3V~+12V。  
下面列举最为常见的 DB-9 接口分布图，引脚和功能描述如下所示：  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/f78b74b128d606926e650f7a1e46f715.png#pic_center)  
其他还有RS-422以及 RS-485 串口标准。RS-422 使用更低电压与差分信号允许线缆长达 1000英尺（300m）。

#### 1.3.3 常用串口信号定义

> **GND - 逻辑地**：从技术上讲，逻辑接地不是信号，但没有它，其他信号就不能工作。从根本上说，逻辑地充当一个参考电压，从而知道哪些电压为正或负。  
> **TXD - 发送数据**  
> **RXD - 接收数据**  
> **RTS - 请求发送**：RTS设置为逻辑0电平表示己方准备好接收数据。一般与CTS一起用于串口流控，通常被设置为默认有效状态。除流控功能外，RTS也可用作通用输出信号，输出高低电平。常用于单片机复位或串口下载电路。  
> **CTS - 清除发送**：CTS信号接收自串口线缆的另一端。信号线逻辑0电平表示己方可以发送数据。一般与RTS一起用  
> 于串口数据流控。  
> **DTR - 数据终端就绪**：DTR 信号用于通知对端计算机或设备己方已就绪（逻辑0电平）或未就绪（逻辑1电平）。DTR也可用作通用输出信号，输出高低电平。常用于单片机复位或串口下载电路。

### 1.4 串口波特率

  波特率是指串口每秒钟传输的bit总数，如：9600波特率。表示1s传输9600个比特。1个比特所需时间为：1/9600 ≈ 0.104ms  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9760ec544b02378b6f61ef635ab37b02.png#pic_center)

### 1.5 串口异步通信

  解析串口数据需要确定一个字符的结束与下一个字符的开始。  
  串口数据线空闲时保持在逻辑1状态直到有字符发送。每个字节起始位在前，字节的每一位紧随其后，一位可选校验位以及一位或者两位停止位。起始位始终是逻辑0，通知对方有新串口数据可用。数据可以同时发送和接收，因此称为“异步”。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/5bdf4d49780373d3f1cd14458d89de1e.png#pic_center)

> **偶校验**：数据位加上校验位中的“1”的个数保持为偶数。  
> **奇校验**：数据位加上校验位中的“1”的个数保持为奇数。  
> **空白校验**：也称Space校验，校验位永远是0。  
> **标志校验**：也称Mark校验，校验位永远是1。  
> **无校验**：没有校验位存在或被传输。  
> **停止位**：在字符之间可能会有 1，1.5 或者 2 位停止位并且这些位总为 1。异步数据格式通常表达成“8N1”，“7E1”等。

### 1.6 全双工和半双工

> 全双工是指设备可以同时发送和接收数据，有两个独立数据通道（一路输入，一路输出）。  
> 半双工是指设备不能同时发送和接收数据，这通常意味着只有一路可以通讯，如RS485串口。

### 1.7 串口流控

  在两个串口设备间传输数据时经常有必要进行数据流控。这可能是受到中间串口通信线路、其中一个设备或者其他存储介质的限制。异步数据流控通常使用的有两种方法。  
  第一种方法通常称为“软件”流控，使用特殊字符开始（XON or DC1）或者停止（XOFF or DC3）数据流。这些字符定义参见 ASCII 码表。这些码值在传输文本信息时很有用，但不能在未经特殊编程时用于  
传输其他类型的信息。  
  第二种方法称作“硬件”流控，使用RTS和CTS信号线取代特殊字符。当接收方准备好接收数据时会将RTS置为逻辑0以请求对方发送数据，当未准备好时置为逻辑1，因此发送方会通过检测 CTS 电平状态判断是否可以发送数据。  
  使用硬件流控至少需要连接的信号线有[GND](https://so.csdn.net/so/search?q=GND&spm=1001.2101.3001.7020)、RXD、TXD、RTS、CTS。使用软件流控只需要GND、RXD、TXD。

### 1.8 Linux系统

  通过shell命令“lsusb”确认usb串口设备是否被正常识别：  
通过“ls /dev”确认插入前后串口设备节点ttyACM（CDC驱动模式下）或ttyUSB（VCP驱动模式下）是否生成；也可通过“dmesg”查看内核消息日志，查看USB串口设备枚举过程及驱动加载过程。

## 2 USB

### 2.1 USB简介

  USB，是英文Universal Serial Bus（通用串行总线）的缩写，是一个外部总线标准，用于规范电脑与外部设备的连接和通讯。是应用在PC领域的接口技术。也就是常见的笔记本电脑的u盘插口，鼠标插口，键盘插口。  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b77df5a4f59e3f4b8f8a0c18f54a75a2.png#pic_center)  
  串口和USB都是串行通信，但两者完全不同，串口1980年诞生，USB1995年诞生。USB通信速率，稳定性都比传统串口好。

### 2.2 USB转串口

1.  PL2303、CP2102、FT232R 芯片是用USB转串口\*(TTL电平输出)\*的芯片，需要安装Windows驱  
    动。
2.  下图是个USB转TTL串口的小板(TTL电平)，芯片为PL2303HX。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/8dfe2f3ae270c32e8a7063453ec72d58.png#pic_center)
3.  如果目标设备上是RS-232串口(D型9针接口)，再串接一片 MAX232芯片转换成 RS-232电平就行，于是产生了USB转RS-232串口的产品，如下图所示。仔细看(从右到左)，USB经过PL2303转成了TTL串口(中间那四个窟窿可以引出)，再经由MAX232转换为RS-232电平，9针串口引出。  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/06fd57af7893e3d3cbfed288cf027bb6.png#pic_center)
4.  现在市面上常见的USB转RS-232是这样的（只要是 D型9针串口，不会是TTL电平的，没特殊说明就默认是RS-232。）  
    ![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/25a55f19ed1a26b9c2e5edb68fde4a12.png#pic_center)

## 3 Android实现串口通讯

在Android设备上实现串口通信，我们需要完成以下几步：

1.  第一步找到串口文件
2.  第二步打开串口文件
3.  第三步发送和读取数据
4.  第四步关闭串口

  **由于串口通讯我们需要调整`波特率、数据位、校验位、停止位、流控`这些参数，Java层直接创建FileDescriptor访问串口文件的形式无法修改这些参数进行串口通讯，所以使用Native层创建FileDescriptor实例，调整对应参数进行串口通讯，提供fd对象给Java层来进行读写操作。**

下图为串口通信参数：

![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/7a5e30e7d71a3df6458d41eeebd94f28.png#pic_center)

### 3.1 第一步找到串口文件

  **Android的串口文件是有一个单独的目录：/dev/ttyS** ，我们`先找到串口文件目录`  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/4a6e69b632d89594be02fba3f10b2535.png#pic_center)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9f26fe40c428eccafe7b463b2ad0ab46.png#pic_center)  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/b6b122968783af60d98a5acea1781228.png#pic_center)  
下面我们用代码`操作这个ttys开头的文件`：

```
 private ArrayList<Driver> getDrivers() throws IOException {
        ArrayList<Driver> drivers = new ArrayList<>();
        LineNumberReader lineNumberReader = new LineNumberReader(new FileReader(DRIVERS_PATH));
        String readLine;
        while ((readLine = lineNumberReader.readLine()) != null) {
            String driverName = readLine.substring(0, 0x15).trim();
            String[] fields = readLine.split(" +");
            // driverName:/dev/tty 
            // driverName:/dev/console 
            // driverName:/dev/ptmx 
            // driverName:/dev/vc/0 
            // driverName:serial 
            // driverName:pty_slave 
            // driverName:pty_master 
            // driverName:unknown 
            Log.d(T.TAG, "SerialPortFinder getDrivers() driverName:" + driverName /*+ " readLine:" + readLine*/);
            if ((fields.length >= 5) && (fields[fields.length - 1].equals(SERIAL_FIELD))) {
                // 判断第四个等不等于serial
                // 找到了新串口驱动是:serial 此串口系列名是:/dev/ttyS 
                Log.d(T.TAG, "SerialPortFinder getDrivers() 找到了新串口驱动是:" + driverName + " 此串口系列名是:" + fields[fields.length - 4]);
                drivers.add(new Driver(driverName, fields[fields.length - 4]));
            }
        }
        return drivers;
    }
```

### 3.2 第二步打开串口文件

  `然后是检验和获取权限权限`，操作串口前通常需要有读写权限

```
  /**
     * 检查权限
     */
    public void checkPermission() {
        if (!device.canRead() || !device.canWrite()) {
            if (!chmod777(device)) {
                Log.i(T.TAG, "SerialPortManager openSerialPort: 没有读写权限");
                if (null != mOnOpenSerialPortListener) {
                    mOnOpenSerialPortListener.onFail(device, OnOpenSerialPortListener.Status.NO_READ_WRITE_PERMISSION);
                }
                return false;
            }
        }
    }

    /*** 文件设置最高权限 777 可读 可写 可执行 * @param file 你要对那个文件，获取root权限 * @return 权限修改是否成功- 返回：成功 与 失败 结果 */
    private boolean chmod777(File file) {
        if (null == file || !file.exists()) {
            // 文件不存在 
            return false;
        }
        try {
            // 获取ROOT权限 
            Process su = Runtime.getRuntime().exec("/system/bin/su");
            // 修改文件属性为 [可读 可写 可执行] 
            String cmd = "chmod 777 " + file.getAbsolutePath() + "\n" + "exit\n";
            su.getOutputStream().write(cmd.getBytes());
            if (0 == su.waitFor() && file.canRead() && file.canWrite() && file.canExecute()) {
                return true;
            }
        } catch (IOException | InterruptedException e) {
            // 没有ROOT权限 
            e.printStackTrace();
        }
        return false;
    }
```

  **检验完权限之后，我们就要用ndk的代码去打开串口进行操作，java层和 Native层的联系就是文件句柄FileDescriptor，也就是代码中的fd。Native层返回FileDescriptor，Java层的FileInputStream、FileOutputStream和FileDescriptor进行绑定，这样Java层就能读取到数据。**

```
  public void start() {
        try {
            // 打开串口-native函数 
            mFd = openNative(device.getAbsolutePath(), baudRate, 0);
            // 读取的流 绑定了 (mFd文件句柄)-通 过文件句柄(mFd)包装出 输入流 
            mFileInputStream = new FileInputStream(mFd);
            // 写入的流 绑定了 (mFd文件句柄)- 通过文件句柄(mFd)包装出 输出流 
            mFileOutputStream = new FileOutputStream(mFd);
            Log.i(T.TAG, "SerialPortManager openSerialPort: 串口已经打开 " + mFd);
            // 串口已 经打开 FileDescriptor[35]
            if (null != mOnOpenSerialPortListener) {
                mOnOpenSerialPortListener.onSuccess(device);
            }
            // 开启发送消息的线程
            startSendThread();
            // 开启接收消息的线程
            startReadThread();
            return true;  
        } catch (Exception e) {
            e.printStackTrace();
            if (null != mOnOpenSerialPortListener) {
                mOnOpenSerialPortListener.onFail(device, OnOpenSerialPortListener.Status.OPEN_FAIL);
            }
        }
    }
```

下面是Native代码：

```
 JNIEXPORT jobject JNICALL Java_com_test_openNative(JNIEnv *env, jclass thiz, jstring path, jint baudrate, jint flags) {
        int fd; // Linux串口文件句柄（本次整个函数最终的关键成果） 
        speed_t speed; // 波特率类型的值 
        jobject mFileDescriptor; // 文件句柄(最终返回的成果) //检查参数，获取波特率参数信息 [先确定好波特率] 
        {
            speed = getBaudrate(baudrate);
            if (speed == -1) {
                LOGE("无效的波特率，证明用户选择的波特率 是错误的");
                return NULL;
            }
        }
        // TODO 第一步：打开串口 
        {
            jboolean iscopy; const char *path_utf = ( * env)->GetStringUTFChars(env, path, & iscopy)
            ;
            LOGD("打开串口 路径是:%s", path_utf); // 打开串口 路径是:/dev/ttyS0 
            fd = open(path_utf, O_RDWR /*| flags*/); // 打开串口的函数，O_RDWR(读 和 写) 
            LOGD("打开串口 open() fd = %d", fd); // open() fd = 44
            ( * env)->ReleaseStringUTFChars(env, path, path_utf);
            // 释放操作 
            if (fd == -1) {
                LOGE("无法打开端口");
                return NULL;
            }
        } 
        LOGD("第一步：打开串口，成功了√√√");
        // TODO 第二步：获取和设置终端属性-配置串口设备 /* TCSANOW：不等数据传输完毕就立即改变属性。 TCSADRAIN：等待所有数据传输结束才改变属性。 TCSAFLUSH：清空输入输出缓冲区才改变属性。 注意：当进行多重修改时，应当在这个函数之后再次调用 tcgetattr() 来检测是否所有修改都成 功实现。*/ 
        {
            struct termios cfg;
            LOGD("执行配置串口中...");
            if (tcgetattr(fd, & cfg)){
                // 获取串口属性 
                LOGE("配置串口tcgetattr() 失败");
                close(fd); // 关闭串口 return NULL; 
            }
            cfmakeraw( & cfg); // 将串口设置成原始模式，并且让fd(文件句柄 对串口可读可 写) 
            cfsetispeed( & cfg, speed); // 设置串口读取波特率 
            cfsetospeed( & cfg, speed); // 设置串口写入波特率 
            if (tcsetattr(fd, TCSANOW, & cfg)){ // 根据上面的配置，再次获取串口属性 
                LOGE("再配置串口tcgetattr() 失败");
                close(fd); // 关闭串口 
                return NULL;
            }
        }
        
        LOGD("第二步：获取和设置终端属性-配置串口设备，成功了√√√");
        // TODO 第三步：构建FileDescriptor.java对象，并赋予丰富串口相关的值 
        {
            jclass cFileDescriptor = ( * env)->FindClass(env, "java/io/FileDescriptor");
            jmethodID iFileDescriptor = ( * env)->
            GetMethodID(env, cFileDescriptor, " <init>", "()V");
            jfieldID descriptorID = ( * env)->GetFieldID(env, cFileDescriptor, "descriptor", "I");
// 反射生成FileDescriptor对象，并赋值 (fd==Linux串口文件句柄) FileDescriptor的构造函数实 例化 
            mFileDescriptor = ( * env)->NewObject(env, cFileDescriptor, iFileDescriptor);
            ( * env)->SetIntField(env, mFileDescriptor, descriptorID, (jint) fd);
// 这里的 fd，就是打开串口的关键成果 }
            LOGD("第三步：构建FileDescriptor.java对象，并赋予丰富串口相关的值，成功了√√√");
            return mFileDescriptor; // 把最终的成果，返回会Java层 
        }
    }
```

这样我们就完成了整个打开串口的操作  
![在这里插入图片描述](https://i-blog.csdnimg.cn/blog_migrate/9a69fa22c0242f80784435760c1b2020.png#pic_center)

### 3.3 发送和读取数据

  **读取和发送数据是对文件IO进行操作，我们必须要在子线程中进行。**

```
  private void startReadThread() {
        mSerialPortReadThread = new SerialPortReadThread(mFileInputStream) {
            @Override
            public void onDataReceived(byte[] bytes) {
                if (null != mOnSerialPortDataListener) {
                    mOnSerialPortDataListener.onDataReceived(bytes);
                }
            }
        };
        mSerialPortReadThread.start();
    }

    /*** 串口消息读取线程 * 开启接收消息的线程 * 读取 串口数据 需要用到线程 */
    public abstract class SerialPortReadThread extends Thread {
        public abstract void onDataReceived(byte[] bytes);

        private static final String TAG = SerialPortReadThread.class.getSimpleName();
        private InputStream mInputStream; // 此输入流==mFileInputStream(关联mFd文件句柄)
        private byte[] mReadBuffer; // 用于装载读取到的串口数据

        public SerialPortReadThread(InputStream inputStream) {
            mInputStream = inputStream;
            mReadBuffer = new byte[1024]; // 缓冲区
        }

        @Override
        public void run() {
            super.run(); // 相当于是一直执行？为什么要一直执行？因为只要App存活，就需要读取 底层发过来的串口数据
            while (!isInterrupted()) {
                try {
                    if (null == mInputStream) {
                        return;
                    }
                    Log.i(TAG, "run: ");
                    int size = mInputStream.read(mReadBuffer);
                    if (-1 == size || 0 >= size) {
                        return;
                    }
                    byte[] readBytes = new byte[size]; // 拷贝到缓冲区
                    System.arraycopy(mReadBuffer, 0, readBytes, 0, size);
                    Log.i(TAG, "run: readBytes = " + new String(readBytes));
                    onDataReceived(readBytes); // 发出去-(间接的发到SerialPortActivity中 去打印显示)
                } catch (IOException e) {
                    e.printStackTrace();
                    return;
                }
            }
        }

        @Override
        public synchronized void start() {
            super.start();
        }

        /**
         * 关闭线程 释放资源
         */
        public void release() {
            interrupt();
            if (null != mInputStream) {
                try {
                    mInputStream.close();
                    mInputStream = null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    private void startSendThread() {
        // 开启发送消息的线程
        mSendingHandlerThread = new HandlerThread("mSendingHandlerThread");
        mSendingHandlerThread.start(); // Handler
        mSendingHandler = new Handler(mSendingHandlerThread.getLooper()) {
            @Override
            public void handleMessage(Message msg) {
                byte[] sendBytes = (byte[]) msg.obj;
                if (null != mFileOutputStream && null != sendBytes && 0 < sendBytes.length) {
                    try {
                        mFileOutputStream.write(sendBytes);
                        if (null != mOnSerialPortDataListener) {
                            mOnSerialPortDataListener.onDataSent(sendBytes); // 【发送 1】
                        }
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
            }
        };
    }
```

  读取和写入数据，其实就是对那两个读入，读处流进行操作，就这样我们就完成了对串口的收发数据。

### 3.4 关闭串口

  **我们用完串口之后，肯定会把串口关闭的，关闭串口，我们就把启动的读和写的线程关掉，在Native层也把串口关掉，将文件句柄绑定的两个流也关掉。**

```
 /*** 关闭串口 */
    public void closeSerialPort() {
        if (null != mFd) {
            closeNative(); // 关闭串口-native函数 
            mFd = null;
        }
        stopSendThread(); // 停止发送消息的线程 
        stopReadThread(); // 停止接收消息的线程
        if (null != mFileInputStream) {
            try {
                mFileInputStream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            mFileInputStream = null;
        }
        if (null != mFileOutputStream) {
            try {
                mFileOutputStream.close();
            } catch (IOException e) {
                e.printStackTrace();
            }
            mFileOutputStream = null;
        }
        mOnOpenSerialPortListener = null;
        mOnSerialPortDataListener = null;
    }
```

下面是Native代码：

```
    /** 关闭串口 * Class: cedric_serial_SerialPort * Method: close * Signature: ()V */
    JNIEXPORT void JNICALL Java_com_test_closeNative(JNIEnv *env, jobject thiz) {
        jclass SerialPortClass = ( * env)->GetObjectClass(env, thiz);
        jclass FileDescriptorClass = ( * env)->FindClass(env, "java/io/FileDescriptor");
        jfieldID mFdID = ( * env)->
        GetFieldID(env, SerialPortClass, "mFd", "Ljava/io/FileDescriptor;");
        jfieldID descriptorID = ( * env)->GetFieldID(env, FileDescriptorClass, "descriptor", "I");
        jobject mFd = ( * env)->GetObjectField(env, thiz, mFdID);
        jint descriptor = ( * env)->GetIntField(env, mFd, descriptorID);
        LOGD("关闭串口 close(fd = %d)", descriptor);
        close(descriptor); // 把此串口文件句柄关闭掉-文件读写流（文件句柄） InputStream/OutputStream=串口 发/收
    }
```

### 3.5 总结

`串口通信，其实就是对文件进行操作，一边读一边写`，就跟上学时你和同桌传纸条似得，以上代码参考的是谷歌的开源的代码，从寻找串口到关闭串口，梳理了一下串口通信的基本流程！希望对大家有用。